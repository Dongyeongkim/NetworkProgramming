#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define MESSAGE_SIZE 1023
#define PACKET_SIZE 1024

#define IDLE 0
#define CHAT_SERVER_DOING 1
#define CHAT_CLIENT_DOING 2

// Transmission Unit OPCODE

#define OK 1
#define FAIL 2
#define REGISTER 3
#define STATUS 4
#define REQUEST 5

// Transmission Unit

typedef struct
{
	uint8_t OPCODE;
	char msg[MESSAGE_SIZE];
} TransmissionUnit;

// My information Struct
typedef struct
{
	char name[20]; // chatting nickname
	char ip[20];   // my ip
	uint16_t port; // chatting TCP server port
	uint8_t state; // 0: IDLE   1:CHAT_SERVER_DOING  2:CHAT_CLIENT_DOING
	char password[30];
} MyInfo;

int mcast_rsocket();
struct sockaddr_in addr;

int main(int argc, char *argv[])
{
	char otherip[20]={0,};
	int multicast_received = 0;
	int connection_status = 0;
	// common
	MyInfo myinfo;
	strcpy(myinfo.password, "Wreckedship");
	TransmissionUnit TU;
	char buf[255];
	char msg[PACKET_SIZE] = {
		0,
	};

	// multicast
	int mcast_rcvsock;
	int str_len;
	int addr_size;
	char mbuf[PACKET_SIZE];

	// command sock
	int cmd_sock = -1;
	struct sockaddr_in cmd_addr, from_addr;
	char ip[20] = {
		0,
	},
		 cport[5] = {
			 0,
		 };
	int port;

	// tcp server and new client  for chat Server
	int tcp_servsock, new_clisock = -1, temp_clisock;
	struct sockaddr_in tcp_servaddr, cli_addr;

	// tcp client for chat client
	int tcp_clisock;
	struct sockaddr_in tcp_cliaddr;

	// select
	fd_set readfds, readfds_backup;
	int fd, maxfd, stat;

	if (argc != 3)
	{
		printf("Usage : %s <My IP> <Service PORT>\n", argv[0]);
		exit(1);
	}

	// keyboard input
	fd = fileno(stdin);

	strcpy(myinfo.name, "DongyeongKim");
	strcpy(myinfo.ip, argv[1]);
	myinfo.port = atoi(argv[2]);
	myinfo.state = IDLE;

	// for mcast receiver
	mcast_rcvsock = mcast_rsocket();

	// for heartbeat
	cmd_sock = socket(PF_INET, SOCK_STREAM, 0);
	memset(&cmd_addr, 0, sizeof(cmd_addr));
	cmd_addr.sin_family = AF_INET;

	// tcp chatting server socket
	tcp_servsock = socket(PF_INET, SOCK_STREAM, 0);
	tcp_servaddr.sin_family = AF_INET;
	tcp_servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	tcp_servaddr.sin_port = htons(atoi(argv[2]));
	if (bind(tcp_servsock, (struct sockaddr *)&tcp_servaddr, sizeof(tcp_servaddr)) == -1)
	{
		printf("bind() error");
		close(tcp_servsock);
		exit(1);
	}
	int statel = listen(tcp_servsock, 5);
	if (statel == -1)
	{
		perror("listen error : ");
		exit(0);
	}

	// tcp chatting client socket
	tcp_clisock = socket(PF_INET, SOCK_STREAM, 0);
	
	maxfd = tcp_clisock; // max socket descriptor

	FD_ZERO(&readfds);

	FD_SET(mcast_rcvsock, &readfds);
	FD_SET(cmd_sock, &readfds);
	FD_SET(tcp_servsock, &readfds);
	FD_SET(fd, &readfds);
	readfds_backup = readfds;

	while (1)
	{
		
		readfds = readfds_backup;
		memset(mbuf, 0, PACKET_SIZE);
		stat = select(maxfd + 1, &readfds, (fd_set *)0, (fd_set *)0, NULL);
		switch (stat)
		{
		case -1: //error
			
			exit(1);
		case 0: // timeout
		
			exit(1);
			break;
		default:
			
			if (FD_ISSET(mcast_rcvsock, &readfds))
			{
				
				recvfrom(mcast_rcvsock, mbuf, PACKET_SIZE, 0,
						 (struct sockaddr *)&from_addr, &addr_size);
				// Get CM IP and Port from mbuf
				strcpy(ip, mbuf);
				memcpy(cport, &mbuf[20], 4);
				port = atoi(cport);

				// Address and Port setting
				cmd_addr.sin_addr.s_addr = inet_addr(ip);
				cmd_addr.sin_port = htons(port);

				if (multicast_received == 0)
				{
					printf("\nChatting Manager IP and Port> [%s] [%d]\n", ip, port);
					multicast_received++;
				}
			}

			if (FD_ISSET(tcp_servsock, &readfds))
			{
				
				
				int servlen = sizeof(tcp_servaddr);
				temp_clisock = accept(tcp_servsock, (struct sockaddr *)&tcp_servaddr, &servlen);

				if (myinfo.state == IDLE)
				{
					new_clisock = temp_clisock;
					if (maxfd < new_clisock)
						maxfd = new_clisock;
					FD_SET(new_clisock, &readfds_backup);
					myinfo.state = CHAT_SERVER_DOING;
					memset(&TU, 0, 1024);
					TU.OPCODE = STATUS;
					memcpy(&TU.msg, &myinfo, 73);
					write(cmd_sock, &TU, 1024);
					memset(&TU, 0, 1024);
					read(new_clisock, &TU.msg,1023);
					printf("Connected from: %s\n",TU.msg);
					strcpy(otherip, TU.msg);
					memset(&TU, 0, 1024);
				}
				else
				{
					char errmsg[100] = "I can't connect with you. now i am talking with another person. type cmd listup to check status\n";
					write(temp_clisock, &errmsg, 100);
					close(temp_clisock);
				}
			}

			// recv when start Server chat
			if (FD_ISSET(new_clisock, &readfds))
			{
				
				memset(msg, 0, PACKET_SIZE);
				str_len = read(new_clisock, msg, PACKET_SIZE);
				if (str_len <= 0)
				{
					FD_CLR(new_clisock, &readfds_backup);
					close(new_clisock);
					new_clisock = -1;
					myinfo.state = IDLE;
					memset(&TU, 0, 1024);
					TU.OPCODE = STATUS;
					memcpy(&TU.msg, &myinfo, 73);
					write(cmd_sock, &TU, 1024);
					memset(&TU, 0, 1024);
				}
				else{
					printf("from: %s\n",otherip);
					puts(msg);}
			}

			// recv when start Client chat
			if (FD_ISSET(tcp_clisock, &readfds))
			{
				

				memset(msg, 0, PACKET_SIZE);
				str_len = read(tcp_clisock, msg, PACKET_SIZE);
				if (str_len <= 0)
				{
					myinfo.state = IDLE;
					memset(&TU, 0, 1024);
					TU.OPCODE = STATUS;
					memcpy(&TU.msg, &myinfo, 73);
					write(cmd_sock, &TU, 1024);
					memset(&TU, 0, 1024);
				}
				else
					puts(msg);
			}
			// recv
			// ================================================================
			//  send

			if (FD_ISSET(fd, &readfds))
			{
				
				//check socket status
				int error = 0;
				socklen_t len = sizeof(error);

				// command mode - need to use TCP
				memset(buf, 0, 255);
				fgets(buf, 255, stdin);
				if (strncmp(buf, "cmd", 3) == 0)
				{
					printf("CMD> %s", &buf[4]);
					if (strncmp(&buf[4], "register", 8) == 0)
					{
						if (-1 == connect(cmd_sock, (struct sockaddr *)&cmd_addr, sizeof(cmd_addr)))
						{
							printf("Can't connect to chatting manager. check network status or network setting\n");
							memset(buf, 0, 255);
						}
						else
						{
							
							// Transmission Unit Setting
							memset(&TU, 0, 1024);
							TU.OPCODE = REGISTER;
							memcpy(&TU.msg, myinfo.name, sizeof(myinfo.name));
							memcpy(&TU.msg[20], myinfo.ip, sizeof(myinfo.ip));
							memcpy(&TU.msg[40], &myinfo.port, 2);
							memcpy(&TU.msg[42], &myinfo.state, 1);
							memcpy(&TU.msg[43], myinfo.password, sizeof(myinfo.password));

							//TCP sending
							write(cmd_sock, &TU, 1024);
							memset(&TU, 0, 1024);
							memset(buf, 0, 255);
							//Check Error or Not
							int sucornot = read(cmd_sock, &TU, 1024);

							if (sucornot == -1)
							{
								printf("Fail to send register message. please retry.\n");
							}
							else if (TU.OPCODE == OK)
							{
								printf("Register process completed without error!\n");
								connection_status = 1;
							}
							else if (TU.OPCODE == FAIL)
							{
								printf("Fail to register. check password again\n");
							}
						}
					}
					if (strncmp(&buf[4], "listup", 6) == 0)
					{

						//TU memory set to 0
						memset(&TU, 0, 1024);
						TU.OPCODE = REQUEST;

						// see connection status
						if (connection_status == 0)
						{
							printf("Please connect to Chatting Manager first.");
							memset(buf, 0, 255);
						}

						else
						{
							write(cmd_sock, &TU, 1024);
							memset(buf, 0, 255);
							memset(&TU, 0, 1024);
							int sucornot = read(cmd_sock, &TU, 1024);

							if (sucornot == -1)
							{
								printf("Fail to send. please retry.\n");
							}

							else if (TU.OPCODE == OK)
							{
								for (int i = 0; i < 10; i++)
								{

									MyInfo otherinfo;
									memcpy(&otherinfo, &TU.msg[i * 73], 73);
									printf("\n\n");
									printf("%s\n", otherinfo.name);
									printf(" =======================\n");
									printf("IP: %s\n", otherinfo.ip);
									printf("PORT: %u\n", otherinfo.port);
									printf("STATE: %u\n", otherinfo.state);
									printf("\n\n");
									memset(&otherinfo, 0, 73);
								}
							}

							else if (TU.OPCODE == FAIL)
							{
								printf("Fail to get list. please retry and check your connection status\n");
							}
							memset(&TU, 0, 1024);
						}
					}

					if (strncmp(&buf[4], "help", 4) == 0)
					{
						printf("1. register: cmd register             \n");
						printf("2. listup: cmd listup                 \n");
						printf("3. help: cmd help                     \n");
						memset(buf, 0, 255);
					}

					else
					{
						printf("Cannot find appropriate command! if you want to see available commands, then type \"cmd help\"\n");
						memset(buf, 0, 255);
					}
				}
				// chatting mode
				else if (strncmp(buf, "cht", 3) == 0)
				{
					printf("CHT> %s", &buf[4]);
					if (strncmp(&buf[4], "help", 4) == 0)
					{
						printf("Chatting Client command help...\n");
						printf("cht connect <IP> <PORT>        \n");
						printf("cht send <MESSAGE>             \n");
						printf("cht close                      \n");
						memset(buf, 0, 255);
					}
					
					if (strncmp(&buf[4], "connect", 7) == 0)
					{
						char *sArr[2] = {
							NULL,
						};
						int j = 0;
						char *ptr = strtok(&buf[11], " ");
						while (ptr != NULL)
						{
							sArr[j] = ptr;
							j++;
							ptr = strtok(NULL, " ");
						}
						char cli_ip[20] = {
							0,
						};
						char cli_port[5] = {
							0,
						};
						strcpy(cli_ip, sArr[0]);
						strcpy(cli_port, sArr[1]);
						memset(&tcp_cliaddr, 0, sizeof(tcp_cliaddr));
						tcp_cliaddr.sin_family = AF_INET;
						tcp_cliaddr.sin_addr.s_addr = inet_addr(cli_ip);
						tcp_cliaddr.sin_port = htons(atoi(cli_port));
						int cli_len = sizeof(tcp_cliaddr);
						
						if (-1 == connect(tcp_clisock, (struct sockaddr *)&tcp_cliaddr, cli_len))
						{
							printf("Fail to connect. please retry.\n");
						}
						else
						{
							myinfo.state = CHAT_CLIENT_DOING;
							memset(&TU, 0, 1024);
							TU.OPCODE = STATUS;
							memcpy(&TU.msg, &myinfo, 43);
							write(cmd_sock, &TU, 1024);
							memset(&TU, 0, 1024);
							strcpy(TU.msg, argv[1]);
							write(tcp_clisock, TU.msg, 1023);
							memset(&TU, 0, 1024);
							printf("Connected!\n");
							FD_SET(tcp_clisock, &readfds_backup);
						}
					}

					if (strncmp(&buf[4], "send", 4) == 0)
					{

						if (myinfo.state == CHAT_SERVER_DOING)
						{

							if (0 >= write(new_clisock, &buf[9], 246))
							{
								printf("send error. check your network status and retry.\n");
							}
						}

						else if (myinfo.state == CHAT_CLIENT_DOING)
						{
							char amsg[200];
							memcpy(amsg, &buf[9], 200);

							printf("%s\n",amsg);
							if (0 >= write(tcp_clisock, amsg, 200))
							{
								printf("send error. check your network status and retry.\n");
							}
						}
						else if (myinfo.state == IDLE)
						{
							printf("Check your connection status!...\n");
						}
					}

					if (strncmp(&buf[4], "disconnect", 10) == 0)
					{
						if (myinfo.state == CHAT_SERVER_DOING)
						{
							if (-1 == close(new_clisock))
							{
								printf("Nothing to close. Check connection status\n");
							}
							else
							{
								FD_CLR(new_clisock, &readfds);
								maxfd = cmd_sock;
							}
						}

						else if (myinfo.state == CHAT_CLIENT_DOING)
						{
							if ( -1 == close(tcp_clisock))
							{
								printf("Nothing to close. Check connection status\n");
							}
							else
							{
								memset(&tcp_cliaddr, 0, sizeof(tcp_cliaddr));
							}
						}
						else if (myinfo.state == IDLE)
						{
							printf("You didn't connect to any device\n");
						}
					}
				}
			}

		} // switch end
	}	  // while end
	return 0;
}

// make Multicasting Receive Socket
int mcast_rsocket()
{
	int mcast_sock;
	struct ip_mreq join_addr;
	int on = 0;

	mcast_sock = socket(PF_INET, SOCK_DGRAM, 0);
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(9100);

	// Bind socket
	if (bind(mcast_sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		printf("bind() error");
		close(mcast_sock);
		exit(1);
	}

	// Specify the multicast Group
	join_addr.imr_multiaddr.s_addr = inet_addr("239.0.120.1");
	// Accept multicast from any interface
	join_addr.imr_interface.s_addr = htonl(INADDR_ANY);

	// Join Multicast Group
	if ((setsockopt(mcast_sock, IPPROTO_IP,
					IP_ADD_MEMBERSHIP, (void *)&join_addr, sizeof(join_addr))) < 0)
	{
		printf(" SetsockOpt Join Error \n");
		close(mcast_sock);
		exit(1);
	}

	return mcast_sock;
}

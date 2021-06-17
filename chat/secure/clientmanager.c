#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define TTL 2

#define MESSAGE_SIZE 1023

// Transmission Unit OPCODE

#define OK 1
#define FAIL 2 
#define REGISTER 3
#define STATUS 4
#define REQUEST 5


// Transmission Unit 

typedef struct {
	uint8_t OPCODE;
	char msg[MESSAGE_SIZE];
	}TransmissionUnit;

// Client info 

typedef struct {
	char name[20];	// chatting nickname
	char ip[20];	// my ip
	uint16_t port;		// chatting TCP server port
	uint8_t state; 	// 0: IDLE   1:CHAT_SERVER_DOING  2:CHAT_CLIENT_DOING
	char password[30];
    }ClientInfo;

// IP-Port info

typedef struct {
    char ip[20];
    char port[5];
}MultiInfo;


// Error handling

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}



// max socket and max buffer size

#define MAX_SOCK 10
#define MAX_BUF 1024


int main(int argc, char **argv)
{
    
    int time_live = TTL;
    int nowclientnum;
    char buf[MAX_BUF];
    struct timeval tv;
    TransmissionUnit TU;
    int client[MAX_SOCK];
    int state, client_len;
    int i, max_client, maxfd;

    fd_set readfds, otherfds, allfds;

    tv.tv_sec = 2;
    tv.tv_usec = 0;
    ClientInfo cl1, cl2, cl3, cl4, cl5, cl6, cl7, cl8, cl9, cl10;
    struct sockaddr_in clientaddr, serveraddr, multi_addr;
    int server_sockfd, client_sockfd, sockfd, multicast_sockfd;

    MultiInfo multiinfo;
    
    strcpy(multiinfo.ip, argv[1]);
    strcpy(multiinfo.port, argv[2]);
    

    ClientInfo* clientarr[10] = {&cl1, &cl2, &cl3, &cl4, &cl5, &cl6, &cl7, &cl8, &cl9, &cl10};

    state = 0;


    if(argc!=3) {
	    printf("Usage : %s <port>\n", argv[0]);
	    exit(1);
    }

    if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket error : ");
        exit(0);
    }

    memset(&serveraddr, 0x00, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(atoi(argv[2]));


    multicast_sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if(multicast_sockfd == -1)  error_handling("MultiCast Socket Creation Error");
    memset(&multi_addr, 0, sizeof(multi_addr));
    multi_addr.sin_family = AF_INET;
    multi_addr.sin_addr.s_addr = inet_addr("239.0.120.1");
    multi_addr.sin_port = htons(9100);

    setsockopt(multicast_sockfd, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&time_live, sizeof(time_live));


    if(bind(multicast_sockfd, (struct sockaddr *)&multi_addr, sizeof(multi_addr)) == -1){
        printf("Error to open multicast port");
    }
    printf("[+] MultiCast is running on 239.0.120.1:%d\n\n", 9100);
    state = bind(server_sockfd, (struct sockaddr *)&serveraddr,
            sizeof(serveraddr));
    
    if (state == -1) {
        perror("bind error : ");
        exit(0);
    }

    state = listen(server_sockfd, 5);
    if (state == -1) {
        perror("listen error : ");
        exit(0);
    }

    client_sockfd = server_sockfd;

    // init
    max_client = -1;
    maxfd = server_sockfd;

   for (i = 0; i < MAX_SOCK; i++) {
        client[i] = -1;
    }

    FD_ZERO(&readfds);
    FD_SET(server_sockfd, &readfds);
    FD_SET(multicast_sockfd, &readfds);
    
    //

    printf("[+] TCP server is running on %s:%s\n", argv[1], argv[2]);
    fflush(stdout);

    while(1)
    {
        allfds = readfds;
        state = select(maxfd + 1 , &allfds, NULL, NULL, &tv);
        switch(state) {
            case -1:
                perror("Select error : ");
                exit(1);
                break;

            case 0: // Timeout
                sendto(multicast_sockfd, &multiinfo, sizeof(multiinfo), 0, (struct sockaddr *)&multi_addr, sizeof(multi_addr));
                tv.tv_sec = 2;
                tv.tv_usec = 0;

                break;
            
            default:
                
                // Server Socket - accept from client
                if (FD_ISSET(server_sockfd, &allfds)) {
                client_len = sizeof(clientaddr);
                client_sockfd = accept(server_sockfd,(struct sockaddr *)&clientaddr, &client_len);
                // keepalive: heartbeat check

                printf("\nconnection from (%s , %d)",inet_ntoa(clientaddr.sin_addr),ntohs(clientaddr.sin_port));
                for (i = 0; i < MAX_SOCK; i++){
                    if (client[i] < 0) {
                        client[i] = client_sockfd;
                        printf("\nclientNUM=%d\nclientFD=%d\n", i+1, client_sockfd);
                        nowclientnum = i+1;
                        break;
                        }
                    }

                printf("accept [%d]\n", client_sockfd);
                printf("===================================\n");
                if (i == MAX_SOCK) {
                        perror("too many clients\n");
                    }
                    FD_SET(client_sockfd,&readfds);

                    if (client_sockfd > maxfd)
                        maxfd = client_sockfd;

                    if (i > max_client)
                        max_client = i;

                    if (--state <= 0)
                        continue;

                }

            // client socket check
                for (i = 0; i <= max_client; i++)
                {
                    if ((sockfd = client[i]) < 0) {
                        continue;
                    }

                    if (FD_ISSET(sockfd, &allfds))
                    {
                        memset(buf, 0, MAX_BUF);

                // Disconnet from Client 
                        if (read(sockfd, buf, MAX_BUF) <= 0) {
                            printf("Close sockfd : %d\n",sockfd);
                            printf("===================================\n");
                            close(sockfd);
                            FD_CLR(sockfd, &readfds);
                            client[i] = -1;
                            memset(clientarr[i],0,73);
                        }
                        else {
                            
                            
                            memcpy(&TU, &buf, 1024);
                            if(TU.OPCODE == REGISTER){
                                ClientInfo newclientinfo;
                                memcpy(&newclientinfo, &TU.msg,73);
                                printf("%s\n",newclientinfo.name);
                                if(strcmp(newclientinfo.password, "Wreckedship")==0){
                                    memset(&newclientinfo.password, 0, sizeof(newclientinfo.password));
                                    memcpy(clientarr[i], &newclientinfo, 73);
                                    printf("%s\n",clientarr[i]->name);
                                    printf("%s\n",clientarr[i]->ip);
                                    printf("%d\n",clientarr[i]->port);
                                    printf("%d\n",clientarr[i]->state);
                                    printf("%s\n", clientarr[i]->password);
                                    memset(&TU, 0, 1024);
                                    TU.OPCODE = OK;
                                    write(sockfd, &TU, 1024);
                                    memset(&TU, 0, 1024);
                                    memset(&newclientinfo, 0, 73);
                                }
                                else{
                                    memset(&TU, 0, 1024);
                                    TU.OPCODE = FAIL;
                                    write(sockfd, &TU, 1024);
                                    printf("Close sockfd : %d\n",sockfd);
                                    printf("Cause: Fail to register.\n");
                                    printf("===================================\n");
                                    close(sockfd);
                                    memset(&TU, 0, 1024);
                                    FD_CLR(sockfd, &readfds);
                                    client[i] = -1;
                                }


                            }
                            if(TU.OPCODE == REQUEST){
                                memset(&TU, 0, 1024);
                                for(int i = 0; i < nowclientnum; i++){
                                    memcpy(&TU.msg,clientarr[i],73);
                                    printf("%s\n",clientarr[i]->name);
                                    printf("%s\n",clientarr[i]->ip);
                                    printf("%d\n",clientarr[i]->port);
                                    printf("%d\n",clientarr[i]->state);
                                    printf("%s\n", clientarr[i]->password);
                                    }
                                TU.OPCODE = OK;
                                write(sockfd, &TU, 1024);
                                memset(&TU, 0, 1024);
                                }
                            }
                            
                        
                            }
                        if (--state <= 0)break;
                } // for
            } // while
        } 
}


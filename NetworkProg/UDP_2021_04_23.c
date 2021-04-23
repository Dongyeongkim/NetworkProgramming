#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE	1024

// error handling function

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

// connnect&send function

int connect_and_send_message(char* ip, int port, char* message){
	int sock;
	int str_len;
	int adr_sz; 
	struct sockaddr_in serv_addr, from_adr;


	sock=socket(PF_INET, SOCK_STREAM, 0);
	if(sock == -1)
		error_handling("socket() error");
	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr=inet_addr(ip);
	serv_addr.sin_port=htons(port);
	while(1)
   {
      fputs("Insert message(q to quit): ", stdout);
      fgets(message, sizeof(message), stdin);     
      if(!strcmp(message,"q\n") || !strcmp(message,"Q\n"))   
         break;
      sendto(sock, message, strlen(message), 0, 
               (struct sockaddr*)&serv_addr, sizeof(serv_addr));
      adr_sz=sizeof(from_adr);
      str_len=recvfrom(sock, message, BUF_SIZE, 0, 
               (struct sockaddr*)&from_adr, &adr_sz);

      message[str_len]=0;
      printf("Message from server: %s", message);
   }  
	
	return sock;
}


// close connection function 

int close_connection(int sock){
	close(sock);
}


//main function

int main(int argc, char* argv[]){
	int port;
	char message[1024];
	char ip[20];
	int sock;

	if( argc != 3){
		printf(" ERROR\n");
		exit(0);
		}
	strcpy(ip, argv[1]);
	port = atoi(argv[2]);
	printf("IP: %s, PORT: %d\n", ip, port);
	sock = connect_and_send_message(ip, port, message);
	close_connection(sock);
}

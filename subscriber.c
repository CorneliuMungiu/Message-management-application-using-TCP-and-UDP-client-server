#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "utils.h"

void usage(char *file){
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

/*Primeste un send cu lungimea mesajului trimis(int) de lungime sizeof(int) si atat timp
cat received va fi mai mic decat marimea pachetului trimis acesta va astepta un nou recv*/
int tcp_recv(int sockfd, char* message){
	memset(message,0,BUFLEN);
	//Nr de octeti primit
	int received = 0;
	//Asteapta lungimea pachetului
	while(received < sizeof(int)){
		int ret = recv(sockfd, message + received, sizeof(int) - received,0);
		if(ret == 0)
			return 0;
		received += ret;
	}

	int size = 0;
	memcpy(&size,(int *)message,sizeof(int));
	memset(message,0,BUFLEN);
	received = 0;
	//Asteapta receive pana primeste pachetul intreg(de dimensiune size)
	while(received < size){
		int ret = recv(sockfd,message + received, size - received,0);
		received += ret;
	}
	return size;
}

/*Trimite un send cu lungimea mesajului intr-un int, apoi trimite mesajul propriuzis*/
void tcp_send(int sockfd, char *message){
	int len = strlen(message) + 1;
	send(sockfd,&len,sizeof(int),0);
	send(sockfd,message,len,0);
}

/*Verifica daca mesajul citit de la tastatura respecta formatul din tema*/
int valid_message(char *buffer){
	char *bufferCpy = calloc(BUFLEN,sizeof(char));
	char *token;
	strcpy(bufferCpy,buffer);
	token = strtok(bufferCpy," ");
	//daca primul argument este subscribe
	if(strcmp(token,"subscribe") == 0){
		token = strtok(NULL," ");
		//Daca exista al 2-lea argument
		if(strcmp(token,"") != 0){
			token = strtok(NULL," ");
			//Daca exista al 3-lea argument
			if(token == NULL)
				return 0;
			//Daca ultimul argument(sf) este 1 sau 0
			if((strcmp(token,"0\n") == 0) || (strcmp(token,"1\n") == 0)){
				printf("Subscribed to topic.\n");
				return 1;
			}
		}
	}else if(strcmp(token,"unsubscribe") == 0){//Unsubscribe
		token = strtok(NULL," ");
		//Daca exista al 2-lea argument(topic)
		if(strcmp(token,"") != 0){
			printf("Unsubscribed from topic.\n");
			return 1;
		}
	}
	return 0;
}

int main(int argc, char *argv[])
{
	setvbuf(stdout,NULL,_IONBF,BUFSIZ);
	
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];
	fd_set read_fds, tmp_fds;

	if (argc < 3) {
		usage(argv[0]);
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	int set = 1;
	ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&set, sizeof(int));
	DIE(ret < 0, "TCP_NODELAY");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");
	//Trimite la server id clientului
	tcp_send(sockfd,argv[1]);

	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);

	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
			// se citeste de la tastatura
			fgets(buffer, BUFLEN - 1, stdin);

			if (strncmp(buffer, "exit", 4) == 0) {
				//Daca se citeste exit de la tastatura se inchide clientul si se 
				//trimite mesaj de exit la client pentru a seta isConnected pe 0
				tcp_send(sockfd,"exit");
				break;
			}
			//Verifica daca textul de la tastatura este valid
			n = valid_message(buffer);
			if(n == 1){//Mesaj Valid
				//Trimite mesajul la server(mesajul de subscribe sau unsubscribe)
				tcp_send(sockfd,buffer);
			}

		}
		//Primeste mesaj de la server
		if (FD_ISSET(sockfd, &tmp_fds)) {
			memset(buffer, 0, BUFLEN);
			//salveaza mesajul in buffer
			tcp_recv(sockfd,buffer);
			//Daca serverul a trimis mesaj de exit(serverul s-a inchis), se inchide si clientul
			if (strncmp(buffer, "exit", 4) == 0) {
				close(sockfd);
				return 0;
			}else{
				//Afiseaza informatia trimisa de server
				printf("%s\n",buffer);
			}
		}
	}

	close(sockfd);

	return 0;
}

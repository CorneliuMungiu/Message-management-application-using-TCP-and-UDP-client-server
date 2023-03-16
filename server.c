#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "utils.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
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

/*Initializeaza un client*/
Client* init_client() {
	Client* client = calloc(1, sizeof(Client));
  client->isConnected = 0;
  client->socket = 0;
  client->topics = NULL;
  client->nrOfTopics = 0;
  client->show_after_connect = NULL;
  client->nrOfShowAfterConnect = 0;
  return client;
}

/*Adauga un client in lista de clienti*/
int add_client(char* id, Client** clients, int *nrOfClients, int sockfd,struct sockaddr_in cli_addr) {
  //Daca inca nu a fost adaugat nici un client in lista de clienti
  if(*clients == NULL){
    Client* client = init_client();
    strcpy(client->id,id);
	client->isConnected = 1;
	client->socket = sockfd;
	*clients = client;
	*nrOfClients = *nrOfClients + 1;
	printf("New client %s connected from %s:%u\n",(*clients)[(*nrOfClients) - 1].id, inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port);
	return 0;
  }
  //Cauta prin toti clientii din lista de clienti
  for(int i = 0; i < (*nrOfClients); i++){
	//Daca a fost gasit un client cu acelasi id in lista
    if(strcmp((*clients)[i].id,id) == 0){
	  //Daca clientul cu id gasit este conectat deja la server
      if((*clients)[i].isConnected == 1){
		printf("Client %s already connected.\n",(*clients)[i].id);
		//Trimite mesaj de exit serverului care incearca sa faca legatura.
		tcp_send(sockfd,"exit");
		return 1;
	  }
	  //Clientul cu acest id este in lista insa s-a deconectat mai devreme si acum se reconecteaza
	  (*clients)[i].isConnected = 1;
      (*clients)[i].socket = sockfd;
	  printf("New client %s connected from %s:%u\n",(*clients)[i].id, inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port);
	  //Se verifica in coada de topicuri daca nu a fost trimis vre-un mesaj de server atat timp
	  //cat clientul a fost inactiv(daca avea setat sf pe 1)
	  if((*clients)[i].show_after_connect != NULL){
		  for(int j = 0; j < (*clients)[i].nrOfShowAfterConnect; j++){
			//Trimite mesaj la client cu mesajele pierdute intre timp(cat a fost deconenctat)
			tcp_send(sockfd,(*clients)[i].show_after_connect[j]);
		  }
		  //Resetez coada
		  (*clients)[i].show_after_connect = NULL;
		  (*clients)[i].nrOfShowAfterConnect = 0;

	  }
      return 0;
    }
  }
  //Un nou client(cu exceptia primului)
  Client* clientul = init_client();
  strcpy(clientul->id,id);
  clientul->socket = sockfd;
  clientul->isConnected = 1;
  //Realoca lista de clienti adaugand memorie pentru inca un client
  (*clients) = realloc((*clients), sizeof(Client) * ((*nrOfClients) + 1));
  memcpy(&((*clients)[*nrOfClients]), clientul, sizeof(Client));
  *nrOfClients = *nrOfClients + 1;
  printf("New client %s connected from %s:%u\n",(*clients)[(*nrOfClients) - 1].id, inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port);
  free(clientul);
  return 0;
}

//Returneaza clientul cu socketul primit ca parametru
Client* getClient(Client** clients,int sockfd, int nrOfClients){
	for(int i = 0; i < nrOfClients; i++){
		if((*clients)[i].socket == sockfd){
			return &(*clients)[i];
		}
	}
	return NULL;
}

//Trimite mesaj de exit tuturor clientilor
void exit_clients(Client** clients, int *nrOfClients){
	for(int i = 0; i < (*nrOfClients);i++){
		//Trimite mesaj de exit la clientul i
		tcp_send((*clients)[i].socket,"exit");
	}
}

//Adauga un nou topic in lista de topicuri
void add_topics(Client **clients, int sockfd, int nrOfClients, char *topic, int sf){
	Client* client = getClient(clients,sockfd,nrOfClients);
	//Daca este primul topic adaugat
	if(client->topics == NULL){
		//Aloca memorie pentru structura topics
		client->topics = calloc(1,sizeof(Topic));
		//Aloca memorie pentru un topic
		client->topics->info = calloc(strlen(topic) + 1,sizeof(char));
		//Adauga topicul
		strcpy(client->topics[0].info,topic);
		client->topics[0].sf = sf;
		client->nrOfTopics++;
		return;
	}
	//Actualizeaza sf topicului in cazul in care clientul a fost deja abonat 
	//la acest topic
	for(int i = 0; i < client->nrOfTopics; i++){
		if(strcmp(topic,client->topics[i].info) == 0){
			client->topics[i].sf = sf;
			return;
		}
	}
	//Adauga un nou topic(cu exceptia primului)
	//Realoca memorie pentru inca un topic
	client->topics = realloc(client->topics,sizeof(Topic) * (client->nrOfTopics + 1));
	client->topics[client->nrOfTopics].info = calloc(strlen(topic) + 1,sizeof(char));
	//Adauga noul topic in lista de topicuri
	strcpy(client->topics[client->nrOfTopics].info,topic);
	client->topics[client->nrOfTopics].sf = sf;
	client->nrOfTopics++;
}

//Adauga un mesaj in coada de mesaje care vor fi trimise dupa reconectarea clientului
void add_show_after_connect(Client *client, char *message){
	//Daca inca nu este nimic in coada
	if(client->show_after_connect == NULL){
		//Aloca memorie pentru lista de stringuri
		client->show_after_connect = calloc(1,sizeof(char*));
		client->show_after_connect[0] = calloc(strlen(message) + 1, sizeof(char));
		//Adauga mesajul in lista de stringuri
		strcpy(client->show_after_connect[0],message);
		client->nrOfShowAfterConnect++;
		return;
	}
	//Realoca memorie daca trebuie sa adauge in coada inca un mesaj
	client->show_after_connect = realloc(client->show_after_connect, sizeof(char*) * (client->nrOfShowAfterConnect + 1));
	client->show_after_connect[client->nrOfShowAfterConnect] = calloc(strlen(message) + 1, sizeof(char));
	//Adauga noul mesaj
	strcpy(client->show_after_connect[client->nrOfShowAfterConnect],message);
	client->nrOfShowAfterConnect++;
}

//Sterge un topic din lista de topicuri
void remove_topics(Client **clients, int sockfd, int nrOfClients, char *topic){
	//Extrage clientul cu socketul sockfd
	Client* client = getClient(clients,sockfd,nrOfClients);
	//Daca clientul nu are nici un topic
	if(client->topics == NULL)
		return;
	//Cauta prin lista de topicuri
	for(int i = 0; i < client->nrOfTopics; i++){
		//daca a gasit topicul cautat
		if(strcmp(client->topics[i].info,topic) == 0){
			//Stege topicul din lista de topicuri
			memmove(&client->topics[i], &client->topics[i + 1], (client->nrOfTopics - i - 1)*sizeof(Topic));
			client->nrOfTopics--;
		}
	}
}

//Parseaza mesajul trimis de udp
char *udp_parse(int sockfd, char *buffer){
	char *message = calloc(BUFLEN, sizeof(char));
	char *topic = calloc(TOPIC_LEN, sizeof(char));
	uint8_t type = buffer[50];
	int padding = 0;

	memcpy(topic,buffer,50);
	sprintf(message, "%s - ", topic);
	padding += strlen(message);

//INT
	if(type == 0){
		uint8_t sign = buffer[50 + sizeof(uint8_t)];
		uint8_t first_byte = buffer[52];
		uint8_t second_byte = buffer[53];
		uint8_t third_byte = buffer[54];
		uint8_t fourth_byte = buffer[55];
	
		int integer = (first_byte << 24) + (second_byte << 16) + (third_byte << 8) + fourth_byte;

		sprintf(message + padding, "INT - %d",(integer * (sign ? -1 : 1)));
	}//SHORT_REAL
	if(type == 1){
		uint8_t first_byte = buffer[51];
		uint8_t second_byte = buffer[52];
		uint16_t short_int = (first_byte << 8) + second_byte;

		if(short_int % 100 != 0){
			sprintf(message + padding, "SHORT_REAL - %.2f", (float)short_int / 100);
		}else{
			sprintf(message + padding, "SHORT_REAL - %.0f", (float)short_int / 100);
		}
	}//FLOAT
	if(type == 2){
		int nr = buffer[56];
		uint8_t first_byte = buffer[52];
		uint8_t second_byte = buffer[53];
		uint8_t third_byte = buffer[54];
		uint8_t fourth_byte = buffer[55];

		uint8_t sign = buffer[51];
		int float_data = (first_byte << 24) + (second_byte << 16) + (third_byte << 8) + fourth_byte;
		int multiple = 1;
		for(int i = 0; i < nr; i++){
			multiple *= 10;
		}

		sprintf(message + padding, "FLOAT - %.4f",((float)float_data/multiple)*(sign ? -1 : 1));
	}//STRING
	if(type == 3){
		sprintf(message + padding, "STRING - %s",buffer + 51);
	}
	free(topic);
	return message;
}

//Cauta un topic in lista de topicuri a clientului si reintoarce indexul acestuia daca il
//gaseste sau -1 in caz contrar
int check_topic_in_client(Client *client, char *topic){
	for(int i = 0; i < client->nrOfTopics; i++){
		if(strcmp(topic,client->topics[i].info) == 0)
			return i;
	}
	return -1;
}

//Trimite un mesaj de la un topic unui client
void send_message_to_client(Client **clients, int nrOfClients, char *topic, char *message){
	for (int i = 0; i < nrOfClients; i++){
		//Daca topicul este in lista de topicuri
		int n = check_topic_in_client(&(*clients)[i],topic);
		//Daca clientul este conectat acum ii trimite mesajul acum
		if((*clients)[i].isConnected == 1){
			if(n != -1){
				// tcp_send((*clients)[i].socket,message);
				//send((*clients)[i].socket,message,BUFLEN,0);
				tcp_send((*clients)[i].socket,message);
			}
		}else{//Daca clientul nu este conectat ii adauga mesajul in coada de asteptare daca sf este setat
			if((n != -1) && ((*clients)[i].topics[n].sf == 1)){
				add_show_after_connect(&(*clients)[i],message);
			}
		}
	}
}

//Primeste mesajele de la udp
int recv_udp(Client** clients, int sockfd, int nrOfClients){
	char *buffer = calloc(BUFLEN, sizeof(char));
	char *topic = calloc(TOPIC_LEN, sizeof(char));
	char *ip = calloc(MAX_ID, sizeof(char));
	struct sockaddr_in *addr = calloc(1,sizeof(struct sockaddr_in));
	unsigned int addrLen = sizeof(struct sockaddr_in);

	recvfrom(sockfd,buffer,BUFLEN,0,(struct sockaddr*)addr,&addrLen);

	strcpy(ip,inet_ntoa(addr->sin_addr));
	memcpy(topic,buffer,50);
	char *send_message = calloc(BUFLEN,sizeof(char));
	char *message = udp_parse(0, buffer);
	sprintf(send_message, "%s:%d - %s",ip,addr->sin_port,message);
	//Trimite parametrii extrasi la send_mesage_to_client
	send_message_to_client(clients,nrOfClients,topic,send_message);
	free(message);
	return 1;
}

int main(int argc, char *argv[])
{
	setvbuf(stdout,NULL,_IONBF,BUFSIZ);

	int tcp_sockfd, udp_sockfd, newsockfd, portno;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int i, ret;
	socklen_t clilen;
	
  	Client* clients = NULL;
  	int nrOfClients = 0;

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc < 2) {
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	
	//Socketul udp
	udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udp_sockfd < 0, "udp_socket");

	//Socketul tcp
	tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcp_sockfd < 0, "tcp_socket");

	portno = atoi(argv[1]);
	DIE(portno == 0, "port");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(tcp_sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "tcp_bind");

	ret = bind(udp_sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "udp_bind");

	ret = listen(tcp_sockfd, MAX_CLIENTS);
	DIE(ret < 0, "tcp_listen");

	int set = 1;
	ret = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&set, sizeof(int));
	DIE(ret < 0, "TCP_NODELAY");

	FD_SET(udp_sockfd, &read_fds);
	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(tcp_sockfd, &read_fds);
	fdmax = tcp_sockfd;
	int exitFor = 0;

	FD_SET(STDIN_FILENO,&read_fds);

	while (1) {
		tmp_fds = read_fds; 
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");
		for (i = 0; i <= fdmax; i++) {
			if(exitFor == 1){
				exitFor = 0;
				break;
			}
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == tcp_sockfd) {
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					clilen = sizeof(cli_addr);
					newsockfd = accept(tcp_sockfd, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfd < 0, "accept");

					ret = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&set, sizeof(int));
					DIE(ret < 0, "TCP_NODELAY");

					memset(buffer, 0, BUFLEN);
					//Extrage mesajul trimis de client
					int n = tcp_recv(newsockfd,buffer);
					//Adauga clientul daca este nevoie
					n = add_client(buffer,&clients,&nrOfClients,newsockfd,cli_addr);
					//Daca clientul este deja conectat
					if(n == 1){
						exitFor = 1;
						continue;
					}
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax) { 
						fdmax = newsockfd;
					}

				}else if(i == udp_sockfd){//UDP
					//Extrage mesajele de la udp
					recv_udp(&clients,i,nrOfClients);
				}else if(i == STDIN_FILENO){//STDIN
					fgets(buffer, BUFLEN - 1, stdin);
					//Daca serverul primeste exit de la stdin
					if (strncmp(buffer, "exit", 4) == 0) {
						//Trimite mesaj de exit tuturor clientilor
						exit_clients(&clients,&nrOfClients);
						return 0;
					}
				}else{//Primeste mesaj de la client
					memset(buffer,0,BUFLEN);				
					tcp_recv(i,buffer);
					//Daca primeste mesaj de exit de la client
					if(strcmp(buffer,"exit") == 0){
						Client* aux = getClient(&clients,i,nrOfClients);
						//Seteaza isConnected pe 0
						aux->isConnected = 0;
						printf("Client %s disconnected.\n",aux->id);
						//Inchide socketul
						close(i);
						FD_CLR(i,&read_fds);
						continue;
					}else {//A primit de la client subscribe sau unsubscribe
						char *subscribe = calloc(BUFLEN,sizeof(char));
						char *topic = calloc(TOPIC_LEN,sizeof(char));
						int sf;
						sscanf(buffer, "%s %s %d",subscribe,topic,&sf);
						//Daca a primit subscribe
						if(strcmp(subscribe,"subscribe") == 0){
							add_topics(&clients,i,nrOfClients,topic,sf);
						}else {//Daca a primit unsubscribe
							remove_topics(&clients,i,nrOfClients,topic);
						}
						
					}
					
				}
			}
		}
	}
	close(tcp_sockfd);
	close(udp_sockfd);

	return 0;
}

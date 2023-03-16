#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>
#include <netinet/tcp.h>

#define BUFLEN		1600	// dimensiunea maxima a calupului de date
#define MAX_CLIENTS	100	// numarul maxim de clienti in asteptare
#define INVALID_MESSAGE ("Format invalid pentru mesaj!")
#define MAX_CLIENTS_NO 100
#define FD_START 4
#define MAX_ID 11
#define TOPIC_LEN 51


typedef struct topic{
  char *info;
  uint8_t sf;
}Topic;

typedef struct client{
  char id[MAX_ID];
  int socket;
  uint8_t isConnected;
  Topic* topics;
  int nrOfTopics;
  char** show_after_connect;
  int nrOfShowAfterConnect;
}Client;

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)


#endif

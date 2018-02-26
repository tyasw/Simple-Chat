/*  prog3_participant.c
 *   CSCI 367 Winter 2018 - Program 3
 *
 *   Author:
 *     William Tyas
 *   Description:
 *
 *   Uses demo_client.c program from Winter 2018
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_WORD_LEN 10

int main( int argc, char **argv) {
	struct hostent *ptrh;	/* pointer to a host table entry */
	struct protoent *ptrp; 	/* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
	int sd;					/* socket descriptor */
	int port; 				/* protocol port number */
	char *host; 			/* pointer to host name */
	int n;					// number of bytes read from recv

	char valid;				// Will server let us join chat group?

	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */

	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./observer server_address server_port\n");
		exit(EXIT_FAILURE);
	}

	port = atoi(argv[2]); /* convert to binary */
	if (port > 0) { /* test for legal value */
		sad.sin_port = htons((u_short)port);
	} else {
		fprintf(stderr,"Error: bad port number %s\n",argv[2]);
		exit(EXIT_FAILURE);
	}

	host = argv[1]; /* if host argument specified */

	/* Convert host name to equivalent IP address and copy to sad. */
	ptrh = gethostbyname(host);
	if ( ptrh == NULL ) {
		fprintf(stderr,"Error: Invalid host: %s\n", host);
		exit(EXIT_FAILURE);
	}

	memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

	/* Map TCP transport protocol name to protocol number. */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create a socket. */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Connect the socket to the specified server. */
	if (connect(sd, (struct sockaddr*)&sad, sizeof(sad)) < 0) {
		fprintf(stderr,"connect failed\n");
		exit(EXIT_FAILURE);
	}

	n = recv(sd, &valid, sizeof(char), MSG_WAITALL);	

	if (valid == 'Y') {
		// Prompt for username
		printf("Please enter a 1 - 10 character username. It may only contain\n");
		printf("upper and lower-case letters, numbers, and underscores: ");

		char username[MAX_WORD_LEN + 2];	// max length + newline + null
		fgets(username, MAX_WORD_LEN + 2, stdin);
		uint8_t nameLen = strlen(username) - 1;
		printf("You entered: %s\n", username);
		while (nameLen == 0 || username[nameLen] != '\n') {
			printf("Please try again: ");
			fgets(username, MAX_WORD_LEN + 2, stdin);
			nameLen = strlen(username) - 1;
			printf("You entered: %s\n", username);
		}
		username[nameLen] = '\0';

		send(sd, &nameLen, sizeof(uint8_t), 0);
		send(sd, &username, nameLen * sizeof(char), 0);
	} else {
		printf("The server is not allowing any more participants to join.\n");
	}

	close(sd);
	exit(EXIT_SUCCESS);
}

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
#define MAX_MSG_LEN 1000

void getUserName(char* username);
void run(int sd);

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
		fprintf(stderr,"./participant server_address server_port\n");
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
		char username[MAX_WORD_LEN + 2];	// max length + newline + null
		getUserName(username);
		uint8_t nameLen = strlen(username);

		send(sd, &nameLen, sizeof(uint8_t), 0);
		send(sd, &username, nameLen * sizeof(char), 0);

		n = recv(sd, &valid, sizeof(char), MSG_WAITALL);
		while (valid != 'Y') {
			if (valid == 'T') {
				// timer is reset; try again
				printf("%s is already taken. Please try again.\n", username);
				getUserName(username);
				nameLen = strlen(username);
				send(sd, &nameLen, sizeof(uint8_t), 0);
				send(sd, &username, nameLen * sizeof(char), 0);
				n = recv(sd, &valid, sizeof(char), MSG_WAITALL);
			} else if (valid == 'I') {
				// try again, timer not reset
				printf("%s is an invalid name. Please try again.\n", username);
				getUserName(username);
				nameLen = strlen(username);
				send(sd, &nameLen, sizeof(uint8_t), 0);
				send(sd, &username, nameLen * sizeof(char), 0);
				n = recv(sd, &valid, sizeof(char), MSG_WAITALL);
			}
		}
		run(sd);
	} else {
		printf("The server is not allowing any more participants to join.\n");
	}

	close(sd);
	exit(EXIT_SUCCESS);
}

/* getUserName
 *
 * Prompt the user for a username. When this function returns, username points
 * to the name the user entered.
 */
void getUserName(char* username) {
	printf("Please enter a 1 - 10 character username. It may only contain\n");
	printf("upper and lower-case letters, numbers, and underscores: ");

	fgets(username, MAX_WORD_LEN + 2, stdin);
	uint8_t nameLen = strlen(username) - 1;		// don't include the null
	printf("You entered: %s\n", username);
	while (nameLen == 0 || username[nameLen] != '\n') {
		// Clear fgets buffer so fgets doesn't read chars we've previously typed
		printf("Please try again: ");
		fgets(username, MAX_WORD_LEN + 2, stdin);
		nameLen = strlen(username) - 1;
		printf("You entered: %s\n", username);
	}
	username[nameLen] = '\0';
}

/* run
 *
 * Prompt the user for a message, send the message to the server, and repeat
 * indefinitely.
 */
void run(int sd) {
	while (1) {
		char message[1000];
		printf("Enter message: ");
		fgets(message, MAX_MSG_LEN + 2, stdin);

		uint16_t msgLen = strlen(message) - 1;	// don't include null
		send(sd, &msgLen, sizeof(uint16_t), 0);
		send(sd, &message, msgLen * sizeof(char), 0);
	}
}

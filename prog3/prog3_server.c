/* prog3_server.c for Program 3
 * CSCI 367
 * 2/22/18
 * William Tyas
 *
 * Description: This is the server source code file for a simple chat
 * application.
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

#define MAX_NUM_PARTICIPANTS 255
#define MAX_NUM_OBSERVERS 255
#define QLEN 6

int main(int argc, char** argv) {
	struct protoent *ptrp;		/* pointer to a protocol table entry */
	struct sockaddr_in sParAd;	/* structure to hold server's address and participant */
	struct sockaddr_in sObsAd;	/* structure to hold server's address and observer */
	struct sockaddr_in parAd;	/* structure to hold participant's address */
	struct sockaddr_in obsAd;	/* structure to hold observer's address */
	int servObsSd, servParSd, obsSd, parSd;	/* socket descriptors */
	int pport, oport;			/* protocol address */
	int obsALen, parALen;		/* length of addresses */
	int optval = 1;				/* boolean value when we set socket option */

	int numObservers = 0;
	int numParticipants = 0;

	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./server participant_port observer_port\n");
		exit(EXIT_FAILURE);
	}

	memset((char *)&sParAd,0,sizeof(sParAd));	/* clear sockaddr structure */
	sParAd.sin_family = AF_INET; 				/* set family to Internet */
	sParAd.sin_addr.s_addr = INADDR_ANY;		/* set the local IP address */

	memset((char *)&sObsAd,0,sizeof(sObsAd));	/* clear sockaddr structure */
	sObsAd.sin_family = AF_INET;				/* set family to Internet */
	sObsAd.sin_addr.s_addr = INADDR_ANY;		/* set the local IP address */

	pport = atoi(argv[1]);
	if (pport > 0) { /* test for illegal value */
		sParAd.sin_port = htons((u_short)pport);
	} else { /* print error message and exit */
		fprintf(stderr,"Error: Bad participant port number %s\n",argv[1]);
		exit(EXIT_FAILURE);
	}

	oport = atoi(argv[2]);
	if (oport > 0) { /* test for illegal value */
		sObsAd.sin_port = htons((u_short)oport);
	} else { /* print error message and exit */
		fprintf(stderr,"Error: Bad observer port number %s\n",argv[2]);
		exit(EXIT_FAILURE);
	}

	/* Map TCP transport protocol name to protocol number */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* --------------------------------------------------------------------
	 * CREATE A SOCKET FOR OBSERVERS
	 *-------------------------------------------------------------------*/
	servObsSd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (servObsSd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Allow reuse of port - avoid "Bind failed" issues */
	if( setsockopt(servObsSd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
		fprintf(stderr, "Error Setting socket option failed\n");
		exit(EXIT_FAILURE);
	}

	/* Bind a local address to the socket */
	if (bind(servObsSd, (struct sockaddr *)&sObsAd, sizeof(sObsAd)) < 0) {
		fprintf(stderr,"Error: Bind failed\n");
		exit(EXIT_FAILURE);
	}

	/* Specify size of request queue */
	if (listen(servObsSd, QLEN) < 0) {
		fprintf(stderr,"Error: Listen failed\n");
		exit(EXIT_FAILURE);
	}

	/* --------------------------------------------------------------------
	 * CREATE A SOCKET FOR PARTICIPANTS
	 *-------------------------------------------------------------------*/
	servParSd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (servParSd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Allow reuse of port - avoid "Bind failed" issues */
	if( setsockopt(servParSd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
		fprintf(stderr, "Error Setting socket option failed\n");
		exit(EXIT_FAILURE);
	}

	/* Bind a local address to the socket */
	if (bind(servParSd, (struct sockaddr *)&sParAd, sizeof(sParAd)) < 0) {
		fprintf(stderr,"Error: Bind 2 failed\n");
		exit(EXIT_FAILURE);
	}

	/* Specify size of request queue */
	if (listen(servParSd, QLEN) < 0) {
		fprintf(stderr,"Error: Listen failed\n");
		exit(EXIT_FAILURE);
	}
	
	while (1) {
		// Set up select() for incoming connection requests
		fd_set inSet;

		// Initialize set of socket descriptors
		FD_ZERO(&inSet);
		FD_SET(servParSd, &inSet);
		FD_SET(servObsSd, &inSet);

		select(FD_SETSIZE, &inSet, NULL, NULL, NULL);

		if (FD_ISSET(servParSd, &inSet)) {
			printf("A participant is waiting.\n");
			parALen = sizeof(parAd);
			if ((parSd = accept(servParSd, (struct sockaddr*)&parAd, (socklen_t*)&parALen)) < 0) {
				fprintf(stderr, "Error: Accept failed\n");
				exit(EXIT_FAILURE);
			} else {
				printf("Participant has successfully connected.\n");
			}
		} else if (FD_ISSET(servObsSd, &inSet)) {
			printf("An observer is waiting.\n");
			obsALen = sizeof(obsAd);
			if ((obsSd = accept(servObsSd, (struct sockaddr*)&obsAd, (socklen_t*)&obsALen)) < 0) {
				fprintf(stderr, "Error: Accept failed\n");
				exit(EXIT_FAILURE);
			} else {
				printf("Observer has successfully connected.\n");
			}
		} else {
			printf("No one is trying to connect.\n");
		}

/*
		close(parSd);
		close(obsSd);
		close(servParSd);
		close(servObsSd);
*/
	}
}

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

#include <assert.h>

#define MAX_NUM_PARTICIPANTS 5
#define MAX_NUM_OBSERVERS 5
#define QLEN 6

int nextAvailParticipant(int* availPars);
int nextAvailObserver(int* availObs);
int isActiveParticipant(int* activePars, int i);
int isValidName(char* username);
int canAffiliate(char* username, int* parSd);

int main(int argc, char** argv) {
	struct protoent *ptrp;		/* pointer to a protocol table entry */

	struct sockaddr_in parAddrs[255];/* structures to hold addresses of participants */
	struct sockaddr_in obsAddrs[255];/* structures to hold addresses of observers */
	int obsSds[255];			/* observer socket descriptors */
	int parSds[255];			/* participant socket descriptors */
	int availObs[255];			// boolean array of available observer sds
	int availPars[255];			// boolean array of available participant sds
	int activePars[255];		// boolean array of active participants
	struct sockaddr_in parAddr;	// temp structure to hold participant's address
	struct sockaddr_in obsAddr; // temp structure to hold observer's address
	struct sockaddr_in sParAd;	/* structure to hold server's address and participant */
	struct sockaddr_in sObsAd;	/* structure to hold server's address and observer */

	int servObsSd, servParSd;	/* socket descriptors */
	int pport, oport;			/* protocol address */
	int obsALen, parALen;		/* length of addresses */
	int optval = 1;				/* boolean value when we set socket option */
	int n;						/* return value of recv */

	int activity;				/* return value of select() */
	int curNumObservers = 0;
	int curNumParticipants = 0;
	char valid;					// Will we let client join?

	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./server participant_port observer_port\n");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < MAX_NUM_PARTICIPANTS; i++) {
		memset((char*) &parAddrs[i], 0, sizeof(parAddrs[i])); /* clear struct */
		parAddrs[i].sin_family = AF_INET;	/* set family to internet */
		parAddrs[i].sin_addr.s_addr = INADDR_ANY;	/* set local IP address */
	}

	for (int i = 0; i < MAX_NUM_OBSERVERS; i++) {
		memset((char*) &obsAddrs[i], 0, sizeof(obsAddrs[i])); /* clear struct */
		obsAddrs[i].sin_family = AF_INET;	/* set family to internet */
		obsAddrs[i].sin_addr.s_addr = INADDR_ANY;	/* set local IP address */
	}

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

	for (int i = 0; i < MAX_NUM_PARTICIPANTS; i++) {
		parSds[i] = 0;
		obsSds[i] = 0;
		availObs[i] = 1;
		availPars[i] = 1;
		activePars[i] = 0;
	}

	while (1) {
		// Set up select() for participants and observers
		fd_set inSet;
		fd_set outSet;

		FD_ZERO(&inSet);
		FD_ZERO(&outSet);

		FD_SET(servParSd, &inSet);
		FD_SET(servObsSd, &inSet);

		for (int i = 0; i < MAX_NUM_PARTICIPANTS; i++) {
			FD_SET(parSds[i], &inSet);
			FD_SET(parSds[i], &outSet);
			FD_SET(obsSds[i], &inSet);
			FD_SET(obsSds[i], &outSet);
		}

		activity = select(FD_SETSIZE, &inSet, &outSet, NULL, NULL);
		if (activity < 0) {
			fprintf(stderr, "Select error.\n");
		}
		if (FD_ISSET(servParSd, &inSet)) {	// Participant waiting to connect
			parALen = sizeof(parAddr);
			int parSd;

			if ((parSd = accept(servParSd, (struct sockaddr*)&parAddr, (socklen_t*)&parALen)) < 0) {
				fprintf(stderr, "Error: accept failed\n");
				exit(EXIT_FAILURE);
			}

			if (curNumParticipants < MAX_NUM_PARTICIPANTS) {	// Continue
				int nextParSd = nextAvailParticipant(availPars);
				assert(nextParSd >= 0);
				availPars[nextParSd] = 0;
				parSds[nextParSd] = parSd;
				parAddrs[nextParSd] = parAddr;
				valid = 'Y';
				send(parSds[nextParSd], (const void*)&valid, sizeof(char), 0);

				FD_SET(parSds[nextParSd], &inSet);
				activity = select(FD_SETSIZE, &inSet, NULL, NULL, NULL);
			} else {											// Disconnect
				valid = 'N';
				send(parSds[curNumParticipants], (const void*)&valid, sizeof(char), 0);
				printf("No more participants allowed.\n");
				close(parSd);
			}
		}
		if (FD_ISSET(servObsSd, &inSet)) {	// Observer waiting to connect
			obsALen = sizeof(obsAddr);
			int obsSd;

			if ((obsSd = accept(servObsSd, (struct sockaddr*)&obsAddr, (socklen_t*)&obsALen)) < 0) {
				fprintf(stderr, "Error: accept failed\n");
				exit(EXIT_FAILURE);
			}

			if (curNumObservers < MAX_NUM_OBSERVERS) {	// Continue
				int nextObsSd = nextAvailObserver(availObs);
				assert(nextObsSd >= 0);
				availObs[nextObsSd] = 0;
				obsSds[nextObsSd] = obsSd;
				obsAddrs[nextObsSd] = obsAddr;
				valid = 'Y';
				send(obsSds[nextObsSd], (const void*)&valid, sizeof(char), 0);

				FD_SET(obsSds[nextObsSd], &inSet);
				activity = select(FD_SETSIZE, &inSet, NULL, NULL, NULL);
			} else {											// Disconnect
				valid = 'N';
				send(obsSds[curNumObservers], (const void*)&valid, sizeof(char), 0);
				printf("No more observers allowed.\n");
				close(obsSd);
			}
		} else {		// Someone else is waiting
			for (int i = 0; i < MAX_NUM_PARTICIPANTS; i++) {
				if (FD_ISSET(parSds[i], &inSet)) {
					if (isActiveParticipant(activePars, i)) { // get ready to receive msg

					} else { // negotiating username
						uint8_t nameSize;
						n = recv(parSds[i], &nameSize, sizeof(uint8_t), MSG_WAITALL);
						char username[nameSize];			// Temporary holding spot for new user
						n = recv(parSds[i], &username, sizeof(username), MSG_WAITALL);
						username[nameSize] = '\0';
						printf("%s\n", username);

						// Verify that username is valid and is not being used already
						int valid = isValidName(username);
						char response;
						if (valid) {
							int available = isAvailablename(username);
							if (available) {
								response = 'Y';
								
								send(parSds[i], &response, sizeof(char), 0);

								// send msg to all observers

								activePars[i] = 1;		// Participant is now active

								// Associate username with parSd and store in trie
								curNumParticipants++;
							} else {
								// reset timer
								response = 'T';
								send(parSds[i], &response, sizeof(char), 0);
							}
						} else {
							// don't reset timer
							response = 'I';
							send(parSds[i], &response, sizeof(char), 0);
						}
					}
				}
			}

			for (int i = 0; i < MAX_NUM_OBSERVERS; i++) {
				if (FD_ISSET(obsSds[i], &inSet)) {
					uint8_t nameSize;
					n = recv(parSds[i], &nameSize, sizeof(uint8_t), MSG_WAITALL);
					char username[nameSize];			// Temporary holding spot for new user
					n = recv(parSds[i], &username, sizeof(username), MSG_WAITALL);
					username[nameSize] = '\0';
					printf("%s\n", username);

					// Verify that username is valid and is not being used already
					int parSd = -1;
					int valid = canAffiliate(username, &parSd);

					// Send 'Y', 'T', or 'N' to participant

					// If valid, send msg to all observers saying that user $username has joined

					// If valid, participant is now an "active participant"
					curNumObservers++;
				}
			}

			for (int i = 0; i < MAX_NUM_PARTICIPANTS; i++) {
				if (FD_ISSET(parSds[i], &outSet)) {
				}
			}

			for (int i = 0; i < MAX_NUM_OBSERVERS; i++) {
				if (FD_ISSET(obsSds[i], &outSet)) {
				}
			}
		}
/*
		close(parSd);
		close(obsSd);
		close(servParSd);
		close(servObsSd);
*/
	}
}

/* nextAvailParticipant
 *
 * Returns the next available socket descriptor for a participant.
 */
int nextAvailParticipant(int* availPars) {
	int nextAvailable = -1;
	int i = 0;
	while (i < MAX_NUM_PARTICIPANTS && availPars[i] == 0) {
		i++;
	}
	
	if (i < MAX_NUM_PARTICIPANTS) {
		nextAvailable = i;
	}
	return nextAvailable;
}

/* nextAvailObserver
 *
 * Returns the next available socket descriptor for an observer.
 */
int nextAvailObserver(int* availObs) {
	int nextAvailable = -1;
	int i = 0;
	while (i < MAX_NUM_OBSERVERS && availObs[i] == 0) {
		i++;
	}
	
	if (i < MAX_NUM_OBSERVERS) {
		nextAvailable = i;
	}
	return nextAvailable;
}

/* isActiveParticipant
 *
 * Checks to see if the participant is active or not.
 */
int isActiveParticipant(int* activePars, int i) {
	int isActive = 0;
	if (activePars[i]) {
		isActive = 1;
	}
	return isActive;
}

/* isValidName
 *
 * Checks if name includes only valid characters, and if the name is unique.
 */
int isValidName(char* username) {
	int valid = 0;
	return valid;
}

/* canAffiliate
 *
 * Checks if observer can affiliate with participant given username. If they
 * can, parSd points to the socket descriptor of the participant.
 */
int canAffiliate(char* username, int* parSd) {
	int canAffiliate = 0;
	return canAffiliate;
}

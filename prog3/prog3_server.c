/* prog3_server.c for Program 3
 * CSCI 367
 * 2/22/18
 * William Tyas
 *
 * Description: the server for a simple chat application.
 */

#include "trie.h"
#include "prog3_server.h"
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
#include <errno.h>
#include <signal.h>

#define MAX_NUM_PARTICIPANTS 5
#define MAX_NUM_OBSERVERS 5
#define QLEN 6

int main(int argc, char** argv) {
	struct protoent *ptrp;		/* pointer to a protocol table entry */
	struct sockaddr_in parAddr;	// temp structure to hold participant's address
	struct sockaddr_in obsAddr; // temp structure to hold observer's address
	struct sockaddr_in sParAd;	/* structure to hold server's address and participant */
	struct sockaddr_in sObsAd;	/* structure to hold server's address and observer */

	int servObsSd, servParSd;	/* socket descriptors for incoming connections */
	int pport, oport;			/* protocol addresses */
	int obsALen, parALen;		/* length of addresses */
	int optval = 1;				/* boolean value when we set socket option */
	int n;						/* return value of recv */

	participant_t participants[255];		// participant structs
	observer_t observers[255];				// observer structs
	int activity;				/* return value of select() */
	int curNumObservers = 0;
	int curNumParticipants = 0;
	char valid;					// Will we let client join?

	for (int i = 0; i < MAX_NUM_PARTICIPANTS; i++) {
		participant* p = malloc(sizeof(participant));
		p->used = 0;
		p->sd = 0;
		p->obsSdIndex = -1;
		memset((char*) &(p->addr), 0, sizeof(struct sockaddr_in)); // clear struct
		p->addr.sin_family = AF_INET;	// set family to internet
		p->addr.sin_addr.s_addr = INADDR_ANY;	// set local IP address
		p->active = 0;
		participants[i] = (participant_t)p;
	}

	for (int i = 0; i < MAX_NUM_OBSERVERS; i++) {
		observer* o = malloc(sizeof(observer));
		o->used = 0;
		o->sd = 0;
		o->parSdIndex = -1;
		memset((char*) &(o->addr), 0, sizeof(struct sockaddr_in)); // clear struct
		o->addr.sin_family = AF_INET;	// set family to internet
		o->addr.sin_addr.s_addr = INADDR_ANY;	// set local IP address
		observers[i] = (observer_t)o;
	}

	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./server participant_port observer_port\n");
		exit(EXIT_FAILURE);
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

	TRIE* usedNames = trie_new();

	while (1) {
		// Set up select() for participants and observers
		signal(SIGPIPE, SIG_IGN);		// SIGPIPE generated when a client closes
		fd_set inSet;
		fd_set outSet;

		setupSelect(&inSet, &outSet, servParSd, servObsSd, participants, observers);
		activity = select(FD_SETSIZE, &inSet, &outSet, NULL, NULL);
		if (activity < 0) {
			perror("Select:\n");
		}
		if (FD_ISSET(servParSd, &inSet)) {	// Participant waiting to connect
			parALen = sizeof(parAddr);
			int parSd;

			if ((parSd = accept(servParSd, (struct sockaddr*)&parAddr, (socklen_t*)&parALen)) < 0) {
				fprintf(stderr, "Error: accept failed\n");
				exit(EXIT_FAILURE);
			}

			if (curNumParticipants < MAX_NUM_PARTICIPANTS) {	// Continue
				participant_t nextAvail = NULL;
				if (nextAvailParticipant(participants, &nextAvail) == 0) {
					fprintf(stderr, "Error: nextAvailParticipant failed\n");
				}
				assert(nextAvail != NULL);
				participant* p = (participant*)nextAvail;
				p->used = 1;
				p->sd = parSd;
				p->addr = parAddr;
				valid = 'Y';
				n = send(p->sd, (const void*)&valid, sizeof(char), MSG_DONTWAIT);

				setupSelect(&inSet, &outSet, servParSd, servObsSd, participants, observers);
				activity = select(FD_SETSIZE, &inSet, NULL, NULL, NULL);
			} else {											// Disconnect
				valid = 'N';
				n = send(parSd, (const void*)&valid, sizeof(char), MSG_DONTWAIT);
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
				observer_t nextAvail = NULL;
				if (nextAvailObserver(observers, &nextAvail) == 0) {
					fprintf(stderr, "Error: nextAvailObserver failed\n");
				}
				assert(nextAvail != NULL);
				observer* o = (observer*)nextAvail;
				o->affiliated = 0;
				o->used = 1;
				o->sd = obsSd;
				o->parSdIndex = -1;
				o->addr = obsAddr;
				valid = 'Y';
				n = send(o->sd, (const void*)&valid, sizeof(char), MSG_DONTWAIT);
				setupSelect(&inSet, &outSet, servParSd, servObsSd, participants, observers);
				activity = select(FD_SETSIZE, &inSet, NULL, NULL, NULL);
			} else {											// Disconnect
				valid = 'N';
				n = send(obsSd, (const void*)&valid, sizeof(char), MSG_DONTWAIT);
				close(obsSd);
			}
		}

		// Someone else is waiting
		for (int i = 0; i < MAX_NUM_OBSERVERS; i++) {
			observer_t oType = observers[i];
			observer* o = (observer*)oType;
			if (FD_ISSET(o->sd, &inSet)) {
				uint8_t nameSize;
				n = recv(o->sd, &nameSize, sizeof(uint8_t), MSG_WAITALL);
				if (n == 0) {
					cleanupObserver(o, participants);
				}
				char username[nameSize];			// Temporary holding spot for new username
				n = recv(o->sd, &username, sizeof(username), MSG_WAITALL);
				if (n == 0) {
					cleanupObserver(o, participants);
				}
				username[nameSize] = '\0';

				// Verify that username matches an active participant and is not being used already
				int* parSdIndex = 0;
				int isAvailable = 0;
				int used = canAffiliate(username, usedNames, &parSdIndex, &isAvailable, participants, i);
				char response;
				if (!used) {
					if (isAvailable) {
						o->affiliated = 1;
						assert(*parSdIndex >= 0);
						response = 'Y';
						n = send(o->sd, &response, sizeof(char), MSG_DONTWAIT);
						o->parSdIndex = *parSdIndex;
						curNumObservers++;

						// send msg to all observers saying that a new observer has joined
						for (int j = 0; j < MAX_NUM_OBSERVERS; j++) {
							char newObsMsg[25] = "A new observer has joined";
							uint16_t msgLen = sizeof(newObsMsg);
							observer_t oType = observers[j];
							observer* o = (observer*) oType;
							if (o->used == 1) {
								n = send(o->sd, &msgLen, sizeof(uint16_t), MSG_DONTWAIT);
								n = send(o->sd, &newObsMsg, msgLen * sizeof(char), MSG_DONTWAIT);
							}
						}
						o->used = 1;
					} else {
						response = 'T';
						n = send(o->sd, &response, sizeof(char), MSG_DONTWAIT);
						// client's timer resets
					}
				} else {
					response = 'N';
					n = send(o->sd, &response, sizeof(char), MSG_DONTWAIT);
					
					// disconnect observer
					//close(obsSds[i]);
					o->used = 0;	// MAYBE?
				}
				setupSelect(&inSet, &outSet, servParSd, servObsSd, participants, observers);
				activity = select(FD_SETSIZE, &inSet, NULL, NULL, NULL);
			}
		}

		for (int i = 0; i < MAX_NUM_PARTICIPANTS; i++) {
			participant_t pType = participants[i];
			participant* p = (participant*)pType;
			if (FD_ISSET(p->sd, &inSet)) {
				if (isActiveParticipant(participants, i)) { // get ready to receive msg
					uint16_t msgLen;
					n = recv(p->sd, &msgLen, sizeof(uint16_t), MSG_WAITALL);
					if (n == 0) {
						cleanupParticipant(p, observers, usedNames);
					} else if (msgLen > 1000) {
						// disconnect on participant and any affiliated observer
						cleanupParticipant(p, observers, usedNames);
					} else {
						char message[msgLen];
						n = recv(p->sd, &message, msgLen * sizeof(char), MSG_WAITALL);

						int isPrivate, isActive;

						int recipientLen;
						isPrivate = isPrivateMsg(message, &recipientLen);
						message[msgLen] = '\0';
						if (isPrivate) {
							char recipient[recipientLen];
							char formattedMsg[msgLen + 14 - (recipientLen + 2)];
							uint16_t fMsgLen = sizeof(formattedMsg);
							processMsg(message, formattedMsg, fMsgLen, participants, usedNames, p->name, recipient, &isActive, &isPrivate);
							if (!isActive) {
								uint16_t warningLen;
								char warning[41];
								// send warning message
							} else {
								int recipientSd, senderSd;
								getPrivateMsgDestination(observers, participants, p->name, recipient, usedNames, &recipientSd, &senderSd);
								n = send(recipientSd, &fMsgLen, sizeof(uint16_t), MSG_DONTWAIT); // recipient
								n = send(senderSd, &fMsgLen, sizeof(uint16_t), MSG_DONTWAIT); // sender
								n = send(recipientSd, &formattedMsg, fMsgLen * sizeof(char), MSG_DONTWAIT);
								n = send(senderSd, &formattedMsg, fMsgLen * sizeof(char), MSG_DONTWAIT);
							}
						} else {
							char formattedMsg[msgLen + 14];
							uint16_t fMsgLen = sizeof(formattedMsg);
							processMsg(message, formattedMsg, fMsgLen, participants, usedNames, p->name, 0, &isActive, &isPrivate);
							for (int j = 0; j < MAX_NUM_OBSERVERS; j++) {
								observer_t oType = observers[j];
								observer* o = (observer*) oType;
								if (o->used == 1) {
									n = send(o->sd, &fMsgLen, sizeof(uint16_t), MSG_DONTWAIT);
									n = send(o->sd, &formattedMsg, fMsgLen * sizeof(char), MSG_DONTWAIT);
								}
							}
						}
					}
				} else { // negotiating username
					uint8_t nameSize;
					n = recv(p->sd, &nameSize, sizeof(uint8_t), MSG_WAITALL);
					char username[nameSize];			// Temporary holding spot for new user
					n = recv(p->sd, &username, sizeof(username), MSG_WAITALL);
					username[nameSize] = '\0';

					// Verify that username is valid and is not being used already
					int valid = isValidName(username);
					char response;
					if (valid) {
						int isUsed = trie_search(usedNames, username, 0);
						if (!isUsed) {
							response = 'Y';
							n = send(p->sd, &response, sizeof(char), MSG_DONTWAIT);
							p->active = 1;

							// Make sure we can get an index into the participant array given a username 
							int success = trie_add(usedNames, username, (void*)&i, sizeof(int));
							if (success < 0) {
								fprintf(stderr, "Error with adding name to list.\n");
							}
							for (int j = 0; j < sizeof(username); j++) {
								p->name[j] = username[j];
							}

							curNumParticipants++;
							// send msg to all observers

							for (int j = 0; j < MAX_NUM_OBSERVERS; j++) {
								observer_t oType = observers[j];
								observer* o = (observer*) oType;
								if (o->affiliated == 1) {
									char broadcastMsg[28];
									sprintf(broadcastMsg, "User %s has joined.\n", username);
									uint16_t msgLen = nameSize + 17;
									broadcastMsg[msgLen] = '\0';
									n = send(o->sd, &msgLen, sizeof(uint16_t), MSG_DONTWAIT);
									n = send(o->sd, &broadcastMsg, msgLen * sizeof(char), MSG_DONTWAIT);
								}
							}
						} else {
							// reset timer
							response = 'T';
							n = send(p->sd, &response, sizeof(char), MSG_DONTWAIT);
						}
					} else {
						// don't reset timer
						response = 'I';
						n = send(p->sd, &response, sizeof(char), MSG_DONTWAIT);
					}
				}
				setupSelect(&inSet, &outSet, servParSd, servObsSd, participants, observers);
				activity = select(FD_SETSIZE, &inSet, NULL, NULL, NULL);
			}
		}

		for (int i = 0; i < MAX_NUM_PARTICIPANTS; i++) {
			participant_t pType = participants[i];
			participant* p = (participant*)pType;
			if (FD_ISSET(p->sd, &outSet)) {
			}
		}

		for (int i = 0; i < MAX_NUM_OBSERVERS; i++) {
			observer_t oType = observers[i];
			observer* o = (observer*)oType;
			if (FD_ISSET(o->sd, &outSet)) {
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
 */
int nextAvailParticipant(participant_t* participants, participant_t* nextAvail) {
	int i = 0;
	participant_t pType = participants[i];
	participant* p = (participant*)pType;
	while (i < MAX_NUM_PARTICIPANTS && p->used == 1) {
		i++;
		pType = participants[i];
		p = (participant*)pType;
	}
	
	if (i < MAX_NUM_PARTICIPANTS) {
		*nextAvail = (participant_t)p;
	}
	return 1;
}

/* nextAvailObserver
 *
 */
int nextAvailObserver(observer_t* observers, observer_t* nextAvail) {
	int i = 0;

	observer_t oType = observers[i];
	observer* o = (observer*)oType;
	while (i < MAX_NUM_OBSERVERS && o->used == 1) {
		i++;
		oType = observers[i];
		o = (observer*)oType;
	}
	
	if (i < MAX_NUM_OBSERVERS) {
		*nextAvail = (observer_t)o;
	}
	return 1;
}

/* isActiveParticipant
 *
 */
int isActiveParticipant(participant_t* participants, int i) {
	participant_t pType = participants[i];
	participant* p = (participant*)pType;
	return p->active;
}

/* isValidName
 *
 */
int isValidName(char* username) {
	int valid = 1;
	int i = 0;
	while (valid && username[i] != '\0') {
		if (!((username[i] > 64 && username[i] < 91) ||
				(username[i] > 96 && username[i] < 123) ||
				(username[i] > 47 && username[i] < 58) ||
				(username[i] == 95))) {
			valid = 0;
		}
		i++;
	}
	return valid;
}

/* canAffiliate
 *
 */
int canAffiliate(char* username, TRIE* names, int** parSd, int* isAvailable, participant_t* participants, int obsSdIndex) {
	int found = trie_search(names, username, (void**)parSd);

	// Use **parSd to find participant struct
	if (found) {
		participant_t pType = participants[**parSd];
		participant* p = (participant*) pType;
		if (p->obsSdIndex < 0 && p->active) {	// Not affiliated yet, can use
			*isAvailable = 1;
			p->obsSdIndex = obsSdIndex;
		} else {
			*isAvailable = 0;
		}
	}

	return !found;
}

/* setupSelect
 *
 */
void setupSelect(fd_set* inSet, fd_set* outSet, int servParSd, int servObsSd,
					participant_t* participants, observer_t* observers) {
	FD_ZERO(inSet);
	FD_ZERO(outSet);

	FD_SET(servParSd, inSet);
	FD_SET(servObsSd, inSet);
	for (int i = 0; i < MAX_NUM_PARTICIPANTS; i++) {
		participant_t pType = participants[i];
		participant* p = (participant*)pType;
		observer_t oType = observers[i];
		observer* o = (observer*)oType;
		FD_SET(p->sd, inSet);
		FD_SET(p->sd, outSet);
		FD_SET(o->sd, inSet);
		FD_SET(o->sd, outSet);
	}
}

/* processMsg
 *
 */
void processMsg(char* message, char* formattedMsg, uint16_t newMsgLen, participant_t* participants, TRIE* names, char* sender, char* recipient, int* isActive, int* isPrivate) {
	*isActive = 0;
	*isPrivate = 0;
	if (message[0] == '@') {		// private message
		*isPrivate = 1;
		int i = 1;
		while (message[i] != ' ') {
			recipient[i-1] = message[i];	
			i++;
		}
		recipient[i-1] = '\0';

		// Check if active
		int* parSd = 0;
		int found = trie_search(names, recipient, (void**)&parSd);
		if (found) {
			participant_t pType = participants[*parSd];
			participant* p = (participant*)pType;
			if (p->active) {
				*isActive = 1;
				i = 0;
				formattedMsg[i] = '-';

				// variable number of spaces (11 - strlen(sender))
				for (i = 1; i < (11 - strlen(sender) + 1); i++) {
					formattedMsg[i] = ' ';
				}

				// sender
				int j = 0;
				for (; i < 12; i++) {
					formattedMsg[i] = sender[j];
					j++;
				}

				formattedMsg[i] = ':';
				i++;
				formattedMsg[i] = ' ';
				i++;

				// message
				j = strlen(recipient) + 2;
				for (; i < newMsgLen; i++) {
					formattedMsg[i] = message[j];
					j++;
				}

				formattedMsg[newMsgLen] = '\0';
			}
		}
	} else {
		int i = 0;
		formattedMsg[i] = '>';

		// variable number of spaces (11 - strlen(sender))
		for (i = 1; i < (11 - strlen(sender) + 1); i++) {
			formattedMsg[i] = ' ';
		}

		// sender
		int j = 0;
		for (; i < 12; i++) {
			formattedMsg[i] = sender[j];
			j++;
		}

		formattedMsg[i] = ':';
		i++;
		formattedMsg[i] = ' ';
		i++;

		// message
		j = 0;
		for (; i < newMsgLen; i++) {
			formattedMsg[i] = message[j];
			j++;
		}

		formattedMsg[newMsgLen] = '\0';
	}
}

/* getPrivateMsgDestination
 *
 */
void getPrivateMsgDestination(observer_t* observers, participant_t* participants, char* senderName, char* recipientName, TRIE* names, int* recipientSd, int* senderSd) {
	int* parSd = 0;
	trie_search(names, senderName, (void**)&parSd);
	participant_t pType = participants[*parSd];
	participant* p = (participant*)pType;
	observer_t oType = observers[p->obsSdIndex];
	observer* o = (observer*)oType;
	*senderSd = o->sd;

	trie_search(names, recipientName, (void**)&parSd);
	pType = participants[*parSd];
	p = (participant*)pType;
	oType = observers[p->obsSdIndex];
	o = (observer*)oType;
	*recipientSd = o->sd;
}

int isPrivateMsg(char* message, int* recipientLen) {
	int isPrivate = 0;
	int nameLen = 0;
	if (message[0] == '@') {
		isPrivate = 1;
		int i = 1;
		while (message[i] != ' ') {
			nameLen++;
			i++;
		}
	}
	*recipientLen = nameLen;
	return (message[0] == '@');
}

void cleanupObserver(observer* o, participant_t* participants) {
	close(o->sd);
	o->affiliated = 0;
	o->used = 0;
	o->sd = 0;

	// if affiliated with active participant, make available
	if (o->parSdIndex >= 0) {
		participant_t pType = participants[o->parSdIndex];
		participant* p = (participant*)pType;
		if (p->active) {
			p->obsSdIndex = -1;
		}
		o->parSdIndex = -1;
	}
}

void cleanupParticipant(participant* p, observer_t* observers, TRIE* usedNames) {
	close(p->sd);
	p->used = 0;
	p->sd = 0; 

	if (p->obsSdIndex >= 0) {
		observer_t oType = observers[p->obsSdIndex];
		observer* o = (observer*)oType;
		
		// disconnect observer
		close(o->sd);
		o->affiliated = 0;
		o->used = 0;
		o->sd = 0;
		o->parSdIndex = -1;
		p->obsSdIndex = -1;
	}
	p->active = 0;
	trie_del(usedNames, p->name);

	// send msg to all observers saying that a new observer has joined
	for (int j = 0; j < MAX_NUM_OBSERVERS; j++) {
		char leftMsg[14 + strlen(p->name)];
		sprintf(leftMsg, "User %s has left\n", p->name);
		int msgLen = sizeof(leftMsg);
		observer_t oType = observers[j];
		observer* o = (observer*) oType;
		if (o->used == 1) {
			send(o->sd, &msgLen, sizeof(uint16_t), MSG_DONTWAIT);
			send(o->sd, &leftMsg, msgLen * sizeof(char), MSG_DONTWAIT);
		}
	}
}

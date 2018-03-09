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

#define MAX_NUM_PARTICIPANTS 3
#define MAX_NUM_OBSERVERS 3
#define QLEN 6

#define max(x,y) ((x) > (y) ? (x) : (y)) 

int main(int argc, char** argv) {
	struct protoent *ptrp;		// pointer to a protocol table entry
	struct sockaddr_in parAddr;	// temp structure to hold participant's address
	struct sockaddr_in obsAddr; // temp structure to hold observer's address
	struct sockaddr_in sParAd;	// structure to hold server's address and participant
	struct sockaddr_in sObsAd;	// structure to hold server's address and observer

	int servObsSd, servParSd;	// socket descriptors for incoming connections
	int pport, oport;			// protocol addresses
	int obsALen, parALen;		// length of addresses
	int optval = 1;				// boolean value when we set socket option
	int n;						// return value of recv

	participant_t participants[255];	// participant structs
	observer_t observers[255];			// observer structs
	int curNumObservers = 0;
	int curNumParticipants = 0;
	int activity;				// return value of select()
	char valid;					// Will we let client join?

	for (int i = 0; i < MAX_NUM_PARTICIPANTS; i++) {
		participants[i] = NULL;
	}

	for (int i = 0; i < MAX_NUM_OBSERVERS; i++) {
		observers[i] = NULL;
	}

	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./server participant_port observer_port\n");
		exit(EXIT_FAILURE);
	}


	pport = atoi(argv[1]);
	if (pport > 0) {
		sParAd.sin_port = htons((u_short)pport);
	} else {
		fprintf(stderr,"Error: Bad participant port number %s\n",argv[1]);
		exit(EXIT_FAILURE);
	}

	oport = atoi(argv[2]);
	if (oport > 0) {
		sObsAd.sin_port = htons((u_short)oport);
	} else {
		fprintf(stderr,"Error: Bad observer port number %s\n",argv[2]);
		exit(EXIT_FAILURE);
	}

	/* Map TCP transport protocol name to protocol number */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	// create sockets to listen for participants and observers
	int success = initListeningConnection(&servObsSd, ptrp, &optval, (struct sockaddr*)&sObsAd);
	success = initListeningConnection(&servParSd, ptrp, &optval, (struct sockaddr*)&sParAd);

	TRIE* usedNames = trie_new();

	while (1) {
		signal(SIGPIPE, SIG_IGN);		// SIGPIPE generated when a client closes
		fd_set inSet;
		fd_set outSet;

		int maxFd;
		setupSelect(&inSet, &outSet, servParSd, servObsSd, participants, observers, &maxFd);
		activity = select(maxFd + 1, &inSet, &outSet, NULL, NULL);
		if (activity < 0) {
			perror("Select:\n");
		}
		if (FD_ISSET(servParSd, &inSet)) {	// Participant waiting to connect
			parALen = sizeof(parAddr);
			int parSd;
			memset((char*) &parAddr, 0, sizeof(struct sockaddr_in)); // clear struct
			parAddr.sin_family = AF_INET;	// set family to internet
			parAddr.sin_addr.s_addr = INADDR_ANY;	// set local IP address

			if ((parSd = accept(servParSd, (struct sockaddr*)&parAddr, (socklen_t*)&parALen)) < 0) {
				fprintf(stderr, "Error: accept failed\n");
				exit(EXIT_FAILURE);
			}

			if (curNumParticipants < MAX_NUM_PARTICIPANTS) {	// Continue
				participant_t nextAvail = NULL;

				if (setupNextAvailParticipant(participants, &nextAvail, parSd, &parAddr) == 0) {
					fprintf(stderr, "Error: setupNextAvailParticipant failed\n");
				}
				assert(nextAvail != NULL);
				participant* p = (participant*)nextAvail;
				valid = 'Y';
				n = send(p->sd, (const void*)&valid, sizeof(char), MSG_DONTWAIT);

				setupSelect(&inSet, &outSet, servParSd, servObsSd, participants, observers, &maxFd);
				activity = select(maxFd + 1, &inSet, NULL, NULL, NULL);
			} else {											// Disconnect
				valid = 'N';
				n = send(parSd, (const void*)&valid, sizeof(char), MSG_DONTWAIT);
				close(parSd);
			}
		}
		if (FD_ISSET(servObsSd, &inSet)) {	// Observer waiting to connect
			obsALen = sizeof(obsAddr);
			int obsSd;
			memset((char*) &obsAddr, 0, sizeof(struct sockaddr_in)); // clear struct
			obsAddr.sin_family = AF_INET;	// set family to internet
			obsAddr.sin_addr.s_addr = INADDR_ANY;	// set local IP address

			if ((obsSd = accept(servObsSd, (struct sockaddr*)&obsAddr, (socklen_t*)&obsALen)) < 0) {
				fprintf(stderr, "Error: accept failed\n");
				exit(EXIT_FAILURE);
			}

			if (curNumObservers < MAX_NUM_OBSERVERS) {	// Continue
				observer_t nextAvail = NULL;
				if (setupNextAvailObserver(observers, &nextAvail, obsSd, &obsAddr) == 0) {
					fprintf(stderr, "Error: setupNextAvailObserver failed\n");
				}
				assert(nextAvail != NULL);
				observer* o = (observer*)nextAvail;
				valid = 'Y';
				n = send(o->sd, (const void*)&valid, sizeof(char), MSG_DONTWAIT);
				setupSelect(&inSet, &outSet, servParSd, servObsSd, participants, observers, &maxFd);
				activity = select(maxFd + 1, &inSet, NULL, NULL, NULL);
			} else {											// Disconnect
				valid = 'N';
				n = send(obsSd, (const void*)&valid, sizeof(char), MSG_DONTWAIT);
				close(obsSd);
			}
		}

		// Someone else is waiting
		for (int i = 0; i < MAX_NUM_OBSERVERS; i++) {
			if (observers[i] != NULL) {
				observer* o = (observer*)observers[i];
				if (FD_ISSET(o->sd, &inSet)) {
					uint8_t nameSize;
					n = recv(o->sd, &nameSize, sizeof(uint8_t), MSG_WAITALL);
					if (n == 0) {
						cleanupObserver(o, i, observers, participants);
						curNumObservers--;
					} else {
						char username[nameSize];			// Temporary holding spot for new username
						n = recv(o->sd, &username, sizeof(username), MSG_WAITALL);
						if (n == 0) {
							cleanupObserver(o, i, observers, participants);
							curNumObservers--;
						} else {
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

									char newObsMsg[26] = "A new observer has joined\n";
									uint16_t msgLen = htons((uint16_t)sizeof(newObsMsg));
									for (int j = 0; j < MAX_NUM_OBSERVERS; j++) {
										if (observers[j] != NULL) {
											observer* o = (observer*)observers[j];
											if (o->used == 1) {
												n = send(o->sd, &msgLen, sizeof(uint16_t), MSG_DONTWAIT);
												n = send(o->sd, &newObsMsg, ntohs(msgLen) * sizeof(char), MSG_DONTWAIT);
											}
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
								cleanupObserver(o, i, observers, participants);
							}
						}
					}
					setupSelect(&inSet, &outSet, servParSd, servObsSd, participants, observers, &maxFd);
					activity = select(maxFd + 1, &inSet, NULL, NULL, NULL);
				}
			}
		}

		for (int i = 0; i < MAX_NUM_PARTICIPANTS; i++) {
			if (participants[i] != NULL) {
				participant* p = (participant*)participants[i];
				if (FD_ISSET(p->sd, &inSet)) {
					if (isActiveParticipant(participants, i)) { // get ready to receive msg
						uint16_t msgLen;
						n = recv(p->sd, &msgLen, sizeof(uint16_t), MSG_WAITALL);
						msgLen = ntohs(msgLen);
						if (n == 0) {
							cleanupParticipant(p, i, participants, observers, usedNames);
							curNumParticipants--;
						} else if (msgLen > 1000) {
							// disconnect participant and affiliated observer
							cleanupParticipant(p, i, participants, observers, usedNames);
							curNumParticipants--;
						} else {
							char message[msgLen];
							n = recv(p->sd, &message, msgLen * sizeof(char), MSG_WAITALL);
							if (n == 0) {
								cleanupParticipant(p, i, participants, observers, usedNames);
								curNumParticipants--;
							}
							message[msgLen] = '\0';
							int valid = isValidMsg(message);
							if (valid) {
								int isPrivate, isActive;
								int recipientLen;
								isPrivate = isPrivateMsg(message, &recipientLen);
								if (isPrivate) {
									char recipient[recipientLen];
									char formattedMsg[msgLen + 14 - (recipientLen + 2)];
									uint16_t fMsgLen = sizeof(formattedMsg);
									if (fMsgLen == 14) {
										printf("Private message requries a space after the recipient name.\n");
									} else {
										processMsg(message, formattedMsg, fMsgLen, participants, usedNames, p->name, recipient, &isActive, isPrivate);
										fMsgLen = htons(fMsgLen);
										if (!isActive) {	// send warning message
											char warning[32 + recipientLen];
											uint16_t warningLen = htons((uint16_t)sizeof(warning));
											int recipientSd, senderSd;
											sprintf(warning, "Warning: user %s doesn't exist...\n", recipient);
											getPrivateMsgDest(observers, participants, p->name, recipient, usedNames, &recipientSd, &senderSd);
											if (recipientSd >= 0) {
												n = send(senderSd, &warningLen, sizeof(uint16_t), MSG_DONTWAIT);
												n = send(senderSd, &warning, ntohs(warningLen) * sizeof(char), MSG_DONTWAIT);
											}
										} else {
											int recipientSd, senderSd;
											getPrivateMsgDest(observers, participants, p->name, recipient, usedNames, &recipientSd, &senderSd);
											if (recipientSd >= 0) {
												n = send(recipientSd, &fMsgLen, sizeof(uint16_t), MSG_DONTWAIT);
												n = send(recipientSd, &formattedMsg, ntohs(fMsgLen) * sizeof(char), MSG_DONTWAIT);
											}
											n = send(senderSd, &fMsgLen, sizeof(uint16_t), MSG_DONTWAIT);
											n = send(senderSd, &formattedMsg, ntohs(fMsgLen) * sizeof(char), MSG_DONTWAIT);
										}
									}
								} else {
									char formattedMsg[msgLen + 14];
									uint16_t fMsgLen = sizeof(formattedMsg);
									processMsg(message, formattedMsg, fMsgLen, participants, usedNames, p->name, 0, &isActive, isPrivate);
									fMsgLen = htons(fMsgLen);
									for (int j = 0; j < MAX_NUM_OBSERVERS; j++) {
										if (observers[j] != NULL) {
											observer* o = (observer*)observers[j];
											if (o->used == 1) {
												n = send(o->sd, &fMsgLen, sizeof(uint16_t), MSG_DONTWAIT);
												n = send(o->sd, &formattedMsg, ntohs(fMsgLen) * sizeof(char), MSG_DONTWAIT);
											}
										}
									}
								}
							} else {
								printf("Invalid message.\n");
							}

						}
					} else { // negotiating username
						uint8_t nameSize;
						n = recv(p->sd, &nameSize, sizeof(uint8_t), MSG_WAITALL);
						if (n == 0) {
							cleanupParticipant(p, i, participants, observers, usedNames);
						} else {
							char username[nameSize];			// Temporary holding spot for new user
							n = recv(p->sd, &username, sizeof(username), MSG_WAITALL);
							if (n == 0) {
								cleanupParticipant(p, i, participants, observers, usedNames);
							} else {
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
										p->name[sizeof(username)] = '\0';

										curNumParticipants++;

										// send msg to all observers
										char broadcastMsg[18 + nameSize];
										sprintf(broadcastMsg, "User %s has joined.\n", username);
										uint16_t msgLen = htons(sizeof(broadcastMsg));
										for (int j = 0; j < MAX_NUM_OBSERVERS; j++) {
											if (observers[j] != NULL) {
												observer* o = (observer*)observers[j];
												if (o->affiliated == 1) {
													n = send(o->sd, &msgLen, sizeof(uint16_t), MSG_DONTWAIT);
													n = send(o->sd, &broadcastMsg, ntohs(msgLen) * sizeof(char), MSG_DONTWAIT);
												}
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
						}
					}
					setupSelect(&inSet, &outSet, servParSd, servObsSd, participants, observers, &maxFd);
					activity = select(maxFd + 1, &inSet, NULL, NULL, NULL);
				}
			}
		}
	}
}

/* initListeningConnection
 *
 */
int initListeningConnection(int* sd, struct protoent* ptrp, int* optVal, struct sockaddr* addr) {
	*sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (*sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Allow reuse of port - avoid "Bind failed" issues */
	if( setsockopt(*sd, SOL_SOCKET, SO_REUSEADDR, optVal, sizeof(*optVal)) < 0 ) {
		fprintf(stderr, "Error Setting socket option failed\n");
		exit(EXIT_FAILURE);
	}

	/* Bind a local address to the socket */
	if (bind(*sd, (const struct sockaddr*)addr, sizeof(*addr)) < 0) {
		fprintf(stderr,"Error: Bind failed\n");
		exit(EXIT_FAILURE);
	}

	/* Specify size of request queue */
	if (listen(*sd, QLEN) < 0) {
		fprintf(stderr,"Error: Listen failed\n");
		exit(EXIT_FAILURE);
	}
	return 1;
}
/* setupSelect
 *
 */
void setupSelect(fd_set* inSet, fd_set* outSet, int servParSd, int servObsSd,
					participant_t* participants, observer_t* observers, int* maxFd) {
	int fdMax = 0;
	FD_ZERO(inSet);
	FD_ZERO(outSet);

	FD_SET(servParSd, inSet);
	fdMax = max(fdMax, servParSd);
	FD_SET(servObsSd, inSet);
	fdMax = max(fdMax, servObsSd);
	for (int i = 0; i < MAX_NUM_PARTICIPANTS; i++) {
		if (participants[i] != NULL) {
			participant* p = (participant*)participants[i];
			FD_SET(p->sd, inSet);
			FD_SET(p->sd, outSet);
			fdMax = max(fdMax, p->sd);
		}
		if (observers[i] != NULL) {
			observer* o = (observer*)observers[i];
			FD_SET(o->sd, inSet);
			FD_SET(o->sd, outSet);
			fdMax = max(fdMax, o->sd);
		}
	}

	*maxFd = fdMax;
}

/* setupNextAvailParticipant
 *
 */
int setupNextAvailParticipant(participant_t* participants, participant_t* nextAvail, int parSd, struct sockaddr_in* parAddr) {
	int success = 1;

	// Find next available participant slot
	int i = 0;
	participant_t pSlot = participants[i];
	while (i < MAX_NUM_PARTICIPANTS && pSlot != NULL) {
		i++;
		pSlot = participants[i];
	}
	
	if (i < MAX_NUM_PARTICIPANTS) {
		participant* p = malloc(sizeof(participant));
		p->used = 1;
		p->sd = parSd;
		p->obsSdIndex = -1;
		p->addr = *parAddr;
		p->active = 0;
		pSlot = (participant_t) p;
		*nextAvail = pSlot;
		participants[i] = pSlot;
	}
	return success;
}

/* setupNextAvailObserver
 *
 */
int setupNextAvailObserver(observer_t* observers, observer_t* nextAvail, int obsSd, struct sockaddr_in* obsAddr) {
	int success = 1;

	// Find next available observer slot
	int i = 0;
	observer_t oSlot = observers[i];
	while (i < MAX_NUM_OBSERVERS && oSlot != NULL) {
		i++;
		oSlot = observers[i];
	}
	
	if (i < MAX_NUM_OBSERVERS) {
		observer* o = malloc(sizeof(observer));
		o->used = 1;
		o->affiliated = 0;
		o->sd = obsSd;
		o->parSdIndex = -1;
		o->addr = *obsAddr;
		oSlot = (observer_t) o;
		*nextAvail = oSlot;
		observers[i] = oSlot;
	}
	return success;
}

/* isActiveParticipant
 *
 */
int isActiveParticipant(participant_t* participants, int i) {
	participant* p = (participant*)participants[i];
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
		participant* p = (participant*)participants[**parSd];
		if (p->obsSdIndex < 0 && p->active) {	// Not affiliated yet, can use
			*isAvailable = 1;
			p->obsSdIndex = obsSdIndex;
		} else {
			*isAvailable = 0;
		}
	}

	return !found;
}

/* isValidMsg
 *
 */
int isValidMsg(char* message) {
	int valid = 0;
	int i = 0;
	while (!valid && message[i] != '\0') {
		if (message[i] != ' ') {
			valid = 1;
		}
		i++;
	}
	return valid;
}

/* processMsg
 *
 */
void processMsg(char* message, char* formattedMsg, uint16_t newMsgLen, participant_t* participants, TRIE* names, char* sender, char* recipient, int* isActive, int isPrivate) {
	*isActive = 0;
	if (isPrivate) {		// private message
		int i = 1;
		while (message[i] != ' ' && message[i] != '\n' && message[i] != '\0') {
			recipient[i-1] = message[i];	
			i++;
		}
		recipient[i-1] = '\0';

		// Check if active
		int* parSd = 0;
		int found = trie_search(names, recipient, (void**)&parSd);
		if (found) {
			participant* p = (participant*)participants[*parSd];
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

/* getPrivateMsgDest
 *
 */
void getPrivateMsgDest(observer_t* observers, participant_t* participants, char* senderName, char* recipientName, TRIE* names, int* recipientSd, int* senderSd) {
	int* parSd = 0;
	trie_search(names, senderName, (void**)&parSd);
	participant* p = (participant*)participants[*parSd];
	if (p->obsSdIndex >= 0) {
		observer* o = (observer*)observers[p->obsSdIndex];
		*senderSd = o->sd;
	} else {
		*senderSd = -1;
	}

	trie_search(names, recipientName, (void**)&parSd);
	p = (participant*)participants[*parSd];
	if (p->obsSdIndex >= 0) {
		observer* o = (observer*)observers[p->obsSdIndex];
		*recipientSd = o->sd;
	} else {
		*recipientSd = -1;
	}
}

/* isPrivateMsg
 *
 */
int isPrivateMsg(char* message, int* recipientLen) {
	int nameLen = 0;
	if (message[0] == '@') {
		int i = 1;
		while (message[i] != ' ' && message[i] != '\n' && message[i] != '\0') {
			nameLen++;
			i++;
		}
	}
	*recipientLen = nameLen;
	return (message[0] == '@');
}

/* cleanupObserver
 *
 */
void cleanupObserver(observer* o, int oIndex, observer_t* observers, participant_t* participants) {
	close(o->sd);
	o->affiliated = 0;
	o->used = 0;
	o->sd = 0;

	// if affiliated with active participant, make available
	if (o->parSdIndex >= 0) {
		participant* p = (participant*)participants[o->parSdIndex];
		if (p->active) {
			p->obsSdIndex = -1;
		}
		o->parSdIndex = -1;
	}

	free(o);
	observers[oIndex] = NULL;
}

/* cleanupParticipant
 *
 */
void cleanupParticipant(participant* p, int pIndex, participant_t* participants, observer_t* observers, TRIE* usedNames) {
	close(p->sd);
	p->used = 0;
	p->sd = 0; 

	if (p->obsSdIndex >= 0) {
		observer* o = (observer*)observers[p->obsSdIndex];
		cleanupObserver(o, p->obsSdIndex, observers, participants);
		p->obsSdIndex = -1;
	}
	p->active = 0;
	trie_del(usedNames, p->name);

	// send msg to all observers saying that the participant has left
	char leftMsg[15 + strlen(p->name)];
	uint16_t msgLen = htons((uint16_t)sizeof(leftMsg));
	leftMsg[sizeof(leftMsg)] = '\0';
	sprintf(leftMsg, "User %s has left\n", p->name);
	for (int j = 0; j < MAX_NUM_OBSERVERS; j++) {
		if (observers[j] != NULL) {
			observer* o = (observer*)observers[j];
			send(o->sd, &msgLen, sizeof(uint16_t), MSG_DONTWAIT);
			send(o->sd, &leftMsg, ntohs(msgLen) * sizeof(char), MSG_DONTWAIT);
		}
	}
	free(p);
	participants[pIndex] = NULL;
}

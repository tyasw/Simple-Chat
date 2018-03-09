/* prog3_server.h
 * William Tyas
 *
 * Functions used in managing a chat server.
 */

#include "trie.h"
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

typedef struct participantSpot {
	int used;					// is spot being used?
	int sd;						// socket descriptor of participant
	char name[10];				// participant's name
	int obsSdIndex;				// index of observer
	struct sockaddr_in addr;	// address
	int active;					// is this an active participant?
} participant;

typedef struct participant* participant_t;

typedef struct observerSpot {
	int used;					// is spot being used?
	int affiliated;				/* is affiliated with a participant yet? Won't
								 * be in the time between observer connecting
								 * and entering participant name
								 */
	int sd;						// observer's socket descriptor
	int parSdIndex;				// index of participant observer is watching
	struct sockaddr_in addr;	// address
} observer;

typedef struct observer* observer_t;

/* initListeningConnection
 *
 */
int initListeningConnection(int* sd, struct protoent* ptrp, int* optVal, struct sockaddr* addr);

/* setupNextAvailParticipant
 *
 * Find the next participant slot that is open. Set it up.
 */
int setupNextAvailParticipant(participant_t* participants, participant_t* nextAvail, int parSd, struct sockaddr_in* parAddr);

/* setupNextAvailObserver
 *
 */
int setupNextAvailObserver(observer_t* observers, observer_t* nextAvail, int obsSd, struct sockaddr_in* obsAddr);

/* isActiveParticipant
 *
 * Checks to see if the participant is active or not.
 */
int isActiveParticipant(participant_t* participants, int i);

/* isValidName
 *
 * Checks if name includes only valid characters, and if the name is unique.
 * Valid characters are upper-case letters, lower-case letters, numbers, and
 * underscores. Whitespace is not allowed.
 */
int isValidName(char* username);

/* canAffiliate
 *
 * Checks if observer can affiliate with participant given username. If they
 * can, parSd points to the index in parSds of the participant.
 */
int canAffiliate(char* username, TRIE* names, int** parSd, int* isAvailable, participant_t* participants, int obsSdIndex);

/* setupSelect
 *
 * Reset the file descriptors we want select to monitor
 */
void setupSelect(fd_set* inSet, fd_set* outSet, int servParSd, int servObsSd,
					participant_t* parSds, observer_t* obsSds, int* maxFd);

/* isValidMsg
 *
 * message		The message
 *
 * Checks to see if the message sent by a participant follows the proper
 * conventions. There should be at least one non-space character in the message.
 * Message should be null-terminated when passed in to this function.
 */
int isValidMsg(char* message);

/* processMsg
 *
 * Process a message sent by a participant. First, check if the recipient is
 * private. Set isPrivate if it is. Then, check if the recipient is active, and
 * set isActive accordingly. Finally, format the message so that it looks pretty.
 */
void processMsg(char* message, char* formattedMsg, uint16_t newMsgLen, participant_t* participants, TRIE* names, char* sender, char* recipient, int* isActive, int isPrivate);


/* getPrivateMsgDest
 *
 * observers				The list of observers
 * participants				The list of participants
 * recipientName			The name of the participant the message is intended for
 * senderName				Which participant sent the private message
 * names					The list of participant names
 * recipientObserverSd		A pointer to the socket descriptor of the observer watching the recipient
 * senderObserverSd			pointer to the socket descriptor of the observer watching the sender
 *
 * Get the socket desciptors of the observers we want to send a private message
 * to. If the recipient is valid, both 
 */
void getPrivateMsgDest(observer_t* observers, participant_t* participants, char* senderName, char* recipientName, TRIE* names, int* recipientObserverSd, int* senderObserverSd);

/* isPrivateMsg
 *
 * message				The message
 * recipientLen			Pointer to the length of the recipient's name
 *
 * Checks if a message is private. It returns the length of the name of the
 * recipient of the message.
 */
int isPrivateMsg(char* message, int* recipientLen);

/* cleanupObserver
 *
 */
void cleanupObserver(observer* o, int oIndex, observer_t* observers, participant_t* participants);

/* cleanupParticipant
 *
 */
void cleanupParticipant(participant* p, int pIndex, participant_t* participants, observer_t* observers, TRIE* usedNames);

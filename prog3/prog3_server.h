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

/* nextAvailParticipant
 *
 * Returns the next available socket descriptor for a participant.
 */
int nextAvailParticipant(participant_t* participants, participant_t* nextAvail);

/* nextAvailObserver
 *
 * Returns the next available socket descriptor for an observer.
 */
int nextAvailObserver(observer_t* observers, observer_t* nextAvail);

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
					participant_t* parSds, observer_t* obsSds);

/* processPublicMsg
 *
 * message				The message a participant sent the server
 * newMsgLen			length of the formatted message
 * formattedMsg			The formatted message we will send
 * username				Who sent us the message
 *
 * Prepend characters to a public message so that the sender of the message
 * is identified, and the message looks pretty.
 */
int processPublicMsg(char* message, uint16_t newMsgLen, char* formattedMsg, char* username);

/* processMsg
 *
 * Process a message sent by a participant. First, check if the recipient is
 * private. Set isPrivate if it is. Then, check if the recipient is active, and
 * set isActive accordingly. Finally, format the message so that it looks pretty.
 */
void processMsg(char* message, char* formattedMsg, uint16_t newMsgLen, participant_t* participants, TRIE* names, char* sender, char* recipient, int* isActive, int* isPrivate);


/* getPrivateMsgDestination
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
void getPrivateMsgDestination(observer_t* observers, participant_t* participants, char* senderName, char* recipientName, TRIE* names, int* recipientObserverSd, int* senderObserverSd);

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
void cleanupObserver(observer* o, participant_t* participants);

/* cleanupParticipant
 *
 */
void cleanupParticipant(participant* p, observer_t* observers, TRIE* usedNames);

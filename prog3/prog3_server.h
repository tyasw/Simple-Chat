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
int canAffiliate(char* username, TRIE* names, int** parSd, participant_t* participants);

/* setupSelect
 *
 * Reset the file descriptors we want select to monitor
 */
void setupSelect(fd_set* inSet, fd_set* outSet, int servParSd, int servObsSd,
					participant_t* parSds, observer_t* obsSds);

/* processPublicMsg
 *
 * Prepend characters to a public message so that the sender of the message
 * is identified, and the message looks pretty.
 */
int processPublicMsg(char* message, uint16_t newMsgLen, char* formattedMsg, char* username);

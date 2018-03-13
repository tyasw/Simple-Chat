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

#include <errno.h>

typedef struct participantSpot {
	int used;					// is spot being used?
	int sd;						// socket descriptor of participant
	char name[10];				// participant's name
	int obsSdIndex;				// index of observer
	struct sockaddr_in addr;	// address
	int active;					// is this an active participant?
	struct timeval timeLeft;
} participant;

typedef struct participant* participant_t;

typedef struct observerSpot {
	int affiliated;				/* affiliated with a participant yet? Won't
								 * be in the time between observer connecting
								 * and entering participant name
								 */
	int sd;						// observer's socket descriptor
	int parSdIndex;				// index of participant observer is watching
	struct sockaddr_in addr;	// address
	struct timeval timeLeft;
} observer;

typedef struct observer* observer_t;

/* initListeningConnection
 *
 * sd			pointer to the socket descriptor we're listening to
 * ptrp			protocol (e.g. TCP) struct	
 * addr			address struct
 *
 * Set up a new connection. Sd will point to the socket descriptor we set up
 * the connection on.
 */
void initListeningConnection(int* sd, struct protoent* ptrp, int* optVal, struct sockaddr* addr);

/* pickMinTime
 *
 * participants			list of participants
 * observers			list of observers
 * minP					pointer to participant with the smallest time left
 * minO					pointer to observer with the smallest time left
 * pIndex				pointer to index in the participants array of minP
 * oIndex				pointer to index in the observers array of minO
 *
 * Choose the client with the smallest timeout left. If the client with the
 * smallest time left is a participant, minP will point to the struct
 * representing that participant, pIndex will point to the index in the
 * participants array where minP is located, and both minO and oIndex will
 * point to NULL. Something similar will happen if the client with the smallest
 * time left is an observer.
 */
time_t pickMinTime(participant_t* participants, observer_t* observers, participant_t* minP, observer_t* minO, int* pIndex, int* oIndex);

/* updateTimeRemaining
 *
 * participants			list of participants
 * observers			list of observers
 * elapsed				amount of time elapsed since select was called
 * names				the names of the participants, needed for cleanup of
 *							clients we disconnect
 * 
 * After we return from select, we check to see how much time has elapsed since
 * we called select. This function decrements the time left for each participant
 * and observer that is currently being timed by that amount.
 */
void updateTimeRemaining(participant_t * participants, observer_t* observers, time_t elapsed, TRIE* names);

/* setupNextAvailParticipant
 *
 * participants			list of participants
 * nextAvail			pointer to next available slot
 * parSd				the socket descriptor of the participant
 * parAddr				pointer to an address struct for the participant
 *
 * Find the next participant slot that is available. Set it up.
 */
int setupNextAvailParticipant(participant_t* participants, participant_t* nextAvail, int parSd, struct sockaddr_in* parAddr);

/* setupNextAvailObserver
 *
 * observers			list of observers
 * nextAvail			pointer to next available slot
 * obsSd				the socket descriptor of the observer
 * obsAddr				pointer to an address struct for the observer 
 *
 * Find the next observer slot that is available. Set it up.
 */
int setupNextAvailObserver(observer_t* observers, observer_t* nextAvail, int obsSd, struct sockaddr_in* obsAddr);

/* isValidName
 *
 * username 			the name
 *
 * Checks if name includes only valid characters, and if the name is unique.
 * Valid characters are upper-case letters, lower-case letters, numbers, and
 * underscores. Whitespace is not allowed.
 */
int isValidName(char* username);

/* canAffiliate
 *
 * username				the username
 * names				names in use already
 * parIndex				a double pointer to the index of the participant in the
 *							participants array
 *
 * Checks if observer can affiliate with participant given username. If they
 * can, parIndex points to the index in participants of the participant.
 */
int canAffiliate(char* username, TRIE* names, int** pIndex, int* isAvailable, participant_t* participants, int obsSdIndex);

/* setupSelect
 *
 * inSet				set of file descriptors we want to monitor input on
 * servParSd			socket descriptor we accept new particpant connections
 *							on
 * servObsSd			socket descriptor we accept new observer connections on
 * participants			list of participants
 * observers			list of observers
 * maxFd				pointer to the max file descriptor we'll monitor
 *
 * Set the file descriptors we want select to monitor.
 */
void setupSelect(fd_set* inSet, int servParSd, int servObsSd,
					participant_t* participants, observer_t* observers, int* maxFd);

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
 * message				the message
 * fMsg					the formatted message
 * fMsgLen				length of fMsg
 * participants			list of participants
 * names				names already in use
 * sender				name of sender
 * recipient			name of recipient
 * isActive				pointer to boolean indicating whether message is being
 *							sent to an active participant
 * isPrivate			pointer to boolean indicating whether message is private
 *
 * Process a message sent by a participant. First, check if the recipient is
 * private. Set isPrivate if it is. Then, check if the recipient is active, and
 * set isActive accordingly. Finally, format the message so that it looks pretty.
 */
void processMsg(char* message, char* fMsg, uint16_t fMsgLen, participant_t* participants, TRIE* names, char* sender, char* recipient, int* isActive, int isPrivate);


/* getPrivateMsgDest
 *
 * observers				The list of observers
 * participants				The list of participants
 * recipientName			The name of the participant the message is intended for
 * senderName				Which participant sent the private message
 * names					The list of participant names
 * recipientSd				pointer to socket descriptor of observer watching the recipient
 * senderSd					pointer to socket descriptor of observer watching the sender
 *
 * Get the socket desciptors of the observers we want to send a private message
 * to. If the recipient is valid, the values recipientSd and senderSd are
 * pointing to are set to the appropriate socket descriptors.
 */
void getPrivateMsgDest(observer_t* observers, participant_t* participants, char* senderName, char* recipientName, TRIE* names, int* recipientSd, int* senderSd);

/* isPrivateMsg
 *
 * message				The message
 * recipientLen			Pointer to the length of the recipient's name
 *
 * Checks if a message is private. It returns a boolean indicating whether
 * the name is private. RecipientLen will point to the length of the
 * recipient's name.
 */
int isPrivateMsg(char* message, int* recipientLen);

/* cleanupObserver
 *
 * o				pointer to the observer
 * oIndex			index in the observers array that o is from
 * observers		list of observers
 * participants		list of participants
 *
 * Disconnect on an observer. Make a spot available in the observers array.
 */
void cleanupObserver(observer* o, int oIndex, observer_t* observers, participant_t* participants);

/* cleanupParticipant
 *
 * p				pointer to the participant
 * pIndex			index in the participants array that p is from
 * participants		list of participants
 * observers		list of observers
 * names			names in use currently
 *
 * Disconnect on a participant and any affiliated participant. Make a spot
 * available in the participants array.
 */
void cleanupParticipant(participant* p, int pIndex, participant_t* participants, observer_t* observers, TRIE* names);

#include <cmsis_os2.h>
#include "general.h"

// add any #includes here
#include <string.h>

// add any #defines here

// might conflict with final.c
#define MAX_GENERALS 7

#define MAX_TRAITORS 2

// max number of messages per recursion level
#define MAX_MSGS0 1 
#define MAX_MSGS1 5
#define MAX_MSGS2 4

// message sizes per recursion level
#define MSG_SIZE0 4
#define MSG_SIZE1 6
#define MSG_SIZE2 8

// add global variables here

// keep track of generals that messages have been received from (per general) in 2D array? 
int receivedFrom[MAX_GENERALS][MAX_TRAITORS];

// id of reporter 
uint8_t rep; 
// id of sender
uint8_t sender;
// number of generals ... might conflict with definition in final.c 
uint8_t n;
// number of traitors 
uint8_t m = 0;

// level of recursion
uint8_t recurse[MAX_GENERALS];

// will store traitors 
uint8_t traitors[MAX_TRAITORS];
bool isFromBroadcast[MAX_GENERALS];

// queue array 
osMessageQueueId_t queues[MAX_GENERALS][MAX_TRAITORS+1];
osSemaphoreId_t done[MAX_GENERALS];
osSemaphoreId_t ready[MAX_GENERALS];

// osSemaphoreId_t ready, done;

/** Record parameters and set up any OS and other resources
  * needed by your general() and broadcast() functions.
  * nGeneral: number of generals
  * loyal: array representing loyalty of corresponding generals
  * reporter: general that will generate output
  * return true if setup successful and n > 3*m, false otherwise
  */
bool setup(uint8_t nGeneral, bool loyal[], uint8_t reporter) {
	// store number of generals globally 
	n = nGeneral; 
	
	// find number of traitors and store indices globally 
	for(int i = 0; i < n; i++) {
		if(!loyal[i]) {
			traitors[m] = i;
			m++;
		}
	}		
	
	// assert n > 3*m before proceeding 
	if(!c_assert(n > 3*m)) {	
		// reset m (since it is global)
		m = 0;
		return false;
	}
	
	// generals communicate via message queues 
	for(int i = 0; i < n; i++) {
		done[i] = osSemaphoreNew(1, 0, NULL);
		ready[i] = osSemaphoreNew(1, 0, NULL);
		for(int j = 0; j < m+1; j++) {
			queues[i][j] = osMessageQueueNew(MAX_MSGS0, 10, NULL);
		}
	}
	
	// store reporter index globally 
	rep = reporter;
	
	return true;
}


/** Delete any OS resources created by setup() and free any memory
  * dynamically allocated by setup().
  */
void cleanup(void) {
	// delete semaphores 
	//osSemaphoreDelete(ready);
	//osSemaphoreDelete(done);
	
	// delete message queues
	for(int i = 0; i < n; i++) {
		osSemaphoreDelete(done[i]);
		for(int j = 0; j < m+1; j++) 
			osMessageQueueDelete(queues[i][j]);
	}
	
	// reset number of traitors 
	m = 0; 
}


/** This function performs the initial broadcast to n-1 generals.
  * It should wait for the generals to finish before returning.
  * Note that the general sending the command does not participate
  * in the OM algorithm.
  * command: either 'A' or 'R'
  * sender: general sending the command to other n-1 generals
  */
void broadcast(char command, uint8_t commander) {	
	// store sender index globally for reference 
	sender = commander;
		
	// prepare initial message for queues 
	char message[MSG_SIZE0], pref[3];
	pref[0] = '0' + commander;
	pref[1] = ':';
	pref[2] = '\0';
	
	// find if sender is loyal
	bool senderLoyal = true;
	for(int i = 0; i < m; i++) {
		if(commander == traitors[i]) 
			senderLoyal = false;
	}

	// case 1: sender is loyal
	if(senderLoyal) {
		for(int i = 0; i < n; i++) {
			strcpy(message, pref);
			// don't send to self!
			if(i != commander) {
				strcat(message, &command);
				// send message to generals via message queues 
				osMessageQueuePut(queues[i][m], &message, 0, 0); 
			}
		}
	}
	
	// case 2: sender is a traitor
	else {
		for(int i = 0; i < n; i++) {
			// strcpy rewrites message 
			strcpy(message, pref);
			if(i != commander) {
				// send 'R' to even-numbered generals and 'A' to odd-numbered
				i % 2 == 0 ? strcat(message, "R") : strcat(message, "A");
				
				// put message in queue 
				osMessageQueuePut(queues[i][m], &message, 0, 0); 
			}
		}
	}
	
	// signal semaphore when command ready 
	// osSemaphoreRelease(ready);
	
	// wait semaphores until OM done 
	for(int i = 0; i < n; i++) {
		if(i != commander) osSemaphoreAcquire(done[i], osWaitForever);
	}
	
	// newline to be nice 
	printf("\n");
}

void om(uint8_t lvl, char msg[], int id) {
	// strings store sent and received messages
	char pref[3] = " ", send[MSG_SIZE2] = " ", received[MSG_SIZE2] = " ";
	
	// preface 
	pref[0] = '0' + id;
	pref[1] = ':';
	pref[2] = '\0';
	
	// check if loyal
	bool isLoyal = true;
	for(int i = 0; i < m; i++) {
		if(id == traitors[i]) 
			isLoyal = false;
	}
	
	// final recursion level OM(0): just print. 
	if(lvl == 0) {
		// print if reporter
		if(id == rep) {
			printf(" %s ", msg);
		}
	}
	
	// m > 0
	else {
		for(int i = 0; i < n; i++) {
			// check if general id is in msg using strchr
			if(i != id && strchr(msg, '0' + i) == NULL) {
				// message begins with "id:"
				strcpy(send, pref);
				
				// if loyal just append received message
				if(isLoyal) strcat(send, msg);
				
				// traitor 
				else {
					// copy all of message characters except last 
					strncat(send, msg, strlen(msg)-1);
					
					// even-numbered sends 'R', odd sends 'A'
					id % 2 == 0 ? strcat(send, "R") : strcat(send, "A");
				}
				
				// send message to lower level
				osMessageQueuePut(queues[i][lvl-1], send, 0, 0);

				// get messages 
				osMessageQueueGet(queues[id][lvl-1], received, NULL, osWaitForever);
				
				// recurse 
				om(lvl-1, received, id);
			}
		}
	}
}


/** Generals are created before each test and deleted after each
  * test.  The function should wait for a value from broadcast()
  * and then use the OM algorithm to solve the Byzantine General's
  * Problem.  The general designated as reporter in setup()
  * should output the messages received in OM(0).
  * idPtr: pointer to general's id number which is in [0,n-1]
  */
void general(void *idPtr) {	
	// dereference ptr to grab id 
	uint8_t id = *(uint8_t *)idPtr;
	
	// prepare initial message for queues 
	char received[MSG_SIZE0];
	
	// get original message from queue
	osMessageQueueGet(queues[id][m], &received, NULL, osWaitForever);
	
	// number of recursion levels
	int recurse = m; 
	
	// call recursive OM algorithm
	om(recurse, received, id);
	
 	osSemaphoreRelease(done[id]);
}

// OM algorithm 
/*

Andrew's suggested approach from help session:

if m == 0:
	if id == r:
		print msg

else (m > 0): 
	command = "id:0:R"
	send to other generals
	receive msgs from other generals
	for each received msg rm:
		om(m-1, rm, id)

*/

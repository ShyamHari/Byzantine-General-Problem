//Name: Shyam Hari 
//ID: 20768444
//This project is dedicated to Professor Igor Ivkovic a professor who I shall never forget for igniting my passion in software

#include <cmsis_os2.h>
#include "general.h"

// add any #includes here
#include <stdio.h> 
#include <stdlib.h> 
#include <stdbool.h> 
#include <stdint.h> 
#include <string.h>

// add any #defines here
//Assuming maximum number of generals is 7
#define MAX_COUNT 7 
#define MAX_RECURSION_LEVELS 3 

uint8_t reporter_id = 0; 
uint8_t traitor_count = 0; 
uint8_t general_count = 0;

bool loyal_copy[MAX_COUNT];

uint8_t commander_id; 
osSemaphoreId_t barrier; 
osMutexId_t printMute; 
uint8_t count = 0; 
osMessageQueueId_t general_messages[MAX_COUNT][MAX_RECURSION_LEVELS]; 
uint8_t doneCount = 0; 
osMutexId_t doneMute; 

bool setup(uint8_t nGeneral, bool loyal[], uint8_t reporter) {
	if(nGeneral > 8 || nGeneral == 0){ 
		return false; 
	}
	for(uint8_t i = 0; i <  nGeneral; i++){
		if(loyal[i] == false){ 
			traitor_count++; 
			loyal_copy[i] = false;  
		}
		else
		{
			loyal_copy[i] = true; 
		}
	}
	general_count = nGeneral; 
	reporter_id = reporter; 
	if(!c_assert(general_count > 3*traitor_count)){
		memset(loyal_copy, 0, MAX_COUNT*sizeof(int)); 
		general_count = 0; 
		traitor_count = 0; 
		return false; 
	}		
	barrier = osSemaphoreNew(general_count, general_count, NULL);
	printMute = osMutexNew(NULL);
	doneMute = osMutexNew(NULL);
	for(uint8_t i = 0; i < general_count; i++)
	{
		for(uint8_t j = 0; j < MAX_RECURSION_LEVELS; j++)
		{
			if(j == 0){
				general_messages[i][j] = osMessageQueueNew(1, sizeof(char)*4, NULL);
			}
			else
			{
				general_messages[i][j] = osMessageQueueNew(10, sizeof(char)*8, NULL);
			}
			if(general_messages[i][j] == NULL)
			{
				return false; 
			}
		}
	}
	return true;
}

void cleanup(void) {
	for(uint8_t i = 0; i < general_count; i++)
	{
		for(uint8_t j = 0; j < MAX_RECURSION_LEVELS; j++)
		{
			osStatus_t deletion = osMessageQueueDelete(general_messages[i][j]);
			if(deletion != osOK){ 
				printf("Deletion did not go as expected"); 
				return; 
			} 
		}
	}
	general_count = 0; 
	reporter_id = 0; 
	memset(loyal_copy, NULL, MAX_COUNT*sizeof(int)); 
	traitor_count = 0; 
	osMutexDelete(printMute);
	osMutexDelete(doneMute);
	count = 0; 
	doneCount = 0;
	osSemaphoreDelete(barrier); 
}

void broadcast(char command, uint8_t commander) {
	//commander does not participate in OM algorithm 
	commander_id = commander;
	bool disloyal_comm = 0; 
	//check if the commander is a traitor to alter the message 
	if(loyal_copy[commander] == false){
			disloyal_comm = 1; 
	} 
	osDelay(100); 
	for(uint8_t i = 0; i < general_count; i++){
		//Do not send to itself 
		if(i != commander){ 
			if(disloyal_comm){
				command = (i%2) ? 'A':'R'; 
			}
			char msg[4];
			snprintf(msg, 4, "%d:%c", commander, command); 
			osStatus_t status = osMessageQueuePut(general_messages[i][0], &msg, NULL, osWaitForever);
			if(status != osOK){ 
					printf("Message sent to general[%d] improperly", i); 
			} 
		}
		count++;
	}
	
	osSemaphoreAcquire(barrier, osWaitForever);
	osSemaphoreRelease(barrier);
}
void om(uint8_t traitorNum, char* msg, uint8_t id){
		if(traitorNum == 0){
			if(id == reporter_id){
				osMutexAcquire(printMute, osWaitForever);
				printf("%s ", msg);
				osMutexRelease(printMute);
				return;
			}
			return;
		}
		else
		{
			//Have traitor number so can identify required length of string 
			//For each new level of recursion the size of the message increases by 3
			char newMessage[strlen(msg)+3];
			snprintf(newMessage, strlen(msg)+3, "%d:%s", id, msg);
			//Check if this Id is a traitor, if it is change the end of the message 
			if(loyal_copy[id] == false){
				//If its even change the end of the message to R
				newMessage[strlen(newMessage)-1] = (id%2) ? 'A':'R';
			}
			for(uint8_t i = 0; i < general_count; i++){
				char general_sent[2];
				//check if message has been sent to general i before
				sprintf(general_sent, "%d", i);
				if(i != id && i != commander_id && strstr(newMessage, general_sent) == NULL){
					osStatus_t check = osMessageQueuePut(general_messages[i][traitorNum], &newMessage, NULL, osWaitForever);;
				}
			}
			for(uint8_t i = 0; i < general_count; i++){
				char general_sent[2];
				//check if message has been sent to general i before
				sprintf(general_sent, "%d", i);
				if(i != id && i != commander_id && strstr(newMessage, general_sent) == NULL){
					char newMessageGet[strlen(msg)+3];
					osStatus_t checkStatus =  osMessageQueueGet(general_messages[i][traitorNum], &newMessageGet, NULL, osWaitForever);
					if(checkStatus == osOK)
					{
						om(traitorNum-1,  newMessageGet, i);
					}
				}
			}
		}			
}

void general(void *idPtr) {
	osSemaphoreAcquire(barrier, osWaitForever);
	uint8_t id = *(uint8_t *)idPtr;
	while(1){ 
		while(general_count != count){}
			char msg[4]; 
			osStatus_t status = osMessageQueueGet(general_messages[id][0], &msg, NULL, 0U);
			//Commander does not participate in OM
			if(status == osOK && id != commander_id){  
				om(traitor_count, msg, id); 
			} 
			osMutexAcquire(doneMute, osWaitForever);
			doneCount++;
			osMutexRelease(doneMute);
			while(doneCount != general_count){}
			osSemaphoreRelease(barrier);
	} 
}

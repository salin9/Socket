#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <vector>
#include <stdlib.h>

#include "rpc.h"		// for definition of ARG_CHAR ... in getSize
#include "message.h"
using namespace std;

/*///////////////////////////////////////////////////
Error Types:
	-300 : ERROR: failed to send message
	-301 : ERROR: failed to receive message
*////////////////////////////////////////////////////

/*
	int getLength(int input);
	
	Decode input. If array, return length. If scalar, return 1
*/
int getLength(int input){	
	int arrayflag = (input & 65535);		// check if we need an array
	int scalar = (arrayflag==0)? 1: arrayflag;	
	return scalar;
}

bool isArray(int input){
	int arrayflag = (input & 65535);		// check if we need an array
	bool result = (arrayflag == 0) ? false : true;	
	return result;
}


int getType(int input){
	return ((input >> 16) & 255);
}

/*
	int getSize(int input);
	
	Decode input and works as the same function as sizeof(input)
*/
int getSize(int input){
	int type = getType(input);
	
	switch (type){
		case ARG_CHAR:
			return sizeof(char);
		case ARG_SHORT:
			return sizeof(short);
		case ARG_INT:
			return sizeof(int);
		case ARG_LONG:
			return sizeof(long);
		case ARG_DOUBLE:
			return sizeof(double);
		case ARG_FLOAT:
			return sizeof(float);
		default:
			return 0;
	}
}


/*
	int sendStringMessage(int sockfd, string words);
	
	Purpose: send an string, words, to sockfd
	
	1. send string size htonl(size)
	2. send the string htonl(words)
	
 */
int sendStringMessage(int sockfd, string words){
	// first send the length of string
	int words_len = words.length() + 1;
	int words_len_htonl = htonl(words_len);
	int byteSent = send(sockfd, &words_len_htonl, sizeof(int), 0);
	if (byteSent <= 0) 
		return (-300);
	
	// then send string
	char* sendWords = (char*)words.c_str();
	int byteLeft = words_len;	// bytes left to be sent
	while (byteLeft > 0){
		byteSent = send(sockfd, sendWords, byteLeft, 0);
		if (byteSent < 0)
			return (-300);
		sendWords += byteSent;
		byteLeft -= byteSent;		
	}

	return words_len;
 }

 /*
	int sendIntMessage(int sockfd, int words);
	
	Purpose: send an integer, words, to sockfd
	
	1. send integer size htonl(size)
	2. send the integer htonl(words)
	
 */
 int sendIntMessage(int sockfd, int words){
	 // first send the size of words
	int words_len = sizeof(int) ;
	int words_len_htonl = htonl(words_len);
	int byteSent = send(sockfd, &words_len_htonl, sizeof(int), 0);
	if (byteSent <= 0) 
		return (-301);
	
	// then send integer
	int sendInt = htonl(words);
	byteSent = send(sockfd, &sendInt, sizeof(int), 0);
	if (byteSent < 0)
		return (-301);

	return words_len;
	 
 }
 
 /*
	int sendArrayMessage(int sockfd, string words);
	
	Purpose: send a 0-terminated integer array, words, to sockfd
	
	1. keep sending until we reach terminate-signal 0
	
 */ 
int sendArrayMessage(int sockfd, int* words){
	int byteSent;
	while (1){
		int sendInt = htonl(*words);
		byteSent = send(sockfd, &sendInt, sizeof(int), 0);
		if (byteSent < 0) 
			return (-302);
		
		if (sendInt == 0) break;
		words++;		// haven't reached the end, send the next integer
	}
	return byteSent;
}

 
/*
	int sendArgsMessage(int sockfd, int* types, void ** words);
	
	Purpose: Send a void** array, words, to sockfd. 
		The type of words[i] is specified in types[i]
		also work if words[i] is an array
	
	1. keep sending words[i] until we reach types' terminate-signal 0

 */ 
 int sendArgsMessage(int sockfd, int* types, void ** words){

 	int total = 0;

 	for(int i = 0; types[i] != 0; i++){
 		int type = getType(types[i]);
 		int size = (getSize(types[i]) * getLength(types[i]));

 		char* msg = (char*) malloc(sizeof(char) * size);

    	switch(type){
            case ARG_CHAR :{
            	char *arg = (char*)malloc(size);
            	arg = (char*)(words[i]);
            	memcpy(msg, arg, size);
            	break;
            }
            case ARG_SHORT :{
            	short *arg = (short*)malloc(size);
            	arg = (short*)(words[i]);
            	memcpy(msg, arg, size);
            	break;
            }
            case ARG_INT :{
            	int *arg = (int*)malloc(size);
            	arg = (int*)(words[i]);
            	memcpy(msg, arg, size);
            	break;
            }
            case ARG_LONG :{
            	long *arg = (long*)malloc(size);
            	arg = (long*)(words[i]);
            	memcpy(msg, arg, size);
            	break;
            }
            case ARG_DOUBLE :{
            	double *arg = (double*)malloc(size);
            	arg = (double*)(words[i]);
            	memcpy(msg, arg, size);
            	break;
            }
            case ARG_FLOAT :{
            	float *arg = (float*)malloc(size);
            	arg = (float*)(words[i]);
            	memcpy(msg, arg, size);
            	break;
            }
        }

        int byteSent = send(sockfd, msg, size, 0);
        if(byteSent < 0) return -303;
        total += byteSent;
 	}

 	return total;
}
 
 
 /*
	int receiveStringMessage(int sockfd, string words);
	
	Purpose: receive an string, words, to sockfd
	
	1. receive string size htonl(size)
	2. receive the string htonl(words)
	
	*Aside: Need to delete string allocated*
	
 */
int receiveStringMessage(int sockfd, string** words){
	// first receive the length of string
	int words_len;
	bzero(&words_len, sizeof(int));
	int byteRecv = recv(sockfd, &words_len, sizeof(int), 0);
	if (byteRecv <= 0) 
		return (-305);
	

	// then receive string
	words_len = ntohl(words_len);	
	char* recvWords = new char[words_len];		// REMEMBER to DELETE it (via temp)!!!
	char* temp = recvWords;				// keep the beginning (to delete)
	
	int length = words_len;
	while (length > 0){
		bzero(recvWords, length);
		byteRecv = recv(sockfd, recvWords, length, 0);
		if (byteRecv <= 0)
			return (-305);
		length -= byteRecv;
		recvWords += byteRecv;
	}
	
	*words = new string(temp);			// REMEMBER to DELETE it after every receiveMessage!!!
	
	delete[] temp;
	
	return words_len;
}


 /*
	int receiveIntMessage(int sockfd, int words);
	
	Purpose: receive an integer, words, to sockfd
	
	1. receive integer size htonl(size)
	2. receive the integer htonl(words)
	
	*Aside: Need to delete integer allocated*
	
 */
int receiveIntMessage(int sockfd, int* words){
	// first receive the size of integer
	int words_len;
	bzero(&words_len, sizeof(int));
	
	int byteRecv = recv(sockfd, &words_len, sizeof(int), 0);
	if (byteRecv <= 0) 
		return (-306);
	

	// then receive integer
	words_len = ntohl(words_len);
	int recvInt;
	byteRecv = recv(sockfd, &recvInt, words_len, 0);
	if (byteRecv < 0)
		return (-306);
	
	*words = ntohl(recvInt);
	
	return byteRecv;
}

/*
	int receiveArrayMessage(int sockfd, string words);
	
	Purpose: receive a 0-terminated integer array, words, to sockfd
	
	1. keep receiving until we reach terminate-signal 0
	
	*Aside: Need to delete array allocated*
	
 */ 
int receiveArrayMessage(int sockfd, int** words){
	int recvInt;
	vector <int> recvWords;
	int count = 0;
	int byteSent;
	while (1){
		
		byteSent = recv(sockfd, &recvInt, sizeof(int), 0);
		if (byteSent < 0) 
			return (-307);
		
		recvInt = ntohl(recvInt);
		recvWords.push_back(recvInt);
		count++;
		
		if (recvInt == 0) break; 	// reached the end
	}
	
	int* temp = new int[count];
	for (int i=0; i<count; i++){
		temp[i] = recvWords[i];
	}
	*words = temp;
	
	
	return byteSent;
}

/*
	int receiveArgsMessage(int sockfd, int* types, void ** words);
	
	Purpose: receive a void** array, words, to sockfd. 
		The type of words[i] is specified in types[i]
		also work if words[i] is an array
	
	1. keep receiving words[i] until we reach type's terminate-signal 0 
	
	*Pre-requisite*: words has been allocated
	
 */ 

int receiveArgsMessage(int sockfd, int* types, void ** words){
	int total = 0;

	for(int i = 0; types[i] != 0; i++){
		int type = getType(types[i]);
		int size = (getSize(types[i]) * getLength(types[i]));

		char *msg = (char*) malloc(sizeof(char) * size);
		int byteRecv = recv(sockfd, msg, size, 0);
		if(byteRecv < 0) return (-308);

		switch(type){
            case ARG_CHAR :{
            	char *arg = (char*) malloc(size);
                memcpy(arg, msg, size);
                words[i] = (void*) arg;
                break;
            }
            case ARG_SHORT :{
            	short *arg = (short*) malloc(size);
                memcpy(arg, msg, size);
                words[i] = (void*) arg;
                break;
            }
            case ARG_INT :{
            	int *arg = (int*) malloc(size);
                memcpy(arg, msg, size);
                words[i] = (void*) arg;
                break;
            }
            case ARG_LONG :{
            	long *arg = (long*) malloc(size);
                memcpy(arg, msg, size);
                words[i] = (void*) arg;
                break;
            }
            case ARG_DOUBLE :{
            	double *arg = (double*) malloc(size);
                memcpy(arg, msg, size);
                words[i] = (void*) arg;
                break;
            }
            case ARG_FLOAT :{
            	float *arg = (float*) malloc(size);
                memcpy(arg, msg, size);
                words[i] = (void*) arg;
                break;
            }
        }

        total += byteRecv;
	}
	
	return total;
}

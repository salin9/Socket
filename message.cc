#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include "message.h"
using namespace std;


int sendMessage(int sockfd, void words){
	// first send the length of string
	int words_len = words.length() +1 ;
	int words_len_htonl = htonl(words_len);
	int byteSent = send(sockfd, &words_len_htonl, sizeof(int), 0);
	if (byteSent <= 0) 
		return byteSent;
	
	// then send string
	char* sendWords = (char*)words.c_str();
	int byteLeft = words_len;	// bytes left to be sent
	while (byteLeft > 0){
		byteSent = send(sockfd, sendWords, byteLeft, 0);
		if (byteSent < 0)
			return byteSent;
		sendWords += byteSent;
		byteLeft -= byteSent;		
	}

	return words_len;
 }

int receiveMessage(int sockfd, void** words, int type){
	// first receive the length of string
	int words_len;
	bzero(&words_len, sizeof(int));
	int byteRecv = recv(sockfd, &words_len, sizeof(int), 0);
	if (byteRecv <= 0) 
		return byteRecv;
	words_len = ntohl(words_len);
	
	// if type is string	
	if (type == 0){			
		char* recvWords = new char[words_len];		// REMEMBER to DELETE it (via temp)!!!
		char* temp = recvWords;				// keep the beginning (to delete)
		
		int length = words_len;
		while (length > 0){
			bzero(recvWords, length);
			byteRecv = recv(sockfd, recvWords, length, 0);
			if (byteRecv <= 0)
				return byteRecv;
			length -= byteRecv;
			recvWords += byteRecv;
		}
		
		*words = new string(temp);			// REMEMBER to DELETE it after every receiveMessage!!!
		
		delete[] temp;
	}
	// if type is integer
	else if (type == 1){
		int recvInt;
		byteRecv = recv(sockfd, recvInt, words_len, 0)
		if (byteRecv < 0)
			return byteRecv;
		**word = ntohl(recvInt);			// well, syntax right???? --------- later
	}
	// if type is an int array
	else if (type == 2){
		int* array = new int[words_len];
		int recvInt;
		for (int i=0; i<words_len; i++){
			byteRecv = recv(sockfd, recvInt, words_len, 0)
			if (byteRecv < 0)
				return byteRecv;
			array[i] = ntohl(recvInt);
		}
		*words = array;
		
	}	
	
	return words_len;
}


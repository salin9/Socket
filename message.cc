#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <vector>
#include "message.h"
using namespace std;


int sendStringMessage(int sockfd, string words){
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

 int sendIntMessage(int sockfd, int words){
	 // first send the size of words
	 cerr << "words to be sent: " << words << endl;
	int words_len = sizeof(int) ;
	int words_len_htonl = htonl(words_len);
	int byteSent = send(sockfd, &words_len_htonl, sizeof(int), 0);
	if (byteSent <= 0) 
		return byteSent;
	
	cerr << "sent words_len " << words_len <<endl;
	
	// then send integer
	int sendInt = htonl(words);
	byteSent = send(sockfd, &sendInt, sizeof(int), 0);
	if (byteSent < 0)
		return byteSent;

	return words_len;
	 
 }
 
 // keep sending until we reach terminal signal 0
int sendArrayMessage(int sockfd, int* words){
	int byteSent;
	while (1){
		int sendInt = htonl(*words);
		byteSent = send(sockfd, &sendInt, sizeof(int), 0);
		if (byteSent < 0) 
			return byteSent;
		
		if (sendInt == 0) break;
		words++;		// haven't reached the end, send the next integer
	}
	return byteSent;
}
 
 
int receiveStringMessage(int sockfd, string** words){
	// first receive the length of string
	int words_len;
	bzero(&words_len, sizeof(int));
	int byteRecv = recv(sockfd, &words_len, sizeof(int), 0);
	if (byteRecv <= 0) 
		return byteRecv;
	

	// then receive string
	words_len = ntohl(words_len);	
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
	
	return words_len;
}

int receiveIntMessage(int sockfd, int* words){
	// first receive the size of integer
	int words_len;
	bzero(&words_len, sizeof(int));
	
	int byteRecv = recv(sockfd, &words_len, sizeof(int), 0);
	if (byteRecv <= 0) 
		return byteRecv;
	

	// then receive integer
	words_len = ntohl(words_len);
	int recvInt;
	byteRecv = recv(sockfd, &recvInt, words_len, 0);
	if (byteRecv < 0)
		return byteRecv;
	
	*words = ntohl(recvInt);			// well, syntax right???? --------- later
	
	return byteRecv;
}

 // keep receiving until we reach terminal signal 0
int receiveArrayMessage(int sockfd, int** words){
	int recvInt;
	vector <int> recvWords;
	int count = 0;
	int byteSent;
	while (1){
		
		byteSent = recv(sockfd, &recvInt, sizeof(int), 0);
		if (byteSent < 0) 
			return byteSent;
		
		recvInt = ntohl(recvInt);
		recvWords.push_back(recvInt);
		count++;
		
		if (recvInt == 0) break; 	// reached the end
	}
	
	int* temp = new int[count];
	for (int i=0; i<count; i++){
		temp[i] = recvWords[i];
	}
	*words = temp;			// check ------------------ later
	
	
	return byteSent;
}


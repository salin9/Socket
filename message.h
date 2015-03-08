#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <string>

/*
int sendMessage(int sockfd, std::string words);
int receiveMessage(int sockfd, std::string** words);
*/

int sendStringMessage(int sockfd, std::string words);
int sendIntMessage(int sockfd, int words);
int sendArrayMessage(int sockfd, int* words);
int receiveStringMessage(int sockfd, std::string** words);
int receiveIntMessage(int sockfd, int* words);
int receiveArrayMessage(int sockfd, int** words);


#endif
#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <string>

bool isArray(int input);
int getLength(int input);
int getType(int input);
int getSize(int input);

int sendStringMessage(int sockfd, std::string words);
int sendIntMessage(int sockfd, int words);
int sendArrayMessage(int sockfd, int* words);
int sendArgsMessage(int sockfd, int* types, void ** words);

int receiveStringMessage(int sockfd, std::string** words);
int receiveIntMessage(int sockfd, int* words);
int receiveArrayMessage(int sockfd, int** words);
int receiveArgsMessage(int sockfd, int* types, void ** words);


#endif

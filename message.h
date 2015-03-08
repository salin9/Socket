#ifndef _MESSAGE_H_
#define _MESSAGE_H_


int sendMessage(int sockfd, void words);

// type: 0: string. 1: int. 2: array, terminated by 0
int receiveMessage(int sockfd, void** words, int type);


#endif
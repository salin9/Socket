#ifndef _BINDER_H_
#define _BINDER_H_

#include <string>
#include <vector>

#define MAX_CLIENT		10	// max simultaneous clients --- no limit. ignore the parameter "backlog" of listen
#define MAX_HOST_NAME	64	// max host name length



struct function{
	std::string* name;		// function name
	int* argTypes;			// function argument types

	function(std::string* name, int* argTypes): name(name), argTypes(argTypes) {}

	bool operator== (const function& other) const {
		if (string(*name) == (*func.name)) return false;
		int i=0;
		while (1){
			//cout  << argTypes[i] << ", " << other.argTypes[i] <<endl;
			if (argTypes[i] == 0 && other.argTypes[i] == 0) return true;
			if (argTypes[i] == 0 || other.argTypes[i] == 0) return false;
			if (argTypes[i] != other.argTypes[i]) return false;
			i++;
		}
   }
	bool operator< (const function& other) const {
		if (string(*name) > (*func.name)) return false;
		
	  	int i=0;
		while (1){
			if (argTypes[i] == 0 && other.argTypes[i] == 0) return false;
			if (argTypes[i] == 0 ) return true;
			if (other.argTypes[i] == 0) return false;
			if (argTypes[i] > other.argTypes[i]) return false;
			if (argTypes[i] < other.argTypes[i]) return true;
			i++;
		}
   }

};

struct server{
	std::string* host;	// server's host name
	int port;			// server's port number
	int sockfd;			// server's file descriptor
	int valid;			// live: 1. terminated: 0
	server(std::string* host, int port, int fd): host(host), port(port), sockfd(fd), valid(1) {}
	bool operator== (const server& other) const {
	   return (*host) == *(other.host)
			&& port == other.port
			&& sockfd == other.sockfd);
	}
};


int initializeSocket();
int printBinderInfo();
void work(int);




#endif

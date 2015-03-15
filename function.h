#ifndef _FUNCTION_H_
#define _FUNCTION_H_

#include <string>
#include <vector>
#include <sstream>

struct function{
	std::string* name;		// function name
	int* argTypes;			// function arguments' types
	//int* argLength;			// function arguments' length
	int size;
	std::string* key;		// used as functionMap key
	
	function(std::string* name, int* argTypes): name(name), argTypes(argTypes) {
		std::string temp = *name;
		
		int i=0;
		while (argTypes[i] != 0){
			i++;
		}
		
		int j = 0;
		while (j < i){
			// ignore real length. only record scalar(0) or array (1)
			int len = argTypes[j] & 65535;
			int type = len ==0 ? argTypes[j] : ((argTypes[j] & 0xFFFF0000 )| 1);
		
			std::stringstream ss;
			ss << type;		
			temp += ss.str();
			
			j++;
		}
		temp += '\0';
		
		size = temp.length();
		
 		key = new std::string[size];
		j=0;		
		while(j < size){
			key[j] = temp[j];
			j++;
		}
	}	

	bool operator== (const function& other) const {
		std::string lhs = "";
		std::string rhs = "";
		
		for (int i=0; i<this->size; i++){ lhs += key[i]; }
		for (int i=0; i<other.size; i++){ rhs += other.key[i]; }
		
		return lhs == rhs;
	}
	
	bool operator< (const function& other) const {

		std::string lhs = "";
		std::string rhs = "";
		
		for (int i=0; i<this->size; i++){ lhs += key[i];}
		for (int i=0; i<other.size; i++){ rhs += other.key[i];}
		
		return lhs < rhs;		
   }
};

struct server{
	std::string* host;	// server's host name
	int port;			// server's port number
	int sockfd;			// server's file descriptor
	server(std::string* host, int port, int fd): host(host), port(port), sockfd(fd) {}
   	bool operator== (const server& other) const {
		return ((*host).compare(*other.host) == 0
			&& port == other.port
			&& sockfd == other.sockfd);
	}
};


#endif
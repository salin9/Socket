#ifndef _FUNCTION_H_
#define _FUNCTION_H_

#include <string>
#include <vector>

struct function{
	std::string* name;		// function name
	int* argTypes;			// function arguments' types
	int* argLength;			// function arguments' length
	int size;
	
	function(std::string* name, int* argTypes): name(name), argTypes(argTypes) {
		int i=0;
		while (argTypes[i] != 0){
			i++;
		}
		size = i;
		argLength = new int[i];		// argLength has 1 element less than argTypes. - remember to delete it
		int j = 0;
		while (j < i){
			argLength[j] = argTypes[j] & 65535;
			j++;
		}
	}
	

	bool operator== (const function& other) const {
		if ((*name).compare(*other.name) != 0) return false;
		if (size != other.size) return false;
		
		int i=0;
		while (1){
			if (argTypes[i] == 0 && other.argTypes[i] == 0) return true;
			//if (argTypes[i] == 0 || other.argTypes[i] == 0) return false;	// actually, no need here
			if (argTypes[i] != other.argTypes[i]) return false;			// if different types
			
			if (( argLength[i] == 0 && other.argLength[i] > 0) 			// if same type, but one array and one scalar
					|| 
				 (argLength[i] > 0 && other.argLength[i] == 0))
				return false;			
			i++;
		}
		return true;
	}
 
	bool operator< (const function& other) const {
		int flag1 = (*name).compare(*other.name);
		// flag1 == 0: name == other.name 
		// flag1 <  0: name  < other.name
		if (flag1 > 0) return false;
		
		if (size > other.size) return false;
		
	  	int i=0;
		int flag2 = 0;	// 0: same type all the way. 1: some length > other length
		while (1){
			// if same type all the way
			if (argTypes[i] == 0 && other.argTypes[i] == 0)
				// if name = other:	flag2=0  => false
				//					flag2=1  => true
				// if name < other:	flag2=0  => true
				//					flag2=1  => true
				return (flag1 < 0 || flag2);
				
			if (argTypes[i] == 0 ) return true;
			if (other.argTypes[i] == 0) return false;
			if (argTypes[i] > other.argTypes[i]) return false;
			if (argTypes[i] < other.argTypes[i]) return true;
			
			// if same type, but different length
			if ( argLength[i] > 0 && other.argLength[i] == 0) 		// define scalar < array
				return false;
			if (argLength[i] == 0 && other.argLength[i] > 0)
				flag2 = 1;
			
			i++;
		}
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
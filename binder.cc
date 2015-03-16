#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <cstring>
#include <stdlib.h>		// exit
#include <unistd.h>		// gethostname
#include <map>
#include <vector>
//#include <utility>      // pair


#include "message.h"
#include "function.h"

using namespace std;

// max simultaneous clients. no limit. ignore the parameter "backlog" of listen
#define MAX_CLIENT		10
// max host name length				  
#define MAX_HOST_NAME	64

// store function-server map. 
// -- vector<int> indicates the index in the serverVector
map <struct function, vector<int> > functionMap;

// stores servers' information
vector<server*> serverVector;

// indicates the next server to be considered by round robin
// -- every time a server implements a service, remove it to the RRqueue's back
vector<int> RRqueue;

int init_index = 0;		// first, choose 0-th server which provides the function service


// error output.  ----- REUSE from A2
static void error(const char *err){
	cerr << err << endl;
}


// initialize socket and listen.   ----- REUSE from A2 (file: stringServer.cc) -----
static int initializeSocket(){
	int sockfd, port;
	struct sockaddr_in address;
	
	// create socket - TCP, IPv4
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) return (-200);

	// specify IP, port
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;				// IPv4
	address.sin_addr.s_addr = INADDR_ANY;		// IP address
	address.sin_port = 0;						// next available port
	
	// bind port and socket
	if (bind(sockfd, (struct sockaddr*)&address, sizeof(address)) < 0)
		return  (-201);

	// listen
	if (listen(sockfd, MAX_CLIENT) < 0)
		return (-202);
	
	return sockfd;		
}

// print the binder info: address and port.    ----- REUSE from A2 (file: stringServer.cc) -----
static int printBinderInfo(int sock){ 
	//get machine name
	char name[MAX_HOST_NAME+1];
	if (gethostname(name, MAX_HOST_NAME) < 0)
		return (-203);
	cout << "BINDER_ADDRESS " << name << endl;

	//get port number
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	if (getsockname(sock, (struct sockaddr *)&addr, &len)  < 0)
		return (-204);
	cout << "BINDER_PORT " << ntohs(addr.sin_port) << endl;
	
	return 0;
}


/*
	int handleRegister(int fd);
	
	register procedure and argument types -- from server -- in functionMap
	  -- if the (procedure, argTypes, server) already registered, send warning. (in rpc, update skeleton)
	  
	Send back success/failure message to file descriptor fd
		1.	first send REGISTER_SUCCESS or REGISTER_FAILURE
		2.	for REGISTER_SUCCESS, then send
				== 0 indicate success
				> 0  indicate warning (eg, same as some previously registered procedure)
			for REGISTER_FAILURE, then send
				< 0  indicate failure (eg, failure to receive)
  
*/
static int handleRegister(int fd ){
	
	cout << "debug: handleRegister" << endl;

	string returnMessage = "";		// indicate success or failure
	int returnValue = 0;			// indicate warning or errors, if any
	
	/* 
	
		receive server's registration information
		including server_identifier,  port,  name,  argTypes
	
	*/

	string* host;
	int port;
	string* name;
	int* argTypes;
	
	int result = receiveStringMessage(fd, &host);	// delete host later!!!	
	if ( result < 0)	{		
		if (result == (-302)) delete host;
		returnMessage = "REGISTER_FAILURE";
		returnValue = (-220);
	}

	cout << "handleRegister: received: " << *host << endl;

	if (receiveIntMessage(fd, &port) <0){
		returnMessage = "REGISTER_FAILURE";
		returnValue = (-221);
	}

	cout << "handleRegister: received: " <<  port << endl;

	result = receiveStringMessage(fd, &name);		// delete name later!!!
	if ( result <0){ 		
		if (result == (-302)) delete name;
		returnMessage = "REGISTER_FAILURE";
		returnValue = (-222);
	}

	cout << "handleRegister: received: " << *name << endl;

	if (receiveArrayMessage(fd, &argTypes) <0){ 		// delete aryTypes later!!!
		returnMessage = "REGISTER_FAILURE";
		returnValue = (-223);
	}

	cout << "handleRegister: received argTypes" << endl;


	// check if any argTypes[i] are valid
	int i = 0;
	while (argTypes[i] != 0){
		int type  = (argTypes[i] >> 16  & 255);
		if ( type < 1 || type > 6){
			returnMessage = "REGISTER_FAILURE";
			returnValue = (-224);
		}
		i++;
	}	
	
	// if any failure occurs, send back response
	if (returnMessage.compare("REGISTER_FAILURE") == 0){
		sendStringMessage(fd, returnMessage);
		sendIntMessage(fd, returnValue);
		return returnValue;		
	}
	
	/*
	
		now register the server & procedure
		build a function map where the key is the function
	 
	*/
	
	struct function tempFunction(name, argTypes);
	struct server* tempServer = new server(host, port, fd);	// keep on heap
	
	// check if server has already registered
	int flag = 0;	// =1 if found the server record
	i = 0;
	for (; i < serverVector.size(); i++){
		if ((*serverVector[i]) == *tempServer) {
			flag = 1;
			break;
		}
	}	
	
	// if the (function,argTypes) not yet registered, add it to functionMap
	// also, indicate success
	if (functionMap.count(tempFunction) == 0){
		cerr << "function not found " << *name << endl;

		vector<int> index;
		
		// this server has been registered before, but not registered with this function
		if (flag){
			index.push_back(i);	
		}
		// else, new server
		else{
			serverVector.push_back(tempServer);
			int count = serverVector.size() - 1;
			index.push_back(count);
			
			// also take a record in RRqueue
			RRqueue.push_back(count);
		}
		
		functionMap[tempFunction] = index;
		
		returnValue = 0;	// no warning
		
	}
	// if the (function,argTypes) already registered
	else{
		cerr << "function found " << *name << endl;
		
		vector<int>* index = &functionMap[tempFunction];
		
		// check if server already exist
		// if server exists, send warning -- update skeleton in rpc (later!)
		if (flag){
			// then check if serverVector[i] has registered with the function
			int flag2 = 0;
			for (int j=0; j<index->size(); j++){
				if (index->at(j) == i){
					flag2 = 1;
					returnValue = (220);		// set warning - previously registered
					break;
				}
			}
			// if the serverVector[i] NOT registered with this function
			if (!flag2){
				index->push_back(i);				
			}
		}
		// else, new server
		else{
			serverVector.push_back(tempServer);
			int count = serverVector.size() -1;
			index->push_back(count);
			
			// also take a record in RRqueue
			RRqueue.push_back(count);
			
			returnValue = 0;				// no warning
		}		
	}
	returnMessage = "REGISTER_SUCCESS";
	
	/*
	
		send back the success/failure message
	
	*/
	if (sendStringMessage(fd, returnMessage) < 0) {
		returnMessage = "REGISTER_FAILURE";
		returnValue = (-225);
		error("ERROR: binder failed to send back success/failure message");
	}
	if (sendIntMessage(fd, returnValue) < 0){
		returnMessage = "REGISTER_FAILURE";	
		returnValue = (-225);
		error("ERROR: binder failed to send back warning/error message");
	}
	
	return returnValue;
	
}


/*

	int handleRequest(int fd);

	handle request from client.
	-- find the available server providing the function, using round robin mechanism
	
	Send back success/failure message to file descriptor fd
		1.	first send LOC_SUCCESS or LOC_FAILURE
		2.	for LOC_SUCCESS, then send server-identifier and port number
			for LOC_FAILURE, then send reason code
	
*/
static int handleRequest(int fd){
	string returnMessage;
	int returnValue;
	
	string* name;
	int* argTypes;
	
	if (receiveStringMessage(fd, &name) <0){					// delete !!!
		returnMessage = "LOC_FAILURE";
		returnValue = (-222);
	}
	if (receiveArrayMessage(fd, &argTypes) <0){				// delete !!!
		returnMessage = "LOC_FAILURE";
		returnValue = (-223);
	}
	
	// if any failure occurs, send back response
	if (returnMessage.compare("LOC_FAILURE") == 0){
		sendStringMessage(fd, returnMessage);
		sendIntMessage(fd, returnValue);
		return returnValue;		
	}
	
	/*
	
		now, use round robin mechanism to find the available server, if any
		
	*/
	struct function tempFunction(name, argTypes);
	
	// if the function not found (not registered), indicate failure
	if (functionMap.count(tempFunction) == 0){
		returnMessage = "LOC_FAILURE";
		returnValue = (-240);
		
		if (sendStringMessage(fd, returnMessage) < 0){
			returnValue = (-225);
			error("ERROR: binder failed to send failure message");
		}
			
		if (sendIntMessage(fd, returnValue) < 0){
			returnValue = (-225);
			error("ERROR: binder failed to send warning/error message");
		}
			
		
	}
	// if function registered, send the available server' host name and port
	else{
		// index = index of all servers providing such a service in serverVector 
		vector <int>* index = &functionMap[tempFunction];
		int serverIndex = 0;		
		int found = 0;	// 1 indicate "found"
		for (int i=0; i < RRqueue.size(); i++){
			// if not found, keep finding
			if (!found){
				for (int j=0; j<index->size(); j++){
					if (RRqueue[i] == index->at(j)){
						found = 1;
						serverIndex = RRqueue[i];
						break;
					}
				}
			}
			// if found, move the index i to the back of RRqueue, by swapping with the previous one
			else{
				int temp = RRqueue[i];
				RRqueue[i] = RRqueue[i-1];
				RRqueue[i-1] = temp;				
			}
		}
		
		// after the loop, we must have found at least one server providing such a service		
		
		// find the index of the first available server by RR
		struct server* temp = serverVector[serverIndex];
		string* host = temp->host;
		int port = temp->port;
		
		cerr << "by RR, the next available server is " << *(temp->host) << endl;;
		
		returnMessage = "LOC_SUCCESS";
		returnValue = 0;
		
		if (sendStringMessage(fd, returnMessage) < 0){
			returnValue = (-225);
			error("ERROR: binder failed to send success message");
		}
			
		if (sendStringMessage(fd, *host) < 0){
			returnValue = (-225);
			error("ERROR: binder failed to send fd number");
		}
			
		if (sendIntMessage(fd, port) < 0){
			returnValue = (-225);
			error("ERROR: binder failed to send port number");
		}
			

	}
	
	return returnValue;
	
}

/*
	void hendleTerminate(int fd, string* words);
	
	receive terminate request from client
	-- send the terminate signal to all servers.
	-- close all sockets
	-- after all servers terminate, the binder terminates
	
*/
static void handleTerminate(int listener, string* words, fd_set &master){
	
	map <struct function, vector<int> >::iterator i;
	
	/*
		loop over the serverVector, notify all servers to terminate
	*/
	int end = serverVector.size();
	for (int i=0; i < end; i++){
		int fd = serverVector[i]->sockfd;
		sendStringMessage(fd, *words);	// ask server to terminate  -- later (REMINDER: on server side. in rpc)
		close(fd);						// close socket
		FD_CLR(fd, &master);
	}
	
	/* 
		Then terminate the binder itself
		have to do that in function - work() -
	 */
	close(listener);				// close binder's socket
	FD_CLR(listener, &master);

}


/*
	clean the serverVector and functionMap
*/
static void clean(){
	cerr << "Starting to clean ... " << endl;
	
	// try clear serverVector
	int end = serverVector.size();
	for (int i=0; i < end; i++){
		delete serverVector[i]->host;
		delete serverVector[i];
	}
	
	// try clear map
	map <struct function,  vector<int> >::iterator i;
	for (i = functionMap.begin(); i != functionMap.end(); i++){
		const struct function* Func  = &(i->first);
		functionMap.erase(i);
		delete Func->name;
		delete [] Func->argTypes;
	}
	
	cerr << "Finished cleaning. " << endl;
	
}


static int handleCache(int fd){
	string returnMessage;
	int returnValue;
	
	string* name;
	int* argTypes;
	
	if (receiveStringMessage(fd, &name) <0){					// delete !!!
		returnMessage = "CACHE_FAILURE";
		returnValue = (-222);
	}
	if (receiveArrayMessage(fd, &argTypes) <0){				// delete !!!
		returnMessage = "CACHE_FAILURE";
		returnValue = (-223);
	}
	
	// if any failure occurs, send back response
	if (returnMessage.compare("CACHE_FAILURE") == 0){
		sendStringMessage(fd, returnMessage);
		sendIntMessage(fd, returnValue);
		return returnValue;		
	}
	
	/*
		now, we have function signature. need to send back all servers providing such service
	*/
	
	struct function tempFunction(name, argTypes);
	
	// if the function not found (not registered), indicate failure
	if (functionMap.count(tempFunction) == 0){
		returnMessage = "CACHE_FAILURE";
		returnValue = (-240);
		
		if (sendStringMessage(fd, returnMessage) < 0){
			returnValue = (-225);
			error("ERROR: binder failed to send failure message");
		}
			
		if (sendIntMessage(fd, returnValue) < 0){
			returnValue = (-225);
			error("ERROR: binder failed to send warning/error message");
		}
		
	}
	// if function registered, send all available servers' host names and ports
	else{
		// index = index of all servers providing such a service in serverVector 
		vector <int>* index = &functionMap[tempFunction];	

		returnMessage = "CACHE_SUCCESS";
		returnValue = 0;
		
		// first indicate success -- some servers available
		if (sendStringMessage(fd, returnMessage) < 0){
			returnValue = (-225);
			error("ERROR: binder failed to send success message");
		}
		
		// first send the number of servers available
		if (sendIntMessage(fd, index->size()) < 0){
				returnValue = (-225);
				error("ERROR: binder failed to send the number of servers to be cached");
		}
		
		// then, loop over index, send all servers' info
		for (int j=0; j<index->size(); j++){
			// find the index of the first available server by RR
			struct server* temp = serverVector[j];
			string* host = temp->host;
			int port = temp->port;
			
			cerr << " cache: send back to client. server: " << *(temp->host) << endl;;
			
			if (sendStringMessage(fd, *host) < 0){
				returnValue = (-225);
				error("ERROR: binder failed to send fd number");
			}
				
			if (sendIntMessage(fd, port) < 0){
				returnValue = (-225);
				error("ERROR: binder failed to send port number");
			}	
		}	

	}
	
	return returnValue;
	
}


/*
	receive & send message from/to client or server
*/
static int work(int listener){
	// select preparation	----- REUSE SELECT part from A2 (file: server.cc) -----
	fd_set master;		// master file descriptor list - currently connected fd
	fd_set read_fds;	// temp  file descriptor list for select()
	int fdmax;			// max file descriptor number
	
	int newsockfd;
	struct sockaddr_in cli_addr;
	socklen_t cli_addr_size;
	
	FD_ZERO(&master);	// clear sets
	FD_ZERO(&read_fds);
	
	// add listener to master set
	FD_SET(listener, &master);
	// keep track of the mas fd. so far, it's listener
	fdmax = listener;
	
	// loop - so it seems the binder keeps working until terminated
	while (1) {

		read_fds = master;
		
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) <0) return (-210);
		
		// run through existing connections, look for data to read		
		for (int i=0; i<=fdmax; i++){
			cout << "i: " << i << endl;
			// if we find some fd ready to read
			if (FD_ISSET(i, &read_fds)){
				cout << "we find some fd ready to read" << endl;
				// if ready to read from listener, handle new connection
				if (i == listener){
					cli_addr_size = sizeof(cli_addr);
					newsockfd = accept(listener, (struct sockaddr*)&cli_addr, &cli_addr_size);
					if (newsockfd < 0){
						cerr << "ERROR: fail on accepting" << endl;
						continue;
						//return (-211);
					}

					FD_SET(newsockfd, &master);		// add to master, since currently connected
					if (newsockfd > fdmax)			// update fdmax
						fdmax = newsockfd;	

					cout << "new connection" << endl;
				}
				// else, if ready to read from client, handle new data
				else{
					cout << "ready to read from client, handle new data" << endl;
					string* words;					// by our mechanism, it should specify message type
					int byteRead = receiveStringMessage(i, &words);	// remember to delete words after usage!!!
					if (byteRead < 0){
						close(i);					// must be a client. since server is kept open until binder asks for temination
						FD_CLR(i, &master);
						//delete words;
						cerr << "ERROR: Binder failed to read from client" << endl;
						//return (-212);
					}
					// else, if an attempt to receive a request fails, assume the client/server has quit
					else if (byteRead == 0){
						close(i);
						FD_CLR(i, &master);
					}
					// else, ok, we receive some request
					else{		
						cout << "get something" << endl;
						string returnMessage;
						int returnValue;
						
						// if server/binder message - REGISTER
						if (*words == "REGISTER"){
							returnValue = handleRegister(i /*fd*/ );
							delete words;
							if (returnValue<0) cerr << "debug: fail to handleRegister " << endl;
							// return returnValue;
						}
						// if client/binder message - LOC_REQUEST
						else if (*words == "LOC_REQUEST"){
							returnValue = handleRequest(i /*fd*/ );
							delete words;
							if (returnValue<0) cerr << "debug: fail to handleRequest " << endl;
							 //return returnValue;							
						}
						// if client/binder message - TERMINATE
						else if (*words == "TERMINATE"){
							// inform all servers to terminate, close server's and binder's socket
							handleTerminate(listener, words, master);
							delete words;
							// maybe clean up serverVector and functionMap 
							clean();
							return 0;
						}
						// if client/binder message - CACHE
						else if (*words == "CACHE"){
							//
							handleCache(i /*fd*/);
							delete words;
							
						}
					}
				}// END			
			}// END new incoming data (fd)
		}// END fd loop
	} //END while
	
}


int main(){
	// listener: binder socket file descriptor
	int listener = initializeSocket();
	if (listener < 0) return listener;
	
	// print binder's IP and port
	int result = printBinderInfo(listener);
	if (result < 0) return result; 
	
	// listen
	result = work(listener);
	if (result < 0) return result;
	
	return 0;
}

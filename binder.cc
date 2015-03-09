#include <binder.h>#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <cstring>
#include <stdlib.h>		// exit
#include <unistd.h>		// gethostname
#include <map>
#include <vector>
#include <utility>      // pair

#include "binder.h"

using namespace std;

// store function-server map. 
//   int-index indicates the next available server by round robin
//   vector<int> indicates the index in the serverVector
map <struct function, pair<int, vector<int> > >functionMap;

// stores servers' information
vector<server*> serverVector;

int init_index = 0;		// first, choose 0-th server which provides the function service



// error output.  ----- REUSE from A2
static void error(const char *err){
	cerr << err << endl;
}


// initialize socket and listen.   ----- REUSE from A2 (file: stringServer.cc) -----
int initializeSocket(){
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
int printBinderInfo(){ 
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
	void handleRegister(int fd);
	
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
static void handleRegister(int fd ){
	
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
	
	if (receiveStringMessage(fd, &host) < 0)	{		// delete host later!!!			
		returnMessage = "REGISTER_FAILURE";
		returnValue = (-220);
	}
	if (receiveIntMessage(fd, &port) <0){
		returnMessage = "REGISTER_FAILURE";
		returnValue = (-221);
	}
	if (receiveStringMessage(fd, &name) <0){ 		// delete name later!!!
		returnMessage = "REGISTER_FAILURE";
		returnValue = (-222);
	}
	if (receiveArrayMessage(fd, argTypes) <0){ 		// delete aryTypes later!!!
		returnMessage = "REGISTER_FAILURE";
		returnValue = (-223);
	}
	
	// if any failure occurs, send back response
	if (strcmp(returnMessage, "REGISTER_FAULURE") == 0){
		sendStringMessage(fd, returnMessage);
		sendIntMessage(fd, returnValue);
		return;		
	}
	
	/*
	
		now register the server & procedure
		build a function map where the key is the function
	 
	*/
	
	struct function tempFunction(name, argTypes);
	struct server* tempServer = new server(host, port, fd);	// keep on heap
	
	// check if server has already registered
	int flag = 0;	// =1 if found the server record
	int i = 0;
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
			int count = serverVector.size();
			index.push_back(count);
		}
		
		functionMap[tempFunction2] = make_pair(init_index, index);
		cout << functionMap[tempFunction2].first << endl;	
		
		returnValue = 0;	// no warning
		
		
	}
	// if the (function,argTypes) already registered
	else{
		cerr << "function found " << *name << endl;
		
		vector<int>* index = &(functionMap[tempFunction].second);
		
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
		}
		// else, new server
		else{
			serverVector.push_back(tempServer);
			int count = serverVector.size();
			index.push_back(count);
			returnValue = 0;				// no warning
		}		
	}
	returnMessage = "REGISTER_SUCCESS";
	
	/*
	
		send back the success/failure message
	
	*/
	if (sendStringMessage(fd, returnMessage) < 0) {
		returnMessage = "REGISTER_FAILURE";				// < ------------------- what ?! 
		returnValue = (-224);
		error("ERROR: binder failed to send back success/failure message");
	}
	if (sendIntMessage(fd, returnValue) < 0){
		returnMessage = "REGISTER_FAILURE";				// < ------------------- what ?! 
		returnValue = (-225);
		error("ERROR: binder failed to send back warning/error message");
	}
	
}


/*

	void handleRequest(int fd);

	handle request from client.
	-- find the available server providing the function, using round robin mechanism
	
	Send back success/failure message to file descriptor fd
		1.	first send LOC_SUCCESS or LOC_FAILURE
		2.	for LOC_SUCCESS, then send server-identifier and port number
			for LOC_FAILURE, then send reason code
	
*/
static void handleRequest(int fd){
	string returnMessage;
	int returnValue;
	
	string* name;
	int* argTypes;
	
	if (receiveStringMessage(fd, &name) <0){					// delete !!!
		returnMessage = "LOC_FAILURE";
		returnValue = (-222);
	}
	if (receiveArrayMessage(fd, argTypes) <0){				// delete !!!
		returnMessage = "LOC_FAILURE";
		returnValue = (-223);
	}
	
	// if any failure occurs, send back response
	if (strcmp(returnMessage, "LOC_FAILURE") == 0){
		sendStringMessage(fd, returnMessage);
		sendIntMessage(fd, returnValue);
		return;		
	}
	
	/*
	
		now, use round robin mechanism to find the available server, if any
		
	*/
	struct function tempFunction(name, argTypes);
	
	// if the function not found (not registered), indicate failure
	if (functionMap.count(tempFunction) == 0){
		returnMessage = "LOC_FAILURE";
		returnValue = (-240);
		
		if (sendStringMessage(fd, returnMessage) < 0)
			error("ERROR: binder failed to send failure message");
		if (sendIntMessage(fd, returnValue) < 0)
			error("ERROR: binder failed to send warning/error message");
		
	}
	// if function registered, return the available server' fd and port
	else{		
		vector <int>* index = &(functionMap[tempFunction].second);
		
		// find the index of the first available server by RR
		int i = (functionMap[tempFunction].first) % (index->size());
		struct server* temp = serverVector[index->at(i)];
		int fd = temp->sockfd;
		int port = temp->port;
		
		returnMessage = "LOC_SUCCESS";
		
		if (sendStringMessage(fd, returnMessage) < 0)
			error("ERROR: binder failed to send success message");
		if (sendIntMessage(fd, fd) < 0)
			error("ERROR: binder failed to send fd number");
		if (sendIntMessage(fd, port) < 0)
			error("ERROR: binder failed to send port number");
		
		// update next available server's index
		functionMap[tempFunction].first++;
	}
	
}

/*
	void hendleTerminate(int fd, string* words);
	
	receive terminate request from client
	-- send the terminate signal to all servers.
	-- close all sockets
	-- after all servers terminate, the binder terminates
	
*/
static void hendleTerminate(int listener, string* words, FD_SET &master){
	
	map <struct function, pair<int, vector<int> > >::iterator i;
	
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
	close(listener);				// close socket
	FD_CLR(listener, &master);
	
	

}


/*
	clean the serverVector and functionMap
*/
static void clear(){
	cerr << "Starting to clean ... " << endl;
	
	// try clear serverVector
	int end = serverVector.size();
	for (int i=0; i < end; i++){
		delete serverVector[i]->host;
		delete serverVector[i];
	}
	
	// try clear map
	map <struct function, pair<int, vector<struct server*> > >::iterator i;
	for (i = functionMap.begin(); i != functionMap.end(); i++){
		const struct function* Func  = &(i->first);
		functionMap.erase(i);
		delete Func->name;
		delete [] Func->argTypes;
	}
	
	cerr << "Finished cleaning. " << endl;
	
}


/*
	receive & send message from/to client or server
*/
void work(int listener){
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
			// if we find some fd ready to read
			if (FD_ISSET(i, &read_fds)){
				// if ready to read from listener, handle new connection
				if (i == listener){
					cli_addr_size = sizeof(cli_addr);
					newsockfd = accept(listener, (struct sockaddr*)&cli_addr, &cli_addr_size);
					if (newsockfd < 0) return (-211);

					FD_SET(newsockfd, &master);		// add to master, since currently connected
					if (newsockfd > fdmax)			// update fdmax
						fdmax = newsockfd;					
				}
				// else, if ready to read from client, handle new data
				else{
					string* words;					// by our mechanism, it should specify message type
					int byteRead = receiveStringMessage(i, &words);	// remember to delete words after usage!!!
					if (byteRead < 0){
						close(i);					// SHOULD remove server (if i=server fd) from map !!!!!!!!!!!!!!!!!!! later
						FD_CLR(i, &master);
						delete words;
						//return (-212);		// hmmm... any message?
					}
					// else, if an attempt to receive a request fails, assume the client/server has quit
					else if (byteRead == 0){
						close(i);					// SHOULD remove server (if i=server fd) from map !!!!!!!!!!!!!!!!!!! later
						FD_CLR(i, &master);
					}
					// else, ok, we receive some request
					else{		
						string returnMessage;
						int returnValue;
						
						// if server/binder message - REGISTER
						if (*words == "REGISTER"){
							handleRegister(i /*fd*/ );
							delete words;
						}
						// if client/binder message - LOC_REQUEST
						else if (*words == "LOC_REQUEST"){
							handleRequest(i /*fd*/ );
							delete words;							
						}
						// if client/binder message - TERMINATE
						else if (*words == "TERMINATE"){
							// inform all servers to terminate, close server's and binder's socket
							hendleTerminate(listener, words, master);
							delete words;
							// maybe clean up serverVector and functionMap 
							clear();
							return;
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
	int result = printBinderInfo()
	if (result < 0) return result; 
	
	// listen
	work(listener);
	
	return 0;
}

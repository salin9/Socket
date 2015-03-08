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
#include "error.h"

using namespace std;

// store function-server map. int-index indicates the next available server by round robin
map <struct function, pair<int, vector<struct server*> > >functionMap;
int init_index = 0;		// first, choose 0-th server which provides the function service

// error output.  ----- REUSE from A2
static void error(const char *err){
	cerr << err << endl;
}


// initialize socket and listen.   ----- REUSE from A2 (file: server.cc) -----
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
	if (listen(sockfd, max_client) < 0)
		return (-202);
	
	return sockfd;		
}

// print the binder info: address and port.    ----- REUSE from A2 (file: server.cc) -----
int printBinderInfo(){ 
	//get machine name
	char name[max_name];
	if (gethostname(name, max_name) < 0)
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
 register procedure and argument types -- from server -- in functionMap
  -- if the (procedure, argTypes, server) already registered, send warning. (in rpc, update skeleton)
  -- send back success/failure message
  
*/
static void handleRegister(int i /*fd*/ ){
	// receive server_identifier,  port,  name,  argTypes
	string* host;
	int port;
	string* name;
	int* argTypes;
	
	if (receiveStringMessage(i, &host) < 0)					// delete !!!
		error("ERROR: binder failed to receive host info");			
	if (receiveIntMessage(i, &port) <0)
		error("ERROR: binder failed to receive port info");
	if (receiveStringMessage(i, &name) <0)					// delete !!!
		error("ERROR: binder failed to receive function name");
	if (receiveArrayMessage(i, argTypes) <0)					// delete !!!
		error("ERROR: binder filed to receive argTypes");
		
	// now register the server & procedure
	struct function tempFunction(name, argTypes);
	struct server tempServer(host, port, i);
	
	// if the function not yet registered
	if (functionMap.find(tempFunction) == functionMap.end()){
		vector<struct server*> Servers;
		Servers.push_back(&tempServer);
		functionMap[tempFunction]  = make_pair(init_index, Servers);
		
		returnValue = 0;
	}
	// if the function already registered
	else{
		vector<struct server*>* Servers = &(functionMap[tempFunction].second);
		
		// check if server already exist
		// if server exists, send warning -- update skeleton in rpc (later!)
		for(int i = 0; i < Servers.size(); i++){
			if (Servers[i] == tempServer){
				// set warning
				returnValue = ERROR_OVERRIDE_PROCEDURE;
				break;
			}
		}
		// if server not exists, register
		if (returnValue != ERROR_OVERRIDE_PROCEDURE){
			Servers->push_back(&tempServer);
			returnValue = 0;
		}
		
	}
	returnMessage = "REGISTER_SUCCESS";				// when is returnMessage = "REGISTER_FAILUER"????
	
	if (sendStringMessage(i, returnMessage) < 0)
		error("ERROR: binder failed to send back success/failure message");
	if (sendIntMessage(i, returnValue) < 0)
		error("ERROR: binder failed to send back warning/error message");
	
}

/*
 handle request from client.
 find the available server providing the function, using round robin mechanism
*/
static void handleRequest(int i /*fd*/){
	string returnMessage;
	int returnValue;
	
	string* name;
	int* argTypes;
	
	if (receiveStringMessage(i, &name) <0)										// delete !!!
		error("ERROR: binder failed to reveice function name");
	if (receiveArrayMessage(i, argTypes) <0)								// delete !!!
		error("ERROR: binder filed to receive argTypes");
		
	struct function tempFunction(name, argTypes);
	
	// if the function not found (not registered)
	if (functionMap.find(tempFunction) == functionMap.end()){
		returnMessage = "LOC_FAILURE";
		returnValue = ERROR_PROCEDURE_NOT_REGISTERED;
		
		if (sendStringMessage(i, returnMessage) < 0)
			error("ERROR: binder failed to send failure message");
		if (sendIntMessage(i, returnValue) < 0)
			error("ERROR: binder failed to send warning/error message");
		
	}
	// if function registered, return available server 
	else{
		
		vector <struct server*>* Servers = &(functionMap[tempFunction].second);
		int i = (functionMap[tempFunction].first) % (*Servers).size();		// find the index of the first available server
		
		int fd = Servers->at(i)->sockfd;
		int port = Servers->at(i)->port;
		returnMessage = "LOC_SUCCESS";
		
		if (sendStringMessage(i, returnMessage) < 0)
			error("ERROR: binder failed to send success message");
		if (sendIntMessage(i, fd) < 0)
			error("ERROR: binder failed to send fd number");
		if (sendIntMessage(i, port) < 0)
			error("ERROR: binder failed to send port number");
		
		// update next available server's index
		functionMap[tempFunction].first++;
	}
	
}

/*
 receive terminate request from client
 send the terminate signal to all servers. after all servers terminate, the binder terminates
*/
static void hendleTerminate(int i /*fd*/, string* words){
	map <struct function, pair<int, vector<struct server*> > >::iterator i;
	for (i = functionMap.begin(); i != functionMap.end(); i++){
		vector<server*>* Servers = &(i->second);
		for (int j = 0; j<Servers.size(); j++)
			if (Servers->at(j)->valid){
				sendStringMessage(i, *words);	// ask server to terminate  -- later (REMINDER: on server side. in rpc)
				Servers->at(j)->valid = 0;		// so that no duplicate "TERMINATE" message sent 
			}
	}
	
	// then terminate the binder itself
	// have to do that in function -work()-
}


static void clearMap(){
	cerr << "Starting to clean ... " << endl;
	
	map <struct function, pair<int, vector<struct server*> > >::iterator i;
	for (i = functionMap.begin(); i != functionMap.end(); i++){
		vector<server*>* Servers = &(i->second.second);
		int end = Servers->size();
		for (int j = 0; j < end; j++){
			delete Servers->at(j)->host;
			//cout << "next server port " << Servers->at(j+1)->port <<endl;
		}
		
		const struct function* Func  = &(i->first);
		functionMap.erase(i);
		delete Func->name;
		delete [] Func->argTypes;
	}
	
	cerr << "Finished cleaning. " << endl;
	
}


// receive & send message from/to client or server
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
		
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) <0)
			error("ERROR: server failed to select");
		
		// run through existing connections, look for data to read		
		for (int i=0; i<=fdmax; i++){
			// if we find some fd ready to read
			if (FD_ISSET(i, &read_fds)){
				// if ready to read from listener, handle new connection
				if (i == listener){
					cli_addr_size = sizeof(cli_addr);
					newsockfd = accept(listener, (struct sockaddr*)&cli_addr, &cli_addr_size);
					if (newsockfd < 0)
						error("ERROR: binder failed to accept");

					FD_SET(newsockfd, &master);		// add to master, since currently connected
					if (newsockfd > fdmax)			// update fdmax
						fdmax = newsockfd;					
				}
				// else, if ready to read from client, handle new data
				else{
					string* words;								// by our mechanism, it should specify message type
					int byteRead = receiveStringMessage(i, &words);	// remeber to delete words after useage!!!
					if (byteRead < 0){
						close(i);								// SHOULD remove server (if i=server fd) from map !!!!!!!!!!!!!!!!!!! later
						FD_CLR(i, &master);
						delete words;	// delete!!!
						error("ERROR: binder failed to read from client");
					}
					// else, if an attempt to receive a request fails, assume the client/server has quit
					else if (byteRead == 0){
						close(i);								// SHOULD remove server (if i=server fd) from map !!!!!!!!!!!!!!!!!!! later
						FD_CLR(i, &master);
					}
					// else, ok, we receive some request
					else{		
						string returnMessage;
						int returnValue;
						
						// if server/binder message - REGISTER
						if (*words == "REGISTER"){
							handleRegister(i /*fd*/ );						
						}
						// if client/binder message - LOC_REQUEST
						else if (*words == "LOC_REQUEST"){
							handleRequest(i /*fd*/ );							
						}
						// if client/binder message - TERMINATE
						else if (*words == "TERMINATE"){
							// inform all servers to terminate
							hendleTerminate(i /*fd*/, words);
							// close itself
							close(i);
							FD_CLR(i, &master);
							// maybe clean up the map? 
							functionMap.clear();
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
	
	int result = printBinderInfo()
	if (result < 0) return result; 
	
	work(listener);
	
	return 0;
}

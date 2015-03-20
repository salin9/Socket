#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <cstring>
#include <stdlib.h>		// exit
#include <unistd.h>		// gethostname
#include <map>
#include <vector>

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



static void printAll(){
	cerr << "serverVector size =" << serverVector.size() << ": ";
	for (int i=0; i<serverVector.size(); i++){
	   cerr << i << "---" << *(serverVector[i]->host) << ", ";
	}
	cerr <<"  ." <<endl;
	
	cerr << "RRqueue size = " << RRqueue.size() << ": ";
	for (int i=0; i<RRqueue.size(); i++){
	   cerr << RRqueue[i] << ", ";
	}
	cerr <<"  ." <<endl;
	
	cerr  << "functionMap = " << functionMap.size() << ": " <<endl;
	map <struct function,  vector<int> >::iterator p;
	for (p = functionMap.begin(); p != functionMap.end(); p++){
		cerr << "   " << *(p->first.name) << ": ";
		vector <int>* index = &(p->second);
		for (int j=0; j<index->size(); j++){
			cerr << j << ", ";
		}
		cerr << ". " << endl;
	}
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


// find the server with file descriptor number fd. 
// -- remove this server from all database: serverVector, RRqueue, functionMap
static void removeDeadServer(int fd){
   int i=0;
   for (; i < serverVector.size(); i++){
		if (serverVector[i]->sockfd == fd) {
		   delete serverVector[i]->host;
		   serverVector.erase(serverVector.begin() + i);
		   break;
		}
	}	

   //remove from RRqueue
   for (int j=0; j < RRqueue.size(); j++){
		if (RRqueue[j] == i) {
		   RRqueue.erase(RRqueue.begin() + j);
		   
		   // if the removed element is not the last one
		   if (j!= RRqueue.size()) j--;
		}
		else if (RRqueue[j] > i){
		   RRqueue[j]--; 
		}
	}
	
	// remove from functionMap
	// -- note: even when a key (registered function) has no values (available servers), 
	//			instead of removing the key, we still keep it. so that we will have different error codes
	map <struct function,  vector<int> >::iterator k;
	for (k = functionMap.begin(); k != functionMap.end(); k++){
		vector <int>* index = &(k->second);
		for (int j=0; j<index->size(); j++){
			if (index->at(j) == i){
				(*index).erase((*index).begin() + j);

				// if the removed element is not the last one
				if (j!=index->size()) j--;		      
			}
			else if (index->at(j) > i){
				(index->at(j))--;
			}
		}
	}
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
	
	//cout << "debug: handleRegister" << endl;

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
	if ( result < 0){
		returnMessage = "REGISTER_FAILURE";
		returnValue = result;
	}

	//cout << "handleRegister: received: " << *host << endl;

	if (receiveIntMessage(fd, &port) <0){
		returnMessage = "REGISTER_FAILURE";
		returnValue = result;
	}

	//cout << "handleRegister: received: " <<  port << endl;

	result = receiveStringMessage(fd, &name);		// delete name later!!!
	if ( result <0){
		returnMessage = "REGISTER_FAILURE";
		returnValue = result;
	}

	//cout << "handleRegister: received: " << *name << endl;

	result = receiveArrayMessage(fd, &argTypes);
	if (result <0){ 		// delete aryTypes later!!!
		returnMessage = "REGISTER_FAILURE";
		returnValue = result;
	}

	//cout << "handleRegister: received argTypes" << endl;


	// check if any argTypes[i] are valid
	int i = 0;
	while (argTypes[i] != 0){
		int type  = (argTypes[i] >> 16  & 255);
		if ( type < 1 || type > 6){
			returnMessage = "REGISTER_FAILURE";
			returnValue = (-220);
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
		//cerr << "function not found " << *name << endl;

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
		//cerr << "function found " << *name << endl;
		
		vector<int>* index = &functionMap[tempFunction];
		
		// check if server already exist
		// if server exists, send warning -- update skeleton in rpc (later!)
		if (flag){
			// then check if serverVector[i] has registered with the function
			int flag2 = 0;
			for (int j=0; j<index->size(); j++){
				if (index->at(j) == i){
					flag2 = 1;
					returnValue = (230);		// set warning - previously registered
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
	result = sendStringMessage(fd, returnMessage);
	if ( result < 0) {
		returnMessage = "REGISTER_FAILURE";
		returnValue = result;
		//cerr << "ERROR: binder failed to send back success/failure message" << endl;
	}
	result = sendIntMessage(fd, returnValue);
	if (result < 0){
		returnMessage = "REGISTER_FAILURE";	
		returnValue = result;
		//cerr << "ERROR: binder failed to send back warning/error message" << endl;
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
	
	int result = receiveStringMessage(fd, &name);
	if (result <0){					// delete !!!
		returnMessage = "LOC_FAILURE";
		returnValue = result;
	}
	result = receiveArrayMessage(fd, &argTypes);
	if (result <0){				// delete !!!
		returnMessage = "LOC_FAILURE";
		returnValue = result;
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
		//cerr << "fucntion not register" << endl;

        returnMessage = "LOC_FAILURE";
		returnValue = (-240);
		
		result = sendStringMessage(fd, returnMessage);
		if (result < 0){
			returnValue = result;
			//cerr << "ERROR: binder failed to send failure message" <<endl;
		}
		result = sendIntMessage(fd, returnValue);
		if (result < 0){
			returnValue = result;
			//cerr << "ERROR: binder failed to send warning/error message" << endl;
		}
			
		
	}
	// if function registered, send the available server' host name and port
	else{
		// index = index of all servers providing such a service in serverVector 
		vector <int>* index = &functionMap[tempFunction];
		
		if (index->size() == 0){
            //cerr << "function registered. but no alive servers " <<endl;

			returnMessage = "LOC_FAILURE";
			returnValue = (-240);	// previously -241

			result = sendStringMessage(fd, returnMessage);
			if (result < 0){
				returnValue = result;
				//cerr << "ERROR: binder failed to send failure message" << endl;
			}
			result = sendIntMessage(fd, returnValue);
			if (result < 0){
				returnValue = result;
				//cerr << "ERROR: binder failed to send warning/error message" << endl;
			}
		}
		else{ //
		    //cerr << "ready to find some server" <<endl;
		
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

			//cerr << "by RR, the next available server is " << *(temp->host) << endl;;

			returnMessage = "LOC_SUCCESS";
			returnValue = 0;
			
			result = sendStringMessage(fd, returnMessage);
			if (result < 0){
				returnValue = result;
				//cerr << "ERROR: binder failed to send success message" << endl;
			}
			result = sendStringMessage(fd, *host);
			if (result < 0){
				returnValue = result;
				//cerr << "ERROR: binder failed to send fd number" << endl;
			}
			result = sendIntMessage(fd, port);
			if (result < 0){
				returnValue = result;
				//cerr << "ERROR: binder failed to send port number" << endl;
			}
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
static int handleTerminate(int listener, string* words, fd_set &master){
	
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

	return 0;
}


/*
	clean the serverVector and functionMap
*/
static void clean(){
	//cerr << "Starting to clean ... " << endl;
	
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
	
	//cerr << "Finished cleaning. " << endl;
	
}


static int handleCache(int fd){
	
	string returnMessage;
	int returnValue;
	
	string* name;
	int* argTypes;
	
	int result = receiveStringMessage(fd, &name);
	if (result <0){					// delete !!!
		returnMessage = "CACHE_FAILURE";
		returnValue = result;
	}
	result = receiveArrayMessage(fd, &argTypes);
	if (result <0){				// delete !!!
		returnMessage = "CACHE_FAILURE";
		returnValue = result;
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
		
		result = sendStringMessage(fd, returnMessage);
		if (result < 0){
			returnValue = result;
			//cerr << "ERROR: binder failed to send failure message" << endl;
		}
		result = sendIntMessage(fd, returnValue);
		if (result < 0){
			returnValue = result;
			//cerr << "ERROR: binder failed to send warning/error message" << endl;
		}
		
	}
	// if function registered, send all available servers' host names and ports
	else{
		// index = index of all servers providing such a service in serverVector 
		vector <int>* index = &functionMap[tempFunction];	

		if (index->size() == 0){
            //cerr << "function registered. but no alive servers " <<endl;

			returnMessage = "CACHE_FAILURE";
			returnValue = (-240);	// previously -241

			result = sendStringMessage(fd, returnMessage);
			if (result < 0){
				returnValue = result;
				//cerr << "ERROR: binder failed to send failure message" << endl;
			}
			result = sendIntMessage(fd, returnValue);
			if (result < 0){
				returnValue = result;
				//cerr << "ERROR: binder failed to send warning/error message" << endl;
			}
		}
		else{
			//cerr << "function registered, and alive servers are available"
			returnMessage = "CACHE_SUCCESS";
			returnValue = 0;
			
			// first indicate success -- some servers available
			result = sendStringMessage(fd, returnMessage);
			if (result < 0){
				returnValue = result;
				//cerr << "ERROR: binder failed to send success message" <<endl;
			}
			
			// first send the number of servers available
			result = sendIntMessage(fd, index->size());
			if (result < 0){
				returnValue = result;
				//cerr << "ERROR: binder failed to send the number of servers to be cached" <<endl;
			}
			
			// then, loop over index, send all servers' info
			for (int j=0; j<index->size(); j++){
				// find the index of the first available server by RR
				struct server* temp = serverVector[j];
				string* host = temp->host;
				int port = temp->port;
				
				//cerr << " cache: send back to client. server: " << *(temp->host) << endl;;
				
				result = sendStringMessage(fd, *host);
				if (result < 0){
					returnValue = result;
					//cerr << "ERROR: binder failed to send fd number" << endl;
				}
				result = sendIntMessage(fd, port);
				if (result < 0){
					returnValue = result;
					//cerr << "ERROR: binder failed to send port number" <<endl;
				}	
			}	
		}
	}
	
	return returnValue;
	
}


/*
	receive & send message from/to client or server
*/
static int work(int listener){
	int returnValue;
	
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
		
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) <0) returnValue = (-210);
		
		// run through existing connections, look for data to read		
		for (int i=0; i<=fdmax; i++){
			// if we find some fd ready to read
			if (FD_ISSET(i, &read_fds)){
				//cerr << "we find some fd ready to read" << endl;
				// if ready to read from listener, handle new connection
				if (i == listener){
					//cerr << "new connection" << endl;
					cli_addr_size = sizeof(cli_addr);
					newsockfd = accept(listener, (struct sockaddr*)&cli_addr, &cli_addr_size);
					if (newsockfd < 0){
						//cerr << "ERROR: fail on accepting" << endl;
						returnValue = (-211);
						continue;
					}

					FD_SET(newsockfd, &master);		// add to master, since currently connected
					if (newsockfd > fdmax)			// update fdmax
						fdmax = newsockfd;	

				}
				// else, if ready to read from client, handle new data
				else{
					//cerr << "ready to read from client, handle new data" << endl;
					string* words;					// by our mechanism, it should specify message type
					int byteRead = receiveStringMessage(i, &words);	// remember to delete words after usage!!!
					if (byteRead < 0){
						//cerr << "ERROR: Binder failed to read from client" << endl;
						close(i);
						FD_CLR(i, &master);
						removeDeadServer(i);
						//printAll();
					}
					// else, if an attempt to receive a request fails, assume the client/server has quit
					else if (byteRead == 0){
						//cerr  << "lost connection" << endl;
						close(i);
						FD_CLR(i, &master);
						removeDeadServer(i);						
						//printAll();
					}
					// else, ok, we receive some request
					else{		
						//cerr << "get something" << endl;
						string returnMessage;						
						
						// if server/binder message - REGISTER
						if (*words == "REGISTER"){
							cerr << "Binder received REGISTER request from server. Processing..." <<endl;
							returnValue = handleRegister(i /*fd*/ );
							delete words;
							cerr << "Binder has finished the registration process with message code = " << returnValue << endl;
							//if (returnValue<0) cerr << "debug: fail to handleRegister " << endl;
						}
						// if client/binder message - LOC_REQUEST
						else if (*words == "LOC_REQUEST"){
							cerr << "Binder received LOC_REQUEST request from client. Processing..." <<endl;
							returnValue = handleRequest(i /*fd*/ );
							delete words;
							cerr << "Binder has finished the loc_request process with message code = " << returnValue << endl;
							//if (returnValue<0) cerr << "debug: fail to handleRequest " << endl;					
						}
						// if client/binder message - TERMINATE
						else if (*words == "TERMINATE"){
							// inform all servers to terminate, close server's and binder's socket
							cerr << "Binder received TERMINATE request from client. Processing..." <<endl;
							returnValue = handleTerminate(listener, words, master);
							delete words;
							// clean up serverVector, RRqueue, and functionMap 
							clean();
							cerr << "Binder has finished termination process with message code = " << returnValue << endl;
							return 0;
						}
						// if client/binder message - CACHE
						else if (*words == "CACHE"){
							cerr << "Binder received CACHE request from client. Processing..." <<endl;
							returnValue = handleCache(i /*fd*/);
							delete words;			
							cerr << "Binder has finished the cache process with message code = " << returnValue <<endl;							
						}
						else {
							cerr << "Binder received invalid request." <<endl;
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
	if (listener < 0) {
		cerr << "Error code " << listener <<endl;
		return listener;
	}
	
	// print binder's IP and port
	int result = printBinderInfo(listener);
	if (result < 0) {
		cerr << "Error code " << result <<endl;
		return result;
	}
	
	// listen
	result = work(listener);
	if (result < 0) return result;
	
	return 0;
}

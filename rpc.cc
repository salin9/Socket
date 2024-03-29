#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>  
#include <netinet/in.h>
#include <cstring>
#include <stdlib.h>    
#include <unistd.h>     
#include <cctype>       
#include <sstream>    
#include <cstdlib>
#include <pthread.h>
#include <vector>
#include <map>

#include "rpc.h"
#include "message.h"
#include "function.h"

using namespace std;

#define MAX_HOST_NAME 64
#define MAX_CLIENT  10

/*

    Global Variables

*/

int server_listener, binder_socket; 
fd_set master;          // master file descriptor list

// server local database
static map <struct function, skeleton>serverDB;
//
vector <pthread_t> threadList;

map <struct function, vector<server*> > clientDB;

int connectTo(const char* server_address, int server_port){
    struct sockaddr_in sa;
    struct hostent *server;
    int sockfd;

    if ((server = gethostbyname(server_address)) == NULL) return (-110);
    
    memset(&sa, 0, sizeof(struct sockaddr_in));

    memcpy((char *)&sa.sin_addr, server->h_addr, server->h_length); /* set address */
    sa.sin_family = server->h_addrtype;
    sa.sin_port = htons(server_port);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        return (-111);
    }

    if (connect(sockfd, (struct sockaddr*)&sa, sizeof(struct sockaddr_in)) < 0) {
        close(sockfd);
        return (-112);
    }

    return sockfd;
}

int interactWithServer(int fd, char* name, int* argTypes, void** args){
    int result;
    /*

        request execution of a server procedure

    */
    string msg_type = "EXECUTE";   
    result = sendStringMessage(fd, msg_type);
    if(result < 0){
        close(fd);
        return result;
    }
    //cout << "send EXECUTE" << endl;
    // send name
    string str2(name);
    result = sendStringMessage(fd, str2);
    if(result < 0){
        close(fd);
        return result;
    }
    //cout << "send " << str2 << endl;
    // send argTypes
    result = sendArrayMessage(fd, argTypes);
    if(result < 0){
        close(fd);
        return result;
    }
    //cout << "send array message" << endl;
    
    // send args
    result = sendArgsMessage(fd, argTypes, args);
    if(result < 0){
        close(fd);
        return result;
    }
    //cout << "send args msg" << endl;

    /*

        get response from server.
    
    */
    string* response_type;
    result = receiveStringMessage(fd, &response_type);
    if(result < 0){
        close(fd);
        return result;
    } 
    if(*response_type == "EXECUTE_FAILURE"){
        //cout << "EXECUTE_FAILURE " << name << endl;
        int errMsg;
        result = receiveIntMessage(fd, &errMsg);
        if(result < 0){
            close(fd);
            return result;
        } 
        close(fd);
        return errMsg;
    }

    // EXECUTE_SUCCESS : name, argTypes, args
    if(*response_type == "EXECUTE_SUCCESS"){
        //cout << "EXECUTE_SUCCESS " << name << endl;
        string* ret_name;
        result = receiveStringMessage(fd, &ret_name);
        if(result < 0){
            close(fd);
            return result;
        } 
        // may need some test case here... like check ret_name equal to name or not.

        result = receiveArrayMessage(fd, &argTypes);
        if(result < 0){
            close(fd);
            return result;
        } 

        result = receiveArgsMessage(fd, argTypes, args);
        if(result < 0){
            close(fd);
            return result;
        } 

        close(fd);
        return 0;
    }
    return 0;
}

/*

    int rpcCall(char * name, int * argTypes, void ** args);

    Client Side:
    The client will execute a RPC by calling the rpcCall function.

    Parameters:
        1. Return value (int):
            == 0 means success
            >  0 means warning
            <  0 means server error (such as no available server)

        2. name:
            The name of the remote procedure to be executed. 
            A procedure of this name must have been registered with the binder. 

        3. argTypes:
            The argTypes array specifies the types of the arguments.
            For example, argTypes[0] specifies the type information for args[0], and so forth.
            
            The last value in the array is 0 means the array stops here, 
            thus the size of argTypes is 1 greater than the size of args.
            
            The argument type integer will be broken down as:
                (a) 
                    The first byte specify the input/output nature of the argument.
                    1st bit of this byte is set means input to the server.
                    2nd bit is set means output from the server.
                    The rest of six bit must be set to 0.
                (b)
                    The second byte contains argument type information.
                    #define ARG_CHAR    1
                    #define ARG_SHORT   2
                    #define ARG_INT     3
                    #define ARG_LONG    4
                    #define ARG_DOUBLE  5
                    #define ARG_FLOAT   6
                (c)
                    Since we should be able to pass arrays to our remote procedure,
                    thus the lower two bytes specifies the length (max 2^16) of the array.
                    If the size is 0, the argument is a scalar, not an array.
                    Note that it is expected that the client programmer will have reserved sufficient space for any output arrays.

            useful definitions:
            #define ARG_INPUT   31
            #define ARG_OUTPUT  30
            
            For example,
                "(1 << ARG_INPUT) | (ARG_INPUT << 16) | 20" means an array of 20 integers being sent to the server.
                "(1 << ARG_INPUT) | (1 << ARG_OUTPUT) | (ARG_DOUBLE << 16) | 30" means 30 doubles sent to and returned from server.

        4. args:
            The args array of pointers to the different arguments.
            For arrays, they are specified by pointers in C/C++.
            We can use these pointers directly, instead of the addresses of the pointers.
            For example, in the case of
                    char stringVar[] = "string"
                            we use stringVar in the argument array, not &stringVar

    
    Usage:
        If the client wished to execute "result = sum(int vect[LENGTH])", the code would be:

        // result = sum(vector);
        #define PARAMETER_COUNT 2   // Number of RPC arguments
        #define LENGTH 23           // Vector length

        int argTypes[PARAMETER_COUNT + 1];
        void **args = (void **)malloc(PARAMETER_COUNT * sizeof(void *));

        argTypes[0] = (1 << ARG_OUTPUT) | (ARG_INT << 16);              // result
        argTypes[1] = (1 << ARG_INPUT)  | (ARG_INT << 16) | LENGTH;     // vector
        argTypes[2] = 0;                                                // Terminator

        args[0] = (void *)&result;
        args[1] = (void *)vector;

        rpcCall("sum", argTypes, args);

        // Note: The number of output arguments is arbitrary and they can be positioned anywhere within the args vector.

    Note:
        To implement the rpcCall function you will need to send a location request message
        to the binder to locate the server for the procedure.
        If this results in failure, the rpcCall should return a negative integer, otherwise, it should return zero.
        After a successful location request, you will need to send an execute-request message to the server.

*/
int rpcCall(char * name, int * argTypes, void ** args){
    int result;
    //  Connect to the binder first
    char* binder_address = getenv("BINDER_ADDRESS");
    if(binder_address == NULL) return (-140);

    //cout << binder_address << endl;
    
    int portnum = atoi(getenv("BINDER_PORT"));
    if(portnum == 0) return (-141);

    //cout << portnum << endl;
    
    int binder_fd = connectTo(binder_address, portnum);
    if(binder_fd < 0) return binder_fd;    
    
    //  After connecting to binder, send location request message

    // send message type first
    string msg_type = "LOC_REQUEST";
    result = sendStringMessage(binder_fd, msg_type);
    if(result < 0){
        close(binder_fd);
        return result;
    }
    // send name
    string str1(name);
    result = sendStringMessage(binder_fd, str1);
    if(result < 0){
        close(binder_fd);
        return result;
    }

    // send argTypes
    result = sendArrayMessage(binder_fd, argTypes);
    if(result < 0){
        close(binder_fd);
        return result;
    }
    
    //cout << "rpcCall: after send the message to binder" << endl;
     
    // Get the binder's response
    string* response_type;
    
    result = receiveStringMessage(binder_fd, &response_type);
    if(result < 0){
        close(binder_fd);
        return result; 
    }    

    if(*response_type == "LOC_FAILURE"){
        //cout << "LOC_FAILURE" << endl;
        int errMsg;
        result = receiveIntMessage(binder_fd, &errMsg);
        if(result < 0){
            close(binder_fd);
            return result; 
        }
        close(binder_fd);
        return errMsg;
    }

    //cout << "LOC_SUCCESS" << endl;
    // LOC_SUCCESS : server_identifier, port
    string* server_identifier;
    result = receiveStringMessage(binder_fd, &server_identifier);
    if(result < 0){
        close(binder_fd);
        return result; 
    }

    const char* server_address = (*server_identifier).c_str();
    //cout << "debug: server_address: " << server_address << endl;

    int server_port;
    result = receiveIntMessage(binder_fd, &server_port);
    if(result < 0){
        close(binder_fd);
        return result; 
    }
    //cout << "debug: server_port: " << server_port << endl;

    //      connect to server
    int server_fd = connectTo(server_address, server_port);
    if(server_fd < 0) return server_fd;

    int ret = interactWithServer(server_fd, name, argTypes, args);
    return ret;
}


int getSeverList(int sockfd, char* name, int* argTypes){
    int result;
    //  After connecting to binder, send location request message

    // send message type first
    string msg_type = "CACHE";
    result = sendStringMessage(sockfd, msg_type);
    if(result < 0){
        close(sockfd);
        return result;
    }
    // send name
    string str1(name);
    result = sendStringMessage(sockfd, str1);
    if(result < 0){
        close(sockfd);
        return result;
    }

    // send argTypes
    result = sendArrayMessage(sockfd, argTypes);
    if(result < 0){
        close(sockfd);
        return result;
    }
        
    //cout << "rpcCacheCall: after send the message to binder" << endl;
     
    // Get the binder's response
    string* response_type;
    
    result = receiveStringMessage(sockfd, &response_type);
    if(result < 0){
        close(sockfd);
        return result; 
    }    

    if(*response_type == "CACHE_FAILURE"){
        //cout << "CACHE_FAILURE" << endl;
        int errMsg;
        result = receiveIntMessage(sockfd, &errMsg);
        if(result < 0){
            close(sockfd);
            return result;
        }
        close(sockfd);
        return errMsg;
    }

    //cout << "CACHE_SUCCESS" << endl;

    int numOfSever;
    result = receiveIntMessage(sockfd, &numOfSever);
    if(result < 0){
        close(sockfd);
        return result; 
    }

    vector<server*> serverVector;

    for(int i = 0; i < numOfSever; i++){
        string* server_identifier;
        result = receiveStringMessage(sockfd, &server_identifier);
        if(result < 0){
            close(sockfd);
            return result; 
        }

        const char* server_address = (*server_identifier).c_str();
        //cout << "debug: server_address: " << server_address << endl;

        int server_port;
        result = receiveIntMessage(sockfd, &server_port);
        if(result < 0){
            close(sockfd);
            return result; 
        }
        //cout << "debug: server_port: " << server_port << endl;

        struct server* tempServer = new server(server_identifier, server_port, 0); // keep on heap
        serverVector.push_back(tempServer);
    }

    string* str = new string(name);
    struct function tempFunction(str, argTypes);
    clientDB[tempFunction] = serverVector;
    close(sockfd);
    return 0;
}


/*

    rpcCacheCall

*/
int rpcCacheCall(char* name, int* argTypes, void** args){
    int result;

    char* binder_address = getenv("BINDER_ADDRESS");
    if(binder_address == NULL) return (-145);
    //cout << binder_address << endl;
        
    int portnum = atoi(getenv("BINDER_PORT"));
    if(portnum == 0) return (-146);
    //cout << portnum << endl;

    int binder_fd, server_fd;

    string* str = new string(name);
    struct function tempFunction(str, argTypes);
    // If client doesn't have any infomation in the database yet.
    if(clientDB.count(tempFunction) == 0){
        binder_fd = connectTo(binder_address, portnum);
        if(binder_fd < 0) return binder_fd;

        int ret = getSeverList(binder_fd, name, argTypes);
        // -240 means no list
        if(ret == (-240)) return (-240);
    } 

    while(true){
        vector<server*> serverVector = clientDB[tempFunction];
        if(serverVector.empty()){
            //cout << "serverVector is empty" << endl;
            binder_fd = connectTo(binder_address, portnum);
            if(binder_fd < 0) return binder_fd;

            int ret = getSeverList(binder_fd, name, argTypes);
            if(ret == (-240)) return (-240);
        }
        serverVector = clientDB[tempFunction];
        while (!serverVector.empty()){
            server* tempServer = serverVector.back();
            string str = *(tempServer->host);
            const char* server_addr = str.c_str();

            //cout << tempServer->port << endl;

            server_fd = connectTo(server_addr, tempServer->port);
            if(server_fd < 0){
                serverVector.pop_back();
            } else {
                int ret = interactWithServer(server_fd, name, argTypes, args);
                return ret;
            }
        }
        clientDB[tempFunction] = serverVector;
    }
}

/*

    int rpcInit(void);

    The server first calls rpcInit(), which does two things:
        1.
            It creates a connection socket to be used for accepting connections from clients.
        2.
            It opens a connection to the binder.
            The server sends register requests to the binder via this connection.
            The connection is left open until the server is down.

    Parameters:
        1. Return value (int):
            == 0 means success
            <  0 means error (using different negative values for different error conditions)

*/
int rpcInit(){
    FD_ZERO(&master);
    /*

        It opens a connection to the binder.
        The server sends register requests to the binder via this connection.
        The connection is left open until the server is down.

    */
    char* binder_address = getenv("BINDER_ADDRESS");
    if(binder_address == NULL) return (-104);
    //cout << binder_address << endl;

    int binder_port = atoi(getenv("BINDER_PORT"));
    if (binder_port == 0) return (-105);
    //cout << binder_port << endl;

    binder_socket = connectTo(binder_address, binder_port);
    if(binder_socket < 0) return binder_socket;

    FD_SET(binder_socket, &master);

    //cout << "rpcInit: connected to binder" << endl;
    /*

        It creates a connection socket to be used for accepting connections from clients.

    */
    char   hostname[MAX_HOST_NAME + 1];
    struct sockaddr_in server_sockaddr;
    
    memset(&server_sockaddr, 0, sizeof(struct sockaddr_in)); /* clear our address */
    
    if((gethostname(hostname, MAX_HOST_NAME)) < 0) return (-100);
    
    //cout << "rpcInit: SERVER_ADDRESS " << hostname << endl;

    server_sockaddr.sin_family = AF_INET;     
    server_sockaddr.sin_addr.s_addr = INADDR_ANY;
    server_sockaddr.sin_port = 0;                

    /* a connection socket to be used for accepting connections from clients. */
    if ((server_listener = socket(AF_INET, SOCK_STREAM, 0)) < 0) return (-101);
    
    /* bind address to socket */
    if (bind(server_listener, (struct sockaddr*)&server_sockaddr, sizeof(struct sockaddr_in)) < 0) {
        close(server_listener);
        return (-102);
    }

    /* max # of queued connects */
    if((listen(server_listener, MAX_CLIENT)) < 0) return (-103);

    //cout << "rpcInit: opened socket for client successfully" << endl;

    return 0;

}

/*
    
    int rpcRegister(char *name, int *argTypes, skeleton f);

    After the server called rpcInit(), 
    the server makes a sequence of calls to rpcRegister to register each server procedure.
    
    This function does two key things:
        1.
            It calls the binder, informing it that a server procedure with 
            the indicated name and list of argument types is available at this server.
        2.
            It makes an entry in a local database, associating the server skeleton with
            the name and list of argument types.

    Parameters:
        1. Return value (int):
            == 0 means success
            >  0 means warning (e.g., this is the same as some previously registered procedure)
            <  0 means failure (e.g., could not locate binder)
        
        2. name:
            Name of the server procedure

        3. argTypes:
            The list of argument types is available for this procedure.

        4. skeleton f:
            skeleton is defined as
                typedef int (*skeleton)(int *, void **);
            The return value indicate if the server function call executes correctly or not.
            Zero means success.
            Negative means failure. In this case, the RPC library at the server side should return 
            an RPC failure message to the client.

            This parameter is the address of the server skeleton, which 
            corresponds to the server procedure that is being registered.

*/
int rpcRegister(char *name, int *argTypes, skeleton f){
    int result;
    /*

        Send register message to binder.

    */
    //cout << "rpcRegister: " << name << endl;

    // Get the server address
    char server_identifier[MAX_HOST_NAME + 1];    
    if((gethostname(server_identifier, MAX_HOST_NAME)) < 0) return (-120);

    // Get the server port number
    int server_port;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if((getsockname(server_listener, (struct sockaddr *)&addr, &len)) < 0) return (-121);
    server_port = ntohs(addr.sin_port);


    // send the message type first
    string msg_type = "REGISTER";
    result = sendStringMessage(binder_socket, msg_type);
    if(result < 0) return result;

    // send the server_identifier
    string str1(server_identifier);
    result = sendStringMessage(binder_socket, str1);
    if(result < 0) return result;

    // send the port number 
    result = sendIntMessage(binder_socket, server_port);
    if(result < 0) return result;

    // send name
    string str2(name);
    result = sendStringMessage(binder_socket, str2);
    if(result < 0) return result;

    // send argTypes
    result = sendArrayMessage(binder_socket, argTypes);
    if(result < 0) return result;
    /*

        receive binder's response

    */
    string* response_type;
    int ret;

    result = receiveStringMessage(binder_socket, &response_type);
    if(result < 0) return result;

    result = receiveIntMessage(binder_socket, &ret);
    if(result < 0) return result;

    // success, then store the data into local file
    if(*response_type == "REGISTER_SUCCESS"){
        string* str = new string(name);
        //cout << "debug: adding " << *str << " into server database" << endl;
        struct function tempFunction(str, argTypes);
        serverDB[tempFunction] = f; 
    } 

    return ret;
}

/*
    helper for execution
    
*/
void* handleExecute(void *arg){

    cerr << "Server received EXECUTE request from client. Processing..." << endl;

    int result;
    int fd = *((int*)arg);

    /*
    cout << "handleExecute, print database:" << endl;
    for (map<struct function, skeleton>::iterator it = serverDB.begin(); it != serverDB.end(); ++it)
        cout << *(it->first.name) << "\n\n";
    */

    string* name;
    int* argTypes;
    void** args;

    // procedure name
    result = receiveStringMessage(fd, &name);
    if(result < 0) exit(result);

    //cout << "handleExecute: " << *name << endl;

    // argument types
    result = receiveArrayMessage(fd, &argTypes);
    if(result < 0) exit(result);

    //cout << "handleExecute: argTypes" << endl;

    int length = 0;
    while(argTypes[length] != 0){length++;} 

    args = new void*[length];
    for(int i = 0; i < length; i++){
        int arraySize = getLength(argTypes[i]);
        int size = getSize(argTypes[i]) * arraySize;
        args[i] = calloc(arraySize, size);
    }

    // arguments
    result = receiveArgsMessage(fd, argTypes, args);
    if(result < 0) exit(result);

    //cout << "handleExecute: args" << endl;

    /*

        get the skeleton from the map.

    */ 
    struct function tempFunction(name, argTypes);

    //cout << "something happend with server database?" << endl;

    // Can't find the function.
    if(serverDB.count(tempFunction) == 0){
        //cout << "EXECUTE_FAILURE" << endl;

        string returnMessage = "EXECUTE_FAILURE";
        result = sendStringMessage(fd, returnMessage);
        if(result < 0) exit(result);

        int returnValue = -170;
        result = sendIntMessage(fd, returnValue);
        if(result < 0) exit(result);
    }
    

    skeleton f = serverDB[tempFunction];
    int ret = (*f)(argTypes, args);

    if (ret >= 0) { 

        //cout << "EXECUTE_SUCCESS " << *name << endl;

        // send the result back to client
        string msg_type = "EXECUTE_SUCCESS";   
        result = sendStringMessage(fd, msg_type);
        if(result < 0) exit(result);

        // send name
        result = sendStringMessage(fd, *name);
        if(result < 0) exit(result);

        // send argTypes
        result = sendArrayMessage(fd, argTypes);
        if(result < 0) exit(result);
        
        // send args
        result = sendArgsMessage(fd, argTypes, args);
        if(result < 0) exit(result);
    }
    else {

        //cout << "EXECUTE_FAILURE " << *name << endl;
        string returnMessage = "EXECUTE_FAILURE";
        result = sendStringMessage(fd, returnMessage);
        if(result < 0) exit(result);

        int returnValue = -171;
        result = sendIntMessage(fd, returnValue);
        if(result < 0) exit(result);
    }

    cerr << "Server has finished EXECUTE request correctly." << endl;
    return NULL;
}

/*

    int rpcExecute(void);

    The server finally calls rpcExecute, which will wait for and receive requests,
    forward them to skeletons, which are expected to unmarshall the messages, call the 
    appropriate procedures as requested by the clients, and marshall the returns.
    Then rpcExectue sends the results back to the client.

    Parameters:
        1. Return value (int):
            == 0 means normally requested termination (the binder has requested termination of the server)
            <  0 means failure (e.g., if there are no registered procedures to serve)
    
    Note:
        rpcExecute should be able to handle multiple requests from clients without blocking,
        so that a slow server func will not choke the whole server.

        To implement the register func you will need to send a register message to the binder.
*/
int rpcExecute(void){
    /*
    cout << "rpcExecute, print server database:" << endl;
    for (map<struct function, skeleton>::iterator it = serverDB.begin(); it != serverDB.end(); ++it)
        cout << *(it->first.name) << "\n\n";
    */

    //  Set up.

    fd_set read_fds;        // temp file descriptor list for select()
    int fdmax;              // maximum file descriptor number
    int bytesRecv;
    int newfd;              // newly accept()ed socket descriptor
    struct sockaddr_in ca;  // client
    socklen_t addrlen;

    FD_ZERO(&read_fds);
    // add listener to master set
    FD_SET(server_listener, &master);
    // keep track of the mas fd. so far, it's listener
    fdmax = server_listener;

    bool terminated = false;
    while(!terminated){
        // select
        read_fds = master;
        if ((select(fdmax+1, &read_fds, NULL, NULL, NULL)) < 0){
            //cerr << "ERROR: fail on selecting" << endl;
            continue;
        }

        // run through existing connections, look for data to read
        for (int i=0; i<=fdmax; i++){
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == server_listener){
                    addrlen = sizeof(ca);
                    newfd = accept(server_listener, (struct sockaddr*)&ca, &addrlen);
                    if (newfd < 0){
                        //cerr << "ERROR: fail on accepting" << endl;
                        continue;
                    }
                    FD_SET(newfd, &master);     // add to master set
                    if (newfd > fdmax)          // keep track of the max
                        fdmax = newfd;                 
                }
                else {
                    // get something to read!
                    string* words;                  
                    bytesRecv = receiveStringMessage(i, &words);
                    if(bytesRecv <= 0){
                        // got error or connection closed by client
                        if (bytesRecv == 0) {
                            // connection closed
                            //cerr << "connection closed" << endl;
                        } else {
                            //cerr << "ERROR: server failed to read from client" << endl;
                        }
                        close(i); 
                        FD_CLR(i, &master); // remove from master set
                    } else {
                        if(*words == "EXECUTE"){
                            //cout << "eventHandler: EXECUTE" << endl;

                            // We create a new thread to handle the request.
                            pthread_t thread;
                            int* fd = (int*) malloc(sizeof(int));
                            *fd = i;
                            int ret = pthread_create(&thread, NULL, &handleExecute, fd);

                            // add the thread to the list.
                            threadList.push_back(thread);
                            FD_CLR(i, &master);
                        }
                        else if(*words == "TERMINATE"){
                            
                            cerr << "Server received TERMINATE request from Binder. Processing..." << endl;
                            // Authentication.
                            if(i == binder_socket) terminated = true;
                        }
                        else {
                            cerr << "Server received invaild request." << endl;
                        }
                        delete words;
                    }
                }
            }
        }
    }

    for(int i = 0; i < threadList.size(); i++) {
        pthread_join(threadList[i], NULL);
    }

    cerr << "Server is ready to terminate." << endl;

    return 0;
}

/*

    int rpcTerminate(void);

    The client executes rpcTerminate() to terminate the system.
    The request is passed to the binder, then binder will inform the servers to terminate gracefully.
    The binder should terminate after all servers have terminated.

    The servers would authenticate the request from the binder by verifying that
    the termination request comes from the binder's IP address/hostname.

*/
int rpcTerminate(void){
    int result;
    //  Connect to the binder first
    char* binder_address = getenv("BINDER_ADDRESS");
    if(binder_address == NULL) return (-180);
    
    int portnum = atoi(getenv("BINDER_PORT"));
    if(portnum == 0) return (-181);
    
    int binder_sockfd = connectTo(binder_address, portnum); 
    if(binder_sockfd < 0) return binder_sockfd; 
    
    //  After connecting to binder, send terminate request message
    string msg_type = "TERMINATE";
    result = sendStringMessage(binder_sockfd, msg_type);
    if(result < 0){
        close(binder_sockfd);
        return result;
    }

    close(binder_sockfd);
    return 0;
}

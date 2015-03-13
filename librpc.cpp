#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <cstring>
#include <stdlib.h>     
#include <unistd.h>     
#include <cctype>       
#include <sstream>

#include "message.h"

using namespace std;

#define MAX_HOST_NAME 256
#define MAX_CLIENT  5


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
    

    //  Connect to the binder first
     

    char* binder_address = getenv("BINDER_ADDRESS");
    if(binder_address == NULL) return (-140);
    
    int portnum = atoi(getenv("BINDER_PORT"));
    if(portnum == 0) return (-141);
    
    struct sockaddr_in sockaddr1;
    struct hostent *binder;
    int socket1;
    
    if ((binder = gethostbyname(binder_address)) == NULL) return (-142);
    
    memset(&sockaddr1, 0, sizeof(struct sockaddr_in));
    
    memcpy((char *)&sockaddr1.sin_addr, binder->h_addr, binder->h_length); /* set address */
    sockaddr1.sin_family = binder->h_addrtype;
    sockaddr1.sin_port = htons(portnum);
    
    // create the socket for connection to the binder.
    if ((socket1 = socket(AF_INET, SOCK_STREAM, 0)) < 0) return (-143);
    
    // connect to the binder.
    if (connect(socket1, (struct sockaddr*)&sockaddr1, sizeof(struct sockaddr_in)) < 0) {
        close(socket1);
        return (-144);
    }
    
    
     
    //  After connecting to binder, send location request message


    // send message type first
    string msg_type = "LOC_REQUEST";
    if(sendStringMessage(socket1, msg_type) < 0) return (-145);

    // send name
    string str1(name);
    if(sendStringMessage(socket1, str1) < 0) return (-145);

    // send argTypes
    if(sendArrayMessage(socket1, argTypes) < 0) return (-145);
    
     
    //    Get the binder's response
     
    
    string* response_type;
    int* argTypes;
    
    if(receiveStringMessage(socket1, &response_type) < 0) return (-146);    

    if(*response_type == "LOC_FAILURE"){
        int errMsg;
        if (receiveIntMessage(socket1, &errMsg) < 0) return (-146);
        return errMsg;
    }

    
    // LOC_SUCCESS : server_identifier, port
    string* server_identifier;
    if(receiveStringMessage(socket1, &server_identifier) < 0) return (-146);
    char* server_address = (*server_identifier).c_str();
    cout << "debug: server_address: " << server_address << endl;

    int server_port;
    if (receiveIntMessage(socket1, &server_port) < 0) return (-146);
    cout << "debug: server_port: " << server_port << endl;
    

    //      connect to server

    
    struct sockaddr_in sa;
    struct hostent *server;
    int socket2;

    if ((server = gethostbyname(server_address)) == NULL) return (-147);
    
    memset(&sa, 0, sizeof(struct sockaddr_in));

    memcpy((char *)&sa.sin_addr, server->h_addr, server->h_length); /* set address */
    sa.sin_family = server->h_addrtype;
    sa.sin_port = htons(server_port);

    if ((socket2 = socket(AF_INET, SOCK_STREAM, 0)) < 0) return (-143);

    if (connect(socket2, (struct sockaddr*)&sa, sizeof(struct sockaddr_in)) < 0) {     /* connect */
        close(socket2);
        return (-148);
    }


    /*

        request execution of a server procedure

    */
    msg_type = "EXECUTE";

    if(sendStringMessage(socket2, msg_type) < 0) return (-145);

    // send name
    string str2(name);
    if(sendStringMessage(socket2, str2) < 0) return (-145);

    // send argTypes
    if(sendArrayMessage(socket2, argTypes) < 0) return (-145);
    

    /*

        send args
        
        convert all of the elements into string format, then send them out.

    */
    




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

    /*

        It creates a connection socket to be used for accepting connections from clients.

    */

	char   hostname[MAX_HOST_NAME + 1];
    int    socket1;
    struct sockaddr_in sockaddr1;
    
    memset(&sockaddr1, 0, sizeof(struct sockaddr_in)); /* clear our address */
    
    if((gethostname(hostname, MAX_HOST_NAME)) < 0) return (-100);
    
    cout << "debug: SERVER_ADDRESS " << hostname << endl;

    sockaddr1.sin_family = AF_INET;     
    sockaddr1.sin_addr.s_addr = INADDR_ANY;
    sockaddr1.sin_port = 0;                

    /* a connection socket to be used for accepting connections from clients. */
    if ((socket1 = socket(AF_INET, SOCK_STREAM, 0)) < 0) return (-101);
    
    /* bind address to socket */
    if (bind(socket1, (struct sockaddr*)&sockaddr1, sizeof(struct sockaddr_in)) < 0) {
        close(socket1);
        return (-102);
    }

    /* max # of queued connects */
    if((listen(socket1, MAX_CLIENT)) < 0) return (-103);


    /*

        It opens a connection to the binder.
        The server sends register requests to the binder via this connection.
        The connection is left open until the server is down.

    */

    char* binder_address = getenv("BINDER_ADDRESS");
    if(binder_address == NULL) return (-104);
    //cout << binder_address << endl;

    int portnum = atoi(getenv("BINDER_PORT"));  
    if (portnum == 0) return (-105);
    //cout << portnum << endl;

    struct sockaddr_in sockaddr2;
    struct hostent *binder;
    int socket2;

    if ((binder = gethostbyname(binder_address)) == NULL) return (-106);
    
    memset(&sockaddr2, 0, sizeof(struct sockaddr_in));

    memcpy((char *)&sockaddr2.sin_addr, binder->h_addr, binder->h_length); /* set address */
    sockaddr2.sin_family = binder->h_addrtype;
    sockaddr2.sin_port = htons(portnum);

    // create the socket for connection to the binder.
    if ((socket2 = socket(AF_INET, SOCK_STREAM, 0)) < 0) return (-107);

    // connect to the binder.
    if (connect(socket2, (struct sockaddr*)&sockaddr2, sizeof(struct sockaddr_in)) < 0) {
        close(socket2);
        return (-108);
    }

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

    int fd_client = 3;
    int fd_binder = 4;

    // send the message type first
    string msg_type = "REGISTER";
    if(sendStringMessage(fd_binder, msg_type) < 0) return (-122);

    // send the server_identifier
    char server_identifier[256 + 1];    
    if((gethostname(server_identifier, 256)) < 0) return (-120);
    string str1(server_identifier);
    if(sendStringMessage(fd_binder, str1) < 0) return (-122);

    // send the port number
    int server_port;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if((getsockname(fd_client, (struct sockaddr *)&addr, &len)) < 0) return (-121);
    server_port = ntohs(addr.sin_port);
    if(sendIntMessage(fd_binder, server_port) < 0) return (-122);

    // send name
    string str2(name);
    if(sendStringMessage(fd_binder, str2) < 0) return (-122);

    // send argTypes
    if(sendArrayMessage(fd_binder, argTypes) < 0) return (-122);
     
    //     receive binder's response
     
    string* response_type;
    int ret;

    if (receiveStringMessage(fd_binder, &response_type) < 0) return (-123);      
    if (receiveIntMessage(fd_binder, &ret) < 0) return (-123);


    /*


        need some thing here to store the procedure in the server.

        We can create an external file under the server folder
        to store the address of procedure.


    */

    return ret;
}


// helper for execution
void handleExecute(int fd){

    string* name;
    int* argTypes;
    string returnMessage = "";

    // procedure name

    if (receiveStringMessage(fd, &name) <0) return (-170);

    // argument types

    if (receiveArrayMessage(fd, argTypes) <0) return (-170);

    // arguments




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

    // this socket id is for clients.
    int listener = 3;

    fd_set master;      // master file descriptor list - currently connected fd
    fd_set read_fds;    // temp  file descriptor list for select()
    int fdmax;          // max file descriptor number
    
    int newsockfd;
    struct sockaddr_in cli_addr;
    socklen_t cli_addr_size;
    
    FD_ZERO(&master);   // clear sets
    FD_ZERO(&read_fds);
    
    // add listener to master set
    FD_SET(listener, &master);
    // keep track of the mas fd. so far, it's listener
    fdmax = listener;
    
    while (1) {
        read_fds = master;
        
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) <0) return (-160);
        
        // run through existing connections, look for data to read      
        for (int i=0; i<=fdmax; i++){
            // if we find some fd ready to read
            if (FD_ISSET(i, &read_fds)){
                // if ready to read from listener, handle new connection
                if (i == listener){
                    cli_addr_size = sizeof(cli_addr);
                    newsockfd = accept(listener, (struct sockaddr*)&cli_addr, &cli_addr_size);
                    if (newsockfd < 0) return (-161);

                    FD_SET(newsockfd, &master);     // add to master, since currently connected
                    if (newsockfd > fdmax)          // update fdmax
                        fdmax = newsockfd;                  
                }
                // else, if ready to read from client, handle new data
                else{
                    string* words;                  // by our mechanism, it should specify message type
                    int byteRead = receiveStringMessage(i, &words); // remember to delete words after usage!!!
                    if (byteRead < 0){
                        close(i);                   // SHOULD remove server (if i=server fd) from map !!!!!!!!!!!!!!!!!!! later
                        FD_CLR(i, &master);
                        delete words;
                        return (-162);
                    }
                    // else, if an attempt to receive a request fails, assume the client has quit
                    else if (byteRead == 0){
                        close(i);                   // SHOULD remove server (if i=server fd) from map !!!!!!!!!!!!!!!!!!! later
                        FD_CLR(i, &master);
                    }
                    // else, ok, we receive some request
                    else{       
                        string returnMessage;
                        int returnValue;
                        
                        // if server/client message - EXECUTE
                        if (*words == "EXECUTE"){
                            delete words;
                            handleExecute(i /*fd*/ );
                        }
                    }
                }// END         
            }// END new incoming data (fd)
        }// END fd loop
    } //END while
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


    
}

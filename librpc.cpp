#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <cstring>
#include <stdlib.h>     
#include <unistd.h>     
#include <cctype>       
#include <sstream>

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
    
    /*
    
     Connect to the binder first
     
    */
    char* binder_address = getenv("BINDER_ADDRESS");
    if(binder_address == NULL) return (-140);
    
    int portnum = atoi(getenv("BINDER_PORT"));
    if(portnum == 0) return (-141);
    
    struct sockaddr_in sockaddr2;
    struct hostent *binder;
    int socket1;
    
    if ((binder = gethostbyname(binder_address)) == NULL) return (-142);
    
    memset(&sockaddr2, 0, sizeof(struct sockaddr_in));
    
    memcpy((char *)&sockaddr2.sin_addr, binder->h_addr, binder->h_length); /* set address */
    sockaddr2.sin_family = binder->h_addrtype;
    sockaddr2.sin_port = htons(portnum);
    
    // create the socket for connection to the binder.
    if ((socket1 = socket(AF_INET, SOCK_STREAM, 0)) < 0) return (-143);
    
    // connect to the binder.
    if (connect(socket1, (struct sockaddr*)&sockaddr2, sizeof(struct sockaddr_in)) < 0) {
        close(socket1);
        return (-144);
    }
    
    /*
     
     After connecting to binder, send location request message
     
    */
    int message_length, message_length_htonl, bytesSent;
    int msg_type = 110;
    int msg_type_htonl;
    string message = "";
    
    // add name
    message += name;
    cout << "message: " << message << endl;
    
    // add artTypes
    for(int i = 0; ; i++){
        if(argTypes[i] == 0) break;
        message += " ";
        message += to_string(argTypes[i]);
    }
    cout << "message: " << message << endl;

    // send the message length first.
    message_length = message.length() + 1;
    message_length_htonl = htonl(message_length);
    bytesSent = send(socket1, &message_length_htonl, sizeof(int), 0);
    // failure case
    if(bytesSent <= 0) return (-145);
    
    // followed by type
    msg_type_htonl = htonl(msg_type);
    bytesSent = send(socket1, &msg_type_htonl, sizeof(int), 0);
    // failure case
    if(bytesSent <= 0) return (-145);
    
    // followed by message
    char* str = (char*) message.c_str();
    
    for(int bytesLeft = message_length; bytesLeft > 0; ){
        bytesSent = send(socket1, str, bytesLeft, 0);
        if(bytesSent <= 0) return (-145);
        str = str + bytesSent;
        bytesLeft = bytesLeft - bytesSent;
    }
    
    /*
     
        Get the binder's response
     
    */
    int bytesRecv;
    
    // get the first 4 bytes for the Message Length.
    bytesRecv = recv(fd_binder, &message_length, sizeof(int), 0);
    if(bytesRecv <= 0) return (-123);
    message_length = ntohl(message_length);
    
    // get the next 4 bytes for the Message Type.
    bytesRecv = recv(socket1, &msg_type, sizeof(int), 0);
    if(bytesRecv <= 0) return (-146);
    msg_type = ntohl(msg_type);
    
    // LOC_FAILURE
    if(msg_type == 112){
        int errMsg;
        // get the next 4 bytes for the error message
        bytesRecv = recv(socket1, &errMsg, sizeof(int), 0);
        if(bytesRecv <= 0) return (-146);
        errMsg = ntohl(errMsg);
        return errMsg;
    }
    
    // LOC_SUCCESS : server_identifier, port
    char* msg = new char[message_length];
    
    // Since the number of bytes actually receive might be less than the number it was set to receive.
    // Need a loop to receive the complete string.
    for(int bytesLeft = message_length; bytesLeft > 0; ){
        bytesRecv = recv(socket1, msg, bytesLeft, 0);
        if(bytesRecv <= 0) return (-146);
        bytesLeft = bytesLeft - bytesRecv;
        msg += bytesRecv;
    }
    msg -= message_length;
    
    string receivedMsg(message);
    
    
    

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
    
    cout << "SERVER_ADDRESS " << hostname << endl;

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

    int message_length, message_length_htonl, bytesSent;
    int msg_type = 100;
    int msg_type_htonl;
    string message = "";

    // construct message

    // add server_identifier
    char   server_identifier[256 + 1];    
    if((gethostname(server_identifier, 256)) < 0) return (-120);
    message += server_identifier;
    message += " ";
    cout << "message: " << message << endl;

    // add port
    int portnum;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    if((getsockname(fd_client, (struct sockaddr *)&addr, &len)) < 0) return (-121);
    portnum = ntohs(addr.sin_port);
    message += to_string(portnum);
    message += " ";
    cout << "message: " << message << endl;

    // add name
    message += name;
    cout << "message: " << message << endl;

    // add artTypes
    for(int i = 0; ; i++){
        if(argTypes[i] == 0) break;
        message += " ";
        message += to_string(argTypes[i]);
    }
    cout << "message: " << message << endl;


    // send the message length first.
    message_length = message.length() + 1;
    message_length_htonl = htonl(message_length);
    bytesSent = send(fd_binder, &message_length_htonl, sizeof(int), 0);
    // failure case
    if(bytesSent <= 0) return (-122);
    
    // followed by type
    msg_type_htonl = htonl(msg_type);
    bytesSent = send(fd_binder, &msg_type_htonl, sizeof(int), 0);
    // failure case
    if(bytesSent <= 0) return (-122);

    // followed by message
    char* str = (char*) message.c_str();

    for(int bytesLeft = message_length; bytesLeft > 0; ){
        bytesSent = send(fd_binder, str, bytesLeft, 0);
        if(bytesSent <= 0) return (-122);
        str = str + bytesSent;
        bytesLeft = bytesLeft - bytesSent;
    }

    /*
     
     
     receive binder's response
     
     
    */
    int bytesRecv, msg;
    // get the first 4 bytes for the Message Length (useless in this case).
    bytesRecv = recv(fd_binder, &msg, sizeof(int), 0);
    if(bytesRecv <= 0) return (-123);
    
    // get the next 4 bytes for the Message Type.
    bytesRecv = recv(fd_binder, &msg_type, sizeof(int), 0);
    if(bytesRecv <= 0) return (-123);
    msg_type = ntohl(msg_type);

    // get the next 4 bytes for the message
    bytesRecv = recv(fd_binder, &msg, sizeof(int), 0);
    if(bytesRecv <= 0) return (-123);
    msg = ntohl(msg);

    //
    return msg;
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

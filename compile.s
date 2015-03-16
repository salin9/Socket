make clean
make
make client1
g++ -c server_functions.h server_functions.c
g++ -c server_functions.h server_function_skels.h server_function_skels.c
g++ -c rpc.h server.c
g++ -L. server_functions.o server_function_skels.o server.o -lrpc -lpthread -o server

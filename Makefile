default: librpc.a

librpc.a: rpc.h message.h librpc.cpp
	g++ -c rpc.h message.h librpc.cpp
	ar -cvq librpc.a message.o librpc.o

binder.o: rpc.h message.h binder.cc
	g++ -c librpc.cpp message.c binder.cc

server.o: rpc.h server_functions.h server_function.c server_function_skels.h servr_function_skels.c server.c
	g++ -c rpc.h server_functions.h server_function.c server_function_skels.h servr_function_skels.c server.c

server: server.o librpc.a server_function.o server_function_skels.o
	g++ -L. server.o -lrpc -o server

client.o: rpc.h client.c
	g++ -c rpc.h client.c

client: client.o librpc.a
	g++ -L. client.o -lrpc -o client

clean:
	rm -f *.o *.a binder server client *.gch


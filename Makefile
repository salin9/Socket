CXX = g++

all:  binder librpc.a

librpc.a: librpc.o message.o
	ar -rcs $@ $^

binder: binder.o message.o
	${CXX} binder.cc message.cc -o binder

client: client.o
	${CXX} client.cc -o client librpc.a

server: server.o server_functions.o server_function_skels.o
	${CXX} -o server server.o server_functions.o server_function_skels.o librpc.a

clean:
	rm -f *.o *.a binder server client *.gch


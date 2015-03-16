CXX = g++

all:  binder librpc.a

librpc.a: rpc.o message.o
	ar -rcs $@ $^

binder: binder.o message.o
	${CXX} binder.cc message.cc -o binder

client1: client1.o librpc.a
	${CXX} -L. $^ -lrpc -lpthread -o $@

server: server.o server_functions.o server_function_skels.o librpc.a
	${CXX} -L. $^ -lrpc -lpthread -o $@

clean:
	rm -f *.o *.a binder server client *.gch


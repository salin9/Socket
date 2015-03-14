CXX	= g++
CXXFLAG	= -g -Wall -MMD
MAKEFILE_NAME = ${firstword ${MAKEFILE_LIST}}

BINDEROBJECTS = binder.o message.o
BINDERDEPENDS = ${BINDEROBJECTS:.o=.d}
BINDEREXEC = binder

SERVEROBJECTS = server.o server_functions.o server_function_skels.o
SERVREDEPENDS = ${SERVEROBJECTS:.o=.d}
SERVEREXEC = server

CLIENTOBJECTS = client1.o
CLIENTDEPENDS = ${CLIENTOBJECTS:.o=.d}
CLIENTEXEC = client1

LIB = librpc.a
LIBOBJECTS = librpc.o message.o
LIBDEPENDS = ${LIBOBJECTS: .o=.d}

EXECS = ${BINDEREXEC} ${SERVEREXEC} ${CLIENTEXEC}

all : ${BINDEREXEC} ${LIB}

${LIB}: ${LIBOBJECTS}
	ar -rcs $@ $^

${BINDEREXEC}: ${BINDEROBJECTS}
	${CXX} $^ -o $@
	
${BINDEROBJECTS} : ${MAKEFILE_NAME}

${SERVEREXEC}: ${SERVEROBJECTS} librpc.a
	${CXX} -L. $^ -lrpc -o $@

${SERVEROBJECTS} : ${MAKEFILE_NAME}
	
${CLIENTEXEC}: ${CLIENTOBJECTS} librpc.a
	${CXX} -L. $^ -lrpc -o $@

${CLIENTOBJECTS} : ${MAKEFILE_NAME}

-include ${DEPENDS}

clean:
	rm -f *.o *.d ${EXECS} ${LIB}

.PHONY : all clean


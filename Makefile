CXX	= g++
CXXFLAG	= -g -Wall -MMD
MAKEFILE_NAME = ${firstword ${MAKEFILE_LIST}}

BINDEROBJECTS = binder.o message.o
BINDERDEPENDS = ${BINDEROBJECTS:.o=.d}
BINDEREXEC = binder

SERVEROBJECTS = server.o server_functions.o server_function_skels.o librpc.o
SERVREDEPENDS = ${SERVEROBJECTS:.o=.d}
SERVEREXEC = server

CLIENTOBJECTS = client1.o librpc.o
DEPENDS1 = ${CLIENTOBJECTS:.o=.d}
CLIENTEXEC = client

OBJECTS = ${BINDEROBJECTS} ${SERVEROBJECTS} ${CLIENTOBJECTS}
EXECS = ${BINDEREXEC} ${SERVEREXEC} ${CLIENTEXEC}

all : ${EXECS}

${BINDEREXEC}: ${BINDEROBJECTS}
	${CXX} ${CXXFLAGS} $^ -o $@

${SERVEREXEC}: ${SERVEROBJECTS}
	${CXX} ${CXXFLAGS} $^ -o $@

${CLIENTEXEC}: ${CLIENTOBJECTS}
	${CXX} ${CXXFLAGS} $^ -o $@

${OBJECTS} : ${MAKEFILE_NAME}

-include ${DEPENDS}

clean:
	rm -f *.o *.d ${EXECS}

.PHONY : all clean


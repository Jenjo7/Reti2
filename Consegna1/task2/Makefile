CFLAGS =-ansi -Wpedantic -Wall 
LIBRARY=myfunction.h

all:	serverUDP.exe clientUDP.exe

clientUDP.exe:	clientUDP.o
		gcc -o clientUDP.exe clientUDP.o

clientUDP.o:	clientUDP.c
		gcc -c clientUDP.c

serverUDP.exe:	serverUDP.o
		gcc -o serverUDP.exe serverUDP.o

serverUDP.o:	serverUDP.c
		gcc -c serverUDP.c ${LIBRARY}

.PHONY: 	clean


clean:		
		rm -f serverUDP.o serverUDP.exe myfunction.o clientUDP.exe clientUDP.o

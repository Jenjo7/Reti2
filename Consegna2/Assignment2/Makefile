CFLAGS =-ansi -Wpedantic -Wall 

all:	netsimulator.exe

netsimulator.exe:	netsimulator.o
		gcc -o netsimulator.exe netsimulator.o

netsimulator.o:	netsimulator.c
		gcc -c netsimulator.c


.PHONY: 	clean


clean:		
		rm -f netsimulator.exe netsimulator.o

all: konkurs

konkurs:  main.o global.o
	mpicc main.o global.o -o bank -lpthread

global.o: global.c global.h
	mpicc global.c -c -Wall -lpthread

main.o: konkurs.c
	mpicc konkurs.c -c -Wall -lpthread

clear: 
	rm *.o
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <mpi.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include "global.h"
#include <string.h>

int DOCTORS_COUNT;
int SALON_SIZE;

int size,rank;
MPI_Datatype MPI_MESSAGE;

int lamportClock;
pthread_mutex_t clock_mtx = PTHREAD_MUTEX_INITIALIZER;

Stack *stack;

pthread_t apiThread, commThread, monitorThread;

bool end = false;

/* Tworzenie wlasnego typu wiadomosci */
void create_message_type();

/* Inicjalizacja - sprawdzenie */
void init(int argc,char **argv);

/* argument musi być, bo wymaga tego pthreads. Wątek komunikacyjny */
void *apiFunc(void *arg);
void *commFunc(void *ptr);
void *monitorFunc(void *ptr);

void sendMsg(int destination, int info_type, int data, int timestamp, int tag);
Message receiveMsg(int tag);

pthread_mutex_t stack_mtx = PTHREAD_MUTEX_INITIALIZER;
void pushMsg(Message *msg, MPI_Status status);
Message *popMsg(MPI_Status *status);
void receiveLoop();


int main(int argc,char **argv){	;
	init(argc, argv);
	receiveLoop();
	pthread_join(apiThread, NULL);
	MPI_Type_free(&MPI_MESSAGE);
    MPI_Finalize();
	return 0;
}

void create_message_type(){
   int blocklengths[3] = {1,1,1};
   MPI_Datatype types[3] = {MPI_INT,MPI_INT,MPI_INT};
   MPI_Aint offsets[3];
   offsets[0] = offsetof(Message, tag);
   offsets[1] = offsetof(Message, timestamp);
   offsets[2] = offsetof(Message, data);

   MPI_Type_create_struct(3, blocklengths, offsets, types, &MPI_MESSAGE);
   MPI_Type_commit(&MPI_MESSAGE);
}

void init(int argc,char **argv){
	printf("sdfsdf\n");
	if (argc < 3){
		printf("Not enough parameters. Usage: %s DOCTORS_COUNT SALON_SIZE\n", argv[0]);
		exit(-1);
	}
	else {
		DOCTORS_COUNT = (int) strtol(argv[1], (char **)NULL, 10);
		SALON_SIZE = (int) strtol(argv[2], (char **)NULL, 10);
	}

	int provided;

	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	if (provided < MPI_THREAD_MULTIPLE) {
		fprintf(stderr, "Brak wystarczającego wsparcia dla wątków - wychodzę!\n");
	    MPI_Finalize();
	    exit(-1);
	}

	create_message_type();

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    srand(rank);

    pthread_create(&apiThread, NULL, apiFunc, 0);
    pthread_create(&commThread, NULL, commFunc, 0);
    if (rank==0) {
		//pthread_create(&monitorThread, NULL, monitorFunc, 0);
    }
}

void *apiFunc(void *arg) {
	int modelsCount;
	while (!end) {
		lamportClock = 0;
		modelsCount = rand()%SALON_SIZE+1;
		struct timespec t = { modelsCount, 0 };
        struct timespec rem = { 1, 0 };
        nanosleep(&t,&rem)
        pthread_mutex_lock(&clock_mtx);
        lamportClock += modelsCount;
        pthread_mutex_unlock(&clock_mtx);
        
	}
	return 0;
}

void *commFunc(void *ptr) {
	Message *msg = NULL;
	MPI_Status status;

	while (!end) {
		while ((msg = popMsg(&status)) == NULL ) {
            pthread_yield(); 
        }

        /* symulowanie opoznienia */
        struct timespec t = { rand()%3+1, 0 };
        struct timespec rem = { 1, 0 };
        nanosleep(&t,&rem)

	}
	return (void *)0;
}

void sendMsg(int destination, int info_type, int data, int timestamp, int tag) {
	Message pkt;
	pkt.tag = info_type;
	pkt.data = data;
	pkt.timestamp = timestamp;

	MPI_Send(&pkt, 1, MPI_MESSAGE, destination, tag, MPI_COMM_WORLD);
}

Message receiveMsg(int tag) {
	Message pkt;
	MPI_Status status;
	MPI_Recv(&pkt, 1, MPI_MESSAGE, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
	return pkt;
}

void pushMsg(Message *msg, MPI_Status status) {
	Stack* newNode = malloc(sizeof(Stack));
   	newNode->msg = msg;
    newNode->status = status;

    pthread_mutex_lock(&stack_mtx);
    newNode->next = stack;
    stack = newNode;
    pthread_mutex_unlock(&stack_mtx);
}

Message* popMsg(MPI_Status *status) {
	Message *msg;
	pthread_mutex_lock(&stack_mtx);
	if (stack == NULL) {
		pthread_mutex_unlock(&stack_mtx	);
		return NULL;
	}
	msg = stack->msg;
	Stack* curr = stack;
	stack = stack->next;

	free(curr);
	return msg;
}

void receiveLoop() {
	MPI_Status status;
    Message message;
    while ( !end ) {
        MPI_Recv(&message, 1, MPI_MESSAGE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        Message *msg = malloc(sizeof(Message));
        memcpy(msg, (const char *)&message, sizeof(Message));
        //printf("Otrzymano %d od %d\n", msg->data, status.MPI_SOURCE);
        pushMsg(msg, status);
    }
}
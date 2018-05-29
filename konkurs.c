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

int modelsCount;
int lamportClock;
/* 2d array ackArray[size][4]
	[0]	- DOC_ACK
	[1] - docID
	[2] - SALON_ACK
	[3] - num of models
*/
int **ackArray;
int myRequestTime;
pthread_mutex_t clock_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stack_mtx = PTHREAD_MUTEX_INITIALIZER;

Stack *stack;

pthread_t apiThread, commThread, monitorThread;

bool end = false;

/* Tworzenie wlasnego typu wiadomosci */
void create_message_type();

/* Inicjalizacja - sprawdzenie */
void init(int argc,char **argv);
void finalize();

/* argument musi być, bo wymaga tego pthreads. Wątek komunikacyjny */
void *apiFunc(void *arg);
void *commFunc(void *ptr);
void *monitorFunc(void *ptr);

void sendMsg(int destination, int tag, int data, int timestamp);
Message receiveMsg(int tag);

//pthread_mutex_t stack_mtx = PTHREAD_MUTEX_INITIALIZER;
void pushMsg(Message *msg, MPI_Status status);
Message *popMsg(MPI_Status *status);
void receiveLoop();

bool checkDoc(int id);
int choseDoc();
bool checkSalon();
bool checkContest();


int main(int argc,char **argv){	;
	init(argc, argv);
	receiveLoop();
	pthread_join(apiThread, NULL);
	finalize();
	return 0;
}

void create_message_type(){
   int blocklengths[2] = {1,1};
   MPI_Datatype types[2] = {MPI_INT,MPI_INT};
   MPI_Aint offsets[2];
   //offsets[0] = offsetof(Message, tag);
   offsets[0] = offsetof(Message, timestamp);
   offsets[1] = offsetof(Message, data);

   MPI_Type_create_struct(2, blocklengths, offsets, types, &MPI_MESSAGE);
   MPI_Type_commit(&MPI_MESSAGE);
}

void randomDelay() {
	struct timespec t = { rand()%3+1, 0 };
    struct timespec rem = { 1, 0 };
    nanosleep(&t,&rem);
}

void init(int argc,char **argv){
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

    ackArray = (int**)malloc(sizeof(int*)*size);
    for (int i=0; i<size; i++) ackArray[i] = (int*)malloc(sizeof(int)*4);

    pthread_create(&apiThread, NULL, apiFunc, 0);
    pthread_create(&commThread, NULL, commFunc, 0);
}

void finalize() {
	free(stack);
	pthread_mutex_destroy(&stack_mtx);
	pthread_mutex_destroy(&clock_mtx);
	MPI_Type_free(&MPI_MESSAGE);
	MPI_Finalize();
}

void *apiFunc(void *arg) {
	srand(time(NULL)+rank);
	int i, docId;
	int iterations = 3;
	while (!end && iterations > 0) {
		/* reset zmiennych */
		pthread_mutex_lock(&clock_mtx);
		lamportClock = rank;
		modelsCount = rand()%SALON_SIZE+1;
		for (i=0; i<size; i++) {
			ackArray[i][0] = EMPTY_TAG;
			ackArray[i][1] = EMPTY_TAG;
			ackArray[i][2] = EMPTY_TAG;
			ackArray[i][3] = EMPTY_TAG;
		}
		pthread_mutex_unlock(&clock_mtx);
		randomDelay();

		/* Dostep do lekarza */
        pthread_mutex_lock(&clock_mtx);
        lamportClock += modelsCount;

        docId = choseDoc();

        ackArray[rank][0] = lamportClock;
        ackArray[rank][1] = docId;
        printf("[%d] Wysylam REQ_DOC_TAG | clock: %d | data: %d = docId\n", rank, lamportClock, docId);
        for (i=0; i<size; i++) {
        	if (i != rank) {
        		sendMsg(i, REQ_DOC_TAG, docId, lamportClock);
        	}
        }
        pthread_mutex_unlock(&clock_mtx);

        while (!checkDoc(docId)) {
        	pthread_yield();
        }

        
        printf("[%d] Wchodze do lekarza nr %d\n", rank, docId);
        sleep(10);
        printf("[%d] Wychodze od lekarza nr %d\n", rank, docId);

        pthread_mutex_lock(&clock_mtx);
        lamportClock += 1;

        /* wejscie do salonu */
        ackArray[rank][2] = lamportClock;
        ackArray[rank][3] = modelsCount;

        printf("[%d] Wysylam REQ_SALON_TAG | clock: %d | data: %d - liczba modelek\n", rank, lamportClock, modelsCount);
        for (i=0; i<size; i++) {
        	if (i != rank) {
        		sendMsg(i, REQ_SALON_TAG, modelsCount, lamportClock);
        	}
        }
        pthread_mutex_unlock(&clock_mtx);

        while (!checkSalon()) {
        	pthread_yield();
        }

        printf("[%d] Wchodze do salonu. Zajmuje %d miejsc.\n", rank, modelsCount);
        sleep(10);
        printf("[%d] Wychodze z salonu\n", rank);
        pthread_mutex_lock(&clock_mtx);
        lamportClock += 1;
        pthread_mutex_unlock(&clock_mtx);

        for (i=0; i<size; i++) {
        	sendMsg(i, READY, -1, lamportClock);
        }

        printf("[%d] Gotowy do konkursu!\n", rank);
        while (!checkContest()) {
        	pthread_yield();
        }
        printf("[%d] Konkurs start!\n", rank);

        //Zakoncz po 3 iteracjach
        iterations -= 1;
        if (iterations == 0 && rank == 0) {
        	for (i=1; i<size; i++) {
        			sendMsg(i, FINNISH_TAG, -1, lamportClock);
        	}
        }

        sleep(5);
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
        randomDelay();

        pthread_mutex_lock(&clock_mtx);
        /* synchronizacja zegara */
        if (lamportClock <= msg->timestamp) {
        	
        	lamportClock = msg->timestamp + 1;
        }

        /* reagowanie na wiadomosc */
        switch(status.MPI_TAG) 
        {
        	case REQ_DOC_TAG:
        		ackArray[status.MPI_SOURCE][1] = msg->data;									// zaznaczenie sobie ktory doktor zajety (zeby potem wybrac z najmniejsza kolejka)

        		if (ackArray[rank][1] != msg->data ||										// ubiega sie do innego lekarza lub nie ubiega sie wcale
        			msg->timestamp < ackArray[rank][0] || 										// wczesniejszy dostep
        			(msg->timestamp == ackArray[rank][0] && status.MPI_SOURCE < rank))			// w tym samym momencie ale wyzszy priotytet
        		{
        			sendMsg(status.MPI_SOURCE, DOC_ACK_TAG, -1, lamportClock);
        		}
        		break;
        	case DOC_ACK_TAG:
        		//printf("[%d] DOC_ACK_TAG od: %d\n", rank, status.MPI_SOURCE);
        		ackArray[status.MPI_SOURCE][0] = DOC_ACK_TAG;
        		break;
        	case REQ_SALON_TAG:
        		ackArray[status.MPI_SOURCE][0] = DOC_ACK_TAG;								// do salonu idzie po wyjsciu od lekarza, wiec wiadomo ze dany proces zwolnil miejsce
        		sendMsg(status.MPI_SOURCE, SALON_ACK_TAG, modelsCount, ackArray[rank][2]);
        		break;
        	case SALON_ACK_TAG:
        		//printf("[%d] SALON_ACK_TAG od: %d\n", rank, status.MPI_SOURCE);
        		ackArray[status.MPI_SOURCE][2] = msg->timestamp;
        		if (ackArray[status.MPI_SOURCE][3] == -1)
        			ackArray[status.MPI_SOURCE][3] = msg->data;
        		break;
        	case READY:
        		ackArray[status.MPI_SOURCE][3] = 0;
        		//ackArray[status.MPI_SOURCE][2] = -1;				//przestaje sie ubiegac
        		break;
        	case FINNISH_TAG:
        		end = true;
        		exit(0);
        		break;
        	default:
        		printf("Nieznany tag! Wychodze\n");
        		end = true;
        		exit(-1);
        		break;
        }
        pthread_mutex_unlock(&clock_mtx);
      
	}
	return (void *)0;
}

void sendMsg(int destination, int tag, int data, int timestamp) {
	Message pkt;
	//pkt.tag = info_type;
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
		pthread_mutex_unlock(&stack_mtx);
		return NULL;
	}
	*status = stack->status;
	msg = stack->msg;
	Stack* curr = stack;
	stack = stack->next;
	free(curr);
	pthread_mutex_unlock(&stack_mtx);
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

bool checkDoc(int id) {
	for (int i=0; i<size; i++) {
		if (ackArray[i][0] == EMPTY_TAG) return false;
	}

	return true;
}

int choseDoc() {
	int docs[DOCTORS_COUNT];

	for (int i=0; i<DOCTORS_COUNT; i++) docs[i] = 0;

	for (int i=0; i<size; i++) {
		if (ackArray[i][1] != -1) docs[ackArray[i][1]]++;
	}

	int min = 99999;
	//int bestDoc = -1;

	for (int i=0; i<DOCTORS_COUNT; i++) {
		if (docs[i] < min) {
			//bestDoc = i;
			min = docs[i];
		}
	}
	return rand()%DOCTORS_COUNT;
}

bool checkSalon() {

	//dopoki nie mam info o liczbie modelek kazdego procesu to false
	for (int i=0; i<size; i++) {
		if (ackArray[i][3] == -1) return false;
	}

	int seatsTaken = 0;
	for (int i=0; i<size; i++) {
		if (i != rank) {
			if (ackArray[i][2] >= 0 && (ackArray[i][2] < ackArray[rank][2] || (ackArray[i][2] == ackArray[rank][2] && i < rank))) {
				seatsTaken += ackArray[i][3];
			}
		}
	}

	//printf("[%d] seats: %d\n", rank, seatsTaken);
	if (SALON_SIZE-seatsTaken >= modelsCount) {
		return true;
	}
	else {
		return false;
	}
}

bool checkContest() {
	for (int i=0; i<size; i++) {
		if (ackArray[i][3] != 0) return false;
	}
	return true;
}
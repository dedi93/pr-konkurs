#define TAG_EMPTY -1

typedef struct Message {
  int tag;
  int timestamp;
  int data;   //doctor's id or how many sit's in salon
} Message;

typedef struct Stack_s {
	Message *msg;
	MPI_Status status;
	struct Stack_s *next;
} Stack;
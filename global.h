#define EMPTY_TAG -1
#define FINNISH_TAG 0
#define DOC_ACK_TAG 1
#define REQ_DOC_TAG 2
#define SALON_ACK_TAG 3
#define REQ_SALON_TAG 4


typedef struct Message {
  //int tag;
  int timestamp;
  int data;   //doctor's id or how many sit's in salon
} Message;

typedef struct Stack_s {
	Message *msg;
	MPI_Status status;
	struct Stack_s *next;
} Stack;
#include "server.h"
#include "handleClients.h"

int letterCount[26];
int completedClients = 0;
int num_clients; // the number of client threads
int msgqid;

void delete_msg_queue() {
  printf("Deleting message queue\n");
  msgctl(msgqid, IPC_RMID, NULL);
}


int main(int argc, char *argv[]){
  // server only take one argument
  // Usage: ./server [process_num]

  if(argc != NUMARGS) {
    printf("Invalid number of arguments! Usage: ./server [process_num]");
    return -1;
  }
  num_clients = atoi(argv[1]);
  
  // Print log for server start
  timestamp();
  printf("Server starts...\n");

  // intialize letterCount content to zero
  for (int i = 0; i < ALPHACOUNT; i++) {
    letterCount[i] = 0;
  }

  pid_t childpid = fork();
  if(childpid == -1) {
    perror("Failed to fork\n");
    return -1;
  }
  if(childpid == 0) {
    execl("/bin/ipcrm", "ipcrm", "-Q", "1000", NULL); // Delete any previous message queue before starting a new message queue
  }
  else if(childpid != wait(NULL)) {
    perror("Parent failed to wait due to signal or error");
    return -1;
  }

  // get access to msg Queue using msgget()
  msgqid = msgget((key_t) 1000, 0666 | IPC_CREAT | IPC_EXCL);
  if(msgqid == -1) {
    perror("Error with msgget() in server\n");
  }

  // create threads to handle incoming client requests
  // num_clients: the number of threads
  // pthread_create() should call processClients() (defined in handleClients.c) and the param should be struct thd_data* 
  // NOTE: clientID starts from 1

  // Create array to store TIDs
  pthread_t* tid = (pthread_t*) malloc(num_clients * sizeof(pthread_t));
  // Error allocating memory for TIDs
	if(tid == NULL) {
		perror("Error in allocating memory for tid array using malloc()");
		exit(EXIT_FAILURE);
	}

  // Create array to store arguments of processClients for each thread
  struct thd_data* thd_data = (struct thd_data*) malloc(num_clients * sizeof(struct thd_data));
  for(int i = 0; i < num_clients; i++) {
    thd_data[i].msgqueue = msgqid;
    thd_data[i].clientID = i+1;
  }

  // Create threads
  for (int i = 0; i < num_clients; i++) {
    int retval = pthread_create(&tid[i], NULL, processClients, &thd_data[i]);
    // Error creating threads
		if(retval != 0) {
			perror("Error in creating a thread for processClients() function using pthread_create()");
			free(tid);
      free(thd_data);
      tid = NULL;
      thd_data = NULL;
			exit(EXIT_FAILURE);
		}
  }

  // join all the threads

  for(int i = 0; i < num_clients; i++) {
    int retval = pthread_join(tid[i], NULL);
    if(retval != 0) {
      perror("Error in waiting for a thread executing processClients() using pthread_join()");
      free(tid);
      free(thd_data);
      tid = NULL;
      thd_data = NULL;
      exit(EXIT_FAILURE);
    }
  }

  free(tid);
  free(thd_data);
  tid = NULL;
  thd_data = NULL;

  //close msgqueue and return
  msgctl(msgqid, IPC_RMID, NULL);

  // Print log for server end
  timestamp();
  printf("Server ends...\n");

  return 0;
}

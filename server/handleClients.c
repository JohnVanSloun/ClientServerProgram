#include "handleClients.h"
#include "server.h"

pthread_mutex_t clientCountLock = PTHREAD_MUTEX_INITIALIZER, letterLock = PTHREAD_MUTEX_INITIALIZER;

// print timestamp
// You can get timestamp using localtime()
void timestamp(){
  struct tm *log_time;
  time_t timestamp;

  time(&timestamp);
  log_time = localtime(&timestamp);
  char* timeStr = asctime(log_time);
  // Remove newline from end of time
  timeStr[strcspn(timeStr, "\n")] = 0;
  printf("[%s] ", timeStr);
}

// Count the occurence of all 26 letters for the first letter of each word in the file, and update letterCount[] correspondingly
void calculateLetterCount(char *filename){
  FILE *fp = fopen(filename, "r");
  int c;

  if (fp == NULL) {
    fprintf(stderr, "Failed to open file %s\n", filename);
    delete_msg_queue();
    exit(EXIT_FAILURE);
  }

  ssize_t read;
  ssize_t len = 0;
  char *line = NULL;

  while( ( read = getline(&line, &len, fp) ) != -1 ) {
    c = line[0]; // get first letter of line

    if(c != '\n' && ( ( (c % 65) < ALPHACOUNT ) || (c % 97) < ALPHACOUNT )) { // validate character input
      // note: characters represent ASCII values as integers
      if(c <= 90) { // first letter is uppercase
        pthread_mutex_lock(&letterLock);
        letterCount[c % 65]++;
        pthread_mutex_unlock(&letterLock);
      }
      else { // first letter is lowercase
        pthread_mutex_lock(&letterLock);
        letterCount[c % 97]++;
        pthread_mutex_unlock(&letterLock);
      }
    }
  }

  if(read != EOF) {
    fprintf(stderr, "getline() error in reading %s", filename);
    free(line);
    line = NULL;
    delete_msg_queue();
    exit(EXIT_FAILURE);
  }

  free(line);
  line = NULL;
  return;
}

// Create a string based on letterCount[], the final return character array 
// The string looks like count1#count2#....#count26#
char* convertLetterCountToChar(){
  char* letterCountString = (char*) malloc(MSGLEN * sizeof(char));
  if(letterCountString == NULL) {
    perror("Error in allocating memory in convertLetterCountToChar() function using malloc()");
    delete_msg_queue();
		exit(EXIT_FAILURE);
  }
  char numString[5];

  for (int i = 0; i < ALPHACOUNT; i++) {
    sprintf(numString, "%d", letterCount[i]);

    strcat(letterCountString, numString);

    if (i != (ALPHACOUNT - 1)) {
      strcat(letterCountString, "#");
    }
  }
  return letterCountString;
}

// called by threads created on server
// args: pointer to the thd_data struct of current client
void* processClients(void* args){

  struct thd_data thd_data = *((struct thd_data *) args);

  while (1) {
    struct msg_buffer buff;
    

    // Waiting to received from client process
    // Should store the message in buff
    timestamp();
    printf("Waiting to rcv from client process %d\n", thd_data.clientID);
    msgrcv(thd_data.msgqueue, (void *) &buff, sizeof(buff), thd_data.clientID, 0);

    // Handle the received message
    if (strcmp(buff.mesg_text, "END") == 0){
      // if the message is END:

      // wait for all threads to complete
      timestamp();
      printf("Thread %d received END from client process %d\n", thd_data.clientID, thd_data.clientID);
      pthread_mutex_lock(&clientCountLock);
      completedClients++;
      pthread_mutex_unlock(&clientCountLock);
      while(completedClients != num_clients);

      // Convert letter array to character array using convertLetterCountToChar()
      // and send it back to client
      // After that, break the while loop to exit the server
      char* result = convertLetterCountToChar();
      buff.mesg_type = thd_data.clientID;
      strncpy(buff.mesg_text, result, MSGLEN);
      timestamp();
      printf("Thread %d sending final letter count to client process %d\n", thd_data.clientID, thd_data.clientID);
      msgsnd(thd_data.msgqueue, (void*)&buff, sizeof(buff.mesg_text), 0);
      free(result);
      result = NULL;
      break;

    } else{
      // Remove newline from end of filepath
      buff.mesg_text[strcspn(buff.mesg_text, "\n")] = 0;

      // if the message is not END, then it can only be a file name (one line in clienti.txt)
      // Call calculateLetterCount() to count letters
      timestamp();
      printf("Thread %d received %s from client process %d\n", thd_data.clientID, buff.mesg_text, thd_data.clientID);
      calculateLetterCount(buff.mesg_text);
    }    

    // After the file is read completely, send an "ACK" message back
    timestamp();
    printf("Thread %d sending ACK to client %d for %s\n", thd_data.clientID, thd_data.clientID, buff.mesg_text);
    strncpy(buff.mesg_text, "ACK", MSGLEN);
    buff.mesg_type = thd_data.clientID+1000;
    msgsnd(thd_data.msgqueue, (void*)&buff, sizeof(buff.mesg_text), 0);
  }
}

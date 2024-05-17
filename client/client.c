#include <sys/types.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

#define MSGLEN 1024
#define NUMARGS 2
#define DIRNULL NULL

struct msg_buffer {
    long mesg_type;
    char mesg_text[MSGLEN];
};

// Should be the same function as in handleClients.c
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

// Traversal the file system recursively and write file pathes to mapper files (ClinetInput/clienti.txt
// mappers: the number of mapper files (equals to the number of clients
// fp[]: an array of file descriptors, each descriptor for the mapper file of one clinet 
// toInsert: the number of the next mapper file to insert file name. Pass the current value of `toInsert` when calling recursiveTraverseFS recursively
// nFiles: the total number of files you traversed
void recursiveTraverseFS(int mappers, char *basePath, FILE *fp[], int *toInsert, int *nFiles){
	struct dirent *dirContentPtr;

  long MAX_PATH = pathconf(".", _PC_PATH_MAX);

	if(MAX_PATH == -1) {
		perror("Failed to determine the pathname length");
		return;
	}

  char fpath[MAX_PATH];

	//check if the directory exists
	DIR *dir = opendir(basePath);
	if(dir == DIRNULL){
		printf("Unable to read directory %s\n", basePath);
		exit(1);
	}

	while((dirContentPtr = readdir(dir)) != DIRNULL){
    		// This while loop traversal all folder/files under `dir`
    		// See https://www.gnu.org/software/libc/manual/html_node/Directory-Entries.html for directory entry formats    
    
		if (strcmp(dirContentPtr->d_name, ".") != 0 &&
			strcmp(dirContentPtr->d_name, "..") != 0 &&
      			strcmp(dirContentPtr->d_name, ".DS_Store") != 0 &&
      			(dirContentPtr->d_name[0] != '.'))
      			{
        			if (dirContentPtr->d_type == DT_REG){
          			// For a file, you write its name into a mapper file (pointed by one entry in fp[])
          			// NOTE: to balance the number of files per client, you can loop though all clients when distributing files
         		 	// e.f. Assume you have 3 clients, then file1 for client1, file2 for client2, file3 for client3, file4 for client1, file 5 for client2...
                sprintf(fpath, "%s/%s", basePath, dirContentPtr->d_name);
          			fprintf(fp[*toInsert], "%s\n", fpath);

          			*nFiles += 1;
         			  *toInsert = (*toInsert + 1) % mappers;
			        }else if (dirContentPtr->d_type == DT_DIR){
          			// For a directory, you call recursiveTraverseFS()
                sprintf(fpath, "%s/%s", basePath, dirContentPtr->d_name);
          			recursiveTraverseFS(mappers, fpath, fp, toInsert, nFiles);
        		  }
		}
	}
}

// Wrapper function for recursiveTraverseFS
// create folder ClientInput and inside the folder create txt file for each client (i.e., Client0.txt)
// After that, call traverseFS() to traversal and partition files
void traverseFS(int clients, char *path){
	FILE *fp[clients];

	//Create a folder 'ClientInput' to store CLient Input Files
	struct stat st;
	if (stat("ClientInput", &st) == -1) {
		int ClientInput = mkdir("ClientInput", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

		if (ClientInput < 0) {
			printf("Failed to create ClientInput directory\n");
			exit(1);
		}
	}
  // open client input files to store paths of files to be processed by each server thread
	int i;
	for (i = 0; i < clients; i++){
		// create the mapper file name (ClinetInput/clienti.txt)
    char filename[32];
    sprintf(filename, "ClientInput/client%d.txt", i);

    fp[i] = fopen(filename, "w");

    if (fp[i] == NULL) {
      printf("Failed to create client%d file", i);
      exit(1);
    }
	}

	// Call recursiveTraverseFS
	int toInsert = 0; //refers to the File to which the current file path should be inserted
	int nFiles = 0;
	recursiveTraverseFS(clients, path, fp, &toInsert, &nFiles);

	// close all the file pointers
  	for (i = 0; i < clients; i++) {
    		fclose(fp[i]);
  	}
}

int main(int argc, char *argv[]){ 
  // logging client start
  timestamp();
  printf("Client starts...\n");

  // Usage: ./client [input folder] [process num]
  char folderName[100] = {'\0'};
  strcpy(folderName, argv[1]);
  int num_clients = atoi(argv[2]);

  // logging file traversal and partitioning
  timestamp();
  printf("Traversing and Partitioning Files\n");

  // call traverseFS() to traverse and partition files
  traverseFS(num_clients, folderName);

  //Get access to the msg Queue
  struct msg_buffer mbuf;
  int msgqid = msgget((key_t) 1000, 0666 | IPC_CREAT);

  // Create folder for outputs
  struct stat st;
  if (stat("Output", &st) == -1) {
	  mkdir("Output", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  }

  // Create `num_clients` children processes using fork()
  pid_t wpid;
  int status = 0;

  for (int i=0; i<num_clients; i++){
    pid_t pid = fork();
    if (pid==0){
      // For each client process, send each line of clienti to server
      char line[MSGLEN]={'\0'};

      char filename[32];
      sprintf(filename, "ClientInput/client%d.txt", i);

      FILE * ftr = fopen(filename, "r"); // ftr should point to the correct clienti.txt
      
      if (ftr == NULL) {
        printf("Failed to open file: %s\n", filename);
        exit(1);
      }

      while (fgets(line, MSGLEN, ftr) != NULL ) {
        // Send line
	      strcpy(mbuf.mesg_text, line);
        
        // Remove newline from end of filepath
        line[strcspn(line, "\n")] = 0;

        mbuf.mesg_type = i+1;
        msgsnd(msgqid, &mbuf, sizeof(mbuf.mesg_text), 0);

        // logging filepath send to server
        timestamp();
        printf("Process %d sent file %s to server\n", getpid(), line);

        // wait for ACK from server before sending the next line
        msgrcv(msgqid, &mbuf, sizeof(mbuf.mesg_text), i+1+1000, 0);

        // logging receival of ACK from server
        timestamp();
        printf("Process %d received acknowledgement for file %s from server\n", getpid(), line);
      }

      fclose(ftr);

      // When finish sending all the lines in clienti.txt
      // send END message to server
      strncpy(mbuf.mesg_text, "END", MSGLEN);
      mbuf.mesg_type = i+1;
      msgsnd(msgqid, &mbuf, sizeof(mbuf.mesg_text), 0);

      // logging send of END to server
      timestamp();
      printf("Process %d sent \"END\" to server\n", getpid());

      //Wait with msgrcv() for the result (output string)
      msgrcv(msgqid, &mbuf, sizeof(mbuf.mesg_text), i+1, 0);
      
      // logging receival of final result from server
      timestamp();
      printf("Final result received from server: %s\n", mbuf.mesg_text);
      
      //write output to file
      sprintf(filename, "Output/client%d_out.txt", i);

      FILE * output_file = fopen(filename, "w");

      if (output_file == NULL) {
        printf("Failed to open Output file");
        exit(1);
      }

      fprintf(output_file, "%s\n", mbuf.mesg_text);

      fclose(output_file);

      exit(0);
    }
  } 

  while ((wpid = wait(&status)) > 0);

  // logging client end
  timestamp();
  printf("Ending Client\n");

  //parent process waits for all children to finish
  return 0;
}

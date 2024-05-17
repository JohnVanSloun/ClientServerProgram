#ifndef HANDLECLIENTS_H
#define HANDLECLIENTS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#define DATAFLDR "../data"
#define ALPHACOUNT 26
#define NUMARGS 2
#define MSGLEN 1024

extern int letterCount[26];
extern int completedClients;
extern int num_clients;

struct thd_data{
	int msgqueue;
	int clientID;
};

struct msg_buffer {
    long mesg_type;
    char mesg_text[MSGLEN];
};

extern pthread_mutex_t clientCountLock, letterLock;

void timestamp();
void* processClients(void* args);

#endif

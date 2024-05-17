CC=gcc
LDFLAGS=-pthread
DBFLAGS=-g
OBJ=obj

all: server.o handleClients.o
	$(CC) $(DBFLAGS) $(LDFLAGS) -o server server.o handleClients.o

server.o: server.c
	$(CC) $(DBFLAGS) $(LDFLAGS) -c server.c

handleClients.o:handleClients.c
	$(CC) $(DBFLAGS) $(LDFLAGS) -c handleClients.c

.PHONY: clean
	
clean:
	rm server *.o

CC = gcc
LDLIBS = -lpthread

all: Server Client

Server: Server.o 

Client: Client.o

Server.o: Server.c 

Client.o: Client.c

clean:
	rm -f *.o
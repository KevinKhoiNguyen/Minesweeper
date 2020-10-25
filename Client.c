#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h>
#include <string.h> 

#define BUFFER 100
#define QUERYBUFFER 200

void sigInterruptHandleClient(int sig_no);
bool connection;

int main(int argc, char *argv[]) {
	
	int sockfd, numbytes;
	struct hostent *he;
	struct sockaddr_in their_addr; /* connector's address information */
	char message[BUFFER];
	connection = true;
	signal(SIGINT, sigInterruptHandleClient);

	/* Check number of arguments */
	if (argc != 3) {
		fprintf(stderr,"Please Specify the host and Port num to server\n");
		exit(1);
	}

	if ((he=gethostbyname(argv[1])) == NULL) {  /* get the host info */
		perror("gethostbyname");
		exit(1);
	}

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {	
		perror("socket");
		exit(1);
	}

	their_addr.sin_port = htons(atoi(argv[2]));    /* short, network byte order */
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);
	their_addr.sin_family = AF_INET;   

	if (connect(sockfd, (struct sockaddr *)&their_addr, \
	sizeof(struct sockaddr)) == -1) {
		perror("connect");
		exit(1);
	}

	// Loop that is able to block until the client specifies a query to the server
    system("clear");
    while (connection) {
        if ((numbytes=recv(sockfd, message, BUFFER, 0)) == -1) {
            perror("recv");
            break;
        }
        if (numbytes > 0) {
            // Check that a message was received and append it with \0
            message[numbytes] = '\0';
            printf("%s",message);
            if (strcmp(&message[numbytes-1], ">") == 0) {
                // greater than symbol indicates reply is awaited from client
                char* to_send = malloc(QUERYBUFFER);
                fgets(to_send, QUERYBUFFER, stdin);
                sscanf(to_send, "%[^\n]", to_send);                
                send(sockfd, to_send, strlen(to_send)+1, 0);
                free(to_send);
            } else if (!strcmp(message, "quit")) {
                break;	
            }
        }
    }

	// Close socket and commence shutdown procedure
    close(sockfd);
    printf("\nClosed the connection to server\n");
	printf("Shutting down...");
	sleep(3);
    
    return EXIT_SUCCESS;
}

/* Signal Interrupt for when ctrl + c is pressed in terminal for clean shutdown */	
void sigInterruptHandleClient(int sig_no) {
	if (sig_no == SIGINT) {
		connection = false;
		printf("\nReceived Signal Interrupt - client is now closing...\n");
		sleep(3);
		exit(1);
	}
}
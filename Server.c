#include <arpa/inet.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/wait.h> 
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include <ctype.h>
#include <time.h>
#include "MinesweeperLib.h"

int main(int argc, char *argv[]) { 
    connection = TRUE;
    signal(SIGINT, sigInterruptServerHandle);
    sem_init(&full, 0, 0);
    sem_init(&empty, 0, NUMTHREADS);
    sem_init(&reader,0,1);
    sem_init(&writer,0,1);
    pthread_mutex_init(&queueLock, NULL);
    pthread_mutex_init(&randLock, NULL);
    
    /* Initialise socket variables */
	int new_fd;  /* listen on sock_fd (Global), new connection on new_fd */
	int port=DEFAULTPORT;
    struct sockaddr_in my_addr;    /* my address information */
	struct sockaddr_in their_addr; /* connector's address information */
	socklen_t sin_size;

    /* Get port number for server to listen on */
    if (argc == 2) {
		port=atoi(argv[1]);
	}

	/* generate the socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	/* generate the end point */
	my_addr.sin_family = AF_INET;         /* host byte order */
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */
	bzero(&(my_addr.sin_zero), 8);  /* Zero the rest of the struct */

	/* bind the socket to the end point */
	if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) \
	== -1) {
		perror("bind");
		exit(1);
	}

	/* start listnening */
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}
	printf("server starts listnening ...\n");

    /* Start up the 10 threads to monitor each client */
    for (int i = 0; i<NUMTHREADS; i++) {
        pthread_create(&threadpool[i], NULL, thread_worker, NULL);
        printf("thread number %d has started\n", 1+i);
    }

    /* Load the Authentication File onto the stack */
    printf("Loading in Authentication.txt...\n");
    LoadAuthenticationFile();
    printf("Done!\n");

    /* Queue Created for sockets who want to be Connected*/
    queue = (queue_t*)malloc(sizeof(queue_t*));
    queue->queue_i = malloc(sizeof(int)*NUMTHREADS);
    queue->head = 0;
    queue->tail = 0;

    /* Accept loop (Producer) */
    while(connection == TRUE) {
        if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
            printf("Error in accepting thread for connection...\n");
            break;
        } else {
            sem_wait(&empty); // waits until empty space in queue
            pthread_mutex_lock(&queueLock); // Critical Section
            addToQueue(queue, new_fd);
            pthread_mutex_unlock(&queueLock); 
            printf("\nNew connection on socket: %d\n", new_fd);
            sem_post(&full); // Available to consumed by thread
        }
    }

    /* Cancel all threads while waiting for them to quit */
    for (int i = 0; i < NUMTHREADS; i++) {
        pthread_cancel(threadpool[i]);
        pthread_join(threadpool[i], NULL);
        printf("Thread number %d has stopped...\n",1+i);
    }

    return EXIT_SUCCESS;
}

/* threadpool worker thread */
void* thread_worker(void* arg) {
    while (connection == TRUE) {
        sem_wait(&full); // wait for a new connection to consume
        pthread_mutex_lock(&queueLock); // critical section
        int socket = deleteFromQueue(queue);    
        pthread_mutex_unlock(&queueLock);
        sem_post(&empty); // announce that a space is available in queue
        function_client(socket); // launch function that will interact with the client
    }
    return NULL;
}

/* Launches the function that each thread uses to perform server computations
for client to play game */
void function_client(int sockID) {
    //initialise state of game, the user's session (GameState) and the playing field
    int i=1;    
    GameState Game = {.games_played=0, .games_won=0};
    Tile t[NUM_TILES_X][NUM_TILES_Y];

    while(connection == TRUE) {   
        //user has quit so clean exit     
        if (i==EXIT) {
            sendMsg(sockID, "quit");
            break;
        }        
        if (i==INTRO) {
            if (intro(sockID,&Game)) i = MAINMENU;   //go to main menu
            else i=EXIT;
        }
        
        if (i==MAINMENU){
            i = mainmenu(sockID,&Game,t);          
        }

        if (i==LEADERBOARD)
        {
            //reader
            sem_wait(&reader);
            readerCount++;
            if (readerCount==1) sem_wait(&writer);
            sem_post(&reader);

            printLeaderboard(sockID,head);

            sem_wait(&reader);
            readerCount--;
            if (readerCount==0) sem_post(&writer);
            sem_post(&reader);
            
            i = MAINMENU;
        }
        
        if (i==MINESWEEPER) {
            i = minesweeper(sockID,&Game,t);
        }

        if (i==REVEAL) {
            i = Reveal(sockID,t);
        }

        if (i==PLACE) {
            i = Place(sockID,&Game,t);
        }

        if (i==SHOWALL) {
            ShowBoard(sockID,&Game,t);
            i = MAINMENU;
        }
    }
    close(sockID);
    printf("socket %d closed\n", sockID);
}


/* Clean Exit to Destroy and free all created variables, structs 
Cancel all threads while waiting for them to quit */
void finalExit() {
    for (int i = 0; i < NUMTHREADS; i++) {
        pthread_cancel(threadpool[i]);
        pthread_join(threadpool[i], NULL);
        printf("Thread number %d has stopped...\n",1+i);
    }
    pthread_mutex_destroy(&queueLock);
    pthread_mutex_destroy(&randLock);
    sem_destroy(&reader);
    sem_destroy(&writer);
    sem_destroy(&full);
    sem_destroy(&empty);
    free(queue->queue_i);
    free(queue);
    close(sockfd);
    printf("Shutting down...");
    sleep(3);
    exit(1);
}


/* Signal Interrupt Function which closes the server cleanly */
void sigInterruptServerHandle(int sig_no) {
    if (sig_no == SIGINT) {
        connection = FALSE;
        printf("\nReceived Signal Interrupt server is now closing...\n");
        finalExit();
    }
}


/* The following functions all relate to the operation of the server*/
/*------------------------------------------------------------------*/

/* Read the usernames and passwords from the Authentication.txt file */
void LoadAuthenticationFile() {
    // str stores one line of the text file at a time
    char str[BUFFER];
    char* filename = "Authentication.txt";
    char username[BUFFER];
    int password;
    FILE* fp = fopen(filename, "r");
    if (fp==NULL) {
        printf("Could not open file %s", filename);
        return;
    }
    int counter = 0;
    while(fgets(str, BUFFER, fp) != NULL) {
        sscanf(str,"%s", username);
        if (strcmp(username,"Username")) {
            password = GetNumber(str);
            passwords[counter] = password;
            strcpy(users[counter],username);
            counter++;
        }
        
    }
}

/* Gets the required password from a line of the authentication text file */
int GetNumber(const char *str) {
  while (!(*str >= '0' && *str <= '9') && (*str != '-') && (*str != '+')) str++;
  int number;
  if (sscanf(str, "%d", &number) == 1) {
    return number;
  }
  // No int found
  return -1; 
}


/* Verification function which checks if the given username and password 
is in the arrays of username and passwords initialised on the stack */
int Verify(char* username, int password) {
    int cont = 0;
    for (int i = 0; i < NUM_AUTH_USERS; i++) {
        if (!strcmp(username,users[i]) && password == passwords[i]) {
        cont = 1;
        }
    }
    return cont;
}


/* 'send Msg' is an interface for the send function to the client
*/
void sendMsg(int sockID, char* msg) {
	size_t length = strlen(msg);
	if (send(sockID, (char*)msg, length, 0) == -1)
		perror("Error in Sending Message to client");
}


/* 'receiveMsg' blocks the process and waits for a response from the server
*/
void receiveMsg(int sockID, char* msg) {
	int numBytes;
	while (1) {
		if ((numBytes=recv(sockID, msg, QUERYBUFFER, 0)) == -1) {
			printf("Error in receiving message from client");
			exit(1);
		}
		if (numBytes > 0) break;
	}
}


/* Adds val to index head in queue */
void addToQueue(queue_t* queue, int val) {
    queue->queue_i[queue->head] = val;
    queue->head = (queue->head+1) % NUMTHREADS;    
    // head is incremented to keep track of which index to insert val at next time
}


/* Removes and returns index tail from queue struct */
int deleteFromQueue(queue_t* queue) {
    int val;
    val = queue->queue_i[queue->tail];
    queue->tail = (queue->tail+1) % NUMTHREADS;
    // tail is incremented to keep track of which index to remove next time
    return val;
}


/* The Functions Below hold the game logic and operation of the minesweeper game */
/*-------------------------------------------------------------------------*/


/* Prints the Leader board by sending Message to the Socket ID 
This is the READER of the leaderboard */
void printLeaderboard(int sockID, struct Leaderboard *n) 
{ 
    //Nothing in leaderboard
    if (n==NULL) {
        sendMsg(sockID,"========================================================\nLeaderboard currently has no entries, come back later\n========================================================\n");
    } else {
        char string[100];
        sendMsg(sockID,"\n\n ===========================LEADERBOARD===========================\n");
        sendMsg(sockID,"Player:\tTime:\tGames Won:\tGames Played:\t\n");
        sendMsg(sockID,"------------------------------------------------------------------\n");
        //iterates through all nodes in linked list
        while (n != NULL) { 
            sprintf(string,"%s\t%d  \t%d\t\t%d \n", n->user,n->time,n->won,n->played); 
            sendMsg(sockID,string);
            n = n->next; 
        }
        sendMsg(sockID,"\n =================================================================\n\n");
    } 
} 


/* Inserts the new scores into the Leaderboard which thus gives the thread
as the Writer of the leaderboard (sorted in descending order by time)*/
void insert_into_leaderboard(struct Leaderboard** head_ref, GameState *Game, int new_time) { 
	//create new
	struct Leaderboard* new = (struct Leaderboard*) malloc(sizeof(struct Leaderboard)); 
	//store data in new
	new->time = new_time;
    new->won = Game->games_won;
    new->played = Game->games_played;
    strcpy(new->user,Game->user); 

    //first item to be put in list goes in first spot
    if (*head_ref==NULL) {
        new->next=NULL;
        *head_ref=new;
        return;
    } else {
        struct Leaderboard *current = *head_ref;
        //place new at front if largest in list
        if (new->time>current->time) {
            new->next=current;
            *head_ref=new;
            return;
        } else {
            struct Leaderboard *temp = current->next;
            while(temp!=NULL) {  
                //place inbetween current and the following node 
                if (new->time>temp->time) {
                    current->next=new;
                    new->next=temp;
                    return;
                } //end if
            //move to next item in list and check again
            current=temp;
            temp=temp->next;
            } //end while
            //place at end of list
            current->next=new;
            new->next=NULL;
            return;
        } //end if-else
    } //end outer if-else 
}


/* Initialises field for minesweeper */
int initialize_field(GameState *Game,Tile t[NUM_TILES_X][NUM_TILES_Y]) {
    
    //iterate through all tiles and set start values
    for (int i=0;i<NUM_TILES_X;i++) {
        for (int j=0;j<NUM_TILES_Y;j++) {
            t[i][j].is_mine=FALSE;
            t[i][j].revealed=FALSE;
            t[i][j].adjacent_mines=0;
            strcpy(t[i][j].icon," ");
        }
    }

    int x,y;

    //place mines in random tiles
    for (int i=0;i<NUM_MINES;i++) {
        do {
            x = getRandTilePosition();
			y = getRandTilePosition();
		} while (t[x][y].is_mine);
        
        t[x][y].is_mine = TRUE;
        printf("%d,%d\n",x,y);
    
        int x1,y1;

        //iterate through all the tiles around a mine
        //and add 1 to it's adjacent mines
        for (int i=0;i<3;i++) {
            x1=x-1+i;
            for (int j=0;j<3;j++) {
            y1=y-1+j;
            if (x1>=0 && x1<9 && y1>=0 && y1<9) t[x1][y1].adjacent_mines+=1;
            } //end inner loop
        } //end middle loop
        t[x][y].adjacent_mines=0;
    } //end outer loop

    //initialize users game
    Game->remaining_mines = NUM_MINES;
    Game->games_played +=1;
    Game->start_time = time(NULL);
}


/* Sends the Introduction Screen to the User */
int intro(int sockID,GameState *Game) {

    char username[10],query[20];
    int password;

    //get username and password from client
    sendMsg(sockID, "\nWelcome to the online Minesweeper gaming system\nYou are required to log on with your registered username and password.\nUsername:\n>");
    receiveMsg(sockID,query);
    strcpy(username,query);
    sendMsg(sockID,"Password:  \n>");
    receiveMsg(sockID,query);
    sscanf(query,"%d",&password);

    //verify
    int valid = Verify(username,password);
    if (valid) strcpy(Game->user,username);
    return valid;
}


/* Sends Main menu to the User */
int mainmenu(int sockID,GameState *Game, Tile t[NUM_TILES_X][NUM_TILES_Y]) {
    char choice[20],query[20];
    int num = 0, i;

    //keep asking user for input until valid
    do {
        sendMsg(sockID,"\nPlease enter a selection:\n<1> Play Minesweeper \n<2> Show Leaderboard \n<3> Quit \n\nSelection option (1-3): \n>");
        receiveMsg(sockID,query);
        sscanf(query,"%d",&num);
    } while(num !=1 && num!=2 && num!=3);

    //Play game
    if (num==1) {
        initialize_field(Game,t);
        i=MINESWEEPER;  //go to minesweeper menu
    }
    // Change state to leaderboard
    if (num==2) i = LEADERBOARD;
    // Change state to EXIT
    if (num==3) i = EXIT;

    return i;        
}

/* Minesweeper Game */
int minesweeper(int sockID,GameState *Game, Tile t[NUM_TILES_X][NUM_TILES_Y]) {
    
    char str[100];
    char holder[30];
    char query[20];
    int i,time_taken;

    //display game stats
    sprintf(str,"\n %s\nRemaining mines: %d \n",Game->user,Game->remaining_mines);
    sendMsg(sockID,str);
    sprintf(str,"Games played: %d\n",Game->games_played);
    sendMsg(sockID,str);
    sprintf(str,"Games won: %d\n",Game->games_won);
    sendMsg(sockID,str);
    time_taken = difftime(time(NULL),Game->start_time);
    sprintf(str,"Current time: %d seconds.\n",time_taken);
    sendMsg(sockID,str);

    // Sends the play field to the user (iterate through tiles)
    sendMsg(sockID, "\t0  1  2  3  4  5  6  7  8\n ---------------------------------");
    for (int y=0;y<NUM_TILES_Y;y++) {
        sprintf(str,"\n %d | \t",y);
        for (int x =0; x<NUM_TILES_X; x++) {
            sprintf(holder,"%s  ",t[x][y].icon);
            strcat(str,holder);
        } //end inner for loop
        sendMsg(sockID,str);
    } //end outer loop

    // Send Client the options to choose from
    sendMsg(sockID,"\nChoose an option: \n");
    sendMsg(sockID,"<R> Reveal Tile\n");
    sendMsg(sockID,"<P> Place Flag\n");
    sendMsg(sockID,"<Q> Quit Game\n\n");

    //keep asking user for input until valid
    do{
        sendMsg(sockID,"Option (R, P, Q):\n>");
        receiveMsg(sockID,query);
    } while(!( !strcmp(query,"R") || !strcmp(query,"P") || !strcmp(query,"Q") ));

    //change state depending on user choice
    if (!strcmp(query,"R")) i= REVEAL;   
    if (!strcmp(query,"P")) i= PLACE;   
    if (!strcmp(query,"Q")) i= SHOWALL;

    return i;
}


/* Reveals the Tile at the given location */
int Reveal(int sockID,Tile t[NUM_TILES_X][NUM_TILES_Y]) {

    int x,y,i;
    char query[20];

    //continue asking user for coordinate inputs until valid
    do{
        sendMsg(sockID,"x co-ordinate of tile to be revealed (0-8): \n>");
        receiveMsg(sockID, query);
        sscanf(query,"%d",&x);
    } while (x<0 || x>8);
    
    do{
        sendMsg(sockID,"y co-ordinate of tile to be revealed (0-8): \n>");
        receiveMsg(sockID, query);
        sscanf(query,"%d",&y);
    } while (y<0 || y>8);

    char icon[10];

    //check state of that tile
    if (t[x][y].revealed) {
        sendMsg(sockID,"That tile has already been revealed\n");
        i=MINESWEEPER;
    } else {
        if (t[x][y].is_mine) {
            sendMsg(sockID,"Oh no! THat was a bomb :( Game over\n");
            i=SHOWALL;
        } else {
            sendMsg(sockID, "Yay! That was not a bomb!\n");
            //if adjacent bombs
            if (t[x][y].adjacent_mines!=0) {
                t[x][y].revealed=TRUE;
                snprintf(icon,BUFFER,"%d",t[x][y].adjacent_mines);
                strcpy(t[x][y].icon,icon);
            } else {
                //expand to show adjacent bombs for surrounding tiles
                int x1,y1;
                for (int i=0;i<3;i++) {
                    x1=x-1+i;
                    for (int j=0;j<3;j++) {
                        y1=y-1+j;
                        if (x1>=0 && x1<9 && y1>=0 && y1<9) {
                            t[x1][y1].revealed=TRUE;
                            snprintf(icon,BUFFER,"%d",t[x1][y1].adjacent_mines); 
                            strcpy(t[x1][y1].icon,icon);
                        } //end if
                    } //end inner loop
                } //end outer loop
            } 
            i=MINESWEEPER;
        }
    }
    return i;
}


/* Place */
int Place(int sockID,GameState *Game,Tile t[NUM_TILES_X][NUM_TILES_Y]){
    
    int x,y,i;
    char query[20];

    //continue asking user for coordinate inputs until valid
    do {
        sendMsg(sockID,"x co-ordinate of tile to be flagged (0-8): \n>");
        receiveMsg(sockID,query);
        sscanf(query,"%d", &x);
    } while(x<0 || x>9);

    do {
        sendMsg(sockID,"y co-ordinate of tile to be flagged (0-8): \n>");
        receiveMsg(sockID,query);
        sscanf(query,"%d",&y);
    } while(x<0 || x>9);

    //check state of tile
    if (t[x][y].revealed) {
        sendMsg(sockID,"That tile has already been revealed\n");
        i=MINESWEEPER;
    } else {
        //location has a mine
        if (t[x][y].is_mine) {
            Game->remaining_mines-=1;
            //has found all mines - WIN CONDITION
            if (Game->remaining_mines==0) {

                char str[50];
                int time_taken;
                
                sendMsg(sockID,"Congratulations! You found all the mines \n");
                Game->games_won +=1;
                time_taken = difftime(time(NULL),Game->start_time);
                sprintf(str,"Current time: %d seconds.\n",time_taken);
                
                //writer
                sem_wait(&writer);
                insert_into_leaderboard(&head,Game,time_taken);
                sem_post(&writer);
                sprintf(str,"Game completed in %d seconds.\n",time_taken);
                sendMsg(sockID,str);

                i = SHOWALL;
            } else {
                strcpy(t[x][y].icon,"*");
                t[x][y].revealed=TRUE;
                sendMsg(sockID,"Mine found and flagged!\n");
                i=MINESWEEPER;
            }

        } else {
            // Location doesn't have a mine
            sendMsg(sockID,"Mine not found...\n");
            i=MINESWEEPER;
        }
    }
    return i;
}


/* Shows the board */
int ShowBoard(int sockID, GameState *Game,Tile t[NUM_TILES_X][NUM_TILES_Y]){
    char icon[5];
    char holder[20];
    char str[100];

    //iterate through all tiles
    for (int y=0;y<9;y++) {
        for (int x=0;x<9;x++) {

            //show if mine
            if (t[x][y].is_mine) {
                strcpy(t[x][y].icon,"*");
            //show adjacent mine values
            } else {
                snprintf(icon,5,"%d",t[x][y].adjacent_mines); 
                strcpy(t[x][y].icon,icon);

            } //end if-else
        } //end inner loop
    } //end outer loop

    //display revealed field to client
    sendMsg(sockID, "REVEALED FIELD:\n\t0  1  2  3  4  5  6  7  8\n ---------------------------------");
    for (int y=0;y<NUM_TILES_Y;y++) {
        sprintf(str,"\n %d | \t",y);
        for (int x =0; x<NUM_TILES_X; x++) {
            sprintf(holder,"%s  ",t[x][y].icon);
            strcat(str,holder);
        } //end inner loop
        sendMsg(sockID,str);
    } //end outer loop
}


/*  Gets a Random position for the x,y coordinate of tile
utilising a mutex to allow the rand() function to be thread safe */
int getRandTilePosition() {
    pthread_mutex_lock(&randLock);
    int position = rand() % NUM_TILES_X;
    pthread_mutex_unlock(&randLock);
    return position;
}
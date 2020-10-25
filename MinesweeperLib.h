#ifndef __MINESWEEPER_LIB__
#define __MINESWEEPER_LIB__

/* Server Constants*/
#define NUMTHREADS 10
#define BACKLOG 10
#define QUERYBUFFER 100
#define DEFAULTPORT 12345

/* Minesweeper Constants*/
#define RANDOM_NUMBER_SEED 42
#define NUM_TILES_X 9
#define NUM_TILES_Y 9
#define NUM_MINES 10
#define BUFFER 20

//Authentication
#define USERNAME_BUFFER 15
#define NUM_AUTH_USERS 10
#define TRUE 1
#define FALSE 0

//States
#define EXIT 0
#define INTRO 1
#define MAINMENU 2
#define MINESWEEPER 3
#define LEADERBOARD 4
#define REVEAL 5
#define PLACE 6
#define SHOWALL 7


/* Structures */
typedef struct queue {
    int* queue_i;
    int head;
    int tail;
} queue_t;

typedef struct
{
	int adjacent_mines;
	int revealed;
	int is_mine;
	char icon[5];
} Tile;

typedef struct
{
	int remaining_mines;
    int games_played;
    int games_won;
    char user[10];
    time_t start_time;
    int time_taken;
} GameState;

struct Leaderboard  
{ 
  int time;
  char user[10];
  int won;
  int played;
  struct Leaderboard *next; 
}; 

/* Function declarations - Server Based functions*/
void sigInterruptServerHandle(int sig_no);
void* thread_worker(void* arg);
void function_client(int sockID);
void sendMsg(int sockID, char* msg);
void receiveMsg(int sockID, char* msg);
int deleteFromQueue(queue_t* queue);
void addToQueue(queue_t* queue, int val);
int GetNumber(const char *str);
int Verify(char* username, int password);
void LoadAuthenticationFile();
void finalExit();

/* Minesweeper based Functions*/
int getRandTilePosition();
int intro(int sockID,GameState *Game);
int mainmenu(int sockID,GameState *Game, Tile t[NUM_TILES_X][NUM_TILES_Y]);
int minesweeper(int sockID,GameState *Game, Tile t[NUM_TILES_X][NUM_TILES_Y]);
int Reveal(int sockID,Tile t[NUM_TILES_X][NUM_TILES_Y]);
int Place(int sockID,GameState *Game,Tile t[NUM_TILES_X][NUM_TILES_Y]);
int ShowBoard(int sockID, GameState *Game,Tile t[NUM_TILES_X][NUM_TILES_Y]);
void printLeaderboard(int sockID, struct Leaderboard *n);
void insert_into_leaderboard(struct Leaderboard** head_ref, GameState *Game, int new_time);

/* Global Variables */
pthread_t threadpool[NUMTHREADS];   // stores the 10 threads for clients
pthread_mutex_t queueLock, randLock;   //  Mutex necessary
sem_t full, empty;  // Semaphore for queue of clients
queue_t* queue;
int connection = FALSE;
char users[NUM_AUTH_USERS][USERNAME_BUFFER];
int passwords[NUM_AUTH_USERS]; 
int sockfd;
struct Leaderboard* head = NULL;
sem_t reader,writer;
int readerCount = 0;

#endif
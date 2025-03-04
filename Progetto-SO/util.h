#ifndef UTIL_H
#define UTIL_H

#include <sys/types.h>

//#define DEBUG_MODE

struct msgBuffer {
   long mtype;       /* message type, must be > 0 */
   int mtext[3];    /* message data */
};

union semun {
   int val;                // valore del SETVAL
   struct semid_ds* buf;   // buffer per IPC_STAT, IPC_SET
   unsigned short* array;  // array per GETALL, SETALL
};

// struct per la wait sul semaforo
extern struct sembuf decSem;
extern struct sembuf incSem;

extern int createAtom(pid_t *atomSID , pid_t master , unsigned int atomicNum);
extern int writeLog(int fd , char* text , ...);
extern int ignoreChildDeath();
extern int getRandValue();
extern int getEnvInt(char* name);
extern void term(pid_t master , int ERR);
extern void __error(char* format , ...);

#ifdef DEBUG_MODE
# define error     __error      /*Prints the formatted string on stderr*/
#else
# define error
#endif

#define max(n , m)  ((n) > (m) ? (n) : (m))

#define SHM_FISSION             0
#define SHM_PRODUCED_ENERGY     1
#define SHM_STORED_ENERGY       2
#define SHM_SCORIE              3
#define SHM_INHIB_PERCENTAGE    4
#define SHM_INHIB_WITHDRAWAL    5

#define USER_INT        0  
#define GENERIC_ERR     1  
#define MELTDOWN_ERR    2  
#define BLACKOUT        3
#define EXPLODE         4 
#define TIMEOUT         5  

#define SECOND    1000000000

#endif
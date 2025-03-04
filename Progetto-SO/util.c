#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/sem.h>
#include <sys/time.h>

#include "util.h"

// struct per operazioni sui semafori
struct sembuf decSem = {0 , -1 , 0};
struct sembuf incSem = {0 , 1 , 0};

/// @brief This function creates a new atom
/// @param atomSID is the session id of the atoms (-1 if the session is not initialized 
///                or NULL if the session id is the same as the parent)
/// @param master is the pid of the master process
/// @param atomicNum is the atomic number of the new atom (this function does not check if it's valid)
/// @return -1 if the fork failed, 0 otherwise
int createAtom(pid_t *atomSID , pid_t master , unsigned int atomicNum){
    pid_t pid = fork();
    if(pid == -1){
        return -1;
    }
    if(atomSID != NULL && *atomSID < 0){
        *atomSID = pid;
        setpgid(*atomSID , 0);
    }
    if(pid == 0){
        if(atomSID != NULL) setpgid(0 , *atomSID);

        char* n_atom_str = alloca((snprintf(NULL , 0 , "%d" , atomicNum) + 1) * sizeof(char));
        sprintf(n_atom_str , "%d" , atomicNum);

        char* master_str = alloca((snprintf(NULL , 0 , "%d" , master) + 1) * sizeof(char));
        sprintf(master_str , "%d" , master);

        execve("atomo", (char* []){"atomo" , master_str , n_atom_str , (char*) NULL}, environ);
        error("execve() creazione atomo");
        exit(EXIT_FAILURE);
    }
    return 0;
}

/// @brief ignores SIGCHLD (to avoid an invasion of zombies)
/// @return 1 for errors
int ignoreChildDeath(){
    struct sigaction chldIgnore;
    chldIgnore.sa_handler = SIG_IGN;
    sigemptyset(&(chldIgnore.sa_mask));
    chldIgnore.sa_flags = 0;
    
    return sigaction(SIGCHLD , &chldIgnore , NULL);
}

/// @brief Returns a random integer between 0 and RAND_MAX inclusive
int getRandValue(){
    struct timeval randSeed;
    gettimeofday(&randSeed , NULL);
    srand(randSeed.tv_usec);
    return rand();
}

/// @brief Returns the value associated with the NAME(-1 in case of errors)
int getEnvInt(char* name){
    char* value = getenv(name);
    if(value == NULL){
        printf("There is no variable named %s in the environment\n" , name);
        return -1;
    }
    return atoi(value);
}

/// @brief Ends the simulation (use only when an error occurred)
/// @param master is the pid of the master
/// @param ERR is the event that caused the termination
void term(pid_t master , int ERR){
    union sigval val;
    val.sival_int = ERR;
    if(sigqueue(master , SIGTSTP , val) == -1){
        error("Unable to send a signal to the master (pid = %d)" , master);
    }
    exit(EXIT_FAILURE);
}

/// @brief Support function. Don't use directly, use error(...)
void __error(char* format , ...){
    int errno_copy = errno;
    va_list args;
    va_start(args , format);
    char* buffer = alloca(vsnprintf(NULL , 0 , format , args) * sizeof(char));
    va_end(args);
    va_start(args , format);
    vsprintf(buffer , format , args);
    va_end(args);
    fprintf(stderr , "%s: %s\n" , buffer , strerror(errno_copy));
}

/// @brief Writes the formatted text on the file descriptor
/// @param fd is not checked (i thrust you)
/// @return the number of bytes written or -1 for errors
int writeLog(int fd , char* text , ...){
    time_t lTime = time(NULL);
    char* timeStr = ctime( &lTime );
    // removes ending \n
    char* term = strchr(timeStr , '\n');
    if(term) *term = '\0';
    //start ov variadic arguments
    va_list args;
    va_start(args , text);
    char* texBuf = alloca(vsnprintf(NULL , 0 , text , args));
    va_end(args);
    va_start(args , text);
    vsprintf(texBuf , text , args);
    va_end(args);
    // allocates and prints the whole string
    char* buf = alloca(snprintf(NULL , 0 , "%s  %s" , timeStr , texBuf));
    sprintf(buf , "%s  %s" , timeStr , texBuf);
    return write(fd , buf , strlen(buf));
}
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/sem.h>

#include "util.h"

int main(int argc , char** argv){
    if(ignoreChildDeath() == -1){
        error("sigaction()");
        term(getppid() , GENERIC_ERR);
    }

    // aspetta che il master dia il via
    int statusSem = semget(ftok("./master",'A') , 2 , 0);
    if(statusSem == -1){
        error("alimentatore: semget() 1\n");
        term(getppid() , GENERIC_ERR);
    }

    int atomSID = atoi(argv[1]);
    const int MAX_N_ATOMICO = getEnvInt("MAX_N_ATOMICO");
    const int N_NUOVI_ATOMI = getEnvInt("N_NUOVI_ATOMI"); 
    const int STEP_ALIMENTAZIONE = getEnvInt("STEP_ALIMENTAZIONE"); 

    // dice al master che è pronto
    struct sembuf ready = {0 , 1 , 0};
    semop(statusSem , &ready , 1);

    // aspetta che il master dia il via
    struct sembuf waitForStart = {1 , 0 , 0};
    semop(statusSem , &waitForStart , 1);

    struct timespec sleep_time;
    while(1){
        /* Generiamo N_NUOVI_ATOMI */
        for(int i = 0 ; i < N_NUOVI_ATOMI ; i++){
            int n_atom = getRandValue() % MAX_N_ATOMICO +  1;
            if(createAtom(&atomSID , getppid() , n_atom) == -1){
                term(getppid() , MELTDOWN_ERR);
            }
        }
        sleep_time.tv_sec = 0;
        sleep_time.tv_nsec = STEP_ALIMENTAZIONE;
        (void)nanosleep(&sleep_time,NULL);
    }
}
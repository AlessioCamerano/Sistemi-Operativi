#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "util.h"

int main(int argc , char** argv){  

    int writeEnd = atoi(argv[1]);
    int atomSID = atoi(argv[2]);
    int STEP_ATTIVATORE = getEnvInt("STEP_ATTIVATORE"); 

    //creo un set di semafori 
    int statusSem = semget(ftok("./master",'A') , 2 , 0);
    if(statusSem == -1){
        error("attivatore: semget() 1");
        term(getppid() , GENERIC_ERR);
    }
    int shmSemId = semget(ftok("./master", 'C'), 1, 0);
    if (shmSemId == -1){
        error("attivatore: semget() 2");
        term(getppid() , GENERIC_ERR);
    } 

    int shmId = shmget(ftok("./master", 'D'), 0 , 0);
    if(shmId == -1){
        error("attivatore: shmget()");
        term(getppid() , GENERIC_ERR);
    }

    //collego la shared
    int* shm_ptr = (int*)shmat(shmId, NULL, 0);
    if (shm_ptr == (int*)-1) {
        error("attivatore: shmat()");
        term(getppid() , GENERIC_ERR);
    }

    int ENERGY_EXPLODE_THRESHOLD = getEnvInt("ENERGY_EXPLODE_THRESHOLD");

    // dice al master che è pronto
    struct sembuf ready = {0 , 1 , 0};
    semop(statusSem , &ready , 1);
   
    // aspetta che il master dia il via
    struct sembuf waitForStart = {1 , 0 , 0};
    semop(statusSem , &waitForStart , 1);

    int activations = 0;

    struct timespec sleep_time;
    while(1){
        //aspetta il permesso
        while (semop(shmSemId, &decSem, 1) == -1) {
            if(errno != EINTR){
                error("attivatore: Error in semop (preRead) semid = %d\t" , shmSemId);
                term(getppid() , GENERIC_ERR);
            }
        }

        // scrive sulla shared
        int energyNet = shm_ptr[SHM_STORED_ENERGY];

        //rimette ad 1 il semaforo 
        while (semop(shmSemId, &incSem, 1) == -1) {
            if(errno != EINTR){
                error("attivatore: Error in semop (postRead) semid = %d\t" , shmSemId);
                term(getppid() , GENERIC_ERR);
            }
        }

        // fissione
        if(energyNet < 2 * (ENERGY_EXPLODE_THRESHOLD / 3)){
            kill(-atomSID , SIGUSR1);

            activations++;
            if (write(writeEnd, &activations, sizeof(int)) == -1) {
                error("attivatore: write()");
                term(getppid() , GENERIC_ERR);
            } 
        }

        sleep_time.tv_sec = 0;
        sleep_time.tv_nsec = STEP_ATTIVATORE;
        (void)nanosleep(&sleep_time,NULL);
    }
}
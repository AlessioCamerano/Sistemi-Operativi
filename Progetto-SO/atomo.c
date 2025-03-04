#include <stdio.h>
#include <unistd.h>
#include <stdlib.h> 
#include <signal.h> 
#include <errno.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include "util.h"

pid_t master;
int n_atom;
int shmSemId;
int *shm_ptr;

int MIN_N_ATOMICO;

void fission(){
    static int activation_percentage = 0;

    if(n_atom <= MIN_N_ATOMICO){
        //aspetta il permesso
        while (semop(shmSemId, &decSem, 1) == -1) {
            if(errno != EINTR){
                error("atom: Error in semop (preRead) semid = %d\t" , shmSemId);
                term(master , GENERIC_ERR);
            } 
        }
        shm_ptr[SHM_SCORIE]++;
        
        //rimette ad 1 il semaforo 
        while (semop(shmSemId, &incSem, 1) == -1) {
            if(errno != EINTR){
                error("atom: Error in semop (postRead) semid = %d\t" , shmSemId);
                term(master , GENERIC_ERR);
            }
        }
        // the atom dies happy
        exit(EXIT_SUCCESS);
    }

    if((getRandValue() % 100 + 1) <= activation_percentage) return;  

    int child_atomic_num = n_atom / 2;
    int parent_atomic_num = n_atom - child_atomic_num;

    //aspetta il permesso
    while (semop(shmSemId, &decSem, 1) == -1) {
        if(errno != EINTR){
            error("atom: Error in semop (preRead) semid = %d\t" , shmSemId);
            term(master , GENERIC_ERR);
        }
    }

    // scrive sulla shared
    shm_ptr[SHM_FISSION]++;
    activation_percentage = shm_ptr[SHM_INHIB_PERCENTAGE];

    int energy_produced = ((child_atomic_num * parent_atomic_num) - max(child_atomic_num , parent_atomic_num));
    int energy_withdrawal = energy_produced * (activation_percentage / (float)100);

    shm_ptr[SHM_INHIB_WITHDRAWAL] += energy_withdrawal;
    shm_ptr[SHM_PRODUCED_ENERGY] += (energy_produced - energy_withdrawal);

    //rimette ad 1 il semaforo 
    while (semop(shmSemId, &incSem, 1) == -1) {
        if(errno != EINTR){
            error("Atom: Error in semop (postRead) semid = %d\t" , shmSemId);
            term(master , GENERIC_ERR);
        }
    }

    if(createAtom(NULL , master , child_atomic_num) == -1){
        term(master , MELTDOWN_ERR);
    }
    n_atom = parent_atomic_num;
}

void handler(int sig){
    if(sig == SIGUSR1){
        fission();
    }
}

int main(int argc , char** argv){
    master = atoi(argv[1]);
    n_atom = atoi(argv[2]);

    if(ignoreChildDeath() == -1){
        error("Atom: sigaction()");
        term(master , GENERIC_ERR);
    }

    MIN_N_ATOMICO = getEnvInt("MIN_N_ATOMICO");

    int statusSem;
    if((statusSem = semget(ftok("./master",'A') , 2 , 0)) == -1){
        printf("%d" , getpid());
        error("Atom: semget() 1");
        term(master , GENERIC_ERR);
    }

    if ((shmSemId = semget(ftok("./master", 'C'), 1, 0)) == -1){
        error("Atom: semget(shmSemId)");
        term(master , GENERIC_ERR);
    } 

    int shmId;
    if((shmId = shmget(ftok("./master", 'D'), 0 , 0)) == -1){
        error("Atom: shmget(shmId)");
        term(master , GENERIC_ERR);
    }

    //collego la shared
    shm_ptr = (int*)shmat(shmId, NULL, 0);
    if (shm_ptr == (int*)-1) {
        error("Atom: shmat()");
        term(master , GENERIC_ERR);
    }

    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&(sa.sa_mask));
    sa.sa_flags = 0;

    if(sigaction(SIGUSR1 , &sa , NULL) == -1){
        error("Atom: sigaction(SIGUSR1)");
        term(master , GENERIC_ERR);
    }

    // dice al master che è pronto
    struct sembuf ready = {0 , 1 , 0};
    (void)semop(statusSem , &ready , 1);

    // aspetta che il master dia il via
    struct sembuf waitForStart = {1 , 0 , 0};
    (void)semop(statusSem , &waitForStart , 1);

    while(1) (void)pause();
}   
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/shm.h>

#include "util.h"

int startSemId = -1, shmSemId = -1 , shmId = -1 , inhibMsgId = -1 , logFd = -1;
pid_t atomSID = -1, alimentatorePID = -1 , attivatorePID = -1 , inhibPID = -1;

int inhib_status = 1;

void clear(int exit_status , int ERR){
    // KILL'EM ALL!
    if(alimentatorePID != -1){
        kill(alimentatorePID , SIGKILL);
    }
    if(attivatorePID != -1){
        kill(attivatorePID , SIGKILL);
    }
    if(inhibPID != -1){
        kill(inhibPID , SIGKILL);
    }
    if(atomSID != -1){
        kill(-atomSID , SIGKILL);
    }  

    // removes all the ipc
    if(startSemId != -1){
        (void)semctl(startSemId , 0 , IPC_RMID);
    }
    if(shmSemId != -1){
        (void)semctl(shmSemId , 0 , IPC_RMID);
    }
    if(inhibMsgId != -1){
        (void)msgctl(inhibMsgId , IPC_RMID , NULL);
    }
    if(shmId != -1){
        (void)shmctl(shmId , IPC_RMID , NULL);
    }

    // closes the files
    if(logFd != -1){
        (void)writeLog(logFd , "Terminating...\n");
        (void)close(logFd);
    }

    switch(ERR){
        case USER_INT:
            printf("\nThe simulation has been interrupted by the user\n");
            break;
        case GENERIC_ERR:
            printf("\nThe simulation terminated due to a generic error\n");
            break;
        case MELTDOWN_ERR:
            printf("\nThe simulation terminated due to a meltdown\n");
            break;
        case BLACKOUT:
            printf("\nThe simulation terminated due to a blackout\n");
            break;
        case EXPLODE:
            printf("\nThe simulation terminated with an explosion\nBOOM!!\n");
            break;
        case TIMEOUT:
            printf("\nThe simulation terminated successfully\n");
            break;
        default:
            printf("\nUnrecognized error code!\n");
    }
    exit(exit_status);
}

void inhibHandler(int sig){
    if(sig == SIGINT){
        inhib_status = (!inhib_status);
    }
}

void stopHandler(int sig , siginfo_t* info , void* ucontext){
    if(sig == SIGTSTP){
        clear(EXIT_FAILURE , info->si_int);
    }
}

int main(int argc , char** argv){
    if(argc > 1){
        inhib_status = atoi(argv[1]) ? 1 : 0;
    }

    // ignores sigint (only for the kids), set to inhibHandler later
    {
        struct sigaction sa;
        sa.sa_handler = SIG_IGN;
        sigemptyset(&(sa.sa_mask));
        sa.sa_flags = 0;

        if(sigaction(SIGINT , &sa , NULL) == -1){
            error("master: sigaction() 1");
            clear(EXIT_FAILURE , GENERIC_ERR);
        }  
    }
    
    {
        struct sigaction sa;
        sa.sa_sigaction = stopHandler;
        sigemptyset(&(sa.sa_mask));
        sa.sa_flags = SA_SIGINFO;

        if(sigaction(SIGTSTP , &sa , NULL) == -1){
            error("master: sigaction() 2");
            clear(EXIT_FAILURE , GENERIC_ERR);
        }  
    } 

    if(ignoreChildDeath() == -1){
        error("master: sigaction() 3");
        clear(EXIT_FAILURE , GENERIC_ERR);
    }

    // environment variables
    FILE* parameters = fopen("./config.txt" , "r");
    if(parameters == NULL){
        error("master: fopen()");
        clear(EXIT_FAILURE , GENERIC_ERR);
    }else{
        char* line = NULL;
        size_t len = 0;
        while(getline(&line , &len , parameters)!= -1){
            char* copy = alloca(strlen(line) * sizeof(char));
            strcpy(copy , line);
            if(putenv(copy)){
                error("master: putenv()");
                clear(EXIT_FAILURE , GENERIC_ERR);
            }
        }
        if(line) free(line);
    }
    fclose(parameters);

    // semaphores
    if((startSemId = semget(ftok("./master",'A') , 2 , IPC_CREAT | IPC_EXCL | 0644)) == -1){
        error("master: semget() 1\n");
        clear(EXIT_FAILURE , GENERIC_ERR);
    }
    if ((shmSemId = semget(ftok("./master", 'C') , 1 , IPC_CREAT | IPC_EXCL | 0644)) == -1){
        error("master: semget() 3\n");
        clear(EXIT_FAILURE , GENERIC_ERR);
    }

    union semun startSet;
    unsigned short array[2] = {0 , 1};
    startSet.array = array;
    if(semctl(startSemId , 0 , SETALL , startSet) == -1){
        error("master: semctl() 1");
        clear(EXIT_FAILURE , GENERIC_ERR);
    }
    union semun sharedSet;
    sharedSet.val = 1;
    if(semctl(shmSemId, 0, SETVAL, sharedSet) == -1){
        error("master: semctl() 3");
        clear(EXIT_FAILURE , GENERIC_ERR);
    }

    // pipes
    int filedes[2];
    if (pipe(filedes) == -1 || fcntl(filedes[0] , F_SETFL , O_NONBLOCK) == -1){
        error("pipe");
        clear(EXIT_FAILURE , GENERIC_ERR);
    }
    
    // Apertura del file (equivalente di open("log.txt", O_CREAT | O_WRONLY | O_TRUNC , 0644))
    // Verifica se l'apertura del file è avvenuta con successo
    if ((logFd = creat("log.txt" , 0644)) == -1) {
        error("master: creat()");
        clear(EXIT_FAILURE , GENERIC_ERR);
    }

    // message queue
    if((inhibMsgId = msgget(ftok("./inibitore" , 1) , IPC_CREAT | IPC_EXCL | 0644)) == -1){
        error("master: msgget()");
        clear(EXIT_FAILURE , GENERIC_ERR);
    }

    //shared memory
    if ((shmId = shmget(ftok("./master", 'D') , 6 * sizeof(int) , IPC_CREAT | IPC_EXCL | 0644)) == -1){
        error("master: shmget()");
        clear(EXIT_FAILURE , GENERIC_ERR);
    }

    int *shm_ptr = (int*)shmat(shmId , NULL , 0);
    if (shm_ptr == (int*)-1) {
        error("master: shmat()");
        clear(EXIT_FAILURE , GENERIC_ERR);
    }

    const int N_ATOMS_INIT = getEnvInt("N_ATOMI_INIT");
    const int MAX_N_ATOMICO = getEnvInt("MAX_N_ATOMICO");
    const int SIM_DURATION = getEnvInt("SIM_DURATION");
    const int ENERGY_DEMAND = getEnvInt("ENERGY_DEMAND");
    const int ENERGY_EXPLODE_THRESHOLD = getEnvInt("ENERGY_EXPLODE_THRESHOLD");
    const int TOT_CHILDREN_INIT = N_ATOMS_INIT + 3;
    
    // atoms
    for(int i = 0 ; i < N_ATOMS_INIT ; i++){            
        int n_atom = getRandValue() % MAX_N_ATOMICO +  1;
        if(createAtom(&atomSID , getpid() , n_atom) == -1){
            clear(EXIT_FAILURE , GENERIC_ERR);
        }
    }

    // crea l'attivatore
    switch (attivatorePID = fork()){
        case -1:
            clear(EXIT_FAILURE , MELTDOWN_ERR);
            break;
        case 0:
            // close the read end of the pipe
            close(filedes[0]);
            char* pipe_str = alloca((snprintf(NULL , 0 , "%d" , filedes[1]) + 1) * sizeof(char));
            sprintf(pipe_str , "%d" , filedes[1]);

            // atom session id
            char* atom_SID_str = alloca((snprintf(NULL , 0 , "%d" , atomSID) + 1) * sizeof(char));
            sprintf(atom_SID_str , "%d" , atomSID);
            execve("./attivatore" , (char* []){"attivatore" , pipe_str , atom_SID_str ,  (char*) NULL} , environ);
            error("execve() attivatore");
            clear(EXIT_FAILURE , GENERIC_ERR);
    }
    // crea l'alimentatore
    switch (alimentatorePID = fork()){
        case -1:    
            clear(EXIT_FAILURE , MELTDOWN_ERR);
            break;
        case 0:
            // atom session id
            ;char* atom_SID_str = alloca((snprintf(NULL , 0 , "%d" , atomSID) + 1) * sizeof(char));
            sprintf(atom_SID_str , "%d" , atomSID);

            execve("./alimentatore" , (char* []){"alimentatore" , atom_SID_str , (char*) NULL} , environ);
            error("execve() alimentatore");
            clear(EXIT_FAILURE , GENERIC_ERR);
    }
    // crea l'inibitore
    switch (inhibPID = fork()){
        case -1:    
            clear(EXIT_FAILURE , MELTDOWN_ERR);
            break;
        case 0:
            // status inibitore
            ;char* inibitore_status_str = alloca((snprintf(NULL , 0 , "%d" , inhib_status) + 1) * sizeof(char));
            sprintf(inibitore_status_str , "%d" , inhib_status);
            // fd log
            ;char* logFd_str = alloca((snprintf(NULL , 0 , "%d" , logFd) + 1) * sizeof(char));
            sprintf(logFd_str , "%d" , logFd);

            execve("./inibitore" , (char* []){"inibitore" , inibitore_status_str , logFd_str , (char*) NULL} , environ);
            error("execve() inibitore");
            clear(EXIT_FAILURE , GENERIC_ERR);
    }
    
    // close the write end of the pipe
    close(filedes[1]);

    // wait for all processes to init
    struct sembuf waitOp = {0 , -TOT_CHILDREN_INIT , 0};
    semop(startSemId , &waitOp , 1);

    // starts the execution
    union semun startOp; startOp.val = 0;
    semctl(startSemId , 1 , SETVAL , startOp);

    // enables the interaction with the inhibitor
    {
        struct sigaction sa;
        sa.sa_handler = inhibHandler;
        sigemptyset(&(sa.sa_mask));
        sa.sa_flags = 0;

        if(sigaction(SIGINT , &sa , NULL) == -1){
            error("master: sigaction() 4");
            clear(EXIT_FAILURE , GENERIC_ERR);
        }  
    }
    
    // main loop
    int activations = 0 , TOTactivations = 0 , activationsLastSec = 0;
    int TOTfission = 0 , fissionLastSec = 0;
    int TOTscorie = 0, scorieLastSec = 0;
    int TOTenergyProduced = 0 , energyProducedLastSec = 0;
    int TOTenergyConsumed = 0 , energyConsumedLastSec = ENERGY_DEMAND;
    int grabLastSec = 0 , TOTgrab = 0;

    union sigval inhib_sig;
    struct timespec sleep_time;
    for(int i = 0 ; i < SIM_DURATION ; i++){
        // sleep
        sleep_time.tv_sec = 1;
        sleep_time.tv_nsec= 0;        
        while(nanosleep(&sleep_time,&sleep_time) == -1){
            if(errno != EINTR){
                error("master: nanosleep()");
                clear(EXIT_FAILURE , GENERIC_ERR);
            }
        }
        
        // reads from the pipe
        int byteRead;
        while(byteRead = read(filedes[0], &activations, sizeof(int))){
            if(byteRead == -1){
                if(errno == EAGAIN){
                    break;
                }else if(errno != EINTR){
                    error("master: read()");
                    clear(EXIT_FAILURE , GENERIC_ERR);
                }
            }
        }

        //wait sul sharedSem
        while (semop(shmSemId, &decSem, 1) == -1) {
            if(errno != EINTR){
                error("master: Error in semop (preRead) semid = %d\t" , shmSemId);
                clear(EXIT_FAILURE , GENERIC_ERR);
            }
        }

        //reads from shared
        fissionLastSec = shm_ptr[SHM_FISSION];
        shm_ptr[SHM_FISSION] = 0;
        TOTfission += fissionLastSec;

        scorieLastSec = shm_ptr[SHM_SCORIE];
        shm_ptr[SHM_SCORIE] = 0;
        TOTscorie += scorieLastSec;
        
        energyProducedLastSec = shm_ptr[SHM_PRODUCED_ENERGY];
        shm_ptr[SHM_PRODUCED_ENERGY] = 0;
        TOTenergyProduced += energyProducedLastSec;

        TOTenergyConsumed += energyConsumedLastSec;

        grabLastSec = shm_ptr[SHM_INHIB_WITHDRAWAL];
        shm_ptr[SHM_INHIB_WITHDRAWAL] = 0;
        TOTgrab += grabLastSec;

        int energyStored = shm_ptr[SHM_STORED_ENERGY] = TOTenergyProduced - TOTenergyConsumed;
        int inhibPercentage = shm_ptr[SHM_INHIB_PERCENTAGE];
        
        while (semop(shmSemId, &incSem, 1) == -1) {
            if(errno != EINTR){
                error("master: Error in semop (postRead) semid = %d\t" , shmSemId);
                clear(EXIT_FAILURE , GENERIC_ERR);
            }
        }

        activationsLastSec = activations - TOTactivations;
        TOTactivations = activations;

        // ricalcola l'inibitore
        struct msgBuffer msg = {1 , {TOTfission , TOTscorie , energyProducedLastSec}};
        while(msgsnd(inhibMsgId , &msg , 3 * sizeof(int) , 0) == -1){
            if(errno != EINTR){
                error("master: error in msgsnd()");
                clear(EXIT_FAILURE , GENERIC_ERR);
            }
        }
        inhib_sig.sival_int = inhib_status;
        (void)sigqueue(inhibPID , SIGUSR1 , inhib_sig);
        
        // pulisce il terminale (si può usare anche system("clear"))
        //printf("\033[H\033[2J\033[3J");
        printf("\033[1;1H\033[2J");
        
        printf("-------------------------------------------------------------\n");

        printf("Attivazioni nell'ultimo secondo: %d\n", activationsLastSec);
        printf("Tot attivazioni: %d\n", TOTactivations);

        printf("\nFissioni nell'ultimo secondo: %d\n", fissionLastSec);
        printf("TOT fissioni: %d\n", TOTfission);

        printf("\nScorie prodotte nell'ultimo secondo: %d\n", scorieLastSec);
        printf("TOT scorie prodotte: %d\n", TOTscorie);

        if(inhib_status){
            printf("\nPercentuale inibitore: %d%%\n" , inhibPercentage);
            printf("Energia prelevata dall'inibitore nell'ultimo secondo: %d\n", grabLastSec);
            printf("TOT energia prelevata: %d\n", TOTgrab);
        }else{
            printf("\nL'inibitore è disattivato\n");
        }
        
        printf("\nEnergia prodotta nell'ultimo secondo: %d\n", energyProducedLastSec);
        printf("TOT energia prodotta: %d\n", TOTenergyProduced);

        printf("\nEnergia consumata nell'ultimo secondo: %d\n", energyConsumedLastSec);
        printf("TOT energia consumata: %d\n", TOTenergyConsumed);

        printf("\nEnergia immagazzinata: %d\n" , energyStored);

        printf("-------------------------------------------------------------\n");

        if(energyStored >= ENERGY_EXPLODE_THRESHOLD){
            clear(EXIT_FAILURE , EXPLODE);
        }else if(energyStored <= 0){
            clear(EXIT_FAILURE , BLACKOUT);
        }
    }
    clear(EXIT_SUCCESS , TIMEOUT);
}
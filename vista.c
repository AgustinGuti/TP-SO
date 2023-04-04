#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "config.h"
#include "shm_config.h"



int main() {

    char *shmPath = "/shm_vista";
    
    int shmFd = shm_open(shmPath, O_RDWR, 0);
    if (shmFd == -1) {
        perror("shm_open");
    }

    struct shmbuf *shmp = mmap(NULL, sizeof(*shmp), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    if (shmp == MAP_FAILED){
        perror("mmap");
    }

    char toPrint[DATA_LENGTH];
    int bytesRead = 0;
    while(1) {
        
        sem_wait(&shmp->readyFiles);
        sem_wait(&shmp->mutex);
        memcpy(toPrint, &shmp->buf[bytesRead], DATA_LENGTH);
        if(toPrint[0] == 0){
            break;
        }
        sem_post(&shmp->mutex);
        bytesRead += DATA_LENGTH;
        printf("md5: %s || ID: %s || filename: %s \n", toPrint, toPrint + MD5_LENGTH+MAX_PATH_LENGTH+2, toPrint + MD5_LENGTH +1);

    }
    shm_unlink(shmPath);
    return 0;
}
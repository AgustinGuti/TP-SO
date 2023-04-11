#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>

#define PID_LENGTH 10
#define DATA_LENGTH (MD5_LENGTH + 1 + MAX_PATH_LENGTH + 1 + PID_LENGTH)


/* Define a structure that will be imposed on the shared
    memory object */

sem_t readyFiles;


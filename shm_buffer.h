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
#include "config.h"

#define PID_LENGTH 10
#define FIXED_CHARS 28
#define DATA_LENGTH (MD5_LENGTH + 1 + MAX_PATH_LENGTH + 1 + PID_LENGTH + FIXED_CHARS)

typedef struct shmData shmData;

shmData *createSharedBuffer(int id, int fileQty);

shmData *joinSharedBuffer(int id);

void initializeSemaphore(shmData *shmDataCreated, int initialValue);

void writeToSharedBuffer(shmData *shmDataCreated, char *data, size_t len);

int readFromSharedBuffer(shmData *shmDataCreated, char* data);

char printFromSharedBuffer(shmData *shmDataCreated);

void closeSharedBuffer(shmData *shmDataCreated);



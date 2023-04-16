// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "shm_buffer.h"

#define SHARE_BETWEEN_PROCESSES 1

struct shmData
{
    sem_t readyFiles;
    char *writeBuffer;
    char *readBuffer;

    size_t bufSize;
    size_t offset;
    size_t bytesWritten;
    size_t bytesRead;
    char shmPath[30];
};

shmData *createSharedBuffer(int id, int fileQty)
{
    char shmPath[30];
    sprintf(shmPath, "/shm_app_%d", id);

    int pageSize = sysconf(_SC_PAGE_SIZE);
    off_t offset = (off_t)ceil((double)sizeof(shmData) / pageSize) * pageSize;

    int shmFd = shm_open(shmPath, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (shmFd == -1)
        perror("shm_open");

    if (ftruncate(shmFd, offset + (fileQty + 1) * DATA_LENGTH) == -1)
        perror("ftruncate");

    /* Map the object into the caller's address space. */

    shmData *shmDataCreated = mmap(NULL, sizeof(shmData), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    if (shmDataCreated == MAP_FAILED)
        perror("mmap");

    initializeSemaphore(shmDataCreated, 0);

    shmDataCreated->offset = offset;
    shmDataCreated->bufSize = DATA_LENGTH * (fileQty + 1);

    strcpy(shmDataCreated->shmPath, shmPath);

    shmDataCreated->writeBuffer = mmap(NULL, shmDataCreated->bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, shmDataCreated->offset);
    if (shmDataCreated->writeBuffer == MAP_FAILED)
        perror("mmap");

    shmDataCreated->bytesWritten = 0;
    shmDataCreated->bytesRead = 0;

    return shmDataCreated;
}

shmData *joinSharedBuffer(int id)
{

    char shmPath[30];
    sprintf(shmPath, "/shm_app_%d", id);

    int shmFd = shm_open(shmPath, O_RDWR, 0);
    if (shmFd == -1)
        perror("shm_open");

    shmData *shmDataCreated = mmap(NULL, sizeof(shmData), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    if (shmDataCreated == MAP_FAILED)
        perror("mmap");

    shmDataCreated->readBuffer = mmap(NULL, shmDataCreated->bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, shmDataCreated->offset);
    if (shmDataCreated->readBuffer == MAP_FAILED)
        perror("mmap");
    return shmDataCreated;
}

void initializeSemaphore(shmData *shmDataCreated, int initialValue)
{
    if (sem_init(&shmDataCreated->readyFiles, SHARE_BETWEEN_PROCESSES, initialValue) == -1)
        perror("sem_init-readyFiles");
}

void writeToSharedBuffer(shmData *shmDataCreated, char *data, size_t len)
{

    memcpy(&(shmDataCreated->writeBuffer[shmDataCreated->bytesWritten]), data, len);
    shmDataCreated->bytesWritten += len;
    sem_post(&shmDataCreated->readyFiles);
}

char printFromSharedBuffer(shmData *shmDataCreated)
{

    char data[DATA_LENGTH] = {0};
    int readBytes = readFromSharedBuffer(shmDataCreated, data);
    printf("%s\n", data);
    return readBytes == 0;
}

int readFromSharedBuffer(shmData *shmDataCreated, char *data)
{

    sem_wait(&shmDataCreated->readyFiles);

    int readBytes = sprintf(data, "%s", shmDataCreated->readBuffer + shmDataCreated->bytesRead);
    shmDataCreated->bytesRead += readBytes + 1;
    return readBytes;
}

void closeSharedBuffer(shmData *shmDataCreated)
{
    sem_close(&shmDataCreated->readyFiles);
    sem_destroy(&shmDataCreated->readyFiles);
    shm_unlink(shmDataCreated->shmPath);
}

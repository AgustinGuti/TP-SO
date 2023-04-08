// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "config.h"
#include "shm_config.h"

int readFileQty();

int main(int argc, char *argv[])
{
    int fileQty;
    if (argc > 2)
    {
        printf("Invalid arguments\n");
        exit(1);
    }
    else if (argc == 2)
    {
        if (!isProcessRunning("aplicacion.out"))
        {
            printf("aplicacion no esta corriendo\n");
            exit(1);
        }
        fileQty = strtol(argv[1], NULL, 10);
    }
    else
    {
        fileQty = readFileQty();
    }

    char *shmPath = "/shm_vista";

    int shmFd = shm_open(shmPath, O_RDWR, 0);
    if (shmFd == -1)
    {
        perror("shm_open");
    }

    struct shmbuf *shmp = mmap(NULL, sizeof(*shmp), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    if (shmp == MAP_FAILED)
    {
        perror("mmap");
    }

    int pageSize = sysconf(_SC_PAGE_SIZE);
    off_t offset = (off_t)ceil((double)sizeof(struct shmbuf) / pageSize) * pageSize;

    char *buffer = mmap(NULL, DATA_LENGTH * (fileQty + 1), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, offset);

    char toPrint[DATA_LENGTH];
    int bytesRead = 0;
    while (1)
    {

        sem_wait(&shmp->readyFiles);
        sem_wait(&shmp->mutex);
        memcpy(toPrint, &buffer[bytesRead], DATA_LENGTH);
        if (toPrint[0] == 0)
        {
            break;
        }
        sem_post(&shmp->mutex);
        bytesRead += DATA_LENGTH;
        printf("md5: %s || ID: %s || filename: %s \n", toPrint, toPrint + MD5_LENGTH + MAX_PATH_LENGTH + 2, toPrint + MD5_LENGTH + 1);
    }
    shm_unlink(shmPath);
    return 0;
}

int readFileQty()
{
    char fileQtyBuf[10] = {0};
    int charsRead = 0;
    while (fileQtyBuf[strlen(fileQtyBuf) - 1] != '\n')
    {
        charsRead += read(0, fileQtyBuf + charsRead, 10 - charsRead);
    }
    return strtol(fileQtyBuf, NULL, 10);
}
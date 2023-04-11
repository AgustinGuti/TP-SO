// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "config.h"
#include "shm_config.h"

int readFileQty();

char printFromSharedMemory(sem_t *readyFiles, char *buffer, char *toPrint, size_t *bytesRead);

char isProcessRunning(char *processName);



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

    sem_t *readyFiles = mmap(NULL, sizeof(readyFiles), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    if (readyFiles == MAP_FAILED)
    {
        perror("mmap");
    }

    int pageSize = sysconf(_SC_PAGE_SIZE);
    off_t offset = (off_t)ceil((double)sizeof(sem_t) / pageSize) * pageSize;

    char *buffer = mmap(NULL, DATA_LENGTH * (fileQty + 1), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, offset);

    char toPrint[DATA_LENGTH];
    size_t bytesRead = 0;
    while (1)
    {
        if (printFromSharedMemory(readyFiles, buffer, toPrint, &bytesRead))
        {
            break;
        }
    }
    shm_unlink(shmPath);
    return 0;
}

int readFileQty()
{
    char *fileQtyBuf = NULL;
    size_t n = 0;
    getline(&fileQtyBuf, &n, stdin);
    int aux = strtol(fileQtyBuf, NULL, 10);
    free(fileQtyBuf);
    return aux;
}

char printFromSharedMemory(sem_t *readyFiles, char *buffer, char *toPrint, size_t *bytesRead)
{
    sem_wait(readyFiles);
    printf("%s\n", buffer + *bytesRead);
    int readBytes = strlen(buffer + *bytesRead);
    *bytesRead += readBytes + 1;
    return readBytes == 0;
}

char isProcessRunning(char *processName)
{
    int findID = fork(); // Escribe al pipe
    if (findID == 0)
    {
        char buffer[strlen(processName) + 20];
        buffer[0] = 0;
        strcat(buffer, "pidof ");
        strcat(buffer, processName);
        strcat(buffer, " > /dev/null");
        char *const params[] = {"/bin/sh", "-c", buffer, NULL};
        execve("/bin/sh", params, 0);
        perror("execve");
        return 0;
    }

    int status;
    waitpid(findID, &status, 0);
    return WEXITSTATUS(status) == 0;
}
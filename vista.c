// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "config.h"
#include "shm_buffer.h"

int readAppPID();

char printFromSharedMemory(sem_t *readyFiles, char *buffer, char *toPrint, size_t *bytesRead);

char isProcessRunning(char *processName);

int main(int argc, char *argv[])
{
    int appPID;
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
        appPID = strtol(argv[1], NULL, 10);
    }
    else
    {
        appPID = readAppPID();
    }

    shmData *shBufferData = joinSharedBuffer(appPID);

    while (1)
    {
        if (printFromSharedBuffer(shBufferData)){
            break;
        }

    }

    closeSharedBuffer(shBufferData);
    return 0;
}

int readAppPID()
{
    char *filePIDBuf = NULL;
    size_t n = 0;
    getline(&filePIDBuf, &n, stdin);
    int aux = strtol(filePIDBuf, NULL, 10);
    free(filePIDBuf);
    return aux;
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
        execve(params[0], params, 0);
        perror("execve");
        return 0;
    }

    int status;
    waitpid(findID, &status, 0);
    return WEXITSTATUS(status) == 0;
}
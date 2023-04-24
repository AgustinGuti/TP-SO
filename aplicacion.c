// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <sys/select.h>
#include "config.h"
#include "shm_buffer.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> /* Definition of AT_* constants */
#include <sys/stat.h>

#define SLAVE_QTY 5
#define INITIAL_FILE_DISTRIBUTION_FACTOR 0.1
#define SET_INITIAL_FILES_PER_SLAVE(fileQty) ((fileQty)*INITIAL_FILE_DISTRIBUTION_FACTOR / SLAVE_QTY)

int sendString(char *string, int fd);

// Crea SLAVE_QTY esclavos, y 2 pipes para cada uno, guarda sus pid, writeFd y readFd en los parametros correspondientes
int createSlaves(int pidSlaves[SLAVE_QTY], int writePipesFd[SLAVE_QTY], int readPipesFd[SLAVE_QTY], int fileQty, int *currentSentFiles, char **files);

int calculateInitialFilesPerSlave(int fileQty, int pageSize);

int buildString(char *string, char *lastMd5, char *lastFilename, int pid);

// Devuelve el numero de esclavo a partir de un FD, o -1 si ese fd no corresponde a ningun esclavo
int getSlaveNumberFromFd(int fd, int readPipesFd[SLAVE_QTY], int writePipesFd[SLAVE_QTY]);

// Envia mensaje de cierre al esclavo, cierra los pipes correspondientes y lo marca como cerrado
void closeSlave(int slaveNumber, int readPipesFd[SLAVE_QTY], int writePipesFd[SLAVE_QTY], fd_set readfds, char isSlaveClosed[SLAVE_QTY]);

int main(int argc, char **argv)
{
    int fileQty = argc - 1;

    int processID = getpid();

    /* Create shared memory object and set its size to the size of our structure. */

    shmData *shBufferData = createSharedBuffer(processID, fileQty);

    /* Initialize semaphores as process-shared, with value 0. */

    initializeSemaphore(shBufferData, 0);

    printf("%d\n", processID);

    sleep(2);

    int readyFilesQty = 0;
    int sentFilesQty = 0;

    int pidSlaves[SLAVE_QTY];
    int writePipesFd[SLAVE_QTY];
    int readPipesFd[SLAVE_QTY];

    fd_set readfds;

    int i;

    char isSlaveClosed[SLAVE_QTY] = {0};
    char initialFilesLeft[SLAVE_QTY] = {0};

    FILE *resultFile = fopen("results.txt", "w");
    if (resultFile == NULL)
    {
        perror("fopen");
    }

    int maxReadFD = createSlaves(pidSlaves, writePipesFd, readPipesFd, fileQty, &sentFilesQty, argv);

    int initialFilesPerSlave = calculateInitialFilesPerSlave(fileQty, sysconf(_SC_PAGE_SIZE));

    // Para almacenar que archivos le mando a cada esclavo
    // Representa el indice del archivo en argv
    int filesSentToSlave[SLAVE_QTY][fileQty];

    int filesSentToSlaveIndex[SLAVE_QTY] = {0};

    int filesReadFromSlaveIndex[SLAVE_QTY] = {0};

    mkfifo("pipeSlaves", 0666);

    for (i = 0; i < SLAVE_QTY; i++)
    {
        if (sentFilesQty == fileQty)
        {
            closeSlave(i, readPipesFd, writePipesFd, readfds, isSlaveClosed);
        }
        int j;
        for (j = 0; j < initialFilesPerSlave && sentFilesQty < fileQty; j++)
        {
            sendString(argv[sentFilesQty + 1], writePipesFd[i]);

            filesSentToSlave[i][filesSentToSlaveIndex[i]++] = sentFilesQty + 1;

            sentFilesQty++;
            initialFilesLeft[i]++;
        }
    }

    char md5result[MD5_LENGTH + 1] = {0};

    while (readyFilesQty < fileQty)
    {
        FD_ZERO(&readfds);
        for (i = 0; i < SLAVE_QTY; i++)
        {
            if (!isSlaveClosed[i])
            {
                FD_SET(readPipesFd[i], &readfds);
            }
        }

        select(maxReadFD + 1, &readfds, NULL, NULL, NULL);

        // Verifico quien esta listo para leer
        int fd;
        for (fd = 0; fd <= maxReadFD; fd++)
        {
            if (FD_ISSET(fd, &readfds))
            {
                int slaveNumber = getSlaveNumberFromFd(fd, readPipesFd, writePipesFd);

                size_t lineLen = 0;
                if ((lineLen = read(fd, md5result, MD5_LENGTH + 1)) == -1)
                {
                    perror("read");
                }

                md5result[lineLen - 1] = 0;

                readyFilesQty++;
                if (slaveNumber != -1)
                {
                    if (initialFilesLeft[slaveNumber] > 0)
                    {
                        initialFilesLeft[slaveNumber]--;
                    }
                    if (initialFilesLeft[slaveNumber] == 0)
                    {
                        if (sentFilesQty == fileQty)
                        {
                            closeSlave(slaveNumber, readPipesFd, writePipesFd, readfds, isSlaveClosed);
                        }
                        else
                        {
                            sendString(argv[1 + sentFilesQty], writePipesFd[slaveNumber]);
                            filesSentToSlave[slaveNumber][filesSentToSlaveIndex[slaveNumber]++] = sentFilesQty + 1;
                            sentFilesQty++;
                        }
                    }

                    char string[DATA_LENGTH] = {0};

                    int fileIndexReadFromSlave = filesSentToSlave[slaveNumber][filesReadFromSlaveIndex[slaveNumber]++];
                    int stringLen = sprintf(string, "md5: %s || ID: %d || filename: %s%c", md5result, pidSlaves[slaveNumber], argv[fileIndexReadFromSlave], '\0');
                    fprintf(resultFile, "%s\n", string);

                    writeToSharedBuffer(shBufferData, string, stringLen);

                    md5result[0] = 0;
                }
            }
        }
    }

    writeToSharedBuffer(shBufferData, "", 1);

    fclose(resultFile);

    closeSharedBuffer(shBufferData);
    return 0;
}

int calculateInitialFilesPerSlave(int fileQty, int pageSize)
{
    int res = SET_INITIAL_FILES_PER_SLAVE(fileQty);
    int pipeCapacity = 16 * pageSize; // A partir de Ubuntu 3.16, el pipe tiene un tamaño de 16 páginas
    if (res * (MAX_PATH_LENGTH + 1) > pipeCapacity)
    {
        res = pipeCapacity / (MAX_PATH_LENGTH + 1);
    }
    if (res == 0)
        res = 1;
    return res;
}

void closeSlave(int slaveNumber, int readPipesFd[SLAVE_QTY], int writePipesFd[SLAVE_QTY], fd_set readfds, char isSlaveClosed[SLAVE_QTY])
{
    if (close(writePipesFd[slaveNumber]) == -1)
    {
        perror("close");
    }
    FD_CLR(readPipesFd[slaveNumber], &readfds);
    if (close(readPipesFd[slaveNumber]) == -1)
    {
        perror("close");
    }
    isSlaveClosed[slaveNumber] = 1;
    if (areAllClosed(isSlaveClosed))
    {
        int fdFifo = open("pipeSlaves", O_WRONLY);
        write(fdFifo, EOF, 1);
        close(fdFifo);
    }
}

int areAllClosed(char isSlaveClosed[])
{
    int i;
    for (i = 0; i < SLAVE_QTY; i++)
    {
        if (isSlaveClosed[i] == 0)
        {
            return 0;
        }
    }
    return 1;
}

int getSlaveNumberFromFd(int fd, int readPipesFd[SLAVE_QTY], int writePipesFd[SLAVE_QTY])
{
    int i;
    for (i = 0; i < SLAVE_QTY; i++)
    {
        if (readPipesFd[i] == fd)
        {
            return i;
        }
    }
    for (i = 0; i < SLAVE_QTY; i++)
    {
        if (writePipesFd[i] == fd)
        {
            return i;
        }
    }

    return -1;
}

int sendString(char *string, int fd)
{
    char aux[MAX_PATH_LENGTH + 1] = {0};
    int len = sprintf(aux, "%s\n", string);
    if (write(fd, aux, len) == -1)
    {
        perror("write");
    }
    return 0;
}

int createSlaves(int pidSlaves[SLAVE_QTY], int writePipesFd[SLAVE_QTY], int readPipesFd[SLAVE_QTY], int fileQty, int *currentSentFiles, char **files)
{

    int i, maxReadFD = 0;
    for (i = 0; i < SLAVE_QTY; i++)
    {
        int pipeFd1[2] = {0};
        int pipeFd2[2] = {0};

        if (pipe(pipeFd1) == -1 || pipe(pipeFd2) == -1)
        {
            perror("pipe");
        }
        else
        {
            writePipesFd[i] = pipeFd1[1];
            readPipesFd[i] = pipeFd2[0];

            if (readPipesFd[i] > maxReadFD)
            {
                maxReadFD = readPipesFd[i];
            }
        }

        char *params[2] = {"./esclavo.out", NULL};

        pidSlaves[i] = fork();
        if (pidSlaves[i] == -1)
        {
            perror("fork");
        }
        else if (pidSlaves[i] == 0)
        {
            // El stdin y el stdout de los esclavos pasan a ser pipes para comunicarse con la aplicación
            if (close(pipeFd1[1]) == -1)
            {
                perror("close");
            }
            if (fclose(stdin) == EOF)
            {
                perror("fclose");
            }
            if (dup(pipeFd1[0]) == -1)
            {
                perror("dup");
            }
            if (close(pipeFd1[0]) == -1)
            {
                perror("close");
            }

            if (close(pipeFd2[0]) == -1)
            {
                perror("close");
            }
            if (fclose(stdout) == EOF)
            {
                perror("fclose");
            }
            if (dup(pipeFd2[1]) == -1)
            {
                perror("dup");
            }
            if (close(pipeFd2[1]) == -1)
            {
                perror("close");
            }

            // char *const params[] = {"./esclavo.out", NULL};

            if (execve(params[0], params, 0) == -1)
            {
                perror("execve");
            }
        }
        // Aplicacion cierra los no usados
        if (close(pipeFd1[0]) == -1)
        {
            perror("close");
        }
        if (close(pipeFd2[1]) == -1)
        {
            perror("close");
        }
    }

    return maxReadFD;
}
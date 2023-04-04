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

#define SLAVE_QTY 5
#define INITIAL_FILE_DISTRIBUTION_FACTOR 0.1
#define SHARE_BETWEEN_PROCESSES 1

int sendString(char *string, int fd);

// Crea SLAVE_QTY esclavos, y 2 pipes para cada uno, guarda sus pid, writeFd y readFd en los parametros correspondientes
int createSlaves(int pidSlaves[SLAVE_QTY], int writePipesFd[SLAVE_QTY], int readPipesFd[SLAVE_QTY], int fileQty, int *currentSentFiles, char **files);

// Devuelve el numero de esclavo a partir de un FD, o -1 si ese fd no corresponde a ningun esclavo
int getSlaveNumberFromFd(int fd, int readPipesFd[SLAVE_QTY], int writePipesFd[SLAVE_QTY]);

// Envia mensaje de cierre al esclavo, cierra los pipes correspondientes y lo marca como cerrado
void closeSlave(int slaveNumber, int readPipesFd[SLAVE_QTY], int writePipesFd[SLAVE_QTY], fd_set readfds, char isSlaveClosed[SLAVE_QTY]);

int main(int argc, char **argv)
{

    char *shmpath = "/shm_vista";

    /* Create shared memory object and set its size to the size
        of our structure. */

    int shmFd = shm_open(shmpath, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (shmFd == -1)
        perror("shm_open");

    if (ftruncate(shmFd, sizeof(struct shmbuf)) == -1)
        perror("ftruncate");

    /* Map the object into the caller's address space. */

    struct shmbuf *shmp = mmap(NULL, sizeof(*shmp), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    if (shmp == MAP_FAILED)
        perror("mmap");

    /* Initialize semaphores as process-shared, with value 0. */

    if (sem_init(&shmp->mutex, SHARE_BETWEEN_PROCESSES, 1) == -1)
        perror("sem_init-mutex");
    if (sem_init(&shmp->readyFiles, SHARE_BETWEEN_PROCESSES, 0) == -1)
        perror("sem_init-readyFiles");

    sem_wait(&shmp->mutex);
    shmp->cnt = 0;
    sem_post(&shmp->mutex);

    sleep(5);

    int fileQty = argc - 1;

    int readyFilesQty = 0;
    int sentFilesQty = 0;

    int pidSlaves[SLAVE_QTY];
    int writePipesFd[SLAVE_QTY];
    int readPipesFd[SLAVE_QTY];

    fd_set readfds;

    int i;

    char lastMd5[MD5_LENGTH + 1];
    char lastFilename[MAX_PATH_LENGTH];
    char message[MD5_LENGTH + MAX_PATH_LENGTH + 2];

    char isSlaveClosed[SLAVE_QTY] = {0};
    char initialFilesLeft[SLAVE_QTY] = {0};

    FILE *resultFile = fopen("results.txt", "w");
    if (resultFile == NULL)
    {
        perror("fopen");
    }

    int maxReadFD = createSlaves(pidSlaves, writePipesFd, readPipesFd, fileQty, &sentFilesQty, argv);
    int initialFilesPerSlave = fileQty * INITIAL_FILE_DISTRIBUTION_FACTOR / SLAVE_QTY;
    // int initialFilesPerSlave = 2;
    for (i = 0; i < SLAVE_QTY; i++)
    {
        int j;
        for (j = 0; j < initialFilesPerSlave; j++)
        {
            sendString(argv[sentFilesQty + 1], writePipesFd[i]);
            sentFilesQty++;
            initialFilesLeft[i]++;
        }
    }

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

                if (read(fd, message, MD5_LENGTH + MAX_PATH_LENGTH) == -1)
                {
                    perror("read");
                }

                strncpy(lastMd5, message, MD5_LENGTH);
                lastMd5[MD5_LENGTH] = '\0';
                strncpy(lastFilename, message + MD5_LENGTH, MAX_PATH_LENGTH);
                readyFilesQty++;
                if (slaveNumber != -1)
                {
                    if (sentFilesQty == fileQty)
                    {
                        closeSlave(slaveNumber, readPipesFd, writePipesFd, readfds, isSlaveClosed);
                    }
                    else
                    {

                        // there are no initial Files Left

                        if (initialFilesLeft[slaveNumber] > 0)
                        {
                            initialFilesLeft[slaveNumber]--;
                        }
                        if (initialFilesLeft[slaveNumber] == 0)
                        {
                            sendString(argv[1 + sentFilesQty], writePipesFd[slaveNumber]);
                            sentFilesQty++;
                        }
                    }

                    // printf("ID is: %d || Filename is: %s || md5 is: %s \n", pidSlaves[slaveNumber], lastFilename, lastMd5);
                    fprintf(resultFile, "md5: %s || ID: %d || filename: %s \n", lastMd5, pidSlaves[slaveNumber], lastFilename);

                    char string[DATA_LENGTH] = {0};
                    sprintf(string, "%s", lastMd5);
                    sprintf(string + MD5_LENGTH + 1, "%s", lastFilename);
                    sprintf(string + MD5_LENGTH + MAX_PATH_LENGTH + 2, "%d", pidSlaves[slaveNumber]);

                    sem_wait(&shmp->mutex);
                    memcpy(&(shmp->buf[shmp->cnt]), string, DATA_LENGTH);
                    shmp->cnt += DATA_LENGTH;
                    sem_post(&shmp->mutex);
                    sem_post(&shmp->readyFiles);

                    lastFilename[0] = 0;
                    lastMd5[0] = 0;
                }
            }
        }
    }

    sem_wait(&shmp->mutex);
    memcpy(&(shmp->buf[shmp->cnt]), "", 1);
    shmp->cnt += DATA_LENGTH;
    sem_post(&shmp->mutex);
    sem_post(&shmp->readyFiles);

    fclose(resultFile);
    shm_unlink(shmpath);

    return 0;
}

void closeSlave(int slaveNumber, int readPipesFd[SLAVE_QTY], int writePipesFd[SLAVE_QTY], fd_set readfds, char isSlaveClosed[SLAVE_QTY])
{
    sendString("", writePipesFd[slaveNumber]);
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
    strcpy(aux, string);
    if (write(fd, aux, MAX_PATH_LENGTH + 1) == -1)
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

        int pipeFd1[2];
        int pipeFd2[2];

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

            if (execve("./esclavo.out", params, 0) == -1)
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
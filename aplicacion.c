// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <sys/select.h>
#include "config.h"
#include "shm_config.h"

#define SLAVE_QTY 5
#define INITIAL_FILE_DISTRIBUTION_FACTOR 0.1
#define SHARE_BETWEEN_PROCESSES 1
#define SET_INITIAL_FILES_PER_SLAVE(fileQty) fileQty *INITIAL_FILE_DISTRIBUTION_FACTOR / SLAVE_QTY

int sendString(char *string, int fd);

// Crea SLAVE_QTY esclavos, y 2 pipes para cada uno, guarda sus pid, writeFd y readFd en los parametros correspondientes
int createSlaves(int pidSlaves[SLAVE_QTY], int writePipesFd[SLAVE_QTY], int readPipesFd[SLAVE_QTY], int fileQty, int *currentSentFiles, char **files);

int calculateInitialFilesPerSlave(int fileQty, int pageSize);

void initSemaphores(struct shmbuf *shmp);

void parseMessage(char *message, char *lastMd5, char *lastFilename);

void buildString(char *string, char *lastMd5, char *lastFilename, int pid);

// Devuelve el numero de esclavo a partir de un FD, o -1 si ese fd no corresponde a ningun esclavo
int getSlaveNumberFromFd(int fd, int readPipesFd[SLAVE_QTY], int writePipesFd[SLAVE_QTY]);

void writeToSharedMemory(char *string, struct shmbuf *shmp, char *buffer);

// Envia mensaje de cierre al esclavo, cierra los pipes correspondientes y lo marca como cerrado
void closeSlave(int slaveNumber, int readPipesFd[SLAVE_QTY], int writePipesFd[SLAVE_QTY], fd_set readfds, char isSlaveClosed[SLAVE_QTY]);

int main(int argc, char **argv)
{
    int fileQty = argc - 1;

    char *shmpath = "/shm_vista";

    /* Create shared memory object and set its size to the size of our structure. */

    int pageSize = sysconf(_SC_PAGE_SIZE);
    off_t offset = (off_t)ceil((double)sizeof(struct shmbuf) / pageSize) * pageSize;

    int shmFd = shm_open(shmpath, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (shmFd == -1)
        perror("shm_open");

    if (ftruncate(shmFd, offset + (fileQty + 1) * DATA_LENGTH) == -1)
        perror("ftruncate");

    /* Map the object into the caller's address space. */

    struct shmbuf *shmp = mmap(NULL, sizeof(*shmp), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    if (shmp == MAP_FAILED)
        perror("mmap");

    char *buffer = mmap(NULL, DATA_LENGTH * (fileQty + 1), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, offset);

    /* Initialize semaphores as process-shared, with value 0. */

    initSemaphores(shmp);

    sleep(2);

    if (isProcessRunning("vista.out"))
    {
        printf("%d\n", fileQty);
        fflush(stdout);
    }

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

    int initialFilesPerSlave = calculateInitialFilesPerSlave(fileQty, pageSize);
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

                parseMessage(message, lastMd5, lastFilename);

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

                    fprintf(resultFile, "md5: %s || ID: %d || filename: %s \n", lastMd5, pidSlaves[slaveNumber], lastFilename);

                    char string[DATA_LENGTH] = {0};
                    buildString(string, lastMd5, lastFilename, pidSlaves[slaveNumber]);

                    writeToSharedMemory(string, shmp, buffer);

                    lastFilename[0] = 0;
                    lastMd5[0] = 0;
                }
            }
        }
    }

    writeToSharedMemory("", shmp, buffer);

    fclose(resultFile);
    shm_unlink(shmpath);

    return 0;
}

void initSemaphores(struct shmbuf *shmp)
{
    if (sem_init(&shmp->mutex, SHARE_BETWEEN_PROCESSES, 1) == -1)
        perror("sem_init-mutex");
    if (sem_init(&shmp->readyFiles, SHARE_BETWEEN_PROCESSES, 0) == -1)
        perror("sem_init-readyFiles");

    sem_wait(&shmp->mutex);
    shmp->cnt = 0;
    sem_post(&shmp->mutex);
}

int calculateInitialFilesPerSlave(int fileQty, int pageSize)
{
    int res = SET_INITIAL_FILES_PER_SLAVE(fileQty);
    int pipeCapacity = 16 * pageSize; // A partir de Ubuntu 3.16, el pipe tiene un tamaño de 16 páginas
    if (res * (MAX_PATH_LENGTH + 1) > pipeCapacity)
    {
        res = pipeCapacity / (MAX_PATH_LENGTH + 1);
    }
    return res;
}

void parseMessage(char *message, char *lastMd5, char *lastFilename)
{
    strncpy(lastMd5, message, MD5_LENGTH);
    lastMd5[MD5_LENGTH] = '\0';
    strncpy(lastFilename, message + MD5_LENGTH, MAX_PATH_LENGTH);
}

void buildString(char *string, char *lastMd5, char *lastFilename, int pid)
{
    strncpy(string, lastMd5, MD5_LENGTH);
    strncpy(string + MD5_LENGTH + 1, lastFilename, MAX_PATH_LENGTH);
    sprintf(string + MD5_LENGTH + MAX_PATH_LENGTH + 2, "%d", pid);
}

void writeToSharedMemory(char *string, struct shmbuf *shmp, char *buffer)
{

    sem_wait(&shmp->mutex);
    memcpy(&(buffer[shmp->cnt]), string, DATA_LENGTH);
    shmp->cnt += DATA_LENGTH;
    sem_post(&shmp->mutex);
    sem_post(&shmp->readyFiles);
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
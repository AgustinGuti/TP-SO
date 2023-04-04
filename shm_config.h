#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PID_LENGTH 10
#define DATA_LENGTH (MD5_LENGTH + MAX_PATH_LENGTH + PID_LENGTH + 2)

#define BUF_SIZE 5057056   /* Maximum size for exchanged string */

/* Define a structure that will be imposed on the shared
    memory object */

struct shmbuf {
    sem_t  mutex;            /* POSIX unnamed semaphore */
    sem_t  readyFiles;            /* POSIX unnamed semaphore */
    size_t cnt;             /* Number of bytes used in 'buf' */
    char   buf[BUF_SIZE];   /* Data being transferred */
};

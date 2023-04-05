#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/select.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"

// Crea un proceso md5sum que hashea filename y deja el resultado en result
void delegateMd5(char *filename, char result[MD5_LENGTH + 1]);

int sendString(char *string, int fd);

int main(int argc, char **argv)
{
    char filename[MAX_PATH_LENGTH] = {0};
    while (1)
    {
        int charsRead = 0;
        if ((charsRead = read(0, filename, MAX_PATH_LENGTH + 1)) == -1)
        {
            perror("read");
        }
        if (charsRead == 0)
        { 
            break;
        }
        char md5Result[MD5_LENGTH + 1];
        delegateMd5(filename, md5Result);
        char resultData[MAX_PATH_LENGTH + MD5_LENGTH + 1];
        resultData[0] = 0;
        strcat(resultData, md5Result);
        resultData[MD5_LENGTH] = 0;
        strcat(resultData, filename);
        resultData[MD5_LENGTH + MAX_PATH_LENGTH] = 0;

        if (write(1, resultData, MAX_PATH_LENGTH + MD5_LENGTH) == -1)
        {
            perror("write");
        }
    }
    return 0;
}

void delegateMd5(char *filename, char result[MD5_LENGTH + 1])
{
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        perror("pipe");
    }

    int md5id = fork(); // Escribe al pipe
    if (md5id == 0)
    {
        close(pipefd[0]); // r-end
        fclose(stdout);
        dup(pipefd[1]);
        close(pipefd[1]);
        char *const params[] = {"md5sum", filename, NULL};
        execvp("md5sum", params);
        perror("execve");
        return;
    }

    close(pipefd[1]);

    waitpid(md5id, NULL, 0);
    read(pipefd[0], result, MD5_LENGTH);
    result[MD5_LENGTH] = 0;

    close(pipefd[0]);
}

int sendString(char *string, int fd)
{
    if (write(fd, string, strlen(string) + 1) == -1)
    {
        perror("write");
    }
    return 0;
}
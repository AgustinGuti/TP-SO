// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/select.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "config.h"

// Crea un proceso md5sum que hashea filename y deja el resultado en result
void delegateMd5(char *filename, char result[MD5_LENGTH + 1]);

int createResultData(char *filename, char *md5, char *resultData);

int main(int argc, char **argv)
{
    size_t linecap = MAX_PATH_LENGTH+1;
    char *filename = malloc(MAX_PATH_LENGTH + 1);

    while (1)
    {
        
        int charsRead = 0;

        errno = 0;
        if ((charsRead = getline(&filename, &linecap, stdin)) == -1)
        {
            if (errno != 0){
                perror("getline");
            }
            break;
        }
        filename[charsRead - 1] = 0;

        char md5Result[MD5_LENGTH + 1];
        delegateMd5(filename, md5Result);
        char resultData[MAX_PATH_LENGTH + MD5_LENGTH + 1] = {0};
        int lenght = sprintf(resultData, "%s\n", md5Result); 

        if (write(1, resultData, lenght) == -1)
        {
            perror("write");
        }
    }
    free(filename);

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
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
#include <fcntl.h>

int readAppPID();

char isProcessRunning(char *processName);

int main(int argc, char *argv[])
{
    int fd = open("pipeSlaves", O_RDONLY);
    char md5Result[33];
    int aux = 1;
    while (aux != 0)
    {
        aux = read(fd, md5Result, 32);
        if (aux == -1)
        {
            perror("read");
            break;
        }
        md5Result[32] = 0;
        printf("%s\n", md5Result);
    }
    close(fd);
    return 0;
}
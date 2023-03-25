#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/select.h>
#include <string.h>
#include "config.h"

//Crea un proceso md5sum que hashea filename y deja el resultado en result
void delegateMd5(char *filename, char result[MD5_LENGTH + 1]);

int sendString(char* string, int fd);

int main(int argc, char** argv){

    fd_set dataInputFds;
    FD_ZERO(&dataInputFds);
    FD_SET(0, &dataInputFds);   //Preparar stdin select

    char filename[MAX_PATH_LENGTH];

    int i;
    for (i = 0; i < argc-2; i++){ 
        char md5Result[MD5_LENGTH + 1];
        delegateMd5(argv[i+1], md5Result);
        char message[MAX_PATH_LENGTH+MD5_LENGTH];
        message[0] = 0; 
        strcat(message, md5Result);
        message[MD5_LENGTH] = 0;
        strcat(message, argv[i+1]);

        write(1, message, MAX_PATH_LENGTH+MD5_LENGTH);
    }
    write(1, "", 1);


    while(1){
        select(1, &dataInputFds, NULL, NULL, NULL);
        
        if (FD_ISSET(0, &dataInputFds)){
            if (read(0, filename, MAX_PATH_LENGTH) == -1){
                perror("read");
            }  
            if (filename[0] == 0){ //Un filename vacio indica que ya no hay mas archivos para procesar
                break;
            }

            char md5Result[MD5_LENGTH + 1];
            delegateMd5(filename, md5Result);
                
            char message[MAX_PATH_LENGTH+MD5_LENGTH];
            message[0] = 0; 
            strcat(message, md5Result);
            message[MD5_LENGTH] = 0;
            strcat(message, filename);

            write(1, message, MAX_PATH_LENGTH+MD5_LENGTH);
          
          }

    }

    return 0;
 
}

void delegateMd5(char *filename, char result[MD5_LENGTH + 1]){
    int pipefd[2];
    if (pipe(pipefd) == -1){
        perror("pipe");
    }

    int md5id = fork();  //Escribe al pipe
    if (md5id == 0){
        close(pipefd[0]);   //r-end
        fclose(stdout);
        dup(pipefd[1]);
        close(pipefd[1]);
        char *const params[] = {"md5sum",filename, NULL};
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

int sendString(char* string, int fd){
    if (write(fd, string, strlen(string)+1) == -1){
        perror("write");
    }
    return 0;
}
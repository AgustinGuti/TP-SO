#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/select.h>
#include <string.h>



int main(){

    fd_set dataInputFds;
    FD_ZERO(&dataInputFds);
    FD_SET(0, &dataInputFds);   //Preparar stdin select

    struct timeval interval;
    interval.tv_sec = 1;
    interval.tv_usec = 0;


    char filename[1024];

    while(1){
        select(1, &dataInputFds, NULL, NULL, &interval);
        sleep(1);
         
        if (FD_ISSET(0, &dataInputFds)){
            if (read(0, filename, 1024) == -1){
                perror("read");
            }  
            if (filename[0] == 0){ //Un filename vacio indica que ya no hay mas archivos para procesar
                break;
            }

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
                return 0;
            }
            close(pipefd[1]);

            char md5Result[33];

            waitpid(md5id, NULL, 0);  
            read(pipefd[0], md5Result, 32);
            md5Result[32] = 0;
           
            close(pipefd[0]);

            char message[1024+32];
            message[0] = 0; 
            strcat(message, md5Result);
            message[32] = 0;
            strcat(message, filename);

            write(1, message, 1024+32);
          
        }

    }

    return 0;
 
}
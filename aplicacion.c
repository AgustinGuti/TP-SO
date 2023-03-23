#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>

#define CANT_ESCLAVOS 5

int main(int argc, char** argv){


    int pidSlaves[CANT_ESCLAVOS];
    int writePipesFd[CANT_ESCLAVOS];
    int readPipesFd[CANT_ESCLAVOS];

    int i;
    for (i = 0; i < CANT_ESCLAVOS; i++){

        int pipefd[2];
        if (pipe(pipefd) == -1){
            perror("pipe");
        }else{
            readPipesFd[i] = pipefd[0];
            writePipesFd[i] = pipefd[1];
        }

        pidSlaves[i] = fork();
        if (pidSlaves[i] == -1){
            perror("fork");
        }else if (pidSlaves[i] == 0){
            //El stdin y el stdout de los esclavos pasan a ser pipes para comunicarse con la aplicación
            if (fclose(stdin) == EOF){
                perror("fclose");
            }
            if (dup(pipefd[0]) == -1){
                perror("dup");
            }
            if (close(pipefd[0]) == -1){
                perror("close");
            }


            if (fclose(stdout) == EOF){
                perror("fclose");
            }
            if (dup(pipefd[1]) == -1){
                perror("dup");
            }
            if (close(pipefd[1]) == -1){
                perror("close");
            }
            
            char *const params[] = {"./esclavo.out", NULL};
            execve("./esclavo.out", params, 0);

            break;
        }

    }

    return 0;
}
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#define CANT_ESCLAVOS 4
#define INITIAL_DISTRIBUTION_PERCENTAJE 0.1

/* Catch Signal Handler functio */
void signal_callback_handler(int signum){
    printf("Caught signal SIGPIPE %d\n",signum);
}

int sendNextFile(char* filename, int fd);

int main(int argc, char** argv){
    /* Catch Signal Handler SIGPIPE */
    signal(SIGPIPE, signal_callback_handler);

    int fileQty = argc - 1;
    int initialFilesPerSlave = ( fileQty / CANT_ESCLAVOS ) * INITIAL_DISTRIBUTION_PERCENTAJE;

    int currentReadyFiles = 0;
    int currentSentFiles = 0;  
    
    int pidSlaves[CANT_ESCLAVOS];
    int writePipesFd[CANT_ESCLAVOS];
    int readPipesFd[CANT_ESCLAVOS];

    int maxReadFD = 0;

    fd_set readfds;
   

    struct timeval interval;
    interval.tv_sec = 10;
    interval.tv_usec = 0;

    int i;
    for (i = 0; i < CANT_ESCLAVOS; i++){

        int pipeFd1[2];
        int pipeFd2[2];

        if (pipe(pipeFd1) == -1 || pipe(pipeFd2) == -1){
            perror("pipe");
        }else{

            writePipesFd[i] = pipeFd1[1];
            readPipesFd[i] = pipeFd2[0];

            if (readPipesFd[i] > maxReadFD){
                maxReadFD = readPipesFd[i];
            }
        
        }

        pidSlaves[i] = fork();
        if (pidSlaves[i] == -1){
            perror("fork");
        }else if (pidSlaves[i] == 0){
            //El stdin y el stdout de los esclavos pasan a ser pipes para comunicarse con la aplicación
            if (close(pipeFd1[1]) == -1){ 
                perror("close");
            }
            if (fclose(stdin) == EOF){
                perror("fclose");
            }
            if (dup(pipeFd1[0]) == -1){
                perror("dup");
            }
            if (close(pipeFd1[0]) == -1){
                perror("close");
            }

            if (close(pipeFd2[0]) == -1){
                perror("close");
            }
            if (fclose(stdout) == EOF){
                perror("fclose");
            }
            if (dup(pipeFd2[1]) == -1){
                perror("dup");
            }
            if (close(pipeFd2[1]) == -1){
                perror("close");
            }
            
            char *const params[] = {"./esclavo.out", NULL};
            execve("./esclavo.out", params, 0);

            break;
        }
        //Aplicacion cierra los no usados
        if (close(pipeFd1[0]) == -1){
            perror("close");
        }
        if (close(pipeFd2[1]) == -1){
            perror("close");
        }


    }

    //Distribucion inicial de archivos
    for (i = 0; i < CANT_ESCLAVOS; i++){
        //for (j = 0; j < initialFilesPerSlave; j++ ){
          //  char* fileName = argv[ 1 + currentSentFiles++]; //1+ para no enviar el primer argumento
            //char* fileName = "aplicacion.c"
            if (currentSentFiles == fileQty){
                write(writePipesFd[i],"", 1);  //Termine
            }else{
                sendNextFile(argv[1+currentSentFiles++], writePipesFd[i]);
            }
          
        //}
    }
     

    char lastMd5[33];
    char lastFilename[1024];

    char isSlaveClosed[CANT_ESCLAVOS];
    for (i = 0; i < CANT_ESCLAVOS; i++){
        isSlaveClosed[i] = 0;
    }

    while (currentReadyFiles < fileQty){
        FD_ZERO(&readfds);
        interval.tv_sec = 0;
        interval.tv_usec = 0;   
        for (i = 0; i < CANT_ESCLAVOS; i++){
            if (!isSlaveClosed[i]){
                FD_SET(readPipesFd[i], &readfds);
            }
        }

        select(maxReadFD+1, &readfds, NULL, NULL, &interval);
        
        //Verifico quien esta listo para leer
        int fd;
        for (fd = 0; fd <= maxReadFD; fd++){
            if (FD_ISSET(fd, &readfds)){
                read(fd, lastMd5, 32);
                read(fd, lastFilename, 1024);
        
                currentReadyFiles++;

             
                //Busco a donde tengo que escribir para contestar
                for (i = 0; i < CANT_ESCLAVOS; i++){
                     
                    if (readPipesFd[i] == fd){

                        if (currentSentFiles == fileQty){
                         
                            write(writePipesFd[i], "", 1);  //Terminé   
                            close(writePipesFd[i]);
                            FD_CLR(readPipesFd[i], &readfds);
                            close(readPipesFd[i]);
                            isSlaveClosed[i] = 1;
                        
                        }else{
                            sendNextFile(argv[1+currentSentFiles++], writePipesFd[i]);
                        }

                    
              
                        printf("Progress: %d/%d, ID is: %d : Filename is: %s, md5 is: %s \n", currentReadyFiles,fileQty,pidSlaves[i], lastFilename,lastMd5);
                        lastFilename[0] = 0;
                        lastMd5[0] = 0;;
                    }
                }
            }
        } 
          
    }

    return 0;
}


int sendNextFile(char* filename, int fd){
    if (write(fd, filename, strlen(filename)+1) == -1){
        perror("write");
    }
    return 0;
}


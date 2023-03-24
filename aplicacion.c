#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "config.h"

#define SLAVE_QTY 4

int sendString(char* string, int fd);

//Crea SLAVE_QTY esclavos, y 2 pipes para cada uno, guarda sus pid, writeFd y readFd en los parametros correspondientes
int createSlaves(int pidSlaves[SLAVE_QTY], int writePipesFd[SLAVE_QTY], int readPipesFd[SLAVE_QTY]);

//Devuelve el numero de esclavo a partir de un FD, o -1 si ese fd no corresponde a ningun esclavo
int getSlaveNumberFromFd(int fd, int readPipesFd[SLAVE_QTY], int writePipesFd[SLAVE_QTY]);

//Envia mensaje de cierre al esclavo, cierra los pipes correspondientes y lo marca como cerrado
void closeSlave(int slaveNumber, int readPipesFd[SLAVE_QTY], int writePipesFd[SLAVE_QTY], fd_set readfds, char isSlaveClosed[SLAVE_QTY]);

int main(int argc, char** argv){

    int fileQty = argc - 1;
    
    int currentReadyFiles = 0;
    int currentSentFiles = 0;  
    
    int pidSlaves[SLAVE_QTY];
    int writePipesFd[SLAVE_QTY];
    int readPipesFd[SLAVE_QTY];

    fd_set readfds;

    struct timeval interval;
    interval.tv_sec = 1;
    interval.tv_usec = 0;

    int i;

    int maxReadFD = createSlaves(pidSlaves, writePipesFd, readPipesFd);

    //Distribucion inicial de archivos
    for (i = 0; i < SLAVE_QTY; i++){
        //for (j = 0; j < initialFilesPerSlave; j++ ){
          //  char* fileName = argv[ 1 + currentSentFiles++]; //1+ para no enviar el primer argumento
            //char* fileName = "aplicacion.c"
            if (currentSentFiles == fileQty){
                sendString("", writePipesFd[i]);              
            }else{
                sendString(argv[1+currentSentFiles++], writePipesFd[i]);
            }
          
        //}
    }
     

    char lastMd5[MD5_LENGTH + 1];
    char lastFilename[MAX_PATH_LENGTH];

    char isSlaveClosed[SLAVE_QTY] = {0};

    while (currentReadyFiles < fileQty){
        FD_ZERO(&readfds);
        interval.tv_sec = 0;
        interval.tv_usec = 0;   
        for (i = 0; i < SLAVE_QTY; i++){
            if (!isSlaveClosed[i]){
                FD_SET(readPipesFd[i], &readfds);
            }
        }

        select(maxReadFD+1, &readfds, NULL, NULL, &interval);
        
        //Verifico quien esta listo para leer
        int fd;
        for (fd = 0; fd <= maxReadFD; fd++){
            if (FD_ISSET(fd, &readfds)){
                if (read(fd, lastMd5, MD5_LENGTH) == -1){
                    perror("read");
                }
                if (read(fd, lastFilename, MAX_PATH_LENGTH) == -1){
                    perror("read");
                }
        
                currentReadyFiles++;

                //Busco a donde tengo que escribir para contestar
                int slaveNumber = getSlaveNumberFromFd(fd, readPipesFd, writePipesFd);

                if (slaveNumber != -1){
                    if (currentSentFiles == fileQty){
                        closeSlave(slaveNumber, readPipesFd, writePipesFd, readfds, isSlaveClosed);                    
                    }else{
                        sendString(argv[1+currentSentFiles++], writePipesFd[slaveNumber]);
                    }            
                    printf("Progress: %d/%d, ID is: %d : Filename is: %s, md5 is: %s \n", currentReadyFiles,fileQty,pidSlaves[slaveNumber], lastFilename,lastMd5);
                    lastFilename[0] = 0;
                    lastMd5[0] = 0;;
                }
                
            }
        } 
          
    }

    return 0;
}

void closeSlave(int slaveNumber, int readPipesFd[SLAVE_QTY], int writePipesFd[SLAVE_QTY], fd_set readfds, char isSlaveClosed[SLAVE_QTY]){
    sendString("", writePipesFd[slaveNumber]);              
    if (close(writePipesFd[slaveNumber]) == -1){
        perror("close");
    }
    FD_CLR(readPipesFd[slaveNumber], &readfds);
    if (close(readPipesFd[slaveNumber]) == -1){
        perror("close");
    }
    isSlaveClosed[slaveNumber] = 1;
}

int getSlaveNumberFromFd(int fd, int readPipesFd[SLAVE_QTY], int writePipesFd[SLAVE_QTY]){
    int i;
    for (i = 0; i < SLAVE_QTY; i++){
        if (readPipesFd[i] == fd){
            return i;
        }
    }
    for (i = 0; i < SLAVE_QTY; i++){
        if (writePipesFd[i] == fd){
            return i;
        }
    }
            
    return -1;      

}

int sendString(char* string, int fd){
    if (write(fd, string, strlen(string)+1) == -1){
        perror("write");
    }
    return 0;
}

int createSlaves(int pidSlaves[SLAVE_QTY], int writePipesFd[SLAVE_QTY], int readPipesFd[SLAVE_QTY]){

    int i, maxReadFD = 0;
    for (i = 0; i < SLAVE_QTY; i++){

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
            if (execve("./esclavo.out", params, 0) == -1){
                perror("execve");
            }
        }
        //Aplicacion cierra los no usados
        if (close(pipeFd1[0]) == -1){
            perror("close");
        }
        if (close(pipeFd2[1]) == -1){
            perror("close");
        }
    }

    return maxReadFD;
}
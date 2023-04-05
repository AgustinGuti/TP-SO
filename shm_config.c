#include "shm_config.h"

char isProcessRunning(char *processName) {
    int findID = fork(); // Escribe al pipe
    if (findID == 0)
    {
        char buffer[strlen(processName) + 20];
        buffer[0] = 0;
        strcat(buffer, "pidof ");
        strcat(buffer, processName);
        strcat(buffer, " > /dev/null");
        char *const params[] = {"/bin/sh", "-c", buffer, NULL};
        execve("/bin/sh", params, 0);
        perror("execve");
        return 0;
    }

    int status;
    waitpid(findID, &status, 0);
    return WEXITSTATUS(status) == 0;
}
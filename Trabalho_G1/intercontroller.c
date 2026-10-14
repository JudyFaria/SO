#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <signal.h>

int main(){

    pid_t pid_kernel = getppid();

    for (int i = 0; ; i++){
        sleep(1);
        kill(pid_kernel, SIGUSR1);
    }

    return 0;
}

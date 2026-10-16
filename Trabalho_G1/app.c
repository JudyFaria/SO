#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <signal.h>

#include "common.h"

int main(int argc, char *argv[]){

    pid_t pid_kernel = getppid();
    
    int id = atoi(argv[1]);
    int fd = atoi(argv[2]);

    int pcs_syscall[] = {1, 1, 1}; // instantes do pedido e A1, A2 e A3
    char ops[] = {'R', 'W', 'R'};

    raise(SIGSTOP); // raise envia um sinal ao próprio processo
    // agora o app só entra no loop se alguém chamar SIGCONT(retomar)

    for (int i = 0; i < MAX; i++){
        printf("A%d PC=%d\n",id, i);

        MsgPC m = { .tipo = MSG_PC,  .num_app = id, .pc = i };
        write(fd, &m, sizeof(m));

        if( i == pcs_syscall[id - 1]){
            MsgPC s ={
                .tipo = MSG_SYSCALL, 
                .num_app = id, 
                .dispositivo = 1, 
                .operacao = ops[id - 1]
            };

            write(fd, &s, sizeof(s));
            kill(pid_kernel, SIGRTMIN); // só manda o sinal ao kernel; os dados estão no pipe
        }
        
        sleep(1); // segue rodando o app parado
    }

    return 0;
}

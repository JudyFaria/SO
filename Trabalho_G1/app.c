#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <signal.h>

#include "common.h"

int main(int argc, char *argv[]){

    setvbuf(stdout, NULL, _IOLBF, 0);   // uma linha = uma escrita (mantém a ordem na saída)

    pid_t pid_kernel = getppid();
    
    int id = atoi(argv[1]);
    int fd = atoi(argv[2]);


    // argv[3] = "<PC><operação>", por exemplo "2R" (pede leitura em PC=2).
    // "-" significa que este app não faz syscall.
    int pc_syscall = -1;
    char op = 'R';
    if (argc > 3 && argv[3][0] != '-'){
        pc_syscall = atoi(argv[3]);
        op = argv[3][strlen(argv[3]) - 1];
        if (op != 'R' && op != 'W') op = 'R';
    }

    raise(SIGSTOP); // raise envia um sinal ao próprio processo
    // agora o app só entra no loop se alguém chamar SIGCONT(retomar)

    for (int i = 0; i < MAX; i++){
        printf("A%d PC=%d\n",id, i);

        MsgPC m = { .tipo = MSG_PC,  .num_app = id, .pc = i };
        write(fd, &m, sizeof(m));

        if( i == pc_syscall){
            MsgPC s ={
                .tipo = MSG_SYSCALL, 
                .num_app = id, 
                .dispositivo = 1, 
                .operacao = op
            };

            write(fd, &s, sizeof(s));
            kill(pid_kernel, SIGRTMIN); // só manda o sinal ao kernel; os dados estão no pipe
        }
        
        sleep(1); // segue rodando o app parado
    }

    return 0;
}

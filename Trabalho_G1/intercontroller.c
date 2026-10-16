#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <signal.h>

int main(int argc, char *argv[]){

    pid_t pid_kernel = getppid();
    int fd = atoi(argv[1]); // descritor de leitura do pipe de I/O

    int pedidos = 0; // I/Os pendentes em D1 (em curso + os que esperam)
    int restante = 0; // ticks que faltam para terminar o I/O em curso

    for (int i = 0; ; i++){
        sleep(1); //  um tick - timeslice, o kernel apenas reage

        // descobre pedidos novos (cada byte = um pedido)
        char buf[16];
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) pedidos += n;

        // avança o I/O corrente
        if (restante > 0){ 
            restante--;
            if(restante == 0){
                pedidos--;
                kill(pid_kernel, SIGUSR2); // gera o IRQ1: terminou o I/O
            }
        }

        // se D1 está livre e há pedido esperando, começa o proximo
        if(restante == 0 && pedidos > 0){
            restante = 3;
        }

        // IRQ0 sai sempre, e depois do IRQ1
        kill(pid_kernel, SIGUSR1); 
    }

    return 0;
}

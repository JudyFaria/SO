#include <stdio.h>
#include <stdlib.h>
#include<sys/wait.h>
#include <unistd.h>

#include <signal.h>

#define N 3
pid_t pids[N];

// handler - função que o sistenma chama quando um sinal chega
struct sigaction sa;

// flag volatile
volatile sig_atomic_t irq0_pendente = 0;

// função do handler
void trata_irq0(int sig) {
    irq0_pendente = 1;    // só marca uma flag
}

int main(){

    sa.sa_handler = trata_irq0; // quem será chamada
    sigemptyset(&sa.sa_mask); // não bloqueia outros sinais durante o handler
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL); // liga SIGUSR1 (IRQ0) a essa função 

    
    // fork do InterController  
    pid_t pid_ic = fork();

    if (pid_ic == 0){
        execl("./interController", "ic", NULL);
        
        perror("execl");
        _exit(1);
    }
    

    // pai cria três filhos
    // for (int i = 0; i < N; i++){
    //     pids[i] = fork();

    //     if (pids[i] == 0){        
    //         // filho: 

    //         char num_processo[5];

    //         // escreve dentro de uma variável de texto
    //         snprintf(num_processo, sizeof(num_processo), "%d", i+1);

    //         execl("./app", "app", num_processo, NULL);
    //         // parâmetros: 1º caminho binário; 2º po convenção, o nome do programa; 3º numero do app; 4º sempre NULL

    //         perror("execl");
    //         _exit(1);

    //     }
    // }

    // espera todos os processos pararem 
    // for (int i = 0; i < N; i++){
    //     waitpid(pids[i], NULL, WUNTRACED);
    //     // WUNTRANCED faz o waitpid retornar quando o filho para, não só quando termina
    // }

    // pai faz o papel do Kernel, rodício manual (Round Robin)
    // int atual = 0;
    // for (int turno = 0; turno < 9; turno++){
    //     kill(pids[atual], SIGCONT); // app começa a rodar
    //     sleep(1); // <- timeslice
    //     kill(pids[atual], SIGSTOP); // timeslice acaba, para o app

    //     atual = (atual + 1) % N;
    // }

    // o pai espera CADA filho 
    // for (int i = 0; i < N; i++){
    //     kill(pids[i], SIGKILL);
    //     waitpid(pids[i], NULL, 0);
    // }

    for (int i = 0; i < 5; i++){
        pause(); // retorna qualquer sinal que tenha handler
        printf("vlr flag antes: %d\n", irq0_pendente);
        if( irq0_pendente != 0){
            irq0_pendente = 0;
            printf("IRQ0 recebido\n");
            printf("vlr flag depois: %d\n", irq0_pendente);
        }
    }
    
    
    kill(pid_ic, SIGKILL);
    waitpid(pid_ic, NULL, 0);

    return 0; 
}
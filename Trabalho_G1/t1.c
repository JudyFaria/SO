#include <stdio.h>
#include <stdlib.h>
#include<sys/wait.h>
#include <unistd.h>

#include <signal.h>

#define N 3
pid_t pids[N];

int main(){



    // pai cria três filhos
    for (int i = 0; i < N; i++){
        pids[i] = fork();

        if (pids[i] == 0){        
            // filho: 

            char num_processo[5];

            // escreve dentro de uma variável de texto
            snprintf(num_processo, sizeof(num_processo), "%d", i+1);

            execl("./app", "app", num_processo, NULL);
            // parâmetros: 1º caminho binário; 2º po convenção, o nome do programa; 3º numero do app; 4º sempre NULL

            perror("execl");
            _exit(1);

        }

        // pai:

    }
    

    // pai: faz o papel do "kernel"
    sleep(3);
    // kill(pid, SIGSTOP); //pausar
    // sleep(3);
    // kill(pid, SIGCONT); // retomar
    // sleep(3);
    // kill(pid, SIGKILL);
    

    // o pai espera CADA filho 
    for (int i = 0; i < N; i++){
        kill(pids[i], SIGKILL);
        waitpid(pids[i], NULL, 0);
    }
    

    return 0; 
}
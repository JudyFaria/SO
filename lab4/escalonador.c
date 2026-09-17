#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define NUM_PROCESSOS 6
#define QUANTUM 1          // duracao do time-slice, em segundos
#define MAX_TIME_SLICES 5  // apos essa quantidade de quantums o processo e encerrado

// Corpo de cada processo/tarefa: um loop eterno.
// O processo so avanca quando recebe SIGCONT do escalonador
// e para de avancar quando recebe SIGSTOP (preempcao).
void tarefa(int id) {
    // Se autossuspende logo apos nascer, para so comecar a rodar
    // quando o escalonador o despachar pela primeira vez.
    raise(SIGSTOP);

    int contador = 0;
    while (1) {
        printf("        [Processo %d | PID %d] em execucao (contador=%d)\n", id, getpid(), ++contador);
        fflush(stdout);
        usleep(200000); // ~5 impressoes por segundo de quantum, so para mostrar que esta rodando
    }
}

int main(void) {
    pid_t pids[NUM_PROCESSOS];
    int slices[NUM_PROCESSOS] = {0}; // quantidade de time-slices ja consumidos por processo
    int ativo[NUM_PROCESSOS];        // 1 = ainda participa do round-robin, 0 = ja terminou

    printf("[ESCALONADOR] Criando %d processos...\n", NUM_PROCESSOS);

    for (int i = 0; i < NUM_PROCESSOS; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            tarefa(i); // filho nunca retorna daqui (loop eterno)
            exit(0);
        }

        pids[i] = pid;
        ativo[i] = 1;
        printf("[ESCALONADOR] Processo %d criado (PID=%d)\n", i, pid);
    }

    // Garante que todos os filhos ja executaram o raise(SIGSTOP) inicial
    // antes do escalonador comecar a despachar (evita corrida de inicializacao).
    for (int i = 0; i < NUM_PROCESSOS; i++) {
        waitpid(pids[i], NULL, WUNTRACED);
    }

    printf("[ESCALONADOR] Todos os processos aguardando. Iniciando Round-Robin "
           "(quantum=%ds, max time-slices=%d)\n\n", QUANTUM, MAX_TIME_SLICES);

    int restantes = NUM_PROCESSOS;
    int atual = 0;

    while (restantes > 0) {
        if (!ativo[atual]) {
            atual = (atual + 1) % NUM_PROCESSOS;
            continue;
        }

        slices[atual]++;
        printf("[ESCALONADOR] >>> Despachando processo %d (PID=%d) - time-slice %d/%d\n",
               atual, pids[atual], slices[atual], MAX_TIME_SLICES);

        kill(pids[atual], SIGCONT); // libera o processo para rodar
        sleep(QUANTUM);             // quantum de execucao do escalonador

        if (slices[atual] >= MAX_TIME_SLICES) {
            // processo consumiu seus 5 time-slices: escalonador o encerra
            kill(pids[atual], SIGKILL);
            waitpid(pids[atual], NULL, 0);
            ativo[atual] = 0;
            restantes--;
            printf("[ESCALONADOR] <<< Processo %d (PID=%d) atingiu %d time-slices. "
                   "Encerrado com SIGKILL.\n\n", atual, pids[atual], MAX_TIME_SLICES);
        } else {
            // preempcao: para o processo e devolve ele para o fim da fila
            kill(pids[atual], SIGSTOP);
            waitpid(pids[atual], NULL, WUNTRACED);
            printf("[ESCALONADOR] <<< Processo %d (PID=%d) preemptado (SIGSTOP).\n\n",
                   atual, pids[atual]);
        }

        atual = (atual + 1) % NUM_PROCESSOS;
    }

    printf("[ESCALONADOR] Todos os %d processos foram encerrados. Fim da execucao.\n", NUM_PROCESSOS);
    return 0;
}

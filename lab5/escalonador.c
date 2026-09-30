#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

#define NUM_PROCESSOS 3

// Janelas de execucao de cada processo dentro do minuto (0-59s)
#define P1_INICIO 5
#define P1_DURACAO 20  // P1 executa de 5s a 25s
#define P2_INICIO 45
#define P2_DURACAO 15  // P2 executa de 45s a 60s (fim do minuto)

typedef enum { P1 = 0, P2 = 1, P3 = 2 } Processo;

// Corpo de cada processo filho: um loop eterno que so avanca
// quando o escalonador libera com SIGCONT (e para com SIGSTOP).
void tarefa(int id) {
    // Se autossuspende logo apos nascer, para so comecar a rodar
    // quando o escalonador o despachar pela primeira vez.
    raise(SIGSTOP);

    int contador = 0;
    while (1) {
        printf("        [P%d | PID %d] em execucao (contador=%d)\n", id + 1, getpid(), ++contador);
        fflush(stdout);
        usleep(200000);
    }
}

// Verifica se 'segundo' (0-59, com fracao) esta dentro da janela
// [inicio, inicio+duracao) de um processo.
int dentro_da_janela(double segundo, int inicio, int duracao) {
    return segundo >= inicio && segundo < inicio + duracao;
}

int main(void) {
    pid_t pids[NUM_PROCESSOS];
    int rodando[NUM_PROCESSOS] = {0, 0, 0}; // estado (rodando/parado) conhecido pelo escalonador

    printf("[ESCALONADOR] Criando processos P1, P2 e P3...\n");

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
        printf("[ESCALONADOR] P%d criado (PID=%d)\n", i + 1, pid);
    }

    // Garante que os 3 filhos ja executaram o raise(SIGSTOP) inicial
    // antes do escalonador comecar a despachar (evita corrida de inicializacao).
    for (int i = 0; i < NUM_PROCESSOS; i++) {
        waitpid(pids[i], NULL, WUNTRACED);
    }

    printf("[ESCALONADOR] P1: inicia em %ds do minuto, executa por %ds (ate %ds)\n",
           P1_INICIO, P1_DURACAO, P1_INICIO + P1_DURACAO);
    printf("[ESCALONADOR] P2: inicia em %ds do minuto, executa por %ds (ate %ds)\n",
           P2_INICIO, P2_DURACAO, P2_INICIO + P2_DURACAO);
    printf("[ESCALONADOR] P3: executa sempre que P1 e P2 estao parados\n\n");

    struct timeval tv;

    while (1) {
        gettimeofday(&tv, NULL);
        double segundo = (tv.tv_sec % 60) + tv.tv_usec / 1000000.0;

        int desejado[NUM_PROCESSOS];
        desejado[P1] = dentro_da_janela(segundo, P1_INICIO, P1_DURACAO);
        desejado[P2] = dentro_da_janela(segundo, P2_INICIO, P2_DURACAO);
        desejado[P3] = !(desejado[P1] || desejado[P2]);

        for (int i = 0; i < NUM_PROCESSOS; i++) {
            if (desejado[i] && !rodando[i]) {
                kill(pids[i], SIGCONT);
                rodando[i] = 1;
                printf("[ESCALONADOR] t=%5.1fs -> P%d LIBERADO (SIGCONT)\n", segundo, i + 1);
            } else if (!desejado[i] && rodando[i]) {
                kill(pids[i], SIGSTOP);
                rodando[i] = 0;
                printf("[ESCALONADOR] t=%5.1fs -> P%d PARADO  (SIGSTOP)\n", segundo, i + 1);
            }
        }

        usleep(100000); // checagem do escalonador: 100ms
    }

    return 0;
}

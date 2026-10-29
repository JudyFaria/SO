#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define TOTAL_MSGS 32

// atraso do consumidor antes de cada recebimento (em unidades) 
#ifndef PERIODO_CONS
#define PERIODO_CONS 1
#endif

// um tipo para cada "sentido" da conversa na mesma fila 
enum { TIPO_DADO = 1, TIPO_ACK = 2 };

typedef struct {
    long mtype;
    int  valor;
} mensagem_t;

#define TAM_PAYLOAD sizeof(int)

static int    qid = -1;
static long   unidade_ms = 1000;
static double t0;

static double agora(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

static void espera(int unidades) {
    long ms = (long)unidades * unidade_ms;
    struct timespec d = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&d, NULL);
}

static void trata_sigint(int sig) {
    (void)sig;
    msgctl(qid, IPC_RMID, NULL);
    _exit(130);
}

static void produtor(void) {
    mensagem_t m, ack;

    for (int i = 1; i <= TOTAL_MSGS; i++) {
        double antes = agora();

        m.mtype = TIPO_DADO;
        m.valor = i;
        if (msgsnd(qid, &m, TAM_PAYLOAD, 0) == -1) {
            perror("produtor: msgsnd");
            exit(1);
        }
        printf("[t=%7.2fs] PRODUTOR  (pid %d) enviou %2d, aguardando ACK...\n",
               agora() - t0, getpid(), i);

        // BLOQUEIA até o consumidor devolver o ACK (tipo 2) 
        if (msgrcv(qid, &ack, TAM_PAYLOAD, TIPO_ACK, 0) == -1) {
            perror("produtor: msgrcv(ACK)");
            exit(1);
        }
        printf("[t=%7.2fs] PRODUTOR  (pid %d) recebeu ACK do %2d "
               "(ficou bloqueado %.2fs)\n",
               agora() - t0, getpid(), ack.valor, agora() - antes);
    }
    printf("[t=%7.2fs] PRODUTOR  (pid %d) terminou: %d mensagens enviadas e confirmadas\n",
           agora() - t0, getpid(), TOTAL_MSGS);
}

static void consumidor(void) {
    mensagem_t m, ack;
    int consumidas = 0;

    for (int i = 0; i < TOTAL_MSGS; i++) {
        espera(PERIODO_CONS);   // simula o consumidor "ocupado"

        if (msgrcv(qid, &m, TAM_PAYLOAD, TIPO_DADO, 0) == -1) {
            perror("consumidor: msgrcv");
            exit(1);
        }
        consumidas++;
        printf("[t=%7.2fs] CONSUMIDOR(pid %d) recebeu %2d\n",
               agora() - t0, getpid(), m.valor);

        // libera o produtor: confirma o recebimento
        ack.mtype = TIPO_ACK;
        ack.valor = m.valor; // devolve o número que recebeu

        if (msgsnd(qid, &ack, TAM_PAYLOAD, 0) == -1) {
            perror("consumidor: msgsnd(ACK)");
            exit(1);
        }
    }
    printf("[t=%7.2fs] CONSUMIDOR(pid %d) terminou: %d mensagens consumidas\n",
           agora() - t0, getpid(), consumidas);
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        unidade_ms = atol(argv[1]);
        if (unidade_ms <= 0) {
            fprintf(stderr, "uso: %s [ms_por_unidade]\n", argv[0]);
            return 1;
        }
    }
    setvbuf(stdout, NULL, _IOLBF, 0);

    qid = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    if (qid == -1) { perror("msgget"); return 1; }

    signal(SIGINT, trata_sigint);
    t0 = agora();

    pid_t pid = fork();
    if (pid == -1) { perror("fork"); msgctl(qid, IPC_RMID, NULL); return 1; }

    if (pid == 0) {
        consumidor();
        exit(0);
    }

    produtor();
    wait(NULL);
    msgctl(qid, IPC_RMID, NULL);
    return 0;
}
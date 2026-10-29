#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
 
#define TOTAL_MSGS 256   // produzidas = consumidas (pelos dois juntos)
#define TAM_FILA    64   // maximo de elementos na fila
 
/* periodos em "unidades" (1 unidade = 1 s por padrao) */
#ifndef PERIODO_PROD
#define PERIODO_PROD 1
#endif
#ifndef PERIODO_C1
#define PERIODO_C1 1
#endif
#ifndef PERIODO_C2
#define PERIODO_C2 2
#endif
 
// tipos de mensagem (campo mtype) 
enum { TIPO_DADO = 1, TIPO_FIM = 2, TIPO_RESUMO = 3 };
 
typedef struct {
    long mtype;   // obrigatorio, primeiro campo, > 0 
    int  valor;   // conteudo da mensagem 
} mensagem_t;
 
#define TAM_PAYLOAD sizeof(int)   // tamanho do conteudo, SEM o mtype 
 
static int    qid = -1;
static long   unidade_ms = 1000;
static double t0;
 

// funções uteis 
 
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
 
// quantas mensagens estao na fila neste instante 
static int ocupacao(void) {
    struct msqid_ds info;
    if (msgctl(qid, IPC_STAT, &info) == -1) return -1;
    return (int)info.msg_qnum;
}
 
// Ctrl+C: remove a fila para nao deixa-la "vazando" no kernel 
static void trata_sigint(int sig) {
    (void)sig;
    msgctl(qid, IPC_RMID, NULL);
    _exit(130);
}
 

// processos
 
static void produtor(void) {
    mensagem_t m;
    m.mtype = TIPO_DADO;
 
    for (int i = 1; i <= TOTAL_MSGS; i++) {
        m.valor = i;
        if (msgsnd(qid, &m, TAM_PAYLOAD, 0) == -1) {
            perror("produtor: msgsnd");
            exit(1);
        }
        printf("[t=%7.2fs] PRODUTOR  (pid %d) enviou %3d | fila=%d\n",
               agora() - t0, getpid(), m.valor, ocupacao());
        if (i < TOTAL_MSGS) espera(PERIODO_PROD);
    }
 
    // uma mensagem de fim para cada consumidor 
    m.mtype = TIPO_FIM;
    m.valor = 0;
    for (int c = 0; c < 2; c++) {
        if (msgsnd(qid, &m, TAM_PAYLOAD, 0) == -1) {
            perror("produtor: msgsnd(FIM)");
            exit(1);
        }
    }
    printf("[t=%7.2fs] PRODUTOR  (pid %d) terminou: %d mensagens produzidas\n",
           agora() - t0, getpid(), TOTAL_MSGS);
}
 
static void consumidor(int id, int periodo) {
    mensagem_t m;
    int consumidas = 0;
 
    while (1) {
        /* msgtyp = -TIPO_FIM: pega o MENOR tipo <= 2, ou seja, dados
         (tipo 1) sempre saem antes da mensagem de fim (tipo 2) */
        if (msgrcv(qid, &m, TAM_PAYLOAD, -TIPO_FIM, 0) == -1) {
            perror("consumidor: msgrcv");
            exit(1);
        }
        if (m.mtype == TIPO_FIM) break;
 
        consumidas++;
        printf("[t=%7.2fs] CONSUMID%d (pid %d) retirou %3d | fila=%d\n",
               agora() - t0, id, getpid(), m.valor, ocupacao());
        espera(periodo);
    }
 
    printf("[t=%7.2fs] CONSUMID%d (pid %d) terminou: consumiu %d mensagens\n",
           agora() - t0, id, getpid(), consumidas);
 
    // informa ao pai quantas consumiu, para conferir o total 
    m.mtype = TIPO_RESUMO;
    m.valor = consumidas;
    if (msgsnd(qid, &m, TAM_PAYLOAD, 0) == -1) {
        perror("consumidor: msgsnd(RESUMO)");
        exit(1);
    }
}
 
// cod principal
int main(int argc, char *argv[]) {
    if (argc > 1) {
        unidade_ms = atol(argv[1]);
        if (unidade_ms <= 0) {
            fprintf(stderr, "uso: %s [ms_por_unidade]\n", argv[0]);
            return 1;
        }
    }
    setvbuf(stdout, NULL, _IOLBF, 0);   // linha a linha, mesmo com redirecionamento 
 
    // 1- cria a fila ANTES do fork: os filhos herdam o qid 
    qid = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    if (qid == -1) { perror("msgget"); return 1; }
 
    // 2- limita a fila a 64 elementos (o limite do kernel e em bytes)
    struct msqid_ds info;
    if (msgctl(qid, IPC_STAT, &info) == -1) { perror("msgctl(IPC_STAT)"); return 1; }
    info.msg_qbytes = TAM_FILA * TAM_PAYLOAD;
    if (msgctl(qid, IPC_SET, &info) == -1) { perror("msgctl(IPC_SET)"); return 1; }
 
    signal(SIGINT, trata_sigint);
    t0 = agora();
 
    printf("fila qid=%d | limite=%d elementos (%lu bytes) | %d mensagens\n",
           qid, TAM_FILA, (unsigned long)(TAM_FILA * TAM_PAYLOAD), TOTAL_MSGS);
 
    // 3- cria os dois consumidores
    int periodos[2] = { PERIODO_C1, PERIODO_C2 };
    for (int c = 0; c < 2; c++) {
        pid_t pid = fork();
        if (pid == -1) { perror("fork"); msgctl(qid, IPC_RMID, NULL); return 1; }
        if (pid == 0) {
            consumidor(c + 1, periodos[c]);
            exit(0);
        }
    }
 
    // 4- o processo pai e o produtor
    produtor();
 
    // 5- recolhe os resumos dos consumidores e confere o total
    int total = 0;
    for (int c = 0; c < 2; c++) {
        mensagem_t r;
        if (msgrcv(qid, &r, TAM_PAYLOAD, TIPO_RESUMO, 0) == -1) {
            perror("pai: msgrcv(RESUMO)");
            break;
        }
        total += r.valor;
    }
    while (wait(NULL) > 0) ;
 
    printf("TOTAL: produzidas=%d consumidas=%d -> %s\n", TOTAL_MSGS, total,
           total == TOTAL_MSGS ? "OK" : "DIVERGENTE");
 
    // 6- remove a fila
    msgctl(qid, IPC_RMID, NULL);
    return 0;
}
#ifndef COMMON_H
#define COMMON_H
#define MAX 5

typedef enum { MSG_PC, MSG_SYSCALL } TipoMsg;

typedef struct {
    TipoMsg tipo;
    int num_app;
    int pc;
    int dispositivo; // 1 = D1
    char operacao; // 'W' ou 'R'
} MsgPC;
#endif
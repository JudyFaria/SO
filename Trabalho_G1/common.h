#define MAX 10

typedef enum { MSG_PC, MSG_SYSCALL } TipoMsg;

typedef struct {
    TipoMsg tipo;
    int num_app;
    int pc;
    int dispositivo; // 1 = D1
    char operacao; // 'W' ou 'R'
} MsgPC;
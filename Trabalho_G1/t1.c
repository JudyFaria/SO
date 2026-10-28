#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>

#include <signal.h>
#include <fcntl.h>
#include "common.h"

// para definir na compilação
#ifndef N
#define N 3
#endif

#if N < 3 || N > 6
#error "N deve ser entre 3 e 6 (enuciado)"
#endif

// --> handler | Sinais
// handler - função que o sistenma chama quando um sinal chega
struct sigaction sa0;
struct sigaction sa1;
struct sigaction saSysCall;
struct sigaction saChld;
struct sigaction saTerm;

// flag volatile
volatile sig_atomic_t irq0_pendente = 0;
volatile sig_atomic_t irq1_pendente = 0;
volatile sig_atomic_t syscall_pendente = 0;
volatile sig_atomic_t filho_terminou = 0;
volatile sig_atomic_t encerrar = 0;

// função do handler
void trata_irq0(int sig) {
    irq0_pendente = 1;    // só marca uma flag
}

void trata_irq1(int sig){
    irq1_pendente = 1;
}

void trata_syscall(int sig){
    syscall_pendente = 1;
}

void trata_chld(int sig){
    filho_terminou = 1;
}

void trata_term(int sig){
    encerrar = 1;
}

// --> PCB 
typedef enum { PRONTO, RODANDO, BLOQUEADO, TERMINADO } Estado;

typedef struct{
    pid_t pid; // quem é no sistema
    int num_app; // número do app (id): 1, 2, 3..
    int pc; // contexto salvo, quantas iterações já executou
    Estado estado;
    int dispositivo;
    char operacao;
    int pediu_syscall;
}PCB;

PCB pcbs[N]; 

// --> Filas (prontos e bloqueados I/O)
typedef struct{
    int itens[N];
    int inicio;
    int tamanho;
} Fila;

Fila prontos;
Fila bloqueados;

// ->  estado do kernel, acessado apenas no laço principal
// canal[0] = leitura, canal[1] = escrita
int canal[2]; // pipe app - kernel (PC e syscall)
int io[2]; // pipe kernel - IC (I/O)

int rodando = -1; // global, usados nas funçoes auxiliares
int terminados = 0;
int tempo = 0; // relógio lógico: quantos IRQ0 já chegaram ( 1 tick = 1s )

void enfileira(Fila *f, int id){
    f->itens[(f->inicio + f->tamanho) % N] = id; //primeira posição livre depois do último elemento
    f->tamanho++;
}

int desenfilera(Fila *f){
    if (f->tamanho == 0){
        return -1;
    }
    int id = f->itens[f->inicio];
    f->inicio = (f->inicio + 1) % N;
    f->tamanho--;

    return id;
}

void imprime_fila(Fila *f){
    for(int i = 0; i < f->tamanho; i++){
        int id = f->itens[(f->inicio + i) % N];
        printf("A%d ", pcbs[id].num_app);
    }
}

void imprime_estado(int rodando, const char *evento){
    printf("[t=%2ds] %-8s| ", tempo, evento);

    if (rodando >= 0){
        printf("A%d rodando", pcbs[rodando].num_app);
    }
    else{
        printf("Nenhum processo rodando");
    }

    printf(" | Prontos: ");
    imprime_fila(&prontos);

    printf(" | Bloqueados: ");
    imprime_fila(&bloqueados);

    printf(" | PCs: ");
    for(int i = 0; i < N; i++){
        printf("A%d=%d%s ", pcbs[i].num_app, pcbs[i].pc, pcbs[i].estado == TERMINADO ? "(fim)" : "");
    }

    printf("\n");
}

// lê tudo que os apps escreveram no pipe e atualiza os PCBs
void atualiza_pcs(int fd_leitura){
    MsgPC m;
    while (read(fd_leitura, &m, sizeof(m)) == sizeof(m)){
        PCB *p = &pcbs[m.num_app - 1];
        if (m.tipo == MSG_PC){
            p->pc = m.pc;
        }
        else{
            p->dispositivo = m.dispositivo;
            p->operacao = m.operacao;
            p->pediu_syscall = 1;
        }
    }
}

void bloqueia( int id, int fd_io){
    pcbs[id].pediu_syscall = 0;
    pcbs[id].estado = BLOQUEADO;

    enfileira(&bloqueados, id);

    char bloq = 1;
    write(fd_io, &bloq, 1); // avisa o IC que tem um pedido novo em D1
}

// tira o próximo da fila de prontos e o libera
int despacha(){
    int id = desenfilera(&prontos);

    if (id >= 0){
        pcbs[id].estado = RODANDO;
        kill(pcbs[id].pid, SIGCONT);
    }

    return id;
}

// registra o término de um processo
void finaliza(int id){
    atualiza_pcs(canal[0]);

    pcbs[id].estado = TERMINADO;
    pcbs[id].pediu_syscall = 0;

    terminados++;

    if (rodando == id){
        rodando = -1;
    }

    printf("[t=%2ds] FIM | A%d terminou (PC final = %d)\n", tempo, pcbs[id].num_app, pcbs[id].pc);
}

// para o processo, de fato, e salva o contexto
int para_e_salva(int id){
    int st = 0;

    kill(pcbs[id].pid, SIGSTOP);
    waitpid(pcbs[id].pid, &st, WUNTRACED);

    if(WIFSTOPPED(st)){
        atualiza_pcs(canal[0]);
        
        return 1; // se realmente parou
    }
    
    finaliza(id); 
    
    return 0; // se já havia terminado 
}

// verifica quais processos terminaram, sem travar
void verifica_terminados(){
    for (int i = 0; i < N; i++){
        if (pcbs[i].estado == TERMINADO) continue;

        int st;
        if (waitpid(pcbs[i].pid, &st, WNOHANG) == pcbs[i].pid){
            finaliza(i);
        }
    }
}

int main(int argc, char *argv[]){

    setvbuf(stdout, NULL, _IOLBF, 0);   // uma linha = uma escrita (antes de qualquer fork)

    // ---- tabela de syscalls: argv[1..N], um token "<PC><R|W>" por app ("-" = nenhum) ----
    // static const char *padrao[6] = {"2R", "2W", "3R", "3W", "4R", "4W"};
    static const char *padrao[6] = {"-", "-", "-", "1R", "-", "-"};
    const char *tokens[N];
    for (int i = 0; i < N; i++)
        tokens[i] = (argc > 1 + i) ? argv[1 + i] : padrao[i];
 
    printf("--- %d apps, MAX=%d, syscalls:", N, MAX);
    for (int i = 0; i < N; i++) printf(" A%d=%s", i + 1, tokens[i]);
    printf(" ---\n");

    // -> registra o handler - irq0
    sa0.sa_handler = trata_irq0; // quem será chamada
    sigemptyset(&sa0.sa_mask); // não bloqueia outros sinais durante o handler
    sa0.sa_flags = 0;
    sigaction(SIGUSR1, &sa0, NULL); // liga SIGUSR1 (IRQ0) a essa função 

    // -> registra o handler - irq1
    sa1.sa_handler = trata_irq1;
    sigemptyset(&sa1.sa_mask);
    sa1.sa_flags = 0;
    sigaction(SIGUSR2, &sa1, NULL); // liga SIGUSR2 (IRQ1) a essa função 
    
    // -> registra o hendler - SysCall
    saSysCall.sa_handler = trata_syscall;
    sigemptyset(&saSysCall.sa_mask);
    saSysCall.sa_flags = 0;
    sigaction(SIGRTMIN, &saSysCall, NULL);

    // -> registra o hendler - sigchld (qunaod o filho termina)
    saChld.sa_handler = trata_chld;
    sigemptyset(&saChld.sa_mask);
    saChld.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGCHLD, &saChld, NULL);

    // -> registra o hendler - sigterm (quando recebe o sinal de término)
    saTerm.sa_handler = trata_term;
    sigemptyset(&saTerm.sa_mask);
    saTerm.sa_flags = 0;
    sigaction(SIGTERM, &saTerm, NULL);
    sigaction(SIGINT, &saTerm, NULL);

    
    // pipe dos apps (comunicação entre processos)
    pipe(canal);
    fcntl(canal[0], F_SETFL, O_NONBLOCK);

        
    // cria os processos e o PCB é preenchido pelo pai
    // pai cria três filhos
    for (int i = 0; i < N; i++){
        
        pid_t pid = fork();

        if (pid == 0){        
            // filho: 
            close(canal[0]); // app não lê, só escreve

            char num_processo[5];
            char fd_str[8];

            // escreve dentro de uma variável de texto
            snprintf(num_processo, sizeof(num_processo), "%d", i+1);
            snprintf(fd_str, sizeof(fd_str), "%d", canal[1]);

            execl("./app", "app", num_processo, fd_str, tokens[i], NULL);
            // parâmetros: 1º caminho binário; 2º po convenção, o nome do programa; 3º numero do app; 4° descritor de escrita; Último - sempre NULL

            perror("execl");
            _exit(1);
        }

        //guarda o pid no pcb
        pcbs[i].pid = pid;
        pcbs[i].num_app = i + 1;
        pcbs[i].pc = 0;
        pcbs[i].estado = PRONTO;
    }

    close(canal[1]); // o kernel só lê, os app têm sua propria cópia

    // espera todos os processos pararem (raise(SIGSTOP) no app)
    for (int i = 0; i < N; i++){
        waitpid(pcbs[i].pid, NULL, WUNTRACED);
        // WUNTRACED faz o waitpid retornar quando o filho para, não só quando termina
    }

    // coloca todos na fila de prontos
    for (int i = 0; i < N; i++){
        enfileira(&prontos, i);
    }

    // pipe de I/O
    pipe(io);
    fcntl(io[0],F_SETFL, O_NONBLOCK);
    
    // começa a rodar os processos, primeiro despacho
    rodando = despacha();
    imprime_estado(rodando, "INICIO");

    // fork do InterController  
    pid_t pid_ic = fork();

    if (pid_ic == 0){
        close(io[1]); // o IC só lê
        close(canal[0]); // não preciso do pipe dos apps

        char fd_io_str[8];
        snprintf(fd_io_str, sizeof(fd_io_str), "%d", io[0]);

        execl("./interController", "ic", fd_io_str ,NULL);
        
        perror("execl");
        _exit(1);
    }
    close(io[0]); // kernel só escreve

    // --> Bloqueia os sinais depois de criar os filhos 
    sigset_t mascara_irq, mascara_vazia; // conjunto de sinais
    
    sigemptyset(&mascara_irq); // esvazia a lista
    sigemptyset(&mascara_vazia);
    
    sigaddset(&mascara_irq, SIGUSR1); // coloca um sinal na lista
    sigaddset(&mascara_irq, SIGUSR2);
    sigaddset(&mascara_irq, SIGRTMIN);
    sigaddset(&mascara_irq, SIGCHLD);

    sigaddset(&mascara_irq, SIGTERM);
    sigaddset(&mascara_irq, SIGINT);

    sigprocmask(SIG_BLOCK, &mascara_irq, NULL); // coloca os sinais em bloqueio no processo


    // --> laço principal do kernel
    
    // pai faz o papel do Kernel, rodício manual (Round Robin)
    int aviso_final = 0;
    for(int i = 0; ;i++){
        // acordou só com o IRQ1? espera o IRQ0 do mesmo tick, para tratar os dois juntos
        while (irq1_pendente && !irq0_pendente && !encerrar)
            sigsuspend(&mascara_vazia);

        if(encerrar) break; // se recebeu SIGTERM ou SIGINT, encerra o kernel
        
        if (irq0_pendente) tempo++; //conta o tick antes de tratar qualquer bloco

        // o processo que terminou saí antes de qualquer outra decisão
        if (filho_terminou){
            filho_terminou = 0;

            verifica_terminados();
            
            if (rodando < 0){
                rodando = despacha();
                imprime_estado(rodando, "FIM");
            }
        }
        
        // processos em espera de I/O têm prioridade
        if (irq1_pendente){
            irq1_pendente = 0; //zera a flag
            
            int liberado = desenfilera(&bloqueados);
            if(liberado >= 0){
                // altera o estado e coloca noa fila de prontos
                pcbs[liberado].estado = PRONTO;
                enfileira(&prontos, liberado);
                printf("[t=%2ds] %-8s| A%d concluiu o I/O em D1 (%c)\n", tempo, "IRQ1", pcbs[liberado].num_app, pcbs[liberado].operacao);
            }

            if(rodando < 0){   
               rodando = despacha();  // a CPU fica livre e já escolhe o proximo processo
                imprime_estado(rodando, "IRQ1");
            }
        }

        // bloco SysCall
        if (syscall_pendente){
            syscall_pendente = 0;
            atualiza_pcs(canal[0]);

            if (rodando >= 0 && pcbs[rodando].pediu_syscall){
                if (para_e_salva(rodando)){
                     bloqueia(rodando, io[1]);
                }
                
                rodando = despacha();  // a CPU fica livre e já escolhe o proximo processo
                imprime_estado(rodando, "SYSCALL");
            }     
        }

        // fim de timeslice
        if(irq0_pendente){
            irq0_pendente = 0; // zera a flag

            // --> Troca de Contexto

            // para quem está rodando e coloca no final da fila
            if (rodando >= 0){
                
                if (para_e_salva(rodando)){
                    if (pcbs[rodando].pediu_syscall){
                        bloqueia(rodando, io[1]);
                    }
                    else{
                        pcbs[rodando].estado = PRONTO;
                        enfileira(&prontos, rodando);
                    }
                }
            }

            // tirar o próximo da fila e libera o novo processo
            rodando = despacha();

            if (terminados < N) imprime_estado(rodando, "IRQ0");
        }

      
        if (terminados == N && !aviso_final){
            aviso_final = 1;
            printf("[t=%2ds] Todos os processos terminaram. Kernel ocioso (Ctrl+C encerra).\n", tempo);
        }

        // // fim do teste: todos terminaram
        // if (terminados == N){
        //     break;
        // }

        // printf("Todos os processos terminaram.\n");

        // desbloqueia e dorme de forma atômica
        sigsuspend(&mascara_vazia);
    }
    
    kill(pid_ic, SIGKILL);
    waitpid(pid_ic, NULL, 0);
    
    for (int i = 0; i < N; i++){
        if(pcbs[i].estado != TERMINADO){
            kill(pcbs[i].pid, SIGKILL); // SIGKILL atinge até o processo parado
            waitpid(pcbs[i].pid, NULL, 0);
        }
    }

    return 0; 
}
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>

#include <signal.h>
#include <fcntl.h>
#include "common.h"

#define N 3
#define NUM_IRQS 20

// --> handler
// handler - função que o sistenma chama quando um sinal chega
struct sigaction sa0;
struct sigaction sa1;
struct sigaction saSysCall;

// flag volatile
volatile sig_atomic_t irq0_pendente = 0;
volatile sig_atomic_t irq1_pendente = 0;
volatile sig_atomic_t syscall_pendente = 0;

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

// --> PCB 
typedef enum { PRONTO, RODANDO, BLOQUEADO } Estado;

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

void bloqueia( int id, int fd_io){
    pcbs[id].pediu_syscall = 0;
    pcbs[id].estado = BLOQUEADO;

    enfileira(&bloqueados, id);

    char bloq = 1;
    write(fd_io, &bloq, 1); // avisa o IC que tem um pedido novo em D1
}

void imprime_fila(Fila *f){
    for(int i = 0; i < f->tamanho; i++){
        int id = f->itens[(f->inicio + i) % N];
        printf("A%d ", pcbs[id].num_app);
    }
}

void imprime_estado(int rodando){
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
        printf("A%d=%d ", pcbs[i].num_app, pcbs[i].pc);
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

int main(){

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

    
    // criando pipe (comunicação entre processos)
    int canal[2]; // canal[0] = leitura, canal[1] = escrita
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

            execl("./app", "app", num_processo, fd_str, NULL);
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
        // WUNTRANCED faz o waitpid retornar quando o filho para, não só quando termina
    }

    // coloca todos na fila de prontos
    for (int i = 0; i < N; i++){
        enfileira(&prontos, i);
        pcbs[i].estado = PRONTO;
    }

    // pipe de I/O
    int io[2];
    pipe(io);
    fcntl(io[0],F_SETFL, O_NONBLOCK);
    
    // começa a rodar os processos
    int rodando = desenfilera(&prontos);
    kill(pcbs[rodando].pid, SIGCONT);
    imprime_estado(rodando);

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

    // --> laço principal do kernel
    
    // pai faz o papel do Kernel, rodício manual (Round Robin)
    int irqs = 0;
    int liberado;

    while (irqs < NUM_IRQS){
        pause(); // retorna qualquer sinal de handler
        
        // processos em espera de I/O têm prioridade
        if (irq1_pendente){
            irq1_pendente = 0; //zera a flag
            
            liberado = desenfilera(&bloqueados);
            if(liberado >= 0){
                // altera o estado e coloca noa fila de prontos
                pcbs[liberado].estado = PRONTO;
                enfileira(&prontos, liberado);
            }

            if(rodando < 0){   
               rodando = desenfilera(&prontos);  // a CPU fica livre e já escolhe o proximo processo
                if (rodando >= 0){
                    pcbs[rodando].estado = RODANDO;
                    kill(pcbs[rodando].pid, SIGCONT);
                }
                imprime_estado(rodando);
            }
    
        }

        // bloco SYSCALL
        if (syscall_pendente){
            syscall_pendente = 0;
            atualiza_pcs(canal[0]);

            if (rodando >= 0 && pcbs[rodando].pediu_syscall){
                kill(pcbs[rodando].pid, SIGSTOP); 
                waitpid(pcbs[rodando].pid, NULL, WUNTRACED);      
                atualiza_pcs(canal[0]); // última leitura, com o PC final
                
                bloqueia(rodando, io[1]);

                rodando = desenfilera(&prontos);  // a CPU fica livre e já escolhe o proximo processo
                if (rodando >= 0){
                    pcbs[rodando].estado = RODANDO;
                    kill(pcbs[rodando].pid, SIGCONT);
                }

                imprime_estado(rodando);
            }     
        }

        if(irq0_pendente){
            irq0_pendente = 0; // zera a flag
            irqs++;

            // --> Troca de Contexto

            // para quem está rodando e coloca no final da fila
            if (rodando >= 0){
                
                kill(pcbs[rodando].pid, SIGSTOP); // apenas manda sinal, não para de fato
                waitpid(pcbs[rodando].pid, NULL, WUNTRACED); // espera parar de fato, garantindo que o app não escreva mais nada antes de o kernel ler.
                
                atualiza_pcs(canal[0]); // salva o contexto, ler o que o app escreveu
                
                if (pcbs[rodando].pediu_syscall){
                    bloqueia(rodando, io[1]);
                }
                else{
                    pcbs[rodando].estado = PRONTO;
                    enfileira(&prontos, rodando);
                }
            }

            // tirar o próximo da fila
            rodando = desenfilera(&prontos);

            // libera o novo processo
            if(rodando >= 0){
                kill(pcbs[rodando].pid, SIGCONT);
                pcbs[rodando].estado = RODANDO;
            }

            imprime_estado(rodando);
        }
    }
    
    kill(pid_ic, SIGKILL);
    waitpid(pid_ic, NULL, 0);

    for (int i = 0; i < N; i++){
        kill(pcbs[i].pid, SIGKILL);
        waitpid(pcbs[i].pid, NULL, 0);
    }

    return 0; 
}
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>

#include <signal.h>
#include <fcntl.h>
#include "common.h"

#define N 3
#define NUM_IRQS 12

// --> handler
// handler - função que o sistenma chama quando um sinal chega
struct sigaction sa;

// flag volatile
volatile sig_atomic_t irq0_pendente = 0;

// função do handler
void trata_irq0(int sig) {
    irq0_pendente = 1;    // só marca uma flag
}

// --> PCB e Fila de prontos
typedef struct{
    pid_t pid; // quem é no sistema
    int num_app; // número do app (id): 1, 2, 3..
    int pc; // contexto salvo, quantas iterações já executou
}PCB;

PCB pcbs[N]; 

int fila_prontos[N];
int inicio = 0;
int tamanho = 0;

void enfileira_prontos(int id){
    fila_prontos[(inicio + tamanho) % N] = id; //primeira posição livre depois do último elemento
    tamanho++;
}

int desienfileira_prontos(){
    if (tamanho == 0){
        return -1;
    }
    int id = fila_prontos[inicio];
    inicio = (inicio + 1) % N;
    tamanho--;

    return id;
}

void imprime_estado(int rodando){
    if (rodando >= 0){
        printf("A%d rodando", pcbs[rodando].num_app);
    }
    else{
        printf("Nenhum processo rodando");
    }

    printf(" | Prontos: ");
    for (int i= 0; i < tamanho; i++){
        int id = fila_prontos[(inicio + i) % N];
        printf("A%d ", pcbs[id].num_app);
    }

    printf(" | PCs: ");
    for(int i = 0; i < N; i++){
        printf("A%d = %d ", pcbs[i].num_app, pcbs[i].pc);
    }

    printf("\n");
}

// lê tudo que os apps escreveram no pipe e atualiza os PCBs
void atualiza_pcs(int fd_leitura){
    MsgPC m;
    while (read(fd_leitura, &m, sizeof(m)) == sizeof(m)){
        pcbs[m.num_app - 1].pc = m.pc;
    }
}

int main(){

    // -> registra o handler
    sa.sa_handler = trata_irq0; // quem será chamada
    sigemptyset(&sa.sa_mask); // não bloqueia outros sinais durante o handler
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL); // liga SIGUSR1 (IRQ0) a essa função 

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
            close(canal == 0); // app não lê, só escreve

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
    }

    close(canal[1]); // o kernel só lê, os app têm sua propria cópia

    // espera todos os processos pararem (raise(SIGSTOP) no app)
    for (int i = 0; i < N; i++){
        waitpid(pcbs[i].pid, NULL, WUNTRACED);
        // WUNTRANCED faz o waitpid retornar quando o filho para, não só quando termina
    }

    // coloca todos na fila de prontos
    for (int i = 0; i < N; i++){
        enfileira_prontos(i);
    }
    
    // começa a rodar os processos
    int rodando = desienfileira_prontos();
    kill(pcbs[rodando].pid, SIGCONT);
    imprime_estado(rodando);

    // fork do InterController  
    pid_t pid_ic = fork();

    if (pid_ic == 0){
        execl("./interController", "ic", NULL);
        
        perror("execl");
        _exit(1);
    }

    // --> laço principal do kernel
    
    // pai faz o papel do Kernel, rodício manual (Round Robin)
    int irqs = 0;
    while (irqs < NUM_IRQS){
        pause(); // retorna qualquer sinal de handler
        
        if(irq0_pendente){
            irq0_pendente = 0; // zera a flag
            irqs++;

            // --> Troca de Contexto

            // para quem está rodando e coloca no final da fila
            if (rodando >= 0){
                kill(pcbs[rodando].pid, SIGSTOP);
                
                waitpid(pcbs[rodando].pid, NULL, WUNTRACED); // esperaparar
                atualiza_pcs(canal[0]); // salva o contexto
                
                enfileira_prontos(rodando);
            }

            // tirar o próximo da fila
            rodando = desienfileira_prontos();

            // libera o novo processo
            if(rodando >= 0){
                kill(pcbs[rodando].pid, SIGCONT);
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
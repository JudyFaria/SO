#include <stdio.h>
#include <sys/types.h>

#define N 3

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

int main(){

    for(int i = 0; i < N; i++){
        pcbs[i].pid = 0;
        pcbs[i].num_app = i + 1;
        pcbs[i].pc = 0;

        enfileira_prontos(i);
    }

    imprime_estado(-1);

    for(int i = 0; i < 7; i++){
        int rodando = desienfileira_prontos();
        imprime_estado(rodando);
        
        if (rodando >= 0){
            pcbs[rodando].pc++; //simula o app executando uma iteração
        }
       
        enfileira_prontos(rodando); // volta para o fim da fila
    }

    return 0;
}
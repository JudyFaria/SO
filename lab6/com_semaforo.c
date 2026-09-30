// Lab Semaforos - Parte 2: COM semaforos (corrige a condicao de corrida)
//
// Mesmo problema da parte 1, mas cada posicao do vetor tem o seu proprio
// semaforo binario (mutex). Assim:
//   - duas tarefas NUNCA alteram a mesma posicao ao mesmo tempo
//     (exclusao mutua na secao critica "le - soma - escreve");
//   - tarefas diferentes continuam executando concorrentemente em
//     posicoes diferentes (nao ha um "cadeado" unico que serialize tudo).
//
// Compilar: gcc -Wall -O2 -pthread com_semaforo.c -o com_semaforo
// Executar: ./com_semaforo [num_tarefas]     (padrao: 100)

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#define TAM_VETOR 10000
#define VALOR_INICIAL 10
#define NUM_TAREFAS_PADRAO 100
#define MAX_LISTAGEM 50 // maximo de faixas com erro impressas na tela
#define TITULO "COM SEMAFORO"

volatile int vetor[TAM_VETOR];
sem_t sem_pos[TAM_VETOR];  // um semaforo por posicao do vetor

pthread_barrier_t largada;

void* tarefa(void* arg) {
    int k = *((int*)arg);
    int delta = (k % 2 == 0) ? k : -k;

    pthread_barrier_wait(&largada);

    for (int i = 0; i < TAM_VETOR; i++) {
        sem_wait(&sem_pos[i]);            // Down: entra na secao critica
        vetor[i] = vetor[i] + delta;
        sem_post(&sem_pos[i]);            // Up: sai da secao critica
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    int n = (argc > 1) ? atoi(argv[1]) : NUM_TAREFAS_PADRAO;
    if (n <= 0) {
        fprintf(stderr, "uso: %s [num_tarefas > 0]\n", argv[0]);
        return 1;
    }

    pthread_t* threads = malloc(n * sizeof(pthread_t));
    int* ids = malloc(n * sizeof(int));

    for (int i = 0; i < TAM_VETOR; i++) {
        vetor[i] = VALOR_INICIAL;
        // pshared = 0: compartilhado entre threads do mesmo processo
        // valor inicial = 1: semaforo binario (livre)
        if (sem_init(&sem_pos[i], 0, 1) != 0) {
            perror("sem_init");
            return 1;
        }
    }

    int esperado = VALOR_INICIAL;
    for (int k = 1; k <= n; k++) esperado += (k % 2 == 0) ? k : -k;

    pthread_barrier_init(&largada, NULL, n);

    for (int i = 0; i < n; i++) {
        ids[i] = i + 1;
        if (pthread_create(&threads[i], NULL, tarefa, &ids[i]) != 0) {
            perror("pthread_create");
            return 1;
        }
    }
    for (int i = 0; i < n; i++) pthread_join(threads[i], NULL);

    pthread_barrier_destroy(&largada);
    for (int i = 0; i < TAM_VETOR; i++) sem_destroy(&sem_pos[i]);

    // Comparacao dos elementos do vetor. Posicoes consecutivas com o mesmo
    // valor errado sao agrupadas em faixas [inicio..fim] para facilitar a leitura.
    int erros = 0, faixas = 0, min = vetor[0], max = vetor[0];
    printf("%s | tarefas: %d | valor esperado em cada posicao: %d\n", TITULO, n, esperado);
    for (int i = 0; i < TAM_VETOR; i++) {
        if (vetor[i] < min) min = vetor[i];
        if (vetor[i] > max) max = vetor[i];
        if (vetor[i] == esperado) continue;

        int ini = i;
        while (i + 1 < TAM_VETOR && vetor[i + 1] == vetor[ini]) i++;
        erros += i - ini + 1;
        if (faixas < MAX_LISTAGEM)
            printf("  posicoes %5d..%5d (%5d pos.): valor %6d (diferenca %+d)\n",
                   ini, i, i - ini + 1, vetor[ini], vetor[ini] - esperado);
        faixas++;
    }
    if (faixas > MAX_LISTAGEM)
        printf("  ... (mais %d faixas com erro omitidas)\n", faixas - MAX_LISTAGEM);

    printf("Posicoes com erro: %d de %d | menor valor: %d | maior valor: %d\n",
           erros, TAM_VETOR, min, max);
    printf("Resultado: %s\n", erros ? "HOUVE ERRO DE CONCORRENCIA" : "nenhum erro detectado");

    free(threads);
    free(ids);
    return 0;
}

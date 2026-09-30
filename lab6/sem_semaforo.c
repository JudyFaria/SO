// Lab Semaforos - Parte 1: SEM semaforos (exibe a condicao de corrida)
//
// Vetor global de 10.000 posicoes inicializado com 10.
// N tarefas (threads) concorrentes; a tarefa k faz, em TODAS as posicoes:
//     k impar -> subtrai k      k par -> soma k
// Valor esperado ao final: 10 + soma_{k=1..N} (-1)^k * k
// (para N = 100: 10 + 50 = 60)
//
// Compilar: gcc -Wall -pthread sem_semaforo.c -o sem_semaforo
// Executar: ./sem_semaforo [num_tarefas]     (padrao: 100)

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define TAM_VETOR 10000
#define VALOR_INICIAL 10
#define NUM_TAREFAS_PADRAO 100
#define MAX_LISTAGEM 50 // maximo de faixas com erro impressas na tela
#define TITULO "SEM SEMAFORO"

// volatile obriga o compilador a fazer, a cada iteracao, a sequencia
// LE da memoria -> SOMA no registrador -> ESCREVE na memoria,
// que e exatamente a secao critica que pode ser intercalada entre threads.
volatile int vetor[TAM_VETOR];

pthread_barrier_t largada; // faz todas as tarefas comecarem juntas

void* tarefa(void* arg) {
    int k = *((int*)arg);                 // numero da tarefa (1..N)
    int delta = (k % 2 == 0) ? k : -k;    // par soma, impar subtrai

    pthread_barrier_wait(&largada);

    for (int i = 0; i < TAM_VETOR; i++) {
        vetor[i] = vetor[i] + delta;      // SECAO CRITICA sem protecao
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

    for (int i = 0; i < TAM_VETOR; i++) vetor[i] = VALOR_INICIAL;

    // valor final esperado se nao houver erro de concorrencia
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

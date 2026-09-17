#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define TAM_VETOR 10000
#define NUM_THREADS 8

int vetor[TAM_VETOR];

// declaração global das barreiras
pthread_barrier_t barreira_inicio;
pthread_barrier_t barreira_fim;

// função que cada thread irá executar
void* trabalhador_thread(void* arg) {
    int id = *((int*)arg);
    int inicio = id * (TAM_VETOR / NUM_THREADS);
    int fim = inicio + (TAM_VETOR / NUM_THREADS);

    // Se for a última thread (ID 31, no caso de 32 threads)
    // Forçamos ela a ir até o final absoluto do vetor.
    // divisão de 32 / 10000
    if (id == NUM_THREADS - 1) {
        fim = TAM_VETOR;
    }

    // 1. Thread chega e fica presa aguardando o "tiro de largada"
    pthread_barrier_wait(&barreira_inicio);

    // 2. Apenas a execução matemática conta no relógio
    for (int i = inicio; i < fim; i++) {
        vetor[i] = (vetor[i] * 2) + 2;
    }
    
    // 3. Thread termina a conta e espera as outras para parar o relógio
    pthread_barrier_wait(&barreira_fim);
        
    pthread_exit(NULL);
}

int main() {
    pthread_t threads[NUM_THREADS];
    int ids[NUM_THREADS];
    struct timespec start, end;

    // Inicialização do vetor
    for (int i = 0; i < TAM_VETOR; i++) vetor[i] = 4;

    // Inicialização das barreiras (Trabalhadoras + 1 thread principal)
    pthread_barrier_init(&barreira_inicio, NULL, NUM_THREADS + 1);
    pthread_barrier_init(&barreira_fim, NULL, NUM_THREADS + 1);

    // Cria as threads (isso fica FORA do relógio)
    for (int i = 0; i < NUM_THREADS; i++) {
        ids[i] = i; 
        pthread_create(&threads[i], NULL, trabalhador_thread, (void*)&ids[i]);
    }

    // Liga o cronômetro EXATAMENTE antes de liberar as threads
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Libera as threads (o main entra na barreira e bate a contagem)
    pthread_barrier_wait(&barreira_inicio);

    // Aguarda todas terminarem a conta na segunda barreira
    pthread_barrier_wait(&barreira_fim);

    // Para o relógio EXATAMENTE após o fim da matemática
    clock_gettime(CLOCK_MONOTONIC, &end);

    // Processo principal aguarda o término oficial (limpeza de memória)
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Destruição das barreiras para liberar recursos
    pthread_barrier_destroy(&barreira_inicio);
    pthread_barrier_destroy(&barreira_fim);

    // Verificação das alterações no vetor 
    int sucesso = 1;
    for (int i = 0; i < TAM_VETOR; i++) {
        if (vetor[i] != 10) { 
            sucesso = 0;
            break;
        }
    }

    double tempo_gasto = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Threads - Verificacao: %s | Tempo: %f s\n", sucesso ? "SUCESSO" : "FALHA", tempo_gasto);

    return 0;
}
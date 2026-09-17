#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/mman.h> // Biblioteca necessária para o mmap

#define TAM_VETOR 10000
#define NUM_PROCESSOS 8

int main() {
    struct timespec start, end;
    int pipe_sync[2];
    int pipe_tempo[2];
    double tempo_execucao_total = 0.0;

    // Alocando a memória COMPARTILHADA entre os processos
    int *vetor = mmap(NULL, TAM_VETOR * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (vetor == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    if (pipe(pipe_sync) == -1 || pipe(pipe_tempo) == -1) {
        perror("pipe");
        return 1;
    }

    // Inicialização do vetor (feita pelo pai na memória compartilhada)
    for (int i = 0; i < TAM_VETOR; i++) vetor[i] = 4;

    for (int i = 0; i < NUM_PROCESSOS; i++) {
        pid_t pid = fork();

        if (pid == 0) { // PROCESSO FILHO
            close(pipe_sync[1]);
            close(pipe_tempo[0]);

            char sinal;
            read(pipe_sync[0], &sinal, 1);
            close(pipe_sync[0]);

            struct timespec inicio_exec, fim_exec;
            clock_gettime(CLOCK_MONOTONIC, &inicio_exec);

            int inicio = i * (TAM_VETOR / NUM_PROCESSOS);
            int fim = inicio + (TAM_VETOR / NUM_PROCESSOS);

            // Filhos agora alteram a memória compartilhada
            for (int j = inicio; j < fim; j++) {
                vetor[j] = (vetor[j] * 2) + 2;
            }

            clock_gettime(CLOCK_MONOTONIC, &fim_exec);

            double tempo_exec = (fim_exec.tv_sec - inicio_exec.tv_sec) + (fim_exec.tv_nsec - inicio_exec.tv_nsec) / 1e9;
            printf("Filho %d executou em %.9f s\n", i, tempo_exec);
            
            write(pipe_tempo[1], &tempo_exec, sizeof(double));
            close(pipe_tempo[1]);
            exit(0);
        } else if (pid < 0) {
            perror("fork");
            return 1;
        }
    }

    // PROCESSO PAI
    close(pipe_sync[0]);
    close(pipe_tempo[1]);

    clock_gettime(CLOCK_MONOTONIC, &start);

    char sinal_inicio[NUM_PROCESSOS] = {0};
    write(pipe_sync[1], sinal_inicio, NUM_PROCESSOS);
    close(pipe_sync[1]);

    for (int i = 0; i < NUM_PROCESSOS; i++) {
        wait(NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double tempo_recebido;
    for (int i = 0; i < NUM_PROCESSOS; i++) {
        read(pipe_tempo[0], &tempo_recebido, sizeof(double));
        tempo_execucao_total += tempo_recebido;
    }
    close(pipe_tempo[0]);

    // Nova verificação: O pai agora deve ver o valor 10!
    int sucesso = 1;
    for (int i = 0; i < TAM_VETOR; i++) {
        if (vetor[i] != 10) {
            sucesso = 0;
            break;
        }
    }

    printf("\nTempo SOMADO dos filhos: %.9f s\n", tempo_execucao_total);
    double tempo_gasto = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Tempo GERAL do pai: %.9f s\n", tempo_gasto);
    printf("Processos com mmap - Verificacao: %s\n", sucesso ? "SUCESSO (Memoria Compartilhada)" : "FALHA");

    // Libera a memória compartilhada adequadamente
    munmap(vetor, TAM_VETOR * sizeof(int));

    return 0;
}
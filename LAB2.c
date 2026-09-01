#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/stat.h>

int main() {
    int id, *n, pid, status;

    // aloca memória compartilhada
    id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); // associa a memória compartilhada ao processo

    if (id == -1){
        printf("Erro shtget!");
        exit(1);
    }

    n = (int*) shmat(id, 0, 0); // comparar o retorno com -1
    
    if ( n == (int*)-1){
        shmctl(id, IPC_RMID, 0);
        exit(1);
    }

    *n = 0;
    
    // cria o processo filho
    pid_t pid_filho = fork();
    
    if (pid_filho < 0) {
        
        // trata erro se o fork falhar
        fprintf(stderr, "Erro ao criar o processo filho.\n");
        
        return 1;
    } 
    else if (pid_filho > 0) {
       
        // cod processo pai
        for (int i = 0; i < 1000; i++) {
            *n += 1;
            printf("processo pai, pid=%d, n=%d\n", getpid(), *n);
        }
        
        // espera o filho terminar 
        wait(NULL); 
    } 
    else {
        // cod processo filho 
        
        // filho cria o processo neto
        pid_t pid_neto = fork();
        
        if (pid_neto < 0) {
            fprintf(stderr, "Erro ao criar o processo neto.\n");
            return 1;
        } 
        else if (pid_neto > 0) {
            
            // loop processo filho
            for (int i = 0; i < 1000; i++) {
                *n += 2;
                printf("processo filho, pid=%d, n=%d\n", getpid(), *n);
            }
            
            // espera o neto terminar
            wait(NULL);
        } 
        else {
            
            // cod processo neto 
            for (int i = 0; i < 1000; i++) {
                *n += 3;
                printf("processo neto, pid=%d, n=%d\n", getpid(), *n);
            }
            
            // neto termina sua execução
            exit(0);
        }
    }

    // libera a memoria compartilhada do processo
    shmdt(n);
    
    // libera a memoria compartilhada
    shmctl(id, IPC_RMID, 0);
    
    return 0;
}
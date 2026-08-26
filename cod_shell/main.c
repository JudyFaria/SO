#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main(){

    char input[200];
    char *parameters[20];
    int status;


    while(1){
        printf("#_meu_shell > ");
        scanf(" %[^\n]", input);

        int i = 0;
        parameters[i] = strtok(input, " ");
        while( parameters[i] != NULL){
            i++;
            parameters[i] = strtok(NULL, " ");
        }

        if (fork() != 0){
            // parent code
            waitpid(-1, &status, 0);
        }
        else{
            //child code
            execvp(parameters[0], parameters);
            
            printf("erro!");
            exit(1);
        }


    }

    return 0;
}
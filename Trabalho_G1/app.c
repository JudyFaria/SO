#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <signal.h>

int main(int argc, char *argv[]){

    int id = atoi(argv[1]);

    raise(SIGSTOP); // raise envia um sinal ao próprio processo
    // agora o app só entra no loop se alguém chamar SIGCONT(retomar)

    for (int i = 0; ; i++){
        printf("A%d PC=%d\n",id, i);
        
        sleep(1); // segue rodando o app parado
    }

    return 0;
}

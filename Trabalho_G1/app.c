#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]){

    int id = atoi(argv[1]);

    for (int i = 0; ; i++){
        printf("A%d PC=%d\n",id, i);
        sleep(1);
    }

    return 0;
}

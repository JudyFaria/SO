CC = gcc
CFLAGS = -Wall -O2 -pthread

all: sem_semaforo com_semaforo

sem_semaforo: sem_semaforo.c
	$(CC) $(CFLAGS) $< -o $@

com_semaforo: com_semaforo.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f sem_semaforo com_semaforo

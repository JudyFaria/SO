# Lab Semáforos – INF1316 – Sistemas Operacionais

## Arquivos

| Arquivo | Conteúdo |
|---|---|
| `sem_semaforo.c` | Parte 1 – 100 tarefas alteram o vetor **sem** sincronização |
| `com_semaforo.c` | Parte 2 – mesmas tarefas, com **um semáforo por posição** do vetor |
| `Makefile` | Compila os dois programas (`make`) |
| `saidas/` | Saídas reais das execuções citadas neste relatório |

Compilação e execução:

```sh
gcc -Wall -O2 -pthread sem_semaforo.c -o sem_semaforo
gcc -Wall -O2 -pthread com_semaforo.c -o com_semaforo
./sem_semaforo          # 100 tarefas (padrão)
./sem_semaforo 1000     # número de tarefas pode ser passado como argumento
./com_semaforo
./com_semaforo 1000
```

## 1. Descrição do problema

- Vetor global `vetor[10000]`, todas as posições inicializadas com **10**.
- São criadas **N = 100** threads (`pthread_create`). A tarefa *k* percorre todo o vetor
  somando *k* se *k* é par e subtraindo *k* se *k* é ímpar
  (tarefa 1: −1, tarefa 2: +2, tarefa 3: −3, …, tarefa 100: +100).
- Todas as tarefas ficam presas numa `pthread_barrier` e são liberadas juntas, para
  maximizar a sobreposição entre elas.

**Valor esperado.** Como soma e subtração são comutativas, a ordem em que as tarefas
executam não importa. Agrupando em pares (−1+2) + (−3+4) + … + (−99+100) = 50 × 1 = 50,
portanto **toda posição deveria terminar com 10 + 50 = 60**.
Em geral, com N tarefas (N par): 10 + N/2 (ex.: 1000 tarefas → 510).

Ao final, o programa compara cada elemento com o valor esperado e lista as posições
erradas, agrupando posições consecutivas com o mesmo valor em faixas `inicio..fim`.

## 2. Parte 1 – sem semáforo

### Por que pode dar erro

A linha `vetor[i] = vetor[i] + delta;` **não é atômica**. Em linguagem de máquina ela vira
três passos:

1. **ler** `vetor[i]` da memória para um registrador;
2. **somar** `delta` no registrador;
3. **escrever** o registrador de volta em `vetor[i]`.

Se duas tarefas executam esses passos intercalados na mesma posição, uma
sobrescreve o resultado da outra (*lost update*). Exemplo com a posição valendo 10:

| Tempo | Tarefa 3 (−3) | Tarefa 4 (+4) | `vetor[i]` |
|---|---|---|---|
| t1 | lê 10 | | 10 |
| t2 | | lê 10 | 10 |
| t3 | calcula 7, escreve 7 | | 7 |
| t4 | | calcula 14, escreve 14 | **14** (a subtração de 3 foi perdida) |

O correto seria 11. Essa sequência "ler–modificar–escrever" é a **seção crítica**.
O vetor foi declarado `volatile` para garantir que o compilador (com `-O2`) realmente
faça a leitura e a escrita na memória a cada iteração, como no código-fonte.

### Resultados obtidos

Execução com **100 tarefas** (`saidas/ex_100.txt`):

```
SEM SEMAFORO | tarefas: 100 | valor esperado em cada posicao: 60
  posicoes     8..  727 (  720 pos.): valor    -40 (diferenca -100)
Posicoes com erro: 720 de 10000 | menor valor: -40 | maior valor: 60
Resultado: HOUVE ERRO DE CONCORRENCIA
```

**Posições com problema de concorrência nesta execução: 8 a 727 (720 posições)**, todas
com valor −40 em vez de 60. A diferença de exatamente −100 mostra que a atualização da
**tarefa 100 (+100) foi perdida** nessas posições: ela leu o valor, e outra tarefa
que estava percorrendo o mesmo trecho do vetor ao mesmo tempo escreveu por cima do seu
resultado. Como as tarefas percorrem o vetor na mesma ordem, quando duas "andam juntas"
o erro aparece em **blocos contíguos** de posições, e não em posições isoladas.

O erro é **não determinístico**: cada execução dá um resultado diferente. Em 20 execuções
com 100 tarefas, o erro apareceu em 14; nas demais o vetor terminou correto, porque cada
tarefa percorre as 10.000 posições tão rápido (microssegundos) que muitas vezes uma termina
antes de a próxima começar, sem sobreposição. Outras execuções com 100 tarefas deram, por
exemplo: 4, 23, 161, 205, 320, 454, 1276 posições erradas, com valores entre −40 e 153.

Como pedido no enunciado ("caso não ocorra, aumente o número de tarefas"), o número de
tarefas foi aumentado:

| Tarefas | Execuções com erro (de 20) |
|---|---|
| 100 | 14 |
| 500 | 20 |
| 1000 | 20 |
| 2000 | 20 |

Execução com **1000 tarefas** (`saidas/ex_1000.txt`, esperado 510 em cada posição):

```
SEM SEMAFORO | tarefas: 1000 | valor esperado em cada posicao: 510
  posicoes     8..   58 (   51 pos.): valor    350 (diferenca -160)
  posicoes   648..  999 (  352 pos.): valor   -226 (diferenca -736)
  posicoes  1000.. 1038 (   39 pos.): valor      5 (diferenca -505)
  posicoes  1039.. 1103 (   65 pos.): valor   -226 (diferenca -736)
  posicoes  1104.. 1113 (   10 pos.): valor    -29 (diferenca -539)
  posicoes  1114.. 1124 (   11 pos.): valor      5 (diferenca -505)
  posicoes  1125.. 1127 (    3 pos.): valor   -226 (diferenca -736)
  posicoes  1246.. 1249 (    4 pos.): valor    300 (diferenca -210)
  posicoes  2536.. 2542 (    7 pos.): valor    282 (diferenca -228)
  posicoes  3400.. 3431 (   32 pos.): valor   1265 (diferenca +755)
  posicoes  3432.. 3447 (   16 pos.): valor   1175 (diferenca +665)
  ... (demais faixas: ver saidas/ex_1000.txt)
Posicoes com erro: 2427 de 10000 | menor valor: -347 | maior valor: 1782
Resultado: HOUVE ERRO DE CONCORRENCIA
```

Com mais tarefas há mais sobreposição: **2427 das 10.000 posições** ficaram erradas, e
várias atualizações foram perdidas na mesma posição (diferenças negativas = somas perdidas;
diferenças positivas = subtrações perdidas).

**Conclusão da parte 1:** houve erro de concorrência — o resultado final depende da
ordem de escalonamento das threads (*condição de corrida*).

## 3. Parte 2 – com semáforo

### Solução

Foi usada a biblioteca `semaphore.h` com semáforos não nomeados (`sem_init`), pois todas
as tarefas são threads do mesmo processo:

```c
sem_t sem_pos[TAM_VETOR];                    // um semáforo por posição
...
sem_init(&sem_pos[i], 0, 1);                 // pshared = 0 (threads), valor inicial 1
...
sem_wait(&sem_pos[i]);                       // Down: entra na seção crítica
vetor[i] = vetor[i] + delta;
sem_post(&sem_pos[i]);                       // Up: sai da seção crítica
...
sem_destroy(&sem_pos[i]);                    // libera o semáforo ao final
```

Cada semáforo começa com valor 1 (binário / *mutex*). A primeira tarefa que faz
`sem_wait` numa posição o leva a 0 e entra; qualquer outra tarefa que tente alterar
a **mesma posição** fica bloqueada até o `sem_post`. Assim a sequência
ler–somar–escrever de uma posição nunca é intercalada.

**Mantendo a concorrência:** foi escolhido **um semáforo por posição**, e não um único
semáforo para o vetor inteiro. Com um único semáforo em volta do laço, as tarefas
executariam uma de cada vez (serializadas), eliminando a concorrência. Com semáforos por
posição, enquanto a tarefa 3 altera a posição 500, a tarefa 4 pode alterar a posição 499
ao mesmo tempo — as threads continuam rodando em paralelo e só se bloqueiam quando
disputam exatamente a mesma posição.

> Alternativa com semáforo nomeado: `sem_t *s = sem_open("/nome", O_CREAT, 0666, 1);`,
> depois `sem_wait(s)` / `sem_post(s)`, e ao final `sem_close(s)` e `sem_unlink("/nome")`.
> Para 10.000 semáforos seria preciso criar 10.000 nomes; como aqui só há threads de um
> mesmo processo, `sem_init` é mais simples e adequado. (Observação: o valor inicial deve
> ser **1** para exclusão mútua; com 0 todas as tarefas ficariam bloqueadas.)

### Resultados obtidos

```
COM SEMAFORO | tarefas: 100 | valor esperado em cada posicao: 60
Posicoes com erro: 0 de 10000 | menor valor: 60 | maior valor: 60
Resultado: nenhum erro detectado
```

```
COM SEMAFORO | tarefas: 1000 | valor esperado em cada posicao: 510
Posicoes com erro: 0 de 10000 | menor valor: 510 | maior valor: 510
Resultado: nenhum erro detectado
```

Foram feitas 20 execuções com 100, 1000 e 2000 tarefas: **nenhuma posição errada em
nenhuma execução**. Todas as 10.000 posições terminam com o valor esperado.

### Custo da sincronização

| Tarefas | Sem semáforo | Com semáforo |
|---|---|---|
| 100 | ≈ 0,008 s | ≈ 0,021 s |
| 1000 | ≈ 0,052 s | ≈ 0,170 s |

A versão com semáforo é cerca de 3× mais lenta, pelo custo das chamadas
`sem_wait`/`sem_post` (duas por posição, por tarefa) e pelos bloqueios quando duas
tarefas disputam a mesma posição. É o preço da corretude.

## 4. Conclusão

- **Sem semáforo**, o acesso concorrente ao vetor global gera condição de corrida: atualizações
  são perdidas em blocos de posições (ex.: posições 8–727 com 100 tarefas; 2427 posições com
  1000 tarefas), e o resultado muda a cada execução. Com 100 tarefas o erro nem sempre aparece;
  aumentando para 500 ou mais, ele aparece em todas as execuções.
- **Com um semáforo por posição**, a seção crítica fica protegida por exclusão mútua, todas as
  posições terminam com o valor correto (60 para 100 tarefas) e as tarefas continuam executando
  concorrentemente.

*Ambiente de teste: Linux, 4 núcleos, gcc com `-O2 -pthread`. Os números exatos variam de
máquina para máquina e de execução para execução.*

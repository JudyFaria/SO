
Roda testes · SH
#!/bin/bash
# Roda os cenários do Trabalho 1, guarda cada saída em saidas/ e confere os requisitos.
#
# Uso:  ./roda_testes.sh                      todos os cenários (kernel em t1.c)
#       SRC=t1.c ./roda_testes.sh             usa outro arquivo para o kernel
#       ./roda_testes.sh teste3               só os cenários cujo nome começa com "teste3"
#       ./roda_testes.sh --confere ARQ N      só confere um arquivo de saída já gerado (N = nº de apps)
 
cd "$(dirname "$0")" || exit 1
mkdir -p saidas
 
SRC=${SRC:-t1.c}   # arquivo-fonte do kernel (padrão: t1.c)
 
MAX=$(grep -oE '#define MAX [0-9]+' common.h | grep -oE '[0-9]+')
 
# confere <arquivo> <n>  ->  imprime as verificações; devolve 0 se tudo OK
confere() {
    local arq=$1 n=$2 ok=1
 
    # 1) cada app executa exatamente MAX iterações (requisito do enunciado)
    for i in $(seq 1 "$n"); do
        c=$(grep -c "^A$i PC=" "$arq")
        if [ "$c" -eq "$MAX" ]; then
            printf "    A%d executou %2d iteracoes   OK\n" "$i" "$c"
        else
            faltam=""
            for p in $(seq 0 $((MAX-1))); do
                grep -q "^A$i PC=$p\$" "$arq" || faltam="$faltam $p"
            done
            printf "    A%d executou %2d iteracoes   ERRO (esperado %d) - PCs que NAO aparecem:%s\n" "$i" "$c" "$MAX" "$faltam"
            ok=0
        fi
    done
 
    # 2) o kernel terminou sozinho
    if grep -q "Todos os processos terminaram" "$arq"; then
        echo "    aviso de fim impresso            OK"
    else
        echo "    aviso de fim NAO apareceu        ERRO"
        ok=0
    fi
 
    # 3) D1 é serial: os fins de I/O saem espaçados de pelo menos 3 ticks
    tempos=$(grep "concluiu o I/O" "$arq" | sed -E 's/^\[t= *([0-9]+)s\].*/\1/')
    if [ -n "$tempos" ]; then
        echo "    IRQ1 (fim de I/O) em t = $(echo $tempos | tr '\n' ' ')"
        menor=$(echo "$tempos" | awk 'NR>1 { d=$1-p; if (m=="" || d<m) m=d } { p=$1 } END { print (m=="" ? "-" : m) }')
        if [ "$menor" = "-" ] || [ "$menor" -ge 3 ]; then
            echo "    menor intervalo entre I/Os: $menor  OK (esperado >= 3)"
        else
            echo "    menor intervalo entre I/Os: $menor  ERRO (esperado >= 3)"
            ok=0
        fi
    fi
 
    # 4) a linha do tempo é monotônica (o t nunca anda para trás)
    volta=$(grep -E "^\[t=" "$arq" | sed -E 's/^\[t= *([0-9]+)s\].*/\1/' | awk 'NR>1 && $1<p { c++ } { p=$1 } END { print c+0 }')
    if [ "$volta" -eq 0 ]; then
        echo "    tempo nunca anda para tras       OK"
    else
        echo "    tempo andou para tras $volta vezes   ERRO"
        ok=0
    fi
 
    # 5) cada processo está em um lugar só (rodando + prontos + bloqueados + terminados = n)
    ruins=$(awk -v n="$n" '
        /Prontos:/ {
            l = $0
            total = (l ~ /[|] A[0-9]+ rodando/) ? 1 : 0
            i = index(l, "Prontos:")
            j = index(l, "| Bloqueados:")
            k = index(l, "| PCs:")
            p = substr(l, i, j - i)
            b = substr(l, j, k - j)
            f = substr(l, k)
            total += gsub(/A[0-9]+/, "&", p) + gsub(/A[0-9]+/, "&", b) + gsub(/[(]fim[)]/, "&", f)
            if (total != n) { ruins++; if (ruins <= 3) print "        linha " NR ": " $0 }
        }
        END { print "N=" ruins + 0 }' "$arq")
    nruins=$(echo "$ruins" | grep '^N=' | cut -d= -f2)
    if [ "$nruins" -eq 0 ]; then
        echo "    invariante rodando+prontos+bloqueados+fim = $n em todas as linhas   OK"
    else
        echo "    invariante violado em $nruins linha(s)   ERRO. As primeiras:"
        echo "$ruins" | grep -v '^N='
        ok=0
    fi
 
    # 6) linhas estragadas: um app escrevendo no meio de uma linha do kernel (ou vice-versa)
    sus=$(grep -nE '^\[t=.*A[0-9] PC=|^A[0-9] PC=[0-9]+.' "$arq")
    if [ -z "$sus" ]; then
        echo "    nenhuma linha embaralhada        OK"
    else
        echo "    LINHAS EMBARALHADAS (saida de dois processos misturada):"
        echo "$sus" | head -5 | sed 's/^/        /'
        ok=0
    fi
 
    [ "$ok" -eq 1 ]
}
 
if [ "$1" = "--confere" ]; then
    echo ">>> conferindo $2 (N=$3)"
    if confere "$2" "$3"; then echo "    => PASSOU"; else echo "    => FALHOU"; fi
    exit 0
fi
 
gcc -Wall -Wextra -o app app.c                         || exit 1
gcc -Wall -Wextra -o interController intercontroller.c || exit 1
if [ ! -f "$SRC" ]; then echo "arquivo do kernel '$SRC' nao encontrado (use SRC=seu_arquivo.c)"; exit 1; fi
# Testa a versão entregue: o kernel é infinito, então cada cenário espera o aviso de fim
# ("Todos os processos terminaram") e encerra o kernel com Ctrl+C (SIGINT).
gcc -Wall -Wextra -DN=3 -o t1_3 "$SRC"                 || exit 1
gcc -Wall -Wextra -DN=6 -o t1_6 "$SRC"                 || exit 1
 
limpa() {
    pkill -x app 2>/dev/null
    pkill -x interController 2>/dev/null
}
 
FALHAS=0
FILTRO=$1
 
# roda <nome> <N> <tabela de syscalls: um token "<PC><R|W>" por app, "-" = nenhum>
roda() {
    local nome=$1 n=$2
    shift 2
 
    if [ -n "$FILTRO" ] && [[ "$nome" != "$FILTRO"* ]]; then return; fi
 
    local arq="saidas/$nome.txt"
    local ok=1
 
    limpa
    echo ">>> $nome  (N=$n, syscalls: $*)"
 
    # ">>" abre o arquivo em modo O_APPEND: cada write vai para o fim do arquivo de forma
    # atômica, mesmo com vários processos escrevendo ao mesmo tempo.
    : > "$arq"
    setsid ./t1_$n "$@" >> "$arq" 2>&1 < /dev/null &
    local K=$! i
    for i in $(seq 240); do
        grep -q "Todos os processos terminaram" "$arq" && break
        kill -0 $K 2>/dev/null || break
        sleep 1
    done
    sleep 1
    if kill -0 $K 2>/dev/null; then
        kill -INT -$K 2>/dev/null            # Ctrl+C: o terminal manda SIGINT ao grupo inteiro
        for i in $(seq 10); do kill -0 $K 2>/dev/null || break; sleep 1; done
    fi
    if kill -0 $K 2>/dev/null; then
        echo "    Ctrl+C nao encerrou o kernel     ERRO"; ok=0
        kill -KILL -$K 2>/dev/null
    else
        echo "    Ctrl+C encerra o kernel          OK"
    fi
 
    confere "$arq" "$n" || ok=0
 
    # nenhum processo pode sobrar
    sobras=$(( $(pgrep -xc app) + $(pgrep -xc interController) ))
    if [ "$sobras" -eq 0 ]; then
        echo "    sem processos sobrando           OK"
    else
        echo "    sobraram $sobras processos        ERRO"
        ok=0
    fi
 
    if [ "$ok" -eq 1 ]; then echo "    => PASSOU"; else echo "    => FALHOU"; FALHAS=$((FALHAS+1)); fi
    echo
    limpa
}
 
# teste7: a versão entregue (kernel infinito) e o encerramento limpo
roda_encerramento() {
    local nome=teste7_encerramento
    if [ -n "$FILTRO" ] && [[ "$nome" != "$FILTRO"* ]]; then return; fi
    local ok=1 arq="saidas/$nome.txt" K i
 
    limpa
    echo ">>> $nome  (kernel infinito: fica vivo e encerra no Ctrl+C e no SIGTERM)"
 
    # (a) todos os apps terminam, o kernel NÃO sai sozinho e fica em silêncio
    setsid ./t1_3 > "$arq" 2>&1 < /dev/null &
    K=$!
    for i in $(seq 120); do grep -q "Todos os processos terminaram" "$arq" && break; sleep 1; done
    if grep -q "Todos os processos terminaram" "$arq"; then
        echo "    todos os apps terminaram         OK"
    else
        echo "    apps nao terminaram em 120s      ERRO"; ok=0
    fi
    sleep 5
    l1=$(wc -l < "$arq"); sleep 5; l2=$(wc -l < "$arq")
    if kill -0 $K 2>/dev/null; then echo "    kernel segue vivo (infinito)     OK"; else echo "    kernel saiu sozinho              ERRO"; ok=0; fi
    if [ "$l1" -eq "$l2" ]; then echo "    ocioso, sem imprimir nada novo   OK"; else echo "    imprimiu $((l2-l1)) linhas ocioso   ERRO"; ok=0; fi
    n=$(grep -c "Todos os processos terminaram" "$arq")
    if [ "$n" -eq 1 ]; then echo "    aviso de fim impresso 1 vez      OK"; else echo "    aviso impresso $n vezes          ERRO"; ok=0; fi
 
    # Ctrl+C: o terminal manda SIGINT para o grupo inteiro (kernel, IC e apps)
    kill -INT -$K 2>/dev/null
    for i in $(seq 10); do kill -0 $K 2>/dev/null || break; sleep 1; done
    if kill -0 $K 2>/dev/null; then echo "    SIGINT: kernel nao encerrou      ERRO"; ok=0; else echo "    SIGINT encerra o kernel          OK"; fi
    sobras=$(( $(pgrep -xc app) + $(pgrep -xc interController) + $(pgrep -xc t1_3) ))
    if [ "$sobras" -eq 0 ]; then echo "    sem processos sobrando           OK"; else echo "    sobraram $sobras processos        ERRO"; ok=0; fi
    limpa
 
    # (b) SIGTERM só no kernel, com apps PARADOS (SIGSTOP) no meio da execução
    setsid ./t1_3 >> "$arq" 2>&1 < /dev/null &
    K=$!
    sleep 8
    kill -TERM $K 2>/dev/null
    for i in $(seq 10); do kill -0 $K 2>/dev/null || break; sleep 1; done
    if kill -0 $K 2>/dev/null; then echo "    SIGTERM: kernel nao encerrou     ERRO"; ok=0; else echo "    SIGTERM encerra o kernel         OK"; fi
    sobras=$(( $(pgrep -xc app) + $(pgrep -xc interController) + $(pgrep -xc t1_3) ))
    if [ "$sobras" -eq 0 ]; then echo "    sem processos sobrando (apps parados tambem)   OK"; else echo "    sobraram $sobras processos        ERRO"; ok=0; fi
 
    if [ "$ok" -eq 1 ]; then echo "    => PASSOU"; else echo "    => FALHOU"; FALHAS=$((FALHAS+1)); fi
    echo
    limpa
}
 
roda teste1_normal            3 2R 2W 3R
roda teste2_syscall_ultima    3 2R 2W 9R
roda teste3_todos_bloqueados  3 1R 1W 1R
roda teste4_seis_apps         6 2R 2W 3R 3W 4R 4W
roda teste5_sem_syscall       3 - - -
roda teste6_seis_bloqueados   6 1R 1W 1R 1W 1R 1W
roda_encerramento
 
if [ "$FALHAS" -eq 0 ]; then echo "TODOS OS CENARIOS PASSARAM"; else echo "$FALHAS CENARIO(S) FALHARAM"; fi
 


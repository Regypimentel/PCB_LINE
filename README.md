Aqui está o **`README.md`** atualizado com o **Named Pipe / FIFO (`mkfifo`)** — é só copiar tudo e colar no editor do GitHub:

```markdown
# PCB Line — Simulador concorrente de linha de montagem de PCB (SMT)

Projeto da disciplina de Sistemas Operacionais / Programação Concorrente (C, POSIX, Linux).

## Objetivo
Simular uma linha SMT de montagem de placas de circuito impresso com três estações
que trabalham em paralelo, comunicando-se por IPC e sincronizando-se com
primitivas POSIX:


```

[Pick & Place]  --mq_send-->  [Forno de Refluxo]  --fifo-->  [Inspeção AOI]
(processo A)                    (processo B)                 (processo C)
threads: feeders + heads + dispatcher
_______________ memória compartilhada (estatísticas) _______________/
Supervisor (processo pai) + SIGUSR1

```

## Como executar
Requisitos: Linux, gcc, make.

```bash
make            # compila
./pcbline       # execução padrão (10 placas)
make race       # demonstra condição de corrida (mutex desligado)
./pcbline -n 20 -c 8 -f 3 -t 3 -d 25 -s 30

```

| Opção | Significado | Padrão |
| --- | --- | --- |
| -n | nº de placas | 10 |
| -c | componentes por placa | 6 |
| -f | threads alimentadoras (produtores) | 2 |
| -t | threads cabeçotes (consumidores) | 2 |
| -d | % de defeito na inspeção | 15 |
| -s | tempo base (ms) | 50 |
| -r | desliga o mutex do contador local (race condition) | off |

Ctrl+C encerra a linha de forma limpa (SIGINT → SIGTERM nos filhos).

## Mecanismos utilizados

| Conceito | Onde |
| --- | --- |
| `fork`, `wait/waitpid`, `exit` | `main.c`: cria as 3 estações; espera o término |
| `exec` | `main.c`: `execlp("date")` no relatório final |
| Threads POSIX (`pthread_create/join`) | `placement.c`: feeders, heads, dispatcher |
| Produtor–consumidor | `placement.c`: buffer circular de componentes |
| Semáforos POSIX (`sem_init/wait/post`) | `placement.c`: `empty`/`full` do buffer |
| Mutex (`pthread_mutex`) | buffer, contadores, estado da placa |
| Variável de condição (`pthread_cond`) | dispatcher espera "placa completa" |
| Fila de mensagens POSIX (`mq_*`) | Pick&Place → Forno |
| Named Pipe / FIFO (`mkfifo`) | Forno → AOI |
| Memória compartilhada (`shm_open`, `mmap`) | estatísticas globais |
| Mutex *process-shared* | protege a memória compartilhada entre processos |
| Sinais (`sigaction`, `kill`) | SIGUSR1 = alerta de defeito; SIGINT = parada |
| Condição de corrida (opcional) | flag `-r` mostra perda de contagem sem mutex |

## Estrutura

```
src/common.h     tipos e constantes compartilhados
src/main.c       supervisor (IPC, fork, sinais, relatório)
src/placement.c  estação Pick & Place (threads)
src/oven.c       forno de refluxo
src/inspect.c    inspeção AOI
Makefile

```

## Integrantes

* Reginaldo José Pimentel Silva

```

```

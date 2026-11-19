PCB Line — Simulador concorrente de linha de montagem de PCB (SMT)

Projeto Final da disciplina de Sistemas Operacionais / Programação Concorrente (C, POSIX, Linux).

Objetivo

Simular uma linha SMT (Surface Mount Technology) de montagem de placas de circuito impresso (PCB) com três estações que trabalham em paralelo, comunicando-se por IPC e sincronizando-se com primitivas POSIX.

Linhas de montagem reais são sistemas concorrentes por natureza: várias máquinas operam ao mesmo tempo, passam peças umas às outras e compartilham indicadores de produção. O projeto reproduz esse cenário para demonstrar, na prática, processos, threads, sincronização, IPC e condição de corrida.

Arquitetura
 [Pick & Place]  --mq_send-->  [Forno de Refluxo]  --pipe-->  [Inspeção AOI]
  (processo A)                    (processo B)                 (processo C)
  threads: feeders + heads + dispatcher
        \______________ memória compartilhada (estatísticas) ______________/
                    Supervisor (processo pai) + sinais (SIGUSR1/SIGINT)
Componente	Tipo	Função
Supervisor	processo pai	cria IPC, faz fork das estações, mostra progresso, trata sinais e gera o relatório final
Pick & Place	processo + threads	feeders produzem componentes, heads os colocam na placa, dispatcher envia a placa pronta ao forno
Forno de refluxo	processo	recebe placas pela fila de mensagens e simula o perfil térmico
Inspeção AOI	processo	recebe placas pelo pipe, aprova ou reprova e alerta o supervisor com SIGUSR1
Como executar

Requisitos: Linux (ou WSL), gcc e make.

bash
git clone https://github.com/Regypimentel/RTOS-para-PCB.git
cd RTOS-para-PCB

make            # compila
./pcbline       # execução normal (com mutex)
make race       # execução com condição de corrida (sem mutex)
make clean      # remove o binário

Exemplo com parâmetros:

bash
./pcbline -n 20 -c 8 -f 3 -t 3 -d 25 -s 30
Opção	Significado	Padrão
-n	número de placas	10
-c	componentes por placa	6
-f	threads alimentadoras (produtores)	2
-t	threads cabeçotes (consumidores)	2
-d	% de defeito na inspeção	15
-s	tempo base de colocação (ms)	50
-r	desliga o mutex do contador local (demo de race condition)	off

Ctrl+C encerra a linha de forma limpa (SIGINT no supervisor, que envia SIGTERM aos filhos e remove a fila e a memória compartilhada).

Demonstração da condição de corrida

Com -r, as threads heads incrementam um contador sem exclusão mútua. Duas threads leem o mesmo valor antes de qualquer uma gravar, e atualizações se perdem:

[PICK&PLACE] contador local: esperado=30 obtido=16 <-- CONDICAO DE CORRIDA!

Sem -r, o mesmo contador é protegido por pthread_mutex e o resultado é sempre correto:

[PICK&PLACE] contador local: esperado=60 obtido=60 (OK)

O valor obtido varia a cada execução com -r, o que é característico de uma condição de corrida.

Mecanismos utilizados
Conceito	Onde
fork, waitpid, exit	main.c: cria as 3 estações e espera o término
exec	main.c: execlp("date") no relatório final
Threads POSIX (pthread_create/join)	placement.c: feeders, heads e dispatcher
Produtor–consumidor	placement.c: buffer circular de componentes
Semáforos POSIX (sem_init/wait/post)	placement.c: empty e full do buffer
Mutex (pthread_mutex)	buffer, contadores e estado da placa
Variável de condição (pthread_cond)	dispatcher espera a placa ficar completa
Fila de mensagens POSIX (mq_*)	Pick & Place → Forno
Pipe anônimo	Forno → AOI
Memória compartilhada (shm_open, mmap)	estatísticas globais da linha
Mutex process-shared	protege a memória compartilhada entre processos
Sinais (sigaction, kill)	SIGUSR1 = alerta de defeito; SIGINT = parada limpa
Simulação de condição de corrida e solução	flag -r vs. execução normal
Estrutura do projeto
src/common.h     tipos, constantes e protótipos compartilhados
src/main.c       supervisor (IPC, fork, sinais, relatório)
src/placement.c  estação Pick & Place (threads, semáforos, mutex, cond)
src/oven.c       forno de refluxo (mq → pipe)
src/inspect.c    inspeção AOI (pipe → estatísticas, sinal)
Makefile
Exemplo de saída
[SUPERVISOR] iniciando linha: 10 placas x 6 componentes | 2 feeders, 2 heads
[PICK&PLACE] placa #1 montada -> enviada ao forno (mq)
[FORNO     ] placa #1: pre-aquecimento 150C > imersao 180C > refluxo 245C > resfriamento
[AOI       ] placa #1 aprovada
...
========== RELATORIO FINAL ==========
Placas produzidas : 10
Componentes       : 60
Aprovadas         : 8
Reprovadas        : 2
Alertas SIGUSR1   : 2
Rendimento (yield): 80.0%
Integrantes
Reginaldo José Pimentel Silva

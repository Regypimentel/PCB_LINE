#ifndef COMMON_H
#define FIFO_OVEN_AOI "/tmp/pcbline_oven_aoi.fifo"
#define COMMON_H
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sched.h>
#include <pthread.h>
#include <semaphore.h>
#include <mqueue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MQ_NAME  "/pcb_mq"
#define SHM_NAME "/pcb_shm"

#define LOG(tag, fmt, ...) printf("[%-10s] " fmt "\n", tag, ##__VA_ARGS__)

/* Placa em producao: trafega pela fila de mensagens e pelo pipe */
typedef struct {
    int id;
    int components;
} board_t;

/* Estatisticas na memoria compartilhada (protegidas por mutex process-shared) */
typedef struct {
    pthread_mutex_t lock;
    int placed_components;
    int boards_assembled;
    int boards_reflowed;
    int boards_ok;
    int boards_rejected;
} stats_t;

typedef struct {
    int boards;       /* quantidade de placas */
    int comps;        /* componentes por placa */
    int feeders;      /* threads alimentadoras */
    int heads;        /* threads cabecotes de colocacao */
    int defect_pct;   /* % de chance de defeito na inspecao */
    int speed_ms;     /* tempo base de colocacao de um componente */
    int race;         /* 1 = demonstra condicao de corrida */
} config_t;

extern config_t cfg;
extern stats_t *g_stats;

void run_placement(void);
void run_oven(int pipe_wr);
void run_inspect(int pipe_rd);
#endif

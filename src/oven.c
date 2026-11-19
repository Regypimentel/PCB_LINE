/* Estacao 2 (processo): Forno de refluxo.
- Recebe placas pela fila de mensagens POSIX e envia ao AOI por FIFO (named pipe). */
#include "common.h"

void run_oven(void) {
    mqd_t mq = mq_open(MQ_NAME, O_RDONLY);
    if (mq == (mqd_t)-1) { perror("mq_open"); exit(1); }

    int fifo_wr = open(FIFO_PATH, O_WRONLY);
    if (fifo_wr < 0) { perror("open fifo_wr"); mq_close(mq); exit(1); }

    board_t b;
    for (int i = 0; i < cfg.boards; i++) {
        if (mq_receive(mq, (char *)&b, sizeof b, NULL) < 0) { perror("mq_receive"); break; }
        LOG("FORNO", "placa #%d: pre-aquecimento 150C > imersao 180C > refluxo 245C > resfriamento", b.id);
        usleep(cfg.speed_ms * 3000);
        pthread_mutex_lock(&g_stats->lock);
        g_stats->boards_reflowed++;
        pthread_mutex_unlock(&g_stats->lock);
        if (write(fifo_wr, &b, sizeof b) != sizeof b) { perror("write fifo"); break; }
    }
    close(fifo_wr);
    mq_close(mq);
}

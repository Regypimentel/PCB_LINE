/* Estacao 3 (processo): Inspecao otica automatica (AOI).
 * Le placas do pipe; defeito => sinal SIGUSR1 ao supervisor. */
#include "common.h"

void run_inspect(int pipe_rd) {
    board_t b;
    unsigned seed = (unsigned)getpid();
    while (read(pipe_rd, &b, sizeof b) == (ssize_t)sizeof b) {
        usleep(cfg.speed_ms * 1000);
        int defect = (rand_r(&seed) % 100) < cfg.defect_pct;
        pthread_mutex_lock(&g_stats->lock);
        if (defect) g_stats->boards_rejected++; else g_stats->boards_ok++;
        pthread_mutex_unlock(&g_stats->lock);
        if (defect) {
            LOG("AOI", "placa #%d REPROVADA (solda fria / tombstone)", b.id);
            kill(getppid(), SIGUSR1);
        } else {
            LOG("AOI", "placa #%d aprovada", b.id);
        }
    }
    close(pipe_rd);
}

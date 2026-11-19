/* Supervisor: cria IPC, faz fork das estacoes, monitora e gera relatorio. */
#include "common.h"
#include <getopt.h>

config_t cfg = { 10, 6, 2, 2, 15, 50, 0 };
stats_t *g_stats;
static volatile sig_atomic_t g_alerts = 0, g_stop = 0;

static void on_usr1(int s) { (void)s; g_alerts++; }
static void on_int(int s)  { (void)s; g_stop = 1; }

static void usage(const char *p) {
    printf("Uso: %s [-n placas] [-c comp/placa] [-f feeders] [-t heads] [-d %%defeito] [-s ms] [-r]\n"
           "  -r  desliga o mutex do contador local (demo de condicao de corrida)\n", p);
}

int main(int argc, char **argv) {
    int o;
    while ((o = getopt(argc, argv, "n:c:f:t:d:s:rh")) != -1) {
        switch (o) {
        case 'n': cfg.boards = atoi(optarg); break;
        case 'c': cfg.comps = atoi(optarg); break;
        case 'f': cfg.feeders = atoi(optarg); break;
        case 't': cfg.heads = atoi(optarg); break;
        case 'd': cfg.defect_pct = atoi(optarg); break;
        case 's': cfg.speed_ms = atoi(optarg); break;
        case 'r': cfg.race = 1; break;
        default: usage(argv[0]); return 1;
        }
    }
    if (cfg.feeders > 16) cfg.feeders = 16;
    if (cfg.heads > 16) cfg.heads = 16;
    setvbuf(stdout, NULL, _IOLBF, 0);

    /* --- memoria compartilhada + mutex process-shared --- */
    shm_unlink(SHM_NAME); mq_unlink(MQ_NAME);
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
    if (fd < 0 || ftruncate(fd, sizeof(stats_t)) < 0) { perror("shm"); return 1; }
    g_stats = mmap(NULL, sizeof(stats_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (g_stats == MAP_FAILED) { perror("mmap"); return 1; }
    close(fd);
    memset(g_stats, 0, sizeof *g_stats);
    pthread_mutexattr_t at;
    pthread_mutexattr_init(&at);
    pthread_mutexattr_setpshared(&at, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&g_stats->lock, &at);

    /* --- fila de mensagens POSIX --- */
    struct mq_attr ma = { .mq_flags = 0, .mq_maxmsg = 10, .mq_msgsize = sizeof(board_t) };
    mqd_t mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0600, &ma);
    if (mq == (mqd_t)-1) { perror("mq_open"); return 1; }
    mq_close(mq);

    /* --- pipe anonimo forno -> AOI --- */
    int pfd[2];
    if (pipe(pfd) < 0) { perror("pipe"); return 1; }

    /* --- sinais --- */
    struct sigaction sa = {0};
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = on_usr1; sigaction(SIGUSR1, &sa, NULL);
    sa.sa_handler = on_int;  sigaction(SIGINT, &sa, NULL);

    LOG("SUPERVISOR", "iniciando linha: %d placas x %d componentes | %d feeders, %d heads%s",
        cfg.boards, cfg.comps, cfg.feeders, cfg.heads, cfg.race ? " | MODO RACE" : "");

    pid_t pids[3];
    if ((pids[0] = fork()) == 0) { close(pfd[0]); close(pfd[1]); run_placement(); _exit(0); }
    if ((pids[1] = fork()) == 0) { close(pfd[0]); run_oven(pfd[1]); _exit(0); }
    if ((pids[2] = fork()) == 0) { close(pfd[1]); run_inspect(pfd[0]); _exit(0); }
    close(pfd[0]); close(pfd[1]);

    int alive = 3;
    while (alive > 0) {
        if (g_stop) {
            LOG("SUPERVISOR", "SIGINT recebido: parando a linha");
            for (int i = 0; i < 3; i++) kill(pids[i], SIGTERM);
            g_stop = 0;
        }
        int st;
        pid_t p = waitpid(-1, &st, WNOHANG);
        if (p > 0) { alive--; continue; }
        if (p < 0 && errno != EINTR) break;
        if (p == 0) {
            pthread_mutex_lock(&g_stats->lock);
            LOG("SUPERVISOR", "progresso: comp=%d montadas=%d forno=%d ok=%d rej=%d",
                g_stats->placed_components, g_stats->boards_assembled, g_stats->boards_reflowed,
                g_stats->boards_ok, g_stats->boards_rejected);
            pthread_mutex_unlock(&g_stats->lock);
            usleep(300000);
        }
    }

    /* --- relatorio final; carimbo de hora via fork + exec --- */
    pid_t d = fork();
    if (d == 0) {
        printf("\n========== RELATORIO FINAL ==========\n");
        fflush(stdout);
        execlp("date", "date", "+Gerado as %H:%M:%S", (char *)NULL);
        _exit(1);
    }
    waitpid(d, NULL, 0);
    printf("Placas produzidas : %d\nComponentes       : %d\nAprovadas         : %d\n"
           "Reprovadas        : %d\nAlertas SIGUSR1   : %d\n",
           g_stats->boards_reflowed, g_stats->placed_components,
           g_stats->boards_ok, g_stats->boards_rejected, (int)g_alerts);
    if (g_stats->boards_reflowed)
        printf("Rendimento (yield): %.1f%%\n", 100.0 * g_stats->boards_ok / g_stats->boards_reflowed);

    pthread_mutex_destroy(&g_stats->lock);
    munmap(g_stats, sizeof(stats_t));
    shm_unlink(SHM_NAME);
    mq_unlink(MQ_NAME);
    return 0;
}

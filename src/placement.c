/* Estacao 1 (processo): Pick & Place com threads.
 * - feeders  -> produtores de componentes
 * - heads    -> consumidores (colocam o componente na placa)
 * - dispatcher -> espera (variavel de condicao) placa completa e envia via mq */
#include "common.h"

#define BUFSZ 8
static struct {
    int buf[BUFSZ]; int in, out;
    pthread_mutex_t m;
    sem_t empty, full;
} ring;

static pthread_mutex_t claim_m = PTHREAD_MUTEX_INITIALIZER;
static int produced = 0, claimed = 0;

static pthread_mutex_t board_m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  board_ready = PTHREAD_COND_INITIALIZER;
static int cur_comps = 0, ready_boards = 0;

static pthread_mutex_t counter_m = PTHREAD_MUTEX_INITIALIZER;
static int counter = 0;   /* contador local usado na demo de corrida */
static mqd_t mq;

static void *feeder(void *arg) {
    long id = (long)arg;
    int total = cfg.boards * cfg.comps;
    for (;;) {
        pthread_mutex_lock(&claim_m);
        if (produced >= total) { pthread_mutex_unlock(&claim_m); break; }
        int comp = produced++;
        pthread_mutex_unlock(&claim_m);

        usleep(cfg.speed_ms * 500);            /* tempo de separar o componente */
        sem_wait(&ring.empty);
        pthread_mutex_lock(&ring.m);
        ring.buf[ring.in] = comp; ring.in = (ring.in + 1) % BUFSZ;
        pthread_mutex_unlock(&ring.m);
        sem_post(&ring.full);
    }
    (void)id;
    return NULL;
}

static void *head(void *arg) {
    long id = (long)arg;
    int total = cfg.boards * cfg.comps;
    for (;;) {
        pthread_mutex_lock(&claim_m);
        if (claimed >= total) { pthread_mutex_unlock(&claim_m); break; }
        claimed++;
        pthread_mutex_unlock(&claim_m);

        sem_wait(&ring.full);
        pthread_mutex_lock(&ring.m);
        int comp = ring.buf[ring.out]; ring.out = (ring.out + 1) % BUFSZ;
        pthread_mutex_unlock(&ring.m);
        sem_post(&ring.empty);

        usleep(cfg.speed_ms * 1000);           /* tempo de colocar */
        (void)comp;

        /* --- secao critica: contador local --- */
        if (cfg.race) {                         /* SEM protecao: race condition */
            int tmp = counter;
            usleep(100);
            counter = tmp + 1;
        } else {                                /* COM mutex */
            pthread_mutex_lock(&counter_m);
            counter++;
            pthread_mutex_unlock(&counter_m);
        }

        /* estatistica global (memoria compartilhada entre processos) */
        pthread_mutex_lock(&g_stats->lock);
        g_stats->placed_components++;
        pthread_mutex_unlock(&g_stats->lock);

        /* placa completa? avisa o dispatcher */
        pthread_mutex_lock(&board_m);
        if (++cur_comps == cfg.comps) {
            cur_comps = 0; ready_boards++;
            pthread_cond_signal(&board_ready);
        }
        pthread_mutex_unlock(&board_m);
    }
    (void)id;
    return NULL;
}

static void *dispatcher(void *arg) {
    (void)arg;
    for (int sent = 0; sent < cfg.boards; sent++) {
        pthread_mutex_lock(&board_m);
        while (ready_boards == 0)
            pthread_cond_wait(&board_ready, &board_m);
        ready_boards--;
        pthread_mutex_unlock(&board_m);

        board_t b = { .id = sent + 1, .components = cfg.comps };
        mq_send(mq, (const char *)&b, sizeof b, 0);

        pthread_mutex_lock(&g_stats->lock);
        g_stats->boards_assembled++;
        pthread_mutex_unlock(&g_stats->lock);
        LOG("PICK&PLACE", "placa #%d montada -> enviada ao forno (mq)", b.id);
    }
    return NULL;
}

void run_placement(void) {
    mq = mq_open(MQ_NAME, O_WRONLY);
    if (mq == (mqd_t)-1) { perror("mq_open"); exit(1); }
    pthread_mutex_init(&ring.m, NULL);
    sem_init(&ring.empty, 0, BUFSZ);
    sem_init(&ring.full, 0, 0);

    pthread_t th_f[16], th_h[16], th_d;
    pthread_create(&th_d, NULL, dispatcher, NULL);
    for (long i = 0; i < cfg.feeders; i++) pthread_create(&th_f[i], NULL, feeder, (void *)i);
    for (long i = 0; i < cfg.heads;   i++) pthread_create(&th_h[i], NULL, head,   (void *)i);
    for (int i = 0; i < cfg.feeders; i++) pthread_join(th_f[i], NULL);
    for (int i = 0; i < cfg.heads;   i++) pthread_join(th_h[i], NULL);
    pthread_join(th_d, NULL);

    int expected = cfg.boards * cfg.comps;
    LOG("PICK&PLACE", "contador local: esperado=%d obtido=%d %s", expected, counter,
        counter == expected ? "(OK)" : "<-- CONDICAO DE CORRIDA!");
    mq_close(mq);
}

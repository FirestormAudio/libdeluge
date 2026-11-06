/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "deluge/leds.h"
#include "leds_priv.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define N_IND 36
#define N_GOLD_COL 2
#define N_GOLD 4

struct deluge_leds {
    int ind[N_IND];
    int gold[N_GOLD_COL][N_GOLD];
    int sync; /* the P6_7 GPIO LED (deluge:sync); -1 if absent */
};

static const char *leds_root(void) {
    const char *r = getenv("DELUGE_LEDS_ROOT");
    return r ? r : "/sys/class/leds";
}

static int open_brightness(const char *name) {
    char p[256];
    snprintf(p, sizeof p, "%s/%s/brightness", leds_root(), name);
    return open(p, O_WRONLY | O_CLOEXEC);
}

static int write_val(int fd, int v) {
    if (fd < 0) return DELUGE_ERR_NODEV;
    char buf[16];
    int n = snprintf(buf, sizeof buf, "%d", v);
    if (n < 0 || (size_t)n >= sizeof buf) return DELUGE_ERR_IO;
    /* sysfs brightness is rewritable at offset 0 */
    if (pwrite(fd, buf, (size_t)n, 0) != n) return DELUGE_ERR_IO;
    /* Truncate to the written length so a shorter value fully replaces a longer
       one. On real sysfs each write hits the store callback (no accumulation) and
       ftruncate is a harmless no-op/ignored; on the host fixture (a real file) this
       is what makes e.g. "5" correctly replace "255" instead of leaving "555". */
    (void)ftruncate(fd, n);
    return DELUGE_OK;
}

deluge_leds_t *deluge_leds_new(void) {
    deluge_leds_t *l = calloc(1, sizeof *l);
    if (!l) return NULL;
    int opened = 0;
    char name[32];
    for (int i = 0; i < N_IND; i++) {
        snprintf(name, sizeof name, "deluge:indicator:%d", i);
        l->ind[i] = open_brightness(name);
        if (l->ind[i] >= 0) opened++;
    }
    for (int c = 0; c < N_GOLD_COL; c++)
        for (int i = 0; i < N_GOLD; i++) {
            snprintf(name, sizeof name, "deluge:gold%d:%d", c, i);
            l->gold[c][i] = open_brightness(name);
            if (l->gold[c][i] >= 0) opened++;
        }
    l->sync = open_brightness("deluge:sync");
    if (l->sync >= 0) opened++;
    if (opened == 0) { deluge_leds_free(l); return NULL; } /* LED class absent */
    return l;
}

void deluge_leds_free(deluge_leds_t *l) {
    if (!l) return;
    for (int i = 0; i < N_IND; i++) if (l->ind[i] >= 0) close(l->ind[i]);
    for (int c = 0; c < N_GOLD_COL; c++)
        for (int i = 0; i < N_GOLD; i++) if (l->gold[c][i] >= 0) close(l->gold[c][i]);
    if (l->sync >= 0) close(l->sync);
    free(l);
}

int deluge_leds_indicator(deluge_leds_t *l, int id, int on) {
    if (!l) return DELUGE_ERR_STATE;
    if (id < 0 || id >= N_IND) return DELUGE_ERR_INVAL;
    return write_val(l->ind[id], on ? 1 : 0);
}

int deluge_leds_gold(deluge_leds_t *l, int col, int i, int brightness) {
    if (!l) return DELUGE_ERR_STATE;
    if (col < 0 || col >= N_GOLD_COL || i < 0 || i >= N_GOLD) return DELUGE_ERR_INVAL;
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    return write_val(l->gold[col][i], brightness);
}

int deluge_leds_sync(deluge_leds_t *l, int on) {
    if (!l) return DELUGE_ERR_STATE;
    return write_val(l->sync, on ? 1 : 0);
}

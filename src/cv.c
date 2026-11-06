/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "deluge/cv.h"
#include "cv_priv.h"
#include <dirent.h>
#include <fcntl.h>
#include <linux/gpio.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define CV_CHANNELS 2
/* MAX5136 transfer function: 1 V = 6552 counts, so 10 V = 65520. Matches the
   stock firmware's CV engine (DelugeFirmware cv_engine.cpp). Unipolar. */
#define DELUGE_CV_COUNTS_PER_V 6552.0f
#define N_GATES 4

struct deluge_cv {
    int raw_fd[CV_CHANNELS];   /* out_voltageN_raw, -1 if absent */
    int gate_fd[N_GATES];      /* per-line GPIO v2 request fd, -1 if absent */
};

/* Scan /dev/gpiochip* for a line named `name`; on match request it as an
   active-low output and return the line-request fd, else -1. */
static int gate_request(const char *name) {
    for (int c = 0; c < 32; c++) {
        char path[32];
        snprintf(path, sizeof path, "/dev/gpiochip%d", c);
        int chip = open(path, O_RDWR | O_CLOEXEC);
        if (chip < 0) continue;
        struct gpiochip_info ci;
        if (ioctl(chip, GPIO_GET_CHIPINFO_IOCTL, &ci) < 0) { close(chip); continue; }
        for (uint32_t off = 0; off < ci.lines; off++) {
            struct gpio_v2_line_info li;
            memset(&li, 0, sizeof li);
            li.offset = off;
            if (ioctl(chip, GPIO_V2_GET_LINEINFO_IOCTL, &li) < 0) continue;
            if (strcmp(li.name, name) != 0) continue;
            struct gpio_v2_line_request req;
            memset(&req, 0, sizeof req);
            req.offsets[0] = off;
            req.num_lines = 1;
            req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT | GPIO_V2_LINE_FLAG_ACTIVE_LOW;
            snprintf(req.consumer, sizeof req.consumer, "libdeluge");
            int rc = ioctl(chip, GPIO_V2_GET_LINE_IOCTL, &req);
            close(chip);
            return (rc < 0) ? -1 : req.fd; /* req.fd filled by the ioctl */
        }
        close(chip);
    }
    return -1;
}

static int gate_write(int fd, int on) {
    if (fd < 0) return DELUGE_ERR_NODEV;
    struct gpio_v2_line_values v;
    memset(&v, 0, sizeof v);
    v.mask = 1;                 /* line index 0 within this 1-line request */
    v.bits = on ? 1 : 0;        /* logical value (active-low handled by the request flag) */
    if (ioctl(fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &v) < 0) return DELUGE_ERR_IO;
    return DELUGE_OK;
}

int deluge_gate_set(deluge_cv_t *cv, int n, int on) {
    if (!cv) return DELUGE_ERR_STATE;
    if (n < 0 || n >= N_GATES) return DELUGE_ERR_INVAL;
    return gate_write(cv->gate_fd[n], on);
}

static const char *iio_root(void) {
    const char *r = getenv("DELUGE_IIO_ROOT");
    return r ? r : "/sys/bus/iio/devices";
}

/* Find the iio:deviceN whose `name` file reads "deluge-cv". Returns 0 + fills dir, or -1. */
static int find_cv_dir(char *out, size_t n) {
    DIR *d = opendir(iio_root());
    if (!d) return -1;
    struct dirent *e;
    int rc = -1;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "iio:device", 10) != 0) continue;
        char np[512], buf[64];
        snprintf(np, sizeof np, "%s/%s/name", iio_root(), e->d_name);
        FILE *f = fopen(np, "r");
        if (!f) continue;
        char *got = fgets(buf, sizeof buf, f);
        fclose(f);
        if (!got) continue;
        buf[strcspn(buf, "\n")] = 0;
        if (strcmp(buf, "deluge-cv") == 0) {
            snprintf(out, n, "%s/%s", iio_root(), e->d_name);
            rc = 0; break;
        }
    }
    closedir(d);
    return rc;
}

static int write_raw_fd(int fd, int v) {
    if (fd < 0) return DELUGE_ERR_NODEV;
    char buf[16];
    int n = snprintf(buf, sizeof buf, "%d", v);
    if (n < 0 || (size_t)n >= sizeof buf) return DELUGE_ERR_IO;
    if (pwrite(fd, buf, (size_t)n, 0) != n) return DELUGE_ERR_IO;
    (void)ftruncate(fd, n); /* shorter value fully replaces longer (fixture + robustness) */
    return DELUGE_OK;
}

deluge_cv_t *deluge_cv_new(void) {
    deluge_cv_t *cv = calloc(1, sizeof *cv);
    if (!cv) return NULL;
    for (int i = 0; i < CV_CHANNELS; i++) cv->raw_fd[i] = -1;
    for (int i = 0; i < N_GATES; i++) cv->gate_fd[i] = -1; /* init before requests */
    int opened = 0;
    char dir[512];
    if (find_cv_dir(dir, sizeof dir) == 0) {
        for (int i = 0; i < CV_CHANNELS; i++) {
            char p[1024];
            snprintf(p, sizeof p, "%s/out_voltage%d_raw", dir, i);
            cv->raw_fd[i] = open(p, O_WRONLY | O_CLOEXEC);
            if (cv->raw_fd[i] >= 0) opened++;
        }
    }
    for (int i = 0; i < N_GATES; i++) {
        char name[16];
        snprintf(name, sizeof name, "deluge-gate%d", i);
        cv->gate_fd[i] = gate_request(name);
        if (cv->gate_fd[i] >= 0) opened++;
    }
    if (opened == 0) { deluge_cv_free(cv); return NULL; }
    return cv;
}

void deluge_cv_free(deluge_cv_t *cv) {
    if (!cv) return;
    for (int i = 0; i < CV_CHANNELS; i++) if (cv->raw_fd[i] >= 0) close(cv->raw_fd[i]);
    for (int i = 0; i < N_GATES; i++) if (cv->gate_fd[i] >= 0) close(cv->gate_fd[i]);
    free(cv);
}

int deluge_cv_set_raw(deluge_cv_t *cv, int ch, uint16_t raw) {
    if (!cv) return DELUGE_ERR_STATE;
    if (ch < 0 || ch >= CV_CHANNELS) return DELUGE_ERR_INVAL;
    return write_raw_fd(cv->raw_fd[ch], (int)raw);
}

int deluge_cv_set_volts(deluge_cv_t *cv, int ch, float volts) {
    if (!cv) return DELUGE_ERR_STATE;
    if (ch < 0 || ch >= CV_CHANNELS) return DELUGE_ERR_INVAL;
    float c = volts * DELUGE_CV_COUNTS_PER_V; /* TODO(hw): offset for bipolar? verify */
    if (isnan(c)) c = 0.0f;
    if (c < 0.0f) c = 0.0f;
    if (c > 65535.0f) c = 65535.0f;
    return write_raw_fd(cv->raw_fd[ch], (int)lrintf(c));
}

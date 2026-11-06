/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#define _GNU_SOURCE
#include "deluge/input.h"
#include "input_priv.h"
#include "input_decode.h"
#include "discover.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

enum src_kind { SRC_PADS, SRC_BUTTONS, SRC_ENCODERS, SRC_CLOCK, SRC_N };
static const char *SRC_NAME[SRC_N] = {
    "Deluge Pads", "Deluge Buttons", "Synthstrom Deluge Encoders", "Deluge Clock In",
};

struct deluge_input {
    int epfd;
    int fd[SRC_N];               /* -1 if that node was absent */
    deluge_pad_decoder pads;

    /* Library-owned delivery thread. */
    pthread_t thread;
    volatile int running;
    int wake[2];                 /* self-pipe: wake[0] read end registered in epfd, wake[1] write end */
    deluge_input_cb cb;
    void *ctx;
};

deluge_input_t *deluge_input_new(void) {
    deluge_input_t *in = calloc(1, sizeof *in);
    if (!in) return NULL;
    in->epfd = epoll_create1(EPOLL_CLOEXEC);
    if (in->epfd < 0) { free(in); return NULL; }
    for (int i = 0; i < SRC_N; i++) in->fd[i] = -1;
    in->wake[0] = in->wake[1] = -1;
    deluge_pad_decoder_init(&in->pads);

    int opened = 0;
    char path[64];
    for (int i = 0; i < SRC_N; i++) {
        if (deluge_find_input_event(SRC_NAME[i], path, sizeof path) != DELUGE_OK) continue;
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        struct epoll_event ev = { .events = EPOLLIN, .data.u32 = (uint32_t)i };
        if (epoll_ctl(in->epfd, EPOLL_CTL_ADD, fd, &ev) < 0) { close(fd); continue; }
        in->fd[i] = fd;
        opened++;
    }
    if (opened == 0) { deluge_input_free(in); return NULL; } /* nothing present -> unavailable */
    return in;
}

void deluge_input_free(deluge_input_t *in) {
    if (!in) return;
    deluge_input_stop(in); /* join the delivery thread before its fds vanish */
    for (int i = 0; i < SRC_N; i++) if (in->fd[i] >= 0) close(in->fd[i]);
    if (in->epfd >= 0) close(in->epfd);
    free(in);
}

int deluge_input_fd(deluge_input_t *in) { return in ? in->epfd : -1; }

/* Decode one input_event from source `k` into out[0..max), returns count written. */
static int decode_one(deluge_input_t *in, int k, const struct input_event *e,
                      deluge_event *out, int max) {
    if (max <= 0) return 0;
    switch (k) {
        case SRC_BUTTONS:  return deluge_decode_button(e, out);
        case SRC_ENCODERS: return deluge_decode_encoder(e, out);
        case SRC_CLOCK:    return deluge_decode_clock(e, out);
        case SRC_PADS:     return deluge_pad_feed(&in->pads, e, out, max);
        default: return 0;
    }
}

int deluge_input_read(deluge_input_t *in, deluge_event *out, int max) {
    if (!in) return DELUGE_ERR_STATE;
    struct epoll_event evs[SRC_N + 1];
    int nready = epoll_wait(in->epfd, evs, SRC_N + 1, 0); /* non-blocking poll */
    if (nready < 0) return DELUGE_ERR_IO;
    int total = 0;
    for (int r = 0; r < nready && total < max; r++) {
        int k = (int)evs[r].data.u32;
        if (k < 0 || k >= SRC_N) continue; /* skip the wake-pipe tag; fd[] has SRC_N entries */
        if (evs[r].events & (EPOLLHUP | EPOLLERR)) {
            /* source went away — drop it so it can't spin epoll_wait */
            if (in->fd[k] >= 0) {
                epoll_ctl(in->epfd, EPOLL_CTL_DEL, in->fd[k], NULL);
                close(in->fd[k]);
                in->fd[k] = -1;
            }
            continue;
        }
        int fd = in->fd[k];
        struct input_event buf[64];
        while (total < max) {
            ssize_t got = read(fd, buf, sizeof buf);
            if (got <= 0) break; /* EAGAIN or EOF: this fd drained */
            int cnt = (int)(got / (ssize_t)sizeof(struct input_event));
            for (int j = 0; j < cnt && total < max; j++)
                total += decode_one(in, k, &buf[j], &out[total], max - total);
            if ((size_t)got < sizeof buf) break;
        }
    }
    return total;
}

/* Library-owned delivery thread. Blocks on epoll_wait (input or the
   self-pipe wake), drains via deluge_input_read, and invokes cb per event.
   Normal priority only -- input is not real-time; SCHED_FIFO is audio-only.
   Mutually exclusive with driving deluge_input_fd/deluge_input_read directly
   while this thread is running -- pick one delivery model. */
static void *input_loop(void *arg) {
    deluge_input_t *in = arg;
    struct epoll_event evs[SRC_N + 1];
    deluge_event batch[DELUGE_PADS];
    while (in->running) {
        int nready = epoll_wait(in->epfd, evs, SRC_N + 1, -1); /* block until input or wake */
        if (nready < 0) {
            if (errno == EINTR) continue;   /* signal — retry */
            break;                          /* hard error — exit thread cleanly */
        }
        int drained = 1;
        while (drained > 0) {
            drained = deluge_input_read(in, batch, DELUGE_PADS); /* non-blocking drain */
            for (int i = 0; i < drained; i++) in->cb(&batch[i], in->ctx);
        }
    }
    return NULL;
}

int deluge_input_start(deluge_input_t *in, deluge_input_cb cb, void *ctx) {
    if (!in || !cb) return DELUGE_ERR_INVAL;
    if (in->running) return DELUGE_ERR_STATE;
    if (pipe2(in->wake, O_CLOEXEC | O_NONBLOCK) < 0) return DELUGE_ERR_IO;
    struct epoll_event we = { .events = EPOLLIN, .data.u32 = (uint32_t)SRC_N };
    if (epoll_ctl(in->epfd, EPOLL_CTL_ADD, in->wake[0], &we) < 0) {
        close(in->wake[0]); close(in->wake[1]);
        in->wake[0] = in->wake[1] = -1;
        return DELUGE_ERR_IO;
    }
    in->cb = cb; in->ctx = ctx; in->running = 1;
    if (pthread_create(&in->thread, NULL, input_loop, in) != 0) {
        in->running = 0;
        epoll_ctl(in->epfd, EPOLL_CTL_DEL, in->wake[0], NULL);
        close(in->wake[0]); close(in->wake[1]);
        in->wake[0] = in->wake[1] = -1;
        return DELUGE_ERR_IO;
    }
    return DELUGE_OK;
}

void deluge_input_stop(deluge_input_t *in) {
    if (!in || !in->running) return;
    in->running = 0;
    char b = 1; ssize_t w = write(in->wake[1], &b, 1); (void)w; /* wake epoll_wait */
    pthread_join(in->thread, NULL);
    epoll_ctl(in->epfd, EPOLL_CTL_DEL, in->wake[0], NULL);
    close(in->wake[0]); close(in->wake[1]);
    in->wake[0] = in->wake[1] = -1;
}

/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#define _GNU_SOURCE
#include "deluge/midi.h"
#include "midi_priv.h"
#include <alsa/asoundlib.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct deluge_midi {
    snd_rawmidi_t *in, *out;
    /* Push-delivery reader thread (deluge_midi_start). */
    pthread_t thread;
    volatile int running;
    int wake[2];            /* self-pipe: [0] polled, [1] written by stop() */
    deluge_midi_cb cb;
    void *ctx;
};

deluge_midi_t *deluge_midi_new(void) {
    deluge_midi_t *m = calloc(1, sizeof *m);
    if (!m) return NULL;
    m->wake[0] = m->wake[1] = -1;
    int rc = snd_rawmidi_open(&m->in, &m->out, "hw:CARD=Deluge", SND_RAWMIDI_NONBLOCK);
    if (rc < 0) {
        fprintf(stderr, "deluge: rawmidi open (hw:CARD=Deluge) failed: %s\n", snd_strerror(rc));
        free(m); return NULL;
    }
    return m;
}
void deluge_midi_free(deluge_midi_t *m) {
    if (!m) return;
    deluge_midi_stop(m); /* join before closing the descriptors it polls */
    if (m->in)  snd_rawmidi_close(m->in);
    if (m->out) snd_rawmidi_close(m->out);
    free(m);
}
deluge_midi_t *deluge_midi(deluge_t *d);   /* defined in core.c */

int deluge_midi_write(deluge_midi_t *m, const uint8_t *buf, size_t n) {
    if (!m || !m->out) return DELUGE_ERR_STATE;
    ssize_t w = snd_rawmidi_write(m->out, buf, n);
    return w < 0 ? DELUGE_ERR_IO : (int)w;
}
int deluge_midi_read(deluge_midi_t *m, uint8_t *buf, size_t n) {
    if (!m || !m->in) return DELUGE_ERR_STATE;
    ssize_t r = snd_rawmidi_read(m->in, buf, n);
    if (r == -EAGAIN) return 0;
    return r < 0 ? DELUGE_ERR_IO : (int)r;
}

static void *midi_loop(void *arg) {
    deluge_midi_t *m = arg;

    int nfds = snd_rawmidi_poll_descriptors_count(m->in);
    if (nfds < 1) return NULL;
    /* +1 for the wake pipe. */
    struct pollfd *pfds = calloc((size_t)nfds + 1, sizeof *pfds);
    if (!pfds) return NULL;
    if (snd_rawmidi_poll_descriptors(m->in, pfds, (unsigned)nfds) < 0) {
        free(pfds);
        return NULL;
    }
    pfds[nfds].fd = m->wake[0];
    pfds[nfds].events = POLLIN;

    uint8_t buf[256];
    while (m->running) {
        if (poll(pfds, (nfds_t)nfds + 1, -1) < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (pfds[nfds].revents & POLLIN) break;  /* stop() */

        /* ALSA multiplexes its own state onto the raw revents, so demangle them
           rather than testing POLLIN directly: a rawmidi descriptor can be
           readable at the OS level with no MIDI byte actually available. */
        unsigned short revents = 0;
        if (snd_rawmidi_poll_descriptors_revents(m->in, pfds, (unsigned)nfds,
                                                 &revents) < 0)
            continue;
        if (!(revents & POLLIN)) continue;

        ssize_t r = snd_rawmidi_read(m->in, buf, sizeof buf);
        if (r == -EAGAIN) continue;
        if (r < 0) {
            /* -ENODEV: the card is gone (rebind/unplug). Nothing improves by
               spinning on it — exit and let deluge_midi_stop join us. */
            if (r == -ENODEV) break;
            continue;
        }
        if (r > 0 && m->cb) m->cb(buf, (size_t)r, m->ctx);
    }
    free(pfds);
    return NULL;
}

int deluge_midi_start(deluge_midi_t *m, deluge_midi_cb cb, void *ctx) {
    if (!m || !m->in || !cb) return DELUGE_ERR_INVAL;
    if (m->running) return DELUGE_ERR_STATE;
    if (pipe2(m->wake, O_CLOEXEC | O_NONBLOCK) < 0) return DELUGE_ERR_IO;
    m->cb = cb; m->ctx = ctx; m->running = 1;
    if (pthread_create(&m->thread, NULL, midi_loop, m) != 0) {
        m->running = 0;
        close(m->wake[0]); close(m->wake[1]);
        m->wake[0] = m->wake[1] = -1;
        return DELUGE_ERR_IO;
    }
    return DELUGE_OK;
}

void deluge_midi_stop(deluge_midi_t *m) {
    if (!m || !m->running) return;
    m->running = 0;
    char b = 1;
    ssize_t w = write(m->wake[1], &b, 1); (void)w;  /* interrupt poll() */
    pthread_join(m->thread, NULL);
    close(m->wake[0]); close(m->wake[1]);
    m->wake[0] = m->wake[1] = -1;
}

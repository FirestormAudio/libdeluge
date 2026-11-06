/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "deluge/audio.h"
#include "audio_convert.h"
#include "audio_priv.h"
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct deluge_audio {
    snd_pcm_t *play, *cap;
    pthread_t thread;
    deluge_audio_cb cb;
    void *ctx;
    volatile int running;      /* cleared by stop(), or by the thread if recovery fails */
    int started;               /* a thread exists and owes a join + PCM close */
    unsigned long xruns;
    uint32_t dither_lfsr;
    int32_t  s_in[DELUGE_PERIOD * DELUGE_CH];
    int32_t  s_out[DELUGE_PERIOD * DELUGE_CH];
    float    f_in[DELUGE_PERIOD * DELUGE_CH];
    float    f_out[DELUGE_PERIOD * DELUGE_CH];
};

/* Largest S24 magnitude, as an integer (audio_convert.c's S24_MAX is a float
   for the scaling maths; adding dither needs the integer bound to clamp to). */
#define S24_INT_MAX 8388607

/* One step of the +/-16-LSB LFSR dither.
 *
 * Mixed into every output sample so sustained silence never presents the codec
 * with ~8192 identical samples, which trips its auto-mute: the codec goes quiet
 * during a rest and the first note after it is audibly late while it un-mutes.
 * Ported from the Deluge firmware's dither_sample. The firmware shifts the
 * result left 8 because its samples are MSB-aligned in i32; ours are ALSA
 * S24_LE, right-aligned in [23:0], so the same +/-16 LSB needs no shift here.
 *
 * `*lfsr` must never be zero: zero is a fixed point of this recurrence, so it
 * would emit a constant -16 forever — a DC offset that is not dither at all and
 * would still trip the auto-mute it exists to prevent. Seeded in
 * deluge_audio_start (the struct arrives zeroed from calloc). */
static int32_t dither_next(uint32_t *lfsr) {
    uint32_t bit = *lfsr & 1u;
    *lfsr >>= 1;
    if (bit) *lfsr ^= 0xB400u;
    return (int32_t)(*lfsr & 0x1Fu) - 0x10;
}

static int cfg(snd_pcm_t *pcm) {
    snd_pcm_hw_params_t *hw;
    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(pcm, hw);
    if (snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) return DELUGE_ERR_FORMAT;
    if (snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S24_LE) < 0) return DELUGE_ERR_FORMAT;
    if (snd_pcm_hw_params_set_channels(pcm, hw, DELUGE_CH) < 0) return DELUGE_ERR_FORMAT;
    unsigned rate = DELUGE_SR;
    if (snd_pcm_hw_params_set_rate(pcm, hw, rate, 0) < 0) return DELUGE_ERR_FORMAT;
    snd_pcm_uframes_t period = DELUGE_PERIOD;
    if (snd_pcm_hw_params_set_period_size(pcm, hw, period, 0) < 0) return DELUGE_ERR_FORMAT;
    unsigned periods = 3;
    snd_pcm_hw_params_set_periods_near(pcm, hw, &periods, 0);
    if (snd_pcm_hw_params(pcm, hw) < 0) return DELUGE_ERR_IO;

    /* Never auto-start on a write: the loop prefills the playback ring with
       silence and then starts the linked pair explicitly. Without this, the
       linked start fires with an empty playback buffer, the DAC underruns
       before the loop's first writei, and (because cap+play are linked) both
       streams xrun-storm every period. */
    snd_pcm_sw_params_t *sw;
    snd_pcm_sw_params_alloca(&sw);
    if (snd_pcm_sw_params_current(pcm, sw) < 0) return DELUGE_ERR_IO;
    snd_pcm_uframes_t boundary = 0;
    if (snd_pcm_sw_params_get_boundary(sw, &boundary) < 0) return DELUGE_ERR_IO;
    if (snd_pcm_sw_params_set_start_threshold(pcm, sw, boundary) < 0) return DELUGE_ERR_IO;
    if (snd_pcm_sw_params(pcm, sw) < 0) return DELUGE_ERR_IO;
    return DELUGE_OK;
}

/* Prefill the playback ring with silence, then explicitly start the linked pair.
 *
 * Called at startup AND after every successful xrun recovery — the latter is
 * not optional. `cfg` sets start_threshold = boundary, which permanently
 * disables auto-start, and `snd_pcm_recover` puts the stream back into
 * PREPARED rather than RUNNING. So a recovered stream never resumes on its
 * own: the next snd_pcm_readi cannot start capture, the loop stops making
 * progress, recovery fails, and the thread exits. That killed audio for the
 * life of the process on the *first* xrun, with no error reachable by the app
 * (observed on hardware 2026-07-21: both PCMs PREPARED, hw_ptr 0, RT thread
 * gone from /proc/<pid>/task, process still alive).
 *
 * Returns 0 if the pair is running, -1 if it could not be started.
 */
static int prime_and_start(struct deluge_audio *a) {
    /* Leave one period of headroom so the loop's first writei doesn't block. */
    memset(a->s_out, 0, sizeof a->s_out);
    snd_pcm_uframes_t bufsz = 0, per = 0;
    if (snd_pcm_get_params(a->play, &bufsz, &per) < 0 || bufsz == 0) {
        /* Extremely unlikely on a just-configured PCM, but never leave bufsz 0
           (that would skip the prefill and reproduce the underrun bug). Fall
           back to the requested depth and log. */
        fprintf(stderr, "deluge: snd_pcm_get_params failed; prefilling default buffer\n");
        bufsz = 3 * (snd_pcm_uframes_t)DELUGE_PERIOD; /* matches the requested period count */
    }
    /* Reserve one period of headroom when the buffer is that large so the loop's
       first writei has room; a smaller negotiated buffer just prefills fully. */
    snd_pcm_uframes_t target = bufsz > (snd_pcm_uframes_t)DELUGE_PERIOD
                                   ? bufsz - (snd_pcm_uframes_t)DELUGE_PERIOD : bufsz;
    for (snd_pcm_uframes_t filled = 0; filled < target; ) {
        snd_pcm_uframes_t want = target - filled;
        if (want > (snd_pcm_uframes_t)DELUGE_PERIOD) want = (snd_pcm_uframes_t)DELUGE_PERIOD;
        snd_pcm_sframes_t w = snd_pcm_writei(a->play, a->s_out, want);
        /* Non-fatal: a partial prefill still beats not starting at all, so fall
           through to the start below rather than giving up on the stream. */
        if (w < 0) { fprintf(stderr, "deluge: playback prefill write failed: %s\n", snd_strerror((int)w)); break; }
        filled += (snd_pcm_uframes_t)w;
    }
    int rc = snd_pcm_start(a->cap);   /* linked -> both start; playback already holds silence */
    if (rc < 0) {
        fprintf(stderr, "deluge: could not start audio streams: %s\n", snd_strerror(rc));
        return -1;
    }
    return 0;
}

static void *loop(void *arg) {
    struct deluge_audio *a = arg;
    struct sched_param sp = { .sched_priority = 80 };
    int prio_rc = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    if (prio_rc != 0)
        fprintf(stderr, "deluge: SCHED_FIFO not granted (need CAP_SYS_NICE?), audio thread at normal priority: %s\n", strerror(prio_rc));
    if (prime_and_start(a) < 0) { a->running = 0; return NULL; }
    while (a->running) {
        snd_pcm_sframes_t n = snd_pcm_readi(a->cap, a->s_in, DELUGE_PERIOD);
        if (n < 0) {
            a->xruns++;
            if (snd_pcm_recover(a->cap, n, 1) < 0) { a->running = 0; break; }
            if (prime_and_start(a) < 0) { a->running = 0; break; }
            memset(a->s_in, 0, sizeof a->s_in);  /* dropped block -> silence */
        }
        deluge_s24_to_f32(a->s_in, a->f_in, DELUGE_PERIOD * DELUGE_CH);
        a->cb(a->f_in, a->f_out, DELUGE_PERIOD, a->ctx);
        deluge_f32_to_s24(a->f_out, a->s_out, DELUGE_PERIOD * DELUGE_CH);
        /* Applied here rather than inside deluge_f32_to_s24 so that conversion
           stays a pure, unit-tested function and the LFSR state lives with the
           stream that owns it. */
        for (int i = 0; i < DELUGE_PERIOD * DELUGE_CH; i++) {
            int32_t v = a->s_out[i] + dither_next(&a->dither_lfsr);
            if (v > S24_INT_MAX) v = S24_INT_MAX;
            else if (v < -S24_INT_MAX) v = -S24_INT_MAX;
            a->s_out[i] = v;
        }
        snd_pcm_sframes_t w = snd_pcm_writei(a->play, a->s_out, DELUGE_PERIOD);
        if (w < 0) {
            a->xruns++;
            if (snd_pcm_recover(a->play, w, 1) < 0) { a->running = 0; break; }
            if (prime_and_start(a) < 0) { a->running = 0; break; }
        }
    }
    return NULL;
}

/* Join the thread and close the PCMs. Also reaps a thread that already exited
   on its own after a failed xrun recovery. */
static void teardown(struct deluge_audio *a) {
    if (!a->started) return;
    a->running = 0;
    pthread_join(a->thread, NULL);
    a->started = 0;
    if (a->cap)  snd_pcm_close(a->cap);
    if (a->play) snd_pcm_close(a->play);
    a->cap = a->play = NULL;
}

int deluge_audio_start(deluge_audio_t *a, deluge_audio_cb cb, void *ctx) {
    if (!a || !cb) return DELUGE_ERR_INVAL;
    if (a->started && a->running) return DELUGE_ERR_STATE;
    teardown(a);
    if (snd_pcm_open(&a->play, "hw:CARD=Deluge", SND_PCM_STREAM_PLAYBACK, 0) < 0) return DELUGE_ERR_NODEV;
    if (snd_pcm_open(&a->cap,  "hw:CARD=Deluge", SND_PCM_STREAM_CAPTURE, 0)  < 0) { snd_pcm_close(a->play); a->play = NULL; return DELUGE_ERR_NODEV; }
    int rc = cfg(a->play); if (rc) goto fail;
    rc = cfg(a->cap);      if (rc) goto fail;
    if (snd_pcm_link(a->cap, a->play) < 0) { rc = DELUGE_ERR_IO; goto fail; }
    a->cb = cb; a->ctx = ctx; a->running = 1;
    /* Any non-zero seed works; zero is a fixed point (see dither_next). */
    a->dither_lfsr = 0xACE1u;
    if (pthread_create(&a->thread, NULL, loop, a) != 0) { rc = DELUGE_ERR_IO; a->running = 0; goto fail; }
    a->started = 1;
    return DELUGE_OK;
fail:
    snd_pcm_close(a->play); snd_pcm_close(a->cap); a->play = a->cap = NULL;
    return rc;
}

void deluge_audio_stop(deluge_audio_t *a) {
    if (a) teardown(a);
}

int deluge_audio_running(deluge_audio_t *a) { return a && a->started && a->running; }

unsigned long deluge_audio_xruns(deluge_audio_t *a) { return a ? a->xruns : 0; }

deluge_audio_t *deluge_audio_new(void) { return calloc(1, sizeof(struct deluge_audio)); }
void deluge_audio_free(deluge_audio_t *a) { if (a) { deluge_audio_stop(a); free(a); } }

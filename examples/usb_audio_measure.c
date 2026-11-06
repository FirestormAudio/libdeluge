/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/* usb_audio_measure — USB audio clock-reconciliation measurement harness.
 *
 * Runs the internal codec (deluge_audio) as the 44.1kHz reference clock and the
 * UAC2 gadget card alongside it, logging the sample-delivery rate of each vs a
 * monotonic wall clock. It shows whether the two clocks track closely enough
 * for a shallow ring buffer with single-sample correction, or whether an
 * asynchronous resampler is needed to bridge them.
 *
 * libdeluge has no USB audio streaming API yet, so this drives the gadget PCM
 * directly with ALSA in a plain blocking loop, only to keep the stream running
 * while it measures.
 *
 * On-device usage (USB role peripheral, gadget bound):
 *   # measure the capture direction (host -> device). On the host, feed silence:
 *   #   aplay -D hw:CARD=Deluge -c2 -r44100 -f S16_LE /dev/zero
 *   ./usb_audio_measure 60 capture
 *
 *   # measure the playback direction (device -> host). On the host, drain it:
 *   #   arecord -D hw:CARD=Deluge -c2 -r44100 -f S16_LE /dev/null
 *   ./usb_audio_measure 60 playback
 *
 * Output: one row/second (t, codec Hz, gadget Hz, ratio, gadget delay, xruns),
 * then a summary with each clock's mean rate and drift in ppm vs 44100.
 */
#include "deluge/deluge.h"
#include "deluge/audio.h"
#include "deluge/usb_audio.h"
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define NOMINAL_SR 44100
#define CHUNK      128           /* frames per transfer (≈2.9ms @44.1k) */

/* Codec (card0 "Deluge") clock reference: a capture-only stream drained on a
   side thread, counting delivered frames. Capture-only deliberately sidesteps
   deluge_audio's full-duplex startup (which underruns without playback prefill)
   — the codec crystal is the same whether we read or write, so a capture-only
   stream is a valid clock reference for the gadget/codec ratio. */
struct ref { volatile unsigned long frames; volatile int stop; snd_pcm_t *pcm; };

static void *ref_thread(void *arg) {
    struct ref *r = arg;
    int32_t buf[CHUNK * 2];      /* S24_LE in 32-bit containers, 2ch */
    while (!r->stop) {
        snd_pcm_sframes_t n = snd_pcm_readi(r->pcm, buf, CHUNK);
        if (n < 0) {
            if (snd_pcm_recover(r->pcm, (int)n, 1) < 0) {
                snd_pcm_prepare(r->pcm);
                snd_pcm_start(r->pcm);
            }
            continue;
        }
        r->frames += (unsigned long)n;
    }
    return NULL;
}

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static double ppm(double measured, double nominal) {
    return (measured / nominal - 1.0) * 1e6;
}

int main(int argc, char **argv) {
    int seconds = (argc > 1) ? atoi(argv[1]) : 60;
    if (seconds <= 0) seconds = 60;
    int playback = (argc > 2 && strcmp(argv[2], "playback") == 0);
    const char *dirname = playback ? "playback (device->host)" : "capture (host->device)";
    /* nocodec: skip the codec reference clock. Use when the codec path is not
       yet healthy on-device — the gadget rate is then measured vs the wall
       clock alone (still a valid drift signal), without the codec RT thread
       contending with the gadget transfer loop. */
    int use_codec = !(argc > 3 && strcmp(argv[3], "nocodec") == 0);

    deluge_t *d = deluge_open();
    if (!d) { fprintf(stderr, "deluge_open failed\n"); return 1; }

    deluge_usb_audio_t *ga = deluge_usb_audio(d);
    if (!ga) {
        fprintf(stderr,
            "No UAC2 gadget card bound. Boot the device from SD with the gadget\n"
            "composite active (USB role = peripheral), then re-run. On the host,\n"
            "run aplay/arecord against hw:CARD=Deluge to pump the stream.\n");
        deluge_close(d);
        return 1;
    }

    deluge_usb_audio_info_t info;
    if (deluge_usb_audio_probe(ga, &info) == DELUGE_OK)
        printf("# gadget card %s: play=%uch cap=%uch rate=%u..%u fmt=0x%x\n",
               deluge_usb_audio_card_id(ga), info.playback_channels,
               info.capture_channels, info.rate_min, info.rate_max, info.formats);

    char hw[48];
    snprintf(hw, sizeof hw, "hw:CARD=%s", deluge_usb_audio_card_id(ga));

    snd_pcm_t *pcm;
    snd_pcm_stream_t stream = playback ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE;
    int rc = snd_pcm_open(&pcm, hw, stream, 0);
    if (rc < 0) { fprintf(stderr, "gadget %s open (%s) failed: %s\n", dirname, hw, snd_strerror(rc)); deluge_close(d); return 1; }
    /* soft_resample=0: use the gadget's true clock, never ALSA rate conversion
       (that would mask exactly the drift we are here to measure). */
    rc = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                            2, NOMINAL_SR, 0, 50000 /* 50ms latency */);
    if (rc < 0) { fprintf(stderr, "gadget set_params failed (44100/S16_LE/2ch): %s\n", snd_strerror(rc)); snd_pcm_close(pcm); deluge_close(d); return 1; }
    snd_pcm_prepare(pcm);
    if (!playback) snd_pcm_start(pcm);   /* capture: begin filling immediately */

    /* Start the codec reference clock (non-fatal if the codec is unavailable —
       then we still measure the gadget rate against the wall clock alone). */
    struct ref ref = {0};
    pthread_t ref_tid;
    int codec = 0;
    if (!use_codec) {
        fprintf(stderr, "# codec reference disabled (nocodec) — gadget measured vs wall clock only\n");
    } else if (snd_pcm_open(&ref.pcm, "hw:CARD=Deluge", SND_PCM_STREAM_CAPTURE, 0) < 0) {
        fprintf(stderr, "# codec (hw:CARD=Deluge) open failed — gadget measured vs wall clock only\n");
    } else if (snd_pcm_set_params(ref.pcm, SND_PCM_FORMAT_S24_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                                  2, NOMINAL_SR, 0, 50000) < 0) {
        fprintf(stderr, "# codec set_params (S24_LE/2ch/44100) failed — gadget vs wall clock only\n");
        snd_pcm_close(ref.pcm); ref.pcm = NULL;
    } else {
        snd_pcm_prepare(ref.pcm);
        snd_pcm_start(ref.pcm);
        if (pthread_create(&ref_tid, NULL, ref_thread, &ref) == 0) codec = 1;
        else { snd_pcm_close(ref.pcm); ref.pcm = NULL; fprintf(stderr, "# codec ref thread create failed\n"); }
    }

    printf("# measuring %s for %ds  (nominal %d Hz)\n", dirname, seconds, NOMINAL_SR);
    printf("# %6s %10s %10s %8s %8s %6s\n", "t_s", "codec_Hz", "gadget_Hz", "ratio", "delay", "xruns");

    int16_t buf[CHUNK * 2];
    memset(buf, 0, sizeof buf);
    unsigned long gframes = 0, xruns = 0, consec_err = 0;
    double t0 = now_s(), tlast = t0;
    unsigned long glast = 0, clast = ref.frames;

    while (now_s() - t0 < (double)seconds) {
        snd_pcm_sframes_t n = playback ? snd_pcm_writei(pcm, buf, CHUNK)
                                       : snd_pcm_readi(pcm, buf, CHUNK);
        if (n < 0) {
            xruns++;
            if (consec_err < 3) fprintf(stderr, "# xfer err t=%.2f n=%ld (%s)\n", now_s() - t0, (long)n, snd_strerror((int)n));
            if (snd_pcm_recover(pcm, (int)n, 1) < 0) {
                snd_pcm_prepare(pcm);
                if (!playback) snd_pcm_start(pcm);
            }
            if (++consec_err > 200) { fprintf(stderr, "gadget: giving up after 200 consecutive errors (%s)\n", snd_strerror((int)n)); break; }
            continue;
        }
        consec_err = 0;
        gframes += (unsigned long)n;

        double t = now_s();
        if (t - tlast >= 1.0) {
            unsigned long cnow = ref.frames;
            double dt = t - tlast;
            double ghz = (double)(gframes - glast) / dt;
            double chz = codec ? (double)(cnow - clast) / dt : 0.0;
            snd_pcm_sframes_t delay = 0;
            snd_pcm_delay(pcm, &delay);
            if (codec)
                printf("  %6.1f %10.1f %10.1f %8.5f %8ld %6lu\n",
                       t - t0, chz, ghz, chz > 0 ? ghz / chz : 0.0, (long)delay, xruns);
            else
                printf("  %6.1f %10s %10.1f %8s %8ld %6lu\n",
                       t - t0, "n/a", ghz, "n/a", (long)delay, xruns);
            tlast = t; glast = gframes; clast = cnow;
        }
    }

    double elapsed = now_s() - t0;
    double gmean = elapsed > 0 ? (double)gframes / elapsed : 0.0;
    printf("\n# --- summary (%s, %.1fs) ---\n", dirname, elapsed);
    printf("# gadget: %.2f Hz mean, drift %+.1f ppm vs %d, xruns=%lu\n",
           gmean, ppm(gmean, NOMINAL_SR), NOMINAL_SR, xruns);
    if (codec) {
        ref.stop = 1;
        pthread_join(ref_tid, NULL);
        double cmean = (double)ref.frames / elapsed;
        printf("# codec:  %.2f Hz mean, drift %+.1f ppm vs %d\n",
               cmean, ppm(cmean, NOMINAL_SR), NOMINAL_SR);
        printf("# gadget/codec rate ratio: %.6f  (%.1f ppm apart)\n",
               cmean > 0 ? gmean / cmean : 0.0, cmean > 0 ? ppm(gmean / cmean, 1.0) : 0.0);
        snd_pcm_close(ref.pcm);
    }
    snd_pcm_close(pcm);
    deluge_close(d);
    return 0;
}

/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/* sine_out: play a 440 Hz sine on both outputs for 3 seconds through the
   audio callback, then print the xrun count. Run with no arguments. */
#include "deluge/audio.h"
#include <math.h>
#include <stdio.h>
#include <unistd.h>

static float phase;
static void cb(const float *in, float *out, int n, void *ctx) {
    (void)in; (void)ctx;
    for (int i = 0; i < n; i++) {
        float s = 0.2f * sinf(phase);
        phase += 2.0f * (float)M_PI * 440.0f / DELUGE_SR;
        if (phase > 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
        out[2*i] = s; out[2*i+1] = s;
    }
}

int main(void) {
    deluge_t *d = deluge_open();
    if (!d) { fprintf(stderr, "open failed\n"); return 1; }
    int rc = deluge_audio_start(deluge_audio(d), cb, NULL);
    if (rc) { fprintf(stderr, "audio: %s\n", deluge_strerror(rc)); return 1; }
    printf("440Hz for 3s...\n");
    sleep(3);
    printf("xruns=%lu\n", deluge_audio_xruns(deluge_audio(d)));
    deluge_close(d);
    return 0;
}

/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/* deluge-selftest: smoke-test every capability (440 Hz tone, display, LEDs,
   CV/gate, SRAM, MIDI echo, input counts, USB role) for about 2 seconds and
   print PASS or WARN based on the xrun count. Run with no arguments. */
#include "deluge/audio.h"
#include "deluge/midi.h"
#include "deluge/input.h"
#include "deluge/leds.h"
#include "deluge/display.h"
#include "deluge/cv.h"
#include "deluge/sram.h"
#include "deluge/usb.h"
#include <math.h>
#include <stdio.h>
#include <unistd.h>

static float phase;
static void cb(const float *in, float *out, int n, void *ctx) {
    (void)in; (void)ctx;
    for (int i = 0; i < n; i++) {
        float s = 0.2f * sinf(phase);
        phase += 2.0f*(float)M_PI*440.0f/DELUGE_SR;
        if (phase > 2.0f*(float)M_PI) phase -= 2.0f*(float)M_PI;
        out[2*i]=s; out[2*i+1]=s;
    }
}

static int input_counts[4]; /* indexed by deluge_event.kind: pad/btn/enc/clk */
static void input_cb(const deluge_event *ev, void *ctx) {
    (void)ctx;
    input_counts[ev->kind]++;
}

int main(void) {
    deluge_t *d = deluge_open();
    if (!d) { fprintf(stderr, "FAIL open\n"); return 1; }
    if (deluge_audio_start(deluge_audio(d), cb, NULL)) { fprintf(stderr, "FAIL audio\n"); return 1; }
    deluge_input_t *in = deluge_input(d);
    if (in) deluge_input_start(in, input_cb, NULL);

    deluge_display_t *disp = deluge_display(d);
    if (disp) {
        uint8_t oled[DELUGE_OLED_BYTES];
        for (size_t i = 0; i < sizeof oled; i++) oled[i] = (i & 1) ? 0xAA : 0x55; /* checker */
        deluge_display_oled_write(disp, oled, sizeof oled);
        uint8_t pads[DELUGE_PADS_BYTES];
        for (size_t i = 0; i < sizeof pads; i += 3) { pads[i] = 8; pads[i+1] = 8; pads[i+2] = 8; } /* dim */
        deluge_display_pads_write(disp, pads, sizeof pads);
        printf("display: ok\n");
    } else {
        printf("display: unavailable\n");
    }

    deluge_leds_t *leds = deluge_leds(d);
    if (leds) {
        deluge_leds_indicator(leds, 0, 1);
        deluge_leds_indicator(leds, 1, 1);
        printf("leds: ok\n");
    } else {
        printf("leds: unavailable\n");
    }

    deluge_cv_t *cv = deluge_cv(d);
    if (cv) {
        deluge_cv_set_raw(cv, 0, 32768); /* CV0 mid-scale */
        deluge_gate_set(cv, 0, 1); usleep(10000); deluge_gate_set(cv, 0, 0); /* pulse gate 0 */
        printf("cv: ok\n");
    } else {
        printf("cv: unavailable\n");
    }

    deluge_sram_t *sram = deluge_sram(d);
    if (sram) {
        size_t n = 0;
        uint8_t *p = deluge_sram_map(sram, &n);
        if (p && n > 0) p[0] = 0x5A; /* write one byte to confirm access */
        printf(p && n > 0 ? "sram: ok\n" : "sram: mapped 0 bytes\n");
    } else {
        printf("sram: unavailable\n");
    }

    uint8_t buf[64];
    for (int t = 0; t < 200; t++) {
        int r = deluge_midi_read(deluge_midi(d), buf, sizeof buf);
        if (r > 0) deluge_midi_write(deluge_midi(d), buf, (size_t)r);
        usleep(10000);
    }
    unsigned long x = deluge_audio_xruns(deluge_audio(d));
    if (in) {
        deluge_input_stop(in);
        printf("input: pad=%d btn=%d enc=%d clk=%d\n",
               input_counts[0], input_counts[1], input_counts[2], input_counts[3]);
    } else {
        printf("input: unavailable\n");
    }
    /* Read-only: do not call deluge_usb_role_set here — flipping to host would
       tear down the USB gadget (CDC-ACM console) mid-test. */
    deluge_usb_t *usb = deluge_usb(d);
    if (usb) {
        deluge_usb_role role;
        if (deluge_usb_role_get(usb, &role) == DELUGE_OK)
            printf("usb-role: %s\n", role == DELUGE_USB_HOST ? "host" : "peripheral");
        else
            printf("usb-role: unavailable\n");
    } else {
        printf("usb-role: unavailable\n");
    }
    deluge_close(d);
    printf("%s: audio ok, xruns=%lu\n", x < 5 ? "PASS" : "WARN", x);
    return 0;
}

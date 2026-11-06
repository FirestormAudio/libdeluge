/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/* usb_midi_dump — read the USB gadget MIDI (deluge_usb_midi) and print received
   bytes as hex. A MIDI-input monitor for the f_midi2 gadget's legacy byte
   stream. Usage: usb_midi_dump [seconds]  (default 10). */
#include "deluge/deluge.h"
#include "deluge/usb_midi.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int secs = argc > 1 ? atoi(argv[1]) : 10;
    deluge_t *d = deluge_open();
    if (!d) { fprintf(stderr, "open failed\n"); return 1; }
    deluge_usb_midi_t *m = deluge_usb_midi(d);
    if (deluge_usb_midi_rebind(m) != DELUGE_OK) {
        fprintf(stderr, "usb midi absent (gadget bound? role=peripheral?)\n");
        deluge_close(d);
        return 1;
    }

    printf("reading usb gadget MIDI for %ds...\n", secs);
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    uint8_t buf[128];
    unsigned long total = 0;
    for (;;) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - t0.tv_sec >= secs) break;
        int n = deluge_usb_midi_read(m, buf, sizeof buf);
        if (n > 0) {
            printf("rx %d:", n);
            for (int i = 0; i < n; i++) printf(" %02x", buf[i]);
            printf("\n");
            total += (unsigned long)n;
        } else {
            usleep(2000);
        }
    }
    printf("total rx bytes: %lu\n", total);
    deluge_close(d);
    return 0;
}

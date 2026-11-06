/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/* midi_echo: echo DIN MIDI in to DIN MIDI out for 10 seconds by polling
   deluge_midi_read(). Run with no arguments. */
#include "deluge/midi.h"
#include <stdio.h>
#include <unistd.h>
int main(void) {
    deluge_t *d = deluge_open();
    if (!d) { fprintf(stderr, "open failed\n"); return 1; }
    deluge_midi_t *m = deluge_midi(d);
    if (!m) { fprintf(stderr, "midi unavailable\n"); deluge_close(d); return 1; }
    uint8_t buf[64];
    printf("echoing MIDI in->out for 10s...\n");
    for (int t = 0; t < 1000; t++) {
        int r = deluge_midi_read(m, buf, sizeof buf);
        if (r > 0) deluge_midi_write(m, buf, (size_t)r);
        usleep(10000);
    }
    deluge_close(d);
    return 0;
}

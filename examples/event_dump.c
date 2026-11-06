/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/* event_dump: print every pad, button, encoder and clock event for 30 seconds,
   using the callback delivery thread. Run with no arguments. */
#include "deluge/input.h"
#include <stdio.h>
#include <unistd.h>

static const char *KIND[] = { "PAD", "BTN", "ENC", "CLK" };
static void cb(const deluge_event *e, void *ctx) {
    (void)ctx;
    printf("%s id=%u val=%d x=%u y=%u p=%u\n",
           KIND[e->kind], e->id, e->value, e->x, e->y, e->pressure);
    fflush(stdout);
}

int main(void) {
    deluge_t *d = deluge_open();
    if (!d) { fprintf(stderr, "open failed\n"); return 1; }
    deluge_input_t *in = deluge_input(d);
    if (!in) { fprintf(stderr, "input unavailable\n"); deluge_close(d); return 1; }
    printf("dumping input for 30s (press pads/buttons/turn encoders)...\n");
    deluge_input_start(in, cb, NULL);
    sleep(30);
    deluge_input_stop(in);
    deluge_close(d);
    return 0;
}

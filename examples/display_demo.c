/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/* display_demo: draw a checkerboard on the OLED, fill the pads dim red, and
   light the indicator and gold-knob LEDs for 3 seconds. Run with no arguments. */
#include "deluge/display.h"
#include "deluge/leds.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    deluge_t *d = deluge_open();
    if (!d) { fprintf(stderr, "open failed\n"); return 1; }

    deluge_display_t *disp = deluge_display(d);
    if (disp) {
        uint8_t oled[DELUGE_OLED_BYTES];
        for (size_t i = 0; i < sizeof oled; i++) oled[i] = (i & 1) ? 0xAA : 0x55; /* checker */
        deluge_display_oled_write(disp, oled, sizeof oled);
        uint8_t pads[DELUGE_PADS_BYTES];
        for (size_t i = 0; i < sizeof pads; i += 3) { pads[i] = 40; pads[i+1] = 0; pads[i+2] = 0; } /* dim red */
        deluge_display_pads_write(disp, pads, sizeof pads);
        printf("wrote OLED checkerboard + pad grid\n");
    } else printf("display unavailable\n");

    deluge_leds_t *leds = deluge_leds(d);
    if (leds) {
        for (int i = 0; i < 36; i++) deluge_leds_indicator(leds, i, i & 1);
        for (int c = 0; c < 2; c++) for (int i = 0; i < 4; i++) deluge_leds_gold(leds, c, i, 64 * i);
        printf("lit indicator + gold LEDs\n");
    } else printf("leds unavailable\n");

    sleep(3);
    deluge_close(d);
    return 0;
}

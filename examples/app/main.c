/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/* example: the smallest deluge_add_app() app. Opens the device, blinks
   indicator LED 0 for 8 seconds and exits. Run with no arguments. */
#include <deluge/deluge.h>
#include <deluge/leds.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
	deluge_t *d = deluge_open();
	if (!d) {
		fprintf(stderr, "deluge_open failed\n");
		return 1;
	}

	deluge_leds_t *leds = deluge_leds(d);
	if (!leds) {
		fprintf(stderr, "LEDs unavailable\n");
		deluge_close(d);
		return 1;
	}

	for (int i = 0; i < 8; i++) {
		deluge_leds_indicator(leds, 0, i % 2);
		sleep(1);
	}

	deluge_close(d);
	return 0;
}

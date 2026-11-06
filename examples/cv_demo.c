/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/* cv_demo: ramp CV output 0 from 0 to full scale, pulse gate 0, and report the
   SRAM mapping. Run with no arguments; watch CV 0 and gate 0 on a scope. */
#include "deluge/cv.h"
#include "deluge/sram.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    deluge_t *d = deluge_open();
    if (!d) { fprintf(stderr, "open failed\n"); return 1; }
    deluge_cv_t *cv = deluge_cv(d);
    if (cv) {
        for (int i = 0; i <= 10; i++) { deluge_cv_set_raw(cv, 0, (uint16_t)(i * 6553)); usleep(50000); }
        deluge_gate_set(cv, 0, 1); usleep(100000); deluge_gate_set(cv, 0, 0);
        printf("cv ramped + gate pulsed\n");
    } else printf("cv/gate unavailable\n");
    deluge_sram_t *sram = deluge_sram(d);
    if (sram) {
        size_t n = 0; void *p = deluge_sram_map(sram, &n);
        printf("sram: %p, %zu bytes\n", p, n);
    } else printf("sram unavailable\n");
    deluge_close(d);
    return 0;
}

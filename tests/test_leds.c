/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "deluge/leds.h"
#include "leds_priv.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Reads back a brightness fixture file written by libdeluge. */
static int read_brightness(const char *root, const char *name) {
    char p[256]; snprintf(p, sizeof p, "%s/%s/brightness", root, name);
    FILE *f = fopen(p, "r"); if (!f) return -1;
    int v = -1; if (fscanf(f, "%d", &v) != 1) v = -1;
    fclose(f); return v;
}

int main(void) {
    const char *root = getenv("DELUGE_LEDS_ROOT");
    assert(root); /* set by ctest to the fixture dir */
    deluge_leds_t *l = deluge_leds_new();
    assert(l);

    assert(deluge_leds_indicator(l, 5, 1) == DELUGE_OK);
    assert(read_brightness(root, "deluge:indicator:5") == 1);
    assert(deluge_leds_indicator(l, 5, 7) == DELUGE_OK); /* any nonzero -> 1 */
    assert(read_brightness(root, "deluge:indicator:5") == 1);
    assert(deluge_leds_indicator(l, 5, 0) == DELUGE_OK);
    assert(read_brightness(root, "deluge:indicator:5") == 0);
    assert(deluge_leds_indicator(l, 36, 1) == DELUGE_ERR_INVAL); /* out of range */

    assert(deluge_leds_gold(l, 1, 3, 200) == DELUGE_OK);
    assert(read_brightness(root, "deluge:gold1:3") == 200);
    assert(deluge_leds_gold(l, 1, 3, 999) == DELUGE_OK); /* clamp to 255 */
    assert(read_brightness(root, "deluge:gold1:3") == 255);
    assert(deluge_leds_gold(l, 1, 3, 5) == DELUGE_OK);    /* shorter value fully replaces "255" */
    assert(read_brightness(root, "deluge:gold1:3") == 5); /* not 555 — write must truncate */
    assert(deluge_leds_gold(l, 2, 0, 1) == DELUGE_ERR_INVAL); /* bad col */
    assert(deluge_leds_gold(l, 0, 4, 1) == DELUGE_ERR_INVAL); /* bad i */

    assert(deluge_leds_sync(l, 1) == DELUGE_OK);
    assert(read_brightness(root, "deluge:sync") == 1);
    assert(deluge_leds_sync(l, 9) == DELUGE_OK); /* any nonzero -> 1 */
    assert(read_brightness(root, "deluge:sync") == 1);
    assert(deluge_leds_sync(l, 0) == DELUGE_OK);
    assert(read_brightness(root, "deluge:sync") == 0);

    assert(deluge_leds_indicator(NULL, 0, 1) == DELUGE_ERR_STATE);
    assert(deluge_leds_gold(NULL, 0, 0, 1) == DELUGE_ERR_STATE);
    assert(deluge_leds_sync(NULL, 1) == DELUGE_ERR_STATE);
    deluge_leds_free(NULL); /* must be NULL-safe */
    deluge_leds_free(l);
    return 0;
}

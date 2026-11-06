/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "deluge/cv.h"
#include "cv_priv.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int read_raw(const char *root, int ch) {
    char p[256];
    snprintf(p, sizeof p, "%s/iio:device0/out_voltage%d_raw", root, ch);
    FILE *f = fopen(p, "r"); if (!f) return -1;
    int v = -1; if (fscanf(f, "%d", &v) != 1) v = -1;
    fclose(f); return v;
}

int main(void) {
    const char *root = getenv("DELUGE_IIO_ROOT");
    assert(root);
    deluge_cv_t *cv = deluge_cv_new();
    assert(cv);

    assert(deluge_cv_set_raw(cv, 0, 12345) == DELUGE_OK);
    assert(read_raw(root, 0) == 12345);
    assert(deluge_cv_set_raw(cv, 1, 65535) == DELUGE_OK);
    assert(read_raw(root, 1) == 65535);
    assert(deluge_cv_set_raw(cv, 1, 42) == DELUGE_OK); /* shorter replaces longer */
    assert(read_raw(root, 1) == 42);
    assert(deluge_cv_set_raw(cv, 2, 0) == DELUGE_ERR_INVAL);  /* only 2 channels */
    assert(deluge_cv_set_raw(cv, -1, 0) == DELUGE_ERR_INVAL);

    /* volts helper: clamps to the raw range regardless of the (placeholder) scale */
    assert(deluge_cv_set_volts(cv, 0, 1e9f) == DELUGE_OK);
    assert(read_raw(root, 0) == 65535);
    assert(deluge_cv_set_volts(cv, 0, -1e9f) == DELUGE_OK);
    assert(read_raw(root, 0) == 0);

    deluge_cv_free(cv);
    return 0;
}

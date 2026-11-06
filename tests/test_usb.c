/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "deluge/usb.h"
#include "usb_priv.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void read_file(const char *p, char *out, size_t n) {
    FILE *f = fopen(p, "r");
    if (!f) { out[0] = 0; return; }
    size_t r = fread(out, 1, n - 1, f);
    out[r] = 0;
    fclose(f);
}

int main(void) {
    const char *path = getenv("DELUGE_USB_ROLE_PATH");
    assert(path); /* set by ctest to the fixture file */
    deluge_usb_t *u = deluge_usb_new();
    assert(u);

    deluge_usb_role r;
    assert(deluge_usb_role_get(u, &r) == DELUGE_OK);
    assert(r == DELUGE_USB_PERIPHERAL); /* fixture initialized to "peripheral\n" */

    assert(deluge_usb_role_set(u, DELUGE_USB_HOST) == DELUGE_OK);
    char buf[32]; read_file(path, buf, sizeof buf);
    assert(strcmp(buf, "host") == 0); /* file is exactly "host" — ftruncate dropped the tail */
    assert(deluge_usb_role_get(u, &r) == DELUGE_OK && r == DELUGE_USB_HOST);

    assert(deluge_usb_role_set(u, DELUGE_USB_PERIPHERAL) == DELUGE_OK);
    assert(deluge_usb_role_get(u, &r) == DELUGE_OK && r == DELUGE_USB_PERIPHERAL);

    assert(deluge_usb_role_get(NULL, &r) == DELUGE_ERR_STATE);
    assert(deluge_usb_role_set(NULL, DELUGE_USB_HOST) == DELUGE_ERR_STATE);
    assert(deluge_usb_role_get(u, NULL) == DELUGE_ERR_INVAL);

    deluge_usb_free(u);
    deluge_usb_free(NULL); /* NULL-safe */
    return 0;
}

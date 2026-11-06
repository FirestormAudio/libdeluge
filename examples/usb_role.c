/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/* usb_role: print the USB port role, or switch it first.
   Usage: usb_role [host|peripheral]. Switching to host drops the USB console. */
#include "deluge/usb.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    deluge_t *d = deluge_open();
    if (!d) { fprintf(stderr, "open failed\n"); return 1; }
    deluge_usb_t *u = deluge_usb(d);
    if (!u) { fprintf(stderr, "usb role control unavailable\n"); deluge_close(d); return 1; }

    if (argc > 1) {
        deluge_usb_role want = (strcmp(argv[1], "host") == 0) ? DELUGE_USB_HOST : DELUGE_USB_PERIPHERAL;
        int rc = deluge_usb_role_set(u, want);
        printf("set %s: %s\n", argv[1], deluge_strerror(rc));
    }
    deluge_usb_role r;
    if (deluge_usb_role_get(u, &r) == DELUGE_OK)
        printf("role: %s\n", r == DELUGE_USB_HOST ? "host" : "peripheral");
    deluge_close(d);
    return 0;
}

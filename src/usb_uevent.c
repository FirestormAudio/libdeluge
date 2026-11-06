/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "usb_uevent.h"
#include "deluge/error.h"
#include <stdlib.h>
#include <string.h>

const char *deluge_usb_host_marker(void) {
    const char *p = getenv("DELUGE_USB_HOST_PATH");
    return (p && *p) ? p : "e8010000.usb";
}

static const char *basename_of(const char *s) {
    const char *slash = strrchr(s, '/');
    return slash ? slash + 1 : s;
}

deluge_usb_class deluge_usb_classify(const char *subsystem, const char *devname) {
    if (!subsystem || !devname) return DELUGE_USB_CLASS_OTHER;
    const char *b = basename_of(devname);
    if (strcmp(subsystem, "sound") == 0) {
        if (strncmp(b, "midi", 4) == 0 || strncmp(b, "ump", 3) == 0) return DELUGE_USB_CLASS_MIDI;
        if (strncmp(b, "pcm", 3) == 0) return DELUGE_USB_CLASS_AUDIO;
        return DELUGE_USB_CLASS_OTHER;
    }
    if (strcmp(subsystem, "input") == 0)
        return strncmp(b, "event", 5) == 0 ? DELUGE_USB_CLASS_INPUT : DELUGE_USB_CLASS_OTHER;
    if (strcmp(subsystem, "block") == 0)
        return strncmp(b, "sd", 2) == 0 ? DELUGE_USB_CLASS_STORAGE : DELUGE_USB_CLASS_OTHER;
    return DELUGE_USB_CLASS_OTHER;
}

int deluge_uevent_is_usb(const char *devpath, const char *host_marker) {
    return devpath && host_marker && strstr(devpath, host_marker) != NULL;
}

int deluge_uevent_parse(char *buf, size_t len, struct deluge_uevent *out) {
    memset(out, 0, sizeof *out);
    /* skip the "ACTION@DEVPATH" header token */
    size_t i = strnlen(buf, len);
    if (i < len) i++; /* past its NUL */
    while (i < len) {
        char *tok = buf + i;
        size_t toklen = strnlen(tok, len - i);
        if (toklen == len - i) return DELUGE_ERR_INVAL; /* truncated: no NUL, unterminated tail */
        if (toklen == 0) break;
        char *eq = memchr(tok, '=', toklen);
        if (eq) {
            *eq = 0;
            char *val = eq + 1;
            if      (!strcmp(tok, "ACTION"))    out->action = val;
            else if (!strcmp(tok, "SUBSYSTEM")) out->subsystem = val;
            else if (!strcmp(tok, "DEVPATH"))   out->devpath = val;
            else if (!strcmp(tok, "DEVNAME"))   out->devname = val;
        }
        i += toklen + 1;
    }
    return (out->action && out->subsystem) ? DELUGE_OK : DELUGE_ERR_INVAL;
}

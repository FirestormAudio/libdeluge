/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "usb_uevent.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Build a raw uevent datagram (header + NUL-separated KEY=VALUE) into buf. */
static size_t mk(char *buf, const char *const *kv, int n) {
    size_t o = 0;
    o += (size_t)sprintf(buf, "add@%s", "/x") + 1;   /* header token + NUL */
    for (int i = 0; i < n; i++) { size_t l = strlen(kv[i]); memcpy(buf + o, kv[i], l + 1); o += l + 1; }
    return o;
}

int main(void) {
    /* classify */
    assert(deluge_usb_classify("sound", "snd/midiC4D0") == DELUGE_USB_CLASS_MIDI);
    assert(deluge_usb_classify("sound", "snd/umpC4D0")  == DELUGE_USB_CLASS_MIDI);
    assert(deluge_usb_classify("sound", "snd/pcmC4D0c") == DELUGE_USB_CLASS_AUDIO);
    assert(deluge_usb_classify("sound", "snd/controlC4")== DELUGE_USB_CLASS_OTHER);
    assert(deluge_usb_classify("input", "input/event5") == DELUGE_USB_CLASS_INPUT);
    assert(deluge_usb_classify("block", "sda1")         == DELUGE_USB_CLASS_STORAGE);
    assert(deluge_usb_classify("net",   "eth0")         == DELUGE_USB_CLASS_OTHER);
    assert(deluge_usb_classify(NULL, NULL)              == DELUGE_USB_CLASS_OTHER);

    /* usb-path filter */
    assert(deluge_uevent_is_usb("/devices/platform/e8010000.usb/usb1/1-1/sound/card4", "e8010000.usb"));
    assert(!deluge_uevent_is_usb("/devices/platform/e8006000.i2s/sound/card0", "e8010000.usb"));
    assert(!deluge_uevent_is_usb(NULL, "e8010000.usb"));

    /* parse */
    char buf[256];
    const char *kv[] = { "ACTION=add", "SUBSYSTEM=sound", "DEVPATH=/devices/x/sound/card4/midiC4D0", "DEVNAME=snd/midiC4D0" };
    size_t len = mk(buf, kv, 4);
    struct deluge_uevent ev;
    assert(deluge_uevent_parse(buf, len, &ev) == DELUGE_OK);
    assert(!strcmp(ev.action, "add") && !strcmp(ev.subsystem, "sound"));
    assert(!strcmp(ev.devname, "snd/midiC4D0"));
    assert(deluge_usb_classify(ev.subsystem, ev.devname) == DELUGE_USB_CLASS_MIDI);

    /* parse: missing subsystem -> INVAL */
    char b2[64]; const char *kv2[] = { "ACTION=add" };
    assert(deluge_uevent_parse(b2, mk(b2, kv2, 1), &ev) == DELUGE_ERR_INVAL);

    /* truncated tail: last record has no terminating NUL -> INVAL, not a bad string */
    char b3[64];
    size_t o = (size_t)sprintf(b3, "add@/x") + 1;          /* header + NUL */
    const char *good = "ACTION=add"; memcpy(b3 + o, good, strlen(good) + 1); o += strlen(good) + 1;
    const char *trunc = "SUBSYSTEM=soun";                   /* NO trailing NUL */
    memcpy(b3 + o, trunc, strlen(trunc)); o += strlen(trunc);
    assert(deluge_uevent_parse(b3, o, &ev) == DELUGE_ERR_INVAL);
    return 0;
}

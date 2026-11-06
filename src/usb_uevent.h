/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef DELUGE_USB_UEVENT_H
#define DELUGE_USB_UEVENT_H
#include <stddef.h>
#include "deluge/usb.h"

struct deluge_uevent { const char *action, *subsystem, *devpath, *devname; };

/* Parse a raw NETLINK_KOBJECT_UEVENT datagram in place (NUL-separated KEY=VALUE
   records after an "ACTION@DEVPATH" header). Mutates buf (splits at '='); the
   out pointers alias into buf. Returns DELUGE_OK / DELUGE_ERR_INVAL. */
int deluge_uevent_parse(char *buf, size_t len, struct deluge_uevent *out);

/* Classify a device by subsystem + dev-relative name (e.g. "snd/midiC4D0"). */
deluge_usb_class deluge_usb_classify(const char *subsystem, const char *devname);

/* Nonzero iff devpath contains host_marker (the USB host controller path). */
int deluge_uevent_is_usb(const char *devpath, const char *host_marker);

/* The host-path marker: env DELUGE_USB_HOST_PATH or the default. */
const char *deluge_usb_host_marker(void);
#endif

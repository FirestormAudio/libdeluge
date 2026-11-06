/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef DELUGE_DISCOVER_H
#define DELUGE_DISCOVER_H
#include <stddef.h>
#include "deluge/error.h"

/* Resolve /dev/input/eventN whose device name matches `name`. */
int deluge_find_input_event(const char *name, char *out, size_t n);
/* Resolve /dev/fbN whose fb_fix_screeninfo.id matches `id`. */
int deluge_find_fb(const char *id, char *out, size_t n);
/* Resolve the ALSA card id (for hw:CARD=<id>) whose card->driver matches `driver`.
   Reads /proc/asound/cards; override with env DELUGE_ASOUND_CARDS (fixture file). */
int deluge_find_card_by_driver(const char *driver, char *out, size_t n);
/* Try each driver in order; return the first present card's id (for hw:CARD=<id>). */
int deluge_find_card_by_drivers(const char *const *drivers, int ndrivers, char *out, size_t n);
#endif

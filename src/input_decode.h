/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef DELUGE_INPUT_DECODE_H
#define DELUGE_INPUT_DECODE_H
#include "deluge/input.h"
struct input_event; /* from <linux/input.h> at call sites */
/* Each returns 1 and fills *out if the event maps to a deluge_event, else 0. */
int deluge_decode_button(const struct input_event *e, deluge_event *out);
int deluge_decode_encoder(const struct input_event *e, deluge_event *out);
int deluge_decode_clock(const struct input_event *e, deluge_event *out);

#include <stdbool.h>
#define DELUGE_PADS 144
typedef struct {
    int cur_slot;                 /* slot selected by the last ABS_MT_SLOT */
    struct {
        int  tracking;            /* current tracking id, -1 = no contact */
        uint16_t x, y, pressure;  /* latest position (grid col/row already divided) */
        bool down;                /* last-reported contact state */
        bool dirty;               /* changed since last SYN flush */
        bool pending_down;        /* contact state accumulated this frame */
    } slot[DELUGE_PADS];
} deluge_pad_decoder;

void deluge_pad_decoder_init(deluge_pad_decoder *d);
int  deluge_pad_feed(deluge_pad_decoder *d, const struct input_event *e, deluge_event *out, int max);
#endif

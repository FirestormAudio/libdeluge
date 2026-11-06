/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "input_decode.h"
#include <linux/input.h>
#include <string.h>

static void base(deluge_event *o, uint8_t kind) { memset(o, 0, sizeof *o); o->kind = kind; }

int deluge_decode_button(const struct input_event *e, deluge_event *out) {
    if (e->type != EV_KEY) return 0;
    int id = (int)e->code - BTN_TRIGGER_HAPPY1;
    if (id < 0 || id > 35) return 0;
    if (e->value != 0 && e->value != 1) return 0; /* drop autorepeat (2) */
    base(out, DELUGE_EV_BUTTON);
    out->id = (uint8_t)id;
    out->value = (int16_t)e->value;
    return 1;
}

int deluge_decode_encoder(const struct input_event *e, deluge_event *out) {
    if (e->type != EV_REL) return 0;
    int id;
    /* Map each physical encoder's evdev REL code (assigned by the kernel
     * `deluge-encoders` driver) to libdeluge's encoder ids (SCROLL_X=0,
     * TEMPO=1, MOD_0=2, MOD_1=3, SCROLL_Y=4, SELECT=5; documented on
     * deluge_event in include/deluge/input.h). The numbering matches the
     * encoder ids used by the Rust deluge-sdk. */
    switch (e->code) {
        case REL_HWHEEL: id = 0; break; /* scroll_x    -> SCROLL_X */
        case REL_MISC:   id = 1; break; /* tempo       -> TEMPO    */
        case REL_RY:     id = 2; break; /* gold_top    -> MOD_0    */
        case REL_RX:     id = 3; break; /* gold_bottom -> MOD_1    */
        case REL_WHEEL:  id = 4; break; /* scroll_y    -> SCROLL_Y */
        case REL_DIAL:   id = 5; break; /* select      -> SELECT   */
        default: return 0;
    }
    base(out, DELUGE_EV_ENCODER);
    out->id = (uint8_t)id;
    out->value = (int16_t)e->value;
    return 1;
}

int deluge_decode_clock(const struct input_event *e, deluge_event *out) {
    if (e->type != EV_MSC || e->code != MSC_RAW) return 0;
    base(out, DELUGE_EV_CLOCK);
    out->value = (int16_t)e->value; /* wraps; consumers detect gaps via deltas */
    return 1;
}

#include <stdbool.h>

void deluge_pad_decoder_init(deluge_pad_decoder *d) {
    memset(d, 0, sizeof *d);
    d->cur_slot = 0;
    for (int i = 0; i < DELUGE_PADS; i++) d->slot[i].tracking = -1;
}

int deluge_pad_feed(deluge_pad_decoder *d, const struct input_event *e, deluge_event *out, int max) {
    if (e->type == EV_ABS) {
        switch (e->code) {
            case ABS_MT_SLOT:
                if (e->value >= 0 && e->value < DELUGE_PADS) d->cur_slot = e->value;
                return 0;
            case ABS_MT_TRACKING_ID: {
                int s = d->cur_slot;
                d->slot[s].tracking = e->value;
                d->slot[s].pending_down = (e->value >= 0);
                d->slot[s].dirty = true;
                return 0;
            }
            case ABS_MT_POSITION_X:
                d->slot[d->cur_slot].x = (uint16_t)(e->value >> 8); /* grid col */
                d->slot[d->cur_slot].dirty = true; return 0;
            case ABS_MT_POSITION_Y:
                d->slot[d->cur_slot].y = (uint16_t)(e->value >> 8); /* grid row */
                d->slot[d->cur_slot].dirty = true; return 0;
            case ABS_MT_PRESSURE:
                d->slot[d->cur_slot].pressure = (uint16_t)e->value;
                d->slot[d->cur_slot].dirty = true; return 0;
            default: return 0;
        }
    }
    if (e->type != EV_SYN || e->code != SYN_REPORT) return 0;

    /* Flush: emit one event per slot whose contact state changed. */
    int n = 0;
    for (int s = 0; s < DELUGE_PADS && n < max; s++) {
        if (!d->slot[s].dirty) continue;
        d->slot[s].dirty = false;
        bool now = d->slot[s].pending_down;
        if (now == d->slot[s].down) continue; /* position-only change: no press/release edge */
        d->slot[s].down = now;
        deluge_event *o = &out[n++];
        memset(o, 0, sizeof *o);
        o->kind = DELUGE_EV_PAD;
        o->id = (uint8_t)s;
        o->value = now ? 1 : 0;
        o->x = d->slot[s].x;
        o->y = d->slot[s].y;
        o->pressure = now ? d->slot[s].pressure : 0;
    }
    return n;
}

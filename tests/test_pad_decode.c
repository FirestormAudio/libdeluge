/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "input_decode.h"
#include <assert.h>
#include <linux/input.h>
#include <string.h>

static struct input_event ie(uint16_t t, uint16_t c, int32_t v) {
    struct input_event e; memset(&e, 0, sizeof e); e.type=t; e.code=c; e.value=v; return e;
}

int main(void) {
    deluge_pad_decoder d;
    deluge_pad_decoder_init(&d);
    deluge_event out[8];
    int n;

    /* Touch down on slot 20 (pad_id 20), col 12 row 2, pressure 4095.
       col*256+128 = 3200, row*256+128 = 640. */
    struct input_event seq_down[] = {
        ie(EV_ABS, ABS_MT_SLOT, 20),
        ie(EV_ABS, ABS_MT_TRACKING_ID, 20),
        ie(EV_ABS, ABS_MT_POSITION_X, 3200),
        ie(EV_ABS, ABS_MT_POSITION_Y, 640),
        ie(EV_ABS, ABS_MT_PRESSURE, 4095),
    };
    for (size_t i = 0; i < sizeof seq_down/sizeof seq_down[0]; i++) {
        n = deluge_pad_feed(&d, &seq_down[i], out, 8);
        assert(n == 0); /* nothing until SYN_REPORT */
    }
    struct input_event syn = ie(EV_SYN, SYN_REPORT, 0);
    n = deluge_pad_feed(&d, &syn, out, 8);
    assert(n == 1);
    assert(out[0].kind == DELUGE_EV_PAD && out[0].id == 20 && out[0].value == 1);
    assert(out[0].x == 12 && out[0].y == 2 && out[0].pressure == 4095);

    /* Lift slot 20: TRACKING_ID -1, then SYN -> one release event. */
    struct input_event up[] = { ie(EV_ABS, ABS_MT_SLOT, 20), ie(EV_ABS, ABS_MT_TRACKING_ID, -1) };
    for (size_t i = 0; i < 2; i++) assert(deluge_pad_feed(&d, &up[i], out, 8) == 0);
    n = deluge_pad_feed(&d, &syn, out, 8);
    assert(n == 1 && out[0].id == 20 && out[0].value == 0);

    /* A SYN with no changes emits nothing. */
    assert(deluge_pad_feed(&d, &syn, out, 8) == 0);

    /* Multi-slot in one frame: two slots change -> two events (slot order). */
    {
        deluge_pad_decoder d2; deluge_pad_decoder_init(&d2);
        struct input_event ms[] = {
            ie(EV_ABS, ABS_MT_SLOT, 10), ie(EV_ABS, ABS_MT_TRACKING_ID, 10),
            ie(EV_ABS, ABS_MT_POSITION_X, 256), ie(EV_ABS, ABS_MT_POSITION_Y, 256),
            ie(EV_ABS, ABS_MT_SLOT, 20), ie(EV_ABS, ABS_MT_TRACKING_ID, 20),
            ie(EV_ABS, ABS_MT_POSITION_X, 512), ie(EV_ABS, ABS_MT_POSITION_Y, 512),
        };
        for (size_t i = 0; i < sizeof ms / sizeof ms[0]; i++)
            assert(deluge_pad_feed(&d2, &ms[i], out, 8) == 0);
        n = deluge_pad_feed(&d2, &syn, out, 8);
        assert(n == 2);
        assert(out[0].id == 10 && out[0].value == 1);
        assert(out[1].id == 20 && out[1].value == 1);
    }

    /* Position-only change on an already-down slot -> no event. */
    {
        deluge_pad_decoder d2; deluge_pad_decoder_init(&d2);
        struct input_event dn[] = { ie(EV_ABS, ABS_MT_SLOT, 7), ie(EV_ABS, ABS_MT_TRACKING_ID, 7) };
        for (size_t i = 0; i < 2; i++) deluge_pad_feed(&d2, &dn[i], out, 8);
        assert(deluge_pad_feed(&d2, &syn, out, 8) == 1);   /* the press */
        struct input_event mv = ie(EV_ABS, ABS_MT_POSITION_X, 999);
        deluge_pad_feed(&d2, &mv, out, 8);
        assert(deluge_pad_feed(&d2, &syn, out, 8) == 0);   /* position-only: nothing */
    }

    /* Same-frame press+release nets no change -> no event. */
    {
        deluge_pad_decoder d2; deluge_pad_decoder_init(&d2);
        struct input_event pr[] = {
            ie(EV_ABS, ABS_MT_SLOT, 3), ie(EV_ABS, ABS_MT_TRACKING_ID, 3),
            ie(EV_ABS, ABS_MT_TRACKING_ID, -1),
        };
        for (size_t i = 0; i < 3; i++) deluge_pad_feed(&d2, &pr[i], out, 8);
        assert(deluge_pad_feed(&d2, &syn, out, 8) == 0);
    }

    /* Re-press (new tracking id) on an already-down slot -> no spurious second press. */
    {
        deluge_pad_decoder d2; deluge_pad_decoder_init(&d2);
        struct input_event a[] = { ie(EV_ABS, ABS_MT_SLOT, 9), ie(EV_ABS, ABS_MT_TRACKING_ID, 9) };
        for (size_t i = 0; i < 2; i++) deluge_pad_feed(&d2, &a[i], out, 8);
        assert(deluge_pad_feed(&d2, &syn, out, 8) == 1);   /* first press */
        struct input_event b = ie(EV_ABS, ABS_MT_TRACKING_ID, 99); /* new id, still down, no lift */
        deluge_pad_feed(&d2, &b, out, 8);
        assert(deluge_pad_feed(&d2, &syn, out, 8) == 0);   /* still down: no second press */
    }
    return 0;
}

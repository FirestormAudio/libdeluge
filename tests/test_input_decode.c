/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "input_decode.h"
#include <assert.h>
#include <linux/input.h>
#include <string.h>

static struct input_event ie(uint16_t type, uint16_t code, int32_t val) {
    struct input_event e; memset(&e, 0, sizeof e);
    e.type = type; e.code = code; e.value = val; return e;
}

int main(void) {
    deluge_event ev;

    /* Button 5 press / release */
    struct input_event b = ie(EV_KEY, BTN_TRIGGER_HAPPY1 + 5, 1);
    assert(deluge_decode_button(&b, &ev) == 1);
    assert(ev.kind == DELUGE_EV_BUTTON && ev.id == 5 && ev.value == 1);
    assert(ev.x == 0 && ev.y == 0 && ev.pressure == 0);
    b = ie(EV_KEY, BTN_TRIGGER_HAPPY1 + 5, 0);
    assert(deluge_decode_button(&b, &ev) == 1 && ev.value == 0);
    /* Non-button EV_KEY (e.g. autorepeat value 2, or unrelated code) -> ignored */
    b = ie(EV_KEY, BTN_TRIGGER_HAPPY1 + 5, 2);
    assert(deluge_decode_button(&b, &ev) == 0);
    b = ie(EV_KEY, KEY_A, 1);
    assert(deluge_decode_button(&b, &ev) == 0);

    /* Encoders: each physical knob's kernel REL code decodes to its libdeluge
     * encoder id (SCROLL_X=0, TEMPO=1, MOD_0=2, MOD_1=3, SCROLL_Y=4,
     * SELECT=5). Signed delta is preserved. */
    struct input_event r = ie(EV_REL, REL_HWHEEL, -1); /* scroll_x -> SCROLL_X */
    assert(deluge_decode_encoder(&r, &ev) == 1);
    assert(ev.kind == DELUGE_EV_ENCODER && ev.id == 0 && ev.value == -1);
    r = ie(EV_REL, REL_MISC, 1); /* tempo -> TEMPO */
    assert(deluge_decode_encoder(&r, &ev) == 1 && ev.id == 1 && ev.value == 1);
    r = ie(EV_REL, REL_RY, -2); /* gold_top -> MOD_0 */
    assert(deluge_decode_encoder(&r, &ev) == 1 && ev.id == 2 && ev.value == -2);
    r = ie(EV_REL, REL_RX, 1); /* gold_bottom -> MOD_1 */
    assert(deluge_decode_encoder(&r, &ev) == 1 && ev.id == 3 && ev.value == 1);
    r = ie(EV_REL, REL_WHEEL, -1); /* scroll_y -> SCROLL_Y */
    assert(deluge_decode_encoder(&r, &ev) == 1 && ev.id == 4 && ev.value == -1);
    r = ie(EV_REL, REL_DIAL, 1); /* select -> SELECT */
    assert(deluge_decode_encoder(&r, &ev) == 1 && ev.id == 5 && ev.value == 1);
    r = ie(EV_REL, REL_MAX, 1); /* unmapped rel code -> ignored */
    assert(deluge_decode_encoder(&r, &ev) == 0);

    /* Clock: EV_MSC/MSC_RAW counter */
    struct input_event c = ie(EV_MSC, MSC_RAW, 42);
    assert(deluge_decode_clock(&c, &ev) == 1);
    assert(ev.kind == DELUGE_EV_CLOCK && ev.value == 42);
    c = ie(EV_MSC, MSC_SCAN, 1); /* wrong MSC code -> ignored */
    assert(deluge_decode_clock(&c, &ev) == 0);
    return 0;
}

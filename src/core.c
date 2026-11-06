/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "deluge/deluge.h"
#include "deluge/audio.h"
#include "deluge/midi.h"
#include "deluge/input.h"
#include "deluge/leds.h"
#include "deluge/display.h"
#include "deluge/cv.h"
#include "deluge/sram.h"
#include "deluge/usb.h"
#include "deluge/usb_midi.h"
#include "deluge/usb_audio.h"
#include "deluge/jacks.h"
#include "audio_priv.h"
#include "midi_priv.h"
#include "input_priv.h"
#include "leds_priv.h"
#include "display_priv.h"
#include "cv_priv.h"
#include "sram_priv.h"
#include "usb_priv.h"
#include "usb_midi_priv.h"
#include "usb_audio_priv.h"
#include "jacks_priv.h"
#include <stdio.h>
#include <stdlib.h>

struct deluge { deluge_audio_t *audio; deluge_midi_t *midi; deluge_input_t *input; deluge_leds_t *leds; deluge_display_t *display; deluge_cv_t *cv; deluge_sram_t *sram; deluge_usb_t *usb; deluge_usb_midi_t *usb_midi; deluge_usb_audio_t *usb_audio; deluge_jacks_t *jacks; };

deluge_audio_t *deluge_audio(deluge_t *d) { return d ? d->audio : NULL; }
deluge_midi_t *deluge_midi(deluge_t *d) { return d ? d->midi : NULL; }
deluge_input_t *deluge_input(deluge_t *d) { return d ? d->input : NULL; }
deluge_leds_t *deluge_leds(deluge_t *d) { return d ? d->leds : NULL; }
deluge_jacks_t *deluge_jacks(deluge_t *d) { return d ? d->jacks : NULL; }
deluge_display_t *deluge_display(deluge_t *d) { return d ? d->display : NULL; }
deluge_cv_t *deluge_cv(deluge_t *d) { return d ? d->cv : NULL; }
deluge_sram_t *deluge_sram(deluge_t *d) { return d ? d->sram : NULL; }
deluge_usb_t *deluge_usb(deluge_t *d) { return d ? d->usb : NULL; }
deluge_usb_midi_t *deluge_usb_midi(deluge_t *d) { return d ? d->usb_midi : NULL; }
deluge_usb_audio_t *deluge_usb_audio(deluge_t *d) { return d ? d->usb_audio : NULL; }

const char *deluge_strerror(int code) {
    switch (code) {
        case DELUGE_OK:         return "ok";
        case DELUGE_ERR_NODEV:  return "device not found";
        case DELUGE_ERR_IO:     return "i/o error";
        case DELUGE_ERR_FORMAT: return "unsupported format";
        case DELUGE_ERR_STATE:  return "invalid state";
        case DELUGE_ERR_NOMEM:  return "out of memory";
        case DELUGE_ERR_INVAL:  return "invalid argument";
        default:                return "unknown error";
    }
}

deluge_t *deluge_open(void) {
    deluge_t *d = calloc(1, sizeof(*d));
    if (!d) return NULL;
    d->audio = deluge_audio_new();
    if (!d->audio) { fprintf(stderr, "deluge: audio init failed\n"); free(d); return NULL; }
    d->midi = deluge_midi_new();       /* NULL if rawmidi is absent — MIDI is then unavailable, not fatal */
    d->input = deluge_input_new();     /* NULL if input nodes absent — non-fatal */
    d->leds = deluge_leds_new();       /* NULL if no deluge:* LED-class devices — non-fatal */
    d->display = deluge_display_new(); /* NULL if neither OLED nor pads fbdev present — non-fatal */
    d->cv = deluge_cv_new();           /* NULL if neither CV nor gates available — non-fatal */
    d->sram = deluge_sram_new();       /* NULL if /dev/deluge-sram absent — non-fatal */
    d->usb = deluge_usb_new();         /* NULL only if neither the role sysfs nor the uevent netlink socket is available — non-fatal */
    d->usb_midi = deluge_usb_midi_new(); /* NULL if no f_midi2 gadget card — non-fatal */
    d->usb_audio = deluge_usb_audio_new(); /* NULL if no UAC2_Gadget card — non-fatal */
    d->jacks = deluge_jacks_new();     /* NULL if the card has no jack kcontrols — non-fatal */
    return d;
}

void deluge_close(deluge_t *d) {
    if (!d) return;
    deluge_audio_free(d->audio);
    deluge_midi_free(d->midi);
    deluge_input_free(d->input);
    deluge_leds_free(d->leds);
    deluge_display_free(d->display);
    deluge_cv_free(d->cv);
    deluge_sram_free(d->sram);
    deluge_usb_free(d->usb);
    deluge_usb_midi_free(d->usb_midi);
    deluge_usb_audio_free(d->usb_audio);
    deluge_jacks_free(d->jacks);
    free(d);
}

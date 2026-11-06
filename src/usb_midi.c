/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "deluge/usb_midi.h"
#include "usb_midi_priv.h"
#include "discover.h"
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>

struct deluge_usb_midi { snd_rawmidi_t *in, *out; };

/* Active-role MIDI card: the gadget f_midi2 card (peripheral) or a plugged
   USB-MIDI device's card (host). The internal codec is driver "Deluge". */
static const char *const MIDI_DRIVERS[] = { "f_midi2", "USB-Audio" };

static void close_midi(struct deluge_usb_midi *m) {
    if (m->in)  { snd_rawmidi_close(m->in);  m->in = NULL; }
    if (m->out) { snd_rawmidi_close(m->out); m->out = NULL; }
}

/* (Re)discover the active-role MIDI card and open its legacy byte-stream rawmidi.
   f_midi2 exposes a UMP device (device 0, not openable via the legacy API) and a
   legacy device (device 1); a plugged USB-MIDI device exposes a legacy device
   directly. Enumerate and open the first the legacy API accepts. */
static int bind_midi(struct deluge_usb_midi *m) {
    close_midi(m);
    char id[32];
    if (deluge_find_card_by_drivers(MIDI_DRIVERS, 2, id, sizeof id) != DELUGE_OK)
        return DELUGE_ERR_NODEV; /* no gadget or plugged MIDI card */
    char ctlname[48];
    snprintf(ctlname, sizeof ctlname, "hw:CARD=%s", id);
    snd_ctl_t *ctl;
    if (snd_ctl_open(&ctl, ctlname, 0) < 0) return DELUGE_ERR_NODEV;
    int dev = -1;
    while (snd_ctl_rawmidi_next_device(ctl, &dev) == 0 && dev >= 0) {
        char hw[64];
        snprintf(hw, sizeof hw, "hw:CARD=%s,%d", id, dev);
        if (snd_rawmidi_open(&m->in, &m->out, hw, SND_RAWMIDI_NONBLOCK) == 0) {
            snd_ctl_close(ctl);
            return DELUGE_OK;
        }
    }
    snd_ctl_close(ctl);
    return DELUGE_ERR_NODEV;
}

deluge_usb_midi_t *deluge_usb_midi_new(void) {
    deluge_usb_midi_t *m = calloc(1, sizeof *m);
    if (!m) return NULL;
    bind_midi(m);   /* unbound is fine — rebind() can bind later (hotplug/role flip) */
    return m;
}

int deluge_usb_midi_rebind(deluge_usb_midi_t *m) {
    if (!m) return DELUGE_ERR_STATE;
    return bind_midi(m);
}

void deluge_usb_midi_free(deluge_usb_midi_t *m) {
    if (!m) return;
    close_midi(m);
    free(m);
}

int deluge_usb_midi_write(deluge_usb_midi_t *m, const uint8_t *buf, size_t n) {
    if (!m) return DELUGE_ERR_STATE;
    if (!m->out) return DELUGE_ERR_NODEV;   /* unbound */
    ssize_t w = snd_rawmidi_write(m->out, buf, n);
    return w < 0 ? DELUGE_ERR_IO : (int)w;
}

int deluge_usb_midi_read(deluge_usb_midi_t *m, uint8_t *buf, size_t n) {
    if (!m) return DELUGE_ERR_STATE;
    if (!m->in) return DELUGE_ERR_NODEV;    /* unbound */
    ssize_t r = snd_rawmidi_read(m->in, buf, n);
    if (r == -EAGAIN) return 0;
    return r < 0 ? DELUGE_ERR_IO : (int)r;
}

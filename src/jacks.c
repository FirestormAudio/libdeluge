/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "deluge/jacks.h"
#include "jacks_priv.h"
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>

/* Jack state is exposed by the deluge-audio machine driver as read-only
 * kcontrols on the card (SNDRV_CTL_ELEM_IFACE_CARD), alongside the writable
 * "Speaker Playback Switch". Using the control interface rather than a private
 * sysfs path keeps the whole audio-jack domain behind one mechanism and makes
 * the state visible to plain `amixer`. */

#define SPEAKER_CTL "Speaker Playback Switch"

struct deluge_jacks {
    snd_ctl_t *ctl;
};

/* Control names, indexed by `enum deluge_jack`. Must match the names the
 * deluge-audio driver registers. */
static const char *jack_ctl_name(enum deluge_jack j) {
    switch (j) {
    case DELUGE_JACK_HEADPHONE:  return "Headphone Jack";
    case DELUGE_JACK_LINE_IN:    return "Line In Jack";
    case DELUGE_JACK_MIC:        return "Mic Jack";
    case DELUGE_JACK_LINE_OUT_L: return "Line Out L Jack";
    case DELUGE_JACK_LINE_OUT_R: return "Line Out R Jack";
    }
    return NULL;
}

/* Read a boolean control by name. Returns 0/1, or a negative DELUGE_ERR_*. */
static int read_bool_ctl(snd_ctl_t *ctl, const char *name) {
    snd_ctl_elem_id_t *id;
    snd_ctl_elem_value_t *val;
    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_value_alloca(&val);

    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_CARD);
    snd_ctl_elem_id_set_name(id, name);
    snd_ctl_elem_value_set_id(val, id);

    if (snd_ctl_elem_read(ctl, val) < 0) return DELUGE_ERR_NODEV;
    return snd_ctl_elem_value_get_boolean(val, 0) ? 1 : 0;
}

static int write_bool_ctl(snd_ctl_t *ctl, const char *name, int on) {
    snd_ctl_elem_id_t *id;
    snd_ctl_elem_value_t *val;
    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_value_alloca(&val);

    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_CARD);
    snd_ctl_elem_id_set_name(id, name);
    snd_ctl_elem_value_set_id(val, id);

    /* Read first so the element id is fully resolved (numid filled in) before
       writing — writing a name-only id works, but reading back also confirms
       the control exists and gives a clean NODEV instead of an opaque EIO. */
    if (snd_ctl_elem_read(ctl, val) < 0) return DELUGE_ERR_NODEV;
    snd_ctl_elem_value_set_boolean(val, 0, on ? 1 : 0);
    if (snd_ctl_elem_write(ctl, val) < 0) return DELUGE_ERR_IO;
    return DELUGE_OK;
}

deluge_jacks_t *deluge_jacks_new(void) {
    snd_ctl_t *ctl;
    if (snd_ctl_open(&ctl, "hw:CARD=Deluge", 0) < 0) {
        fprintf(stderr, "deluge: jack controls unavailable (no hw:CARD=Deluge)\n");
        return NULL;
    }
    /* Probe one jack control: a card without them is a kernel too old for the
       jack kcontrols, and reporting that at open time beats every later get()
       failing individually with no explanation. */
    if (read_bool_ctl(ctl, jack_ctl_name(DELUGE_JACK_HEADPHONE)) < 0) {
        fprintf(stderr, "deluge: sound card has no jack controls "
                        "(kernel without deluge-audio jack kcontrols?)\n");
        snd_ctl_close(ctl);
        return NULL;
    }
    deluge_jacks_t *j = calloc(1, sizeof *j);
    if (!j) { snd_ctl_close(ctl); return NULL; }
    j->ctl = ctl;
    return j;
}

void deluge_jacks_free(deluge_jacks_t *j) {
    if (!j) return;
    if (j->ctl) snd_ctl_close(j->ctl);
    free(j);
}

int deluge_jacks_get(deluge_jacks_t *j, enum deluge_jack jack) {
    if (!j || !j->ctl) return DELUGE_ERR_STATE;
    const char *name = jack_ctl_name(jack);
    if (!name) return DELUGE_ERR_INVAL;
    return read_bool_ctl(j->ctl, name);
}

int deluge_jacks_set_speaker(deluge_jacks_t *j, int on) {
    if (!j || !j->ctl) return DELUGE_ERR_STATE;
    return write_bool_ctl(j->ctl, SPEAKER_CTL, on);
}

int deluge_jacks_get_speaker(deluge_jacks_t *j) {
    if (!j || !j->ctl) return DELUGE_ERR_STATE;
    return read_bool_ctl(j->ctl, SPEAKER_CTL);
}

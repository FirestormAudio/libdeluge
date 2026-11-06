/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/* USB audio card discovery and probing. Streaming is not implemented in
 * 0.1.x: this file finds the USB audio card, holds its id, and offers a
 * read-only probe. See include/deluge/usb_audio.h. */
#include "deluge/usb_audio.h"
#include "usb_audio_priv.h"
#include "discover.h"
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <string.h>

struct deluge_usb_audio { char card_id[32]; };

/* Active-role audio card: the gadget UAC2 card (peripheral) or a plugged USB
   audio interface (host). The internal codec is driver "Deluge", so no collision. */
static const char *const AUDIO_DRIVERS[] = { "UAC2_Gadget", "USB-Audio" };

/* (Re)discover the audio card; store its id, or clear to unbound. */
static int bind_audio(struct deluge_usb_audio *a) {
    char id[32];
    if (deluge_find_card_by_drivers(AUDIO_DRIVERS, 2, id, sizeof id) != DELUGE_OK) {
        a->card_id[0] = 0;
        return DELUGE_ERR_NODEV;
    }
    snprintf(a->card_id, sizeof a->card_id, "%s", id);
    return DELUGE_OK;
}

deluge_usb_audio_t *deluge_usb_audio_new(void) {
    deluge_usb_audio_t *a = calloc(1, sizeof *a);
    if (!a) return NULL;
    bind_audio(a);   /* unbound is fine — rebind() can bind later (hotplug/role flip) */
    return a;
}

int deluge_usb_audio_rebind(deluge_usb_audio_t *a) {
    if (!a) return DELUGE_ERR_STATE;
    return bind_audio(a);
}

void deluge_usb_audio_free(deluge_usb_audio_t *a) { free(a); }

const char *deluge_usb_audio_card_id(deluge_usb_audio_t *a) {
    return (a && a->card_id[0]) ? a->card_id : NULL;
}

/* Open one direction, fill channels/rate/formats from hw_params_any, close. */
static int probe_dir(const char *hw, snd_pcm_stream_t dir,
                     unsigned int *channels, unsigned int *rmin,
                     unsigned int *rmax, int *formats) {
    snd_pcm_t *pcm;
    if (snd_pcm_open(&pcm, hw, dir, SND_PCM_NONBLOCK) < 0) {
        *channels = 0; /* direction absent is not an error */
        return DELUGE_OK;
    }
    snd_pcm_hw_params_t *hp;
    snd_pcm_hw_params_alloca(&hp);
    if (snd_pcm_hw_params_any(pcm, hp) < 0) { snd_pcm_close(pcm); return DELUGE_ERR_IO; }
    unsigned int cmax = 0, lo = 0, hi = 0;
    snd_pcm_hw_params_get_channels_max(hp, &cmax);
    snd_pcm_hw_params_get_rate_min(hp, &lo, NULL);
    snd_pcm_hw_params_get_rate_max(hp, &hi, NULL);
    *channels = cmax;
    /* Union (any-direction) rate envelope: min of the minima, max of the
       maxima across the present directions. */
    if (*rmin == 0 || lo < *rmin) *rmin = lo;
    if (hi > *rmax) *rmax = hi;
    if (snd_pcm_hw_params_test_format(pcm, hp, SND_PCM_FORMAT_S16_LE) == 0) *formats |= DELUGE_USB_AUDIO_FMT_S16;
    if (snd_pcm_hw_params_test_format(pcm, hp, SND_PCM_FORMAT_S24_LE) == 0) *formats |= DELUGE_USB_AUDIO_FMT_S24;
    if (snd_pcm_hw_params_test_format(pcm, hp, SND_PCM_FORMAT_S32_LE) == 0) *formats |= DELUGE_USB_AUDIO_FMT_S32;
    snd_pcm_close(pcm);
    return DELUGE_OK;
}

int deluge_usb_audio_probe(deluge_usb_audio_t *a, deluge_usb_audio_info_t *out) {
    if (!a) return DELUGE_ERR_STATE;
    if (!out) return DELUGE_ERR_INVAL;
    if (!a->card_id[0]) return DELUGE_ERR_NODEV;   /* unbound: no gadget/USB-Audio card */
    memset(out, 0, sizeof *out);
    char hw[48];
    snprintf(hw, sizeof hw, "hw:CARD=%s", a->card_id);
    int rc = probe_dir(hw, SND_PCM_STREAM_PLAYBACK, &out->playback_channels,
                       &out->rate_min, &out->rate_max, &out->formats);
    if (rc) return rc;
    return probe_dir(hw, SND_PCM_STREAM_CAPTURE, &out->capture_channels,
                     &out->rate_min, &out->rate_max, &out->formats);
}

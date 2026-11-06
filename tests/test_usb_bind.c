/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "deluge/usb_audio.h"
#include "usb_audio_priv.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef FIXDIR
#define FIXDIR "tests/fixtures/asound"  /* CMake overrides with an absolute path */
#endif

/* deluge_usb_audio is persistent + rebindable; rebind is discovery-only (no PCM
   open), so it is fully host-testable via DELUGE_ASOUND_CARDS fixtures. */
int main(void) {
    /* unbound at open (device-side, nothing plugged: only the internal codec) */
    setenv("DELUGE_ASOUND_CARDS", FIXDIR "/internal-only", 1);
    deluge_usb_audio_t *a = deluge_usb_audio_new();
    assert(a != NULL);                                  /* persistent handle */
    assert(deluge_usb_audio_card_id(a) == NULL);        /* unbound */
    deluge_usb_audio_info_t info;
    assert(deluge_usb_audio_probe(a, &info) == DELUGE_ERR_NODEV);
    assert(deluge_usb_audio_rebind(a) == DELUGE_ERR_NODEV); /* still nothing */

    /* a gadget card appears -> rebind binds it */
    setenv("DELUGE_ASOUND_CARDS", FIXDIR "/uac2-gadget", 1);
    assert(deluge_usb_audio_rebind(a) == DELUGE_OK);
    assert(deluge_usb_audio_card_id(a) && strcmp(deluge_usb_audio_card_id(a), "UAC2Gadget") == 0);

    /* flip to a plugged host USB-Audio card -> rebind re-points */
    setenv("DELUGE_ASOUND_CARDS", FIXDIR "/usb-audio-host", 1);
    assert(deluge_usb_audio_rebind(a) == DELUGE_OK);
    assert(deluge_usb_audio_card_id(a) && strcmp(deluge_usb_audio_card_id(a), "Device") == 0);

    /* device removed -> rebind reports unbound, card_id NULL again */
    setenv("DELUGE_ASOUND_CARDS", FIXDIR "/internal-only", 1);
    assert(deluge_usb_audio_rebind(a) == DELUGE_ERR_NODEV);
    assert(deluge_usb_audio_card_id(a) == NULL);

    deluge_usb_audio_free(a);
    assert(deluge_usb_audio_rebind(NULL) == DELUGE_ERR_STATE);
    return 0;
}

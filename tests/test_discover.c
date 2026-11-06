/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "discover.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef FIXDIR
#define FIXDIR "tests/fixtures/asound"  /* CMake overrides with an absolute path */
#endif

int main(void) {
    char path[64];
    int rc = deluge_find_input_event("Deluge Pads", path, sizeof path);
    assert(rc == DELUGE_OK);
    assert(strcmp(path, "/dev/input/event7") == 0);
    rc = deluge_find_input_event("No Such Device", path, sizeof path);
    assert(rc == DELUGE_ERR_NODEV);

    /* card-by-driver: DELUGE_ASOUND_CARDS points at tests/fixtures/asound/cards */
    char id[32];
    rc = deluge_find_card_by_driver("UAC2_Gadget", id, sizeof id);
    assert(rc == DELUGE_OK && strcmp(id, "UAC2Gadget") == 0);

    rc = deluge_find_card_by_driver("f_midi2", id, sizeof id);
    assert(rc == DELUGE_OK && strcmp(id, "MIDI20Gadget") == 0);

    /* driver token containing a dash still parses, and id trailing pad is trimmed */
    rc = deluge_find_card_by_driver("USB-Audio", id, sizeof id);
    assert(rc == DELUGE_OK && strcmp(id, "Deluge") == 0);

    rc = deluge_find_card_by_driver("nonesuch", id, sizeof id);
    assert(rc == DELUGE_ERR_NODEV);

    rc = deluge_find_card_by_driver(NULL, id, sizeof id);
    assert(rc == DELUGE_ERR_INVAL);

    /* missing cards file -> NODEV (not a crash) */
    setenv("DELUGE_ASOUND_CARDS", "/nonexistent/cards", 1);
    rc = deluge_find_card_by_driver("f_midi2", id, sizeof id);
    assert(rc == DELUGE_ERR_NODEV);

    /* driver-order: try gadget driver then USB-Audio, first present wins */
    const char *audio_drivers[] = { "UAC2_Gadget", "USB-Audio" };
    const char *midi_drivers[]  = { "f_midi2", "USB-Audio" };

    setenv("DELUGE_ASOUND_CARDS", FIXDIR "/uac2-gadget", 1);
    rc = deluge_find_card_by_drivers(audio_drivers, 2, id, sizeof id);
    assert(rc == DELUGE_OK && strcmp(id, "UAC2Gadget") == 0);   /* gadget wins */
    rc = deluge_find_card_by_drivers(midi_drivers, 2, id, sizeof id);
    assert(rc == DELUGE_OK && strcmp(id, "Gadget") == 0);       /* f_midi2 card id */

    setenv("DELUGE_ASOUND_CARDS", FIXDIR "/usb-audio-host", 1);
    rc = deluge_find_card_by_drivers(audio_drivers, 2, id, sizeof id);
    assert(rc == DELUGE_OK && strcmp(id, "Device") == 0);       /* falls through to USB-Audio */
    rc = deluge_find_card_by_drivers(midi_drivers, 2, id, sizeof id);
    assert(rc == DELUGE_OK && strcmp(id, "Device") == 0);

    setenv("DELUGE_ASOUND_CARDS", FIXDIR "/internal-only", 1);
    rc = deluge_find_card_by_drivers(audio_drivers, 2, id, sizeof id);
    assert(rc == DELUGE_ERR_NODEV);                             /* neither present */

    rc = deluge_find_card_by_drivers(NULL, 2, id, sizeof id);
    assert(rc == DELUGE_ERR_INVAL);
    return 0;
}

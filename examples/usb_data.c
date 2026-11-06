/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/* usb_data: report whether the USB MIDI and USB audio handles exist and probe
   the USB audio card's channels, rates and formats. Run with no arguments. */
#include "deluge/deluge.h"
#include "deluge/usb_midi.h"
#include "deluge/usb_audio.h"
#include <stdio.h>

int main(void) {
    deluge_t *d = deluge_open();
    if (!d) { fprintf(stderr, "open failed\n"); return 1; }
    /* The handles always exist; whether a device is bound is what varies. */
    printf("usb midi:  %s\n",
           deluge_usb_midi_rebind(deluge_usb_midi(d)) == DELUGE_OK ? "present" : "absent");
    deluge_usb_audio_t *a = deluge_usb_audio(d);
    if (!deluge_usb_audio_card_id(a)) {
        printf("usb audio: absent\n");
    } else {
        deluge_usb_audio_info_t info;
        if (deluge_usb_audio_probe(a, &info) == DELUGE_OK)
            printf("usb audio: card %s  play=%uch cap=%uch  rate=%u..%u  fmt=0x%x\n",
                   deluge_usb_audio_card_id(a), info.playback_channels,
                   info.capture_channels, info.rate_min, info.rate_max, info.formats);
        else
            printf("usb audio: card %s (probe failed)\n", deluge_usb_audio_card_id(a));
    }
    deluge_close(d);
    return 0;
}

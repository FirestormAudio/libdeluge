/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "deluge/deluge.h"
#include "deluge/audio.h"
#include "deluge/midi.h"
#include "deluge/leds.h"
#include "deluge/display.h"
#include "deluge/cv.h"
#include "deluge/sram.h"
#include "deluge/usb.h"
#include "deluge/usb_midi.h"
#include "deluge/usb_audio.h"
#include <assert.h>
#include <stddef.h>

/* On a host with no "Deluge" ALSA card: open must still succeed, the audio
   handle must exist (opened lazily, no device touched at open), and the midi
   handle must be NULL (rawmidi absent) — graceful per-capability degradation.
   leds/display are exercised too: no deluge:* sysfs / deluge-* fbdev on this
   host, so both accessors must return NULL (and, incidentally, this is what
   link-tests the public deluge_leds/deluge_display accessors). cv/sram are
   exercised the same way: no deluge-cv iio/gpio or /dev/deluge-sram on this
   host, so both accessors must return NULL (link-tests deluge_cv/deluge_sram).
   usb: test_open does not set DELUGE_USB_ROLE_PATH, and ctest runs each test
   in its own process, so the default role sysfs path is absent on this host
   -> role_fd unavailable. But deluge_usb_new also opens an uevent netlink
   socket, which an unprivileged process CAN bind on this host, so the
   deluge_usb handle is still non-NULL (link-tests deluge_usb/deluge_usb_fd). */
int main(void) {
    deluge_t *d = deluge_open();
    assert(d != NULL);                 /* not fatal despite missing MIDI device */
    assert(deluge_audio(d) != NULL);   /* audio handle always present */
    assert(deluge_midi(d) == NULL);    /* MIDI unavailable -> NULL, not a crash */
    assert(deluge_leds(d) == NULL);    /* no deluge:* sysfs on host */
    assert(deluge_display(d) == NULL); /* no deluge-* fbdev on host */
    assert(deluge_cv(d) == NULL);      /* no deluge-cv iio / gpio on host */
    assert(deluge_sram(d) == NULL);    /* no /dev/deluge-sram on host */
    assert(deluge_usb(d) != NULL);       /* deluge_usb is now available via the uevent netlink socket (needs AF_NETLINK; may be NULL in a sandbox that blocks netlink) */
    assert(deluge_usb_fd(deluge_usb(d)) >= 0); /* netlink fd present (same AF_NETLINK caveat as above) */
    assert(deluge_usb_midi(d) != NULL);                           /* persistent handle now (USB-4b) */
    { uint8_t b[4]; assert(deluge_usb_midi_read(deluge_usb_midi(d), b, sizeof b) == DELUGE_ERR_NODEV); } /* unbound */
    assert(deluge_usb_audio(d) != NULL);                          /* persistent handle now (USB-4b) */
    assert(deluge_usb_audio_card_id(deluge_usb_audio(d)) == NULL); /* unbound: no gadget/USB-Audio card */
    deluge_close(d);
    return 0;
}

/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef DELUGE_USB_MIDI_PRIV_H
#define DELUGE_USB_MIDI_PRIV_H
#include "deluge/usb_midi.h"
deluge_usb_midi_t *deluge_usb_midi_new(void); /* NULL if no f_midi2 gadget card */
void              deluge_usb_midi_free(deluge_usb_midi_t *);
#endif

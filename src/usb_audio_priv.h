/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef DELUGE_USB_AUDIO_PRIV_H
#define DELUGE_USB_AUDIO_PRIV_H
#include "deluge/usb_audio.h"
deluge_usb_audio_t *deluge_usb_audio_new(void); /* NULL if no UAC2_Gadget card */
void               deluge_usb_audio_free(deluge_usb_audio_t *);
#endif

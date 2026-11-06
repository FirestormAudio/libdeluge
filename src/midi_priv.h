/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef DELUGE_MIDI_PRIV_H
#define DELUGE_MIDI_PRIV_H
#include "deluge/midi.h"
deluge_midi_t *deluge_midi_new(void);
void           deluge_midi_free(deluge_midi_t *);
#endif

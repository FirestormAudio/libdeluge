/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef DELUGE_AUDIO_PRIV_H
#define DELUGE_AUDIO_PRIV_H
#include "deluge/audio.h"
deluge_audio_t *deluge_audio_new(void);
void            deluge_audio_free(deluge_audio_t *);
#endif

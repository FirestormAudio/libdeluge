/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef DELUGE_LEDS_PRIV_H
#define DELUGE_LEDS_PRIV_H
#include "deluge/leds.h"
deluge_leds_t *deluge_leds_new(void); /* NULL if no LED files could be opened */
void           deluge_leds_free(deluge_leds_t *);
#endif

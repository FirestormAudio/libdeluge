/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef DELUGE_DISPLAY_PRIV_H
#define DELUGE_DISPLAY_PRIV_H
#include "deluge/display.h"
deluge_display_t *deluge_display_new(void); /* NULL if neither fb present */
void              deluge_display_free(deluge_display_t *);
#endif

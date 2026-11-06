/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef DELUGE_JACKS_PRIV_H
#define DELUGE_JACKS_PRIV_H
#include "deluge/jacks.h"
deluge_jacks_t *deluge_jacks_new(void); /* NULL if the card has no jack controls */
void            deluge_jacks_free(deluge_jacks_t *);
#endif

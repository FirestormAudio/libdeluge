/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef DELUGE_INPUT_PRIV_H
#define DELUGE_INPUT_PRIV_H
#include "deluge/input.h"
deluge_input_t *deluge_input_new(void); /* NULL if no input nodes could be opened */
void            deluge_input_free(deluge_input_t *);
#endif

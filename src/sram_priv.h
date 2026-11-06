/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef DELUGE_SRAM_PRIV_H
#define DELUGE_SRAM_PRIV_H
#include "deluge/sram.h"
deluge_sram_t *deluge_sram_new(void); /* NULL if /dev/deluge-sram absent */
void           deluge_sram_free(deluge_sram_t *);
#endif

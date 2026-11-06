/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/**
 * @file sram.h
 * @brief Fast on-chip SRAM as cached scratch memory.
 *
 * @c /dev/deluge-sram exposes a 1 MB window of the RZ/A1L's on-chip SRAM.
 * It is faster to reach than SDRAM, so it suits hot DSP data such as lookup
 * tables and delay lines.
 */
#ifndef DELUGE_SRAM_H
#define DELUGE_SRAM_H
#include "deluge/deluge.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/** @brief SRAM capability handle. */
typedef struct deluge_sram deluge_sram_t;

/**
 * @brief Get the SRAM capability.
 *
 * @param d Device handle. May be NULL.
 * @return The handle (owned by @p d, valid until deluge_close()), or NULL if
 *         @p d is NULL or @c /dev/deluge-sram could not be opened and mapped.
 */
deluge_sram_t *deluge_sram(deluge_t *d);

/**
 * @brief Get the SRAM mapping.
 *
 * libdeluge maps the whole region once at open, so this call does no I/O and
 * always returns the same pointer.
 *
 * @param s       SRAM handle.
 * @param out_len If not NULL, receives the mapped size in bytes (0 if @p s is
 *                NULL).
 * @return The mapping (cached, read/write), or NULL if @p s is NULL. It stays
 *         valid until deluge_close(); do not @c munmap it.
 */
void *deluge_sram_map(deluge_sram_t *s, size_t *out_len);

#ifdef __cplusplus
}
#endif
#endif

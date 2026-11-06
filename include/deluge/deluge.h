/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/**
 * @file deluge.h
 * @brief Device handle: open and close the Deluge.
 *
 * Every other header hangs a capability off the ::deluge_t returned by
 * deluge_open(). See docs/api.md for the lifecycle, error and threading
 * conventions shared by the whole API.
 */
#ifndef DELUGE_H
#define DELUGE_H
#include "deluge/error.h"
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle to the device. One per process is the expected use. */
typedef struct deluge deluge_t;

/**
 * @brief Open the Deluge and every capability that is present.
 *
 * Each capability (audio, MIDI, input, display, ...) is opened independently.
 * A device node that cannot be opened is reported on stderr and leaves the
 * matching accessor (deluge_midi(), deluge_input(), ...) returning NULL; the
 * rest of the handle still works.
 *
 * @return A new handle, or NULL if allocating the handle or the audio
 *         capability fails.
 */
deluge_t *deluge_open(void);

/**
 * @brief Close the device and free the handle.
 *
 * Stops every library-owned thread (audio, input, MIDI, USB hotplug), closes
 * all descriptors and unmaps all mappings. Every capability handle and mapping
 * obtained from @p d becomes invalid.
 *
 * @param d Handle from deluge_open(). NULL is a no-op.
 * @note Do not call this from a libdeluge callback.
 */
void      deluge_close(deluge_t *d);

#ifdef __cplusplus
}
#endif
#endif

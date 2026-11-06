/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/**
 * @file usb_midi.h
 * @brief USB MIDI as a raw MIDI 1.0 byte stream.
 *
 * The bound device is the USB MIDI gadget (@c f_midi2) in peripheral role, or
 * a plugged-in USB MIDI device (@c USB-Audio) in host role. libdeluge opens
 * its legacy rawmidi byte stream. Native UMP is not supported yet. This is
 * separate from the DIN port in midi.h.
 */
#ifndef DELUGE_USB_MIDI_H
#define DELUGE_USB_MIDI_H
#include "deluge/deluge.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/** @brief USB MIDI capability handle. */
typedef struct deluge_usb_midi deluge_usb_midi_t;

/**
 * @brief Get the USB MIDI capability.
 *
 * The handle exists whether or not a device is bound, so it survives role
 * switches and hotplug; call deluge_usb_midi_rebind() to pick up a device that
 * appears later.
 *
 * @param d Device handle. May be NULL.
 * @return The handle (owned by @p d, valid until deluge_close()), or NULL if
 *         @p d is NULL or allocation failed.
 */
deluge_usb_midi_t *deluge_usb_midi(deluge_t *d);

/**
 * @brief Send MIDI bytes.
 *
 * @param m   USB MIDI handle.
 * @param buf Bytes to send.
 * @param n   Number of bytes.
 * @return The number of bytes accepted; ::DELUGE_ERR_STATE if @p m is NULL;
 *         ::DELUGE_ERR_NODEV if no device is bound; ::DELUGE_ERR_IO if the
 *         write fails (including when the output buffer is full).
 */
int deluge_usb_midi_write(deluge_usb_midi_t *m, const uint8_t *buf, size_t n);

/**
 * @brief Read received MIDI bytes without blocking.
 *
 * @param m   USB MIDI handle.
 * @param buf Buffer to fill.
 * @param n   Capacity of @p buf.
 * @return The number of bytes read (0 if none are pending);
 *         ::DELUGE_ERR_STATE if @p m is NULL; ::DELUGE_ERR_NODEV if no device
 *         is bound; ::DELUGE_ERR_IO if the read fails.
 */
int deluge_usb_midi_read (deluge_usb_midi_t *m, uint8_t *buf, size_t n);

/**
 * @brief Find the USB MIDI device again and open it.
 *
 * Closes the current device, if any, then binds to the first matching card
 * whose rawmidi byte stream can be opened. Call after a hotplug event or a role
 * switch.
 *
 * @param m USB MIDI handle.
 * @return ::DELUGE_OK if a device is now bound; ::DELUGE_ERR_NODEV if none was
 *         found (the handle is then unbound); ::DELUGE_ERR_STATE if @p m is
 *         NULL.
 * @note Not safe to call while another thread is reading or writing @p m.
 */
int deluge_usb_midi_rebind(deluge_usb_midi_t *m);

#ifdef __cplusplus
}
#endif
#endif

/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/**
 * @file midi.h
 * @brief 5-pin DIN MIDI as a raw byte stream.
 *
 * Bytes are passed through unparsed; running status, SysEx and real-time
 * messages arrive exactly as they came off the wire. Read either by polling
 * deluge_midi_read() or by starting a reader thread with deluge_midi_start(),
 * not both at once. For USB MIDI, see usb_midi.h.
 */
#ifndef DELUGE_MIDI_H
#define DELUGE_MIDI_H
#include "deluge/deluge.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/** @brief DIN MIDI capability handle. */
typedef struct deluge_midi deluge_midi_t;

/**
 * @brief Get the DIN MIDI capability.
 *
 * @param d Device handle. May be NULL.
 * @return The handle (owned by @p d, valid until deluge_close()), or NULL if
 *         @p d is NULL or the rawmidi device on @c hw:CARD=Deluge could not be
 *         opened.
 */
deluge_midi_t *deluge_midi(deluge_t *d);

/**
 * @brief Send MIDI bytes.
 *
 * @param m   MIDI handle.
 * @param buf Bytes to send.
 * @param n   Number of bytes.
 * @return The number of bytes accepted; ::DELUGE_ERR_STATE if @p m is NULL;
 *         ::DELUGE_ERR_IO if the write fails (including when the output
 *         buffer is full).
 */
int deluge_midi_write(deluge_midi_t *m, const uint8_t *buf, size_t n);

/**
 * @brief Read received MIDI bytes without blocking.
 *
 * @param m   MIDI handle.
 * @param buf Buffer to fill.
 * @param n   Capacity of @p buf.
 * @return The number of bytes read (0 if none are pending);
 *         ::DELUGE_ERR_STATE if @p m is NULL; ::DELUGE_ERR_IO if the read
 *         fails.
 */
int deluge_midi_read (deluge_midi_t *m, uint8_t *buf, size_t n);

/**
 * @brief MIDI receive callback, called on the reader thread.
 *
 * @param data Received bytes. Valid only for this call.
 * @param n    Number of bytes.
 * @param ctx  The pointer passed to deluge_midi_start().
 *
 * @note Keep it short and non-blocking: copy the bytes into a queue and
 *       signal whoever consumes them. A slow callback delays the next read.
 */
typedef void (*deluge_midi_cb)(const uint8_t *data, size_t n, void *ctx);

/**
 * @brief Start a reader thread that delivers received bytes to @p cb.
 *
 * Use this instead of polling deluge_midi_read() when you want to wait for
 * MIDI without sleeping and retrying. The thread blocks until bytes arrive and
 * exits on its own if the device goes away.
 *
 * @param m   MIDI handle.
 * @param cb  Callback. Must not be NULL.
 * @param ctx Passed to every @p cb call.
 * @return ::DELUGE_OK; ::DELUGE_ERR_INVAL if @p m or @p cb is NULL;
 *         ::DELUGE_ERR_STATE if already started; ::DELUGE_ERR_IO if creating
 *         the wake pipe or the thread fails.
 */
int  deluge_midi_start(deluge_midi_t *m, deluge_midi_cb cb, void *ctx);

/**
 * @brief Stop the reader thread.
 *
 * Wakes the thread and blocks until it has exited; @p cb is not called after
 * this returns. deluge_close() stops the thread before it closes the device,
 * so you do not need to call this before closing.
 *
 * @param m MIDI handle. NULL, or a handle that is not started, is a no-op.
 * @note Do not call this from inside the callback.
 */
void deluge_midi_stop (deluge_midi_t *m);

#ifdef __cplusplus
}
#endif
#endif

/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/**
 * @file input.h
 * @brief Pads, buttons, encoders and clock input as one event stream.
 *
 * Two delivery models; use one or the other, not both at once:
 * - Callback: deluge_input_start() runs a library-owned thread that calls your
 *   callback once per event.
 * - Your own loop: poll deluge_input_fd() and drain it with
 *   deluge_input_read().
 */
#ifndef DELUGE_INPUT_H
#define DELUGE_INPUT_H
#include "deluge/deluge.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Values of deluge_event::kind. */
enum {
    DELUGE_EV_PAD = 0,      /**< A pad was pressed or released. */
    DELUGE_EV_BUTTON = 1,   /**< A button was pressed or released. */
    DELUGE_EV_ENCODER = 2,  /**< An encoder was turned. */
    DELUGE_EV_CLOCK = 3,    /**< A pulse arrived on the clock input. */
};

/**
 * @brief One input event. Fields not listed for a kind are 0.
 *
 * ::DELUGE_EV_PAD
 * - @c id: pad index, 0..143. Each pad has a fixed index.
 * - @c value: 1 on press, 0 on release.
 * - @c x, @c y: pad column 0..17 and row 0..7, with (0, 0) at the top left.
 *   Set on release as well as press. These match the pad framebuffer
 *   coordinates in display.h.
 * - @c pressure: 0..4095 on press, 0 on release. The current driver reports
 *   4095 for every press.
 *
 * ::DELUGE_EV_BUTTON
 * - @c id: the front-panel controller's button id, 0..35. Encoder push
 *   clicks arrive as buttons too. docs/api.md lists the ids.
 * - @c value: 1 on press, 0 on release. Auto-repeat is not reported.
 *
 * ::DELUGE_EV_ENCODER
 * - @c id: 0 scroll X, 1 tempo, 2 upper gold knob, 3 lower gold knob,
 *   4 scroll Y, 5 select.
 * - @c value: +1 for one step clockwise, -1 for one step anticlockwise. The
 *   detented encoders give one step per detent; the gold knobs have no
 *   detents and give finer steps.
 *
 * ::DELUGE_EV_CLOCK
 * - @c value: the low 16 bits of the kernel's running pulse count. It wraps;
 *   compare consecutive values to detect missed pulses.
 */
typedef struct {
    uint8_t  kind;      /**< One of the DELUGE_EV_* values. */
    uint8_t  id;        /**< Pad, button or encoder index; see above. */
    int16_t  value;     /**< Press state, encoder steps or clock count; see above. */
    uint16_t x;         /**< Pad column (pads only). */
    uint16_t y;         /**< Pad row (pads only). */
    uint16_t pressure;  /**< Pad pressure (pads only). */
} deluge_event;

/** @brief Input capability handle. */
typedef struct deluge_input deluge_input_t;

/**
 * @brief Get the input capability.
 *
 * @param d Device handle. May be NULL.
 * @return The handle (owned by @p d, valid until deluge_close()), or NULL if
 *         @p d is NULL or none of the pad, button, encoder and clock input
 *         devices could be opened.
 */
deluge_input_t *deluge_input(deluge_t *d);

/**
 * @brief Get a descriptor to poll for pending input.
 *
 * The descriptor is an epoll fd that becomes readable when any input source
 * has events. Add it to your own @c poll / @c epoll loop and call
 * deluge_input_read() when it is readable.
 *
 * @param in Input handle.
 * @return The descriptor, or -1 if @p in is NULL. libdeluge owns it; do not
 *         close it. It stays valid until deluge_close().
 */
int deluge_input_fd(deluge_input_t *in);

/**
 * @brief Read pending input events without blocking.
 *
 * @param in  Input handle.
 * @param out Array to receive events.
 * @param max Capacity of @p out. Pass at least 144 (one event per pad) so a
 *            full multi-touch frame always fits; raw events that do not fit
 *            in @p out are dropped.
 * @return The number of events written, 0..@p max (0 if none are pending);
 *         ::DELUGE_ERR_STATE if @p in is NULL; ::DELUGE_ERR_IO if polling
 *         fails.
 * @note Do not call this while a callback thread from deluge_input_start() is
 *       running.
 */
int deluge_input_read(deluge_input_t *in, deluge_event *out, int max);

/**
 * @brief Input callback, called once per event on the input thread.
 *
 * @param ev  The event. Valid only for this call.
 * @param ctx The pointer passed to deluge_input_start().
 */
typedef void (*deluge_input_cb)(const deluge_event *ev, void *ctx);

/**
 * @brief Start a library-owned thread that delivers events to @p cb.
 *
 * The thread runs at normal (not real-time) priority and blocks until input
 * arrives.
 *
 * @param in  Input handle.
 * @param cb  Callback. Must not be NULL.
 * @param ctx Passed to every @p cb call.
 * @return ::DELUGE_OK; ::DELUGE_ERR_INVAL if @p in or @p cb is NULL;
 *         ::DELUGE_ERR_STATE if already started; ::DELUGE_ERR_IO if creating
 *         the wake pipe or the thread fails.
 */
int  deluge_input_start(deluge_input_t *in, deluge_input_cb cb, void *ctx);

/**
 * @brief Stop the input thread.
 *
 * Wakes the thread and blocks until it has exited; @p cb is not called after
 * this returns. deluge_close() calls this for you.
 *
 * @param in Input handle. NULL, or a handle that is not started, is a no-op.
 * @note Do not call this from inside the callback.
 */
void deluge_input_stop(deluge_input_t *in);

#ifdef __cplusplus
}
#endif
#endif

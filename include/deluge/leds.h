/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/**
 * @file leds.h
 * @brief Indicator, gold-knob and SYNC LEDs.
 *
 * These are sysfs LED-class devices under @c /sys/class/leds/ (@c deluge:*).
 * The pad LEDs are a framebuffer; see display.h.
 */
#ifndef DELUGE_LEDS_H
#define DELUGE_LEDS_H
#include "deluge/deluge.h"
#ifdef __cplusplus
extern "C" {
#endif

/** @brief LEDs capability handle. */
typedef struct deluge_leds deluge_leds_t;

/**
 * @brief Get the LEDs capability.
 *
 * @param d Device handle. May be NULL.
 * @return The handle (owned by @p d, valid until deluge_close()), or NULL if
 *         @p d is NULL or no @c deluge:* LED could be opened.
 */
deluge_leds_t *deluge_leds(deluge_t *d);

/**
 * @brief Turn an indicator LED on or off.
 *
 * @param l  LEDs handle.
 * @param id Indicator index, 0..35 (LED @c deluge:indicator:N).
 * @param on Non-zero for on, 0 for off.
 * @return ::DELUGE_OK; ::DELUGE_ERR_STATE if @p l is NULL;
 *         ::DELUGE_ERR_INVAL if @p id is out of range; ::DELUGE_ERR_NODEV if
 *         that LED was not found at open; ::DELUGE_ERR_IO if the write fails.
 */
int deluge_leds_indicator(deluge_leds_t *l, int id, int on);

/**
 * @brief Set the brightness of one LED in a gold-knob column.
 *
 * @param l          LEDs handle.
 * @param col        Column, 0 or 1 (LED @c deluge:goldC:I for column C, LED I).
 * @param i          LED within the column, 0..3.
 * @param brightness 0..255; values outside the range are clamped.
 * @return ::DELUGE_OK; ::DELUGE_ERR_STATE if @p l is NULL;
 *         ::DELUGE_ERR_INVAL if @p col or @p i is out of range;
 *         ::DELUGE_ERR_NODEV if that LED was not found at open;
 *         ::DELUGE_ERR_IO if the write fails.
 */
int deluge_leds_gold(deluge_leds_t *l, int col, int i, int brightness);

/**
 * @brief Turn the SYNC LED on or off.
 *
 * The SYNC LED is a plain GPIO LED (@c deluge:sync), not driven through the
 * front-panel controller.
 *
 * @param l  LEDs handle.
 * @param on Non-zero for on, 0 for off.
 * @return ::DELUGE_OK; ::DELUGE_ERR_STATE if @p l is NULL;
 *         ::DELUGE_ERR_NODEV if the LED was not found at open;
 *         ::DELUGE_ERR_IO if the write fails.
 */
int deluge_leds_sync(deluge_leds_t *l, int on);

#ifdef __cplusplus
}
#endif
#endif

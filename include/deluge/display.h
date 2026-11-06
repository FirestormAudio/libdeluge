/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/**
 * @file display.h
 * @brief OLED and pad-LED framebuffers.
 *
 * Both displays are Linux framebuffers that libdeluge maps into memory. Write
 * a whole frame with the @c _write functions, or draw straight into the
 * mapping returned by the @c _map functions; the kernel flushes changes to the
 * hardware shortly afterwards in either case.
 *
 * OLED layout (::DELUGE_OLED_BYTES = 688): 128 x 43 pixels, 1 bit per pixel,
 * 16 bytes per row, rows top to bottom. Within a byte the most significant
 * bit is the leftmost pixel; a set bit lights the pixel. Pixel (x, y) is bit
 * @c 7 - (x % 8) of byte @c y * 16 + x / 8.
 *
 * Pad layout (::DELUGE_PADS_BYTES = 432): 18 columns x 8 rows, 3 bytes per pad
 * in R, G, B order, 54 bytes per row, rows top to bottom, columns left to
 * right. Pad (x, y) starts at byte @c y * 54 + x * 3. These coordinates match
 * the @c x / @c y of pad events from input.h.
 */
#ifndef DELUGE_DISPLAY_H
#define DELUGE_DISPLAY_H
#include "deluge/deluge.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Display capability handle. */
typedef struct deluge_display deluge_display_t;

/** @brief Size of one OLED frame: 128 x 43, 1 bpp, 16 bytes per row. */
#define DELUGE_OLED_BYTES 688u
/** @brief Size of one pad frame: 18 x 8, 24 bpp RGB, 54 bytes per row. */
#define DELUGE_PADS_BYTES 432u

/**
 * @brief Get the display capability.
 *
 * @param d Device handle. May be NULL.
 * @return The handle (owned by @p d, valid until deluge_close()), or NULL if
 *         @p d is NULL or neither framebuffer is present. If only one is
 *         present, calls on the other return ::DELUGE_ERR_NODEV or NULL.
 */
deluge_display_t *deluge_display(deluge_t *d);

/**
 * @brief Copy a full frame to the OLED.
 *
 * @param disp Display handle.
 * @param buf  Frame in the OLED layout described above.
 * @param len  Must equal ::DELUGE_OLED_BYTES.
 * @return ::DELUGE_OK; ::DELUGE_ERR_STATE if @p disp is NULL;
 *         ::DELUGE_ERR_NODEV if the OLED framebuffer is absent;
 *         ::DELUGE_ERR_INVAL if @p len is wrong or the mapped framebuffer is
 *         smaller than expected.
 */
int deluge_display_oled_write(deluge_display_t *disp, const uint8_t *buf, size_t len);

/**
 * @brief Copy a full frame to the pad LEDs.
 *
 * @param disp Display handle.
 * @param buf  Frame in the pad layout described above.
 * @param len  Must equal ::DELUGE_PADS_BYTES.
 * @return Same as deluge_display_oled_write(), for the pad framebuffer.
 */
int deluge_display_pads_write(deluge_display_t *disp, const uint8_t *buf, size_t len);

/**
 * @brief Get the OLED framebuffer mapping for drawing in place.
 *
 * @param disp    Display handle.
 * @param out_len If not NULL, receives the mapped size in bytes (0 if the OLED
 *                is absent). Left untouched if @p disp is NULL.
 * @return The mapping, or NULL if @p disp is NULL or the OLED is absent. The
 *         mapping is owned by libdeluge and stays valid until deluge_close();
 *         do not @c munmap it.
 */
uint8_t *deluge_display_oled_map(deluge_display_t *disp, size_t *out_len);

/**
 * @brief Get the pad framebuffer mapping for drawing in place.
 *
 * @param disp    Display handle.
 * @param out_len As for deluge_display_oled_map().
 * @return As for deluge_display_oled_map(), for the pad framebuffer.
 */
uint8_t *deluge_display_pads_map(deluge_display_t *disp, size_t *out_len);

/**
 * @brief Set the pad-LED refresh interval.
 *
 * This is the front-panel controller's LED refresh period: a shorter period
 * is a higher duty cycle, so a lower value makes the pads brighter. It does
 * not change how often frames are sent to the pads.
 *
 * Written through the @c deluge-pic driver's @c refresh_time sysfs attribute.
 *
 * @param disp        Display handle.
 * @param interval_ms Interval in milliseconds, 0..25.
 * @return ::DELUGE_OK; ::DELUGE_ERR_STATE if @p disp is NULL;
 *         ::DELUGE_ERR_INVAL if @p interval_ms is outside 0..25;
 *         ::DELUGE_ERR_NODEV if the attribute is absent (the @c deluge-pic
 *         driver is not bound); ::DELUGE_ERR_IO if the write fails.
 */
int deluge_display_pads_set_refresh(deluge_display_t *disp, int interval_ms);

#ifdef __cplusplus
}
#endif
#endif

/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/**
 * @file cv.h
 * @brief CV outputs (2 DAC channels) and gate outputs (4 lines).
 *
 * CV goes through the @c deluge-cv IIO DAC; gates go through the GPIO
 * character device. Each call is one synchronous sysfs write or ioctl, so do
 * not call these from the audio callback.
 */
#ifndef DELUGE_CV_H
#define DELUGE_CV_H
#include "deluge/deluge.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/** @brief CV/gate capability handle. */
typedef struct deluge_cv deluge_cv_t;

/**
 * @brief Get the CV/gate capability.
 *
 * @param d Device handle. May be NULL.
 * @return The handle (owned by @p d, valid until deluge_close()), or NULL if
 *         @p d is NULL or neither a CV channel nor a gate line could be opened.
 */
deluge_cv_t *deluge_cv(deluge_t *d);

/**
 * @brief Set a CV output to a raw DAC count.
 *
 * @param cv  CV handle.
 * @param ch  Channel, 0 or 1.
 * @param raw DAC count, 0..65535.
 * @return ::DELUGE_OK; ::DELUGE_ERR_STATE if @p cv is NULL;
 *         ::DELUGE_ERR_INVAL if @p ch is out of range; ::DELUGE_ERR_NODEV if
 *         that channel was not found at open; ::DELUGE_ERR_IO if the write
 *         fails.
 */
int deluge_cv_set_raw(deluge_cv_t *cv, int ch, uint16_t raw);

/**
 * @brief Set a CV output in volts.
 *
 * Converts at 6552 counts per volt and clamps to the DAC range, so the output
 * is unipolar: negative values give 0 V and the ceiling is about 10 V. NaN
 * gives 0 V.
 *
 * @note The scale matches the stock firmware's (1 V = 6552 counts), which is a
 *       nominal figure, not a per-unit calibration. Use deluge_cv_set_raw()
 *       if you need exact control of the output.
 *
 * @param cv    CV handle.
 * @param ch    Channel, 0 or 1.
 * @param volts Target voltage.
 * @return Same as deluge_cv_set_raw().
 */
int deluge_cv_set_volts(deluge_cv_t *cv, int ch, float volts);

/**
 * @brief Set a gate output.
 *
 * The gate lines are active-low V-trig; libdeluge requests them active-low, so
 * @p on is the logical state.
 *
 * @param cv CV handle.
 * @param n  Gate, 0..3.
 * @param on Non-zero turns the gate on, 0 turns it off.
 * @return ::DELUGE_OK; ::DELUGE_ERR_STATE if @p cv is NULL;
 *         ::DELUGE_ERR_INVAL if @p n is out of range; ::DELUGE_ERR_NODEV if
 *         that gate line was not found at open; ::DELUGE_ERR_IO if the ioctl
 *         fails.
 */
int deluge_gate_set(deluge_cv_t *cv, int n, int on);

#ifdef __cplusplus
}
#endif
#endif

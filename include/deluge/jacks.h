/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/**
 * @file jacks.h
 * @brief Jack-detect state and the on-board speaker switch.
 *
 * Both are ALSA controls on the @c Deluge sound card, so @c amixer shows the
 * same values.
 */
#ifndef DELUGE_JACKS_H
#define DELUGE_JACKS_H
#include "deluge/deluge.h"
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Jacks capability handle. */
typedef struct deluge_jacks deluge_jacks_t;

/** @brief Jacks with an insertion-detect line. */
enum deluge_jack {
    DELUGE_JACK_HEADPHONE = 0,   /**< Headphone output. */
    DELUGE_JACK_LINE_IN   = 1,   /**< Line input. */
    DELUGE_JACK_MIC       = 2,   /**< Microphone input. */
    DELUGE_JACK_LINE_OUT_L = 3,  /**< Left line output. */
    DELUGE_JACK_LINE_OUT_R = 4,  /**< Right line output. */
};

/**
 * @brief Get the jacks capability.
 *
 * @param d Device handle. May be NULL.
 * @return The handle (owned by @p d, valid until deluge_close()), or NULL if
 *         @p d is NULL, the sound card cannot be opened, or the card has no
 *         jack controls.
 */
deluge_jacks_t *deluge_jacks(deluge_t *d);

/**
 * @brief Read whether a jack has a plug inserted.
 *
 * The detect pins have no interrupt, so each call samples the pin. Poll this
 * if you need to notice changes; there is no notification.
 *
 * @param j    Jacks handle.
 * @param jack Which jack.
 * @return 1 if inserted, 0 if not; ::DELUGE_ERR_STATE if @p j is NULL;
 *         ::DELUGE_ERR_INVAL if @p jack is not a ::deluge_jack value;
 *         ::DELUGE_ERR_NODEV if the control cannot be read.
 */
int deluge_jacks_get(deluge_jacks_t *j, enum deluge_jack jack);

/**
 * @brief Request the on-board speaker on or off.
 *
 * This sets the card's "Speaker Playback Switch" control, which is a request,
 * not a direct control of the amplifier. The kernel turns the speaker on only
 * when this switch is on and no output jack is inserted, so with headphones
 * plugged in the speaker stays muted whatever you set.
 *
 * @param j  Jacks handle.
 * @param on Non-zero to request the speaker on, 0 for off.
 * @return ::DELUGE_OK; ::DELUGE_ERR_STATE if @p j is NULL;
 *         ::DELUGE_ERR_NODEV if the control is missing; ::DELUGE_ERR_IO if
 *         the write fails.
 */
int deluge_jacks_set_speaker(deluge_jacks_t *j, int on);

/**
 * @brief Read the speaker switch.
 *
 * Returns the request set by deluge_jacks_set_speaker(), not whether the
 * amplifier is actually on; the kernel may be muting it for an inserted jack.
 *
 * @param j Jacks handle.
 * @return 1 or 0; ::DELUGE_ERR_STATE if @p j is NULL; ::DELUGE_ERR_NODEV if
 *         the control cannot be read.
 */
int deluge_jacks_get_speaker(deluge_jacks_t *j);

#ifdef __cplusplus
}
#endif
#endif

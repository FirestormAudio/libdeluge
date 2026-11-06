/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/**
 * @file usb_audio.h
 * @brief USB audio card discovery and probing.
 *
 * Streaming is not implemented in 0.1.x; this header covers discovery and
 * probing only. To move audio over USB today, open the card yourself with ALSA
 * using the id from deluge_usb_audio_card_id().
 *
 * The bound card is the USB audio gadget (@c UAC2_Gadget) in peripheral role,
 * or a plugged-in USB audio interface (@c USB-Audio) in host role. It is
 * separate from the internal codec in audio.h.
 */
#ifndef DELUGE_USB_AUDIO_H
#define DELUGE_USB_AUDIO_H
#include "deluge/deluge.h"
#ifdef __cplusplus
extern "C" {
#endif

/** @brief USB audio capability handle. */
typedef struct deluge_usb_audio deluge_usb_audio_t;

/** @brief Format bit: signed 16-bit little-endian. */
#define DELUGE_USB_AUDIO_FMT_S16 0x1
/** @brief Format bit: signed 24-bit little-endian in 32-bit containers. */
#define DELUGE_USB_AUDIO_FMT_S24 0x2
/** @brief Format bit: signed 32-bit little-endian. */
#define DELUGE_USB_AUDIO_FMT_S32 0x4

/** @brief Capabilities reported by deluge_usb_audio_probe(). */
typedef struct {
    unsigned int playback_channels; /**< Maximum channels, device to USB host. 0 if there is no playback stream. */
    unsigned int capture_channels;  /**< Maximum channels, USB host to device. 0 if there is no capture stream. */
    unsigned int rate_min;          /**< Lowest rate supported by either direction. */
    unsigned int rate_max;          /**< Highest rate supported by either direction. */
    int formats;                    /**< Formats supported by either direction, OR of DELUGE_USB_AUDIO_FMT_*. */
} deluge_usb_audio_info_t;

/**
 * @brief Get the USB audio capability.
 *
 * The handle exists whether or not a card is bound, so it survives role
 * switches and hotplug; call deluge_usb_audio_rebind() to pick up a card that
 * appears later.
 *
 * @param d Device handle. May be NULL.
 * @return The handle (owned by @p d, valid until deluge_close()), or NULL if
 *         @p d is NULL or allocation failed.
 */
deluge_usb_audio_t *deluge_usb_audio(deluge_t *d);

/**
 * @brief Get the bound card's ALSA id, for use as <tt>hw:CARD=\<id\></tt>.
 *
 * @param a USB audio handle.
 * @return The id, owned by @p a and valid until the next rebind or
 *         deluge_close(); or NULL if @p a is NULL or no card is bound.
 */
const char *deluge_usb_audio_card_id(deluge_usb_audio_t *a);

/**
 * @brief Report the bound card's channels, rates and formats.
 *
 * Briefly opens each direction's PCM, queries it and closes it. No stream is
 * started. A direction that cannot be opened (absent, or in use) reports 0
 * channels.
 *
 * @param a   USB audio handle.
 * @param out Receives the report.
 * @return ::DELUGE_OK; ::DELUGE_ERR_STATE if @p a is NULL;
 *         ::DELUGE_ERR_INVAL if @p out is NULL; ::DELUGE_ERR_NODEV if no card
 *         is bound; ::DELUGE_ERR_IO if querying an opened PCM fails.
 */
int deluge_usb_audio_probe(deluge_usb_audio_t *a, deluge_usb_audio_info_t *out);

/**
 * @brief Find the USB audio card again and bind to it.
 *
 * Call after a hotplug event or a role switch.
 *
 * @param a USB audio handle.
 * @return ::DELUGE_OK if a card is now bound; ::DELUGE_ERR_NODEV if none was
 *         found (the handle is then unbound); ::DELUGE_ERR_STATE if @p a is
 *         NULL.
 */
int deluge_usb_audio_rebind(deluge_usb_audio_t *a);

#ifdef __cplusplus
}
#endif
#endif

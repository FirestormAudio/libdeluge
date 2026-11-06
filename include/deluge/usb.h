/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/**
 * @file usb.h
 * @brief USB port role (peripheral or host) and hotplug notification.
 *
 * The Deluge has one USB port. In peripheral role it is a USB gadget (serial
 * console, USB audio, USB MIDI); in host role devices can be plugged into it.
 * For the USB MIDI and USB audio data handles, see usb_midi.h and
 * usb_audio.h.
 */
#ifndef DELUGE_USB_H
#define DELUGE_USB_H
#include "deluge/deluge.h"
#ifdef __cplusplus
extern "C" {
#endif

/** @brief USB port role. */
typedef enum {
    DELUGE_USB_PERIPHERAL = 0,  /**< The Deluge is a USB device (gadget). */
    DELUGE_USB_HOST = 1         /**< The Deluge is the USB host. */
} deluge_usb_role;

/** @brief Hotplug action. */
typedef enum {
    DELUGE_USB_ADDED = 0,   /**< A device node appeared (any uevent action other than "remove"). */
    DELUGE_USB_REMOVED = 1  /**< A device node went away. */
} deluge_usb_hotplug;

/** @brief Class of a hotplugged device node. */
typedef enum {
    DELUGE_USB_CLASS_MIDI = 0,  /**< ALSA rawmidi or UMP node (@c snd/midi*, @c snd/ump*). */
    DELUGE_USB_CLASS_AUDIO,     /**< ALSA PCM node (@c snd/pcm*). */
    DELUGE_USB_CLASS_INPUT,     /**< evdev node (@c input/event*). */
    DELUGE_USB_CLASS_STORAGE,   /**< Block device or partition (@c sd*). */
    DELUGE_USB_CLASS_OTHER      /**< Anything else. Not delivered to the hotplug callback. */
} deluge_usb_class;

/**
 * @brief Hotplug callback, called on the hotplug thread.
 *
 * @param ev      Whether the node was added or removed.
 * @param cls     Device class.
 * @param devnode Device node path, for example @c "/dev/snd/midiC4D0",
 *                @c "/dev/input/event5" or @c "/dev/sda1". Valid only for this
 *                call.
 * @param ctx     The pointer passed to deluge_usb_watch().
 */
typedef void (*deluge_usb_hotplug_cb)(deluge_usb_hotplug ev, deluge_usb_class cls,
                                      const char *devnode, void *ctx);

/** @brief USB capability handle. */
typedef struct deluge_usb deluge_usb_t;

/**
 * @brief Get the USB capability.
 *
 * @param d Device handle. May be NULL.
 * @return The handle (owned by @p d, valid until deluge_close()), or NULL if
 *         @p d is NULL or neither the role control nor the hotplug socket is
 *         available. If only one is available, the functions that need the
 *         other return ::DELUGE_ERR_NODEV or -1.
 */
deluge_usb_t *deluge_usb(deluge_t *d);

/**
 * @brief Read the current port role.
 *
 * @param u   USB handle.
 * @param out Receives the role.
 * @return ::DELUGE_OK; ::DELUGE_ERR_STATE if @p u is NULL;
 *         ::DELUGE_ERR_INVAL if @p out is NULL; ::DELUGE_ERR_NODEV if the
 *         role control is absent; ::DELUGE_ERR_IO if the read fails or the
 *         role is neither host nor peripheral.
 */
int deluge_usb_role_get(deluge_usb_t *u, deluge_usb_role *out);

/**
 * @brief Switch the port role.
 *
 * @warning Switching to ::DELUGE_USB_HOST removes the USB gadget: the USB
 *          serial console and any USB MIDI or USB audio connection to a
 *          computer drop until you switch back. Call deluge_usb_midi_rebind()
 *          and deluge_usb_audio_rebind() after switching.
 *
 * @param u    USB handle.
 * @param role New role.
 * @return ::DELUGE_OK; ::DELUGE_ERR_STATE if @p u is NULL;
 *         ::DELUGE_ERR_NODEV if the role control is absent;
 *         ::DELUGE_ERR_IO if the write fails.
 */
int deluge_usb_role_set(deluge_usb_t *u, deluge_usb_role role);

/**
 * @brief Start a library-owned thread that reports USB hotplug to @p cb.
 *
 * Only device nodes whose sysfs path is under the USB controller are reported
 * (devices plugged into the port in host role), and only the classes in
 * ::deluge_usb_class other than ::DELUGE_USB_CLASS_OTHER. Use this or
 * deluge_usb_fd(), not both.
 *
 * @param u   USB handle.
 * @param cb  Callback. Must not be NULL.
 * @param ctx Passed to every @p cb call.
 * @return ::DELUGE_OK; ::DELUGE_ERR_INVAL if @p u or @p cb is NULL;
 *         ::DELUGE_ERR_NODEV if the hotplug socket is unavailable;
 *         ::DELUGE_ERR_STATE if already watching; ::DELUGE_ERR_IO if setting
 *         up the thread fails.
 */
int  deluge_usb_watch(deluge_usb_t *u, deluge_usb_hotplug_cb cb, void *ctx);

/**
 * @brief Stop the hotplug thread.
 *
 * Wakes the thread and blocks until it has exited; @p cb is not called after
 * this returns. deluge_close() calls this for you.
 *
 * @param u USB handle. NULL, or a handle that is not watching, is a no-op.
 * @note Do not call this from inside the callback.
 */
void deluge_usb_unwatch(deluge_usb_t *u);

/**
 * @brief Get the raw hotplug socket for your own poll loop.
 *
 * The descriptor is a non-blocking @c NETLINK_KOBJECT_UEVENT socket. Reading
 * it returns raw kernel uevent messages for every device, not just USB ones;
 * you parse and filter them yourself. Do not read it while deluge_usb_watch()
 * is running.
 *
 * @param u USB handle.
 * @return The descriptor, or -1 if @p u is NULL or the socket is unavailable.
 *         libdeluge owns it; do not close it. It stays valid until
 *         deluge_close().
 */
int  deluge_usb_fd(deluge_usb_t *u);

#ifdef __cplusplus
}
#endif
#endif

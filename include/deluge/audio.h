/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/**
 * @file audio.h
 * @brief Internal codec audio I/O through a real-time callback.
 *
 * libdeluge opens the codec's playback and capture streams as a linked pair at
 * a fixed format (::DELUGE_SR Hz, ::DELUGE_CH channels, ::DELUGE_PERIOD-frame
 * periods) and calls your callback once per period from its own audio thread.
 */
#ifndef DELUGE_AUDIO_H
#define DELUGE_AUDIO_H
#include "deluge/deluge.h"
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Sample rate in Hz. */
#define DELUGE_SR      44100
/** @brief Channel count (stereo). */
#define DELUGE_CH      2
/** @brief Frames per callback. */
#define DELUGE_PERIOD  128

/** @brief Audio capability handle. */
typedef struct deluge_audio deluge_audio_t;

/**
 * @brief Audio callback, called once per period on the audio thread.
 *
 * @param in      Captured audio (line in): @p nframes frames of interleaved
 *                L/R @c float samples in [-1, 1]. After an xrun the block is
 *                silence. Read-only, valid only for this call.
 * @param out     Buffer for @p nframes frames of interleaved L/R @c float
 *                samples. Write every sample: the buffer still holds the
 *                previous period's data. Values outside [-1, 1] are clamped.
 * @param nframes Number of frames; always ::DELUGE_PERIOD.
 * @param ctx     The pointer passed to deluge_audio_start().
 *
 * @warning The callback runs on a real-time thread. It must not allocate,
 *          block, take locks shared with non-real-time threads, or make
 *          system calls (including printf). Overrunning the period causes an
 *          xrun, counted by deluge_audio_xruns().
 */
typedef void (*deluge_audio_cb)(const float *in, float *out, int nframes, void *ctx);

/**
 * @brief Get the audio capability.
 *
 * @param d Device handle. May be NULL.
 * @return The audio handle (owned by @p d, valid until deluge_close()), or
 *         NULL if @p d is NULL. The handle exists even when the codec is
 *         absent; deluge_audio_start() reports that.
 */
deluge_audio_t *deluge_audio(deluge_t *d);

/**
 * @brief Open the codec and start calling @p cb.
 *
 * Opens @c hw:CARD=Deluge for playback and capture as a linked pair, configures
 * S24_LE at ::DELUGE_SR / ::DELUGE_CH / ::DELUGE_PERIOD, and starts the audio
 * thread. The thread asks for @c SCHED_FIFO priority 80; that needs
 * @c CAP_SYS_NICE (or a suitable @c RLIMIT_RTPRIO). If it is refused, a message
 * goes to stderr and the thread runs at normal priority.
 *
 * Xruns are recovered automatically. If recovery fails, the audio thread exits
 * and the callback is no longer called; deluge_audio_running() then returns 0.
 * Calling deluge_audio_start() again cleans up and restarts.
 *
 * @param a   Audio handle.
 * @param cb  Callback. Must not be NULL.
 * @param ctx Passed to every @p cb call.
 * @return ::DELUGE_OK; ::DELUGE_ERR_INVAL if @p a or @p cb is NULL;
 *         ::DELUGE_ERR_STATE if already running; ::DELUGE_ERR_NODEV if the
 *         codec cannot be opened; ::DELUGE_ERR_FORMAT if it rejects the
 *         format; ::DELUGE_ERR_IO if linking the streams or creating the thread
 *         fails.
 * @note Call deluge_audio_start() and deluge_audio_stop() from one
 *       controlling thread, never from inside the callback.
 */
int  deluge_audio_start(deluge_audio_t *a, deluge_audio_cb cb, void *ctx);

/**
 * @brief Stop the audio thread and close the codec.
 *
 * Blocks until the thread has exited; the callback is not called after this
 * returns. deluge_close() calls this for you.
 *
 * Also releases the codec after the thread has exited on its own (see
 * deluge_audio_start()).
 *
 * @param a Audio handle. NULL, or a handle that was never started, is a no-op.
 * @note Same threading rule as deluge_audio_start().
 */
void deluge_audio_stop(deluge_audio_t *a);

/**
 * @brief Report whether the audio thread is running.
 *
 * @param a Audio handle. May be NULL.
 * @return 1 while the callback is being called; 0 if @p a is NULL, audio was
 *         never started or was stopped, or the thread exited after a failed
 *         xrun recovery.
 * @note Safe to call from any thread.
 */
int  deluge_audio_running(deluge_audio_t *a);

/**
 * @brief Number of xruns (overruns or underruns) since the handle was created.
 *
 * @param a Audio handle.
 * @return The running count, or 0 if @p a is NULL. The counter is written by
 *         the audio thread without synchronisation, so read it as an
 *         approximate, monotonically increasing value.
 */
unsigned long deluge_audio_xruns(deluge_audio_t *a);

#ifdef __cplusplus
}
#endif
#endif

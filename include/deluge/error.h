/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/**
 * @file error.h
 * @brief Error codes returned by libdeluge functions.
 *
 * Functions that return @c int return ::DELUGE_OK (0) or a non-negative count
 * on success, and a negative ::deluge_err on failure.
 */
#ifndef DELUGE_ERROR_H
#define DELUGE_ERROR_H
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Error codes. All errors are negative. */
typedef enum {
    DELUGE_OK            =  0,  /**< Success. */
    DELUGE_ERR_NODEV     = -1,  /**< An expected device node or control was not found. */
    DELUGE_ERR_IO        = -2,  /**< An open, read, write or ioctl call failed. */
    DELUGE_ERR_FORMAT    = -3,  /**< The hardware rejected the requested PCM format. */
    DELUGE_ERR_STATE     = -4,  /**< Called in the wrong lifecycle state; most functions also return this for a NULL handle. */
    DELUGE_ERR_NOMEM     = -5,  /**< Allocation failed. Reserved; no function currently returns it. */
    DELUGE_ERR_INVAL     = -6,  /**< Invalid argument. */
} deluge_err;

/**
 * @brief Describe an error code.
 *
 * @param code A ::deluge_err value (or any int).
 * @return A static, lower-case English string. Unrecognised codes return
 *         "unknown error". Never NULL; do not free it.
 * @note Thread-safe.
 */
const char *deluge_strerror(int code);

#ifdef __cplusplus
}
#endif
#endif

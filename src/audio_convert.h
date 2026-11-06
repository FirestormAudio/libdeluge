/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef DELUGE_AUDIO_CONVERT_H
#define DELUGE_AUDIO_CONVERT_H
#include <stdint.h>
void deluge_f32_to_s24(const float *in, int32_t *out, int samples);
void deluge_s24_to_f32(const int32_t *in, float *out, int samples);
#endif

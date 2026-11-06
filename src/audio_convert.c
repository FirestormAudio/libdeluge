/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "audio_convert.h"
#include <math.h>

#define S24_MAX 8388607.0f  /* 2^23 - 1 */

void deluge_f32_to_s24(const float *in, int32_t *out, int samples) {
    for (int i = 0; i < samples; i++) {
        float v = fmaxf(-1.0f, fminf(1.0f, in[i]));  /* NaN -> 1.0 (finite), no UB on cast */
        out[i] = (int32_t)(v * S24_MAX);
    }
}

void deluge_s24_to_f32(const int32_t *in, float *out, int samples) {
    for (int i = 0; i < samples; i++)
        out[i] = (float)in[i] / S24_MAX;
}

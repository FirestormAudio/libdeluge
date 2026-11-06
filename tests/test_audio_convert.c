/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "audio_convert.h"
#include <assert.h>
#include <math.h>

int main(void) {
    /* Full-scale and zero round-trip within 1 LSB of 24-bit. */
    float in[4]  = { 1.0f, -1.0f, 0.0f, 0.5f };
    int32_t s24[4];
    float back[4];
    deluge_f32_to_s24(in, s24, 4);

    assert(s24[0] ==  8388607);   /* +full scale  */
    assert(s24[1] == -8388607);   /* -full scale (symmetric clamp) */
    assert(s24[2] ==  0);
    assert(s24[3] ==  4194303 || s24[3] == 4194304); /* ~0.5 */

    deluge_s24_to_f32(s24, back, 4);
    for (int i = 0; i < 4; i++) assert(fabsf(back[i] - in[i]) < 1.0f / 8388607.0f + 1e-6f);

    /* Clamp beyond [-1,1]. */
    float hot[2] = { 2.0f, -3.0f };
    int32_t c[2];
    deluge_f32_to_s24(hot, c, 2);
    assert(c[0] == 8388607 && c[1] == -8388607);

    /* Non-finite inputs must clamp to a finite in-range value (no UB on cast). */
    float nf[3] = { NAN, INFINITY, -INFINITY };
    int32_t nfo[3];
    deluge_f32_to_s24(nf, nfo, 3);
    assert(nfo[0] ==  8388607);   /* NaN  -> fminf(1,NaN)=1  -> +full scale */
    assert(nfo[1] ==  8388607);   /* +Inf -> +full scale */
    assert(nfo[2] == -8388607);   /* -Inf -> -full scale */
    return 0;
}

/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "deluge/deluge.h"
#include <assert.h>
#include <string.h>

int main(void) {
    assert(strcmp(deluge_strerror(DELUGE_ERR_NODEV), "device not found") == 0);
    assert(strcmp(deluge_strerror(DELUGE_OK), "ok") == 0);
    deluge_close(NULL); /* must be NULL-safe */
    return 0;
}

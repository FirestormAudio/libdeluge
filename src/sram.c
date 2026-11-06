/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "deluge/sram.h"
#include "sram_priv.h"
#include <fcntl.h>
#include <linux/types.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

/* Mirror of deluge-linux board/synthstrom/deluge/kernel/include/uapi/deluge-sram.h.
   The magic and number must match that header exactly. */
struct deluge_sram_info { __u32 phys; __u32 size; };
#define DELUGE_SRAM_IOC_MAGIC 'D'
#define DELUGE_SRAM_GET_INFO  _IOR(DELUGE_SRAM_IOC_MAGIC, 1, struct deluge_sram_info)

struct deluge_sram { int fd; void *map; size_t len; };

deluge_sram_t *deluge_sram_new(void) {
    int fd = open("/dev/deluge-sram", O_RDWR | O_CLOEXEC);
    if (fd < 0) return NULL;
    struct deluge_sram_info info;
    if (ioctl(fd, DELUGE_SRAM_GET_INFO, &info) < 0 || info.size == 0) { close(fd); return NULL; }
    void *m = mmap(NULL, info.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (m == MAP_FAILED) { close(fd); return NULL; }
    deluge_sram_t *s = calloc(1, sizeof *s);
    if (!s) { munmap(m, info.size); close(fd); return NULL; }
    s->fd = fd; s->map = m; s->len = info.size;
    return s;
}

void deluge_sram_free(deluge_sram_t *s) {
    if (!s) return;
    if (s->map) munmap(s->map, s->len);
    if (s->fd >= 0) close(s->fd);
    free(s);
}

void *deluge_sram_map(deluge_sram_t *s, size_t *out_len) {
    if (out_len) *out_len = s ? s->len : 0;
    return s ? s->map : NULL;
}

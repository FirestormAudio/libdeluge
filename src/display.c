/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "deluge/display.h"
#include "display_priv.h"
#include "discover.h"
#include <dirent.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

struct fbpanel { int fd; uint8_t *map; size_t len; };

struct deluge_display { struct fbpanel oled, pads; int refresh_fd; };

/* Open the deluge-pic platform device's `refresh_time` attribute.
 *
 * The attribute is registered through the driver's dev_groups, so it appears on
 * whichever device is bound to the driver. Scan that bind directory rather than
 * hardcoding the SoC address (`e8007800.serial`): the address is a DTS detail,
 * and hardcoding it would silently stop matching if the node ever moved. */
static int open_refresh_time(void) {
    const char *root = getenv("DELUGE_PIC_DRIVER_ROOT");
    if (!root) root = "/sys/bus/platform/drivers/deluge-pic";
    DIR *dir = opendir(root);
    if (!dir) return -1;
    int fd = -1;
    struct dirent *e;
    while ((e = readdir(dir))) {
        if (e->d_name[0] == '.') continue;
        char p[512];
        int n = snprintf(p, sizeof p, "%s/%s/refresh_time", root, e->d_name);
        if (n < 0 || (size_t)n >= sizeof p) continue;
        fd = open(p, O_WRONLY | O_CLOEXEC);
        if (fd >= 0) break;
    }
    closedir(dir);
    return fd;
}

static int open_fb(const char *id, size_t expect, struct fbpanel *p) {
    p->fd = -1; p->map = NULL; p->len = 0;
    char path[32];
    if (deluge_find_fb(id, path, sizeof path) != DELUGE_OK) return -1;
    int fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0) return -1;
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0 ||
        ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0) { close(fd); return -1; }
    size_t len = (size_t)fix.line_length * var.yres;
    void *m = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (m == MAP_FAILED) { close(fd); return -1; }
    p->fd = fd; p->map = m; p->len = len;
    if (p->len != expect)
        fprintf(stderr, "deluge: %s fb mapped size %zu != expected %zu — writes will be rejected\n",
                id, p->len, expect);
    return 0;
}

deluge_display_t *deluge_display_new(void) {
    deluge_display_t *d = calloc(1, sizeof *d);
    if (!d) return NULL;
    int a = open_fb("deluge-oled", DELUGE_OLED_BYTES, &d->oled);
    int b = open_fb("deluge-pads", DELUGE_PADS_BYTES, &d->pads);
    if (a < 0 && b < 0) { deluge_display_free(d); return NULL; } /* neither present */
    /* Optional: a missing refresh_time attribute is not a reason to fail the
       whole display — the framebuffers still work, and set_refresh reports
       DELUGE_ERR_NODEV on its own. */
    d->refresh_fd = open_refresh_time();
    return d;
}

static void close_fb(struct fbpanel *p) {
    if (p->map) munmap(p->map, p->len);
    if (p->fd >= 0) close(p->fd);
    p->map = NULL; p->fd = -1; p->len = 0;
}

void deluge_display_free(deluge_display_t *d) {
    if (!d) return;
    close_fb(&d->oled); close_fb(&d->pads);
    if (d->refresh_fd >= 0) close(d->refresh_fd);
    free(d);
}

int deluge_display_pads_set_refresh(deluge_display_t *d, int interval_ms) {
    if (!d) return DELUGE_ERR_STATE;
    /* The kernel rejects >25 too; check here so the error is INVAL rather than
       an opaque write failure, and so the range is documented in one place the
       caller can actually see. */
    if (interval_ms < 0 || interval_ms > 25) return DELUGE_ERR_INVAL;
    if (d->refresh_fd < 0) return DELUGE_ERR_NODEV;
    char buf[8];
    int n = snprintf(buf, sizeof buf, "%d", interval_ms);
    if (n < 0 || (size_t)n >= sizeof buf) return DELUGE_ERR_IO;
    /* pwrite at 0 + truncate, matching leds.c: each sysfs write hits the store
       callback whole, and the truncate is what makes a shorter value fully
       replace a longer one on the host test fixture (a real file). */
    if (pwrite(d->refresh_fd, buf, (size_t)n, 0) != n) return DELUGE_ERR_IO;
    (void)ftruncate(d->refresh_fd, n);
    return DELUGE_OK;
}

static int blit(struct fbpanel *p, const uint8_t *buf, size_t len, size_t expect) {
    if (!p->map) return DELUGE_ERR_NODEV;
    if (len != expect || len > p->len) return DELUGE_ERR_INVAL;
    memcpy(p->map, buf, len); /* deferred-io flushes to hardware */
    return DELUGE_OK;
}

int deluge_display_oled_write(deluge_display_t *d, const uint8_t *buf, size_t len) {
    if (!d) return DELUGE_ERR_STATE;
    return blit(&d->oled, buf, len, DELUGE_OLED_BYTES);
}
int deluge_display_pads_write(deluge_display_t *d, const uint8_t *buf, size_t len) {
    if (!d) return DELUGE_ERR_STATE;
    return blit(&d->pads, buf, len, DELUGE_PADS_BYTES);
}

static uint8_t *fb_map(struct fbpanel *p, size_t *out_len) {
    if (out_len) *out_len = p->len;
    return p->map;
}
uint8_t *deluge_display_oled_map(deluge_display_t *d, size_t *out_len) {
    return d ? fb_map(&d->oled, out_len) : NULL;
}
uint8_t *deluge_display_pads_map(deluge_display_t *d, size_t *out_len) {
    return d ? fb_map(&d->pads, out_len) : NULL;
}

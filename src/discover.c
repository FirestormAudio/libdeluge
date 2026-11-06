/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "discover.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

static const char *input_root(void) {
    const char *r = getenv("DELUGE_INPUT_ROOT");
    return r ? r : "/sys/class/input";
}

int deluge_find_input_event(const char *name, char *out, size_t n) {
    DIR *d = opendir(input_root());
    if (!d) return DELUGE_ERR_NODEV;
    struct dirent *e;
    int rc = DELUGE_ERR_NODEV;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "event", 5) != 0) continue;
        char np[512], buf[128];
        snprintf(np, sizeof np, "%s/%s/device/name", input_root(), e->d_name);
        FILE *f = fopen(np, "r");
        if (!f) continue;
        char *got = fgets(buf, sizeof buf, f);
        fclose(f);
        if (!got) continue;
        buf[strcspn(buf, "\n")] = 0;
        if (strcmp(buf, name) == 0) {
            snprintf(out, n, "/dev/input/%s", e->d_name);
            rc = DELUGE_OK;
            break;
        }
    }
    closedir(d);
    return rc;
}

int deluge_find_fb(const char *id, char *out, size_t n) {
    for (int i = 0; i < 32; i++) {
        char p[32];
        snprintf(p, sizeof p, "/dev/fb%d", i);
        int fd = open(p, O_RDWR);
        if (fd < 0) continue;
        struct fb_fix_screeninfo fix;
        int ok = ioctl(fd, FBIOGET_FSCREENINFO, &fix) == 0 &&
                 strcmp((char *)fix.id, id) == 0;
        close(fd);
        if (ok) { snprintf(out, n, "%s", p); return DELUGE_OK; }
    }
    return DELUGE_ERR_NODEV;
}

static const char *asound_cards_path(void) {
    const char *p = getenv("DELUGE_ASOUND_CARDS");
    return p ? p : "/proc/asound/cards";
}

int deluge_find_card_by_driver(const char *driver, char *out, size_t n) {
    if (!driver || !out || n == 0) return DELUGE_ERR_INVAL;
    FILE *f = fopen(asound_cards_path(), "r");
    if (!f) return DELUGE_ERR_NODEV;
    char line[256];
    int rc = DELUGE_ERR_NODEV;
    while (fgets(line, sizeof line, f)) {
        /* card lines look like: " 4 [UAC2Gadget     ]: UAC2_Gadget - shortname"
           the indented second (longname) line has no "]:" and is skipped. */
        char *lb = strchr(line, '[');
        char *rb = lb ? strchr(lb, ']') : NULL;
        if (!lb || !rb || rb[1] != ':') continue;
        /* id = between '[' and ']', trailing spaces trimmed */
        char id[32];
        size_t idlen = (size_t)(rb - lb - 1);
        if (idlen >= sizeof id) idlen = sizeof id - 1;
        memcpy(id, lb + 1, idlen);
        id[idlen] = 0;
        while (idlen > 0 && id[idlen - 1] == ' ') id[--idlen] = 0;
        /* driver = first whitespace-delimited token after "]:" */
        const char *p = rb + 2;               /* skip "]:" */
        while (*p == ' ') p++;
        char drv[32];
        size_t di = 0;
        while (*p && *p != ' ' && *p != '\n' && di < sizeof drv - 1) drv[di++] = *p++;
        drv[di] = 0;
        if (strcmp(drv, driver) == 0) {
            snprintf(out, n, "%s", id);
            rc = DELUGE_OK;
            break;
        }
    }
    fclose(f);
    return rc;
}

int deluge_find_card_by_drivers(const char *const *drivers, int ndrivers, char *out, size_t n) {
    if (!drivers || !out || n == 0) return DELUGE_ERR_INVAL;
    for (int i = 0; i < ndrivers; i++)
        if (drivers[i] && deluge_find_card_by_driver(drivers[i], out, n) == DELUGE_OK)
            return DELUGE_OK;
    return DELUGE_ERR_NODEV;
}

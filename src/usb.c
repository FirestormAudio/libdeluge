/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#define _GNU_SOURCE
#include "deluge/usb.h"
#include "usb_priv.h"
#include "usb_uevent.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

/* The bus symlink is stable; the device path sits under the DT soc bus. */
#define DEFAULT_ROLE_PATH "/sys/bus/platform/devices/e8010000.usb/role"

struct deluge_usb {
    int role_fd;                 /* -1 if the role attr is absent */
    int nl_fd;                   /* uevent netlink socket, -1 if unavailable */
    int epfd;
    pthread_t thread;
    volatile int running;
    int wake[2];
    deluge_usb_hotplug_cb cb;
    void *ctx;
};

static const char *role_path(void) {
    const char *p = getenv("DELUGE_USB_ROLE_PATH");
    return p ? p : DEFAULT_ROLE_PATH;
}

static int uevent_socket(void) {
    int fd = socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, NETLINK_KOBJECT_UEVENT);
    if (fd < 0) return -1;
    struct sockaddr_nl sa = { .nl_family = AF_NETLINK, .nl_groups = 1 /* kernel uevents */ };
    if (bind(fd, (struct sockaddr *)&sa, sizeof sa) < 0) { close(fd); return -1; }
    return fd;
}

deluge_usb_t *deluge_usb_new(void) {
    int role = open(role_path(), O_RDWR | O_CLOEXEC);
    int nl = uevent_socket();
    if (role < 0 && nl < 0) return NULL;         /* neither sub-feature available */
    deluge_usb_t *u = calloc(1, sizeof *u);
    if (!u) { if (role >= 0) close(role); if (nl >= 0) close(nl); return NULL; }
    u->role_fd = role; u->nl_fd = nl; u->epfd = -1;
    u->wake[0] = u->wake[1] = -1;
    return u;
}

void deluge_usb_free(deluge_usb_t *u) {
    if (!u) return;
    deluge_usb_unwatch(u);                        /* join the thread before fds vanish */
    if (u->role_fd >= 0) close(u->role_fd);
    if (u->nl_fd >= 0) close(u->nl_fd);
    free(u);
}

int deluge_usb_role_get(deluge_usb_t *u, deluge_usb_role *out) {
    if (!u) return DELUGE_ERR_STATE;
    if (!out) return DELUGE_ERR_INVAL;
    if (u->role_fd < 0) return DELUGE_ERR_NODEV;
    char buf[32];
    ssize_t n = pread(u->role_fd, buf, sizeof buf - 1, 0);
    if (n < 0) return DELUGE_ERR_IO;
    buf[n] = 0;
    buf[strcspn(buf, "\n")] = 0;
    if (strcmp(buf, "host") == 0)       { *out = DELUGE_USB_HOST; return DELUGE_OK; }
    if (strcmp(buf, "peripheral") == 0) { *out = DELUGE_USB_PERIPHERAL; return DELUGE_OK; }
    return DELUGE_ERR_IO;
}

int deluge_usb_role_set(deluge_usb_t *u, deluge_usb_role role) {
    if (!u) return DELUGE_ERR_STATE;
    if (u->role_fd < 0) return DELUGE_ERR_NODEV;
    const char *s = (role == DELUGE_USB_HOST) ? "host" : "peripheral";
    size_t len = strlen(s);
    if (pwrite(u->role_fd, s, len, 0) != (ssize_t)len) return DELUGE_ERR_IO;
    (void)ftruncate(u->role_fd, (off_t)len);
    return DELUGE_OK;
}

int deluge_usb_fd(deluge_usb_t *u) { return u ? u->nl_fd : -1; }

/* Read one datagram, classify, deliver if it's a USB-plugged device. */
static void drain_once(deluge_usb_t *u) {
    char buf[2048];
    for (;;) {
        ssize_t n = recv(u->nl_fd, buf, sizeof buf - 1, 0);
        if (n <= 0) { break; } /* EAGAIN or closed */
        buf[n] = 0;
        struct deluge_uevent ev;
        if (deluge_uevent_parse(buf, (size_t)n, &ev) != DELUGE_OK) continue;
        if (!ev.devname) continue;                       /* only device-node events */
        if (!deluge_uevent_is_usb(ev.devpath, deluge_usb_host_marker())) continue;
        deluge_usb_class cls = deluge_usb_classify(ev.subsystem, ev.devname);
        if (cls == DELUGE_USB_CLASS_OTHER) continue;
        deluge_usb_hotplug act = (strcmp(ev.action, "remove") == 0)
                                     ? DELUGE_USB_REMOVED : DELUGE_USB_ADDED;
        char node[128];
        snprintf(node, sizeof node, "/dev/%s", ev.devname);
        u->cb(act, cls, node, u->ctx);
    }
}

static void *usb_loop(void *arg) {
    deluge_usb_t *u = arg;
    struct epoll_event evs[2];
    while (u->running) {
        int nready = epoll_wait(u->epfd, evs, 2, -1);
        if (nready < 0) { if (errno == EINTR) continue; break; }
        for (int i = 0; i < nready; i++)
            if (evs[i].data.u32 == 0) drain_once(u);   /* 0 = netlink; 1 = wake */
    }
    return NULL;
}

int deluge_usb_watch(deluge_usb_t *u, deluge_usb_hotplug_cb cb, void *ctx) {
    if (!u || !cb) return DELUGE_ERR_INVAL;
    if (u->nl_fd < 0) return DELUGE_ERR_NODEV;
    if (u->running) return DELUGE_ERR_STATE;
    u->epfd = epoll_create1(EPOLL_CLOEXEC);
    if (u->epfd < 0) return DELUGE_ERR_IO;
    if (pipe2(u->wake, O_CLOEXEC | O_NONBLOCK) < 0) { close(u->epfd); u->epfd = -1; return DELUGE_ERR_IO; }
    struct epoll_event ne = { .events = EPOLLIN, .data.u32 = 0 };
    struct epoll_event we = { .events = EPOLLIN, .data.u32 = 1 };
    if (epoll_ctl(u->epfd, EPOLL_CTL_ADD, u->nl_fd, &ne) < 0 ||
        epoll_ctl(u->epfd, EPOLL_CTL_ADD, u->wake[0], &we) < 0) goto fail;
    u->cb = cb; u->ctx = ctx; u->running = 1;
    if (pthread_create(&u->thread, NULL, usb_loop, u) != 0) { u->running = 0; goto fail; }
    return DELUGE_OK;
fail:
    close(u->wake[0]); close(u->wake[1]); u->wake[0] = u->wake[1] = -1;
    close(u->epfd); u->epfd = -1;
    return DELUGE_ERR_IO;
}

void deluge_usb_unwatch(deluge_usb_t *u) {
    if (!u || !u->running) return;
    u->running = 0;
    char b = 1; ssize_t w = write(u->wake[1], &b, 1); (void)w;
    pthread_join(u->thread, NULL);
    close(u->wake[0]); close(u->wake[1]); u->wake[0] = u->wake[1] = -1;
    close(u->epfd); u->epfd = -1;
}

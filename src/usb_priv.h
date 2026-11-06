/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef DELUGE_USB_PRIV_H
#define DELUGE_USB_PRIV_H
#include "deluge/usb.h"
deluge_usb_t *deluge_usb_new(void); /* NULL only if neither the role attr nor the uevent netlink socket opens */
void          deluge_usb_free(deluge_usb_t *);
#endif

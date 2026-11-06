/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef DELUGE_CV_PRIV_H
#define DELUGE_CV_PRIV_H
#include "deluge/cv.h"
deluge_cv_t *deluge_cv_new(void); /* NULL if neither CV nor gates available */
void         deluge_cv_free(deluge_cv_t *);
#endif

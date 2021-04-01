/*                      B V I E W _ U T I L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup bview
 *
 * @brief
 * Functions related to bview.h
 *
 */
#ifndef DM_BVIEW_UTIL_H
#define DM_BVIEW_UTIL_H

#include "common.h"
#include "bn/tol.h"
#include "dm/defines.h"
#include "dm/bview.h"
#include "dm/bview_util.h"

/** @{ */
/** @file bview_util.h */

__BEGIN_DECLS

/* Set default values for a bview. */
DM_EXPORT extern void bview_init(struct bview *v);

/* Sync values within the bview, perform callbacks if any are defined */
DM_EXPORT extern void bview_update(struct bview *gvp);

/* Return 1 if the visible contents differ
 * Return 2 if visible content is the same but settings differ
 * Return 3 if content is the same but user data, dmp or callbacks differ
 * Return -1 if one or more of the views is NULL
 * Else return 0 */
DM_EXPORT extern int bview_differ(struct bview *v1, struct bview *v2);

/* Return a hash of the contents of the bview container.  Returns 0 on
 * failure. */
DM_EXPORT extern unsigned long long bview_hash(struct bview *v);

/* Return -1 if sync fails, else 0 */
//DM_EXPORT extern int bview_sync(struct bview *dest, struct bview *src);


/* View manipulations fall into several categories - we define flags which can
 * be combined to specify various modes. */
#define BVIEW_VIEW       0
#define BVIEW_MODEL      1
#define BVIEW_EDIT       2
#define BVIEW_ADC        3

/* Note that some of these are mutually exclusive as far as producing any
 * changes - a simultaneous constraint in X and Y, for example, results in a
 * no-op. */
#define BVIEW_IDLE       0x0
#define BVIEW_ROT        0x1
#define BVIEW_TRANS      0x2
#define BVIEW_SCALE      0x4
#define BVIEW_CON_X      0x8
#define BVIEW_CON_Y      0x10
#define BVIEW_CON_Z      0x20
#define BVIEW_CON_GRID   0x40
#define BVIEW_CON_LINES  0x80

/* Update a view in response to X,Y coordinate changes as generated
 * by a graphical interface's mouse motion. */
DM_EXPORT extern int bview_adjust(struct bview *v, int dx, int dy, point_t keypoint, int mode, unsigned long long flags);

__END_DECLS

/** @} */

#endif /* DM_BVIEW_UTIL_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

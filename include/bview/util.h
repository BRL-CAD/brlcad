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
#ifndef BVIEW_UTIL_H
#define BVIEW_UTIL_H

#include "common.h"
#include "bn/tol.h"
#include "dm/defines.h"
#include "bview/defines.h"

/** @{ */
/** @file bview/util.h */

__BEGIN_DECLS

/* Set default values for a bview. */
BVIEW_EXPORT extern void bview_init(struct bview *v);

/* Sync values within the bview, perform callbacks if any are defined */
BVIEW_EXPORT extern void bview_update(struct bview *gvp);

/* Update objects in the selection set (if any) and their children */
BVIEW_EXPORT extern int bview_update_selected(struct bview *gvp);

/* Return 1 if the visible contents differ
 * Return 2 if visible content is the same but settings differ
 * Return 3 if content is the same but user data, dmp or callbacks differ
 * Return -1 if one or more of the views is NULL
 * Else return 0 */
BVIEW_EXPORT extern int bview_differ(struct bview *v1, struct bview *v2);

/* Return a hash of the contents of the bview container.  Returns 0 on failure. */
BVIEW_EXPORT extern unsigned long long bview_hash(struct bview *v);

/* Return a hash of the contents of a display list.  Returns 0 on failure. */
BVIEW_EXPORT extern unsigned long long bview_dl_hash(struct display_list *dl);

/* Return -1 if sync fails, else 0 */
//BVIEW_EXPORT extern int bview_sync(struct bview *dest, struct bview *src);

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
BVIEW_EXPORT extern int bview_adjust(struct bview *v, int dx, int dy, point_t keypoint, int mode, unsigned long long flags);

/* Snap sample 2D point to lines active in the view */
BVIEW_EXPORT extern int bview_snap_lines_2d(struct bview *v, fastf_t *fx, fastf_t *fy);

/* Snap sample 2D point to grid active in the view */
BVIEW_EXPORT extern int bview_snap_grid_2d(struct bview *v, fastf_t *fx, fastf_t *fy);

/* Snap sample 3D point to lines active in the view */
BVIEW_EXPORT extern int bview_snap_lines_3d(point_t *out_pt, struct bview *v, point_t *p);


BVIEW_EXPORT extern void bview_view_center_linesnap(struct bview *v);

/* Beginning extraction of the core of libtclcad view object manipulation logic.
 * The following functions will initially be pretty straightforward mappings
 * from libtclcad, and will likely evolve over time. */

BVIEW_EXPORT extern void bview_screen_to_view(struct bview *v, fastf_t *fx, fastf_t *fy, fastf_t x, fastf_t y);


__END_DECLS

/** @} */

#endif /* BVIEW_UTIL_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

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
/** @addtogroup bv
 *
 * @brief
 * Functions related to bv.h
 *
 */
#ifndef BV_UTIL_H
#define BV_UTIL_H

#include "common.h"
#include "bn/tol.h"
#include "dm/defines.h"
#include "bv/defines.h"

/** @{ */
/** @file bv/util.h */

__BEGIN_DECLS

/* Set default values for a bv. */
BV_EXPORT extern void bv_init(struct bview *v);
BV_EXPORT extern void bv_free(struct bview *v);

BV_EXPORT extern void bv_settings_init(struct bview_settings *s);

/* Copy the size and camera info (deliberately not a full copy of all view state) */
BV_EXPORT extern void bv_sync(struct bview *dest, struct bview *src);

/* Copy settings (potentially) common to the view and scene objects */
BV_EXPORT extern void bv_obj_settings_sync(struct bv_obj_settings *dest, struct bv_obj_settings *src);

/* Sync values within the bv, perform callbacks if any are defined */
BV_EXPORT extern void bv_update(struct bview *gvp);

/* Update objects in the selection set (if any) and their children */
BV_EXPORT extern int bv_update_selected(struct bview *gvp);

/* Return 1 if the visible contents differ
 * Return 2 if visible content is the same but settings differ
 * Return 3 if content is the same but user data, dmp or callbacks differ
 * Return -1 if one or more of the views is NULL
 * Else return 0 */
BV_EXPORT extern int bv_differ(struct bview *v1, struct bview *v2);

/* Return a hash of the contents of the bv container.  Returns 0 on failure. */
BV_EXPORT extern unsigned long long bv_hash(struct bview *v);

/* Return a hash of the contents of a display list.  Returns 0 on failure. */
BV_EXPORT extern unsigned long long bv_dl_hash(struct display_list *dl);


/* Note that some of these are mutually exclusive as far as producing any
 * changes - a simultaneous constraint in X and Y, for example, results in a
 * no-op. */
#define BV_IDLE       0x0
#define BV_ROT        0x1
#define BV_TRANS      0x2
#define BV_SCALE      0x4
#define BV_CON_X      0x8
#define BV_CON_Y      0x10
#define BV_CON_Z      0x20
#define BV_CON_GRID   0x40
#define BV_CON_LINES  0x80

/* Update a view in response to X,Y coordinate changes as generated
 * by a graphical interface's mouse motion. */
BV_EXPORT extern int bv_adjust(struct bview *v, int dx, int dy, point_t keypoint, int mode, unsigned long long flags);

/* Beginning extraction of the core of libtclcad view object manipulation
 * logic.  The following functions will initially be pretty straightforward
 * mappings from libtclcad, and will likely evolve over time.
 */

/* Return -1 if width and/or height are unset (and hence a meaningful
 * calculation is impossible), else 0. */
BV_EXPORT extern int bv_screen_to_view(struct bview *v, fastf_t *fx, fastf_t *fy, fastf_t x, fastf_t y);

/* Initialize a scene object to standard default settings */
BV_EXPORT extern void bv_scene_obj_init(struct bv_scene_obj *s, struct bv_scene_obj *free_scene_obj);

/* Free the object contents, including all child objects */
BV_EXPORT extern void bv_scene_obj_free(struct bv_scene_obj *s, struct bv_scene_obj *free_scene_obj);

/* Compute the min, max, and center points of the scene object. */
BV_EXPORT extern void bv_scene_obj_bound(struct bv_scene_obj *s);

/* Find the nearest (mode == 0) or farthest (mode == 1) data_vZ value from
 * the vlist points in s in the context of view v */
BV_EXPORT extern fastf_t bv_vZ_calc(struct bv_scene_obj *s, struct bview *v, int mode);

__END_DECLS

/** @} */

#endif /* BV_UTIL_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

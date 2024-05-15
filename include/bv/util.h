/*                      B V I E W _ U T I L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2024 United States Government as represented by
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
/** @addtogroup bv_util
 *
 */
/** @{ */
/** @file bv/util.h */

#ifndef BV_UTIL_H
#define BV_UTIL_H

#include "common.h"
#include "bn/tol.h"
#include "dm/defines.h"
#include "bv/defines.h"

__BEGIN_DECLS

/* Set default values for a bv. */
BV_EXPORT extern void bv_init(struct bview *v, struct bview_set *s);
BV_EXPORT extern void bv_free(struct bview *v);

/**
 * FIXME: this routine is suspect and needs investigating.  if run
 * during view initialization, the shaders regression test fails.
 */
BV_EXPORT void bv_mat_aet(struct bview *v);

BV_EXPORT extern void bv_settings_init(struct bview_settings *s);

/* To use default scaling (0.5 model scale == 2.0 view factor) use
 * this as an argument to bv_autoview's scale parameter */
#define BV_AUTOVIEW_SCALE_DEFAULT -1
/**
 * Automatically set up the view to make the scene objects visible
 */
BV_EXPORT extern void bv_autoview(struct bview *v, fastf_t scale, int all_view_objs);

/* Copy the size and camera info (deliberately not a full copy of all view state) */
BV_EXPORT extern void bv_sync(struct bview *dest, struct bview *src);

/* Copy settings (potentially) common to the view and scene objects.
 * Return 0 if no changes were made to dest.  If dest did have one
 * or more settings updated from src, return 1. */
BV_EXPORT extern int bv_obj_settings_sync(struct bv_obj_settings *dest, struct bv_obj_settings *src);

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

/* Returns number of objects defined in any object container
 * known to this view (0 if completely cleared). */
BV_EXPORT extern size_t bv_clear(struct bview *v, int flags);

/* Note that some of these are mutually exclusive as far as producing any
 * changes - a simultaneous constraint in X and Y, for example, results in a
 * no-op. */
#define BV_IDLE       0x000
#define BV_ROT        0x001
#define BV_TRANS      0x002
#define BV_SCALE      0x004
#define BV_CENTER     0x008
#define BV_CON_X      0x010
#define BV_CON_Y      0x020
#define BV_CON_Z      0x040
#define BV_CON_GRID   0x080
#define BV_CON_LINES  0x100

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

/* Return -1 if width and/or height are unset (and hence a meaningful
 * calculation is impossible), else 0.
 *
 * x and y will normally be integers, but the types are float to allow for
 * the possibility of sub-pixel coordinate specifications.
 */
BV_EXPORT extern int bv_screen_pt(point_t *p, fastf_t x, fastf_t y, struct bview *v);



/* Compute the min, max, and center points of the scene object.
 * Return 1 if a bound was computed, else 0 */
BV_EXPORT extern int bv_scene_obj_bound(struct bv_scene_obj *s, struct bview *v);

/* Find the nearest (mode == 0) or farthest (mode == 1) data_vZ value from
 * the vlist points in s in the context of view v */
BV_EXPORT extern fastf_t bv_vZ_calc(struct bv_scene_obj *s, struct bview *v, int mode);

/* Copy object attributes (but not geometry) from src to dest */
BV_EXPORT extern void bv_obj_sync(struct bv_scene_obj *dest, struct bv_scene_obj *src);

/* Mark object and any child objects as stale for the drawing routines */
/* There are a few options for this situation - this one, which requires the client code
 * to explicitly notify the drawing routines they need to do work, an internal options
 * hash stored in the bv_scene_obj itself which is checked at render time, and setter
 * wrapper functions that do the bookkeeping for the caller (in lieu of directly setting
 * values in the bv_scene_obj struct.)  The first one isn't ideal because the visual will
 * be wrong if the caller doesn't supply the notification, the second has unknown
 * performance implications, and the third would be a major rework of how the bv_scene_obj
 * data is accessed (effectively, making the internal storage of bv_scene_obj fully hidden
 * a.l.a the libdm rework.)  Not sure what the best option is yet... leaning towards #2
 * if it is "fast enough"... */
BV_EXPORT void bv_obj_stale(struct bv_scene_obj *s);

/* Given a view, create an object of the specified type.  Like bv_obj_get, except it
 * leaves the addition of objects to the client.  Lower level. */
BV_EXPORT struct bv_scene_obj *
bv_obj_create(struct bview *v, int type);

/* Given a view, create an object of the specified type and add it to the
 * appropriate container.  Issues such as memory management as a function of
 * view settings are handled internally, so client codes don't need to manage
 * it. */
BV_EXPORT struct bv_scene_obj *
bv_obj_get(struct bview *v, int type);

/* Given an object, create an object that is a child of that object.  Issues
 * such as memory management as a function of view settings are handled
 * internally, so client codes don't need to manage it. */
BV_EXPORT struct bv_scene_obj *
bv_obj_get_child(struct bv_scene_obj *s);

/* Clear the contents of an object (including releasing its children), but keep
 * it active in the view.  Generally used when redrawing an object */
BV_EXPORT void
bv_obj_reset(struct bv_scene_obj *s);

/* Release an object to the internal pools. */
BV_EXPORT void
bv_obj_put(struct bv_scene_obj *o);

/* Given a scene object and a name vname, glob match child names and uuids to
 * attempt to locate a child of s that matches vname */
BV_EXPORT struct bv_scene_obj *
bv_find_child(struct bv_scene_obj *s, const char *vname);

/* Given a view and a name vname, glob match names and uuids to attempt to
 * locate a scene object in v that matches vname.
 *
 * NOTE - currently this is searching the top level objects, but does not walk
 * down into their children.  May want to support that in the future... */
BV_EXPORT struct bv_scene_obj *
bv_find_obj(struct bview *v, const char *vname);

/* Given a seed name, generate a name that does not collide with any existing
 * object names in the top level.  If the seed name does not collide, it is
 * returned as the result - otherwise, a name based on the seed name will be
 * generated.
 */
BV_EXPORT void
bv_uniq_obj_name(struct bu_vls *oname, const char *seed, struct bview *v);

/* For the specified object/view pairing, return the appropriate scene object
 * to use with that view.  Usually this will return s, but if a Level of Detail
 * scheme or some other view-aware rendering of the object is active, that object
 * will be returned instead. */
BV_EXPORT struct bv_scene_obj *
bv_obj_for_view(struct bv_scene_obj *s, struct bview *v);

/* Get a view-specific object vobj for view v on object s.  */
BV_EXPORT struct bv_scene_obj *
bv_obj_get_vo(struct bv_scene_obj *s, struct bview *v);

/* Check for the presence of view-specific objects */
BV_EXPORT int
bv_obj_have_vo(struct bv_scene_obj *s, struct bview *v);

/* Clear view-specific objects */
BV_EXPORT int
bv_clear_view_obj(struct bv_scene_obj *s, struct bview *v);

/* Set the illumination state on the object and its children to ill_state.
 * Returns 0 if no states were changed, and 1 if one or more states were
 * updated. */
BV_EXPORT int
bv_illum_obj(struct bv_scene_obj *s, char ill_state);

/* For the given view, return a pointer to the bu_ptbl holding active scene
 * objects with the specified type.  Note that view-specific db objects are not
 * part of these sets - they should be retrieved from the scene objects in this
 * set with bv_obj_for_view. */
BV_EXPORT struct bu_ptbl *
bv_view_objs(struct bview *v, int type);

/* Given a view, construct the view plane */
BV_EXPORT int
bv_view_plane(plane_t *p, struct bview *v);


/* Environment variable controlled logging.
 *
 * Set BV_LOG to numerical levels to get increasingly
 * verbose reporting of drawing info */
#define BV_ENABLE_ENV_LOGGING 1
BV_EXPORT void
bv_log(int level, const char *fmt, ...)  _BU_ATTR_PRINTF23;


/* Debugging function for printing contents of views */
BV_EXPORT void
bv_view_print(const char *title, struct bview *v, int verbosity);

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

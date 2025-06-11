/*                      B V I E W _ U T I L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2025 United States Government as represented by
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
BV_EXPORT extern int bv_autoview(struct bview *v, struct bu_ptbl *o, fastf_t scale);

/* Copy the size and camera info (deliberately not a full copy of all view state) */
BV_EXPORT extern void bv_sync(struct bview *dest, struct bview *src);

/* Copy settings (potentially) common to the view and scene objects.
 * Return 0 if no changes were made to dest.  If dest did have one
 * or more settings updated from src, return 1. */
BV_EXPORT extern int bv_obj_settings_sync(struct bv_obj_settings *dest, struct bv_obj_settings *src);

/* Sync values within the bv, perform callbacks if any are defined */
BV_EXPORT extern void bv_update(struct bview *gvp, struct bu_ptbl *objs);

/* Update objects in the selection set (if any) and their children */
BV_EXPORT extern int bv_update_selected(struct bview *gvp);

/* Clear or reset the knob states.  Specify a category to indicate which
 * variables should be reset:
 *
 * BV_KNOBS_ALL resets both rate and absolute values
 * BV_KNOBS_RATE resets rate only
 * BV_KNOBS_ABS resets absolute only
 */
#define BV_KNOBS_ALL 0
#define BV_KNOBS_RATE 1
#define BV_KNOBS_ABS 2
BV_EXPORT extern void bv_knobs_reset(struct bview_knobs *k, int category);

/**
 * @brief
 * Process an individual libbv knob command.
 *
 * Note that the reason rvec, do_rot, tvec and do_tran are set, rather than an
 * immediate view update being performed, is to allow parent applications to
 * process multiple commands before finally triggering the bv_knobs_rot or
 * bv_knobs_tran functions to implement the accumulated instructions.
 *
 * @param[out] rvec     Pointer to rotation vector
 * @param[out] do_rot   Pointer to flag indicating whether the command implies a rotation op is needed
 * @param[out] tvec     Pointer to translation vector
 * @param[out] do_tran  Pointer to flag indicating whether the command implies a translation op is needed
 *
 * @param[in] v          bview structure
 * @param[in] cmd        command string - valid entries are x, y, z, X, Y Z, ax, ay, az, aX, aY, aZ, S, aS
 * @param[in] f          numerical parameter to cmd (i.e. aX 0.1 - required for all commands)
 * @param[in] origin     char indicating origin - may be 'e' (eye_pt), 'm' (model origin) or 'v' (view origin - default)
 * @param[in] model_flag Manipulate view using model coordinates rather than view coordinates
 * @param[in] incr_flag  Treat f parameter as an incremental change rather than an absolute setting
 *
 * @return
 * Returns BRLCAD_OK if command was successfully processed, BRLCAD_ERROR otherwise.
 * */
BV_EXPORT extern int bv_knobs_cmd_process(
	vect_t *rvec, int *do_rot, vect_t *tvec, int *do_tran,
        struct bview *v, const char *cmd, fastf_t f,
        char origin, int model_flag, int incr_flag
	);

/* @brief
 * Process a knob rotation vector.
 *
 * @param[in] v          bview structure
 * @param[in] rvec      Pointer to rotation vector
 * @param[in] origin     char indicating origin - may be 'e' (eye_pt), 'm' (model origin) or 'v' (view origin - default)
 * @param[in] model_flag Manipulate view using model coordinates rather than view coordinates
 */
BV_EXPORT extern void
bv_knobs_rot(struct bview *v,
	vect_t *rvec,
	char origin,
	int model_flag);


/* @brief
 * Process a knob translation vector.
 *
 * @param[in] v          bview structure
 * @param[in] tvec      Pointer to translation vector
 * @param[in] model_flag Manipulate view using model coordinates rather than view coordinates
 */
BV_EXPORT extern void
bv_knobs_tran(struct bview *v,
	vect_t *tvec,
	int model_flag);


/* Update the bview struct's knob rate flags based on the vector values. */
BV_EXPORT extern void
bv_update_rate_flags(struct bview *v);

/* Return a hash of the contents of the bv container.  Returns 0 on failure. */
BV_EXPORT extern unsigned long long bv_hash(struct bview *v);

/* Return a hash of the contents of a display list.  Returns 0 on failure. */
BV_EXPORT extern unsigned long long bv_dl_hash(struct display_list *dl);

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
BV_EXPORT extern int bv_scene_obj_bound(struct bv_scene_obj *s);

/* Find the nearest (mode == 0) or farthest (mode == 1) data_vZ value from
 * the vlist points in s in the context of view v */
BV_EXPORT extern fastf_t bv_vZ_calc(struct bv_scene_obj *s, struct bview *v, int mode);

/* Copy object attributes (but not geometry) from src to dest */
BV_EXPORT extern void bv_obj_sync(struct bv_scene_obj *dest, struct bv_scene_obj *src);

/* Create an object, either by obtaining memory from free_scene_objs or
 * mallocing it. */
BV_EXPORT struct bv_scene_obj *
bv_obj_get(struct bv_scene_obj *free_scene_objs, struct bu_list *vlfree);

BV_EXPORT void
bv_obj_add_child(struct bv_scene_obj *o, struct bv_scene_obj *co);

/* Clear the contents of an object. */
BV_EXPORT void
bv_obj_reset(struct bv_scene_obj *s);

/* Release an object to it's free_scene_objs pool.  or (if free_scene_objs is
 * NULL) free it. */
BV_EXPORT void
bv_obj_put(struct bv_scene_obj *o);

/* Given a scene object and a name vname, glob match child names and uuids to
 * attempt to locate a child of s that matches vname */
BV_EXPORT struct bv_scene_obj *
bv_find_child(struct bv_scene_obj *s, const char *vname);

/* Set the illumination state on the object and its children to ill_state.
 * Returns 0 if no states were changed, and 1 if one or more states were
 * updated. */
BV_EXPORT int
bv_illum_obj(struct bv_scene_obj *s, char ill_state);

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

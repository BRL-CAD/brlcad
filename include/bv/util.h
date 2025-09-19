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
#include "bu/hash.h"
#include "bu/ptbl.h"
#include "bn/tol.h"
#include "bv/defines.h"
#include "dm/defines.h"

__BEGIN_DECLS

/* Set default values for a bv. */
BV_EXPORT extern void bv_init(struct bview *v);
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
BV_EXPORT extern void bv_autoview(struct bview *v, const struct bu_ptbl *so, fastf_t scale);

/* Copy the size and camera info (deliberately not a full copy of all view state) */
BV_EXPORT extern void bv_sync(struct bview *dest, struct bview *src);

/* Sync values within the bv, perform callbacks if any are defined */
BV_EXPORT extern void bv_update(struct bview *gvp);

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

/* Hash the semantic (non-pointer) contents of a bview_knobs struct.  This
 * intentionally excludes any *_udata pointers to avoid pointer address noise
 * and skips any padding that would be present if hashing the raw struct.
 *
 * If 'state' is non-NULL the supplied hash state is updated in-place and the
 * function returns 0 (the caller is expected to finalize the hash later.)
 * If 'state' is NULL an internal hash state is created, populated and
 * finalized and the resulting hash value is returned.
 *
 * Returns:
 *   0     if k is NULL or state supplied (non-owning mode)
 *   hash  if state is NULL (owning mode)
 */
BV_EXPORT extern unsigned long long
bv_knobs_hash(struct bview_knobs *k, struct bu_data_hash_state *state);

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

/**
 * @brief
 * Rotate the view based on an Euler angle triplet (degrees) specified
 * in one of several coordinate frames, about one of several origins.
 *
 * coords:  Rotation input frame
 *   'v' - rvec in view coordinates
 *   'm' - rvec in model coordinates (converted via Rv * Rm * Rv^{-1})
 *   'o' - rvec in object coordinates (use obj_rot to map object->model->view,
 *         fallback to 'v' semantics if obj_rot is NULL)
 *
 * origin:  Rotation pivot specifier
 *   'v' : view center (0,0,0 in view space)
 *   'm' : model origin
 *   'e' : eye point (0,0,1 in view space)
 *   'k' : model-space custom pivot supplied via pvt_pt
 *   Any unrecognized value falls back to 'v'.
 *
 * obj_rot:
 *   Accumulated object->model rotation matrix when coords=='o'. NULL otherwise.
 *
 * pvt_pt:
 *   Model-space pivot point when origin=='k'. Ignored otherwise.
 *   NULL == model origin.
 *
 * BEHAVIOR
 * 1. rvec is converted into a pure view-space rotation matrix according
 *    to 'coords' (and obj_rot for 'o').
 * 2. If origin != 'v', the view center (gv_center) is relocated so the
 *    specified pivot is invariant under the applied rotation.
 * 3. gv_rotation is post-multiplied by the view rotation matrix.
 * 4. bv_update(v) refreshes derived matrices. Absolute translation bookkeeping
 *    (tra_v_abs / tra_m_abs) is always recomputed
 *
 * @param[in,out] v      target bview structure
 * @param[in] rvec       rotation vector (Euler angles, degrees) expressed in the coordinate frame indicated by coords.
 * @param[in] origin     char indicating origin - may be 'e' (eye_pt), 'm' (model origin), 'v' (view origin - default) or 'k' (keypoint)
 * @param[in] coords     coordinate frame - may be 'm' (model), 'o' (obj coords via obj_rot), or 'v' (view)
 * @param[in] obj_rot    pointer to accumulated object rotation matrix (may be NULL)
 * @param[in] pvt_pt     model space pivot point
 */
BV_EXPORT extern void
bv_knobs_rot(struct bview *v,
	const vect_t rvec,
	char origin,
	char coords,
	const matp_t obj_rot,
	const pointp_t pvt_pt);

/* @brief
 * Process a knob translation vector.
 *
 * @param[in] v          bview structure
 * @param[in] tvec      Pointer to translation vector
 * @param[in] model_flag Manipulate view using model coordinates rather than view coordinates
 */
BV_EXPORT extern void
bv_knobs_tran(struct bview *v,
	const vect_t tvec,
	int model_flag);


/* Update the bview struct's knob rate flags based on the vector values. */
BV_EXPORT extern void
bv_update_rate_flags(struct bview *v);


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
BV_EXPORT extern int bv_screen_to_view(const struct bview *v, fastf_t *fx, fastf_t *fy, fastf_t x, fastf_t y);

/* Return -1 if width and/or height are unset (and hence a meaningful
 * calculation is impossible), else 0.
 *
 * x and y will normally be integers, but the types are float to allow for
 * the possibility of sub-pixel coordinate specifications.
 */
BV_EXPORT extern int bv_screen_pt(point_t *p, fastf_t x, fastf_t y, struct bview *v);

/* Find the nearest (mode == 0) or farthest (mode == 1) data_vZ value from
 * the vlist points in s in the context of view v */
BV_EXPORT extern fastf_t bv_vZ_calc(struct bv_scene_obj *s, struct bview *v, int mode);

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

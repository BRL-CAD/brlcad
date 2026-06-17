/*                         U T I L . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @addtogroup libbsg
 *
 * @brief
 * Scene-graph lifecycle and query utilities.
 */
/** @{ */
/* @file bsg/util.h */

#ifndef BSG_UTIL_H
#define BSG_UTIL_H

#include "common.h"
#include "bu/hash.h"
#include "bn/tol.h"
#include "bsg/defines.h"
#include "bsg/scene_builder.h"

__BEGIN_DECLS

/**
 * Initialize a BSG view record.  BSG wrapper around
 * bsg_init().  @p v must point to allocated but uninitialized storage.
 * @p s is the optional view-set the view belongs to; pass NULL when unused.
 */
BSG_EXPORT extern void
bsg_view_init(struct bsg_view *v, struct bsg_view_set *s);

/**
 * Initialize @p dest as a fresh BSG view and copy semantic view state from
 * @p src.  Owned storage such as scene-object pools, callback recursion
 * guards, view-state records, local settings records, and retained scene
 * handles are newly initialized for @p dest rather than shared with @p src.
 * @p s is the optional view-set the new view belongs to.
 */
BSG_EXPORT extern void
bsg_view_init_copy(struct bsg_view *dest,
		   const struct bsg_view *src,
		   struct bsg_view_set *s);

/**
 * Free resources owned by a BSG view record.  BSG wrapper around bsg_free().
 * Does not free the memory for @p v itself.
 */
BSG_EXPORT extern void
bsg_view_free(struct bsg_view *v);

/**
 * Return the table of views registered in the view-set @p s.
 * BSG-namespaced wrapper around bsg_set_views().
 * Returns NULL when @p s is NULL.
 */
BSG_EXPORT extern struct bu_ptbl *
bsg_set_views(struct bsg_view_set *s);

__END_DECLS

/* =========================================================================
 * Legacy bv_* API declarations
 *
 * The following declarations were previously in bv/util.h.  bv/util.h is
 * now a backward-compatibility bridge that includes this header.  All
 * callers should migrate to bsg/util.h.  The bv_* names will be retired
 * once the struct renames (bsg_view → bsg_view, etc.) are complete.
 * ========================================================================= */

__BEGIN_DECLS

/* Set default values for a bv. */
BSG_EXPORT extern void bsg_init(struct bsg_view *v, struct bsg_view_set *s);
BSG_EXPORT extern void bsg_free(struct bsg_view *v);

/* Zero-initialize a bsg_data_tclcad
 * block. */
BSG_EXPORT extern void bsg_data_tclcad_init(struct bsg_data_tclcad *d);
BSG_EXPORT extern int bsg_view_is_independent(const struct bsg_view *v);
BSG_EXPORT extern bsg_scene_ref bsg_view_independent_scope_ref(struct bsg_view *v, int create);
BSG_EXPORT extern void bsg_view_independent_scope_destroy(struct bsg_view *v);

BSG_EXPORT void bsg_mat_aet(struct bsg_view *v);

#define BSG_AUTOVIEW_SCALE_DEFAULT -1
BSG_EXPORT extern void bsg_autoview(struct bsg_view *v, fastf_t scale, int all_view_objs);
BSG_EXPORT extern void bsg_autoview_bounds(struct bsg_view *v, fastf_t scale, const point_t min, const point_t max);

/* Copy the size and camera info */
BSG_EXPORT extern void bsg_sync(struct bsg_view *dest, struct bsg_view *src);

/* Camera accessor functions */
BSG_EXPORT extern fastf_t bsg_view_get_scale(const struct bsg_view *v);
BSG_EXPORT extern void    bsg_view_set_scale(struct bsg_view *v, fastf_t scale);
BSG_EXPORT extern fastf_t bsg_view_get_size(const struct bsg_view *v);
BSG_EXPORT extern void    bsg_view_set_size(struct bsg_view *v, fastf_t size);
BSG_EXPORT extern fastf_t bsg_view_get_perspective(const struct bsg_view *v);
BSG_EXPORT extern void    bsg_view_set_perspective(struct bsg_view *v, fastf_t perspective);
BSG_EXPORT extern void bsg_view_get_aet(const struct bsg_view *v, vect_t aet);
BSG_EXPORT extern void bsg_view_set_aet(struct bsg_view *v, const vect_t aet);
BSG_EXPORT extern void bsg_view_get_rotation(const struct bsg_view *v, mat_t rot);
BSG_EXPORT extern void bsg_view_set_rotation(struct bsg_view *v, const mat_t rot);
BSG_EXPORT extern void bsg_view_get_center_vec(const struct bsg_view *v, point_t center);
BSG_EXPORT extern void bsg_view_set_center_vec(struct bsg_view *v, const point_t center);

/* Copy settings common to views and shape nodes */
BSG_EXPORT extern int bsg_appearance_settings_sync(struct bsg_appearance_settings *dest, const struct bsg_appearance_settings *src);

/* Sync values within the bv, perform callbacks if any are defined */
BSG_EXPORT extern void bsg_update(struct bsg_view *gvp);

/* Update objects in the selection set (if any) and their children */
BSG_EXPORT extern int bsg_update_selected(struct bsg_view *gvp);

/* Clear or reset the knob states. */
#ifndef BSG_KNOBS_ALL
#define BSG_KNOBS_ALL 0
#define BSG_KNOBS_RATE 1
#define BSG_KNOBS_ABS 2
#endif
BSG_EXPORT extern void bsg_knobs_reset(struct bsg_view_knobs *k, int category);
BSG_EXPORT extern unsigned long long bsg_knobs_hash(struct bsg_view_knobs *k, struct bu_data_hash_state *state);
BSG_EXPORT extern int bsg_knobs_cmd_process(vect_t *rvec, int *do_rot, vect_t *tvec, int *do_tran, struct bsg_view *v, const char *cmd, fastf_t f, char origin, int model_flag, int incr_flag);
BSG_EXPORT extern void bsg_knobs_rot(struct bsg_view *v, const vect_t rvec, char origin, char coords, const matp_t obj_rot, const pointp_t pvt_pt);
BSG_EXPORT extern void bsg_knobs_tran(struct bsg_view *v, const vect_t tvec, int model_flag);
BSG_EXPORT extern void bsg_update_rate_flags(struct bsg_view *v);

/* Comparison and hash */
BSG_EXPORT extern int bsg_differ(struct bsg_view *v1, struct bsg_view *v2);
BSG_EXPORT extern unsigned long long bsg_hash(struct bsg_view *v);
BSG_EXPORT extern size_t bsg_clear(struct bsg_view *v, int flags);

/* Mouse/view coordinate utilities */
#ifndef BSG_IDLE
#define BSG_IDLE       0x000
#define BSG_ROT        0x001
#define BSG_TRANS      0x002
#define BSG_SCALE      0x004
#define BSG_CENTER     0x008
#define BSG_CON_X      0x010
#define BSG_CON_Y      0x020
#define BSG_CON_Z      0x040
#define BSG_CON_GRID   0x080
#define BSG_CON_LINES  0x100
#endif
BSG_EXPORT extern int bsg_adjust(struct bsg_view *v, int dx, int dy, point_t keypoint, int mode, unsigned long long flags);
BSG_EXPORT extern int bsg_screen_to_view(struct bsg_view *v, fastf_t *fx, fastf_t *fy, fastf_t x, fastf_t y);
BSG_EXPORT extern int bsg_screen_pt(point_t *p, fastf_t x, fastf_t y, struct bsg_view *v);

/* Object lookup */
BSG_EXPORT void bsg_uniq_obj_name(struct bu_vls *oname, const char *seed, struct bsg_view *v);
BSG_EXPORT int bsg_view_plane(plane_t *p, struct bsg_view *v);

/* Environment variable controlled logging */
#define BSG_ENABLE_ENV_LOGGING 1
BSG_EXPORT void bsg_log(int level, const char *fmt, ...)  _BU_ATTR_PRINTF23;

/* Debugging */
BSG_EXPORT void bsg_view_print(const char *title, struct bsg_view *v, int verbosity);

__END_DECLS

#endif /* BSG_UTIL_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

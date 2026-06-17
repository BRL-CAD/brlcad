/*                   N M G _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2022-2026 United States Government as represented by
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
/** @file nmg_private.h
 *
 * Functions that are used internally by multiple NMG files but are
 * not intended to be public API.
 *
 * For the moment we will leave the export annotations, in case unit
 * testing requires us to work with these functions specifically in
 * testing executables.
 *
 */

#include "common.h"
#include "vmath.h"
#include "bsg/vlist.h"
#include "nmg/defines.h"

struct bn_tol;
struct bsg_vlblock;
struct bu_list;
struct edge;
struct edgeuse;
struct faceuse;
struct loopuse;
struct model;
struct nmgregion;
struct shell;
struct vertex;

typedef void (*nmg_plot_anim_upcall_t)(struct bsg_vlblock *, int, int);

NMG_EXPORT extern nmg_plot_anim_upcall_t nmg_plot_anim_upcall;

/**
 * @brief Internal routine to kill an edge geometry structure (of either
 * type), if all the edgeuses on its list have vanished.  Regardless,
 * the edgeuse's geometry pointer is cleared.
 *
 * This routine does only a single edgeuse.  If the edgeuse mate has
 * geometry to be killed, make a second call.  Sometimes only one of
 * the two needs to release the geometry.
 *
 * @retval 0 If the old edge geometry (eu->g.magic_p) has other uses.
 * @retval 1 If the old edge geometry has been destroyed. Caller beware!
 *
 * NOT INTENDED FOR GENERAL USE!
 * However, it is not unique to mk.c - mod.c needs it for nmg_eusplit().
 */
NMG_EXPORT extern int nmg_keg(struct edgeuse *eu);


/* Currently commented out */
NMG_EXPORT extern double nmg_vu_angle_measure(struct vertexuse   *vu,
                                              vect_t x_dir,
                                              vect_t y_dir,
                                              int assessment,
                                              int in);

NMG_EXPORT extern void nmg_vlblock_v(struct bsg_vlblock *vbp,
				     const struct vertex *v,
				     long *tab,
				     struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_e(struct bsg_vlblock *vbp,
				     const struct edge *e,
				     long *tab,
				     int red,
				     int green,
				     int blue,
				     struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_eu(struct bsg_vlblock *vbp,
				      const struct edgeuse *eu,
				      long *tab,
				      int red,
				      int green,
				      int blue,
				      int fancy,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_euleft(struct bu_list *vh,
					  const struct edgeuse *eu,
					  const point_t center,
					  const mat_t mat,
					  const vect_t xvec,
					  const vect_t yvec,
					  double len,
					  struct bu_list *vlfree,
					  const struct bn_tol *tol);
NMG_EXPORT extern void nmg_vlblock_around_eu(struct bsg_vlblock *vbp,
					     const struct edgeuse *arg_eu,
					     long *tab,
					     int fancy,
					     struct bu_list *vlfree,
					     const struct bn_tol *tol);
NMG_EXPORT extern void nmg_vlblock_lu(struct bsg_vlblock *vbp,
				      const struct loopuse *lu,
				      long *tab,
				      int red,
				      int green,
				      int blue,
				      int fancy,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_fu(struct bsg_vlblock *vbp,
				      const struct faceuse *fu,
				      long *tab,
				      int fancy,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_s(struct bsg_vlblock *vbp,
				     const struct shell *s,
				     int fancy,
				     struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_r(struct bsg_vlblock *vbp,
				     const struct nmgregion *r,
				     int fancy,
				     struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_m(struct bsg_vlblock *vbp,
				     const struct model *m,
				     int fancy,
				     struct bu_list *vlfree);
NMG_EXPORT extern void nmg_pl_edges_in_2_shells(struct bsg_vlblock *vbp,
						long *b,
						const struct edgeuse *eu,
						int fancy,
						struct bu_list *vlfree,
						const struct bn_tol *tol);

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

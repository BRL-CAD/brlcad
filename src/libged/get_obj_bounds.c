/*                         G E T _ O B J _ B O U N D S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/get_obj_bounds.c
 *
 * Calculate object bounds.
 *
 * TODO - why are there two versions of this?
 *
 * TODO - this belongs at the librt level, and probably
 * lower than that once libg is split out...
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bu/str.h"
#include "bg/obr.h"
#include "vmath.h"
#include "raytrace.h"
#include "ged.h"

#include "./ged_private.h"

int
ged_get_obj_bounds(struct ged *gedp,
                   int argc,
                   const char *argv[],
                   int use_air,
                   point_t rpp_min,
                   point_t rpp_max)
{
    return rt_obj_bounds(gedp->ged_result_str, gedp->dbip, argc, argv, use_air, rpp_min, rpp_max);
}


/* Point accumulator shared by evaluated and tree-derived bound paths. */
struct _ged_tight_bounds_state {
    point_t tmin;
    point_t tmax;
    point_t *points;
    size_t point_count;
    size_t point_capacity;
    int collect_points;
};

static void
_ged_tight_bounds_add_point(struct _ged_tight_bounds_state *st, const point_t point)
{
    VMINMAX(st->tmin, st->tmax, point);
    if (!st->collect_points)
	return;

    if (st->point_count == st->point_capacity) {
	const size_t initial_capacity = 4096;
	const size_t new_capacity = st->point_capacity ? 2 * st->point_capacity : initial_capacity;
	st->points = (point_t *)bu_realloc(st->points, new_capacity * sizeof(point_t),
		"evaluated bounding points");
	st->point_capacity = new_capacity;
    }
    VMOVE(st->points[st->point_count], point);
    st->point_count++;
}

static void
_ged_add_box_corners(struct _ged_tight_bounds_state *st,
		     const point_t bmin,
		     const point_t bmax,
		     const matp_t matrix)
{
    for (int corner = 0; corner < 8; ++corner) {
	point_t local_point, model_point;
	VSET(local_point,
	    (corner & 1) ? bmax[X] : bmin[X],
	    (corner & 2) ? bmax[Y] : bmin[Y],
	    (corner & 4) ? bmax[Z] : bmin[Z]);
	if (matrix)
	    MAT4X3PNT(model_point, matrix, local_point);
	else
	    VMOVE(model_point, local_point);
	_ged_tight_bounds_add_point(st, model_point);
    }
}

static int
_ged_tree_oriented_points(struct rt_i *rtip,
			  const union tree *tree,
			  struct _ged_tight_bounds_state *st)
{
    RT_CK_TREE(tree);
    switch (tree->tr_op) {
	case OP_SOLID: {
	    const struct soltab *solid = tree->tr_a.tu_stp;
	    struct rt_db_internal intern;
	    point_t local_min, local_max;
	    int bounds_ret = -1;

	    RT_CK_SOLTAB(solid);
	    RT_DB_INTERNAL_INIT(&intern);
	    if (rt_db_get_internal(&intern, solid->st_dp, rtip->rti_dbip, NULL) >= 0) {
		if (intern.idb_meth->ft_bbox)
		    bounds_ret = rt_obj_bbox(&intern, &local_min, &local_max, &rtip->rti_tol);
		rt_db_free_internal(&intern);
	    }
	    if (bounds_ret < 0) {
		_ged_add_box_corners(st, solid->st_min, solid->st_max, NULL);
		return BRLCAD_OK;
	    }
	    _ged_add_box_corners(st, local_min, local_max, solid->st_matp);
	    return BRLCAD_OK;
	}
	case OP_UNION:
	case OP_XOR:
	    if (_ged_tree_oriented_points(rtip, tree->tr_b.tb_left, st) != BRLCAD_OK)
		return BRLCAD_ERROR;
	    return _ged_tree_oriented_points(rtip, tree->tr_b.tb_right, st);
	case OP_INTERSECT: {
	    point_t intersection_min, intersection_max;
	    if (rt_bound_tree(tree, intersection_min, intersection_max) < 0)
		return BRLCAD_ERROR;
	    _ged_add_box_corners(st, intersection_min, intersection_max, NULL);
	    return BRLCAD_OK;
	}
	case OP_SUBTRACT:
	    return _ged_tree_oriented_points(rtip, tree->tr_b.tb_left, st);
	case OP_NOP:
	    return BRLCAD_OK;
	default:
	    return BRLCAD_ERROR;
    }
}

static int
_ged_tree_oriented_bounds(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  int use_air,
			  struct _ged_tight_bounds_state *st)
{
    struct rt_i *rtip = rt_i_create(gedp->dbip);
    struct region *region;

    if (rtip == RTI_NULL)
	return BRLCAD_ERROR;
    rtip->useair = use_air;
    for (int i = 0; i < argc; ++i) {
	if (rt_gettree(rtip, argv[i]) < 0) {
	    rt_i_destroy(rtip);
	    return BRLCAD_ERROR;
	}
    }
    rt_prep(rtip);
    for (BU_LIST_FOR(region, region, &rtip->HeadRegion)) {
	if (_ged_tree_oriented_points(rtip, region->reg_treetop, st) != BRLCAD_OK) {
	    rt_i_destroy(rtip);
	    return BRLCAD_ERROR;
	}
    }
    rt_i_destroy(rtip);
    return st->point_count ? BRLCAD_OK : BRLCAD_ERROR;
}

static int
_ged_sample_evaluated_bounds(struct ged *gedp,
			     int argc,
			     const char *argv[],
			     int use_air,
			     const point_t bounds_min,
			     const point_t bounds_max,
			     struct _ged_tight_bounds_state *st,
			     point_t oriented_corners[8])
{
    struct rt_i *rtip = rt_i_create(gedp->dbip);
    double surface_area = 0.0;
    int loaded = 0;

    if (rtip == RTI_NULL)
	return BRLCAD_ERROR;
    rtip->useair = use_air;
    for (int i = 0; i < argc; ++i) {
	if (rt_gettree(rtip, argv[i]) < 0) {
	    rt_i_destroy(rtip);
	    return BRLCAD_ERROR;
	}
	loaded++;
    }
    if (!loaded) {
	rt_i_destroy(rtip);
	return BRLCAD_ERROR;
    }
    rt_prep_parallel(rtip, 1);
    const int crossings = rt_crofton_shoot(&surface_area, NULL, &st->tmin,
	&st->tmax, oriented_corners, NULL, NULL, rtip, NULL, bounds_min, bounds_max);
    rt_i_destroy(rtip);
    return crossings > 0 ? BRLCAD_OK : BRLCAD_ERROR;
}

/*
 * _ged_obj_tight_bounds()
 *
 * Compute a "tight" axis-aligned bounding box for the given object(s) that,
 * unlike rt_obj_bounds()/rt_bound_tree(), DOES account for subtracted
 * (OP_SUBTRACT / negative) material.  The default librt RPP bounding recurses
 * into a subtraction's right-hand (negative) subtree but discards its RPP (see
 * src/librt/bbox.c), so carved-away material never shrinks the reported box.
 *
 * This routine instead derives the box from geometry evaluated by the ray
 * tracer.  It uses the shared Cauchy-Crofton sampler until its surface-area
 * estimate stabilizes, then folds the converged entry/exit point set into the
 * reported AABB.  The result contracts to reflect subtracted material and is
 * approximate to the converged sampling resolution.
 *
 * If the loose bound cannot be obtained, ray prep fails, or no material is hit,
 * the loose bound is returned unchanged so the caller never hard-fails.
 *
 * Returns BRLCAD_OK on success (rpp_min/rpp_max always set to a usable box).
 */
int
_ged_obj_tight_bounds(struct ged *gedp,
		      int argc,
		      const char *argv[],
		      int use_air,
		      point_t rpp_min,
		      point_t rpp_max)
{
    struct _ged_tight_bounds_state st;
    point_t loose_min, loose_max;
    vect_t span;

    /* Start from the loose bound (this also validates the object list). */
    if (rt_obj_bounds(gedp->ged_result_str, gedp->dbip, argc, argv, use_air, loose_min, loose_max) & BRLCAD_ERROR)
	return BRLCAD_ERROR;

    VMOVE(rpp_min, loose_min);
    VMOVE(rpp_max, loose_max);

    VSUB2(span, loose_max, loose_min);
    if (span[X] <= 0.0 || span[Y] <= 0.0 || span[Z] <= 0.0)
	return BRLCAD_OK; /* degenerate loose box; nothing to tighten */

    VSETALL(st.tmin, INFINITY);
    VSETALL(st.tmax, -INFINITY);
    st.points = NULL;
    st.point_count = 0;
    st.point_capacity = 0;
    st.collect_points = 0;
    const int sample_ret = _ged_sample_evaluated_bounds(gedp, argc, argv,
	use_air, loose_min, loose_max, &st, NULL);

    /* If we hit material, use the evaluated (tight) box; otherwise keep loose. */
    if (sample_ret == BRLCAD_OK
	&& st.tmin[X] <= st.tmax[X]
	&& st.tmin[Y] <= st.tmax[Y]
	&& st.tmin[Z] <= st.tmax[Z]) {
	VMOVE(rpp_min, st.tmin);
	VMOVE(rpp_max, st.tmax);
    }

    return BRLCAD_OK;
}

int
_ged_obj_oriented_bounds(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 int use_air,
			 int evaluated,
			 point_t corners[8])
{
    struct _ged_tight_bounds_state st;
    point_t loose_min, loose_max;
    point_t *corner_ptrs[8];

    if (!corners || rt_obj_bounds(gedp->ged_result_str, gedp->dbip, argc, argv,
		use_air, loose_min, loose_max) & BRLCAD_ERROR)
	return BRLCAD_ERROR;

    VSETALL(st.tmin, INFINITY);
    VSETALL(st.tmax, -INFINITY);
    st.points = NULL;
    st.point_count = 0;
    st.point_capacity = 0;
    st.collect_points = 1;
    const int points_ret = evaluated ?
	_ged_sample_evaluated_bounds(gedp, argc, argv, use_air,
	    loose_min, loose_max, &st, corners) :
	_ged_tree_oriented_bounds(gedp, argc, argv, use_air, &st);
    if (points_ret != BRLCAD_OK) {
	if (st.points)
	    bu_free(st.points, "evaluated bounding points");
	return BRLCAD_ERROR;
    }

    if (evaluated)
	return BRLCAD_OK;

    for (int i = 0; i < 8; ++i)
	corner_ptrs[i] = &corners[i];
    const int fit_ret = bg_3d_obb(corner_ptrs, &st.points[0][0],
	(int)st.point_count);
    bu_free(st.points, "evaluated bounding points");
    return fit_ret ? BRLCAD_ERROR : BRLCAD_OK;
}

static int
get_objpath_mat(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    struct _ged_trace_data *gtdp)
{
    int i, pos_in;

    /*
     * paths are matched up to last input member
     * ANY path the same up to this point is considered as matching
     */

    /* initialize gtd */
    gtdp->gtd_gedp = gedp;
    gtdp->gtd_flag = _GED_EVAL_ONLY;
    gtdp->gtd_prflag = 0;

    pos_in = 0;

    if (argc == 1 && strchr(argv[0], '/')) {
	char *tok;
	char *av0;
	gtdp->gtd_objpos = 0;

	av0 = bu_strdup(argv[0]);
	tok = strtok(av0, "/");
	while (tok) {
	    if ((gtdp->gtd_obj[gtdp->gtd_objpos++] =
		 db_lookup(gedp->dbip, tok, LOOKUP_NOISY)) == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "get_objpath_mat: Failed to find %s", tok);
		free(av0);
		return BRLCAD_ERROR;
	    }

	    tok = strtok((char *)0, "/");
	}

	free(av0);
    } else {
	gtdp->gtd_objpos = argc;

	/* build directory pointer array for desired path */
	for (i = 0; i < gtdp->gtd_objpos; i++) {
	    if ((gtdp->gtd_obj[i] =
		 db_lookup(gedp->dbip, argv[pos_in+i], LOOKUP_NOISY)) == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "get_objpath_mat: Failed to find %s", argv[pos_in+i]);
		return BRLCAD_ERROR;
	    }
	}
    }

    MAT_IDN(gtdp->gtd_xform);
    ged_trace(gtdp->gtd_obj[0], 0, bn_mat_identity, gtdp, 1);

    return BRLCAD_OK;
}


/**
 * @brief
 * This version works if the last member of the path is a primitive.
 */
int
_ged_get_obj_bounds2(struct ged *gedp,
		     int argc,
		     const char *argv[],
		     struct _ged_trace_data *gtdp,
		     point_t rpp_min,
		     point_t rpp_max)
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_i *rtip;
    struct soltab st;
    mat_t imat;

    /* initialize RPP bounds */
    VSETALL(rpp_min, MAX_FASTF);
    VREVERSE(rpp_max, rpp_min);

    if (get_objpath_mat(gedp, argc, argv, gtdp) & BRLCAD_ERROR)
	return BRLCAD_ERROR;

    dp = gtdp->gtd_obj[gtdp->gtd_objpos-1];
    GED_DB_GET_INTERN(gedp, &intern, dp, gtdp->gtd_xform, BRLCAD_ERROR);

    /* Make a new rt_i instance from the existing db_i structure */
    rtip = rt_i_create(gedp->dbip);
    if (rtip == RTI_NULL) {
	bu_vls_printf(gedp->ged_result_str, "rt_i_create failure for %s", gedp->dbip->dbi_filename);
	return BRLCAD_ERROR;
    }

    memset(&st, 0, sizeof(struct soltab));

    st.l.magic = RT_SOLTAB_MAGIC;
    st.l2.magic = RT_SOLTAB2_MAGIC;
    st.st_dp = dp;
    MAT_IDN(imat);
    st.st_matp = imat;
    st.st_meth = intern.idb_meth;

    /* Get bounds from internal object */
    VMOVE(st.st_min, rpp_min);
    VMOVE(st.st_max, rpp_max);
    if (intern.idb_meth->ft_prep)
	intern.idb_meth->ft_prep(&st, &intern, rtip);
    VMOVE(rpp_min, st.st_min);
    VMOVE(rpp_max, st.st_max);

    rt_i_destroy(rtip);
    rt_db_free_internal(&intern);

    return BRLCAD_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

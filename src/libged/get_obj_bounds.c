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


/* Accumulator shared with the ray-trace hit callback used by the tight-bound
 * path.  Every actual entry/exit point of solid material (as evaluated by the
 * ray tracer, which honors OP_SUBTRACT) is folded into the running AABB. */
struct _ged_tight_bounds_state {
    point_t tmin;
    point_t tmax;
    int hit_count;
};

static int
_ged_tight_bounds_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    struct _ged_tight_bounds_state *st = (struct _ged_tight_bounds_state *)ap->a_uptr;
    struct partition *pp;

    for (pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
	point_t in_pt, out_pt;

	VJOIN1(in_pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	VJOIN1(out_pt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

	VMINMAX(st->tmin, st->tmax, in_pt);
	VMINMAX(st->tmin, st->tmax, out_pt);
	st->hit_count++;
    }

    return 1;
}

static int
_ged_tight_bounds_miss(struct application *UNUSED(ap))
{
    return 0;
}

static int
_ged_tight_bounds_overlap(struct application *UNUSED(ap),
			  struct partition *UNUSED(pp),
			  struct region *UNUSED(reg1),
			  struct region *UNUSED(reg2),
			  struct partition *UNUSED(hp))
{
    return 0;
}

/* Number of rays fired per axis (per side of the loose bounding box). */
#define GED_TIGHT_GRID 128

/*
 * _ged_obj_tight_bounds()
 *
 * Compute a "tight" axis-aligned bounding box for the given object(s) that,
 * unlike rt_obj_bounds()/rt_bound_tree(), DOES account for subtracted
 * (OP_SUBTRACT / negative) material.  The default librt RPP bounding recurses
 * into a subtraction's right-hand (negative) subtree but discards its RPP (see
 * src/librt/bbox.c), so carved-away material never shrinks the reported box.
 *
 * This routine instead derives the box from the geometry as actually EVALUATED
 * by the ray tracer: it first obtains the loose RPP, then fires a dense grid of
 * rays across each face of that RPP (from all three axis directions).  The
 * entry/exit points of every solid partition returned by rt_shootray() -- which
 * fully evaluates the CSG boolean tree including subtractions -- are folded into
 * the reported AABB.  The result therefore contracts to reflect subtracted
 * material, to the resolution of the ray grid (i.e. it is approximate, matching
 * the evaluated/facetized bound the caller would otherwise have to hand-roll).
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
    struct rt_i *rtip = NULL;
    struct _ged_tight_bounds_state st;
    point_t loose_min, loose_max;
    vect_t span;
    int i, j, k;
    int loaded = 0;

    /* Start from the loose bound (this also validates the object list). */
    if (rt_obj_bounds(gedp->ged_result_str, gedp->dbip, argc, argv, use_air, loose_min, loose_max) & BRLCAD_ERROR)
	return BRLCAD_ERROR;

    VMOVE(rpp_min, loose_min);
    VMOVE(rpp_max, loose_max);

    VSUB2(span, loose_max, loose_min);
    if (span[X] <= 0.0 || span[Y] <= 0.0 || span[Z] <= 0.0)
	return BRLCAD_OK; /* degenerate loose box; nothing to tighten */

    rtip = rt_i_create(gedp->dbip);
    if (rtip == RTI_NULL)
	return BRLCAD_OK; /* fall back to loose */

    rtip->useair = use_air;

    for (i = 0; i < argc; i++) {
	if (rt_gettree(rtip, argv[i]) < 0) {
	    rt_i_destroy(rtip);
	    return BRLCAD_OK; /* un-traceable input: keep loose bound */
	}
	loaded++;
    }
    if (!loaded) {
	rt_i_destroy(rtip);
	return BRLCAD_OK;
    }

    rt_prep(rtip);

    VSETALL(st.tmin, INFINITY);
    VSETALL(st.tmax, -INFINITY);
    st.hit_count = 0;

    /* Pad the grid extent slightly so rays graze the loose faces cleanly. */
    {
	vect_t pad;
	VSCALE(pad, span, 1.0e-6);
	VSUB2(loose_min, loose_min, pad);
	VADD2(loose_max, loose_max, pad);
	VSUB2(span, loose_max, loose_min);
    }

    /* Fire an NxN grid of rays along each of the 3 axes.  For axis a, rays run
     * parallel to a, sampled over the other two axes' spans. */
    for (int axis = 0; axis < 3; axis++) {
	int u = (axis + 1) % 3;
	int v = (axis + 2) % 3;
	struct application ap;

	RT_APPLICATION_INIT(&ap);
	ap.a_rt_i = rtip;
	ap.a_resource = &rt_uniresource;
	ap.a_hit = _ged_tight_bounds_hit;
	ap.a_miss = _ged_tight_bounds_miss;
	ap.a_overlap = _ged_tight_bounds_overlap;
	ap.a_onehit = 0;
	ap.a_uptr = (void *)&st;

	VSETALL(ap.a_ray.r_dir, 0.0);
	ap.a_ray.r_dir[axis] = 1.0;

	for (j = 0; j < GED_TIGHT_GRID; j++) {
	    fastf_t fu = (GED_TIGHT_GRID > 1) ? ((fastf_t)j + 0.5) / (fastf_t)GED_TIGHT_GRID : 0.5;
	    for (k = 0; k < GED_TIGHT_GRID; k++) {
		fastf_t fv = (GED_TIGHT_GRID > 1) ? ((fastf_t)k + 0.5) / (fastf_t)GED_TIGHT_GRID : 0.5;

		VSETALL(ap.a_ray.r_pt, 0.0);
		ap.a_ray.r_pt[axis] = loose_min[axis] - span[axis]; /* start behind the box */
		ap.a_ray.r_pt[u] = loose_min[u] + fu * span[u];
		ap.a_ray.r_pt[v] = loose_min[v] + fv * span[v];

		(void)rt_shootray(&ap);
	    }
	}
    }

    rt_i_destroy(rtip);

    /* If we hit material, use the evaluated (tight) box; otherwise keep loose. */
    if (st.hit_count > 0
	&& st.tmin[X] <= st.tmax[X]
	&& st.tmin[Y] <= st.tmax[Y]
	&& st.tmin[Z] <= st.tmax[Z]) {
	VMOVE(rpp_min, st.tmin);
	VMOVE(rpp_max, st.tmax);
    }

    return BRLCAD_OK;
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

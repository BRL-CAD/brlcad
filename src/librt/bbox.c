/*                          B B O X . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2026 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file librt/bbox.c
 *
 * Routines related to creating and processing bounding boxes.
 *
 */
/** @} */

#include "common.h"

#include <math.h>
#include <string.h>

#include "bu/path.h"
#include "vmath.h"
#include "raytrace.h"
#include "./librt_private.h"


static int
rt_bound_db_internal(point_t *bmin, point_t *bmax,
		     struct rt_db_internal *ip,
		     const struct bg_tess_tol *ttol,
		     const struct bn_tol *tol)
{
    int ret = -1;

    if (ip->idb_type == ID_HALF) {
	VSETALL(*bmin, -INFINITY);
	VSETALL(*bmax, INFINITY);
	return 0;
    }

    if (ip->idb_meth->ft_bbox)
	ret = ip->idb_meth->ft_bbox(ip, bmin, bmax, tol);

    if (ret < 0 && ip->idb_meth->ft_plot) {
	struct bu_list vhead;
	BU_LIST_INIT(&vhead);
	if (ip->idb_meth->ft_plot(&vhead, ip, ttol, tol, NULL) >= 0)
	    ret = bv_vlist_bbox(&vhead, bmin, bmax, NULL, NULL);
	BV_FREE_VLIST(&rt_vlfree, &vhead);
    }

    return ret;
}

int
rt_bound_tree(const union tree *tp, vect_t tree_min, vect_t tree_max)
{
    vect_t r_min, r_max;		/* rpp for right side of tree */

    VSETALL(r_min, INFINITY);
    VSETALL(r_max, -INFINITY);

    RT_CK_TREE(tp);

    switch (tp->tr_op) {

	case OP_SOLID: {
	    const struct soltab *stp;

	    stp = tp->tr_a.tu_stp;
	    RT_CK_SOLTAB(stp);
	    if (stp->st_aradius <= 0) {
		bu_log("rt_bound_tree: encountered dead solid '%s'\n",
		       stp->st_dp->d_namep);
		return -1;	/* ERROR */
	    }

	    if (stp->st_aradius >= INFINITY) {
		VSETALL(tree_min, -INFINITY);
		VSETALL(tree_max,  INFINITY);
		return 0;
	    }
	    VMOVE(tree_min, stp->st_min);
	    VMOVE(tree_max, stp->st_max);
	    return 0;
	}

	default:
	    bu_log("rt_bound_tree(%p): unknown op=x%x\n",
		   (void *)tp, tp->tr_op);
	    return -1;

	case OP_XOR:
	case OP_UNION:
	    /* BINARY type -- expand to contain both */
	    if (rt_bound_tree(tp->tr_b.tb_left, tree_min, tree_max) < 0 ||
		rt_bound_tree(tp->tr_b.tb_right, r_min, r_max) < 0)
		return -1;
	    VMIN(tree_min, r_min);
	    VMAX(tree_max, r_max);
	    break;
	case OP_INTERSECT:
	    /* BINARY type -- find common area only */
	    if (rt_bound_tree(tp->tr_b.tb_left, tree_min, tree_max) < 0 ||
		rt_bound_tree(tp->tr_b.tb_right, r_min, r_max) < 0)
		return -1;
	    /* min = largest min, max = smallest max */
	    VMAX(tree_min, r_min);
	    VMIN(tree_max, r_max);
	    break;
	case OP_SUBTRACT:
	    /* BINARY type -- just use left tree */
	    if (rt_bound_tree(tp->tr_b.tb_left, tree_min, tree_max) < 0 ||
		rt_bound_tree(tp->tr_b.tb_right, r_min, r_max) < 0)
		return -1;
	    /* Discard right rpp: this yields a LOOSE axis-aligned bound that
	     * IGNORES subtracted (negative) material -- the box never shrinks
	     * to reflect carved-away geometry.  A tight bound that accounts for
	     * subtractions must evaluate the boolean tree (e.g. the ray-traced
	     * evaluated-geometry path exposed via the "bb -t" command). */
	    break;
	case OP_GUARD:
	case OP_XNOP:
	    return rt_bound_tree(tp->tr_b.tb_left, tree_min, tree_max);
	case OP_NOT:
	    VSETALL(tree_min, -INFINITY);
	    VSETALL(tree_max, INFINITY);
	    return 0;
	case OP_NOP:
	    /* Implies that this tree has nothing in it */
	    break;
    }
    return 0;
}


/**
 * Return a pointer to the corresponding region structure of the given
 * region's name (reg_name), or REGION_NULL if it does not exist.
 *
 * If the full path of a region is specified, then that one is
 * returned.  However, if only the database node name of the region is
 * specified and that region has been referenced multiple time in the
 * tree, then this routine will simply return the first one.
 */
static struct region *
get_region(struct rt_i *rtip, const char *reg_name)
{
    struct region *regp;
    char *reg_base = bu_path_basename(reg_name, NULL);

    RT_CK_RTI(rtip);
    for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
	char *cp;
	/* First, check for a match of the full path */
	if (BU_STR_EQUAL(reg_base, regp->reg_name)) {
	    bu_free(reg_base, "reg_base free");
	    return regp;
	}
	/* Second, check for a match of the database node name */
	cp = bu_path_basename(regp->reg_name, NULL);
	if (BU_STR_EQUAL(cp, reg_name)) {
	    bu_free(reg_base, "reg_base free");
	    bu_free(cp, "cp free");
	    return regp;
	}
	bu_free(cp, "cp free");
    }
    bu_free(reg_base, "reg_base free");

    return REGION_NULL;
}


int
rt_rpp_region(struct rt_i *rtip, const char *reg_name, fastf_t *min_rpp, fastf_t *max_rpp)
{
    struct region *regp;

    RT_CHECK_RTI(rtip);

    regp = get_region(rtip, reg_name);
    if (regp == REGION_NULL) return 0;
    if (rt_bound_tree(regp->reg_treetop, min_rpp, max_rpp) < 0)
	return 0;
    return 1;
}


int
rt_in_rpp(struct xray *rp,
	  register const fastf_t *invdir,	/* inverses of rp->r_dir[] */
	  register const fastf_t *min,
	  register const fastf_t *max)
{
    register const fastf_t *pt = &rp->r_pt[0];
    register fastf_t sv;
#define st sv			/* reuse the register */
    register fastf_t rmin = -MAX_FASTF;
    register fastf_t rmax =  MAX_FASTF;

    /* Start with infinite ray, and trim it down */

    /* X axis */
    if (*invdir < -SMALL_FASTF && *invdir > -INFINITY) {
	/* Heading towards smaller numbers */
	/* if (*min > *pt) miss */
	if (rmax > (sv = (*min - *pt) * *invdir))
	    rmax = sv;
	if (rmin < (st = (*max - *pt) * *invdir))
	    rmin = st;
    }  else if (*invdir > SMALL_FASTF && *invdir < INFINITY) {
	/* Heading towards larger numbers */
	/* if (*max < *pt) miss */
	if (rmax > (st = (*max - *pt) * *invdir))
	    rmax = st;
	if (rmin < ((sv = (*min - *pt) * *invdir)))
	    rmin = sv;
    } else {
	/*
	 * Direction cosines along this axis is NEAR 0,
	 * which implies that the ray is perpendicular to the axis,
	 * so merely check position against the boundaries.
	 */
	if ((*min > *pt) || (*max < *pt))
	    return 0;	/* MISS */
    }

    /* Y axis */
    pt++; invdir++; max++; min++;
    if (*invdir < -SMALL_FASTF && *invdir > -INFINITY) {
	if (rmax > (sv = (*min - *pt) * *invdir))
	    rmax = sv;
	if (rmin < (st = (*max - *pt) * *invdir))
	    rmin = st;
    }  else if (*invdir > SMALL_FASTF && *invdir < INFINITY) {
	if (rmax > (st = (*max - *pt) * *invdir))
	    rmax = st;
	if (rmin < ((sv = (*min - *pt) * *invdir)))
	    rmin = sv;
    } else {
	if ((*min > *pt) || (*max < *pt))
	    return 0;	/* MISS */
    }

    /* Z axis */
    pt++; invdir++; max++; min++;
    if (*invdir < -SMALL_FASTF && *invdir > -INFINITY) {
	if (rmax > (sv = (*min - *pt) * *invdir))
	    rmax = sv;
	if (rmin < (st = (*max - *pt) * *invdir))
	    rmin = st;
    }  else if (*invdir > SMALL_FASTF && *invdir < INFINITY) {
	if (rmax > (st = (*max - *pt) * *invdir))
	    rmax = st;
	if (rmin < ((sv = (*min - *pt) * *invdir)))
	    rmin = sv;
    } else {
	if ((*min > *pt) || (*max < *pt))
	    return 0;	/* MISS */
    }

    /* If equal, RPP is actually a plane */
    if (rmin > rmax)
	return 0;	/* MISS */

    /* HIT.  Only now do rp->r_min and rp->r_max have to be written */
    rp->r_min = rmin;
    rp->r_max = rmax;
    return 1;		/* HIT */
}


/* A name alone cannot distinguish transformed instances of a solid. */
static struct soltab *
rt_find_solid_with_mat(const struct rt_i *rtip,
		       const struct directory *dp, const mat_t mat)
{
    struct soltab *stp;

    RT_VISIT_ALL_SOLTABS_START(stp, (struct rt_i *)rtip) {
	if (stp->st_dp != dp)
	    continue;
	const fastf_t *st_mat = stp->st_matp ?
	    stp->st_matp : bn_mat_identity;
	if (bn_mat_is_equal(mat, st_mat, &rtip->rti_tol))
	    return stp;
    } RT_VISIT_ALL_SOLTABS_END;

    return NULL;
}


/**
 * Traverse the original database tree while tracking member transforms.
 *
 * Returns 0 on success, -1 if no bounds can be computed.
 */
static int
rt_traverse_tree_mat(struct rt_i *rtip, const union tree *tp,
		     fastf_t *tree_min, fastf_t *tree_max,
		     const mat_t mat)
{
    vect_t r_min, r_max;		/* rpp for right side of tree */

    if (!tp)
	return -1;

    RT_CK_TREE(tp);

    switch (tp->tr_op) {

	case OP_SOLID:
	    {
		const struct soltab *stp;

		stp = tp->tr_a.tu_stp;
		if (!stp)
		    return -1;
		RT_CK_SOLTAB(stp);
		if (stp->st_aradius <= 0) {
		    bu_log("rt_traverse_tree: encountered dead solid '%s'\n",
			   stp->st_dp->d_namep);
		    return -1;	/* ERROR */
		}

		if (stp->st_aradius >= INFINITY) {
		    VSETALL(tree_min, -INFINITY);
		    VSETALL(tree_max,  INFINITY);
		    return 0;
		}
		VMOVE(tree_min, stp->st_min);
		VMOVE(tree_max, stp->st_max);
		return 0;
	    }

	default:
	    bu_log("rt_traverse_tree(%p): unknown op=x%x\n", (void *)tp, (tp->tr_op));
	    return -1;

	case OP_XOR:
	case OP_UNION:
	    /* BINARY type -- expand to contain both */
	    if (rt_traverse_tree_mat(rtip, tp->tr_b.tb_left,
		tree_min, tree_max, mat) < 0 ||
		rt_traverse_tree_mat(rtip, tp->tr_b.tb_right,
		r_min, r_max, mat) < 0)
		return -1;
	    VMIN(tree_min, r_min);
	    VMAX(tree_max, r_max);
	    break;
	case OP_INTERSECT:
	    /* BINARY type -- find common area only */
	    if (rt_traverse_tree_mat(rtip, tp->tr_b.tb_left,
		tree_min, tree_max, mat) < 0 ||
		rt_traverse_tree_mat(rtip, tp->tr_b.tb_right,
		r_min, r_max, mat) < 0)
		return -1;
	    /* min = largest min, max = smallest max */
	    VMAX(tree_min, r_min);
	    VMIN(tree_max, r_max);
	    break;
	case OP_SUBTRACT:
	    /* A - B cannot extend beyond A.  In particular, do not traverse B:
	     * rt_gettree's subtractor shaker may have already discarded a
	     * disjoint B (and its soltab) from the prepared tree, while this
	     * routine may be walking the original database tree. */
	    if (rt_traverse_tree_mat(rtip, tp->tr_b.tb_left,
		tree_min, tree_max, mat) < 0)
		return -1;
	    break;

	    /* This case is especially for handling rt_db_internal
	     * formats which generally contain a "unloaded" tree with
	     * tp->tr_op = OP_DB_LEAF
	     */
	case OP_DB_LEAF:
	    {
		const struct soltab *stp;
		struct directory *dp;
		mat_t leaf_mat;

		if (rtip == NULL) {
		    bu_log("rt_traverse_tree: A valid rtip was not passed for calculating bounds of '%s'\n",
			   tp->tr_l.tl_name);
		    return -1;
		}

		MAT_COPY(leaf_mat, mat);
		if (tp->tr_l.tl_mat)
		    bn_mat_mul(leaf_mat, mat, tp->tr_l.tl_mat);

		dp = db_lookup(rtip->rti_dbip, tp->tr_l.tl_name,
			       LOOKUP_QUIET);
		if (dp == RT_DIR_NULL)
		    return -1;

		stp = rt_find_solid_with_mat(rtip, dp, leaf_mat);
		if (stp == NULL) {

		    /* Recurse through combinations using their original
		     * trees.  A primitive without a matching prepared
		     * instance has no valid bounds. */
		    struct rt_db_internal intern;
		    struct rt_comb_internal *combp;

		    if (!rt_db_lookup_internal(rtip->rti_dbip, tp->tr_l.tl_name, &dp, &intern, LOOKUP_NOISY)) {
			bu_log("rt_traverse_tree: rt_db_lookup_internal(%s) failed to get the internal form",
			       tp->tr_l.tl_name);
			return -1;
		    }

		    if (intern.idb_minor_type != ID_COMBINATION) {
			bu_log("rt_traverse_tree: no prepared instance"
			       " for '%s'\n", tp->tr_l.tl_name);
			rt_db_free_internal(&intern);
			return -1;
		    }

		    combp = (struct rt_comb_internal *)intern.idb_ptr;
		    RT_CK_COMB(combp);
		    int ret = rt_traverse_tree_mat(rtip, combp->tree,
			tree_min, tree_max, leaf_mat);
		    rt_db_free_internal(&intern);
		    if (ret)
			return -1;
		} else {
		    /* Got a solid pointer, get bounds and return */
		    RT_CK_SOLTAB(stp);
		    if (stp->st_aradius <= 0) {
			bu_log("rt_traverse_tree: encountered dead solid '%s'\n",
			       stp->st_dp->d_namep);
			return -1;	/* ERROR */
		    }

		    if (stp->st_aradius >= INFINITY) {
			VSETALL(tree_min, -INFINITY);
			VSETALL(tree_max,  INFINITY);
			return 0;
		    }

		    VMOVE(tree_min, stp->st_min);
		    VMOVE(tree_max, stp->st_max);
		}

		return 0;
	    }

	case OP_NOP:
	    /* An empty tree has no bounds to return. */
	    return -1;
    }
    return 0;
}


int
rt_traverse_tree(struct rt_i *rtip, const union tree *tp,
		 fastf_t *tree_min, fastf_t *tree_max)
{
    mat_t mat;

    MAT_IDN(mat);
    return rt_traverse_tree_mat(rtip, tp, tree_min, tree_max, mat);
}


int
rt_bound_instance(point_t *bmin, point_t *bmax,
		  struct directory *dp,
		  struct db_i *dbip,
		  const struct bg_tess_tol *ttol,
		  const struct bn_tol *tol,
		  mat_t *s_mat
    )
{
    if (UNLIKELY(!bmin || !bmax || !dp || !dbip))
	return -1;

    VSET(*bmin, INFINITY, INFINITY, INFINITY);
    VSET(*bmax, -INFINITY, -INFINITY, -INFINITY);

    struct rt_db_internal dbintern;
    RT_DB_INTERNAL_INIT(&dbintern);
    struct rt_db_internal *ip = &dbintern;
    int ret = rt_db_get_internal(ip, dp, dbip, *s_mat);
    if (ret < 0)
	return -1;

    int bbret = rt_bound_db_internal(bmin, bmax, ip, ttol, tol);

    rt_db_free_internal(&dbintern);
    return bbret;
}


int
rt_bound_internal(struct db_i *dbip, struct directory *dp,
		  point_t rpp_min, point_t rpp_max)
{
    struct rt_i *rtip;
    struct rt_db_internal intern;
    struct rt_comb_internal *combp;
    vect_t tree_min, tree_max;
    const union tree *tp;
    union tree primitive_tree;

    /* Initialize RPP bounds */
    VSETALL(rpp_min, MAX_FASTF);
    VREVERSE(rpp_max, rpp_min);

    if ((rtip=rt_i_create(dbip)) == RTI_NULL) {
	bu_log("rt_bound_internal: rt_i_create() failure while getting bb for %s\n", dp->d_namep);
	return -1;
    }

    /* Call rt_gettree() to get the bounds for primitives, else soltab
     * ptr is null
     */
    if (rt_gettree(rtip, dp->d_namep) < 0) {
	bu_log("rt_bound_internal: rt_gettree('%s') failed\n", dp->d_namep);
	rt_i_destroy(rtip);
	return -1;
    }


    if (!rt_db_lookup_internal(dbip, dp->d_namep, &dp, &intern, LOOKUP_NOISY)) {
	bu_log("rt_bound_internal: lookup failed for '%s'\n",
	       dp->d_namep);
	rt_i_destroy(rtip);
	return -1;
    }

    /* If passed rt_db_internal is a combination(a group or a region)
     * then further calls needed.
     */
    if (intern.idb_minor_type == ID_COMBINATION) {
	combp = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(combp);
	tp = combp->tree;
    } else {
	/* A primitive uses its prepared solid directly. */
	RT_TREE_INIT(&primitive_tree);
	primitive_tree.tr_l.tl_op = OP_SOLID;
	primitive_tree.tr_a.tu_stp = rt_find_solid(rtip, dp->d_namep);
	tp = &primitive_tree;
    }

    if (rt_traverse_tree(rtip, tp, tree_min, tree_max)) {
	bu_log("rt_bound_internal: tree traversal failed\n");
	rt_db_free_internal(&intern);
	rt_i_destroy(rtip);
	return -1;
    }


    VMOVE(rpp_min, tree_min);
    VMOVE(rpp_max, tree_max);

    /* Check if the model bounds look correct e.g. if they are all 0,
     * then it's not correct.
     */
    if ((NEAR_ZERO(rpp_min[0], SMALL_FASTF) || rpp_min[0] <= -INFINITY || rpp_min[0] >= INFINITY) &&
	(NEAR_ZERO(rpp_min[1], SMALL_FASTF) || rpp_min[1] <= -INFINITY || rpp_min[1] >= INFINITY) &&
	(NEAR_ZERO(rpp_min[2], SMALL_FASTF) || rpp_min[2] <= -INFINITY || rpp_min[2] >= INFINITY) &&
	(NEAR_ZERO(rpp_max[0], SMALL_FASTF) || rpp_max[0] <= -INFINITY || rpp_max[0] >= INFINITY) &&
	(NEAR_ZERO(rpp_max[1], SMALL_FASTF) || rpp_max[1] <= -INFINITY || rpp_max[1] >= INFINITY) &&
	(NEAR_ZERO(rpp_max[2], SMALL_FASTF) || rpp_max[2] <= -INFINITY || rpp_max[2] >= INFINITY)
	) {
	bu_log("rt_bound_internal: Warning : The returned bounds of the model may not be correct\n");
    }

    rt_db_free_internal(&intern);
    rt_i_destroy(rtip);
    return 0;
}


int
rt_obj_bounds(struct bu_vls *msgs,
	      struct db_i *dbip,
	      int argc,
	      const char *argv[],
	      int use_air,
	      point_t rpp_min,
	      point_t rpp_max)
{
    int i;
    struct rt_i *rtip;
    struct db_full_path path;
    struct region *regp;

    /* Make a new rt_i instance from the existing db_i structure */
    rtip = rt_i_create(dbip);
    if (rtip == RTI_NULL) {
	if (msgs)
	    bu_vls_printf(msgs, "rt_i_create failure for %s\n", dbip->dbi_filename);
	return BRLCAD_ERROR;
    }

    rtip->useair = use_air;

    /* Get trees for list of objects/paths */
    for (i = 0; i < argc; i++) {
	int gottree;

	/* Get full_path structure for argument */
	db_full_path_init(&path);
	if (db_string_to_path(&path,  rtip->rti_dbip, argv[i])) {
	    if (msgs)
		bu_vls_printf(msgs, "db_string_to_path failed for %s\n", argv[i]);
	    rt_i_destroy(rtip);
	    return BRLCAD_ERROR;
	}

	/* check if we already got this tree */
	gottree = 0;
	for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
	    struct db_full_path tmp_path;

	    db_full_path_init(&tmp_path);
	    if (db_string_to_path(&tmp_path, rtip->rti_dbip, regp->reg_name)) {
		if (msgs)
		    bu_vls_printf(msgs, "db_string_to_path failed for %s\n", regp->reg_name);
		rt_i_destroy(rtip);
		db_free_full_path(&path);
		return BRLCAD_ERROR;
	    }
	    if (path.fp_names[0] == tmp_path.fp_names[0])
		gottree = 1;
	    db_free_full_path(&tmp_path);
	    if (gottree)
		break;
	}

	/* if we don't already have it, get it */
	if (!gottree && rt_gettree(rtip, argv[i])) {
	    if (msgs)
		bu_vls_printf(msgs, "rt_gettree failed for %s\n", argv[i]);
	    rt_i_destroy(rtip);
	    db_free_full_path(&path);
	    return BRLCAD_ERROR;
	}
	db_free_full_path(&path);
    }

    /* prep calculates bounding boxes of solids */
    rt_prep(rtip);

    /* initialize RPP bounds */
    VSETALL(rpp_min, INFINITY);
    VSETALL(rpp_max, -INFINITY);
    for (i = 0; i < argc; i++) {
	vect_t reg_min = VINIT_ZERO;
	vect_t reg_max = VINIT_ZERO;
	const char *reg_name = NULL;
	size_t name_len = 0;

	/* check if input name is a region */
	for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
	    reg_name = regp->reg_name;
	    if (*argv[i] != '/' && *reg_name == '/')
		reg_name++;

	    if (BU_STR_EQUAL(reg_name, argv[i])) {
		/* input name was a region */
		if (rt_bound_tree(regp->reg_treetop, reg_min, reg_max)) {
		    if (msgs)
			bu_vls_printf(msgs, "rt_bound_tree failed for %s\n", regp->reg_name);
		    rt_i_destroy(rtip);
		    return BRLCAD_ERROR;
		}
		VMINMAX(rpp_min, rpp_max, reg_min);
		VMINMAX(rpp_min, rpp_max, reg_max);

		goto found;
	    }
	}

	/* input name may be a group, need to check all regions under
	 * that group
	 */
	name_len = strlen(argv[i]);
	for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
	    reg_name = regp->reg_name;
	    if (*argv[i] != '/' && *reg_name == '/')
		reg_name++;

	    if (bu_strncmp(argv[i], reg_name, name_len))
		continue;

	    /* This is part of the group */
	    if (rt_bound_tree(regp->reg_treetop, reg_min, reg_max)) {
		if (msgs)
		    bu_vls_printf(msgs, "rt_bound_tree failed for %s\n", regp->reg_name);
		rt_i_destroy(rtip);
		return BRLCAD_ERROR;
	    }
	    VMINMAX(rpp_min, rpp_max, reg_min);
	    VMINMAX(rpp_min, rpp_max, reg_max);
	}

    found:;
    }

    rt_i_destroy(rtip);

    return BRLCAD_OK;
}


struct rt_obj_bounds_mat_state {
    point_t minimum;
    point_t maximum;
    int flags;
    int use_air;
    int have_bounds;
    int error;
};


static int
rt_bounds_are_valid(const point_t minimum, const point_t maximum)
{
    int axis;

    for (axis = 0; axis < 3; axis++) {
	if (isnan(minimum[axis]) || isnan(maximum[axis]))
	    return 0;
    }
    return 1;
}


static int
rt_bounds_are_empty(const point_t minimum, const point_t maximum)
{
    int axis;

    for (axis = 0; axis < 3; axis++) {
	if (minimum[axis] > maximum[axis])
	    return 1;
    }
    return 0;
}


static int
rt_bounds_are_finite(const point_t minimum, const point_t maximum)
{
    int axis;

    if (!rt_bounds_are_valid(minimum, maximum) ||
	rt_bounds_are_empty(minimum, maximum))
	return 0;
    for (axis = 0; axis < 3; axis++) {
	if (!isfinite(minimum[axis]) || !isfinite(maximum[axis]))
	    return 0;
    }
    return 1;
}


static int
rt_obj_bounds_mat_region_start(struct db_tree_state *state,
			       const struct db_full_path *UNUSED(path),
			       const struct rt_comb_internal *UNUSED(combination),
			       void *client_data)
{
    struct rt_obj_bounds_mat_state *bounds =
	(struct rt_obj_bounds_mat_state *)client_data;

    return !bounds->use_air && state->ts_aircode != 0 ? -1 : 0;
}


static union tree *
rt_obj_bounds_mat_leaf(struct db_tree_state *state,
		       const struct db_full_path *UNUSED(path),
		       struct rt_db_internal *internal,
		       void *client_data)
{
    struct rt_obj_bounds_mat_state *bounds =
	(struct rt_obj_bounds_mat_state *)client_data;
    point_t minimum, maximum;
    struct soltab *solid;
    union tree *leaf;

    VSETALL(minimum, INFINITY);
    VSETALL(maximum, -INFINITY);
    if (rt_bound_db_internal(&minimum, &maximum, internal,
		state->ts_ttol, state->ts_tol) < 0) {
	bounds->error = 1;
	return TREE_NULL;
    }
    if (!rt_bounds_are_valid(minimum, maximum)) {
	bounds->error = 1;
	return TREE_NULL;
    }

    BU_GET(solid, struct soltab);
    *solid = (struct soltab)RT_SOLTAB_INIT_ZERO;
    solid->l.magic = RT_SOLTAB_MAGIC;
    solid->l2.magic = RT_SOLTAB2_MAGIC;
    solid->st_aradius = rt_bounds_are_finite(minimum, maximum) ? 1.0 : INFINITY;
    VMOVE(solid->st_min, minimum);
    VMOVE(solid->st_max, maximum);

    BU_GET(leaf, union tree);
    RT_TREE_INIT(leaf);
    leaf->tr_op = OP_SOLID;
    leaf->tr_a.tu_stp = solid;
    return leaf;
}


static void
rt_obj_bounds_mat_free_tree(union tree *tree, int keep_root)
{
    if (!tree)
	return;

    switch (tree->tr_op) {
	case OP_SOLID:
	    if (tree->tr_a.tu_stp)
		BU_PUT(tree->tr_a.tu_stp, struct soltab);
	    break;
	case OP_UNION:
	case OP_XOR:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	    rt_obj_bounds_mat_free_tree(tree->tr_b.tb_left, 0);
	    rt_obj_bounds_mat_free_tree(tree->tr_b.tb_right, 0);
	    break;
	case OP_GUARD:
	case OP_XNOP:
	case OP_NOT:
	    rt_obj_bounds_mat_free_tree(tree->tr_b.tb_left, 0);
	    break;
	default:
	    break;
    }

    if (keep_root) {
	RT_TREE_INIT(tree);
	tree->tr_op = OP_NOP;
    } else {
	BU_PUT(tree, union tree);
    }
}


static union tree *
rt_obj_bounds_mat_region_end(struct db_tree_state *UNUSED(state),
			     const struct db_full_path *UNUSED(path),
			     union tree *tree,
			     void *client_data)
{
    struct rt_obj_bounds_mat_state *bounds =
	(struct rt_obj_bounds_mat_state *)client_data;
    point_t minimum, maximum;

    VSETALL(minimum, INFINITY);
    VSETALL(maximum, -INFINITY);
    if (rt_bound_tree(tree, minimum, maximum) < 0 ||
	!rt_bounds_are_valid(minimum, maximum)) {
	bounds->error = 1;
    } else if (!rt_bounds_are_empty(minimum, maximum) &&
	(rt_bounds_are_finite(minimum, maximum) ||
	 !(bounds->flags & RT_OBJ_BOUNDS_IGNORE_INFINITE))) {
	if (!bounds->have_bounds) {
	    VMOVE(bounds->minimum, minimum);
	    VMOVE(bounds->maximum, maximum);
	    bounds->have_bounds = 1;
	} else {
	    VMIN(bounds->minimum, minimum);
	    VMAX(bounds->maximum, maximum);
	}
    }

    rt_obj_bounds_mat_free_tree(tree, 1);
    return tree;
}


int
rt_obj_bounds_mat(struct bu_vls *msgs,
		  struct db_i *dbip,
		  int argc,
		  const char *argv[],
		  int use_air,
		  const mat_t model2bounds,
		  int flags,
		  point_t rpp_min,
		  point_t rpp_max)
{
    struct db_tree_state state = RT_DBTS_INIT_IDN;
    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct rt_obj_bounds_mat_state bounds;
    int status;

    if (!dbip || argc <= 0 || !argv || !rpp_min || !rpp_max ||
	(flags & ~RT_OBJ_BOUNDS_IGNORE_INFINITE))
	return BRLCAD_ERROR;

    VSETALL(bounds.minimum, INFINITY);
    VSETALL(bounds.maximum, -INFINITY);
    bounds.flags = flags;
    bounds.use_air = use_air;
    bounds.have_bounds = 0;
    bounds.error = 0;
    state.ts_dbip = dbip;
    state.ts_ttol = &ttol;
    state.ts_tol = &tol;
    if (model2bounds)
	MAT_COPY(state.ts_mat, model2bounds);

    status = db_walk_tree(dbip, argc, argv, 1, &state,
	rt_obj_bounds_mat_region_start, rt_obj_bounds_mat_region_end,
	rt_obj_bounds_mat_leaf, &bounds);
    if (status < 0 || bounds.error || !bounds.have_bounds) {
	if (msgs)
	    bu_vls_printf(msgs, "unable to determine transformed object bounds\n");
	return BRLCAD_ERROR;
    }

    VMOVE(rpp_min, bounds.minimum);
    VMOVE(rpp_max, bounds.maximum);
    return BRLCAD_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

/*                          B B O X . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2022 United States Government as represented by
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

#include <string.h>

#include "bu/path.h"
#include "vmath.h"
#include "raytrace.h"


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
	    /* Discard right rpp */
	    break;
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


/**
 * Traverse the passed tree using rt_db_internals to show the way
 * This function supports rt_bound_internal and is internal to librt
 *
 * Returns -
 * 0 success
 * -1 failure (tree_min and tree_max may have been altered)
 */
int
rt_traverse_tree(struct rt_i *rtip, const union tree *tp, fastf_t *tree_min, fastf_t *tree_max)
{
    vect_t r_min, r_max;		/* rpp for right side of tree */

    RT_CK_TREE(tp);

    switch (tp->tr_op) {

	case OP_SOLID:
	    {
		const struct soltab *stp;

		stp = tp->tr_a.tu_stp;
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
	    if (rt_traverse_tree(rtip, tp->tr_b.tb_left, tree_min, tree_max) < 0 ||
		rt_traverse_tree(rtip, tp->tr_b.tb_right, r_min, r_max) < 0)
		return -1;
	    VMIN(tree_min, r_min);
	    VMAX(tree_max, r_max);
	    break;
	case OP_INTERSECT:
	    /* BINARY type -- find common area only */
	    if (rt_traverse_tree(rtip, tp->tr_b.tb_left, tree_min, tree_max) < 0 ||
		rt_traverse_tree(rtip, tp->tr_b.tb_right, r_min, r_max) < 0)
		return -1;
	    /* min = largest min, max = smallest max */
	    VMAX(tree_min, r_min);
	    VMIN(tree_max, r_max);
	    break;
	case OP_SUBTRACT:
	    /* BINARY type -- just use left tree */
	    if (rt_traverse_tree(rtip, tp->tr_b.tb_left, tree_min, tree_max) < 0 ||
		rt_traverse_tree(rtip, tp->tr_b.tb_right, r_min, r_max) < 0)
		return -1;
	    /* Discard right rpp */
	    break;

	    /* This case is especially for handling rt_db_internal formats which generally contain a "unloaded"
	     * tree with tp->tr_op = OP_DB_LEAF
	     */
	case OP_DB_LEAF:
	    {
		const struct soltab *stp;

		if (rtip == NULL) {
		    bu_log("rt_traverse_tree: A valid rtip was not passed for calculating bounds of '%s'\n",
			   tp->tr_l.tl_name);
		    return -1;
		}

		/* Good to go */

		/* Attempt to get a solid pointer, will fail for combs */
		stp = rt_find_solid(rtip, tp->tr_l.tl_name);
		if (stp == NULL) {

		    /* It was a comb! get an internal format and repeat the whole thing that got us here
		     * in the 1st place
		     */
		    struct rt_db_internal intern;
		    struct directory *dp;
		    struct rt_comb_internal *combp;

		    /* Get the directory pointer */
		    if ((dp=db_lookup(rtip->rti_dbip, tp->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
			bu_log("rt_traverse_tree: db_lookup(%s) failed", tp->tr_l.tl_name);
			return -1;
		    }

		    /* Why does recursion work with the internal format ?
		     * The internal format does have the boolean op for a comb in the tr_a.tu_op field
		     * in the root node, even though it has no prims at the leaves.
		     * So recursive calls to load the prim further down the tree, will return the correct bb
		     * as we are going through the proper switch case in each step down the tree
		     */
		    if (!rt_db_lookup_internal(rtip->rti_dbip, tp->tr_l.tl_name, &dp, &intern, LOOKUP_NOISY,
					       &rt_uniresource)) {
			bu_log("rt_traverse_tree: rt_db_lookup_internal(%s) failed to get the internal form",
			       tp->tr_l.tl_name);
			return -1;
		    }

		    /* The passed rt_db_internal should be a comb, prepare a rt_comb_internal */
		    if (intern.idb_minor_type == ID_COMBINATION) {
			combp = (struct rt_comb_internal *)intern.idb_ptr;
		    } else {
			/* if it's not a comb, then something else is cooking */
			bu_log("rt_traverse_tree: WARNING : rt_db_lookup_internal(%s) got the internal form of a primitive when it should not, the bounds may not be correct", tp->tr_l.tl_name);
			return -1;
		    }

		    RT_CK_COMB(combp);
		    /* further down the rabbit hole */
		    if (rt_traverse_tree(rtip, combp->tree, tree_min, tree_max)) {
			bu_log("rt_traverse_tree: rt_bound_tree() failed\n");
			return -1;
		    }
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
	    /* Implies that this tree has nothing in it */
	    break;
    }
    return 0;
}

int
rt_bound_instance(point_t *bmin, point_t *bmax,
           struct directory *dp,
           struct db_i *dbip,
           const struct bg_tess_tol *ttol,
           const struct bn_tol *tol,
           mat_t *s_mat,
           struct resource *res,
           struct bview *v
	)
{
    if (UNLIKELY(!bmin || !bmax || !dp || !dbip || !res))
	return -1;

    VSET(*bmin, INFINITY, INFINITY, INFINITY);
    VSET(*bmax, -INFINITY, -INFINITY, -INFINITY);

    struct rt_db_internal dbintern;
    RT_DB_INTERNAL_INIT(&dbintern);
    struct rt_db_internal *ip = &dbintern;
    int ret = rt_db_get_internal(ip, dp, dbip, *s_mat, res);
    if (ret < 0)
	return -1;

    int bbret = -1;
    if (ip->idb_meth->ft_bbox)
	bbret = ip->idb_meth->ft_bbox(ip, bmin, bmax, tol);

    if (bbret < 0 && ip->idb_meth->ft_plot) {
	/* As a fallback for primitives that don't have a bbox function,
	 * (there are still some as of 2021) use the old bounding method of
	 * calculating the default (non-adaptive) plot for the primitive
	 * and using the extent of the plotted segments as the bounds.
	 */
	struct bu_list vhead;
	BU_LIST_INIT(&(vhead));
	if (ip->idb_meth->ft_plot(&vhead, ip, ttol, tol, v) >= 0) {
	    if (bv_vlist_bbox(&vhead, bmin, bmax, NULL, NULL)) {
		if (v)
		    BV_FREE_VLIST(&v->gv_objs.gv_vlfree, &vhead);
		rt_db_free_internal(&dbintern);
		return -1;
	    }
	    if (v)
		BV_FREE_VLIST(&v->gv_objs.gv_vlfree, &vhead);
	    bbret = 0;
	}
    }

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
    union tree *tp;

    /* Initialize RPP bounds */
    VSETALL(rpp_min, MAX_FASTF);
    VREVERSE(rpp_max, rpp_min);

    if ((rtip=rt_new_rti(dbip)) == RTI_NULL) {
	bu_log("rt_bound_internal: rt_new_rti() failure while getting bb for %s\n", dp->d_namep);
	return -1;
    }

    /* Call rt_gettree() to get the bounds for primitives, else soltab ptr is null */
    if (rt_gettree(rtip, dp->d_namep) < 0) {
	bu_log("rt_bound_internal: rt_gettree('%s') failed\n", dp->d_namep);
	rt_free_rti(rtip);
	return -1;
    }


    if (!rt_db_lookup_internal(dbip, dp->d_namep, &dp, &intern, LOOKUP_NOISY, &rt_uniresource)) {
	bu_exit(1, "rt_bound_internal: rt_db_lookup_internal(%s) failed to get the internal form", dp->d_namep);
	rt_free_rti(rtip);
	return -1;
    }

    /* If passed rt_db_internal is a combination(a group or a region) then further calls needed */
    if (intern.idb_minor_type == ID_COMBINATION) {
	combp = (struct rt_comb_internal *)intern.idb_ptr;
    } else {
	/* A primitive was passed, construct a struct rt_comb_internal with a single leaf node */
	BU_ALLOC(combp, struct rt_comb_internal);
	RT_COMB_INTERNAL_INIT(combp);
	combp->region_flag = 0;

	BU_GET(tp, union tree);
	RT_TREE_INIT(tp);
	tp->tr_l.tl_op = OP_SOLID;
	tp->tr_l.tl_name = "dummy";
	tp->tr_l.tl_mat = (matp_t)NULL;
	tp->tr_a.tu_stp = rt_find_solid(rtip, dp->d_namep);
	combp->tree = tp;
    }

    RT_CK_COMB(combp);
    if (rt_traverse_tree(rtip, combp->tree, tree_min, tree_max)) {
	bu_log("rt_bound_internal: rt_bound_tree() failed\n");
	rt_db_free_internal(&intern);
	rt_free_rti(rtip);
	return -1;
    }


    VMOVE(rpp_min, tree_min);
    VMOVE(rpp_max, tree_max);

    /* Check if the model bounds look correct e.g. if they are all 0, then it's not correct */
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
    rt_free_rti(rtip);
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
    rtip = rt_new_rti(dbip);
    if (rtip == RTI_NULL) {
	if (msgs)
	    bu_vls_printf(msgs, "rt_new_rti failure for %s\n", dbip->dbi_filename);
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
	    rt_free_rti(rtip);
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
		rt_free_rti(rtip);
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
	    rt_free_rti(rtip);
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
		    rt_free_rti(rtip);
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
		rt_free_rti(rtip);
		return BRLCAD_ERROR;
	    }
	    VMINMAX(rpp_min, rpp_max, reg_min);
	    VMINMAX(rpp_min, rpp_max, reg_max);
	}

    found:;
    }

    rt_free_rti(rtip);

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

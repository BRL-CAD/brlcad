/*                          B B O X . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2011 United States Government as represented by
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

#include "bu.h"
#include "vmath.h"
#include "raytrace.h"


/**
 * R T _ B O U N D _ T R E E
 *
 * Calculate the bounding RPP of the region whose boolean tree is
 * 'tp'.  The bounding RPP is returned in tree_min and tree_max, which
 * need not have been initialized first.
 *
 * Returns -
 * 0 success
 * -1 failure (tree_min and tree_max may have been altered)
 */
int
rt_bound_tree(const union tree *tp, fastf_t *tree_min, fastf_t *tree_max)
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
	    bu_log("rt_bound_tree(x%x): unknown op=x%x\n",
		   tp, tp->tr_op);
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
HIDDEN struct region *
_rt_getregion(struct rt_i *rtip, const char *reg_name)
{
    struct region *regp;
    char *reg_base = bu_basename(reg_name);

    RT_CK_RTI(rtip);
    for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
	char *cp;
	/* First, check for a match of the full path */
	if (*reg_base == regp->reg_name[0] &&
	    BU_STR_EQUAL(reg_base, regp->reg_name)){
	    bu_free(reg_base, "reg_base free");
	    return regp;
	}
	/* Second, check for a match of the database node name */
	cp = bu_basename(regp->reg_name);
	if (*cp == *reg_name && BU_STR_EQUAL(cp, reg_name)){
	    bu_free(reg_base, "reg_base free");
	    bu_free(cp, "cp free");
	    return regp;
	}
	bu_free(cp, "cp free");
    }
    bu_free(reg_base, "reg_base free");

    return REGION_NULL;
}


/**
 * R T _ R P P _ R E G I O N
 *
 * Calculate the bounding RPP for a region given the name of the
 * region node in the database.  See remarks in _rt_getregion() above
 * for name conventions.  Returns 0 for failure (and prints a
 * diagnostic), or 1 for success.
 */
int
rt_rpp_region(struct rt_i *rtip, const char *reg_name, fastf_t *min_rpp, fastf_t *max_rpp)
{
    struct region *regp;

    RT_CHECK_RTI(rtip);

    regp = _rt_getregion(rtip, reg_name);
    if (regp == REGION_NULL) return 0;
    if (rt_bound_tree(regp->reg_treetop, min_rpp, max_rpp) < 0)
	return 0;
    return 1;
}


/**
 * R T _ I N _ R P P
 *
 * Compute the intersections of a ray with a rectangular parallelpiped
 * (RPP) that has faces parallel to the coordinate planes
 *
 * The algorithm here was developed by Gary Kuehl for GIFT.  A good
 * description of the approach used can be found in "??" by XYZZY and
 * Barsky, ACM Transactions on Graphics, Vol 3 No 1, January 1984.
 *
 * Note: The computation of entry and exit distance is mandatory, as
 * the final test catches the majority of misses.
 *
 * Note: A hit is returned if the intersect is behind the start point.
 *
 * Returns -
 * 0 if ray does not hit RPP,
 * !0 if ray hits RPP.
 *
 * Implicit return -
 * rp->r_min = dist from start of ray to point at which ray ENTERS solid
 * rp->r_max = dist from start of ray to point at which ray LEAVES solid
 */
int
rt_in_rpp(struct xray *rp,
	  register const fastf_t *invdir,	/* inverses of rp->r_dir[] */
	  register const fastf_t *min,
	  register const fastf_t *max)
{
    register const fastf_t *pt = &rp->r_pt[0];
    register fastf_t sv;
#define st sv			/* reuse the register */
    register fastf_t rmin = -INFINITY;
    register fastf_t rmax =  INFINITY;

    /* Start with infinite ray, and trim it down */

    /* X axis */
    if (*invdir < 0.0) {
	/* Heading towards smaller numbers */
	/* if (*min > *pt) miss */
	if (rmax > (sv = (*min - *pt) * *invdir))
	    rmax = sv;
	if (rmin < (st = (*max - *pt) * *invdir))
	    rmin = st;
    }  else if (*invdir > 0.0) {
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
    if (*invdir < 0.0) {
	if (rmax > (sv = (*min - *pt) * *invdir))
	    rmax = sv;
	if (rmin < (st = (*max - *pt) * *invdir))
	    rmin = st;
    }  else if (*invdir > 0.0) {
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
    if (*invdir < 0.0) {
	if (rmax > (sv = (*min - *pt) * *invdir))
	    rmax = sv;
	if (rmin < (st = (*max - *pt) * *invdir))
	    rmin = st;
    }  else if (*invdir > 0.0) {
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
 * Supporting function for R T _ B O U N D _ I N T E R N A L
 *
 * Called when a combination is detected during db_functree() tree walk 
 *
 * Returns -
 *  0 success
 * -1 failure
 * 
 */
void
comb_func(struct db_i *dbip,
	  struct directory *dp,
	  genptr_t ptr)
{
    struct rt_db_internal *intern = NULL;

    bu_log("rt_bound_internal: comb_func called\n");

    if (rt_db_get_internal(intern, dp, dbip, ptr, &rt_uniresource) < 0){
        bu_log("rt_bound_internal: comb_func : rt_db_get_internal('%s') failed\n", dp->d_namep);       
    }    
  
}


/**
 * Supporting function for R T _ B O U N D _ I N T E R N A L
 *
 * Called when a leaf is detected during db_functree() tree walk 
 *
 * Returns -
 *  0 success
 * -1 failure
 *
 */
void
leaf_func(struct db_i *dbip,
	  struct directory *dp,
	  genptr_t ptr)
{
    struct rt_db_internal *intern = NULL;

    bu_log("rt_bound_internal: leaf_func called\n");

    if (rt_db_get_internal(intern, dp, dbip, ptr, &rt_uniresource) < 0){
        bu_log("rt_bound_internal: leaf_func : rt_db_get_internal('%s') failed\n", dp->d_namep);        
    }   
}
 
/**
 * R T _ B O U N D _ I N T E R N A L
 *
 * Calculate the bounding RPP of the internal format passed in 'ip'. 
 * The bounding RPP is returned in rpp_min and rpp_max in mm
 *
 * Returns -
 *  0 success
 * -1 failure
 * -2 a region/comb was passed so model bounds are 0(will be fixed soon)
 */
int
rt_bound_internal(struct rt_db_internal *ip, point_t rpp_min, point_t rpp_max)
{
    struct db_i *dbip; 
    struct directory *dp = (struct directory *)NULL; 
    struct rt_i *rtip;
    struct rt_comb_internal comb;
    struct rt_comb_internal *combp = &comb;
    vect_t tree_min, tree_max;
   
    RT_CK_DB_INTERNAL(ip);
   
    /* Create an empty db_i */
    dbip = db_open_inmem();   
   
    RT_CK_DBI(dbip);
    RT_CK_WDB(dbip->dbi_wdbp);
   
    /* Insert the passed struct rt_db_internal into the db_i, use a dummy name */
    if (wdb_put_internal(dbip->dbi_wdbp, "dummy", ip, 1.0) < 0) {
        bu_log("rt_bound_internal: wdb_put_internal() failed\n");
        db_close(dbip);
        return -1;
    }
   
    /* The rt_db_internal pointed to by ip is(annoyingly!) freed by wdb_put_internal(), so look it up */
    dp = db_lookup(dbip, "dummy", LOOKUP_NOISY);
    if (!dp) {
        bu_log("rt_bound_internal: db_lookup('dummy') failed\n");
        db_close(dbip);
        return -1;
    }
   
    /* Now get back the rt_db_internal representation of the user passed object */
    if (rt_db_get_internal(ip, dp, dbip, bn_mat_identity, &rt_uniresource) < 0) {
        bu_log("rt_bound_internal:  rt_db_get_internal('dummy') failed\n");
        db_close(dbip);
        return -1;
    }
   
    /* Get a new rtip for the dbip */
    rtip = rt_new_rti(dbip);
   
    /* Initialize RPP bounds */
    VSETALL(rpp_min, MAX_FASTF);
    VREVERSE(rpp_max, rpp_min);
   
    /* Call rt_gettree() to get the bounds, this is sufficient for primitives : may not be required soon */
    if (rt_gettree(rtip, "dummy") < 0){
        bu_log("rt_bound_internal: rt_gettree('dummy') failed\n");
        goto cleanup;
        return -1;
    }
    
    /* For combinations, walk the tree once adding leaves and combinations to the dbip as detected */
    db_functree(dbip, dp, comb_func, leaf_func, &rt_uniresource, NULL);

         
    /* If passed rt_db_internal is a combination(a group or a region) then further calls needed */
    if(ip->idb_minor_type == ID_COMBINATION){
        combp = (struct rt_comb_internal *)ip->idb_ptr;       
    }
    else{
        VMOVE(rpp_min, rtip->mdl_min); /* temporary, till db_functree() works */
        VMOVE(rpp_max, rtip->mdl_max);        
        goto cleanup;
        return 0;        
        
        /* A primitive was passed, construct a struct rt_comb_internal with a single leaf node */
        /*RT_COMB_INTERNAL_INIT(combp);   
        union tree *tp;
        RT_GET_TREE(tp, &rt_uniresource);
        tp->tr_op = OP_DB_LEAF;
        tp->tr_l.tl_name = bu_strdup(dp->d_namep);
        combp->tree = tp;*/              
    }
   
    RT_CK_COMB(combp);
    if (rt_bound_tree(combp->tree, tree_min, tree_max)) {
        bu_log("rt_bound_internal: rt_bound_tree('region') failed\n");
        goto cleanup;
        return -1;
    }
          
    VMOVE(rpp_min, tree_min);
    VMOVE(rpp_max, tree_max);
   
    /* Check if the model bounds look correct e.g. if they are all 0, then its not correct */
    if(  (abs(rpp_min[0]) == 0  || rpp_min[0] <= -INFINITY || rpp_min[0] >= INFINITY) &&
         (abs(rpp_min[1]) == 0  || rpp_min[1] <= -INFINITY || rpp_min[1] >= INFINITY) &&
         (abs(rpp_min[2]) == 0  || rpp_min[2] <= -INFINITY || rpp_min[2] >= INFINITY) &&
         (abs(rpp_max[0]) == 0  || rpp_max[0] <= -INFINITY || rpp_max[0] >= INFINITY) &&
         (abs(rpp_max[1]) == 0  || rpp_max[1] <= -INFINITY || rpp_max[1] >= INFINITY) &&
         (abs(rpp_max[2]) == 0  || rpp_max[2] <= -INFINITY || rpp_max[2] >= INFINITY)){       
       bu_log("rt_bound_internal: Warning : The returned bounds of the model may not be correct\n");         
    }     
   
    cleanup:
    rt_free_rti(rtip);
    db_close(dbip);   
   
    return 0;
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

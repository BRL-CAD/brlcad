/*                         M A T E R . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @file librt/mater.c
 *
 * Code to deal with establishing and maintaining the tables which map
 * region ID codes into worthwhile material information (colors and
 * outboard database "handles").
 *
 * These really are "db_" routines, more fundamental than "rt_".
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include "bio.h"

#include "vmath.h"
#include "raytrace.h"
#include "mater.h"


/*
 * It is expected that entries on this mater list will be sorted in
 * strictly ascending order, with no overlaps (ie, monotonicly
 * increasing).
 */
static struct mater *material_head = MATER_NULL;


/**
 *
 */
void
rt_pr_mater(register const struct mater *mp)
{
    (void)bu_log("%5d..%d\t", mp->mt_low, mp->mt_high);
    (void)bu_log("%d, %d, %d\t", mp->mt_r, mp->mt_g, mp->mt_b);
}


HIDDEN void
_rt_check_overlap(struct mater *newp)
{
    struct mater *zot;

    /* Check for overlap, ie, redefinition of following colors */
    while (newp->mt_forw != MATER_NULL &&
	   newp->mt_high >= newp->mt_forw->mt_low)
    {
	if (newp->mt_high >= newp->mt_forw->mt_high) {
	    /* Drop this mater struct */
	    zot = newp->mt_forw;
	    newp->mt_forw = zot->mt_forw;
	    bu_log("dropping overlaping region-id based material property entry:\n");
	    rt_pr_mater(zot);
	    bu_free((char *)zot, "getstruct mater");
	    continue;
	}
	if (newp->mt_high >= newp->mt_forw->mt_low) {
	    /* Shorten this mater struct, then done */
	    bu_log("Shortening region-id based material property entry rhs range, from:\n");
	    rt_pr_mater(newp->mt_forw);
	    bu_log("to:\n");
	    newp->mt_forw->mt_low = newp->mt_high+1;
	    rt_pr_mater(newp->mt_forw);
	    continue;	/* more conservative than returning */
	}
    }
}


/**
 * While any additional database records are created and written here,
 * it is the responsibility of the caller to color_putrec(newp) if
 * needed.
 */
void
rt_insert_color(struct mater *newp)
{
    struct mater *mp;
    struct mater *zot;

    if (material_head == MATER_NULL || newp->mt_high < material_head->mt_low) {
	/* Insert at head of list */
	newp->mt_forw = material_head;
	material_head = newp;
	return;
    }
    if (newp->mt_low < material_head->mt_low) {
	/* Insert at head of list, check for redefinition */
	newp->mt_forw = material_head;
	material_head = newp;
	_rt_check_overlap(newp);
	return;
    }
    for (mp = material_head; mp != MATER_NULL; mp = mp->mt_forw) {
	if (mp->mt_low == newp->mt_low && mp->mt_high <= newp->mt_high) {
	    bu_log("dropping overwritten region-id based material property entry:\n");
	    newp->mt_forw = mp->mt_forw;
	    rt_pr_mater(mp);
	    *mp = *newp;		/* struct copy */
	    bu_free((char *)newp, "getstruct mater");
	    newp = mp;
	    _rt_check_overlap(newp);
	    return;
	}
	if (mp->mt_low < newp->mt_low && mp->mt_high > newp->mt_high) {
	    /* New range entirely contained in old range; split */
	    bu_log("Splitting region-id based material property entry into 3 ranges\n");
	    BU_GETSTRUCT(zot, mater);
	    *zot = *mp;		/* struct copy */
	    zot->mt_daddr = MATER_NO_ADDR;
	    /* zot->mt_high = mp->mt_high; */
	    zot->mt_low = newp->mt_high+1;
	    mp->mt_high = newp->mt_low-1;
	    /* mp, newp, zot */
	    /* zot->mt_forw = mp->mt_forw; */
	    newp->mt_forw = zot;
	    mp->mt_forw = newp;
	    rt_pr_mater(mp);
	    rt_pr_mater(newp);
	    rt_pr_mater(zot);
	    return;
	}
	if (mp->mt_high > newp->mt_low) {
	    /* Overlap to the left: Shorten preceeding entry */
	    bu_log("Shortening region-id based material property entry lhs range, from:\n");
	    rt_pr_mater(mp);
	    bu_log("to:\n");
	    mp->mt_high = newp->mt_low-1;
	    rt_pr_mater(mp);
	    /* Now append */
	    newp->mt_forw = mp->mt_forw;
	    mp->mt_forw = newp;
	    _rt_check_overlap(newp);
	    return;
	}
	if (mp->mt_forw == MATER_NULL ||
	    newp->mt_low < mp->mt_forw->mt_low) {
	    /* Append */
	    newp->mt_forw = mp->mt_forw;
	    mp->mt_forw = newp;
	    _rt_check_overlap(newp);
	    return;
	}
    }
    bu_log("fell out of rt_insert_color loop, append region-id based material property entry to end of list\n");
    /* Append at end */
    newp->mt_forw = MATER_NULL;
    mp->mt_forw = newp;
    return;
}


/**
 * Called from db_scan() when initially scanning database.
 */
void
rt_color_addrec(int low, int hi, int r, int g, int b, off_t addr)
{
    register struct mater *mp;

    BU_GETSTRUCT(mp, mater);
    mp->mt_low = low;
    mp->mt_high = hi;
    mp->mt_r = r;
    mp->mt_g = g;
    mp->mt_b = b;
    mp->mt_daddr = addr;
    rt_insert_color(mp);
}


/**
 * If the GIFT regionid of this region falls into a mapped area of
 * regionid-driven color override.
 */
void
rt_region_color_map(register struct region *regp)
{
    register struct mater *mp;

    if (regp == REGION_NULL) {
	bu_log("color_map(NULL)\n");
	return;
    }
    for (mp = material_head; mp != MATER_NULL; mp = mp->mt_forw) {
	if (regp->reg_regionid <= mp->mt_high &&
	    regp->reg_regionid >= mp->mt_low) {
	    regp->reg_mater.ma_color_valid = 1;
	    regp->reg_mater.ma_color[0] =
		(((double)mp->mt_r)+0.5)*bn_inv255;
	    regp->reg_mater.ma_color[1] =
		(((double)mp->mt_g)+0.5)*bn_inv255;
	    regp->reg_mater.ma_color[2] =
		(((double)mp->mt_b)+0.5)*bn_inv255;
	    return;
	}
    }
}


/**
 *
 */
void
rt_vls_color_map(struct bu_vls *str)
{
    struct mater *mp;

    BU_CK_VLS(str);

    for (mp = material_head; mp != MATER_NULL; mp = mp->mt_forw) {
	bu_vls_printf(str,
		      "{%d %d %d %d %d} ",
		      mp->mt_low,
		      mp->mt_high,
		      mp->mt_r,
		      mp->mt_g,
		      mp->mt_b);
    }
}


/**
 * returns the material linked list head node
 */
struct mater *
rt_material_head()
{
    return material_head;
}


/**
 * set the material linked list head node
 */
void
rt_new_material_head(struct mater *newmat)
{
    material_head = newmat;
}


/**
 * returns a copy of the material linked list head node
 */
struct mater *
rt_dup_material_head()
{
    register struct mater *mp = NULL;
    register struct mater *newmp = NULL;
    struct mater *newmater = NULL;
    struct mater *dupmater = NULL;

    mp = material_head;
    while (mp != MATER_NULL) {
	BU_GETSTRUCT(newmater, mater);
	*newmater = *mp; /* struct copy */
	newmater->mt_forw = MATER_NULL;

	if (dupmater == NULL) {
	    dupmater = newmater;
	} else {
	    newmp->mt_forw = newmater;
	}

	newmp = newmater;
	mp = mp->mt_forw;
    }

    return dupmater;
}


/**
 * Really should be db_color_free().  Called from db_close().
 */
void
rt_color_free(void)
{
    register struct mater *mp;

    while ((mp = material_head) != MATER_NULL) {
	material_head = mp->mt_forw;	/* Dequeue 'mp' */
	/* mt_handle? */
	bu_free((char *)mp, "getstruct mater");
	mp = MATER_NULL;
    }
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

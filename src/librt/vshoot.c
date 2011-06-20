/*                        V S H O O T . C
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
/** @addtogroup ray */
/** @{ */
/** @file librt/vshoot.c
 *
 * EXPERIMENTAL vector version of the Ray Tracing program shot
 * coordinator.
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "vmath.h"
#include "raytrace.h"


/**
 * Stub function which will "similate" a call to a vector shot routine
 */
HIDDEN void
vshot_stub(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
/* An array of solid pointers */
/* An array of ray pointers */
/* array of segs (results returned) */
/* Number of ray/object pairs */
/* pointer to an application */
{
    register int i;
    register struct seg *tmp_seg;
    struct seg seghead;
    int ret;

    BU_LIST_INIT(&(seghead.l));

    /* go through each ray/solid pair and call a scalar function */
    for (i = 0; i < n; i++) {
	if (stp[i] != 0) {
	    /* skip call if solid table pointer is NULL */
	    /* do scalar call, place results in segp array */
	    ret = -1;
	    if (rt_functab[stp[i]->st_id].ft_shot) {
		ret = rt_functab[stp[i]->st_id].ft_shot(stp[i], rp[i], ap, &seghead);
	    }
	    if (ret <= 0) {
		segp[i].seg_stp=(struct soltab *) 0;
	    } else {
		tmp_seg = BU_LIST_FIRST(seg, &(seghead.l));
		BU_LIST_DEQUEUE(&(tmp_seg->l));
		segp[i] = *tmp_seg; /* structure copy */
		RT_FREE_SEG(tmp_seg, ap->a_resource);
	    }
	}
    }
}


/**
 * R T _ S H O O T R A Y
 *
 * Given a ray, shoot it at all the relevant parts of the model,
 * (building the HeadSeg chain), and then call rt_boolregions() to
 * build and evaluate the partition chain.  If the ray actually hit
 * anything, call the application's a_hit() routine with a pointer to
 * the partition chain, otherwise, call the application's a_miss()
 * routine.
 *
 * It is important to note that rays extend infinitely only in the
 * positive direction.  The ray is composed of all points P, where
 *
 * P = r_pt + K * r_dir
 *
 * for K ranging from 0 to +infinity.  There is no looking backwards.
 *
 * It is also important to note that the direction vector r_dir must
 * have unit length; this is mandatory, and is not ordinarily checked,
 * in the name of efficiency.
 *
 * Input:  Pointer to an application structure, with these mandatory fields:
 * a_ray.r_pt Starting point of ray to be fired
 * a_ray.r_dir UNIT VECTOR with direction to fire in (dir cosines)
 * a_hit Routine to call when something is hit
 * a_miss Routine to call when ray misses everything
 *
 * Calls user's a_miss() or a_hit() routine as appropriate.  Passes
 * a_hit() routine list of partitions, with only hit_dist fields
 * valid.  Normal computation deferred to user code, to avoid needless
 * computation here.
 *
 * Returns: whatever the application function returns (an int).
 *
 * NOTE: The appliction functions may call rt_shootray() recursively.
 * Thus, none of the local variables may be static.
 *
 * An open issue for execution in a PARALLEL environment is locking of
 * the statistics variables.
 */
int
rt_vshootray(struct application *ap)
{
    struct seg *HeadSeg;
    int ret;
    auto vect_t inv_dir;	/* inverses of ap->a_ray.r_dir */
    struct bu_bitv *solidbits;	/* bits for all solids shot so far */
    struct bu_ptbl *regionbits;	/* bits for all involved regions */
    char *status;
    auto struct partition InitialPart;	/* Head of Initial Partitions */
    auto struct partition FinalPart;	/* Head of Final Partitions */
    int nrays = 1;	/* for now */
    int vlen;
    int id;
    int i;
    struct soltab **ary_stp;	/* array of pointers */
    struct xray **ary_rp;	/* array of pointers */
    struct seg *ary_seg;	/* array of structures */
    struct rt_i *rtip;
    int done;

#define BACKING_DIST (-2.0)		/* mm to look behind start point */
    rtip = ap->a_rt_i;
    RT_AP_CHECK(ap);
    if (ap->a_resource == RESOURCE_NULL) {
	ap->a_resource = &rt_uniresource;
	rt_uniresource.re_magic = RESOURCE_MAGIC;
    }
    RT_CK_RESOURCE(ap->a_resource);

    if (RT_G_DEBUG&(DEBUG_ALLRAYS|DEBUG_SHOOT|DEBUG_PARTITION)) {
	bu_log("\n**********mshootray cpu=%d  %d, %d lvl=%d (%s)\n",
	       ap->a_resource->re_cpu,
	       ap->a_x, ap->a_y,
	       ap->a_level,
	       ap->a_purpose != (char *)0 ? ap->a_purpose : "?");
	VPRINT("Pnt", ap->a_ray.r_pt);
	VPRINT("Dir", ap->a_ray.r_dir);
    }

    rtip->rti_nrays++;
    if (rtip->needprep)
	rt_prep(rtip);

    /* Allocate dynamic memory */
    vlen = nrays * rtip->rti_maxsol_by_type;
    ary_stp = (struct soltab **)bu_calloc(vlen, sizeof(struct soltab *),
					  "*ary_stp[]");
    ary_rp = (struct xray **)bu_calloc(vlen, sizeof(struct xray *),
				       "*ary_rp[]");
    ary_seg = (struct seg *)bu_calloc(vlen, sizeof(struct seg),
				      "ary_seg[]");

    /**** for each ray, do this ****/

    InitialPart.pt_forw = InitialPart.pt_back = &InitialPart;
    FinalPart.pt_forw = FinalPart.pt_back = &FinalPart;

    HeadSeg = RT_SEG_NULL;

    solidbits = rt_get_solidbitv(rtip->nsolids, ap->a_resource);
    bu_bitv_clear(solidbits);

    if (BU_LIST_IS_EMPTY(&ap->a_resource->re_region_ptbl)) {
	BU_GETSTRUCT(regionbits, bu_ptbl);
	bu_ptbl_init(regionbits, 7, "rt_shootray() regionbits ptbl");
    } else {
	regionbits = BU_LIST_FIRST(bu_ptbl, &ap->a_resource->re_region_ptbl);
	BU_LIST_DEQUEUE(&regionbits->l);
	BU_CK_PTBL(regionbits);
    }

    /* Compute the inverse of the direction cosines */
    if (!ZERO(ap->a_ray.r_dir[X])) {
	inv_dir[X]=1.0/ap->a_ray.r_dir[X];
    } else {
	inv_dir[X] = INFINITY;
	ap->a_ray.r_dir[X] = 0.0;
    }
    if (!ZERO(ap->a_ray.r_dir[Y])) {
	inv_dir[Y]=1.0/ap->a_ray.r_dir[Y];
    } else {
	inv_dir[Y] = INFINITY;
	ap->a_ray.r_dir[Y] = 0.0;
    }
    if (!ZERO(ap->a_ray.r_dir[Z])) {
	inv_dir[Z]=1.0/ap->a_ray.r_dir[Z];
    } else {
	inv_dir[Z] = INFINITY;
	ap->a_ray.r_dir[Z] = 0.0;
    }

    /*
     * XXX handle infinite solids here, later.
     */

    /*
     * If ray does not enter the model RPP, skip on.
     * If ray ends exactly at the model RPP, trace it.
     */
    if (!rt_in_rpp(&ap->a_ray, inv_dir, rtip->mdl_min, rtip->mdl_max)  ||
	ap->a_ray.r_max < 0.0) {
	rtip->nmiss_model++;
	if (ap->a_miss)
	    ret = ap->a_miss(ap);
	else
	    ret = 0;
	status = "MISS model";
	goto out;
    }

    /* For each type of solid to be shot at, assemble the vectors */
    for (id = 1; id <= ID_MAX_SOLID; id++) {
	register int nsol;

	if ((nsol = rtip->rti_nsol_by_type[id]) <= 0) continue;

	/* For each instance of this solid type */
	for (i = nsol-1; i >= 0; i--) {
	    ary_stp[i] = rtip->rti_sol_by_type[id][i];
	    ary_rp[i] = &(ap->a_ray);	/* XXX, sb [ray] */
	    ary_seg[i].seg_stp = SOLTAB_NULL;
	    BU_LIST_INIT(&ary_seg[i].l);
	}
	/* bounding box check */
	/* bit vector per ray check */
	/* mark elements to be skipped with ary_stp[] = SOLTAB_NULL */
	ap->a_rt_i->nshots += nsol;	/* later: skipped ones */
	if (rt_functab[id].ft_vshot) {
	    rt_functab[id].ft_vshot(ary_stp, ary_rp, ary_seg, nsol, ap);
	} else {
	    vshot_stub(ary_stp, ary_rp, ary_seg, nsol, ap);
	}


	/* set bits for all solids shot at for each ray */

	/* append resulting seg list to input for boolweave */
	for (i = nsol-1; i >= 0; i--) {
	    register struct seg *seg2;

	    if (ary_seg[i].seg_stp == SOLTAB_NULL) {
		/* MISS */
		ap->a_rt_i->nmiss++;
		continue;
	    }
	    ap->a_rt_i->nhits++;

	    /* For now, do it the slow way.  sb [ray] */
	    /* MUST dup it -- all segs have to live till after a_hit() */
	    RT_GET_SEG(seg2, ap->a_resource);
	    *seg2 = ary_seg[i];	/* struct copy */
	    /* rt_boolweave(seg2, &InitialPart, ap); */
	    bu_bomb("FIXME: need to call boolweave here");

	    /* Add seg chain to list of used segs awaiting reclaim */

#if 0
	    /* FIXME: need to use waiting_segs/finished_segs here in
	     * conjunction with rt_boolweave()
	    {
		register struct seg *seg3 = seg2;
		while (seg3->seg_next != RT_SEG_NULL)
		    seg3 = seg3->seg_next;
		seg3->seg_next = HeadSeg;
		HeadSeg = seg2;
	    }
	    */
#endif
	}
    }

    /*
     * Ray has finally left known space.
     */
    if (InitialPart.pt_forw == &InitialPart) {
	if (ap->a_miss)
	    ret = ap->a_miss(ap);
	else
	    ret = 0;
	status = "MISSed all primitives";
	goto freeup;
    }

    /*
     * All intersections of the ray with the model have been computed.
     * Evaluate the boolean trees over each partition.
     */
    done = rt_boolfinal(&InitialPart, &FinalPart, BACKING_DIST, INFINITY, regionbits, ap, solidbits);

    if (done > 0) goto hitit;

    if (FinalPart.pt_forw == &FinalPart) {
	if (ap->a_miss)
	    ret = ap->a_miss(ap);
	else
	    ret = 0;
	status = "MISS bool";
	goto freeup;
    }

    /*
     * Ray/model intersections exist.  Pass the list to the user's
     * a_hit() routine.  Note that only the hit_dist elements of
     * pt_inhit and pt_outhit have been computed yet.  To compute both
     * hit_point and hit_normal, use the
     *
     * RT_HIT_NORMAL(NULL, hitp, stp, rayp, 0);
     *
     * macro.  To compute just hit_point, use
     *
     * VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
     */
hitit:
    if (RT_G_DEBUG&DEBUG_SHOOT) rt_pr_partitions(rtip, &FinalPart, "a_hit()");

    if (ap->a_hit)
	ret = ap->a_hit(ap, &FinalPart, HeadSeg/* &finished_segs */);
    else
	ret = 0;
    status = "HIT";

    /*
     * Processing of this ray is complete.  Free dynamic resources.
     */
freeup:
    {
	register struct partition *pp;

	/* Free up initial partition list */
	for (pp = InitialPart.pt_forw; pp != &InitialPart;) {
	    register struct partition *newpp;
	    newpp = pp;
	    pp = pp->pt_forw;
	    FREE_PT(newpp, ap->a_resource);
	}
	/* Free up final partition list */
	for (pp = FinalPart.pt_forw; pp != &FinalPart;) {
	    register struct partition *newpp;
	    newpp = pp;
	    pp = pp->pt_forw;
	    FREE_PT(newpp, ap->a_resource);
	}
    }
    /* Segs can't be freed until after a_hit() has returned */
    RT_FREE_SEG_LIST(HeadSeg, ap->a_resource);

out:
    bu_free((char *)ary_stp, "*ary_stp[]");
    bu_free((char *)ary_rp, "*ary_rp[]");
    bu_free((char *)ary_seg, "ary_seg[]");

    if (solidbits != NULL) {
	bu_bitv_free(solidbits);
    }
    if (RT_G_DEBUG&(DEBUG_ALLRAYS|DEBUG_SHOOT|DEBUG_PARTITION)) {
	bu_log("----------mshootray cpu=%d  %d, %d lvl=%d (%s) %s ret=%d\n",
	       ap->a_resource->re_cpu,
	       ap->a_x, ap->a_y,
	       ap->a_level,
	       ap->a_purpose != (char *)0 ? ap->a_purpose : "?",
	       status, ret);
    }
    return ret;
}


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

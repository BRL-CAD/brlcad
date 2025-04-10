/*                        B U N D L E . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2025 United States Government as represented by
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

#include "common.h"

#include <string.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"

#include "bn.h"
#include "raytrace.h"

#include "librt_private.h"


/* book-keeping structure so rt_shootrays can keep track of which rays
 * have completed without without semaphore-locking.
 */
struct shootrays_data {
    struct application *ap;
    struct bu_bitv *done;
};


/**
 * Note that the direction vector r_dir must have unit length; this is
 * mandatory, and is not ordinarily checked, in the name of
 * efficiency.
 *
 * Input: Pointer to an application structure, with these mandatory
 * fields:
 *
 *	a_ray.r_pt	Starting point of ray to be fired
 *	a_ray.r_dir	UNIT VECTOR with direction to fire in (dir cosines)
 *	a_hit		Routine to call when something is hit
 *	a_miss		Routine to call when ray misses everything
 *
 * Calls user's a_miss() or a_hit() routine as appropriate.  Passes
 * a_hit() routine list of partitions, with only hit_dist fields
 * valid.  Normal computation deferred to user code, to avoid needless
 * computation here.
 *
 * Formal Return: whatever the application function returns (an int).
 *
 * NOTE: The application functions may call rt_shootray() recursively.
 * Thus, none of the local variables may be static.
 *
 * To prevent having to lock the statistics variables in a PARALLEL
 * environment, all the statistics variables have been moved into the
 * 'resource' structure, which is allocated per-CPU.
 *
 * XXX maybe parameter with NORM, UV, CURVE bits?
 */
int
rt_shootray_bundle(struct application *ap, struct xray *rays, int nrays)
{
    struct rt_shootray_status ss;
    struct seg new_segs;	/* from solid intersections */
    struct seg waiting_segs;	/* awaiting rt_boolweave() */
    struct seg finished_segs;	/* processed by rt_boolweave() */
    fastf_t last_bool_start;
    struct bu_bitv *solidbits;	/* bits for all solids shot so far */
    struct bu_ptbl *regionbits;	/* table of all involved regions */
    const char *status;
    struct partition InitialPart;	/* Head of Initial Partitions */
    struct partition FinalPart;	/* Head of Final Partitions */
    struct soltab **stpp;
    register const union cutter *cutp;
    struct resource *resp;
    struct rt_i *rtip;
    const int debug_shoot = RT_G_DEBUG & RT_DEBUG_SHOOT;

    memset(&ss, 0, sizeof(struct rt_shootray_status));

    RT_AP_CHECK(ap);
    if (ap->a_magic) {
	RT_CK_AP(ap);
    } else {
	ap->a_magic = RT_AP_MAGIC;
    }
    if (ap->a_ray.magic) {
	RT_CK_RAY(&(ap->a_ray));
    } else {
	ap->a_ray.magic = RT_RAY_MAGIC;
    }
    if (!ap->a_resource) {
	ap->a_resource = &rt_uniresource;
    }
    RT_CK_RESOURCE(ap->a_resource);
    ss.ap = ap;
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);
    resp = ap->a_resource;
    RT_CK_RESOURCE(resp);
    ss.resp = resp;

    if (RT_G_DEBUG&(RT_DEBUG_ALLRAYS|RT_DEBUG_SHOOT|RT_DEBUG_PARTITION|RT_DEBUG_ALLHITS)) {
	bu_log_indent_delta(2);
	bu_log("\n**********shootray_bundle cpu=%d  %d, %d lvl=%d (%s)\n",
	       resp->re_cpu,
	       ap->a_x, ap->a_y,
	       ap->a_level,
	       ap->a_purpose != (char *)0 ? ap->a_purpose : "?");
	bu_log("Pnt (%g, %g, %g) a_onehit=%d\n",
	       V3ARGS(ap->a_ray.r_pt),
	       ap->a_onehit);
	VPRINT("Dir", ap->a_ray.r_dir);
    }

    if (rtip->needprep)
	rt_prep(rtip);

    InitialPart.pt_forw = InitialPart.pt_back = &InitialPart;
    InitialPart.pt_magic = PT_HD_MAGIC;
    FinalPart.pt_forw = FinalPart.pt_back = &FinalPart;
    FinalPart.pt_magic = PT_HD_MAGIC;
    ap->a_Final_Part_hdp = &FinalPart;

    BU_LIST_INIT(&new_segs.l);
    BU_LIST_INIT(&waiting_segs.l);
    BU_LIST_INIT(&finished_segs.l);
    ap->a_finished_segs_hdp = &finished_segs;

    if (!BU_LIST_IS_INITIALIZED(&resp->re_parthead)) {
	/* XXX This shouldn't happen any more */
	bu_log("rt_shootray_bundle() resp=%p uninitialized, fixing it\n", (void *)resp);
	/*
	 * We've been handed a mostly un-initialized resource struct,
	 * with only a magic number and a cpu number filled in.
	 * Init it and add it to the table.
	 * This is how application-provided resource structures
	 * are remembered for later cleanup by the library.
	 */
	rt_init_resource(resp, resp->re_cpu, rtip);

	/* Ensure that this CPU's resource structure is registered */
	BU_ASSERT(BU_PTBL_GET(&rtip->rti_resources, resp->re_cpu) != NULL);
    }

    solidbits = rt_get_solidbitv(rtip->nsolids, resp);

    if (BU_LIST_IS_EMPTY(&resp->re_region_ptbl)) {
	BU_ALLOC(regionbits, struct bu_ptbl);
	bu_ptbl_init(regionbits, 7, "rt_shootray_bundle() regionbits ptbl");
    } else {
	regionbits = BU_LIST_FIRST(bu_ptbl, &resp->re_region_ptbl);
	BU_LIST_DEQUEUE(&regionbits->l);
	BU_CK_PTBL(regionbits);
    }

    /* Verify that direction vector has unit length */
    if (RT_G_DEBUG) {
	fastf_t f, diff;
	f = MAGSQ(ap->a_ray.r_dir);
	if (NEAR_ZERO(f, 0.0001)) {
	    bu_bomb("rt_shootray_bundle:  zero length dir vector\n");
	}
	diff = f - 1;
	if (!NEAR_ZERO(diff, 0.0001)) {
	    bu_log("rt_shootray_bundle: non-unit dir vect (x%d y%d lvl%d)\n",
		   ap->a_x, ap->a_y, ap->a_level);
	    f = 1/f;
	    VSCALE(ap->a_ray.r_dir, ap->a_ray.r_dir, f);
	}
    }

    /* Compute the inverse of the direction cosines */
    /* TODO: utilize VINVDIR after getting rid of nugrid */
    if (ap->a_ray.r_dir[X] < -SQRT_SMALL_FASTF) {
	ss.abs_inv_dir[X] = -(ss.inv_dir[X]=1.0/ap->a_ray.r_dir[X]);
	ss.rstep[X] = -1;
    } else if (ap->a_ray.r_dir[X] > SQRT_SMALL_FASTF) {
	ss.abs_inv_dir[X] =  (ss.inv_dir[X]=1.0/ap->a_ray.r_dir[X]);
	ss.rstep[X] = 1;
    } else {
	ap->a_ray.r_dir[X] = 0.0;
	ss.abs_inv_dir[X] = ss.inv_dir[X] = INFINITY;
	ss.rstep[X] = 0;
    }
    if (ap->a_ray.r_dir[Y] < -SQRT_SMALL_FASTF) {
	ss.abs_inv_dir[Y] = -(ss.inv_dir[Y]=1.0/ap->a_ray.r_dir[Y]);
	ss.rstep[Y] = -1;
    } else if (ap->a_ray.r_dir[Y] > SQRT_SMALL_FASTF) {
	ss.abs_inv_dir[Y] =  (ss.inv_dir[Y]=1.0/ap->a_ray.r_dir[Y]);
	ss.rstep[Y] = 1;
    } else {
	ap->a_ray.r_dir[Y] = 0.0;
	ss.abs_inv_dir[Y] = ss.inv_dir[Y] = INFINITY;
	ss.rstep[Y] = 0;
    }
    if (ap->a_ray.r_dir[Z] < -SQRT_SMALL_FASTF) {
	ss.abs_inv_dir[Z] = -(ss.inv_dir[Z]=1.0/ap->a_ray.r_dir[Z]);
	ss.rstep[Z] = -1;
    } else if (ap->a_ray.r_dir[Z] > SQRT_SMALL_FASTF) {
	ss.abs_inv_dir[Z] =  (ss.inv_dir[Z]=1.0/ap->a_ray.r_dir[Z]);
	ss.rstep[Z] = 1;
    } else {
	ap->a_ray.r_dir[Z] = 0.0;
	ss.abs_inv_dir[Z] = ss.inv_dir[Z] = INFINITY;
	ss.rstep[Z] = 0;
    }

    /* If ray does not enter the model RPP, skip on.
     * If ray ends exactly at the model RPP, trace it.
     */
    if (!rt_in_rpp(&ap->a_ray, ss.inv_dir, rtip->mdl_min, rtip->mdl_max)  ||
	ap->a_ray.r_max < 0.0) {
	resp->re_nmiss_model++;
	if (ap->a_miss)
	    ap->a_return = ap->a_miss(ap);
	else
	    ap->a_return = 0;
	status = "MISS model";
	goto out;
    }

    /* The interesting part of the ray starts at distance 0.  If the
     * ray enters the model at a negative distance, (i.e., the ray
     * starts within the model RPP), we only look at little bit behind
     * (BACKING_DIST) to see if we are just coming out of something,
     * but never further back than the intersection with the model
     * RPP.  If the ray enters the model at a positive distance, we
     * always start there.  It is vital that we never pick a start
     * point outside the model RPP, or the space partitioning tree
     * will pick the wrong box and the ray will miss it.
     *
     * BACKING_DIST should probably be determined by floating point
     * noise factor due to model RPP size -vs- number of bits of
     * floating point mantissa significance, rather than a constant,
     * but that is too hideous to think about here.  Also note that
     * applications that really depend on knowing what region they are
     * leaving from should probably back their own start-point up,
     * rather than depending on it here, but it isn't much trouble
     * here.
     */
    ss.box_start = ss.model_start = ap->a_ray.r_min;
    ss.box_end = ss.model_end = ap->a_ray.r_max;

    if (ss.box_start < BACKING_DIST)
	ss.box_start = BACKING_DIST; /* Only look a little bit behind */

    ss.lastcut = CUTTER_NULL;
    ss.old_status = (struct rt_shootray_status *)NULL;
    ss.curcut = &ap->a_rt_i->rti_CutHead;

    if (ss.curcut->cut_type == CUT_CUTNODE || ss.curcut->cut_type == CUT_BOXNODE) {
	ss.lastcell = ss.curcut;
	VMOVE(ss.curmin, rtip->mdl_min);
	VMOVE(ss.curmax, rtip->mdl_max);
    }

    last_bool_start = BACKING_DIST;
    ss.newray = ap->a_ray;		/* struct copy */
    ss.odist_corr = ss.obox_start = ss.obox_end = -99;
    ss.dist_corr = 0.0;

    /*
     * While the ray remains inside model space, push from box to box
     * until ray emerges from model space again (or first hit is
     * found, if user is impatient).  It is vitally important to
     * always stay within the model RPP, or the space partitioning tree
     * will pick wrong boxes & miss them.
     */
    while ((cutp = rt_advance_to_next_cell(&ss)) != CUTTER_NULL) {
	if (debug_shoot) {
	    rt_pr_cut(cutp, 0);
	}

	if (cutp->bn.bn_len <= 0) {
	    /* Push ray onwards to next box */
	    ss.box_start = ss.box_end;
	    resp->re_nempty_cells++;
	    continue;
	}

	/* Consider all objects within the box */
	stpp = &(cutp->bn.bn_list[cutp->bn.bn_len-1]);
	for (; stpp >= cutp->bn.bn_list; stpp--) {
	    register struct soltab *stp = *stpp;
	    int ray;

	    if (BU_BITTEST(solidbits, stp->st_bit)) {
		resp->re_ndup++;
		continue;	/* already shot */
	    }

	    /* Shoot all the rays in the bundle through this solid */
	    /* XXX open issue: entering neighboring cells too? */
	    BU_BITSET(solidbits, stp->st_bit);

	    for (ray=0; ray < nrays; ray++) {
		struct xray ss2_newray;
		int ret;

		/* Be compatible with the ss backing distance stuff */
		VMOVE(ss2_newray.r_dir, rays[ray].r_dir);
		VJOIN1(ss2_newray.r_pt, rays[ray].r_pt, ss.dist_corr, ss2_newray.r_dir);

		/* Check against bounding RPP, if desired by solid */
		if (OBJ[stp->st_id].ft_use_rpp) {
		    if (!rt_in_rpp(&ss2_newray, ss.inv_dir,
				   stp->st_min, stp->st_max)) {
			if (debug_shoot)bu_log("rpp miss %s by ray %d\n", stp->st_name, ray);
			resp->re_prune_solrpp++;
			continue;	/* MISS */
		    }
		    if (ss.dist_corr + ss2_newray.r_max < BACKING_DIST) {
			if (debug_shoot)bu_log("rpp skip %s, dist_corr=%g, r_max=%g, by ray %d\n", stp->st_name, ss.dist_corr, ss2_newray.r_max, ray);
			resp->re_prune_solrpp++;
			continue;	/* MISS */
		    }
		}

		if (debug_shoot)bu_log("shooting %s with ray %d\n", stp->st_name, ray);
		resp->re_shots++;

		BU_LIST_INIT(&(new_segs.l));

		ret = -1;
		if (OBJ[stp->st_id].ft_shot) {
		    ret = OBJ[stp->st_id].ft_shot(stp, &ss2_newray, ap, &new_segs);
		}
		if (ret <= 0) {
		    resp->re_shot_miss++;
		    continue;	/* MISS */
		}

		/* Add seg chain to list awaiting rt_boolweave() */
		{
		    register struct seg *s2;
		    while (BU_LIST_WHILE(s2, seg, &(new_segs.l))) {
			BU_LIST_DEQUEUE(&(s2->l));
			/* Restore to original distance */
			s2->seg_in.hit_dist += ss.dist_corr;
			s2->seg_out.hit_dist += ss.dist_corr;
			s2->seg_in.hit_rayp = s2->seg_out.hit_rayp = &rays[ray];
			BU_LIST_INSERT(&(waiting_segs.l), &(s2->l));
		    }
		}
		resp->re_shot_hit++;
		break;			/* HIT */
	    }
	}
	if (RT_G_DEBUG & RT_DEBUG_ADVANCE)
	    rt_plot_cell(cutp, &ss, &(waiting_segs.l), rtip);

	/*
	 * If a_onehit == 0 and a_ray_length <= 0, then the ray
	 * is traced to +infinity.
	 *
	 * If a_onehit != 0, then it indicates how many hit points
	 * (which are greater than the ray start point of 0.0) the
	 * application requires, i.e., partitions with inhit >= 0.  (If
	 * negative, indicates number of non-air hits needed).
	 *
	 * If this box yielded additional segments, immediately weave
	 * them into the partition list, and perform final boolean
	 * evaluation.
	 *
	 * If this results in the required number of final partitions,
	 * then cease ray-tracing and hand the partitions over to the
	 * application.
	 *
	 * All partitions will have valid in and out distances.
	 * a_ray_length is treated similarly to a_onehit.
	 */
	if (ap->a_onehit != 0 && BU_LIST_NON_EMPTY(&(waiting_segs.l))) {
	    int done;

	    /* Weave these segments into partition list */
	    rt_boolweave(&finished_segs, &waiting_segs, &InitialPart, ap);

	    /* Evaluate regions up to box_end */
	    done = rt_boolfinal(&InitialPart, &FinalPart,
				last_bool_start, ss.box_end, regionbits, ap, solidbits);
	    last_bool_start = ss.box_end;

	    /* See if enough partitions have been acquired */
	    if (done > 0) goto hitit;
	}

	if (ap->a_ray_length > 0.0 && ss.box_end >= ap->a_ray_length)
	    goto weave;

	/* Push ray onwards to next box */
	ss.box_start = ss.box_end;
    }

    /*
     * Ray has finally left known space -- Weave any remaining
     * segments into the partition list.
     */
weave:
    if (RT_G_DEBUG&RT_DEBUG_ADVANCE)
	bu_log("rt_shootray_bundle: ray has left known space\n");

    if (BU_LIST_NON_EMPTY(&(waiting_segs.l))) {
	rt_boolweave(&finished_segs, &waiting_segs, &InitialPart, ap);
    }

    /* finished_segs chain now has all segments hit by this ray */
    if (BU_LIST_IS_EMPTY(&(finished_segs.l))) {
	if (ap->a_miss)
	    ap->a_return = ap->a_miss(ap);
	else
	    ap->a_return = 0;
	status = "MISS primitives";
	goto out;
    }

    /*
     * All intersections of the ray with the model have been computed.
     * Evaluate the boolean trees over each partition.
     */
    (void)rt_boolfinal(&InitialPart, &FinalPart, BACKING_DIST,
		       INFINITY,
		       regionbits, ap, solidbits);

    if (FinalPart.pt_forw == &FinalPart) {
	if (ap->a_miss)
	    ap->a_return = ap->a_miss(ap);
	else
	    ap->a_return = 0;
	status = "MISS bool";
	RT_FREE_PT_LIST(&InitialPart, resp);
	RT_FREE_SEG_LIST(&finished_segs, resp);
	goto out;
    }

    /*
     * Ray/model intersections exist.  Pass the list to the user's
     * a_hit() routine.  Note that only the hit_dist elements of
     * pt_inhit and pt_outhit have been computed yet.  To compute both
     * hit_point and hit_normal, use the
     *
     * RT_HIT_NORMAL(NULL, hitp, stp, rayp, 0)
     *
     * macro.  To compute just hit_point, use
     *
     * VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
     */
hitit:
    if (debug_shoot) rt_pr_partitions(rtip, &FinalPart, "a_hit()");

    /*
     * Before recursing, release storage for unused Initial
     * partitions.  finished_segs can not be released yet, because
     * FinalPart partitions will point to hits in those segments.
     */
    RT_FREE_PT_LIST(&InitialPart, resp);

    /*
     * finished_segs is only used by special hit routines which don't
     * follow the traditional solid modeling paradigm.
     */
    if (RT_G_DEBUG&RT_DEBUG_ALLHITS) rt_pr_partitions(rtip, &FinalPart, "Partition list passed to a_hit() routine");
    if (ap->a_hit)
	ap->a_return = ap->a_hit(ap, &FinalPart, &finished_segs);
    else
	ap->a_return = 0;
    status = "HIT";

    RT_FREE_SEG_LIST(&finished_segs, resp);
    RT_FREE_PT_LIST(&FinalPart, resp);

    /*
     * Processing of this ray is complete.
     */
out:
    /* Return dynamic resources to their freelists.  */
    BU_CK_BITV(solidbits);
    BU_LIST_APPEND(&resp->re_solid_bitv, &solidbits->l);
    BU_CK_PTBL(regionbits);
    BU_LIST_APPEND(&resp->re_region_ptbl, &regionbits->l);

    /*
     * Record essential statistics in per-processor data structure.
     */
    resp->re_nshootray++;

    /* Terminate any logging */
    if (RT_G_DEBUG&(RT_DEBUG_ALLRAYS|RT_DEBUG_SHOOT|RT_DEBUG_PARTITION|RT_DEBUG_ALLHITS)) {
	bu_log_indent_delta(-2);
	bu_log("----------shootray_bundle cpu=%d  %d, %d lvl=%d (%s) %s ret=%d\n",
	       resp->re_cpu,
	       ap->a_x, ap->a_y,
	       ap->a_level,
	       ap->a_purpose != (char *)0 ? ap->a_purpose : "?",
	       status, ap->a_return);
    }
    return ap->a_return;
}


/*
 * 'static' local hit function that simply adds hit partition to a ray
 * bundle structure passed in through ap->a_uptr and updates hit/miss
 * stats.
 */
static int
bundle_hit(register struct application *ap, struct partition *PartHeadp, struct seg *segp)
{
    struct partition_bundle *bundle = (struct partition_bundle *)ap->a_uptr;
    struct partition_list *new_shotline;

    if (PartHeadp->pt_forw == PartHeadp) {
	bundle->misses++;
	return 0;		/* Nothing hit?? */
    }

    bundle->hits++;

    if (bundle->list == NULL) {
	/*
	 * setup partition collection
	 */
	BU_ALLOC(bundle->list, struct partition_list);
	BU_LIST_INIT(&(bundle->list->l));
    }

    /* add a new partition to list */
    BU_ALLOC(new_shotline, struct partition_list);

    /* steal partition list */
    BU_LIST_INIT_MAGIC((struct bu_list *)&new_shotline->PartHeadp, PT_HD_MAGIC);
    BU_LIST_APPEND_LIST((struct bu_list *)&new_shotline->PartHeadp, (struct bu_list *)PartHeadp);

    BU_LIST_INIT_MAGIC(&new_shotline->segHeadp.l, RT_SEG_MAGIC);
    BU_LIST_APPEND_LIST(&new_shotline->segHeadp.l, &segp->l);

    new_shotline->ap = ap;
    BU_LIST_PUSH(&(bundle->list->l), &(new_shotline->l));

    return 1;

}


/*
 * 'static' local hit function that simply miss stats for bundled rays.
 */
static int
bundle_miss(register struct application *ap)
{
    struct partition_bundle *bundle = (struct partition_bundle *)ap->a_uptr;

    bundle->misses++;

    return 0;
}


#ifdef SHOOTRAYS_IN_PARALLEL
static void
shootrays_in_parallel(int UNUSED(cpu), void *data)
{
    size_t i;
    struct shootrays_data *rays = (struct shootrays_data *)data;

    RT_CK_APPLICATION(&rays->ap[0]);
    BU_CK_BITV(rays->done);

    i = 0;
    while (rays->ap[i].a_magic != RT_AP_MAGIC) {
	bu_semaphore_acquire(RT_SEM_WORKER);
	if (BU_BITTEST(rays->done, i)) {
	    BU_BITSET(rays->done, i);
	    bu_semaphore_release(RT_SEM_WORKER);

	    rt_shootray(&rays->ap[i]);

	} else {
	    bu_semaphore_release(RT_SEM_WORKER);
	}
	i++;
    }

    return;
}
#endif

int
rt_shootrays(struct application_bundle *bundle)
{
    struct partition_bundle *pb = NULL;
    void *a_uptr_backup = NULL;
    struct xray a_ray;
    int (*a_hit)(struct application *, struct partition *, struct seg *);
    int (*a_miss)(struct application *);

    struct rt_i * rt_i = bundle->b_ap.a_rt_i;		/**< @brief this librt instance */
    struct resource * resource = bundle->b_ap.a_resource;	/**< @brief dynamic memory resources */
    struct xrays *r;
    struct partition_list *pl;

    size_t nrays = 0;
    struct application *ray_aps = NULL;

/* #define SHOOTRAYS_IN_PARALLEL 1 */

#ifdef SHOOTRAYS_IN_PARALLEL
    struct shootrays_data rays = {NULL, NULL};
#endif

    /*
     * temporarily hijack ap->a_uptr, ap->a_ray, ap->a_hit(), ap->a_miss()
     */
    a_uptr_backup = bundle->b_ap.a_uptr;
    a_ray = bundle->b_ap.a_ray;
    a_hit = bundle->b_ap.a_hit;
    a_miss = bundle->b_ap.a_miss;

    /*
     * Along with updating some bundle hit/miss counters the default
     * callback functions differ from their general user defined
     * counterparts by detaching the ray hit partition list and
     * segment list and attaching it to a partition bundle. Users can
     * define their own functions but should remember to hi-jack the
     * partition and segment list or the single ray handling function
     * will return memory allocated to these list prior to the bundle
     * b_hit() routine.
     */

    /* Shoot bundle of Rays */
    bundle->b_ap.a_purpose = "bundled ray";
    if (!bundle->b_ap.a_hit)
	bundle->b_ap.a_hit = bundle_hit;
    if (!bundle->b_ap.a_miss)
	bundle->b_ap.a_miss = bundle_miss;

    BU_ALLOC(pb, struct partition_bundle);
    pb->ap = &bundle->b_ap;
    pb->hits = pb->misses = 0;

    bundle->b_uptr = (void *)pb;

    /* PASS1: count up how many rays we have */
    nrays = 0;
    for (BU_LIST_FOR (r, xrays, &bundle->b_rays.l)) {
	nrays++;
    }

    /* +1 to 0-terminate the array */
    ray_aps = (struct application*)bu_calloc(nrays+1, sizeof(struct application), "app rays");

    /* PASS2: fill in our AoS */
    nrays = 0;
    for (BU_LIST_FOR (r, xrays, &bundle->b_rays.l)) {
	struct application *ray_ap = &ray_aps[nrays];

	*ray_ap = bundle->b_ap; /* structure copy */
	ray_ap->a_ray = r->ray; /* structure copy */

	ray_ap->a_ray.magic = RT_RAY_MAGIC;
	ray_ap->a_uptr = (void *)pb;
	ray_ap->a_rt_i = rt_i;
	ray_ap->a_resource = resource;

	nrays++;
    }

    /* PASS3: shoot our rays */
#ifndef SHOOTRAYS_IN_PARALLEL
    nrays = 0;
    for (BU_LIST_FOR (r, xrays, &bundle->b_rays.l)) {
	rt_shootray(&ray_aps[nrays]);
	nrays++;
    }
#else
    rays.ap = ray_aps;
    rays.done = bu_bitv_new(nrays);
    bu_parallel(&shootrays_in_parallel, 0, &rays);
#endif


    if ((bundle->b_hit) && (pb->hits > 0)) {
	/* "HIT" */
	bundle->b_return = bundle->b_hit(bundle, pb);
    } else if (bundle->b_miss) {
	/* "MISS" */
	bundle->b_return = bundle->b_miss(bundle);
    } else {
	/* "MISS (unexpected)" */
	bundle->b_return = 0;
    }

    if (pb->list != NULL) {
	while (BU_LIST_WHILE(pl, partition_list, &(pb->list->l))) {
	    BU_LIST_DEQUEUE(&(pl->l));
	    RT_FREE_SEG_LIST(&pl->segHeadp, resource);
	    RT_FREE_PT_LIST(&pl->PartHeadp, resource);
	    bu_free(pl, "free partition_list pl");
	}
	bu_free(pb->list, "free partition_list header");
    }
    bu_free(pb, "partition bundle");
    /* Free all the pl->ap ray application structures - don't do it
     * as part of the while loop above or we end up with a double
     * free error. */
    bu_free(ray_aps, "app rays");

    /*
     * set back to original values before exiting
     */
    bundle->b_ap.a_uptr = a_uptr_backup;
    bundle->b_ap.a_ray = a_ray;
    bundle->b_ap.a_hit = a_hit;
    bundle->b_ap.a_miss = a_miss;

    return bundle->b_return;
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

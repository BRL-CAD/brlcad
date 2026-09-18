/*                        V S H O O T . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2026 United States Government as represented by
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
 * rt_vshootray() is a drop-in alternative to rt_shootray().  Rather than
 * walking the space partitioning and shooting solids one at a time, it
 * shoots the ray against ALL solids of each type in a single batched
 * ft_vshot() call (falling back to a per-ray stub for types that lack a
 * vshot), then weaves the resulting segments through the normal
 * rt_boolweave()/rt_boolfinal() pipeline and invokes a_hit()/a_miss()
 * just like rt_shootray().  It exists to exercise and benchmark the
 * ft_vshot() callbacks from userland.
 *
 * Limitations (it is EXPERIMENTAL):
 *  - No per-solid spatial culling: every solid of every type present is
 *    intersected on every ray that enters the model RPP.  This is a win
 *    only when ft_vshot() is faster than N scalar shots and the scene is
 *    dominated by solids of a few vectorized types.
 *  - ft_vshot() returns a single (outer-span) segment per solid, so
 *    non-convex solids that scalar-shot as multiple segments are
 *    approximated by their outer span (the rt_tor_vshot() convention).
 *  - Infinite solids are not special-cased: a ray that misses the model
 *    RPP is treated as a miss.
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "vmath.h"
#include "raytrace.h"
#include "librt_private.h"


#define BACKING_DIST (-2.0)		/* mm to look behind start point */


/**
 * When the RT_VSHOOT_SCALAR environment variable is set, rt_vshootray()
 * forces the per-ray scalar stub even for solid types that have an
 * ft_vshot().  This makes it possible to compare, within the exact same
 * (cull-free, batched) coordinator, the vectorized callbacks against the
 * scalar shot -- isolating the ft_vshot() speedup from the coordinator's
 * own overhead.  Checked once and cached.
 */
static int
vshoot_force_scalar(void)
{
    static int checked = 0;
    static int val = 0;
    if (!checked) {
	val = (getenv("RT_VSHOOT_SCALAR") != NULL);
	checked = 1;
    }
    return val;
}


/**
 * Stub used for solid types that have no ft_vshot(): emulate a vector
 * shot by calling the scalar ft_shot() once per solid, writing the first
 * returned segment into the caller's flat segp[] array.
 */
static void
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
	    if (OBJ[stp[i]->st_id].ft_shot) {
		ret = OBJ[stp[i]->st_id].ft_shot(stp[i], rp[i], ap, &seghead);
	    }
	    if (ret <= 0) {
		segp[i].seg_stp=(struct soltab *) 0;
	    } else {
		tmp_seg = BU_LIST_FIRST(seg, &(seghead.l));
		BU_LIST_DEQUEUE(&(tmp_seg->l));
		segp[i] = *tmp_seg; /* structure copy */
		RT_FREE_SEG(tmp_seg, ap->a_resource);
		/* discard any remaining (interior) segments */
		while (BU_LIST_WHILE(tmp_seg, seg, &(seghead.l))) {
		    BU_LIST_DEQUEUE(&(tmp_seg->l));
		    RT_FREE_SEG(tmp_seg, ap->a_resource);
		}
	    }
	}
    }
}


/**
 * Generic flat-array ft_vshot() built on a scalar ft_shot().  See the
 * declaration in librt_private.h.  This is the baseline (parity) vshot
 * for primitives whose intersection core is not vectorized: it simply
 * runs the scalar shot per ray and collapses the returned seg list to
 * its outer span.  It captures the flat-output convention but NOT any
 * batching win (the scalar shot still allocates its own segs), so it is
 * expected to be ~1.0x -- a correct starting point for later
 * optimization.
 */
void
rt_vshot_via_shot(int (*shotfn)(struct soltab *, struct xray *, struct application *, struct seg *),
		  struct soltab **stp, struct xray **rp, struct seg *segp, int n,
		  struct application *ap)
{
    struct resource *resp;
    int i;

    resp = (ap && ap->a_resource) ? ap->a_resource : &rt_uniresource;

    for (i = 0; i < n; i++) {
	struct seg seghd;
	struct seg *s;
	fastf_t mn, mx;

	if (stp[i] == 0)
	    continue;			/* skip this ray */
	segp[i].seg_stp = (struct soltab *)0;	/* assume MISS */

	BU_LIST_INIT(&seghd.l);
	if (!shotfn || shotfn(stp[i], rp[i], ap, &seghd) <= 0)
	    continue;
	if (BU_LIST_IS_EMPTY(&seghd.l))
	    continue;

	mn = INFINITY;
	mx = -INFINITY;
	for (BU_LIST_FOR(s, seg, &seghd.l)) {
	    if (s->seg_in.hit_dist < mn) {
		mn = s->seg_in.hit_dist;
		segp[i].seg_in = s->seg_in;	/* struct copy */
	    }
	    if (s->seg_out.hit_dist > mx) {
		mx = s->seg_out.hit_dist;
		segp[i].seg_out = s->seg_out;	/* struct copy */
	    }
	}
	segp[i].seg_stp = stp[i];

	while (BU_LIST_WHILE(s, seg, &seghd.l)) {
	    BU_LIST_DEQUEUE(&s->l);
	    RT_FREE_SEG(s, resp);
	}
    }
}


/**
 * EXPERIMENTAL vectorized counterpart to rt_shootray().
 *
 * See the file comment for the model and its limitations.  The control
 * flow (resource/bitv/ptbl acquisition, model-RPP reject, boolweave,
 * boolfinal, a_hit/a_miss dispatch, and freelist return) mirrors
 * rt_shootray() so that, for scenes of convex solids, the produced image
 * is identical -- only the per-solid intersection is routed through
 * ft_vshot() in batches.
 *
 * Returns whatever the application's a_hit()/a_miss() returns.
 */
int
rt_vshootray(struct application *ap)
{
    struct seg waiting_segs;		/* awaiting rt_boolweave() */
    struct seg finished_segs;		/* woven by rt_boolweave() */
    struct partition InitialPart;	/* head of Initial Partitions */
    struct partition FinalPart;		/* head of Final Partitions */
    struct bu_bitv *solidbits;		/* bits for all solids shot */
    struct bu_ptbl *regionbits;		/* bits for all involved regions */
    struct resource *resp;
    struct rt_i *rtip;
    const char *status = "";
    vect_t inv_dir;			/* inverses of ap->a_ray.r_dir */
    int id, i;
    struct soltab **ary_stp = NULL;	/* scratch: solids of one type */
    struct xray **ary_rp = NULL;	/* scratch: all == &ap->a_ray */
    struct seg *ary_seg = NULL;		/* scratch: flat vshot results */
    int vlen;

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

    rtip = ap->a_rt_i;
    if (!rtip)
	return 0;
    if (!ap->a_resource)
	ap->a_resource = &rt_uniresource;
    resp = ap->a_resource;
    RT_CK_RESOURCE(resp);

    if (rtip->needprep)
	rt_prep(rtip);

    /* Initialize partition and segment lists. */
    InitialPart.pt_forw = InitialPart.pt_back = &InitialPart;
    InitialPart.pt_magic = PT_HD_MAGIC;
    FinalPart.pt_forw = FinalPart.pt_back = &FinalPart;
    FinalPart.pt_magic = PT_HD_MAGIC;
    ap->a_Final_Part_hdp = &FinalPart;
    BU_LIST_INIT(&waiting_segs.l);
    BU_LIST_INIT(&finished_segs.l);
    ap->a_finished_segs_hdp = &finished_segs;

    if (!BU_LIST_IS_INITIALIZED(&resp->re_parthead))
	rt_init_resource(resp, resp->re_cpu, rtip);

    resp->re_nshootray++;

    solidbits = rt_get_solidbitv(rtip->stats.nsolids, resp);

    if (BU_LIST_IS_EMPTY(&resp->re_region_ptbl)) {
	BU_ALLOC(regionbits, struct bu_ptbl);
	bu_ptbl_init(regionbits, 7, "rt_vshootray() regionbits ptbl");
    } else {
	regionbits = BU_LIST_FIRST(bu_ptbl, &resp->re_region_ptbl);
	BU_LIST_DEQUEUE(&regionbits->l);
	BU_CK_PTBL(regionbits);
    }

    /* Compute the inverse of the direction cosines. */
    if (!ZERO(ap->a_ray.r_dir[X])) {
	inv_dir[X] = 1.0/ap->a_ray.r_dir[X];
    } else {
	inv_dir[X] = INFINITY;
	ap->a_ray.r_dir[X] = 0.0;
    }
    if (!ZERO(ap->a_ray.r_dir[Y])) {
	inv_dir[Y] = 1.0/ap->a_ray.r_dir[Y];
    } else {
	inv_dir[Y] = INFINITY;
	ap->a_ray.r_dir[Y] = 0.0;
    }
    if (!ZERO(ap->a_ray.r_dir[Z])) {
	inv_dir[Z] = 1.0/ap->a_ray.r_dir[Z];
    } else {
	inv_dir[Z] = INFINITY;
	ap->a_ray.r_dir[Z] = 0.0;
    }
    VMOVE(ap->a_inv_dir, inv_dir);

    /*
     * If the ray does not enter the model RPP, it is a miss.  (Infinite
     * solids are intentionally not handled by this experimental path.)
     */
    if (!rt_in_rpp(&ap->a_ray, inv_dir, rtip->mdl_min, rtip->mdl_max) ||
	ap->a_ray.r_max < 0.0) {
	resp->re_nmiss_model++;
	ap->a_return = ap->a_miss ? ap->a_miss(ap) : 0;
	status = "MISS model";
	goto out;
    }

    /* Scratch arrays sized to the largest per-type instance count. */
    vlen = (int)rtip->i->rti_maxsol_by_type;
    if (vlen < 1)
	vlen = 1;
    ary_stp = (struct soltab **)bu_calloc(vlen, sizeof(struct soltab *), "vshoot ary_stp[]");
    ary_rp = (struct xray **)bu_calloc(vlen, sizeof(struct xray *), "vshoot ary_rp[]");
    ary_seg = (struct seg *)bu_calloc(vlen, sizeof(struct seg), "vshoot ary_seg[]");

    /* For each solid type present, batch-shoot all instances. */
    for (id = 1; id <= ID_MAX_SOLID; id++) {
	int nsol = (int)rtip->i->rti_nsol_by_type[id];
	int use_stub = 0;
	if (nsol <= 0)
	    continue;
	if (!OBJ[id].ft_vshot && !OBJ[id].ft_shot)
	    continue;

	for (i = 0; i < nsol; i++) {
	    struct soltab *stp = rtip->i->rti_sol_by_type[id][i];
	    ary_stp[i] = stp;
	    if (stp->st_nu_inv_matp)
		use_stub = 1;
	    ary_rp[i] = &ap->a_ray;
	    ary_seg[i].seg_stp = SOLTAB_NULL;
	    BU_BITSET(solidbits, stp->st_bit);	/* mark as shot */
	}

	resp->re_shots += nsol;

	if (!use_stub && OBJ[id].ft_vshot && !vshoot_force_scalar()) {
	    OBJ[id].ft_vshot(ary_stp, ary_rp, ary_seg, nsol, ap);
	} else {
	    vshot_stub(ary_stp, ary_rp, ary_seg, nsol, ap);
	}

	/* Promote each hit to a real seg on the waiting list. */
	for (i = 0; i < nsol; i++) {
	    struct seg *segp;
	    if (ary_seg[i].seg_stp == SOLTAB_NULL) {
		resp->re_shot_miss++;
		continue;
	    }
	    resp->re_shot_hit++;
	    RT_GET_SEG(segp, resp);
	    segp->seg_stp = ary_seg[i].seg_stp;
	    segp->seg_in = ary_seg[i].seg_in;		/* struct copy */
	    segp->seg_out = ary_seg[i].seg_out;		/* struct copy */
	    segp->seg_in.hit_magic = RT_HIT_MAGIC;
	    segp->seg_out.hit_magic = RT_HIT_MAGIC;
	    segp->seg_in.hit_rayp = segp->seg_out.hit_rayp = &ap->a_ray;
	    BU_LIST_INSERT(&(waiting_segs.l), &(segp->l));
	}
    }

    bu_free((char *)ary_stp, "vshoot ary_stp[]");
    bu_free((char *)ary_rp, "vshoot ary_rp[]");
    bu_free((char *)ary_seg, "vshoot ary_seg[]");
    ary_stp = NULL;
    ary_rp = NULL;
    ary_seg = NULL;

    /* Weave the segments into the partition list. */
    if (BU_LIST_NON_EMPTY(&(waiting_segs.l)))
	rt_boolweave(&finished_segs, &waiting_segs, &InitialPart, ap);

    if (BU_LIST_IS_EMPTY(&(finished_segs.l))) {
	ap->a_return = ap->a_miss ? ap->a_miss(ap) : 0;
	status = "MISS primitives";
	RT_FREE_PT_LIST(&InitialPart, resp);
	goto out;
    }

    /* Evaluate the boolean trees over each partition. */
    (void)rt_boolfinal(&InitialPart, &FinalPart, BACKING_DIST, INFINITY,
		       regionbits, ap, solidbits);

    RT_FREE_PT_LIST(&InitialPart, resp);

    if (FinalPart.pt_forw == &FinalPart) {
	ap->a_return = ap->a_miss ? ap->a_miss(ap) : 0;
	status = "MISS bool";
	RT_FREE_SEG_LIST(&finished_segs, resp);
	goto out;
    }

    /* Ray/model intersections exist: hand the partitions to the app. */
    if (ap->a_hit) {
	ap->a_return = ap->a_hit(ap, &FinalPart, &finished_segs);
	status = "HIT";
    } else {
	ap->a_return = 0;
	status = "MISS (no a_hit)";
    }

    RT_FREE_SEG_LIST(&finished_segs, resp);
    RT_FREE_PT_LIST(&FinalPart, resp);

out:
    if (ary_stp) bu_free((char *)ary_stp, "vshoot ary_stp[]");
    if (ary_rp) bu_free((char *)ary_rp, "vshoot ary_rp[]");
    if (ary_seg) bu_free((char *)ary_seg, "vshoot ary_seg[]");

    /* Return dynamic resources to their freelists. */
    BU_CK_BITV(solidbits);
    BU_LIST_APPEND(&resp->re_solid_bitv, &solidbits->l);
    BU_CK_PTBL(regionbits);
    BU_LIST_APPEND(&resp->re_region_ptbl, &regionbits->l);

    if (RT_G_DEBUG&(RT_DEBUG_ALLRAYS|RT_DEBUG_SHOOT|RT_DEBUG_PARTITION)) {
	bu_log("----------vshootray cpu=%d  %d, %d lvl=%d (%s) %s ret=%d\n",
	       resp->re_cpu, ap->a_x, ap->a_y, ap->a_level,
	       ap->a_purpose != (char *)0 ? ap->a_purpose : "?",
	       status, ap->a_return);
    }
    return ap->a_return;
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

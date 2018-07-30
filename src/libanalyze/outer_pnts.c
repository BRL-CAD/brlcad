/*                    O U T E R _ P N T S . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2018 United States Government as represented by
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
/** @file outer_pnts.c
 *
 * Brief description
 *
 */
#include "common.h"

#include <string.h> /* for memset */

#include "vmath.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "bn/mat.h"
#include "bu/time.h"
#include "raytrace.h"
#include "analyze.h"
#include "./analyze_private.h"

struct outer_pnts_container {
    struct bu_ptbl *pnset;
};

HIDDEN void
_tgc_hack_fix(struct partition *part, struct soltab *stp) {
    /* hack fix for bad tgc surfaces - avoids a logging crash, which is probably something else altogether... */
    if (bu_strncmp("rec", stp->st_meth->ft_label, 3) == 0 || bu_strncmp("tgc", stp->st_meth->ft_label, 3) == 0) {

	/* correct invalid surface number */
	if (part->pt_inhit->hit_surfno < 1 || part->pt_inhit->hit_surfno > 3) {
	    part->pt_inhit->hit_surfno = 2;
	}
	if (part->pt_outhit->hit_surfno < 1 || part->pt_outhit->hit_surfno > 3) {
	    part->pt_outhit->hit_surfno = 2;
	}
    }
}

HIDDEN int
outer_pnts_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    struct pnt_normal *in_pt, *out_pt;
    struct partition *in_part = PartHeadp->pt_forw;
    struct partition *out_part = PartHeadp->pt_back;
    struct soltab *stp = in_part->pt_inseg->seg_stp;
    struct soltab *ostp = out_part->pt_inseg->seg_stp;
    struct rt_gen_worker_vars *s = (struct rt_gen_worker_vars *)(ap->a_uptr);
    struct outer_pnts_container *state = (struct outer_pnts_container *)(s->ptr);

    _tgc_hack_fix(in_part, stp);
    _tgc_hack_fix(out_part, ostp);

    BU_ALLOC(in_pt, struct pnt_normal);
    VJOIN1(in_pt->v, ap->a_ray.r_pt, in_part->pt_inhit->hit_dist, ap->a_ray.r_dir);
    RT_HIT_NORMAL(in_pt->n, in_part->pt_inhit, stp, &(app->a_ray), in_part->pt_inflip);
    bu_ptbl_ins(state->pnset, (long *)in_pt);

    /* add "out" hit point info (unless half-space) */
    if (bu_strncmp("half", ostp->st_meth->ft_label, 4) != 0) {
	BU_ALLOC(out_pt, struct pnt_normal);
	VJOIN1(out_pt->v, ap->a_ray.r_pt, out_part->pt_outhit->hit_dist, ap->a_ray.r_dir);
	RT_HIT_NORMAL(out_pt->n, out_part->pt_outhit, ostp, &(app->a_ray), out_part->pt_outflip);
	bu_ptbl_ins(state->pnset, (long *)out_pt);
    }

    return 0;
}

HIDDEN int
outer_pnts_overlap(struct application *ap, struct partition *UNUSED(pp),
		struct region *UNUSED(reg1), struct region *UNUSED(reg2),
		struct partition *UNUSED(hp))
{
    RT_CK_APPLICATION(ap);
    return 0;
}


HIDDEN int
outer_pnts_miss(struct application *ap)
{
    RT_CK_APPLICATION(ap);
    return 0;
}

/* 0 = success, -1 error */
int
analyze_outer_pnts(struct rt_pnts_internal *rpnts, struct db_i *dbip,
       const char *obj, struct bn_tol *tol)
{
    int pntcnt = 0;
    int ret, i, j;
    fastf_t oldtime, currtime;
    int ind = 0;
    int count = 0;
    struct rt_i *rtip;
    int ncpus = bu_avail_cpus();
    fastf_t *rays;
    struct rt_gen_worker_vars *state = (struct rt_gen_worker_vars *)bu_calloc(ncpus+1, sizeof(struct rt_gen_worker_vars ), "state");
    struct outer_pnts_container *local_state = (struct outer_pnts_container *)bu_calloc(ncpus+1, sizeof(struct outer_pnts_container), "local state");
    struct resource *resp = (struct resource *)bu_calloc(ncpus+1, sizeof(struct resource), "resources");

    if (!rpnts || !dbip || !obj || !tol || ncpus == 0) {
	ret = 0;
	goto memfree;
    }

    oldtime = bu_gettime();

    rtip = rt_new_rti(dbip);

    for (i = 0; i < ncpus+1; i++) {
	/* standard */
	state[i].rtip = rtip;
	state[i].fhit = outer_pnts_hit;
	state[i].fmiss = outer_pnts_miss;
	state[i].foverlap = outer_pnts_overlap;
	state[i].resp = &resp[i];
	state[i].ind_src = &ind;
	rt_init_resource(state[i].resp, i, rtip);
	/* local */
	BU_GET(local_state[i].pnset, struct bu_ptbl);
	bu_ptbl_init(local_state[i].pnset, 64, "first and last hit points");
	state[i].ptr = (void *)&(local_state[i]);
    }
    if (rt_gettree(rtip, obj) < 0) return -1;

    rt_prep_parallel(rtip, ncpus);

    currtime = bu_gettime();
    bu_log("prep time: %.1f\n", (currtime - oldtime)/1e6);

    count = analyze_get_bbox_rays(&rays, rtip->mdl_min, rtip->mdl_max, tol);
    for (i = 0; i < ncpus+1; i++) {
	state[i].step = (int)(count/ncpus * 0.1);
    }
/*
    bu_log("ray cnt: %d\n", count);
*/
    ret = 0;
    for (i = 0; i < ncpus+1; i++) {
	state[i].rays_cnt = count;
	state[i].rays = rays;
    }

    oldtime = bu_gettime();
    bu_parallel(analyze_gen_worker, ncpus, (void *)state);

    /* Collect and print all of the results */

    for (i = 0; i < ncpus+1; i++) {
	pntcnt += (int)BU_PTBL_LEN(local_state[i].pnset);
    }

    if (pntcnt < 1) {
	ret = ANALYZE_ERROR;
    } else {
	rpnts->count = pntcnt;
	BU_ALLOC(rpnts->point, struct pnt_normal);
	BU_LIST_INIT(&(((struct pnt_normal *)rpnts->point)->l));
	for (i = 0; i < ncpus+1; i++) {
	    for (j = 0; j < (int)BU_PTBL_LEN(local_state[i].pnset); j++) {
		BU_LIST_PUSH(&(((struct pnt_normal *)rpnts->point)->l), &((struct pnt_normal *)BU_PTBL_GET(local_state[i].pnset, j))->l);
	    }
	}
	ret = 0;
    }

memfree:
    /* Free memory not stored in tables */
    for (i = 0; i < ncpus+1; i++) {
	if (local_state[i].pnset != NULL) {
	    bu_ptbl_free(local_state[i].pnset);
	    BU_PUT(local_state[i].pnset, struct bu_ptbl);
	}
    }
    bu_free(state, "free state containers");
    bu_free(local_state, "free state containers");
    bu_free(resp, "free resources");
    return ret;
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

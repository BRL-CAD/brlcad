/*                    O B J _ T O  _ P N T S . C
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
/** @file obj_to_pnts.c
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
    struct bu_ptbl *pnset = (struct bu_ptbl *)(s->ptr);

    RT_CK_APPLICATION(ap);

    _tgc_hack_fix(in_part, stp);
    _tgc_hack_fix(out_part, ostp);

    BU_ALLOC(in_pt, struct pnt_normal);
    VJOIN1(in_pt->v, ap->a_ray.r_pt, in_part->pt_inhit->hit_dist, ap->a_ray.r_dir);
    RT_HIT_NORMAL(in_pt->n, in_part->pt_inhit, stp, &(app->a_ray), in_part->pt_inflip);
    bu_ptbl_ins(pnset, (long *)in_pt);

    /* add "out" hit point info (unless half-space) */
    if (bu_strncmp("half", ostp->st_meth->ft_label, 4) != 0) {
	BU_ALLOC(out_pt, struct pnt_normal);
	VJOIN1(out_pt->v, ap->a_ray.r_pt, out_part->pt_outhit->hit_dist, ap->a_ray.r_dir);
	RT_HIT_NORMAL(out_pt->n, out_part->pt_outhit, ostp, &(app->a_ray), out_part->pt_outflip);
	bu_ptbl_ins(pnset, (long *)out_pt);
    }

    return 0;
}

HIDDEN int
all_pnts_hit(struct application *app, struct partition *partH, struct seg *UNUSED(segs))
{
    struct pnt_normal *pt;
    struct partition *pp;
    struct soltab *stp;
    struct rt_gen_worker_vars *s = (struct rt_gen_worker_vars *)(app->a_uptr);
    struct bu_ptbl *pnset = (struct bu_ptbl *)(s->ptr);

    RT_CK_APPLICATION(app);

    /* add all hit points */
    for (pp = partH->pt_forw; pp != partH; pp = pp->pt_forw) {

	/* always add in hit */
	stp = pp->pt_inseg->seg_stp;
	_tgc_hack_fix(pp, stp);
	BU_ALLOC(pt, struct pnt_normal);
	VJOIN1(pt->v, app->a_ray.r_pt, pp->pt_inhit->hit_dist, app->a_ray.r_dir);
	RT_HIT_NORMAL(pt->n, pp->pt_inhit, stp, &(app->a_ray), pp->pt_inflip);
	bu_ptbl_ins(pnset, (long *)pt);

	/* add "out" hit point unless it's a half-space */
	if (bu_strncmp("half", stp->st_meth->ft_label, 4) != 0) {
	    BU_ALLOC(pt, struct pnt_normal);
	    VJOIN1(pt->v, app->a_ray.r_pt, pp->pt_outhit->hit_dist, app->a_ray.r_dir);
	    RT_HIT_NORMAL(pt->n, pp->pt_outhit, stp, &(app->a_ray), pp->pt_outflip);
	    bu_ptbl_ins(pnset, (long *)pt);
	}
    }

    return 0;
}

HIDDEN int
op_overlap(struct application *ap, struct partition *UNUSED(pp),
		struct region *UNUSED(reg1), struct region *UNUSED(reg2),
		struct partition *UNUSED(hp))
{
    RT_CK_APPLICATION(ap);
    return 0;
}


HIDDEN int
op_miss(struct application *ap)
{
    RT_CK_APPLICATION(ap);
    return 0;
}

void
analyze_prand_pnt_worker(int cpu, void *ptr)
{
    struct application ap;
    struct rt_gen_worker_vars *state = &(((struct rt_gen_worker_vars *)ptr)[cpu]);
    int i;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = state->rtip;
    ap.a_hit = state->fhit;
    ap.a_miss = state->fmiss;
    ap.a_overlap = state->foverlap;
    ap.a_onehit = 0;
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.a_resource = state->resp;
    ap.a_uptr = (void *)state;

    for (i = 0; i < state->rays_cnt; i++) {
	VSET(ap.a_ray.r_pt, state->rays[6*i+0], state->rays[6*i+1], state->rays[6*i+2]);
	VSET(ap.a_ray.r_dir, state->rays[6*i+3], state->rays[6*i+4], state->rays[6*i+5]);
	rt_shootray(&ap);
    }
}

void
get_random_rays(fastf_t *rays, long int craynum)
{
    long int i = 0;
    if (!rays) return;
    for (i = 0; i < craynum; i++) {
	rays[i*6+0] = 0.0;
	rays[i*6+1] = 0.0;
	rays[i*6+2] = 0.0;
	rays[i*6+3] = 0.0;
	rays[i*6+4] = 0.0;
	rays[i*6+5] = 0.0;
    }
}

void
get_sobol_rays(fastf_t *rays, long int craynum)
{
    long int i = 0;
    if (!rays) return;
    for (i = 0; i < craynum; i++) {
	rays[i*6+0] = 0.0;
	rays[i*6+1] = 0.0;
	rays[i*6+2] = 0.0;
	rays[i*6+3] = 0.0;
	rays[i*6+4] = 0.0;
	rays[i*6+5] = 0.0;
    }
}

/* 0 = success, -1 error */
int
analyze_obj_to_pnts(struct rt_pnts_internal *rpnts, struct db_i *dbip,
       const char *obj, struct bn_tol *tol, int flags, long int max_ray_cnt, unsigned int max_time)
{
    int pntcnt = 0;
    int ret, i, j;
    int do_grid = 1;
    fastf_t oldtime, currtime;
    int ind = 0;
    int count = 0;
    struct rt_i *rtip;
    long int mrc;
    int ncpus = bu_avail_cpus();
    struct rt_gen_worker_vars *state = (struct rt_gen_worker_vars *)bu_calloc(ncpus+1, sizeof(struct rt_gen_worker_vars ), "state");
    struct bu_ptbl **output_pnts = (struct bu_ptbl **)bu_calloc(ncpus+1, sizeof(struct bu_ptbl *), "local state");
    struct resource *resp = (struct resource *)bu_calloc(ncpus+1, sizeof(struct resource), "resources");
    long int raycntseed = (max_ray_cnt < 10000000) ? max_ray_cnt : 10000000;
    long int craynum = raycntseed/(ncpus+1);

    if (!rpnts || !dbip || !obj || !tol || ncpus == 0) {
	ret = 0;
	goto memfree;
    }

    /* Only skip the grid if it's 1) not explicitly on and 2) we have another method */
    if (!(flags & ANALYZE_OBJ_TO_PNTS_GRID)) {
	if ((flags & ANALYZE_OBJ_TO_PNTS_RAND) || (flags & ANALYZE_OBJ_TO_PNTS_SOBOL)) {
	    do_grid = 0;
	}
    }

    oldtime = bu_gettime();

    rtip = rt_new_rti(dbip);

    for (i = 0; i < ncpus+1; i++) {
	/* standard */
	state[i].rtip = rtip;
	if (flags & ANALYZE_OBJ_TO_PNTS_SURF) {
	    state[i].fhit = outer_pnts_hit;
	} else {
	    state[i].fhit = all_pnts_hit;
	}
	state[i].fmiss = op_miss;
	state[i].foverlap = op_overlap;
	state[i].resp = &resp[i];
	state[i].ind_src = &ind;
	rt_init_resource(state[i].resp, i, rtip);
	/* per-cpu hit point storage */
	BU_GET(output_pnts[i], struct bu_ptbl);
	bu_ptbl_init(output_pnts[i], 64, "first and last hit points");
	state[i].ptr = (void *)output_pnts[i];
    }
    if (rt_gettree(rtip, obj) < 0) return -1;

    rt_prep_parallel(rtip, ncpus);

    currtime = bu_gettime();
    bu_log("prep time: %.1f\n", (currtime - oldtime)/1e6);

    /* Regardless of whether or not we're shooting the grid, it is our
     * guide for how many rays to fire */
    if (do_grid) {
	fastf_t *rays;
	count = analyze_get_bbox_rays(&rays, rtip->mdl_min, rtip->mdl_max, tol);
	for (i = 0; i < ncpus+1; i++) {
	    state[i].step = (int)(count/ncpus * 0.1);
	    state[i].rays_cnt = count;
	    state[i].rays = rays;
	}
	oldtime = bu_gettime();
	bu_parallel(analyze_gen_worker, ncpus, (void *)state);
	currtime = bu_gettime();
	bu_log("grid raytrace time: %.1f\n", (currtime - oldtime)/1e6);
	for (i = 0; i < ncpus+1; i++) {
	    state[i].rays = NULL;
	}
    }

    /* We now know enough to get the max ray count, if it wasn't supplied */
    mrc = (max_ray_cnt) ? max_ray_cnt : (long int)((16 * rtip->rti_radius*rtip->rti_radius)/tol->dist_sq);

    if (flags & ANALYZE_OBJ_TO_PNTS_RAND) {
	fastf_t delta = 0;
	long int raycnt = 0;
	for (i = 0; i < ncpus+1; i++) {
	    if (!state[i].rays) {
		state[i].rays = (fastf_t *)bu_calloc(craynum * 6 + 1, sizeof(fastf_t), "rays");
	    }
	}
	oldtime = bu_gettime();
	while (delta < (fastf_t)max_time && raycnt < mrc) {
	    for (i = 0; i < ncpus+1; i++) {
		get_random_rays(state[i].rays, craynum);
	    }
	    bu_parallel(analyze_prand_pnt_worker, ncpus, (void *)state);
	    raycnt += craynum * (ncpus+1);
	    delta = (bu_gettime() - oldtime)/1e6;
	}
    }

    if (flags & ANALYZE_OBJ_TO_PNTS_SOBOL) {
	fastf_t delta = 0;
	long int raycnt = 0;
	for (i = 0; i < ncpus+1; i++) {
	    if (!state[i].rays) {
		state[i].rays = (fastf_t *)bu_calloc(craynum * 6 + 1, sizeof(fastf_t), "rays");
	    }
	}
	oldtime = bu_gettime();
	while (delta < (fastf_t)max_time && raycnt < mrc) {
	    for (i = 0; i < ncpus+1; i++) {
		get_sobol_rays(state[i].rays, craynum);
	    }
	    bu_parallel(analyze_prand_pnt_worker, ncpus, (void *)state);
	    raycnt += craynum * (ncpus+1);
	    delta = (bu_gettime() - oldtime)/1e6;
	}
    }

    /* Collect and print all of the results */

    for (i = 0; i < ncpus+1; i++) {
	pntcnt += (int)BU_PTBL_LEN(output_pnts[i]);
    }

    if (pntcnt < 1) {
	ret = ANALYZE_ERROR;
    } else {
	rpnts->count = pntcnt;
	BU_ALLOC(rpnts->point, struct pnt_normal);
	BU_LIST_INIT(&(((struct pnt_normal *)rpnts->point)->l));
	for (i = 0; i < ncpus+1; i++) {
	    for (j = 0; j < (int)BU_PTBL_LEN(output_pnts[i]); j++) {
		BU_LIST_PUSH(&(((struct pnt_normal *)rpnts->point)->l), &((struct pnt_normal *)BU_PTBL_GET(output_pnts[i], j))->l);
	    }
	}
	ret = 0;
    }

memfree:
    /* Free memory not stored in tables */
    for (i = 0; i < ncpus+1; i++) {
	if (output_pnts[i] != NULL) {
	    bu_ptbl_free(output_pnts[i]);
	    BU_PUT(output_pnts[i], struct bu_ptbl);
	}
    }
    bu_free(state, "free state containers");
    bu_free(output_pnts, "free state containers");
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

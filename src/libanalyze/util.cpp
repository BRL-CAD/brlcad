/*                       U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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
/** @file libanalyze/util.c
 *
 */
#include "common.h"

#include <string.h> /* for memset */
#include <map>

#include "vmath.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "bn/mat.h"
#include "analyze_private.h"
#include "raytrace.h"
#include "analyze.h"


extern "C" void
analyze_gen_worker(int cpu, void *ptr)
{
    struct application ap;
    struct rt_gen_worker_vars *state = &(((struct rt_gen_worker_vars *)ptr)[cpu]);
    int start_ind, end_ind, i;
    int state_jmp = 0;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = state->rtip;
    ap.a_hit = state->fhit;
    ap.a_miss = state->fmiss;
    ap.a_overlap = state->foverlap;
    ap.a_onehit = 0;
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.a_resource = state->resp;
    ap.a_uptr = (void *)state;

    /* Because a zero step means an infinite loop, ensure we are moving ahead
     * at least 1 ray each step */
    if (!state->step) state_jmp = 1;

    bu_semaphore_acquire(RT_SEM_WORKER);
    start_ind = (*state->ind_src);
    (*state->ind_src) = start_ind + state->step + state_jmp;
    //bu_log("increment(%d): %d\n", cpu, start_ind);
    bu_semaphore_release(RT_SEM_WORKER);
    end_ind = start_ind + state->step + state_jmp - 1;
    while (start_ind < state->rays_cnt) {
	for (i = start_ind; i <= end_ind && i < state->rays_cnt; i++) {
	    state->curr_ind = i;
	    VSET(ap.a_ray.r_pt, state->rays[6*i+0], state->rays[6*i+1], state->rays[6*i+2]);
	    VSET(ap.a_ray.r_dir, state->rays[6*i+3], state->rays[6*i+4], state->rays[6*i+5]);
	    rt_shootray(&ap);
	}
	bu_semaphore_acquire(RT_SEM_WORKER);
	start_ind = (*state->ind_src);
	(*state->ind_src) = start_ind + state->step + state_jmp;
	//bu_log("increment(%d): %d\n", cpu, start_ind);
	bu_semaphore_release(RT_SEM_WORKER);
	end_ind = start_ind + state->step + state_jmp - 1;
    }
}

extern "C" int
analyze_get_bbox_rays(fastf_t **rays, point_t min, point_t max, struct bn_tol *tol)
{
    int ret = 0;
    int count = 0;
    point_t mid;
    struct rt_pattern_data *xdata = NULL;
    struct rt_pattern_data *ydata = NULL;
    struct rt_pattern_data *zdata = NULL;

    if (!rays || !tol) return 0;

    if (NEAR_ZERO(DIST_PNT_PNT_SQ(min, max), VUNITIZE_TOL)) return 0;

    /* We've got the bounding box - set up the grids */
    VSET(mid, (max[0] + min[0])/2, (max[1] + min[1])/2, (max[2] + min[2])/2);

    BU_GET(xdata, struct rt_pattern_data);
    VSET(xdata->center_pt, min[0] - 0.1 * fabs(min[0]), mid[1], mid[2]);
    VSET(xdata->center_dir, 1, 0, 0);
    xdata->vn = 2;
    xdata->pn = 2;
    xdata->n_vec = (vect_t *)bu_calloc(xdata->vn + 1, sizeof(vect_t), "vects array");
    xdata->n_p = (fastf_t *)bu_calloc(xdata->pn + 1, sizeof(fastf_t), "params array");
    xdata->n_p[0] = tol->dist;
    xdata->n_p[1] = tol->dist;
    VSET(xdata->n_vec[0], 0, max[1] - mid[1], 0);
    VSET(xdata->n_vec[1], 0, 0, max[2] - mid[2]);
    ret = rt_pattern(xdata, RT_PATTERN_RECT_ORTHOGRID);
    bu_free(xdata->n_vec, "x vec inputs");
    bu_free(xdata->n_p, "x p inputs");
    if (ret < 0) {
	count = ret;
	goto memfree;
    }

    BU_GET(ydata, struct rt_pattern_data);
    VSET(ydata->center_pt, mid[0], min[1] - 0.1 * fabs(min[1]), mid[2]);
    VSET(ydata->center_dir, 0, 1, 0);
    ydata->vn = 2;
    ydata->pn = 2;
    ydata->n_vec = (vect_t *)bu_calloc(ydata->vn + 1, sizeof(vect_t), "vects array");
    ydata->n_p = (fastf_t *)bu_calloc(ydata->pn + 1, sizeof(fastf_t), "params array");
    ydata->n_p[0] = tol->dist;
    ydata->n_p[1] = tol->dist;
    VSET(ydata->n_vec[0], max[0] - mid[0], 0, 0);
    VSET(ydata->n_vec[1], 0, 0, max[2] - mid[2]);
    ret = rt_pattern(ydata, RT_PATTERN_RECT_ORTHOGRID);
    bu_free(ydata->n_vec, "y vec inputs");
    bu_free(ydata->n_p, "y p inputs");
    if (ret < 0) {
	count = ret;
	goto memfree;
    }

    BU_GET(zdata, struct rt_pattern_data);
    VSET(zdata->center_pt, mid[0], mid[1], min[2] - 0.1 * fabs(min[2]));
    VSET(zdata->center_dir, 0, 0, 1);
    zdata->vn = 2;
    zdata->pn = 2;
    zdata->n_vec = (vect_t *)bu_calloc(zdata->vn + 1, sizeof(vect_t), "vects array");
    zdata->n_p = (fastf_t *)bu_calloc(zdata->pn + 1, sizeof(fastf_t), "params array");
    zdata->n_p[0] = tol->dist;
    zdata->n_p[1] = tol->dist;
    VSET(zdata->n_vec[0], max[0] - mid[0], 0, 0);
    VSET(zdata->n_vec[1], 0, max[1] - mid[1], 0);
    ret = rt_pattern(zdata, RT_PATTERN_RECT_ORTHOGRID);
    bu_free(zdata->n_vec, "x vec inputs");
    bu_free(zdata->n_p, "x p inputs");
    if (ret < 0) {
	count = ret;
	goto memfree;
    }

    /* Consolidate the grids into a single ray array */
    {
	size_t si, sj;
	(*rays) = (fastf_t *)bu_calloc((xdata->ray_cnt + ydata->ray_cnt + zdata->ray_cnt + 1) * 6, sizeof(fastf_t), "rays");
	count = 0;
	for (si = 0; si < xdata->ray_cnt; si++) {
	    for (sj = 0; sj < 6; sj++) {
		(*rays)[6*count+sj] = xdata->rays[6*si + sj];
	    }
	    count++;
	}
	for (si = 0; si < ydata->ray_cnt; si++) {
	    for (sj = 0; sj < 6; sj++) {
		(*rays)[6*count+sj] = ydata->rays[6*si + sj];
	    }
	    count++;
	}
	for (si = 0; si < zdata->ray_cnt; si++) {
	    for (sj = 0; sj < 6; sj++) {
		(*rays)[6*count+sj] = zdata->rays[6*si+sj];
	    }
	    count++;
	}

    }
/*
    bu_log("ray cnt: %d\n", count);
*/

memfree:
    /* Free memory not stored in tables */
    if (xdata && xdata->rays) bu_free(xdata->rays, "x rays");
    if (ydata && ydata->rays) bu_free(ydata->rays, "y rays");
    if (zdata && zdata->rays) bu_free(zdata->rays, "z rays");
    if (xdata) BU_PUT(xdata, struct rt_pattern_data);
    if (ydata) BU_PUT(ydata, struct rt_pattern_data);
    if (zdata) BU_PUT(zdata, struct rt_pattern_data);
    return count;
}

/* TODO - consolidate with above */
extern "C" int
analyze_get_scaled_bbox_rays(fastf_t **rays, point_t min, point_t max, fastf_t ratio)
{
    int ret, count;
    point_t mid;
    struct rt_pattern_data *xdata = NULL;
    struct rt_pattern_data *ydata = NULL;
    struct rt_pattern_data *zdata = NULL;

    if (!rays) return 0;

    if (NEAR_ZERO(DIST_PNT_PNT_SQ(min, max), VUNITIZE_TOL)) return 0;

    /* We've got the bounding box - set up the grids */
    VSET(mid, (max[0] + min[0])/2, (max[1] + min[1])/2, (max[2] + min[2])/2);

    BU_GET(xdata, struct rt_pattern_data);
    VSET(xdata->center_pt, min[0] - 0.1 * fabs(min[0]), mid[1], mid[2]);
    VSET(xdata->center_dir, 1, 0, 0);
    xdata->vn = 2;
    xdata->pn = 2;
    xdata->n_vec = (vect_t *)bu_calloc(xdata->vn + 1, sizeof(vect_t), "vects array");
    xdata->n_p = (fastf_t *)bu_calloc(xdata->pn + 1, sizeof(fastf_t), "params array");
    xdata->n_p[0] = (max[1] - min[1])*ratio;
    xdata->n_p[1] = (max[2] - min[2])*ratio;
    VSET(xdata->n_vec[0], 0, max[1] - mid[1], 0);
    VSET(xdata->n_vec[1], 0, 0, max[2] - mid[2]);
    ret = rt_pattern(xdata, RT_PATTERN_RECT_ORTHOGRID);
    bu_free(xdata->n_vec, "x vec inputs");
    bu_free(xdata->n_p, "x p inputs");
    if (ret < 0) {
	ret = 0;
	goto memfree;
    }

    BU_GET(ydata, struct rt_pattern_data);
    VSET(ydata->center_pt, mid[0], min[1] - 0.1 * fabs(min[1]), mid[2]);
    VSET(ydata->center_dir, 0, 1, 0);
    ydata->vn = 2;
    ydata->pn = 2;
    ydata->n_vec = (vect_t *)bu_calloc(ydata->vn + 1, sizeof(vect_t), "vects array");
    ydata->n_p = (fastf_t *)bu_calloc(ydata->pn + 1, sizeof(fastf_t), "params array");
    ydata->n_p[0] = (max[0] - min[0])*ratio;
    ydata->n_p[1] = (max[2] - min[2])*ratio;
    VSET(ydata->n_vec[0], max[0] - mid[0], 0, 0);
    VSET(ydata->n_vec[1], 0, 0, max[2] - mid[2]);
    ret = rt_pattern(ydata, RT_PATTERN_RECT_ORTHOGRID);
    bu_free(ydata->n_vec, "y vec inputs");
    bu_free(ydata->n_p, "y p inputs");
    if (ret < 0) {
	ret = 0;
	goto memfree;
    }

    BU_GET(zdata, struct rt_pattern_data);
    VSET(zdata->center_pt, mid[0], mid[1], min[2] - 0.1 * fabs(min[2]));
    VSET(zdata->center_dir, 0, 0, 1);
    zdata->vn = 2;
    zdata->pn = 2;
    zdata->n_vec = (vect_t *)bu_calloc(zdata->vn + 1, sizeof(vect_t), "vects array");
    zdata->n_p = (fastf_t *)bu_calloc(zdata->pn + 1, sizeof(fastf_t), "params array");
    zdata->n_p[0] = (max[0] - min[0])*ratio;
    zdata->n_p[1] = (max[1] - min[1])*ratio;
    VSET(zdata->n_vec[0], max[0] - mid[0], 0, 0);
    VSET(zdata->n_vec[1], 0, max[1] - mid[1], 0);
    ret = rt_pattern(zdata, RT_PATTERN_RECT_ORTHOGRID);
    bu_free(zdata->n_vec, "x vec inputs");
    bu_free(zdata->n_p, "x p inputs");
    if (ret < 0) {
	ret = 0;
	goto memfree;
    }

    /* Consolidate the grids into a single ray array */
    {
	size_t si, sj;
	(*rays) = (fastf_t *)bu_calloc((xdata->ray_cnt + ydata->ray_cnt + zdata->ray_cnt + 1) * 6, sizeof(fastf_t), "rays");
	count = 0;
	for (si = 0; si < xdata->ray_cnt; si++) {
	    for (sj = 0; sj < 6; sj++) {
		(*rays)[6*count+sj] = xdata->rays[6*si + sj];
	    }
	    count++;
	}
	for (si = 0; si < ydata->ray_cnt; si++) {
	    for (sj = 0; sj < 6; sj++) {
		(*rays)[6*count+sj] = ydata->rays[6*si + sj];
	    }
	    count++;
	}
	for (si = 0; si < zdata->ray_cnt; si++) {
	    for (sj = 0; sj < 6; sj++) {
		(*rays)[6*count+sj] = zdata->rays[6*si+sj];
	    }
	    count++;
	}

    }
/*
    bu_log("ray cnt: %d\n", count);
*/
    return count;

memfree:
    /* Free memory not stored in tables */
    if (xdata && xdata->rays) bu_free(xdata->rays, "x rays");
    if (ydata && ydata->rays) bu_free(ydata->rays, "y rays");
    if (zdata && zdata->rays) bu_free(zdata->rays, "z rays");
    if (xdata) BU_PUT(xdata, struct rt_pattern_data);
    if (ydata) BU_PUT(ydata, struct rt_pattern_data);
    if (zdata) BU_PUT(zdata, struct rt_pattern_data);
    return ret;
}


struct segfilter_container {
    int ray_dir;
    int ncpus;
    fastf_t tol;
    fastf_t *rays;
    int rays_cnt;
    int cnt;
    getray_t g_ray;
    getflag_t g_flag;
    struct bu_ptbl *test;
};

HIDDEN int
segfilter_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    point_t in_pt, out_pt;
    struct partition *part;
    fastf_t part_len = 0.0;
    struct rt_gen_worker_vars *s = (struct rt_gen_worker_vars *)(ap->a_uptr);
    struct segfilter_container *state = (struct segfilter_container *)(s->ptr);

    for (part = PartHeadp->pt_forw; part != PartHeadp; part = part->pt_forw) {
        VJOIN1(in_pt, ap->a_ray.r_pt, part->pt_inhit->hit_dist, ap->a_ray.r_dir);
        VJOIN1(out_pt, ap->a_ray.r_pt, part->pt_outhit->hit_dist, ap->a_ray.r_dir);
        part_len = DIST_PNT_PNT(in_pt, out_pt);
        if (part_len > state->tol) {
            state->cnt++;
            return 0;
        }
    }

    return 0;
}

HIDDEN int
segfilter_miss(struct application *ap)
{
    RT_CK_APPLICATION(ap);

    return 0;
}

HIDDEN int
segfilter_overlap(struct application *ap,
                struct partition *UNUSED(pp),
                struct region *UNUSED(reg1),
                struct region *UNUSED(reg2),
                struct partition *UNUSED(hp))
{
    RT_CK_APPLICATION(ap);
    return 0;
}

extern "C" void
segfilter_gen_worker(int cpu, void *ptr)
{
    struct application ap;
    struct rt_gen_worker_vars *s = &(((struct rt_gen_worker_vars *)ptr)[cpu]);
    struct segfilter_container *state = (struct segfilter_container *)(s->ptr);
    int start_ind, end_ind, i;
    int state_jmp = 0;

    /* If test is NULL in state or it's zero length, just return. */
    if (!state->test || !BU_PTBL_LEN(state->test)) return;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = s->rtip;
    ap.a_hit = s->fhit;
    ap.a_miss = s->fmiss;
    ap.a_overlap = s->foverlap;
    ap.a_onehit = 0;
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.a_resource = s->resp;
    ap.a_uptr = (void *)s;

    /* Because a zero step means an infinite loop, ensure we are moving ahead
     * at least 1 ray each step */
    if (!s->step) state_jmp = 1;

    bu_semaphore_acquire(RT_SEM_WORKER);
    start_ind = (*s->ind_src);
    (*s->ind_src) = start_ind + s->step + state_jmp;
    //bu_log("increment(%d): %d\n", cpu, start_ind);
    bu_semaphore_release(RT_SEM_WORKER);
    end_ind = start_ind + s->step + state_jmp - 1;
    while (start_ind < (int)BU_PTBL_LEN(state->test)) {
	for (i = start_ind; i <= end_ind && i < (int)BU_PTBL_LEN(state->test); i++) {
	    point_t r_pt;
	    vect_t v1, v2;
	    int valid = 0;
	    struct xray *ray = (*state->g_ray)((void *)BU_PTBL_GET(state->test, i));
	    int *d_valid = (*state->g_flag)((void *)BU_PTBL_GET(state->test, i));
	    VMOVE(ap.a_ray.r_dir, ray->r_dir);
	    /* Construct 4 rays and test around the original hit.*/
	    bn_vec_perp(v1, ray->r_dir);
	    VCROSS(v2, v1, ray->r_dir);
	    VUNITIZE(v1);
	    VUNITIZE(v2);

	    valid = 0;
	    VJOIN2(r_pt, ray->r_pt, 0.5 * state->tol, v1, 0.5 * state->tol, v2);
	    state->cnt = 0;
	    VMOVE(ap.a_ray.r_pt, r_pt);
	    rt_shootray(&ap);
	    if (state->cnt) valid++;
	    state->cnt = 0;
	    VJOIN2(r_pt, ray->r_pt, -0.5 * state->tol, v1, -0.5 * state->tol, v2);
	    state->cnt = 0;
	    VMOVE(ap.a_ray.r_pt, r_pt);
	    rt_shootray(&ap);
	    if (state->cnt) valid++;
	    VJOIN2(r_pt, ray->r_pt, -0.5 * state->tol, v1, 0.5 * state->tol, v2);
	    state->cnt = 0;
	    VMOVE(ap.a_ray.r_pt, r_pt);
	    rt_shootray(&ap);
	    if (state->cnt) valid++;
	    state->cnt = 0;
	    VJOIN2(r_pt, ray->r_pt, 0.5 * state->tol, v1, -0.5 * state->tol, v2);
	    state->cnt = 0;
	    VMOVE(ap.a_ray.r_pt, r_pt);
	    rt_shootray(&ap);
	    if (state->cnt) valid++;

	    if (valid == 4) {
		*d_valid = 1;
	    }
	}
	bu_semaphore_acquire(RT_SEM_WORKER);
	start_ind = (*s->ind_src);
	(*s->ind_src) = start_ind + s->step + state_jmp;
	//bu_log("increment(%d): %d\n", cpu, start_ind);
	bu_semaphore_release(RT_SEM_WORKER);
	end_ind = start_ind + s->step + state_jmp - 1;
    }
}

/* TODO - may be several possible operations here:
 *
 * filter all rays not "solid" (as in gdiff)
 * filter all rays not surrounded by "similar" results (need to keep subtractions)
 * filter all rays that are "solid"
 * filter all rays surrounded by "similar" results (grab only grazing rays)
 */
extern "C" void
analyze_seg_filter(struct bu_ptbl *segs, getray_t gray, getflag_t gflag, struct rt_i *rtip, struct resource *resp, fastf_t tol, int ncpus)
{
    int i = 0;
    int ind = 0;
    int step = 0;
    struct rt_gen_worker_vars *state;
    struct segfilter_container *local_state;

    if (!segs || !rtip) return;

    state = (struct rt_gen_worker_vars *)bu_calloc(ncpus+1, sizeof(struct rt_gen_worker_vars ), "state");
    local_state = (struct segfilter_container *)bu_calloc(ncpus+1, sizeof(struct segfilter_container), "local resources");

    step = (int)(BU_PTBL_LEN(segs)/ncpus * 0.1);

    /* Preparing rtip is the responsibility of the calling function - here we're just setting
     * up the resources.  The rays are contained in the individual segs structures and are
     * extracted by the gen_worker function. */
    for (i = 0; i < ncpus+1; i++) {
	state[i].rtip = rtip;
	state[i].resp = &resp[i];
	state[i].ind_src = &ind;
	state[i].fhit = segfilter_hit;
	state[i].fmiss = segfilter_miss;
	state[i].foverlap = segfilter_overlap;
	state[i].step = step;
	local_state[i].tol = tol;
	local_state[i].ncpus = ncpus;
	local_state[i].g_ray = gray;
	local_state[i].g_flag = gflag;
	local_state[i].test = segs;
	state[i].ptr = (void *)&(local_state[i]);
    }

    bu_parallel(segfilter_gen_worker, ncpus, (void *)state);

    bu_free(local_state, "local state");
    bu_free(state, "state");

    return;
}

HIDDEN struct xray *
mp_ray(void *container)
{
    struct minimal_partitions *d = (struct minimal_partitions *)container;
    return &(d->ray);
}

HIDDEN int *
mp_flag(void *container)
{
    struct minimal_partitions *d = (struct minimal_partitions *)container;
    return &(d->valid);
}

struct solids_container {
    fastf_t tol;
    struct minimal_partitions **results;
};

HIDDEN int
sp_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    int hit_cnt = 0;
    struct partition *part;
    fastf_t part_len = 0.0;
    struct minimal_partitions *p;
    struct rt_gen_worker_vars *s = (struct rt_gen_worker_vars *)(ap->a_uptr);
    struct solids_container *state = (struct solids_container *)(s->ptr);
    fastf_t g1 = -DBL_MAX;
    fastf_t g2 = -DBL_MAX;

    for (part = PartHeadp->pt_forw; part != PartHeadp; part = part->pt_forw) {
	part_len = part->pt_outhit->hit_dist - part->pt_inhit->hit_dist;
	if (part_len > state->tol) hit_cnt++;
    }
    if (!hit_cnt) return 0;
    if (state->results[s->curr_ind]) {
	bu_log("Huh??\n");
    }
    BU_GET(state->results[s->curr_ind], struct minimal_partitions);
    p = state->results[s->curr_ind];
    p->index = s->curr_ind;
    VMOVE(p->ray.r_pt, ap->a_ray.r_pt);
    VMOVE(p->ray.r_dir, ap->a_ray.r_dir);
    p->hit_cnt = 0;
    p->gap_cnt = 0;
    p->hits = (fastf_t *)bu_calloc(hit_cnt * 2, sizeof(fastf_t), "overlaps");
    if (hit_cnt > 1)
	p->gaps = (fastf_t *)bu_calloc((hit_cnt - 1) * 2, sizeof(fastf_t), "overlaps");
    for (part = PartHeadp->pt_forw; part != PartHeadp; part = part->pt_forw) {
	part_len = part->pt_outhit->hit_dist - part->pt_inhit->hit_dist;
	if (part_len > state->tol) {
	    if (g1 > -DBL_MAX) g2 = part->pt_inhit->hit_dist;
	    //bu_log("hit: t = %f to t = %f, len: %f\n", part->pt_inhit->hit_dist, part->pt_outhit->hit_dist, part_len);
	    p->hits[p->hit_cnt] = part->pt_inhit->hit_dist;
	    p->hits[p->hit_cnt + 1] = part->pt_outhit->hit_dist;
	    p->hit_cnt = p->hit_cnt + 2;
	    if (g2 > -DBL_MAX) {
		//bu_log("gap: t = %f to t = %f, len: %f\n", g1, g2, g2 - g1);
		p->gaps[p->gap_cnt] = g1;
		p->gaps[p->gap_cnt + 1] = g2;
		p->gap_cnt = p->gap_cnt + 2;
	    }
	    g1 = part->pt_outhit->hit_dist;
	}
    }
    p->hit_cnt = hit_cnt;
    p->gap_cnt = (int)(p->gap_cnt / 2);
    return 0;
}

HIDDEN int
sp_miss(struct application *ap)
{
    RT_CK_APPLICATION(ap);
    return 0;
}

HIDDEN int
sp_overlap(struct application *ap,
	struct partition *UNUSED(pp),
	struct region *UNUSED(reg1),
	struct region *UNUSED(reg2),
	struct partition *UNUSED(hp))
{
    RT_CK_APPLICATION(ap);
    return 0;
}

/* Will need the region flag set for this to work... */
extern "C" int
analyze_get_solid_partitions(struct bu_ptbl *results, struct rt_gen_worker_vars *pstate, fastf_t *rays, size_t ray_cnt,
	struct db_i *dbip, const char *obj, struct bn_tol *tol, size_t ncpus, int filter)
{
    size_t i;
    int ind = 0;
    int ret = 0;
    size_t j;
    struct rt_gen_worker_vars *state;
    struct solids_container *local_state;
    struct resource *resp;
    struct rt_i *rtip;
    struct bu_ptbl temp_results = BU_PTBL_INIT_ZERO;
    struct minimal_partitions **ray_results;

    if (!results || !rays || ray_cnt == 0 || ncpus == 0 || !dbip || !obj || !tol)
	return 0;

    if (!pstate) {
	state = (struct rt_gen_worker_vars *)bu_calloc(ncpus+1, sizeof(struct rt_gen_worker_vars), "state");
	resp = (struct resource *)bu_calloc(ncpus+1, sizeof(struct resource), "resources");
	rtip = rt_new_rti(dbip);
    } else {
	state = pstate;
	resp = state->resp;
	rtip = state->rtip;
    }
    local_state = (struct solids_container *)bu_calloc(ncpus+1, sizeof(struct solids_container), "local state");
    ray_results = (struct minimal_partitions **)bu_calloc(ray_cnt+1, sizeof(struct minimal_partitions *), "results");

    if (!pstate) {
	for (i = 0; i < ncpus+1; i++) {
	    /* standard */
	    state[i].rtip = rtip;
	    state[i].resp = &resp[i];
	    rt_init_resource(state[i].resp, i, rtip);
	}
	if (rt_gettree(rtip, obj) < 0) {
	    ret = -1;
	    goto memfree;
	}
	rt_prep_parallel(rtip, ncpus);
    }

    for (i = 0; i < ncpus+1; i++) {
	/* standard */
	state[i].fhit = sp_hit;
	state[i].fmiss = sp_miss;
	state[i].foverlap = sp_overlap;
	state[i].step = (int)(ray_cnt/ncpus * 0.1);
	state[i].ind_src = &ind;

	/* local */
	local_state[i].tol = 0.5;
	local_state[i].results = ray_results;
	state[i].ptr = (void *)&(local_state[i]);
    }

    for (i = 0; i < ncpus+1; i++) {
	state[i].rays_cnt = ray_cnt;
	state[i].rays = rays;
    }

    bu_parallel(analyze_gen_worker, ncpus, (void *)state);

    for (i = 0; i < ray_cnt; i++) {
	struct minimal_partitions *p = ray_results[i];
	if (p) {
	    bu_ptbl_ins(&temp_results, (long *)p);
	}
    }
    if (filter) {
	analyze_seg_filter(&temp_results, &mp_ray, &mp_flag, rtip, resp, 0.5, ncpus);
    } else {
	for (j = 0; j < BU_PTBL_LEN(&temp_results); j++) {
	    struct minimal_partitions *p = (struct minimal_partitions *)BU_PTBL_GET(&temp_results, j);
	    p->valid = 1;
	}
    }
    // TODO - assign valid results to final results table and free invalid results
    for (j = 0; j < BU_PTBL_LEN(&temp_results); j++) {
	struct minimal_partitions *p = (struct minimal_partitions *)BU_PTBL_GET(&temp_results, j);
	if (p->valid) {
	    bu_ptbl_ins(results, (long *)p);
	}
    }

    ret = BU_PTBL_LEN(results);
memfree:

    bu_free(ray_results, "free state");
    bu_free(local_state, "free state");
    if (!pstate) {
	bu_free(state, "free state");
	bu_free(resp, "free state");
    }
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

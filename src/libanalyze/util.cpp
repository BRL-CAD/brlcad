/*                       U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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

extern "C" {
#include "vmath.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "bn/mat.h"
#include "analyze_private.h"
}
#include "raytrace.h"
extern "C" {
#include "analyze.h"
}

extern "C" void
analyze_gen_worker(int cpu, void *ptr)
{
    struct application ap;
    struct rt_gen_worker_vars *state = &(((struct rt_gen_worker_vars *)ptr)[cpu]);
    int start_ind, end_ind, i;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = state->rtip;
    ap.a_hit = state->fhit;
    ap.a_miss = state->fmiss;
    ap.a_overlap = state->foverlap;
    ap.a_onehit = 0;
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.a_resource = state->resp;
    ap.a_uptr = (void *)state;

    bu_semaphore_acquire(RT_SEM_WORKER);
    start_ind = (*state->ind_src);
    (*state->ind_src) = start_ind + state->step;
    //bu_log("increment(%d): %d\n", cpu, start_ind);
    bu_semaphore_release(RT_SEM_WORKER);
    end_ind = start_ind + state->step - 1;
    while (start_ind < state->rays_cnt) {
	for (i = start_ind; i <= end_ind && i < state->rays_cnt; i++) {
	    VSET(ap.a_ray.r_pt, state->rays[6*i+0], state->rays[6*i+1], state->rays[6*i+2]);
	    VSET(ap.a_ray.r_dir, state->rays[6*i+3], state->rays[6*i+4], state->rays[6*i+5]);
	    rt_shootray(&ap);
	}
	bu_semaphore_acquire(RT_SEM_WORKER);
	start_ind = (*state->ind_src);
	(*state->ind_src) = start_ind + state->step;
	//bu_log("increment(%d): %d\n", cpu, start_ind);
	bu_semaphore_release(RT_SEM_WORKER);
	end_ind = start_ind + state->step - 1;
    }
}

extern "C" int
analyze_get_bbox_rays(fastf_t **rays, point_t min, point_t max, struct bn_tol *tol)
{
    int ret, count;
    point_t mid;
    struct rt_pattern_data *xdata, *ydata, *zdata;

    if (!rays || !tol) return 0;

    if (NEAR_ZERO(DIST_PT_PT_SQ(min, max), VUNITIZE_TOL)) return 0;

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
    ydata->n_p[0] = tol->dist;
    ydata->n_p[1] = tol->dist;
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
    zdata->n_p[0] = tol->dist;
    zdata->n_p[1] = tol->dist;
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
    bu_free(xdata->rays, "x rays");
    bu_free(ydata->rays, "y rays");
    bu_free(zdata->rays, "z rays");
    BU_PUT(xdata, struct rt_pattern_data);
    BU_PUT(ydata, struct rt_pattern_data);
    BU_PUT(zdata, struct rt_pattern_data);
    return ret;
}


struct segfilter_container {
    struct rt_i *rtip;
    struct resource *resp;
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
    struct segfilter_container *state = (struct segfilter_container *)(ap->a_uptr);

    for (part = PartHeadp->pt_forw; part != PartHeadp; part = part->pt_forw) {
        VJOIN1(in_pt, ap->a_ray.r_pt, part->pt_inhit->hit_dist, ap->a_ray.r_dir);
        VJOIN1(out_pt, ap->a_ray.r_pt, part->pt_outhit->hit_dist, ap->a_ray.r_dir);
        part_len = DIST_PT_PT(in_pt, out_pt);
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

HIDDEN void
segfilter_gen_worker(int cpu, void *ptr)
{
    struct application ap;
    struct segfilter_container *state = &(((struct segfilter_container *)ptr)[cpu]);
    fastf_t si, ei;
    int start_ind, end_ind, i;
    if (cpu == 0) {
        /* If we're serial, start at the beginning */
        start_ind = 0;
        end_ind = BU_PTBL_LEN(state->test) - 1;
    } else {
        si = (fastf_t)(cpu - 1)/(fastf_t)state->ncpus * (fastf_t) BU_PTBL_LEN(state->test);
        ei = (fastf_t)cpu/(fastf_t)state->ncpus * (fastf_t) BU_PTBL_LEN(state->test) - 1;
        start_ind = (int)si;
        end_ind = (int)ei;
        if (BU_PTBL_LEN(state->test) - end_ind < 3) end_ind = BU_PTBL_LEN(state->test) - 1;
        /*
        bu_log("start_ind (%d): %d\n", cpu, start_ind);
        bu_log("end_ind (%d): %d\n", cpu, end_ind);
        */
    }

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = state->rtip;
    ap.a_hit = segfilter_hit;
    ap.a_miss = segfilter_miss;
    ap.a_overlap = segfilter_overlap;
    ap.a_onehit = 0;
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.a_resource = state->resp;
    ap.a_uptr = (void *)state;

    for (i = start_ind; i <= end_ind; i++) {
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
}



/* TODO - may be several possible operations here:
 *
 * filter all rays not "solid" (as in gdiff)
 * filter all rays not surrounded by "similar" results (need to keep subtractions)
 * filter all rays that are "solid"
 * filter all rays surrouned by "similar" results (grab only grazing rays)
 */
extern "C" void
analyze_seg_filter(struct bu_ptbl *segs, getray_t gray, getflag_t gflag, struct rt_i *rtip, struct resource *resp, fastf_t tol)
{
    int i = 0;
    int ncpus = bu_avail_cpus();
    struct segfilter_container *state;

    if (!segs || !rtip) return;

    state = (struct segfilter_container *)bu_calloc(ncpus+1, sizeof(struct segfilter_container), "resources");
    /* Preparing rtip is the responsibility of the calling function - here we're just setting
     * up the resources.  The rays are contained in the individual segs structures and are
     * extracted by the gen_worker function. */
    for (i = 0; i < ncpus+1; i++) {
	state[i].rtip = rtip;
	state[i].tol = tol;
	state[i].ncpus = ncpus;
	state[i].resp = &resp[i];
	state[i].g_ray = gray;
	state[i].g_flag = gflag;
	state[i].test = segs;
    }

    bu_parallel(segfilter_gen_worker, ncpus, (void *)state);

    bu_free(state, "state");

    return;
}

struct raydiff_container {
    struct rt_i *rtip;
    struct resource *resp;
    int ray_dir;
    int ncpus;
    fastf_t tol;
    int have_diffs;
    const char *left_name;
    const char *right_name;
    struct bu_ptbl *left;
    struct bu_ptbl *both;
    struct bu_ptbl *right;
    const fastf_t *rays;
    int rays_cnt;
    int cnt;
    struct bu_ptbl *test;
};

struct xray *
mp_ray(void *container)
{
    struct minimal_partitions *d = (struct minimal_partitions *)container;
    return &(d->ray);
}

int *
mp_flag(void *container)
{
    struct minimal_partitions *d = (struct minimal_partitions *)container;
    return &(d->valid);
}


int
analyze_get_solid_partitions(struct bu_ptbl *results, const fastf_t *rays, int ray_cnt,
	struct db_i *dbip, const char *obj, struct bn_tol *tol)
{
    int i, j;
    int ret = 0;
    int ncpus = bu_avail_cpus();
    struct raydiff_container *state;
    struct resource *resp;
    struct rt_i *rtip;

    if (!results || !rays || ray_cnt == 0 || !dbip || !obj || !tol) return 0;

    state = (struct raydiff_container *)bu_calloc(ncpus+1, sizeof(struct raydiff_container), "state");
    resp = (struct resource *)bu_calloc(ncpus+1, sizeof(struct resource), "resources");
    rtip = rt_new_rti(dbip);

    for (i = 0; i < ncpus+1; i++) {
	state[i].rtip = rtip;
	state[i].tol = 0.5;
	state[i].ncpus = ncpus;
	state[i].resp = &resp[i];
	rt_init_resource(state[i].resp, i, state->rtip);
	BU_GET(state[i].both, struct bu_ptbl);
	bu_ptbl_init(state[i].both, 64, "hits on both solids");
    }

    if (rt_gettree(rtip, obj) < 0) {
	ret = -1;
	goto memfree;
    }

    rt_prep_parallel(rtip, ncpus);

    ret = 0;
    for (i = 0; i < ncpus+1; i++) {
	state[i].rays_cnt = ray_cnt;
	state[i].rays = rays;
    }

    bu_parallel(analyze_gen_worker, ncpus, (void *)state);

    for (i = 0; i < ncpus+1; i++) {
	for (j = 0; j < (int)BU_PTBL_LEN(state[i].both); j++) {
	    bu_ptbl_ins(results, BU_PTBL_GET(state[i].both, j));
	}
    }
    analyze_seg_filter(results, &mp_ray, &mp_flag, rtip, resp, 0.5);

    // TODO - remove invalid results

    ret = BU_PTBL_LEN(results);

memfree:

    for (i = 0; i < ncpus+1; i++) {
	BU_PUT(state[i].both, struct bu_ptbl);
    }
    bu_free(state, "free state");
    bu_free(resp, "free state");

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

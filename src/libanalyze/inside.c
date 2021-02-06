/*                       I N S I D E . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2021 United States Government as represented by
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
/** @file inside.c
 *
 * Test if one object is fully inside another object by raytracing.
 *
 */
#include "common.h"

#include <string.h> /* for memset */

#include "vmath.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "raytrace.h"
#include "analyze.h"

struct rayio_info {
    int left_hit_cnt;
    int right_hit_cnt;
    int have_overlap;
};


struct rayio_container {
    struct rt_i *rtip;
    struct resource *resp;
    int ray_dir;
    int ncpus;
    const char *left_name;
    const char *right_name;
    fastf_t *rays;
    int rays_cnt;
    struct rayio_info *results;
    int ray_id;
};


HIDDEN int
rayio_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    struct partition *part;
    struct rayio_container *state = (struct rayio_container *)(ap->a_uptr);

    for (part = PartHeadp->pt_forw; part != PartHeadp; part = part->pt_forw) {
	if (!bu_strncmp(part->pt_regionp->reg_name+1, state->left_name, strlen(state->left_name))) {
	    state->results[state->ray_id].left_hit_cnt++;
	    continue;
	}
	if (!bu_strncmp(part->pt_regionp->reg_name+1, state->right_name, strlen(state->right_name))) {
	    state->results[state->ray_id].right_hit_cnt++;
	    continue;
	}
    }

    return 0;
}


HIDDEN int
rayio_overlap(struct application *ap,
		struct partition *UNUSED(pp),
		struct region *UNUSED(reg1),
		struct region *UNUSED(reg2),
		struct partition *UNUSED(hp))
{
    struct rayio_container *state = (struct rayio_container *)ap->a_uptr;
    state->results[state->ray_id].have_overlap++;
    return 0;
}


HIDDEN int
rayio_miss(struct application *ap)
{
    RT_CK_APPLICATION(ap);

    return 0;
}


HIDDEN void
rayio_gen_worker(int cpu, void *ptr)
{
    struct application ap;
    struct rayio_container *state = &(((struct rayio_container *)ptr)[cpu]);
    fastf_t si, ei;
    int start_ind, end_ind, i;
    if (cpu == 0) {
	/* If we're serial, start at the beginning */
	start_ind = 0;
	end_ind = state->rays_cnt - 1;
    } else {
	si = (fastf_t)(cpu - 1)/(fastf_t)state->ncpus * (fastf_t) state->rays_cnt;
	ei = (fastf_t)cpu/(fastf_t)state->ncpus * (fastf_t) state->rays_cnt - 1;
	start_ind = (int)si;
	end_ind = (int)ei;
	if (state->rays_cnt - end_ind < 3) end_ind = state->rays_cnt - 1;
    }

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = state->rtip;
    ap.a_hit = rayio_hit;
    ap.a_miss = rayio_miss;
    ap.a_overlap = rayio_overlap;
    ap.a_onehit = 0;
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.a_resource = state->resp;
    ap.a_uptr = (void *)state;

    for (i = start_ind; i <= end_ind; i++) {
	VSET(ap.a_ray.r_pt, state->rays[6*i+0], state->rays[6*i+1], state->rays[6*i+2]);
	VSET(ap.a_ray.r_dir, state->rays[6*i+3], state->rays[6*i+4], state->rays[6*i+5]);
	state->ray_id = i;
	rt_shootray(&ap);
    }
}


/* 0 = not inside, 1 = inside within tolerance */
int
analyze_obj_inside(struct db_i *dbip, const char *outside_candidate, const char *inside_candidate, fastf_t in_tol)
{
    int ret = 1;
    int count = 0;
    struct rt_i *rtip;
    int ncpus = bu_avail_cpus();
    point_t min, mid, max;
    struct rt_pattern_data *xdata, *ydata, *zdata;
    fastf_t *rays;
    fastf_t tol = in_tol;
    struct rayio_container *state;
    struct rayio_info *ray_results;

    if (!dbip || !outside_candidate || !inside_candidate) return 0;

    if (tol <= 0) tol = BN_TOL_DIST;

    rtip = rt_new_rti(dbip);
    if (rt_gettree(rtip, outside_candidate) < 0) return 0;
    if (rt_gettree(rtip, inside_candidate) < 0) return 0;
    rt_prep_parallel(rtip, 1);

    /* Now we've got the bounding box - set up the grids */
    VMOVE(min, rtip->mdl_min);
    VMOVE(max, rtip->mdl_max);
    VSET(mid, (max[0] + min[0])/2, (max[1] + min[1])/2, (max[2] + min[2])/2);

    BU_GET(xdata, struct rt_pattern_data);
    VSET(xdata->center_pt, min[0] - 0.1 * min[0], mid[1], mid[2]);
    VSET(xdata->center_dir, 1, 0, 0);
    xdata->vn = 2;
    xdata->pn = 2;
    xdata->n_vec = (vect_t *)bu_calloc(xdata->vn + 1, sizeof(vect_t), "vects array");
    xdata->n_p = (fastf_t *)bu_calloc(xdata->pn + 1, sizeof(fastf_t), "params array");
    xdata->n_p[0] = tol;
    xdata->n_p[1] = tol;
    VSET(xdata->n_vec[0], 0, max[1], 0);
    VSET(xdata->n_vec[1], 0, 0, max[2]);
    ret = rt_pattern(xdata, RT_PATTERN_RECT_ORTHOGRID);
    bu_free(xdata->n_vec, "x vec inputs");
    bu_free(xdata->n_p, "x p inputs");
    if (ret < 0) return 0;


    BU_GET(ydata, struct rt_pattern_data);
    VSET(ydata->center_pt, mid[0], min[1] - 0.1 * min[1], mid[2]);
    VSET(ydata->center_dir, 0, 1, 0);
    ydata->vn = 2;
    ydata->pn = 2;
    ydata->n_vec = (vect_t *)bu_calloc(ydata->vn + 1, sizeof(vect_t), "vects array");
    ydata->n_p = (fastf_t *)bu_calloc(ydata->pn + 1, sizeof(fastf_t), "params array");
    ydata->n_p[0] = tol;
    ydata->n_p[1] = tol;
    VSET(ydata->n_vec[0], max[0], 0, 0);
    VSET(ydata->n_vec[1], 0, 0, max[2]);
    ret = rt_pattern(ydata, RT_PATTERN_RECT_ORTHOGRID);
    bu_free(ydata->n_vec, "y vec inputs");
    bu_free(ydata->n_p, "y p inputs");
    if (ret < 0) return 0;

    BU_GET(zdata, struct rt_pattern_data);
    VSET(zdata->center_pt, mid[0], mid[1], min[2] - 0.1 * min[2]);
    VSET(zdata->center_dir, 0, 0, 1);
    zdata->vn = 2;
    zdata->pn = 2;
    zdata->n_vec = (vect_t *)bu_calloc(zdata->vn + 1, sizeof(vect_t), "vects array");
    zdata->n_p = (fastf_t *)bu_calloc(zdata->pn + 1, sizeof(fastf_t), "params array");
    zdata->n_p[0] = tol;
    zdata->n_p[1] = tol;
    VSET(zdata->n_vec[0], max[0], 0, 0);
    VSET(zdata->n_vec[1], 0, max[1], 0);
    ret = rt_pattern(zdata, RT_PATTERN_RECT_ORTHOGRID);
    bu_free(zdata->n_vec, "x vec inputs");
    bu_free(zdata->n_p, "x p inputs");
    if (ret < 0) return 0;

    /* Consolidate the grids into a single ray array */
    {
	size_t i, j;
	rays = (fastf_t *)bu_calloc((xdata->ray_cnt + ydata->ray_cnt + zdata->ray_cnt + 1) * 6, sizeof(fastf_t), "rays");
	count = 0;
	for (i = 0; i < xdata->ray_cnt; i++) {
	    for (j = 0; j < 6; j++) {
		rays[6*count+j] = xdata->rays[6*i + j];
	    }
	    count++;
	}
	for (i = 0; i < ydata->ray_cnt; i++) {
	    for (j = 0; j < 6; j++) {
		rays[6*count+j] = ydata->rays[6*i + j];
	    }
	    count++;
	}
	for (i = 0; i < zdata->ray_cnt; i++) {
	    for (j = 0; j < 6; j++) {
		rays[6*count+j] = zdata->rays[6*i+j];
	    }
	    count++;
	}

    }
    bu_free(xdata->rays, "x rays");
    bu_free(ydata->rays, "y rays");
    bu_free(zdata->rays, "z rays");
    BU_PUT(xdata, struct rt_pattern_data);
    BU_PUT(ydata, struct rt_pattern_data);
    BU_PUT(zdata, struct rt_pattern_data);

    /* Per ray data containers */
    ray_results = (struct rayio_info *)bu_calloc(count + 1, sizeof(struct rayio_info), "rays");

/*
    bu_log("ray cnt: %d\n", count);
*/
    {
	int i;
	state = (struct rayio_container *)bu_calloc(ncpus+1, sizeof(struct rayio_container), "resources");
	for (i = 0; i < ncpus+1; i++) {
	    state[i].rtip = rtip;
	    state[i].ncpus = ncpus;
	    state[i].left_name = bu_strdup(outside_candidate);
	    state[i].right_name = bu_strdup(inside_candidate);
	    BU_GET(state[i].resp, struct resource);
	    rt_init_resource(state[i].resp, i, state->rtip);
	    state[i].rays_cnt = count;
	    state[i].rays = rays;
	    state[i].results = ray_results;
	}
	bu_parallel(rayio_gen_worker, ncpus, (void *)state);

	ret = 1;
	for (i = 0; i < count; i++) {
	    if (ray_results[i].right_hit_cnt) ret = 0;
	    if (ray_results[i].have_overlap && !ray_results[i].left_hit_cnt) ret = 0;
	}

	/* Free memory */
	for (i = 0; i < ncpus+1; i++) {
	    bu_free((void *)state[i].left_name, "left name");
	    bu_free((void *)state[i].right_name, "right name");
	    BU_PUT(state[i].resp, struct resource);
	}
	bu_free((void *)ray_results, "ray results");
	bu_free(state, "free state containers");
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

/*                       R A Y D I F F . C
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
/** @file raydiff.c
 *
 * Brief description
 *
 */
#include "common.h"

#include <string.h> /* for memset */

#include "vmath.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "raytrace.h"

struct diff_seg {
    point_t in_pt;
    point_t out_pt;
};

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
    fastf_t *rays;
    int rays_cnt;
};

#define RDIFF_ADD_DSEG(_ptbl, p1, p2) {\
    struct diff_seg *dseg; \
    BU_GET(dseg, struct diff_seg); \
    VMOVE(dseg->in_pt, p1); \
    VMOVE(dseg->out_pt, p2); \
    bu_ptbl_ins(_ptbl, (long *)dseg); \
}


HIDDEN int
raydiff_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    point_t in_pt, out_pt;
    struct partition *part;
    fastf_t part_len = 0.0;
    struct raydiff_container *state = (struct raydiff_container *)(ap->a_uptr);
    /*rt_pr_seg(segs);*/
    /*rt_pr_partitions(ap->a_rt_i, PartHeadp, "hits");*/

    for (part = PartHeadp->pt_forw; part != PartHeadp; part = part->pt_forw) {
	VJOIN1(in_pt, ap->a_ray.r_pt, part->pt_inhit->hit_dist, ap->a_ray.r_dir);
	VJOIN1(out_pt, ap->a_ray.r_pt, part->pt_outhit->hit_dist, ap->a_ray.r_dir);
	part_len = DIST_PT_PT(in_pt, out_pt);
	if (part_len > state->tol) {
	    state->have_diffs = 1;
	    if (state->left && !bu_strncmp(part->pt_regionp->reg_name+1, state->left_name, strlen(state->left_name))) {
		RDIFF_ADD_DSEG(state->left, in_pt, out_pt);
		bu_log("LEFT diff vol (%s) (len: %f): %g %g %g -> %g %g %g\n", part->pt_regionp->reg_name, part_len, V3ARGS(in_pt), V3ARGS(out_pt));
		continue;
	    }
	    if (state->right && !bu_strncmp(part->pt_regionp->reg_name+1, state->right_name, strlen(state->right_name))) {
		RDIFF_ADD_DSEG(state->right, in_pt, out_pt);
		bu_log("RIGHT diff vol (%s) (len: %f): %g %g %g -> %g %g %g\n", part->pt_regionp->reg_name, part_len, V3ARGS(in_pt), V3ARGS(out_pt));
		continue;
	    }
	    /* If we aren't collecting segments, we already have our final answer */
	    if (!state->left && !state->right) return 0;
	}
    }

    return 0;
}


HIDDEN int
raydiff_overlap(struct application *ap,
		struct partition *pp,
		struct region *reg1,
		struct region *reg2,
		struct partition *UNUSED(hp))
{
    point_t in_pt, out_pt;
    fastf_t overlap_len = 0.0;
    struct raydiff_container *state = (struct raydiff_container *)ap->a_uptr;

    VJOIN1(in_pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
    VJOIN1(out_pt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

    overlap_len = DIST_PT_PT(in_pt, out_pt);

    if (overlap_len > state->tol) {
	RDIFF_ADD_DSEG(state->both, in_pt, out_pt);
	bu_log("OVERLAP (%s and %s) (len: %f): %g %g %g -> %g %g %g\n", reg1->reg_name, reg2->reg_name, overlap_len, V3ARGS(in_pt), V3ARGS(out_pt));
    }

    return 0;
}


HIDDEN int
raydiff_miss(struct application *ap)
{
    RT_CK_APPLICATION(ap);

    return 0;
}


HIDDEN void
raydiff_gen_worker(int cpu, void *ptr)
{
    struct application ap;
    struct raydiff_container *state = &(((struct raydiff_container *)ptr)[cpu]);
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
	bu_log("start_ind (%d): %d\n", cpu, start_ind);
	bu_log("end_ind (%d): %d\n", cpu, end_ind);
    }

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = state->rtip;
    ap.a_hit = raydiff_hit;
    ap.a_miss = raydiff_miss;
    ap.a_overlap = raydiff_overlap;
    ap.a_onehit = 0;
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.a_resource = state->resp;
    ap.a_uptr = (void *)state;

    for (i = start_ind; i <= end_ind; i++) {
	VSET(ap.a_ray.r_pt, state->rays[6*i+0], state->rays[6*i+1], state->rays[6*i+2]);
	VSET(ap.a_ray.r_dir, state->rays[6*i+3], state->rays[6*i+4], state->rays[6*i+5]);
	rt_shootray(&ap);
    }
}


/* 0 = no difference within tolerance, 1 = difference >= tolerance */
int
analyze_raydiff(/* TODO - decide what to return.  Probably some sort of left, common, right segment sets.  See what rtcheck does... */
	struct db_i *dbip, const char *obj1, const char *obj2, struct bn_tol *tol)
{
    int ret;
    int count = 0;
    struct rt_i *rtip;
    int ncpus = bu_avail_cpus();
    point_t min, mid, max;
    struct rt_pattern_data *xdata, *ydata, *zdata;
    fastf_t *rays;
    struct raydiff_container *state;

    if (!dbip || !obj1 || !obj2 || !tol) return 0;

    rtip = rt_new_rti(dbip);
    if (rt_gettree(rtip, obj1) < 0) return -1;
    if (rt_gettree(rtip, obj2) < 0) return -1;
    rt_prep_parallel(rtip, 1);


    /* Now we've got the bounding box - set up the grids */
    VMOVE(min, rtip->mdl_min);
    VMOVE(max, rtip->mdl_max);
    VSET(mid, (max[0] - min[0])/2, (max[1] - min[1])/2, (max[2] - min[2])/2);

    BU_GET(xdata, struct rt_pattern_data);
    VSET(xdata->center_pt, min[0] - 0.1 * min[0], mid[1], mid[2]);
    VSET(xdata->center_dir, 1, 0, 0);
    xdata->vn = 2;
    xdata->pn = 2;
    xdata->n_vec = (vect_t *)bu_calloc(xdata->vn + 1, sizeof(vect_t), "vects array");
    xdata->n_p = (fastf_t *)bu_calloc(xdata->pn + 1, sizeof(fastf_t), "params array");
    xdata->n_p[0] = 50; /* TODO - get tolerances from caller */
    xdata->n_p[1] = 50;
    VSET(xdata->n_vec[0], 0, max[1], 0);
    VSET(xdata->n_vec[1], 0, 0, max[2]);
    ret = rt_pattern(xdata, RT_PATTERN_RECT_ORTHOGRID);
    bu_free(xdata->n_vec, "x vec inputs");
    bu_free(xdata->n_p, "x p inputs");
    if (ret < 0) return -1;


    BU_GET(ydata, struct rt_pattern_data);
    VSET(ydata->center_pt, mid[0], min[1] - 0.1 * min[1], mid[2]);
    VSET(ydata->center_dir, 0, 1, 0);
    ydata->vn = 2;
    ydata->pn = 2;
    ydata->n_vec = (vect_t *)bu_calloc(ydata->vn + 1, sizeof(vect_t), "vects array");
    ydata->n_p = (fastf_t *)bu_calloc(ydata->pn + 1, sizeof(fastf_t), "params array");
    ydata->n_p[0] = 50; /* TODO - get tolerances from caller */
    ydata->n_p[1] = 50;
    VSET(ydata->n_vec[0], max[0], 0, 0);
    VSET(ydata->n_vec[1], 0, 0, max[2]);
    ret = rt_pattern(ydata, RT_PATTERN_RECT_ORTHOGRID);
    bu_free(ydata->n_vec, "y vec inputs");
    bu_free(ydata->n_p, "y p inputs");
    if (ret < 0) return -1;

    BU_GET(zdata, struct rt_pattern_data);
    VSET(zdata->center_pt, mid[0], mid[1], min[2] - 0.1 * min[2]);
    VSET(zdata->center_dir, 0, 0, 1);
    zdata->vn = 2;
    zdata->pn = 2;
    zdata->n_vec = (vect_t *)bu_calloc(zdata->vn + 1, sizeof(vect_t), "vects array");
    zdata->n_p = (fastf_t *)bu_calloc(zdata->pn + 1, sizeof(fastf_t), "params array");
    zdata->n_p[0] = 50; /* TODO - get tolerances from caller */
    zdata->n_p[1] = 50;
    VSET(zdata->n_vec[0], max[0], 0, 0);
    VSET(zdata->n_vec[1], 0, max[1], 0);
    ret = rt_pattern(zdata, RT_PATTERN_RECT_ORTHOGRID);
    bu_free(zdata->n_vec, "x vec inputs");
    bu_free(zdata->n_p, "x p inputs");
    if (ret < 0) return -1;

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

    bu_log("ray cnt: %d\n", count);

    {
	int i, j;
	ncpus = 2;
	state = (struct raydiff_container *)bu_calloc(ncpus+1, sizeof(struct raydiff_container), "resources");
	for (i = 0; i < ncpus+1; i++) {
	    state[i].rtip = rtip;
	    state[i].tol = 0.5;
	    state[i].ncpus = ncpus;
	    state[i].left_name = bu_strdup(obj1);
	    state[i].right_name = bu_strdup(obj2);
	    BU_GET(state[i].resp, struct resource);
	    rt_init_resource(state[i].resp, i, state->rtip);
	    BU_GET(state[i].left, struct bu_ptbl);
	    bu_ptbl_init(state[i].left, 64, "left solid hits");
	    BU_GET(state[i].both, struct bu_ptbl);
	    bu_ptbl_init(state[i].both, 64, "hits on both solids");
	    BU_GET(state[i].right, struct bu_ptbl);
	    bu_ptbl_init(state[i].right, 64, "right solid hits");
	    state[i].rays_cnt = count;
	    state[i].rays = rays;
	}
	bu_parallel(raydiff_gen_worker, ncpus, (void *)state);

	/* Collect and print all of the results */
	for (i = 0; i < ncpus+1; i++) {
	    for (j = 0; j < (int)BU_PTBL_LEN(state[i].left); j++) {
		struct diff_seg *dseg = (struct diff_seg *)BU_PTBL_GET(state[i].left, j);
		bu_log("Result: LEFT diff vol (%s): %g %g %g -> %g %g %g\n", obj1, V3ARGS(dseg->in_pt), V3ARGS(dseg->out_pt));
	    }
	    for (j = 0; j < (int)BU_PTBL_LEN(state[i].both); j++) {
		struct diff_seg *dseg = (struct diff_seg *)BU_PTBL_GET(state[i].both, j);
		bu_log("Result: BOTH): %g %g %g -> %g %g %g\n", V3ARGS(dseg->in_pt), V3ARGS(dseg->out_pt));
	    }
	    for (j = 0; j < (int)BU_PTBL_LEN(state[i].right); j++) {
		struct diff_seg *dseg = (struct diff_seg *)BU_PTBL_GET(state[i].right, j);
		bu_log("Result: RIGHT diff vol (%s): %g %g %g -> %g %g %g\n", obj2, V3ARGS(dseg->in_pt), V3ARGS(dseg->out_pt));
	    }
	}

	/* Free results */
	for (i = 0; i < ncpus+1; i++) {
	    for (j = 0; j < (int)BU_PTBL_LEN(state[i].left); j++) {
		struct diff_seg *dseg = (struct diff_seg *)BU_PTBL_GET(state[i].left, j);
		BU_PUT(dseg, struct diff_seg);
	    }
	    bu_ptbl_free(state[i].left);
	    BU_PUT(state[i].left, struct diff_seg);
	    for (j = 0; j < (int)BU_PTBL_LEN(state[i].both); j++) {
		struct diff_seg *dseg = (struct diff_seg *)BU_PTBL_GET(state[i].both, j);
		BU_PUT(dseg, struct diff_seg);
	    }
	    bu_ptbl_free(state[i].both);
	    BU_PUT(state[i].both, struct diff_seg);
	    for (j = 0; j < (int)BU_PTBL_LEN(state[i].right); j++) {
		struct diff_seg *dseg = (struct diff_seg *)BU_PTBL_GET(state[i].right, j);
		BU_PUT(dseg, struct diff_seg);
	    }
	    bu_ptbl_free(state[i].right);
	    BU_PUT(state[i].right, struct diff_seg);

	    bu_free((void *)state[i].left_name, "left name");
	    bu_free((void *)state[i].right_name, "right name");
	    BU_PUT(state[i].resp, struct resource);
	}
	bu_free(state, "free state containers");
    }
    return 0;
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

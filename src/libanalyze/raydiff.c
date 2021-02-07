/*                       R A Y D I F F . C
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
#include "bn/mat.h"
#include "bu/time.h"
#include "raytrace.h"
#include "analyze.h"
#include "./analyze_private.h"

void
analyze_raydiff_results_init(struct analyze_raydiff_results **results)
{
    if (!results) return;
    BU_GET((*results), struct analyze_raydiff_results);
    BU_GET((*results)->left, struct bu_ptbl);
    BU_GET((*results)->right, struct bu_ptbl);
    BU_GET((*results)->both, struct bu_ptbl);
    bu_ptbl_init((*results)->left, 64, "init left");
    bu_ptbl_init((*results)->right, 64, "init right");
    bu_ptbl_init((*results)->both, 64, "init both");
}

void
analyze_raydiff_results_free(struct analyze_raydiff_results *results)
{
    size_t i;
    if (!results) return;
    if (results->left) {
	for (i = 0; i < BU_PTBL_LEN(results->left); i++) {
	    struct diff_seg *dseg = (struct diff_seg *)BU_PTBL_GET(results->left, i);
	    BU_PUT(dseg, struct diff_seg);
	}
	bu_ptbl_free(results->left);
	BU_PUT(results->left, struct diff_seg);
    }
    if (results->both) {
	for (i = 0; i < BU_PTBL_LEN(results->both); i++) {
	    struct diff_seg *dseg = (struct diff_seg *)BU_PTBL_GET(results->both, i);
	    BU_PUT(dseg, struct diff_seg);
	}
	bu_ptbl_free(results->both);
	BU_PUT(results->both, struct diff_seg);
    }
    if (results->right) {
	for (i = 0; i < BU_PTBL_LEN(results->right); i++) {
	    struct diff_seg *dseg = (struct diff_seg *)BU_PTBL_GET(results->right, i);
	    BU_PUT(dseg, struct diff_seg);
	}
	bu_ptbl_free(results->right);
	BU_PUT(results->right, struct diff_seg);
    }
    BU_PUT(results, struct analyze_raydiff_results);
}

struct raydiff_container {
    fastf_t tol;
    int have_diffs;
    const char *left_name;
    const char *right_name;
    struct bu_ptbl *left;
    struct bu_ptbl *both;
    struct bu_ptbl *right;
    int cnt;
    struct bu_ptbl *test;
};

#define RDIFF_ADD_DSEG(_ptbl, p1, p2) {\
    struct diff_seg *dseg; \
    BU_GET(dseg, struct diff_seg); \
    VMOVE(dseg->in_pt, p1); \
    VMOVE(dseg->out_pt, p2); \
    VMOVE(dseg->ray.r_pt, ap->a_ray.r_pt); \
    VMOVE(dseg->ray.r_dir, ap->a_ray.r_dir); \
    dseg->valid = 0; \
    bu_ptbl_ins(_ptbl, (long *)dseg); \
}

struct xray *
diff_ray(void *container)
{
    struct diff_seg *d = (struct diff_seg *)container;
    return &(d->ray);
}

int *
diff_flag(void *container)
{
    struct diff_seg *d = (struct diff_seg *)container;
    return &(d->valid);
}

HIDDEN int
raydiff_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    point_t in_pt, out_pt;
    struct partition *part;
    fastf_t part_len = 0.0;
    struct rt_gen_worker_vars *s = (struct rt_gen_worker_vars *)(ap->a_uptr);
    struct raydiff_container *state = (struct raydiff_container *)(s->ptr);

    for (part = PartHeadp->pt_forw; part != PartHeadp; part = part->pt_forw) {
	VJOIN1(in_pt, ap->a_ray.r_pt, part->pt_inhit->hit_dist, ap->a_ray.r_dir);
	VJOIN1(out_pt, ap->a_ray.r_pt, part->pt_outhit->hit_dist, ap->a_ray.r_dir);
	part_len = DIST_PNT_PNT(in_pt, out_pt);
	if (part_len > state->tol) {
	    state->have_diffs = 1;
	    if (state->left && !bu_strncmp(part->pt_regionp->reg_name+1, state->left_name, strlen(state->left_name))) {
		RDIFF_ADD_DSEG(state->left, in_pt, out_pt);
		/*bu_log("LEFT diff vol (%s) (len: %f): %g %g %g -> %g %g %g\n", part->pt_regionp->reg_name, part_len, V3ARGS(in_pt), V3ARGS(out_pt));*/
		continue;
	    }
	    if (state->right && !bu_strncmp(part->pt_regionp->reg_name+1, state->right_name, strlen(state->right_name))) {
		RDIFF_ADD_DSEG(state->right, in_pt, out_pt);
		/*bu_log("RIGHT diff vol (%s) (len: %f): %g %g %g -> %g %g %g\n", part->pt_regionp->reg_name, part_len, V3ARGS(in_pt), V3ARGS(out_pt));*/
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
		struct region *UNUSED(reg1),
		struct region *UNUSED(reg2),
		struct partition *UNUSED(hp))
{
    point_t in_pt, out_pt;
    fastf_t overlap_len = 0.0;
    struct rt_gen_worker_vars *s = (struct rt_gen_worker_vars *)(ap->a_uptr);
    struct raydiff_container *state = (struct raydiff_container *)(s->ptr);

    VJOIN1(in_pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
    VJOIN1(out_pt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

    overlap_len = DIST_PNT_PNT(in_pt, out_pt);

    if (overlap_len > state->tol) {
	RDIFF_ADD_DSEG(state->both, in_pt, out_pt);
	/*bu_log("OVERLAP (%s and %s) (len: %f): %g %g %g -> %g %g %g\n", reg1->reg_name, reg2->reg_name, overlap_len, V3ARGS(in_pt), V3ARGS(out_pt));*/
    }

    return 0;
}


HIDDEN int
raydiff_miss(struct application *ap)
{
    RT_CK_APPLICATION(ap);

    return 0;
}


/* 0 = no difference within tolerance, 1 = difference >= tolerance */
int
analyze_raydiff(struct analyze_raydiff_results **results, struct db_i *dbip,
       const char *left, const char *right, struct bn_tol *tol, int solidcheck)
{
    int ret, i, j;
    fastf_t oldtime, currtime;
    int ind = 0;
    int count = 0;
    struct rt_i *rtip;
    int ncpus = bu_avail_cpus();
    fastf_t *rays;
    struct rt_gen_worker_vars *state = (struct rt_gen_worker_vars *)bu_calloc(ncpus+1, sizeof(struct rt_gen_worker_vars ), "state");
    struct raydiff_container *local_state = (struct raydiff_container *)bu_calloc(ncpus+1, sizeof(struct raydiff_container), "local state");
    struct bu_ptbl test_tbl = BU_PTBL_INIT_ZERO;
    struct resource *resp = (struct resource *)bu_calloc(ncpus+1, sizeof(struct resource), "resources");

    if (!dbip || !left || !right|| !tol || ncpus == 0) {
	ret = 0;
	goto memfree;
    }

    oldtime = bu_gettime();

    rtip = rt_new_rti(dbip);

    for (i = 0; i < ncpus+1; i++) {
	/* standard */
	state[i].rtip = rtip;
	state[i].fhit = raydiff_hit;
	state[i].fmiss = raydiff_miss;
	state[i].foverlap = raydiff_overlap;
	state[i].resp = &resp[i];
	state[i].ind_src = &ind;
	rt_init_resource(state[i].resp, i, rtip);
	/* local */
	local_state[i].tol = 0.5;
	local_state[i].left_name = bu_strdup(left);
	local_state[i].right_name = bu_strdup(right);
	BU_GET(local_state[i].left, struct bu_ptbl);
	bu_ptbl_init(local_state[i].left, 64, "left solid hits");
	BU_GET(local_state[i].both, struct bu_ptbl);
	bu_ptbl_init(local_state[i].both, 64, "hits on both solids");
	BU_GET(local_state[i].right, struct bu_ptbl);
	bu_ptbl_init(local_state[i].right, 64, "right solid hits");
	state[i].ptr = (void *)&(local_state[i]);
    }
#if 0
    {
        /*
	 * Really weird behavior sometimes - builds a tree that raytraces
	 * very slowly.  After switching out this version with the individual
	 * rt_gettree version below, then switching back, everything works???
         */
	const char *argv[2];
	argv[0] = left;
	argv[1] = right;
	if (rt_gettrees(rtip, 2, argv, ncpus) < 0) {
	    ret = -1;
	    goto memfree;
	}
    }
#endif
#if 1
    if (rt_gettree(rtip, left) < 0) return -1;
    if (rt_gettree(rtip, right) < 0) return -1;
#endif

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

    /* If we want to do a solidity check, do it here. */
    if (solidcheck) {
	for (i = 0; i < ncpus+1; i++) {
	    for (j = 0; j < (int)BU_PTBL_LEN(local_state[i].left); j++) {
		bu_ptbl_ins(&test_tbl, BU_PTBL_GET(local_state[i].left, j));
	    }
	    for (j = 0; j < (int)BU_PTBL_LEN(local_state[i].right); j++) {
		bu_ptbl_ins(&test_tbl, BU_PTBL_GET(local_state[i].right, j));
	    }
	}
	analyze_seg_filter(&test_tbl, &diff_ray, &diff_flag, rtip, resp, 0.5, ncpus);
    } else {
	/* Not restricting to solids, all are valid */
	for (i = 0; i < ncpus+1; i++) {
	    for (j = 0; j < (int)BU_PTBL_LEN(local_state[i].left); j++) {
		struct diff_seg *d = (struct diff_seg *)BU_PTBL_GET(local_state[i].left, j);
		d->valid = 1;
	    }
	    for (j = 0; j < (int)BU_PTBL_LEN(local_state[i].right); j++) {
		struct diff_seg *d = (struct diff_seg *)BU_PTBL_GET(local_state[i].right, j);
		d->valid = 1;
	    }
	}
    }
    currtime = bu_gettime();
    bu_log("rt time: %.1f\n", (currtime - oldtime)/1e6);

    /* Collect and print all of the results */
    if (results) analyze_raydiff_results_init(results);
    for (i = 0; i < ncpus+1; i++) {
	for (j = 0; j < (int)BU_PTBL_LEN(local_state[i].left); j++) {
	    struct diff_seg *d = (struct diff_seg *)BU_PTBL_GET(local_state[i].left, j);
	    if (d->valid) {
		if (results) bu_ptbl_ins((*results)->left, (long *)d);
		ret = 1;
	    }
	}
	if (results) {
	    for (j = 0; j < (int)BU_PTBL_LEN(local_state[i].both); j++) {
		bu_ptbl_ins((*results)->both, BU_PTBL_GET(local_state[i].both, j));
	    }
	}
	for (j = 0; j < (int)BU_PTBL_LEN(local_state[i].right); j++) {
	    struct diff_seg *d = (struct diff_seg *)BU_PTBL_GET(local_state[i].right, j);
	    if (d->valid) {
		if (results) bu_ptbl_ins((*results)->right, (long *)d);
		ret = 1;
	    }
	}
    }


memfree:
    /* Free memory not stored in tables */
    for (i = 0; i < ncpus+1; i++) {
	if (local_state[i].left != NULL) BU_PUT(local_state[i].left, struct bu_ptbl);
	if (local_state[i].right != NULL) BU_PUT(local_state[i].right, struct bu_ptbl);
	if (local_state[i].both != NULL) BU_PUT(local_state[i].both, struct bu_ptbl);
	if (local_state[i].left_name)  bu_free((void *)local_state[i].left_name, "left name");
	if (local_state[i].right_name) bu_free((void *)local_state[i].right_name, "right name");
	/*BU_PUT(state[i].resp, struct resource);*/
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

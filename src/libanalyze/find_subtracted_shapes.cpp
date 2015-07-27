#include "common.h"

#include <string.h> /* for memset */

#include "vmath.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "bn/mat.h"
#include "raytrace.h"
#include "analyze.h"

struct subtract_analyze_container {
    struct rt_i *rtip;
    struct resource *resp;
    int ray_dir;
    int ncpus;
    fastf_t tol;
    int have_diffs;
    const char *left_name;
    const char *right_name;
    fastf_t *rays;
    int rays_cnt;
    int cnt;
    struct bu_ptbl *test;
};

HIDDEN int
subtract_analyze_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    point_t in_pt, out_pt;
    struct partition *part;
    fastf_t part_len = 0.0;
    struct subtract_analyze_container *state = (struct subtract_analyze_container *)(ap->a_uptr);
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
subtract_analyze_overlap(struct application *ap,
		struct partition *pp,
		struct region *UNUSED(reg1),
		struct region *UNUSED(reg2),
		struct partition *UNUSED(hp))
{
    point_t in_pt, out_pt;
    fastf_t overlap_len = 0.0;
    struct subtract_analyze_container *state = (struct subtract_analyze_container *)ap->a_uptr;

    VJOIN1(in_pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
    VJOIN1(out_pt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

    overlap_len = DIST_PT_PT(in_pt, out_pt);

    if (overlap_len > state->tol) {
	RDIFF_ADD_DSEG(state->both, in_pt, out_pt);
	/*bu_log("OVERLAP (%s and %s) (len: %f): %g %g %g -> %g %g %g\n", reg1->reg_name, reg2->reg_name, overlap_len, V3ARGS(in_pt), V3ARGS(out_pt));*/
    }

    return 0;
}


HIDDEN int
subtract_analyze_miss(struct application *ap)
{
    RT_CK_APPLICATION(ap);

    return 0;
}


HIDDEN void
subtract_analyze_gen_worker(int cpu, void *ptr)
{
    struct application ap;
    struct subtract_analyze_container *state = &(((struct subtract_analyze_container *)ptr)[cpu]);
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
	/*
	bu_log("start_ind (%d): %d\n", cpu, start_ind);
	bu_log("end_ind (%d): %d\n", cpu, end_ind);
	*/
    }

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = state->rtip;
    ap.a_hit = subtract_analyze_hit;
    ap.a_miss = subtract_analyze_miss;
    ap.a_overlap = subtract_analyze_overlap;
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

/* the inputs are a "control" object which will provide the "ground truth" ray partitions against
 * which candidates will be evaluated, the "target" csg comb which may need subtractions
 * added to its definition, a table of candidate objects which may or may not need to be
 * subtracted from comb, the database instance pointer, and tolerance information*/
int
analyze_find_subtracted(struct directory *comb_dp, struct db_i *dbip,
       const char *control, const struct bu_ptbl *candidates, struct bn_tol *tol)
{

    // Raytrace the comb_dp as-is, collect all hit partitions and all rays associated with them
    // if the partition structs don't retain that information.


    // Raytrace the control object with all the rays that garnered hits on comb_dp.  Save any
    // hits (and rays) that share just a start or just and end point with one of the comb_dp partitions,
    // but not hits that produce identical segments.

    // If there are no rays from step 2, no additional subtractions are needed and we are done.

    // If there are rays from step 2, proceed to test those rays against the candidates to see
    // if the partitions generated by the one or more candidates correspond to "gaps" missing
    // from the comb_dp.
    //
    // If solids are found that provide the correct gaps, add them to comb_db's definition as new
    // subtractions
    //
    // Note that this step should be done before the final subtract_analyze verification.

    int ret, i, j;
    int count = 0;
    struct rt_i *rtip;
    int ncpus = bu_avail_cpus();
    point_t min, mid, max;
    struct rt_pattern_data *xdata, *ydata, *zdata;
    fastf_t *rays;
    struct subtract_analyze_container *state = (struct subtract_analyze_container *)bu_calloc(ncpus+1, sizeof(struct subtract_analyze_container), "resources");
    struct bu_ptbl test_tbl = BU_PTBL_INIT_ZERO;

    if (!dbip || !left || !right|| !tol) {
	ret = 0;
	goto memfree;
    }

    rtip = rt_new_rti(dbip);

    for (i = 0; i < ncpus+1; i++) {
	state[i].rtip = rtip;
	state[i].tol = 0.5;
	state[i].ncpus = ncpus;
	state[i].left_name = bu_strdup(left);
	state[i].right_name = bu_strdup(right);
	BU_GET(state[i].resp, struct resource);
	rt_init_resource(state[i].resp, i, state->rtip);
	BU_GET(state[i].left, struct bu_ptbl);
	bu_ptbl_init(state[i].left, 64, "left solid hits");
	BU_GET(state[i].both, struct bu_ptbl);
	bu_ptbl_init(state[i].both, 64, "hits on both solids");
	BU_GET(state[i].right, struct bu_ptbl);
	bu_ptbl_init(state[i].right, 64, "right solid hits");
    }
    {
	const char *argv[2];
	argv[0] = left;
	argv[1] = right;
	if (rt_gettrees(rtip, 2, argv, ncpus) < 0) {
	    ret = -1;
	    goto memfree;
	}
    }
    /*
    if (rt_gettree(rtip, left) < 0) return -1;
    if (rt_gettree(rtip, right) < 0) return -1;
    */
    rt_prep_parallel(rtip, ncpus);


    count = analyze_get_bbox_rays(&rays, rtip->mdl_min, rtip->mdl_max, tol);

    ret = 0;
    for (i = 0; i < ncpus+1; i++) {
	state[i].rays_cnt = count;
	state[i].rays = rays;
    }

    bu_parallel(subtract_analyze_gen_worker, ncpus, (void *)state);

    /* If we want to do a solidity check, do it here. */
    if (solidcheck) {
	for (i = 0; i < ncpus+1; i++) {
	    for (j = 0; j < (int)BU_PTBL_LEN(state[i].left); j++) {
		bu_ptbl_ins(&test_tbl, BU_PTBL_GET(state[i].left, j));
	    }
	    for (j = 0; j < (int)BU_PTBL_LEN(state[i].right); j++) {
		bu_ptbl_ins(&test_tbl, BU_PTBL_GET(state[i].right, j));
	    }
	}
	for (i = 0; i < ncpus+1; i++) {
	    state[i].test = &test_tbl;
	}
	bu_parallel(raycheck_gen_worker, ncpus, (void *)state);
    } else {
	/* Not restricting to solids, all are valid */
	for (i = 0; i < ncpus+1; i++) {
	    for (j = 0; j < (int)BU_PTBL_LEN(state[i].left); j++) {
		struct diff_seg *d = (struct diff_seg *)BU_PTBL_GET(state[i].left, j);
		d->valid = 1;
	    }
	    for (j = 0; j < (int)BU_PTBL_LEN(state[i].right); j++) {
		struct diff_seg *d = (struct diff_seg *)BU_PTBL_GET(state[i].right, j);
		d->valid = 1;
	    }
	}
    }

    /* Collect and print all of the results */
    if (results) analyze_find_subtracted_results_init(results);
    for (i = 0; i < ncpus+1; i++) {
	for (j = 0; j < (int)BU_PTBL_LEN(state[i].left); j++) {
	    struct diff_seg *d = (struct diff_seg *)BU_PTBL_GET(state[i].left, j);
	    if (d->valid) {
		if (results) bu_ptbl_ins((*results)->left, (long *)d);
		ret = 1;
	    }
	}
	if (results) {
	    for (j = 0; j < (int)BU_PTBL_LEN(state[i].both); j++) {
		bu_ptbl_ins((*results)->both, BU_PTBL_GET(state[i].both, j));
	    }
	}
	for (j = 0; j < (int)BU_PTBL_LEN(state[i].right); j++) {
	    struct diff_seg *d = (struct diff_seg *)BU_PTBL_GET(state[i].right, j);
	    if (d->valid) {
		if (results) bu_ptbl_ins((*results)->right, (long *)d);
		ret = 1;
	    }
	}
    }


memfree:
    /* Free memory not stored in tables */
    bu_free(xdata->rays, "x rays");
    bu_free(ydata->rays, "y rays");
    bu_free(zdata->rays, "z rays");
    BU_PUT(xdata, struct rt_pattern_data);
    BU_PUT(ydata, struct rt_pattern_data);
    BU_PUT(zdata, struct rt_pattern_data);
    for (i = 0; i < ncpus+1; i++) {
	BU_PUT(state[i].left, struct bu_ptbl);
	BU_PUT(state[i].right, struct bu_ptbl);
	BU_PUT(state[i].both, struct bu_ptbl);
	bu_free((void *)state[i].left_name, "left name");
	bu_free((void *)state[i].right_name, "right name");
	BU_PUT(state[i].resp, struct resource);
    }
    bu_free(state, "free state containers");

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

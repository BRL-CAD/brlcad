#include "common.h"

#include <string.h> /* for memset */
#include <vector>

extern "C" {
#include "vmath.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "bn/mat.h"
#include "raytrace.h"
#include "analyze.h"
#include "./analyze_private.h"
}

#define MINIMUM_RAYS_FOR_DECISION 500

HIDDEN void
plot_min_partitions(struct bu_ptbl *p, const char *cname)
{
    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_vls name2 = BU_VLS_INIT_ZERO;
    bu_vls_printf(&name, "hits_%s", cname);
    bu_vls_printf(&name2, "gaps_%s", cname);
    FILE* plot_file = fopen(bu_vls_addr(&name), "w");
    FILE* plot_file_gaps = fopen(bu_vls_addr(&name2), "w");
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    pl_color(plot_file, r, g, b);
    int r2 = int(256*drand48() + 1.0);
    int g2 = int(256*drand48() + 1.0);
    int b2 = int(256*drand48() + 1.0);
    pl_color(plot_file_gaps, r2, g2, b2);
    for (size_t i = 0; i < BU_PTBL_LEN(p); i++) {
	struct minimal_partitions *mp = (struct minimal_partitions *)BU_PTBL_GET(p, i);
	for (int j = 0; j < mp->hit_cnt * 2; j=j+2) {
	    point_t p1, p2;
	    VJOIN1(p1, mp->ray.r_pt, mp->hits[j], mp->ray.r_dir);
	    VJOIN1(p2, mp->ray.r_pt, mp->hits[j+1], mp->ray.r_dir);
	    pdv_3move(plot_file, p1);
	    pdv_3cont(plot_file, p2);
	}
	for (int j = 0; j < mp->gap_cnt * 2; j=j+2) {
	    point_t p1, p2;
	    VJOIN1(p1, mp->ray.r_pt, mp->gaps[j], mp->ray.r_dir);
	    VJOIN1(p2, mp->ray.r_pt, mp->gaps[j+1], mp->ray.r_dir);
	    pdv_3move(plot_file_gaps, p1);
	    pdv_3cont(plot_file_gaps, p2);
	}

    }
    fclose(plot_file);
    fclose(plot_file_gaps);
}


HIDDEN int
find_missing_gaps(struct bu_ptbl *missing, struct bu_ptbl *p_brep, struct bu_ptbl *p_comb, struct subbrep_object_data *candidate, int max_cnt)
{
    ON_BoundingBox *bbox = candidate->bbox;

    if (!missing || !p_brep || !p_comb || !candidate || max_cnt == 0) return 0;

    //1. Set up pointer arrays for both partition lists
    struct minimal_partitions **brep = (struct minimal_partitions **)bu_calloc(max_cnt, sizeof(struct minimal_partitions *), "array1");
    struct minimal_partitions **comb = (struct minimal_partitions **)bu_calloc(max_cnt, sizeof(struct minimal_partitions *), "array2");

    //2. Iterate over the tbls and assign each array it's minimal partition pointer
    //   based on the index stored in the minimal_partitions struct.
    for (size_t i = 0; i < BU_PTBL_LEN(p_brep); i++) {
	struct minimal_partitions *p = (struct minimal_partitions *)BU_PTBL_GET(p_brep, i);
	brep[p->index] = p;
    }
    for (size_t i = 0; i < BU_PTBL_LEN(p_comb); i++) {
	struct minimal_partitions *p = (struct minimal_partitions *)BU_PTBL_GET(p_comb, i);
	comb[p->index] = p;
    }

    //3. Find brep gaps that are within comb solid segments.  These are the "missing" gaps.
    for (int i = 0; i < max_cnt; i++) {
	if (brep[i] && comb[i]) {
	    std::vector<fastf_t> missing_gaps;
	    if (brep[i]->gap_cnt > 0) {
		// iterate over the gaps and see if any of them are inside comb solids on this ray
		for (int j = 0; j < brep[i]->gap_cnt; j++) {
		    fastf_t g1 = brep[i]->gaps[2*j];
		    fastf_t g2 = brep[i]->gaps[2*j+1];
		    for (int k = 0; k < comb[i]->hit_cnt; k++) {
			fastf_t h1 = comb[i]->hits[2*k];
			fastf_t h2 = comb[i]->hits[2*k+1];
			// If the gap ends before the hit starts, we're done
			if (g2 < h1) break;
			// If the gap starts after the hit ends, skip to the next hit
			if (g1 > h2) continue;
			// If we've got some sort of overlap between a gap and a hit, something needs
			// to be trimmed away - see if it's from this solid and has a reasonable length
			if ((NEAR_ZERO(g1 - h1, VUNITIZE_TOL) || h1 < g1) || (NEAR_ZERO(g2 - h2, VUNITIZE_TOL) || h2 > g2)) {
			    fastf_t miss1, miss2;
			    // All we need trimmed away from this comb is the portion of the positive volume
			    // being contributed by the comb - if the gap is bigger than the hit, the rest of
			    // the removal from the parent is handled elsewhere
			    if (g1 < h1 || NEAR_ZERO(g1 - h1, VUNITIZE_TOL)){
				miss1 = h1;
			    } else {
				miss1 = g1;
			    }
			    if (g2 > h2 || NEAR_ZERO(g2 - h2, VUNITIZE_TOL)){
				miss2 = h2;
			    } else {
				miss2 = g2;
			    }
			    if (miss2 - miss1 > VUNITIZE_TOL) {
				ON_3dPoint m1, m2;
				point_t p1, p2;
				VJOIN1(p1, brep[i]->ray.r_pt, miss1, brep[i]->ray.r_dir);
				VJOIN1(p2, brep[i]->ray.r_pt, miss2, brep[i]->ray.r_dir);
				// TODO - probably a better way to make this assignment...
				m1.x = p1[0];
				m1.y = p1[1];
				m1.z = p1[2];
				m2.x = p2[0];
				m2.y = p2[1];
				m2.z = p2[2];
				if (bbox->IsPointIn(m1) || bbox->IsPointIn(m2)) {
				    missing_gaps.push_back(miss1);
				    missing_gaps.push_back(miss2);
				}
			    }
			}
		    }
		}
	    }
	    if (missing_gaps.size() > 0) {
		std::vector<fastf_t>::iterator f_it;
		struct minimal_partitions *np;
		BU_GET(np, struct minimal_partitions);
		VMOVE(np->ray.r_pt, brep[i]->ray.r_pt);
		VMOVE(np->ray.r_dir, brep[i]->ray.r_dir);
		np->index = brep[i]->index;
		np->gap_cnt = (int)(missing_gaps.size()/2);
		np->gaps = (fastf_t *)bu_calloc(np->gap_cnt * 2, sizeof(fastf_t), "missing gaps");
		for (std::vector<fastf_t>::size_type vind = 0; vind != missing_gaps.size(); vind++) {
		    np->gaps[vind] = missing_gaps[vind];
		}
		bu_ptbl_ins(missing, (long *)np);
	    }
	}
    }

    return BU_PTBL_LEN(missing);
}

HIDDEN int
subtraction_decision(struct bu_ptbl *missing, struct bu_ptbl *candidate, int max_cnt)
{
    if (!missing || !candidate || max_cnt == 0) return 0;

    //1. Set up pointer arrays for both partition lists
    struct minimal_partitions **mp = (struct minimal_partitions **)bu_calloc(max_cnt, sizeof(struct minimal_partitions *), "array1");
    struct minimal_partitions **cp = (struct minimal_partitions **)bu_calloc(max_cnt, sizeof(struct minimal_partitions *), "array2");

    //2. Iterate over the tbls and assign each array it's minimal partition pointer
    //   based on the index stored in the minimal_partitions struct.
    for (size_t i = 0; i < BU_PTBL_LEN(missing); i++) {
	struct minimal_partitions *p = (struct minimal_partitions *)BU_PTBL_GET(missing, i);
	mp[p->index] = p;
    }
    for (size_t i = 0; i < BU_PTBL_LEN(candidate); i++) {
	struct minimal_partitions *p = (struct minimal_partitions *)BU_PTBL_GET(candidate, i);
	cp[p->index] = p;
    }

    //3. If the candidate has a hit entry and an exit that correspond to a missing gap entry and exit (not necessarily
    //   in pairs, because there may be intruding positive volumes in the final brep) it's a subtraction.
    std::vector<fastf_t> entry_match;
    std::vector<fastf_t> exit_match;
    for (int i = 0; i < max_cnt; i++) {
	if (mp[i] && cp[i]) {
	    if (cp[i]->hit_cnt > 0) {
		// iterate over the entry hits
		for (int j = 0; j < cp[i]->hit_cnt*2; j=j+2) {
		    fastf_t h = cp[i]->hits[j];
		    //point_t hp;
		    //VJOIN1(hp, cp->ray.r_pt, cp->hits[j], cp->ray.r_dir);
		    for (int k = 0; k < mp[i]->gap_cnt*2; k=k+2) {
			fastf_t g = mp[i]->gaps[k];
			if (h < g) continue;
			//point_t gp;
			//VJOIN1(mp, mp->ray.r_pt, mp->gaps[k], mp->ray.r_dir);
			if (NEAR_ZERO(g - h,  VUNITIZE_TOL)) {
			    entry_match.push_back(h);
			}
			if (g > h) break;
		    }
		}
		// iterate over the exit hits
		for (int j = 1; j < cp[i]->hit_cnt*2; j=j+2) {
		    fastf_t h = cp[i]->hits[j];
		    //point_t hp;
		    //VJOIN1(hp, cp->ray.r_pt, cp->hits[j], cp->ray.r_dir);
		    for (int k = 1; k < mp[i]->gap_cnt*2; k=k+2) {
			fastf_t g = mp[i]->gaps[k];
			if (h < g) continue;
			//point_t gp;
			//VJOIN1(mp, mp->ray.r_pt, mp->gaps[k], mp->ray.r_dir);
			if (NEAR_ZERO(g - h, VUNITIZE_TOL)) {
			    exit_match.push_back(h);
			}
			if (g > h) break;
		    }
		}
	    }
	}
    }
bu_log("missing size: %d, entry size: %d, exit size: %d\n", BU_PTBL_LEN(missing), (int)(entry_match.size()), (int)(exit_match.size()));
    return ((size_t)((entry_match.size() + exit_match.size())/2) > 0) ? 1 : 0;
}

HIDDEN int
minimum_partitions_bbox(struct bu_ptbl *missing, point_t *bmin, point_t *bmax)
{
    if (!missing || !bmin || !bmax || BU_PTBL_LEN(missing) == 0) return 0;

    ON_BoundingBox bbox;
    bbox.m_min.x = ON_DBL_MAX;
    bbox.m_min.y = ON_DBL_MAX;
    bbox.m_min.z = ON_DBL_MAX;
    bbox.m_max.x = -ON_DBL_MAX;
    bbox.m_max.y = -ON_DBL_MAX;
    bbox.m_max.z = -ON_DBL_MAX;

    for (size_t i = 0; i < BU_PTBL_LEN(missing); i++) {
	struct minimal_partitions *mp = (struct minimal_partitions *)BU_PTBL_GET(missing, i);
	for (int j = 0; j < mp->gap_cnt; j++) {
	    ON_3dPoint p;
	    point_t p1;
	    VJOIN1(p1, mp->ray.r_pt, mp->gaps[j], mp->ray.r_dir);
	    p.x = p1[0];
	    p.y = p1[1];
	    p.z = p1[2];
	    bbox.Set(p, true);
	}
    }

    VMOVE((*bmin), bbox.Min());
    VMOVE((*bmax), bbox.Max());

    return 1;
}

HIDDEN int
refine_missing_rays(struct bu_ptbl *new_missing, fastf_t **candidate_rays, int *ray_cnt, struct bu_ptbl *old_missing,
       	const char *pbrep, struct rt_gen_worker_vars *pbrep_rtvars,
       	const char *curr_comb, struct rt_gen_worker_vars *ccomb_vars,
	struct subbrep_object_data *candidate, int pcpus, struct rt_wdb *wdbp, int depth)
{
    if (!new_missing || !candidate_rays || !ray_cnt || (*ray_cnt) == 0 || !old_missing) return 0;
    if (!pbrep || !pbrep_rtvars || !curr_comb || !ccomb_vars || !candidate || !wdbp) return 0;

    point_t bmin, bmax;
    fastf_t *c_rays = NULL;
    fastf_t mult = depth * 2 + 10;

    struct bu_ptbl o_brep_results = BU_PTBL_INIT_ZERO;
    struct bu_ptbl curr_comb_results = BU_PTBL_INIT_ZERO;

    (void)minimum_partitions_bbox(old_missing, &bmin, &bmax);

    fastf_t x_dist = fabs(bmax[0] - bmin[0]);
    fastf_t y_dist = fabs(bmax[1] - bmin[1]);
    fastf_t z_dist = fabs(bmax[2] - bmin[2]);
    fastf_t dmin = (x_dist < y_dist) ? x_dist : y_dist;
    dmin = (z_dist < dmin) ? z_dist : dmin;
    struct bn_tol tol = {BN_TOL_MAGIC, dmin/mult, dmin/mult * dmin/mult, 1.0e-6, 1.0 - 1.0e-6 };

    // Construct the rays to shoot from the bbox  (TODO what tol?)
    (*ray_cnt) = analyze_get_bbox_rays(&c_rays, bmin, bmax, &tol);

    (*candidate_rays) = c_rays;

    analyze_get_solid_partitions(&o_brep_results, pbrep_rtvars, c_rays, (*ray_cnt), wdbp->dbip, pbrep, &tol, pcpus);
    analyze_get_solid_partitions(&curr_comb_results, ccomb_vars, c_rays, (*ray_cnt), wdbp->dbip, curr_comb, &tol, pcpus);

    //Compare the two partition/gap sets that result.
    struct bu_ptbl missing = BU_PTBL_INIT_ZERO;
    int missing_gaps = find_missing_gaps(&missing, &o_brep_results, &curr_comb_results, candidate, (*ray_cnt));

    // copy missing gaps to new_missing
    for (size_t i = 0; i < BU_PTBL_LEN(&missing); i++) {
	bu_ptbl_ins(new_missing, BU_PTBL_GET(&missing, i));
    }

    bu_log("missing: %d\n", BU_PTBL_LEN(new_missing));

    return missing_gaps;
}

/* Pass in the parent brep rt prep and non-finalized comb prep to avoid doing them multiple times.
 * */
extern "C" int
analyze_find_subtracted(struct bu_ptbl *UNUSED(results), struct rt_wdb *wdbp, const char *pbrep, struct rt_gen_worker_vars *pbrep_rtvars, const char *curr_comb, struct bu_ptbl *candidates, void *data, int pcpus)
{
    struct rt_gen_worker_vars *ccomb_vars;
    struct resource *ccomb_resp;
    struct rt_i *ccomb_rtip;
    //size_t ncpus = bu_avail_cpus();
    size_t ncpus = (size_t)pcpus;
    struct subbrep_object_data *curr_union_data = (struct subbrep_object_data *)data;
    //const ON_Brep *brep = curr_union_data->brep;
    size_t i = 0;
    //struct bn_tol tol = {BN_TOL_MAGIC, 0.5, 0.5 * 0.5, 1.0e-6, 1.0 - 1.0e-6 };
    struct bu_ptbl to_subtract = BU_PTBL_INIT_ZERO;

    if (!curr_union_data) return 0;

    // Parent brep prep is worth passing in, but curr_comb should be prepped here, since we're iterating
    // over all candidates.  Might be worth caching *all* preps on all objects for later more efficient
    // diff validation processing, but that needs more thought
    ccomb_vars = (struct rt_gen_worker_vars *)bu_calloc(ncpus+1, sizeof(struct rt_gen_worker_vars ), "ccomb state");
    ccomb_resp = (struct resource *)bu_calloc(ncpus+1, sizeof(struct resource), "ccomb resources");
    ccomb_rtip = rt_new_rti(wdbp->dbip);
    for (i = 0; i < ncpus+1; i++) {
	ccomb_vars[i].rtip = ccomb_rtip;
	ccomb_vars[i].resp = &ccomb_resp[i];
	rt_init_resource(ccomb_vars[i].resp, i, ccomb_rtip);
    }
    if (rt_gettree(ccomb_rtip, curr_comb) < 0) {
	// TODO - free memory
	return 0;
    }
    rt_prep_parallel(ccomb_rtip, ncpus);

    for (i = 0; i < BU_PTBL_LEN(candidates); i++) {
	point_t bmin, bmax;
	int ray_cnt = 0;
	fastf_t *c_rays = NULL;
	fastf_t *candidate_rays = NULL;
	struct subbrep_object_data *candidate = (struct subbrep_object_data *)BU_PTBL_GET(candidates, i);

	bu_log("\nTesting %s against %s\n", bu_vls_addr(candidate->name_root), bu_vls_addr(curr_union_data->name_root));

	// 1. Get the subbrep_bbox.

	if (!candidate->bbox_set) {
	    bu_log("How did we call this a candidate without doing the bbox calculation already?\n");
	    subbrep_bbox(candidate);
	}
	VMOVE(bmin, candidate->bbox->Min());
	VMOVE(bmax, candidate->bbox->Max());
	fastf_t x_dist = fabs(bmax[0] - bmin[0]);
	fastf_t y_dist = fabs(bmax[1] - bmin[1]);
	fastf_t z_dist = fabs(bmax[2] - bmin[2]);
	fastf_t dmin = (x_dist < y_dist) ? x_dist : y_dist;
	dmin = (z_dist < dmin) ? z_dist : dmin;
	struct bn_tol tol = {BN_TOL_MAGIC, dmin/10, dmin/10 * dmin/10, 1.0e-6, 1.0 - 1.0e-6 };

	// Construct the rays to shoot from the bbox  (TODO what tol?)
	ray_cnt = analyze_get_bbox_rays(&c_rays, bmin, bmax, &tol);
	candidate_rays = c_rays;

	// 2. The rays come from the dimensions of the candidate bbox, but
	//    first check to see whether the original and the ccomb have any
	//    disagreement about what should be there in this particular zone.  If
	//    they don't, then we don't need to prep and shoot this candidate at
	//    all.  If they do, we need to know what the disagreement is in order
	//    to determine if this candidate resolves it.

	struct bu_ptbl o_brep_results = BU_PTBL_INIT_ZERO;
	struct bu_ptbl curr_comb_results = BU_PTBL_INIT_ZERO;

	bu_log("Original brep:\n");
	analyze_get_solid_partitions(&o_brep_results, pbrep_rtvars, candidate_rays, ray_cnt, wdbp->dbip, pbrep, &tol, pcpus);
	struct bu_vls tmp_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&tmp_name, "%s-%s_%s.pl", pbrep, bu_vls_addr(curr_union_data->name_root), bu_vls_addr(candidate->name_root));
	plot_min_partitions(&o_brep_results, bu_vls_addr(&tmp_name));
	bu_log("Control comb: %s\n", curr_comb);
	analyze_get_solid_partitions(&curr_comb_results, ccomb_vars, candidate_rays, ray_cnt, wdbp->dbip, curr_comb, &tol, pcpus);
	bu_vls_sprintf(&tmp_name, "%s-%s_%s-ccomb.pl", pbrep, bu_vls_addr(curr_union_data->name_root), bu_vls_addr(candidate->name_root));
	plot_min_partitions(&curr_comb_results, bu_vls_addr(&tmp_name));
	//
	// 3. Compare the two partition/gap sets that result.
	//
	// TODO - incorporate a bbox filter into the missing gap results
	struct bu_ptbl *missing = NULL;
	struct bu_ptbl *fmissing;
	BU_GET(fmissing, struct bu_ptbl);
	bu_ptbl_init(fmissing, 8, "ptbl init");
	missing = fmissing;
	int missing_gaps = find_missing_gaps(fmissing, &o_brep_results, &curr_comb_results, candidate, ray_cnt);
	if (missing_gaps > 0 && BU_PTBL_LEN(missing) >= MINIMUM_RAYS_FOR_DECISION) {
	    bu_vls_sprintf(&tmp_name, "%s-%s_%s-missing.pl", pbrep, bu_vls_addr(curr_union_data->name_root), bu_vls_addr(candidate->name_root));
	    plot_min_partitions(missing, bu_vls_addr(&tmp_name));
	}
	// 4.  If there are missing gaps in curr_comb, prep candidate and
	// shoot the same rays against it.  If candidate 1) removes either all or a
	// subset of the missing material and 2) does not remove material that is
	// present in the results from the original brep, it is subtracted.  Add it
	// to the results set.
#if 1
	int depth = 1;
	while (missing_gaps > 0 && BU_PTBL_LEN(missing) < MINIMUM_RAYS_FOR_DECISION) {
	    struct bu_ptbl *nmissing;
	    fmissing = missing;
	    BU_GET(nmissing, struct bu_ptbl);
	    bu_ptbl_init(nmissing, 8, "ptbl init");
	    missing_gaps = refine_missing_rays(nmissing, &candidate_rays, &ray_cnt, fmissing, pbrep, pbrep_rtvars, curr_comb, ccomb_vars, candidate, pcpus, wdbp, depth);
	    missing = nmissing;
	    depth++;
	}
	if (missing_gaps > 0) {
	    bu_vls_sprintf(&tmp_name, "%s-%s_%s-missing.pl", pbrep, bu_vls_addr(curr_union_data->name_root), bu_vls_addr(candidate->name_root));
	    plot_min_partitions(missing, bu_vls_addr(&tmp_name));
	    bu_log("refined to depth %d\n", depth);
	}
#endif
	// TODO - If we do find missing gaps in the first pass, need to construct a bbox of those missing gaps and compare
	// it to the size of the bbox used to generate the first set of rays.  If the bbox of the missing gaps is much smaller
	// than the original bbox, the ray sample is going to be too rough to allow for a reliable determination and we need
	// to re-shoot just the area of interest with a finer grid.  Might also be able to use missing gap count to trigger this...
	// ideally, pass to an iterative function that adjusts until we have a reasonable data size

	int decision = 0;
	struct bu_ptbl candidate_results = BU_PTBL_INIT_ZERO;
	if (missing_gaps > 0) {
	    // Make a region copy of the candidate
	    struct directory *dp;
	    struct bu_external external;
	    struct bu_vls tmp_comb_name = BU_VLS_INIT_ZERO;
	    struct rt_gen_worker_vars *candidate_vars;
	    struct resource *candidate_resp;
	    struct rt_i *candidate_rtip;
	    bu_log("Testing missing gaps(%d) candidate %s\n", missing_gaps, bu_vls_addr(candidate->obj_name));

	    dp = db_lookup(wdbp->dbip, bu_vls_addr(candidate->obj_name), LOOKUP_QUIET);
	    if (!dp || db_get_external(&external, dp, wdbp->dbip)) {
		bu_log("Database read error on object %s\n", bu_vls_addr(candidate->obj_name));
		break;
	    }
	    bu_vls_sprintf(&tmp_comb_name, "%s-r", bu_vls_addr(candidate->obj_name));
	    if (wdb_export_external(wdbp, &external, bu_vls_addr(&tmp_comb_name), dp->d_flags, dp->d_minor_type) < 0) {
		bu_log("Database write error on tmp object %s\n", bu_vls_addr(&tmp_comb_name));
		break;
	    }
	    // Now the region flag
	    dp = db_lookup(wdbp->dbip, bu_vls_addr(&tmp_comb_name), LOOKUP_QUIET);
	    dp->d_flags |= RT_DIR_REGION;

	    // Set up some resources
	    candidate_vars = (struct rt_gen_worker_vars *)bu_calloc(ncpus+1, sizeof(struct rt_gen_worker_vars ), "candidate state");
	    candidate_resp = (struct resource *)bu_calloc(ncpus+1, sizeof(struct resource), "candidate resources");
	    candidate_rtip = rt_new_rti(wdbp->dbip);
	    for (size_t k = 0; k < ncpus+1; k++) {
		candidate_vars[k].rtip = candidate_rtip;
		candidate_vars[k].resp = &candidate_resp[k];
		rt_init_resource(candidate_vars[k].resp, k, candidate_rtip);
	    }
	    if (rt_gettree(candidate_rtip, dp->d_namep) < 0) {
		// TODO - free memory
		return 0;
	    }
	    rt_prep_parallel(candidate_rtip, ncpus);

	    analyze_get_solid_partitions(&candidate_results, candidate_vars, candidate_rays, ray_cnt, wdbp->dbip, dp->d_namep, &tol, pcpus);

	    //rt_clean(candidate_rtip);
	    bu_free(candidate_vars, "free vars");
	    bu_free(candidate_resp, "free resp");

	    bu_vls_sprintf(&tmp_name, "%s-%s_%s-candidate.pl", pbrep, bu_vls_addr(curr_union_data->name_root), bu_vls_addr(candidate->name_root));
	    plot_min_partitions(&candidate_results, bu_vls_addr(&tmp_name));

	    // remove the temp comb.
	    dp = db_lookup(wdbp->dbip, dp->d_namep, LOOKUP_QUIET);
	    if (dp != RT_DIR_NULL) {
		if (db_delete(wdbp->dbip, dp) != 0 || db_dirdelete(wdbp->dbip, dp) != 0) {
		    bu_log("error deleting temp object %s\n", bu_vls_addr(&tmp_comb_name));
		}
	    }

	    // 5.  If we've got hits back from the candidate, evaluate them against the missing gaps and the ccomb hits.
	    //     If all candidate solid segments are within the missing gaps or are on rays that didn't report a ccomb hit,
	    //     the candidate is a subtraction to be added to the tree definition.
	    decision = subtraction_decision(missing, &candidate_results, ray_cnt);

	}
	if (decision) {
	    bu_log("Subtract %s from %s\n", bu_vls_addr(candidate->obj_name), bu_vls_addr(curr_union_data->obj_name));
	    bu_ptbl_ins(&to_subtract, (long *)candidate);
	}
	bu_log("subtraction decision: %d\n\n", decision);
    }

    // blegh - can't do this through libged, since we're using it - is there a sane librt API to do this somewhere?
    if (BU_PTBL_LEN(&to_subtract) > 0) {
	struct bu_vls sub_comb_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&sub_comb_name, "csg_%s-s_%s.c", pbrep, bu_vls_addr(curr_union_data->name_root));
	struct directory *cdp = db_lookup(wdbp->dbip, bu_vls_addr(&sub_comb_name), LOOKUP_QUIET);
	if (cdp == RT_DIR_NULL) {
	    bu_log("error looking up subtraction object %s\n", bu_vls_addr(&sub_comb_name));
	} else {
	    struct rt_db_internal intern;
	    struct rt_comb_internal *comb;
	    union tree *tp;
	    struct rt_tree_array *tree_list;
	    size_t node_count;
	    size_t actual_count;
	    size_t curr_count;
	    if (rt_db_get_internal(&intern, cdp, wdbp->dbip, NULL, &rt_uniresource) < 0) {
		bu_log("error getting subtraction internal\n");
	    }
	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    RT_CK_COMB(comb);
	    if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
		db_non_union_push(comb->tree, &rt_uniresource);
	    }
	    curr_count = db_tree_nleaves(comb->tree);
	    node_count = curr_count + BU_PTBL_LEN(&to_subtract);
	    tree_list = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list");
	    if (comb->tree) {
		actual_count = BU_PTBL_LEN(&to_subtract) + (struct rt_tree_array *)db_flatten_tree(tree_list, comb->tree, OP_UNION, 1, &rt_uniresource) - tree_list;
		BU_ASSERT_SIZE_T(actual_count, ==, node_count);
		comb->tree = TREE_NULL;
	    }
	    for (size_t si = 0; si < BU_PTBL_LEN(&to_subtract); si++) {
		struct subbrep_object_data *s = (struct subbrep_object_data *)BU_PTBL_GET(&to_subtract, si);
		tree_list[curr_count].tl_op = OP_UNION;
		RT_GET_TREE(tp, &rt_uniresource);
		tree_list[curr_count].tl_tree = tp;
		tp->tr_l.tl_op = OP_DB_LEAF;
		tp->tr_l.tl_name = bu_strdup(bu_vls_addr(s->obj_name));
		tp->tr_l.tl_mat = (matp_t)NULL;

		++curr_count;
	    }
	    comb->tree = (union tree *)db_mkgift_tree(tree_list, node_count, &rt_uniresource);
	    if (rt_db_put_internal(cdp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
		bu_log("error writing out subtraction comb\n");
	    }
	}
    }

    bu_free(ccomb_vars, "free vars");
    bu_free(ccomb_resp, "free resp");

    // Once all candidates are processed, return the BU_PTBL_LEN of results.
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

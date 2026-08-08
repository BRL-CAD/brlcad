/*                       B R E P _ R A Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file brep_ray.cpp
 *
 * Directed comparisons of analytic primitive and converted-BREP ray hits.
 */

#include "common.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#include "bu/app.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"
#include "wdb.h"
#include "../librt_private.h"


struct ray_result {
    int shot_hits = 0;
    int segments = 0;
    double in_dist = 0.0;
    double out_dist = 0.0;
    vect_t in_normal = VINIT_ZERO;
    vect_t out_normal = VINIT_ZERO;
};


static const size_t MAX_TEST_PARTITIONS = 8;


struct partition_interval {
    double in_dist = 0.0;
    double out_dist = 0.0;
    vect_t in_normal = VINIT_ZERO;
    vect_t out_normal = VINIT_ZERO;
};


struct partition_result {
    size_t partitions = 0;
    bool overflow = false;
    partition_interval intervals[MAX_TEST_PARTITIONS];
};


struct prepared_model {
    struct db_i *dbip = NULL;
    struct rt_i *rtip = NULL;
    struct resource resp = {};
    bool resource_initialized = false;
};


struct sampled_ray {
    point_t origin = VINIT_ZERO;
    vect_t direction = VINIT_ZERO;
};


struct directed_partition_ray {
    const char *name;
    point_t origin;
    vect_t direction;
    size_t partitions;
    double distances[2 * MAX_TEST_PARTITIONS];
};


static struct soltab *
prep_solid(struct rt_i *rtip, struct rt_db_internal *intern, int type)
{
    struct soltab *stp = (struct soltab *)bu_calloc(1, sizeof(struct soltab),
	"direct ray test soltab");
    stp->l.magic = RT_SOLTAB_MAGIC;
    stp->l2.magic = RT_SOLTAB2_MAGIC;
    stp->st_rtip = rtip;
    stp->st_id = type;
    stp->st_meth = &OBJ[type];

    if (OBJ[type].ft_prep(stp, intern, rtip)) {
	if (stp->st_specific && stp->st_meth && stp->st_meth->ft_free)
	    stp->st_meth->ft_free(stp);
	bu_free(stp, "direct ray test soltab");
	return NULL;
    }
    return stp;
}


static void
free_solid(struct soltab *stp)
{
    if (!stp)
	return;
    if (stp->st_meth && stp->st_meth->ft_free)
	stp->st_meth->ft_free(stp);
    bu_free(stp, "direct ray test soltab");
}


static void
free_prepared_model(prepared_model &model)
{
    if (model.resource_initialized && model.rtip) {
	rt_clean_resource_basic(model.rtip, &model.resp);
	BU_PTBL_SET(&model.rtip->rti_resources, 0, NULL);
	model.resource_initialized = false;
    }
    if (model.rtip) {
	rt_i_destroy(model.rtip);
	model.rtip = NULL;
    }
    if (model.dbip) {
	db_close(model.dbip);
	model.dbip = NULL;
    }
}


static bool
prep_partition_model(prepared_model &model,
    const struct rt_db_internal *intern, const char *name,
    const struct bn_tol *tol)
{
    model.dbip = db_open_inmem();
    if (!model.dbip)
	return false;

    struct rt_wdb *wdbp = wdb_dbopen(model.dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
	free_prepared_model(model);
	return false;
    }

    struct rt_db_internal tmp_intern;
    RT_DB_INTERNAL_INIT(&tmp_intern);
    tmp_intern.idb_major_type = intern->idb_major_type;
    tmp_intern.idb_type = intern->idb_minor_type;
    tmp_intern.idb_meth = &OBJ[intern->idb_minor_type];
    tmp_intern.idb_ptr = intern->idb_ptr;

    struct bu_external ext;
    BU_EXTERNAL_INIT(&ext);
    if (rt_db_cvt_to_ext5(&ext, name, &tmp_intern, 1.0, model.dbip,
	    intern->idb_major_type) < 0) {
	bu_free_external(&ext);
	free_prepared_model(model);
	return false;
    }

    if (wdb_export_external(wdbp, &ext, name,
	    db_flags_internal(&tmp_intern),
	    (unsigned char)intern->idb_minor_type) < 0) {
	bu_free_external(&ext);
	free_prepared_model(model);
	return false;
    }
    bu_free_external(&ext);
    db_update_nref(model.dbip);

    model.rtip = rt_i_create(model.dbip);
    if (!model.rtip) {
	free_prepared_model(model);
	return false;
    }
    model.rtip->rti_tol = *tol;
    if (rt_gettree(model.rtip, name) < 0) {
	free_prepared_model(model);
	return false;
    }
    rt_prep_parallel(model.rtip, 1);
    rt_init_resource(&model.resp, 0, model.rtip);
    model.resource_initialized = true;
    return true;
}


static bool
export_internal_object(struct db_i *dbip, struct rt_wdb *wdbp,
    const struct rt_db_internal *intern, const char *name)
{
    struct rt_db_internal tmp_intern;
    RT_DB_INTERNAL_INIT(&tmp_intern);
    tmp_intern.idb_major_type = intern->idb_major_type;
    tmp_intern.idb_type = intern->idb_minor_type;
    tmp_intern.idb_meth = &OBJ[intern->idb_minor_type];
    tmp_intern.idb_ptr = intern->idb_ptr;

    struct bu_external ext;
    BU_EXTERNAL_INIT(&ext);
    if (rt_db_cvt_to_ext5(&ext, name, &tmp_intern, 1.0, dbip,
	    intern->idb_major_type) < 0) {
	bu_free_external(&ext);
	return false;
    }
    if (wdb_export_external(wdbp, &ext, name,
	    db_flags_internal(&tmp_intern),
	    (unsigned char)intern->idb_minor_type) < 0) {
	bu_free_external(&ext);
	return false;
    }
    bu_free_external(&ext);
    return true;
}


static bool
export_brep_conversion(struct db_i *dbip, struct rt_wdb *wdbp,
    struct rt_db_internal *intern, const char *name,
    const struct bn_tol *tol)
{
    ON_Brep *brep = ON_Brep::New();
    OBJ[intern->idb_minor_type].ft_brep(&brep, intern, tol);
    if (!brep)
	return false;
    struct rt_brep_internal brep_internal = {};
    brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
    brep_internal.brep = brep;
    struct rt_db_internal brep_intern;
    RT_DB_INTERNAL_INIT(&brep_intern);
    brep_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    brep_intern.idb_type = ID_BREP;
    brep_intern.idb_meth = &OBJ[ID_BREP];
    brep_intern.idb_ptr = &brep_internal;
    const bool result = export_internal_object(dbip, wdbp, &brep_intern,
	name);
    delete brep;
    return result;
}


static bool
prep_region_model(prepared_model &model, const char *region_name,
    const struct bn_tol *tol)
{
    if (!model.dbip)
	return false;
    db_update_nref(model.dbip);
    model.rtip = rt_i_create(model.dbip);
    if (!model.rtip)
	return false;
    model.rtip->rti_tol = *tol;
    if (rt_gettree(model.rtip, region_name) < 0)
	return false;
    rt_prep_parallel(model.rtip, 1);
    rt_init_resource(&model.resp, 0, model.rtip);
    model.resource_initialized = true;
    return true;
}


static bool
prep_binary_csg_model(prepared_model &model,
    struct rt_db_internal *left_intern,
    struct rt_db_internal *right_intern, int member_operation,
    const struct bn_tol *tol, bool brep_leaves)
{
    model.dbip = db_open_inmem();
    if (!model.dbip)
	return false;
    struct rt_wdb *wdbp = wdb_dbopen(model.dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp)
	return false;

    const bool left_ok = brep_leaves ?
	export_brep_conversion(model.dbip, wdbp, left_intern, "left.s", tol) :
	export_internal_object(model.dbip, wdbp, left_intern, "left.s");
    const bool right_ok = brep_leaves ?
	export_brep_conversion(model.dbip, wdbp, right_intern, "right.s", tol) :
	export_internal_object(model.dbip, wdbp, right_intern, "right.s");
    struct wmember members;
    BU_LIST_INIT(&members.l);
    if (!left_ok || !right_ok ||
	    !mk_addmember("left.s", &members.l, NULL, WMOP_UNION) ||
	    !mk_addmember("right.s", &members.l, NULL, member_operation) ||
	    mk_lcomb(wdbp, "oracle.r", &members, 1, NULL, NULL, NULL, 0))
	return false;
    return prep_region_model(model, "oracle.r", tol);
}


static int
partition_hit(struct application *ap, struct partition *head,
    struct seg *UNUSED(segs))
{
    partition_result *result = static_cast<partition_result *>(ap->a_uptr);
    struct partition *pp;
    for (pp = head->pt_forw; pp != head; pp = pp->pt_forw) {
	if (result->partitions >= MAX_TEST_PARTITIONS) {
	    result->overflow = true;
	    continue;
	}
	partition_interval &interval = result->intervals[result->partitions++];
	interval.in_dist = pp->pt_inhit->hit_dist;
	interval.out_dist = pp->pt_outhit->hit_dist;
	RT_HIT_NORMAL(interval.in_normal, pp->pt_inhit,
	    pp->pt_inseg->seg_stp, &ap->a_ray, pp->pt_inflip);
	RT_HIT_NORMAL(interval.out_normal, pp->pt_outhit,
	    pp->pt_outseg->seg_stp, &ap->a_ray, pp->pt_outflip);
    }
    return 1;
}


static int
partition_miss(struct application *UNUSED(ap))
{
    return 0;
}


static partition_result
shoot_partitions(prepared_model &model, const sampled_ray &ray)
{
    partition_result result;
    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = model.rtip;
    ap.a_resource = &model.resp;
    ap.a_hit = partition_hit;
    ap.a_miss = partition_miss;
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.a_onehit = 0;
    ap.a_uptr = &result;
    VMOVE(ap.a_ray.r_pt, ray.origin);
    VMOVE(ap.a_ray.r_dir, ray.direction);
    ap.a_ray.magic = RT_RAY_MAGIC;
    rt_shootray(&ap);
    return result;
}


static ray_result
shoot_solid(struct soltab *stp, struct rt_i *rtip, struct resource *resp,
    const point_t origin, const vect_t direction)
{
    ray_result result;
    struct application ap;
    struct seg seghead;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_resource = resp;
    ap.a_onehit = 0;
    VMOVE(ap.a_ray.r_pt, origin);
    VMOVE(ap.a_ray.r_dir, direction);
    VUNITIZE(ap.a_ray.r_dir);
    ap.a_ray.magic = RT_RAY_MAGIC;
    BU_LIST_INIT(&seghead.l);

    result.shot_hits = rt_obj_shot(stp, &ap.a_ray, &ap, &seghead);
    struct seg *segp;
    for (BU_LIST_FOR(segp, seg, &seghead.l)) {
	result.segments++;
	if (result.segments != 1)
	    continue;
	result.in_dist = segp->seg_in.hit_dist;
	result.out_dist = segp->seg_out.hit_dist;
	struct hit in = segp->seg_in;
	struct hit out = segp->seg_out;
	rt_obj_norm(&in, stp, &ap.a_ray);
	rt_obj_norm(&out, stp, &ap.a_ray);
	VMOVE(result.in_normal, in.hit_normal);
	VMOVE(result.out_normal, out.hit_normal);
    }

    while (BU_LIST_WHILE(segp, seg, &seghead.l)) {
	BU_LIST_DEQUEUE(&segp->l);
	RT_FREE_SEG(segp, resp);
    }
    return result;
}


static ray_result
shoot_brep_legacy(struct soltab *stp, struct rt_i *rtip,
    struct resource *resp, const point_t origin, const vect_t direction)
{
    ray_result result;
    struct application ap;
    struct seg seghead;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_resource = resp;
    ap.a_onehit = 0;
    VMOVE(ap.a_ray.r_pt, origin);
    VMOVE(ap.a_ray.r_dir, direction);
    VUNITIZE(ap.a_ray.r_dir);
    ap.a_ray.magic = RT_RAY_MAGIC;
    BU_LIST_INIT(&seghead.l);

    result.shot_hits = _rt_brep_shot_legacy(stp, &ap.a_ray, &ap, &seghead);
    struct seg *segp;
    for (BU_LIST_FOR(segp, seg, &seghead.l)) {
	result.segments++;
	if (result.segments != 1)
	    continue;
	result.in_dist = segp->seg_in.hit_dist;
	result.out_dist = segp->seg_out.hit_dist;
	struct hit in = segp->seg_in;
	struct hit out = segp->seg_out;
	rt_obj_norm(&in, stp, &ap.a_ray);
	rt_obj_norm(&out, stp, &ap.a_ray);
	VMOVE(result.in_normal, in.hit_normal);
	VMOVE(result.out_normal, out.hit_normal);
    }

    while (BU_LIST_WHILE(segp, seg, &seghead.l)) {
	BU_LIST_DEQUEUE(&segp->l);
	RT_FREE_SEG(segp, resp);
    }
    return result;
}


static int
shoot_brep_trace(struct soltab *stp, struct rt_i *rtip,
    struct resource *resp, const sampled_ray &ray,
    struct rt_brep_shot_trace &trace)
{
    struct application ap;
    struct seg seghead;
    struct xray xray;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_resource = resp;
    VMOVE(xray.r_pt, ray.origin);
    VMOVE(xray.r_dir, ray.direction);
    xray.magic = RT_RAY_MAGIC;
    BU_LIST_INIT(&seghead.l);
    const int hits = _rt_brep_shot_trace(stp, &xray, &ap, &seghead, &trace);

    struct seg *segp;
    while (BU_LIST_WHILE(segp, seg, &seghead.l)) {
	BU_LIST_DEQUEUE(&segp->l);
	RT_FREE_SEG(segp, resp);
    }
    return hits;
}


static bool
brep_trace_fixed_workspaces_match(const struct rt_brep_shot_trace &trace,
    bool allow_leaf_fallback = false)
{
    const bool leaf_workspace_matches =
	(!trace.fixed_leaf_overflow && !trace.fixed_leaf_fallback &&
	 trace.fixed_leaf_count == trace.fixed_leaf_stored &&
	 trace.fixed_leaf_count == trace.intersected_leaves &&
	 trace.fixed_leaf_mismatches == 0) ||
	(allow_leaf_fallback && trace.fixed_leaf_overflow &&
	 trace.fixed_leaf_fallback &&
	 trace.fixed_leaf_count > RT_BREP_MAX_LEAVES &&
	 trace.fixed_leaf_stored == RT_BREP_MAX_LEAVES &&
	 trace.fixed_leaf_count == trace.intersected_leaves &&
	 trace.fixed_leaf_mismatches == 1);
    return leaf_workspace_matches &&
	trace.prepared_production_attempts == 1 &&
	trace.prepared_production_eligible ==
	    trace.prepared_production_selected &&
	trace.prepared_production_eligible <= 1 &&
	trace.prepared_production_fallback >=
	    RT_BREP_PREPARED_FALLBACK_NONE &&
	trace.prepared_production_fallback < RT_BREP_PREPARED_FALLBACK_COUNT &&
	((trace.prepared_production_selected &&
	  trace.prepared_production_fallback ==
	    RT_BREP_PREPARED_FALLBACK_NONE &&
	  trace.prepared_production_hits % 2 == 0) ||
	 (!trace.prepared_production_selected &&
	  trace.prepared_production_fallback !=
	    RT_BREP_PREPARED_FALLBACK_NONE)) &&
	!trace.fixed_hit_overflow &&
	!trace.fixed_hit_fallback &&
	trace.fixed_hit_count == trace.fixed_hit_stored &&
	trace.fixed_hit_count == trace.raw_hits &&
	trace.fixed_hit_mismatches == 0 &&
	trace.fixed_after_near_miss == trace.after_near_miss &&
	trace.fixed_after_near_hit == trace.after_near_hit &&
	trace.fixed_after_grazing == trace.after_grazing &&
	trace.fixed_after_duplicates == trace.after_duplicates &&
	trace.fixed_after_direction_cleanup == trace.after_direction_cleanup &&
	trace.fixed_cleanup_mismatches == 0 &&
	trace.trim_queries == trace.candidate_roots &&
	trace.trim_noalloc_candidates == trace.trim_allocating_candidates &&
	trace.trim_candidate_mismatches == 0 &&
	trace.trim_status_mismatches == 0 &&
	trace.trim_closest_mismatches == 0 &&
	trace.trim_distance_mismatches == 0 &&
	trace.trim_equivalence_mismatches == 0 &&
	trace.face_trim_queries == trace.trim_queries &&
	trace.face_trim_status_mismatches == 0 &&
	trace.face_trim_hit_class_mismatches == 0 &&
	trace.face_trim_adjacency_mismatches == 0 &&
	trace.face_trim_equivalence_mismatches == 0 &&
	trace.local_trim_failures == 0 &&
	trace.local_trim_queries + trace.local_trim_failures ==
	    trace.stored_local_roots &&
	trace.local_event_failures == 0 &&
	trace.local_event_overflow == 0 &&
	trace.local_candidate_failures == 0 &&
	trace.local_candidate_overflow == 0 &&
	trace.local_event_segment_overflow == 0 &&
	trace.local_event_final_segments ==
	    trace.local_event_stored_segments &&
	trace.legacy_unique_roots == trace.legacy_unique_roots_matched +
	    trace.legacy_unique_roots_unmatched &&
	trace.local_unique_roots == trace.local_unique_roots_matched +
	    trace.local_unique_roots_unmatched &&
	trace.matched_root_events == trace.legacy_unique_roots_matched &&
	trace.matched_root_events == trace.local_unique_roots_matched &&
	trace.root_event_mismatches <= trace.matched_root_events;
}


static bool
finite_unit_vector(const vect_t value)
{
    return std::isfinite(value[X]) && std::isfinite(value[Y]) &&
	std::isfinite(value[Z]) && fabs(MAGNITUDE(value) - 1.0) < 1.0e-7;
}


static int
check_ray(const char *label, struct soltab *implicit_stp,
    struct soltab *brep_stp, struct rt_i *rtip, struct resource *resp,
    const point_t origin, const vect_t direction, double expected_in,
    double expected_out)
{
    const double distance_tolerance = std::max(1.0e-9,
	rtip->rti_tol.dist);
    int failures = 0;
    ray_result implicit_result = shoot_solid(implicit_stp, rtip, resp,
	origin, direction);
    ray_result brep_result = shoot_solid(brep_stp, rtip, resp, origin,
	direction);

    if (implicit_result.segments != 1 || brep_result.segments != 1) {
	std::printf("FAIL: %-18s segments implicit=%d BREP=%d\n", label,
	    implicit_result.segments, brep_result.segments);
	return 1;
    }

    if (fabs(implicit_result.in_dist - expected_in) > distance_tolerance ||
	    fabs(implicit_result.out_dist - expected_out) > distance_tolerance ||
	    fabs(brep_result.in_dist - expected_in) > distance_tolerance ||
	    fabs(brep_result.out_dist - expected_out) > distance_tolerance) {
	std::printf("FAIL: %-18s distances implicit=[%.17g %.17g] "
	    "BREP=[%.17g %.17g] expected=[%.17g %.17g]\n", label,
	    implicit_result.in_dist, implicit_result.out_dist,
	    brep_result.in_dist, brep_result.out_dist, expected_in,
	    expected_out);
	failures++;
    }

    if (fabs(implicit_result.in_dist - brep_result.in_dist) >
	    distance_tolerance ||
	    fabs(implicit_result.out_dist - brep_result.out_dist) >
	    distance_tolerance) {
	std::printf("FAIL: %-18s implicit/BREP distances differ\n", label);
	failures++;
    }

    if (!finite_unit_vector(brep_result.in_normal) ||
	    !finite_unit_vector(brep_result.out_normal) ||
	    VDOT(brep_result.in_normal, direction) >= 0.0 ||
	    VDOT(brep_result.out_normal, direction) <= 0.0) {
	std::printf("FAIL: %-18s invalid BREP normals in=(%.17g %.17g %.17g) "
	    "out=(%.17g %.17g %.17g)\n", label,
	    V3ARGS(brep_result.in_normal), V3ARGS(brep_result.out_normal));
	failures++;
    }

    return failures;
}


static const char *
ray_class(const ray_result &result, double tolerance)
{
    if (result.segments == 0)
	return "MISS";
    if (result.segments != 1)
	return "MULTI";
    if (fabs(result.out_dist - result.in_dist) <= tolerance)
	return "CONTACT";
    return "INTERVAL";
}


static bool
brep_trace_covers_t(const struct rt_brep_shot_trace &trace, double dist,
    double tolerance)
{
    for (size_t i = 0; i < trace.stored_surface_boxes; ++i) {
	const struct rt_brep_trace_surface_box &box = trace.surface_boxes[i];
	if (dist >= box.t_min - tolerance && dist <= box.t_max + tolerance)
	    return true;
    }
    return false;
}


static bool
brep_trace_box_covers_both(const struct rt_brep_shot_trace &trace,
    double first, double second, double tolerance)
{
    for (size_t i = 0; i < trace.stored_surface_boxes; ++i) {
	const struct rt_brep_trace_surface_box &box = trace.surface_boxes[i];
	if (first >= box.t_min - tolerance && first <= box.t_max + tolerance &&
		second >= box.t_min - tolerance &&
		second <= box.t_max + tolerance)
	    return true;
    }
    return false;
}


static void
brep_trace_root_coverage_diagnostic(const char *label,
    const struct rt_brep_shot_trace &trace)
{
    if (!trace.legacy_unique_roots_unmatched &&
	    !trace.local_unique_roots_unmatched &&
	    !trace.root_event_mismatches)
	return;
    std::printf("Root coverage diagnostic %s: legacy=%zu/%zu/%zu "
	"local=%zu/%zu/%zu clusters=%zu segments=%zu\n", label,
	trace.legacy_unique_roots, trace.legacy_unique_roots_matched,
	trace.legacy_unique_roots_unmatched, trace.local_unique_roots,
	trace.local_unique_roots_matched, trace.local_unique_roots_unmatched,
	trace.stored_local_clusters, trace.final_segments);
    for (size_t root_index = 0; root_index < trace.stored_roots;
	    ++root_index) {
	const struct rt_brep_trace_root &root = trace.roots[root_index];
	std::printf("  legacy %zu face=%d t=%.17g uv=%.17g/%.17g "
	    "normal=%.17g class=%d trim=%d distance=%.17g adj=%d "
	    "direction=%d\n", root_index,
	    root.face_index, root.dist, root.uv[0], root.uv[1],
	    root.normal_dot, root.hit_class, root.trim_status,
	    root.trim_distance, root.adjacent_face_index, root.direction);
    }
    for (size_t root_index = 0;
	    root_index < trace.stored_local_roots; ++root_index) {
	const struct rt_brep_trace_local_root &root =
	    trace.local_roots[root_index];
	std::printf("  local %zu face=%d span=%d t=%.17g "
	    "uv=%.17g/%.17g residual=%.17g normal=%.17g class=%d "
	    "trim=%d distance=%.17g adj=%d direction=%d\n",
	    root_index, root.face_index, root.span_index, root.dist,
	    root.uv[0], root.uv[1], root.residual, root.normal_dot,
	    root.hit_class, root.trim_status, root.trim_distance,
	    root.adjacent_face_index, root.direction);
    }
}


struct brep_root_event_summary {
    size_t matched = 0;
    size_t mismatched = 0;
    size_t trim_status_mismatches = 0;
    size_t hit_class_mismatches = 0;
    size_t direction_mismatches = 0;
    size_t adjacency_mismatches = 0;
    size_t local_trim_queries = 0;
    size_t local_trim_candidates = 0;
    size_t face_trim_queries = 0;
    size_t face_trim_candidates = 0;
    size_t face_trim_mismatches = 0;
    size_t local_event_groups = 0;
    size_t local_event_contacts = 0;
    size_t local_event_clean_misses = 0;
    size_t local_candidate_hits = 0;
    size_t local_candidate_failures = 0;
    size_t local_candidate_overflow = 0;
    size_t local_candidate_stage_mismatches = 0;
    size_t local_candidate_semantic_stage_mismatches = 0;
    size_t local_candidate_semantic_stage[RT_BREP_TRACE_CLEANUP_STAGES] = {};
    size_t local_candidate_hit_mismatches = 0;
    size_t local_event_hits = 0;
    size_t local_event_failures = 0;
    size_t local_event_overflow = 0;
    size_t local_event_stage_mismatches = 0;
    size_t local_event_hit_mismatches = 0;
    size_t local_event_count_mismatches = 0;
    size_t local_event_t_mismatches = 0;
    size_t local_event_face_mismatches = 0;
    size_t local_event_trim_mismatches = 0;
    size_t local_event_edge_mismatches = 0;
    size_t local_event_class_mismatches = 0;
    size_t local_event_direction_mismatches = 0;
    size_t local_event_adjacency_mismatches = 0;
    size_t local_event_repaired = 0;
    size_t local_event_final_segments = 0;
    size_t local_event_final_mismatches = 0;
    size_t surface_isolated_boxes = 0;
    size_t surface_corrector_attempts = 0;
    size_t surface_corrector_converged = 0;
    size_t surface_krawczyk_boxes = 0;
    size_t surface_krawczyk_min_depth = 0;
    size_t surface_krawczyk_max_depth = 0;
    size_t surface_subdivision_boxes = 0;
    size_t surface_subdivision_max_boxes = 0;
    size_t surface_clip_attempts = 0;
    size_t surface_clip_contractions = 0;
    size_t surface_clip_inconclusive = 0;
    size_t surface_clip_restriction_failures = 0;
    size_t prepared_production_attempts = 0;
    size_t prepared_production_eligible = 0;
    size_t prepared_production_selected = 0;
    size_t prepared_production_fallback[RT_BREP_PREPARED_FALLBACK_COUNT] = {};
    double surface_clip_max_fraction_removed = 0.0;
    double maximum_t_error = 0.0;
    double maximum_uv_error = 0.0;
    double maximum_trim_error = 0.0;
    double maximum_normal_dot_error = 0.0;
    double maximum_face_trim_error = 0.0;
    double minimum_surface_isolated_t_width = 0.0;
    double maximum_surface_isolated_t_width = 0.0;
};


static void
brep_accumulate_root_events(brep_root_event_summary &summary,
    const struct rt_brep_shot_trace &trace)
{
    summary.matched += trace.matched_root_events;
    summary.mismatched += trace.root_event_mismatches;
    summary.trim_status_mismatches += trace.root_trim_status_mismatches;
    summary.hit_class_mismatches += trace.root_hit_class_mismatches;
    summary.direction_mismatches += trace.root_direction_mismatches;
    summary.adjacency_mismatches += trace.root_adjacency_mismatches;
    summary.local_trim_queries += trace.local_trim_queries;
    summary.local_trim_candidates += trace.local_trim_candidates;
    summary.face_trim_queries += trace.face_trim_queries;
    summary.face_trim_candidates += trace.face_trim_candidates;
    summary.face_trim_mismatches +=
	trace.face_trim_equivalence_mismatches;
    summary.local_event_groups += trace.local_event_groups;
    summary.local_event_contacts += trace.local_event_contacts;
    summary.local_event_clean_misses += trace.local_event_clean_misses;
    summary.local_candidate_hits += trace.local_candidate_hits;
    summary.local_candidate_failures += trace.local_candidate_failures;
    summary.local_candidate_overflow += trace.local_candidate_overflow;
    summary.local_candidate_stage_mismatches +=
	trace.local_candidate_stage_mismatches;
    summary.local_candidate_semantic_stage_mismatches +=
	trace.local_candidate_semantic_stage_mismatches;
    for (size_t stage_index = 0;
	    stage_index < RT_BREP_TRACE_CLEANUP_STAGES; ++stage_index)
	summary.local_candidate_semantic_stage[stage_index] +=
	    trace.local_candidate_semantic_stage[stage_index];
    summary.local_candidate_hit_mismatches +=
	trace.local_candidate_hit_mismatches;
    summary.local_event_hits += trace.local_event_hits;
    summary.local_event_failures += trace.local_event_failures;
    summary.local_event_overflow += trace.local_event_overflow;
    summary.local_event_stage_mismatches +=
	trace.local_event_stage_mismatches;
    summary.local_event_hit_mismatches += trace.local_event_hit_mismatches;
    summary.local_event_count_mismatches +=
	trace.local_event_count_mismatches;
    summary.local_event_t_mismatches += trace.local_event_t_mismatches;
    summary.local_event_face_mismatches += trace.local_event_face_mismatches;
    summary.local_event_trim_mismatches += trace.local_event_trim_mismatches;
    summary.local_event_edge_mismatches += trace.local_event_edge_mismatches;
    summary.local_event_class_mismatches += trace.local_event_class_mismatches;
    summary.local_event_direction_mismatches +=
	trace.local_event_direction_mismatches;
    summary.local_event_adjacency_mismatches +=
	trace.local_event_adjacency_mismatches;
    summary.local_event_repaired += trace.local_event_repaired;
    summary.local_event_final_segments += trace.local_event_final_segments;
    summary.local_event_final_mismatches +=
	trace.local_event_final_mismatches;
    summary.surface_corrector_attempts += trace.surface_corrector_attempts;
    summary.surface_corrector_converged += trace.surface_corrector_converged;
    if (trace.surface_krawczyk_boxes) {
	if (!summary.surface_krawczyk_boxes)
	    summary.surface_krawczyk_min_depth =
		trace.surface_krawczyk_min_depth;
	else
	    summary.surface_krawczyk_min_depth = std::min(
		summary.surface_krawczyk_min_depth,
		trace.surface_krawczyk_min_depth);
	summary.surface_krawczyk_boxes += trace.surface_krawczyk_boxes;
	summary.surface_krawczyk_max_depth = std::max(
	    summary.surface_krawczyk_max_depth,
	    trace.surface_krawczyk_max_depth);
    }
    summary.surface_subdivision_boxes += trace.surface_subdivision_boxes;
    summary.surface_subdivision_max_boxes = std::max(
	summary.surface_subdivision_max_boxes,
	trace.surface_subdivision_boxes);
    summary.surface_clip_attempts += trace.surface_clip_attempts;
    summary.surface_clip_contractions += trace.surface_clip_contractions;
    summary.surface_clip_inconclusive += trace.surface_clip_inconclusive;
    summary.surface_clip_restriction_failures +=
	trace.surface_clip_restriction_failures;
    summary.prepared_production_attempts +=
	trace.prepared_production_attempts;
    summary.prepared_production_eligible +=
	trace.prepared_production_eligible;
    summary.prepared_production_selected +=
	trace.prepared_production_selected;
    if (trace.prepared_production_fallback >=
	    RT_BREP_PREPARED_FALLBACK_NONE &&
	    trace.prepared_production_fallback <
	    RT_BREP_PREPARED_FALLBACK_COUNT)
	summary.prepared_production_fallback
	    [trace.prepared_production_fallback]++;
    summary.surface_clip_max_fraction_removed = std::max(
	summary.surface_clip_max_fraction_removed,
	(double)trace.surface_clip_max_fraction_removed);
    summary.maximum_t_error = std::max(summary.maximum_t_error,
	(double)trace.root_match_max_t_error);
    summary.maximum_uv_error = std::max(summary.maximum_uv_error,
	(double)trace.root_match_max_uv_error);
    summary.maximum_trim_error = std::max(summary.maximum_trim_error,
	(double)trace.root_match_max_trim_error);
    summary.maximum_normal_dot_error = std::max(
	summary.maximum_normal_dot_error,
	(double)trace.root_match_max_normal_dot_error);
    summary.maximum_face_trim_error = std::max(
	summary.maximum_face_trim_error,
	(double)trace.face_trim_max_near_distance_error);
    if (trace.surface_isolated_boxes) {
	if (!summary.surface_isolated_boxes)
	    summary.minimum_surface_isolated_t_width =
		trace.surface_isolated_min_t_width;
	else
	    summary.minimum_surface_isolated_t_width = std::min(
		summary.minimum_surface_isolated_t_width,
		(double)trace.surface_isolated_min_t_width);
	summary.maximum_surface_isolated_t_width = std::max(
	    summary.maximum_surface_isolated_t_width,
	    (double)trace.surface_isolated_max_t_width);
	summary.surface_isolated_boxes += trace.surface_isolated_boxes;
    }
}


static void
brep_print_prepared_event_summary(const char *label,
	const brep_root_event_summary &summary)
{
    std::printf("%s prepared candidate cleanup: hits=%zu failures=%zu "
	"overflow=%zu count-stage-differences=%zu "
	"semantic-stage-differences=%zu[%zu/%zu/%zu/%zu/%zu] "
	"cleaned-hit-differences=%zu\n", label,
	summary.local_candidate_hits, summary.local_candidate_failures,
	summary.local_candidate_overflow,
	summary.local_candidate_stage_mismatches,
	summary.local_candidate_semantic_stage_mismatches,
	summary.local_candidate_semantic_stage[0],
	summary.local_candidate_semantic_stage[1],
	summary.local_candidate_semantic_stage[2],
	summary.local_candidate_semantic_stage[3],
	summary.local_candidate_semantic_stage[4],
	summary.local_candidate_hit_mismatches);
    std::printf("%s prepared physical cleanup: groups=%zu contacts=%zu "
	"clean-misses=%zu hits=%zu failures=%zu overflow=%zu "
	"count-stage-differences=%zu cleaned-hit-differences="
	"%zu[%zu/%zu/%zu/%zu/%zu/%zu/%zu/%zu] repairs=%zu "
	"segments=%zu partition-changes=%zu\n", label,
	summary.local_event_groups, summary.local_event_contacts,
	summary.local_event_clean_misses, summary.local_event_hits,
	summary.local_event_failures, summary.local_event_overflow,
	summary.local_event_stage_mismatches,
	summary.local_event_hit_mismatches,
	summary.local_event_count_mismatches,
	summary.local_event_t_mismatches,
	summary.local_event_face_mismatches,
	summary.local_event_trim_mismatches,
	summary.local_event_edge_mismatches,
	summary.local_event_class_mismatches,
	summary.local_event_direction_mismatches,
	summary.local_event_adjacency_mismatches,
	summary.local_event_repaired,
	summary.local_event_final_segments,
	summary.local_event_final_mismatches);
    std::printf("%s prepared adaptive isolation: leaf-width=%.6g/%.6g "
	"corrector=%zu/%zu krawczyk=%zu depth=%zu/%zu "
	"subdivision=%zu/%zu clip=%zu/%zu/%zu+%zu/%.3g\n", label,
	summary.minimum_surface_isolated_t_width,
	summary.maximum_surface_isolated_t_width,
	summary.surface_corrector_converged,
	summary.surface_corrector_attempts,
	summary.surface_krawczyk_boxes,
	summary.surface_krawczyk_min_depth,
	summary.surface_krawczyk_max_depth,
	summary.surface_subdivision_boxes,
	summary.surface_subdivision_max_boxes,
	summary.surface_clip_contractions,
	summary.surface_clip_attempts,
	summary.surface_clip_inconclusive,
	summary.surface_clip_restriction_failures,
	summary.surface_clip_max_fraction_removed);
    std::printf("%s prepared production: selected=%zu/%zu/%zu "
	"fallback=none:%zu non-solid:%zu plate:%zu unsupported:%zu "
	"surface-work:%zu boxes:%zu uncertified:%zu local-work:%zu "
	"coverage:%zu event:%zu hit-build:%zu hit-work:%zu partition:%zu\n",
	label, summary.prepared_production_selected,
	summary.prepared_production_eligible,
	summary.prepared_production_attempts,
	summary.prepared_production_fallback[0],
	summary.prepared_production_fallback[1],
	summary.prepared_production_fallback[2],
	summary.prepared_production_fallback[3],
	summary.prepared_production_fallback[4],
	summary.prepared_production_fallback[5],
	summary.prepared_production_fallback[6],
	summary.prepared_production_fallback[7],
	summary.prepared_production_fallback[8],
	summary.prepared_production_fallback[9],
	summary.prepared_production_fallback[10],
	summary.prepared_production_fallback[11],
	summary.prepared_production_fallback[12]);
}


static size_t
brep_trace_unique_local_roots(const struct rt_brep_shot_trace &trace,
    double tolerance)
{
    std::vector<double> distances;
    distances.reserve(trace.stored_local_roots);
    for (size_t i = 0; i < trace.stored_local_roots; ++i)
	distances.push_back(trace.local_roots[i].dist);
    std::sort(distances.begin(), distances.end());
    size_t unique = 0;
    double previous = 0.0;
    for (size_t i = 0; i < distances.size(); ++i) {
	if (!unique || fabs(distances[i] - previous) > tolerance) {
	    unique++;
	    previous = distances[i];
	}
    }
    return unique;
}


static bool
brep_trace_local_root_near(const struct rt_brep_shot_trace &trace,
    double distance, double tolerance)
{
    for (size_t i = 0; i < trace.stored_local_roots; ++i) {
	if (fabs(trace.local_roots[i].dist - distance) <= tolerance)
	    return true;
    }
    return false;
}


static double
brep_trace_local_root_error(const struct rt_brep_shot_trace &trace,
    double distance)
{
    double error = INFINITY;
    for (size_t i = 0; i < trace.stored_local_roots; ++i)
	error = std::min(error, fabs(trace.local_roots[i].dist - distance));
    return error;
}


static std::vector<double>
grazing_clearances(double radius, double distance_tolerance)
{
    std::vector<double> clearances;
    for (int exponent = 1; exponent <= 50; ++exponent)
	clearances.push_back(std::ldexp(radius, -exponent));

    const double chord_ratios[] = {100.0, 10.0, 2.0, 1.1, 1.0, 0.9,
	0.5, 0.1, 0.01};
    for (size_t i = 0; i < sizeof(chord_ratios) / sizeof(chord_ratios[0]);
	    ++i) {
	const double half_chord = chord_ratios[i] * distance_tolerance * 0.5;
	const double root = sqrt(std::max(0.0,
	    radius * radius - half_chord * half_chord));
	clearances.push_back((half_chord * half_chord) / (radius + root));
    }

    std::sort(clearances.begin(), clearances.end(), std::greater<double>());
    clearances.erase(std::unique(clearances.begin(), clearances.end()),
	clearances.end());
    return clearances;
}


static void
grazing_report(struct soltab *implicit_stp, struct soltab *brep_stp,
    struct rt_i *rtip, struct resource *resp, double radius)
{
    std::vector<double> clearances = grazing_clearances(radius,
	rtip->rti_tol.dist);

    std::printf("# signed sphere grazing report\n");
    std::printf("# h/R chord/tol implicit implicit_chord BREP BREP_chord "
	"BREP_endpoint_error\n");

    vect_t direction = {1.0, 0.0, 0.0};
    for (int sign_value = 1; sign_value >= -1; --sign_value) {
	if (sign_value == 0) {
	    point_t origin = {-2.0 * radius, radius, 0.0};
	    ray_result implicit_result = shoot_solid(implicit_stp, rtip, resp,
		origin, direction);
	    ray_result brep_result = shoot_solid(brep_stp, rtip, resp, origin,
		direction);
	    std::printf("% .17g % .17g %-8s % .17g %-8s % .17g % .17g\n",
		0.0, 0.0, ray_class(implicit_result, rtip->rti_tol.dist),
		implicit_result.out_dist - implicit_result.in_dist,
		ray_class(brep_result, rtip->rti_tol.dist),
		brep_result.out_dist - brep_result.in_dist, 0.0);
	    continue;
	}
	for (size_t i = 0; i < clearances.size(); ++i) {
	    const double h = sign_value * clearances[i];
	    const double b = radius - h;
	    point_t origin = {-2.0 * radius, b, 0.0};
	    ray_result implicit_result = shoot_solid(implicit_stp, rtip, resp,
		origin, direction);
	    ray_result brep_result = shoot_solid(brep_stp, rtip, resp, origin,
		direction);
	    const double analytic_chord = h > 0.0 ?
		2.0 * sqrt(std::max(0.0, 2.0 * radius * h - h * h)) : 0.0;
	    const double brep_chord = brep_result.segments == 1 ?
		brep_result.out_dist - brep_result.in_dist : 0.0;
	    double endpoint_error = 0.0;
	    if (h > 0.0 && brep_result.segments == 1) {
		const double expected_in = 2.0 * radius - analytic_chord * 0.5;
		const double expected_out = 2.0 * radius + analytic_chord * 0.5;
		endpoint_error = std::max(fabs(brep_result.in_dist - expected_in),
		    fabs(brep_result.out_dist - expected_out));
	    }
	    std::printf("% .17g % .17g %-8s % .17g %-8s % .17g % .17g\n",
		h / radius, analytic_chord / rtip->rti_tol.dist,
		ray_class(implicit_result, rtip->rti_tol.dist),
		implicit_result.segments == 1 ? implicit_result.out_dist -
		implicit_result.in_dist : 0.0,
		ray_class(brep_result, rtip->rti_tol.dist), brep_chord,
		endpoint_error);
	}
    }
}


static int
check_grazing_ratchet(struct soltab *implicit_stp, struct soltab *brep_stp,
    struct rt_i *rtip, struct resource *resp, double radius)
{
    int failures = 0;
    bool brep_interval_ended = false;
    double previous_brep_chord = INFINITY;
    size_t maximum_isolation_boxes = 0;
    size_t maximum_isolated_boxes = 0;
    size_t maximum_workspace = 0;
    size_t outside_ambiguous = 0;
    double largest_outside_ambiguous = 0.0;
    size_t maximum_local_attempts = 0;
    size_t maximum_local_failures = 0;
    size_t maximum_local_duplicates = 0;
    size_t maximum_fixed_leaves = 0;
    size_t maximum_fixed_hits = 0;
    size_t resolved_local_misses = 0;
    size_t resolved_local_cases = 0;
    size_t subtolerance_local_contacts = 0;
    size_t subtolerance_local_misses = 0;
    size_t subtolerance_local_invalid = 0;
    double maximum_local_endpoint_error = 0.0;
    size_t outside_local_candidates = 0;
    size_t outside_local_invalid = 0;
    size_t legacy_roots_without_local = 0;
    size_t invalid_legacy_roots_without_local = 0;
    size_t outside_legacy_only_contacts = 0;
    size_t local_roots_without_legacy = 0;
    brep_root_event_summary root_events;
    double largest_outside_local_candidate = 0.0;
    vect_t direction = {1.0, 0.0, 0.0};
    const std::vector<double> clearances = grazing_clearances(radius,
	rtip->rti_tol.dist);

    /* Resolved intervals must agree with the analytic oracle.  Below the
     * model distance tolerance, either a contact-sized segment or no segment
     * is acceptable, but an interval must never restart nearer the tangent. */
    for (size_t i = 0; i < clearances.size(); ++i) {
	const double h = clearances[i];
	const double b = radius - h;
	if (!(b < radius))
	    continue;
	point_t origin = {-2.0 * radius, b, 0.0};
	const double analytic_chord =
	    2.0 * sqrt(std::max(0.0, 2.0 * radius * h - h * h));
	ray_result implicit_result = shoot_solid(implicit_stp, rtip, resp,
	    origin, direction);
	ray_result brep_result = shoot_solid(brep_stp, rtip, resp, origin,
	    direction);
	ray_result repeated_result = shoot_solid(brep_stp, rtip, resp, origin,
	    direction);
	sampled_ray trace_ray;
	VMOVE(trace_ray.origin, origin);
	VMOVE(trace_ray.direction, direction);
	struct rt_brep_shot_trace trace;
	(void)shoot_brep_trace(brep_stp, rtip, resp, trace_ray, trace);
	maximum_fixed_leaves = std::max(maximum_fixed_leaves,
	    trace.fixed_leaf_count);
	maximum_fixed_hits = std::max(maximum_fixed_hits,
	    trace.fixed_hit_count);
	maximum_isolation_boxes = std::max(maximum_isolation_boxes,
	    trace.surface_subdivision_boxes);
	maximum_isolated_boxes = std::max(maximum_isolated_boxes,
	    trace.surface_isolated_boxes);
	maximum_workspace = std::max(maximum_workspace,
	    trace.surface_workspace_high_water);
	maximum_local_attempts = std::max(maximum_local_attempts,
	    trace.local_root_attempts);
	maximum_local_failures = std::max(maximum_local_failures,
	    trace.local_root_failures);
	maximum_local_duplicates = std::max(maximum_local_duplicates,
	    trace.local_root_duplicates);
	legacy_roots_without_local += trace.legacy_unique_roots_unmatched;
	invalid_legacy_roots_without_local +=
	    trace.legacy_unique_roots_unmatched;
	local_roots_without_legacy += trace.local_unique_roots_unmatched;
	brep_accumulate_root_events(root_events, trace);
	if (trace.legacy_unique_roots_unmatched) {
	    std::printf("Inside root coverage h/T=%.17g\n",
		h / rtip->rti_tol.dist);
	    brep_trace_root_coverage_diagnostic("inside", trace);
	}
	const double expected_in = 2.0 * radius - analytic_chord * 0.5;
	const double expected_out = 2.0 * radius + analytic_chord * 0.5;
	const bool roots_covered = brep_trace_covers_t(trace, expected_in,
	    1.0e-7) && brep_trace_covers_t(trace, expected_out, 1.0e-7);
	const bool roots_separated = !brep_trace_box_covers_both(trace,
	    expected_in, expected_out, 1.0e-7);
	const bool local_roots_covered = brep_trace_local_root_near(trace,
	    expected_in, 1.0e-7) && brep_trace_local_root_near(trace,
	    expected_out, 1.0e-7);
	if (analytic_chord >= rtip->rti_tol.dist) {
	    resolved_local_cases++;
	    maximum_local_endpoint_error = std::max(
		maximum_local_endpoint_error,
		std::max(brep_trace_local_root_error(trace, expected_in),
		brep_trace_local_root_error(trace, expected_out)));
	    if (!local_roots_covered ||
		    brep_trace_unique_local_roots(trace,
		    0.1 * rtip->rti_tol.dist) != 2 ||
		    trace.stored_local_clusters != 2 ||
		    trace.local_clusters[0].classification !=
		    RT_BREP_TRACE_ENTERING ||
		    trace.local_clusters[1].classification !=
		    RT_BREP_TRACE_LEAVING) {
		resolved_local_misses++;
		std::printf("Local grazing diagnostic h/T=%.17g chord/T=%.17g "
		    "attempts=%zu candidates=%zu failures=%zu unique=%zu "
		    "expected=%.17g/%.17g\n", h / rtip->rti_tol.dist,
		    analytic_chord / rtip->rti_tol.dist,
		    trace.local_root_attempts, trace.local_root_candidates,
		    trace.local_root_failures,
		    brep_trace_unique_local_roots(trace, 1.0e-7),
		    expected_in, expected_out);
		for (size_t local_index = 0;
			local_index < trace.stored_local_roots; ++local_index) {
		    const struct rt_brep_trace_local_root &root =
			trace.local_roots[local_index];
		    std::printf("  local root %zu face=%d span=%d t=%.17g "
			"uv=%.17g/%.17g residual=%.17g normal=%.17g\n",
			local_index, root.face_index, root.span_index, root.dist,
			root.uv[0], root.uv[1], root.residual,
			root.normal_dot);
		}
		for (size_t box_index = 0;
			box_index < trace.stored_surface_boxes; ++box_index) {
		    const struct rt_brep_trace_surface_box &box =
			trace.surface_boxes[box_index];
		    std::printf("  local box %zu face=%d span=%d "
			"uv=%.17g/%.17g %.17g/%.17g t=%.17g/%.17g\n",
			box_index, box.face_index, box.span_index,
			box.uv_min[0], box.uv_max[0], box.uv_min[1],
			box.uv_max[1], box.t_min, box.t_max);
		}
	    }
	} else {
	    const size_t contact_clusters = brep_trace_unique_local_roots(trace,
		rtip->rti_tol.dist);
	    if (contact_clusters)
		subtolerance_local_contacts++;
	    else
		subtolerance_local_misses++;
	    bool local_outside_contact = contact_clusters > 1;
	    for (size_t local_index = 0;
		    local_index < trace.stored_local_roots; ++local_index) {
		if (fabs(trace.local_roots[local_index].dist - 2.0 * radius) >
			rtip->rti_tol.dist)
		    local_outside_contact = true;
	    }
	    if (local_outside_contact)
		subtolerance_local_invalid++;
	}
	if (!brep_trace_fixed_workspaces_match(trace) ||
		!trace.supported_surface_faces ||
		trace.candidate_surface_spans + trace.excluded_surface_spans !=
		trace.prepared_surface_spans || trace.surface_workspace_exhausted ||
		trace.surface_box_overflow ||
		trace.surface_clip_restriction_failures ||
		trace.surface_clip_max_fraction_removed >
		0.5 + 64.0 * DBL_EPSILON ||
		trace.local_root_overflow ||
		trace.local_cluster_overflow ||
		trace.local_root_candidates != trace.stored_local_roots ||
		trace.local_root_clusters != trace.stored_local_clusters ||
		trace.local_root_attempts != trace.local_root_candidates +
		trace.local_root_failures + trace.local_root_duplicates ||
		trace.surface_isolated_boxes != trace.stored_surface_boxes ||
		trace.final_segments != (size_t)brep_result.segments ||
		(analytic_chord >= rtip->rti_tol.dist &&
		 (trace.prepared_production_selected ||
		  trace.prepared_production_fallback !=
		    RT_BREP_PREPARED_FALLBACK_UNCERTIFIED)) ||
		(analytic_chord >= rtip->rti_tol.dist && !roots_covered) ||
		(analytic_chord >= 10.0 * rtip->rti_tol.dist &&
		!roots_separated)) {
	    std::printf("FAIL: grazing isolation h/R=%.17g spans=%zu/%zu "
		"boxes=%zu/%zu workspace=%zu+%zu clip=%zu/%zu/%zu/%.3g "
		"covered=%d separated=%d\n",
		h / radius, trace.candidate_surface_spans,
		trace.prepared_surface_spans, trace.surface_subdivision_boxes,
		trace.surface_isolated_boxes,
		trace.surface_workspace_high_water,
		trace.surface_workspace_exhausted,
		trace.surface_clip_contractions, trace.surface_clip_attempts,
		trace.surface_clip_restriction_failures,
		trace.surface_clip_max_fraction_removed, roots_covered,
		roots_separated);
	    failures++;
	}

	if (brep_result.segments != repeated_result.segments ||
	    (brep_result.segments == 1 &&
	    (std::memcmp(&brep_result.in_dist, &repeated_result.in_dist,
		sizeof(double)) != 0 ||
	    std::memcmp(&brep_result.out_dist, &repeated_result.out_dist,
		sizeof(double)) != 0))) {
	    std::printf("FAIL: grazing h/R=%.17g is nondeterministic\n",
		h / radius);
	    failures++;
	}

	if (brep_result.segments == 1) {
	    const double brep_chord = brep_result.out_dist -
		brep_result.in_dist;
	    if (brep_interval_ended) {
		std::printf("FAIL: grazing interval restarted at h/R=%.17g\n",
		    h / radius);
		failures++;
	    }
	    if (brep_chord > previous_brep_chord + rtip->rti_tol.dist) {
		std::printf("FAIL: grazing chord grew at h/R=%.17g: "
		    "%.17g > %.17g\n", h / radius, brep_chord,
		    previous_brep_chord);
		failures++;
	    }
	    previous_brep_chord = brep_chord;
	} else {
	    brep_interval_ended = true;
	}

	if (analytic_chord + rtip->rti_tol.dist * 1.0e-6 >=
		rtip->rti_tol.dist) {
	    if (implicit_result.segments != 1 || brep_result.segments != 1 ||
		    fabs(brep_result.in_dist - expected_in) > rtip->rti_tol.dist ||
		    fabs(brep_result.out_dist - expected_out) > rtip->rti_tol.dist) {
		std::printf("FAIL: resolved grazing interval h/R=%.17g "
		    "implicit=%d BREP=%d\n", h / radius,
		    implicit_result.segments, brep_result.segments);
		failures++;
	    }
	} else if (brep_result.segments > 1 ||
		(brep_result.segments == 1 &&
		brep_result.out_dist - brep_result.in_dist >
		rtip->rti_tol.dist + 1.0e-9)) {
	    std::printf("FAIL: sub-tolerance grazing material h/R=%.17g "
		"BREP=%d chord=%.17g\n", h / radius,
		brep_result.segments, brep_result.out_dist -
		brep_result.in_dist);
	    failures++;
	}
    }

    std::printf("Sphere grazing isolation: max-boxes=%zu max-isolated=%zu "
	"workspace-high-water=%zu local-attempts=%zu local-failures=%zu "
	"local-duplicates=%zu "
	"resolved-local-misses=%zu/%zu max-local-endpoint=%.3g "
	"sub-T-contact/miss/invalid=%zu/%zu/%zu\n",
	maximum_isolation_boxes,
	maximum_isolated_boxes, maximum_workspace, maximum_local_attempts,
	maximum_local_failures, maximum_local_duplicates,
	resolved_local_misses, resolved_local_cases,
	maximum_local_endpoint_error, subtolerance_local_contacts,
	subtolerance_local_misses, subtolerance_local_invalid);
    if (resolved_local_misses) {
	std::printf("FAIL: bounded local roots missed %zu resolved sphere "
	    "grazing cases\n", resolved_local_misses);
	failures++;
    }
    if (subtolerance_local_invalid) {
	std::printf("FAIL: %zu sub-tolerance grazing cases did not collapse "
	    "to a contact-sized local cluster\n", subtolerance_local_invalid);
	failures++;
    }

    /* Exact tangency and every representable mirrored outside clearance must
     * be misses for both the analytic and converted representations. */
    point_t tangent_origin = {-2.0 * radius, radius, 0.0};
    ray_result implicit_tangent = shoot_solid(implicit_stp, rtip, resp,
	tangent_origin, direction);
    ray_result brep_tangent = shoot_solid(brep_stp, rtip, resp,
	tangent_origin, direction);
    sampled_ray tangent_trace_ray;
    VMOVE(tangent_trace_ray.origin, tangent_origin);
    VMOVE(tangent_trace_ray.direction, direction);
    struct rt_brep_shot_trace tangent_trace;
    (void)shoot_brep_trace(brep_stp, rtip, resp, tangent_trace_ray,
	tangent_trace);
    legacy_roots_without_local +=
	tangent_trace.legacy_unique_roots_unmatched;
    invalid_legacy_roots_without_local +=
	tangent_trace.legacy_unique_roots_unmatched;
    local_roots_without_legacy +=
	tangent_trace.local_unique_roots_unmatched;
    brep_accumulate_root_events(root_events, tangent_trace);
    brep_trace_root_coverage_diagnostic("exact tangent", tangent_trace);
    maximum_fixed_leaves = std::max(maximum_fixed_leaves,
	tangent_trace.fixed_leaf_count);
    maximum_fixed_hits = std::max(maximum_fixed_hits,
	tangent_trace.fixed_hit_count);
    if (implicit_tangent.segments != 0 || brep_tangent.segments != 0) {
	std::printf("FAIL: exact tangent did not miss: implicit=%d BREP=%d\n",
	    implicit_tangent.segments, brep_tangent.segments);
	failures++;
    }
    if (!brep_trace_fixed_workspaces_match(tangent_trace) ||
	    !brep_trace_covers_t(tangent_trace, 2.0 * radius, 1.0e-7) ||
	    tangent_trace.surface_workspace_exhausted ||
	    tangent_trace.surface_box_overflow ||
	    tangent_trace.surface_clip_restriction_failures ||
	    tangent_trace.surface_clip_max_fraction_removed >
	    0.5 + 64.0 * DBL_EPSILON ||
	    tangent_trace.prepared_production_selected ||
	    tangent_trace.prepared_production_fallback !=
	    RT_BREP_PREPARED_FALLBACK_UNCERTIFIED ||
	    tangent_trace.local_root_overflow ||
	    tangent_trace.local_cluster_overflow ||
	    tangent_trace.stored_local_clusters != 1 ||
	    tangent_trace.local_clusters[0].classification !=
	    RT_BREP_TRACE_LOCAL_CONTACT ||
	    tangent_trace.local_root_attempts !=
	    tangent_trace.local_root_candidates +
	    tangent_trace.local_root_failures +
	    tangent_trace.local_root_duplicates) {
	std::printf("FAIL: exact tangent isolation boxes=%zu workspace=%zu+%zu\n",
	    tangent_trace.surface_isolated_boxes,
	    tangent_trace.surface_workspace_high_water,
	    tangent_trace.surface_workspace_exhausted);
	failures++;
    }
    std::printf("Sphere tangent local roots: attempts=%zu candidates=%zu "
	"failures=%zu contact-clusters=%zu\n",
	tangent_trace.local_root_attempts,
	tangent_trace.local_root_candidates, tangent_trace.local_root_failures,
	brep_trace_unique_local_roots(tangent_trace,
	0.1 * rtip->rti_tol.dist));
    if (!brep_trace_local_root_near(tangent_trace, 2.0 * radius,
	    0.1 * rtip->rti_tol.dist) ||
	    brep_trace_unique_local_roots(tangent_trace,
	    0.1 * rtip->rti_tol.dist) != 1) {
	std::printf("FAIL: bounded local roots did not form one tangent "
	    "contact cluster\n");
	failures++;
    }

    for (size_t i = 0; i < clearances.size(); ++i) {
	const double h = -clearances[i];
	point_t origin = {-2.0 * radius, radius - h, 0.0};
	if (!(origin[Y] > radius))
	    continue;
	ray_result implicit_result = shoot_solid(implicit_stp, rtip, resp,
	    origin, direction);
	ray_result brep_result = shoot_solid(brep_stp, rtip, resp, origin,
	    direction);
	sampled_ray trace_ray;
	VMOVE(trace_ray.origin, origin);
	VMOVE(trace_ray.direction, direction);
	struct rt_brep_shot_trace trace;
	(void)shoot_brep_trace(brep_stp, rtip, resp, trace_ray, trace);
	legacy_roots_without_local += trace.legacy_unique_roots_unmatched;
	local_roots_without_legacy += trace.local_unique_roots_unmatched;
	if (trace.legacy_unique_roots_unmatched) {
	    bool valid_legacy_contact =
		-h <= 1.0e-6 * rtip->rti_tol.dist &&
		trace.legacy_unique_roots == 1 &&
		trace.legacy_unique_roots_unmatched == 1 &&
		trace.local_unique_roots == 0 && trace.final_segments == 0;
	    for (size_t root_index = 0; root_index < trace.stored_roots;
		    ++root_index) {
		const struct rt_brep_trace_root &root = trace.roots[root_index];
		if (fabs(root.dist - 2.0 * radius) >
			0.1 * rtip->rti_tol.dist ||
			fabs(root.normal_dot) > BREP_GRAZING_DOT_TOL)
		    valid_legacy_contact = false;
	    }
	    if (valid_legacy_contact) {
		outside_legacy_only_contacts +=
		    trace.legacy_unique_roots_unmatched;
	    } else {
		invalid_legacy_roots_without_local +=
		    trace.legacy_unique_roots_unmatched;
		std::printf("Outside invalid legacy-only root h/T=%.17g\n",
		    h / rtip->rti_tol.dist);
		brep_trace_root_coverage_diagnostic("outside legacy-only",
		    trace);
	    }
	}
	brep_accumulate_root_events(root_events, trace);
	maximum_fixed_leaves = std::max(maximum_fixed_leaves,
	    trace.fixed_leaf_count);
	maximum_fixed_hits = std::max(maximum_fixed_hits,
	    trace.fixed_hit_count);
	if (!brep_trace_fixed_workspaces_match(trace) ||
		trace.surface_workspace_exhausted || trace.surface_box_overflow ||
		trace.surface_clip_restriction_failures ||
		trace.surface_clip_max_fraction_removed >
		0.5 + 64.0 * DBL_EPSILON ||
		trace.local_root_overflow ||
		trace.local_cluster_overflow ||
		trace.local_root_clusters != trace.stored_local_clusters ||
		trace.local_root_attempts != trace.local_root_candidates +
		trace.local_root_failures + trace.local_root_duplicates) {
	    std::printf("FAIL: outside grazing isolation overflow h/R=%.17g\n",
		h / radius);
	    failures++;
	}
	if (trace.surface_isolated_boxes) {
	    outside_ambiguous++;
	    largest_outside_ambiguous = std::max(largest_outside_ambiguous,
		-h);
	}
	if (trace.local_root_candidates) {
	    outside_local_candidates += trace.local_root_candidates;
	    largest_outside_local_candidate = std::max(
		largest_outside_local_candidate, -h);
	    bool valid_contact = trace.local_unique_roots == 1 &&
		trace.stored_local_clusters == 1 &&
		trace.local_clusters[0].classification ==
		RT_BREP_TRACE_LOCAL_CONTACT && trace.final_segments == 0;
	    for (size_t local_index = 0;
		    local_index < trace.stored_local_roots; ++local_index) {
		if (fabs(trace.local_roots[local_index].dist -
			2.0 * radius) > 0.1 * rtip->rti_tol.dist)
		    valid_contact = false;
	    }
	    if (!valid_contact) {
		outside_local_invalid++;
		std::printf("Outside root coverage h/T=%.17g\n",
		    h / rtip->rti_tol.dist);
		brep_trace_root_coverage_diagnostic("outside", trace);
	    }
	}
	if (implicit_result.segments != 0 || brep_result.segments != 0) {
	    std::printf("FAIL: outside grazing h/R=%.17g did not miss: "
		"implicit=%d BREP=%d\n", h / radius,
		implicit_result.segments, brep_result.segments);
	    failures++;
	}
    }
    std::printf("Sphere grazing miss-side isolation: ambiguous=%zu "
	"largest-h/T=%.9g local-candidates=%zu local-invalid=%zu "
	"largest-local-h/T=%.9g\n",
	outside_ambiguous, largest_outside_ambiguous / rtip->rti_tol.dist,
	outside_local_candidates, outside_local_invalid,
	largest_outside_local_candidate / rtip->rti_tol.dist);
    if (largest_outside_ambiguous > 1.0e-6 * rtip->rti_tol.dist) {
	std::printf("FAIL: miss-side isolation ambiguity spread to h/T=%.17g\n",
	    largest_outside_ambiguous / rtip->rti_tol.dist);
	failures++;
    }
    if (largest_outside_local_candidate >
	    1.0e-6 * rtip->rti_tol.dist) {
	std::printf("FAIL: miss-side local roots spread to h/T=%.17g\n",
	    largest_outside_local_candidate / rtip->rti_tol.dist);
	failures++;
    }
    if (outside_local_invalid) {
	std::printf("FAIL: %zu miss-side local roots were not contact-only\n",
	    outside_local_invalid);
	failures++;
    }

    brep_root_event_summary interior_events;
    const ON_3dVector interior_directions[] = {
	ON_3dVector(0.371, 0.529, 0.763),
	ON_3dVector(-0.613, 0.247, 0.751),
	ON_3dVector(0.193, -0.881, 0.432)
    };
    for (size_t direction_index = 0; direction_index <
	    sizeof(interior_directions) / sizeof(interior_directions[0]);
	    ++direction_index) {
	ON_3dVector ray_direction = interior_directions[direction_index];
	ON_3dVector first = ON_CrossProduct(ray_direction,
	    ON_3dVector(0.0, 0.0, 1.0));
	if (!ray_direction.Unitize() || !first.Unitize()) {
	    failures++;
	    continue;
	}
	ON_3dVector second = ON_CrossProduct(ray_direction, first);
	if (!second.Unitize()) {
	    failures++;
	    continue;
	}
	const ON_3dPoint closest = 0.31 * radius * first +
	    0.17 * radius * second;
	for (int reverse = 0; reverse <= 1; ++reverse) {
	    const ON_3dVector direction_vector = reverse ?
		-ray_direction : ray_direction;
	    const ON_3dPoint ray_origin = closest +
		(reverse ? 2.0 : -2.0) * radius * ray_direction;
	    sampled_ray ray;
	    VSET(ray.origin, ray_origin.x, ray_origin.y, ray_origin.z);
	    VSET(ray.direction, direction_vector.x, direction_vector.y,
		direction_vector.z);
	    const ray_result implicit_result = shoot_solid(implicit_stp, rtip,
		resp, ray.origin, ray.direction);
	    const ray_result production_result = shoot_solid(brep_stp, rtip,
		resp, ray.origin, ray.direction);
	    const ray_result legacy_result = shoot_brep_legacy(brep_stp, rtip,
		resp, ray.origin, ray.direction);
	    struct rt_brep_shot_trace trace;
	    (void)shoot_brep_trace(brep_stp, rtip, resp, ray, trace);
	    brep_accumulate_root_events(interior_events, trace);
	    const bool prepared_matches = implicit_result.segments == 1 &&
		trace.local_event_final_segments == 1 &&
		trace.local_event_stored_segments == 1 &&
		fabs(trace.local_event_segment_in[0] -
		implicit_result.in_dist) <= rtip->rti_tol.dist &&
		fabs(trace.local_event_segment_out[0] -
		implicit_result.out_dist) <= rtip->rti_tol.dist;
	    if (!brep_trace_fixed_workspaces_match(trace) ||
		production_result.segments != 1 ||
		legacy_result.segments != 1 || !prepared_matches ||
		fabs(production_result.in_dist - implicit_result.in_dist) >
		rtip->rti_tol.dist ||
		fabs(production_result.out_dist - implicit_result.out_dist) >
		rtip->rti_tol.dist ||
		fabs(legacy_result.in_dist - implicit_result.in_dist) >
		rtip->rti_tol.dist ||
		fabs(legacy_result.out_dist - implicit_result.out_dist) >
		rtip->rti_tol.dist ||
		trace.legacy_unique_roots_unmatched ||
		trace.local_unique_roots_unmatched ||
		trace.root_event_mismatches ||
		trace.local_event_final_mismatches ||
		trace.prepared_production_selected != 1 ||
		trace.prepared_production_fallback !=
		RT_BREP_PREPARED_FALLBACK_NONE ||
		trace.surface_krawczyk_boxes != 2 ||
		!trace.surface_clip_contractions ||
		trace.surface_clip_restriction_failures ||
		trace.surface_clip_max_fraction_removed >
		0.5 + 64.0 * DBL_EPSILON ||
		trace.surface_subdivision_max_depth >= 24 ||
		trace.surface_subdivision_boxes > 80) {
		std::printf("FAIL: oblique interior sphere ray %zu/%d "
		    "implicit/production/legacy/prepared=%d/%d/%d/%zu "
		    "roots=%zu/%zu "
		    "events=%zu partitions=%zu krawczyk=%zu depth=%zu "
		    "boxes=%zu\n", direction_index, reverse,
		    implicit_result.segments, production_result.segments,
		    legacy_result.segments,
		    trace.local_event_final_segments,
		    trace.legacy_unique_roots_unmatched,
		    trace.local_unique_roots_unmatched,
		    trace.root_event_mismatches,
		    trace.local_event_final_mismatches,
		    trace.surface_krawczyk_boxes,
		    trace.surface_subdivision_max_depth,
		    trace.surface_subdivision_boxes);
		failures++;
	    }
	}
    }
    brep_print_prepared_event_summary("Sphere interior", interior_events);
    std::printf("Sphere fixed workspaces: leaves=%zu/%d raw-hits=%zu/%d\n",
	maximum_fixed_leaves, RT_BREP_MAX_LEAVES, maximum_fixed_hits,
	RT_BREP_MAX_HITS);
    std::printf("Sphere prepared-span root coverage: "
	"legacy-unmatched=%zu/%zu/%zu direct-only=%zu "
	"events=%zu mismatched=%zu/%zu/%zu/%zu/%zu "
	"max-errors=%.3g/%.3g/%.3g/%.3g trims=%zu/%zu "
	"face-trims=%zu/%zu/%zu/%.3g\n", legacy_roots_without_local,
	outside_legacy_only_contacts, invalid_legacy_roots_without_local,
	local_roots_without_legacy, root_events.matched,
	root_events.mismatched, root_events.trim_status_mismatches,
	root_events.hit_class_mismatches, root_events.direction_mismatches,
	root_events.adjacency_mismatches, root_events.maximum_t_error,
	root_events.maximum_uv_error, root_events.maximum_trim_error,
	root_events.maximum_normal_dot_error, root_events.local_trim_queries,
	root_events.local_trim_candidates, root_events.face_trim_queries,
	root_events.face_trim_candidates, root_events.face_trim_mismatches,
	root_events.maximum_face_trim_error);
    brep_print_prepared_event_summary("Sphere", root_events);
    if (invalid_legacy_roots_without_local) {
	std::printf("FAIL: prepared spans missed %zu valid legacy sphere roots\n",
	    invalid_legacy_roots_without_local);
	failures++;
    }
    if (root_events.mismatched) {
	std::printf("FAIL: %zu matched sphere roots changed event class\n",
	    root_events.mismatched);
	failures++;
    }
    if (!root_events.surface_clip_contractions ||
	    root_events.surface_clip_restriction_failures ||
	    root_events.surface_clip_max_fraction_removed >
	    0.5 + 64.0 * DBL_EPSILON) {
	std::printf("FAIL: grazing clipping contractions=%zu attempts=%zu "
	    "inconclusive=%zu restriction-failures=%zu max-removed=%.3g\n",
	    root_events.surface_clip_contractions,
	    root_events.surface_clip_attempts,
	    root_events.surface_clip_inconclusive,
	    root_events.surface_clip_restriction_failures,
	    root_events.surface_clip_max_fraction_removed);
	failures++;
    }
    if (root_events.surface_krawczyk_boxes) {
	std::printf("FAIL: %zu grazing sphere boxes terminated adaptively\n",
	    root_events.surface_krawczyk_boxes);
	failures++;
    }
    if (root_events.local_event_failures ||
	    root_events.local_event_overflow ||
	    root_events.local_event_final_mismatches) {
	std::printf("FAIL: prepared sphere event partitions failures=%zu "
	    "overflow=%zu mismatches=%zu\n",
	    root_events.local_event_failures,
	    root_events.local_event_overflow,
	    root_events.local_event_final_mismatches);
	failures++;
    }

    /* The implicit sphere rejects an outward ray beginning on its surface.
     * BREP currently returns the entirely nonpositive segment [-2R, 0].
     * Permit that known result to disappear, but prevent it from becoming
     * nondeterministic or leaking material forward from the ray origin. */
    point_t boundary_origin = {radius, 0.0, 0.0};
    vect_t boundary_direction = {1.0, 0.0, 0.0};
    ray_result implicit_boundary = shoot_solid(implicit_stp, rtip, resp,
	boundary_origin, boundary_direction);
    ray_result brep_boundary = shoot_solid(brep_stp, rtip, resp,
	boundary_origin, boundary_direction);
    ray_result repeated_boundary = shoot_solid(brep_stp, rtip, resp,
	boundary_origin, boundary_direction);
    if (implicit_boundary.segments != 0 ||
	    brep_boundary.segments != repeated_boundary.segments ||
	    (brep_boundary.segments == 1 &&
	    (brep_boundary.out_dist > rtip->rti_tol.dist ||
	    std::memcmp(&brep_boundary.in_dist, &repeated_boundary.in_dist,
		sizeof(double)) != 0 ||
	    std::memcmp(&brep_boundary.out_dist, &repeated_boundary.out_dist,
		sizeof(double)) != 0)) ||
	    brep_boundary.segments > 1) {
	std::printf("FAIL: outward surface-start ratchet implicit=%d BREP=%d "
	    "interval=[%.17g %.17g]\n", implicit_boundary.segments,
	    brep_boundary.segments, brep_boundary.in_dist,
	    brep_boundary.out_dist);
	failures++;
    }

    return failures;
}


static int
check_transformed_sphere(struct rt_i *rtip, struct resource *resp,
    const char *fixture_name, const point_t center, double radius)
{
    int failures = 0;
    struct rt_ell_internal ell = {};
    ell.magic = RT_ELL_INTERNAL_MAGIC;
    VMOVE(ell.v, center);
    VSET(ell.a, radius, 0.0, 0.0);
    VSET(ell.b, 0.0, radius, 0.0);
    VSET(ell.c, 0.0, 0.0, radius);

    struct rt_db_internal ell_intern;
    RT_DB_INTERNAL_INIT(&ell_intern);
    ell_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ell_intern.idb_type = ID_ELL;
    ell_intern.idb_meth = &OBJ[ID_ELL];
    ell_intern.idb_ptr = &ell;

    struct soltab *implicit_stp = prep_solid(rtip, &ell_intern, ID_ELL);
    if (!implicit_stp) {
	std::printf("FAIL: %s implicit prep\n", fixture_name);
	return 1;
    }

    ON_Brep *brep = ON_Brep::New();
    OBJ[ID_ELL].ft_brep(&brep, &ell_intern, &rtip->rti_tol);
    if (!brep) {
	std::printf("FAIL: %s BREP conversion\n", fixture_name);
	free_solid(implicit_stp);
	return 1;
    }
    struct rt_brep_internal brep_internal = {};
    brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
    brep_internal.brep = brep;
    struct rt_db_internal brep_intern;
    RT_DB_INTERNAL_INIT(&brep_intern);
    brep_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    brep_intern.idb_type = ID_BREP;
    brep_intern.idb_meth = &OBJ[ID_BREP];
    brep_intern.idb_ptr = &brep_internal;

    struct soltab *brep_stp = prep_solid(rtip, &brep_intern, ID_BREP);
    if (!brep_stp) {
	std::printf("FAIL: %s BREP prep\n", fixture_name);
	delete brep_internal.brep;
	free_solid(implicit_stp);
	return 1;
    }

    struct transformed_ray {
	const char *name;
	point_t origin;
	vect_t direction;
	double expected_in;
	double expected_out;
    } rays[3];
    rays[0].name = "north pole";
    VSET(rays[0].origin, center[X], center[Y], center[Z] + 2.0 * radius);
    VSET(rays[0].direction, 0.0, 0.0, -1.0);
    rays[0].expected_in = radius;
    rays[0].expected_out = 3.0 * radius;
    rays[1].name = "positive-x seam";
    VSET(rays[1].origin, center[X] + 2.0 * radius, center[Y], center[Z]);
    VSET(rays[1].direction, -1.0, 0.0, 0.0);
    rays[1].expected_in = radius;
    rays[1].expected_out = 3.0 * radius;
    rays[2].name = "inside";
    VMOVE(rays[2].origin, center);
    VSET(rays[2].direction, 0.0, 1.0, 0.0);
    rays[2].expected_in = -radius;
    rays[2].expected_out = radius;

    for (int repeat = 0; repeat < 4; ++repeat) {
	for (size_t i = 0; i < sizeof(rays) / sizeof(rays[0]); ++i) {
	    char label[128];
	    std::snprintf(label, sizeof(label), "%s %s", fixture_name,
		rays[i].name);
	    failures += check_ray(label, implicit_stp, brep_stp, rtip, resp,
		rays[i].origin, rays[i].direction, rays[i].expected_in,
		rays[i].expected_out);
	}
    }

    free_solid(brep_stp);
    free_solid(implicit_stp);
    return failures;
}


static double
relative_error(double observed, double expected)
{
    return fabs(expected) > SMALL_FASTF ? fabs(observed - expected) /
	fabs(expected) : fabs(observed);
}


struct sampled_point {
    point_t value = VINIT_ZERO;
};


static double
paired_rand01(std::mt19937_64 &rng)
{
    return std::generate_canonical<double, 53>(rng);
}


static void
paired_point_on_sphere(double radius, const point_t center, point_t out,
    std::mt19937_64 &rng)
{
    const double theta = 2.0 * M_PI * paired_rand01(rng);
    const double phi = acos(2.0 * paired_rand01(rng) - 1.0);
    const double sin_phi = sin(phi);
    out[X] = center[X] + radius * sin_phi * cos(theta);
    out[Y] = center[Y] + radius * sin_phi * sin(theta);
    out[Z] = center[Z] + radius * cos(phi);
}


static std::vector<sampled_ray>
generate_paired_rays(size_t ray_count, double radius, const point_t center)
{
    static const uint64_t seed = 0x9e3779b97f4a7c15ULL;
    std::mt19937_64 rng(seed);
    std::vector<sampled_point> points(ray_count * 2);
    for (size_t i = 0; i < points.size(); ++i)
	paired_point_on_sphere(radius, center, points[i].value, rng);

    for (size_t i = points.size() - 1; i > 0; --i) {
	size_t j = (size_t)(paired_rand01(rng) * (i + 1));
	if (j > i)
	    j = i;
	std::swap(points[i], points[j]);
    }

    std::vector<sampled_ray> rays(ray_count);
    for (size_t i = 0; i < ray_count; ++i) {
	VMOVE(rays[i].origin, points[2 * i].value);
	VSUB2(rays[i].direction, points[2 * i + 1].value,
	    points[2 * i].value);
	VUNITIZE(rays[i].direction);
    }
    return rays;
}


static double
partition_chord(const partition_result &result)
{
    double chord = 0.0;
    for (size_t i = 0; i < result.partitions; ++i)
	chord += result.intervals[i].out_dist - result.intervals[i].in_dist;
    return chord;
}


static bool
partition_result_valid(const partition_result &result, const vect_t direction)
{
    if (result.overflow)
	return false;
    for (size_t i = 0; i < result.partitions; ++i) {
	const partition_interval &interval = result.intervals[i];
	if (!std::isfinite(interval.in_dist) ||
		!std::isfinite(interval.out_dist) ||
		interval.out_dist < interval.in_dist ||
		!finite_unit_vector(interval.in_normal) ||
		!finite_unit_vector(interval.out_normal) ||
		VDOT(interval.in_normal, direction) >= 0.0 ||
		VDOT(interval.out_normal, direction) <= 0.0)
	    return false;
    }
    return true;
}


static double
sample_standard_error(double sum, double sum_squared, size_t sample_count)
{
    if (sample_count < 2)
	return 0.0;
    const double variance = std::max(0.0,
	(sum_squared - sum * sum / sample_count) / (sample_count - 1));
    return sqrt(variance / sample_count);
}


static int
check_shared_crofton_fixture(const char *label,
    struct rt_db_internal *implicit_intern, const struct bn_tol *tol,
    const point_t bbox_min, const point_t bbox_max, double analytic_area,
    double analytic_volume, size_t ray_count,
    const directed_partition_ray *directed_rays = NULL,
    size_t directed_ray_count = 0,
    prepared_model *supplied_implicit_model = NULL,
    prepared_model *supplied_brep_model = NULL,
    bool enforce_comparison = true, size_t *comparison_issues_out = NULL)
{
    int failures = 0;
    size_t comparison_issues = 0;
    const char *comparison_status = enforce_comparison ? "FAIL" : "KNOWN";
    prepared_model local_implicit_model;
    prepared_model local_brep_model;
    prepared_model &implicit_model = supplied_implicit_model ?
	*supplied_implicit_model : local_implicit_model;
    prepared_model &brep_model = supplied_brep_model ?
	*supplied_brep_model : local_brep_model;

    if ((supplied_implicit_model == NULL) !=
	    (supplied_brep_model == NULL)) {
	std::printf("FAIL: %s incomplete supplied model pair\n", label);
	return 1;
    }

    struct rt_brep_internal brep_internal = {};
    if (!supplied_implicit_model) {
	ON_Brep *brep = ON_Brep::New();
	OBJ[implicit_intern->idb_minor_type].ft_brep(&brep, implicit_intern,
	    tol);
	if (!brep) {
	    std::printf("FAIL: %s shared Crofton BREP conversion\n", label);
	    return 1;
	}

	brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
	brep_internal.brep = brep;
	struct rt_db_internal brep_intern;
	RT_DB_INTERNAL_INIT(&brep_intern);
	brep_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	brep_intern.idb_type = ID_BREP;
	brep_intern.idb_meth = &OBJ[ID_BREP];
	brep_intern.idb_ptr = &brep_internal;

	if (!prep_partition_model(implicit_model, implicit_intern,
		"paired_implicit.s", tol) ||
		!prep_partition_model(brep_model, &brep_intern,
		"paired_brep.s", tol)) {
	    std::printf("FAIL: %s shared Crofton model preparation\n", label);
	    free_prepared_model(brep_model);
	    free_prepared_model(implicit_model);
	    delete brep_internal.brep;
	    return 1;
	}
    }

    for (size_t i = 0; i < directed_ray_count; ++i) {
	sampled_ray ray;
	VMOVE(ray.origin, directed_rays[i].origin);
	VMOVE(ray.direction, directed_rays[i].direction);
	VUNITIZE(ray.direction);
	partition_result implicit_result = shoot_partitions(implicit_model, ray);
	partition_result brep_result = shoot_partitions(brep_model, ray);
	bool valid = partition_result_valid(implicit_result, ray.direction) &&
	    partition_result_valid(brep_result, ray.direction) &&
	    implicit_result.partitions == directed_rays[i].partitions &&
	    brep_result.partitions == directed_rays[i].partitions;
	for (size_t j = 0; valid && j < directed_rays[i].partitions; ++j) {
	    const double expected_in = directed_rays[i].distances[2 * j];
	    const double expected_out = directed_rays[i].distances[2 * j + 1];
	    valid = fabs(implicit_result.intervals[j].in_dist - expected_in) <=
		tol->dist &&
		fabs(implicit_result.intervals[j].out_dist - expected_out) <=
		tol->dist &&
		fabs(brep_result.intervals[j].in_dist - expected_in) <=
		tol->dist &&
		fabs(brep_result.intervals[j].out_dist - expected_out) <=
		tol->dist;
	}
	if (!valid) {
	    std::printf("%s: %s directed partition %s implicit=%zu "
		"BREP=%zu expected=%zu\n", comparison_status, label,
		directed_rays[i].name,
		implicit_result.partitions, brep_result.partitions,
		directed_rays[i].partitions);
	    comparison_issues++;
	}
    }

    const size_t candidate_checkpoints[] = {2000, 8000, 32000, 128000};
    std::vector<size_t> checkpoints;
    for (size_t i = 0; i < sizeof(candidate_checkpoints) /
	    sizeof(candidate_checkpoints[0]); ++i) {
	if (candidate_checkpoints[i] <= ray_count)
	    checkpoints.push_back(candidate_checkpoints[i]);
    }
    if (checkpoints.empty() || checkpoints.back() != ray_count)
	checkpoints.push_back(ray_count);

    point_t center;
    VADD2SCALE(center, bbox_max, bbox_min, 0.5);
    vect_t bbox_diagonal;
    VSUB2(bbox_diagonal, bbox_max, bbox_min);
    const double sampling_radius = 0.5 * MAGNITUDE(bbox_diagonal);
    const std::vector<sampled_ray> rays = generate_paired_rays(ray_count,
	sampling_radius, center);
    size_t implicit_crossings = 0;
    size_t brep_crossings = 0;
    size_t differing_lines = 0;
    size_t tangent_band_lines = 0;
    double implicit_total_chord = 0.0;
    double brep_total_chord = 0.0;
    double signed_chord_difference = 0.0;
    double absolute_chord_difference = 0.0;
    double maximum_endpoint_error = 0.0;
    double implicit_area_sum = 0.0;
    double implicit_area_sum_squared = 0.0;
    double brep_area_sum = 0.0;
    double brep_area_sum_squared = 0.0;
    double implicit_volume_sum = 0.0;
    double implicit_volume_sum_squared = 0.0;
    double brep_volume_sum = 0.0;
    double brep_volume_sum_squared = 0.0;
    const double area_scale = 4.0 * M_PI * sampling_radius *
	sampling_radius;
    const double volume_scale = M_PI * sampling_radius * sampling_radius;
    size_t checkpoint_index = 0;
    size_t reported = 0;

    for (size_t i = 0; i < rays.size(); ++i) {
	partition_result implicit_result = shoot_partitions(implicit_model,
	    rays[i]);
	partition_result brep_result = shoot_partitions(brep_model, rays[i]);
	const double implicit_chord = partition_chord(implicit_result);
	const double brep_chord = partition_chord(brep_result);
	implicit_crossings += 2 * implicit_result.partitions;
	brep_crossings += 2 * brep_result.partitions;
	implicit_total_chord += implicit_chord;
	brep_total_chord += brep_chord;
	signed_chord_difference += brep_chord - implicit_chord;
	absolute_chord_difference += fabs(brep_chord - implicit_chord);
	const double implicit_area_contribution = area_scale *
	    implicit_result.partitions;
	const double brep_area_contribution = area_scale *
	    brep_result.partitions;
	const double implicit_volume_contribution = volume_scale *
	    implicit_chord;
	const double brep_volume_contribution = volume_scale * brep_chord;
	implicit_area_sum += implicit_area_contribution;
	implicit_area_sum_squared += implicit_area_contribution *
	    implicit_area_contribution;
	brep_area_sum += brep_area_contribution;
	brep_area_sum_squared += brep_area_contribution *
	    brep_area_contribution;
	implicit_volume_sum += implicit_volume_contribution;
	implicit_volume_sum_squared += implicit_volume_contribution *
	    implicit_volume_contribution;
	brep_volume_sum += brep_volume_contribution;
	brep_volume_sum_squared += brep_volume_contribution *
	    brep_volume_contribution;

	if (!partition_result_valid(implicit_result, rays[i].direction) ||
		!partition_result_valid(brep_result, rays[i].direction)) {
	    if (reported++ < 5)
		std::printf("%s: %s shared Crofton ray %zu has invalid "
		    "partition data\n", comparison_status, label, i);
	    comparison_issues++;
	    continue;
	}

	bool line_differs = implicit_result.partitions !=
	    brep_result.partitions;
	bool tangent_band = false;
	if (line_differs) {
	    tangent_band = std::max(implicit_chord, brep_chord) <= tol->dist;
	    if (tangent_band) {
		tangent_band_lines++;
	    } else {
		if (reported++ < 5)
		    std::printf("%s: %s shared Crofton ray %zu partition "
			"count implicit=%zu BREP=%zu chords=[%.17g %.17g]\n",
			comparison_status, label, i,
			implicit_result.partitions, brep_result.partitions,
			implicit_chord, brep_chord);
		comparison_issues++;
	    }
	}

	if (!line_differs) {
	    for (size_t j = 0; j < implicit_result.partitions; ++j) {
		const double in_error = fabs(implicit_result.intervals[j].in_dist -
		    brep_result.intervals[j].in_dist);
		const double out_error = fabs(implicit_result.intervals[j].out_dist -
		    brep_result.intervals[j].out_dist);
		maximum_endpoint_error = std::max(maximum_endpoint_error,
		    std::max(in_error, out_error));
		if (in_error > tol->dist || out_error > tol->dist) {
		    line_differs = true;
		    if (reported++ < 5)
			std::printf("%s: %s shared Crofton ray %zu endpoint "
			    "errors=[%.17g %.17g]\n", comparison_status,
			    label, i, in_error,
			    out_error);
		    comparison_issues++;
		}
	    }
	}
	if (line_differs)
	    differing_lines++;

	if (checkpoint_index < checkpoints.size() &&
		i + 1 == checkpoints[checkpoint_index]) {
	    const size_t samples = i + 1;
	    const double implicit_area_estimate = implicit_area_sum / samples;
	    const double brep_area_estimate = brep_area_sum / samples;
	    const double implicit_volume_estimate = implicit_volume_sum / samples;
	    const double brep_volume_estimate = brep_volume_sum / samples;
	    const double implicit_area_se = sample_standard_error(
		implicit_area_sum, implicit_area_sum_squared, samples);
	    const double brep_area_se = sample_standard_error(brep_area_sum,
		brep_area_sum_squared, samples);
	    const double implicit_volume_se = sample_standard_error(
		implicit_volume_sum, implicit_volume_sum_squared, samples);
	    const double brep_volume_se = sample_standard_error(brep_volume_sum,
		brep_volume_sum_squared, samples);
	    const double area_band = std::max(analytic_area * 0.01,
		4.0 * std::max(implicit_area_se, brep_area_se));
	    const double volume_band = std::max(analytic_volume * 0.01,
		4.0 * std::max(implicit_volume_se, brep_volume_se));
	    if (fabs(implicit_area_estimate - analytic_area) > area_band ||
		    fabs(brep_area_estimate - analytic_area) > area_band ||
		    fabs(implicit_volume_estimate - analytic_volume) >
		    volume_band ||
		    fabs(brep_volume_estimate - analytic_volume) > volume_band) {
		std::printf("%s: %s shared Crofton confidence at %zu rays "
		    "area-band=%.17g volume-band=%.17g\n",
		    comparison_status, label, samples, area_band, volume_band);
		comparison_issues++;
	    }
	    std::printf("Shared Crofton %s checkpoint %zu: area implicit="
		"%.9g+/-%.3g BREP=%.9g+/-%.3g volume implicit="
		"%.9g+/-%.3g BREP=%.9g+/-%.3g\n", label, samples,
		implicit_area_estimate, implicit_area_se, brep_area_estimate,
		brep_area_se, implicit_volume_estimate, implicit_volume_se,
		brep_volume_estimate, brep_volume_se);
	    checkpoint_index++;
	}
    }

    const double implicit_area = 4.0 * M_PI * sampling_radius *
	sampling_radius * implicit_crossings / (2.0 * ray_count);
    const double brep_area = 4.0 * M_PI * sampling_radius *
	sampling_radius * brep_crossings / (2.0 * ray_count);
    const double implicit_volume = M_PI * sampling_radius * sampling_radius *
	implicit_total_chord / ray_count;
    const double brep_volume = M_PI * sampling_radius * sampling_radius *
	brep_total_chord / ray_count;
    if (relative_error(implicit_area, analytic_area) > 0.06 ||
	    relative_error(implicit_volume, analytic_volume) > 0.06 ||
	    relative_error(brep_area, analytic_area) > 0.06 ||
	    relative_error(brep_volume, analytic_volume) > 0.06 ||
	    relative_error(brep_area, implicit_area) > 0.001 ||
	    relative_error(brep_volume, implicit_volume) > 0.001) {
	std::printf("%s: %s shared Crofton aggregates analytic="
	    "[%.17g %.17g] "
	    "implicit=[%.17g %.17g] BREP=[%.17g %.17g]\n",
	    comparison_status, label, analytic_area, analytic_volume,
	    implicit_area, implicit_volume, brep_area, brep_volume);
	comparison_issues++;
    }

    std::printf("Shared Crofton %s: rays=%zu differing=%zu "
	"tangent-band=%zu max-endpoint=%.3g signed-chord=%.3g "
	"absolute-chord=%.3g\n", label, ray_count, differing_lines,
	tangent_band_lines, maximum_endpoint_error, signed_chord_difference,
	absolute_chord_difference);

    if (!supplied_implicit_model) {
	free_prepared_model(brep_model);
	free_prepared_model(implicit_model);
	delete brep_internal.brep;
    }
    if (comparison_issues_out)
	*comparison_issues_out = comparison_issues;
    if (enforce_comparison)
	failures += (int)comparison_issues;
    return failures;
}


static int
check_brep_leaf_csg_fixture(const char *label,
    struct rt_db_internal *left_intern,
    struct rt_db_internal *right_intern, int member_operation,
    const struct bn_tol *tol, const point_t bbox_min,
    const point_t bbox_max, double analytic_area, double analytic_volume,
    const directed_partition_ray *directed_rays,
    size_t directed_ray_count)
{
    prepared_model implicit_model;
    prepared_model brep_model;
    if (!prep_binary_csg_model(implicit_model, left_intern, right_intern,
	    member_operation, tol, false) ||
	    !prep_binary_csg_model(brep_model, left_intern, right_intern,
	    member_operation, tol, true)) {
	std::printf("FAIL: %s BREP-leaf CSG preparation\n", label);
	free_prepared_model(brep_model);
	free_prepared_model(implicit_model);
	return 1;
    }

    const int failures = check_shared_crofton_fixture(label, NULL, tol,
	bbox_min, bbox_max, analytic_area, analytic_volume, 8000,
	directed_rays, directed_ray_count, &implicit_model, &brep_model);
    free_prepared_model(brep_model);
    free_prepared_model(implicit_model);
    return failures;
}


static int
check_shared_primitive_corpus(const struct bn_tol *tol)
{
    int failures = 0;

    struct rt_ell_internal ellipsoid = {};
    ellipsoid.magic = RT_ELL_INTERNAL_MAGIC;
    VSET(ellipsoid.v, 4.0, -3.0, 2.0);
    VSET(ellipsoid.a, 10.0, 0.0, 0.0);
    VSET(ellipsoid.b, 0.0, 10.0, 0.0);
    VSET(ellipsoid.c, 0.0, 0.0, 6.0);
    struct rt_db_internal ellipsoid_intern;
    RT_DB_INTERNAL_INIT(&ellipsoid_intern);
    ellipsoid_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ellipsoid_intern.idb_type = ID_ELL;
    ellipsoid_intern.idb_meth = &OBJ[ID_ELL];
    ellipsoid_intern.idb_ptr = &ellipsoid;
    point_t ellipsoid_min = {-6.0, -13.0, -4.0};
    point_t ellipsoid_max = {14.0, 7.0, 8.0};
    const double eccentricity = 0.8;
    const double ellipsoid_area = 2.0 * M_PI * 100.0 *
	(1.0 + (1.0 - eccentricity * eccentricity) /
	 eccentricity * atanh(eccentricity));
    const double ellipsoid_volume = (4.0 / 3.0) * M_PI * 100.0 * 6.0;
    const directed_partition_ray ellipsoid_rays[] = {
	{"north pole", {4.0, -3.0, 14.0}, {0.0, 0.0, -1.0}, 1,
	    {6.0, 18.0}},
	{"periodic seam", {24.0, -3.0, 2.0}, {-1.0, 0.0, 0.0}, 1,
	    {10.0, 30.0}}
    };
    failures += check_shared_crofton_fixture("oblate-ellipsoid",
	&ellipsoid_intern, tol, ellipsoid_min, ellipsoid_max,
	ellipsoid_area, ellipsoid_volume, 8000, ellipsoid_rays,
	sizeof(ellipsoid_rays) / sizeof(ellipsoid_rays[0]));

    struct rt_arb_internal arb = {};
    arb.magic = RT_ARB_INTERNAL_MAGIC;
    VSET(arb.pt[0], -11.0, -7.0, -5.0);
    VSET(arb.pt[1], 9.0, -7.0, -5.0);
    VSET(arb.pt[2], 9.0, 5.0, -5.0);
    VSET(arb.pt[3], -11.0, 5.0, -5.0);
    VSET(arb.pt[4], -11.0, -7.0, 3.0);
    VSET(arb.pt[5], 9.0, -7.0, 3.0);
    VSET(arb.pt[6], 9.0, 5.0, 3.0);
    VSET(arb.pt[7], -11.0, 5.0, 3.0);
    struct rt_db_internal arb_intern;
    RT_DB_INTERNAL_INIT(&arb_intern);
    arb_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    arb_intern.idb_type = ID_ARB8;
    arb_intern.idb_meth = &OBJ[ID_ARB8];
    arb_intern.idb_ptr = &arb;
    point_t arb_min = {-11.0, -7.0, -5.0};
    point_t arb_max = {9.0, 5.0, 3.0};
    const double arb_diagonal = sqrt(20.0 * 20.0 + 12.0 * 12.0 +
	8.0 * 8.0);
    const directed_partition_ray arb_rays[] = {
	{"face forward", {-31.0, -1.0, -1.0}, {1.0, 0.0, 0.0}, 1,
	    {20.0, 40.0}},
	{"face reverse", {29.0, -1.0, -1.0}, {-1.0, 0.0, 0.0}, 1,
	    {20.0, 40.0}},
	{"opposite vertices", {-31.0, -19.0, -13.0},
	    {20.0, 12.0, 8.0}, 1, {arb_diagonal, 2.0 * arb_diagonal}}
    };
    failures += check_shared_crofton_fixture("arb8-box", &arb_intern, tol,
	arb_min, arb_max, 992.0, 1920.0, 8000, arb_rays,
	sizeof(arb_rays) / sizeof(arb_rays[0]));

    struct rt_tgc_internal cylinder = {};
    cylinder.magic = RT_TGC_INTERNAL_MAGIC;
    VSET(cylinder.v, 3.0, -4.0, -7.0);
    VSET(cylinder.h, 0.0, 0.0, 14.0);
    VSET(cylinder.a, 6.0, 0.0, 0.0);
    VSET(cylinder.b, 0.0, 6.0, 0.0);
    VMOVE(cylinder.c, cylinder.a);
    VMOVE(cylinder.d, cylinder.b);
    struct rt_db_internal cylinder_intern;
    RT_DB_INTERNAL_INIT(&cylinder_intern);
    cylinder_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    cylinder_intern.idb_type = ID_TGC;
    cylinder_intern.idb_meth = &OBJ[ID_TGC];
    cylinder_intern.idb_ptr = &cylinder;
    point_t cylinder_min = {-3.0, -10.0, -7.0};
    point_t cylinder_max = {9.0, 2.0, 7.0};
    const directed_partition_ray cylinder_rays[] = {
	{"axis", {3.0, -4.0, -14.0}, {0.0, 0.0, 1.0}, 1,
	    {7.0, 21.0}},
	{"periodic seam", {15.0, -4.0, 0.0}, {-1.0, 0.0, 0.0}, 1,
	    {6.0, 18.0}},
	{"periodic seam reverse", {-9.0, -4.0, 0.0},
	    {1.0, 0.0, 0.0}, 1, {6.0, 18.0}}
    };
    failures += check_shared_crofton_fixture("rcc", &cylinder_intern, tol,
	cylinder_min, cylinder_max, 240.0 * M_PI, 504.0 * M_PI, 8000,
	cylinder_rays, sizeof(cylinder_rays) / sizeof(cylinder_rays[0]));

    struct rt_tgc_internal cone = cylinder;
    VSET(cone.c, 3.0, 0.0, 0.0);
    VSET(cone.d, 0.0, 3.0, 0.0);
    struct rt_db_internal cone_intern;
    RT_DB_INTERNAL_INIT(&cone_intern);
    cone_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    cone_intern.idb_type = ID_TGC;
    cone_intern.idb_meth = &OBJ[ID_TGC];
    cone_intern.idb_ptr = &cone;
    const double cone_slant = sqrt(14.0 * 14.0 + 3.0 * 3.0);
    const double cone_area = M_PI * (6.0 + 3.0) * cone_slant +
	M_PI * (6.0 * 6.0 + 3.0 * 3.0);
    const directed_partition_ray cone_rays[] = {
	{"axis", {3.0, -4.0, -14.0}, {0.0, 0.0, 1.0}, 1,
	    {7.0, 21.0}},
	{"periodic seam", {15.0, -4.0, 0.0}, {-1.0, 0.0, 0.0}, 1,
	    {7.5, 16.5}}
    };
    failures += check_shared_crofton_fixture("truncated-cone", &cone_intern,
	tol, cylinder_min, cylinder_max, cone_area, 294.0 * M_PI, 8000,
	cone_rays, sizeof(cone_rays) / sizeof(cone_rays[0]));

    struct rt_tor_internal torus = {};
    torus.magic = RT_TOR_INTERNAL_MAGIC;
    VSET(torus.v, 0.0, 0.0, 0.0);
    VSET(torus.h, 0.0, 0.0, 1.0);
    torus.r_a = 12.0;
    torus.r_h = 3.0;
    struct rt_db_internal torus_intern;
    RT_DB_INTERNAL_INIT(&torus_intern);
    torus_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    torus_intern.idb_type = ID_TOR;
    torus_intern.idb_meth = &OBJ[ID_TOR];
    torus_intern.idb_ptr = &torus;
    point_t torus_min = {-15.0, -15.0, -3.0};
    point_t torus_max = {15.0, 15.0, 3.0};
    const directed_partition_ray torus_rays[] = {
	{"two intervals", {-20.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 2,
	    {5.0, 11.0, 29.0, 35.0}},
	{"two intervals reverse", {20.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, 2,
	    {5.0, 11.0, 29.0, 35.0}},
	{"central hole", {0.0, 0.0, -10.0}, {0.0, 0.0, 1.0}, 0,
	    {0.0, 0.0}},
	{"tube vertical", {12.0, 0.0, -10.0}, {0.0, 0.0, 1.0}, 1,
	    {7.0, 13.0}}
    };
    failures += check_shared_crofton_fixture("torus", &torus_intern, tol,
	torus_min, torus_max, 144.0 * M_PI * M_PI,
	216.0 * M_PI * M_PI, 8000, torus_rays,
	sizeof(torus_rays) / sizeof(torus_rays[0]));

    return failures;
}


static void
init_sphere_internal(struct rt_ell_internal &sphere,
    struct rt_db_internal &intern, const point_t center, double radius)
{
    sphere = {};
    sphere.magic = RT_ELL_INTERNAL_MAGIC;
    VMOVE(sphere.v, center);
    VSET(sphere.a, radius, 0.0, 0.0);
    VSET(sphere.b, 0.0, radius, 0.0);
    VSET(sphere.c, 0.0, 0.0, radius);
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_ELL;
    intern.idb_meth = &OBJ[ID_ELL];
    intern.idb_ptr = &sphere;
}


static int
check_brep_leaf_csg_corpus(const struct bn_tol *tol)
{
    int failures = 0;
    struct rt_ell_internal left_sphere;
    struct rt_ell_internal right_sphere;
    struct rt_db_internal left_intern;
    struct rt_db_internal right_intern;

    point_t disjoint_left_center = {-6.0, 0.0, 0.0};
    point_t disjoint_right_center = {6.0, 0.0, 0.0};
    init_sphere_internal(left_sphere, left_intern, disjoint_left_center, 4.0);
    init_sphere_internal(right_sphere, right_intern, disjoint_right_center,
	4.0);
    point_t disjoint_min = {-10.0, -4.0, -4.0};
    point_t disjoint_max = {10.0, 4.0, 4.0};
    const directed_partition_ray disjoint_rays[] = {
	{"two components", {-20.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 2,
	    {10.0, 18.0, 22.0, 30.0}},
	{"two components reverse", {20.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, 2,
	    {10.0, 18.0, 22.0, 30.0}}
    };
    failures += check_brep_leaf_csg_fixture("disjoint-sphere-union",
	&left_intern, &right_intern, WMOP_UNION, tol,
	disjoint_min, disjoint_max, 128.0 * M_PI,
	(512.0 / 3.0) * M_PI, disjoint_rays,
	sizeof(disjoint_rays) / sizeof(disjoint_rays[0]));

    point_t overlap_left_center = {-3.0, 0.0, 0.0};
    point_t overlap_right_center = {3.0, 0.0, 0.0};
    init_sphere_internal(left_sphere, left_intern, overlap_left_center, 5.0);
    init_sphere_internal(right_sphere, right_intern, overlap_right_center,
	5.0);
    point_t overlap_union_min = {-8.0, -5.0, -5.0};
    point_t overlap_union_max = {8.0, 5.0, 5.0};
    const directed_partition_ray overlap_union_rays[] = {
	{"merged interval", {-20.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1,
	    {12.0, 28.0}},
	{"merged interval reverse", {20.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, 1,
	    {12.0, 28.0}}
    };
    failures += check_brep_leaf_csg_fixture("overlapping-sphere-union",
	&left_intern, &right_intern, WMOP_UNION, tol,
	overlap_union_min, overlap_union_max, 160.0 * M_PI,
	(896.0 / 3.0) * M_PI, overlap_union_rays,
	sizeof(overlap_union_rays) / sizeof(overlap_union_rays[0]));

    point_t overlap_intersection_min = {-2.0, -4.0, -4.0};
    point_t overlap_intersection_max = {2.0, 4.0, 4.0};
    const directed_partition_ray overlap_intersection_rays[] = {
	{"lens", {-20.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1,
	    {18.0, 22.0}},
	{"lens reverse", {20.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, 1,
	    {18.0, 22.0}}
    };
    failures += check_brep_leaf_csg_fixture("sphere-intersection-lens",
	&left_intern, &right_intern, WMOP_INTERSECT, tol,
	overlap_intersection_min, overlap_intersection_max, 40.0 * M_PI,
	(104.0 / 3.0) * M_PI, overlap_intersection_rays,
	sizeof(overlap_intersection_rays) /
	    sizeof(overlap_intersection_rays[0]));

    point_t concentric_center = VINIT_ZERO;
    init_sphere_internal(left_sphere, left_intern, concentric_center, 8.0);
    init_sphere_internal(right_sphere, right_intern, concentric_center, 3.0);
    point_t shell_min = {-8.0, -8.0, -8.0};
    point_t shell_max = {8.0, 8.0, 8.0};
    const directed_partition_ray shell_rays[] = {
	{"cavity", {-12.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 2,
	    {4.0, 9.0, 15.0, 20.0}},
	{"cavity reverse", {12.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, 2,
	    {4.0, 9.0, 15.0, 20.0}}
    };
    failures += check_brep_leaf_csg_fixture("concentric-sphere-cavity",
	&left_intern, &right_intern, WMOP_SUBTRACT, tol,
	shell_min, shell_max, 292.0 * M_PI, (1940.0 / 3.0) * M_PI,
	shell_rays, sizeof(shell_rays) / sizeof(shell_rays[0]));

    struct rt_arb_internal box = {};
    box.magic = RT_ARB_INTERNAL_MAGIC;
    VSET(box.pt[0], -8.0, -8.0, -5.0);
    VSET(box.pt[1], 8.0, -8.0, -5.0);
    VSET(box.pt[2], 8.0, 8.0, -5.0);
    VSET(box.pt[3], -8.0, 8.0, -5.0);
    VSET(box.pt[4], -8.0, -8.0, 5.0);
    VSET(box.pt[5], 8.0, -8.0, 5.0);
    VSET(box.pt[6], 8.0, 8.0, 5.0);
    VSET(box.pt[7], -8.0, 8.0, 5.0);
    struct rt_db_internal box_intern;
    RT_DB_INTERNAL_INIT(&box_intern);
    box_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    box_intern.idb_type = ID_ARB8;
    box_intern.idb_meth = &OBJ[ID_ARB8];
    box_intern.idb_ptr = &box;

    struct rt_tgc_internal cutter = {};
    cutter.magic = RT_TGC_INTERNAL_MAGIC;
    VSET(cutter.v, 0.0, 0.0, -6.0);
    VSET(cutter.h, 0.0, 0.0, 12.0);
    VSET(cutter.a, 3.0, 0.0, 0.0);
    VSET(cutter.b, 0.0, 3.0, 0.0);
    VMOVE(cutter.c, cutter.a);
    VMOVE(cutter.d, cutter.b);
    struct rt_db_internal cutter_intern;
    RT_DB_INTERNAL_INIT(&cutter_intern);
    cutter_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    cutter_intern.idb_type = ID_TGC;
    cutter_intern.idb_meth = &OBJ[ID_TGC];
    cutter_intern.idb_ptr = &cutter;
    point_t drilled_box_min = {-8.0, -8.0, -5.0};
    point_t drilled_box_max = {8.0, 8.0, 5.0};
    const directed_partition_ray drilled_box_rays[] = {
	{"cross hole", {-12.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 2,
	    {4.0, 9.0, 15.0, 20.0}},
	{"through hole", {0.0, 0.0, -10.0}, {0.0, 0.0, 1.0}, 0,
	    {0.0, 0.0}},
	{"through material", {5.0, 0.0, -10.0}, {0.0, 0.0, 1.0}, 1,
	    {5.0, 15.0}}
    };
    failures += check_brep_leaf_csg_fixture("box-minus-cylinder", &box_intern,
	&cutter_intern, WMOP_SUBTRACT, tol, drilled_box_min,
	drilled_box_max, 1152.0 + 42.0 * M_PI, 2560.0 - 90.0 * M_PI,
	drilled_box_rays,
	sizeof(drilled_box_rays) / sizeof(drilled_box_rays[0]));

    return failures;
}


struct cobb_seam_frame {
    int face_index = 0;
    int side_index = 1;
    int edge_index = -1;
    ON_3dPoint point;
    ON_3dVector normal;
    ON_3dVector tangent;
    ON_3dVector target_conormal;
};


static bool
cobb_target_edge(const ON_Brep *brep, int face_index, int side_index,
    int &edge_index)
{
    if (!brep || face_index < 0 || face_index >= brep->m_F.Count() ||
	    side_index < 0 || side_index >= 4)
	return false;
    const ON_BrepFace &face = brep->m_F[face_index];
    if (face.m_li.Count() != 1)
	return false;
    const int loop_index = face.m_li[0];
    if (loop_index < 0 || loop_index >= brep->m_L.Count())
	return false;
    const ON_BrepLoop &loop = brep->m_L[loop_index];
    if (loop.m_ti.Count() != 4)
	return false;
    const int trim_index = loop.m_ti[side_index];
    if (trim_index < 0 || trim_index >= brep->m_T.Count())
	return false;
    edge_index = brep->m_T[trim_index].m_ei;
    return edge_index >= 0 && edge_index < brep->m_E.Count();
}


static bool
cobb_seam_geometry(const ON_Brep *brep, const ON_3dPoint &origin,
    cobb_seam_frame &frame)
{
    if (!cobb_target_edge(brep, frame.face_index, frame.side_index,
	    frame.edge_index))
	return false;
    const ON_BrepEdge &edge = brep->m_E[frame.edge_index];
    const ON_Curve *curve = edge.EdgeCurveOf();
    if (!curve || !curve->Ev1Der(curve->Domain().Mid(), frame.point,
	    frame.tangent) || !frame.tangent.Unitize())
	return false;
    frame.normal = frame.point - origin;
    if (!frame.normal.Unitize())
	return false;
    frame.target_conormal = ON_CrossProduct(frame.tangent, frame.normal);
    if (!frame.target_conormal.Unitize())
	return false;

    const ON_BrepFace &face = brep->m_F[frame.face_index];
    const ON_Surface *surface = face.SurfaceOf();
    if (!surface)
	return false;
    const ON_3dPoint face_center = surface->PointAt(surface->Domain(0).Mid(),
	surface->Domain(1).Mid());
    if (frame.target_conormal * (face_center - frame.point) < 0.0)
	frame.target_conormal.Reverse();
    return true;
}


static bool
cobb_perturb_boundary_interior(ON_Brep *brep, int face_index,
    int side_index, const ON_3dPoint &origin, double displacement)
{
    if (!brep || face_index < 0 || face_index >= brep->m_F.Count())
	return false;
    ON_NurbsSurface *surface = ON_NurbsSurface::Cast(const_cast<ON_Surface *>(
	brep->m_F[face_index].SurfaceOf()));
    if (!surface || surface->CVCount(0) < 3 || surface->CVCount(1) < 3)
	return false;

    const int fixed_index = (side_index == 0 || side_index == 3) ? 0 :
	((side_index == 1) ? surface->CVCount(0) - 1 :
	 surface->CVCount(1) - 1);
    const int varying_count = (side_index % 2) ? surface->CVCount(1) :
	surface->CVCount(0);
    for (int varying = 1; varying < varying_count - 1; ++varying) {
	const int i = (side_index % 2) ? fixed_index : varying;
	const int j = (side_index % 2) ? varying : fixed_index;
	ON_4dPoint cv;
	if (!surface->GetCV(i, j, cv) || fabs(cv.w) <= DBL_MIN)
	    return false;
	ON_3dPoint euclidean(cv.x / cv.w, cv.y / cv.w, cv.z / cv.w);
	ON_3dVector direction = euclidean - origin;
	if (!direction.Unitize())
	    return false;
	cv.x += displacement * direction.x * cv.w;
	cv.y += displacement * direction.y * cv.w;
	cv.z += displacement * direction.z * cv.w;
	if (!surface->SetCV(i, j, cv))
	    return false;
    }
    surface->DestroyRuntimeCache(true);
    brep->DestroyRuntimeCache(true);
    return true;
}


static bool
cobb_trim_lift(const ON_BrepTrim &trim, double edge_fraction,
    ON_3dPoint &point)
{
    const ON_BrepFace *face = trim.Face();
    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
    const ON_Curve *curve = trim.TrimCurveOf();
    const ON_Interval domain = trim.Domain();
    if (!surface || !curve || !domain.IsIncreasing())
	return false;
    const double trim_fraction = trim.m_bRev3d ? 1.0 - edge_fraction :
	edge_fraction;
    const ON_3dPoint uv = curve->PointAt(
	domain.ParameterAt(trim_fraction));
    point = surface->PointAt(uv.x, uv.y);
    return uv.IsValid() && point.IsValid();
}


static double
cobb_seam_discrepancy(const ON_Brep *brep, int edge_index)
{
    if (!brep || edge_index < 0 || edge_index >= brep->m_E.Count())
	return INFINITY;
    const ON_BrepEdge &edge = brep->m_E[edge_index];
    if (edge.m_ti.Count() != 2)
	return INFINITY;
    const ON_BrepTrim &first = brep->m_T[edge.m_ti[0]];
    const ON_BrepTrim &second = brep->m_T[edge.m_ti[1]];
    double maximum = 0.0;
    for (int sample = 0; sample <= 256; ++sample) {
	const double fraction = (double)sample / 256.0;
	ON_3dPoint first_lift;
	ON_3dPoint second_lift;
	if (!cobb_trim_lift(first, fraction, first_lift) ||
		!cobb_trim_lift(second, fraction, second_lift))
	    return INFINITY;
	maximum = std::max(maximum, first_lift.DistanceTo(second_lift));
    }
    return maximum;
}


static ON_Brep *
cobb_bowed_seam_variant(const ON_Brep *pristine, const ON_3dPoint &origin,
    double signed_target_gap, cobb_seam_frame &frame, double &measured_gap,
    double &applied_displacement)
{
    measured_gap = INFINITY;
    applied_displacement = signed_target_gap;
    if (!pristine || !(fabs(signed_target_gap) > 0.0) ||
	    !cobb_seam_geometry(pristine, origin, frame))
	return NULL;

    ON_Brep *variant = NULL;
    for (int iteration = 0; iteration < 4; ++iteration) {
	delete variant;
	variant = new ON_Brep(*pristine);
	if (!cobb_perturb_boundary_interior(variant, frame.face_index,
		frame.side_index, origin, applied_displacement)) {
	    delete variant;
	    return NULL;
	}
	measured_gap = cobb_seam_discrepancy(variant, frame.edge_index);
	if (!(measured_gap > 0.0) || !std::isfinite(measured_gap)) {
	    delete variant;
	    return NULL;
	}
	const double ratio = fabs(signed_target_gap) / measured_gap;
	if (fabs(ratio - 1.0) <= 1.0e-4)
	    break;
	applied_displacement *= ratio;
    }

    variant->m_E[frame.edge_index].m_tolerance = measured_gap * 1.01;
    return variant;
}


static int
check_cobb_sphere_corpus(const struct bn_tol *tol)
{
    int failures = 0;
    const double radius = 10.0;
    const ON_3dPoint origin(0.0, 0.0, 0.0);
    ON_Brep *unsewn = ON_Brep_CobbSphereUnsewn(radius, origin);
    ON_Brep *sewn = ON_Brep_CobbSphereSewn(radius, origin);
    if (!unsewn || !sewn) {
	std::printf("FAIL: Cobb sphere construction unsewn=%d sewn=%d\n",
	    unsewn != NULL, sewn != NULL);
	delete unsewn;
	delete sewn;
	return 1;
    }

    ON_wString unsewn_messages;
    ON_wString sewn_messages;
    ON_TextLog unsewn_log(unsewn_messages);
    ON_TextLog sewn_log(sewn_messages);
    bool unsewn_oriented = false;
    bool unsewn_boundary = false;
    bool sewn_oriented = false;
    bool sewn_boundary = false;
    const bool unsewn_valid = unsewn->IsValid(&unsewn_log);
    const bool unsewn_manifold = unsewn->IsManifold(&unsewn_oriented,
	&unsewn_boundary);
    const bool sewn_valid = sewn->IsValid(&sewn_log);
    const bool sewn_manifold = sewn->IsManifold(&sewn_oriented,
	&sewn_boundary);
    bool paired_edges = true;
    for (int i = 0; i < sewn->m_E.Count(); ++i)
	paired_edges = paired_edges && sewn->m_E[i].m_ti.Count() == 2;

    if (!unsewn_valid || unsewn->IsSolid() ||
	    unsewn->m_F.Count() != 6 || unsewn->m_V.Count() != 24 ||
	    unsewn->m_E.Count() != 24 || unsewn->m_T.Count() != 24) {
	ON_String log_text(unsewn_messages);
	std::printf("FAIL: legacy Cobb topology valid=%d solid=%d "
	    "manifold=%d oriented=%d boundary=%d V/E/T/F=%d/%d/%d/%d\n%s",
	    unsewn_valid, unsewn->IsSolid(), unsewn_manifold,
	    unsewn_oriented, unsewn_boundary, unsewn->m_V.Count(),
	    unsewn->m_E.Count(), unsewn->m_T.Count(), unsewn->m_F.Count(),
	    log_text.Array());
	failures++;
    }
    if (!sewn_valid || !sewn->IsSolid() || !sewn_manifold ||
	    !sewn_oriented || sewn_boundary || !paired_edges ||
	    sewn->m_F.Count() != 6 || sewn->m_V.Count() != 8 ||
	    sewn->m_E.Count() != 12 || sewn->m_T.Count() != 24) {
	ON_String log_text(sewn_messages);
	std::printf("FAIL: sewn Cobb topology valid=%d solid=%d manifold=%d "
	    "oriented=%d boundary=%d paired=%d V/E/T/F=%d/%d/%d/%d\n%s",
	    sewn_valid, sewn->IsSolid(), sewn_manifold, sewn_oriented,
	    sewn_boundary, paired_edges, sewn->m_V.Count(), sewn->m_E.Count(),
	    sewn->m_T.Count(), sewn->m_F.Count(), log_text.Array());
	failures++;
    }

    const double unsewn_radial_error = ON_Brep_CobbSphereMaxRadialError(
	unsewn, radius, origin);
    const double sewn_radial_error = ON_Brep_CobbSphereMaxRadialError(sewn,
	radius, origin);
    std::printf("Cobb sphere baseline: max radial error unsewn=%.17g "
	"sewn=%.17g\n", unsewn_radial_error, sewn_radial_error);
    if (!std::isfinite(unsewn_radial_error) ||
	    !std::isfinite(sewn_radial_error) ||
	    fabs(unsewn_radial_error - sewn_radial_error) >
	    64.0 * DBL_EPSILON * radius)
	failures++;

    struct cobb_scale_case {
	const char *name;
	double radius;
	ON_3dPoint origin;
    } scale_cases[] = {
	{"small translated", 0.01, ON_3dPoint(1.25, -2.5, 5.0)},
	{"large translated", 1.0e4,
	    ON_3dPoint(1.0e6, -2.0e6, 3.0e6)}
    };
    for (size_t i = 0; i < sizeof(scale_cases) / sizeof(scale_cases[0]); ++i) {
	ON_Brep *scaled = ON_Brep_CobbSphereSewn(scale_cases[i].radius,
	    scale_cases[i].origin);
	const double radial_error = ON_Brep_CobbSphereMaxRadialError(scaled,
	    scale_cases[i].radius, scale_cases[i].origin);
	const double coordinate_scale = std::max(scale_cases[i].radius,
	    std::max(fabs(scale_cases[i].origin.x),
	    std::max(fabs(scale_cases[i].origin.y),
		fabs(scale_cases[i].origin.z))));
	const double error_limit = std::max(1.0e-12 * scale_cases[i].radius,
	    512.0 * DBL_EPSILON * coordinate_scale);
	if (!scaled || !scaled->IsSolid() || scaled->m_V.Count() != 8 ||
		scaled->m_E.Count() != 12 || !std::isfinite(radial_error) ||
		radial_error > error_limit) {
	    std::printf("FAIL: Cobb %s topology/radial error=%.17g "
		"limit=%.17g V/E=%d/%d\n", scale_cases[i].name,
		radial_error, error_limit, scaled ? scaled->m_V.Count() : 0,
		scaled ? scaled->m_E.Count() : 0);
	    failures++;
	}
	delete scaled;
    }

    struct rt_ell_internal sphere;
    struct rt_db_internal sphere_intern;
    point_t center = VINIT_ZERO;
    init_sphere_internal(sphere, sphere_intern, center, radius);
    struct rt_brep_internal cobb_internal = {};
    cobb_internal.magic = RT_BREP_INTERNAL_MAGIC;
    cobb_internal.brep = sewn;
    struct rt_db_internal cobb_intern;
    RT_DB_INTERNAL_INIT(&cobb_intern);
    cobb_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    cobb_intern.idb_type = ID_BREP;
    cobb_intern.idb_meth = &OBJ[ID_BREP];
    cobb_intern.idb_ptr = &cobb_internal;

    prepared_model implicit_model;
    prepared_model cobb_model;
    if (!prep_partition_model(implicit_model, &sphere_intern,
	    "cobb_oracle.s", tol) ||
	    !prep_partition_model(cobb_model, &cobb_intern, "cobb_sewn.s", tol)) {
	std::printf("FAIL: Cobb sphere paired model preparation\n");
	free_prepared_model(cobb_model);
	free_prepared_model(implicit_model);
	delete unsewn;
	delete sewn;
	return failures + 1;
    }

    const double inv_sqrt2 = 1.0 / sqrt(2.0);
    const double inv_sqrt3 = 1.0 / sqrt(3.0);
    const directed_partition_ray rays[] = {
	{"positive x face", {20.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, 1,
	    {10.0, 30.0}},
	{"negative z face", {0.0, 0.0, -20.0}, {0.0, 0.0, 1.0}, 1,
	    {10.0, 30.0}},
	{"xy seam", {20.0*inv_sqrt2, 20.0*inv_sqrt2, 0.0},
	    {-inv_sqrt2, -inv_sqrt2, 0.0}, 1, {10.0, 30.0}},
	{"positive vertex", {20.0*inv_sqrt3, 20.0*inv_sqrt3,
	    20.0*inv_sqrt3}, {-inv_sqrt3, -inv_sqrt3, -inv_sqrt3}, 1,
	    {10.0, 30.0}}
    };
    point_t bbox_min = {-radius, -radius, -radius};
    point_t bbox_max = {radius, radius, radius};
    failures += check_shared_crofton_fixture("sewn-cobb-sphere", NULL, tol,
	bbox_min, bbox_max, 4.0 * M_PI * radius * radius,
	(4.0 / 3.0) * M_PI * radius * radius * radius, 8000, rays,
	sizeof(rays) / sizeof(rays[0]), &implicit_model, &cobb_model);

    free_prepared_model(cobb_model);
    free_prepared_model(implicit_model);
    delete unsewn;
    delete sewn;
    return failures;
}


static bool
partition_results_match(const partition_result &first,
    const partition_result &second, double endpoint_tolerance,
    double &maximum_endpoint_error)
{
    maximum_endpoint_error = 0.0;
    if (first.overflow || second.overflow ||
	    first.partitions != second.partitions) {
	maximum_endpoint_error = INFINITY;
	return false;
    }
    for (size_t i = 0; i < first.partitions; ++i) {
	const double in_error = fabs(first.intervals[i].in_dist -
	    second.intervals[i].in_dist);
	const double out_error = fabs(first.intervals[i].out_dist -
	    second.intervals[i].out_dist);
	maximum_endpoint_error = std::max(maximum_endpoint_error,
	    std::max(in_error, out_error));
    }
    return maximum_endpoint_error <= endpoint_tolerance;
}


static bool
prepared_event_partitions_match(const struct rt_brep_shot_trace &trace,
	const partition_result &oracle, double endpoint_tolerance,
	double &maximum_endpoint_error)
{
    maximum_endpoint_error = 0.0;
    if (oracle.overflow || trace.local_event_segment_overflow ||
	    trace.local_event_final_segments != oracle.partitions ||
	    trace.local_event_stored_segments != oracle.partitions) {
	maximum_endpoint_error = INFINITY;
	return false;
    }
    for (size_t segment_index = 0; segment_index < oracle.partitions;
	    ++segment_index) {
	const double in_error = fabs(
	    trace.local_event_segment_in[segment_index] -
	    oracle.intervals[segment_index].in_dist);
	const double out_error = fabs(
	    trace.local_event_segment_out[segment_index] -
	    oracle.intervals[segment_index].out_dist);
	maximum_endpoint_error = std::max(maximum_endpoint_error,
	    std::max(in_error, out_error));
    }
    return maximum_endpoint_error <= endpoint_tolerance;
}


static sampled_ray
cobb_seam_grazing_ray(const cobb_seam_frame &frame,
    const ON_3dPoint &origin, double radius, double clearance, bool reverse)
{
    const double closest_radius = radius - clearance;
    const ON_3dPoint closest = origin + closest_radius * frame.normal;
    const double start_sign = reverse ? 1.0 : -1.0;
    const ON_3dPoint ray_origin = closest +
	start_sign * 2.0 * radius * frame.target_conormal;
    const ON_3dVector direction = reverse ? -frame.target_conormal :
	frame.target_conormal;
    sampled_ray ray;
    VSET(ray.origin, ray_origin.x, ray_origin.y, ray_origin.z);
    VSET(ray.direction, direction.x, direction.y, direction.z);
    return ray;
}


static size_t
brep_trace_unique_roots(const struct rt_brep_shot_trace &trace)
{
    std::vector<double> distances;
    distances.reserve(trace.stored_roots);
    for (size_t i = 0; i < trace.stored_roots; ++i)
	distances.push_back(trace.roots[i].dist);
    std::sort(distances.begin(), distances.end());
    size_t unique = 0;
    double previous = 0.0;
    for (size_t i = 0; i < distances.size(); ++i) {
	if (!unique || fabs(distances[i] - previous) >
		BREP_SAME_POINT_TOLERANCE) {
	    unique++;
	    previous = distances[i];
	}
    }
    return unique;
}


static const struct rt_brep_trace_edge *
brep_trace_edge(const struct rt_brep_shot_trace &trace, int edge_index)
{
    for (size_t i = 0; i < trace.stored_edges; ++i) {
	if (trace.edges[i].edge_index == edge_index)
	    return &trace.edges[i];
    }
    return NULL;
}


static bool
brep_trace_root_isolated(const struct rt_brep_shot_trace &trace,
    const struct rt_brep_trace_root &root)
{
    const double parameter_tolerance = 1.0e-12;
    for (size_t i = 0; i < trace.stored_surface_boxes; ++i) {
	const struct rt_brep_trace_surface_box &box = trace.surface_boxes[i];
	if (box.face_index == root.face_index &&
		root.uv[0] >= box.uv_min[0] - parameter_tolerance &&
		root.uv[0] <= box.uv_max[0] + parameter_tolerance &&
		root.uv[1] >= box.uv_min[1] - parameter_tolerance &&
		root.uv[1] <= box.uv_max[1] + parameter_tolerance &&
		root.dist >= box.t_min - 1.0e-7 &&
		root.dist <= box.t_max + 1.0e-7)
	    return true;
    }
    return false;
}


static ON_Xform
cobb_similarity_transform(double scale, const ON_3dVector &translation)
{
    /* A cyclic axis permutation is a proper 120-degree rotation. */
    ON_Xform xform(ON_Xform::IdentityTransformation);
    xform[0][0] = 0.0;
    xform[0][1] = 0.0;
    xform[0][2] = scale;
    xform[0][3] = translation.x;
    xform[1][0] = scale;
    xform[1][1] = 0.0;
    xform[1][2] = 0.0;
    xform[1][3] = translation.y;
    xform[2][0] = 0.0;
    xform[2][1] = scale;
    xform[2][2] = 0.0;
    xform[2][3] = translation.z;
    return xform;
}


static ON_Xform
cobb_axis_angle_similarity_transform(double scale,
    const ON_3dVector &translation, ON_3dVector axis, double angle)
{
    ON_Xform xform(ON_Xform::IdentityTransformation);
    if (!axis.Unitize())
	return xform;
    const double c = cos(angle);
    const double s = sin(angle);
    const double v = 1.0 - c;
    const double rotation[3][3] = {
	{axis.x * axis.x * v + c,
	 axis.x * axis.y * v - axis.z * s,
	 axis.x * axis.z * v + axis.y * s},
	{axis.y * axis.x * v + axis.z * s,
	 axis.y * axis.y * v + c,
	 axis.y * axis.z * v - axis.x * s},
	{axis.z * axis.x * v - axis.y * s,
	 axis.z * axis.y * v + axis.x * s,
	 axis.z * axis.z * v + c}
    };
    for (int row = 0; row < 3; ++row) {
	for (int column = 0; column < 3; ++column)
	    xform[row][column] = scale * rotation[row][column];
    }
    xform[0][3] = translation.x;
    xform[1][3] = translation.y;
    xform[2][3] = translation.z;
    return xform;
}


static ON_Xform
cobb_axis_angle_affine_transform(const ON_3dVector &scale,
    const ON_3dVector &translation, ON_3dVector axis, double angle)
{
    ON_Xform xform(ON_Xform::IdentityTransformation);
    if (!axis.Unitize())
	return xform;
    const double c = cos(angle);
    const double s = sin(angle);
    const double v = 1.0 - c;
    const double rotation[3][3] = {
	{axis.x * axis.x * v + c,
	 axis.x * axis.y * v - axis.z * s,
	 axis.x * axis.z * v + axis.y * s},
	{axis.y * axis.x * v + axis.z * s,
	 axis.y * axis.y * v + c,
	 axis.y * axis.z * v - axis.x * s},
	{axis.z * axis.x * v - axis.y * s,
	 axis.z * axis.y * v + axis.x * s,
	 axis.z * axis.z * v + c}
    };
    const double scales[3] = {scale.x, scale.y, scale.z};
    for (int row = 0; row < 3; ++row) {
	for (int column = 0; column < 3; ++column)
	    xform[row][column] = rotation[row][column] * scales[column];
    }
    xform[0][3] = translation.x;
    xform[1][3] = translation.y;
    xform[2][3] = translation.z;
    return xform;
}


static ON_3dPoint
cobb_transform_point(const ON_Xform &xform, const ON_3dPoint &point)
{
    return ON_3dPoint(
	xform[0][0] * point.x + xform[0][1] * point.y +
	xform[0][2] * point.z + xform[0][3],
	xform[1][0] * point.x + xform[1][1] * point.y +
	xform[1][2] * point.z + xform[1][3],
	xform[2][0] * point.x + xform[2][1] * point.y +
	xform[2][2] * point.z + xform[2][3]);
}


static ON_3dVector
cobb_transform_vector(const ON_Xform &xform, const ON_3dVector &vector)
{
    return ON_3dVector(
	xform[0][0] * vector.x + xform[0][1] * vector.y +
	xform[0][2] * vector.z,
	xform[1][0] * vector.x + xform[1][1] * vector.y +
	xform[1][2] * vector.z,
	xform[2][0] * vector.x + xform[2][1] * vector.y +
	xform[2][2] * vector.z);
}


struct exact_dyadic {
    int64_t mantissa;
    int exponent;
};


static bool
exact_dyadic_normalize(exact_dyadic &value)
{
    while (value.mantissa && !(value.mantissa % 2)) {
	value.mantissa /= 2;
	value.exponent++;
    }
    return true;
}


static bool
exact_dyadic_scale_mantissa(int64_t mantissa, int shift, int64_t &result)
{
    if (shift < 0 || shift >= 63)
	return false;
    const int64_t factor = INT64_C(1) << shift;
    if ((mantissa > 0 && mantissa > INT64_MAX / factor) ||
	    (mantissa < 0 && mantissa < INT64_MIN / factor))
	return false;
    result = mantissa * factor;
    return true;
}


static bool
exact_dyadic_add(const exact_dyadic &first, const exact_dyadic &second,
    exact_dyadic &result)
{
    const int exponent = std::min(first.exponent, second.exponent);
    const int first_shift = first.exponent - exponent;
    const int second_shift = second.exponent - exponent;
    int64_t first_mantissa;
    int64_t second_mantissa;
    if (!exact_dyadic_scale_mantissa(first.mantissa, first_shift,
	    first_mantissa) ||
	    !exact_dyadic_scale_mantissa(second.mantissa, second_shift,
		second_mantissa))
	return false;
    if ((second_mantissa > 0 &&
	    first_mantissa > INT64_MAX - second_mantissa) ||
	    (second_mantissa < 0 &&
	    first_mantissa < INT64_MIN - second_mantissa))
	return false;
    result.mantissa = first_mantissa + second_mantissa;
    result.exponent = exponent;
    return exact_dyadic_normalize(result);
}


static bool
exact_dyadic_multiply(const exact_dyadic &first,
    const exact_dyadic &second, exact_dyadic &result)
{
    const int64_t a = first.mantissa;
    const int64_t b = second.mantissa;
    if ((a > 0 && b > 0 && a > INT64_MAX / b) ||
	    (a > 0 && b < 0 && b < INT64_MIN / a) ||
	    (a < 0 && b > 0 && a < INT64_MIN / b) ||
	    (a < 0 && b < 0 && a < INT64_MAX / b))
	return false;
    result.mantissa = a * b;
    result.exponent = first.exponent + second.exponent;
    return exact_dyadic_normalize(result);
}


static bool
exact_dyadic_subtract(const exact_dyadic &first,
    const exact_dyadic &second, exact_dyadic &result)
{
    if (second.mantissa == INT64_MIN)
	return false;
    const exact_dyadic negative = {-second.mantissa, second.exponent};
    return exact_dyadic_add(first, negative, result);
}


static bool
exact_dyadic_divide(const exact_dyadic &numerator,
    const exact_dyadic &denominator, exact_dyadic &result)
{
    exact_dyadic normalized_numerator = numerator;
    exact_dyadic normalized_denominator = denominator;
    exact_dyadic_normalize(normalized_numerator);
    exact_dyadic_normalize(normalized_denominator);
    if (!normalized_denominator.mantissa ||
	    normalized_numerator.mantissa %
	    normalized_denominator.mantissa)
	return false;
    result.mantissa = normalized_numerator.mantissa /
	normalized_denominator.mantissa;
    result.exponent = normalized_numerator.exponent -
	normalized_denominator.exponent;
    return exact_dyadic_normalize(result);
}


static bool
exact_dyadic_half_sum(const exact_dyadic &first,
    const exact_dyadic &second, exact_dyadic &result)
{
    if (!exact_dyadic_add(first, second, result))
	return false;
    result.exponent--;
    return exact_dyadic_normalize(result);
}


static bool
exact_dyadic_midpoint(const exact_dyadic *input, int order,
    exact_dyadic &result)
{
    if (!input || order < 1 || order > 16)
	return false;
    exact_dyadic work[16];
    for (int i = 0; i < order; ++i)
	work[i] = input[i];
    for (int level = 1; level < order; ++level) {
	for (int i = 0; i < order - level; ++i) {
	    if (!exact_dyadic_half_sum(work[i], work[i + 1], work[i]))
		return false;
	}
    }
    result = work[0];
    return true;
}


static bool
exact_dyadic_bezier_split(const exact_dyadic *input, int order,
    const exact_dyadic &parameter, exact_dyadic *left,
    exact_dyadic *right)
{
    if (!input || !left || !right || order < 2 || order > 16)
	return false;
    exact_dyadic complement;
    if (!exact_dyadic_subtract({1, 0}, parameter, complement))
	return false;
    exact_dyadic work[16];
    for (int i = 0; i < order; ++i)
	work[i] = input[i];
    left[0] = work[0];
    right[order - 1] = work[order - 1];
    for (int level = 1; level < order; ++level) {
	for (int i = 0; i < order - level; ++i) {
	    exact_dyadic first;
	    exact_dyadic second;
	    if (!exact_dyadic_multiply(complement, work[i], first) ||
		    !exact_dyadic_multiply(parameter, work[i + 1], second) ||
		    !exact_dyadic_add(first, second, work[i]))
		return false;
	}
	left[level] = work[0];
	right[order - level - 1] = work[order - level - 1];
    }
    return true;
}


static bool
exact_dyadic_bezier_split_half(const exact_dyadic *input, int order,
    exact_dyadic *left, exact_dyadic *right)
{
    return exact_dyadic_bezier_split(input, order, {1, -1}, left, right);
}


static bool
exact_dyadic_bezier_restrict_quarters(const exact_dyadic *input, int order,
    int minimum_quarters, int maximum_quarters, exact_dyadic *output)
{
    if (!input || !output || minimum_quarters < 0 ||
	    maximum_quarters > 4 || minimum_quarters >= maximum_quarters)
	return false;
    exact_dyadic first[16];
    exact_dyadic second[16];
    const exact_dyadic *current = input;
    if (maximum_quarters == 2) {
	if (!exact_dyadic_bezier_split_half(input, order, first, second))
	    return false;
	current = first;
    } else if (maximum_quarters != 4) {
	return false;
    }
    if (minimum_quarters) {
	const bool half_of_current =
	    (maximum_quarters == 2 && minimum_quarters == 1) ||
	    (maximum_quarters == 4 && minimum_quarters == 2);
	if (!half_of_current ||
		!exact_dyadic_bezier_split_half(current, order, first, second))
	    return false;
	current = second;
    }
    for (int i = 0; i < order; ++i)
	output[i] = current[i];
    return true;
}


static bool
exact_dyadic_surface_restrict_quarters(const exact_dyadic *input,
    int u_order, int v_order, const int minimum_quarters[2],
    const int maximum_quarters[2], exact_dyadic *output)
{
    exact_dyadic u_restricted[256];
    exact_dyadic source[16];
    exact_dyadic result[16];
    for (int j = 0; j < v_order; ++j) {
	for (int i = 0; i < u_order; ++i)
	    source[i] = input[(size_t)i * v_order + j];
	if (!exact_dyadic_bezier_restrict_quarters(source, u_order,
		minimum_quarters[0], maximum_quarters[0], result))
	    return false;
	for (int i = 0; i < u_order; ++i)
	    u_restricted[(size_t)i * v_order + j] = result[i];
    }
    for (int i = 0; i < u_order; ++i) {
	for (int j = 0; j < v_order; ++j)
	    source[j] = u_restricted[(size_t)i * v_order + j];
	if (!exact_dyadic_bezier_restrict_quarters(source, v_order,
		minimum_quarters[1], maximum_quarters[1], result))
	    return false;
	for (int j = 0; j < v_order; ++j)
	    output[(size_t)i * v_order + j] = result[j];
    }
    return true;
}


static bool
exact_dyadic_equal(exact_dyadic first, exact_dyadic second)
{
    exact_dyadic_normalize(first);
    exact_dyadic_normalize(second);
    return first.mantissa == second.mantissa &&
	first.exponent == second.exponent;
}


static bool
exact_dyadic_bezier_reparameterize(const exact_dyadic *input, int order,
    const exact_dyadic &minimum, const exact_dyadic &maximum,
    exact_dyadic *output)
{
    if (!input || !output || order < 2 || order > 16)
	return false;
    if (exact_dyadic_equal(minimum, {0, 0}) &&
	    exact_dyadic_equal(maximum, {1, 0})) {
	for (int i = 0; i < order; ++i)
	    output[i] = input[i];
	return true;
    }

    exact_dyadic first[16];
    exact_dyadic second[16];
    exact_dyadic unused[16];
    exact_dyadic from_minimum;
    if (!exact_dyadic_subtract({1, 0}, minimum, from_minimum))
	return false;
    if (from_minimum.mantissa) {
	if (!exact_dyadic_bezier_split(input, order, minimum, unused,
		second))
	    return false;
	exact_dyadic numerator;
	exact_dyadic local_maximum;
	if (!exact_dyadic_subtract(maximum, minimum, numerator) ||
		!exact_dyadic_divide(numerator, from_minimum,
		    local_maximum) ||
		!exact_dyadic_bezier_split(second, order, local_maximum,
		    first, unused))
	    return false;
	for (int i = 0; i < order; ++i)
	    output[i] = first[i];
	return true;
    }

    if (!maximum.mantissa ||
	    !exact_dyadic_bezier_split(input, order, maximum, first, unused))
	return false;
    exact_dyadic local_minimum;
    if (!exact_dyadic_divide(minimum, maximum, local_minimum) ||
	    !exact_dyadic_bezier_split(first, order, local_minimum, unused,
		second))
	return false;
    for (int i = 0; i < order; ++i)
	output[i] = second[i];
    return true;
}


static bool
exact_dyadic_surface_reparameterize(const exact_dyadic *input, int u_order,
    int v_order, const exact_dyadic minimum[2],
    const exact_dyadic maximum[2], exact_dyadic *output)
{
    exact_dyadic u_reparameterized[256];
    exact_dyadic source[16];
    exact_dyadic result[16];
    for (int j = 0; j < v_order; ++j) {
	for (int i = 0; i < u_order; ++i)
	    source[i] = input[(size_t)i * v_order + j];
	if (!exact_dyadic_bezier_reparameterize(source, u_order, minimum[0],
		maximum[0], result))
	    return false;
	for (int i = 0; i < u_order; ++i)
	    u_reparameterized[(size_t)i * v_order + j] = result[i];
    }
    for (int i = 0; i < u_order; ++i) {
	for (int j = 0; j < v_order; ++j)
	    source[j] = u_reparameterized[(size_t)i * v_order + j];
	if (!exact_dyadic_bezier_reparameterize(source, v_order, minimum[1],
		maximum[1], result))
	    return false;
	for (int j = 0; j < v_order; ++j)
	    output[(size_t)i * v_order + j] = result[j];
    }
    return true;
}


static bool
exact_dyadic_bezier_reparameterization_row_sums(int order,
    const exact_dyadic &minimum, const exact_dyadic &maximum,
    exact_dyadic *row_sums)
{
    if (!row_sums || order < 2 || order > 16)
	return false;
    for (int row = 0; row < order; ++row)
	row_sums[row] = {0, 0};
    for (int basis = 0; basis < order; ++basis) {
	exact_dyadic input[16] = {};
	exact_dyadic output[16];
	input[basis] = {1, 0};
	if (!exact_dyadic_bezier_reparameterize(input, order, minimum,
		maximum, output))
	    return false;
	for (int row = 0; row < order; ++row) {
	    if (output[row].mantissa == INT64_MIN)
		return false;
	    exact_dyadic magnitude = {llabs(output[row].mantissa),
		output[row].exponent};
	    if (!exact_dyadic_add(row_sums[row], magnitude, row_sums[row]))
		return false;
	}
    }
    return true;
}


static bool
exact_dyadic_surface_midpoint(const exact_dyadic *input, int u_order,
    int v_order, exact_dyadic &result)
{
    exact_dyadic source[16];
    exact_dyadic v_control[16];
    for (int j = 0; j < v_order; ++j) {
	for (int i = 0; i < u_order; ++i)
	    source[i] = input[(size_t)i * v_order + j];
	if (!exact_dyadic_midpoint(source, u_order, v_control[j]))
	    return false;
    }
    return exact_dyadic_midpoint(v_control, v_order, result);
}


static long double
exact_dyadic_value(const exact_dyadic &value)
{
    return std::ldexp((long double)value.mantissa, value.exponent);
}


static int
check_brep_interval_enclosures()
{
    const int scale_exponents[] = {-400, -40, 0, 40, 400};
    const int product_exponents[] = {-200, 0, 200};
    const int coordinate_exponents[] = {-300, 0, 300};
    const int direction_exponents[] = {-100, 0, 100};
    const int plane_exponents[] = {-50, 0, 50};
    const int64_t product_intervals[][4] = {
	{-3, 5, -7, 2}, {1, 4, 2, 8}, {-9, -2, -5, -1},
	{-8, -1, 2, 7}, {0, 3, -4, 0}
    };
    const int division_intervals[][4] = {
	{-7, 5, 0, 2}, {1, 9, 1, 3}, {-9, -1, 0, 3}, {0, 7, 1, 2}
    };
    const int restriction_quarters[][4] = {
	{0, 0, 2, 2}, {1, 2, 2, 4}, {2, 1, 4, 2}
    };
    struct exact_reparameterization_case {
	exact_dyadic u_minimum;
	exact_dyadic u_maximum;
	exact_dyadic v_minimum;
	exact_dyadic v_maximum;
    } reparameterization_cases[] = {
	{{-1, 0}, {1, 0}, {0, 0}, {2, 0}},
	{{0, 0}, {2, 0}, {1, 0}, {2, 0}},
	{{-1, -1}, {1, 0}, {-1, 0}, {1, 0}},
	{{-1, -10}, {1, 0}, {0, 0}, {1025, -10}},
	{{2, 0}, {3, 0}, {0, 0}, {1, 0}}
    };
    double values[2][256] = {};
    exact_dyadic exact[2][256] = {};
    const fastf_t root[2] = {0.5, 0.5};
    size_t cases = 0;
    size_t function_checks = 0;
    size_t derivative_checks = 0;
    size_t product_checks = 0;
    size_t division_checks = 0;
    size_t coefficient_checks = 0;
    size_t restriction_checks = 0;
    size_t reparameterization_checks = 0;
    size_t clip_checks = 0;
    size_t clip_contractions = 0;
    long double maximum_function_width_ratio = 0.0L;
    long double maximum_restriction_width_ratio = 0.0L;
    long double maximum_reparameterization_width_ratio = 0.0L;
    int failures = 0;

    for (size_t coordinate_scale = 0; coordinate_scale <
	    sizeof(coordinate_exponents) / sizeof(coordinate_exponents[0]);
	    ++coordinate_scale) {
	for (size_t direction_scale = 0; direction_scale <
		sizeof(direction_exponents) / sizeof(direction_exponents[0]);
		++direction_scale) {
	    for (size_t plane_scale = 0; plane_scale <
		    sizeof(plane_exponents) / sizeof(plane_exponents[0]);
		    ++plane_scale) {
		for (int weight = 1; weight <= 2; ++weight) {
		    const int64_t cv_mantissa[3] = {5, -7, 4};
		    const int64_t origin_mantissa[3] = {2, -3, 1};
		    const int64_t direction_mantissa[3] = {1, 1, 0};
		    const int64_t plane_mantissa[2][3] = {
			{1, -2, 3}, {-3, 1, 2}
		    };
		    fastf_t cv[4];
		    fastf_t origin[3];
		    fastf_t direction[3];
		    fastf_t planes[2][3];
		    for (int component = 0; component < 3; ++component) {
			cv[component] = std::ldexp((double)cv_mantissa[component],
			    coordinate_exponents[coordinate_scale]);
			origin[component] = std::ldexp(
			    (double)origin_mantissa[component],
			    coordinate_exponents[coordinate_scale]);
			direction[component] = std::ldexp(
			    (double)direction_mantissa[component],
			    direction_exponents[direction_scale]);
			for (int equation = 0; equation < 2; ++equation) {
			    planes[equation][component] = std::ldexp(
				(double)plane_mantissa[equation][component],
				plane_exponents[plane_scale]);
			}
		    }
		    cv[3] = weight;
		    struct rt_brep_coefficient_test_result observed = {};
		    if (!_rt_brep_coefficient_test(cv, origin, direction, planes,
			    &observed)) {
			std::printf("FAIL: coefficient interval unavailable "
			    "scale=%d/%d/%d weight=%d\n",
			    coordinate_exponents[coordinate_scale],
			    direction_exponents[direction_scale],
			    plane_exponents[plane_scale], weight);
			failures++;
			continue;
		    }
		    int64_t numerator_mantissa[3];
		    for (int component = 0; component < 3; ++component)
			numerator_mantissa[component] = cv_mantissa[component] -
			    weight * origin_mantissa[component];
		    for (int equation = 0; equation < 2; ++equation) {
			int64_t dot_mantissa = 0;
			for (int component = 0; component < 3; ++component)
			    dot_mantissa += numerator_mantissa[component] *
				plane_mantissa[equation][component];
			const long double exact_value = std::ldexp(
			    (long double)dot_mantissa,
			    coordinate_exponents[coordinate_scale] +
			    plane_exponents[plane_scale]);
			coefficient_checks++;
			if ((long double)observed.function_minimum[equation] >
				exact_value ||
				(long double)observed.function_maximum[equation] <
				exact_value) {
			    std::printf("FAIL: coefficient function enclosure "
				"scale=%d/%d/%d weight=%d equation=%d\n",
				coordinate_exponents[coordinate_scale],
				direction_exponents[direction_scale],
				plane_exponents[plane_scale], weight, equation);
			    failures++;
			}
		    }
		    const int64_t ray_dot_mantissa =
			numerator_mantissa[0] + numerator_mantissa[1];
		    const long double exact_ray = std::ldexp(
			(long double)ray_dot_mantissa,
			coordinate_exponents[coordinate_scale] -
			direction_exponents[direction_scale] - 1);
		    coefficient_checks++;
		    if ((long double)observed.ray_minimum > exact_ray ||
			    (long double)observed.ray_maximum < exact_ray) {
			std::printf("FAIL: coefficient ray enclosure "
			    "scale=%d/%d/%d weight=%d\n",
			    coordinate_exponents[coordinate_scale],
			    direction_exponents[direction_scale],
			    plane_exponents[plane_scale], weight);
			failures++;
		    }
		}
	    }
	}
    }

    for (size_t interval_index = 0; interval_index <
	    sizeof(product_intervals) / sizeof(product_intervals[0]);
	    ++interval_index) {
	for (size_t first_scale = 0; first_scale <
		sizeof(product_exponents) / sizeof(product_exponents[0]);
		++first_scale) {
	    for (size_t second_scale = 0; second_scale <
		    sizeof(product_exponents) / sizeof(product_exponents[0]);
		    ++second_scale) {
		fastf_t first[2] = {
		    std::ldexp((double)product_intervals[interval_index][0],
			product_exponents[first_scale]),
		    std::ldexp((double)product_intervals[interval_index][1],
			product_exponents[first_scale])
		};
		fastf_t second[2] = {
		    std::ldexp((double)product_intervals[interval_index][2],
			product_exponents[second_scale]),
		    std::ldexp((double)product_intervals[interval_index][3],
			product_exponents[second_scale])
		};
		fastf_t observed[2] = {};
		const int product_exponent = product_exponents[first_scale] +
		    product_exponents[second_scale];
		int64_t exact_products[4] = {
		    product_intervals[interval_index][0] *
			product_intervals[interval_index][2],
		    product_intervals[interval_index][0] *
			product_intervals[interval_index][3],
		    product_intervals[interval_index][1] *
			product_intervals[interval_index][2],
		    product_intervals[interval_index][1] *
			product_intervals[interval_index][3]
		};
		const int64_t exact_minimum = *std::min_element(exact_products,
		    exact_products + 4);
		const int64_t exact_maximum = *std::max_element(exact_products,
		    exact_products + 4);
		const long double expected_minimum = std::ldexp(
		    (long double)exact_minimum, product_exponent);
		const long double expected_maximum = std::ldexp(
		    (long double)exact_maximum, product_exponent);
		product_checks++;
		if (!_rt_brep_interval_product_test(first, second, observed) ||
			(long double)observed[0] > expected_minimum ||
			(long double)observed[1] < expected_maximum) {
		    std::printf("FAIL: interval product enclosure case=%zu "
			"scale=%d/%d\n", interval_index,
			product_exponents[first_scale],
			product_exponents[second_scale]);
		    failures++;
		}
	    }
	}
    }

    for (size_t interval_index = 0; interval_index <
	    sizeof(division_intervals) / sizeof(division_intervals[0]);
	    ++interval_index) {
	for (size_t numerator_scale = 0; numerator_scale <
		sizeof(product_exponents) / sizeof(product_exponents[0]);
		++numerator_scale) {
	    for (size_t denominator_scale = 0; denominator_scale <
		    sizeof(product_exponents) / sizeof(product_exponents[0]);
		    ++denominator_scale) {
		fastf_t numerator[2] = {
		    std::ldexp((double)division_intervals[interval_index][0],
			product_exponents[numerator_scale]),
		    std::ldexp((double)division_intervals[interval_index][1],
			product_exponents[numerator_scale])
		};
		fastf_t denominator[2] = {
		    std::ldexp(1.0, product_exponents[denominator_scale] +
			division_intervals[interval_index][2]),
		    std::ldexp(1.0, product_exponents[denominator_scale] +
			division_intervals[interval_index][3])
		};
		long double exact_quotients[4];
		size_t quotient = 0;
		for (int numerator_endpoint = 0; numerator_endpoint < 2;
			numerator_endpoint++) {
		    for (int denominator_endpoint = 0; denominator_endpoint < 2;
			    denominator_endpoint++) {
			exact_quotients[quotient++] = std::ldexp(
			    (long double)division_intervals[interval_index]
				[numerator_endpoint],
			    product_exponents[numerator_scale] -
			    product_exponents[denominator_scale] -
			    division_intervals[interval_index]
				[2 + denominator_endpoint]);
		    }
		}
		const long double exact_minimum = *std::min_element(
		    exact_quotients, exact_quotients + 4);
		const long double exact_maximum = *std::max_element(
		    exact_quotients, exact_quotients + 4);
		fastf_t observed[2] = {};
		division_checks++;
		if (!_rt_brep_interval_divide_test(numerator, denominator,
			observed) || (long double)observed[0] > exact_minimum ||
			(long double)observed[1] < exact_maximum) {
		    std::printf("FAIL: interval quotient enclosure case=%zu "
			"scale=%d/%d\n", interval_index,
			product_exponents[numerator_scale],
			product_exponents[denominator_scale]);
		    failures++;
		}
	    }
	}
    }

    for (int u_order = 2; u_order <= 16; ++u_order) {
	for (int v_order = 2; v_order <= 16; ++v_order) {
	    const size_t count = (size_t)u_order * v_order;
	    for (size_t scale_index = 0; scale_index <
		    sizeof(scale_exponents) / sizeof(scale_exponents[0]);
		    ++scale_index) {
		const int scale_exponent = scale_exponents[scale_index];
		for (int equation = 0; equation < 2; ++equation) {
		    for (int i = 0; i < u_order; ++i) {
			for (int j = 0; j < v_order; ++j) {
			    const size_t index = (size_t)i * v_order + j;
			    int64_t mantissa = (i * 5 + j * 3 + equation * 2 +
				u_order + 2 * v_order) % 7 - 3;
			    if (!mantissa)
				mantissa = ((i + j + equation) & 1) ? 1 : -1;
			    exact[equation][index] = {mantissa,
				scale_exponent};
			    values[equation][index] = std::ldexp(
				(double)mantissa, scale_exponent);
			}
		    }
		    for (size_t i = count; i < 256; ++i) {
			exact[equation][i] = {0, 0};
			values[equation][i] = 0.0;
		    }
		}
		const fastf_t coefficient_error[2] = {
		    std::ldexp(1.0, scale_exponent - 32),
		    std::ldexp(1.0, scale_exponent - 32)
		};
		struct rt_brep_interval_test_result observed = {};
		cases++;
		if (!_rt_brep_interval_test(values[0], values[1], u_order,
			v_order, coefficient_error, root, &observed)) {
		    std::printf("FAIL: interval audit unavailable order=%d/%d "
			"scale=%d\n", u_order, v_order, scale_exponent);
		    failures++;
		    continue;
		}

		for (int equation = 0; equation < 2; ++equation) {
		    exact_dyadic midpoint = {};
		    if (!exact_dyadic_surface_midpoint(exact[equation], u_order,
			    v_order, midpoint)) {
			failures++;
			continue;
		    }
		    const long double nominal = exact_dyadic_value(midpoint);
		    const long double error = coefficient_error[equation];
		    const long double lower =
			observed.function_minimum[equation];
		    const long double upper =
			observed.function_maximum[equation];
		    const long double width_ratio =
			(upper - lower) / (2.0L * error);
		    maximum_function_width_ratio = std::max(
			maximum_function_width_ratio, width_ratio);
		    function_checks++;
		    if (!std::isfinite((double)lower) ||
			    !std::isfinite((double)upper) ||
			    lower > nominal - error || upper < nominal + error ||
			    !(width_ratio <= 1.05L)) {
			std::printf("FAIL: interval function enclosure order=%d/%d "
			    "scale=%d equation=%d width/error=%.9Lg\n",
			    u_order, v_order, scale_exponent, equation,
			    width_ratio);
			failures++;
		    }

		    for (int direction = 0; direction < 2; ++direction) {
			const int degree = direction == 0 ? u_order - 1 :
			    v_order - 1;
			const int first_count = direction == 0 ? u_order - 1 :
			    u_order;
			const int second_count = direction == 0 ? v_order :
			    v_order - 1;
			const long double derivative_error =
			    2.0L * degree * error;
			for (int i = 0; i < first_count; ++i) {
			    for (int j = 0; j < second_count; ++j) {
				const size_t previous =
				    (size_t)i * v_order + j;
				const size_t next = direction == 0 ?
				    (size_t)(i + 1) * v_order + j :
				    (size_t)i * v_order + j + 1;
				const int64_t derivative_mantissa = degree *
				    (exact[equation][next].mantissa -
				     exact[equation][previous].mantissa);
				const long double derivative = std::ldexp(
				    (long double)derivative_mantissa,
				    scale_exponent);
				derivative_checks++;
				if ((long double)observed.jacobian_minimum
					[equation][direction] >
					derivative - derivative_error ||
					(long double)observed.jacobian_maximum
					[equation][direction] <
					derivative + derivative_error) {
				    std::printf("FAIL: interval derivative "
					"enclosure order=%d/%d scale=%d "
					"equation=%d direction=%d\n",
					u_order, v_order, scale_exponent,
					equation, direction);
				    failures++;
				}
			    }
			}
		    }

			    for (size_t restriction = 0; restriction <
			    sizeof(restriction_quarters) /
			    sizeof(restriction_quarters[0]); ++restriction) {
			const int minimum_quarters[2] = {
			    restriction_quarters[restriction][0],
			    restriction_quarters[restriction][1]
			};
			const int maximum_quarters[2] = {
			    restriction_quarters[restriction][2],
			    restriction_quarters[restriction][3]
			};
			const fastf_t minimum[2] = {
			    0.25 * minimum_quarters[0],
			    0.25 * minimum_quarters[1]
			};
			const fastf_t maximum[2] = {
			    0.25 * maximum_quarters[0],
			    0.25 * maximum_quarters[1]
			};
			exact_dyadic expected[256];
			fastf_t restricted[256] = {};
			fastf_t restricted_error = 0.0;
			if (!exact_dyadic_surface_restrict_quarters(exact[equation],
				u_order, v_order, minimum_quarters,
				maximum_quarters, expected) ||
				!_rt_brep_restrict_test(values[equation], u_order,
				v_order, coefficient_error[equation], minimum,
				maximum, restricted, &restricted_error)) {
			    std::printf("FAIL: interval restriction unavailable "
				"order=%d/%d scale=%d case=%zu\n", u_order,
				v_order, scale_exponent, restriction);
			    failures++;
			    continue;
			}
			const long double exact_error = coefficient_error[equation];
			const long double restriction_width_ratio = restricted_error /
			    exact_error;
			maximum_restriction_width_ratio = std::max(
			    maximum_restriction_width_ratio,
			    restriction_width_ratio);
			for (size_t i = 0; i < count; ++i) {
			    const long double restricted_nominal =
				exact_dyadic_value(expected[i]);
			    const long double restricted_lower = std::nextafter(
				restricted[i] - restricted_error, -INFINITY);
			    const long double restricted_upper = std::nextafter(
				restricted[i] + restricted_error, INFINITY);
			    restriction_checks++;
			    if (!std::isfinite((double)restricted_lower) ||
				    !std::isfinite((double)restricted_upper) ||
				    restricted_lower >
					restricted_nominal - exact_error ||
				    restricted_upper <
					restricted_nominal + exact_error ||
				    !(restriction_width_ratio <= 1.05L)) {
				std::printf("FAIL: interval restriction enclosure "
				    "order=%d/%d scale=%d case=%zu "
				    "coefficient=%zu width/error=%.9Lg\n",
				    u_order, v_order, scale_exponent,
				    restriction, i, restriction_width_ratio);
				failures++;
				    }
				}
			    }

			    for (size_t reparameterization = 0; reparameterization <
				    sizeof(reparameterization_cases) /
				    sizeof(reparameterization_cases[0]);
				    ++reparameterization) {
				if (reparameterization == 3 && u_order + v_order > 8)
				    continue;
				const exact_reparameterization_case &test =
				    reparameterization_cases[reparameterization];
				const exact_dyadic exact_minimum[2] = {
				    test.u_minimum, test.v_minimum
				};
				const exact_dyadic exact_maximum[2] = {
				    test.u_maximum, test.v_maximum
				};
				const fastf_t minimum[2] = {
				    (fastf_t)exact_dyadic_value(exact_minimum[0]),
				    (fastf_t)exact_dyadic_value(exact_minimum[1])
				};
				const fastf_t maximum[2] = {
				    (fastf_t)exact_dyadic_value(exact_maximum[0]),
				    (fastf_t)exact_dyadic_value(exact_maximum[1])
				};
				exact_dyadic expected[256];
				exact_dyadic u_row_sums[16];
				exact_dyadic v_row_sums[16];
				fastf_t reparameterized[256] = {};
				fastf_t reparameterized_error = 0.0;
				if (!exact_dyadic_surface_reparameterize(exact[equation],
					u_order, v_order, exact_minimum,
					exact_maximum, expected) ||
					!exact_dyadic_bezier_reparameterization_row_sums(
					u_order, exact_minimum[0], exact_maximum[0],
					u_row_sums) ||
					!exact_dyadic_bezier_reparameterization_row_sums(
					v_order, exact_minimum[1], exact_maximum[1],
					v_row_sums) ||
					!_rt_brep_reparameterize_test(values[equation],
					u_order, v_order, coefficient_error[equation],
					minimum, maximum, reparameterized,
					&reparameterized_error)) {
				    std::printf("FAIL: interval reparameterization "
					"unavailable order=%d/%d scale=%d "
					"equation=%d case=%zu\n", u_order, v_order,
					scale_exponent, equation, reparameterization);
				    failures++;
				    continue;
				}

				long double exact_errors[256];
				long double maximum_exact_error = 0.0L;
				const exact_dyadic input_error = {
				    1, scale_exponent - 32
				};
				bool exact_error_available = true;
				for (int i = 0; i < u_order; ++i) {
				    for (int j = 0; j < v_order; ++j) {
					const size_t index =
					    (size_t)i * v_order + j;
					exact_dyadic directional_error;
					exact_dyadic tensor_error;
					if (!exact_dyadic_multiply(input_error,
						u_row_sums[i], directional_error) ||
						!exact_dyadic_multiply(
						directional_error, v_row_sums[j],
						tensor_error)) {
					    exact_error_available = false;
					    break;
					}
					exact_errors[index] =
					    exact_dyadic_value(tensor_error);
					maximum_exact_error = std::max(
					    maximum_exact_error, exact_errors[index]);
				    }
				    if (!exact_error_available)
					break;
				}
				if (!exact_error_available ||
					!(maximum_exact_error > 0.0L)) {
				    std::printf("FAIL: exact reparameterization error "
					"unavailable order=%d/%d scale=%d "
					"equation=%d case=%zu\n", u_order, v_order,
					scale_exponent, equation, reparameterization);
				    failures++;
				    continue;
				}
				const long double reparameterization_width_ratio =
				    reparameterized_error / maximum_exact_error;
				maximum_reparameterization_width_ratio = std::max(
				    maximum_reparameterization_width_ratio,
				    reparameterization_width_ratio);
				for (size_t i = 0; i < count; ++i) {
				    const long double reparameterized_nominal =
					exact_dyadic_value(expected[i]);
				    const long double reparameterized_lower = std::nextafter(
					reparameterized[i] - reparameterized_error,
					-INFINITY);
				    const long double reparameterized_upper = std::nextafter(
					reparameterized[i] + reparameterized_error,
					INFINITY);
				    reparameterization_checks++;
				    if (!std::isfinite((double)reparameterized_lower) ||
					    !std::isfinite(
					    (double)reparameterized_upper) ||
					    reparameterized_lower >
					    reparameterized_nominal - exact_errors[i] ||
					    reparameterized_upper <
					    reparameterized_nominal + exact_errors[i] ||
					    !(reparameterization_width_ratio <= 1.05L)) {
					std::printf("FAIL: interval reparameterization "
					    "enclosure order=%d/%d scale=%d "
					    "equation=%d case=%zu coefficient=%zu "
					    "width/error=%.9Lg\n", u_order,
					    v_order, scale_exponent, equation,
					    reparameterization, i,
					    reparameterization_width_ratio);
					failures++;
				    }
				}
			    }
		}
	    }
	}
    }

    const double clip_roots[][2] = {
	{0.125, 0.875}, {0.5, 0.5}, {0.9375, 0.0625}
    };
    const int clip_scale_exponents[] = {-200, 0, 200};
    for (int u_order = 2; u_order <= 16; ++u_order) {
	for (int v_order = 2; v_order <= 16; ++v_order) {
	    for (size_t root_index = 0; root_index <
		    sizeof(clip_roots) / sizeof(clip_roots[0]); ++root_index) {
		for (size_t scale_index = 0; scale_index <
			sizeof(clip_scale_exponents) /
			sizeof(clip_scale_exponents[0]); ++scale_index) {
		    const double scale = std::ldexp(1.0,
			clip_scale_exponents[scale_index]);
		    const fastf_t error[2] = {
			fabs(scale) * 1.0e-10, fabs(scale) * 1.0e-10
		    };
		    for (int i = 0; i < u_order; ++i) {
			const double u = (double)i / (u_order - 1);
			for (int j = 0; j < v_order; ++j) {
			    const double v = (double)j / (v_order - 1);
			    const size_t index = (size_t)i * v_order + j;
			    const int first_pattern =
				(int)((3 * i + 5 * j + root_index) % 5) - 2;
			    const int second_pattern =
				(int)((7 * i + 2 * j + root_index) % 5) - 2;
			    values[0][index] = scale *
				((u - clip_roots[root_index][0]) +
				0.125 * (v - clip_roots[root_index][1])) +
				0.25 * error[0] * first_pattern;
			    values[1][index] = scale *
				((v - clip_roots[root_index][1]) -
				0.2 * (u - clip_roots[root_index][0])) +
				0.25 * error[1] * second_pattern;
			}
		    }
		    fastf_t range[4] = {};
		    clip_checks++;
		    if (!_rt_brep_clip_test(values[0], values[1], u_order,
			    v_order, error, range) ||
			    range[0] > clip_roots[root_index][0] ||
			    range[1] < clip_roots[root_index][0] ||
			    range[2] > clip_roots[root_index][1] ||
			    range[3] < clip_roots[root_index][1]) {
			std::printf("FAIL: Bernstein clipping enclosure "
			    "order=%d/%d scale=%d root=%.17g/%.17g "
			    "range=%.17g/%.17g %.17g/%.17g\n", u_order,
			    v_order, clip_scale_exponents[scale_index],
			    clip_roots[root_index][0], clip_roots[root_index][1],
			    range[0], range[1], range[2], range[3]);
			failures++;
		    } else if (range[1] - range[0] < 1.0 ||
			    range[3] - range[2] < 1.0) {
			clip_contractions++;
		    }
		}
	    }
	}
    }
    if (clip_contractions != clip_checks) {
	std::printf("FAIL: Bernstein clipping did not contract %zu/%zu "
	    "affine systems\n", clip_checks - clip_contractions, clip_checks);
	failures++;
    }

    if (!failures) {
	std::printf("BREP interval enclosure audit: PASS cases=%zu "
	    "function=%zu derivative=%zu product=%zu quotient=%zu "
	    "coefficient=%zu restriction=%zu reparameterization=%zu "
	    "clip=%zu/%zu "
	    "max-width/error=%.9Lg/%.9Lg/%.9Lg\n",
	    cases, function_checks, derivative_checks, product_checks,
	    division_checks, coefficient_checks, restriction_checks,
	    reparameterization_checks, clip_contractions, clip_checks,
	    maximum_function_width_ratio,
	    maximum_restriction_width_ratio,
	    maximum_reparameterization_width_ratio);
    }
    return failures;
}


static int
check_sphere_adaptive_similarity(const struct bn_tol *tol)
{
    const double radius = 10.0;
    const ON_3dPoint base_center(0.0, 0.0, 0.0);
    struct similarity_case {
	const char *name;
	double scale;
	ON_3dVector translation;
	ON_3dVector axis;
	double angle;
    } cases[] = {
	{"identity", 1.0, ON_3dVector(0.0, 0.0, 0.0),
	    ON_3dVector(1.0, 0.0, 0.0), 0.0},
	{"translated", 1.0, ON_3dVector(-31.25, 47.5, 103.75),
	    ON_3dVector(1.0, 0.0, 0.0), 0.0},
	{"oblique", 1.0, ON_3dVector(-19.0, 23.0, 41.0),
	    ON_3dVector(1.0, -2.0, 0.5), 0.731},
	{"small", 0.01, ON_3dVector(1.25, -2.5, 5.0),
	    ON_3dVector(-0.3, 1.0, 0.7), -1.113},
	{"large", 1.0e4, ON_3dVector(1.0e6, -2.0e6, 3.0e6),
	    ON_3dVector(2.0, 0.25, -1.0), 2.017}
    };
    const ON_3dVector directions[] = {
	ON_3dVector(0.371, 0.529, 0.763),
	ON_3dVector(-0.613, 0.247, 0.751),
	ON_3dVector(0.193, -0.881, 0.432)
    };
    const double half_chord = radius * sqrt(1.0 - 0.31 * 0.31 -
	0.17 * 0.17);
    const double expected_in = 2.0 * radius - half_chord;
    const double expected_out = 2.0 * radius + half_chord;
    int failures = 0;
    size_t total_rays = 0;
    size_t total_krawczyk = 0;
    size_t total_boxes = 0;
    size_t minimum_boxes = SIZE_MAX;
    size_t maximum_boxes = 0;
    size_t minimum_depth = SIZE_MAX;
    size_t maximum_depth = 0;
    size_t total_clip_attempts = 0;
    size_t total_clip_contractions = 0;
    size_t total_clip_inconclusive = 0;
    size_t total_clip_restriction_failures = 0;
    double maximum_clip_fraction_removed = 0.0;
    double maximum_implicit_error = 0.0;
    double maximum_production_error = 0.0;
    double maximum_legacy_error = 0.0;
    double maximum_prepared_error = 0.0;

    for (size_t case_index = 0;
	    case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
	const similarity_case &test = cases[case_index];
	const ON_Xform xform = cobb_axis_angle_similarity_transform(
	    test.scale, test.translation, test.axis, test.angle);
	const ON_3dPoint center = cobb_transform_point(xform, base_center);
	const ON_3dVector a = cobb_transform_vector(xform,
	    ON_3dVector(radius, 0.0, 0.0));
	const ON_3dVector b = cobb_transform_vector(xform,
	    ON_3dVector(0.0, radius, 0.0));
	const ON_3dVector c = cobb_transform_vector(xform,
	    ON_3dVector(0.0, 0.0, radius));
	struct rt_ell_internal ell = {};
	ell.magic = RT_ELL_INTERNAL_MAGIC;
	VSET(ell.v, center.x, center.y, center.z);
	VSET(ell.a, a.x, a.y, a.z);
	VSET(ell.b, b.x, b.y, b.z);
	VSET(ell.c, c.x, c.y, c.z);
	struct rt_db_internal ell_intern;
	RT_DB_INTERNAL_INIT(&ell_intern);
	ell_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ell_intern.idb_type = ID_ELL;
	ell_intern.idb_meth = &OBJ[ID_ELL];
	ell_intern.idb_ptr = &ell;

	struct bn_tol case_tol = *tol;
	case_tol.dist = tol->dist * test.scale;
	case_tol.dist_sq = case_tol.dist * case_tol.dist;
	struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
	if (!rtip) {
	    std::printf("FAIL: adaptive sphere %s rt_i construction\n",
		test.name);
	    failures++;
	    continue;
	}
	rtip->rti_tol = case_tol;
	struct resource resource = {};
	rt_init_resource(&resource, 0, rtip);
	struct soltab *implicit_stp = prep_solid(rtip, &ell_intern, ID_ELL);
	ON_Brep *brep = ON_Brep::New();
	OBJ[ID_ELL].ft_brep(&brep, &ell_intern, &case_tol);
	struct rt_brep_internal brep_internal = {};
	brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
	brep_internal.brep = brep;
	struct rt_db_internal brep_intern;
	RT_DB_INTERNAL_INIT(&brep_intern);
	brep_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	brep_intern.idb_type = ID_BREP;
	brep_intern.idb_meth = &OBJ[ID_BREP];
	brep_intern.idb_ptr = &brep_internal;
	struct soltab *brep_stp = brep ? prep_solid(rtip, &brep_intern,
	    ID_BREP) : NULL;
	if (!implicit_stp || !brep_stp) {
	    std::printf("FAIL: adaptive sphere %s prep implicit/BREP=%d/%d\n",
		test.name, implicit_stp != NULL, brep_stp != NULL);
	    failures++;
	} else {
	    const double coordinate_scale = std::max(radius * test.scale,
		std::max(fabs(test.translation.x),
		std::max(fabs(test.translation.y),
		fabs(test.translation.z))));
	    const double normalized_limit = std::max(0.1 * tol->dist,
		8192.0 * DBL_EPSILON * coordinate_scale / test.scale);
	    for (size_t direction_index = 0; direction_index <
		    sizeof(directions) / sizeof(directions[0]);
		    ++direction_index) {
		ON_3dVector direction = directions[direction_index];
		ON_3dVector first = ON_CrossProduct(direction,
		    ON_3dVector(0.0, 0.0, 1.0));
		if (!direction.Unitize() || !first.Unitize()) {
		    failures++;
		    continue;
		}
		ON_3dVector second = ON_CrossProduct(direction, first);
		if (!second.Unitize()) {
		    failures++;
		    continue;
		}
		const ON_3dPoint closest = 0.31 * radius * first +
		    0.17 * radius * second;
		for (int reverse = 0; reverse <= 1; ++reverse) {
		    const ON_3dVector base_direction = reverse ?
			-direction : direction;
		    const ON_3dPoint base_origin = closest +
			(reverse ? 2.0 : -2.0) * radius * direction;
		    const ON_3dPoint transformed_origin =
			cobb_transform_point(xform, base_origin);
		    ON_3dVector transformed_direction =
			cobb_transform_vector(xform, base_direction);
		    if (!transformed_direction.Unitize()) {
			failures++;
			continue;
		    }
		    sampled_ray ray;
		    VSET(ray.origin, transformed_origin.x,
			transformed_origin.y, transformed_origin.z);
		    VSET(ray.direction, transformed_direction.x,
			transformed_direction.y, transformed_direction.z);
		    const ray_result implicit_result = shoot_solid(implicit_stp,
			rtip, &resource, ray.origin, ray.direction);
		    const ray_result production_result = shoot_solid(brep_stp,
			rtip, &resource, ray.origin, ray.direction);
		    const ray_result legacy_result = shoot_brep_legacy(brep_stp,
			rtip, &resource, ray.origin, ray.direction);
		    struct rt_brep_shot_trace trace;
		    (void)shoot_brep_trace(brep_stp, rtip, &resource, ray,
			trace);
		    total_rays++;
		    total_krawczyk += trace.surface_krawczyk_boxes;
		    total_boxes += trace.surface_subdivision_boxes;
		    total_clip_attempts += trace.surface_clip_attempts;
		    total_clip_contractions +=
			trace.surface_clip_contractions;
		    total_clip_inconclusive += trace.surface_clip_inconclusive;
		    total_clip_restriction_failures +=
			trace.surface_clip_restriction_failures;
		    maximum_clip_fraction_removed = std::max(
			maximum_clip_fraction_removed,
			(double)trace.surface_clip_max_fraction_removed);
		    minimum_boxes = std::min(minimum_boxes,
			trace.surface_subdivision_boxes);
		    maximum_boxes = std::max(maximum_boxes,
			trace.surface_subdivision_boxes);
		    if (trace.surface_krawczyk_boxes) {
			minimum_depth = std::min(minimum_depth,
			    trace.surface_krawczyk_min_depth);
			maximum_depth = std::max(maximum_depth,
			    trace.surface_krawczyk_max_depth);
		    }
		    const double implicit_error = std::max(
			fabs(implicit_result.in_dist / test.scale - expected_in),
			fabs(implicit_result.out_dist / test.scale - expected_out));
		    const double production_error = std::max(
			fabs(production_result.in_dist / test.scale - expected_in),
			fabs(production_result.out_dist / test.scale - expected_out));
		    const double legacy_error = std::max(
			fabs(legacy_result.in_dist / test.scale - expected_in),
			fabs(legacy_result.out_dist / test.scale - expected_out));
		    const double prepared_error =
			trace.local_event_stored_segments == 1 ? std::max(
			fabs(trace.local_event_segment_in[0] / test.scale -
			    expected_in),
			fabs(trace.local_event_segment_out[0] / test.scale -
			    expected_out)) : INFINITY;
		    maximum_implicit_error = std::max(maximum_implicit_error,
			implicit_error);
		    maximum_production_error = std::max(maximum_production_error,
			production_error);
		    maximum_legacy_error = std::max(maximum_legacy_error,
			legacy_error);
		    maximum_prepared_error = std::max(maximum_prepared_error,
			prepared_error);
		    const bool bad = implicit_result.segments != 1 ||
			production_result.segments != 1 ||
			legacy_result.segments != 1 ||
			trace.local_event_final_segments != 1 ||
			trace.local_event_stored_segments != 1 ||
			implicit_error > normalized_limit ||
			production_error > normalized_limit ||
			legacy_error > normalized_limit ||
			prepared_error > normalized_limit ||
			!brep_trace_fixed_workspaces_match(trace) ||
			trace.legacy_unique_roots != 2 ||
			trace.local_unique_roots != 2 ||
			trace.legacy_unique_roots_unmatched ||
			trace.local_unique_roots_unmatched ||
			trace.matched_root_events != 2 ||
			trace.root_event_mismatches ||
			trace.local_event_groups != 2 ||
			trace.local_event_contacts ||
			trace.local_event_clean_misses ||
			trace.local_event_hits != 2 ||
			trace.local_event_failures ||
			trace.local_event_overflow ||
			trace.local_event_final_mismatches ||
			trace.prepared_production_selected != 1 ||
			trace.prepared_production_fallback !=
			RT_BREP_PREPARED_FALLBACK_NONE ||
			trace.surface_krawczyk_boxes != 2 ||
			!trace.surface_clip_attempts ||
			!trace.surface_clip_contractions ||
			trace.surface_clip_restriction_failures ||
			trace.surface_clip_max_fraction_removed >
			0.5 + 64.0 * DBL_EPSILON ||
			trace.surface_subdivision_max_depth >= 24 ||
			trace.surface_workspace_exhausted ||
			trace.surface_box_overflow;
		    if (bad) {
			std::printf("FAIL: adaptive sphere similarity %s %zu/%d "
			    "segments=%d/%d/%d/%zu roots=%zu/%zu events=%zu/%zu "
			    "krawczyk=%zu depth=%zu boxes=%zu clip=%zu/%zu/%zu "
			    "errors=%.3g/%.3g/%.3g/%.3g limit=%.3g\n",
			    test.name, direction_index, reverse,
			    implicit_result.segments,
			    production_result.segments, legacy_result.segments,
			    trace.local_event_final_segments,
			    trace.legacy_unique_roots,
			    trace.local_unique_roots,
			    trace.matched_root_events,
			    trace.root_event_mismatches,
			    trace.surface_krawczyk_boxes,
			    trace.surface_subdivision_max_depth,
			    trace.surface_subdivision_boxes,
			    trace.surface_clip_contractions,
			    trace.surface_clip_attempts,
			    trace.surface_clip_restriction_failures,
			    implicit_error, production_error, legacy_error,
			    prepared_error,
			    normalized_limit);
			failures++;
		    }
		}
	    }
	}

	free_solid(brep_stp);
	free_solid(implicit_stp);
	rt_clean_resource_basic(rtip, &resource);
	BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
	rt_i_destroy(rtip);
    }

    /* The immediately preceding no-clipping similarity gate visited 1,602
     * boxes for these same 30 rays.  Keep this broad threshold as a direct
     * work-reduction ratchet for the strictly qualified production path. */
    if (total_rays == 30 && (total_boxes >= 1602 ||
	    !total_clip_contractions || total_clip_restriction_failures ||
	    maximum_clip_fraction_removed > 0.5 + 64.0 * DBL_EPSILON)) {
	std::printf("FAIL: adaptive sphere clipping work "
	    "boxes=%zu/1602 clip=%zu/%zu/%zu+%zu/%.3g\n", total_boxes,
	    total_clip_contractions, total_clip_attempts,
	    total_clip_inconclusive, total_clip_restriction_failures,
	    maximum_clip_fraction_removed);
	failures++;
    }

    if (!failures) {
	std::printf("Sphere adaptive similarity invariance: PASS "
	    "rays=%zu krawczyk=%zu depth=%zu/%zu boxes=%zu/%zu/%zu "
	    "clip=%zu/%zu/%zu+%zu/%.3g "
	    "max-errors=%.3g/%.3g/%.3g/%.3g\n",
	    total_rays,
	    total_krawczyk, minimum_depth, maximum_depth, minimum_boxes,
	    total_boxes, maximum_boxes, total_clip_contractions,
	    total_clip_attempts, total_clip_inconclusive,
	    total_clip_restriction_failures, maximum_clip_fraction_removed,
	    maximum_implicit_error,
	    maximum_production_error, maximum_legacy_error,
	    maximum_prepared_error);
    }
    return failures;
}


static int
check_ellipsoid_adaptive_affine(const struct bn_tol *tol)
{
    const double radius = 10.0;
    const ON_3dPoint base_center(0.0, 0.0, 0.0);
    struct affine_case {
	const char *name;
	ON_3dVector scale;
	ON_3dVector translation;
	ON_3dVector axis;
	double angle;
	double grazing_floor;
	int expected_fallback[3];
    } cases[] = {
	{"oblate", ON_3dVector(1.0, 1.0, 0.6),
	    ON_3dVector(-31.25, 47.5, 103.75),
	    ON_3dVector(1.0, -2.0, 0.5), 0.731, 0.0,
	    {RT_BREP_PREPARED_FALLBACK_NONE,
	     RT_BREP_PREPARED_FALLBACK_NONE,
	     RT_BREP_PREPARED_FALLBACK_NONE}},
	{"triaxial", ON_3dVector(0.3, 1.7, 4.0),
	    ON_3dVector(-19.0, 23.0, 41.0),
	    ON_3dVector(-0.3, 1.0, 0.7), -1.113, 1.0e-6,
	    {RT_BREP_PREPARED_FALLBACK_NONE,
	     RT_BREP_PREPARED_FALLBACK_NONE,
	     RT_BREP_PREPARED_FALLBACK_NONE}},
	{"small-triaxial", ON_3dVector(0.005, 0.025, 0.1),
	    ON_3dVector(1.25, -2.5, 5.0),
	    ON_3dVector(0.2, -0.7, 1.0), 1.337, 0.0,
	    {RT_BREP_PREPARED_FALLBACK_NONE,
	     RT_BREP_PREPARED_FALLBACK_NONE,
	     RT_BREP_PREPARED_FALLBACK_NONE}},
	{"high-condition", ON_3dVector(0.01, 0.2, 1.0),
	    ON_3dVector(-203.0, 307.0, 509.0),
	    ON_3dVector(1.0, 0.4, -0.2), -0.917, 1.0e-6,
	    {RT_BREP_PREPARED_FALLBACK_NONE,
	     RT_BREP_PREPARED_FALLBACK_NONE,
	     RT_BREP_PREPARED_FALLBACK_UNCERTIFIED}},
	{"extreme-condition", ON_3dVector(0.001, 0.05, 1.0),
	    ON_3dVector(17.0, -29.0, 43.0),
	    ON_3dVector(-0.6, 0.3, 1.0), 0.583, 1.0e-4,
	    {RT_BREP_PREPARED_FALLBACK_SURFACE_BOXES,
	     RT_BREP_PREPARED_FALLBACK_UNCERTIFIED,
	     RT_BREP_PREPARED_FALLBACK_SURFACE_BOXES}},
	{"large-triaxial", ON_3dVector(100.0, 600.0, 2500.0),
	    ON_3dVector(1.0e6, -2.0e6, 3.0e6),
	    ON_3dVector(2.0, 0.25, -1.0), 2.017, 0.0,
	    {RT_BREP_PREPARED_FALLBACK_NONE,
	     RT_BREP_PREPARED_FALLBACK_NONE,
	     RT_BREP_PREPARED_FALLBACK_NONE}}
    };
    const ON_3dVector directions[] = {
	ON_3dVector(0.371, 0.529, 0.763),
	ON_3dVector(-0.613, 0.247, 0.751),
	ON_3dVector(0.193, -0.881, 0.432)
    };
    const double half_chord = radius * sqrt(1.0 - 0.31 * 0.31 -
	0.17 * 0.17);
    const double expected_in = 2.0 * radius - half_chord;
    const double expected_out = 2.0 * radius + half_chord;
    const double grazing_clearance_ratios[] = {
	100.0, 1.0, 0.01, 1.0e-4, 1.0e-6, 1.0e-8, 0.0,
	-1.0e-8, -1.0e-6, -1.0e-4, -0.01, -1.0, -100.0
    };
    struct affine_grazing_summary {
	size_t rays = 0;
	size_t implicit_segments = 0;
	size_t production_segments = 0;
	size_t selected = 0;
	size_t fallback[RT_BREP_PREPARED_FALLBACK_COUNT] = {};
	double minimum_chord_ratio = DBL_MAX;
	double maximum_chord_ratio = 0.0;
	double maximum_endpoint_error = 0.0;
    } grazing[sizeof(grazing_clearance_ratios) /
	sizeof(grazing_clearance_ratios[0])];
    int failures = 0;
    size_t total_rays = 0;
    size_t total_krawczyk = 0;
    size_t total_boxes = 0;
    size_t minimum_boxes = SIZE_MAX;
    size_t maximum_boxes = 0;
    size_t minimum_depth = SIZE_MAX;
    size_t maximum_depth = 0;
    size_t selected_rays = 0;
    size_t surface_box_fallbacks = 0;
    size_t uncertified_fallbacks = 0;
    size_t expected_selected_rays = 0;
    size_t expected_surface_box_fallbacks = 0;
    size_t expected_uncertified_fallbacks = 0;
    double minimum_condition = DBL_MAX;
    double maximum_condition = 0.0;
    double maximum_implicit_error = 0.0;
    double maximum_production_error = 0.0;
    double maximum_legacy_error = 0.0;
    double maximum_prepared_error = 0.0;
    size_t grazing_rays = 0;
    size_t grazing_resolved_misses = 0;

    const auto check_affine_grazing_case = [&](const affine_case &test,
	    const ON_Xform &xform,
	    const struct bn_tol &case_tol, double coordinate_scale,
	    struct rt_i *rtip, struct resource *case_resource,
	    struct soltab *implicit_stp, struct soltab *brep_stp) {
	if (!(test.grazing_floor > 0.0))
	    return;
	ON_3dVector grazing_direction = directions[0];
	ON_3dVector grazing_normal = ON_CrossProduct(grazing_direction,
	    ON_3dVector(0.0, 0.0, 1.0));
	if (!grazing_direction.Unitize() || !grazing_normal.Unitize()) {
	    std::printf("FAIL: adaptive ellipsoid %s grazing frame\n",
		test.name);
	    failures++;
	    return;
	}
	const double required_clearance_ratio = test.grazing_floor;
	bool brep_interval_ended[2] = {false, false};

	for (size_t ratio_index = 0; ratio_index <
		sizeof(grazing_clearance_ratios) /
		sizeof(grazing_clearance_ratios[0]); ++ratio_index) {
	    const double clearance =
		grazing_clearance_ratios[ratio_index] * tol->dist;
	    const double closest_radius = radius - clearance;
	    const double half_squared = radius * radius -
		closest_radius * closest_radius;
	    const double grazing_half_chord = half_squared > 0.0 ?
		sqrt(half_squared) : 0.0;
	    const ON_3dPoint closest = closest_radius * grazing_normal;
	    affine_grazing_summary &summary = grazing[ratio_index];
	    int forward_production_segments = -1;
	    for (int reverse = 0; reverse <= 1; ++reverse) {
		const ON_3dVector base_direction = reverse ?
		    -grazing_direction : grazing_direction;
		const ON_3dPoint base_origin = closest +
		    (reverse ? 2.0 : -2.0) * radius * grazing_direction;
		const ON_3dPoint transformed_origin =
		    cobb_transform_point(xform, base_origin);
		ON_3dVector transformed_direction =
		    cobb_transform_vector(xform, base_direction);
		const double direction_length = transformed_direction.Length();
		if (!(direction_length > DBL_MIN) ||
			!std::isfinite(direction_length) ||
			!transformed_direction.Unitize()) {
		    failures++;
		    continue;
		}
		sampled_ray ray;
		VSET(ray.origin, transformed_origin.x, transformed_origin.y,
		    transformed_origin.z);
		VSET(ray.direction, transformed_direction.x,
		    transformed_direction.y, transformed_direction.z);
		const ray_result implicit_result = shoot_solid(implicit_stp,
		    rtip, case_resource, ray.origin, ray.direction);
		const ray_result production_result = shoot_solid(brep_stp,
		    rtip, case_resource, ray.origin, ray.direction);
		const ray_result legacy_result = shoot_brep_legacy(brep_stp,
		    rtip, case_resource, ray.origin, ray.direction);
		struct rt_brep_shot_trace trace;
		(void)shoot_brep_trace(brep_stp, rtip, case_resource, ray,
		    trace);
		grazing_rays++;
		summary.rays++;
		summary.implicit_segments += implicit_result.segments;
		summary.production_segments += production_result.segments;
		summary.selected += trace.prepared_production_selected;
		if (trace.prepared_production_fallback >= 0 &&
			trace.prepared_production_fallback <
			RT_BREP_PREPARED_FALLBACK_COUNT)
		    summary.fallback[trace.prepared_production_fallback]++;
		const double chord_ratio = case_tol.dist > 0.0 ?
		    2.0 * grazing_half_chord * direction_length /
		    case_tol.dist : 0.0;
		summary.minimum_chord_ratio = std::min(
		    summary.minimum_chord_ratio, chord_ratio);
		summary.maximum_chord_ratio = std::max(
		    summary.maximum_chord_ratio, chord_ratio);
		const double normalized_limit = std::max(
		    case_tol.dist / direction_length,
		    8192.0 * DBL_EPSILON * coordinate_scale /
		    direction_length);
		const double grazing_in =
		    2.0 * radius - grazing_half_chord;
		const double grazing_out =
		    2.0 * radius + grazing_half_chord;
		const double implicit_error = implicit_result.segments == 1 ?
		    std::max(fabs(implicit_result.in_dist / direction_length -
			grazing_in),
		    fabs(implicit_result.out_dist / direction_length -
			grazing_out)) : 0.0;
		const double production_error = production_result.segments == 1 ?
		    std::max(fabs(production_result.in_dist / direction_length -
			grazing_in),
		    fabs(production_result.out_dist / direction_length -
			grazing_out)) : 0.0;
		const double legacy_error = legacy_result.segments == 1 ?
		    std::max(fabs(legacy_result.in_dist / direction_length -
			grazing_in),
		    fabs(legacy_result.out_dist / direction_length -
			grazing_out)) : 0.0;
		const double prepared_error =
		    trace.local_event_stored_segments == 1 ?
		    std::max(fabs(trace.local_event_segment_in[0] /
			direction_length - grazing_in),
		    fabs(trace.local_event_segment_out[0] /
			direction_length - grazing_out)) : 0.0;
		summary.maximum_endpoint_error = std::max(
		    summary.maximum_endpoint_error, production_error);
		const bool required_hit = clearance > 0.0 &&
		    grazing_clearance_ratios[ratio_index] >=
		    required_clearance_ratio;
		const bool analytic_resolved = clearance > 0.0 &&
		    2.0 * grazing_half_chord * direction_length >=
		    case_tol.dist;
		const bool exact_contact = !(clearance < 0.0) &&
		    !(clearance > 0.0);
		const size_t production_segments =
		    (size_t)production_result.segments;
		const bool restarted = production_segments &&
		    brep_interval_ended[reverse];
		if (!production_segments)
		    brep_interval_ended[reverse] = true;
		if (analytic_resolved && !production_segments)
		    grazing_resolved_misses++;
		const bool reversal_mismatch = reverse &&
		    production_result.segments != forward_production_segments;
		if (!reverse)
		    forward_production_segments = production_result.segments;
		const bool selected_partition_matches =
		    !trace.prepared_production_selected ||
		    (trace.local_event_final_segments == production_segments &&
		     trace.local_event_stored_segments == production_segments &&
		     (!production_segments ||
		      prepared_error <= normalized_limit));
		const bool bad = production_result.segments !=
			legacy_result.segments ||
		    trace.final_segments != production_segments ||
		    (clearance > 0.0 && implicit_result.segments != 1) ||
		    (clearance < 0.0 && implicit_result.segments != 0) ||
		    (exact_contact && implicit_result.segments > 1) ||
		    (required_hit && production_result.segments != 1) ||
		    (clearance <= 0.0 && production_result.segments != 0) ||
		    production_result.segments > 1 ||
		    restarted ||
		    reversal_mismatch ||
		    (clearance > 0.0 && implicit_error > normalized_limit) ||
		    production_error > normalized_limit ||
		    legacy_error > normalized_limit ||
		    !selected_partition_matches ||
		    !brep_trace_fixed_workspaces_match(trace, true) ||
		    trace.surface_workspace_exhausted ||
		    trace.surface_clip_restriction_failures;
		if (bad) {
		    std::printf("FAIL: affine grazing %s ratio=%.3g "
			"reverse=%d chord/T=%.3g "
			"segments=%d/%d/%d/%zu required/trend=%d/%d "
			"selection=%zu/%d "
			"leaves=%zu/%zu boxes=%zu/%zu "
			"errors=%.3g/%.3g/%.3g/%.3g limit=%.3g\n",
			test.name, grazing_clearance_ratios[ratio_index],
			reverse, chord_ratio, implicit_result.segments,
			production_result.segments, legacy_result.segments,
			trace.final_segments, required_hit,
			restarted || reversal_mismatch,
			trace.prepared_production_selected,
			trace.prepared_production_fallback,
			trace.fixed_leaf_count, trace.fixed_leaf_overflow,
			trace.surface_subdivision_boxes,
			trace.surface_box_overflow, implicit_error,
			production_error, legacy_error, prepared_error,
			normalized_limit);
		    std::printf("  solver calls=%zu status=%zu/%zu/%zu/%zu/"
			"%zu/%zu/%zu/%zu/%zu/%zu/%zu roots=%zu/%zu "
			"local=%zu/%zu/%zu/%zu isolated=%zu/%zu\n",
			trace.solver_calls,
			trace.solver_status[0], trace.solver_status[1],
			trace.solver_status[2], trace.solver_status[3],
			trace.solver_status[4], trace.solver_status[5],
			trace.solver_status[6], trace.solver_status[7],
			trace.solver_status[8], trace.solver_status[9],
			trace.solver_status[10], trace.candidate_roots,
			trace.stored_roots, trace.local_root_attempts,
			trace.local_root_candidates,
			trace.stored_local_roots, trace.local_root_failures,
			trace.surface_isolated_boxes,
			trace.surface_krawczyk_boxes);
		    failures++;
		}
	    }
	}
    };

    for (size_t case_index = 0;
	    case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
	const affine_case &test = cases[case_index];
	const double minimum_scale = std::min(test.scale.x,
	    std::min(test.scale.y, test.scale.z));
	const double maximum_scale = std::max(test.scale.x,
	    std::max(test.scale.y, test.scale.z));
	const double condition = maximum_scale / minimum_scale;
	minimum_condition = std::min(minimum_condition, condition);
	maximum_condition = std::max(maximum_condition, condition);
	const ON_Xform xform = cobb_axis_angle_affine_transform(test.scale,
	    test.translation, test.axis, test.angle);
	const ON_3dPoint center = cobb_transform_point(xform, base_center);
	const ON_3dVector a = cobb_transform_vector(xform,
	    ON_3dVector(radius, 0.0, 0.0));
	const ON_3dVector b = cobb_transform_vector(xform,
	    ON_3dVector(0.0, radius, 0.0));
	const ON_3dVector c = cobb_transform_vector(xform,
	    ON_3dVector(0.0, 0.0, radius));
	struct rt_ell_internal ell = {};
	ell.magic = RT_ELL_INTERNAL_MAGIC;
	VSET(ell.v, center.x, center.y, center.z);
	VSET(ell.a, a.x, a.y, a.z);
	VSET(ell.b, b.x, b.y, b.z);
	VSET(ell.c, c.x, c.y, c.z);
	struct rt_db_internal ell_intern;
	RT_DB_INTERNAL_INIT(&ell_intern);
	ell_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ell_intern.idb_type = ID_ELL;
	ell_intern.idb_meth = &OBJ[ID_ELL];
	ell_intern.idb_ptr = &ell;

	/* A general affine map has no single length scale.  Scale the model
	 * tolerance with the smallest singular value so it remains conservative
	 * for the thinnest ellipsoid direction. */
	struct bn_tol case_tol = *tol;
	case_tol.dist = tol->dist * minimum_scale;
	case_tol.dist_sq = case_tol.dist * case_tol.dist;
	struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
	if (!rtip) {
	    std::printf("FAIL: adaptive ellipsoid %s rt_i construction\n",
		test.name);
	    failures++;
	    continue;
	}
	rtip->rti_tol = case_tol;
	struct resource resource = {};
	rt_init_resource(&resource, 0, rtip);
	struct soltab *implicit_stp = prep_solid(rtip, &ell_intern, ID_ELL);
	ON_Brep *brep = ON_Brep::New();
	OBJ[ID_ELL].ft_brep(&brep, &ell_intern, &case_tol);
	struct rt_brep_internal brep_internal = {};
	brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
	brep_internal.brep = brep;
	struct rt_db_internal brep_intern;
	RT_DB_INTERNAL_INIT(&brep_intern);
	brep_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	brep_intern.idb_type = ID_BREP;
	brep_intern.idb_meth = &OBJ[ID_BREP];
	brep_intern.idb_ptr = &brep_internal;
	struct soltab *brep_stp = brep ? prep_solid(rtip, &brep_intern,
	    ID_BREP) : NULL;
	if (!implicit_stp || !brep_stp) {
	    std::printf("FAIL: adaptive ellipsoid %s prep "
		"implicit/BREP=%d/%d\n", test.name, implicit_stp != NULL,
		brep_stp != NULL);
	    failures++;
	} else {
	    const double coordinate_scale = std::max(radius * maximum_scale,
		std::max(fabs(test.translation.x),
		std::max(fabs(test.translation.y),
		fabs(test.translation.z))));
	    for (size_t direction_index = 0; direction_index <
		    sizeof(directions) / sizeof(directions[0]);
		    ++direction_index) {
		ON_3dVector direction = directions[direction_index];
		ON_3dVector first = ON_CrossProduct(direction,
		    ON_3dVector(0.0, 0.0, 1.0));
		if (!direction.Unitize() || !first.Unitize()) {
		    failures++;
		    continue;
		}
		ON_3dVector second = ON_CrossProduct(direction, first);
		if (!second.Unitize()) {
		    failures++;
		    continue;
		}
		const ON_3dPoint closest = 0.31 * radius * first +
		    0.17 * radius * second;
		for (int reverse = 0; reverse <= 1; ++reverse) {
		    const ON_3dVector base_direction = reverse ?
			-direction : direction;
		    const ON_3dPoint base_origin = closest +
			(reverse ? 2.0 : -2.0) * radius * direction;
		    const ON_3dPoint transformed_origin =
			cobb_transform_point(xform, base_origin);
		    ON_3dVector transformed_direction =
			cobb_transform_vector(xform, base_direction);
		    const double direction_scale =
			transformed_direction.Length();
		    if (!(direction_scale > DBL_MIN) ||
			    !std::isfinite(direction_scale) ||
			    !transformed_direction.Unitize()) {
			failures++;
			continue;
		    }
		    sampled_ray ray;
		    VSET(ray.origin, transformed_origin.x,
			transformed_origin.y, transformed_origin.z);
		    VSET(ray.direction, transformed_direction.x,
			transformed_direction.y, transformed_direction.z);
		    const ray_result implicit_result = shoot_solid(implicit_stp,
			rtip, &resource, ray.origin, ray.direction);
		    const ray_result production_result = shoot_solid(brep_stp,
			rtip, &resource, ray.origin, ray.direction);
		    const ray_result legacy_result = shoot_brep_legacy(brep_stp,
			rtip, &resource, ray.origin, ray.direction);
		    struct rt_brep_shot_trace trace;
		    (void)shoot_brep_trace(brep_stp, rtip, &resource, ray,
			trace);
		    total_rays++;
		    total_krawczyk += trace.surface_krawczyk_boxes;
		    total_boxes += trace.surface_subdivision_boxes;
		    minimum_boxes = std::min(minimum_boxes,
			trace.surface_subdivision_boxes);
		    maximum_boxes = std::max(maximum_boxes,
			trace.surface_subdivision_boxes);
		    if (trace.surface_krawczyk_boxes) {
			minimum_depth = std::min(minimum_depth,
			    trace.surface_krawczyk_min_depth);
			maximum_depth = std::max(maximum_depth,
			    trace.surface_krawczyk_max_depth);
		    }
		    const double normalized_limit = std::max(
			0.1 * case_tol.dist / direction_scale,
			8192.0 * DBL_EPSILON * coordinate_scale /
			direction_scale);
		    const double implicit_error = std::max(
			fabs(implicit_result.in_dist / direction_scale -
			    expected_in),
			fabs(implicit_result.out_dist / direction_scale -
			    expected_out));
		    const double production_error = std::max(
			fabs(production_result.in_dist / direction_scale -
			    expected_in),
			fabs(production_result.out_dist / direction_scale -
			    expected_out));
		    const double legacy_error = std::max(
			fabs(legacy_result.in_dist / direction_scale -
			    expected_in),
			fabs(legacy_result.out_dist / direction_scale -
			    expected_out));
		    const double prepared_error =
			trace.local_event_stored_segments == 1 ? std::max(
			fabs(trace.local_event_segment_in[0] / direction_scale -
			    expected_in),
			fabs(trace.local_event_segment_out[0] / direction_scale -
			    expected_out)) : INFINITY;
		    maximum_implicit_error = std::max(maximum_implicit_error,
			implicit_error);
		    maximum_production_error = std::max(
			maximum_production_error, production_error);
		    maximum_legacy_error = std::max(maximum_legacy_error,
			legacy_error);
		    const int expected_fallback =
			test.expected_fallback[direction_index];
		    const bool expected_selected = expected_fallback ==
			RT_BREP_PREPARED_FALLBACK_NONE;
		    if (expected_selected)
			expected_selected_rays++;
		    else if (expected_fallback ==
			    RT_BREP_PREPARED_FALLBACK_SURFACE_BOXES)
			expected_surface_box_fallbacks++;
		    else if (expected_fallback ==
			    RT_BREP_PREPARED_FALLBACK_UNCERTIFIED)
			expected_uncertified_fallbacks++;
		    if (trace.prepared_production_selected) {
			selected_rays++;
			maximum_prepared_error = std::max(
			    maximum_prepared_error, prepared_error);
		} else if (trace.prepared_production_fallback ==
			RT_BREP_PREPARED_FALLBACK_SURFACE_BOXES) {
			surface_box_fallbacks++;
		} else if (trace.prepared_production_fallback ==
			RT_BREP_PREPARED_FALLBACK_UNCERTIFIED) {
			uncertified_fallbacks++;
		}
		    const bool bad = implicit_result.segments != 1 ||
			production_result.segments != 1 ||
			legacy_result.segments != 1 ||
			trace.final_segments != 1 ||
			implicit_error > normalized_limit ||
			production_error > normalized_limit ||
			legacy_error > normalized_limit ||
			!brep_trace_fixed_workspaces_match(trace,
			    !expected_selected) ||
			trace.legacy_unique_roots != 2 ||
			trace.prepared_production_selected !=
			    (expected_selected ? 1 : 0) ||
			trace.prepared_production_fallback !=
			    expected_fallback ||
			trace.surface_workspace_exhausted ||
			trace.surface_clip_restriction_failures ||
			(!expected_selected &&
			 (fabs(production_result.in_dist -
			    legacy_result.in_dist) / direction_scale >
			    normalized_limit ||
			  fabs(production_result.out_dist -
			    legacy_result.out_dist) / direction_scale >
			    normalized_limit)) ||
			(expected_selected &&
			 (trace.local_event_final_segments != 1 ||
			  trace.local_event_stored_segments != 1 ||
			  prepared_error > normalized_limit ||
			  trace.local_unique_roots != 2 ||
			  trace.legacy_unique_roots_unmatched ||
			  trace.local_unique_roots_unmatched ||
			  trace.matched_root_events != 2 ||
			  trace.root_event_mismatches ||
			  trace.local_event_groups != 2 ||
			  trace.local_event_contacts ||
			  trace.local_event_clean_misses ||
			  trace.local_event_hits != 2 ||
			  trace.local_event_final_mismatches ||
			  trace.surface_krawczyk_boxes != 2 ||
			  trace.surface_subdivision_max_depth >= 24 ||
			  trace.surface_box_overflow));
		    if (bad) {
			std::printf("FAIL: adaptive ellipsoid affine %s %zu/%d "
			    "segments=%d/%d/%d/%zu roots=%zu/%zu "
			    "krawczyk=%zu depth=%zu boxes=%zu "
			    "selection=%zu/%d/%d fixed=%d leaves=%zu/%zu/%zu "
			    "hits=%zu/%zu/%zu errors=%.3g/%.3g/%.3g/%.3g "
			    "limit=%.3g\n", test.name, direction_index, reverse,
			    implicit_result.segments,
			    production_result.segments, legacy_result.segments,
			    trace.local_event_final_segments,
			    trace.legacy_unique_roots,
			    trace.local_unique_roots,
			    trace.surface_krawczyk_boxes,
			    trace.surface_subdivision_max_depth,
			    trace.surface_subdivision_boxes,
			    trace.prepared_production_selected,
			    trace.prepared_production_fallback,
			    expected_fallback,
			    brep_trace_fixed_workspaces_match(trace,
				!expected_selected),
			    trace.fixed_leaf_count, trace.fixed_leaf_stored,
			    trace.fixed_leaf_overflow, trace.fixed_hit_count,
			    trace.fixed_hit_stored, trace.fixed_hit_overflow,
			    implicit_error, production_error, legacy_error,
			    prepared_error, normalized_limit);
			failures++;
		    }
		}
	    }
	    check_affine_grazing_case(test, xform, case_tol,
		coordinate_scale, rtip, &resource, implicit_stp, brep_stp);
	}

	free_solid(brep_stp);
	free_solid(implicit_stp);
	rt_clean_resource_basic(rtip, &resource);
	BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
	rt_i_destroy(rtip);
    }

    if (selected_rays != expected_selected_rays ||
	    surface_box_fallbacks != expected_surface_box_fallbacks ||
	    uncertified_fallbacks != expected_uncertified_fallbacks) {
	std::printf("FAIL: ellipsoid affine fallback totals "
	    "selected=%zu/%zu boxes=%zu/%zu uncertified=%zu/%zu\n",
	    selected_rays, expected_selected_rays, surface_box_fallbacks,
	    expected_surface_box_fallbacks, uncertified_fallbacks,
	    expected_uncertified_fallbacks);
	failures++;
    }

    const size_t expected_grazing_rays =
	3 * 2 * sizeof(grazing_clearance_ratios) /
	sizeof(grazing_clearance_ratios[0]);
    if (grazing_rays != expected_grazing_rays) {
	std::printf("FAIL: ellipsoid affine grazing rays=%zu/%zu\n",
	    grazing_rays, expected_grazing_rays);
	failures++;
    }
    /* The capability floor is enforced per affine case above.  Keep the
     * aggregate gap monotone while allowing later solver work to reduce it. */
    if (grazing_resolved_misses > 6) {
	std::printf("FAIL: ellipsoid affine resolved grazing misses=%zu/6\n",
	    grazing_resolved_misses);
	failures++;
    }
    for (size_t ratio_index = 0; ratio_index <
	    sizeof(grazing_clearance_ratios) /
	    sizeof(grazing_clearance_ratios[0]); ++ratio_index) {
	const affine_grazing_summary &summary = grazing[ratio_index];
	size_t fallback_total = 0;
	for (size_t fallback = 0;
		fallback < RT_BREP_PREPARED_FALLBACK_COUNT; ++fallback)
	    fallback_total += summary.fallback[fallback];
	if (summary.rays != 6 || fallback_total != summary.rays) {
	    std::printf("FAIL: ellipsoid affine grazing ratio=%.3g "
		"rays/fallback=%zu/%zu\n",
		grazing_clearance_ratios[ratio_index], summary.rays,
		fallback_total);
	    failures++;
	}
	std::printf("Ellipsoid affine grazing ratio=% .3g rays=%zu "
	    "segments=%zu/%zu selected=%zu "
	    "fallback=%zu/%zu/%zu/%zu chord/T=%.3g/%.3g "
	    "max-error=%.3g\n", grazing_clearance_ratios[ratio_index],
	    summary.rays, summary.implicit_segments,
	    summary.production_segments, summary.selected,
	    summary.fallback[RT_BREP_PREPARED_FALLBACK_NONE],
	    summary.fallback[RT_BREP_PREPARED_FALLBACK_SURFACE_BOXES],
	    summary.fallback[RT_BREP_PREPARED_FALLBACK_UNCERTIFIED],
	    summary.fallback[RT_BREP_PREPARED_FALLBACK_PARTITION],
	    summary.minimum_chord_ratio, summary.maximum_chord_ratio,
	    summary.maximum_endpoint_error);
    }

    if (!failures) {
	std::printf("Ellipsoid ordinary affine invariance: PASS "
	    "rays=%zu selected=%zu fallback=%zu/%zu "
	    "condition=%.3g/%.3g krawczyk=%zu "
	    "cert-depth=%zu/%zu boxes=%zu/%zu/%zu "
	    "max-errors=%.3g/%.3g/%.3g/%.3g\n", total_rays,
	    selected_rays, surface_box_fallbacks, uncertified_fallbacks,
	    minimum_condition, maximum_condition, total_krawczyk,
	    minimum_depth, maximum_depth, minimum_boxes, total_boxes,
	    maximum_boxes, maximum_implicit_error, maximum_production_error,
	    maximum_legacy_error, maximum_prepared_error);
	std::printf("Ellipsoid affine grazing ratchet: PASS rays=%zu "
	    "resolved-gaps=%zu floors=1e-6/1e-6/1e-4\n",
	    grazing_rays, grazing_resolved_misses);
    }
    return failures;
}


static bool
cobb_reparameterize_edge_trims(ON_Brep *brep, int edge_index,
    double &maximum_locus_error, double &minimum_midpoint_shift)
{
    maximum_locus_error = 0.0;
    minimum_midpoint_shift = DBL_MAX;
    double coordinate_scale = 1.0;
    if (!brep || edge_index < 0 || edge_index >= brep->m_E.Count())
	return false;
    const ON_BrepEdge &edge = brep->m_E[edge_index];
    if (edge.m_ti.Count() != 2)
	return false;

    const double constants[2] = {0.05, 20.0};
    for (int side = 0; side < 2; ++side) {
	const int trim_index = edge.m_ti[side];
	if (trim_index < 0 || trim_index >= brep->m_T.Count())
	    return false;
	ON_BrepTrim &trim = brep->m_T[trim_index];
	ON_NurbsCurve *curve = brep->MakeTrimCurveNurb(trim);
	if (!curve || !curve->Domain().IsIncreasing())
	    return false;
	const ON_NurbsCurve original(*curve);
	const ON_Interval domain = curve->Domain();
	const ON_3dPoint original_midpoint = original.PointAt(domain.Mid());
	if (!curve->Reparameterize(constants[side]))
	    return false;
	for (int sample = 0; sample <= 64; ++sample) {
	    const double fraction = (double)sample / 64.0;
	    /* Reparameterize maps every old knot k to
	     * c*k/((c-1)*k+1).  Invert that map to find the original
	     * parameter evaluated at this new parameter. */
	    const double denominator = constants[side] +
		(1.0 - constants[side]) * fraction;
	    const double mapped_fraction = fraction / denominator;
	    const ON_3dPoint expected = original.PointAt(
		domain.ParameterAt(mapped_fraction));
	    const ON_3dPoint actual = curve->PointAt(
		domain.ParameterAt(fraction));
	    if (!expected.IsValid() || !actual.IsValid())
		return false;
	    coordinate_scale = std::max(coordinate_scale,
		std::max(fabs(expected.x), std::max(fabs(expected.y),
		std::max(fabs(actual.x), fabs(actual.y)))));
	    maximum_locus_error = std::max(maximum_locus_error,
		expected.DistanceTo(actual));
	}
	minimum_midpoint_shift = std::min(minimum_midpoint_shift,
	    original_midpoint.DistanceTo(curve->PointAt(domain.Mid())));
	curve->DestroyRuntimeCache(true);
	/* Reparameterize() can move an endpoint knot by roundoff.  Refresh the
	 * proxy's cached real-curve domain so the unchanged full locus remains a
	 * valid trim representation. */
	trim.SetProxyCurve(curve);
    }
    brep->DestroyRuntimeCache(true);
    const double locus_limit = 4096.0 * DBL_EPSILON * coordinate_scale;
    return std::isfinite(maximum_locus_error) &&
	std::isfinite(minimum_midpoint_shift) &&
	maximum_locus_error <= locus_limit &&
	minimum_midpoint_shift > 0.1 && brep->IsValid();
}


static bool
cobb_make_ambiguous_edge_trim(ON_Brep *brep, int edge_index)
{
    if (!brep || edge_index < 0 || edge_index >= brep->m_E.Count())
	return false;
    const ON_BrepEdge &edge = brep->m_E[edge_index];
    if (edge.m_ti.Count() != 2)
	return false;
    const int trim_index = edge.m_ti[0];
    if (trim_index < 0 || trim_index >= brep->m_T.Count())
	return false;
    ON_BrepTrim &trim = brep->m_T[trim_index];
    const ON_Curve *original = trim.TrimCurveOf();
    if (!original || !original->Domain().IsIncreasing())
	return false;
    const ON_Interval domain = original->Domain();
    const ON_3dPoint start = original->PointAt(domain.Min());
    const ON_3dPoint end = original->PointAt(domain.Max());
    if (!start.IsValid() || !end.IsValid() ||
	    start.DistanceTo(end) <= DBL_MIN)
	return false;

    /* Traverse the same boundary forward, backward, then forward.  The locus
     * stays on the valid UV boundary, but its edge correspondence is not
     * one-to-one and the middle span has the wrong orientation. */
    ON_NurbsCurve *ambiguous = ON_NurbsCurve::New(2, false, 2, 4);
    if (!ambiguous || !ambiguous->SetCV(0, start) ||
	    !ambiguous->SetCV(1, end) || !ambiguous->SetCV(2, start) ||
	    !ambiguous->SetCV(3, end) ||
	    !ambiguous->MakeClampedUniformKnotVector() ||
	    !ambiguous->SetDomain(domain.Min(), domain.Max())) {
	delete ambiguous;
	return false;
    }
    const int curve_index = brep->AddTrimCurve(ambiguous);
    if (curve_index < 0) {
	delete ambiguous;
	return false;
    }
    if (!brep->SetTrimCurve(trim, curve_index))
	return false;
    brep->SetTrimIsoFlags(trim);
    brep->DestroyRuntimeCache(true);
    return true;
}


static int
check_cobb_classifier_invariance(const struct bn_tol *tol)
{
    const double radius = 10.0;
    const ON_3dPoint origin(0.0, 0.0, 0.0);
    ON_Brep *pristine = ON_Brep_CobbSphereSewn(radius, origin);
    cobb_seam_frame frame;
    double measured_gap = 0.0;
    double displacement = 0.0;
    ON_Brep *base = cobb_bowed_seam_variant(pristine, origin, -tol->dist,
	frame, measured_gap, displacement);
    delete pristine;
    if (!base) {
	std::printf("FAIL: Cobb classifier-invariance construction\n");
	return 1;
    }

    enum rotation_kind {
	NO_ROTATION,
	CYCLIC_ROTATION,
	AXIS_ANGLE_ROTATION
    };
    struct transform_case {
	const char *name;
	double scale;
	ON_3dVector translation;
	rotation_kind rotation;
	ON_3dVector axis;
	double angle;
	bool reparameterize;
    } cases[] = {
	{"identity", 1.0, ON_3dVector(0.0, 0.0, 0.0), NO_ROTATION,
	    ON_3dVector(0.0, 0.0, 0.0), 0.0, false},
	{"trim-reparameterized", 1.0, ON_3dVector(0.0, 0.0, 0.0),
	    NO_ROTATION, ON_3dVector(0.0, 0.0, 0.0), 0.0, true},
	{"translated", 1.0, ON_3dVector(-31.25, 47.5, 103.75),
	    NO_ROTATION, ON_3dVector(0.0, 0.0, 0.0), 0.0, false},
	{"cyclic-rotated-translated", 1.0,
	    ON_3dVector(13.0, -17.0, 29.0), CYCLIC_ROTATION,
	    ON_3dVector(0.0, 0.0, 0.0), 0.0, false},
	{"oblique-rotated-translated", 1.0,
	    ON_3dVector(-19.0, 23.0, 41.0), AXIS_ANGLE_ROTATION,
	    ON_3dVector(1.0, -2.0, 0.5), 0.731, false},
	{"small-similarity", 0.01, ON_3dVector(1.25, -2.5, 5.0),
	    AXIS_ANGLE_ROTATION, ON_3dVector(-0.3, 1.0, 0.7), -1.113,
	    false},
	{"large-similarity", 1.0e4,
	    ON_3dVector(1.0e6, -2.0e6, 3.0e6), AXIS_ANGLE_ROTATION,
	    ON_3dVector(2.0, 0.25, -1.0), 2.017, false}
    };
    double reference_existing[2] = {0.0, 0.0};
    double reference_continuation[2] = {0.0, 0.0};
    size_t reference_local_root_count[3][2] = {};
    size_t reference_local_cluster_count[3][2] = {};
    int reference_local_cluster_class[3][2]
	[RT_BREP_TRACE_MAX_LOCAL_CLUSTERS] = {};
    double reference_local_root_distances[3][2]
	[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};
    int failures = 0;
    size_t maximum_fixed_leaves = 0;
    size_t maximum_fixed_hits = 0;
    size_t maximum_discrepancy_cells = 0;
    size_t maximum_discrepancy_depth = 0;
    double maximum_discrepancy_width_ratio = 0.0;
    double maximum_parameter_locus_error = 0.0;
    double minimum_parameter_midpoint_shift = DBL_MAX;

    for (size_t case_index = 0;
	    case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
	const transform_case &test = cases[case_index];
	ON_Xform xform(ON_Xform::IdentityTransformation);
	if (test.rotation == CYCLIC_ROTATION)
	    xform = cobb_similarity_transform(test.scale, test.translation);
	else if (test.rotation == AXIS_ANGLE_ROTATION)
	    xform = cobb_axis_angle_similarity_transform(test.scale,
		test.translation, test.axis, test.angle);
	ON_Brep *variant = new ON_Brep(*base);
	if (!variant->Transform(xform)) {
	    std::printf("FAIL: Cobb %s BREP transform\n", test.name);
	    delete variant;
	    failures++;
	    continue;
	}
	if (test.reparameterize) {
	    double locus_error = 0.0;
	    double midpoint_shift = 0.0;
	    if (!cobb_reparameterize_edge_trims(variant, frame.edge_index,
		    locus_error, midpoint_shift)) {
		std::printf("FAIL: Cobb %s trim reparameterization\n",
		    test.name);
		delete variant;
		failures++;
		continue;
	    }
	    maximum_parameter_locus_error = std::max(
		maximum_parameter_locus_error, locus_error);
	    minimum_parameter_midpoint_shift = std::min(
		minimum_parameter_midpoint_shift, midpoint_shift);
	}
	/* ON_Brep tolerances are model-space lengths.  Restore their exact
	 * similarity-scaled values independently of Transform's policy. */
	for (int vertex_index = 0; vertex_index < variant->m_V.Count();
		++vertex_index)
	    variant->m_V[vertex_index].m_tolerance =
		base->m_V[vertex_index].m_tolerance * test.scale;
	for (int edge_index = 0; edge_index < variant->m_E.Count(); ++edge_index)
	    variant->m_E[edge_index].m_tolerance =
		base->m_E[edge_index].m_tolerance * test.scale;

	struct bn_tol case_tol = *tol;
	case_tol.dist = tol->dist * test.scale;
	case_tol.dist_sq = case_tol.dist * case_tol.dist;
	struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
	if (!rtip) {
	    std::printf("FAIL: Cobb %s rt_i construction\n", test.name);
	    delete variant;
	    failures++;
	    continue;
	}
	rtip->rti_tol = case_tol;
	struct resource resource = {};
	rt_init_resource(&resource, 0, rtip);

	struct rt_brep_internal variant_internal = {};
	variant_internal.magic = RT_BREP_INTERNAL_MAGIC;
	variant_internal.brep = variant;
	struct rt_db_internal variant_intern;
	RT_DB_INTERNAL_INIT(&variant_intern);
	variant_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	variant_intern.idb_type = ID_BREP;
	variant_intern.idb_meth = &OBJ[ID_BREP];
	variant_intern.idb_ptr = &variant_internal;
	struct soltab *stp = prep_solid(rtip, &variant_intern, ID_BREP);
	if (!stp) {
	    std::printf("FAIL: Cobb %s BREP prep\n", test.name);
	    failures++;
	} else {
	    const double clearance_ratios[] = {0.9, 0.0, -0.9};
	    for (size_t state_index = 0; state_index <
		    sizeof(clearance_ratios) / sizeof(clearance_ratios[0]);
		    ++state_index) {
		const double clearance = clearance_ratios[state_index] *
		    tol->dist;
		const int expected_state = clearance > 0.0 ? 1 :
		    (clearance < 0.0 ? -1 : 0);
		for (int reverse = 0; reverse <= 1; ++reverse) {
		    const sampled_ray base_ray = cobb_seam_grazing_ray(frame,
			origin, radius, clearance, reverse != 0);
		    const ON_3dPoint base_ray_origin(base_ray.origin);
		    const ON_3dVector base_direction(base_ray.direction);
		    const ON_3dPoint transformed_origin =
			cobb_transform_point(xform, base_ray_origin);
		    ON_3dVector transformed_direction =
			cobb_transform_vector(xform, base_direction);
		    if (!transformed_direction.Unitize()) {
			std::printf("FAIL: Cobb %s transformed direction\n",
			    test.name);
			failures++;
			continue;
		    }
		    sampled_ray ray;
		    VSET(ray.origin, transformed_origin.x,
			transformed_origin.y, transformed_origin.z);
		    VSET(ray.direction, transformed_direction.x,
			transformed_direction.y, transformed_direction.z);
		    const ray_result production_result = shoot_solid(stp, rtip,
			&resource, ray.origin, ray.direction);
		    struct rt_brep_shot_trace trace;
		    const int trace_hits = shoot_brep_trace(stp, rtip,
			&resource, ray, trace);
		maximum_fixed_leaves = std::max(maximum_fixed_leaves,
		    trace.fixed_leaf_count);
		maximum_fixed_hits = std::max(maximum_fixed_hits,
		    trace.fixed_hit_count);
		    const struct rt_brep_trace_edge *edge =
			brep_trace_edge(trace, frame.edge_index);
		    if (edge && edge->discrepancy_bounded) {
			maximum_discrepancy_cells = std::max(
			    maximum_discrepancy_cells,
			    edge->discrepancy_bound_cells);
			maximum_discrepancy_depth = std::max(
			    maximum_discrepancy_depth,
			    edge->discrepancy_bound_depth);
			maximum_discrepancy_width_ratio = std::max(
			    maximum_discrepancy_width_ratio,
			    (edge->discrepancy_upper_bound -
			    edge->discrepancy_lower_bound) /
			    (tol->dist * test.scale));
		    }
		    const size_t expected_closures = expected_state == 1 ? 1 : 0;
		    const int expected_direction = reverse ?
			RT_BREP_TRACE_ENTERING : RT_BREP_TRACE_LEAVING;
		    const double coordinate_scale = std::max(radius * test.scale,
			std::max(fabs(test.translation.x),
			std::max(fabs(test.translation.y),
			fabs(test.translation.z))));
		    const double normalized_limit = std::max(1.0e-9,
			4096.0 * DBL_EPSILON * coordinate_scale / test.scale);
		    const double normalized_root_limit = std::max(
			normalized_limit, 0.01 * tol->dist);
		    std::vector<double> local_root_distances;
		    local_root_distances.reserve(trace.stored_local_roots);
		    bool local_root_invalid = false;
		    for (size_t root_index = 0;
			    root_index < trace.stored_local_roots; ++root_index) {
			const struct rt_brep_trace_local_root &root =
			    trace.local_roots[root_index];
			local_root_distances.push_back(root.dist / test.scale);
			if (!std::isfinite(root.dist) ||
				!std::isfinite(root.residual) ||
				root.residual / test.scale > normalized_root_limit)
			    local_root_invalid = true;
		    }
		    std::sort(local_root_distances.begin(),
			local_root_distances.end());
		    const size_t local_cluster_count =
			trace.stored_local_clusters;
		    if (case_index == 0) {
			reference_local_root_count[state_index][reverse] =
			    local_root_distances.size();
			reference_local_cluster_count[state_index][reverse] =
			    local_cluster_count;
			for (size_t cluster_index = 0;
				cluster_index < local_cluster_count; ++cluster_index)
			    reference_local_cluster_class[state_index][reverse]
				[cluster_index] =
				trace.local_clusters[cluster_index].classification;
			for (size_t root_index = 0;
				root_index < local_root_distances.size(); ++root_index)
			    reference_local_root_distances[state_index][reverse]
				[root_index] = local_root_distances[root_index];
		    }
		    bool local_roots_differ = expected_state == 0 ?
			local_cluster_count !=
			reference_local_cluster_count[state_index][reverse] :
			local_root_distances.size() !=
			reference_local_root_count[state_index][reverse];
		    if (!local_roots_differ && expected_state != 0) {
			for (size_t root_index = 0;
				root_index < local_root_distances.size(); ++root_index) {
			    if (fabs(local_root_distances[root_index] -
				    reference_local_root_distances[state_index]
				    [reverse][root_index]) > normalized_root_limit) {
				local_roots_differ = true;
				break;
			    }
			}
		    }
		    if (expected_state == 0) {
			local_roots_differ = local_roots_differ ||
			    local_cluster_count != 1;
			for (size_t root_index = 0;
				root_index < local_root_distances.size(); ++root_index) {
			    if (fabs(local_root_distances[root_index] -
				    2.0 * radius) > 0.1 * tol->dist)
				local_roots_differ = true;
			}
		    }
		    if (local_cluster_count !=
			    reference_local_cluster_count[state_index][reverse]) {
			local_roots_differ = true;
		    } else {
			for (size_t cluster_index = 0;
				cluster_index < local_cluster_count; ++cluster_index) {
			    if (trace.local_clusters[cluster_index].classification !=
				    reference_local_cluster_class[state_index][reverse]
				    [cluster_index]) {
				local_roots_differ = true;
				break;
			    }
			}
		    }
		    bool bad = !brep_trace_fixed_workspaces_match(trace) ||
			!edge || !edge->candidate_spans ||
			!edge->within_edge_tolerance || !edge->sector_valid ||
			!edge->discrepancy_bounded ||
			edge->discrepancy_bound_exhausted ||
			edge->discrepancy_lower_bound < 0.0 ||
			edge->discrepancy_upper_bound <
			edge->discrepancy_lower_bound ||
			edge->measured_discrepancy >
			edge->discrepancy_upper_bound + normalized_limit *
			test.scale ||
			edge->discrepancy_upper_bound -
			edge->discrepancy_lower_bound >
			edge->discrepancy_bound_tolerance +
			normalized_limit * test.scale ||
			edge->tolerance_inferred ||
			!edge->discrepancy_measured ||
			!edge->correspondence_supported ||
			!edge->discrepancy_authorized ||
			fabs(edge->model_tolerance / test.scale -
			tol->dist) > normalized_limit ||
			fabs(edge->measured_discrepancy / test.scale -
			measured_gap) > normalized_limit ||
			trace.supported_surface_faces != 6 ||
			trace.unsupported_surface_faces != 0 ||
			trace.prepared_surface_spans != 6 ||
			trace.candidate_surface_spans +
			trace.excluded_surface_spans != 6 ||
			trace.surface_workspace_exhausted != 0 ||
			trace.surface_box_overflow != 0 ||
			trace.root_overflow != 0 ||
			trace.local_root_overflow != 0 ||
			trace.local_cluster_overflow != 0 ||
			trace.local_trim_failures != 0 ||
			trace.legacy_unique_roots_unmatched != 0 ||
			trace.root_event_mismatches != 0 ||
			trace.local_root_candidates !=
			trace.stored_local_roots ||
			trace.local_root_clusters !=
			trace.stored_local_clusters ||
			fabs(trace.local_cluster_tolerance / test.scale -
			0.1 * tol->dist) > normalized_limit ||
			trace.local_root_attempts !=
			trace.local_root_candidates +
			trace.local_root_failures +
			trace.local_root_duplicates ||
			local_root_invalid || local_roots_differ ||
			edge->closest_state != expected_state ||
			fabs(edge->distance / test.scale - fabs(clearance)) >
			normalized_limit ||
			fabs(edge->ray_dist / test.scale - 2.0 * radius) >
			normalized_limit ||
			trace.closure_candidates != expected_closures;
		    if (expected_closures) {
			if (case_index == 0) {
			    reference_existing[reverse] =
				trace.closure_existing_dist;
			    reference_continuation[reverse] =
				trace.continuation_dist;
			}
			bad = bad || trace.closure_edge_index != frame.edge_index ||
			    trace.closure_missing_direction != expected_direction ||
			    fabs(trace.closure_edge_dist / test.scale -
			    2.0 * radius) > normalized_limit ||
			    fabs(trace.closure_existing_dist / test.scale -
			    reference_existing[reverse]) > normalized_root_limit ||
			    (reverse ?
			    trace.closure_edge_dist >= trace.closure_existing_dist :
			    trace.closure_edge_dist <= trace.closure_existing_dist);
			bad = bad || trace.continuation_attempts != 1 ||
			    trace.continuation_candidates != 1 ||
			    trace.continuation_certified_candidates != 1 ||
			    !trace.continuation_certificate_root_boxes ||
			    trace.continuation_certificate_root_boxes !=
			    trace.continuation_certificate_isolated ||
			    trace.continuation_certificate_exhausted != 0 ||
			    trace.continuation_certificate_existing_overlap != 0 ||
			    trace.continuation_dist <
			    trace.continuation_certificate_t_min -
			    normalized_root_limit * test.scale ||
			    trace.continuation_dist >
			    trace.continuation_certificate_t_max +
			    normalized_root_limit * test.scale ||
			    trace.continuation_face_index < 0 ||
			    fabs(trace.continuation_dist / test.scale -
			    reference_continuation[reverse]) >
			    normalized_root_limit ||
			    (reverse ? trace.continuation_dist >=
			    trace.closure_edge_dist : trace.continuation_dist <=
			    trace.closure_edge_dist) ||
			    trace.closure_shadow_segments != 1 ||
			    trace_hits != 2 || trace.final_segments != 1 ||
			    production_result.segments != 1 ||
			    fabs(production_result.in_dist -
			    trace.closure_shadow_in_dist) >
			    normalized_root_limit * test.scale ||
			    fabs(production_result.out_dist -
			    trace.closure_shadow_out_dist) >
			    normalized_root_limit * test.scale ||
			    trace.closure_shadow_in_dist >=
			    trace.closure_shadow_out_dist;
		    } else {
			bad = bad || trace.continuation_attempts != 0 ||
			    trace.continuation_candidates != 0 ||
			    trace.continuation_certified_candidates != 0 ||
			    trace.continuation_certificate_boxes != 0 ||
			    trace.closure_shadow_segments != 0 || trace_hits != 0 ||
			    trace.final_segments != 0 ||
			    production_result.segments != 0;
		    }
		    if (bad) {
			brep_trace_root_coverage_diagnostic(test.name, trace);
			std::printf("FAIL: Cobb %s classifier state=%d "
			    "reverse=%d observed=%d distance=%.17g "
			    "edge-t=%.17g closure=%zu/%zu direction=%d/%d "
			    "existing-t=%.17g local=%zu/%zu clusters=%zu/%zu "
			    "failures=%zu invalid=%d differ=%d "
			    "leaves=%zu/%zu stored=%zu overflow=%zu fallback=%zu "
			    "mismatch=%zu hits=%zu/%zu overflow=%zu fallback=%zu "
			    "mismatch=%zu "
			    "trim=%zu/%zu/%zu mismatch=%zu local-trim=%zu/%zu/%zu "
			    "coverage=%zu/%zu/%zu/%zu events=%zu/%zu "
			    "surface=%zu/%zu/%zu overflow=%zu "
			    "certificate=%zu/%zu/%zu/%zu+%zu\n",
			    test.name,
			    expected_state,
			    reverse, edge ? edge->closest_state : -99,
			    edge ? edge->distance : INFINITY,
			    edge ? edge->ray_dist : INFINITY,
			    trace.closure_candidates, expected_closures,
			    trace.closure_missing_direction,
			    expected_direction, trace.closure_existing_dist,
			    local_root_distances.size(),
			    reference_local_root_count[state_index][reverse],
			    local_cluster_count,
			    reference_local_cluster_count[state_index][reverse],
			    trace.local_root_failures,
			    local_root_invalid, local_roots_differ,
			    trace.intersected_leaves, trace.fixed_leaf_count,
			    trace.fixed_leaf_stored, trace.fixed_leaf_overflow,
			    trace.fixed_leaf_fallback,
			    trace.fixed_leaf_mismatches, trace.fixed_hit_count,
			    trace.fixed_hit_stored, trace.fixed_hit_overflow,
			    trace.fixed_hit_fallback,
			    trace.fixed_hit_mismatches, trace.trim_queries,
			    trace.trim_noalloc_candidates,
			    trace.trim_allocating_candidates,
			    trace.trim_equivalence_mismatches,
			    trace.local_trim_queries, trace.local_trim_candidates,
			    trace.local_trim_failures, trace.legacy_unique_roots,
			    trace.legacy_unique_roots_unmatched,
			    trace.local_unique_roots,
			    trace.local_unique_roots_unmatched,
			    trace.matched_root_events, trace.root_event_mismatches,
			    trace.candidate_surface_spans,
			    trace.surface_subdivision_boxes,
			    trace.surface_isolated_boxes,
			    trace.surface_box_overflow,
			    trace.continuation_certificate_boxes,
			    trace.continuation_certificate_isolated,
			    trace.continuation_certificate_root_boxes,
			    trace.continuation_certificate_existing_overlap,
			    trace.continuation_certificate_exhausted);
			failures++;
		    }
		}
	    }
	    free_solid(stp);
	}
	rt_clean_resource_basic(rtip, &resource);
	BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
	rt_i_destroy(rtip);
    }

    delete base;
    if (!failures) {
	std::printf("Cobb classifier similarity/parameter invariance: PASS "
	    "max-leaves=%zu/%d max-raw-hits=%zu/%d "
	    "parameter-locus-error=%.3g midpoint-shift=%.3g "
	    "seam-bound-cells=%zu depth=%zu width/T=%.3g\n",
	    maximum_fixed_leaves, RT_BREP_MAX_LEAVES,
	    maximum_fixed_hits, RT_BREP_MAX_HITS,
	    maximum_parameter_locus_error,
	    minimum_parameter_midpoint_shift, maximum_discrepancy_cells,
	    maximum_discrepancy_depth, maximum_discrepancy_width_ratio);
    }
    return failures;
}


static int
check_cobb_ambiguous_correspondence(const struct bn_tol *tol,
    struct rt_i *rtip, struct resource *resource)
{
    const double radius = 10.0;
    const ON_3dPoint origin(0.0, 0.0, 0.0);
    ON_Brep *pristine = ON_Brep_CobbSphereSewn(radius, origin);
    cobb_seam_frame frame;
    double measured_gap = 0.0;
    double displacement = 0.0;
    ON_Brep *variant = cobb_bowed_seam_variant(pristine, origin,
	-tol->dist, frame, measured_gap, displacement);
    delete pristine;
    if (!variant || !cobb_make_ambiguous_edge_trim(variant,
	    frame.edge_index)) {
	std::printf("FAIL: Cobb ambiguous correspondence construction\n");
	delete variant;
	return 1;
    }

    struct rt_brep_internal variant_internal = {};
    variant_internal.magic = RT_BREP_INTERNAL_MAGIC;
    variant_internal.brep = variant;
    struct rt_db_internal variant_intern;
    RT_DB_INTERNAL_INIT(&variant_intern);
    variant_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    variant_intern.idb_type = ID_BREP;
    variant_intern.idb_meth = &OBJ[ID_BREP];
    variant_intern.idb_ptr = &variant_internal;
    struct soltab *stp = prep_solid(rtip, &variant_intern, ID_BREP);
    if (!stp) {
	std::printf("FAIL: Cobb ambiguous correspondence prep\n");
	return 1;
    }

    int failures = 0;
    for (int reverse = 0; reverse <= 1; ++reverse) {
	const sampled_ray ray = cobb_seam_grazing_ray(frame, origin, radius,
	    0.9 * tol->dist, reverse != 0);
	struct rt_brep_shot_trace trace;
	(void)shoot_brep_trace(stp, rtip, resource, ray, trace);
	const struct rt_brep_trace_edge *edge = brep_trace_edge(trace,
	    frame.edge_index);
	if (!brep_trace_fixed_workspaces_match(trace) || !edge ||
		!edge->discrepancy_measured ||
		!edge->discrepancy_authorized ||
		edge->correspondence_supported || edge->candidate_spans ||
		edge->within_edge_tolerance || edge->sector_valid ||
		edge->discrepancy_bounded ||
		edge->discrepancy_bound_exhausted ||
		trace.closure_candidates || trace.continuation_attempts ||
		trace.closure_shadow_segments ||
		trace.after_direction_cleanup != 1 || trace.final_segments != 0) {
	    std::printf("FAIL: Cobb ambiguous correspondence reverse=%d "
		"edge=%d measured=%d authorized=%d correspondence=%d "
		"spans=%zu within=%d sector=%d bound=%d exhausted=%d "
		"closure=%zu continuation=%zu segment=%zu cleanup=%zu\n",
		reverse,
		frame.edge_index, edge ? edge->discrepancy_measured : -1,
		edge ? edge->discrepancy_authorized : -1,
		edge ? edge->correspondence_supported : -1,
		edge ? edge->candidate_spans : 0,
		edge ? edge->within_edge_tolerance : -1,
		edge ? edge->sector_valid : -1,
		edge ? edge->discrepancy_bounded : -1,
		edge ? edge->discrepancy_bound_exhausted : -1,
		trace.closure_candidates, trace.continuation_attempts,
		trace.closure_shadow_segments, trace.after_direction_cleanup);
	    failures++;
	}
    }

    free_solid(stp);
    if (!failures)
	std::printf("Cobb ambiguous trim correspondence fallback: PASS\n");
    return failures;
}


static int
check_cobb_discrepancy_bound_budget(const struct bn_tol *tol,
    struct rt_i *rtip, struct resource *resource)
{
    const size_t expected_cell_budget = 4096;
    const size_t target_count = 3;
    const double radius = 10.0;
    const ON_3dPoint origin(0.0, 0.0, 0.0);
    ON_Brep *variant = ON_Brep_CobbSphereSewn(radius, origin);
    if (!variant) {
	std::printf("FAIL: Cobb adaptive seam budget construction\n");
	return 1;
    }

    struct budget_target {
	int face_index = -1;
	int side_index = -1;
	int edge_index = -1;
	double measured_discrepancy = INFINITY;
    } targets[target_count];
    std::vector<bool> used_faces(variant->m_F.Count(), false);
    size_t targets_found = 0;
    for (int face_index = 0; face_index < variant->m_F.Count() &&
	    targets_found < target_count; ++face_index) {
	if (used_faces[face_index])
	    continue;
	for (int side_index = 0; side_index < 4; ++side_index) {
	    int edge_index = -1;
	    if (!cobb_target_edge(variant, face_index, side_index,
		    edge_index))
		continue;
	    const ON_BrepEdge &edge = variant->m_E[edge_index];
	    if (edge.m_ti.Count() != 2)
		continue;
	    int other_face = -1;
	    bool contains_face = false;
	    bool valid_faces = true;
	    for (int trim_side = 0; trim_side < 2; ++trim_side) {
		const int trim_index = edge.m_ti[trim_side];
		if (trim_index < 0 || trim_index >= variant->m_T.Count()) {
		    valid_faces = false;
		    break;
		}
		const int incident_face =
		    variant->m_T[trim_index].FaceIndexOf();
		if (incident_face == face_index)
		    contains_face = true;
		else
		    other_face = incident_face;
	    }
	    if (!valid_faces || !contains_face || other_face < 0 ||
		    other_face >= variant->m_F.Count() ||
		    used_faces[other_face])
		continue;
	    targets[targets_found].face_index = face_index;
	    targets[targets_found].side_index = side_index;
	    targets[targets_found].edge_index = edge_index;
	    used_faces[face_index] = true;
	    used_faces[other_face] = true;
	    targets_found++;
	    break;
	}
    }

    bool construction_ok = targets_found == target_count;
    for (size_t target_index = 0; construction_ok &&
	    target_index < target_count; ++target_index) {
	budget_target &target = targets[target_index];
	construction_ok = cobb_perturb_boundary_interior(variant,
	    target.face_index, target.side_index, origin, -0.5 * tol->dist);
	if (!construction_ok)
	    break;
	target.measured_discrepancy = cobb_seam_discrepancy(variant,
	    target.edge_index);
	construction_ok = std::isfinite(target.measured_discrepancy) &&
	    target.measured_discrepancy > 1.0e-6 * tol->dist &&
	    target.measured_discrepancy <= tol->dist;
	if (construction_ok)
	    variant->m_E[target.edge_index].m_tolerance =
		target.measured_discrepancy * 1.01;
    }
    if (!construction_ok || !variant->IsValid()) {
	std::printf("FAIL: Cobb adaptive seam budget geometry targets=%zu/%zu\n",
	    targets_found, target_count);
	delete variant;
	return 1;
    }

    struct rt_brep_internal variant_internal = {};
    variant_internal.magic = RT_BREP_INTERNAL_MAGIC;
    variant_internal.brep = variant;
    struct rt_db_internal variant_intern;
    RT_DB_INTERNAL_INIT(&variant_intern);
    variant_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    variant_intern.idb_type = ID_BREP;
    variant_intern.idb_meth = &OBJ[ID_BREP];
    variant_intern.idb_ptr = &variant_internal;
    struct soltab *stp = prep_solid(rtip, &variant_intern, ID_BREP);
    if (!stp) {
	std::printf("FAIL: Cobb adaptive seam budget prep\n");
	return 1;
    }

    sampled_ray ray;
    VSET(ray.origin, 0.0, 0.0, 2.0 * radius);
    VSET(ray.direction, 0.0, 0.0, -1.0);
    struct rt_brep_shot_trace trace;
    (void)shoot_brep_trace(stp, rtip, resource, ray, trace);
    size_t bounded_count = 0;
    size_t exhausted_count = 0;
    size_t total_cells = 0;
    int failures = 0;
    const double limit = std::max(1.0e-10 * tol->dist,
	512.0 * DBL_EPSILON * radius);
    for (size_t target_index = 0; target_index < target_count;
	    ++target_index) {
	const budget_target &target = targets[target_index];
	const struct rt_brep_trace_edge *edge = brep_trace_edge(trace,
	    target.edge_index);
	if (!edge) {
	    std::printf("FAIL: Cobb adaptive seam budget edge=%d missing\n",
		target.edge_index);
	    failures++;
	    continue;
	}
	total_cells += edge->discrepancy_bound_cells;
	if (edge->discrepancy_bounded)
	    bounded_count++;
	if (edge->discrepancy_bound_exhausted)
	    exhausted_count++;
	bool invalid = !edge->discrepancy_measured ||
	    !edge->correspondence_supported ||
	    !edge->discrepancy_authorized ||
	    (edge->discrepancy_bounded &&
	    edge->discrepancy_bound_exhausted);
	if (edge->discrepancy_bounded) {
	    invalid = invalid || edge->discrepancy_lower_bound >
		target.measured_discrepancy + limit ||
		edge->discrepancy_upper_bound <
		target.measured_discrepancy - limit ||
		edge->discrepancy_upper_bound -
		edge->discrepancy_lower_bound >
		edge->discrepancy_bound_tolerance + limit;
	} else {
	    invalid = invalid || !edge->discrepancy_bound_exhausted;
	}
	if (invalid) {
	    std::printf("FAIL: Cobb adaptive seam budget edge=%d "
		"measured=%.17g bound=%.17g/%.17g target=%.17g "
		"bounded=%d exhausted=%d cells=%zu depth=%zu\n",
		target.edge_index, target.measured_discrepancy,
		edge->discrepancy_lower_bound,
		edge->discrepancy_upper_bound,
		edge->discrepancy_bound_tolerance,
		edge->discrepancy_bounded,
		edge->discrepancy_bound_exhausted,
		edge->discrepancy_bound_cells,
		edge->discrepancy_bound_depth);
	    failures++;
	}
    }
    if (!brep_trace_fixed_workspaces_match(trace) || bounded_count < 1 ||
	    exhausted_count < 1 || total_cells > expected_cell_budget) {
	std::printf("FAIL: Cobb adaptive seam budget bounded=%zu exhausted=%zu "
	    "cells=%zu/%zu\n", bounded_count, exhausted_count, total_cells,
	    expected_cell_budget);
	failures++;
    }
    free_solid(stp);
    if (!failures)
	std::printf("Cobb adaptive seam budget: PASS bounded=%zu "
	    "exhausted=%zu cells=%zu/%zu\n", bounded_count,
	    exhausted_count, total_cells, expected_cell_budget);
    return failures;
}


static int
check_cobb_tolerance_metadata(const struct bn_tol *tol, struct rt_i *rtip,
    struct resource *resource)
{
    const double radius = 10.0;
    const ON_3dPoint origin(0.0, 0.0, 0.0);
    ON_Brep *pristine = ON_Brep_CobbSphereSewn(radius, origin);
    if (!pristine) {
	std::printf("FAIL: Cobb tolerance-metadata pristine construction\n");
	return 1;
    }
    struct metadata_case {
	const char *name;
	double declared_ratio;
	bool unset;
	bool inferred;
	bool authorized;
    } cases[] = {
	{"correct", 1.01, false, false, true},
	{"unset", 0.0, true, true, true},
	{"explicit-zero", 0.0, false, false, false},
	{"half", 0.5, false, false, false},
	{"double", 2.0, false, false, true}
    };

    int failures = 0;
    for (size_t case_index = 0;
	    case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
	cobb_seam_frame frame;
	double measured_gap = 0.0;
	double displacement = 0.0;
	ON_Brep *variant = cobb_bowed_seam_variant(pristine, origin,
	    -2.0 * tol->dist, frame, measured_gap, displacement);
	if (!variant) {
	    std::printf("FAIL: Cobb %s tolerance variant\n",
		cases[case_index].name);
	    failures++;
	    continue;
	}
	variant->m_E[frame.edge_index].m_tolerance = cases[case_index].unset ?
	    ON_UNSET_VALUE : cases[case_index].declared_ratio * measured_gap;

	struct rt_brep_internal variant_internal = {};
	variant_internal.magic = RT_BREP_INTERNAL_MAGIC;
	variant_internal.brep = variant;
	struct rt_db_internal variant_intern;
	RT_DB_INTERNAL_INIT(&variant_intern);
	variant_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	variant_intern.idb_type = ID_BREP;
	variant_intern.idb_meth = &OBJ[ID_BREP];
	variant_intern.idb_ptr = &variant_internal;
	struct soltab *stp = prep_solid(rtip, &variant_intern, ID_BREP);
	if (!stp) {
	    std::printf("FAIL: Cobb %s tolerance prep\n",
		cases[case_index].name);
	    failures++;
	    continue;
	}

	const sampled_ray ray = cobb_seam_grazing_ray(frame, origin, radius,
	    0.9 * tol->dist, false);
	struct rt_brep_shot_trace trace;
	(void)shoot_brep_trace(stp, rtip, resource, ray, trace);
	const struct rt_brep_trace_edge *edge =
	    brep_trace_edge(trace, frame.edge_index);
	double expected_tolerance = tol->dist;
	if (cases[case_index].unset)
	    expected_tolerance = std::max(expected_tolerance, measured_gap);
	else
	    expected_tolerance = std::max(expected_tolerance,
		cases[case_index].declared_ratio * measured_gap);
	const double limit = std::max(1.0e-10 * tol->dist,
	    512.0 * DBL_EPSILON * radius);
	const bool declared_ok = cases[case_index].unset ?
	    edge && !ON_IsValid(edge->declared_tolerance) :
	    edge && fabs(edge->declared_tolerance -
	    cases[case_index].declared_ratio * measured_gap) <= limit;
	if (!brep_trace_fixed_workspaces_match(trace) ||
		!edge || !edge->discrepancy_measured ||
		!edge->correspondence_supported ||
		edge->discrepancy_bounded ||
		edge->discrepancy_bound_exhausted ||
		!declared_ok || edge->tolerance_inferred !=
		cases[case_index].inferred ||
		edge->discrepancy_authorized != cases[case_index].authorized ||
		fabs(edge->model_tolerance - tol->dist) > limit ||
		fabs(edge->measured_discrepancy - measured_gap) > limit ||
		fabs(edge->edge_tolerance - expected_tolerance) > limit ||
		edge->within_edge_tolerance != cases[case_index].authorized ||
		(cases[case_index].authorized && !edge->candidate_spans) ||
		(!cases[case_index].authorized && edge->candidate_spans)) {
	    std::printf("FAIL: Cobb %s tolerance metadata declared=%.17g "
		"model=%.17g measured=%.17g bound=%.17g/%.17g "
		"effective=%.17g "
		"inferred=%d/%d authorized=%d/%d within=%d spans=%zu\n",
		cases[case_index].name,
		edge ? edge->declared_tolerance : INFINITY,
		edge ? edge->model_tolerance : INFINITY,
		edge ? edge->measured_discrepancy : INFINITY,
		edge ? edge->discrepancy_lower_bound : INFINITY,
		edge ? edge->discrepancy_upper_bound : INFINITY,
		edge ? edge->edge_tolerance : INFINITY,
		edge ? edge->tolerance_inferred : -1,
		cases[case_index].inferred,
		edge ? edge->discrepancy_authorized : -1,
		cases[case_index].authorized,
		edge ? edge->within_edge_tolerance : -1,
		edge ? edge->candidate_spans : 0);
	    failures++;
	}
	free_solid(stp);
    }
    delete pristine;
    if (!failures)
	std::printf("Cobb seam tolerance metadata: PASS\n");
    return failures;
}


static int
check_brep_edge_sector_fixture(const char *label, ON_Brep *brep,
    int target_edge_index, ON_3dVector inside, const struct bn_tol *tol,
    struct rt_i *rtip, struct resource *resource)
{
    if (!brep || !brep->IsSolid() || target_edge_index < 0 ||
	    target_edge_index >= brep->m_E.Count()) {
	std::printf("FAIL: %s edge-sector geometry\n", label);
	delete brep;
	return 1;
    }
    for (int edge_index = 0; edge_index < brep->m_E.Count(); ++edge_index)
	brep->m_E[edge_index].m_tolerance = tol->dist;

    const ON_BrepEdge &target_edge = brep->m_E[target_edge_index];
    ON_3dPoint edge_point;
    ON_3dVector edge_tangent;
    if (!target_edge.Ev1Der(target_edge.Domain().Mid(), edge_point,
	    edge_tangent) || !edge_tangent.Unitize()) {
	std::printf("FAIL: %s edge-sector target evaluation\n", label);
	delete brep;
	return 1;
    }
    inside -= (inside * edge_tangent) * edge_tangent;
    ON_3dVector line_direction = ON_CrossProduct(edge_tangent, inside);
    if (!inside.Unitize() || !line_direction.Unitize()) {
	std::printf("FAIL: %s edge-sector frame\n", label);
	delete brep;
	return 1;
    }

    struct rt_brep_internal brep_internal = {};
    brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
    brep_internal.brep = brep;
    struct rt_db_internal brep_intern;
    RT_DB_INTERNAL_INIT(&brep_intern);
    brep_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    brep_intern.idb_type = ID_BREP;
    brep_intern.idb_meth = &OBJ[ID_BREP];
    brep_intern.idb_ptr = &brep_internal;
    struct soltab *stp = prep_solid(rtip, &brep_intern, ID_BREP);
    if (!stp) {
	std::printf("FAIL: %s edge-sector BREP prep\n", label);
	delete brep_internal.brep;
	return 1;
    }

    int failures = 0;
    const int states[] = {-1, 0, 1};
    for (size_t state_index = 0;
	    state_index < sizeof(states) / sizeof(states[0]); ++state_index) {
	const int expected_state = states[state_index];
	const ON_3dPoint closest = edge_point +
	    expected_state * 0.5 * tol->dist * inside;
	for (int reverse = 0; reverse <= 1; ++reverse) {
	    const ON_3dVector direction = reverse ? -line_direction :
		line_direction;
	    const ON_3dPoint ray_origin = closest - 12.0 * direction;
	    sampled_ray ray;
	    VSET(ray.origin, ray_origin.x, ray_origin.y, ray_origin.z);
	    VSET(ray.direction, direction.x, direction.y, direction.z);
	    struct rt_brep_shot_trace trace;
	    (void)shoot_brep_trace(stp, rtip, resource, ray, trace);
	    const struct rt_brep_trace_edge *observation =
		brep_trace_edge(trace, target_edge_index);
	    const double expected_distance = expected_state ?
		0.5 * tol->dist : 0.0;
	    if (!brep_trace_fixed_workspaces_match(trace) ||
		    !observation || !observation->within_edge_tolerance ||
		    !observation->correspondence_supported ||
		    !observation->candidate_spans ||
		    !observation->sector_valid ||
		    observation->closest_state != expected_state ||
		    fabs(observation->distance - expected_distance) >
		    1.0e-10 || fabs(observation->ray_edge_dot) > 1.0e-10) {
		std::printf("FAIL: %s edge sector state=%d reverse=%d "
		    "observed=%d valid=%d distance=%.17g spans=%zu "
		    "ray-edge=%.17g\n", label, expected_state, reverse,
		    observation ? observation->closest_state : -99,
		    observation ? observation->sector_valid : 0,
		    observation ? observation->distance : INFINITY,
		    observation ? observation->candidate_spans : 0,
		    observation ? observation->ray_edge_dot : INFINITY);
		failures++;
	    }
	}
    }
    free_solid(stp);
    return failures;
}


static int
check_brep_edge_sector_box(const struct bn_tol *tol, struct rt_i *rtip,
    struct resource *resource)
{
    struct rt_arb_internal box = {};
    box.magic = RT_ARB_INTERNAL_MAGIC;
    VSET(box.pt[0], -4.0, -3.0, -2.0);
    VSET(box.pt[1], 4.0, -3.0, -2.0);
    VSET(box.pt[2], 4.0, 3.0, -2.0);
    VSET(box.pt[3], -4.0, 3.0, -2.0);
    VSET(box.pt[4], -4.0, -3.0, 2.0);
    VSET(box.pt[5], 4.0, -3.0, 2.0);
    VSET(box.pt[6], 4.0, 3.0, 2.0);
    VSET(box.pt[7], -4.0, 3.0, 2.0);
    struct rt_db_internal box_intern;
    RT_DB_INTERNAL_INIT(&box_intern);
    box_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    box_intern.idb_type = ID_ARB8;
    box_intern.idb_meth = &OBJ[ID_ARB8];
    box_intern.idb_ptr = &box;

    ON_Brep *brep = ON_Brep::New();
    OBJ[ID_ARB8].ft_brep(&brep, &box_intern, tol);
    if (!brep || brep->m_E.Count() != 12) {
	std::printf("FAIL: convex edge-sector box conversion\n");
	delete brep;
	return 1;
    }
    const int target_edge_index = 0;
    const ON_3dPoint edge_point = brep->m_E[target_edge_index].PointAt(
	brep->m_E[target_edge_index].Domain().Mid());
    const ON_3dVector inside = ON_3dPoint(0.0, 0.0, 0.0) - edge_point;
    return check_brep_edge_sector_fixture("convex", brep,
	target_edge_index, inside, tol, rtip, resource);
}


static int
check_brep_edge_sector_concave(const struct bn_tol *tol, struct rt_i *rtip,
    struct resource *resource)
{
    ON_3dPointArray points;
    points.Append(ON_3dPoint(-3.0, -3.0, 0.0));
    points.Append(ON_3dPoint(3.0, -3.0, 0.0));
    points.Append(ON_3dPoint(3.0, -1.0, 0.0));
    points.Append(ON_3dPoint(-1.0, -1.0, 0.0));
    points.Append(ON_3dPoint(-1.0, 3.0, 0.0));
    points.Append(ON_3dPoint(-3.0, 3.0, 0.0));
    points.Append(points[0]);
    ON_PolylineCurve profile(points);
    ON_Extrusion extrusion;
    if (!ON_Extrusion::CreateFrom3dCurve(profile, &ON_Plane::World_xy,
	    4.0, true, &extrusion)) {
	std::printf("FAIL: concave edge-sector extrusion construction\n");
	return 1;
    }
    ON_Brep *brep = extrusion.BrepForm();
    if (!brep || !brep->IsSolid()) {
	std::printf("FAIL: concave edge-sector BREP conversion\n");
	delete brep;
	return 1;
    }

    const ON_3dPoint expected_midpoint(-1.0, -1.0, 2.0);
    int target_edge_index = -1;
    double target_distance = DBL_MAX;
    for (int edge_index = 0; edge_index < brep->m_E.Count(); ++edge_index) {
	const ON_BrepEdge &edge = brep->m_E[edge_index];
	ON_3dPoint point;
	ON_3dVector tangent;
	if (!edge.Ev1Der(edge.Domain().Mid(), point, tangent) ||
		!point.IsValid() || !tangent.Unitize() || fabs(tangent.z) < 0.9)
	    continue;
	const double distance = point.DistanceTo(expected_midpoint);
	if (distance < target_distance) {
	    target_distance = distance;
	    target_edge_index = edge_index;
	}
    }
    if (target_edge_index < 0 || target_distance > 1.0e-10) {
	std::printf("FAIL: concave edge-sector target search distance=%.17g\n",
	    target_distance);
	delete brep;
	return 1;
    }
    return check_brep_edge_sector_fixture("concave", brep,
	target_edge_index, ON_3dVector(-1.0, -1.0, 0.0), tol, rtip,
	resource);
}


static int
check_brep_edge_sector_seam(const struct bn_tol *tol, struct rt_i *rtip,
    struct resource *resource)
{
    struct rt_ell_internal sphere;
    struct rt_db_internal sphere_intern;
    point_t center = VINIT_ZERO;
    init_sphere_internal(sphere, sphere_intern, center, 5.0);
    ON_Brep *brep = ON_Brep::New();
    OBJ[ID_ELL].ft_brep(&brep, &sphere_intern, tol);
    if (!brep || !brep->IsSolid()) {
	std::printf("FAIL: same-surface seam BREP conversion\n");
	delete brep;
	return 1;
    }

    int target_edge_index = -1;
    for (int edge_index = 0; edge_index < brep->m_E.Count(); ++edge_index) {
	const ON_BrepEdge &edge = brep->m_E[edge_index];
	if (edge.m_ti.Count() != 2)
	    continue;
	const int first_face = brep->m_T[edge.m_ti[0]].FaceIndexOf();
	const int second_face = brep->m_T[edge.m_ti[1]].FaceIndexOf();
	if (first_face >= 0 && first_face == second_face) {
	    target_edge_index = edge_index;
	    break;
	}
    }
    if (target_edge_index < 0) {
	std::printf("FAIL: same-surface seam target search\n");
	delete brep;
	return 1;
    }
    const ON_BrepEdge &edge = brep->m_E[target_edge_index];
    const ON_3dPoint edge_point = edge.PointAt(edge.Domain().Mid());
    const ON_3dVector inside = ON_3dPoint(0.0, 0.0, 0.0) - edge_point;
    return check_brep_edge_sector_fixture("same-surface seam", brep,
	target_edge_index, inside, tol, rtip, resource);
}


static int
check_brep_vertex_fan_fallback(const struct bn_tol *tol, struct rt_i *rtip,
    struct resource *resource)
{
    struct rt_arb_internal box = {};
    box.magic = RT_ARB_INTERNAL_MAGIC;
    VSET(box.pt[0], -4.0, -3.0, -2.0);
    VSET(box.pt[1], 4.0, -3.0, -2.0);
    VSET(box.pt[2], 4.0, 3.0, -2.0);
    VSET(box.pt[3], -4.0, 3.0, -2.0);
    VSET(box.pt[4], -4.0, -3.0, 2.0);
    VSET(box.pt[5], 4.0, -3.0, 2.0);
    VSET(box.pt[6], 4.0, 3.0, 2.0);
    VSET(box.pt[7], -4.0, 3.0, 2.0);
    struct rt_db_internal box_intern;
    RT_DB_INTERNAL_INIT(&box_intern);
    box_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    box_intern.idb_type = ID_ARB8;
    box_intern.idb_meth = &OBJ[ID_ARB8];
    box_intern.idb_ptr = &box;

    ON_Brep *brep = ON_Brep::New();
    OBJ[ID_ARB8].ft_brep(&brep, &box_intern, tol);
    const ON_3dPoint target_point(4.0, 3.0, 2.0);
    int target_vertex = -1;
    if (brep) {
	for (int vertex_index = 0; vertex_index < brep->m_V.Count();
		++vertex_index) {
	    if (brep->m_V[vertex_index].point.DistanceTo(target_point) <=
		    1.0e-12) {
		target_vertex = vertex_index;
		break;
	    }
	}
    }
    int incident_edges[3] = {-1, -1, -1};
    size_t incident_count = 0;
    if (brep && target_vertex >= 0) {
	for (int edge_index = 0; edge_index < brep->m_E.Count();
		++edge_index) {
	    const ON_BrepEdge &edge = brep->m_E[edge_index];
	    if (edge.m_vi[0] != target_vertex && edge.m_vi[1] != target_vertex)
		continue;
	    if (incident_count < 3)
		incident_edges[incident_count] = edge_index;
	    incident_count++;
	}
    }
    if (!brep || !brep->IsSolid() || target_vertex < 0 ||
	    incident_count != 3) {
	std::printf("FAIL: convex vertex-fan geometry vertex=%d edges=%zu\n",
	    target_vertex, incident_count);
	delete brep;
	return 1;
    }
    for (int edge_index = 0; edge_index < brep->m_E.Count(); ++edge_index)
	brep->m_E[edge_index].m_tolerance = tol->dist;

    struct rt_brep_internal brep_internal = {};
    brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
    brep_internal.brep = brep;
    struct rt_db_internal brep_intern;
    RT_DB_INTERNAL_INIT(&brep_intern);
    brep_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    brep_intern.idb_type = ID_BREP;
    brep_intern.idb_meth = &OBJ[ID_BREP];
    brep_intern.idb_ptr = &brep_internal;
    struct soltab *stp = prep_solid(rtip, &brep_intern, ID_BREP);
    if (!stp) {
	std::printf("FAIL: convex vertex-fan BREP prep\n");
	delete brep_internal.brep;
	return 1;
    }

    ON_3dVector diagonal(1.0, 1.0, 1.0);
    diagonal.Unitize();
    struct vertex_ray_case {
	const char *name;
	ON_3dPoint origin;
	ON_3dVector direction;
    } cases[] = {
	{"through-forward", target_point + 20.0 * diagonal, -diagonal},
	{"through-reverse", target_point - 20.0 * diagonal, diagonal},
	{"surface-outward", target_point, diagonal},
	{"surface-inward", target_point, -diagonal}
    };
    int failures = 0;
    size_t maximum_contact_edges = 0;
    size_t maximum_near_inside_edges = 0;
    size_t maximum_near_closure_candidates = 0;
    for (size_t case_index = 0;
	    case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
	sampled_ray ray;
	VSET(ray.origin, cases[case_index].origin.x,
	    cases[case_index].origin.y, cases[case_index].origin.z);
	VSET(ray.direction, cases[case_index].direction.x,
	    cases[case_index].direction.y, cases[case_index].direction.z);
	struct rt_brep_shot_trace trace;
	const int trace_hits = shoot_brep_trace(stp, rtip, resource, ray,
	    trace);
	size_t contact_edges = 0;
	bool invalid_edge = false;
	for (size_t incident = 0; incident < 3; ++incident) {
	    const struct rt_brep_trace_edge *edge = brep_trace_edge(trace,
		incident_edges[incident]);
	    if (!edge || !edge->correspondence_supported ||
		    !edge->within_edge_tolerance || !edge->candidate_spans ||
		    !edge->sector_valid || edge->closest_state != 0 ||
		    edge->distance > 1.0e-10)
		invalid_edge = true;
	    else
		contact_edges++;
	}
	maximum_contact_edges = std::max(maximum_contact_edges,
	    contact_edges);
	if (!brep_trace_fixed_workspaces_match(trace) || invalid_edge ||
		contact_edges != 3 || trace.closure_candidates ||
		trace.continuation_attempts || trace.closure_shadow_segments ||
		trace_hits != 2 || trace.final_segments != 1) {
	    std::printf("FAIL: convex vertex-fan %s contacts=%zu/3 "
		"closure=%zu continuation=%zu shadow=%zu "
		"hits=%d/2 final=%zu/1\n", cases[case_index].name,
		contact_edges, trace.closure_candidates,
		trace.continuation_attempts, trace.closure_shadow_segments,
		trace_hits, trace.final_segments);
	    failures++;
	}
    }

    ON_3dVector fan_direction(1.0, -1.0, 0.0);
    fan_direction.Unitize();
    const ON_3dPoint fan_closest = target_point -
	0.25 * tol->dist * ON_3dVector(1.0, 1.0, 1.0);
    for (int reverse = 0; reverse <= 1; ++reverse) {
	const ON_3dVector direction = reverse ? -fan_direction :
	    fan_direction;
	const ON_3dPoint ray_origin = fan_closest - 20.0 * direction;
	sampled_ray ray;
	VSET(ray.origin, ray_origin.x, ray_origin.y, ray_origin.z);
	VSET(ray.direction, direction.x, direction.y, direction.z);
	struct rt_brep_shot_trace trace;
	(void)shoot_brep_trace(stp, rtip, resource, ray, trace);
	size_t qualified_edges = 0;
	size_t inside_edges = 0;
	int states[3] = {-99, -99, -99};
	for (size_t incident = 0; incident < 3; ++incident) {
	    const struct rt_brep_trace_edge *edge = brep_trace_edge(trace,
		incident_edges[incident]);
	    if (!edge)
		continue;
	    states[incident] = edge->closest_state;
	    if (edge->correspondence_supported &&
		    edge->within_edge_tolerance && edge->candidate_spans &&
		    edge->sector_valid) {
		qualified_edges++;
		if (edge->closest_state == 1)
		    inside_edges++;
	    }
	}
	const bool ambiguous_closure = trace.closure_candidates == 0 ||
	    trace.closure_candidates >= 2;
	maximum_near_inside_edges = std::max(maximum_near_inside_edges,
	    inside_edges);
	maximum_near_closure_candidates = std::max(
	    maximum_near_closure_candidates, trace.closure_candidates);
	if (!brep_trace_fixed_workspaces_match(trace) ||
		qualified_edges != 3 || inside_edges < 2 ||
		!ambiguous_closure || trace.continuation_attempts ||
		trace.closure_shadow_segments) {
	    std::printf("FAIL: convex vertex-fan near reverse=%d "
		"qualified=%zu/3 inside=%zu states=%d/%d/%d closure=%zu "
		"continuation=%zu shadow=%zu final=%zu\n", reverse,
		qualified_edges, inside_edges, states[0], states[1], states[2],
		trace.closure_candidates, trace.continuation_attempts,
		trace.closure_shadow_segments, trace.final_segments);
	    failures++;
	}
    }
    free_solid(stp);
    if (!failures)
	std::printf("Convex vertex-fan fallback: PASS contact-edges=%zu "
	    "near-inside=%zu closure-candidates=%zu\n", maximum_contact_edges,
	    maximum_near_inside_edges, maximum_near_closure_candidates);
    return failures;
}


static int
check_cobb_bowed_seam_corpus(const struct bn_tol *tol, bool emit_report,
    struct rt_i *trace_rtip, struct resource *trace_resource)
{
    int failures = 0;
    const double radius = 10.0;
    const ON_3dPoint origin(0.0, 0.0, 0.0);
    ON_Brep *pristine = ON_Brep_CobbSphereSewn(radius, origin);
    if (!pristine) {
	std::printf("FAIL: bowed Cobb pristine construction\n");
	return 1;
    }

    struct rt_ell_internal sphere;
    struct rt_db_internal sphere_intern;
    point_t center = VINIT_ZERO;
    init_sphere_internal(sphere, sphere_intern, center, radius);
    prepared_model implicit_model;
    if (!prep_partition_model(implicit_model, &sphere_intern,
	    "cobb_bow_oracle.s", tol)) {
	std::printf("FAIL: bowed Cobb implicit preparation\n");
	delete pristine;
	return 1;
    }

    const double gap_ratios[] = {0.1, 0.25, 0.5, 0.9, 1.0, 1.1, 2.0, 10.0};
    const double clearance_ratios[] = {
	100.0, 10.0, 2.0, 1.1, 1.0, 0.9, 0.5, 0.1,
	0.0, -0.1, -1.0, -10.0, -100.0
    };
    size_t total_rays = 0;
    size_t differing_partitions = 0;
    size_t uncertainty_band_differences = 0;
    size_t excessive_differences = 0;
    size_t uncertainty_band_invalid = 0;
    size_t below_envelope_crack_leaks = 0;
    size_t below_envelope_legacy_cases = 0;
    size_t below_envelope_repairs = 0;
    size_t reversal_inconsistencies = 0;
    size_t leaks_before_candidate_storage = 0;
    size_t leaks_during_trim_classification = 0;
    size_t leaks_during_hit_cleanup = 0;
    size_t leaks_with_target_edge_evidence = 0;
    size_t leaks_with_inside_sector_evidence = 0;
    size_t leaks_with_shadow_closure = 0;
    size_t leaks_with_shadow_continuation = 0;
    size_t leaks_with_certified_continuation = 0;
    size_t leaks_with_shadow_segment = 0;
    size_t leaks_with_single_local_cluster = 0;
    size_t leaks_with_double_local_cluster = 0;
    size_t leaks_with_triple_local_cluster = 0;
    size_t local_roots_without_legacy_root = 0;
    size_t legacy_roots_without_local_root = 0;
    size_t prepared_partition_improvements = 0;
    size_t prepared_partition_regressions = 0;
    size_t prepared_partition_ambiguous = 0;
    brep_root_event_summary root_events;
    size_t sector_inside = 0;
    size_t sector_contact = 0;
    size_t sector_outside = 0;
    size_t maximum_subdivision_boxes = 0;
    size_t maximum_isolated_boxes = 0;
    size_t maximum_subdivision_depth = 0;
    size_t maximum_workspace_high_water = 0;
    size_t maximum_certificate_boxes = 0;
    size_t maximum_certificate_workspace = 0;
    size_t maximum_local_root_attempts = 0;
    size_t maximum_local_root_failures = 0;
    size_t maximum_local_root_duplicates = 0;
    size_t maximum_fixed_leaves = 0;
    size_t maximum_fixed_hits = 0;
    size_t maximum_discrepancy_bound_cells = 0;
    size_t maximum_discrepancy_bound_depth = 0;
    double maximum_discrepancy_bound_width_ratio = 0.0;
    double maximum_calibration_error = 0.0;
    double maximum_edge_distance_error = 0.0;
    double maximum_lift_error = 0.0;
    double maximum_continuation_error = 0.0;
    double maximum_certificate_width = 0.0;
    double maximum_prepared_oracle_error = 0.0;

    if (emit_report) {
	std::printf("cobb_family,direction,g_over_T,h_over_T,"
	    "reverse,root_separation_over_T,implicit_partitions,brep_partitions,"
	    "implicit_chord,brep_chord,endpoint_error,valid,deterministic,"
	    "within_uncertainty,leaves,candidates,raw_hits,after_near_miss,"
	    "unique_candidates,after_near_hit,after_grazing,after_duplicates,"
	    "after_direction,final_hits,final_segments,edge_observations,"
	    "edge_candidates,prepared_edge_spans,candidate_edge_spans,"
	    "target_edge_distance,target_edge_tolerance,target_edge_spans,"
	    "target_edge_within,target_sector_valid,target_closest_state,"
	    "supported_surface_faces,unsupported_surface_faces,"
	    "prepared_surface_spans,candidate_surface_spans,"
	    "excluded_surface_spans,subdivision_boxes,isolated_boxes,"
	    "subdivision_max_depth,workspace_high_water,"
	    "workspace_exhausted\n");
	std::printf("cobb_leaf_traversal_columns,direction,g_over_T,h_over_T,"
	    "reverse,list_leaves,fixed_leaves,fixed_stored,overflow,"
	    "fallback,order_mismatches,raw_hits,fixed_hits,fixed_hit_stored,"
	    "fixed_hit_overflow,fixed_hit_fallback,fixed_hit_mismatches,"
	    "trim_queries,"
	    "trim_noalloc_candidates,trim_allocating_candidates,"
	    "trim_mismatches\n");
	std::printf("cobb_closure_columns,direction,g_over_T,h_over_T,reverse,"
	    "candidate_count,edge_index,edge_t,existing_t,missing_direction\n");
	std::printf("cobb_continuation_columns,direction,g_over_T,h_over_T,"
	    "reverse,attempts,candidates,face,t,u,v,residual,normal_dot,"
	    "iterations,certificate_boxes,certificate_isolated,"
	    "certificate_root_boxes,certificate_workspace,"
	    "certificate_exhausted,certificate_existing_overlap,"
	    "certified_candidates,certificate_t_min,certificate_t_max,"
	    "shadow_segments,shadow_in,shadow_out\n");
	std::printf("cobb_box_columns,direction,g_over_T,h_over_T,reverse,"
	    "box_index,face,u_min,u_max,v_min,v_max,t_min,t_max,depth\n");
	std::printf("cobb_root_columns,direction,g_over_T,h_over_T,reverse,"
	    "root_index,face,t,u,v,normal_dot,trim_distance,trim_status,"
	    "hit_class,adjacent_face\n");
	std::printf("cobb_local_root_columns,direction,g_over_T,h_over_T,"
	    "reverse,root_index,face,span,t,u,v,residual,normal_dot,"
	    "iterations\n");
	std::printf("cobb_local_summary_columns,direction,g_over_T,h_over_T,"
	    "reverse,attempts,candidates,failures,duplicates,overflow,"
	    "cluster_tolerance,clusters,cluster_overflow\n");
	std::printf("cobb_local_cluster_columns,direction,g_over_T,h_over_T,"
	    "reverse,cluster_index,face,t_min,t_max,normal_dot_min,"
	    "normal_dot_max,roots,entering,leaving,tangent,classification\n");
	std::printf("cobb_solver_columns,direction,g_over_T,h_over_T,reverse,"
	    "solver_calls,no_root,converged_regular,converged_singular,"
	    "duplicate,outside_domain,jacobian_singular,stalled,"
	    "iteration_limit,evaluation_failed,nonfinite,capacity_exhausted\n");
	std::printf("cobb_edge_columns,direction,g_over_T,h_over_T,reverse,"
	    "edge_index,face0,face1,distance,ray_t,edge_parameter,"
	    "edge_tolerance,model_tolerance,declared_tolerance,"
	    "measured_discrepancy,discrepancy_lower_bound,"
	    "discrepancy_upper_bound,discrepancy_bound_tolerance,"
	    "discrepancy_bounded,discrepancy_bound_cells,"
	    "discrepancy_bound_depth,discrepancy_bound_exhausted,"
	    "discrepancy_measured,correspondence_supported,"
	    "discrepancy_authorized,tolerance_inferred,candidate_spans,"
	    "within_edge_tolerance,lift0,lift1,"
	    "normal_dot0,normal_dot1,ray_edge_dot,sector_valid,closest_state\n");
    }

    for (size_t ratio_index = 0; ratio_index < sizeof(gap_ratios) /
	    sizeof(gap_ratios[0]); ++ratio_index) {
	for (int sign = -1; sign <= 1; sign += 2) {
	    const double target_gap = gap_ratios[ratio_index] * tol->dist;
	    cobb_seam_frame frame;
	    double measured_gap = 0.0;
	    double applied_displacement = 0.0;
	    ON_Brep *variant = cobb_bowed_seam_variant(pristine, origin,
		sign * target_gap, frame, measured_gap, applied_displacement);
	    const double calibration_error = variant ?
		fabs(measured_gap - target_gap) : INFINITY;
	    maximum_calibration_error = std::max(maximum_calibration_error,
		calibration_error);
	    if (!variant || !variant->IsValid() || !variant->IsSolid() ||
		    calibration_error > 1.0e-3 * target_gap) {
		std::printf("FAIL: bowed Cobb construction sign=%d g/T=%.3g "
		    "measured=%.17g target=%.17g displacement=%.17g\n",
		    sign, gap_ratios[ratio_index], measured_gap, target_gap,
		    applied_displacement);
		delete variant;
		failures++;
		continue;
	    }

	    struct rt_brep_internal variant_internal = {};
	    variant_internal.magic = RT_BREP_INTERNAL_MAGIC;
	    variant_internal.brep = variant;
	    struct rt_db_internal variant_intern;
	    RT_DB_INTERNAL_INIT(&variant_intern);
	    variant_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	    variant_intern.idb_type = ID_BREP;
	    variant_intern.idb_meth = &OBJ[ID_BREP];
	    variant_intern.idb_ptr = &variant_internal;
	    prepared_model variant_model;
	    if (!prep_partition_model(variant_model, &variant_intern,
		    "cobb_bowed.s", tol)) {
		std::printf("FAIL: bowed Cobb prep sign=%d g/T=%.3g\n", sign,
		    gap_ratios[ratio_index]);
		delete variant;
		failures++;
		continue;
	    }

	    ON_Brep *trace_geometry = new ON_Brep(*variant);
	    struct rt_brep_internal trace_internal = {};
	    trace_internal.magic = RT_BREP_INTERNAL_MAGIC;
	    trace_internal.brep = trace_geometry;
	    struct rt_db_internal trace_intern;
	    RT_DB_INTERNAL_INIT(&trace_intern);
	    trace_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	    trace_intern.idb_type = ID_BREP;
	    trace_intern.idb_meth = &OBJ[ID_BREP];
	    trace_intern.idb_ptr = &trace_internal;
	    struct soltab *trace_stp = prep_solid(trace_rtip, &trace_intern,
		ID_BREP);
	    if (!trace_stp) {
		std::printf("FAIL: bowed Cobb trace prep sign=%d g/T=%.3g\n",
		    sign, gap_ratios[ratio_index]);
		delete trace_internal.brep;
		free_prepared_model(variant_model);
		delete variant;
		failures++;
		continue;
	    }

	    for (size_t clearance_index = 0; clearance_index <
		    sizeof(clearance_ratios) /
		    sizeof(clearance_ratios[0]); ++clearance_index) {
		const double clearance = clearance_ratios[clearance_index] *
		    tol->dist;
		const double root_separation = clearance > 0.0 ?
		    2.0 * sqrt(2.0 * radius * clearance -
			clearance * clearance) : 0.0;
		partition_result forward_implicit;
		partition_result forward_variant;
		for (int reverse = 0; reverse <= 1; ++reverse) {
		    const sampled_ray ray = cobb_seam_grazing_ray(frame,
			origin, radius, clearance, reverse != 0);
		    const partition_result implicit_result = shoot_partitions(
			implicit_model, ray);
		    const partition_result variant_result = shoot_partitions(
			variant_model, ray);
		    const partition_result repeated_result = shoot_partitions(
			variant_model, ray);
		    struct rt_brep_shot_trace trace;
		    (void)shoot_brep_trace(trace_stp, trace_rtip,
			trace_resource, ray, trace);
		    maximum_fixed_leaves = std::max(maximum_fixed_leaves,
			trace.fixed_leaf_count);
		    maximum_fixed_hits = std::max(maximum_fixed_hits,
			trace.fixed_hit_count);
		    const size_t unique_candidates = brep_trace_unique_roots(trace);
		    maximum_subdivision_boxes = std::max(maximum_subdivision_boxes,
			trace.surface_subdivision_boxes);
		    maximum_isolated_boxes = std::max(maximum_isolated_boxes,
			trace.surface_isolated_boxes);
		    maximum_subdivision_depth = std::max(
			maximum_subdivision_depth,
			trace.surface_subdivision_max_depth);
		    maximum_workspace_high_water = std::max(
			maximum_workspace_high_water,
			trace.surface_workspace_high_water);
		    maximum_local_root_attempts = std::max(
			maximum_local_root_attempts, trace.local_root_attempts);
		    maximum_local_root_failures = std::max(
			maximum_local_root_failures, trace.local_root_failures);
		    maximum_local_root_duplicates = std::max(
			maximum_local_root_duplicates,
			trace.local_root_duplicates);
		    legacy_roots_without_local_root +=
			trace.legacy_unique_roots_unmatched;
		    brep_accumulate_root_events(root_events, trace);
		    maximum_certificate_boxes = std::max(
			maximum_certificate_boxes,
			trace.continuation_certificate_boxes);
		    maximum_certificate_workspace = std::max(
			maximum_certificate_workspace,
			trace.continuation_certificate_workspace);
		    if (trace.continuation_certificate_root_boxes)
			maximum_certificate_width = std::max(
			    maximum_certificate_width,
			    trace.continuation_certificate_t_max -
			    trace.continuation_certificate_t_min);
		    if (!brep_trace_fixed_workspaces_match(trace) ||
			    trace.root_overflow ||
			    trace.solver_calls != trace.intersected_leaves ||
			    trace.candidate_roots != trace.stored_roots ||
			    trace.final_segments != variant_result.partitions ||
			    trace.edge_overflow ||
			    trace.edge_evaluation_failures ||
			    trace.edge_observations != trace.stored_edges ||
			    trace.manifold_edges != 12 ||
			    trace.prepared_edge_spans != 12 ||
			    trace.supported_surface_faces != 6 ||
			    trace.unsupported_surface_faces != 0 ||
			    trace.prepared_surface_spans != 6 ||
			    trace.candidate_surface_spans +
			    trace.excluded_surface_spans !=
			    trace.prepared_surface_spans ||
			    trace.surface_subdivision_boxes <
			    trace.candidate_surface_spans ||
			    trace.surface_workspace_exhausted != 0 ||
			    trace.surface_box_overflow != 0 ||
			    trace.local_root_overflow != 0 ||
			    trace.local_cluster_overflow != 0 ||
			    trace.local_root_candidates !=
			    trace.stored_local_roots ||
			    trace.local_root_clusters !=
			    trace.stored_local_clusters ||
			    fabs(trace.local_cluster_tolerance -
			    0.1 * tol->dist) > 1.0e-15 ||
			    trace.local_root_attempts !=
			    trace.local_root_candidates +
			    trace.local_root_failures +
			    trace.local_root_duplicates ||
			    trace.surface_isolated_boxes !=
			    trace.stored_surface_boxes ||
			    (trace.final_hits != 1 &&
			    trace.closure_candidates != 0) ||
			    trace.candidate_edge_spans >
			    trace.prepared_edge_spans) {
			std::printf("FAIL: bowed Cobb trace accounting sign=%d "
			    "g/T=%.3g h/T=%.3g reverse=%d leaves/calls=%zu/%zu "
			    "roots=%zu/%zu+%zu segments/partitions=%zu/%zu "
			    "edges=%zu/%zu+%zu failed=%zu spans=%zu/%zu\n",
			    sign, gap_ratios[ratio_index],
			    clearance_ratios[clearance_index], reverse,
			    trace.intersected_leaves, trace.solver_calls,
			    trace.candidate_roots, trace.stored_roots,
			    trace.root_overflow, trace.final_segments,
			    variant_result.partitions, trace.manifold_edges,
			    trace.stored_edges, trace.edge_overflow,
			    trace.edge_evaluation_failures,
			    trace.candidate_edge_spans,
			    trace.prepared_edge_spans);
			failures++;
		    }
		    for (size_t root_index = 0;
			    root_index < trace.stored_roots; ++root_index) {
			if (!brep_trace_root_isolated(trace,
				trace.roots[root_index])) {
			    std::printf("FAIL: bowed Cobb root exclusion sign=%d "
				"g/T=%.3g h/T=%.3g reverse=%d root=%zu "
				"face=%d uv=%.17g/%.17g boxes=%zu\n", sign,
				gap_ratios[ratio_index],
				clearance_ratios[clearance_index], reverse,
				root_index, trace.roots[root_index].face_index,
				trace.roots[root_index].uv[0],
				trace.roots[root_index].uv[1],
				trace.stored_surface_boxes);
			    failures++;
			}
		    }
		    for (size_t local_index = 0;
			    local_index < trace.stored_local_roots; ++local_index) {
			bool matched = false;
			for (size_t root_index = 0;
				root_index < trace.stored_roots; ++root_index) {
			    if (fabs(trace.local_roots[local_index].dist -
				    trace.roots[root_index].dist) <=
				    0.1 * tol->dist) {
				matched = true;
				break;
			    }
			}
			if (!matched) {
			    local_roots_without_legacy_root++;
			    std::printf("FAIL: bowed Cobb local root lacks legacy "
				"root sign=%d g/T=%.3g h/T=%.3g reverse=%d "
				"root=%zu t=%.17g legacy=%zu\n", sign,
				gap_ratios[ratio_index],
				clearance_ratios[clearance_index], reverse,
				local_index, trace.local_roots[local_index].dist,
				trace.stored_roots);
			    failures++;
			}
		    }
		    const struct rt_brep_trace_edge *target_edge =
			brep_trace_edge(trace, frame.edge_index);
		    if (target_edge && target_edge->discrepancy_bounded) {
			maximum_discrepancy_bound_cells = std::max(
			    maximum_discrepancy_bound_cells,
			    target_edge->discrepancy_bound_cells);
			maximum_discrepancy_bound_depth = std::max(
			    maximum_discrepancy_bound_depth,
			    target_edge->discrepancy_bound_depth);
			maximum_discrepancy_bound_width_ratio = std::max(
			    maximum_discrepancy_bound_width_ratio,
			    (target_edge->discrepancy_upper_bound -
			    target_edge->discrepancy_lower_bound) /
			    tol->dist);
		    }
		    if (!target_edge) {
			std::printf("FAIL: bowed Cobb target edge observation sign=%d "
			    "g/T=%.3g h/T=%.3g reverse=%d edge=%d\n", sign,
			    gap_ratios[ratio_index],
			    clearance_ratios[clearance_index], reverse,
			    frame.edge_index);
			failures++;
		    }
		    const double edge_distance_error = target_edge ?
			fabs(target_edge->distance - fabs(clearance)) : INFINITY;
		    maximum_edge_distance_error = std::max(
			maximum_edge_distance_error, edge_distance_error);
		    const double edge_distance_limit = std::max(1.0e-10 * tol->dist,
			512.0 * DBL_EPSILON * radius);
		    const bool expected_discrepancy_bound =
			measured_gap <= tol->dist + edge_distance_limit;
		    const bool invalid_discrepancy_bound = target_edge &&
			(expected_discrepancy_bound ?
			(!target_edge->discrepancy_bounded ||
			target_edge->discrepancy_bound_exhausted ||
			target_edge->discrepancy_lower_bound >
			measured_gap + edge_distance_limit ||
			target_edge->discrepancy_upper_bound <
			measured_gap - edge_distance_limit ||
			target_edge->discrepancy_upper_bound -
			target_edge->discrepancy_lower_bound >
			target_edge->discrepancy_bound_tolerance +
			edge_distance_limit) :
			(target_edge->discrepancy_bounded ||
			target_edge->discrepancy_bound_exhausted));
		    const bool expected_edge_evidence = target_edge &&
			fabs(clearance) <= target_edge->edge_tolerance +
			edge_distance_limit;
		    const int expected_closest_state = clearance > 0.0 ? 1 :
			(clearance < 0.0 ? -1 : 0);
		    if (expected_edge_evidence && target_edge) {
			const double lift_error = fabs(std::max(
			    target_edge->lift_distance[0],
			    target_edge->lift_distance[1]) - measured_gap);
			maximum_lift_error = std::max(maximum_lift_error,
			    lift_error);
			if (target_edge->closest_state > 0)
			    sector_inside++;
			else if (target_edge->closest_state < 0)
			    sector_outside++;
			else
			    sector_contact++;
		    }
		    if (!target_edge || edge_distance_error > edge_distance_limit ||
			    !target_edge->discrepancy_measured ||
			    !target_edge->correspondence_supported ||
			    invalid_discrepancy_bound ||
			    !target_edge->discrepancy_authorized ||
			    target_edge->tolerance_inferred ||
			    fabs(target_edge->model_tolerance - tol->dist) >
			    edge_distance_limit ||
			    fabs(target_edge->declared_tolerance -
			    1.01 * measured_gap) > edge_distance_limit ||
			    fabs(target_edge->measured_discrepancy - measured_gap) >
			    edge_distance_limit ||
			    target_edge->within_edge_tolerance !=
			    expected_edge_evidence ||
			    (target_edge->within_edge_tolerance &&
			    (!target_edge->candidate_spans ||
			    !target_edge->sector_valid ||
			    target_edge->closest_state != expected_closest_state ||
			    fabs(std::max(target_edge->lift_distance[0],
				target_edge->lift_distance[1]) - measured_gap) >
			    edge_distance_limit ||
			    fabs(target_edge->ray_edge_dot) > 1.0e-10))) {
			std::printf("FAIL: bowed Cobb target edge distance sign=%d "
			    "g/T=%.3g h/T=%.3g reverse=%d distance=%.17g "
			    "expected=%.17g limit=%.17g tolerance=%.17g "
			    "spans=%zu within=%d/%d sector=%d state=%d/%d "
			    "lifts=%.17g/%.17g ray-edge=%.17g\n", sign,
			    gap_ratios[ratio_index],
			    clearance_ratios[clearance_index], reverse,
			    target_edge ? target_edge->distance : INFINITY,
			    fabs(clearance), edge_distance_limit,
			    target_edge ? target_edge->edge_tolerance : INFINITY,
			    target_edge ? target_edge->candidate_spans : 0,
			    target_edge ? target_edge->within_edge_tolerance : -1,
			    expected_edge_evidence,
			    target_edge ? target_edge->sector_valid : -1,
			    target_edge ? target_edge->closest_state : -99,
			    expected_closest_state,
			    target_edge ? target_edge->lift_distance[0] : INFINITY,
			    target_edge ? target_edge->lift_distance[1] : INFINITY,
			    target_edge ? target_edge->ray_edge_dot : INFINITY);
			failures++;
		    }
		    const bool valid = partition_result_valid(implicit_result,
			ray.direction) && partition_result_valid(variant_result,
			ray.direction) && partition_result_valid(repeated_result,
			ray.direction);
		    double repeat_error = 0.0;
		    const bool deterministic = partition_results_match(
			variant_result, repeated_result, 1.0e-12,
			repeat_error);
		    double endpoint_error = 0.0;
		    const bool same = partition_results_match(implicit_result,
			variant_result, tol->dist, endpoint_error);
		    double prepared_endpoint_error = 0.0;
		    const bool prepared_same = prepared_event_partitions_match(
			trace, implicit_result, tol->dist,
			prepared_endpoint_error);
		    if (trace.local_event_final_mismatches) {
			if (std::isfinite(prepared_endpoint_error))
			    maximum_prepared_oracle_error = std::max(
				maximum_prepared_oracle_error,
				prepared_endpoint_error);
			const char *classification = "ambiguous";
			if (prepared_same && !same) {
			    prepared_partition_improvements++;
			    classification = "improvement";
			} else if (!prepared_same && same) {
			    prepared_partition_regressions++;
			    classification = "regression";
			} else {
			    prepared_partition_ambiguous++;
			}
			std::printf("Prepared partition %s sign=%d g/T=%.17g "
			    "h/T=%.17g reverse=%d legacy=%zu local=%zu "
			    "implicit=%zu errors=%.17g/%.17g "
			    "intervals=%.17g/%.17g %.17g/%.17g "
			    "%.17g/%.17g\n", classification, sign,
			    gap_ratios[ratio_index],
			    clearance_ratios[clearance_index], reverse,
			    variant_result.partitions,
			    trace.local_event_final_segments,
			    implicit_result.partitions, endpoint_error,
			    prepared_endpoint_error,
			    variant_result.partitions ?
			    variant_result.intervals[0].in_dist : 0.0,
			    variant_result.partitions ?
			    variant_result.intervals[0].out_dist : 0.0,
			    trace.local_event_stored_segments ?
			    trace.local_event_segment_in[0] : 0.0,
			    trace.local_event_stored_segments ?
			    trace.local_event_segment_out[0] : 0.0,
			    implicit_result.partitions ?
			    implicit_result.intervals[0].in_dist : 0.0,
			    implicit_result.partitions ?
			    implicit_result.intervals[0].out_dist : 0.0);
		    }
		    const double implicit_chord = partition_chord(implicit_result);
		    const double variant_chord = partition_chord(variant_result);
		    const bool within_uncertainty = fabs(clearance) <=
			2.0 * (tol->dist + measured_gap);
		    if (!reverse) {
			forward_implicit = implicit_result;
			forward_variant = variant_result;
		    } else {
			const bool implicit_reversal =
			    forward_implicit.partitions ==
			    implicit_result.partitions &&
			    fabs(partition_chord(forward_implicit) -
				partition_chord(implicit_result)) <= tol->dist;
			const bool variant_reversal =
			    forward_variant.partitions ==
			    variant_result.partitions &&
			    fabs(partition_chord(forward_variant) -
				partition_chord(variant_result)) <= tol->dist;
			if (!implicit_reversal) {
			    std::printf("FAIL: implicit Cobb seam reversal "
				"h/T=%.3g\n",
				clearance_ratios[clearance_index]);
			    failures++;
			}
			if (!variant_reversal) {
			    reversal_inconsistencies++;
			    if (gap_ratios[ratio_index] <= 1.0 &&
				    clearance > 1.01 * measured_gap) {
				std::printf("FAIL: bowed Cobb reversal defect "
				    "spread sign=%d g/T=%.3g h/T=%.3g\n",
				    sign, gap_ratios[ratio_index],
				    clearance_ratios[clearance_index]);
				failures++;
			    }
			}
		    }
		    total_rays++;
		    if (!same) {
			differing_partitions++;
			if (within_uncertainty) {
			    uncertainty_band_differences++;
			} else {
			    excessive_differences++;
			}
		    }
		    if (!deterministic || (!valid && !within_uncertainty)) {
			std::printf("FAIL: bowed Cobb invalid/nondeterministic "
			    "sign=%d g/T=%.3g h/T=%.3g reverse=%d\n", sign,
			    gap_ratios[ratio_index],
			    clearance_ratios[clearance_index], reverse);
			failures++;
		    }
		    if (!valid && within_uncertainty)
			uncertainty_band_invalid++;
		    const bool crack_leak = clearance > 0.0 &&
			implicit_result.partitions > 0 &&
			variant_result.partitions == 0;
		    const bool legacy_crack_case =
			gap_ratios[ratio_index] <= 1.0 && clearance > 0.0 &&
			implicit_result.partitions > 0 && trace.final_hits == 1;
		    if (clearance <= 0.0 && trace.closure_candidates != 0) {
			std::printf("FAIL: bowed Cobb exterior/contact closure "
			    "sign=%d g/T=%.3g h/T=%.3g reverse=%d "
			    "candidates=%zu edge=%d\n", sign,
			    gap_ratios[ratio_index],
			    clearance_ratios[clearance_index], reverse,
			    trace.closure_candidates, trace.closure_edge_index);
			failures++;
		    }
		    if (gap_ratios[ratio_index] <= 1.0 && crack_leak)
			below_envelope_crack_leaks++;
		    if (legacy_crack_case) {
			below_envelope_legacy_cases++;
			if (variant_result.partitions == 1 &&
				trace.final_segments == 1)
			    below_envelope_repairs++;
			const size_t unique_local_roots =
			    trace.stored_local_clusters;
			if (unique_local_roots == 1) {
			    leaks_with_single_local_cluster++;
			} else if (unique_local_roots == 2) {
			    leaks_with_double_local_cluster++;
			} else if (unique_local_roots == 3) {
			    leaks_with_triple_local_cluster++;
			} else {
			    std::printf("FAIL: bowed Cobb crack leak has %zu local "
				"roots sign=%d g/T=%.3g h/T=%.3g reverse=%d "
				"attempts=%zu failures=%zu\n", unique_local_roots,
				sign, gap_ratios[ratio_index],
				clearance_ratios[clearance_index], reverse,
				trace.local_root_attempts,
				trace.local_root_failures);
			    failures++;
			}
			if (!brep_trace_local_root_near(trace,
				trace.closure_existing_dist,
				BREP_SAME_POINT_TOLERANCE)) {
			    std::printf("FAIL: bowed Cobb existing closure root "
				"lacks local root sign=%d g/T=%.3g h/T=%.3g "
				"reverse=%d t=%.17g\n", sign,
				gap_ratios[ratio_index],
				clearance_ratios[clearance_index], reverse,
				trace.closure_existing_dist);
			    failures++;
			}
			if (target_edge && target_edge->within_edge_tolerance)
			    leaks_with_target_edge_evidence++;
			if (target_edge && target_edge->sector_valid &&
				target_edge->closest_state == 1)
			    leaks_with_inside_sector_evidence++;
			if (trace.closure_candidates == 1 &&
				trace.closure_edge_index == frame.edge_index)
			    leaks_with_shadow_closure++;
			if (trace.continuation_candidates == 1)
			    leaks_with_shadow_continuation++;
			if (trace.continuation_certified_candidates == 1)
			    leaks_with_certified_continuation++;
			if (trace.closure_shadow_segments == 1)
			    leaks_with_shadow_segment++;
			const double expected_continuation_dist = reverse ?
			    implicit_result.intervals[0].in_dist :
			    implicit_result.intervals[0].out_dist;
			const double continuation_error = fabs(
			    trace.continuation_dist - expected_continuation_dist);
			maximum_continuation_error = std::max(
			    maximum_continuation_error, continuation_error);
			const int expected_missing_direction = reverse ?
			    RT_BREP_TRACE_ENTERING : RT_BREP_TRACE_LEAVING;
			const bool closure_ordered = reverse ?
			    trace.closure_edge_dist < trace.closure_existing_dist :
			    trace.closure_edge_dist > trace.closure_existing_dist;
			if (trace.closure_candidates != 1 ||
				trace.closure_edge_index != frame.edge_index ||
				trace.closure_missing_direction !=
				expected_missing_direction ||
				!closure_ordered || !target_edge ||
				fabs(trace.closure_edge_dist -
				target_edge->ray_dist) > edge_distance_limit) {
			    std::printf("FAIL: bowed Cobb shadow closure sign=%d "
				"g/T=%.3g h/T=%.3g reverse=%d candidates=%zu "
				"edge=%d/%d direction=%d/%d t=%.17g/%.17g\n",
				sign, gap_ratios[ratio_index],
				clearance_ratios[clearance_index], reverse,
				trace.closure_candidates,
				trace.closure_edge_index, frame.edge_index,
				trace.closure_missing_direction,
				expected_missing_direction,
				trace.closure_edge_dist,
				trace.closure_existing_dist);
			    failures++;
			}
			const int expected_continuation_face = target_edge &&
			    target_edge->face_index[0] == frame.face_index ?
			    target_edge->face_index[1] :
			    (target_edge ? target_edge->face_index[0] : -1);
			if (trace.continuation_attempts != 1 ||
				trace.continuation_candidates != 1 ||
				trace.continuation_certified_candidates != 1 ||
				!trace.continuation_certificate_root_boxes ||
				trace.continuation_certificate_root_boxes !=
				trace.continuation_certificate_isolated ||
				trace.continuation_certificate_exhausted ||
				trace.continuation_certificate_existing_overlap ||
				expected_continuation_dist <
				trace.continuation_certificate_t_min - 1.0e-7 ||
				expected_continuation_dist >
				trace.continuation_certificate_t_max + 1.0e-7 ||
				(trace.closure_existing_dist >=
				trace.continuation_certificate_t_min - 1.0e-7 &&
				trace.closure_existing_dist <=
				trace.continuation_certificate_t_max + 1.0e-7) ||
				trace.closure_shadow_segments != 1 ||
				fabs(trace.closure_shadow_in_dist -
				implicit_result.intervals[0].in_dist) > 1.0e-7 ||
				fabs(trace.closure_shadow_out_dist -
				implicit_result.intervals[0].out_dist) > 1.0e-7 ||
				trace.continuation_face_index !=
				expected_continuation_face ||
				continuation_error > 1.0e-7 ||
				trace.continuation_residual > 1.0e-7 ||
				(reverse ? trace.continuation_normal_dot >= 0.0 :
				trace.continuation_normal_dot <= 0.0)) {
			    std::printf("FAIL: bowed Cobb shadow continuation "
				"sign=%d g/T=%.3g h/T=%.3g reverse=%d "
				"attempts=%zu candidates=%zu face=%d/%d "
				"t=%.17g/%.17g error=%.17g residual=%.17g "
				"normal-dot=%.17g\n", sign,
				gap_ratios[ratio_index],
				clearance_ratios[clearance_index], reverse,
				trace.continuation_attempts,
				trace.continuation_candidates,
				trace.continuation_face_index,
				expected_continuation_face,
				trace.continuation_dist,
				expected_continuation_dist,
				continuation_error,
				trace.continuation_residual,
				trace.continuation_normal_dot);
			    failures++;
			}
			if (variant_result.partitions != 1 ||
				trace.final_segments != 1) {
			    std::printf("FAIL: bowed Cobb certified repair was not "
				"published sign=%d g/T=%.3g h/T=%.3g reverse=%d "
				"partitions=%zu segments=%zu\n", sign,
				gap_ratios[ratio_index],
				clearance_ratios[clearance_index], reverse,
				variant_result.partitions, trace.final_segments);
			    failures++;
			}
			if (unique_candidates < 2) {
			    leaks_before_candidate_storage++;
			} else if (trace.raw_hits < 2) {
			    leaks_during_trim_classification++;
			} else {
			    leaks_during_hit_cleanup++;
			}
		    }
		    if (gap_ratios[ratio_index] > 1.0 &&
			    trace.closure_candidates != 0) {
			std::printf("FAIL: bowed Cobb above-model-tolerance "
			    "closure sign=%d g/T=%.3g h/T=%.3g reverse=%d "
			    "candidates=%zu\n", sign, gap_ratios[ratio_index],
			    clearance_ratios[clearance_index], reverse,
			    trace.closure_candidates);
			failures++;
		    }
		    /* Outside the measured support mismatch, the deliberately bowed
		     * surface must not change the implicit solid classification. */
		    if (gap_ratios[ratio_index] <= 1.0 &&
			    clearance > 1.01 * measured_gap &&
			    implicit_result.partitions !=
			    variant_result.partitions) {
			std::printf("FAIL: bowed Cobb below-envelope leak "
			    "sign=%d g/T=%.3g h/T=%.3g reverse=%d "
			    "partitions=%zu/%zu\n", sign,
			    gap_ratios[ratio_index],
			    clearance_ratios[clearance_index], reverse,
			    implicit_result.partitions,
			    variant_result.partitions);
			failures++;
		    }
		    if (emit_report) {
			std::printf("cobb_leaf_traversal,%s,%.9g,%.9g,%d,"
			    "%zu,%zu,%zu,%zu,%zu,%zu,"
			    "%zu,%zu,%zu,%zu,%zu,%zu,"
			    "%zu,%zu,%zu,%zu\n",
			    sign > 0 ? "outward" : "inward",
			    measured_gap / tol->dist,
			    clearance / tol->dist, reverse,
			    trace.intersected_leaves, trace.fixed_leaf_count,
			    trace.fixed_leaf_stored, trace.fixed_leaf_overflow,
			    trace.fixed_leaf_fallback,
			    trace.fixed_leaf_mismatches, trace.raw_hits,
			    trace.fixed_hit_count, trace.fixed_hit_stored,
			    trace.fixed_hit_overflow,
			    trace.fixed_hit_fallback,
			    trace.fixed_hit_mismatches, trace.trim_queries,
			    trace.trim_noalloc_candidates,
			    trace.trim_allocating_candidates,
			    trace.trim_equivalence_mismatches);
			std::printf("bowed_surface_seam,%s,%.9g,%.9g,%d,%.9g,"
			    "%zu,%zu,%.9g,%.9g,%.9g,%d,%d,%d,%zu,%zu,%zu,"
			    "%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,"
			    "%zu,%.9g,%.9g,%zu,%d,%d,%d,%zu,%zu,%zu,%zu,"
			    "%zu,%zu,%zu,%zu,%zu,%zu\n",
			    sign > 0 ? "outward" :
			    "inward", measured_gap / tol->dist,
			    clearance / tol->dist, reverse,
			    root_separation / tol->dist,
			    implicit_result.partitions,
			    variant_result.partitions, implicit_chord,
			    variant_chord, endpoint_error, valid, deterministic,
			    within_uncertainty, trace.intersected_leaves,
			    trace.candidate_roots, trace.raw_hits,
			    trace.after_near_miss, unique_candidates,
			    trace.after_near_hit,
			    trace.after_grazing, trace.after_duplicates,
			    trace.after_direction_cleanup, trace.final_hits,
			    trace.final_segments, trace.edge_observations,
			    trace.edges_within_tolerance,
			    trace.prepared_edge_spans,
			    trace.candidate_edge_spans,
			    target_edge ? target_edge->distance : INFINITY,
			    target_edge ? target_edge->edge_tolerance : INFINITY,
			    target_edge ? target_edge->candidate_spans : 0,
			    target_edge ? target_edge->within_edge_tolerance : -1,
			    target_edge ? target_edge->sector_valid : -1,
			    target_edge ? target_edge->closest_state : -99,
			    trace.supported_surface_faces,
			    trace.unsupported_surface_faces,
			    trace.prepared_surface_spans,
			    trace.candidate_surface_spans,
			    trace.excluded_surface_spans,
			    trace.surface_subdivision_boxes,
			    trace.surface_isolated_boxes,
			    trace.surface_subdivision_max_depth,
			    trace.surface_workspace_high_water,
			    trace.surface_workspace_exhausted);
			std::printf("cobb_closure,%s,%.9g,%.9g,%d,%zu,%d,"
			    "%.17g,%.17g,%d\n",
			    sign > 0 ? "outward" : "inward",
			    measured_gap / tol->dist, clearance / tol->dist,
			    reverse, trace.closure_candidates,
			    trace.closure_edge_index, trace.closure_edge_dist,
			    trace.closure_existing_dist,
			    trace.closure_missing_direction);
			std::printf("cobb_continuation,%s,%.9g,%.9g,%d,%zu,%zu,"
			    "%d,%.17g,%.17g,%.17g,%.17g,%.17g,%zu,%zu,%zu,"
			    "%zu,%zu,%zu,%zu,%zu,%.17g,%.17g,%zu,%.17g,"
			    "%.17g\n",
			    sign > 0 ? "outward" : "inward",
			    measured_gap / tol->dist, clearance / tol->dist,
			    reverse, trace.continuation_attempts,
			    trace.continuation_candidates,
			    trace.continuation_face_index,
			    trace.continuation_dist, trace.continuation_uv[0],
			    trace.continuation_uv[1],
			    trace.continuation_residual,
			    trace.continuation_normal_dot,
			    trace.continuation_iterations,
			    trace.continuation_certificate_boxes,
			    trace.continuation_certificate_isolated,
			    trace.continuation_certificate_root_boxes,
			    trace.continuation_certificate_workspace,
			    trace.continuation_certificate_exhausted,
			    trace.continuation_certificate_existing_overlap,
			    trace.continuation_certified_candidates,
			    trace.continuation_certificate_t_min,
			    trace.continuation_certificate_t_max,
			    trace.closure_shadow_segments,
			    trace.closure_shadow_in_dist,
			    trace.closure_shadow_out_dist);
			for (size_t box_index = 0;
				box_index < trace.stored_surface_boxes;
				++box_index) {
			    const struct rt_brep_trace_surface_box &box =
				trace.surface_boxes[box_index];
			    std::printf("cobb_box,%s,%.9g,%.9g,%d,%zu,%d,"
				"%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%d\n",
				sign > 0 ? "outward" : "inward",
				measured_gap / tol->dist,
				clearance / tol->dist, reverse, box_index,
				box.face_index, box.uv_min[0], box.uv_max[0],
				box.uv_min[1], box.uv_max[1], box.t_min,
				box.t_max, box.depth);
			}
			for (size_t root_index = 0;
				root_index < trace.stored_roots; ++root_index) {
			    const struct rt_brep_trace_root &root =
				trace.roots[root_index];
			    std::printf("cobb_root,%s,%.9g,%.9g,%d,%zu,%d,"
				"%.17g,%.17g,%.17g,%.17g,%.17g,%d,%d,%d\n",
				sign > 0 ? "outward" : "inward",
				measured_gap / tol->dist,
				clearance / tol->dist, reverse, root_index,
				root.face_index, root.dist, root.uv[0],
				root.uv[1], root.normal_dot,
				root.trim_distance, root.trim_status,
				root.hit_class, root.adjacent_face_index);
			}
			for (size_t root_index = 0;
				root_index < trace.stored_local_roots;
				++root_index) {
			    const struct rt_brep_trace_local_root &root =
				trace.local_roots[root_index];
			    std::printf("cobb_local_root,%s,%.9g,%.9g,%d,%zu,"
				"%d,%d,%.17g,%.17g,%.17g,%.17g,%.17g,%zu\n",
				sign > 0 ? "outward" : "inward",
				measured_gap / tol->dist,
				clearance / tol->dist, reverse, root_index,
				root.face_index, root.span_index, root.dist,
				root.uv[0], root.uv[1], root.residual,
				root.normal_dot, root.iterations);
			}
			std::printf("cobb_local_summary,%s,%.9g,%.9g,%d,%zu,%zu,"
			    "%zu,%zu,%zu,%.17g,%zu,%zu\n",
			    sign > 0 ? "outward" : "inward",
			    measured_gap / tol->dist, clearance / tol->dist,
			    reverse, trace.local_root_attempts,
			    trace.local_root_candidates, trace.local_root_failures,
			    trace.local_root_duplicates,
			    trace.local_root_overflow,
			    trace.local_cluster_tolerance,
			    trace.stored_local_clusters,
			    trace.local_cluster_overflow);
			for (size_t cluster_index = 0;
				cluster_index < trace.stored_local_clusters;
				++cluster_index) {
			    const struct rt_brep_trace_local_cluster &cluster =
				trace.local_clusters[cluster_index];
			    std::printf("cobb_local_cluster,%s,%.9g,%.9g,%d,%zu,"
				"%d,%.17g,%.17g,%.17g,%.17g,%zu,%zu,%zu,%zu,%d\n",
				sign > 0 ? "outward" : "inward",
				measured_gap / tol->dist,
				clearance / tol->dist, reverse, cluster_index,
				cluster.face_index, cluster.dist_min,
				cluster.dist_max, cluster.normal_dot_min,
				cluster.normal_dot_max, cluster.roots,
				cluster.entering_roots, cluster.leaving_roots,
				cluster.tangent_roots, cluster.classification);
			}
			std::printf("cobb_solver,%s,%.9g,%.9g,%d,%zu",
			    sign > 0 ? "outward" : "inward",
			    measured_gap / tol->dist, clearance / tol->dist,
			    reverse, trace.solver_calls);
			for (size_t status = 0;
				status < RT_BREP_TRACE_SOLVER_STATUS_COUNT;
				++status)
			    std::printf(",%zu", trace.solver_status[status]);
			std::printf("\n");
			for (size_t edge_observation = 0;
				edge_observation < trace.stored_edges;
				++edge_observation) {
			    const struct rt_brep_trace_edge &edge =
				trace.edges[edge_observation];
			    std::printf("cobb_edge,%s,%.9g,%.9g,%d,%d,%d,%d,"
				"%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,"
				"%.17g,%.17g,%.17g,%d,%zu,%zu,%d,"
				"%d,%d,%d,%d,%zu,%d,%.17g,%.17g,"
				"%.17g,%.17g,%.17g,%d,%d\n",
				sign > 0 ? "outward" : "inward",
				measured_gap / tol->dist,
				clearance / tol->dist, reverse,
				edge.edge_index, edge.face_index[0],
				edge.face_index[1], edge.distance,
				edge.ray_dist, edge.edge_parameter,
				edge.edge_tolerance,
				edge.model_tolerance,
				edge.declared_tolerance,
				edge.measured_discrepancy,
				edge.discrepancy_lower_bound,
				edge.discrepancy_upper_bound,
				edge.discrepancy_bound_tolerance,
				edge.discrepancy_bounded,
				edge.discrepancy_bound_cells,
				edge.discrepancy_bound_depth,
				edge.discrepancy_bound_exhausted,
				edge.discrepancy_measured,
				edge.correspondence_supported,
				edge.discrepancy_authorized,
				edge.tolerance_inferred,
				edge.candidate_spans,
				edge.within_edge_tolerance,
				edge.lift_distance[0], edge.lift_distance[1],
				edge.face_normal_dot[0],
				edge.face_normal_dot[1], edge.ray_edge_dot,
				edge.sector_valid, edge.closest_state);
			}
		    }
		}
	    }

	    free_solid(trace_stp);
	    free_prepared_model(variant_model);
	    delete variant;
	}
    }

    if (!below_envelope_legacy_cases || below_envelope_crack_leaks ||
	    below_envelope_repairs != below_envelope_legacy_cases) {
	std::printf("FAIL: bowed Cobb production repair coverage "
	    "legacy=%zu repaired=%zu leaks=%zu\n",
	    below_envelope_legacy_cases, below_envelope_repairs,
	    below_envelope_crack_leaks);
	failures++;
    }

    std::printf("Cobb bowed seam matrix: rays=%zu differing=%zu "
	"uncertainty-band=%zu outside-band=%zu band-invalid=%zu "
	"below-envelope-leaks=%zu legacy-cases=%zu repairs=%zu "
	"reversal-inconsistencies=%zu legacy-stages=%zu/%zu/%zu "
	"edge-evidence=%zu "
	"inside-evidence=%zu shadow-closure=%zu "
	"shadow-continuation=%zu certified-continuation=%zu "
	"shadow-segment=%zu local-clusters=%zu/%zu/%zu "
	"sector-states=%zu/%zu/%zu "
	"max-edge-error=%.3g max-lift-error=%.3g "
	"max-continuation-error=%.3g max-calibration=%.3g\n",
	total_rays, differing_partitions, uncertainty_band_differences,
	excessive_differences, uncertainty_band_invalid,
	below_envelope_crack_leaks, below_envelope_legacy_cases,
	below_envelope_repairs,
	reversal_inconsistencies,
	leaks_before_candidate_storage, leaks_during_trim_classification,
	leaks_during_hit_cleanup, leaks_with_target_edge_evidence,
	leaks_with_inside_sector_evidence, leaks_with_shadow_closure,
	leaks_with_shadow_continuation,
	leaks_with_certified_continuation,
	leaks_with_shadow_segment,
	leaks_with_single_local_cluster,
	leaks_with_double_local_cluster,
	leaks_with_triple_local_cluster,
	sector_inside, sector_contact,
	sector_outside, maximum_edge_distance_error, maximum_lift_error,
	maximum_continuation_error,
	maximum_calibration_error);
    std::printf("Cobb surface isolation: max-boxes=%zu max-isolated=%zu "
	"max-depth=%zu workspace-high-water=%zu local-attempts=%zu "
	"local-failures=%zu local-duplicates=%zu unmatched-legacy=%zu "
	"unmatched-local=%zu events=%zu mismatched=%zu/%zu/%zu/%zu/%zu "
	"max-errors=%.3g/%.3g/%.3g/%.3g trims=%zu/%zu "
	"face-trims=%zu/%zu/%zu/%.3g\n",
	maximum_subdivision_boxes, maximum_isolated_boxes,
	maximum_subdivision_depth, maximum_workspace_high_water,
	maximum_local_root_attempts, maximum_local_root_failures,
	maximum_local_root_duplicates,
	legacy_roots_without_local_root,
	local_roots_without_legacy_root, root_events.matched,
	root_events.mismatched, root_events.trim_status_mismatches,
	root_events.hit_class_mismatches, root_events.direction_mismatches,
	root_events.adjacency_mismatches, root_events.maximum_t_error,
	root_events.maximum_uv_error, root_events.maximum_trim_error,
	root_events.maximum_normal_dot_error, root_events.local_trim_queries,
	root_events.local_trim_candidates, root_events.face_trim_queries,
	root_events.face_trim_candidates, root_events.face_trim_mismatches,
	root_events.maximum_face_trim_error);
    brep_print_prepared_event_summary("Cobb", root_events);
    std::printf("Cobb prepared partition changes: improvements=%zu "
	"regressions=%zu ambiguous=%zu max-changed-oracle-error=%.3g\n",
	prepared_partition_improvements, prepared_partition_regressions,
	prepared_partition_ambiguous, maximum_prepared_oracle_error);
    if (prepared_partition_regressions || prepared_partition_ambiguous) {
	std::printf("FAIL: prepared Cobb partitions have %zu regressions and "
	    "%zu ambiguous changes\n", prepared_partition_regressions,
	    prepared_partition_ambiguous);
	failures++;
    }
    if (root_events.surface_krawczyk_boxes) {
	std::printf("FAIL: %zu bowed Cobb boxes terminated adaptively\n",
	    root_events.surface_krawczyk_boxes);
	failures++;
    }
    if (root_events.local_event_failures ||
	    root_events.local_event_overflow ||
	    root_events.local_candidate_semantic_stage[0] != 0 ||
	    root_events.local_event_final_mismatches !=
	    prepared_partition_improvements + prepared_partition_regressions +
	    prepared_partition_ambiguous) {
	std::printf("FAIL: prepared Cobb event accounting failures=%zu "
	    "overflow=%zu near-miss-stage=%zu changes=%zu/%zu\n",
	    root_events.local_event_failures,
	    root_events.local_event_overflow,
	    root_events.local_candidate_semantic_stage[0],
	    root_events.local_event_final_mismatches,
	    prepared_partition_improvements + prepared_partition_regressions +
	    prepared_partition_ambiguous);
	failures++;
    }
    if (legacy_roots_without_local_root ||
	    local_roots_without_legacy_root) {
	std::printf("FAIL: Cobb prepared-span root coverage legacy=%zu "
	    "local=%zu\n", legacy_roots_without_local_root,
	    local_roots_without_legacy_root);
	failures++;
    }
    if (root_events.mismatched) {
	std::printf("FAIL: %zu matched Cobb roots changed event class\n",
	    root_events.mismatched);
	failures++;
    }
    std::printf("Cobb continuation certificate: max-boxes=%zu "
	"workspace-high-water=%zu max-t-width=%.9g\n",
	maximum_certificate_boxes, maximum_certificate_workspace,
	maximum_certificate_width);
    std::printf("Cobb fixed workspaces: leaves=%zu/%d raw-hits=%zu/%d\n",
	maximum_fixed_leaves, RT_BREP_MAX_LEAVES, maximum_fixed_hits,
	RT_BREP_MAX_HITS);
    std::printf("Cobb adaptive seam bounds: cells=%zu depth=%zu "
	"max-width/T=%.6g\n", maximum_discrepancy_bound_cells,
	maximum_discrepancy_bound_depth,
	maximum_discrepancy_bound_width_ratio);
    free_prepared_model(implicit_model);
    delete pristine;
    return failures;
}


static int
check_crofton_sphere(struct rt_db_internal *ell_intern,
    const struct bn_tol *tol, double radius)
{
    ON_Brep *brep = ON_Brep::New();
    OBJ[ID_ELL].ft_brep(&brep, ell_intern, tol);
    if (!brep) {
	std::printf("FAIL: Crofton sphere-to-BREP conversion\n");
	return 1;
    }

    struct rt_brep_internal brep_internal = {};
    brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
    brep_internal.brep = brep;
    struct rt_db_internal brep_intern;
    RT_DB_INTERNAL_INIT(&brep_intern);
    brep_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    brep_intern.idb_type = ID_BREP;
    brep_intern.idb_meth = &OBJ[ID_BREP];
    brep_intern.idb_ptr = &brep_internal;

    struct rt_crofton_params params = {20000, 0.0, 0.0};
    fastf_t implicit_area = 0.0;
    fastf_t implicit_volume = 0.0;
    fastf_t brep_area = 0.0;
    fastf_t brep_volume = 0.0;
    rt_crofton_sample(&implicit_area, &implicit_volume, ell_intern, &params);
    rt_crofton_sample(&brep_area, &brep_volume, &brep_intern, &params);
    delete brep_internal.brep;

    const double analytic_area = 4.0 * M_PI * radius * radius;
    const double analytic_volume = (4.0 / 3.0) * M_PI * radius * radius *
	radius;
    const double aggregate_tolerance = 0.03;
    const double paired_tolerance = 0.01;
    if (relative_error(implicit_area, analytic_area) > aggregate_tolerance ||
	    relative_error(implicit_volume, analytic_volume) >
	    aggregate_tolerance ||
	    relative_error(brep_area, analytic_area) > aggregate_tolerance ||
	    relative_error(brep_volume, analytic_volume) > aggregate_tolerance ||
	    relative_error(brep_area, implicit_area) > paired_tolerance ||
	    relative_error(brep_volume, implicit_volume) > paired_tolerance) {
	std::printf("FAIL: Crofton sphere analytic=[%.17g %.17g] "
	    "implicit=[%.17g %.17g] BREP=[%.17g %.17g]\n", analytic_area,
	    analytic_volume, implicit_area, implicit_volume, brep_area,
	    brep_volume);
	return 1;
    }
    return 0;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    const bool report_grazing = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--grazing-report");
    const bool report_cobb = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--cobb-report");
    const bool affine_only = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--affine-only");
    if (argc != 1 && !report_grazing && !report_cobb && !affine_only)
	bu_exit(1, "Usage: %s [--grazing-report|--cobb-report|"
	    "--affine-only]\n", argv[0]);

    const double radius = 10.0;
    struct rt_ell_internal ell = {};
    ell.magic = RT_ELL_INTERNAL_MAGIC;
    VSET(ell.v, 0.0, 0.0, 0.0);
    VSET(ell.a, radius, 0.0, 0.0);
    VSET(ell.b, 0.0, radius, 0.0);
    VSET(ell.c, 0.0, 0.0, radius);

    struct rt_db_internal ell_intern;
    RT_DB_INTERNAL_INIT(&ell_intern);
    ell_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ell_intern.idb_type = ID_ELL;
    ell_intern.idb_meth = &OBJ[ID_ELL];
    ell_intern.idb_ptr = &ell;

    struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
    if (!rtip)
	bu_exit(1, "rt_dirbuild_inmem() failed\n");
    rtip->rti_tol.magic = BN_TOL_MAGIC;
    rtip->rti_tol.dist = 0.0005;
    rtip->rti_tol.dist_sq = rtip->rti_tol.dist * rtip->rti_tol.dist;
    rtip->rti_tol.perp = 1.0e-6;
    rtip->rti_tol.para = 1.0 - rtip->rti_tol.perp;

    struct resource resp = {};
    rt_init_resource(&resp, 0, rtip);

    if (affine_only) {
	const int affine_failures =
	    check_ellipsoid_adaptive_affine(&rtip->rti_tol);
	rt_clean_resource_basic(rtip, &resp);
	BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
	rt_i_destroy(rtip);
	return affine_failures ? 1 : 0;
    }

    struct soltab *implicit_stp = prep_solid(rtip, &ell_intern, ID_ELL);
    if (!implicit_stp)
	bu_exit(1, "implicit sphere prep failed\n");

    ON_Brep *brep = ON_Brep::New();
    OBJ[ID_ELL].ft_brep(&brep, &ell_intern, &rtip->rti_tol);
    if (!brep)
	bu_exit(1, "sphere-to-BREP conversion failed\n");
    struct rt_brep_internal brep_internal = {};
    brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
    brep_internal.brep = brep;
    struct rt_db_internal brep_intern;
    RT_DB_INTERNAL_INIT(&brep_intern);
    brep_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    brep_intern.idb_type = ID_BREP;
    brep_intern.idb_meth = &OBJ[ID_BREP];
    brep_intern.idb_ptr = &brep_internal;

    struct soltab *brep_stp = prep_solid(rtip, &brep_intern, ID_BREP);
    if (!brep_stp) {
	delete brep_internal.brep;
	free_solid(implicit_stp);
	bu_exit(1, "converted BREP sphere prep failed\n");
    }

    struct directed_ray {
	const char *label;
	point_t origin;
	vect_t direction;
	double expected_in;
	double expected_out;
    } rays[] = {
	{"north-pole down", {0.0, 0.0, 20.0}, {0.0, 0.0, -1.0},
	    radius, 3.0 * radius},
	{"south-pole up", {0.0, 0.0, -20.0}, {0.0, 0.0, 1.0},
	    radius, 3.0 * radius},
	{"positive-x seam", {20.0, 0.0, 0.0}, {-1.0, 0.0, 0.0},
	    radius, 3.0 * radius},
	{"negative-y", {0.0, -20.0, 0.0}, {0.0, 1.0, 0.0},
	    radius, 3.0 * radius},
	{"inside toward pole", {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0},
	    -radius, radius},
	{"seam start inward", {radius, 0.0, 0.0}, {-1.0, 0.0, 0.0},
	    0.0, 2.0 * radius}
    };

    int failures = 0;
    failures += check_brep_interval_enclosures();
    for (size_t repeat = 0; repeat < 16; ++repeat) {
	for (size_t i = 0; i < sizeof(rays) / sizeof(rays[0]); ++i)
	    failures += check_ray(rays[i].label, implicit_stp, brep_stp,
		rtip, &resp, rays[i].origin, rays[i].direction,
		rays[i].expected_in, rays[i].expected_out);
    }

    failures += check_grazing_ratchet(implicit_stp, brep_stp, rtip, &resp,
	radius);

    point_t small_center = {1.25, -2.5, 5.0};
    failures += check_transformed_sphere(rtip, &resp, "small-translated",
	small_center, 0.01);
    point_t large_center = {1.0e6, -2.0e6, 3.0e6};
    failures += check_transformed_sphere(rtip, &resp, "large-translated",
	large_center, 1.0e4);

    if (report_grazing)
	grazing_report(implicit_stp, brep_stp, rtip, &resp, radius);

    free_solid(brep_stp);
    free_solid(implicit_stp);

    point_t sphere_min = {-radius, -radius, -radius};
    point_t sphere_max = {radius, radius, radius};
    failures += check_shared_crofton_fixture("sphere", &ell_intern,
	&rtip->rti_tol, sphere_min, sphere_max,
	4.0 * M_PI * radius * radius,
	(4.0 / 3.0) * M_PI * radius * radius * radius, 32000);
    failures += check_shared_primitive_corpus(&rtip->rti_tol);
    failures += check_brep_leaf_csg_corpus(&rtip->rti_tol);
    failures += check_cobb_sphere_corpus(&rtip->rti_tol);
    failures += check_brep_edge_sector_box(&rtip->rti_tol, rtip, &resp);
    failures += check_brep_edge_sector_concave(&rtip->rti_tol, rtip, &resp);
    failures += check_brep_edge_sector_seam(&rtip->rti_tol, rtip, &resp);
    failures += check_brep_vertex_fan_fallback(&rtip->rti_tol, rtip, &resp);
    failures += check_sphere_adaptive_similarity(&rtip->rti_tol);
    failures += check_ellipsoid_adaptive_affine(&rtip->rti_tol);
    failures += check_cobb_classifier_invariance(&rtip->rti_tol);
    failures += check_cobb_ambiguous_correspondence(&rtip->rti_tol, rtip,
	&resp);
    failures += check_cobb_discrepancy_bound_budget(&rtip->rti_tol, rtip,
	&resp);
    failures += check_cobb_tolerance_metadata(&rtip->rti_tol, rtip, &resp);
    failures += check_cobb_bowed_seam_corpus(&rtip->rti_tol, report_cobb,
	rtip, &resp);
    failures += check_crofton_sphere(&ell_intern, &rtip->rti_tol, radius);

    rt_clean_resource_basic(rtip, &resp);
    BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
    rt_i_destroy(rtip);

    if (failures)
	std::printf("BREP ray correctness corpus: %d failure(s)\n", failures);
    else
	std::printf("BREP ray correctness corpus: PASS\n");
    return failures ? 1 : 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

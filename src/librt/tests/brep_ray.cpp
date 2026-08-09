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
#include "brep/surfacetree.h"
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
    struct directory *dp = (struct directory *)bu_calloc(1,
	sizeof(struct directory), "direct ray test directory");
    dp->d_magic = RT_DIR_MAGIC;
    dp->d_namep = (char *)"direct_ray_test.s";
    stp->st_dp = dp;
    stp->st_id = type;
    stp->st_meth = &OBJ[type];

    if (OBJ[type].ft_prep(stp, intern, rtip)) {
	if (stp->st_specific && stp->st_meth && stp->st_meth->ft_free)
	    stp->st_meth->ft_free(stp);
	bu_free((void *)stp->st_dp, "direct ray test directory");
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
    bu_free((void *)stp->st_dp, "direct ray test directory");
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


static bool
prep_nested_sphere_csg_model(prepared_model &model,
    const struct bn_tol *tol, bool brep_leaves)
{
    if (!tol)
	return false;
    model.dbip = db_open_inmem();
    if (!model.dbip)
	return false;
    struct rt_wdb *wdbp = wdb_dbopen(model.dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp)
	return false;

    struct rt_ell_internal outer = {};
    outer.magic = RT_ELL_INTERNAL_MAGIC;
    VSET(outer.v, 0.0, 0.0, 0.0);
    VSET(outer.a, 5.0, 0.0, 0.0);
    VSET(outer.b, 0.0, 5.0, 0.0);
    VSET(outer.c, 0.0, 0.0, 5.0);
    struct rt_db_internal outer_intern;
    RT_DB_INTERNAL_INIT(&outer_intern);
    outer_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    outer_intern.idb_type = ID_ELL;
    outer_intern.idb_meth = &OBJ[ID_ELL];
    outer_intern.idb_ptr = &outer;

    struct rt_ell_internal inner = {};
    inner.magic = RT_ELL_INTERNAL_MAGIC;
    VSET(inner.v, 0.0, 0.0, 0.0);
    VSET(inner.a, 2.0, 0.0, 0.0);
    VSET(inner.b, 0.0, 2.0, 0.0);
    VSET(inner.c, 0.0, 0.0, 2.0);
    struct rt_db_internal inner_intern;
    RT_DB_INTERNAL_INIT(&inner_intern);
    inner_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    inner_intern.idb_type = ID_ELL;
    inner_intern.idb_meth = &OBJ[ID_ELL];
    inner_intern.idb_ptr = &inner;

    const bool outer_ok = brep_leaves ?
	export_brep_conversion(model.dbip, wdbp, &outer_intern, "outer.s", tol) :
	export_internal_object(model.dbip, wdbp, &outer_intern, "outer.s");
    const bool inner_ok = brep_leaves ?
	export_brep_conversion(model.dbip, wdbp, &inner_intern, "inner.s", tol) :
	export_internal_object(model.dbip, wdbp, &inner_intern, "inner.s");

    mat_t left;
    mat_t right;
    MAT_IDN(left);
    MAT_IDN(right);
    MAT_DELTAS(left, -8.0, 0.0, 0.0);
    MAT_DELTAS(right, 8.0, 0.0, 0.0);
    struct wmember pair_members;
    BU_LIST_INIT(&pair_members.l);
    const bool pair_ok = outer_ok && inner_ok &&
	mk_addmember("outer.s", &pair_members.l, left, WMOP_UNION) &&
	mk_addmember("outer.s", &pair_members.l, right, WMOP_UNION) &&
	!mk_lcomb(wdbp, "outer_pair.c", &pair_members, 0, NULL, NULL, NULL, 0);

    struct wmember region_members;
    BU_LIST_INIT(&region_members.l);
    if (!pair_ok ||
	    !mk_addmember("outer_pair.c", &region_members.l, NULL,
		WMOP_UNION) ||
	    !mk_addmember("inner.s", &region_members.l, left,
		WMOP_SUBTRACT) ||
	    mk_lcomb(wdbp, "oracle.r", &region_members, 1, NULL, NULL, NULL, 0))
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


static void
report_grazing_trace(const char *label, double chord_ratio, int reverse,
    const struct rt_brep_shot_trace &trace)
{
    std::printf("%s trace ratio/reverse=%.17g/%d "
	"prepared faces/reparam/spans/candidate/excluded=%zu/%zu/%zu/%zu/%zu "
	"fallback=%d boxes/local/events=%zu/%zu/%zu "
	"legacy leaves/raw/stages/final=%zu/%zu/%zu/%zu/%zu/%zu/%zu/%zu "
	"solver calls/roots=%zu/%zu\n", label, chord_ratio, reverse,
	trace.supported_surface_faces, trace.reparameterized_surface_faces,
	trace.prepared_surface_spans,
	trace.candidate_surface_spans, trace.excluded_surface_spans,
	trace.prepared_production_fallback, trace.stored_surface_boxes,
	trace.stored_local_roots, trace.stored_physical_events,
	trace.intersected_leaves, trace.raw_hits, trace.after_near_miss,
	trace.after_near_hit, trace.after_grazing, trace.after_duplicates,
	trace.after_direction_cleanup, trace.final_segments,
	trace.solver_calls, trace.stored_roots);
    std::printf("  prepared isolation boxes/krawczyk/fold="
	"%zu/%zu/%zu fold roots/complete=%zu/%zu "
	"events complete/unresolved/state=%zu/%zu/%zu "
	"terminal expansion=%zu/%zu/%zu/%zu/%zu refine/budget=%zu/%zu "
	"high-water=%zu\n",
	trace.surface_isolated_boxes, trace.surface_krawczyk_boxes,
	trace.surface_fold_attempts, trace.stored_surface_fold_roots,
	trace.surface_fold_complete, trace.physical_event_complete,
	trace.physical_event_unresolved, trace.physical_event_state_failures,
	trace.surface_terminal_expansion_attempts,
	trace.surface_terminal_expansion_available,
	trace.surface_terminal_expansion_exclusions,
	trace.surface_terminal_expansion_krawczyk,
	trace.surface_terminal_expansion_failures,
	trace.surface_terminal_expansion_refinements,
	trace.surface_terminal_expansion_budget_exhausted,
	trace.surface_terminal_expansion_high_water);
    std::printf("  fold proof expansion/corridor/unique/graph/boundary/strip="
	"%zu/%zu/%zu/%zu/%zu/%zu failures=%zu/%zu/%zu/%zu\n",
	trace.surface_fold_expansion_certified,
	trace.surface_fold_corridor_available,
	trace.surface_fold_corridor_unique,
	trace.surface_fold_corridor_graph_certified,
	trace.surface_fold_boundary_existence_certified,
	trace.surface_fold_strip_excluded,
	trace.surface_fold_expansion_failures,
	trace.surface_fold_corridor_failures,
	trace.surface_fold_boundary_existence_failures,
	trace.surface_fold_strip_restriction_failures +
	    trace.surface_fold_strip_arithmetic_failures +
	    trace.surface_fold_strip_depth_exhausted +
	    trace.surface_fold_strip_workspace_exhausted);
    std::printf("  fold mixed pairs=%zu localization=%zu/%zu/%zu "
	"unmatched=%zu direction=%zu/%zu trim=%zu/%zu\n",
	trace.surface_fold_mixed_pairs,
	trace.surface_fold_localization_attempts,
	trace.surface_fold_localization_certified,
	trace.surface_fold_localization_failures,
	trace.surface_fold_unmatched_roots,
	trace.surface_fold_direction_checks,
	trace.surface_fold_direction_mismatches,
	trace.surface_fold_trim_queries,
	trace.surface_fold_trim_failures);
    for (size_t box_index = 0; box_index < trace.stored_surface_boxes;
	    ++box_index) {
	const struct rt_brep_trace_surface_box &box =
	    trace.surface_boxes[box_index];
	std::printf("  box[%zu] span/depth/disposition/sign/t="
	    "%d/%d/%d/%d %.17g %.17g\n", box_index, box.span_index,
	    box.depth, box.disposition, box.determinant_sign,
	    box.t_min, box.t_max);
    }
    for (size_t root_index = 0; root_index < trace.stored_local_roots;
	    ++root_index) {
	const struct rt_brep_trace_local_root &root =
	    trace.local_roots[root_index];
	std::printf("  local[%zu] span/t/uv/dot/trim/class/dir="
	    "%d %.17g %.17g %.17g %.17g/%d/%d/%d\n", root_index,
	    root.span_index, root.dist, root.uv[0], root.uv[1],
	    root.normal_dot, root.trim_status, root.hit_class,
	    root.direction);
    }
    for (size_t root_index = 0;
	    root_index < trace.stored_surface_fold_roots; ++root_index) {
	const struct rt_brep_trace_fold_root &root =
	    trace.surface_fold_roots_data[root_index];
	std::printf("  fold[%zu] span/t/range/uv/dot/trim/class/dir/sign="
	    "%d %.17g %.17g %.17g %.17g %.17g %.17g/%d/%d/%d/%d\n",
	    root_index, root.span_index, root.dist, root.t_min, root.t_max,
	    root.uv[0], root.uv[1], root.normal_dot, root.trim_status,
	    root.hit_class, root.direction, root.determinant_sign);
    }
    for (size_t root_index = 0; root_index < trace.stored_roots;
	    ++root_index) {
	const struct rt_brep_trace_root &root = trace.roots[root_index];
	std::printf("  root[%zu] t/uv/dot/trim/class/dir="
	    "%.17g %.17g %.17g %.17g/%d/%d/%d\n", root_index,
	    root.dist, root.uv[0], root.uv[1], root.normal_dot,
	    root.trim_status, root.hit_class, root.direction);
    }
    std::printf("  solver status:");
    for (size_t status = 0; status < RT_BREP_TRACE_SOLVER_STATUS_COUNT;
	    ++status)
	std::printf(" %zu", trace.solver_status[status]);
    std::printf("\n");
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
    size_t surface_corrector_statuses = 0;
    size_t local_corrector_statuses = 0;
    for (size_t status = 0;
	    status < RT_BREP_TRACE_CORRECTOR_STATUS_COUNT; ++status) {
	surface_corrector_statuses += trace.surface_corrector_status[status];
	local_corrector_statuses += trace.local_corrector_status[status];
    }
    const bool corrector_statuses_match =
	surface_corrector_statuses == trace.surface_corrector_attempts &&
	local_corrector_statuses == trace.local_root_attempts &&
	trace.surface_corrector_status[RT_BREP_TRACE_CORRECTOR_CONVERGED] ==
	    trace.surface_corrector_converged &&
	trace.local_corrector_failure_ratios <=
	    trace.local_root_attempts -
	    trace.local_corrector_status[RT_BREP_TRACE_CORRECTOR_CONVERGED] &&
	(!trace.local_corrector_failure_ratios ||
	 (trace.local_corrector_failure_ratios &&
	  trace.local_corrector_min_failure_ratio > 1.0 &&
	  trace.local_corrector_max_failure_ratio >=
	    trace.local_corrector_min_failure_ratio));
    const bool rotated_hulls_match =
	trace.surface_rotated_hull_attempts ==
	trace.surface_rotated_hull_exclusions +
	trace.surface_rotated_hull_retained +
	trace.surface_rotated_hull_inconclusive;
    return leaf_workspace_matches &&
	corrector_statuses_match &&
	rotated_hulls_match &&
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
	(trace.surface_fold_root_failures ||
	 trace.surface_fold_trim_queries + trace.surface_fold_trim_failures ==
	    trace.stored_surface_fold_roots) &&
	trace.surface_fold_promoted_pairs <= 1 &&
	(!trace.surface_fold_promoted_pairs ||
	 trace.prepared_production_selected) &&
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


static bool
brep_trace_regular_event_stream_valid(
    const struct rt_brep_shot_trace &trace, size_t expected_segments)
{
    if (trace.surface_regular_orientation_attempts !=
	    trace.surface_krawczyk_boxes ||
	    trace.surface_regular_orientation_signed !=
	    trace.surface_regular_orientation_attempts ||
	    trace.surface_regular_orientation_uncertain ||
	    trace.surface_regular_orientation_failures ||
	    trace.physical_event_attempts != trace.stored_surface_boxes ||
	    trace.physical_event_regular != trace.stored_physical_events ||
	    trace.physical_event_clean_outside ||
	    trace.physical_event_near_trim ||
	    trace.physical_event_unresolved ||
	    trace.physical_event_direction_checks !=
	    trace.stored_surface_boxes ||
	    trace.physical_event_direction_mismatches ||
	    trace.physical_event_overflow ||
	    trace.physical_event_complete != 1 ||
	    trace.physical_event_state_failures ||
	    trace.physical_event_material_segments != expected_segments ||
	    trace.physical_event_subminimum_contacts ||
	    trace.physical_event_tolerance_ambiguous ||
	    trace.stored_physical_events != 2 * expected_segments)
	return false;

    bool used_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    bool used_root[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};
    for (size_t event_index = 0;
	    event_index < trace.stored_physical_events; ++event_index) {
	const struct rt_brep_trace_physical_event &event =
	    trace.physical_events[event_index];
	if (event.source_box >= trace.stored_surface_boxes ||
		event.source_root >= trace.stored_local_roots ||
		used_box[event.source_box] || used_root[event.source_root])
	    return false;
	used_box[event.source_box] = true;
	used_root[event.source_root] = true;
	const struct rt_brep_trace_surface_box &box =
	    trace.surface_boxes[event.source_box];
	const struct rt_brep_trace_local_root &root =
	    trace.local_roots[event.source_root];
	if (box.disposition != RT_BREP_TRACE_BOX_RESOLVED_REGULAR ||
		!box.determinant_sign ||
		event.certificate != RT_BREP_TRACE_EVENT_REGULAR_INTERIOR ||
		event.source_kind != RT_BREP_TRACE_EVENT_SOURCE_LOCAL_ROOT ||
		event.source_box_count != 1 || event.edge_index != -1 ||
		event.determinant_sign != box.determinant_sign ||
		event.face_index != box.face_index ||
		event.face_index != root.face_index ||
		event.span_index != box.span_index ||
		event.span_index != root.span_index ||
		event.hit_class != root.hit_class ||
		event.trim_status != root.trim_status ||
		event.adjacent_face_index != root.adjacent_face_index ||
		event.hit_class != 0 || event.direction != root.direction ||
		std::memcmp(&event.dist, &root.dist, sizeof(event.dist)) ||
		std::memcmp(&event.t_min, &box.t_min, sizeof(event.t_min)) ||
		std::memcmp(&event.t_max, &box.t_max, sizeof(event.t_max)) ||
		event.dist < event.t_min ||
		event.dist > event.t_max ||
		std::memcmp(event.uv, root.uv, sizeof(event.uv)) ||
		event.direction !=
		    (event_index % 2 ? RT_BREP_TRACE_LEAVING :
		    RT_BREP_TRACE_ENTERING) ||
		(event_index && trace.physical_events[event_index - 1].t_min >
		    event.t_min))
	    return false;
    }
    return true;
}


static bool
brep_trace_fold_event_stream_valid(const struct rt_brep_shot_trace &trace)
{
    if (trace.surface_regular_orientation_attempts ||
	    trace.surface_regular_orientation_signed ||
	    trace.surface_regular_orientation_uncertain ||
	    trace.surface_regular_orientation_failures ||
	    trace.physical_event_attempts != 2 ||
	    trace.physical_event_regular || trace.physical_event_boundary != 2 ||
	    trace.physical_event_clean_outside ||
	    trace.physical_event_near_trim || trace.physical_event_unresolved ||
	    trace.physical_event_direction_checks != 2 ||
	    trace.physical_event_direction_mismatches ||
	    trace.physical_event_overflow ||
	    trace.physical_event_complete != 1 ||
	    trace.physical_event_state_failures ||
	    trace.physical_event_material_segments != 1 ||
	    trace.physical_event_subminimum_contacts ||
	    trace.physical_event_tolerance_ambiguous ||
	    trace.stored_physical_events != 2)
	return false;

    bool used_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    bool used_root[RT_BREP_TRACE_MAX_FOLD_ROOTS] = {};
    for (size_t event_index = 0; event_index < 2; ++event_index) {
	const struct rt_brep_trace_physical_event &event =
	    trace.physical_events[event_index];
	if (event.source_box >= trace.stored_surface_boxes ||
		event.source_root >= trace.stored_surface_fold_roots ||
		used_box[event.source_box] || used_root[event.source_root])
	    return false;
	used_box[event.source_box] = true;
	used_root[event.source_root] = true;
	const struct rt_brep_trace_surface_box &box =
	    trace.surface_boxes[event.source_box];
	const struct rt_brep_trace_fold_root &root =
	    trace.surface_fold_roots_data[event.source_root];
	if (box.disposition != RT_BREP_TRACE_BOX_RESOLVED_BOUNDARY ||
		box.determinant_sign != root.determinant_sign ||
		event.certificate != RT_BREP_TRACE_EVENT_BOUNDARY_FOLD ||
		event.source_kind != RT_BREP_TRACE_EVENT_SOURCE_FOLD_ROOT ||
		event.source_box_count != 1 || event.edge_index != -1 ||
		event.determinant_sign != root.determinant_sign ||
		event.face_index != box.face_index ||
		event.face_index != root.face_index ||
		event.span_index != box.span_index ||
		event.span_index != root.span_index ||
		event.hit_class != root.hit_class ||
		event.trim_status != root.trim_status ||
		event.adjacent_face_index != root.adjacent_face_index ||
		event.hit_class != 0 ||
		event.direction != root.direction ||
		std::memcmp(&event.dist, &root.dist, sizeof(event.dist)) ||
		std::memcmp(&event.t_min, &root.t_min, sizeof(event.t_min)) ||
		std::memcmp(&event.t_max, &root.t_max, sizeof(event.t_max)) ||
		std::memcmp(event.uv, root.uv, sizeof(event.uv)) ||
		event.dist < event.t_min || event.dist > event.t_max ||
		event.direction !=
		    (event_index ? RT_BREP_TRACE_LEAVING :
		    RT_BREP_TRACE_ENTERING))
	    return false;
    }
    return trace.physical_events[0].t_max <
	trace.physical_events[1].t_min;
}


static bool
brep_trace_surface_boxes_connected_independently(
    const struct rt_brep_trace_surface_box &first,
    const struct rt_brep_trace_surface_box &second)
{
    if (first.face_index != second.face_index ||
	first.span_index != second.span_index)
	return false;
    for (int direction = 0; direction < 2; ++direction) {
	const double scale = std::max(1.0,
	    std::max(fabs(first.uv_min[direction]),
	    std::max(fabs(first.uv_max[direction]),
	    std::max(fabs(second.uv_min[direction]),
		fabs(second.uv_max[direction])))));
	const double tolerance = 256.0 * DBL_EPSILON * scale;
	if (first.uv_max[direction] <
		second.uv_min[direction] - tolerance ||
	    second.uv_max[direction] <
		first.uv_min[direction] - tolerance)
	    return false;
    }
    const double t_scale = std::max(1.0,
	std::max(fabs(first.t_min), std::max(fabs(first.t_max),
	std::max(fabs(second.t_min), fabs(second.t_max)))));
    const double t_tolerance = 256.0 * DBL_EPSILON * t_scale;
    return first.t_max >= second.t_min - t_tolerance &&
	second.t_max >= first.t_min - t_tolerance;
}


static bool
brep_trace_seam_event_stream_valid(
    const struct rt_brep_shot_trace &trace,
    const struct rt_brep_trace_edge *edge,
    const ON_Brep *brep, const partition_result &oracle,
    double model_tolerance, bool cobb_reparameterized = false,
    double endpoint_tolerance_scale = 2.0e-4)
{
    if (!edge || !brep || edge->edge_index < 0 ||
	    edge->edge_index >= brep->m_E.Count() ||
	    !edge->correspondence_supported || edge->correspondence_exhausted ||
	    !edge->discrepancy_endpoints_certified ||
	    !edge->discrepancy_bounded || edge->discrepancy_bound_exhausted ||
	    !edge->discrepancy_authorized ||
	    edge->discrepancy_proof_class != RT_BREP_SEAM_GAP_INSIDE ||
	    oracle.partitions != 1 || oracle.overflow ||
	    trace.physical_event_seam_attempts != 1 ||
	    trace.physical_event_seam != 2 ||
	    trace.physical_event_seam_certified != 1 ||
	    trace.physical_event_seam_failures ||
	    trace.physical_event_unresolved ||
	    trace.physical_event_direction_mismatches ||
	    trace.physical_event_overflow ||
	    trace.physical_event_complete != 1 ||
	    trace.physical_event_state_failures ||
	    trace.physical_event_material_segments != 1 ||
	    trace.physical_event_subminimum_contacts ||
	    trace.physical_event_tolerance_ambiguous ||
	    trace.physical_event_attempts != trace.stored_surface_boxes ||
	    trace.stored_physical_events != 2)
	return false;

    const struct rt_brep_trace_physical_event *existing = NULL;
    const struct rt_brep_trace_physical_event *continuation = NULL;
    bool contact_pair = false;
    for (size_t event_index = 0; event_index < 2; ++event_index) {
	const struct rt_brep_trace_physical_event &event =
	    trace.physical_events[event_index];
	if (event.direction != (event_index ? RT_BREP_TRACE_LEAVING :
		RT_BREP_TRACE_ENTERING) || event.edge_index != edge->edge_index ||
		event.dist < event.t_min || event.dist > event.t_max)
	    return false;
	if (event.certificate == RT_BREP_TRACE_EVENT_SEAM_EXISTING)
	    existing = &event;
	else if (event.certificate ==
		RT_BREP_TRACE_EVENT_SEAM_CONTINUATION)
	    continuation = &event;
	else if (event.certificate ==
		RT_BREP_TRACE_EVENT_SEAM_CONTACT_EXISTING) {
	    existing = &event;
	    contact_pair = true;
	} else if (event.certificate ==
		RT_BREP_TRACE_EVENT_SEAM_CONTACT_CONTINUATION) {
	    continuation = &event;
	    contact_pair = true;
	} else {
	    return false;
	}
    }
    const bool matching_certificates = existing && continuation &&
	((!contact_pair &&
	  existing->certificate == RT_BREP_TRACE_EVENT_SEAM_EXISTING &&
	  continuation->certificate ==
	    RT_BREP_TRACE_EVENT_SEAM_CONTINUATION) ||
	 (contact_pair && existing->certificate ==
	    RT_BREP_TRACE_EVENT_SEAM_CONTACT_EXISTING &&
	  continuation->certificate ==
	    RT_BREP_TRACE_EVENT_SEAM_CONTACT_CONTINUATION));
    const bool oblique_contact = contact_pair &&
	trace.physical_event_seam_oblique_pairs == 1;
    const bool oblique_evidence_valid = contact_pair ?
	((oblique_contact && trace.physical_event_seam_oblique_cells > 0 &&
	  trace.physical_event_seam_oblique_box_links ==
	    trace.physical_event_seam_contact_boxes &&
	  edge->frame_interval_supported && edge->frame_interval_cells > 0) ||
	 (!trace.physical_event_seam_oblique_pairs &&
	  !trace.physical_event_seam_oblique_cells &&
	  !trace.physical_event_seam_oblique_box_links)) :
	(!trace.physical_event_seam_oblique_pairs &&
	 !trace.physical_event_seam_oblique_cells &&
	 !trace.physical_event_seam_oblique_box_links);
    const bool source_union =
	trace.physical_event_seam_source_union_certified == 1;
    const bool source_union_evidence_valid =
	(!trace.physical_event_seam_source_union_certified &&
	 !trace.physical_event_seam_source_union_boxes &&
	 !trace.physical_event_seam_source_union_root_boxes) ||
	(source_union && existing &&
	 trace.physical_event_seam_source_union_boxes ==
	    existing->source_box_count &&
	 trace.physical_event_seam_source_union_root_boxes > 0 &&
	 trace.physical_event_seam_source_union_root_boxes <
	    trace.physical_event_seam_source_union_boxes);
    const double endpoint_tolerance = std::max(
	endpoint_tolerance_scale * model_tolerance,
	4096.0 * DBL_EPSILON * std::max(1.0,
	    fabs(trace.physical_events[1].dist)));
    if (!matching_certificates || !oblique_evidence_valid ||
	    !source_union_evidence_valid ||
	    trace.physical_event_direction_checks !=
		(contact_pair ? 4 : 2) ||
	    (contact_pair ?
	    (trace.physical_event_seam_contact_pairs != 1 ||
	     !trace.physical_event_seam_contact_boxes ||
	     trace.physical_event_seam_contact_roots != 2 ||
	     trace.physical_event_seam_contact_miss_roots > 1) :
	    (trace.physical_event_seam_contact_pairs ||
	     trace.physical_event_seam_contact_boxes ||
	     trace.physical_event_seam_contact_roots ||
	     trace.physical_event_seam_contact_miss_roots)) ||
	    existing->source_kind != RT_BREP_TRACE_EVENT_SOURCE_LOCAL_ROOT ||
	    existing->source_root >= trace.stored_local_roots ||
	    existing->source_box >= trace.stored_surface_boxes ||
	    !existing->source_box_count || existing->trim_status == 1 ||
	    (existing->hit_class != 0 && existing->hit_class != 2) ||
	    continuation->source_kind !=
		RT_BREP_TRACE_EVENT_SOURCE_SEAM_CONTINUATION ||
	    continuation->source_box != (size_t)-1 ||
	    continuation->source_box_count ||
	    continuation->source_root != (size_t)-1 ||
	    continuation->hit_class != 4 || continuation->trim_status != 1 ||
	    continuation->span_index < 0 ||
	    trace.physical_events[0].t_max >= trace.physical_events[1].t_min ||
	    oracle.intervals[0].in_dist <
		trace.physical_events[0].t_min - endpoint_tolerance ||
	    oracle.intervals[0].in_dist >
		trace.physical_events[0].t_max + endpoint_tolerance ||
	    oracle.intervals[0].out_dist <
		trace.physical_events[1].t_min - endpoint_tolerance ||
	    oracle.intervals[0].out_dist >
		trace.physical_events[1].t_max + endpoint_tolerance ||
	    fabs(trace.physical_events[0].dist -
		oracle.intervals[0].in_dist) > endpoint_tolerance ||
	    fabs(trace.physical_events[1].dist -
		oracle.intervals[0].out_dist) > endpoint_tolerance)
	return false;

    const struct rt_brep_trace_local_root &source_root =
	trace.local_roots[existing->source_root];
    if (existing->face_index != source_root.face_index ||
	    existing->span_index != source_root.span_index ||
	    existing->hit_class != source_root.hit_class ||
	    existing->trim_status != source_root.trim_status ||
	    existing->direction != source_root.direction ||
	    std::memcmp(&existing->dist, &source_root.dist,
		sizeof(existing->dist)) ||
	    std::memcmp(existing->uv, source_root.uv, sizeof(existing->uv)))
	return false;

    const double t_roundoff =
	128.0 * DBL_EPSILON * std::max(1.0, fabs(edge->ray_dist));
    const double source_t_tolerance = std::max(0.1 * model_tolerance,
	t_roundoff);
    const double witness_tolerance = std::max(model_tolerance, t_roundoff);
    const double uv_tolerance = 128.0 * DBL_EPSILON;
    size_t source_boxes = 0;
    size_t witness_boxes = 0;
    size_t contact_boxes = 0;
    double source_t_min = DBL_MAX;
    double source_t_max = -DBL_MAX;
    double contact_t_min = DBL_MAX;
    double contact_t_max = -DBL_MAX;
    bool root_owned[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};
    bool source_root_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    bool source_component_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    size_t source_root_boxes = 0;
    root_owned[existing->source_root] = true;
    size_t contact_roots[2] = {(size_t)-1, (size_t)-1};
    size_t contact_root_count = 0;
    bool contact_miss_root[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};
    size_t contact_miss_roots = 0;
    int contact_face = -1;
    int contact_span = -1;
    ON_2dPoint contact_edge_uv(0.0, 0.0);
    if (contact_pair) {
	for (size_t root_index = 0;
		root_index < trace.stored_local_roots; ++root_index) {
	    if (root_index == existing->source_root)
		continue;
	    if (contact_root_count >= 2)
		return false;
	    const struct rt_brep_trace_local_root &root =
		trace.local_roots[root_index];
	    const bool incident = root.face_index == edge->face_index[0] ||
		root.face_index == edge->face_index[1];
	    const bool trim_hit = root.trim_status != 1 &&
		(root.hit_class == 0 || root.hit_class == 2);
	    const bool trim_miss = root.trim_status == 1 &&
		(root.hit_class == 1 || root.hit_class == 3);
	    if (!incident || root.face_index == source_root.face_index ||
		    (!trim_hit && !trim_miss) ||
		    !std::isfinite(root.normal_dot) ||
		    fabs(root.normal_dot) <= BREP_GRAZING_DOT_TOL)
		return false;
	    contact_miss_root[root_index] = trim_miss;
	    contact_miss_roots += trim_miss ? 1 : 0;
	    contact_roots[contact_root_count++] = root_index;
	}
	if (contact_root_count != 2 || contact_miss_roots > 1 ||
		contact_miss_roots !=
		    trace.physical_event_seam_contact_miss_roots)
	    return false;
	if (trace.local_roots[contact_roots[1]].dist <
		trace.local_roots[contact_roots[0]].dist)
	    std::swap(contact_roots[0], contact_roots[1]);
	const struct rt_brep_trace_local_root &lower =
	    trace.local_roots[contact_roots[0]];
	const struct rt_brep_trace_local_root &upper =
	    trace.local_roots[contact_roots[1]];
	if (lower.face_index != upper.face_index ||
		lower.span_index != upper.span_index ||
		lower.direction != RT_BREP_TRACE_ENTERING ||
		upper.direction != RT_BREP_TRACE_LEAVING ||
		!(lower.dist < upper.dist) ||
		lower.dist <= trace.physical_events[0].dist ||
		upper.dist >= trace.physical_events[1].dist ||
		edge->closest_state != 1)
	    return false;
	contact_face = lower.face_index;
	contact_span = lower.span_index;
	if (contact_face < 0 || contact_face >= brep->m_F.Count())
	    return false;
	const ON_BrepEdge &model_edge = brep->m_E[edge->edge_index];
	if (!model_edge.Domain().IsIncreasing())
	    return false;
	double edge_fraction = model_edge.Domain().NormalizedParameterAt(
	    edge->edge_parameter);
	const ON_BrepTrim *contact_trim = NULL;
	int contact_side = -1;
	for (int side = 0; side < model_edge.m_ti.Count(); ++side) {
	    const int trim_index = model_edge.m_ti[side];
	    if (trim_index >= 0 && trim_index < brep->m_T.Count() &&
		    brep->m_T[trim_index].FaceIndexOf() == contact_face) {
		contact_trim = &brep->m_T[trim_index];
		contact_side = side;
		break;
	    }
	}
	if (!contact_trim || !contact_trim->Domain().IsIncreasing() ||
		contact_side < 0 || !std::isfinite(edge_fraction))
	    return false;
	double trim_fraction = contact_trim->m_bRev3d ?
	    1.0 - edge_fraction : edge_fraction;
	if (cobb_reparameterized) {
	    const double constant = contact_side ? 20.0 : 0.05;
	    trim_fraction = constant * trim_fraction /
		((constant - 1.0) * trim_fraction + 1.0);
	}
	const ON_3dPoint uv = contact_trim->PointAt(
	    contact_trim->Domain().ParameterAt(trim_fraction));
	if (!uv.IsValid())
	    return false;
	contact_edge_uv = ON_2dPoint(uv.x, uv.y);
	const ON_Surface *surface =
	    brep->m_F[contact_face].SurfaceOf();
	const ON_3dPoint edge_point = model_edge.PointAt(edge->edge_parameter);
	const ON_3dPoint trim_lift = surface ?
	    surface->PointAt(contact_edge_uv.x, contact_edge_uv.y) :
	    ON_3dPoint::UnsetPoint;
	if (!ON_IsValid(edge->model_tolerance) || edge->model_tolerance < 0.0)
	    return false;
	double closure_tolerance = edge->model_tolerance;
	if (ON_IsValid(edge->declared_tolerance) &&
		edge->declared_tolerance >= 0.0)
	    closure_tolerance = std::max(closure_tolerance,
		(double)edge->declared_tolerance);
	if (!surface || !edge_point.IsValid() || !trim_lift.IsValid() ||
		!std::isfinite(closure_tolerance) || closure_tolerance < 0.0)
	    return false;
	for (size_t root_offset = 0; root_offset < 2; ++root_offset) {
	    const size_t root_index = contact_roots[root_offset];
	    if (!contact_miss_root[root_index])
		continue;
	    const struct rt_brep_trace_local_root &root =
		trace.local_roots[root_index];
	    const ON_3dPoint root_point = surface->PointAt(root.uv[0],
		root.uv[1]);
	    if (!root_point.IsValid())
		return false;
	    const double coordinate_scale = std::max(1.0,
		std::max(fabs(root_point.x), std::max(fabs(root_point.y),
		    fabs(root_point.z))));
	    const double tube_roundoff = std::max(ON_ZERO_TOLERANCE,
		4096.0 * DBL_EPSILON * coordinate_scale);
	    if (root_point.DistanceTo(edge_point) >
			closure_tolerance + tube_roundoff ||
		    root_point.DistanceTo(trim_lift) >
			closure_tolerance + tube_roundoff)
		return false;
	}
    }
    for (size_t box_index = 0;
	    box_index < trace.stored_surface_boxes; ++box_index) {
	const struct rt_brep_trace_surface_box &box =
	    trace.surface_boxes[box_index];
	const bool source_root_match =
	    box.face_index == source_root.face_index &&
	    box.span_index == source_root.span_index &&
	    source_root.dist >= box.t_min - source_t_tolerance &&
	    source_root.dist <= box.t_max + source_t_tolerance &&
	    source_root.uv[0] >= box.uv_min[0] - uv_tolerance &&
	    source_root.uv[0] <= box.uv_max[0] + uv_tolerance &&
	    source_root.uv[1] >= box.uv_min[1] - uv_tolerance &&
	    source_root.uv[1] <= box.uv_max[1] + uv_tolerance;
	const bool source_component_match = source_union &&
	    !source_root_match && box.face_index == source_root.face_index &&
	    box.span_index == source_root.span_index &&
	    box.disposition == RT_BREP_TRACE_BOX_RESOLVED_BOUNDARY &&
	    !box.determinant_sign;
	const bool source = source_root_match || source_component_match;
	if (source) {
	    if (box.disposition != RT_BREP_TRACE_BOX_RESOLVED_BOUNDARY)
		return false;
	    if (source_union && source_root_match && !box.determinant_sign)
		return false;
	    source_root_box[box_index] = source_root_match;
	    source_component_box[box_index] = source_component_match;
	    source_root_boxes += source_root_match ? 1 : 0;
	    source_boxes++;
	    source_t_min = std::min(source_t_min, (double)box.t_min);
	    source_t_max = std::max(source_t_max, (double)box.t_max);
	    continue;
	}
	if (box.disposition != RT_BREP_TRACE_BOX_RESOLVED_CONTACT ||
		box.determinant_sign ||
		(!oblique_contact &&
		 (edge->ray_dist < box.t_min - witness_tolerance ||
		  edge->ray_dist > box.t_max + witness_tolerance)))
	    return false;
	bool box_has_witness = false;
	for (size_t root_index = 0;
		root_index < trace.stored_local_roots; ++root_index) {
	    if (root_index == existing->source_root)
		continue;
	    const struct rt_brep_trace_local_root &root =
		trace.local_roots[root_index];
	    const bool incident = root.face_index == edge->face_index[0] ||
		root.face_index == edge->face_index[1];
	    const bool in_box = root.face_index == box.face_index &&
		root.span_index == box.span_index &&
		root.dist >= box.t_min - source_t_tolerance &&
		root.dist <= box.t_max + source_t_tolerance &&
		root.uv[0] >= box.uv_min[0] - uv_tolerance &&
		root.uv[0] <= box.uv_max[0] + uv_tolerance &&
		root.uv[1] >= box.uv_min[1] - uv_tolerance &&
		root.uv[1] <= box.uv_max[1] + uv_tolerance;
	    const bool contact_root = contact_pair &&
		(root_index == contact_roots[0] ||
		 root_index == contact_roots[1]);
	    const bool qualifying_root = contact_pair ?
		(contact_root && root.face_index == contact_face &&
		 root.span_index == contact_span &&
		 ((root.trim_status != 1 &&
		   (root.hit_class == 0 || root.hit_class == 2)) ||
		  contact_miss_root[root_index]) &&
		 fabs(root.normal_dot) > BREP_GRAZING_DOT_TOL) :
		(fabs(root.normal_dot) <= BREP_GRAZING_DOT_TOL &&
		 fabs(root.dist - edge->ray_dist) <= witness_tolerance);
	    if (!incident || !qualifying_root || !in_box)
		continue;
	    box_has_witness = true;
	    root_owned[root_index] = true;
	}
	const bool certified_corridor_box = oblique_contact && contact_pair &&
	    box.face_index == contact_face && box.span_index == contact_span;
	if (!box_has_witness && !certified_corridor_box)
	    return false;
	if (contact_pair) {
	    if (!oblique_contact &&
		    (contact_edge_uv.x < box.uv_min[0] - uv_tolerance ||
		    contact_edge_uv.x > box.uv_max[0] + uv_tolerance ||
		    contact_edge_uv.y < box.uv_min[1] - uv_tolerance ||
		    contact_edge_uv.y > box.uv_max[1] + uv_tolerance))
		return false;
	    contact_boxes++;
	    contact_t_min = std::min(contact_t_min, (double)box.t_min);
	    contact_t_max = std::max(contact_t_max, (double)box.t_max);
	} else {
	    witness_boxes++;
	}
    }
    for (size_t root_index = 0;
	    root_index < trace.stored_local_roots; ++root_index) {
	if (!root_owned[root_index])
	    return false;
    }
    if (source_union) {
	bool connected[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
	for (size_t box_index = 0;
		box_index < trace.stored_surface_boxes; ++box_index)
	    connected[box_index] = source_root_box[box_index];
	bool changed = true;
	while (changed) {
	    changed = false;
	    for (size_t candidate_index = 0;
		    candidate_index < trace.stored_surface_boxes;
		    ++candidate_index) {
		if (!source_component_box[candidate_index] ||
			connected[candidate_index])
		    continue;
		for (size_t component_index = 0;
			component_index < trace.stored_surface_boxes;
			++component_index) {
		    if (!connected[component_index] ||
			    !brep_trace_surface_boxes_connected_independently(
				trace.surface_boxes[candidate_index],
				trace.surface_boxes[component_index]))
			continue;
		    connected[candidate_index] = true;
		    changed = true;
		    break;
		}
	    }
	}
	for (size_t box_index = 0;
		box_index < trace.stored_surface_boxes; ++box_index)
	    if (source_component_box[box_index] && !connected[box_index])
		return false;
    }
    return source_boxes == existing->source_box_count &&
	source_root_box[existing->source_box] &&
	source_root_boxes ==
	    (source_union ?
	     trace.physical_event_seam_source_union_root_boxes : source_boxes) &&
	trace.physical_event_seam_witness_boxes == witness_boxes &&
	trace.physical_event_seam_contact_boxes == contact_boxes &&
	trace.physical_event_near_trim == witness_boxes + contact_boxes &&
	!std::memcmp(&source_t_min, &existing->t_min, sizeof(source_t_min)) &&
	!std::memcmp(&source_t_max, &existing->t_max, sizeof(source_t_max)) &&
	(contact_pair ?
	 (contact_boxes > 0 &&
	  contact_t_min > trace.physical_events[0].dist &&
	  contact_t_max < trace.physical_events[1].dist &&
	  trace.physical_event_seam_contact_roots == contact_root_count &&
	  !trace.physical_event_seam_edge_only_candidates) :
	 ((!witness_boxes &&
	  trace.physical_event_seam_edge_only_candidates == 1) ||
	 (witness_boxes &&
	  trace.physical_event_seam_edge_only_candidates == 0)));
}


static bool
brep_trace_source_union_negative_controls(
    const struct rt_brep_shot_trace &trace,
    const struct rt_brep_trace_edge *edge, const ON_Brep *brep,
    const partition_result &oracle, double model_tolerance)
{
    if (!edge || !brep ||
	trace.physical_event_seam_source_union_certified != 1)
	return false;
    size_t existing_event = (size_t)-1;
    for (size_t event_index = 0;
	    event_index < trace.stored_physical_events; ++event_index) {
	if (trace.physical_events[event_index].certificate ==
		RT_BREP_TRACE_EVENT_SEAM_CONTACT_EXISTING) {
	    existing_event = event_index;
	    break;
	}
    }
    if (existing_event == (size_t)-1 ||
	trace.physical_events[existing_event].source_root >=
	    trace.stored_local_roots)
	return false;
    const struct rt_brep_trace_local_root &root =
	trace.local_roots[trace.physical_events[existing_event].source_root];
    size_t rootless_box = (size_t)-1;
    for (size_t box_index = 0;
	    box_index < trace.stored_surface_boxes; ++box_index) {
	const struct rt_brep_trace_surface_box &box =
	    trace.surface_boxes[box_index];
	if (box.face_index == root.face_index &&
		box.span_index == root.span_index &&
		box.disposition == RT_BREP_TRACE_BOX_RESOLVED_BOUNDARY &&
		!box.determinant_sign) {
	    rootless_box = box_index;
	    break;
	}
    }
    if (rootless_box == (size_t)-1)
	return false;

    struct rt_brep_shot_trace disconnected = trace;
    disconnected.surface_boxes[rootless_box].uv_min[0] += 0.25;
    disconnected.surface_boxes[rootless_box].uv_max[0] += 0.25;
    if (brep_trace_seam_event_stream_valid(disconnected, edge, brep, oracle,
	    model_tolerance))
	return false;

    struct rt_brep_shot_trace bad_ledger = trace;
    bad_ledger.physical_event_seam_source_union_root_boxes =
	bad_ledger.physical_event_seam_source_union_boxes;
    if (brep_trace_seam_event_stream_valid(bad_ledger, edge, brep, oracle,
	    model_tolerance))
	return false;

    struct rt_brep_shot_trace bad_representative = trace;
    bad_representative.physical_events[existing_event].source_box =
	rootless_box;
    return !brep_trace_seam_event_stream_valid(bad_representative, edge,
	brep, oracle, model_tolerance);
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
    size_t surface_regular_orientation_attempts = 0;
    size_t surface_regular_orientation_signed = 0;
    size_t surface_regular_orientation_uncertain = 0;
    size_t surface_regular_orientation_failures = 0;
    size_t physical_event_attempts = 0;
    size_t physical_event_regular = 0;
    size_t physical_event_boundary = 0;
    size_t physical_event_seam = 0;
    size_t physical_event_seam_attempts = 0;
    size_t physical_event_seam_root_candidates = 0;
    size_t physical_event_seam_closure_candidates = 0;
    size_t physical_event_seam_continuation_candidates = 0;
    size_t physical_event_seam_certified = 0;
    size_t physical_event_seam_failures = 0;
    size_t physical_event_seam_ownership_failures = 0;
    size_t physical_event_seam_witness_failures = 0;
    size_t physical_event_seam_edge_only_candidates = 0;
    size_t physical_event_seam_box_failures = 0;
    size_t physical_event_seam_root_coverage_failures = 0;
    size_t physical_event_seam_witness_boxes = 0;
    size_t physical_event_seam_witness_roots = 0;
    size_t physical_event_seam_contact_pairs = 0;
    size_t physical_event_seam_contact_boxes = 0;
    size_t physical_event_seam_contact_roots = 0;
    size_t physical_event_seam_contact_miss_roots = 0;
    size_t physical_event_seam_oblique_pairs = 0;
    size_t physical_event_seam_oblique_cells = 0;
    size_t physical_event_seam_oblique_box_links = 0;
    size_t physical_event_clean_outside = 0;
    size_t physical_event_near_trim = 0;
    size_t physical_event_unresolved = 0;
    size_t physical_event_direction_mismatches = 0;
    size_t physical_event_overflow = 0;
    size_t physical_event_complete = 0;
    size_t physical_event_state_failures = 0;
    size_t physical_event_material_segments = 0;
    size_t physical_event_subminimum_contacts = 0;
    size_t physical_event_tolerance_ambiguous = 0;
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
    summary.surface_regular_orientation_attempts +=
	trace.surface_regular_orientation_attempts;
    summary.surface_regular_orientation_signed +=
	trace.surface_regular_orientation_signed;
    summary.surface_regular_orientation_uncertain +=
	trace.surface_regular_orientation_uncertain;
    summary.surface_regular_orientation_failures +=
	trace.surface_regular_orientation_failures;
    summary.physical_event_attempts += trace.physical_event_attempts;
    summary.physical_event_regular += trace.physical_event_regular;
    summary.physical_event_boundary += trace.physical_event_boundary;
    summary.physical_event_seam += trace.physical_event_seam;
    summary.physical_event_seam_attempts +=
	trace.physical_event_seam_attempts;
    summary.physical_event_seam_root_candidates +=
	trace.physical_event_seam_root_candidates;
    summary.physical_event_seam_closure_candidates +=
	trace.physical_event_seam_closure_candidates;
    summary.physical_event_seam_continuation_candidates +=
	trace.physical_event_seam_continuation_candidates;
    summary.physical_event_seam_certified +=
	trace.physical_event_seam_certified;
    summary.physical_event_seam_failures +=
	trace.physical_event_seam_failures;
    summary.physical_event_seam_ownership_failures +=
	trace.physical_event_seam_ownership_failures;
    summary.physical_event_seam_witness_failures +=
	trace.physical_event_seam_witness_failures;
    summary.physical_event_seam_edge_only_candidates +=
	trace.physical_event_seam_edge_only_candidates;
    summary.physical_event_seam_box_failures +=
	trace.physical_event_seam_box_failures;
    summary.physical_event_seam_root_coverage_failures +=
	trace.physical_event_seam_root_coverage_failures;
    summary.physical_event_seam_witness_boxes +=
	trace.physical_event_seam_witness_boxes;
    summary.physical_event_seam_witness_roots +=
	trace.physical_event_seam_witness_roots;
    summary.physical_event_seam_contact_pairs +=
	trace.physical_event_seam_contact_pairs;
    summary.physical_event_seam_contact_boxes +=
	trace.physical_event_seam_contact_boxes;
    summary.physical_event_seam_contact_roots +=
	trace.physical_event_seam_contact_roots;
    summary.physical_event_seam_contact_miss_roots +=
	trace.physical_event_seam_contact_miss_roots;
    summary.physical_event_seam_oblique_pairs +=
	trace.physical_event_seam_oblique_pairs;
    summary.physical_event_seam_oblique_cells +=
	trace.physical_event_seam_oblique_cells;
    summary.physical_event_seam_oblique_box_links +=
	trace.physical_event_seam_oblique_box_links;
    summary.physical_event_clean_outside +=
	trace.physical_event_clean_outside;
    summary.physical_event_near_trim += trace.physical_event_near_trim;
    summary.physical_event_unresolved += trace.physical_event_unresolved;
    summary.physical_event_direction_mismatches +=
	trace.physical_event_direction_mismatches;
    summary.physical_event_overflow += trace.physical_event_overflow;
    summary.physical_event_complete += trace.physical_event_complete;
    summary.physical_event_state_failures +=
	trace.physical_event_state_failures;
    summary.physical_event_material_segments +=
	trace.physical_event_material_segments;
    summary.physical_event_subminimum_contacts +=
	trace.physical_event_subminimum_contacts;
    summary.physical_event_tolerance_ambiguous +=
	trace.physical_event_tolerance_ambiguous;
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
    std::printf("%s certified event ledger: orientation=%zu/%zu/%zu/%zu "
	"boxes=%zu regular/boundary/seam=%zu/%zu/%zu outside=%zu near=%zu "
	"unresolved=%zu "
	"direction-mismatch=%zu overflow=%zu complete=%zu state-failure=%zu "
	"segments/contact/ambiguous=%zu/%zu/%zu "
	"seam=attempt/root/closure/continuation/certified/failure/ownership:"
	"%zu/%zu/%zu/%zu/%zu/%zu/%zu "
	"ownership=witness/edge-only/box/root:%zu/%zu/%zu/%zu "
	"witness-box/root:%zu/%zu contact-pair/box/root/miss:%zu/%zu/%zu/%zu "
	"oblique-pair/cell/link:%zu/%zu/%zu\n",
	label,
	summary.surface_regular_orientation_signed,
	summary.surface_regular_orientation_attempts,
	summary.surface_regular_orientation_uncertain,
	summary.surface_regular_orientation_failures,
	summary.physical_event_attempts, summary.physical_event_regular,
	summary.physical_event_boundary, summary.physical_event_seam,
	summary.physical_event_clean_outside, summary.physical_event_near_trim,
	summary.physical_event_unresolved,
	summary.physical_event_direction_mismatches,
	summary.physical_event_overflow, summary.physical_event_complete,
	summary.physical_event_state_failures,
	summary.physical_event_material_segments,
	summary.physical_event_subminimum_contacts,
	summary.physical_event_tolerance_ambiguous,
	summary.physical_event_seam_attempts,
	summary.physical_event_seam_root_candidates,
	summary.physical_event_seam_closure_candidates,
	summary.physical_event_seam_continuation_candidates,
	summary.physical_event_seam_certified,
	summary.physical_event_seam_failures,
	summary.physical_event_seam_ownership_failures,
	summary.physical_event_seam_witness_failures,
	summary.physical_event_seam_edge_only_candidates,
	summary.physical_event_seam_box_failures,
	summary.physical_event_seam_root_coverage_failures,
	summary.physical_event_seam_witness_boxes,
	summary.physical_event_seam_witness_roots,
	summary.physical_event_seam_contact_pairs,
	summary.physical_event_seam_contact_boxes,
	summary.physical_event_seam_contact_roots,
	summary.physical_event_seam_contact_miss_roots,
	summary.physical_event_seam_oblique_pairs,
	summary.physical_event_seam_oblique_cells,
	summary.physical_event_seam_oblique_box_links);
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
check_grazing_local_root_certificate_trend(struct soltab *brep_stp,
    struct soltab *implicit_stp, struct soltab *solid_brep_stp,
    struct rt_i *rtip, struct resource *resp, double radius,
    const char *label, const ON_3dPoint &circle_center,
    ON_3dVector radial, ON_3dVector tangent, double normal_dot_scale,
    size_t allowed_subtolerance_miss_roots,
    bool strict_implicit_boundary_oracle,
    bool require_boundary_ambiguity, bool require_prepared_selection,
    bool require_terminal_expansion)
{
    /* This ratchets the local theorem for already found simple roots.  It
     * deliberately does not claim complete root search or grazing-event
     * publication authority. */
    const double chord_ratios[] = {100.0, 10.0, 2.0, 1.1, 1.0, 0.9,
	0.5, 0.1, 0.01};
    if (!brep_stp || !implicit_stp || !solid_brep_stp || !rtip || !resp ||
	    !(radius > 0.0) ||
	    !label || !radial.Unitize() || !tangent.Unitize() ||
	    fabs(radial * tangent) > 64.0 * DBL_EPSILON ||
	    !(normal_dot_scale > 0.0))
	return 1;

    int failures = 0;
    size_t resolved_pairs = 0;
    size_t subtolerance_pairs = 0;
    size_t subtolerance_unavailable = 0;
    size_t tangent_rejections = 0;
    size_t miss_side_cases = 0;
    size_t subtolerance_miss_roots = 0;
    size_t resolved_solid_matches = 0;
    size_t resolved_prepared_selections = 0;
    size_t terminal_expansion_ratchets = 0;
    size_t minimum_boundary_ambiguities = 0;
    size_t subtolerance_solid_hits = 0;
    size_t implicit_tangent_segments = 0;
    size_t implicit_outside_segments = 0;
    size_t maximum_attempts = 0;
    size_t maximum_high_water = 0;
    double minimum_separation = INFINITY;
    double maximum_radius_ratio = 0.0;
    double maximum_contraction = 0.0;
    double maximum_image = 0.0;
    double maximum_normal_error = 0.0;
    for (int reverse = 0; reverse <= 1; ++reverse) {
	double previous_separation = INFINITY;
	bool unavailable = false;
	for (size_t ratio_index = 0;
		ratio_index < sizeof(chord_ratios) /
		sizeof(chord_ratios[0]); ++ratio_index) {
	    const double chord_ratio = chord_ratios[ratio_index];
	    const double half_chord = 0.5 * chord_ratio *
		rtip->rti_tol.dist;
	    const double closest_distance = sqrt(std::max(0.0,
		radius * radius - half_chord * half_chord));
	    const double clearance = (half_chord * half_chord) /
		(radius + closest_distance);
	    const ON_3dPoint closest = circle_center +
		(radius - clearance) * radial;
	    const ON_3dVector direction = reverse ? -tangent : tangent;
	    const ON_3dPoint origin = closest - 2.0 * radius * direction;
	    sampled_ray ray;
	    VSET(ray.origin, origin.x, origin.y, origin.z);
	    VSET(ray.direction, direction.x, direction.y, direction.z);
	    struct rt_brep_shot_trace trace;
	    (void)shoot_brep_trace(brep_stp, rtip, resp, ray, trace);

	    const double expected[2] = {2.0 * radius - half_chord,
		2.0 * radius + half_chord};
	    const ray_result implicit_result = shoot_solid(implicit_stp, rtip,
		resp, ray.origin, ray.direction);
	    const ray_result solid_brep_result = shoot_solid(solid_brep_stp,
		rtip, resp, ray.origin, ray.direction);
	    const double implicit_error = implicit_result.segments == 1 ?
		std::max(fabs(implicit_result.in_dist - expected[0]),
		    fabs(implicit_result.out_dist - expected[1])) : INFINITY;
	    if (implicit_result.segments != 1 || implicit_error > 1.0e-7) {
		std::printf("FAIL: %s grazing implicit oracle "
		    "ratio/reverse=%.17g/%d segments=%d error=%.17g\n",
		    label, chord_ratio, reverse, implicit_result.segments,
		    implicit_error);
		failures++;
	    }
	    size_t root_index[2] = {SIZE_MAX, SIZE_MAX};
	    double root_error[2] = {INFINITY, INFINITY};
	    for (size_t local_index = 0;
		    local_index < trace.stored_local_roots; ++local_index) {
		for (int endpoint = 0; endpoint < 2; ++endpoint) {
		    const double error = fabs(
			trace.local_roots[local_index].dist - expected[endpoint]);
		    if (error < root_error[endpoint]) {
			root_error[endpoint] = error;
			root_index[endpoint] = local_index;
		    }
		}
	    }
	    const bool pair_found = root_index[0] != SIZE_MAX &&
		root_index[1] != SIZE_MAX && root_index[0] != root_index[1] &&
		root_error[0] <= 1.0e-7 && root_error[1] <= 1.0e-7;
	    const bool local_pair_required = chord_ratio >= 1.0;
	    const bool resolved = chord_ratio > 1.0;
	    const bool minimum_boundary = local_pair_required && !resolved;
	    const double solid_error = solid_brep_result.segments == 1 ?
		std::max(fabs(solid_brep_result.in_dist - expected[0]),
		    fabs(solid_brep_result.out_dist - expected[1])) : INFINITY;
	    if (resolved) {
		struct rt_brep_shot_trace solid_trace;
		(void)shoot_brep_trace(solid_brep_stp, rtip, resp, ray,
		    solid_trace);
		const bool prepared_selected =
		    solid_trace.prepared_production_selected == 1 &&
		    solid_trace.prepared_production_fallback ==
			RT_BREP_PREPARED_FALLBACK_NONE &&
		    solid_trace.final_segments == 1 &&
		    solid_trace.physical_event_complete == 1 &&
		    !solid_trace.physical_event_unresolved &&
		    !solid_trace.physical_event_state_failures;
		const bool terminal_case = require_terminal_expansion &&
		    (ratio_index == 2 || ratio_index == 3);
		const bool terminal_ratchet = !terminal_case ||
		    (solid_trace.surface_terminal_expansion_attempts > 0 &&
		     solid_trace.surface_terminal_expansion_available > 0 &&
		     solid_trace.surface_terminal_expansion_exclusions > 0 &&
		     solid_trace.surface_terminal_expansion_refinements > 0 &&
		     !solid_trace.surface_terminal_expansion_failures &&
		     !solid_trace.surface_terminal_expansion_budget_exhausted);
		if (solid_brep_result.segments != 1 ||
			solid_error > rtip->rti_tol.dist ||
			(require_prepared_selection && !prepared_selected) ||
			!terminal_ratchet) {
		    std::printf("FAIL: %s grazing solid BREP "
			"ratio/reverse=%.17g/%d segments=%d error=%.17g "
			"selected/fallback/complete=%zu/%d/%zu "
			"expansion=%zu/%zu/%zu refine/fail/budget=%zu/%zu/%zu\n",
			label, chord_ratio, reverse,
			solid_brep_result.segments, solid_error,
			solid_trace.prepared_production_selected,
			solid_trace.prepared_production_fallback,
			solid_trace.physical_event_complete,
			solid_trace.surface_terminal_expansion_attempts,
			solid_trace.surface_terminal_expansion_available,
			solid_trace.surface_terminal_expansion_exclusions,
			solid_trace.surface_terminal_expansion_refinements,
			solid_trace.surface_terminal_expansion_failures,
			solid_trace.surface_terminal_expansion_budget_exhausted);
		    report_grazing_trace(label, chord_ratio, reverse,
			solid_trace);
		    failures++;
		} else {
		    resolved_solid_matches++;
		    resolved_prepared_selections += prepared_selected ? 1 : 0;
		    if (terminal_case)
			terminal_expansion_ratchets++;
		}
	    } else if (minimum_boundary) {
		struct rt_brep_shot_trace solid_trace;
		(void)shoot_brep_trace(solid_brep_stp, rtip, resp, ray,
		    solid_trace);
		const double boundary_chord = solid_brep_result.segments == 1 ?
		    solid_brep_result.out_dist - solid_brep_result.in_dist : 0.0;
		const double boundary_slack = std::max(
		    0.01 * rtip->rti_tol.dist,
		    4096.0 * DBL_EPSILON * std::max(1.0, 2.0 * radius));
		const bool expected_boundary_fallback =
		    require_boundary_ambiguity ?
		    !solid_trace.prepared_production_selected &&
			solid_trace.prepared_production_fallback ==
			    RT_BREP_PREPARED_FALLBACK_EVENT_CLASS &&
			solid_trace.physical_event_tolerance_ambiguous == 1 &&
			solid_trace.physical_event_state_failures == 1 :
		    !solid_trace.prepared_production_selected &&
			solid_trace.prepared_production_fallback ==
			    RT_BREP_PREPARED_FALLBACK_UNCERTIFIED &&
			!solid_trace.physical_event_tolerance_ambiguous &&
			!solid_trace.physical_event_state_failures;
		if (solid_brep_result.segments > 1 ||
			solid_trace.final_segments !=
			    (size_t)solid_brep_result.segments ||
			(solid_brep_result.segments == 1 &&
			 (solid_error > boundary_slack ||
			  boundary_chord > rtip->rti_tol.dist +
			    2.0 * boundary_slack)) ||
			!expected_boundary_fallback) {
		    std::printf("FAIL: %s minimum-size boundary "
			"ratio/reverse=%.17g/%d segments/final=%d/%zu "
			"ambiguous/state/fallback=%zu/%zu/%d "
			"chord/error/T=%.17g/%.17g/%.17g selected=%zu\n", label,
			chord_ratio, reverse, solid_brep_result.segments,
			solid_trace.final_segments,
			solid_trace.physical_event_tolerance_ambiguous,
			solid_trace.physical_event_state_failures,
			solid_trace.prepared_production_fallback, boundary_chord,
			solid_error, rtip->rti_tol.dist,
			solid_trace.prepared_production_selected);
		    failures++;
		} else {
		    minimum_boundary_ambiguities++;
		}
	    } else if (solid_brep_result.segments == 1) {
		subtolerance_solid_hits++;
	    }
	    if (!pair_found) {
		if (local_pair_required) {
		    std::printf("FAIL: %s grazing local certificate pair "
			"ratio/reverse=%.17g/%d roots=%zu errors=%.17g/%.17g\n",
			label, chord_ratio, reverse, trace.stored_local_roots,
			root_error[0], root_error[1]);
		    failures++;
		} else {
		    unavailable = true;
		    subtolerance_unavailable++;
		}
		continue;
	    }

	    const struct rt_brep_trace_local_root &first =
		trace.local_roots[root_index[0]];
	    const struct rt_brep_trace_local_root &second =
		trace.local_roots[root_index[1]];
	    const double expected_normal_dot = normal_dot_scale *
		half_chord / radius;
	    const double normal_error = std::max(
		fabs(fabs(first.normal_dot) - expected_normal_dot),
		fabs(fabs(second.normal_dot) - expected_normal_dot));
	    maximum_normal_error = std::max(maximum_normal_error, normal_error);
	    if (!(first.normal_dot * second.normal_dot < 0.0) ||
		    normal_error > 1.0e-7) {
		std::printf("FAIL: %s grazing local normal trend "
		    "ratio/reverse=%.17g/%d dots=%.17g/%.17g expected=%.17g\n",
		    label, chord_ratio, reverse, first.normal_dot,
		    second.normal_dot,
		    expected_normal_dot);
		failures++;
	    }
	    struct rt_brep_local_root_test_result result[2] = {};
	    const struct rt_brep_trace_local_root *roots[2] = {&first, &second};
	    /* Get the common span normalization, then repeat with the production
	     * neighbor-exclusion cap. */
	    bool certified = first.face_index == second.face_index &&
		first.span_index == second.span_index;
	    for (int endpoint = 0; endpoint < 2 && certified; ++endpoint) {
		certified = _rt_brep_surface_local_root_test(brep_stp,
		    ray.origin, ray.direction, roots[endpoint]->face_index,
		    roots[endpoint]->span_index, roots[endpoint]->uv, 1.0,
		    &result[endpoint]) && result[endpoint].available &&
		    result[endpoint].certified &&
		    result[endpoint].model_image_available &&
		    result[endpoint].weight_minimum > 0.0;
	    }
	    double separation = INFINITY;
	    if (certified) {
		separation = 0.0;
		for (int direction_index = 0; direction_index < 2;
			direction_index++)
		    separation = std::max(separation, fabs(
			result[1].normalized_root[direction_index] -
			result[0].normalized_root[direction_index]));
		certified = separation > 0.0 && std::isfinite(separation);
	    }
	    const double radius_cap = certified ?
		std::nextafter(0.25 * separation, 0.0) : 0.0;
	    for (int endpoint = 0; endpoint < 2 && certified; ++endpoint) {
		struct rt_brep_local_root_test_result capped = {};
		certified = radius_cap > 0.0 &&
		    _rt_brep_surface_local_root_test(brep_stp, ray.origin,
			ray.direction, roots[endpoint]->face_index,
			roots[endpoint]->span_index, roots[endpoint]->uv,
			radius_cap, &capped) && capped.available &&
		    capped.certified && capped.model_image_available &&
		    capped.weight_minimum > 0.0 &&
		    capped.radius < 0.25 * separation;
		for (int direction_index = 0;
			direction_index < 2 && certified; ++direction_index)
		    certified = capped.normalized_root[direction_index] -
			capped.radius >= 0.0 &&
			capped.normalized_root[direction_index] +
			capped.radius <= 1.0;
		if (certified)
		    result[endpoint] = capped;
	    }
	    if (!certified) {
		if (local_pair_required ||
			(unavailable && chord_ratio >= 1.0)) {
		    std::printf("FAIL: %s grazing local certificate trend "
			"ratio/reverse=%.17g/%d face/span=%d/%d,%d/%d "
			"separation=%.17g cap=%.17g\n", label, chord_ratio,
			reverse,
			first.face_index, first.span_index, second.face_index,
			second.span_index, separation, radius_cap);
		    failures++;
		}
		unavailable = true;
		subtolerance_unavailable += local_pair_required ? 0 : 1;
		continue;
	    }
	    if (unavailable && local_pair_required) {
		std::printf("FAIL: %s grazing local certificate restarted "
		    "ratio/reverse=%.17g/%d\n", label, chord_ratio, reverse);
		failures++;
	    }
	    if (separation > previous_separation +
		    512.0 * DBL_EPSILON * std::max(1.0, previous_separation)) {
		std::printf("FAIL: %s grazing normalized root separation grew "
		    "ratio/reverse=%.17g/%d %.17g > %.17g\n", label,
		    chord_ratio, reverse, separation, previous_separation);
		failures++;
	    }
	    previous_separation = separation;
	    if (local_pair_required)
		resolved_pairs++;
	    else
		subtolerance_pairs++;
	    minimum_separation = std::min(minimum_separation, separation);
	    for (int endpoint = 0; endpoint < 2; ++endpoint) {
		maximum_attempts = std::max(maximum_attempts,
		    result[endpoint].attempts);
		maximum_high_water = std::max(maximum_high_water,
		    result[endpoint].expansion_high_water);
		maximum_radius_ratio = std::max(maximum_radius_ratio,
		    (double)result[endpoint].radius / separation);
		maximum_contraction = std::max(maximum_contraction,
		    (double)result[endpoint].contraction_bound);
		maximum_image = std::max(maximum_image,
		    (double)result[endpoint].model_image_displacement);
	    }
	}

	/* A double root must not pass the simple-root theorem. */
	const ON_3dPoint tangent_point = circle_center + radius * radial;
	const ON_3dVector direction = reverse ? -tangent : tangent;
	const ON_3dPoint origin = tangent_point - 2.0 * radius * direction;
	sampled_ray ray;
	VSET(ray.origin, origin.x, origin.y, origin.z);
	VSET(ray.direction, direction.x, direction.y, direction.z);
	struct rt_brep_shot_trace trace;
	(void)shoot_brep_trace(brep_stp, rtip, resp, ray, trace);
	const ray_result implicit_tangent = shoot_solid(implicit_stp, rtip,
	    resp, ray.origin, ray.direction);
	const ray_result solid_brep_tangent = shoot_solid(solid_brep_stp, rtip,
	    resp, ray.origin, ray.direction);
	bool tangent_certified = false;
	for (size_t root_index = 0; root_index < trace.stored_local_roots;
		++root_index) {
	    struct rt_brep_local_root_test_result result = {};
	    const struct rt_brep_trace_local_root &root =
		trace.local_roots[root_index];
	    if (_rt_brep_surface_local_root_test(brep_stp, ray.origin,
		    ray.direction, root.face_index, root.span_index, root.uv,
		    1.0e-3, &result) && result.available && result.certified)
		tangent_certified = true;
	}
	if (tangent_certified) {
	    std::printf("FAIL: %s exact tangent received a unique-root "
		"certificate reverse=%d\n", label, reverse);
	    failures++;
	} else if (!trace.stored_local_roots || trace.final_segments ||
		solid_brep_tangent.segments ||
		(strict_implicit_boundary_oracle &&
		 implicit_tangent.segments)) {
	    std::printf("FAIL: %s exact tangent root/final state=%zu/%zu "
		"implicit/solid=%d/%d reverse=%d\n", label,
		trace.stored_local_roots, trace.final_segments,
		implicit_tangent.segments, solid_brep_tangent.segments, reverse);
	    failures++;
	} else {
	    tangent_rejections++;
	    implicit_tangent_segments += implicit_tangent.segments;
	}

	/* Matching negative clearances must remain root-free. */
	for (size_t ratio_index = 0;
		ratio_index < sizeof(chord_ratios) /
		sizeof(chord_ratios[0]); ++ratio_index) {
	    const double half_chord = 0.5 * chord_ratios[ratio_index] *
		rtip->rti_tol.dist;
	    const double closest_distance = sqrt(std::max(0.0,
		radius * radius - half_chord * half_chord));
	    const double clearance = (half_chord * half_chord) /
		(radius + closest_distance);
	    const ON_3dPoint outside = circle_center +
		(radius + clearance) * radial;
	    const ON_3dPoint outside_origin =
		outside - 2.0 * radius * direction;
	    sampled_ray outside_ray;
	    VSET(outside_ray.origin, outside_origin.x, outside_origin.y,
		outside_origin.z);
	    VSET(outside_ray.direction, direction.x, direction.y, direction.z);
	    struct rt_brep_shot_trace outside_trace;
	    (void)shoot_brep_trace(brep_stp, rtip, resp, outside_ray,
		outside_trace);
	    const ray_result implicit_outside = shoot_solid(implicit_stp, rtip,
		resp, outside_ray.origin, outside_ray.direction);
	    const ray_result solid_brep_outside = shoot_solid(solid_brep_stp,
		rtip, resp, outside_ray.origin, outside_ray.direction);
	    const bool resolved = chord_ratios[ratio_index] >= 1.0;
	    if ((strict_implicit_boundary_oracle &&
		    implicit_outside.segments) ||
		    solid_brep_outside.segments ||
		    outside_trace.final_segments ||
		    (resolved && outside_trace.stored_local_roots)) {
		std::printf("FAIL: %s grazing local miss side ratio/reverse="
		    "%.17g/%d roots/final=%zu/%zu\n", label,
		    chord_ratios[ratio_index], reverse,
		    outside_trace.stored_local_roots,
		    outside_trace.final_segments);
		failures++;
	    } else {
		miss_side_cases++;
		implicit_outside_segments += implicit_outside.segments;
		if (!resolved)
		    subtolerance_miss_roots +=
			outside_trace.stored_local_roots;
	    }
	}
    }

    if (resolved_pairs != 10) {
	std::printf("FAIL: %s grazing local resolved certificates=%zu/10\n",
	    label, resolved_pairs);
	failures++;
    }

    if (resolved_solid_matches != 8) {
	std::printf("FAIL: %s grazing resolved solid matches=%zu/8\n",
	    label, resolved_solid_matches);
	failures++;
    }
    if (require_prepared_selection && resolved_prepared_selections != 8) {
	std::printf("FAIL: %s grazing prepared selections=%zu/8\n",
	    label, resolved_prepared_selections);
	failures++;
    }
    const size_t expected_terminal_ratchets =
	require_terminal_expansion ? 4 : 0;
    if (terminal_expansion_ratchets != expected_terminal_ratchets) {
	std::printf("FAIL: %s grazing terminal expansion ratchets=%zu/%zu\n",
	    label, terminal_expansion_ratchets, expected_terminal_ratchets);
	failures++;
    }
    if (minimum_boundary_ambiguities != 2) {
	std::printf("FAIL: %s grazing minimum-boundary ambiguities=%zu/2\n",
	    label, minimum_boundary_ambiguities);
	failures++;
    }
    if (subtolerance_pairs < 6 || subtolerance_unavailable > 2) {
	std::printf("FAIL: %s grazing local sub-tolerance capability="
	    "%zu/6 unavailable=%zu/2\n", label, subtolerance_pairs,
	    subtolerance_unavailable);
	failures++;
    }
    if (tangent_rejections != 2 || miss_side_cases != 18) {
	std::printf("FAIL: %s grazing local tangent/miss coverage="
	    "%zu/2 %zu/18\n", label, tangent_rejections, miss_side_cases);
	failures++;
    }
    if (subtolerance_miss_roots > allowed_subtolerance_miss_roots) {
	std::printf("FAIL: %s grazing local sub-tolerance miss roots="
	    "%zu/%zu\n", label, subtolerance_miss_roots,
	    allowed_subtolerance_miss_roots);
	failures++;
    }
    if (!failures)
	std::printf("%s grazing local certificates: PASS resolved=%zu "
	    "sub-T=%zu unavailable=%zu tangent/miss=%zu/%zu miss-roots=%zu "
	    "solid=%zu sub-T-solid=%zu "
	    "prepared=%zu expansion=%zu boundary=%zu "
	    "implicit tangent/outside=%zu/%zu "
	    "min-separation=%.3g "
	    "max-radius/separation=%.3g attempts=%zu contraction=%.3g "
	    "image=%.3g normal-error=%.3g high-water=%zu\n", label,
	    resolved_pairs, subtolerance_pairs, subtolerance_unavailable,
	    tangent_rejections, miss_side_cases, subtolerance_miss_roots,
	    resolved_solid_matches, subtolerance_solid_hits,
	    resolved_prepared_selections, terminal_expansion_ratchets,
	    minimum_boundary_ambiguities, implicit_tangent_segments,
	    implicit_outside_segments,
	    minimum_separation, maximum_radius_ratio,
	    maximum_attempts, maximum_contraction, maximum_image,
	    maximum_normal_error, maximum_high_water);
    return failures;
}


static int
check_torus_grazing_local_root_certificate_trend(struct rt_i *rtip,
    struct resource *resp)
{
    struct rt_tor_internal torus = {};
    torus.magic = RT_TOR_INTERNAL_MAGIC;
    VSET(torus.v, 0.0, 0.0, 0.0);
    VSET(torus.h, 0.0, 0.0, 1.0);
    torus.r_a = 12.0;
    torus.r_h = 3.0;
    torus.r_b = torus.r_a;
    VSET(torus.a, torus.r_a, 0.0, 0.0);
    VSET(torus.b, 0.0, torus.r_a, 0.0);
    struct rt_db_internal torus_intern;
    RT_DB_INTERNAL_INIT(&torus_intern);
    torus_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    torus_intern.idb_type = ID_TOR;
    torus_intern.idb_meth = &OBJ[ID_TOR];
    torus_intern.idb_ptr = &torus;

    ON_Brep *brep = ON_Brep::New();
    OBJ[ID_TOR].ft_brep(&brep, &torus_intern, &rtip->rti_tol);
    ON_NurbsSurface nurbs;
    const ON_Surface *converted_surface = brep && brep->m_F.Count() == 1 ?
	brep->m_F[0].SurfaceOf() : NULL;
    const int nurb_form_status = converted_surface ?
	converted_surface->GetNurbForm(nurbs) : 0;
    ON_Brep *solid_geometry = brep ? new ON_Brep(*brep) : NULL;
    delete brep;
    brep = ON_Brep::New();
    ON_BrepFace *face = nurb_form_status > 0 ? brep->NewFace(nurbs) : NULL;
    if (!face) {
	std::printf("FAIL: torus grazing BREP conversion\n");
	delete solid_geometry;
	delete brep;
	return 1;
    }
    brep->Standardize();
    brep->Compact();
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
	std::printf("FAIL: torus grazing BREP prep\n");
	delete brep_internal.brep;
	delete solid_geometry;
	return 1;
    }
    struct soltab *implicit_stp = prep_solid(rtip, &torus_intern, ID_TOR);
    struct rt_brep_internal solid_internal = {};
    solid_internal.magic = RT_BREP_INTERNAL_MAGIC;
    solid_internal.brep = solid_geometry;
    struct rt_db_internal solid_intern;
    RT_DB_INTERNAL_INIT(&solid_intern);
    solid_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    solid_intern.idb_type = ID_BREP;
    solid_intern.idb_meth = &OBJ[ID_BREP];
    solid_intern.idb_ptr = &solid_internal;
    struct soltab *solid_brep_stp = solid_geometry ?
	prep_solid(rtip, &solid_intern, ID_BREP) : NULL;
    if (!implicit_stp || !solid_brep_stp) {
	std::printf("FAIL: torus grazing oracle prep implicit/solid=%d/%d\n",
	    implicit_stp != NULL, solid_brep_stp != NULL);
	free_solid(solid_brep_stp);
	if (!solid_brep_stp)
	    delete solid_internal.brep;
	free_solid(implicit_stp);
	free_solid(brep_stp);
	return 1;
    }

    const double theta = 0.731;
    const double phi = 0.647;
    const ON_3dVector major_radial(cos(theta), sin(theta), 0.0);
    const ON_3dVector major_tangent(-sin(theta), cos(theta), 0.0);
    const ON_3dVector tube_radial = cos(phi) * major_radial +
	sin(phi) * ON_3dVector::ZAxis;
    const ON_3dVector tube_tangent = -sin(phi) * major_radial +
	cos(phi) * ON_3dVector::ZAxis;
    const ON_3dPoint tube_center = torus.r_a * major_radial;
    int failures = check_grazing_local_root_certificate_trend(brep_stp,
	implicit_stp, solid_brep_stp, rtip, resp, torus.r_h,
	"Torus tube-direction", tube_center, tube_radial, tube_tangent,
	1.0, 0, false, true, true, true);

    const double major_radius = torus.r_a + torus.r_h * cos(phi);
    const ON_3dPoint major_center(0.0, 0.0, torus.r_h * sin(phi));
    failures += check_grazing_local_root_certificate_trend(brep_stp,
	implicit_stp, solid_brep_stp, rtip, resp, major_radius,
	"Torus major-direction", major_center, major_radial, major_tangent,
	cos(phi), 2, false, true, true, false);

    free_solid(solid_brep_stp);
    free_solid(implicit_stp);
    free_solid(brep_stp);
    return failures;
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
		!brep_trace_regular_event_stream_valid(trace, 1) ||
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
    if (root_events.physical_event_complete !=
	    root_events.prepared_production_selected) {
	std::printf("FAIL: prepared sphere/event-ledger selection=%zu/%zu\n",
	    root_events.prepared_production_selected,
	    root_events.physical_event_complete);
	failures++;
    }
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
check_nested_brep_leaf_csg_fixture(const struct bn_tol *tol)
{
    prepared_model implicit_model;
    prepared_model brep_model;
    if (!prep_nested_sphere_csg_model(implicit_model, tol, false) ||
	    !prep_nested_sphere_csg_model(brep_model, tol, true)) {
	std::printf("FAIL: nested transformed sphere shell preparation\n");
	free_prepared_model(brep_model);
	free_prepared_model(implicit_model);
	return 1;
    }

    point_t bbox_min = {-13.0, -5.0, -5.0};
    point_t bbox_max = {13.0, 5.0, 5.0};
    const directed_partition_ray rays[] = {
	{"three intervals", {-20.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 3,
	    {7.0, 10.0, 14.0, 17.0, 23.0, 33.0}},
	{"three intervals reverse", {20.0, 0.0, 0.0},
	    {-1.0, 0.0, 0.0}, 3,
	    {7.0, 17.0, 23.0, 26.0, 30.0, 33.0}},
	{"left shell", {-8.0, -10.0, 0.0}, {0.0, 1.0, 0.0}, 2,
	    {5.0, 8.0, 12.0, 15.0}},
	{"right solid", {8.0, -10.0, 0.0}, {0.0, 1.0, 0.0}, 1,
	    {5.0, 15.0}}
    };
    const int failures = check_shared_crofton_fixture(
	"nested-transformed-sphere-shell", NULL, tol, bbox_min, bbox_max,
	216.0 * M_PI, (968.0 / 3.0) * M_PI, 8000, rays,
	sizeof(rays) / sizeof(rays[0]), &implicit_model, &brep_model);
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

    failures += check_nested_brep_leaf_csg_fixture(tol);

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
cobb_perturb_boundary_row(ON_Brep *brep, int face_index,
    int side_index, const ON_3dPoint &origin, double displacement,
    bool move_endpoints)
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
    const int first = move_endpoints ? 0 : 1;
    const int last = move_endpoints ? varying_count : varying_count - 1;
    for (int varying = first; varying < last; ++varying) {
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
cobb_perturb_boundary_interior(ON_Brep *brep, int face_index,
    int side_index, const ON_3dPoint &origin, double displacement)
{
    return cobb_perturb_boundary_row(brep, face_index, side_index, origin,
	displacement, false);
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


static bool
cobb_edge_endpoint_contract(const ON_Brep *brep, int edge_index,
    double model_tolerance, double &maximum_edge_vertex_gap,
    double &maximum_lift_vertex_gap)
{
    maximum_edge_vertex_gap = 0.0;
    maximum_lift_vertex_gap = 0.0;
    if (!brep || edge_index < 0 || edge_index >= brep->m_E.Count() ||
	    !ON_IsValid(model_tolerance) || model_tolerance < 0.0)
	return false;
    const ON_BrepEdge &edge = brep->m_E[edge_index];
    if (edge.m_ti.Count() != 2 || !edge.Domain().IsIncreasing())
	return false;
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
	const int vertex_index = edge.m_vi[endpoint];
	if (vertex_index < 0 || vertex_index >= brep->m_V.Count())
	    return false;
	const ON_BrepVertex &vertex = brep->m_V[vertex_index];
	const double vertex_tolerance =
	    ON_IsValid(vertex.m_tolerance) && vertex.m_tolerance >= 0.0 ?
	    std::max(model_tolerance, vertex.m_tolerance) : model_tolerance;
	const ON_3dPoint edge_point = edge.PointAt(endpoint ?
	    edge.Domain().Max() : edge.Domain().Min());
	if (!vertex.point.IsValid() || !edge_point.IsValid())
	    return false;
	const double edge_vertex_gap = vertex.point.DistanceTo(edge_point);
	maximum_edge_vertex_gap = std::max(maximum_edge_vertex_gap,
	    edge_vertex_gap);
	for (int side = 0; side < 2; ++side) {
	    const int trim_index = edge.m_ti[side];
	    if (trim_index < 0 || trim_index >= brep->m_T.Count())
		return false;
	    const ON_BrepTrim &trim = brep->m_T[trim_index];
	    const int trim_endpoint = trim.m_bRev3d ? 1 - endpoint : endpoint;
	    ON_3dPoint lift;
	    if (trim.m_vi[trim_endpoint] != vertex_index ||
		    !cobb_trim_lift(trim, endpoint, lift))
		return false;
	    const double lift_vertex_gap = vertex.point.DistanceTo(lift);
	    maximum_lift_vertex_gap = std::max(maximum_lift_vertex_gap,
		lift_vertex_gap);
	    const double coordinate_scale = std::max(1.0,
		std::max(fabs(vertex.point.x), std::max(fabs(vertex.point.y),
		std::max(fabs(vertex.point.z), std::max(fabs(edge_point.x),
		std::max(fabs(edge_point.y), std::max(fabs(edge_point.z),
		std::max(fabs(lift.x), std::max(fabs(lift.y),
		    fabs(lift.z))))))))));
	    const double roundoff = std::max(ON_ZERO_TOLERANCE,
		512.0 * DBL_EPSILON * coordinate_scale);
	    if (edge_vertex_gap > vertex_tolerance + roundoff ||
		    lift_vertex_gap > vertex_tolerance + roundoff)
		return false;
	}
    }
    return true;
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


static ON_Brep *
cobb_endpoint_moving_seam_variant(const ON_Brep *pristine,
    const ON_3dPoint &origin, double signed_target_gap,
    cobb_seam_frame &frame, double &measured_gap,
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
	if (!cobb_perturb_boundary_row(variant, frame.face_index,
		frame.side_index, origin, applied_displacement, true)) {
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

    /* Moving the endpoints also creates discrepancy on the two neighboring
     * edges.  Declare each affected face-edge envelope from its independently
     * measured lift gap so validity and prep authorization cannot depend on
     * the target edge alone. */
    const ON_BrepFace &face = variant->m_F[frame.face_index];
    if (face.m_li.Count() != 1) {
	delete variant;
	return NULL;
    }
    const ON_BrepLoop &loop = variant->m_L[face.m_li[0]];
    const ON_Surface *face_surface = face.SurfaceOf();
    if (!face_surface) {
	delete variant;
	return NULL;
    }
    for (int trim_slot = 0; trim_slot < loop.m_ti.Count(); ++trim_slot) {
	const int trim_index = loop.m_ti[trim_slot];
	if (trim_index < 0 || trim_index >= variant->m_T.Count()) {
	    delete variant;
	    return NULL;
	}
	const int edge_index = variant->m_T[trim_index].m_ei;
	const double discrepancy = cobb_seam_discrepancy(variant, edge_index);
	if (edge_index < 0 || edge_index >= variant->m_E.Count() ||
		!std::isfinite(discrepancy)) {
	    delete variant;
	    return NULL;
	}
	variant->m_E[edge_index].m_tolerance = discrepancy > 0.0 ?
	    1.01 * discrepancy : 0.0;
	const ON_BrepTrim &trim = variant->m_T[trim_index];
	if (!trim.Domain().IsIncreasing()) {
	    delete variant;
	    return NULL;
	}
	for (int endpoint = 0; endpoint < 2; ++endpoint) {
	    const int vertex_index = trim.m_vi[endpoint];
	    const double parameter = endpoint ?
		trim.Domain().Max() : trim.Domain().Min();
	    const ON_3dPoint uv = trim.PointAt(parameter);
	    const ON_3dPoint lift = face_surface->PointAt(uv.x, uv.y);
	    if (vertex_index < 0 || vertex_index >= variant->m_V.Count() ||
		    !uv.IsValid() || !lift.IsValid()) {
		delete variant;
		return NULL;
	    }
	    ON_BrepVertex &vertex = variant->m_V[vertex_index];
	    const double vertex_gap = vertex.point.DistanceTo(lift);
	    const double old_tolerance =
		ON_IsValid(vertex.m_tolerance) && vertex.m_tolerance >= 0.0 ?
		vertex.m_tolerance : 0.0;
	    vertex.m_tolerance = std::max(old_tolerance,
		vertex_gap > 0.0 ? 1.01 * vertex_gap : 0.0);
	}
    }
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


static sampled_ray
cobb_seam_oblique_ray(const cobb_seam_frame &frame,
    const ON_3dPoint &origin, double radius, double clearance,
    double tangent_component, bool reverse)
{
    const double closest_radius = radius - clearance;
    const ON_3dPoint closest = origin + closest_radius * frame.normal;
    ON_3dVector direction = frame.target_conormal +
	tangent_component * frame.tangent;
    direction.Unitize();
    if (reverse)
	direction.Reverse();
    const ON_3dPoint ray_origin = closest - 2.0 * radius * direction;
    sampled_ray ray;
    VSET(ray.origin, ray_origin.x, ray_origin.y, ray_origin.z);
    VSET(ray.direction, direction.x, direction.y, direction.z);
    return ray;
}


static bool
cobb_seam_endpoint_chord_ray(const cobb_seam_frame &frame,
    const ON_3dPoint &origin, double radius, double chord,
    int conormal_sign, bool reverse, sampled_ray &ray)
{
    if (!(radius > 0.0) || !(chord >= 0.0) || chord >= 2.0 * radius ||
	    (conormal_sign != -1 && conormal_sign != 1))
	return false;
    const ON_3dPoint seam_point = origin + radius * frame.normal;
    const double denominator = sqrt(4.0 * radius * radius - chord * chord);
    const double radial_component = chord / denominator;
    ON_3dVector direction =
	(double)conormal_sign * frame.target_conormal -
	radial_component * frame.normal;
    if (!direction.Unitize())
	return false;
    const ON_3dPoint other_point = seam_point + chord * direction;
    const ON_3dPoint ray_origin = reverse ?
	other_point + 2.0 * radius * direction :
	seam_point - 2.0 * radius * direction;
    if (reverse)
	direction.Reverse();
    const double coordinate_scale = std::max(1.0,
	std::max(radius, std::max(fabs(origin.x), std::max(fabs(origin.y),
	    fabs(origin.z)))));
    const double roundoff = std::max(ON_ZERO_TOLERANCE,
	4096.0 * DBL_EPSILON * coordinate_scale);
    if (fabs(other_point.DistanceTo(origin) - radius) > roundoff)
	return false;
    VSET(ray.origin, ray_origin.x, ray_origin.y, ray_origin.z);
    VSET(ray.direction, direction.x, direction.y, direction.z);
    return true;
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
brep_trace_vertex_event_owned(const struct rt_brep_shot_trace &trace,
    const ON_Brep &brep, const struct rt_brep_trace_physical_event &event)
{
    if (event.certificate != RT_BREP_TRACE_EVENT_VERTEX_FAN ||
	    event.source_kind != RT_BREP_TRACE_EVENT_SOURCE_VERTEX_FAN ||
	    event.vertex_index < 0 || event.vertex_index >= brep.m_V.Count() ||
	    event.source_root >= trace.stored_local_roots ||
	    event.source_box >= trace.stored_surface_boxes ||
	    !event.source_box_count)
	return false;
    const ON_BrepVertex &vertex = brep.m_V[event.vertex_index];
    std::vector<int> faces;
    for (int incident = 0; incident < vertex.m_ei.Count(); ++incident) {
	const int edge_index = vertex.m_ei[incident];
	if (edge_index < 0 || edge_index >= brep.m_E.Count())
	    return false;
	const ON_BrepEdge &edge = brep.m_E[edge_index];
	for (int trim_side = 0; trim_side < edge.m_ti.Count(); ++trim_side) {
	    const int trim_index = edge.m_ti[trim_side];
	    if (trim_index < 0 || trim_index >= brep.m_T.Count())
		return false;
	    const int face_index = brep.m_T[trim_index].FaceIndexOf();
	    if (std::find(faces.begin(), faces.end(), face_index) ==
		    faces.end())
		faces.push_back(face_index);
	}
    }
    if (faces.size() != (size_t)vertex.m_ei.Count())
	return false;

    const double t_tolerance = 1.0e-9 *
	std::max(1.0, fabs(event.dist));
    size_t roots = 0;
    size_t boxes = 0;
    for (size_t face = 0; face < faces.size(); ++face) {
	size_t face_roots = 0;
	size_t face_boxes = 0;
	for (size_t root_index = 0;
		root_index < trace.stored_local_roots; ++root_index) {
	    const struct rt_brep_trace_local_root &root =
		trace.local_roots[root_index];
	    if (root.face_index == faces[face] &&
		    fabs(root.dist - event.dist) <= t_tolerance) {
		if (root.direction != event.direction)
		    return false;
		face_roots++;
		roots++;
	    }
	}
	for (size_t box_index = 0;
		box_index < trace.stored_surface_boxes; ++box_index) {
	    const struct rt_brep_trace_surface_box &box =
		trace.surface_boxes[box_index];
	    if (box.face_index == faces[face] &&
		    event.dist >= box.t_min - t_tolerance &&
		    event.dist <= box.t_max + t_tolerance) {
		if (box.disposition != RT_BREP_TRACE_BOX_RESOLVED_BOUNDARY)
		    return false;
		face_boxes++;
		boxes++;
	    }
	}
	if (face_roots > 1 || (face_roots == 0) != (face_boxes == 0))
	    return false;
    }
    for (size_t root_index = 0; root_index < trace.stored_local_roots;
	    ++root_index) {
	const struct rt_brep_trace_local_root &root =
	    trace.local_roots[root_index];
	if (fabs(root.dist - event.dist) <= t_tolerance &&
		std::find(faces.begin(), faces.end(), root.face_index) ==
		faces.end())
	    return false;
    }
    return roots > 0 && boxes == event.source_box_count &&
	trace.local_roots[event.source_root].face_index == event.face_index;
}


static bool
brep_trace_edge_cluster_owned(const struct rt_brep_shot_trace &trace,
    const struct rt_brep_trace_edge &edge,
    const struct rt_brep_trace_physical_event *event,
    size_t &owned_roots, size_t &owned_boxes)
{
    owned_roots = 0;
    owned_boxes = 0;
    if (!edge.sector_valid || !edge.line_state_valid ||
	    edge.face_index[0] < 0 || edge.face_index[1] < 0)
	return false;
    const bool contact = edge.line_before_state == edge.line_after_state;
    const bool tolerance_transition = contact && event &&
	trace.physical_event_edge_tolerance_transitions == 1 &&
	trace.physical_event_edge_joint_components == 1 &&
	trace.physical_event_edge_contacts == 0;
    if (!tolerance_transition && contact != (event == NULL))
	return false;
    if (event && (event->certificate !=
		RT_BREP_TRACE_EVENT_MANIFOLD_EDGE ||
	    event->source_kind != RT_BREP_TRACE_EVENT_SOURCE_MANIFOLD_EDGE ||
	    event->edge_index != edge.edge_index || event->vertex_index != -1 ||
	    (!tolerance_transition &&
	     event->direction != edge.line_transition_direction) ||
	    !event->source_box_count ||
	    event->source_root >= trace.stored_local_roots ||
	    event->source_box >= trace.stored_surface_boxes))
	return false;
    const double t_tolerance = std::max(1.0e-9 *
	std::max(1.0, fabs(edge.ray_dist)),
	std::max(0.0, (double)edge.edge_tolerance));
    const double witness_tolerance = 1.0e-9 *
	std::max(1.0, fabs(edge.ray_dist));
    bool box_owned[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
    int face_roots[2] = {0, 0};
    int face_direction[2] = {-1, -1};
    for (size_t root_index = 0;
	    root_index < trace.stored_local_roots; ++root_index) {
	const struct rt_brep_trace_local_root &root =
	    trace.local_roots[root_index];
	if (fabs(root.dist - edge.ray_dist) > t_tolerance)
	    continue;
	int face_slot = root.face_index == edge.face_index[0] ? 0 :
	    (root.face_index == edge.face_index[1] ? 1 : -1);
	if (face_slot < 0)
	    continue;
	bool root_box[RT_BREP_TRACE_MAX_SURFACE_BOXES] = {};
	size_t root_boxes = 0;
	for (size_t box_index = 0;
		box_index < trace.stored_surface_boxes; ++box_index) {
	    const struct rt_brep_trace_surface_box &box =
		trace.surface_boxes[box_index];
	    if (box.face_index != root.face_index ||
		    box.span_index != root.span_index ||
		    root.dist < box.t_min - t_tolerance ||
		    root.dist > box.t_max + t_tolerance ||
		    edge.ray_dist < box.t_min - witness_tolerance ||
		    edge.ray_dist > box.t_max + witness_tolerance)
		continue;
	    const int expected_disposition = contact && !tolerance_transition ?
		RT_BREP_TRACE_BOX_RESOLVED_CONTACT :
		RT_BREP_TRACE_BOX_RESOLVED_BOUNDARY;
	    if (box_owned[box_index] ||
		    box.disposition != expected_disposition)
		return false;
	    root_box[box_index] = true;
	    root_boxes++;
	}
	if (!root_boxes)
	    continue;
	if (!std::isfinite(root.normal_dot) ||
		(event && root.direction != event->direction) ||
		(face_roots[face_slot] &&
		 root.direction != face_direction[face_slot]))
	    return false;
	face_direction[face_slot] = root.direction;
	face_roots[face_slot]++;
	for (size_t box_index = 0;
		box_index < trace.stored_surface_boxes; ++box_index) {
	    if (!root_box[box_index])
		continue;
	    box_owned[box_index] = true;
	    owned_boxes++;
	}
	owned_roots++;
    }
    if (!owned_roots || !owned_boxes)
	return false;
    for (size_t box_index = 0;
	    box_index < trace.stored_surface_boxes; ++box_index) {
	const struct rt_brep_trace_surface_box &box =
	    trace.surface_boxes[box_index];
	if (box.t_max < edge.ray_dist - witness_tolerance ||
		box.t_min > edge.ray_dist + witness_tolerance ||
		(box.face_index != edge.face_index[0] &&
		 box.face_index != edge.face_index[1]))
	    continue;
	if (!box_owned[box_index])
	    return false;
    }
    if (event && (event->source_box_count != owned_boxes ||
	    fabs(event->dist - edge.ray_dist) > t_tolerance ||
	    event->t_min > event->dist || event->dist > event->t_max))
	return false;
    return true;
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


static int
check_torus_status2_similarity(const struct bn_tol *tol)
{
    if (!tol || !(tol->dist > 0.0) || !std::isfinite(tol->dist))
	return 1;
    struct similarity_case {
	const char *name;
	double scale;
	ON_3dVector translation;
	ON_3dVector axis;
	double angle;
    } cases[] = {
	{"translated-rotated", 1.0, ON_3dVector(-31.25, 47.5, 103.75),
	    ON_3dVector(1.0, -2.0, 0.5), 0.731},
	{"small", 0.01, ON_3dVector(1.25, -2.5, 5.0),
	    ON_3dVector(-0.3, 1.0, 0.7), -1.113},
	{"large", 1.0e4, ON_3dVector(1.0e6, -2.0e6, 3.0e6),
	    ON_3dVector(2.0, 0.25, -1.0), 2.017}
    };
    const double chord_ratios[] = {2.0, 1.1};
    const double theta = 0.731;
    const double phi = 0.647;
    const ON_3dVector base_major_radial(cos(theta), sin(theta), 0.0);
    const ON_3dVector base_tube_radial =
	cos(phi) * base_major_radial + sin(phi) * ON_3dVector::ZAxis;
    const ON_3dVector base_tube_tangent =
	-sin(phi) * base_major_radial + cos(phi) * ON_3dVector::ZAxis;
    const ON_3dPoint base_tube_center = 12.0 * base_major_radial;
    int failures = 0;
    size_t rays = 0;
    size_t selected = 0;
    size_t expansion_ratchets = 0;
    size_t implicit_artifacts = 0;
    size_t maximum_refinements = 0;
    size_t maximum_high_water = 0;
    double maximum_implicit_error = 0.0;
    double maximum_brep_error = 0.0;

    for (size_t case_index = 0;
	    case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
	const similarity_case &test = cases[case_index];
	const ON_Xform xform = cobb_axis_angle_similarity_transform(
	    test.scale, test.translation, test.axis, test.angle);
	const ON_3dPoint center = cobb_transform_point(xform,
	    ON_3dPoint::Origin);
	ON_3dVector h = cobb_transform_vector(xform,
	    ON_3dVector::ZAxis);
	const ON_3dVector a = cobb_transform_vector(xform,
	    ON_3dVector(12.0, 0.0, 0.0));
	const ON_3dVector b = cobb_transform_vector(xform,
	    ON_3dVector(0.0, 12.0, 0.0));
	if (!h.Unitize()) {
	    failures++;
	    continue;
	}
	struct rt_tor_internal torus = {};
	torus.magic = RT_TOR_INTERNAL_MAGIC;
	VSET(torus.v, center.x, center.y, center.z);
	VSET(torus.h, h.x, h.y, h.z);
	torus.r_a = 12.0 * test.scale;
	torus.r_h = 3.0 * test.scale;
	torus.r_b = torus.r_a;
	VSET(torus.a, a.x, a.y, a.z);
	VSET(torus.b, b.x, b.y, b.z);
	struct rt_db_internal torus_intern;
	RT_DB_INTERNAL_INIT(&torus_intern);
	torus_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	torus_intern.idb_type = ID_TOR;
	torus_intern.idb_meth = &OBJ[ID_TOR];
	torus_intern.idb_ptr = &torus;

	struct bn_tol case_tol = *tol;
	case_tol.dist = tol->dist * test.scale;
	case_tol.dist_sq = case_tol.dist * case_tol.dist;
	struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
	if (!rtip) {
	    std::printf("FAIL: status-2 torus %s rt_i construction\n",
		test.name);
	    failures++;
	    continue;
	}
	rtip->rti_tol = case_tol;
	struct resource resource = {};
	rt_init_resource(&resource, 0, rtip);
	struct soltab *implicit_stp = prep_solid(rtip, &torus_intern, ID_TOR);
	ON_Brep *brep = ON_Brep::New();
	OBJ[ID_TOR].ft_brep(&brep, &torus_intern, &case_tol);
	ON_NurbsSurface nurbs;
	const ON_Surface *surface = brep && brep->m_F.Count() == 1 ?
	    brep->m_F[0].SurfaceOf() : NULL;
	const int nurb_form_status = surface ?
	    surface->GetNurbForm(nurbs) : 0;
	struct rt_brep_internal brep_internal = {};
	brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
	brep_internal.brep = brep;
	struct rt_db_internal brep_intern;
	RT_DB_INTERNAL_INIT(&brep_intern);
	brep_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	brep_intern.idb_type = ID_BREP;
	brep_intern.idb_meth = &OBJ[ID_BREP];
	brep_intern.idb_ptr = &brep_internal;
	struct soltab *brep_stp = nurb_form_status == 2 ?
	    prep_solid(rtip, &brep_intern, ID_BREP) : NULL;
	if (!implicit_stp || !brep_stp) {
	    std::printf("FAIL: status-2 torus %s prep/status=%d/%d/%d\n",
		test.name, implicit_stp != NULL, brep_stp != NULL,
		nurb_form_status);
	    failures++;
	} else {
	    const double radius = 3.0 * test.scale;
	    const ON_3dPoint tube_center = cobb_transform_point(xform,
		base_tube_center);
	    ON_3dVector tube_radial = cobb_transform_vector(xform,
		base_tube_radial);
	    ON_3dVector tube_tangent = cobb_transform_vector(xform,
		base_tube_tangent);
	    if (!tube_radial.Unitize() || !tube_tangent.Unitize()) {
		failures++;
	    } else {
		for (size_t ratio_index = 0; ratio_index <
			sizeof(chord_ratios) / sizeof(chord_ratios[0]);
			++ratio_index) {
		    const double chord_ratio = chord_ratios[ratio_index];
		    const double half_chord = 0.5 * chord_ratio * case_tol.dist;
		    const double closest_distance = sqrt(std::max(0.0,
			radius * radius - half_chord * half_chord));
		    const double clearance = (half_chord * half_chord) /
			(radius + closest_distance);
		    const ON_3dPoint closest = tube_center +
			(radius - clearance) * tube_radial;
		    for (int reverse = 0; reverse <= 1; ++reverse) {
			const ON_3dVector direction = reverse ?
			    -tube_tangent : tube_tangent;
			const ON_3dPoint origin = closest -
			    2.0 * radius * direction;
			sampled_ray ray;
			VSET(ray.origin, origin.x, origin.y, origin.z);
			VSET(ray.direction, direction.x, direction.y,
			    direction.z);
			const double expected[2] = {
			    2.0 * radius - half_chord,
			    2.0 * radius + half_chord
			};
			const ray_result implicit_result = shoot_solid(
			    implicit_stp, rtip, &resource, ray.origin,
			    ray.direction);
			const ray_result brep_result = shoot_solid(brep_stp,
			    rtip, &resource, ray.origin, ray.direction);
			struct rt_brep_shot_trace trace;
			(void)shoot_brep_trace(brep_stp, rtip, &resource, ray,
			    trace);
			const double implicit_error =
			    implicit_result.segments == 1 ? std::max(
			    fabs(implicit_result.in_dist - expected[0]),
			    fabs(implicit_result.out_dist - expected[1])) :
			    INFINITY;
			const double brep_error = brep_result.segments == 1 ?
			    std::max(fabs(brep_result.in_dist - expected[0]),
			    fabs(brep_result.out_dist - expected[1])) : INFINITY;
			const double coordinate_scale = std::max(radius,
			    std::max(fabs(origin.x), std::max(fabs(origin.y),
			    fabs(origin.z))));
			const double error_limit = std::max(0.01 * case_tol.dist,
			    32768.0 * DBL_EPSILON * coordinate_scale);
			if (implicit_result.segments == 1 &&
				implicit_error <= error_limit)
			    maximum_implicit_error = std::max(
				maximum_implicit_error,
				implicit_error / test.scale);
			else
			    implicit_artifacts++;
			maximum_brep_error = std::max(maximum_brep_error,
			    brep_error / test.scale);
			maximum_refinements = std::max(maximum_refinements,
			    trace.surface_terminal_expansion_refinements);
			maximum_high_water = std::max(maximum_high_water,
			    trace.surface_terminal_expansion_high_water);
			const bool good = brep_result.segments == 1 &&
			    brep_error <= case_tol.dist &&
			    trace.reparameterized_surface_faces == 1 &&
			    trace.prepared_production_selected == 1 &&
			    trace.prepared_production_fallback ==
				RT_BREP_PREPARED_FALLBACK_NONE &&
			    trace.prepared_production_hits == 2 &&
			    trace.final_segments == 1 &&
			    trace.physical_event_complete == 1 &&
			    !trace.physical_event_unresolved &&
			    !trace.physical_event_state_failures &&
			    trace.surface_terminal_expansion_attempts > 0 &&
			    trace.surface_terminal_expansion_available > 0 &&
			    trace.surface_terminal_expansion_exclusions > 0 &&
			    trace.surface_terminal_expansion_refinements > 0 &&
			    !trace.surface_terminal_expansion_failures &&
			    !trace.surface_terminal_expansion_budget_exhausted &&
			    !trace.surface_fold_root_failures &&
			    !trace.local_trim_failures &&
			    brep_trace_fixed_workspaces_match(trace);
			rays++;
			if (good) {
			    selected++;
			    expansion_ratchets++;
			} else {
			    std::printf("FAIL: status-2 torus similarity %s "
				"ratio/reverse=%.3g/%d segments=%d/%d "
				"errors=%.3g/%.3g limit=%.3g "
				"selected/fallback/complete=%zu/%d/%zu "
				"expansion=%zu/%zu/%zu/%zu/%zu/%zu\n",
				test.name, chord_ratio, reverse,
				implicit_result.segments, brep_result.segments,
				implicit_error, brep_error, error_limit,
				trace.prepared_production_selected,
				trace.prepared_production_fallback,
				trace.physical_event_complete,
				trace.surface_terminal_expansion_attempts,
				trace.surface_terminal_expansion_available,
				trace.surface_terminal_expansion_exclusions,
				trace.surface_terminal_expansion_refinements,
				trace.surface_terminal_expansion_failures,
				trace.surface_terminal_expansion_budget_exhausted);
			    report_grazing_trace(test.name, chord_ratio, reverse,
				trace);
			    failures++;
			}
		    }
		}
	    }
	}

	free_solid(brep_stp);
	if (!brep_stp && brep_internal.brep)
	    delete brep_internal.brep;
	free_solid(implicit_stp);
	rt_clean_resource_basic(rtip, &resource);
	BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
	rt_i_destroy(rtip);
    }

    if (rays != 12 || selected != rays || expansion_ratchets != rays) {
	std::printf("FAIL: status-2 torus similarity coverage="
	    "%zu/%zu/%zu expected=12\n", rays, selected,
	    expansion_ratchets);
	failures++;
    }
    if (!failures)
	std::printf("Status-2 torus similarity invariance: PASS "
	    "rays=%zu selected=%zu expansion=%zu implicit-artifacts=%zu "
	    "max-refine/high=%zu/%zu max-errors=%.3g/%.3g\n", rays,
	    selected, expansion_ratchets, implicit_artifacts,
	    maximum_refinements, maximum_high_water, maximum_implicit_error,
	    maximum_brep_error);
    return failures;
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
    if (!first.mantissa) {
	result = second;
	return exact_dyadic_normalize(result);
    }
    if (!second.mantissa) {
	result = first;
	return exact_dyadic_normalize(result);
    }
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
    if (!input || !left || !right || order < 2 ||
	    order > RT_BREP_DETERMINANT_TEST_MAX_ORDER)
	return false;
    exact_dyadic complement;
    if (!exact_dyadic_subtract({1, 0}, parameter, complement))
	return false;
    exact_dyadic work[RT_BREP_DETERMINANT_TEST_MAX_ORDER];
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
    exact_dyadic first[RT_BREP_DETERMINANT_TEST_MAX_ORDER];
    exact_dyadic second[RT_BREP_DETERMINANT_TEST_MAX_ORDER];
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


static uint64_t
exact_binomial_coefficient(int degree, int index)
{
    if (index < 0 || index > degree)
	return 0;
    index = std::min(index, degree - index);
    uint64_t result = 1;
    for (int i = 1; i <= index; ++i)
	result = result * (uint64_t)(degree - index + i) / (uint64_t)i;
    return result;
}


static bool
exact_integer_surface_derivative(const int64_t *input, int u_order,
    int v_order, int direction, int64_t *output, int output_order[2])
{
    if (!input || !output || !output_order ||
	    (direction != 0 && direction != 1) ||
	    u_order < 2 || v_order < 2 || u_order > 16 || v_order > 16)
	return false;
    output_order[0] = u_order - (direction == 0 ? 1 : 0);
    output_order[1] = v_order - (direction == 1 ? 1 : 0);
    const int degree = direction == 0 ? u_order - 1 : v_order - 1;
    for (int i = 0; i < output_order[0]; ++i) {
	for (int j = 0; j < output_order[1]; ++j) {
	    const size_t previous = (size_t)i * v_order + j;
	    const size_t next = direction == 0 ?
		(size_t)(i + 1) * v_order + j :
		(size_t)i * v_order + j + 1;
	    output[(size_t)i * output_order[1] + j] =
		degree * (input[next] - input[previous]);
	}
    }
    return true;
}


static bool
exact_integer_surface_product_coefficient(const int64_t *first,
    const int first_order[2], const int64_t *second,
    const int second_order[2], int output_i, int output_j,
    int64_t &numerator, uint64_t &denominator)
{
    if (!first || !first_order || !second || !second_order ||
	    first_order[0] < 1 || first_order[1] < 1 ||
	    second_order[0] < 1 || second_order[1] < 1)
	return false;
    const int first_degree[2] = {
	first_order[0] - 1, first_order[1] - 1
    };
    const int second_degree[2] = {
	second_order[0] - 1, second_order[1] - 1
    };
    const uint64_t u_denominator = exact_binomial_coefficient(
	first_degree[0] + second_degree[0], output_i);
    const uint64_t v_denominator = exact_binomial_coefficient(
	first_degree[1] + second_degree[1], output_j);
    if (!u_denominator || !v_denominator ||
	    u_denominator > UINT64_MAX / v_denominator)
	return false;
    denominator = u_denominator * v_denominator;
    numerator = 0;
    const int first_u_minimum = std::max(0,
	output_i - second_degree[0]);
    const int first_u_maximum = std::min(first_degree[0], output_i);
    const int first_v_minimum = std::max(0,
	output_j - second_degree[1]);
    const int first_v_maximum = std::min(first_degree[1], output_j);
    for (int i = first_u_minimum; i <= first_u_maximum; ++i) {
	const int second_i = output_i - i;
	const uint64_t u_weight =
	    exact_binomial_coefficient(first_degree[0], i) *
	    exact_binomial_coefficient(second_degree[0], second_i);
	for (int j = first_v_minimum; j <= first_v_maximum; ++j) {
	    const int second_j = output_j - j;
	    const uint64_t v_weight =
		exact_binomial_coefficient(first_degree[1], j) *
		exact_binomial_coefficient(second_degree[1], second_j);
	    if (u_weight > UINT64_MAX / v_weight)
		return false;
	    const uint64_t weight = u_weight * v_weight;
	    const int64_t value =
		first[(size_t)i * first_order[1] + j] *
		second[(size_t)second_i * second_order[1] + second_j];
	    if (value && weight > (uint64_t)INT64_MAX /
		    (uint64_t)llabs(value))
		return false;
	    const int64_t term = value * (int64_t)weight;
	    if ((term > 0 && numerator > INT64_MAX - term) ||
		    (term < 0 && numerator < INT64_MIN - term))
		return false;
	    numerator += term;
	}
    }
    return true;
}


struct exact_integer_interval {
    int64_t minimum;
    int64_t maximum;
};


static bool
exact_integer_interval_surface_derivative(const exact_integer_interval *input,
    int u_order, int v_order, int direction, exact_integer_interval *output,
    int output_order[2])
{
    if (!input || !output || !output_order ||
	    (direction != 0 && direction != 1) ||
	    u_order < 2 || v_order < 2 || u_order > 16 || v_order > 16)
	return false;
    output_order[0] = u_order - (direction == 0 ? 1 : 0);
    output_order[1] = v_order - (direction == 1 ? 1 : 0);
    const int degree = direction == 0 ? u_order - 1 : v_order - 1;
    for (int i = 0; i < output_order[0]; ++i) {
	for (int j = 0; j < output_order[1]; ++j) {
	    const size_t previous = (size_t)i * v_order + j;
	    const size_t next = direction == 0 ?
		(size_t)(i + 1) * v_order + j :
		(size_t)i * v_order + j + 1;
	    exact_integer_interval &value =
		output[(size_t)i * output_order[1] + j];
	    value.minimum = degree *
		(input[next].minimum - input[previous].maximum);
	    value.maximum = degree *
		(input[next].maximum - input[previous].minimum);
	}
    }
    return true;
}


static bool
exact_integer_interval_surface_product_coefficient(
    const exact_integer_interval *first, const int first_order[2],
    const exact_integer_interval *second, const int second_order[2],
    int output_i, int output_j, int64_t &minimum_numerator,
    int64_t &maximum_numerator, uint64_t &denominator)
{
    if (!first || !first_order || !second || !second_order ||
	    first_order[0] < 1 || first_order[1] < 1 ||
	    second_order[0] < 1 || second_order[1] < 1)
	return false;
    const int first_degree[2] = {
	first_order[0] - 1, first_order[1] - 1
    };
    const int second_degree[2] = {
	second_order[0] - 1, second_order[1] - 1
    };
    const uint64_t u_denominator = exact_binomial_coefficient(
	first_degree[0] + second_degree[0], output_i);
    const uint64_t v_denominator = exact_binomial_coefficient(
	first_degree[1] + second_degree[1], output_j);
    if (!u_denominator || !v_denominator ||
	    u_denominator > UINT64_MAX / v_denominator)
	return false;
    denominator = u_denominator * v_denominator;
    minimum_numerator = 0;
    maximum_numerator = 0;
    const int first_u_minimum = std::max(0,
	output_i - second_degree[0]);
    const int first_u_maximum = std::min(first_degree[0], output_i);
    const int first_v_minimum = std::max(0,
	output_j - second_degree[1]);
    const int first_v_maximum = std::min(first_degree[1], output_j);
    for (int i = first_u_minimum; i <= first_u_maximum; ++i) {
	const int second_i = output_i - i;
	const uint64_t u_weight =
	    exact_binomial_coefficient(first_degree[0], i) *
	    exact_binomial_coefficient(second_degree[0], second_i);
	for (int j = first_v_minimum; j <= first_v_maximum; ++j) {
	    const int second_j = output_j - j;
	    const uint64_t v_weight =
		exact_binomial_coefficient(first_degree[1], j) *
		exact_binomial_coefficient(second_degree[1], second_j);
	    if (u_weight > UINT64_MAX / v_weight)
		return false;
	    const uint64_t weight = u_weight * v_weight;
	    const exact_integer_interval &a =
		first[(size_t)i * first_order[1] + j];
	    const exact_integer_interval &b =
		second[(size_t)second_i * second_order[1] + second_j];
	    const int64_t products[4] = {
		a.minimum * b.minimum, a.minimum * b.maximum,
		a.maximum * b.minimum, a.maximum * b.maximum
	    };
	    const int64_t minimum = *std::min_element(products, products + 4);
	    const int64_t maximum = *std::max_element(products, products + 4);
	    if ((minimum && weight > (uint64_t)INT64_MAX /
		    (uint64_t)llabs(minimum)) ||
		    (maximum && weight > (uint64_t)INT64_MAX /
		    (uint64_t)llabs(maximum)))
		return false;
	    const int64_t minimum_term = minimum * (int64_t)weight;
	    const int64_t maximum_term = maximum * (int64_t)weight;
	    if ((minimum_term > 0 &&
		    minimum_numerator > INT64_MAX - minimum_term) ||
		    (minimum_term < 0 &&
		    minimum_numerator < INT64_MIN - minimum_term) ||
		    (maximum_term > 0 &&
		    maximum_numerator > INT64_MAX - maximum_term) ||
		    (maximum_term < 0 &&
		    maximum_numerator < INT64_MIN - maximum_term))
		return false;
	    minimum_numerator += minimum_term;
	    maximum_numerator += maximum_term;
	}
    }
    return true;
}


static int
check_brep_interval_enclosures()
{
    const int scale_exponents[] = {-400, -40, 0, 40, 400};
    const int product_exponents[] = {-200, 0, 200};
    const int coordinate_exponents[] = {-300, 0, 300};
    const int direction_exponents[] = {-100, 0, 100};
    const int plane_exponents[] = {-50, 0, 50};
    const int linear_transform_exponents[] = {-100, 0, 100};
    const int64_t linear_transform_mantissa[][2][2] = {
	{{1, 0}, {0, 1}},
	{{1, 1}, {-1, 1}},
	{{3, -2}, {1, 4}},
	{{1, -1}, {1, 1}}
    };
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
    size_t expansion_function_checks = 0;
    size_t derivative_checks = 0;
    size_t expansion_derivative_checks = 0;
    size_t expansion_interval_contractions = 0;
    size_t expansion_interval_high_water = 0;
    size_t product_checks = 0;
    size_t expansion_product_checks = 0;
    size_t expansion_product_contractions = 0;
    size_t expansion_product_high_water = 0;
    size_t expansion_product_fallbacks = 0;
    size_t division_checks = 0;
    size_t linear_hull_checks = 0;
    size_t determinant_checks = 0;
    size_t determinant_signed = 0;
    size_t determinant_uncertain_checks = 0;
    size_t determinant_restriction_checks = 0;
    size_t coefficient_checks = 0;
    size_t expansion_coefficient_checks = 0;
    size_t expansion_coefficient_contractions = 0;
    size_t expansion_coefficient_high_water = 0;
    size_t restriction_checks = 0;
    size_t per_coefficient_restriction_checks = 0;
    size_t expansion_restriction_checks = 0;
    size_t expansion_restriction_contractions = 0;
    size_t expansion_restriction_high_water = 0;
    size_t expansion_normalization_fallbacks = 0;
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
			expansion_coefficient_checks++;
			expansion_coefficient_high_water = std::max(
			    expansion_coefficient_high_water,
			    observed.expansion_high_water);
			const long double interval_width =
			    (long double)observed.function_maximum[equation] -
			    observed.function_minimum[equation];
			const long double expansion_width =
			    (long double)observed.
				expansion_function_maximum[equation] -
			    observed.expansion_function_minimum[equation];
			if (!observed.expansion_available ||
				(long double)observed.
				    expansion_function_minimum[equation] >
				exact_value ||
				(long double)observed.
				    expansion_function_maximum[equation] <
				exact_value || expansion_width > interval_width) {
			    std::printf("FAIL: expansion coefficient enclosure "
				"scale=%d/%d/%d weight=%d equation=%d "
				"width=%.9Lg/%.9Lg high=%zu\n",
				coordinate_exponents[coordinate_scale],
				direction_exponents[direction_scale],
				plane_exponents[plane_scale], weight, equation,
				expansion_width, interval_width,
				observed.expansion_high_water);
			    failures++;
			}
			if (expansion_width < interval_width)
			    expansion_coefficient_contractions++;
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
		fastf_t expansion_observed[2] = {};
		size_t expansion_high_water = 0;
		expansion_product_checks++;
		if (!_rt_brep_expansion_interval_product_test(first, second,
			expansion_observed, &expansion_high_water) ||
			(long double)expansion_observed[0] > expected_minimum ||
			(long double)expansion_observed[1] < expected_maximum ||
			(long double)expansion_observed[1] -
			    expansion_observed[0] >
			(long double)observed[1] - observed[0]) {
		    std::printf("FAIL: expansion interval product enclosure "
			"case=%zu scale=%d/%d\n", interval_index,
			product_exponents[first_scale],
			product_exponents[second_scale]);
		    failures++;
		}
		expansion_product_high_water = std::max(
		    expansion_product_high_water, expansion_high_water);
		if ((long double)expansion_observed[1] -
			expansion_observed[0] <
			(long double)observed[1] - observed[0])
		    expansion_product_contractions++;
	    }
	}
    }

    {
	const fastf_t denormal = std::ldexp(1.0,
	    DBL_MIN_EXP - DBL_MANT_DIG);
	const fastf_t first[2] = {denormal, denormal};
	const fastf_t second[2] = {0.5, 0.5};
	fastf_t observed[2] = {};
	size_t high_water = 0;
	if (_rt_brep_expansion_interval_product_test(first, second, observed,
		&high_water)) {
	    std::printf("FAIL: expansion product accepted unrepresentable "
		"subnormal residual\n");
	    failures++;
	} else {
	    expansion_product_fallbacks++;
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
		for (size_t transform_case = 0; transform_case <
			sizeof(linear_transform_mantissa) /
			sizeof(linear_transform_mantissa[0]); ++transform_case) {
		    for (size_t transform_scale = 0; transform_scale <
			    sizeof(linear_transform_exponents) /
			    sizeof(linear_transform_exponents[0]);
			    ++transform_scale) {
			fastf_t transform[2][2];
			exact_dyadic exact_transform[2][2];
			for (int row = 0; row < 2; ++row) {
			    for (int column = 0; column < 2; ++column) {
				exact_transform[row][column] = {
				    linear_transform_mantissa[transform_case]
					[row][column],
				    linear_transform_exponents[transform_scale]
				};
				transform[row][column] = std::ldexp(
				    (double)exact_transform[row][column].mantissa,
				    exact_transform[row][column].exponent);
			    }
			}
			struct rt_brep_linear_hull_test_result observed_hull = {};
			if (!_rt_brep_linear_hull_test(values[0], values[1],
				count, coefficient_error, transform,
				&observed_hull)) {
			    std::printf("FAIL: linear hull unavailable order=%d/%d "
				"scale=%d transform=%zu/%d\n", u_order, v_order,
				scale_exponent, transform_case,
				linear_transform_exponents[transform_scale]);
			    failures++;
			    continue;
			}
			bool expected_excluded = false;
			for (int row = 0; row < 2; ++row) {
			    long double expected_minimum = LDBL_MAX;
			    long double expected_maximum = -LDBL_MAX;
			    for (size_t i = 0; i < count; ++i) {
				exact_dyadic nominal = {0, 0};
				exact_dyadic uncertainty = {0, 0};
				for (int source = 0; source < 2; ++source) {
				    exact_dyadic term;
				    exact_dyadic error_term;
				    exact_dyadic magnitude =
					exact_transform[row][source];
				    magnitude.mantissa = llabs(magnitude.mantissa);
				    const exact_dyadic exact_error = {
					1, scale_exponent - 32
				    };
				    if (!exact_dyadic_multiply(
					    exact_transform[row][source],
					    exact[source][i], term) ||
					    !exact_dyadic_add(nominal, term, nominal) ||
					    !exact_dyadic_multiply(magnitude,
						exact_error, error_term) ||
					    !exact_dyadic_add(uncertainty,
						error_term, uncertainty)) {
					failures++;
					continue;
				    }
				}
				exact_dyadic lower;
				exact_dyadic upper;
				if (!exact_dyadic_subtract(nominal, uncertainty,
					lower) || !exact_dyadic_add(nominal,
					uncertainty, upper)) {
				    failures++;
				    continue;
				}
				expected_minimum = std::min(expected_minimum,
				    exact_dyadic_value(lower));
				expected_maximum = std::max(expected_maximum,
				    exact_dyadic_value(upper));
			    }
			    linear_hull_checks += count;
			    if ((long double)observed_hull.minimum[row] >
				    expected_minimum ||
				    (long double)observed_hull.maximum[row] <
				    expected_maximum) {
				std::printf("FAIL: linear hull enclosure order=%d/%d "
				    "scale=%d transform=%zu/%d row=%d\n", u_order,
				    v_order, scale_exponent, transform_case,
				    linear_transform_exponents[transform_scale], row);
				failures++;
			    }
			    if (expected_minimum > 0.0L ||
				    expected_maximum < 0.0L)
				expected_excluded = true;
			}
			if (observed_hull.excluded && !expected_excluded) {
			    std::printf("FAIL: linear hull false exclusion "
				"order=%d/%d scale=%d transform=%zu/%d\n",
				u_order, v_order, scale_exponent, transform_case,
				linear_transform_exponents[transform_scale]);
			    failures++;
			}
		    }
		}
		struct rt_brep_interval_test_result observed = {};
		cases++;
		if (!_rt_brep_interval_test(values[0], values[1], u_order,
			v_order, coefficient_error, root, &observed)) {
		    std::printf("FAIL: interval audit unavailable order=%d/%d "
			"scale=%d\n", u_order, v_order, scale_exponent);
		    failures++;
		    continue;
		}
		struct rt_brep_interval_test_result expansion_observed = {};
		size_t expansion_evaluation_high_water = 0;
		if (!_rt_brep_expansion_interval_test(values[0], values[1],
			u_order, v_order, coefficient_error, root,
			&expansion_observed,
			&expansion_evaluation_high_water)) {
		    std::printf("FAIL: expansion interval audit unavailable "
			"order=%d/%d scale=%d\n", u_order, v_order,
			scale_exponent);
		    failures++;
		    continue;
		}
		expansion_interval_high_water = std::max(
		    expansion_interval_high_water,
		    expansion_evaluation_high_water);

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
		    const long double expansion_lower =
			expansion_observed.function_minimum[equation];
		    const long double expansion_upper =
			expansion_observed.function_maximum[equation];
		    expansion_function_checks++;
		    if (expansion_lower > nominal - error ||
			    expansion_upper < nominal + error ||
			    expansion_upper - expansion_lower > upper - lower) {
			std::printf("FAIL: expansion function enclosure "
			    "order=%d/%d scale=%d equation=%d\n", u_order,
			    v_order, scale_exponent, equation);
			failures++;
		    }
		    if (expansion_upper - expansion_lower < upper - lower)
			expansion_interval_contractions++;

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
				const long double expansion_derivative_minimum =
				    expansion_observed.jacobian_minimum
					[equation][direction];
				const long double expansion_derivative_maximum =
				    expansion_observed.jacobian_maximum
					[equation][direction];
				expansion_derivative_checks++;
				if (expansion_derivative_minimum >
					derivative - derivative_error ||
					expansion_derivative_maximum <
					derivative + derivative_error ||
					expansion_derivative_maximum -
					expansion_derivative_minimum >
					(long double)observed.jacobian_maximum
					    [equation][direction] -
					observed.jacobian_minimum
					    [equation][direction]) {
				    std::printf("FAIL: expansion derivative "
					"enclosure order=%d/%d scale=%d "
					"equation=%d direction=%d\n", u_order,
					v_order, scale_exponent, equation,
					direction);
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
				exact_dyadic exact_lower_input[256];
				exact_dyadic exact_upper_input[256];
				exact_dyadic exact_lower_output[256];
				exact_dyadic exact_upper_output[256];
				fastf_t individual_error[256] = {};
				fastf_t interval_minimum[256] = {};
				fastf_t interval_maximum[256] = {};
				bool individual_exact = true;
				for (size_t i = 0; i < count; ++i) {
				    const exact_dyadic error_value = {
					(int64_t)(i % 3 + 1), scale_exponent - 8
				    };
				    individual_error[i] =
					(fastf_t)exact_dyadic_value(error_value);
				    if (!exact_dyadic_subtract(exact[equation][i],
					    error_value, exact_lower_input[i]) ||
					    !exact_dyadic_add(exact[equation][i],
						error_value, exact_upper_input[i]))
					individual_exact = false;
				}
				if (!individual_exact ||
					!exact_dyadic_surface_restrict_quarters(
					    exact_lower_input, u_order, v_order,
					    minimum_quarters, maximum_quarters,
					    exact_lower_output) ||
					!exact_dyadic_surface_restrict_quarters(
					    exact_upper_input, u_order, v_order,
					    minimum_quarters, maximum_quarters,
					    exact_upper_output) ||
					!_rt_brep_interval_restrict_test(
					    values[equation], individual_error, u_order,
					    v_order, minimum, maximum, interval_minimum,
					    interval_maximum)) {
				    std::printf("FAIL: per-coefficient restriction "
					"unavailable order=%d/%d scale=%d case=%zu\n",
					u_order, v_order, scale_exponent, restriction);
				    failures++;
				} else {
				    for (size_t i = 0; i < count; ++i) {
					const long double exact_minimum =
					    exact_dyadic_value(exact_lower_output[i]);
					const long double exact_maximum =
					    exact_dyadic_value(exact_upper_output[i]);
					per_coefficient_restriction_checks++;
					if ((long double)interval_minimum[i] >
						exact_minimum ||
						(long double)interval_maximum[i] <
						exact_maximum) {
					    std::printf("FAIL: per-coefficient restriction "
						"enclosure order=%d/%d scale=%d "
						"case=%zu coefficient=%zu\n", u_order,
						v_order, scale_exponent, restriction, i);
					    failures++;
					}
				    }
				}
				fastf_t expansion_minimum[256] = {};
				fastf_t expansion_maximum[256] = {};
				size_t expansion_high_water = 0;
				if (!individual_exact ||
					!_rt_brep_expansion_restrict_test(
					    values[equation], individual_error, u_order,
					    v_order, minimum, maximum,
					    expansion_minimum, expansion_maximum,
					    &expansion_high_water)) {
				    std::printf("FAIL: expansion restriction unavailable "
					"order=%d/%d scale=%d case=%zu\n", u_order,
					v_order, scale_exponent, restriction);
				    failures++;
				} else {
				    expansion_restriction_high_water = std::max(
					expansion_restriction_high_water,
					expansion_high_water);
				    for (size_t i = 0; i < count; ++i) {
					const long double exact_minimum =
					    exact_dyadic_value(exact_lower_output[i]);
					const long double exact_maximum =
					    exact_dyadic_value(exact_upper_output[i]);
					expansion_restriction_checks++;
					const long double interval_width =
					    (long double)interval_maximum[i] -
					    interval_minimum[i];
					const long double expansion_width =
					    (long double)expansion_maximum[i] -
					    expansion_minimum[i];
					if ((long double)expansion_minimum[i] >
						exact_minimum ||
						(long double)expansion_maximum[i] <
						exact_maximum ||
						expansion_width > interval_width) {
					    std::printf("FAIL: expansion restriction "
						"enclosure order=%d/%d scale=%d "
						"case=%zu coefficient=%zu width=%.17Lg/%.17Lg "
						"bounds=%.17Lg/%.17Lg:%.17Lg/%.17Lg\n",
						u_order, v_order, scale_exponent,
						restriction, i, expansion_width,
						interval_width,
						(long double)expansion_minimum[i],
						exact_minimum,
						(long double)expansion_maximum[i],
						exact_maximum);
					    failures++;
					}
					if (expansion_width < interval_width)
					    expansion_restriction_contractions++;
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

    const int determinant_scale_exponents[] = {-200, 0, 200};
    const int determinant_restriction_quarters[][2] = {
	{0, 2}, {1, 2}, {2, 4}
    };
    /*
     * Determinant orders are even from 2 through 30.  Exercise every order
     * as each tensor axis, then the maximum-order corner.  Since restriction
     * is separable, this covers the two univariate operators without an
     * unnecessarily expensive all-pairs matrix.
     */
    for (int order_case = 0; order_case < 30; ++order_case) {
	const int u_order = order_case < 15 ? 2 + 2 * order_case :
	    (order_case < 29 ? 2 : RT_BREP_DETERMINANT_TEST_MAX_ORDER);
	const int v_order = order_case < 15 ? 2 :
	    (order_case < 29 ? 4 + 2 * (order_case - 15) :
	    RT_BREP_DETERMINANT_TEST_MAX_ORDER);
	for (size_t scale_index = 0; scale_index <
		sizeof(determinant_scale_exponents) /
		sizeof(determinant_scale_exponents[0]); ++scale_index) {
		const int scale_exponent =
		    determinant_scale_exponents[scale_index];
		exact_dyadic exact_minimum[
		    RT_BREP_DETERMINANT_TEST_MAX_COEFFICIENTS];
		exact_dyadic exact_maximum[
		    RT_BREP_DETERMINANT_TEST_MAX_COEFFICIENTS];
		fastf_t input_minimum[
		    RT_BREP_DETERMINANT_TEST_MAX_COEFFICIENTS] = {};
		fastf_t input_maximum[
		    RT_BREP_DETERMINANT_TEST_MAX_COEFFICIENTS] = {};
		fastf_t output_minimum[
		    RT_BREP_DETERMINANT_TEST_MAX_COEFFICIENTS] = {};
		fastf_t output_maximum[
		    RT_BREP_DETERMINANT_TEST_MAX_COEFFICIENTS] = {};
		for (int i = 0; i < u_order; ++i) {
		    for (int j = 0; j < v_order; ++j) {
			const size_t index = (size_t)i * v_order + j;
			const int64_t center = 4 *
			    ((3 * i + 5 * j + u_order + v_order) % 5 - 2);
			const int64_t error = (i + 2 * j) % 2 + 1;
			exact_minimum[index] = {
			    center - error, scale_exponent
			};
			exact_maximum[index] = {
			    center + error, scale_exponent
			};
			input_minimum[index] = (fastf_t)exact_dyadic_value(
			    exact_minimum[index]);
			input_maximum[index] = (fastf_t)exact_dyadic_value(
			    exact_maximum[index]);
		    }
		}
		for (int direction = 0; direction < 2; ++direction) {
		    for (size_t restriction = 0; restriction <
			    sizeof(determinant_restriction_quarters) /
			    sizeof(determinant_restriction_quarters[0]);
			    ++restriction) {
			const int minimum_quarters =
			    determinant_restriction_quarters[restriction][0];
			const int maximum_quarters =
			    determinant_restriction_quarters[restriction][1];
			fastf_t minimum[2] = {0.0, 0.0};
			fastf_t maximum[2] = {1.0, 1.0};
			minimum[direction] = 0.25 * minimum_quarters;
			maximum[direction] = 0.25 * maximum_quarters;
			if (!_rt_brep_determinant_restrict_test(input_minimum,
				input_maximum, u_order, v_order, minimum, maximum,
				output_minimum, output_maximum)) {
			    std::printf("FAIL: determinant restriction unavailable "
				"order=%d/%d scale=%d direction=%d case=%zu\n",
				u_order, v_order, scale_exponent, direction,
				restriction);
			    failures++;
			    continue;
			}
			const int line_count = direction == 0 ? v_order : u_order;
			const int direction_order = direction == 0 ?
			    u_order : v_order;
			for (int line = 0; line < line_count; ++line) {
			    exact_dyadic lower_source[
				RT_BREP_DETERMINANT_TEST_MAX_ORDER];
			    exact_dyadic upper_source[
				RT_BREP_DETERMINANT_TEST_MAX_ORDER];
			    exact_dyadic lower_expected[
				RT_BREP_DETERMINANT_TEST_MAX_ORDER];
			    exact_dyadic upper_expected[
				RT_BREP_DETERMINANT_TEST_MAX_ORDER];
			    for (int k = 0; k < direction_order; ++k) {
				const size_t index = direction == 0 ?
				    (size_t)k * v_order + line :
				    (size_t)line * v_order + k;
				lower_source[k] = exact_minimum[index];
				upper_source[k] = exact_maximum[index];
			    }
			    if (!exact_dyadic_bezier_restrict_quarters(
				    lower_source, direction_order, minimum_quarters,
				    maximum_quarters, lower_expected) ||
				    !exact_dyadic_bezier_restrict_quarters(
				    upper_source, direction_order, minimum_quarters,
				    maximum_quarters, upper_expected)) {
				std::printf("FAIL: exact determinant restriction "
				    "unavailable order=%d/%d direction=%d "
				    "case=%zu line=%d\n", u_order, v_order,
				    direction, restriction, line);
				failures++;
				continue;
			    }
			    for (int k = 0; k < direction_order; ++k) {
				const size_t index = direction == 0 ?
				    (size_t)k * v_order + line :
				    (size_t)line * v_order + k;
				const long double expected_minimum =
				    exact_dyadic_value(lower_expected[k]);
				const long double expected_maximum =
				    exact_dyadic_value(upper_expected[k]);
				determinant_restriction_checks++;
				if ((long double)output_minimum[index] >
					expected_minimum ||
					(long double)output_maximum[index] <
					expected_maximum) {
				    std::printf("FAIL: determinant restriction "
					"enclosure order=%d/%d scale=%d "
					"direction=%d case=%zu coefficient=%zu\n",
					u_order, v_order, scale_exponent,
					direction, restriction, index);
				    failures++;
				}
			    }
			}
		    }
		}
	}
    }
    for (int u_order = 2; u_order <= 16; ++u_order) {
	for (int v_order = 2; v_order <= 16; ++v_order) {
	    int64_t exact_coefficient[2][256] = {};
	    int64_t exact_derivative[2][2][256] = {};
	    int derivative_order[2][2][2] = {};
	    double coefficient_error[2][256] = {};
	    for (int i = 0; i < u_order; ++i) {
		for (int j = 0; j < v_order; ++j) {
		    const size_t index = (size_t)i * v_order + j;
		    exact_coefficient[0][index] = (3 * i + u_order) & 1;
		    exact_coefficient[1][index] =
			(3 * i + 5 * j + u_order + v_order) & 1;
		}
	    }
	    bool derivatives_available = true;
	    for (int equation = 0; equation < 2; ++equation) {
		for (int direction = 0; direction < 2; ++direction) {
		    if (!exact_integer_surface_derivative(
			    exact_coefficient[equation], u_order, v_order,
			    direction, exact_derivative[equation][direction],
			    derivative_order[equation][direction]))
			derivatives_available = false;
		}
	    }
	    if (!derivatives_available) {
		failures++;
		continue;
	    }
	    for (size_t scale_index = 0; scale_index <
		    sizeof(determinant_scale_exponents) /
		    sizeof(determinant_scale_exponents[0]); ++scale_index) {
		const int scale_exponent =
		    determinant_scale_exponents[scale_index];
		for (int equation = 0; equation < 2; ++equation) {
		    for (int i = 0; i < u_order; ++i) {
			for (int j = 0; j < v_order; ++j) {
			    const size_t index = (size_t)i * v_order + j;
			    values[equation][index] = std::ldexp(
				(double)exact_coefficient[equation][index],
				scale_exponent);
			}
		    }
		}
		struct rt_brep_determinant_test_result observed = {};
		if (!_rt_brep_determinant_test(values[0], coefficient_error[0],
			values[1], coefficient_error[1], u_order, v_order,
			&observed) || observed.u_order != 2 * u_order - 2 ||
			observed.v_order != 2 * v_order - 2) {
		    std::printf("FAIL: determinant coefficients unavailable "
			"order=%d/%d scale=%d output=%d/%d\n", u_order,
			v_order, scale_exponent, observed.u_order,
			observed.v_order);
		    failures++;
		    continue;
		}
		for (int i = 0; i < observed.u_order; ++i) {
		    for (int j = 0; j < observed.v_order; ++j) {
			int64_t positive_numerator = 0;
			int64_t negative_numerator = 0;
			uint64_t positive_denominator = 0;
			uint64_t negative_denominator = 0;
			if (!exact_integer_surface_product_coefficient(
				exact_derivative[0][0],
				derivative_order[0][0],
				exact_derivative[1][1],
				derivative_order[1][1], i, j,
				positive_numerator, positive_denominator) ||
				!exact_integer_surface_product_coefficient(
				exact_derivative[0][1],
				derivative_order[0][1],
				exact_derivative[1][0],
				derivative_order[1][0], i, j,
				negative_numerator, negative_denominator) ||
				positive_denominator != negative_denominator) {
			    std::printf("FAIL: exact determinant unavailable "
				"order=%d/%d coefficient=%d/%d\n", u_order,
				v_order, i, j);
			    failures++;
			    continue;
			}
			const int64_t exact_numerator =
			    positive_numerator - negative_numerator;
			const long double exact_value = std::ldexp(
			    (long double)exact_numerator /
				(long double)positive_denominator,
			    2 * scale_exponent);
			const size_t index = (size_t)i * observed.v_order + j;
			determinant_checks++;
			if (exact_numerator &&
				((observed.minimum[index] > 0.0 &&
				  observed.maximum[index] > 0.0) ||
				 (observed.minimum[index] < 0.0 &&
				  observed.maximum[index] < 0.0)))
			    determinant_signed++;
			if ((long double)observed.minimum[index] > exact_value ||
				(long double)observed.maximum[index] < exact_value) {
			    std::printf("FAIL: determinant coefficient enclosure "
				"order=%d/%d scale=%d coefficient=%d/%d "
				"range=%.17g/%.17g exact=%.21Lg\n", u_order,
				v_order, scale_exponent, i, j,
				observed.minimum[index], observed.maximum[index],
				exact_value);
			    failures++;
			}
		    }
		}
	    }
	}
    }

    for (int u_order = 2; u_order <= 8; ++u_order) {
	for (int v_order = 2; v_order <= 8; ++v_order) {
	    exact_integer_interval exact_coefficient[2][256] = {};
	    exact_integer_interval exact_derivative[2][2][256] = {};
	    int derivative_order[2][2][2] = {};
	    int64_t center[2][256] = {};
	    int64_t error[2][256] = {};
	    double determinant_error[2][256] = {};
	    for (int i = 0; i < u_order; ++i) {
		for (int j = 0; j < v_order; ++j) {
		    const size_t index = (size_t)i * v_order + j;
		    center[0][index] = 4 * ((3 * i + u_order) & 1);
		    center[1][index] = 4 *
			((3 * i + 5 * j + u_order + v_order) & 1);
		    error[1][index] = (i + 2 * j) % 2 + 1;
		    for (int equation = 0; equation < 2; ++equation) {
			exact_coefficient[equation][index] = {
			    center[equation][index] - error[equation][index],
			    center[equation][index] + error[equation][index]
			};
		    }
		}
	    }
	    bool derivatives_available = true;
	    for (int equation = 0; equation < 2; ++equation) {
		for (int direction = 0; direction < 2; ++direction) {
		    if (!exact_integer_interval_surface_derivative(
			    exact_coefficient[equation], u_order, v_order,
			    direction, exact_derivative[equation][direction],
			    derivative_order[equation][direction]))
			derivatives_available = false;
		}
	    }
	    if (!derivatives_available) {
		failures++;
		continue;
	    }
	    for (size_t scale_index = 0; scale_index <
		    sizeof(determinant_scale_exponents) /
		    sizeof(determinant_scale_exponents[0]); ++scale_index) {
		const int unit_exponent =
		    determinant_scale_exponents[scale_index] - 2;
		for (int equation = 0; equation < 2; ++equation) {
		    for (int i = 0; i < u_order; ++i) {
			for (int j = 0; j < v_order; ++j) {
			    const size_t index = (size_t)i * v_order + j;
			    values[equation][index] = std::ldexp(
				(double)center[equation][index], unit_exponent);
			    determinant_error[equation][index] = std::ldexp(
				(double)error[equation][index], unit_exponent);
			}
		    }
		}
		struct rt_brep_determinant_test_result observed = {};
		if (!_rt_brep_determinant_test(values[0], determinant_error[0],
			values[1], determinant_error[1], u_order, v_order,
			&observed)) {
		    std::printf("FAIL: uncertain determinant unavailable "
			"order=%d/%d scale=%d\n", u_order, v_order,
			determinant_scale_exponents[scale_index]);
		    failures++;
		    continue;
		}
		for (int i = 0; i < observed.u_order; ++i) {
		    for (int j = 0; j < observed.v_order; ++j) {
			int64_t positive_minimum = 0;
			int64_t positive_maximum = 0;
			int64_t negative_minimum = 0;
			int64_t negative_maximum = 0;
			uint64_t positive_denominator = 0;
			uint64_t negative_denominator = 0;
			if (!exact_integer_interval_surface_product_coefficient(
				exact_derivative[0][0],
				derivative_order[0][0],
				exact_derivative[1][1],
				derivative_order[1][1], i, j,
				positive_minimum, positive_maximum,
				positive_denominator) ||
				!exact_integer_interval_surface_product_coefficient(
				exact_derivative[0][1],
				derivative_order[0][1],
				exact_derivative[1][0],
				derivative_order[1][0], i, j,
				negative_minimum, negative_maximum,
				negative_denominator) ||
				positive_denominator != negative_denominator) {
			    std::printf("FAIL: exact uncertain determinant "
				"unavailable order=%d/%d coefficient=%d/%d\n",
				u_order, v_order, i, j);
			    failures++;
			    continue;
			}
			const int64_t exact_minimum_numerator =
			    positive_minimum - negative_maximum;
			const int64_t exact_maximum_numerator =
			    positive_maximum - negative_minimum;
			const long double exact_minimum = std::ldexp(
			    (long double)exact_minimum_numerator /
				(long double)positive_denominator,
			    2 * unit_exponent);
			const long double exact_maximum = std::ldexp(
			    (long double)exact_maximum_numerator /
				(long double)positive_denominator,
			    2 * unit_exponent);
			const size_t index = (size_t)i * observed.v_order + j;
			determinant_uncertain_checks++;
			if ((long double)observed.minimum[index] > exact_minimum ||
				(long double)observed.maximum[index] < exact_maximum) {
			    std::printf("FAIL: uncertain determinant enclosure "
				"order=%d/%d scale=%d coefficient=%d/%d\n",
				u_order, v_order,
				determinant_scale_exponents[scale_index], i, j);
			    failures++;
			}
		    }
		}
	    }
	}
    }

    {
	const fastf_t denormal = std::ldexp(1.0,
	    DBL_MIN_EXP - DBL_MANT_DIG);
	const fastf_t mixed_scale[4] = {
	    4.0 * denormal, 1.0, -0.5, 0.25
	};
	const fastf_t mixed_error[4] = {
	    2.0 * denormal, 0.0, 0.0, 0.0
	};
	const fastf_t minimum[2] = {0.0, 0.0};
	const fastf_t maximum[2] = {0.75, 0.75};
	fastf_t observed_minimum[4] = {};
	fastf_t observed_maximum[4] = {};
	size_t high_water = 0;
	if (_rt_brep_expansion_restrict_test(mixed_scale, mixed_error, 2, 2,
		minimum, maximum, observed_minimum, observed_maximum,
		&high_water)) {
	    std::printf("FAIL: expansion restriction accepted lossy "
		"power-of-two normalization\n");
	    failures++;
	} else {
	    expansion_normalization_fallbacks++;
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
	    "function=%zu+%zu derivative=%zu+%zu/%zu/%zu "
	    "product=%zu+%zu/%zu/%zu+%zu quotient=%zu "
	    "linear-hull=%zu determinant=%zu/%zu+%zu restrict=%zu "
	    "coefficient=%zu+%zu/%zu/%zu "
	    "restriction=%zu+%zu+%zu/%zu/%zu "
	    "normalization-fallback=%zu "
	    "reparameterization=%zu "
	    "clip=%zu/%zu "
	    "max-width/error=%.9Lg/%.9Lg/%.9Lg\n",
	    cases, function_checks, expansion_function_checks,
	    derivative_checks, expansion_derivative_checks,
	    expansion_interval_contractions, expansion_interval_high_water,
	    product_checks, expansion_product_checks,
	    expansion_product_contractions, expansion_product_high_water,
	    expansion_product_fallbacks,
	    division_checks, linear_hull_checks, determinant_signed,
	    determinant_checks, determinant_uncertain_checks,
	    determinant_restriction_checks, coefficient_checks,
	    expansion_coefficient_checks, expansion_coefficient_contractions,
	    expansion_coefficient_high_water,
	    restriction_checks, per_coefficient_restriction_checks,
	    expansion_restriction_checks,
	    expansion_restriction_contractions,
	    expansion_restriction_high_water,
	    expansion_normalization_fallbacks,
	    reparameterization_checks, clip_contractions, clip_checks,
	    maximum_function_width_ratio,
	    maximum_restriction_width_ratio,
	    maximum_reparameterization_width_ratio);
    }
    return failures;
}


static int
check_brep_local_root_solver()
{
    int failures = 0;
    size_t cases = 0;
    size_t certified = 0;
    size_t rejected = 0;
    size_t maximum_attempts = 0;
    size_t maximum_high_water = 0;
    double maximum_radius = 0.0;

    const auto run = [&](const char *name, const fastf_t *first_minimum,
	    const fastf_t *first_maximum, const fastf_t *second_minimum,
	    const fastf_t *second_maximum, int u_order, int v_order,
	    const fastf_t root[2], double radius_limit,
	    bool expected_available, bool expected_certified) {
	struct rt_brep_local_root_test_result result = {};
	cases++;
	const bool called = _rt_brep_local_root_test(first_minimum,
	    first_maximum, second_minimum, second_maximum, u_order, v_order,
	    root, radius_limit, &result);
	const bool bad = !called || result.available !=
	    (expected_available ? 1 : 0) || result.certified !=
	    (expected_certified ? 1 : 0) ||
	    (result.certified &&
	     (!(result.radius > 0.0) || result.radius > radius_limit ||
	      !(result.contraction_bound < 1.0) ||
	      !(result.image_minimum[0] > -result.radius) ||
	      !(result.image_maximum[0] < result.radius) ||
	      !(result.image_minimum[1] > -result.radius) ||
	      !(result.image_maximum[1] < result.radius)));
	if (bad) {
	    std::printf("FAIL: local root %s call/available/certified="
		"%d/%d/%d expected=1/%d/%d attempts=%zu radius=%.17g "
		"correction/contraction=%.17g/%.17g image=[%.17g %.17g]x"
		"[%.17g %.17g]\n", name, called ? 1 : 0,
		result.available, result.certified,
		expected_available ? 1 : 0, expected_certified ? 1 : 0,
		result.attempts, result.radius, result.correction_bound,
		result.contraction_bound, result.image_minimum[0],
		result.image_maximum[0], result.image_minimum[1],
		result.image_maximum[1]);
	    failures++;
	} else if (result.certified) {
	    certified++;
	} else {
	    rejected++;
	}
	maximum_attempts = std::max(maximum_attempts, result.attempts);
	maximum_high_water = std::max(maximum_high_water,
	    result.expansion_high_water);
	maximum_radius = std::max(maximum_radius, (double)result.radius);
    };

    const int scale_exponents[] = {-100, 0, 100};
    const double boundary_shift = std::ldexp(1.0, -42);
    const fastf_t boundary_root[2] = {1.0, 0.5};
    for (size_t scale_index = 0;
	    scale_index < sizeof(scale_exponents) /
	    sizeof(scale_exponents[0]); ++scale_index) {
	const double scale = std::ldexp(1.0, scale_exponents[scale_index]);
	fastf_t minimum[2][4] = {};
	fastf_t maximum[2][4] = {};
	for (int i = 0; i < 2; ++i) {
	    for (int j = 0; j < 2; ++j) {
		const size_t index = (size_t)i * 2 + j;
		const double f = i - (1.0 + boundary_shift);
		const double g = j - 0.5;
		minimum[0][index] = maximum[0][index] =
		    scale * (f + 0.25 * g);
		minimum[1][index] = maximum[1][index] =
		    scale * (-0.5 * f + g);
	    }
	}
	char name[64];
	std::snprintf(name, sizeof(name), "affine-boundary-%d",
	    scale_exponents[scale_index]);
	run(name, minimum[0], maximum[0], minimum[1], maximum[1], 2, 2,
	    boundary_root, 1.0e-6, true, true);
    }

    {
	const int u_order = 9;
	const int v_order = 9;
	fastf_t minimum[2][81] = {};
	fastf_t maximum[2][81] = {};
	for (int i = 0; i < u_order; ++i) {
	    for (int j = 0; j < v_order; ++j) {
		const size_t index = (size_t)i * v_order + j;
		const double f = (double)i / (u_order - 1) -
		    (1.0 + boundary_shift);
		const double g = (double)j / (v_order - 1) - 0.5;
		minimum[0][index] = maximum[0][index] = f + 0.25 * g;
		minimum[1][index] = maximum[1][index] = -0.5 * f + g;
	    }
	}
	run("high-order-affine", minimum[0], maximum[0],
	    minimum[1], maximum[1], u_order, v_order, boundary_root,
	    1.0e-6, true, true);
    }

    {
	const int u_order = 16;
	const int v_order = 8;
	fastf_t minimum[2][128] = {};
	fastf_t maximum[2][128] = {};
	const double root_power = pow(1.0 + boundary_shift, 15.0);
	for (int i = 0; i < u_order; ++i) {
	    for (int j = 0; j < v_order; ++j) {
		const size_t index = (size_t)i * v_order + j;
		const double u = (double)i / (u_order - 1);
		const double v = (double)j / (v_order - 1);
		minimum[0][index] = maximum[0][index] =
		    (i == u_order - 1 ? 1.0 : 0.0) - root_power +
		    0.125 * (v - 0.5);
		minimum[1][index] = maximum[1][index] =
		    -0.25 * (u - (1.0 + boundary_shift)) + v - 0.5;
	    }
	}
	run("degree-fifteen-boundary", minimum[0], maximum[0], minimum[1],
	    maximum[1], u_order, v_order, boundary_root, 1.0e-6, true,
	    true);
    }

    const int determinant_exponents[] = {-20, -40};
    for (size_t exponent_index = 0;
	    exponent_index < sizeof(determinant_exponents) /
	    sizeof(determinant_exponents[0]); ++exponent_index) {
	const double delta = std::ldexp(1.0,
	    determinant_exponents[exponent_index]);
	fastf_t minimum[2][4] = {};
	fastf_t maximum[2][4] = {};
	for (int i = 0; i < 2; ++i) {
	    for (int j = 0; j < 2; ++j) {
		const size_t index = (size_t)i * 2 + j;
		const double u = i - (1.0 + boundary_shift);
		const double v = j - 0.5;
		minimum[0][index] = maximum[0][index] = u + v;
		minimum[1][index] = maximum[1][index] =
		    u + (1.0 + delta) * v;
	    }
	}
	char name[64];
	std::snprintf(name, sizeof(name), "near-singular-affine-%d",
	    determinant_exponents[exponent_index]);
	run(name, minimum[0], maximum[0], minimum[1], maximum[1], 2, 2,
	    boundary_root, 1.0e-6, true, true);
    }

    const double near_root = 1.0 + std::ldexp(1.0, -42);
    const double other_root = 1.0 - std::ldexp(1.0, -12);
    const double quadratic[3] = {
	other_root * near_root,
	other_root * near_root - 0.5 * (other_root + near_root),
	(1.0 - other_root) * (1.0 - near_root)
    };
    for (size_t scale_index = 0;
	    scale_index < sizeof(scale_exponents) /
	    sizeof(scale_exponents[0]); ++scale_index) {
	const double scale = std::ldexp(1.0, scale_exponents[scale_index]);
	fastf_t minimum[2][6] = {};
	fastf_t maximum[2][6] = {};
	for (int i = 0; i < 3; ++i) {
	    for (int j = 0; j < 2; ++j) {
		const size_t index = (size_t)i * 2 + j;
		minimum[0][index] = maximum[0][index] =
		    scale * quadratic[i];
		minimum[1][index] = maximum[1][index] =
		    scale * (j - 0.5);
	    }
	}
	char name[64];
	std::snprintf(name, sizeof(name), "nearby-boundary-pair-%d",
	    scale_exponents[scale_index]);
	run(name, minimum[0], maximum[0], minimum[1], maximum[1], 3, 2,
	    boundary_root, 0.25 * (1.0 - other_root), true, true);
    }

    {
	const double first_root = 1.0 - std::ldexp(1.0, -10);
	const double second_root = 1.0 + std::ldexp(1.0, -11);
	const double coefficients[3] = {
	    first_root * second_root,
	    first_root * second_root - 0.5 * (first_root + second_root),
	    (1.0 - first_root) * (1.0 - second_root)
	};
	fastf_t minimum[2][6] = {};
	fastf_t maximum[2][6] = {};
	for (int i = 0; i < 3; ++i) {
	    for (int j = 0; j < 2; ++j) {
		const size_t index = (size_t)i * 2 + j;
		minimum[0][index] = maximum[0][index] = coefficients[i];
		minimum[1][index] = maximum[1][index] = j - 0.5;
	    }
	}
	run("two-roots-in-box", minimum[0], maximum[0], minimum[1],
	    maximum[1], 3, 2, boundary_root,
	    2.0 * (second_root - first_root), true, false);
    }

    {
	fastf_t minimum[2][6] = {};
	fastf_t maximum[2][6] = {};
	const double square[3] = {1.0, 0.0, 0.0};
	for (int i = 0; i < 3; ++i) {
	    for (int j = 0; j < 2; ++j) {
		const size_t index = (size_t)i * 2 + j;
		minimum[0][index] = maximum[0][index] = square[i];
		minimum[1][index] = maximum[1][index] = j - 0.5;
	    }
	}
	run("singular-boundary", minimum[0], maximum[0], minimum[1],
	    maximum[1], 3, 2, boundary_root, 1.0e-4, false, false);
    }

    {
	fastf_t minimum[2][4] = {};
	fastf_t maximum[2][4] = {};
	for (int i = 0; i < 2; ++i) {
	    for (int j = 0; j < 2; ++j) {
		const size_t index = (size_t)i * 2 + j;
		minimum[0][index] = i - 1.25;
		maximum[0][index] = i - 0.75;
		minimum[1][index] = j - 0.75;
		maximum[1][index] = j - 0.25;
	    }
	}
	run("uncertain-affine", minimum[0], maximum[0], minimum[1],
	    maximum[1], 2, 2, boundary_root, 1.0e-3, true, false);
    }

    fastf_t valid_minimum[4] = {-1.0, -1.0, 0.0, 0.0};
    fastf_t valid_maximum[4] = {-1.0, -1.0, 0.0, 0.0};
    fastf_t reversed_minimum[4] = {1.0, -1.0, 0.0, 0.0};
    struct rt_brep_local_root_test_result invalid = {};
    if (_rt_brep_local_root_test(reversed_minimum, valid_maximum,
	    valid_minimum, valid_maximum, 2, 2, boundary_root, 1.0e-3,
	    &invalid) ||
	    _rt_brep_local_root_test(valid_minimum, valid_maximum,
		valid_minimum, valid_maximum, 2, 2, boundary_root, -1.0,
		&invalid)) {
	std::printf("FAIL: local root invalid-input rejection\n");
	failures++;
    }

    fastf_t tube_upper[2] = {0.0, 0.0};
    if (!_rt_brep_local_root_tube_test(0.25, 0.5, 0.25, 0.751, 0.0,
	    tube_upper) || !(tube_upper[0] > 0.75) ||
	    !(tube_upper[1] > 0.5) ||
	    _rt_brep_local_root_tube_test(0.25, 0.5, 0.25, 0.74, 0.0,
		tube_upper) ||
	    _rt_brep_local_root_tube_test(-0.25, 0.5, 0.25, 0.751, 0.0,
		tube_upper)) {
	std::printf("FAIL: local root tube containment controls\n");
	failures++;
    }

    if (!failures)
	std::printf("BREP local root solver: PASS cases=%zu certified=%zu "
	    "rejected=%zu max-attempts=%zu radius=%.3g high-water=%zu\n",
	    cases, certified, rejected, maximum_attempts, maximum_radius,
	    maximum_high_water);
    return failures;
}


static ON_Brep *
weighted_bilinear_patch(double low_weight, double high_weight, double scale,
    const ON_3dVector &translation)
{
    if (!(low_weight > 0.0) || !(high_weight > 0.0) ||
	    !std::isfinite(low_weight) || !std::isfinite(high_weight) ||
	    !(scale > 0.0) || !std::isfinite(scale) ||
	    !translation.IsValid())
	return NULL;
    ON_BezierSurface bezier(3, true, 2, 2);
    for (int i = 0; i < 2; ++i) {
	for (int j = 0; j < 2; ++j) {
	    const double weight = i ? high_weight : low_weight;
	    const ON_3dPoint point(translation.x + scale * i,
		translation.y + scale * j, translation.z);
	    if (!bezier.SetCV(i, j, ON_4dPoint(weight * point.x,
		    weight * point.y, weight * point.z, weight)))
		return NULL;
	}
    }
    ON_NurbsSurface nurbs;
    if (!bezier.GetNurbForm(nurbs))
	return NULL;
    ON_Brep *brep = ON_Brep::New();
    ON_BrepFace *face = brep ? brep->NewFace(nurbs) : NULL;
    if (!face) {
	delete brep;
	return NULL;
    }
    face->m_bRev = false;
    brep->Standardize();
    brep->Compact();
    return brep;
}


static int
check_brep_weighted_local_root_solver()
{
    struct weighted_case {
	const char *name;
	double low_weight;
	double high_weight;
	double scale;
	ON_3dVector translation;
	bool reverse;
	bool expect_prep;
    } cases[] = {
	{"balanced", 1.0, 1.0, 1.0, ON_3dVector(0.0, 0.0, 0.0), false,
	    true},
	{"extreme-weight", std::ldexp(1.0, -10),
	    std::ldexp(1.0, 10), 1.0,
	    ON_3dVector(0.0, 0.0, 0.0), true, true},
	{"extreme-weight-similarity", std::ldexp(1.0, -10),
	    std::ldexp(1.0, 10), 1.0e4,
	    ON_3dVector(1.0e6, -2.0e6, 3.0e6), false, true},
	{"singular-weight-prep", std::ldexp(1.0, -200),
	    std::ldexp(1.0, 200), 1.0,
	    ON_3dVector(0.0, 0.0, 0.0), false, false}
    };
    int failures = 0;
    size_t certified = 0;
    size_t prep_rejected = 0;
    double maximum_image = 0.0;
    size_t maximum_root_high_water = 0;
    size_t maximum_model_high_water = 0;
    for (size_t case_index = 0;
	    case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
	const weighted_case &test = cases[case_index];
	ON_Brep *brep = weighted_bilinear_patch(test.low_weight,
	    test.high_weight, test.scale, test.translation);
	struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
	if (!brep || !rtip) {
	    std::printf("FAIL: weighted local root %s construction\n",
		test.name);
	    delete brep;
	    if (rtip)
		rt_i_destroy(rtip);
	    failures++;
	    continue;
	}
	rtip->rti_tol.magic = BN_TOL_MAGIC;
	rtip->rti_tol.dist = 0.0005 * test.scale;
	rtip->rti_tol.dist_sq = rtip->rti_tol.dist * rtip->rti_tol.dist;
	rtip->rti_tol.perp = 1.0e-6;
	rtip->rti_tol.para = 1.0 - rtip->rti_tol.perp;
	struct resource resource = {};
	rt_init_resource(&resource, 0, rtip);
	struct rt_brep_internal brep_internal = {};
	brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
	brep_internal.brep = brep;
	bool tree_valid = false;
	brlcad::SurfaceTree::FailureReason tree_failure =
	    brlcad::SurfaceTree::FAILURE_NONE;
	{
	    brlcad::SurfaceTree prep_tree(&brep->m_F[0], true, 8);
	    tree_valid = prep_tree.Valid();
	    tree_failure = prep_tree.Failure();
	}
	if (tree_valid != test.expect_prep ||
		(tree_valid && tree_failure !=
		 brlcad::SurfaceTree::FAILURE_NONE) ||
		(!tree_valid && tree_failure ==
		 brlcad::SurfaceTree::FAILURE_NONE) ||
		(!test.expect_prep && tree_failure !=
		 brlcad::SurfaceTree::FAILURE_NORMAL_EVALUATION)) {
	    std::printf("FAIL: weighted local root %s surface tree "
		"valid/failure=%d/%d expected=%d\n", test.name,
		tree_valid ? 1 : 0, (int)tree_failure,
		test.expect_prep ? 1 : 0);
	    failures++;
	}
	ON_3dPoint point = ON_3dPoint::UnsetPoint;
	if (brep->m_F.Count() == 1 && brep->m_F[0].SurfaceOf())
	    point = brep->m_F[0].SurfaceOf()->PointAt(1.0, 0.5);
	fastf_t ray_origin[3] = {point.x, point.y,
	    point.z + (test.reverse ? -test.scale : test.scale)};
	fastf_t ray_direction[3] = {0.0, 0.0,
	    test.reverse ? 1.0 : -1.0};
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BREP;
	intern.idb_meth = &OBJ[ID_BREP];
	intern.idb_ptr = &brep_internal;
	struct soltab *stp = prep_solid(rtip, &intern, ID_BREP);
	if (!test.expect_prep) {
	    if (stp) {
		std::printf("FAIL: weighted local root %s prep succeeded\n",
		    test.name);
		free_solid(stp);
		failures++;
	    } else {
		prep_rejected++;
	    }
	    rt_clean_resource_basic(rtip, &resource);
	    BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
	    rt_i_destroy(rtip);
	    continue;
	}
	struct rt_brep_local_root_test_result result = {};
	bool called = false;
	if (stp && point.IsValid()) {
	    fastf_t uv[2] = {1.0, 0.5};
	    called = _rt_brep_surface_local_root_test(stp,
		ray_origin, ray_direction, 0, 0, uv, 1.0e-6, &result);
	}
	const bool extension = result.normalized_root[0] + result.radius > 1.0;
	const bool bad = !stp || !called || !result.available ||
	    !result.certified || !result.model_image_available ||
	    !extension || !(result.radius > 0.0) ||
	    !(result.weight_minimum > 0.0) ||
	    !(result.weight_maximum >= result.weight_minimum) ||
	    !std::isfinite(result.model_image_displacement);
	if (bad) {
	    std::printf("FAIL: weighted local root %s "
		"prep/call/root/image/extension=%d/%d/%d/%d/%d "
		"radius=%.17g weight=%.17g/%.17g displacement=%.17g\n",
		test.name, stp != NULL, called ? 1 : 0, result.certified,
		result.model_image_available, extension ? 1 : 0,
		result.radius, result.weight_minimum, result.weight_maximum,
		result.model_image_displacement);
	    failures++;
	} else {
	    certified++;
	    maximum_image = std::max(maximum_image,
		(double)result.model_image_displacement);
	    maximum_root_high_water = std::max(maximum_root_high_water,
		result.expansion_high_water);
	    maximum_model_high_water = std::max(maximum_model_high_water,
		result.model_expansion_high_water);
	}
	if (stp)
	    free_solid(stp);
	rt_clean_resource_basic(rtip, &resource);
	BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
	rt_i_destroy(rtip);
    }
    if (!failures)
	std::printf("BREP weighted local root: PASS cases=%zu certified=%zu "
	    "prep-rejected=%zu root/model-high-water=%zu/%zu max-image=%.3g\n",
	    sizeof(cases) / sizeof(cases[0]), certified,
	    prep_rejected, maximum_root_high_water, maximum_model_high_water,
	    maximum_image);
    return failures;
}


static int
check_brep_fold_interval_classifier()
{
    struct interval_case {
	const char *name;
	double lower[2];
	double upper[2];
	int expected;
    } cases[] = {
	{"resolved", {1.0, 1.01}, {1.12, 1.13},
	    RT_BREP_FOLD_GAP_RESOLVED},
	{"subminimum", {1.0, 1.01}, {1.04, 1.05},
	    RT_BREP_FOLD_GAP_SUBMINIMUM},
	{"ambiguous", {1.0, 1.01}, {1.09, 1.12},
	    RT_BREP_FOLD_GAP_AMBIGUOUS},
	{"contact", {1.0, 1.01}, {1.005, 1.006},
	    RT_BREP_FOLD_GAP_SUBMINIMUM}
    };
    const double direction_scales[] = {0.25, 1.0, 16.0};
    const double model_scales[] = {0.01, 1.0, 1.0e4};
    const double base_direction[3] = {3.0, 4.0, 0.0};
    const double base_tolerance = 0.5;
    int failures = 0;
    size_t checks = 0;
    for (size_t case_index = 0;
	    case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
	for (size_t direction_index = 0; direction_index <
		sizeof(direction_scales) / sizeof(direction_scales[0]);
		++direction_index) {
	    for (size_t model_index = 0; model_index <
		    sizeof(model_scales) / sizeof(model_scales[0]);
		    ++model_index) {
		const double direction_scale =
		    direction_scales[direction_index];
		const double model_scale = model_scales[model_index];
		const double parameter_scale = model_scale / direction_scale;
		fastf_t lower[2] = {
		    cases[case_index].lower[0] * parameter_scale,
		    cases[case_index].lower[1] * parameter_scale
		};
		fastf_t upper[2] = {
		    cases[case_index].upper[0] * parameter_scale,
		    cases[case_index].upper[1] * parameter_scale
		};
		fastf_t direction[3] = {
		    base_direction[0] * direction_scale,
		    base_direction[1] * direction_scale,
		    base_direction[2] * direction_scale
		};
		struct rt_brep_fold_interval_test_result result = {};
		checks++;
		const long double exact_gap_minimum =
		    (long double)upper[0] - (long double)lower[1];
		const long double exact_gap_maximum =
		    (long double)upper[1] - (long double)lower[0];
		const long double exact_minimum_t =
		    (long double)(base_tolerance * model_scale) /
		    hypotl((long double)direction[0],
			(long double)direction[1]);
		if (!_rt_brep_fold_interval_test(lower, upper, direction,
			base_tolerance * model_scale, &result) ||
			result.classification != cases[case_index].expected ||
			(long double)result.gap_minimum > exact_gap_minimum ||
			(long double)result.gap_maximum < exact_gap_maximum ||
			(long double)result.minimum_t_minimum > exact_minimum_t ||
			(long double)result.minimum_t_maximum < exact_minimum_t) {
		    std::printf("FAIL: fold interval %s direction/model=%.3g/"
			"%.3g class=%d/%d gap=%.17g/%.17g min=%.17g/%.17g\n",
			cases[case_index].name, direction_scale, model_scale,
			result.classification, cases[case_index].expected,
			result.gap_minimum, result.gap_maximum,
			result.minimum_t_minimum,
			result.minimum_t_maximum);
		    failures++;
		}
	    }
	}
    }

    fastf_t valid_lower[2] = {1.0, 1.01};
    fastf_t valid_upper[2] = {1.12, 1.13};
    fastf_t valid_direction[3] = {3.0, 4.0, 0.0};
    fastf_t reversed[2] = {1.01, 1.0};
    fastf_t zero_direction[3] = {0.0, 0.0, 0.0};
    struct rt_brep_fold_interval_test_result result = {};
    if (_rt_brep_fold_interval_test(reversed, valid_upper,
	    valid_direction, base_tolerance, &result) ||
	    _rt_brep_fold_interval_test(valid_lower, valid_upper,
	    zero_direction, base_tolerance, &result) ||
	    _rt_brep_fold_interval_test(valid_lower, valid_upper,
	    valid_direction, -base_tolerance, &result)) {
	std::printf("FAIL: fold interval invalid-input rejection\n");
	failures++;
    }
    if (!failures)
	std::printf("BREP fold interval classifier: PASS cases=%zu "
	    "classes=resolved/subminimum/ambiguous/contact\n", checks);
    return failures;
}


static int
check_brep_trim_interval_solver()
{
    int failures = 0;
    size_t cases = 0;
    size_t enclosures = 0;
    size_t rejected = 0;

    const auto set_cv = [](struct rt_brep_trim_interval_test_span &span,
	    int index, double x, double y, double weight) {
	span.control[index][0] = x;
	span.control[index][1] = y;
	span.control[index][2] = weight;
    };
    const auto set_line = [&](struct rt_brep_trim_interval_test_span &span,
	    double domain_minimum, double domain_maximum, double x0,
	    double y0, double x1, double y1) {
	span = {};
	span.order = 2;
	span.domain_minimum = domain_minimum;
	span.domain_maximum = domain_maximum;
	set_cv(span, 0, x0, y0, 1.0);
	set_cv(span, 1, x1, y1, 1.0);
    };
    const auto run = [&](const char *name,
	    const struct rt_brep_trim_interval_test_span *spans,
	    size_t span_count, size_t span_begin, size_t cell_span_count,
	    const fastf_t domain[2], bool expected_call,
	    bool expected_available,
	    struct rt_brep_trim_interval_test_result &result) {
	result = {};
	cases++;
	const bool called = _rt_brep_trim_interval_test(spans, span_count,
	    span_begin, cell_span_count, domain, &result);
	if (called != expected_call ||
		(called && result.available !=
		    (expected_available ? 1 : 0))) {
	    std::printf("FAIL: trim interval %s call/available=%d/%d "
		"expected=%d/%d\n", name, called ? 1 : 0,
		result.available, expected_call ? 1 : 0,
		expected_available ? 1 : 0);
	    failures++;
	    return false;
	}
	if (called && result.available)
	    enclosures++;
	else if (called)
	    rejected++;
	return true;
    };
    const auto enclosed = [](double value, double minimum,
	    double maximum) {
	const double scale = std::max(1.0, fabs(value));
	const double allowance = 128.0 * DBL_EPSILON * scale;
	return value >= minimum - allowance && value <= maximum + allowance;
    };
    const auto evaluate = [](const struct rt_brep_trim_interval_test_span &s,
	    double parameter, double value[2], double derivative[2]) {
	double homogeneous[RT_BREP_TRIM_INTERVAL_TEST_MAX_ORDER][3] = {};
	double difference[RT_BREP_TRIM_INTERVAL_TEST_MAX_ORDER][3] = {};
	const double normalized = (parameter - s.domain_minimum) /
	    (s.domain_maximum - s.domain_minimum);
	for (int i = 0; i < s.order; ++i) {
	    const double weight = s.control[i][2];
	    homogeneous[i][0] = s.control[i][0] * weight;
	    homogeneous[i][1] = s.control[i][1] * weight;
	    homogeneous[i][2] = weight;
	}
	for (int i = 0; i < s.order - 1; ++i) {
	    for (int component = 0; component < 3; ++component) {
		difference[i][component] = (s.order - 1) *
		    (homogeneous[i + 1][component] -
		    homogeneous[i][component]);
	    }
	}
	for (int level = s.order - 1; level > 0; --level) {
	    for (int i = 0; i < level; ++i) {
		for (int component = 0; component < 3; ++component) {
		    homogeneous[i][component] =
			(1.0 - normalized) * homogeneous[i][component] +
			normalized * homogeneous[i + 1][component];
		}
	    }
	}
	for (int level = s.order - 2; level > 0; --level) {
	    for (int i = 0; i < level; ++i) {
		for (int component = 0; component < 3; ++component) {
		    difference[i][component] =
			(1.0 - normalized) * difference[i][component] +
			normalized * difference[i + 1][component];
		}
	    }
	}
	if (!(homogeneous[0][2] > 0.0))
	    return false;
	const double weight_squared = homogeneous[0][2] *
	    homogeneous[0][2];
	for (int direction = 0; direction < 2; ++direction) {
	    value[direction] = homogeneous[0][direction] /
		homogeneous[0][2];
	    derivative[direction] =
		(difference[0][direction] * homogeneous[0][2] -
		 homogeneous[0][direction] * difference[0][2]) /
		weight_squared;
	}
	return true;
    };

    struct rt_brep_trim_interval_test_span rational[1] = {};
    rational[0].order = 3;
    rational[0].domain_minimum = 0.0;
    rational[0].domain_maximum = 1.0;
    set_cv(rational[0], 0, 0.0, 0.0, 1.0);
    set_cv(rational[0], 1, 0.5, 1.0, 2.0);
    set_cv(rational[0], 2, 1.0, 0.0, 1.0);
    const fastf_t restricted_domain[2] = {0.1, 0.3};
    struct rt_brep_trim_interval_test_result rational_result = {};
    if (run("positive-rational", rational, 1, 0, 1,
	    restricted_domain, true, true, rational_result)) {
	if (!(rational_result.derivative_minimum[0] > 0.0) ||
		!(rational_result.derivative_minimum[1] > 0.0)) {
	    std::printf("FAIL: trim interval positive rational signs\n");
	    failures++;
	}
	for (size_t sample = 0; sample <= 64; ++sample) {
	    const double parameter = restricted_domain[0] +
		(restricted_domain[1] - restricted_domain[0]) * sample / 64.0;
	    double value[2];
	    double derivative[2];
	    if (!evaluate(rational[0], parameter, value, derivative)) {
		std::printf("FAIL: trim interval rational evaluation\n");
		failures++;
		break;
	    }
	    const double local_scale =
		(restricted_domain[1] - restricted_domain[0]) /
		(rational[0].domain_maximum - rational[0].domain_minimum);
	    for (int direction = 0; direction < 2; ++direction) {
		if (!enclosed(value[direction],
			rational_result.uv_minimum[direction],
			rational_result.uv_maximum[direction]) ||
			!enclosed(local_scale * derivative[direction],
			rational_result.derivative_minimum[direction],
			rational_result.derivative_maximum[direction])) {
		    std::printf("FAIL: trim interval rational sample=%zu "
			"direction=%d value=%.17g in=[%.17g %.17g] "
			"derivative=%.17g in=[%.17g %.17g]\n", sample,
			direction, value[direction],
			rational_result.uv_minimum[direction],
			rational_result.uv_maximum[direction],
			local_scale * derivative[direction],
			rational_result.derivative_minimum[direction],
			rational_result.derivative_maximum[direction]);
		    failures++;
		    sample = 65;
		    break;
		}
	    }
	}
    }

    struct rt_brep_trim_interval_test_span joined[2] = {};
    set_line(joined[0], 0.0, 0.5, 0.0, 0.0, 0.5, 0.5);
    set_line(joined[1], 0.5, 1.0, 0.5, 0.5, 1.0, 0.0);
    const fastf_t joined_domain[2] = {0.25, 0.75};
    struct rt_brep_trim_interval_test_result joined_result = {};
    if (run("complete-multispan", joined, 2, 0, 2, joined_domain,
	    true, true, joined_result) &&
	    (!enclosed(0.25, joined_result.uv_minimum[0],
		joined_result.uv_maximum[0]) ||
	    !enclosed(0.75, joined_result.uv_minimum[0],
		joined_result.uv_maximum[0]) ||
	    !enclosed(0.25, joined_result.uv_minimum[1],
		joined_result.uv_maximum[1]) ||
	    !enclosed(0.5, joined_result.uv_minimum[1],
		joined_result.uv_maximum[1]) ||
	    !(joined_result.derivative_minimum[0] > 0.0) ||
	    !(joined_result.derivative_minimum[1] < 0.0) ||
	    !(joined_result.derivative_maximum[1] > 0.0))) {
	std::printf("FAIL: trim interval multispan hull/signs\n");
	failures++;
    }
    struct rt_brep_trim_interval_test_span unordered[2] = {
	joined[1], joined[0]
    };
    struct rt_brep_trim_interval_test_result result = {};
    run("unordered-complete-multispan", unordered, 2, 0, 2,
	joined_domain, true, true, result);

    struct rt_brep_trim_interval_test_span reversed[1] = {};
    set_line(reversed[0], 0.0, 1.0, 1.0, 0.0, 0.0, 1.0);
    const fastf_t full_domain[2] = {0.0, 1.0};
    struct rt_brep_trim_interval_test_result reversed_result = {};
    if (run("reversed-direction", reversed, 1, 0, 1, full_domain,
	    true, true, reversed_result) &&
	    (!(reversed_result.derivative_maximum[0] < 0.0) ||
	    !(reversed_result.derivative_minimum[1] > 0.0))) {
	std::printf("FAIL: trim interval reversed derivative signs\n");
	failures++;
    }

    const fastf_t boundary_domain[2] = {0.25, 0.5};
    run("exact-span-boundary", joined, 2, 0, 2, boundary_domain,
	true, true, result);
    run("missing-provenance", joined, 2, 2, 1, joined_domain,
	true, false, result);
    run("zero-span-provenance", joined, 2, 0, 0, joined_domain,
	true, false, result);

    struct rt_brep_trim_interval_test_span incomplete[2] = {};
    set_line(incomplete[0], 0.0, 0.4, 0.0, 0.0, 0.4, 0.4);
    set_line(incomplete[1], 0.6, 1.0, 0.6, 0.4, 1.0, 0.0);
    const fastf_t incomplete_domain[2] = {0.3, 0.7};
    run("incomplete-coverage", incomplete, 2, 0, 2,
	incomplete_domain, true, false, result);

    struct rt_brep_trim_interval_test_span overlap[3] = {};
    set_line(overlap[0], 0.0, 0.35, 0.0, 0.0, 0.35, 0.0);
    set_line(overlap[1], 0.0, 0.35, 0.0, 0.1, 0.35, 0.1);
    set_line(overlap[2], 0.65, 1.0, 0.65, 0.0, 1.0, 0.0);
    run("overlap-does-not-mask-gap", overlap, 3, 0, 3, full_domain,
	true, false, result);

    struct rt_brep_trim_interval_test_span bad_weight[1] = {rational[0]};
    bad_weight[0].control[1][2] = 0.0;
    run("zero-weight", bad_weight, 1, 0, 1, full_domain,
	true, false, result);
    bad_weight[0] = rational[0];
    bad_weight[0].control[1][2] = -1.0;
    run("negative-weight", bad_weight, 1, 0, 1, full_domain,
	true, false, result);

    struct rt_brep_trim_interval_test_result turning_result = {};
    if (run("turning-derivative", rational, 1, 0, 1, full_domain,
	    true, true, turning_result) &&
	    (turning_result.derivative_minimum[1] > 0.0 ||
	    turning_result.derivative_maximum[1] < 0.0)) {
	std::printf("FAIL: trim interval turning derivative excludes zero\n");
	failures++;
    }

    struct rt_brep_trim_interval_test_span nonfinite[1] = {rational[0]};
    nonfinite[0].control[0][0] = INFINITY;
    run("nonfinite-control", nonfinite, 1, 0, 1, full_domain,
	false, false, result);
    fastf_t nonfinite_domain[2] = {NAN, 1.0};
    run("nonfinite-domain", rational, 1, 0, 1, nonfinite_domain,
	false, false, result);

    if (!failures)
	std::printf("BREP rational trim interval corpus: PASS cases=%zu "
	    "enclosures=%zu rejected=%zu\n", cases, enclosures, rejected);
    return failures;
}


static int
check_brep_source_union_solver()
{
    const fastf_t coefficient_error[2] = {0.0, 0.0};
    int failures = 0;
    size_t systems = 0;
    size_t certified = 0;
    size_t rejected = 0;

    const auto set_box = [](struct rt_brep_source_union_test_box &box,
	    double u_minimum, double u_maximum, double v_minimum,
	    double v_maximum, double t_minimum, double t_maximum, int role) {
	box.uv_minimum[0] = u_minimum;
	box.uv_maximum[0] = u_maximum;
	box.uv_minimum[1] = v_minimum;
	box.uv_maximum[1] = v_maximum;
	box.t_minimum = t_minimum;
	box.t_maximum = t_maximum;
	box.role = role;
    };
    const auto run_case = [&](const char *name,
	    const fastf_t *first_coefficients,
	    const fastf_t *second_coefficients, int order,
	    const struct rt_brep_source_union_test_box *boxes,
	    size_t box_count, const fastf_t root[2], size_t expected_eligible,
	    size_t expected_component, bool expected_complete,
	    bool expected_attempted, bool expected_certified) {
	struct rt_brep_source_union_test_result result = {};
	systems++;
	if (!_rt_brep_source_union_test(first_coefficients,
		second_coefficients, order, order, coefficient_error, boxes,
		box_count, root, &result) ||
		result.eligible_boxes != expected_eligible ||
		result.root_boxes != 1 ||
		result.component_boxes != expected_component ||
		result.component_complete != (expected_complete ? 1 : 0) ||
		result.krawczyk_attempted != (expected_attempted ? 1 : 0) ||
		result.certified != (expected_certified ? 1 : 0)) {
	    std::printf("FAIL: source-union solver %s "
		"eligible/root/component=%zu/%zu/%zu expected=%zu/1/%zu "
		"complete/attempted/certified=%d/%d/%d expected=%d/%d/%d\n",
		name, result.eligible_boxes, result.root_boxes,
		result.component_boxes, expected_eligible, expected_component,
		result.component_complete, result.krawczyk_attempted,
		result.certified, expected_complete ? 1 : 0,
		expected_attempted ? 1 : 0, expected_certified ? 1 : 0);
	    failures++;
	    return;
	}
	if (result.certified)
	    certified++;
	else
	    rejected++;
    };

    fastf_t higher_order[2][16] = {};
    const double higher_u[4] = {
	-17.0 / 32.0, -13.0 / 96.0, 13.0 / 96.0, 17.0 / 32.0
    };
    const double linear_v[4] = {-0.5, -1.0 / 6.0, 1.0 / 6.0, 0.5};
    for (int i = 0; i < 4; ++i) {
	for (int j = 0; j < 4; ++j) {
	    const size_t index = (size_t)i * 4 + j;
	    higher_order[0][index] = higher_u[i];
	    higher_order[1][index] = linear_v[j];
	}
    }
    const fastf_t centered_root[2] = {0.5, 0.5};
    struct rt_brep_source_union_test_box connected[2] = {};
    set_box(connected[0], 0.46875, 0.53125, 0.46875, 0.53125,
	1.0, 1.125, RT_BREP_SOURCE_UNION_TEST_ROOT);
    set_box(connected[1], 0.53125, 0.5625, 0.46875, 0.53125,
	1.0625, 1.1875, RT_BREP_SOURCE_UNION_TEST_CANDIDATE);
    run_case("higher-order-unique", higher_order[0], higher_order[1], 4,
	connected, 2, centered_root, 2, 2, true, true, true);

    struct rt_brep_source_union_test_box uv_disconnected[2] = {
	connected[0], connected[1]
    };
    uv_disconnected[1].uv_minimum[0] = 0.625;
    uv_disconnected[1].uv_maximum[0] = 0.6875;
    run_case("uv-disconnected", higher_order[0], higher_order[1], 4,
	uv_disconnected, 2, centered_root, 2, 1, false, false, false);

    struct rt_brep_source_union_test_box t_disconnected[2] = {
	connected[0], connected[1]
    };
    t_disconnected[1].t_minimum = 1.25;
    t_disconnected[1].t_maximum = 1.375;
    run_case("t-disconnected", higher_order[0], higher_order[1], 4,
	t_disconnected, 2, centered_root, 2, 1, false, false, false);

    fastf_t multiple_root[2][9] = {};
    const double multiple_u[3] = {15.0 / 64.0, -17.0 / 64.0,
	15.0 / 64.0};
    const double linear_quadratic_v[3] = {-0.5, 0.0, 0.5};
    for (int i = 0; i < 3; ++i) {
	for (int j = 0; j < 3; ++j) {
	    const size_t index = (size_t)i * 3 + j;
	    multiple_root[0][index] = multiple_u[i];
	    multiple_root[1][index] = linear_quadratic_v[j];
	}
    }
    const fastf_t first_multiple_root[2] = {0.375, 0.5};
    struct rt_brep_source_union_test_box multiple_boxes[2] = {};
    set_box(multiple_boxes[0], 0.3125, 0.4375, 0.4375, 0.5625,
	1.0, 1.125, RT_BREP_SOURCE_UNION_TEST_ROOT);
    set_box(multiple_boxes[1], 0.4375, 0.6875, 0.4375, 0.5625,
	1.0625, 1.1875, RT_BREP_SOURCE_UNION_TEST_CANDIDATE);
    run_case("multiple-root-hull", multiple_root[0], multiple_root[1], 3,
	multiple_boxes, 2, first_multiple_root, 2, 2, true, true, false);

    fastf_t singular[2][9] = {};
    const double singular_u[3] = {0.25, -0.25, 0.25};
    for (int i = 0; i < 3; ++i) {
	for (int j = 0; j < 3; ++j) {
	    const size_t index = (size_t)i * 3 + j;
	    singular[0][index] = singular_u[i];
	    singular[1][index] = linear_quadratic_v[j];
	}
    }
    struct rt_brep_source_union_test_box singular_boxes[3] = {};
    set_box(singular_boxes[0], 0.46875, 0.53125, 0.46875, 0.53125,
	1.0, 1.125, RT_BREP_SOURCE_UNION_TEST_ROOT);
    set_box(singular_boxes[1], 0.4375, 0.46875, 0.46875, 0.53125,
	1.0625, 1.1875, RT_BREP_SOURCE_UNION_TEST_CANDIDATE);
    set_box(singular_boxes[2], 0.53125, 0.5625, 0.46875, 0.53125,
	1.0625, 1.1875, RT_BREP_SOURCE_UNION_TEST_CANDIDATE);
    run_case("singular-root", singular[0], singular[1], 3,
	singular_boxes, 3, centered_root, 3, 3, true, true, false);

    const fastf_t invalid_root[2] = {0.75, 0.5};
    struct rt_brep_source_union_test_result invalid_result = {};
    if (_rt_brep_source_union_test(higher_order[0], higher_order[1], 4, 4,
	    coefficient_error, connected, 2, invalid_root, &invalid_result)) {
	std::printf("FAIL: source-union solver invalid root-box provenance\n");
	failures++;
    }

    if (!failures)
	std::printf("BREP source-union solver corpus: PASS systems=%zu "
	    "certified/rejected=%zu/%zu invalid=1\n", systems, certified,
	    rejected);
    return failures;
}


static int
check_brep_fold_solver()
{
    const double equation_transform[][2][2] = {
	{{1.0, 0.0}, {0.0, 1.0}},
	{{0.0, 1.0}, {1.0, 0.0}},
	{{-1.0, 0.0}, {0.0, 1.0}},
	{{std::ldexp(1.0, 100), 0.0},
	 {0.0, std::ldexp(1.0, -100)}},
	{{0.0, std::ldexp(1.0, -100)},
	 {-std::ldexp(1.0, 100), 0.0}}
    };
    struct parameter_variant {
	bool reverse_u;
	bool reverse_v;
	bool swap;
    } parameter_variants[] = {
	{false, false, false},
	{true, false, false},
	{false, true, false},
	{false, false, true},
	{true, true, true}
    };
    const int separation_exponents[] = {4, 8, 12, 16, 20, 24};
    const double regular_root = 0.37;
    int failures = 0;
    size_t systems = 0;
    size_t expected_roots = 0;
    size_t maximum_regular_solves = 0;
    size_t certificate_boxes = 0;
    size_t expansion_certificate_boxes = 0;
    size_t expansion_certificate_high_water = 0;
    size_t corridor_boxes = 0;
    size_t graph_corridor_cases = 0;
    size_t graph_strip_cases = 0;
    size_t graph_contact_cases = 0;
    size_t graph_proof_boxes = 0;
    size_t graph_proof_contractions = 0;
    size_t graph_boundary_available = 0;
    size_t graph_boundary_certificates = 0;
    size_t graph_boundary_contractions = 0;
    size_t graph_strip_boxes = 0;
    size_t graph_strip_contractions = 0;
    size_t graph_expansion_high_water = 0;
    size_t certificates_below_old_cutoff = 0;
    double minimum_separation = DBL_MAX;
    double maximum_root_error = 0.0;
    double maximum_residual_ratio = 0.0;
    double minimum_determinant_ratio = DBL_MAX;

    const auto run_case = [&](double epsilon, size_t transform_index,
	    size_t variant_index, size_t expected_count,
	    double root_separation) {
	double base[2][9];
	for (int i = 0; i < 3; ++i) {
	    for (int j = 0; j < 3; ++j) {
		const size_t index = (size_t)i * 3 + j;
		base[0][index] = 0.5 * i - regular_root;
		const double weak_control[3] = {
		    0.25 - epsilon, -0.25 - epsilon,
		    0.25 - epsilon
		};
		base[1][index] = weak_control[j];
	    }
	}
	const parameter_variant &variant = parameter_variants[variant_index];
	double parameterized[2][9];
	for (int equation = 0; equation < 2; ++equation) {
	    double reversed[9];
	    for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
		    const int source_i = variant.reverse_u ? 2 - i : i;
		    const int source_j = variant.reverse_v ? 2 - j : j;
		    reversed[(size_t)i * 3 + j] =
			base[equation][(size_t)source_i * 3 + source_j];
		}
	    }
	    for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
		    parameterized[equation][(size_t)i * 3 + j] =
			variant.swap ? reversed[(size_t)j * 3 + i] :
			reversed[(size_t)i * 3 + j];
		}
	    }
	}
	double coefficients[2][9];
	double coefficient_scale = 0.0;
	for (int equation = 0; equation < 2; ++equation) {
	    for (size_t i = 0; i < 9; ++i) {
		coefficients[equation][i] =
		    equation_transform[transform_index][equation][0] *
			parameterized[0][i] +
		    equation_transform[transform_index][equation][1] *
			parameterized[1][i];
		coefficient_scale = std::max(coefficient_scale,
		    fabs(coefficients[equation][i]));
	    }
	}
	const fastf_t coefficient_error[2] = {0.0, 0.0};
	struct rt_brep_fold_test_result observed = {};
	struct rt_brep_fold_test_result repeated = {};
	systems++;
	expected_roots += expected_count;
	if (!_rt_brep_fold_test(coefficients[0], coefficients[1], 3, 3,
		coefficient_error, &observed) ||
		!_rt_brep_fold_test(coefficients[0], coefficients[1], 3, 3,
		    coefficient_error, &repeated)) {
	    std::printf("FAIL: fold solver rejected epsilon=%.17g "
		"transform=%zu variant=%zu\n", epsilon, transform_index,
		variant_index);
	    failures++;
	    return;
	}
	bool deterministic = observed.frame_available == repeated.frame_available &&
	    observed.regular_direction == repeated.regular_direction &&
	    observed.capacity_exhausted == repeated.capacity_exhausted &&
	    observed.samples == repeated.samples &&
	    observed.regular_solves == repeated.regular_solves &&
	    observed.brackets == repeated.brackets &&
	    observed.candidate_count == repeated.candidate_count;
	for (size_t i = 0; deterministic && i < observed.candidate_count; ++i) {
	    deterministic = std::memcmp(observed.uv[i], repeated.uv[i],
		sizeof(observed.uv[i])) == 0 &&
		std::memcmp(&observed.residual[i], &repeated.residual[i],
		    sizeof(observed.residual[i])) == 0 &&
		std::memcmp(&observed.weak_bracket_width[i],
		    &repeated.weak_bracket_width[i],
		    sizeof(observed.weak_bracket_width[i])) == 0;
	}
	if (!deterministic) {
	    std::printf("FAIL: fold solver nondeterministic epsilon=%.17g "
		"transform=%zu variant=%zu\n", epsilon, transform_index,
		variant_index);
	    failures++;
	    return;
	}
	maximum_regular_solves = std::max(maximum_regular_solves,
	    observed.regular_solves);
	if (!observed.frame_available || observed.samples != 17 ||
		observed.regular_solves < observed.samples ||
		observed.regular_solves > 147 ||
		observed.brackets != expected_count ||
		observed.capacity_exhausted ||
		observed.candidate_count != expected_count) {
	    std::printf("FAIL: fold candidate/work epsilon=%.17g "
		"transform=%zu variant=%zu observed=%zu expected=%zu "
		"frame/capacity=%d/%d samples/solves/brackets=%zu/%zu/%zu\n",
		epsilon, transform_index,
		variant_index, observed.candidate_count, expected_count,
		observed.frame_available, observed.capacity_exhausted,
		observed.samples, observed.regular_solves, observed.brackets);
	    failures++;
	    return;
	}
	if (!expected_count)
	    return;
	double expected[2][2] = {};
	if (expected_count == 1) {
	    expected[0][0] = regular_root;
	    expected[0][1] = 0.5;
	} else {
	    expected[0][0] = expected[1][0] = regular_root;
	    expected[0][1] = 0.5 - 0.5 * root_separation;
	    expected[1][1] = 0.5 + 0.5 * root_separation;
	}
	for (size_t root_index = 0; root_index < expected_count;
		++root_index) {
	    if (variant.reverse_u)
		expected[root_index][0] = 1.0 - expected[root_index][0];
	    if (variant.reverse_v)
		expected[root_index][1] = 1.0 - expected[root_index][1];
	    if (variant.swap)
		std::swap(expected[root_index][0], expected[root_index][1]);
	}
	bool used[RT_BREP_FOLD_TEST_MAX_CANDIDATES] = {};
	for (size_t expected_index = 0; expected_index < expected_count;
		++expected_index) {
	    size_t best_index = SIZE_MAX;
	    double best_error = DBL_MAX;
	    for (size_t candidate_index = 0;
		    candidate_index < observed.candidate_count;
		    ++candidate_index) {
		if (used[candidate_index])
		    continue;
		const double error = hypot(
		    observed.uv[candidate_index][0] -
			expected[expected_index][0],
		    observed.uv[candidate_index][1] -
			expected[expected_index][1]);
		if (error < best_error) {
		    best_error = error;
		    best_index = candidate_index;
		}
	    }
	    if (best_index == SIZE_MAX || best_error > 2.0e-12) {
		std::printf("FAIL: fold candidate location epsilon=%.17g "
		    "transform=%zu variant=%zu root=%zu error=%.17g\n",
		    epsilon, transform_index, variant_index, expected_index,
		    best_error);
		failures++;
		continue;
	    }
	    used[best_index] = true;
	    maximum_root_error = std::max(maximum_root_error, best_error);
	    const double residual_ratio = coefficient_scale > 0.0 ?
		observed.residual[best_index] / coefficient_scale :
		observed.residual[best_index];
	    maximum_residual_ratio = std::max(maximum_residual_ratio,
		residual_ratio);
	    if (!(residual_ratio <= 2.0e-12)) {
		std::printf("FAIL: fold candidate residual epsilon=%.17g "
		    "transform=%zu variant=%zu root=%zu ratio=%.17g\n",
		    epsilon, transform_index, variant_index, expected_index,
		    residual_ratio);
		failures++;
	    }
	}
    };

    for (size_t transform_index = 0; transform_index <
	    sizeof(equation_transform) / sizeof(equation_transform[0]);
	    ++transform_index) {
	for (size_t variant_index = 0; variant_index <
		sizeof(parameter_variants) / sizeof(parameter_variants[0]);
		++variant_index) {
	    for (size_t separation_index = 0; separation_index <
		    sizeof(separation_exponents) /
		    sizeof(separation_exponents[0]); ++separation_index) {
		const double separation = std::ldexp(1.0,
		    -separation_exponents[separation_index]);
		const double epsilon = 0.25 * separation * separation;
		minimum_separation = std::min(minimum_separation, separation);
		run_case(epsilon, transform_index, variant_index, 2,
		    separation);
	    }
	    run_case(0.0, transform_index, variant_index, 1, 0.0);
	    run_case(-std::ldexp(1.0, -20), transform_index, variant_index,
		0, 0.0);
	}
    }

    const int parameter_shear_exponents[] = {0, 10, 20};
    for (size_t shear_index = 0; shear_index <
	    sizeof(parameter_shear_exponents) /
	    sizeof(parameter_shear_exponents[0]); ++shear_index) {
	const double shear = std::ldexp(1.0,
	    parameter_shear_exponents[shear_index]);
	for (size_t separation_index = 0; separation_index <
		sizeof(separation_exponents) /
		sizeof(separation_exponents[0]); ++separation_index) {
	    const double separation = std::ldexp(1.0,
		-separation_exponents[separation_index]);
	    const double delta = 0.5 * separation;
	    const double epsilon = delta * delta;
	    const double regular_width = 0.25;
	    const double weak_width = std::min(0.5 * delta,
		regular_width / (8.0 * shear));
	    for (int side = -1; side <= 1; side += 2) {
		double corridor_coefficients[2][9];
		double corridor_errors[2][9] = {};
		const double corridor_minimum = side < 0 ?
		    -1.5 * delta : 0.25 * delta;
		const double corridor_maximum = side < 0 ?
		    -0.25 * delta : 1.5 * delta;
		const double corridor_width =
		    corridor_maximum - corridor_minimum;
		const double corridor_regular_width =
		    4.0 * shear * delta + 0.25;
		const double corridor_weak_control[3] = {
		    corridor_minimum * corridor_minimum - epsilon,
		    corridor_minimum * corridor_maximum - epsilon,
		    corridor_maximum * corridor_maximum - epsilon
		};
		for (int i = 0; i < 3; ++i) {
		    for (int j = 0; j < 3; ++j) {
			const size_t index = (size_t)i * 3 + j;
			const double weak_parameter = corridor_minimum +
			    0.5 * j * corridor_width;
			corridor_coefficients[0][index] =
			    corridor_regular_width * (0.5 * i - 0.5) +
			    shear * (weak_parameter - side * delta);
			corridor_coefficients[1][index] =
			    corridor_weak_control[j];
		    }
		}
		struct rt_brep_corridor_test_result corridor = {};
		corridor_boxes++;
		if (!_rt_brep_corridor_test(corridor_coefficients[0],
			corridor_errors[0], corridor_coefficients[1],
			corridor_errors[1], 3, 3, 0, &corridor) ||
			!corridor.available || !corridor.unique ||
			corridor.determinant_sign != side) {
		    std::printf("FAIL: fold corridor proof unavailable "
			"separation=%.17g shear=2^%d side=%d "
			"available/derivative/boundary/determinant/unique="
			"%d/%d/%d/%d/%d sign=%d\n", separation,
			parameter_shear_exponents[shear_index], side,
			corridor.available, corridor.regular_derivative_signed,
			corridor.regular_boundaries_opposed,
			corridor.determinant_signed, corridor.unique,
			corridor.determinant_sign);
		    failures++;
		}

		double coefficients[2][9];
		double errors[2][9] = {};
		const double weak_minimum = side * delta - 0.5 * weak_width;
		const double weak_maximum = weak_minimum + weak_width;
		const double weak_control[3] = {
		    weak_minimum * weak_minimum - epsilon,
		    weak_minimum * weak_maximum - epsilon,
		    weak_maximum * weak_maximum - epsilon
		};
		for (int i = 0; i < 3; ++i) {
		    for (int j = 0; j < 3; ++j) {
			const size_t index = (size_t)i * 3 + j;
			coefficients[0][index] =
			    regular_width * (0.5 * i - 0.5) +
			    shear * weak_width * (0.5 * j - 0.5);
			coefficients[1][index] = weak_control[j];
		    }
		}
		struct rt_brep_determinant_test_result determinant = {};
		struct rt_brep_krawczyk_test_result krawczyk = {};
		struct rt_brep_krawczyk_test_result expansion_krawczyk = {};
		size_t expansion_high_water = 0;
		const fastf_t root[2] = {0.5, 0.5};
		certificate_boxes++;
		if (!_rt_brep_determinant_test(coefficients[0], errors[0],
			coefficients[1], errors[1], 3, 3, &determinant) ||
			!_rt_brep_krawczyk_test(coefficients[0], errors[0],
			    coefficients[1], errors[1], 3, 3, root,
			    &krawczyk) || !krawczyk.available ||
			!krawczyk.certified ||
			!_rt_brep_expansion_krawczyk_test(coefficients[0],
			    errors[0], coefficients[1], errors[1], 3, 3,
			    root, &expansion_krawczyk,
			    &expansion_high_water) ||
			!expansion_krawczyk.available ||
			!expansion_krawczyk.certified) {
		    std::printf("FAIL: fold certificate unavailable "
			"separation=%.17g shear=2^%d side=%d "
			"available/certified=%d/%d+%d/%d ratio=%.17g/%.17g\n",
			separation,
			parameter_shear_exponents[shear_index], side,
			krawczyk.available, krawczyk.certified,
			expansion_krawczyk.available,
			expansion_krawczyk.certified,
			krawczyk.determinant_ratio,
			expansion_krawczyk.determinant_ratio);
		    failures++;
		    continue;
		}
		expansion_certificate_boxes++;
		expansion_certificate_high_water = std::max(
		    expansion_certificate_high_water, expansion_high_water);
		bool determinant_signed = true;
		const size_t determinant_count =
		    (size_t)determinant.u_order * determinant.v_order;
		for (size_t i = 0; i < determinant_count; ++i) {
		    if ((side < 0 && !(determinant.maximum[i] < 0.0)) ||
			    (side > 0 && !(determinant.minimum[i] > 0.0))) {
			determinant_signed = false;
			break;
		    }
		}
		if (!determinant_signed) {
		    std::printf("FAIL: fold determinant sign unavailable "
			"separation=%.17g shear=2^%d side=%d\n", separation,
			parameter_shear_exponents[shear_index], side);
		    failures++;
		}
		minimum_determinant_ratio = std::min(minimum_determinant_ratio,
		    (double)krawczyk.determinant_ratio);
		if (krawczyk.determinant_ratio <
			BREP_INTERSECTION_ROOT_EPSILON)
		    certificates_below_old_cutoff++;
	    }
	}
    }

    /*
     * Exercise the bounded implicit-graph proof independently of the direct
     * whole-box result.  A coupled term varies the determinant away from the
     * regular graph enough to make its whole-box sign uncertain, while all
     * coefficients, errors, domains, and scales are dyadic.  Restricting the
     * original determinant onto the contracted graph enclosure must recover
     * the strict sign without a reparameterization scale loss.
     */
    const int graph_scale_exponents[] = {-100, 0, 100};
    const auto run_graph_box = [&](double q_minimum, double q_maximum,
	    double epsilon, int side, bool swap, int scale_exponent,
	    bool test_determinant, bool expected_excluded,
	    const char *label) {
	fastf_t minimum[2][9] = {};
	fastf_t maximum[2][9] = {};
	const double regular_width = 0.25;
	const double delta = sqrt(std::max(0.0, epsilon));
	const double coupling = 8.0 * delta;
	const double q_control[3] = {
	    q_minimum, 0.5 * q_minimum + 0.5 * q_maximum, q_maximum
	};
	const double q_squared_control[3] = {
	    q_minimum * q_minimum,
	    q_minimum * q_maximum,
	    q_maximum * q_maximum
	};
	const double base_error = epsilon > 0.0 ?
	    std::ldexp(epsilon, -40) : 0.0;
	for (int i = 0; i < 3; ++i) {
	    const double phi = regular_width * (0.5 * i - 0.5);
	    for (int j = 0; j < 3; ++j) {
		const size_t source_index = (size_t)i * 3 + j;
		const size_t index = swap ? (size_t)j * 3 + i : source_index;
		const double value[2] = {
		    std::ldexp(phi, scale_exponent),
		    std::ldexp(q_squared_control[j] - epsilon +
			coupling * phi * q_control[j], -scale_exponent)
		};
		const double error[2] = {
		    std::ldexp(base_error, scale_exponent),
		    std::ldexp(base_error, -scale_exponent)
		};
		for (int equation = 0; equation < 2; ++equation) {
		    minimum[equation][index] = value[equation] -
			error[equation];
		    maximum[equation][index] = value[equation] +
			error[equation];
		}
	    }
	}
	struct rt_brep_fold_graph_test_result observed = {};
	const int regular_direction = swap ? 1 : 0;
	if (!_rt_brep_fold_graph_test(minimum[0], maximum[0], minimum[1],
		maximum[1], 3, 3, regular_direction,
		test_determinant ? 1 : 0, test_determinant ? 0 : 1,
		&observed) || !observed.available ||
		!observed.regular_derivative_signed ||
		!observed.regular_boundaries_opposed) {
	    std::printf("FAIL: fold graph unavailable %s epsilon=%.17g "
		"side=%d swap=%d scale=%d available/regular/boundary="
		"%d/%d/%d\n", label, epsilon, side, swap, scale_exponent,
		observed.available, observed.regular_derivative_signed,
		observed.regular_boundaries_opposed);
	    failures++;
	    return;
	}
	graph_expansion_high_water = std::max(graph_expansion_high_water,
	    observed.expansion_high_water);
	graph_boundary_available += observed.boundary_existence_available ? 1 : 0;
	graph_boundary_certificates +=
	    observed.boundary_existence_certified ? 1 : 0;
	graph_boundary_contractions +=
	    observed.boundary_existence_contractions;
	if (observed.expansion_high_water >= RT_BREP_EXPANSION_CAPACITY) {
	    std::printf("FAIL: fold graph expansion capacity %s high=%zu\n",
		label, observed.expansion_high_water);
	    failures++;
	}
	if (test_determinant) {
	    graph_corridor_cases++;
	    graph_proof_boxes += observed.graph_boxes;
	    graph_proof_contractions += observed.graph_contractions;
	    const int expected_sign = swap ? -side : side;
	    if (observed.whole_determinant_signed ||
		    !observed.graph_determinant_signed ||
		    observed.determinant_sign != expected_sign ||
		    !observed.graph_boxes || !observed.graph_contractions ||
		    !observed.boundary_existence_available ||
		    !observed.boundary_existence_certified ||
		    !observed.boundary_existence_contractions) {
		std::printf("FAIL: fold implicit determinant %s epsilon=%.17g "
		    "side=%d swap=%d scale=%d whole/graph/sign=%d/%d/%d "
		    "expected=%d boxes/contractions=%zu/%zu "
		    "boundary=%d/%d/%zu "
		    "failure=r%zu/d%zu/s%zu/z%zu/w%zu\n", label,
		    epsilon, side, swap, scale_exponent,
		    observed.whole_determinant_signed,
		    observed.graph_determinant_signed,
		    observed.determinant_sign, expected_sign,
		    observed.graph_boxes, observed.graph_contractions,
		    observed.boundary_existence_available,
		    observed.boundary_existence_certified,
		    observed.boundary_existence_contractions,
		    observed.graph_restriction_failures,
		    observed.graph_determinant_failures,
		    observed.graph_sign_conflicts,
		    observed.graph_depth_exhausted,
		    observed.graph_workspace_exhausted);
		failures++;
	    }
	} else {
	    graph_strip_boxes += observed.strip_boxes;
	    graph_strip_contractions += observed.strip_contractions;
	    if (epsilon > 0.0)
		graph_strip_cases++;
	    else
		graph_contact_cases++;
	    if ((observed.system_excluded != (expected_excluded ? 1 : 0)) ||
		    !observed.boundary_existence_available ||
		    observed.boundary_existence_certified ||
		    !observed.strip_boxes ||
		    (expected_excluded && !observed.strip_contractions) ||
		    observed.strip_restriction_failures ||
		    observed.strip_arithmetic_failures ||
		    observed.strip_workspace_exhausted ||
		    observed.strip_depth_exhausted !=
			(expected_excluded ? 0u : 1u)) {
		std::printf("FAIL: fold implicit strip %s epsilon=%.17g "
		    "side=%d swap=%d scale=%d excluded=%d expected=%d "
		    "boundary=%d/%d/%zu boxes/contractions=%zu/%zu "
		    "failure=r%zu/a%zu/z%zu/w%zu\n",
		    label, epsilon, side,
		    swap, scale_exponent, observed.system_excluded,
		    expected_excluded ? 1 : 0,
		    observed.boundary_existence_available,
		    observed.boundary_existence_certified,
		    observed.boundary_existence_contractions,
		    observed.strip_boxes,
		    observed.strip_contractions,
		    observed.strip_restriction_failures,
		    observed.strip_arithmetic_failures,
		    observed.strip_depth_exhausted,
		    observed.strip_workspace_exhausted);
		failures++;
	    }
	}
    };

    for (size_t separation_index = 0; separation_index <
	    sizeof(separation_exponents) / sizeof(separation_exponents[0]);
	    ++separation_index) {
	const double delta = std::ldexp(1.0,
	    -separation_exponents[separation_index] - 1);
	const double epsilon = delta * delta;
	for (int side = -1; side <= 1; side += 2) {
	    const double corridor_minimum = side < 0 ?
		-1.5 * delta : 0.25 * delta;
	    const double corridor_maximum = side < 0 ?
		-0.25 * delta : 1.5 * delta;
	    const double strip_minimum = side < 0 ? -0.5 * delta : 0.0;
	    const double strip_maximum = side < 0 ? 0.0 : 0.5 * delta;
	    for (int swap = 0; swap <= 1; ++swap) {
		for (size_t scale_index = 0; scale_index <
			sizeof(graph_scale_exponents) /
			sizeof(graph_scale_exponents[0]); ++scale_index) {
		    run_graph_box(corridor_minimum, corridor_maximum,
			epsilon, side, swap != 0,
			graph_scale_exponents[scale_index], true, false,
			"corridor");
		    run_graph_box(strip_minimum, strip_maximum, epsilon,
			side, swap != 0, graph_scale_exponents[scale_index],
			false, true, "strip");
		}
	    }
	}
    }
    run_graph_box(0.0, 0.5, 0.0, 1, false, 0, false, false,
	"contact");

    if (!failures) {
	std::printf("BREP fold solver corpus: PASS systems=%zu roots=%zu "
	    "min-separation=%.9g max-error/residual=%.9g/%.9g "
	    "max-regular-solves=%zu certificates=%zu/%zu+%zu/%zu "
	    "corridors=%zu graph=%zu/%zu/%zu boxes=%zu/%zu+%zu/%zu "
	    "boundary=%zu/%zu/%zu "
	    "high=%zu "
	    "min-det-ratio=%.9g\n",
	    systems, expected_roots, minimum_separation,
	    maximum_root_error, maximum_residual_ratio,
	    maximum_regular_solves, certificate_boxes,
	    certificates_below_old_cutoff, expansion_certificate_boxes,
	    expansion_certificate_high_water, corridor_boxes,
	    graph_corridor_cases, graph_strip_cases, graph_contact_cases,
	    graph_proof_boxes, graph_proof_contractions, graph_strip_boxes,
	    graph_strip_contractions, graph_boundary_available,
	    graph_boundary_certificates, graph_boundary_contractions,
	    graph_expansion_high_water,
	    minimum_determinant_ratio);
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
			!brep_trace_regular_event_stream_valid(trace, 1) ||
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
	     RT_BREP_PREPARED_FALLBACK_NONE}},
	{"extreme-condition", ON_3dVector(0.001, 0.05, 1.0),
	    ON_3dVector(17.0, -29.0, 43.0),
	    ON_3dVector(-0.6, 0.3, 1.0), 0.583, 1.0e-4,
	    {RT_BREP_PREPARED_FALLBACK_NONE,
	     RT_BREP_PREPARED_FALLBACK_NONE,
	     RT_BREP_PREPARED_FALLBACK_NONE}},
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
	size_t fold_attempts = 0;
	size_t fold_candidates = 0;
	size_t fold_certified = 0;
	double fold_minimum_ratio = DBL_MAX;
	size_t expansion_attempts = 0;
	size_t expansion_available = 0;
	size_t expansion_certified = 0;
	size_t expansion_failures = 0;
	size_t expansion_high_water = 0;
	double expansion_minimum_ratio = DBL_MAX;
	double minimum_chord_ratio = DBL_MAX;
	double maximum_chord_ratio = 0.0;
	double maximum_endpoint_error = 0.0;
    } grazing[sizeof(grazing_clearance_ratios) /
	sizeof(grazing_clearance_ratios[0])];
    int failures = 0;
    size_t total_rays = 0;
    size_t total_krawczyk = 0;
    size_t total_boxes = 0;
    size_t total_rotated_hull_attempts = 0;
    size_t total_rotated_hull_exclusions = 0;
    size_t total_rotated_hull_inconclusive = 0;
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
    size_t grazing_expansion_ratchets = 0;
    size_t grazing_corridor_ratchets = 0;
    size_t grazing_fold_event_ratchets = 0;
    size_t grazing_gap_maximum_boxes = 0;
    size_t grazing_gap_rotated_exclusions = 0;

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
		summary.fold_attempts += trace.surface_fold_attempts;
		summary.fold_candidates += trace.surface_fold_candidates;
		summary.fold_certified +=
		    trace.surface_fold_krawczyk_certified;
		summary.expansion_attempts +=
		    trace.surface_fold_expansion_attempts;
		summary.expansion_available +=
		    trace.surface_fold_expansion_available;
		summary.expansion_certified +=
		    trace.surface_fold_expansion_certified;
		summary.expansion_failures +=
		    trace.surface_fold_expansion_failures;
		summary.expansion_high_water = std::max(
		    summary.expansion_high_water,
		    trace.surface_fold_expansion_high_water);
		if (trace.surface_fold_krawczyk_available) {
		    summary.fold_minimum_ratio = std::min(
			summary.fold_minimum_ratio,
			(double)trace.surface_fold_min_determinant_ratio);
		}
		if (trace.surface_fold_expansion_available) {
		    summary.expansion_minimum_ratio = std::min(
			summary.expansion_minimum_ratio,
			(double)trace.
			    surface_fold_expansion_min_determinant_ratio);
		}
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
		const double fold_event_error =
		    trace.surface_fold_resolved_pairs == 1 ?
		    std::max(fabs(trace.surface_fold_segment_in /
			direction_length - grazing_in),
		    fabs(trace.surface_fold_segment_out /
			direction_length - grazing_out)) : DBL_MAX;
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
		size_t expansion_certificate_floor = SIZE_MAX;
		if (!std::strcmp(test.name, "high-condition") &&
			ratio_index == 5)
		    expansion_certificate_floor = 2;
		if (!std::strcmp(test.name, "extreme-condition") &&
			ratio_index == 4)
		    expansion_certificate_floor = 2;
		if (!std::strcmp(test.name, "extreme-condition") &&
			ratio_index == 5)
		    expansion_certificate_floor = 0;
		const bool known_expansion_case =
		    expansion_certificate_floor != SIZE_MAX;
		if (known_expansion_case) {
		    const bool expansion_improved =
			trace.surface_fold_expansion_certified ||
			trace.surface_fold_expansion_best_image_excess <
			    trace.surface_fold_best_image_excess;
		    const bool expansion_bad = !known_expansion_case ||
			!trace.surface_fold_expansion_attempts ||
			trace.surface_fold_expansion_available !=
			    trace.surface_fold_expansion_attempts ||
			trace.surface_fold_expansion_failures ||
			!trace.surface_fold_expansion_contraction_attempts ||
			trace.surface_fold_expansion_contraction_attempts !=
			    trace.surface_fold_expansion_contracted +
			    trace.surface_fold_expansion_contraction_empty +
			    trace.surface_fold_expansion_contraction_unchanged ||
			(expansion_certificate_floor &&
			 !trace.surface_fold_expansion_contracted) ||
			!trace.surface_fold_expansion_high_water ||
			trace.surface_fold_expansion_high_water >=
			    RT_BREP_EXPANSION_CAPACITY ||
			trace.surface_fold_expansion_certified <
			    expansion_certificate_floor ||
			!expansion_improved;
		    grazing_expansion_ratchets++;
		    if (expansion_bad) {
			std::printf("FAIL: affine grazing expansion ratchet %s "
			    "ratio=%.3g reverse=%d "
			    "attempts/available/certified/failures="
			    "%zu/%zu/%zu/%zu contraction=%zu/%zu/%zu/%zu "
			    "floor=%zu "
			    "excess=%.3g/%.3g high-water=%zu/%d\n",
			    test.name,
			    grazing_clearance_ratios[ratio_index], reverse,
			    trace.surface_fold_expansion_attempts,
			    trace.surface_fold_expansion_available,
			    trace.surface_fold_expansion_certified,
			    trace.surface_fold_expansion_failures,
			    trace.surface_fold_expansion_contraction_attempts,
			    trace.surface_fold_expansion_contracted,
			    trace.surface_fold_expansion_contraction_empty,
			    trace.surface_fold_expansion_contraction_unchanged,
			    expansion_certificate_floor,
			    trace.surface_fold_best_image_excess,
			    trace.surface_fold_expansion_best_image_excess,
			    trace.surface_fold_expansion_high_water,
			    RT_BREP_EXPANSION_CAPACITY);
			failures++;
		    }
		    const bool corridor_bad =
			trace.surface_fold_corridor_attempts != 2 ||
			trace.surface_fold_corridor_available != 2 ||
			trace.surface_fold_corridor_regular_signed != 2 ||
			trace.surface_fold_corridor_boundaries_opposed != 2 ||
			trace.surface_fold_corridor_determinant_signed != 2 ||
			trace.surface_fold_corridor_unique != 2 ||
			trace.surface_fold_corridor_graph_attempts !=
			    trace.surface_fold_corridor_graph_certified ||
			trace.surface_fold_corridor_graph_failures ||
			trace.surface_fold_corridor_graph_restriction_failures ||
			trace.surface_fold_corridor_graph_determinant_failures ||
			trace.surface_fold_corridor_graph_sign_conflicts ||
			trace.surface_fold_corridor_graph_depth_exhausted ||
			trace.surface_fold_corridor_graph_workspace_exhausted ||
			trace.surface_fold_boundary_existence_attempts != 2 ||
			trace.surface_fold_boundary_existence_available != 2 ||
			trace.surface_fold_boundary_existence_certified != 2 ||
			!trace.surface_fold_boundary_existence_contractions ||
			trace.surface_fold_boundary_existence_failures ||
			trace.surface_fold_strip_excluded != 2 ||
			!trace.surface_fold_strip_boxes ||
			!trace.surface_fold_strip_contractions ||
			trace.surface_fold_strip_restriction_failures ||
			trace.surface_fold_strip_arithmetic_failures ||
			trace.surface_fold_strip_depth_exhausted ||
			trace.surface_fold_strip_workspace_exhausted ||
			trace.surface_fold_complete != 2 ||
			trace.surface_fold_corridor_failures ||
			!trace.surface_fold_corridor_high_water ||
			trace.surface_fold_corridor_high_water >=
			    RT_BREP_EXPANSION_CAPACITY;
		    grazing_corridor_ratchets++;
		    if (corridor_bad) {
			std::printf("FAIL: affine grazing corridor ratchet %s "
			    "ratio=%.3g reverse=%d corridor="
			    "%zu/%zu/%zu/%zu/%zu/%zu graph="
			    "%zu/%zu/%zu/%zu/%zu/%zu/%zu existence="
			    "%zu/%zu/%zu/%zu/%zu strip="
			    "%zu/%zu/%zu complete=%zu/%zu failures/high=%zu/%zu\n",
			    test.name, grazing_clearance_ratios[ratio_index],
			    reverse, trace.surface_fold_corridor_attempts,
			    trace.surface_fold_corridor_available,
			    trace.surface_fold_corridor_regular_signed,
			    trace.surface_fold_corridor_boundaries_opposed,
			    trace.surface_fold_corridor_determinant_signed,
			    trace.surface_fold_corridor_unique,
			    trace.surface_fold_corridor_graph_attempts,
			    trace.surface_fold_corridor_graph_certified,
			    trace.surface_fold_corridor_graph_failures,
			    trace.surface_fold_corridor_graph_restriction_failures,
			    trace.surface_fold_corridor_graph_determinant_failures,
			    trace.surface_fold_corridor_graph_depth_exhausted,
			    trace.surface_fold_corridor_graph_workspace_exhausted,
			    trace.surface_fold_boundary_existence_attempts,
			    trace.surface_fold_boundary_existence_available,
			    trace.surface_fold_boundary_existence_certified,
			    trace.surface_fold_boundary_existence_contractions,
			    trace.surface_fold_boundary_existence_failures,
			    trace.surface_fold_strip_excluded,
			    trace.surface_fold_strip_boxes,
			    trace.surface_fold_strip_contractions,
			    trace.surface_fold_complete,
			    trace.surface_fold_expansion_certified,
			    trace.surface_fold_corridor_failures,
			    trace.surface_fold_corridor_high_water);
			failures++;
		    }
		    bool fold_roots_clean =
			trace.stored_surface_fold_roots == 2;
		    for (size_t fold_index = 0; fold_roots_clean &&
			    fold_index < trace.stored_surface_fold_roots;
			    ++fold_index) {
			const struct rt_brep_trace_fold_root &root =
			    trace.surface_fold_roots_data[fold_index];
			fold_roots_clean = root.trim_status == 0 &&
			    root.hit_class == 0;
		    }
		    const bool fold_event_bad =
			trace.surface_fold_roots != 2 ||
			trace.stored_surface_fold_roots != 2 ||
			trace.surface_fold_root_overflow ||
			trace.surface_fold_root_failures ||
			trace.surface_fold_localization_attempts != 48 ||
			!trace.surface_fold_localization_certified ||
			!trace.surface_fold_localization_contractions ||
			trace.surface_fold_localization_failures ||
			trace.surface_fold_direction_checks != 2 ||
			trace.surface_fold_direction_mismatches ||
			trace.surface_fold_trim_queries != 2 ||
			trace.surface_fold_trim_failures ||
			!fold_roots_clean ||
			trace.surface_fold_topology_pairs != 1 ||
			trace.surface_fold_duplicate_events ||
			trace.surface_fold_material_pairs != 1 ||
			trace.surface_fold_void_pairs ||
			trace.surface_fold_resolved_pairs != 1 ||
			trace.surface_fold_subminimum_contacts ||
			trace.surface_fold_tolerance_ambiguous ||
			trace.surface_fold_unmatched_roots ||
			!(trace.surface_fold_pair_gap_min >
			    trace.surface_fold_minimum_t) ||
			!(trace.surface_fold_segment_in <
			    trace.surface_fold_segment_out) ||
			trace.surface_fold_promoted_pairs != 1 ||
			!brep_trace_fold_event_stream_valid(trace) ||
			fold_event_error > normalized_limit;
		    grazing_fold_event_ratchets++;
		    if (fold_event_bad) {
			std::printf("FAIL: affine fold event ratchet %s "
			    "ratio=%.3g reverse=%d roots=%zu/%zu/%zu/%zu "
			    "localize=%zu/%zu/%zu/%zu directions=%zu/%zu "
			    "events=%zu/%zu/%zu/%zu/%zu/%zu/%zu/%zu "
			    "gap/min=%.3g/%.3g/%.3g segment=%.17g/%.17g "
			    "error/limit=%.3g/%.3g\n", test.name,
			    grazing_clearance_ratios[ratio_index], reverse,
			    trace.surface_fold_roots,
			    trace.stored_surface_fold_roots,
			    trace.surface_fold_root_overflow,
			    trace.surface_fold_root_failures,
			    trace.surface_fold_localization_attempts,
			    trace.surface_fold_localization_certified,
			    trace.surface_fold_localization_contractions,
			    trace.surface_fold_localization_failures,
			    trace.surface_fold_direction_checks,
			    trace.surface_fold_direction_mismatches,
			    trace.surface_fold_topology_pairs,
			    trace.surface_fold_duplicate_events,
			    trace.surface_fold_material_pairs,
			    trace.surface_fold_void_pairs,
			    trace.surface_fold_resolved_pairs,
			    trace.surface_fold_subminimum_contacts,
			    trace.surface_fold_tolerance_ambiguous,
			    trace.surface_fold_unmatched_roots,
			    trace.surface_fold_pair_gap_min,
			    trace.surface_fold_pair_gap_max,
			    trace.surface_fold_minimum_t,
			    trace.surface_fold_segment_in,
			    trace.surface_fold_segment_out,
			    fold_event_error, normalized_limit);
			failures++;
		    }
		}
		if (analytic_resolved && !production_segments) {
		    grazing_resolved_misses++;
		    grazing_gap_maximum_boxes = std::max(
			grazing_gap_maximum_boxes,
			trace.surface_isolated_boxes);
		    grazing_gap_rotated_exclusions +=
			trace.surface_rotated_hull_exclusions;
		    std::printf("Ellipsoid affine fold gap %s ratio=%.3g "
			"reverse=%d boxes=%zu fold=%zu/%zu/%zu/%zu "
			"min-ratio/excess=%.3g/%.3g "
			"expansion=%zu/%zu/%zu/%zu/%.3g/%.3g/%zu "
			"corridor=%zu/%zu/%zu/%zu/%zu/%zu "
			"graph=%zu/%zu/%zu/%zu/%zu "
			"existence=%zu/%zu/%zu/%zu/%zu "
			"strip=%zu/%zu/%zu/%zu total=%zu/%zu "
			"fold-roots=%zu/%zu/%zu/%zu directions=%zu/%zu "
			"localize=%zu/%zu/%zu/%zu "
			"events=%zu/%zu/%zu/%zu/%zu/%zu/%zu/%zu "
			"gap/min=%.3g/%.3g/%.3g segment=%.17g/%.17g "
			"local=%zu/%zu/%zu/%zu\n",
			test.name,
			grazing_clearance_ratios[ratio_index], reverse,
			trace.surface_isolated_boxes,
			trace.surface_fold_attempts,
			trace.surface_fold_candidates,
			trace.surface_fold_krawczyk_available,
			trace.surface_fold_krawczyk_certified,
			trace.surface_fold_min_determinant_ratio,
			trace.surface_fold_best_image_excess,
			trace.surface_fold_expansion_attempts,
			trace.surface_fold_expansion_available,
			trace.surface_fold_expansion_certified,
			trace.surface_fold_expansion_failures,
			trace.surface_fold_expansion_min_determinant_ratio,
			trace.surface_fold_expansion_best_image_excess,
			trace.surface_fold_expansion_high_water,
			trace.surface_fold_corridor_attempts,
			trace.surface_fold_corridor_available,
			trace.surface_fold_corridor_regular_signed,
			trace.surface_fold_corridor_boundaries_opposed,
			trace.surface_fold_corridor_determinant_signed,
			trace.surface_fold_corridor_unique,
			trace.surface_fold_corridor_graph_attempts,
			trace.surface_fold_corridor_graph_certified,
			trace.surface_fold_corridor_graph_boxes,
			trace.surface_fold_corridor_graph_contractions,
			trace.surface_fold_corridor_graph_failures,
			trace.surface_fold_boundary_existence_attempts,
			trace.surface_fold_boundary_existence_available,
			trace.surface_fold_boundary_existence_certified,
			trace.surface_fold_boundary_existence_contractions,
			trace.surface_fold_boundary_existence_failures,
			trace.surface_fold_strip_excluded,
			trace.surface_fold_complete,
			trace.surface_fold_strip_boxes,
			trace.surface_fold_strip_contractions,
			trace.surface_fold_corridor_failures,
			trace.surface_fold_corridor_high_water,
			trace.surface_fold_roots,
			trace.stored_surface_fold_roots,
			trace.surface_fold_root_overflow,
			trace.surface_fold_root_failures,
			trace.surface_fold_direction_checks,
			trace.surface_fold_direction_mismatches,
			trace.surface_fold_localization_attempts,
			trace.surface_fold_localization_certified,
			trace.surface_fold_localization_contractions,
			trace.surface_fold_localization_failures,
			trace.surface_fold_topology_pairs,
			trace.surface_fold_duplicate_events,
			trace.surface_fold_material_pairs,
			trace.surface_fold_void_pairs,
			trace.surface_fold_resolved_pairs,
			trace.surface_fold_subminimum_contacts,
			trace.surface_fold_tolerance_ambiguous,
			trace.surface_fold_unmatched_roots,
			trace.surface_fold_pair_gap_min,
			trace.surface_fold_pair_gap_max,
			trace.surface_fold_minimum_t,
			trace.surface_fold_segment_in,
			trace.surface_fold_segment_out,
			trace.local_root_candidates,
			trace.stored_local_roots,
			trace.local_root_failures,
			trace.local_root_duplicates);
		    for (size_t fold_index = 0; fold_index <
			    trace.stored_surface_fold_roots; ++fold_index) {
			const struct rt_brep_trace_fold_root &root =
			    trace.surface_fold_roots_data[fold_index];
			std::printf("  fold root %zu face/span=%d/%d "
			    "t=%.17g [%.17g %.17g] uv=%.17g/%.17g "
			    "trim/class=%d/%d distance=%.17g\n", fold_index,
			    root.face_index, root.span_index, root.dist,
			    root.t_min, root.t_max, root.uv[0], root.uv[1],
			    root.trim_status, root.hit_class,
			    root.trim_distance);
		    }
		    for (size_t local_index = 0;
			    local_index < trace.stored_local_roots; ++local_index) {
			const struct rt_brep_trace_local_root &root =
			    trace.local_roots[local_index];
			std::printf("  local root %zu face/span=%d/%d "
			    "t=%.17g uv=%.17g/%.17g\n", local_index,
			    root.face_index, root.span_index, root.dist,
			    root.uv[0], root.uv[1]);
		    }
		}
		const bool reversal_mismatch = reverse &&
		    production_result.segments != forward_production_segments;
		if (!reverse)
		    forward_production_segments = production_result.segments;
		const bool fold_selected =
		    trace.surface_fold_promoted_pairs == 1;
		const bool selected_partition_matches =
		    !trace.prepared_production_selected ||
		    (fold_selected ?
		     production_segments == 1 &&
			fold_event_error <= normalized_limit :
		     trace.local_event_final_segments == production_segments &&
			trace.local_event_stored_segments == production_segments &&
			(!production_segments ||
			 prepared_error <= normalized_limit));
		const bool solver_partition_matches =
		    production_result.segments == legacy_result.segments ||
		    (fold_selected && production_result.segments ==
		    implicit_result.segments);
		const bool selected_event_ledger_matches =
		    !trace.prepared_production_selected ||
		    (trace.physical_event_complete == 1 &&
		     !trace.physical_event_state_failures &&
		     !trace.physical_event_overflow &&
		     !trace.physical_event_unresolved &&
		     trace.physical_event_material_segments ==
			 production_segments);
		const bool fixed_match =
		    brep_trace_fixed_workspaces_match(trace, true);
		const bool bad = !solver_partition_matches ||
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
		    !selected_event_ledger_matches ||
		    !fixed_match ||
		    trace.surface_workspace_exhausted ||
		    trace.surface_clip_restriction_failures;
		if (bad) {
		    std::printf("FAIL: affine grazing %s ratio=%.3g "
			"reverse=%d chord/T=%.3g "
			"segments=%d/%d/%d/%zu required/trend=%d/%d "
			"selection=%zu/%d fold=%d fixed=%d partition=%d "
			"leaves=%zu/%zu boxes=%zu/%zu "
			"errors=%.3g/%.3g/%.3g/%.3g limit=%.3g\n",
			test.name, grazing_clearance_ratios[ratio_index],
			reverse, chord_ratio, implicit_result.segments,
			production_result.segments, legacy_result.segments,
			trace.final_segments, required_hit,
			restarted || reversal_mismatch,
			trace.prepared_production_selected,
			trace.prepared_production_fallback, fold_selected,
			fixed_match, selected_partition_matches,
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
		    std::printf("  corrector local status=%zu/%zu/%zu/%zu/"
			"%zu/%zu/%zu/%zu surface=%zu/%zu/%zu/%zu/"
			"%zu/%zu/%zu/%zu failure-ratio=%.3g/%.3g/%zu\n",
			trace.local_corrector_status[0],
			trace.local_corrector_status[1],
			trace.local_corrector_status[2],
			trace.local_corrector_status[3],
			trace.local_corrector_status[4],
			trace.local_corrector_status[5],
			trace.local_corrector_status[6],
			trace.local_corrector_status[7],
			trace.surface_corrector_status[0],
			trace.surface_corrector_status[1],
			trace.surface_corrector_status[2],
			trace.surface_corrector_status[3],
			trace.surface_corrector_status[4],
			trace.surface_corrector_status[5],
			trace.surface_corrector_status[6],
			trace.surface_corrector_status[7],
			trace.local_corrector_min_failure_ratio,
			trace.local_corrector_max_failure_ratio,
			trace.local_corrector_failure_ratios);
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
		    total_rotated_hull_attempts +=
			trace.surface_rotated_hull_attempts;
		    total_rotated_hull_exclusions +=
			trace.surface_rotated_hull_exclusions;
		    total_rotated_hull_inconclusive +=
			trace.surface_rotated_hull_inconclusive;
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
			!brep_trace_fixed_workspaces_match(trace, true) ||
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
			  !brep_trace_regular_event_stream_valid(trace, 1) ||
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
			    brep_trace_fixed_workspaces_match(trace, true),
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
    if (grazing_resolved_misses) {
	std::printf("FAIL: ellipsoid affine resolved grazing misses=%zu/0\n",
	    grazing_resolved_misses);
	failures++;
    }
    if (grazing_expansion_ratchets != 6 || grazing_corridor_ratchets != 6 ||
	    grazing_fold_event_ratchets != 6) {
	std::printf("FAIL: ellipsoid affine fold ratchets=%zu/%zu/%zu "
	    "expected=6\n", grazing_expansion_ratchets,
	    grazing_corridor_ratchets, grazing_fold_event_ratchets);
	failures++;
    }
    if (grazing_resolved_misses &&
	    (grazing_gap_maximum_boxes > 2 ||
	     !grazing_gap_rotated_exclusions)) {
	std::printf("FAIL: ellipsoid affine rotated grazing isolation "
	    "gaps=%zu max-boxes=%zu exclusions=%zu\n",
	    grazing_resolved_misses, grazing_gap_maximum_boxes,
	    grazing_gap_rotated_exclusions);
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
	    "fold=%zu/%zu/%zu/%.3g expansion=%zu/%zu/%zu/%zu/%.3g/%zu "
	    "max-error=%.3g\n",
	    grazing_clearance_ratios[ratio_index],
	    summary.rays, summary.implicit_segments,
	    summary.production_segments, summary.selected,
	    summary.fallback[RT_BREP_PREPARED_FALLBACK_NONE],
	    summary.fallback[RT_BREP_PREPARED_FALLBACK_SURFACE_BOXES],
	    summary.fallback[RT_BREP_PREPARED_FALLBACK_UNCERTIFIED],
	    summary.fallback[RT_BREP_PREPARED_FALLBACK_PARTITION],
	    summary.minimum_chord_ratio, summary.maximum_chord_ratio,
	    summary.fold_attempts, summary.fold_candidates,
	    summary.fold_certified,
	    summary.fold_minimum_ratio < DBL_MAX ?
		summary.fold_minimum_ratio : 0.0,
	    summary.expansion_attempts, summary.expansion_available,
	    summary.expansion_certified, summary.expansion_failures,
	    summary.expansion_minimum_ratio < DBL_MAX ?
		summary.expansion_minimum_ratio : 0.0,
	    summary.expansion_high_water,
	    summary.maximum_endpoint_error);
    }

    if (!failures) {
	std::printf("Ellipsoid ordinary affine invariance: PASS "
	    "rays=%zu selected=%zu fallback=%zu/%zu "
	    "condition=%.3g/%.3g krawczyk=%zu "
	    "cert-depth=%zu/%zu boxes=%zu/%zu/%zu "
	    "rotated-hull=%zu/%zu/%zu "
	    "max-errors=%.3g/%.3g/%.3g/%.3g\n", total_rays,
	    selected_rays, surface_box_fallbacks, uncertified_fallbacks,
	    minimum_condition, maximum_condition, total_krawczyk,
	    minimum_depth, maximum_depth, minimum_boxes, total_boxes,
	    maximum_boxes, total_rotated_hull_exclusions,
	    total_rotated_hull_attempts, total_rotated_hull_inconclusive,
	    maximum_implicit_error, maximum_production_error,
	    maximum_legacy_error, maximum_prepared_error);
	std::printf("Ellipsoid affine grazing ratchet: PASS rays=%zu "
	    "resolved-gaps=%zu expansion/corridor/event-ratchets=%zu/%zu/%zu "
	    "floors=1e-6/1e-6/1e-4\n",
	    grazing_rays, grazing_resolved_misses,
	    grazing_expansion_ratchets, grazing_corridor_ratchets,
	    grazing_fold_event_ratchets);
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
cobb_curve_edge_trim(ON_Brep *brep, int edge_index, int face_index,
    double signed_target_lift_shift, double &maximum_lift_shift,
    double &maximum_u_shift, double &maximum_v_shift)
{
    maximum_lift_shift = INFINITY;
    maximum_u_shift = INFINITY;
    maximum_v_shift = INFINITY;
    if (!brep || edge_index < 0 || edge_index >= brep->m_E.Count() ||
	face_index < 0 || face_index >= brep->m_F.Count() ||
	!(fabs(signed_target_lift_shift) > 0.0) ||
	!std::isfinite(signed_target_lift_shift))
	return false;
    const ON_BrepEdge &edge = brep->m_E[edge_index];
    if (edge.m_ti.Count() != 2)
	return false;
    int trim_index = -1;
    for (int side = 0; side < edge.m_ti.Count(); ++side) {
	const int candidate = edge.m_ti[side];
	if (candidate >= 0 && candidate < brep->m_T.Count() &&
		brep->m_T[candidate].FaceIndexOf() == face_index) {
	    trim_index = candidate;
	    break;
	}
    }
    if (trim_index < 0)
	return false;
    ON_BrepTrim &trim = brep->m_T[trim_index];
    const ON_Curve *trim_curve = trim.TrimCurveOf();
    const ON_Surface *surface = trim.Face() ? trim.Face()->SurfaceOf() : NULL;
    if (!trim_curve || !surface || !trim_curve->Domain().IsIncreasing())
	return false;
    const ON_Curve *original_curve = trim_curve->DuplicateCurve();
    if (!original_curve)
	return false;
    const ON_Interval domain = original_curve->Domain();
    const ON_3dPoint start = original_curve->PointAt(domain.Min());
    const ON_3dPoint end = original_curve->PointAt(domain.Max());
    const ON_2dPoint midpoint(0.5 * (start.x + end.x),
	0.5 * (start.y + end.y));
    ON_2dVector inward(-(end.y - start.y), end.x - start.x);
    const ON_2dPoint face_center(surface->Domain(0).Mid(),
	surface->Domain(1).Mid());
    if (!start.IsValid() || !end.IsValid() || !inward.Unitize()) {
	delete original_curve;
	return false;
    }
    if (inward * (face_center - midpoint) < 0.0)
	inward.Reverse();

    ON_3dPoint surface_point;
    ON_3dVector surface_u;
    ON_3dVector surface_v;
    if (!surface->Ev1Der(midpoint.x, midpoint.y, surface_point,
	    surface_u, surface_v)) {
	delete original_curve;
	return false;
    }
    const ON_3dVector lift_derivative =
	inward.x * surface_u + inward.y * surface_v;
    const double lift_rate = lift_derivative.Length();
    if (!(lift_rate > DBL_MIN) || !std::isfinite(lift_rate)) {
	delete original_curve;
	return false;
    }
    /* A quadratic Bezier reaches half its middle-control displacement at
     * t=1/2.  Use the surface directional derivative only to size a subtle
     * perturbation; the independently sampled 3D lift below is the fixture's
     * measured fact. */
    const double control_displacement =
	2.0 * signed_target_lift_shift / lift_rate;
    const ON_3dPoint control(midpoint.x + control_displacement * inward.x,
	midpoint.y + control_displacement * inward.y, 0.0);
    ON_NurbsCurve *curved = ON_NurbsCurve::New(2, false, 3, 3);
    const bool controls_set = curved && curved->SetCV(0, start) &&
	curved->SetCV(1, control) && curved->SetCV(2, end);
    if (!controls_set || !curved->MakeClampedUniformKnotVector() ||
	    !curved->SetDomain(domain.Min(), domain.Max())) {
	delete original_curve;
	delete curved;
	return false;
    }

    maximum_lift_shift = 0.0;
    maximum_u_shift = 0.0;
    maximum_v_shift = 0.0;
    double maximum_chord_error = 0.0;
    for (int sample = 0; sample <= 128; ++sample) {
	const double fraction = (double)sample / 128.0;
	const double parameter = domain.ParameterAt(fraction);
	const ON_3dPoint old_uv = original_curve->PointAt(parameter);
	const ON_3dPoint new_uv = curved->PointAt(parameter);
	const ON_2dPoint chord((1.0 - fraction) * start.x + fraction * end.x,
	    (1.0 - fraction) * start.y + fraction * end.y);
	const ON_3dPoint old_lift = surface->PointAt(old_uv.x, old_uv.y);
	const ON_3dPoint new_lift = surface->PointAt(new_uv.x, new_uv.y);
	if (!old_uv.IsValid() || !new_uv.IsValid() || !old_lift.IsValid() ||
		!new_lift.IsValid()) {
	    delete original_curve;
	    delete curved;
	    return false;
	}
	maximum_chord_error = std::max(maximum_chord_error,
	    old_uv.DistanceTo(ON_3dPoint(chord.x, chord.y, 0.0)));
	maximum_u_shift = std::max(maximum_u_shift,
	    fabs(new_uv.x - old_uv.x));
	maximum_v_shift = std::max(maximum_v_shift,
	    fabs(new_uv.y - old_uv.y));
	maximum_lift_shift = std::max(maximum_lift_shift,
	    old_lift.DistanceTo(new_lift));
    }
    const double uv_scale = std::max(1.0,
	std::max(fabs(start.x), std::max(fabs(start.y),
	std::max(fabs(end.x), fabs(end.y)))));
    const double uv_roundoff = 4096.0 * DBL_EPSILON * uv_scale;
    if (maximum_chord_error > uv_roundoff ||
	    !(maximum_lift_shift >
		0.01 * fabs(signed_target_lift_shift)) ||
	    maximum_lift_shift > 2.0 * fabs(signed_target_lift_shift)) {
	delete original_curve;
	delete curved;
	return false;
    }

    const int curve_index = brep->AddTrimCurve(curved);
    if (curve_index < 0 || !brep->SetTrimCurve(trim, curve_index)) {
	delete original_curve;
	if (curve_index < 0)
	    delete curved;
	return false;
    }
    trim.m_tolerance[0] = 1.01 * maximum_u_shift;
    trim.m_tolerance[1] = 1.01 * maximum_v_shift;
    brep->SetTrimIsoFlags(trim);
    brep->DestroyRuntimeCache(true);
    const ON_3dPoint installed_start = trim.PointAt(domain.Min());
    const ON_3dPoint installed_end = trim.PointAt(domain.Max());
    delete original_curve;
    return installed_start.IsValid() && installed_end.IsValid() &&
	installed_start.DistanceTo(start) <= uv_roundoff &&
	installed_end.DistanceTo(end) <= uv_roundoff &&
	trim.m_iso != ON_Surface::W_iso &&
	trim.m_iso != ON_Surface::E_iso &&
	trim.m_iso != ON_Surface::S_iso &&
	trim.m_iso != ON_Surface::N_iso && brep->IsValid();
}


static bool
cobb_edge_lift_discrepancy(const ON_Brep *brep, int edge_index,
    double &maximum)
{
    maximum = INFINITY;
    if (!brep || edge_index < 0 || edge_index >= brep->m_E.Count())
	return false;
    const ON_BrepEdge &edge = brep->m_E[edge_index];
    if (edge.m_ti.Count() != 2 || !edge.Domain().IsIncreasing())
	return false;
    maximum = 0.0;
    for (int sample = 0; sample <= 256; ++sample) {
	const double fraction = (double)sample / 256.0;
	const ON_3dPoint edge_point = edge.PointAt(
	    edge.Domain().ParameterAt(fraction));
	if (!edge_point.IsValid())
	    return false;
	for (int side = 0; side < 2; ++side) {
	    const int trim_index = edge.m_ti[side];
	    if (trim_index < 0 || trim_index >= brep->m_T.Count())
		return false;
	    ON_3dPoint lift;
	    if (!cobb_trim_lift(brep->m_T[trim_index], fraction, lift))
		return false;
	    maximum = std::max(maximum, edge_point.DistanceTo(lift));
	}
    }
    return std::isfinite(maximum);
}


static bool
cobb_perturb_edge_curve(ON_Brep *brep, int edge_index,
    const ON_3dPoint &origin, double displacement)
{
    if (!brep || edge_index < 0 || edge_index >= brep->m_E.Count() ||
	    !std::isfinite(displacement) || !(fabs(displacement) > 0.0))
	return false;
    ON_BrepEdge &edge = brep->m_E[edge_index];
    ON_NurbsCurve nurbs;
    if (edge.GetNurbForm(nurbs) != 1 || nurbs.m_cv_count < 3 ||
	    !nurbs.Domain().IsIncreasing())
	return false;
    const ON_3dPoint start = edge.PointAtStart();
    const ON_3dPoint end = edge.PointAtEnd();
    for (int cv_index = 1; cv_index + 1 < nurbs.m_cv_count; ++cv_index) {
	ON_4dPoint cv;
	if (!nurbs.GetCV(cv_index, cv) || fabs(cv.w) <= DBL_MIN)
	    return false;
	ON_3dPoint point(cv.x / cv.w, cv.y / cv.w, cv.z / cv.w);
	ON_3dVector direction = point - origin;
	if (!direction.Unitize())
	    return false;
	cv.x += displacement * direction.x * cv.w;
	cv.y += displacement * direction.y * cv.w;
	cv.z += displacement * direction.z * cv.w;
	if (!nurbs.SetCV(cv_index, cv))
	    return false;
    }

    ON_NurbsCurve *replacement = new ON_NurbsCurve(nurbs);
    const int curve_index = brep->AddEdgeCurve(replacement);
    if (curve_index < 0 ||
	    !brep->SetEdgeCurve(brep->m_E[edge_index], curve_index)) {
	if (curve_index < 0)
	    delete replacement;
	return false;
    }
    brep->m_E[edge_index].UnsetPlineEdgeParameters();
    brep->DestroyRuntimeCache(true);
    const ON_BrepEdge &installed = brep->m_E[edge_index];
    const double coordinate_scale = std::max(1.0,
	std::max(fabs(start.x), std::max(fabs(start.y),
	std::max(fabs(start.z), std::max(fabs(end.x),
	std::max(fabs(end.y), fabs(end.z)))))));
    const double roundoff = std::max(ON_ZERO_TOLERANCE,
	4096.0 * DBL_EPSILON * coordinate_scale);
    return installed.PointAtStart().DistanceTo(start) <= roundoff &&
	installed.PointAtEnd().DistanceTo(end) <= roundoff;
}


static ON_Brep *
cobb_trim_only_variant(const ON_Brep *pristine, const ON_3dPoint &origin,
    double signed_target_gap, cobb_seam_frame &frame, double &measured_gap,
    double &applied_displacement, double &maximum_u_shift,
    double &maximum_v_shift)
{
    measured_gap = INFINITY;
    applied_displacement = signed_target_gap;
    maximum_u_shift = INFINITY;
    maximum_v_shift = INFINITY;
    if (!pristine || !(fabs(signed_target_gap) > 0.0) ||
	    !cobb_seam_geometry(pristine, origin, frame))
	return NULL;

    ON_Brep *variant = NULL;
    for (int iteration = 0; iteration < 4; ++iteration) {
	delete variant;
	variant = new ON_Brep(*pristine);
	/* The helper validates the installed trim before returning.  Give that
	 * intermediate model a conservative declared edge envelope; replace it
	 * with the independently chosen test policy after calibration. */
	variant->m_E[frame.edge_index].m_tolerance =
	    4.0 * fabs(applied_displacement);
	double maximum_lift_shift = INFINITY;
	if (!cobb_curve_edge_trim(variant, frame.edge_index,
		frame.face_index, applied_displacement, maximum_lift_shift,
		maximum_u_shift, maximum_v_shift)) {
	    delete variant;
	    return NULL;
	}
	measured_gap = cobb_seam_discrepancy(variant, frame.edge_index);
	if (!(measured_gap > 0.0) || !std::isfinite(measured_gap) ||
		fabs(maximum_lift_shift - measured_gap) >
		    0.02 * measured_gap) {
	    delete variant;
	    return NULL;
	}
	const double ratio = fabs(signed_target_gap) / measured_gap;
	if (fabs(ratio - 1.0) <= 1.0e-4)
	    break;
	applied_displacement *= ratio;
    }
    variant->m_E[frame.edge_index].m_tolerance = 1.01 * measured_gap;
    if (!variant->IsValid()) {
	delete variant;
	return NULL;
    }
    return variant;
}


static ON_Brep *
cobb_edge_only_variant(const ON_Brep *pristine, const ON_3dPoint &origin,
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
	if (!cobb_perturb_edge_curve(variant, frame.edge_index, origin,
		applied_displacement) ||
		!cobb_edge_lift_discrepancy(variant, frame.edge_index,
		    measured_gap) || !(measured_gap > 0.0)) {
	    delete variant;
	    return NULL;
	}
	const double ratio = fabs(signed_target_gap) / measured_gap;
	if (fabs(ratio - 1.0) <= 1.0e-4)
	    break;
	applied_displacement *= ratio;
    }
    variant->m_E[frame.edge_index].m_tolerance = 1.01 * measured_gap;
    if (!variant->IsValid()) {
	delete variant;
	return NULL;
    }
    return variant;
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

    /* This degree-15 scalar control net reverses only between the old
     * screen's t=0.4 and t=0.6 samples.  Values and derivatives at all four
     * samples remain forward.  Every control stays on the original UV
     * segment, while the global proof must reject the hidden non-injective
     * traversal. */
    const double fraction[16] = {
	0.0,
	0.066666666666666666,
	0.13333333333333333,
	0.22748150458765812,
	0.33625349748826455,
	0.7208158028380063,
	0.5751840105173857,
	0.6005489273945116,
	0.44750405015860617,
	0.6305690007427176,
	0.30313585207409444,
	0.5477137465243344,
	0.9423385356407661,
	0.8666666666666667,
	0.93333333333333335,
	1.0
    };
    ON_NurbsCurve *ambiguous = ON_NurbsCurve::New(2, false, 16, 16);
    bool controls_set = ambiguous != NULL;
    for (int i = 0; controls_set && i < 16; ++i) {
	const ON_3dPoint point = (1.0 - fraction[i]) * start +
	    fraction[i] * end;
	controls_set = ambiguous->SetCV(i, point);
    }
    if (!ambiguous || !controls_set ||
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
    double reference_existing[4][2] = {};
    double reference_continuation[4][2] = {};
    size_t reference_local_root_count[4][2] = {};
    size_t reference_local_cluster_count[4][2] = {};
    int reference_local_cluster_class[4][2]
	[RT_BREP_TRACE_MAX_LOCAL_CLUSTERS] = {};
    double reference_local_root_distances[4][2]
	[RT_BREP_TRACE_MAX_LOCAL_ROOTS] = {};
    int failures = 0;
    size_t maximum_fixed_leaves = 0;
    size_t maximum_fixed_hits = 0;
    size_t maximum_correspondence_cells = 0;
    size_t maximum_correspondence_depth = 0;
    size_t maximum_discrepancy_cells = 0;
    size_t maximum_discrepancy_depth = 0;
    size_t contact_similarity_cases = 0;
    size_t oblique_similarity_cases = 0;
    size_t source_union_similarity_cases = 0;
    size_t minimum_contact_boxes = RT_BREP_TRACE_MAX_SURFACE_BOXES;
    size_t maximum_contact_boxes = 0;
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
	    const double clearance_ratios[] = {0.9, 1.0, 0.0, -0.9};
	    for (size_t state_index = 0; state_index <
		    sizeof(clearance_ratios) / sizeof(clearance_ratios[0]);
		    ++state_index) {
		const double clearance = clearance_ratios[state_index] *
		    tol->dist;
		const int expected_state = clearance > 0.0 ? 1 :
		    (clearance < 0.0 ? -1 : 0);
		const bool expected_contact_pair = state_index == 1;
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
		    if (edge) {
			maximum_correspondence_cells = std::max(
			    maximum_correspondence_cells,
			    edge->correspondence_cells);
			maximum_correspondence_depth = std::max(
			    maximum_correspondence_depth,
			    edge->correspondence_depth);
		    }
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
		    const size_t expected_closures =
			expected_state == 1 && !expected_contact_pair ? 1 : 0;
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
			edge->discrepancy_lower_bound >
			edge->measured_discrepancy + normalized_limit *
			test.scale ||
			edge->measured_discrepancy >
			edge->discrepancy_upper_bound + normalized_limit *
			test.scale ||
			!(edge->discrepancy_upper_bound <
			edge->edge_tolerance) ||
			edge->tolerance_inferred ||
			!edge->discrepancy_measured ||
			!edge->discrepancy_sample_authorized ||
			!edge->correspondence_screened ||
			!edge->correspondence_supported ||
			!edge->correspondence_cells ||
			edge->correspondence_exhausted ||
			edge->discrepancy_proof_class !=
			RT_BREP_SEAM_GAP_INSIDE ||
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
			(!expected_contact_pair &&
			 trace.closure_candidates != expected_closures);
		    if (expected_contact_pair) {
			contact_similarity_cases++;
			minimum_contact_boxes = std::min(minimum_contact_boxes,
			    trace.physical_event_seam_contact_boxes);
			maximum_contact_boxes = std::max(maximum_contact_boxes,
			    trace.physical_event_seam_contact_boxes);
			const double transformed_radius = radius * test.scale;
			const double transformed_clearance =
			    tol->dist * test.scale;
			const double half_chord = sqrt(2.0 * transformed_radius *
			    transformed_clearance - transformed_clearance *
			    transformed_clearance);
			partition_result contact_oracle;
			contact_oracle.partitions = 1;
			contact_oracle.intervals[0].in_dist =
			    2.0 * transformed_radius - half_chord;
			contact_oracle.intervals[0].out_dist =
			    2.0 * transformed_radius + half_chord;
			bad = bad || trace_hits != 2 ||
			    trace.final_segments != 1 ||
			    production_result.segments != 1 ||
			    !brep_trace_seam_event_stream_valid(trace, edge,
				variant, contact_oracle, case_tol.dist,
				test.reparameterize);
		    } else if (expected_closures) {
			if (case_index == 0) {
			    reference_existing[state_index][reverse] =
				trace.closure_existing_dist;
			    reference_continuation[state_index][reverse] =
				trace.continuation_dist;
			}
			bad = bad || trace.closure_edge_index != frame.edge_index ||
			    trace.closure_missing_direction != expected_direction ||
			    fabs(trace.closure_edge_dist / test.scale -
			    2.0 * radius) > normalized_limit ||
			    fabs(trace.closure_existing_dist / test.scale -
			    reference_existing[state_index][reverse]) >
				normalized_root_limit ||
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
			    reference_continuation[state_index][reverse]) >
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
			bad = bad || trace.physical_event_seam_contact_pairs ||
			    trace.physical_event_seam_contact_boxes ||
			    trace.physical_event_seam_contact_roots ||
			    trace.physical_event_seam_contact_miss_roots;
		    } else {
			bad = bad || trace.continuation_attempts != 0 ||
			    trace.continuation_candidates != 0 ||
			    trace.continuation_certified_candidates != 0 ||
			    trace.continuation_certificate_boxes != 0 ||
			    trace.closure_shadow_segments != 0 || trace_hits != 0 ||
			    trace.final_segments != 0 ||
			    production_result.segments != 0 ||
			    trace.physical_event_seam_contact_pairs ||
			    trace.physical_event_seam_contact_boxes ||
			    trace.physical_event_seam_contact_roots ||
			    trace.physical_event_seam_contact_miss_roots;
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
			    "certificate=%zu/%zu/%zu/%zu+%zu "
			    "contact=%zu/%zu/%zu certified/selected=%zu/%zu "
			    "seam-failure/ownership/witness/box/root="
			    "%zu/%zu/%zu/%zu/%zu\n",
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
			    trace.continuation_certificate_exhausted,
			    trace.physical_event_seam_contact_pairs,
			    trace.physical_event_seam_contact_boxes,
			    trace.physical_event_seam_contact_roots,
			    trace.physical_event_seam_certified,
			    trace.prepared_production_selected,
			    trace.physical_event_seam_failures,
			    trace.physical_event_seam_ownership_failures,
			    trace.physical_event_seam_witness_failures,
			    trace.physical_event_seam_box_failures,
			    trace.physical_event_seam_root_coverage_failures);
			failures++;
		    }
		}
	    }

	    const double tangent_components[] = {0.001, 0.1};
	    for (size_t component_index = 0; component_index <
		    sizeof(tangent_components) / sizeof(tangent_components[0]);
		    ++component_index) {
	    const double tangent_component =
		tangent_components[component_index];
	    const bool expect_source_union = component_index == 1;
	    for (int reverse = 0; reverse <= 1; ++reverse) {
		const sampled_ray base_ray = cobb_seam_oblique_ray(frame,
		    origin, radius, tol->dist, tangent_component, reverse != 0);
		const ON_3dPoint transformed_origin = cobb_transform_point(xform,
		    ON_3dPoint(base_ray.origin));
		ON_3dVector transformed_direction = cobb_transform_vector(xform,
		    ON_3dVector(base_ray.direction));
		if (!transformed_direction.Unitize()) {
		    std::printf("FAIL: Cobb %s oblique direction\n", test.name);
		    failures++;
		    continue;
		}
		sampled_ray ray;
		VSET(ray.origin, transformed_origin.x, transformed_origin.y,
		    transformed_origin.z);
		VSET(ray.direction, transformed_direction.x,
		    transformed_direction.y, transformed_direction.z);
		const ray_result production_result = shoot_solid(stp, rtip,
		    &resource, ray.origin, ray.direction);
		struct rt_brep_shot_trace trace;
		const int trace_hits = shoot_brep_trace(stp, rtip, &resource,
		    ray, trace);
		const struct rt_brep_trace_edge *edge = brep_trace_edge(trace,
		    frame.edge_index);
		const double transformed_radius = radius * test.scale;
		const double transformed_clearance = tol->dist * test.scale;
		const double half_chord = sqrt(2.0 * transformed_radius *
		    transformed_clearance - transformed_clearance *
		    transformed_clearance);
		partition_result oracle;
		oracle.partitions = 1;
		oracle.intervals[0].in_dist = 2.0 * transformed_radius -
		    half_chord;
		oracle.intervals[0].out_dist = 2.0 * transformed_radius +
		    half_chord;
		const double expected_edge_dot = tangent_component /
		    sqrt(1.0 + tangent_component * tangent_component);
		const double dot_error = edge ?
		    fabs(fabs(edge->ray_edge_dot) - expected_edge_dot) : INFINITY;
		const bool no_source_union =
		    !trace.physical_event_seam_source_union_certified &&
		    !trace.physical_event_seam_source_union_root_boxes &&
		    !trace.physical_event_seam_source_union_boxes;
		const bool valid_source_union =
		    trace.physical_event_seam_source_union_certified == 1 &&
		    trace.physical_event_seam_source_union_root_boxes > 0 &&
		    trace.physical_event_seam_source_union_root_boxes <
			trace.physical_event_seam_source_union_boxes;
		const bool source_union_evidence = expect_source_union ?
		    (no_source_union || valid_source_union) : no_source_union;
		const bool bad = !brep_trace_fixed_workspaces_match(trace) ||
		    !edge || !edge->frame_interval_supported ||
		    !edge->frame_interval_cells ||
		    !edge->within_edge_tolerance || !edge->sector_valid ||
		    dot_error > 1.0e-10 || trace_hits != 2 ||
		    trace.final_segments != 1 || production_result.segments != 1 ||
		    trace.physical_event_seam_oblique_pairs != 1 ||
		    !trace.physical_event_seam_oblique_cells ||
		    trace.physical_event_seam_oblique_box_links !=
			trace.physical_event_seam_contact_boxes ||
		    !source_union_evidence ||
		    trace.prepared_production_selected != 1 ||
		    trace.prepared_production_fallback !=
			RT_BREP_PREPARED_FALLBACK_NONE ||
		    !brep_trace_seam_event_stream_valid(trace, edge, variant,
			oracle, case_tol.dist, test.reparameterize);
		if (bad) {
		    std::printf("FAIL: Cobb %s oblique component=%.17g "
			"reverse=%d edge=%d "
			"frame=%d/%zu dot-error=%.3g hits/segments=%d/%zu/%d "
			"oblique=%zu/%zu/%zu contact-boxes=%zu selected=%zu "
			"union=%zu/%zu/%zu fallback=%d\n", test.name,
			tangent_component, reverse, edge != NULL,
			edge ? edge->frame_interval_supported : 0,
			edge ? edge->frame_interval_cells : 0, dot_error,
			trace_hits, trace.final_segments, production_result.segments,
			trace.physical_event_seam_oblique_pairs,
			trace.physical_event_seam_oblique_cells,
			trace.physical_event_seam_oblique_box_links,
			trace.physical_event_seam_contact_boxes,
			trace.prepared_production_selected,
			trace.physical_event_seam_source_union_certified,
			trace.physical_event_seam_source_union_root_boxes,
			trace.physical_event_seam_source_union_boxes,
			trace.prepared_production_fallback);
		    failures++;
		} else {
		    oblique_similarity_cases++;
		    source_union_similarity_cases += valid_source_union ? 1 : 0;
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
	    "correspondence-cells=%zu depth=%zu "
	    "seam-bound-cells=%zu depth=%zu width/T=%.3g "
	    "contact-cases=%zu oblique-cases=%zu source-union-cases=%zu "
	    "boxes=%zu/%zu\n",
	    maximum_fixed_leaves, RT_BREP_MAX_LEAVES,
	    maximum_fixed_hits, RT_BREP_MAX_HITS,
	    maximum_parameter_locus_error,
	    minimum_parameter_midpoint_shift, maximum_correspondence_cells,
	    maximum_correspondence_depth, maximum_discrepancy_cells,
	    maximum_discrepancy_depth, maximum_discrepancy_width_ratio,
	    contact_similarity_cases, oblique_similarity_cases,
	    source_union_similarity_cases,
	    minimum_contact_boxes,
	    maximum_contact_boxes);
    }
    return failures;
}


static int
check_cobb_oblique_contact_trend(const struct bn_tol *tol, bool emit_report)
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
    if (!variant)
	return 1;

    struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
    if (!rtip) {
	delete variant;
	return 1;
    }
    rtip->rti_tol = *tol;
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
	rt_clean_resource_basic(rtip, &resource);
	BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
	rt_i_destroy(rtip);
	return 1;
    }

    enum oblique_expectation {
	EXPECT_FALLBACK,
	EXPECT_OBSERVE,
	EXPECT_CERTIFIED
    };
    struct oblique_case {
	double component;
	oblique_expectation expectation;
	bool source_union;
    } cases[] = {
	{1.0e-6, EXPECT_FALLBACK, false},
	{1.0e-4, EXPECT_CERTIFIED, false},
	{1.0e-3, EXPECT_CERTIFIED, false},
	{0.01, EXPECT_CERTIFIED, false},
	{0.05, EXPECT_CERTIFIED, false},
	{0.1, EXPECT_CERTIFIED, true},
	{0.25, EXPECT_CERTIFIED, false},
	{0.5, EXPECT_CERTIFIED, false},
	{1.0, EXPECT_CERTIFIED, false},
	{10.0, EXPECT_FALLBACK, false},
	{1.0e6, EXPECT_FALLBACK, false}
    };
    int failures = 0;
    size_t certified = 0;
    size_t fail_closed = 0;
    size_t observed_certified = 0;
    size_t observed_fallback = 0;
    for (size_t case_index = 0; case_index <
	    sizeof(cases) / sizeof(cases[0]); ++case_index) {
	for (int reverse = 0; reverse <= 1; ++reverse) {
	    const sampled_ray ray = cobb_seam_oblique_ray(frame, origin,
		radius, tol->dist, cases[case_index].component, reverse != 0);
	    const ray_result production_result = shoot_solid(stp, rtip,
		&resource, ray.origin, ray.direction);
	    struct rt_brep_shot_trace trace;
	    const int trace_hits = shoot_brep_trace(stp, rtip, &resource, ray,
		trace);
	    const struct rt_brep_trace_edge *edge = brep_trace_edge(trace,
		frame.edge_index);
	    const bool selected = trace.prepared_production_selected == 1 &&
		trace.prepared_production_fallback ==
		    RT_BREP_PREPARED_FALLBACK_NONE;
	    const bool source_union_evidence =
		(!trace.physical_event_seam_source_union_certified &&
		 !trace.physical_event_seam_source_union_root_boxes &&
		 !trace.physical_event_seam_source_union_boxes) ||
		(trace.physical_event_seam_source_union_certified == 1 &&
		 trace.physical_event_seam_source_union_root_boxes > 0 &&
		 trace.physical_event_seam_source_union_root_boxes <
		    trace.physical_event_seam_source_union_boxes);
	    const bool certified_evidence = selected && source_union_evidence &&
		edge &&
		edge->frame_interval_supported && edge->frame_interval_cells > 0 &&
		trace.physical_event_seam_certified == 1 &&
		trace.physical_event_seam_contact_pairs == 1 &&
		trace.physical_event_seam_contact_boxes > 0 &&
		trace.physical_event_seam_contact_roots == 2 &&
		!trace.physical_event_seam_contact_miss_roots &&
		trace.physical_event_seam_oblique_pairs == 1 &&
		trace.physical_event_seam_oblique_cells > 0 &&
		trace.physical_event_seam_oblique_box_links ==
		    trace.physical_event_seam_contact_boxes;
	    const bool fallback_evidence = !selected &&
		trace.prepared_production_fallback !=
		    RT_BREP_PREPARED_FALLBACK_NONE &&
		!trace.physical_event_seam_oblique_pairs &&
		!trace.physical_event_seam_oblique_cells &&
		!trace.physical_event_seam_oblique_box_links &&
		!trace.physical_event_seam_contact_pairs &&
		!trace.physical_event_seam_contact_boxes &&
		!trace.physical_event_seam_contact_roots &&
		!trace.physical_event_seam_contact_miss_roots &&
		!trace.physical_event_seam_source_union_certified &&
		!trace.physical_event_seam_source_union_root_boxes &&
		!trace.physical_event_seam_source_union_boxes;
	    bool bad = !brep_trace_fixed_workspaces_match(trace) || !edge ||
		!edge->frame_interval_supported || !edge->frame_interval_cells ||
		trace.root_overflow || trace.edge_overflow ||
		trace.surface_workspace_exhausted || trace.surface_box_overflow ||
		trace.local_root_overflow || trace.local_cluster_overflow;
	    if (cases[case_index].expectation == EXPECT_CERTIFIED) {
		const double half_chord = sqrt(2.0 * radius * tol->dist -
		    tol->dist * tol->dist);
		partition_result oracle;
		oracle.partitions = 1;
		oracle.intervals[0].in_dist = 2.0 * radius - half_chord;
		oracle.intervals[0].out_dist = 2.0 * radius + half_chord;
		bad = bad || !certified_evidence || trace_hits != 2 ||
		    trace.final_segments != 1 || production_result.segments != 1 ||
		    (cases[case_index].source_union &&
		     (trace.physical_event_seam_source_union_certified != 1 ||
		      trace.physical_event_seam_source_union_root_boxes != 1 ||
		      trace.physical_event_seam_source_union_boxes != 2)) ||
		    !brep_trace_seam_event_stream_valid(trace, edge, variant,
			oracle, tol->dist);
		if (cases[case_index].source_union)
		    bad = bad || !brep_trace_source_union_negative_controls(
			trace, edge, variant, oracle, tol->dist);
		if (!bad)
		    certified++;
	    } else if (cases[case_index].expectation == EXPECT_FALLBACK) {
		bad = bad || !fallback_evidence ||
		    trace.physical_event_seam_failures != 1;
		if (!bad)
		    fail_closed++;
	    } else {
		bad = bad || (!certified_evidence && !fallback_evidence);
		if (!bad && certified_evidence)
		    observed_certified++;
		else if (!bad)
		    observed_fallback++;
	    }
	    if (bad) {
		std::printf("FAIL: Cobb oblique trend component=%.17g reverse=%d "
		    "expect=%d edge=%d frame=%d/%zu selected=%zu fallback=%d "
		    "seam/contact/oblique=%zu/%zu/%zu cells/links=%zu/%zu "
		    "hits/segments=%d/%zu/%d failures=%zu\n",
		    cases[case_index].component, reverse,
		    (int)cases[case_index].expectation, edge != NULL,
		    edge ? edge->frame_interval_supported : 0,
		    edge ? edge->frame_interval_cells : 0,
		    trace.prepared_production_selected,
		    trace.prepared_production_fallback,
		    trace.physical_event_seam_certified,
		    trace.physical_event_seam_contact_pairs,
		    trace.physical_event_seam_oblique_pairs,
		    trace.physical_event_seam_oblique_cells,
		    trace.physical_event_seam_oblique_box_links,
		    trace_hits, trace.final_segments, production_result.segments,
		    trace.physical_event_seam_failures);
		failures++;
	    }
	    if (emit_report) {
		std::printf("oblique component=%.17g reverse=%d edge-dot=%.17g "
		"distance=%.17g frame=%d/%zu roots/boxes=%zu/%zu seam=%zu "
		"contact=%zu/%zu/%zu oblique=%zu/%zu/%zu union=%zu/%zu/%zu "
		"selected/final=%zu/%zu failures=%zu/%zu/%zu/%zu/%zu\n",
		cases[case_index].component, reverse,
		edge ? edge->ray_edge_dot : INFINITY,
		edge ? edge->distance : INFINITY,
		edge ? edge->frame_interval_supported : -1,
		edge ? edge->frame_interval_cells : 0, trace.stored_local_roots,
		trace.stored_surface_boxes, trace.physical_event_seam_certified,
		trace.physical_event_seam_contact_pairs,
		trace.physical_event_seam_contact_boxes,
		trace.physical_event_seam_contact_roots,
		trace.physical_event_seam_oblique_pairs,
		trace.physical_event_seam_oblique_cells,
		trace.physical_event_seam_oblique_box_links,
		trace.physical_event_seam_source_union_certified,
		trace.physical_event_seam_source_union_root_boxes,
		trace.physical_event_seam_source_union_boxes,
		trace.prepared_production_selected, trace.final_segments,
		trace.physical_event_seam_failures,
		trace.physical_event_seam_ownership_failures,
		trace.physical_event_seam_witness_failures,
		trace.physical_event_seam_box_failures,
		trace.physical_event_seam_root_coverage_failures);
	    }
	    if (!emit_report)
		continue;
	    for (size_t root_index = 0; root_index < trace.stored_local_roots;
		    ++root_index) {
		const struct rt_brep_trace_local_root &root =
		    trace.local_roots[root_index];
		std::printf("  root[%zu] face/span=%d/%d t=%.17g uv=%.17g/%.17g "
		    "dir/class/trim=%d/%d/%d ndot=%.17g\n", root_index,
		    root.face_index, root.span_index, root.dist, root.uv[0],
		    root.uv[1], root.direction, root.hit_class,
		    root.trim_status, root.normal_dot);
	    }
	    for (size_t box_index = 0; box_index < trace.stored_surface_boxes;
		    ++box_index) {
		const struct rt_brep_trace_surface_box &box =
		    trace.surface_boxes[box_index];
		std::printf("  box[%zu] face/span=%d/%d uv=[%.17g %.17g]x"
		    "[%.17g %.17g] t=[%.17g %.17g] depth=%d disp/det=%d/%d\n",
		    box_index, box.face_index, box.span_index, box.uv_min[0],
		    box.uv_max[0], box.uv_min[1], box.uv_max[1], box.t_min,
		    box.t_max, box.depth, box.disposition, box.determinant_sign);
	    }
	}
    }

    free_solid(stp);
    rt_clean_resource_basic(rtip, &resource);
    BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
    rt_i_destroy(rtip);
    if (!failures)
	std::printf("Cobb oblique contact trend: PASS certified=%zu "
	    "fail-closed=%zu observed=%zu/%zu\n", certified, fail_closed,
	    observed_certified, observed_fallback);
    return failures;
}


static int
brep_trace_contact_local_root_certificate(const struct soltab *stp,
    const sampled_ray &ray, const struct rt_brep_shot_trace &trace,
    struct rt_brep_local_root_test_result &result, double &root_separation,
    bool &analytic_extension, size_t &miss_count, size_t &hit_count)
{
    const struct rt_brep_trace_local_root *miss = NULL;
    miss_count = 0;
    for (size_t root_index = 0; root_index < trace.stored_local_roots;
	    ++root_index) {
	const struct rt_brep_trace_local_root &root =
	    trace.local_roots[root_index];
	if (root.trim_status == 1 &&
		(root.hit_class == 1 || root.hit_class == 3)) {
	    if (!miss)
		miss = &root;
	    miss_count++;
	}
    }
    if (miss_count != 1 || !miss)
	return 0;

    const struct rt_brep_trace_local_root *hit = NULL;
    hit_count = 0;
    for (size_t root_index = 0; root_index < trace.stored_local_roots;
	    ++root_index) {
	const struct rt_brep_trace_local_root &root =
	    trace.local_roots[root_index];
	if (root.face_index == miss->face_index &&
		root.span_index == miss->span_index && root.trim_status != 1 &&
		(root.hit_class == 0 || root.hit_class == 2)) {
	    if (!hit)
		hit = &root;
	    hit_count++;
	}
    }
    if (hit_count != 1 || !hit ||
	    !_rt_brep_surface_local_root_test(stp, ray.origin, ray.direction,
		miss->face_index, miss->span_index, miss->uv, 1.0, &result) ||
	    !result.available || !result.certified ||
	    !result.model_image_available || !(result.weight_minimum > 0.0) ||
	    !std::isfinite(result.model_image_displacement))
	return 0;

    fastf_t normalized_hit[2];
    root_separation = 0.0;
    analytic_extension = false;
    for (int direction = 0; direction < 2; ++direction) {
	const double span_length = result.span_maximum[direction] -
	    result.span_minimum[direction];
	if (!(span_length > 0.0) || !std::isfinite(span_length))
	    return 0;
	normalized_hit[direction] = (hit->uv[direction] -
	    result.span_minimum[direction]) / span_length;
	if (!std::isfinite(normalized_hit[direction]))
	    return 0;
	root_separation = std::max(root_separation,
	    fabs(normalized_hit[direction] -
		result.normalized_root[direction]));
	analytic_extension = analytic_extension ||
	    result.normalized_root[direction] - result.radius < 0.0 ||
	    result.normalized_root[direction] + result.radius > 1.0;
    }
    return root_separation > 0.0 &&
	result.radius < 0.25 * root_separation;
}


static int
check_cobb_nonisoparametric_oblique_side(const struct bn_tol *tol,
    bool contact_face_trim)
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
    double maximum_lift_shift = INFINITY;
    const double target_lift_shift =
	(contact_face_trim ? 0.1 : 0.05) * tol->dist;
    int curved_face = -1;
    if (variant) {
	const ON_BrepEdge &edge = variant->m_E[frame.edge_index];
	for (int side = 0; side < edge.m_ti.Count(); ++side) {
	    const int trim_index = edge.m_ti[side];
	    if (trim_index >= 0 && trim_index < variant->m_T.Count() &&
		    (variant->m_T[trim_index].FaceIndexOf() ==
		    frame.face_index) == contact_face_trim) {
		curved_face = variant->m_T[trim_index].FaceIndexOf();
		break;
	    }
	}
    }
    double maximum_u_shift = INFINITY;
    double maximum_v_shift = INFINITY;
    if (!variant || !cobb_curve_edge_trim(variant, frame.edge_index,
	    curved_face, target_lift_shift, maximum_lift_shift,
	    maximum_u_shift, maximum_v_shift) ||
	    !std::isfinite(maximum_u_shift) ||
	    !std::isfinite(maximum_v_shift)) {
	std::printf("FAIL: Cobb non-isoparametric %s trim construction\n",
	    contact_face_trim ? "contact-face" : "opposite-face");
	delete variant;
	return 1;
    }
    measured_gap = cobb_seam_discrepancy(variant, frame.edge_index);
    if (!std::isfinite(measured_gap) || !(measured_gap > 0.0)) {
	std::printf("FAIL: Cobb non-isoparametric seam measurement\n");
	delete variant;
	return 1;
    }
    variant->m_E[frame.edge_index].m_tolerance = std::max(
	1.01 * measured_gap, 1.01 * tol->dist);

    struct transform_case {
	const char *name;
	double scale;
	ON_3dVector translation;
	ON_3dVector axis;
	double angle;
    } cases[] = {
	{"identity", 1.0, ON_3dVector(0.0, 0.0, 0.0),
	    ON_3dVector(1.0, 0.0, 0.0), 0.0},
	{"oblique", 1.0, ON_3dVector(-19.0, 23.0, 41.0),
	    ON_3dVector(1.0, -2.0, 0.5), 0.731},
	{"small", 0.01, ON_3dVector(1.25, -2.5, 5.0),
	    ON_3dVector(-0.3, 1.0, 0.7), -1.113},
	{"large", 1.0e4, ON_3dVector(1.0e6, -2.0e6, 3.0e6),
	    ON_3dVector(2.0, 0.25, -1.0), 2.017}
    };
    const double tangent_components[] = {0.001, 0.1};
    int failures = 0;
    size_t certified = 0;
    size_t source_unions = 0;
    size_t miss_roots = 0;
    size_t threshold_fallbacks = 0;
    size_t local_root_certificates = 0;
    size_t local_root_extensions = 0;
    size_t local_root_maximum_attempts = 0;
    size_t local_root_maximum_high_water = 0;
    double local_root_maximum_radius = 0.0;
    double local_root_maximum_correction = 0.0;
    double local_root_maximum_contraction = 0.0;
    double local_root_maximum_image = 0.0;
    for (size_t case_index = 0;
	    case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
	const transform_case &test = cases[case_index];
	const ON_Xform xform = cobb_axis_angle_similarity_transform(test.scale,
	    test.translation, test.axis, test.angle);
	ON_Brep *transformed = new ON_Brep(*variant);
	if (!transformed->Transform(xform)) {
	    std::printf("FAIL: Cobb non-isoparametric %s transform\n",
		test.name);
	    delete transformed;
	    failures++;
	    continue;
	}
	for (int vertex_index = 0; vertex_index < transformed->m_V.Count();
		++vertex_index)
	    transformed->m_V[vertex_index].m_tolerance =
		variant->m_V[vertex_index].m_tolerance * test.scale;
	for (int edge_index = 0; edge_index < transformed->m_E.Count();
		++edge_index)
	    transformed->m_E[edge_index].m_tolerance =
		variant->m_E[edge_index].m_tolerance * test.scale;
	struct bn_tol case_tol = *tol;
	case_tol.dist = tol->dist * test.scale;
	case_tol.dist_sq = case_tol.dist * case_tol.dist;
	struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
	if (!rtip) {
	    delete transformed;
	    failures++;
	    continue;
	}
	rtip->rti_tol = case_tol;
	struct resource resource = {};
	rt_init_resource(&resource, 0, rtip);
	struct rt_brep_internal transformed_internal = {};
	transformed_internal.magic = RT_BREP_INTERNAL_MAGIC;
	transformed_internal.brep = transformed;
	struct rt_db_internal transformed_intern;
	RT_DB_INTERNAL_INIT(&transformed_intern);
	transformed_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	transformed_intern.idb_type = ID_BREP;
	transformed_intern.idb_meth = &OBJ[ID_BREP];
	transformed_intern.idb_ptr = &transformed_internal;
	struct soltab *stp = prep_solid(rtip, &transformed_intern, ID_BREP);
	if (!stp) {
	    std::printf("FAIL: Cobb non-isoparametric %s prep\n",
		test.name);
	    failures++;
	} else {
	    for (size_t component_index = 0; component_index <
		    sizeof(tangent_components) /
		    sizeof(tangent_components[0]); ++component_index) {
		const double component = tangent_components[component_index];
		const bool expect_source_union = component_index == 1;
		for (int reverse = 0; reverse <= 1; ++reverse) {
		    const sampled_ray base_ray = cobb_seam_oblique_ray(frame,
			origin, radius, tol->dist, component, reverse != 0);
		    const ON_3dPoint transformed_origin = cobb_transform_point(
			xform, ON_3dPoint(base_ray.origin));
		    ON_3dVector transformed_direction = cobb_transform_vector(
			xform, ON_3dVector(base_ray.direction));
		    if (!transformed_direction.Unitize()) {
			failures++;
			continue;
		    }
		    sampled_ray ray;
		    VSET(ray.origin, transformed_origin.x, transformed_origin.y,
			transformed_origin.z);
		    VSET(ray.direction, transformed_direction.x,
			transformed_direction.y, transformed_direction.z);
		    const ray_result production_result = shoot_solid(stp, rtip,
			&resource, ray.origin, ray.direction);
		    struct rt_brep_shot_trace trace;
		    const int trace_hits = shoot_brep_trace(stp, rtip, &resource,
			ray, trace);
		    const struct rt_brep_trace_edge *edge = brep_trace_edge(trace,
			frame.edge_index);
		    struct rt_brep_local_root_test_result local_root = {};
		    double local_root_separation = 0.0;
		    bool local_root_extension = false;
		    size_t local_root_misses = 0;
		    size_t local_root_hits = 0;
		    const bool local_root_evidence = !contact_face_trim ||
			brep_trace_contact_local_root_certificate(stp, ray, trace,
			    local_root, local_root_separation,
			    local_root_extension, local_root_misses,
			    local_root_hits);
		    local_root_maximum_attempts = std::max(
			local_root_maximum_attempts, local_root.attempts);
		    local_root_maximum_high_water = std::max(
			local_root_maximum_high_water,
			local_root.expansion_high_water);
		    local_root_maximum_radius = std::max(
			local_root_maximum_radius, (double)local_root.radius);
		    local_root_maximum_correction = std::max(
			local_root_maximum_correction,
			(double)local_root.correction_bound);
		    local_root_maximum_contraction = std::max(
			local_root_maximum_contraction,
			(double)local_root.contraction_bound);
		    local_root_maximum_image = std::max(
			local_root_maximum_image,
			(double)local_root.model_image_displacement);
		    const double transformed_radius = radius * test.scale;
		    const double half_chord = sqrt(2.0 * transformed_radius *
			case_tol.dist - case_tol.dist * case_tol.dist);
		    partition_result oracle;
		    oracle.partitions = 1;
		    oracle.intervals[0].in_dist = 2.0 * transformed_radius -
			half_chord;
		    oracle.intervals[0].out_dist = 2.0 * transformed_radius +
			half_chord;
		    const bool no_source_union =
			!trace.physical_event_seam_source_union_certified &&
			!trace.physical_event_seam_source_union_root_boxes &&
			!trace.physical_event_seam_source_union_boxes;
		    const bool valid_source_union =
			trace.physical_event_seam_source_union_certified == 1 &&
			trace.physical_event_seam_source_union_root_boxes > 0 &&
			trace.physical_event_seam_source_union_root_boxes <
			    trace.physical_event_seam_source_union_boxes;
		    const bool source_union_evidence = expect_source_union ?
			(case_index ? (no_source_union || valid_source_union) :
			 valid_source_union) : no_source_union;
		    const bool contact_closure_split = !contact_face_trim ||
			(!trace.closure_candidates &&
			 !trace.continuation_attempts && edge &&
			 !edge->tolerance_inferred &&
			 edge->declared_tolerance > edge->model_tolerance &&
			 edge->measured_discrepancy > edge->model_tolerance &&
			 trace.physical_event_seam_closure_candidates == 1 &&
			 trace.physical_event_seam_continuation_candidates == 1);
		    const bool local_root_transaction = !contact_face_trim ||
			(trace.physical_event_seam_declared_contact_pairs == 1 &&
			 trace.physical_event_seam_local_root_attempts == 1 &&
			 trace.physical_event_seam_local_root_available == 1 &&
			 trace.physical_event_seam_local_root_certified == 1 &&
			 trace.physical_event_seam_local_root_extensions == 1 &&
			 !trace.physical_event_seam_local_root_tube_failures &&
			 trace.physical_event_seam_local_root_edge_upper <=
			    trace.physical_event_seam_local_root_tube_tolerance +
			    trace.physical_event_seam_local_root_tube_roundoff &&
			 trace.physical_event_seam_local_root_trim_upper <=
			    trace.physical_event_seam_local_root_tube_tolerance +
			    trace.physical_event_seam_local_root_tube_roundoff);
		    const bool bad = !brep_trace_fixed_workspaces_match(trace) ||
			!edge || !edge->correspondence_supported ||
			edge->correspondence_exhausted ||
			!edge->discrepancy_authorized ||
			edge->discrepancy_proof_class !=
			    RT_BREP_SEAM_GAP_INSIDE ||
			!edge->frame_interval_supported ||
			!edge->frame_interval_cells ||
			!edge->within_edge_tolerance || !edge->sector_valid ||
			trace_hits != 2 || trace.final_segments != 1 ||
			production_result.segments != 1 ||
			trace.physical_event_seam_oblique_pairs != 1 ||
			!trace.physical_event_seam_oblique_cells ||
			trace.physical_event_seam_oblique_box_links !=
			    trace.physical_event_seam_contact_boxes ||
			trace.physical_event_seam_contact_miss_roots !=
			    (contact_face_trim ? 1 : 0) ||
			!local_root_evidence ||
			!local_root_transaction ||
			!contact_closure_split ||
			!source_union_evidence ||
			trace.prepared_production_selected != 1 ||
			trace.prepared_production_fallback !=
			    RT_BREP_PREPARED_FALLBACK_NONE ||
			!brep_trace_seam_event_stream_valid(trace, edge,
			    transformed, oracle, case_tol.dist);
		    if (bad) {
			std::printf("FAIL: Cobb non-isoparametric %s "
			    "component=%.17g reverse=%d "
			    "edge/correspondence/frame=%d/%d/%d "
			    "hits/segments=%d/%zu/%d oblique=%zu/%zu/%zu "
			    "closure=%zu/%zu/%zu "
			    "union=%zu/%zu/%zu selected/fallback=%zu/%d "
			    "root-proof=%d/%d/%zu/%zu/%.3g/%.3g "
			    "transaction=%zu/%zu/%zu/%zu/%zu/%zu "
			    "failures=%zu/%zu/%zu/%zu/%zu\n", test.name,
			    component, reverse, edge != NULL,
			    edge ? edge->correspondence_supported : 0,
			    edge ? edge->frame_interval_supported : 0,
			    trace_hits, trace.final_segments,
			    production_result.segments,
			    trace.physical_event_seam_oblique_pairs,
			    trace.physical_event_seam_oblique_cells,
			    trace.physical_event_seam_oblique_box_links,
			    trace.closure_candidates,
			    trace.physical_event_seam_closure_candidates,
			    trace.physical_event_seam_continuation_candidates,
			    trace.physical_event_seam_source_union_certified,
			    trace.physical_event_seam_source_union_root_boxes,
			    trace.physical_event_seam_source_union_boxes,
			    trace.prepared_production_selected,
			    trace.prepared_production_fallback,
			    local_root.available, local_root.certified,
			    local_root_misses, local_root_hits,
			    local_root.radius, local_root_separation,
			    trace.physical_event_seam_declared_contact_pairs,
			    trace.physical_event_seam_local_root_attempts,
			    trace.physical_event_seam_local_root_available,
			    trace.physical_event_seam_local_root_certified,
			    trace.physical_event_seam_local_root_extensions,
			    trace.physical_event_seam_local_root_tube_failures,
			    trace.physical_event_seam_failures,
			    trace.physical_event_seam_ownership_failures,
			    trace.physical_event_seam_witness_failures,
			    trace.physical_event_seam_box_failures,
			    trace.physical_event_seam_root_coverage_failures);
			failures++;
		    } else {
			certified++;
			source_unions += valid_source_union ? 1 : 0;
			miss_roots +=
			    trace.physical_event_seam_contact_miss_roots;
			if (contact_face_trim) {
			    local_root_certificates++;
			    local_root_extensions +=
				local_root_extension ? 1 : 0;
			}
		    }
		}
	    }
	    free_solid(stp);
	}
	rt_clean_resource_basic(rtip, &resource);
	BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
	rt_i_destroy(rtip);
    }

    if (contact_face_trim) {
	struct tolerance_case {
	    const char *name;
	    double declared;
	    bool inferred;
	} tolerance_cases[] = {
	    {"model-only", tol->dist, false},
	    {"exact-gap", measured_gap, false},
	    {"unset", ON_UNSET_VALUE, true}
	};
	for (size_t tolerance_index = 0;
		tolerance_index < sizeof(tolerance_cases) /
		    sizeof(tolerance_cases[0]); ++tolerance_index) {
	    const tolerance_case &policy = tolerance_cases[tolerance_index];
	    ON_Brep *control = new ON_Brep(*variant);
	    control->m_E[frame.edge_index].m_tolerance = policy.declared;
	    struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
	    if (!rtip) {
		delete control;
		failures++;
		continue;
	    }
	    rtip->rti_tol = *tol;
	    struct resource resource = {};
	    rt_init_resource(&resource, 0, rtip);
	    struct rt_brep_internal control_internal = {};
	    control_internal.magic = RT_BREP_INTERNAL_MAGIC;
	    control_internal.brep = control;
	    struct rt_db_internal control_intern;
	    RT_DB_INTERNAL_INIT(&control_intern);
	    control_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	    control_intern.idb_type = ID_BREP;
	    control_intern.idb_meth = &OBJ[ID_BREP];
	    control_intern.idb_ptr = &control_internal;
	    struct soltab *stp = prep_solid(rtip, &control_intern, ID_BREP);
	    if (!stp) {
		std::printf("FAIL: Cobb contact-trim %s prep\n", policy.name);
		failures++;
	    } else {
		for (int reverse = 0; reverse <= 1; ++reverse) {
		    const sampled_ray ray = cobb_seam_oblique_ray(frame, origin,
			radius, tol->dist, 0.001, reverse != 0);
		    const ray_result production_result = shoot_solid(stp, rtip,
			&resource, ray.origin, ray.direction);
		    struct rt_brep_shot_trace trace;
		    const int trace_hits = shoot_brep_trace(stp, rtip, &resource,
			ray, trace);
		    const struct rt_brep_trace_edge *edge = brep_trace_edge(trace,
			frame.edge_index);
		    const double half_chord = sqrt(2.0 * radius * tol->dist -
			tol->dist * tol->dist);
		    const double expected_in = 2.0 * radius - half_chord;
		    const double expected_out = 2.0 * radius + half_chord;
		    const double endpoint_error =
			production_result.segments == 1 ?
			std::max(fabs(production_result.in_dist - expected_in),
			    fabs(production_result.out_dist - expected_out)) :
			INFINITY;
		    const bool bad = !brep_trace_fixed_workspaces_match(trace) ||
			!edge || edge->tolerance_inferred !=
			    (policy.inferred ? 1 : 0) ||
			trace_hits != 2 || trace.final_segments != 1 ||
			production_result.segments != 1 ||
			trace.prepared_production_selected ||
			trace.prepared_production_fallback ==
			    RT_BREP_PREPARED_FALLBACK_NONE ||
			trace.closure_candidates || trace.continuation_attempts ||
			trace.physical_event_seam_closure_candidates ||
			trace.physical_event_seam_continuation_candidates ||
			trace.physical_event_seam_certified ||
			trace.physical_event_seam_contact_pairs ||
			trace.physical_event_seam_contact_boxes ||
			trace.physical_event_seam_contact_roots ||
			trace.physical_event_seam_contact_miss_roots ||
			trace.physical_event_seam_declared_contact_pairs ||
			trace.physical_event_seam_local_root_attempts ||
			trace.physical_event_seam_local_root_available ||
			trace.physical_event_seam_local_root_certified ||
			trace.physical_event_seam_local_root_extensions ||
			trace.physical_event_seam_local_root_tube_failures ||
			trace.physical_event_seam_oblique_pairs ||
			trace.physical_event_seam_source_union_certified ||
			trace.physical_event_seam_source_union_root_boxes ||
			trace.physical_event_seam_source_union_boxes;
		    if (bad) {
			std::printf("FAIL: Cobb contact-trim threshold %s "
			    "reverse=%d edge/inferred=%d/%d declared=%.17g "
			    "hits/segments=%d/%zu/%d error=%.17g "
			    "selected/fallback=%zu/%d seam/oblique=%zu/%zu\n",
			    policy.name, reverse, edge != NULL,
			    edge ? edge->tolerance_inferred : -1,
			    edge ? edge->declared_tolerance : INFINITY, trace_hits,
			    trace.final_segments, production_result.segments,
			    endpoint_error, trace.prepared_production_selected,
			    trace.prepared_production_fallback,
			    trace.physical_event_seam_certified,
			    trace.physical_event_seam_oblique_pairs);
			failures++;
		    } else {
			threshold_fallbacks++;
		    }
		}
		free_solid(stp);
	    }
	    rt_clean_resource_basic(rtip, &resource);
	    BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
	    rt_i_destroy(rtip);
	}
    }

    if (contact_face_trim &&
	    (local_root_certificates != 16 || local_root_extensions != 16)) {
	std::printf("FAIL: Cobb contact-trim local root certificates="
	    "%zu/16 extensions=%zu/16\n", local_root_certificates,
	    local_root_extensions);
	failures++;
    }

    delete variant;
    if (!failures)
	std::printf("Cobb non-isoparametric %s oblique seam: PASS "
	    "cases=%zu certified=%zu source-unions=%zu "
	    "miss-roots=%zu threshold-fallbacks=%zu "
	    "local-root=%zu/%zu attempts/high-water=%zu/%zu "
	    "radius/correction/contraction/image=%.3g/%.3g/%.3g/%.3g "
	    "lift/T=%.6g gap/T=%.6g\n",
	    contact_face_trim ? "contact-face" : "opposite-face",
	    sizeof(cases) / sizeof(cases[0]),
	    certified, source_unions, miss_roots, threshold_fallbacks,
	    local_root_certificates, local_root_extensions,
	    local_root_maximum_attempts, local_root_maximum_high_water,
	    local_root_maximum_radius, local_root_maximum_correction,
	    local_root_maximum_contraction, local_root_maximum_image,
	    maximum_lift_shift / tol->dist,
	    measured_gap / tol->dist);
    return failures;
}


static int
check_cobb_nonisoparametric_oblique(const struct bn_tol *tol)
{
    return check_cobb_nonisoparametric_oblique_side(tol, false);
}


static int
check_cobb_contact_trim_oblique(const struct bn_tol *tol)
{
    return check_cobb_nonisoparametric_oblique_side(tol, true);
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
		!edge->discrepancy_sample_authorized ||
		edge->discrepancy_authorized ||
		edge->discrepancy_proof_class !=
		RT_BREP_SEAM_GAP_UNAVAILABLE ||
		!edge->correspondence_screened ||
		edge->correspondence_supported || !edge->correspondence_cells ||
		!edge->correspondence_exhausted ||
		edge->correspondence_depth != 24 ||
		edge->frame_interval_supported || edge->frame_interval_cells ||
		edge->candidate_spans ||
		edge->within_edge_tolerance || edge->sector_valid ||
		edge->discrepancy_bounded ||
		edge->discrepancy_bound_exhausted ||
		trace.closure_candidates || trace.continuation_attempts ||
		trace.closure_shadow_segments ||
		trace.after_direction_cleanup != 1 || trace.final_segments != 0) {
	    std::printf("FAIL: Cobb ambiguous correspondence reverse=%d "
		"edge=%d measured=%d sample=%d authorized=%d proof=%d "
		"correspondence=%d/%d "
		"cells=%zu depth=%zu proof-exhausted=%d frame=%d/%zu "
		"spans=%zu within=%d sector=%d bound=%d exhausted=%d "
		"closure=%zu continuation=%zu segment=%zu cleanup=%zu\n",
		reverse,
		frame.edge_index, edge ? edge->discrepancy_measured : -1,
		edge ? edge->discrepancy_sample_authorized : -1,
		edge ? edge->discrepancy_authorized : -1,
		edge ? edge->discrepancy_proof_class : -1,
		edge ? edge->correspondence_screened : -1,
		edge ? edge->correspondence_supported : -1,
		edge ? edge->correspondence_cells : 0,
		edge ? edge->correspondence_depth : 0,
		edge ? edge->correspondence_exhausted : -1,
		edge ? edge->frame_interval_supported : -1,
		edge ? edge->frame_interval_cells : 0,
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
	    !edge->discrepancy_sample_authorized ||
	    !edge->correspondence_screened ||
	    !edge->correspondence_supported ||
	    !edge->correspondence_cells ||
	    edge->correspondence_exhausted ||
	    (edge->discrepancy_bounded &&
	    edge->discrepancy_bound_exhausted);
	if (edge->discrepancy_bounded) {
	    invalid = invalid || !edge->discrepancy_authorized ||
		edge->discrepancy_proof_class !=
		RT_BREP_SEAM_GAP_INSIDE ||
		edge->discrepancy_lower_bound >
		target.measured_discrepancy + limit ||
		edge->discrepancy_upper_bound <
		target.measured_discrepancy - limit ||
		!(edge->discrepancy_upper_bound < edge->edge_tolerance);
	} else {
	    invalid = invalid || edge->discrepancy_authorized ||
		edge->discrepancy_proof_class !=
		RT_BREP_SEAM_GAP_UNAVAILABLE ||
		!edge->discrepancy_bound_exhausted;
	}
	if (invalid) {
	    std::printf("FAIL: Cobb adaptive seam budget edge=%d "
		"measured=%.17g bound=%.17g/%.17g target=%.17g "
		"bounded=%d exhausted=%d proof=%d authorized=%d "
		"cells=%zu depth=%zu\n",
		target.edge_index, target.measured_discrepancy,
		edge->discrepancy_lower_bound,
		edge->discrepancy_upper_bound,
		edge->discrepancy_bound_tolerance,
		edge->discrepancy_bounded,
		edge->discrepancy_bound_exhausted,
		edge->discrepancy_proof_class,
		edge->discrepancy_authorized,
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
	bool sample_authorized;
	int proof_class;
	bool authorized;
    } cases[] = {
	{"correct", 1.01, false, false, true,
	    RT_BREP_SEAM_GAP_INSIDE, true},
	{"unset", 0.0, true, true, true,
	    RT_BREP_SEAM_GAP_AMBIGUOUS, false},
	{"explicit-zero", 0.0, false, false, false,
	    RT_BREP_SEAM_GAP_OUTSIDE, false},
	{"half", 0.5, false, false, false,
	    RT_BREP_SEAM_GAP_OUTSIDE, false},
	{"double", 2.0, false, false, true,
	    RT_BREP_SEAM_GAP_INSIDE, true}
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
	const bool proof_bounds_ok = edge &&
	    (cases[case_index].proof_class == RT_BREP_SEAM_GAP_INSIDE ?
	    edge->discrepancy_upper_bound < edge->edge_tolerance :
	    (cases[case_index].proof_class == RT_BREP_SEAM_GAP_OUTSIDE ?
	    edge->discrepancy_lower_bound > edge->edge_tolerance :
	    edge->discrepancy_lower_bound <= edge->edge_tolerance &&
	    edge->discrepancy_upper_bound >= edge->edge_tolerance));
	if (!brep_trace_fixed_workspaces_match(trace) ||
		!edge || !edge->discrepancy_measured ||
		!edge->correspondence_screened ||
		!edge->correspondence_supported ||
		!edge->correspondence_cells ||
		edge->correspondence_exhausted ||
		!edge->discrepancy_bounded ||
		edge->discrepancy_bound_exhausted ||
		!declared_ok || edge->tolerance_inferred !=
		cases[case_index].inferred ||
		edge->discrepancy_sample_authorized !=
		cases[case_index].sample_authorized ||
		edge->discrepancy_proof_class !=
		cases[case_index].proof_class || !proof_bounds_ok ||
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
		"inferred=%d/%d sample=%d/%d proof=%d/%d "
		"authorized=%d/%d within=%d spans=%zu\n",
		cases[case_index].name,
		edge ? edge->declared_tolerance : INFINITY,
		edge ? edge->model_tolerance : INFINITY,
		edge ? edge->measured_discrepancy : INFINITY,
		edge ? edge->discrepancy_lower_bound : INFINITY,
		edge ? edge->discrepancy_upper_bound : INFINITY,
		edge ? edge->edge_tolerance : INFINITY,
		edge ? edge->tolerance_inferred : -1,
		cases[case_index].inferred,
		edge ? edge->discrepancy_sample_authorized : -1,
		cases[case_index].sample_authorized,
		edge ? edge->discrepancy_proof_class : -1,
		cases[case_index].proof_class,
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
check_cobb_isolated_trim_transition_similarity(const struct bn_tol *tol)
{
    if (!tol || !(tol->dist > 0.0))
	return 1;
    const double radius = 10.0;
    const ON_3dPoint origin(0.0, 0.0, 0.0);
    ON_Brep *pristine = ON_Brep_CobbSphereSewn(radius, origin);
    if (!pristine) {
	std::printf("FAIL: isolated trim transition similarity pristine\n");
	return 1;
    }
    cobb_seam_frame frame;
    double measured_gap = INFINITY;
    double applied_displacement = INFINITY;
    double maximum_u_shift = INFINITY;
    double maximum_v_shift = INFINITY;
    ON_Brep *base = cobb_trim_only_variant(pristine, origin,
	0.9 * tol->dist, frame, measured_gap, applied_displacement,
	maximum_u_shift, maximum_v_shift);
    delete pristine;
    if (!base || !std::isfinite(measured_gap) ||
	    fabs(measured_gap - 0.9 * tol->dist) > 0.01 * tol->dist) {
	std::printf("FAIL: isolated trim transition similarity construction\n");
	delete base;
	return 1;
    }
    base->m_E[frame.edge_index].m_tolerance = tol->dist;

    struct transform_case {
	const char *name;
	double scale;
	ON_3dVector translation;
	ON_3dVector axis;
	double angle;
    } cases[] = {
	{"oblique", 1.0, ON_3dVector(-19.0, 23.0, 41.0),
	    ON_3dVector(1.0, -2.0, 0.5), 0.731},
	{"small", 0.01, ON_3dVector(1.25, -2.5, 5.0),
	    ON_3dVector(-0.3, 1.0, 0.7), -1.113},
	{"large", 1.0e4, ON_3dVector(1.0e6, -2.0e6, 3.0e6),
	    ON_3dVector(2.0, 0.25, -1.0), 2.017}
    };
    int failures = 0;
    size_t rays = 0;
    double maximum_normalized_error = 0.0;
    for (size_t case_index = 0;
	    case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
	const transform_case &test = cases[case_index];
	const ON_Xform xform = cobb_axis_angle_similarity_transform(test.scale,
	    test.translation, test.axis, test.angle);
	ON_Brep *variant = new ON_Brep(*base);
	if (!variant->Transform(xform)) {
	    std::printf("FAIL: isolated trim transition %s transform\n",
		test.name);
	    delete variant;
	    failures++;
	    continue;
	}
	for (int vertex_index = 0; vertex_index < variant->m_V.Count();
		++vertex_index)
	    variant->m_V[vertex_index].m_tolerance =
		base->m_V[vertex_index].m_tolerance * test.scale;
	for (int edge_index = 0; edge_index < variant->m_E.Count();
		++edge_index)
	    variant->m_E[edge_index].m_tolerance =
		base->m_E[edge_index].m_tolerance * test.scale;
	struct bn_tol case_tol = *tol;
	case_tol.dist = tol->dist * test.scale;
	case_tol.dist_sq = case_tol.dist * case_tol.dist;
	struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
	if (!rtip) {
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
	    std::printf("FAIL: isolated trim transition %s prep\n",
		test.name);
	    failures++;
	} else {
	    for (int reverse = 0; reverse <= 1; ++reverse) {
		sampled_ray base_ray;
		const double chord = 1.1 * tol->dist;
		const bool ray_valid = cobb_seam_endpoint_chord_ray(frame, origin,
		    radius, chord, 1, reverse != 0, base_ray);
		if (!ray_valid) {
		    std::printf("FAIL: isolated trim transition %s reverse=%d "
			"ray construction\n", test.name, reverse);
		    failures++;
		    continue;
		}
		const ON_3dPoint transformed_origin = cobb_transform_point(xform,
		    ON_3dPoint(base_ray.origin));
		ON_3dVector transformed_direction = cobb_transform_vector(xform,
		    ON_3dVector(base_ray.direction));
		const bool direction_valid = transformed_direction.Unitize();
		sampled_ray ray;
		VSET(ray.origin, transformed_origin.x, transformed_origin.y,
		    transformed_origin.z);
		VSET(ray.direction, transformed_direction.x,
		    transformed_direction.y, transformed_direction.z);
		const ray_result result = shoot_solid(stp, rtip, &resource,
		    ray.origin, ray.direction);
		struct rt_brep_shot_trace trace;
		const int trace_hits = shoot_brep_trace(stp, rtip, &resource,
		    ray, trace);
		const struct rt_brep_trace_edge *edge = brep_trace_edge(trace,
		    frame.edge_index);
		const double expected_in = 2.0 * radius * test.scale;
		const double expected_out = (2.0 * radius + chord) * test.scale;
		const double normalized_error = std::max(
		    fabs(result.in_dist - expected_in),
		    fabs(result.out_dist - expected_out)) / test.scale;
		maximum_normalized_error = std::max(maximum_normalized_error,
		    normalized_error);
		if (!direction_valid || !edge ||
			!edge->correspondence_supported ||
			edge->correspondence_exhausted ||
			!edge->discrepancy_endpoints_certified ||
			!edge->discrepancy_bounded ||
			edge->discrepancy_bound_exhausted ||
			!edge->discrepancy_authorized ||
			edge->discrepancy_proof_class !=
			    RT_BREP_SEAM_GAP_INSIDE ||
			!brep_trace_fixed_workspaces_match(trace) ||
			trace_hits != 2 || trace.final_segments != 1 ||
			result.segments != 1 ||
			trace.physical_event_edge_attempts != 1 ||
			trace.physical_event_edge_candidates != 1 ||
			trace.physical_event_edge != 1 ||
			trace.physical_event_edge_contacts ||
			trace.physical_event_edge_tolerance_transitions != 1 ||
			trace.physical_event_edge_joint_components != 1 ||
			trace.physical_event_edge_joint_failure_stage ||
			trace.physical_event_edge_certified != 1 ||
			trace.physical_event_edge_failures ||
			trace.physical_event_complete != 1 ||
			trace.physical_event_material_segments != 1 ||
			trace.prepared_production_selected != 1 ||
			trace.prepared_production_fallback !=
			    RT_BREP_PREPARED_FALLBACK_NONE ||
			!finite_unit_vector(result.in_normal) ||
			!finite_unit_vector(result.out_normal) ||
			VDOT(result.in_normal, ray.direction) >= 0.0 ||
			VDOT(result.out_normal, ray.direction) <= 0.0 ||
			normalized_error > tol->dist) {
		    std::printf("FAIL: isolated trim transition %s reverse=%d "
			"edge/auth=%d/%d hits/segments=%d/%zu/%d "
			"candidate/event/tolerance/joint/cert/fail="
			"%zu/%zu/%zu/%zu/%zu/%zu selected/fallback=%zu/%d "
			"error=%.17g\n", test.name, reverse, edge != NULL,
			edge ? edge->discrepancy_authorized : -1, trace_hits,
			trace.final_segments, result.segments,
			trace.physical_event_edge_candidates,
			trace.physical_event_edge,
			trace.physical_event_edge_tolerance_transitions,
			trace.physical_event_edge_joint_components,
			trace.physical_event_edge_certified,
			trace.physical_event_edge_failures,
			trace.prepared_production_selected,
			trace.prepared_production_fallback, normalized_error);
		    failures++;
		} else {
		    rays++;
		}
	    }
	    free_solid(stp);
	}
	rt_clean_resource_basic(rtip, &resource);
	BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
	rt_i_destroy(rtip);
    }
    delete base;
    if (rays != 6) {
	std::printf("FAIL: isolated trim transition similarity coverage=%zu/6\n",
	    rays);
	failures++;
    }
    if (!failures)
	std::printf("Cobb isolated trim transition similarity: PASS "
	    "rays=%zu max-normalized-error=%.3g\n", rays,
	    maximum_normalized_error);
    return failures;
}


static int
check_cobb_isolated_defect_corpus(const struct bn_tol *tol,
    struct rt_i *rtip, struct resource *resource)
{
    if (!tol || !rtip || !resource)
	return 1;
    const double radius = 10.0;
    const ON_3dPoint origin(0.0, 0.0, 0.0);
    ON_Brep *pristine = ON_Brep_CobbSphereSewn(radius, origin);
    if (!pristine) {
	std::printf("FAIL: isolated Cobb defect pristine construction\n");
	return 1;
    }

    struct rt_ell_internal sphere = {};
    sphere.magic = RT_ELL_INTERNAL_MAGIC;
    VSET(sphere.v, 0.0, 0.0, 0.0);
    VSET(sphere.a, radius, 0.0, 0.0);
    VSET(sphere.b, 0.0, radius, 0.0);
    VSET(sphere.c, 0.0, 0.0, radius);
    struct rt_db_internal sphere_intern;
    RT_DB_INTERNAL_INIT(&sphere_intern);
    sphere_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    sphere_intern.idb_type = ID_ELL;
    sphere_intern.idb_meth = &OBJ[ID_ELL];
    sphere_intern.idb_ptr = &sphere;
    struct soltab *implicit_stp = prep_solid(rtip, &sphere_intern, ID_ELL);
    if (!implicit_stp) {
	delete pristine;
	std::printf("FAIL: isolated Cobb defect implicit prep\n");
	return 1;
    }

    struct defect_family {
	const char *name;
	bool trim_only;
	int signs;
    } families[] = {
	{"trim-only", true, 1},
	{"edge-only", false, 2}
    };
    const double gap_ratios[] = {0.25, 0.9};
    const double chord_ratios[] = {100.0, 10.0, 2.0, 1.1, 0.0};
    int failures = 0;
    size_t rays = 0;
    size_t prepared = 0;
    size_t fallback = 0;
    size_t contacts = 0;
    size_t family_rays[2] = {0, 0};
    size_t family_prepared[2] = {0, 0};
    size_t family_fallback[2] = {0, 0};
    size_t family_seam_certified[2] = {0, 0};
    size_t family_edge_certified[2] = {0, 0};
    size_t family_regular[2] = {0, 0};
    size_t family_near_trim[2] = {0, 0};
    size_t family_unresolved[2] = {0, 0};
    size_t family_seam_attempts[2] = {0, 0};
    size_t family_seam_candidates[2] = {0, 0};
    size_t family_seam_failures[2] = {0, 0};
    size_t family_seam_ownership_failures[2] = {0, 0};
    size_t family_seam_witness_failures[2] = {0, 0};
    size_t family_seam_box_failures[2] = {0, 0};
    size_t family_seam_root_failures[2] = {0, 0};
    size_t family_edge_attempts[2] = {0, 0};
    size_t family_edge_candidates[2] = {0, 0};
    size_t family_edge_failures[2] = {0, 0};
    size_t family_edge_lift_witnesses[2] = {0, 0};
    size_t family_edge_tolerance_transitions[2] = {0, 0};
    size_t family_edge_candidate_failure_stage[2][9] = {};
    size_t family_joint_attempts[2] = {0, 0};
    size_t family_joint_certified[2] = {0, 0};
    size_t family_joint_boxes[2] = {0, 0};
    size_t family_joint_roots[2] = {0, 0};
    size_t family_joint_complement_visited[2] = {0, 0};
    size_t family_joint_complement_high_water[2] = {0, 0};
    size_t family_joint_failure_stage[2][16] = {};
    size_t family_component_attempts[2] = {0, 0};
    size_t family_component_certified[2] = {0, 0};
    size_t family_component_boxes[2] = {0, 0};
    size_t family_component_roots[2] = {0, 0};
    size_t family_component_failure_stage[2][7] = {};
    size_t family_fallback_reason[2][RT_BREP_PREPARED_FALLBACK_COUNT] = {};
    double maximum_endpoint_error = 0.0;
    double maximum_gap_calibration = 0.0;

    for (size_t family_index = 0;
	    family_index < sizeof(families) / sizeof(families[0]);
	    ++family_index) {
	const defect_family &family = families[family_index];
	for (size_t gap_index = 0;
		gap_index < sizeof(gap_ratios) / sizeof(gap_ratios[0]);
		++gap_index) {
	    for (int sign_index = 0; sign_index < family.signs;
		    ++sign_index) {
		const double sign = sign_index ? -1.0 : 1.0;
		const double target_gap = gap_ratios[gap_index] * tol->dist;
		cobb_seam_frame frame;
		double measured_gap = INFINITY;
		double applied_displacement = INFINITY;
		double maximum_u_shift = 0.0;
		double maximum_v_shift = 0.0;
		ON_Brep *variant = family.trim_only ?
		    cobb_trim_only_variant(pristine, origin, sign * target_gap,
			frame, measured_gap, applied_displacement,
			maximum_u_shift, maximum_v_shift) :
		    cobb_edge_only_variant(pristine, origin, sign * target_gap,
			frame, measured_gap, applied_displacement);
		if (!variant) {
		    std::printf("FAIL: Cobb %s construction sign=%g g/T=%.3g\n",
			family.name, sign, gap_ratios[gap_index]);
		    failures++;
		    continue;
		}
		variant->m_E[frame.edge_index].m_tolerance = tol->dist;
		double edge_gap = INFINITY;
		double surface_gap = cobb_seam_discrepancy(variant,
		    frame.edge_index);
		const double radial_error = ON_Brep_CobbSphereMaxRadialError(
		    variant, radius, origin, 16);
		const double calibration = fabs(measured_gap - target_gap) /
		    target_gap;
		maximum_gap_calibration = std::max(maximum_gap_calibration,
		    calibration);
		const double coordinate_roundoff = std::max(ON_ZERO_TOLERANCE,
		    4096.0 * DBL_EPSILON * radius);
		const bool anisotropic_trim = !family.trim_only ||
		    (std::max(maximum_u_shift, maximum_v_shift) > 0.0 &&
		    std::min(maximum_u_shift, maximum_v_shift) <=
			coordinate_roundoff);
		if (!cobb_edge_lift_discrepancy(variant, frame.edge_index,
			edge_gap) || !variant->IsValid() || !variant->IsSolid() ||
			calibration > 2.0e-3 || radial_error >
			coordinate_roundoff || !anisotropic_trim ||
			fabs(edge_gap - measured_gap) >
			    0.02 * measured_gap ||
			(family.trim_only ?
			 fabs(surface_gap - measured_gap) >
			     0.02 * measured_gap :
			 surface_gap > coordinate_roundoff)) {
		    std::printf("FAIL: Cobb %s geometry sign=%g g/T=%.3g "
			"measured/edge/surface=%.17g/%.17g/%.17g "
			"calibration=%.3g radial=%.3g uv=%.3g/%.3g "
			"valid/solid=%d/%d\n", family.name, sign,
			gap_ratios[gap_index], measured_gap, edge_gap,
			surface_gap, calibration, radial_error,
			maximum_u_shift, maximum_v_shift, variant->IsValid(),
			variant->IsSolid());
		    failures++;
		    delete variant;
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
		struct soltab *stp = prep_solid(rtip, &variant_intern, ID_BREP);
		if (!stp) {
		    std::printf("FAIL: Cobb %s prep sign=%g g/T=%.3g\n",
			family.name, sign, gap_ratios[gap_index]);
		    failures++;
		    continue;
		}

		for (size_t chord_index = 0;
			chord_index < sizeof(chord_ratios) /
			    sizeof(chord_ratios[0]); ++chord_index) {
		    const double chord = chord_ratios[chord_index] * tol->dist;
		    for (int conormal_index = 0; conormal_index < 2;
			    ++conormal_index) {
			const int conormal_sign = conormal_index ? 1 : -1;
			for (int reverse = 0; reverse <= 1; ++reverse) {
			    sampled_ray ray;
			    if (!cobb_seam_endpoint_chord_ray(frame, origin, radius,
				    chord, conormal_sign, reverse != 0, ray)) {
				std::printf("FAIL: Cobb %s ray construction "
				    "dt/T=%.3g side/reverse=%d/%d\n", family.name,
				    chord_ratios[chord_index], conormal_sign,
				    reverse);
				failures++;
				continue;
			    }
			const ray_result implicit_result = shoot_solid(implicit_stp,
			    rtip, resource, ray.origin, ray.direction);
			const ray_result variant_result = shoot_solid(stp, rtip,
			    resource, ray.origin, ray.direction);
			struct rt_brep_shot_trace trace;
			(void)shoot_brep_trace(stp, rtip, resource, ray, trace);
			const struct rt_brep_trace_edge *edge = brep_trace_edge(
			    trace, frame.edge_index);
			const int expected_segments = chord > tol->dist ? 1 : 0;
			double endpoint_error = 0.0;
			if (expected_segments) {
			    const double expected_in = 2.0 * radius;
			    const double expected_out = 2.0 * radius + chord;
			    endpoint_error = std::max(
				fabs(variant_result.in_dist - expected_in),
				fabs(variant_result.out_dist - expected_out));
			    maximum_endpoint_error = std::max(
				maximum_endpoint_error, endpoint_error);
			}
			const bool normal_valid = !expected_segments ||
			    (finite_unit_vector(variant_result.in_normal) &&
			    finite_unit_vector(variant_result.out_normal) &&
			    VDOT(variant_result.in_normal, ray.direction) < 0.0 &&
			    VDOT(variant_result.out_normal, ray.direction) > 0.0);
			const double edge_limit = std::max(1.0e-10 * tol->dist,
			    512.0 * DBL_EPSILON * radius);
			const bool edge_valid = edge &&
			    edge->discrepancy_measured &&
			    edge->correspondence_supported &&
			    !edge->correspondence_exhausted &&
			    edge->discrepancy_endpoints_certified &&
			    edge->discrepancy_bounded &&
			    !edge->discrepancy_bound_exhausted &&
			    edge->discrepancy_proof_class ==
				RT_BREP_SEAM_GAP_INSIDE &&
			    edge->discrepancy_authorized &&
			    fabs(edge->measured_discrepancy - measured_gap) <=
				edge_limit &&
			    fabs(edge->edge_tolerance - tol->dist) <= edge_limit;
			const bool selected = trace.prepared_production_selected > 0;
			const bool joint_metadata_valid =
			    trace.physical_event_edge_joint_components <=
				trace.physical_event_edge_joint_attempts &&
			    (!trace.physical_event_edge_joint_components ||
			     (trace.physical_event_edge_joint_boxes > 0 &&
			      trace.physical_event_edge_joint_roots >= 2)) &&
			    ((trace.physical_event_edge_joint_attempts >
			      trace.physical_event_edge_joint_components) ==
			     (trace.physical_event_edge_joint_failure_stage > 0));
			const bool tolerance_transition_valid =
			    trace.physical_event_edge_tolerance_transitions <= 1 &&
			    (!trace.physical_event_edge_tolerance_transitions ||
			     (family.trim_only && expected_segments && selected &&
			      trace.physical_event_edge_joint_components == 1 &&
			      trace.physical_event_edge == 1 &&
			      !trace.physical_event_edge_contacts));
			if (!brep_trace_fixed_workspaces_match(trace) || !edge_valid ||
				!joint_metadata_valid || !tolerance_transition_valid ||
				implicit_result.segments != expected_segments ||
				variant_result.segments != expected_segments ||
				trace.final_segments != (size_t)expected_segments ||
				endpoint_error > tol->dist || !normal_valid ||
				(selected ? trace.prepared_production_fallback !=
				    RT_BREP_PREPARED_FALLBACK_NONE :
				 trace.prepared_production_fallback ==
				    RT_BREP_PREPARED_FALLBACK_NONE)) {
			    std::printf("FAIL: Cobb %s ray sign=%g g/T=%.3g "
				"dt/T=%.3g side/reverse=%d/%d "
				"implicit/variant/final="
				"%d/%d/%zu error=%.3g edge=%d proof/auth="
				"%d/%d selected/fallback=%d/%d\n",
				family.name, sign, gap_ratios[gap_index],
				chord_ratios[chord_index], conormal_sign, reverse,
				implicit_result.segments, variant_result.segments,
				trace.final_segments, endpoint_error, edge != NULL,
				edge ? edge->discrepancy_proof_class : -1,
				edge ? edge->discrepancy_authorized : -1,
				selected, trace.prepared_production_fallback);
			    failures++;
			} else {
			    prepared += selected ? 1 : 0;
			    fallback += selected ? 0 : 1;
			    contacts += chord_index == 4 ? 1 : 0;
			    family_rays[family_index]++;
			    family_prepared[family_index] += selected ? 1 : 0;
			    family_fallback[family_index] += selected ? 0 : 1;
			    family_seam_certified[family_index] +=
				trace.physical_event_seam_certified;
			    family_edge_certified[family_index] +=
				trace.physical_event_edge_certified;
			    family_regular[family_index] +=
				trace.physical_event_regular;
			    family_near_trim[family_index] +=
				trace.physical_event_near_trim;
			    family_unresolved[family_index] +=
				trace.physical_event_unresolved;
			    family_seam_attempts[family_index] +=
				trace.physical_event_seam_attempts;
			    family_seam_candidates[family_index] +=
				trace.physical_event_seam_root_candidates;
			    family_seam_failures[family_index] +=
				trace.physical_event_seam_failures;
			    family_seam_ownership_failures[family_index] +=
				trace.physical_event_seam_ownership_failures;
			    family_seam_witness_failures[family_index] +=
				trace.physical_event_seam_witness_failures;
			    family_seam_box_failures[family_index] +=
				trace.physical_event_seam_box_failures;
			    family_seam_root_failures[family_index] +=
				trace.physical_event_seam_root_coverage_failures;
			    family_edge_attempts[family_index] +=
				trace.physical_event_edge_attempts;
			    family_edge_candidates[family_index] +=
				trace.physical_event_edge_candidates;
			    family_edge_failures[family_index] +=
				trace.physical_event_edge_failures;
			    family_edge_lift_witnesses[family_index] +=
				trace.physical_event_edge_lift_witnesses;
			    family_edge_tolerance_transitions[family_index] +=
				trace.physical_event_edge_tolerance_transitions;
			    if (trace.physical_event_edge_failures &&
				trace.physical_event_edge_candidate_failure_stage > 0 &&
				trace.physical_event_edge_candidate_failure_stage < 9)
				family_edge_candidate_failure_stage[family_index]
				    [trace.physical_event_edge_candidate_failure_stage]++;
			    family_joint_attempts[family_index] +=
				trace.physical_event_edge_joint_attempts;
			    family_joint_certified[family_index] +=
				trace.physical_event_edge_joint_components;
			    family_joint_boxes[family_index] +=
				trace.physical_event_edge_joint_boxes;
			    family_joint_roots[family_index] +=
				trace.physical_event_edge_joint_roots;
			    family_joint_complement_visited[family_index] +=
				trace.physical_event_edge_joint_complement_visited;
			    family_joint_complement_high_water[family_index] = std::max(
				family_joint_complement_high_water[family_index],
				trace.physical_event_edge_joint_complement_high_water);
			    if (trace.physical_event_edge_joint_attempts >
				    trace.physical_event_edge_joint_components &&
				trace.physical_event_edge_joint_failure_stage > 0 &&
				trace.physical_event_edge_joint_failure_stage < 16)
				family_joint_failure_stage[family_index]
				    [trace.physical_event_edge_joint_failure_stage]++;
			    family_component_attempts[family_index] +=
				trace.physical_event_regular_component_attempts;
			    family_component_certified[family_index] +=
				trace.physical_event_regular_component_certified;
			    family_component_boxes[family_index] +=
				trace.physical_event_regular_component_boxes;
			    family_component_roots[family_index] +=
				trace.physical_event_regular_component_roots;
			    if (trace.physical_event_regular_component_attempts >
				    trace.physical_event_regular_component_certified &&
				    trace.physical_event_regular_component_failure_stage > 0 &&
				    trace.physical_event_regular_component_failure_stage < 7)
				family_component_failure_stage[family_index]
				    [trace.physical_event_regular_component_failure_stage]++;
			    if (!selected && trace.prepared_production_fallback >= 0 &&
				    trace.prepared_production_fallback <
					RT_BREP_PREPARED_FALLBACK_COUNT)
				family_fallback_reason[family_index]
				    [trace.prepared_production_fallback]++;
			}
			    rays++;
			}
		    }
		}
		free_solid(stp);
	    }
	}
    }

    free_solid(implicit_stp);
    delete pristine;
    if (rays != 120 || contacts != 24 || family_rays[0] != 40 ||
	    family_rays[1] != 80 ||
	    family_prepared[0] + family_fallback[0] != family_rays[0] ||
	    family_prepared[1] + family_fallback[1] != family_rays[1]) {
	std::printf("FAIL: Cobb isolated defect coverage rays=%zu/120 "
	    "families=%zu/40,%zu/80 contacts=%zu/24\n", rays,
	    family_rays[0], family_rays[1], contacts);
	failures++;
    }
    if (family_prepared[0] < 32 || family_prepared[1] < 64 ||
	    prepared < 96 || family_edge_lift_witnesses[1] < 64 ||
	    family_edge_tolerance_transitions[0] < 2 ||
	    family_joint_certified[0] + family_joint_certified[1] < 60 ||
	    family_component_certified[0] +
		family_component_certified[1] < 36) {
	std::printf("FAIL: Cobb isolated defect prepared ratchet "
	    "families=%zu/32,%zu/64 total=%zu/96 lift=%zu/64 "
	    "tolerance-transition=%zu/2 joint=%zu/60 components=%zu/36\n",
	    family_prepared[0],
	    family_prepared[1],
	    prepared, family_edge_lift_witnesses[1],
	    family_edge_tolerance_transitions[0],
	    family_joint_certified[0] + family_joint_certified[1],
	    family_component_certified[0] + family_component_certified[1]);
	failures++;
    }
    if (!failures) {
	for (size_t family_index = 0;
		family_index < sizeof(families) / sizeof(families[0]);
		++family_index) {
	    std::printf("Cobb isolated %s: rays=%zu selected/fallback=%zu/%zu "
		"seam/edge-certified=%zu/%zu fallback-reasons=",
		families[family_index].name, family_rays[family_index],
		family_prepared[family_index], family_fallback[family_index],
		family_seam_certified[family_index],
		family_edge_certified[family_index]);
	    bool first_reason = true;
	    for (int reason = 0; reason < RT_BREP_PREPARED_FALLBACK_COUNT;
		    ++reason) {
		if (family_fallback_reason[family_index][reason]) {
		    std::printf("%s%d:%zu", first_reason ? "" : ",",
			reason, family_fallback_reason[family_index][reason]);
		    first_reason = false;
		}
	    }
	    std::printf("\n");
	    std::printf("  events regular/near/unresolved=%zu/%zu/%zu "
		"seam attempt/candidate/failure=%zu/%zu/%zu "
		"failure-kind=%zu/%zu/%zu/%zu "
		"edge attempt/candidate/failure/lift/tolerance-transition="
		"%zu/%zu/%zu/%zu/%zu\n",
		family_regular[family_index], family_near_trim[family_index],
		family_unresolved[family_index],
		family_seam_attempts[family_index],
		family_seam_candidates[family_index],
		family_seam_failures[family_index],
		family_seam_ownership_failures[family_index],
		family_seam_witness_failures[family_index],
		family_seam_box_failures[family_index],
		family_seam_root_failures[family_index],
		family_edge_attempts[family_index],
		family_edge_candidates[family_index],
		family_edge_failures[family_index],
		family_edge_lift_witnesses[family_index],
		family_edge_tolerance_transitions[family_index]);
	    std::printf("  edge candidate failure-stage=");
	    bool first_edge_stage = true;
	    for (int stage = 0; stage < 9; ++stage) {
		if (!family_edge_candidate_failure_stage[family_index][stage])
		    continue;
		std::printf("%s%d:%zu", first_edge_stage ? "" : ",", stage,
		    family_edge_candidate_failure_stage[family_index][stage]);
		first_edge_stage = false;
	    }
	    std::printf("\n");
	    std::printf("  joint attempt/certified/boxes/roots="
		"%zu/%zu/%zu/%zu complement visited/high-water=%zu/%zu "
		"failure-stage=", family_joint_attempts[family_index],
		family_joint_certified[family_index],
		family_joint_boxes[family_index], family_joint_roots[family_index],
		family_joint_complement_visited[family_index],
		family_joint_complement_high_water[family_index]);
	    bool first_joint_stage = true;
	    for (int stage = 0; stage < 16; ++stage) {
		if (!family_joint_failure_stage[family_index][stage])
		    continue;
		std::printf("%s%d:%zu", first_joint_stage ? "" : ",", stage,
		    family_joint_failure_stage[family_index][stage]);
		first_joint_stage = false;
	    }
	    std::printf("\n");
	    std::printf("  components attempt/certified/boxes/roots="
		"%zu/%zu/%zu/%zu failure-stage=",
		family_component_attempts[family_index],
		family_component_certified[family_index],
		family_component_boxes[family_index],
		family_component_roots[family_index]);
	    bool first_stage = true;
	    for (int stage = 0; stage < 7; ++stage) {
		if (!family_component_failure_stage[family_index][stage])
		    continue;
		std::printf("%s%d:%zu", first_stage ? "" : ",", stage,
		    family_component_failure_stage[family_index][stage]);
		first_stage = false;
	    }
	    std::printf("\n");
	}
	std::printf("Cobb isolated trim/edge defects: PASS rays=%zu "
	    "selected/fallback=%zu/%zu contacts=%zu "
	    "max-calibration=%.3g max-endpoint=%.3g\n", rays, prepared,
	    fallback, contacts, maximum_gap_calibration,
	    maximum_endpoint_error);
    }
    failures += check_cobb_isolated_trim_transition_similarity(tol);
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
		    !observation->correspondence_screened ||
		    !observation->correspondence_supported ||
		    !observation->correspondence_cells ||
		    observation->correspondence_exhausted ||
		    !observation->candidate_spans ||
		    !observation->sector_valid ||
		    observation->closest_state != expected_state ||
		    fabs(observation->distance - expected_distance) >
		    1.0e-10 || fabs(observation->ray_edge_dot) > 1.0e-10) {
		std::printf("FAIL: %s edge sector state=%d reverse=%d "
		    "observed=%d valid=%d distance=%.17g spans=%zu "
		    "ray-edge=%.17g correspondence=%d/%d cells=%zu "
		    "depth=%zu exhausted=%d\n", label, expected_state, reverse,
		    observation ? observation->closest_state : -99,
		    observation ? observation->sector_valid : 0,
		    observation ? observation->distance : INFINITY,
		    observation ? observation->candidate_spans : 0,
		    observation ? observation->ray_edge_dot : INFINITY,
		    observation ? observation->correspondence_screened : -1,
		    observation ? observation->correspondence_supported : -1,
		    observation ? observation->correspondence_cells : 0,
		    observation ? observation->correspondence_depth : 0,
		    observation ? observation->correspondence_exhausted : -1);
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
check_brep_same_surface_edge_events(ON_Brep *brep, int target_edge_index,
    const ON_3dVector &base_inside, const struct bn_tol *tol,
    struct rt_i *rtip, struct resource *resource)
{
    if (!brep || target_edge_index < 0 ||
	    target_edge_index >= brep->m_E.Count()) {
	delete brep;
	return 1;
    }
    for (int edge_index = 0; edge_index < brep->m_E.Count(); ++edge_index)
	brep->m_E[edge_index].m_tolerance = tol->dist;
    const ON_3dPoint edge_point = brep->m_E[target_edge_index].PointAt(
	brep->m_E[target_edge_index].Domain().Mid());
    ON_3dVector inside = base_inside;
    ON_3dVector edge_tangent = brep->m_E[target_edge_index].TangentAt(
	brep->m_E[target_edge_index].Domain().Mid());
    if (!inside.Unitize() || !edge_tangent.Unitize()) {
	delete brep;
	return 1;
    }
    ON_3dVector radial = edge_point - ON_3dPoint::Origin;
    ON_3dVector conormal = ON_CrossProduct(edge_tangent, radial);
    if (!conormal.Unitize()) {
	delete brep;
	return 1;
    }
    inside += 0.25 * conormal + 0.17 * edge_tangent;
    if (!inside.Unitize()) {
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
	delete brep_internal.brep;
	return 1;
    }

    int failures = 0;
    double maximum_endpoint_error = 0.0;
    size_t maximum_owned_roots = 0;
    size_t maximum_owned_boxes = 0;
    for (int reverse_ray = 0; reverse_ray <= 1; ++reverse_ray) {
	const ON_3dVector direction = reverse_ray ? -inside : inside;
	const ON_3dPoint origin = edge_point - 12.0 * direction;
	sampled_ray ray;
	VSET(ray.origin, origin.x, origin.y, origin.z);
	VSET(ray.direction, direction.x, direction.y, direction.z);
	struct rt_brep_shot_trace trace;
	const int trace_hits = shoot_brep_trace(stp, rtip, resource, ray,
	    trace);
	const ray_result production_result = shoot_solid(stp, rtip, resource,
	    ray.origin, ray.direction);
	const double radius = edge_point.DistanceTo(ON_3dPoint::Origin);
	const double center_parameter = (ON_3dPoint::Origin - origin) *
	    direction;
	const ON_3dPoint closest = origin + center_parameter * direction;
	const double half_chord = sqrt(std::max(0.0, radius * radius -
	    closest.DistanceTo(ON_3dPoint::Origin) *
	    closest.DistanceTo(ON_3dPoint::Origin)));
	const double expected_in = center_parameter - half_chord;
	const double expected_out = center_parameter + half_chord;
	const double endpoint_error = production_result.segments == 1 ?
	    std::max(fabs(production_result.in_dist - expected_in),
		fabs(production_result.out_dist - expected_out)) : DBL_MAX;
	maximum_endpoint_error = std::max(maximum_endpoint_error,
	    endpoint_error);
	size_t edge_events = 0;
	size_t target_events = 0;
	size_t regular_events = 0;
	size_t owned_roots = 0;
	size_t owned_boxes = 0;
	bool invalid = false;
	for (size_t event_index = 0;
		event_index < trace.stored_physical_events; ++event_index) {
	    const struct rt_brep_trace_physical_event &event =
		trace.physical_events[event_index];
	    if (event.certificate == RT_BREP_TRACE_EVENT_MANIFOLD_EDGE) {
		edge_events++;
		if (event.edge_index == target_edge_index)
		    target_events++;
		const struct rt_brep_trace_edge *edge =
		    brep_trace_edge(trace, event.edge_index);
		size_t roots = 0;
		size_t boxes = 0;
		if (!edge || !brep_trace_edge_cluster_owned(trace, *edge,
			&event, roots, boxes))
		    invalid = true;
		owned_roots += roots;
		owned_boxes += boxes;
	    } else if (event.certificate ==
		    RT_BREP_TRACE_EVENT_REGULAR_INTERIOR) {
		regular_events++;
	    } else {
		invalid = true;
	    }
	}
	maximum_owned_roots = std::max(maximum_owned_roots, owned_roots);
	maximum_owned_boxes = std::max(maximum_owned_boxes, owned_boxes);
	if (!brep_trace_fixed_workspaces_match(trace) ||
		trace.physical_event_edge_attempts != 1 ||
		!trace.physical_event_edge ||
		trace.physical_event_edge != edge_events ||
		trace.physical_event_edge != 1 ||
		trace.physical_event_edge_contacts ||
		trace.physical_event_edge_certified != 1 ||
		trace.physical_event_edge_failures || target_events != 1 ||
		owned_roots != trace.physical_event_edge_owned_roots ||
		owned_boxes != trace.physical_event_edge_owned_boxes ||
		edge_events != 1 || regular_events != 1 || invalid ||
		trace.physical_event_complete != 1 ||
		trace.physical_event_material_segments != 1 ||
		trace.prepared_production_fallback !=
		    RT_BREP_PREPARED_FALLBACK_NONE ||
		trace.prepared_production_selected != 1 || trace_hits != 2 ||
		production_result.segments != 1 || endpoint_error > 1.0e-7) {
	    std::printf("FAIL: same-surface edge reverse=%d "
		"attempt/candidate/event/contact/cert/fail="
		"%zu/%zu/%zu/%zu/%zu/%zu target=%zu regular=%zu "
		"owned=%zu/%zu events=%zu complete=%zu selected=%zu "
		"fallback=%d hits=%d segments=%d error=%.17g\n",
		reverse_ray, trace.physical_event_edge_attempts,
		trace.physical_event_edge_candidates,
		trace.physical_event_edge,
		trace.physical_event_edge_contacts,
		trace.physical_event_edge_certified,
		trace.physical_event_edge_failures, target_events,
		regular_events, trace.physical_event_edge_owned_boxes,
		trace.physical_event_edge_owned_roots,
		trace.stored_physical_events,
		trace.physical_event_complete,
		trace.prepared_production_selected,
		trace.prepared_production_fallback, trace_hits,
		production_result.segments, endpoint_error);
	    const struct rt_brep_trace_edge *target_edge =
		brep_trace_edge(trace, target_edge_index);
	    if (target_edge)
		std::printf("  SSE e=%d f=%d/%d d=%.17g t=%.17g "
		    "sector=%d line=%d state=%d/%d dir=%d\n",
		    target_edge->edge_index, target_edge->face_index[0],
		    target_edge->face_index[1], target_edge->distance,
		    target_edge->ray_dist, target_edge->sector_valid,
		    target_edge->line_state_valid,
		    target_edge->line_before_state,
		    target_edge->line_after_state,
		    target_edge->line_transition_direction);
	    for (size_t box_index = 0;
		    box_index < trace.stored_surface_boxes; ++box_index) {
		const struct rt_brep_trace_surface_box &box =
		    trace.surface_boxes[box_index];
		std::printf("  SSB %zu f=%d s=%d t=[%.17g,%.17g] "
		    "d=%d sign=%d\n", box_index, box.face_index,
		    box.span_index, box.t_min, box.t_max,
		    box.disposition, box.determinant_sign);
	    }
	    for (size_t root_index = 0;
		    root_index < trace.stored_local_roots; ++root_index) {
		const struct rt_brep_trace_local_root &root =
		    trace.local_roots[root_index];
		std::printf("  SSR %zu f=%d s=%d t=%.17g trim=%d "
		    "class=%d dir=%d nd=%.17g\n", root_index,
		    root.face_index, root.span_index, root.dist,
		    root.trim_status, root.hit_class, root.direction,
		    root.normal_dot);
	    }
	    failures++;
	}
    }
    if (!failures)
	std::printf("Same-surface manifold edge: PASS roots=%zu boxes=%zu "
	    "oracle-error=%.3g\n", maximum_owned_roots,
	    maximum_owned_boxes, maximum_endpoint_error);
    free_solid(stp);
    return failures;
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
    int failures = check_brep_same_surface_edge_events(new ON_Brep(*brep),
	target_edge_index, inside, tol, rtip, resource);
    failures += check_brep_edge_sector_fixture("same-surface seam", brep,
	target_edge_index, inside, tol, rtip, resource);
    return failures;
}


static int
check_brep_vertex_fan_transition(const struct bn_tol *tol, struct rt_i *rtip,
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
	size_t vertex_events = 0;
	size_t regular_events = 0;
	bool invalid_vertex_event = false;
	const int expected_vertex_direction =
	    cases[case_index].direction * diagonal < 0.0 ?
	    RT_BREP_TRACE_ENTERING : RT_BREP_TRACE_LEAVING;
	for (size_t event_index = 0;
		event_index < trace.stored_physical_events; ++event_index) {
	    const struct rt_brep_trace_physical_event &event =
		trace.physical_events[event_index];
	    if (event.certificate == RT_BREP_TRACE_EVENT_VERTEX_FAN) {
		vertex_events++;
		if (event.source_kind !=
			RT_BREP_TRACE_EVENT_SOURCE_VERTEX_FAN ||
			event.vertex_index != target_vertex ||
			event.edge_index != -1 ||
			event.source_box_count != 3 ||
			event.hit_class != 4 ||
			event.direction != expected_vertex_direction ||
			!brep_trace_vertex_event_owned(trace, *brep, event))
		    invalid_vertex_event = true;
	    } else if (event.certificate ==
		    RT_BREP_TRACE_EVENT_REGULAR_INTERIOR) {
		regular_events++;
		if (event.vertex_index != -1 || event.edge_index != -1 ||
			event.source_box_count != 1)
		    invalid_vertex_event = true;
	    } else {
		invalid_vertex_event = true;
	    }
	}
	size_t contact_edges = 0;
	bool invalid_edge = false;
	for (size_t incident = 0; incident < 3; ++incident) {
	    const struct rt_brep_trace_edge *edge = brep_trace_edge(trace,
		incident_edges[incident]);
	    if (!edge || !edge->correspondence_screened ||
		    !edge->correspondence_supported ||
		    !edge->correspondence_cells ||
		    edge->correspondence_exhausted ||
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
		trace.prepared_vertex_records != 8 ||
		trace.supported_vertex_records != 8 ||
		trace.physical_event_vertex_attempts != 1 ||
		trace.physical_event_vertex_candidates != 1 ||
		trace.physical_event_vertex != 1 ||
		trace.physical_event_vertex_certified != 1 ||
		trace.physical_event_vertex_failures ||
		trace.physical_event_vertex_winding_ambiguous ||
		trace.physical_event_vertex_owned_boxes != 3 ||
		trace.physical_event_vertex_owned_roots != 3 ||
		trace.physical_event_regular != 1 ||
		trace.physical_event_complete != 1 ||
		trace.physical_event_material_segments != 1 ||
		trace.stored_physical_events != 2 || vertex_events != 1 ||
		regular_events != 1 || invalid_vertex_event ||
		trace.prepared_production_fallback !=
		RT_BREP_PREPARED_FALLBACK_NONE ||
		trace.prepared_production_eligible != 1 ||
		trace.prepared_production_selected != 1 ||
		trace.prepared_production_hits != 2 ||
		trace_hits != 2 || trace.final_segments != 1) {
	    std::printf("FAIL: convex vertex-fan %s contacts=%zu/3 "
		"closure=%zu continuation=%zu shadow=%zu "
		"vertices=%zu/%zu attempt/candidate/event/cert/fail="
		"%zu/%zu/%zu/%zu/%zu owned=%zu/%zu events=%zu/%zu/%zu "
		"complete=%zu selected=%zu fallback=%d hits=%d/2 final=%zu/1\n",
		cases[case_index].name,
		contact_edges, trace.closure_candidates,
		trace.continuation_attempts, trace.closure_shadow_segments,
		trace.prepared_vertex_records, trace.supported_vertex_records,
		trace.physical_event_vertex_attempts,
		trace.physical_event_vertex_candidates,
		trace.physical_event_vertex,
		trace.physical_event_vertex_certified,
		trace.physical_event_vertex_failures,
		trace.physical_event_vertex_owned_boxes,
		trace.physical_event_vertex_owned_roots,
		trace.stored_physical_events, vertex_events, regular_events,
		trace.physical_event_complete,
		trace.prepared_production_selected,
		trace.prepared_production_fallback, trace_hits,
		trace.final_segments);
	    failures++;
	}
    }

    ON_3dVector fan_direction(1.0, -1.0, 0.0);
    fan_direction.Unitize();
    for (int reverse = 0; reverse <= 1; ++reverse) {
	const ON_3dVector direction = reverse ? -fan_direction :
	    fan_direction;
	const ON_3dPoint ray_origin = target_point - 20.0 * direction;
	sampled_ray ray;
	VSET(ray.origin, ray_origin.x, ray_origin.y, ray_origin.z);
	VSET(ray.direction, direction.x, direction.y, direction.z);
	struct rt_brep_shot_trace trace;
	(void)shoot_brep_trace(stp, rtip, resource, ray, trace);
	if (!brep_trace_fixed_workspaces_match(trace) ||
		trace.physical_event_vertex_attempts != 1 ||
		trace.physical_event_vertex_candidates ||
		trace.physical_event_vertex ||
		trace.physical_event_vertex_certified ||
		trace.physical_event_vertex_winding_ambiguous != 1 ||
		trace.prepared_production_selected) {
	    std::printf("FAIL: convex vertex-fan tangent reverse=%d "
		"attempt/candidate/event/cert/ambiguous=%zu/%zu/%zu/%zu/%zu "
		"selected=%zu fallback=%d\n", reverse,
		trace.physical_event_vertex_attempts,
		trace.physical_event_vertex_candidates,
		trace.physical_event_vertex,
		trace.physical_event_vertex_certified,
		trace.physical_event_vertex_winding_ambiguous,
		trace.prepared_production_selected,
		trace.prepared_production_fallback);
	    failures++;
	}
    }
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
	    if (edge->correspondence_screened &&
		    edge->correspondence_supported &&
		    edge->correspondence_cells &&
		    !edge->correspondence_exhausted &&
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
		trace.closure_shadow_segments ||
		trace.physical_event_vertex ||
		trace.physical_event_vertex_certified ||
		trace.prepared_production_selected) {
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
	std::printf("Convex vertex-fan transition: PASS contact-edges=%zu "
	    "near-inside=%zu closure-candidates=%zu\n", maximum_contact_edges,
	    maximum_near_inside_edges, maximum_near_closure_candidates);
    return failures;
}


static int
check_brep_manifold_edge_similarity_case(const char *label, double scale,
    const ON_Xform &xform, struct soltab *stp,
    struct rt_i *rtip, struct resource *resource)
{
    struct edge_case {
	ON_3dVector direction;
	size_t transitions;
	size_t contacts;
	size_t regular;
	ON_3dPoint endpoint[2];
    } cases[] = {
	{ON_3dVector(1.0, 1.0, 0.0), 2, 0, 0,
	    {ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(2.0, 2.0, 0.0)}},
	{ON_3dVector(1.0, -1.0, 0.0), 2, 1, 0,
	    {ON_3dPoint(0.0, 4.0, 0.0), ON_3dPoint(4.0, 0.0, 0.0)}},
	{ON_3dVector(0.25, 0.5, 1.0), 1, 0, 1,
	    {ON_3dPoint(1.5, 1.0, -2.0), ON_3dPoint(2.0, 2.0, 0.0)}}
    };
    int failures = 0;
    const ON_3dPoint edge_point = cobb_transform_point(xform,
	ON_3dPoint(2.0, 2.0, 0.0));
    for (size_t case_index = 0;
	    case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
	ON_3dVector transformed_direction = cobb_transform_vector(xform,
	    cases[case_index].direction);
	if (!transformed_direction.Unitize())
	    return failures + 1;
	const ON_3dPoint endpoint[2] = {
	    cobb_transform_point(xform, cases[case_index].endpoint[0]),
	    cobb_transform_point(xform, cases[case_index].endpoint[1])
	};
	for (int reverse_ray = 0; reverse_ray <= 1; ++reverse_ray) {
	    const ON_3dVector direction = reverse_ray ?
		-transformed_direction : transformed_direction;
	    const ON_3dPoint origin = edge_point - 10.0 * scale * direction;
	    sampled_ray ray;
	    VSET(ray.origin, origin.x, origin.y, origin.z);
	    VSET(ray.direction, direction.x, direction.y, direction.z);
	    struct rt_brep_shot_trace trace;
	    const int trace_hits = shoot_brep_trace(stp, rtip, resource,
		ray, trace);
	    const ray_result production_result = shoot_solid(stp, rtip,
		resource, ray.origin, ray.direction);
	    const double p0 = (endpoint[0] - origin) * direction;
	    const double p1 = (endpoint[1] - origin) * direction;
	    const double expected_in = std::min(p0, p1);
	    const double expected_out = std::max(p0, p1);
	    const double endpoint_tolerance = std::max(1.0e-8 * scale,
		4096.0 * DBL_EPSILON * std::max(1.0,
		std::max(fabs(expected_in), fabs(expected_out))));
	    const double endpoint_error = production_result.segments == 1 ?
		std::max(fabs(production_result.in_dist - expected_in),
		    fabs(production_result.out_dist - expected_out)) : DBL_MAX;
	    size_t edge_events = 0;
	    size_t regular_events = 0;
	    size_t classified_contacts = 0;
	    size_t reconstructed_contacts = 0;
	    size_t reconstructed_roots = 0;
	    size_t reconstructed_boxes = 0;
	    bool invalid = false;
	    for (size_t event_index = 0;
		    event_index < trace.stored_physical_events; ++event_index) {
		const struct rt_brep_trace_physical_event &event =
		    trace.physical_events[event_index];
		if (event.certificate ==
			RT_BREP_TRACE_EVENT_MANIFOLD_EDGE) {
		    edge_events++;
		    const struct rt_brep_trace_edge *edge =
			brep_trace_edge(trace, event.edge_index);
		    size_t roots = 0;
		    size_t boxes = 0;
		    if (!edge || !brep_trace_edge_cluster_owned(trace, *edge,
			    &event, roots, boxes))
			invalid = true;
		    reconstructed_roots += roots;
		    reconstructed_boxes += boxes;
		} else if (event.certificate ==
			RT_BREP_TRACE_EVENT_REGULAR_INTERIOR) {
		    regular_events++;
		} else {
		    invalid = true;
		}
	    }
	    for (size_t edge_index = 0;
		    edge_index < trace.stored_edges; ++edge_index) {
		const struct rt_brep_trace_edge &edge = trace.edges[edge_index];
		const double exact_tolerance = std::max(1.0e-9 * scale,
		    4096.0 * DBL_EPSILON * std::max(1.0,
		    fabs(edge.ray_dist)));
		if (edge.distance > exact_tolerance || !edge.line_state_valid ||
			edge.line_before_state != edge.line_after_state)
		    continue;
		classified_contacts++;
		bool contact_evidence = false;
		const double evidence_tolerance = std::max(exact_tolerance,
		    std::max(0.0, (double)edge.edge_tolerance));
		for (size_t root_index = 0;
			root_index < trace.stored_local_roots; ++root_index) {
		    const struct rt_brep_trace_local_root &root =
			trace.local_roots[root_index];
		    if ((root.face_index == edge.face_index[0] ||
			 root.face_index == edge.face_index[1]) &&
			fabs(root.dist - edge.ray_dist) <= evidence_tolerance)
			contact_evidence = true;
		}
		if (!contact_evidence)
		    continue;
		size_t roots = 0;
		size_t boxes = 0;
		if (!brep_trace_edge_cluster_owned(trace, edge, NULL, roots,
			boxes))
		    invalid = true;
		reconstructed_contacts++;
		reconstructed_roots += roots;
		reconstructed_boxes += boxes;
	    }
	    if (!brep_trace_fixed_workspaces_match(trace) ||
		    trace.physical_event_edge_attempts != 1 ||
		    trace.physical_event_edge_candidates !=
			trace.physical_event_edge +
			trace.physical_event_edge_contacts ||
		    trace.physical_event_edge != cases[case_index].transitions ||
		    trace.physical_event_edge_contacts !=
			reconstructed_contacts ||
		    classified_contacts != cases[case_index].contacts ||
		    trace.physical_event_edge_contacts >
			cases[case_index].contacts ||
		    trace.physical_event_edge_certified != 1 ||
		    trace.physical_event_edge_failures ||
		    trace.physical_event_regular != cases[case_index].regular ||
		    edge_events != cases[case_index].transitions ||
		    regular_events != cases[case_index].regular ||
		    reconstructed_roots !=
			trace.physical_event_edge_owned_roots ||
		    reconstructed_boxes !=
			trace.physical_event_edge_owned_boxes || invalid ||
		    trace.physical_event_complete != 1 ||
		    trace.physical_event_material_segments != 1 ||
		    trace.stored_physical_events != 2 ||
		    trace.prepared_production_fallback !=
			RT_BREP_PREPARED_FALLBACK_NONE ||
		    trace.prepared_production_selected != 1 ||
		    trace_hits != 2 || production_result.segments != 1 ||
		    endpoint_error > endpoint_tolerance) {
		std::printf("FAIL: manifold edge similarity %s case=%zu "
		    "reverse=%d candidate/event/contact=%zu/%zu/%zu "
		    "cert/fail=%zu/%zu owned=%zu/%zu rebuilt=%zu/%zu/%zu "
		    "regular=%zu events=%zu complete=%zu selected=%zu "
		    "fallback=%d hits=%d segments=%d error=%.17g/%.17g\n",
		    label, case_index, reverse_ray,
		    trace.physical_event_edge_candidates,
		    trace.physical_event_edge,
		    trace.physical_event_edge_contacts,
		    trace.physical_event_edge_certified,
		    trace.physical_event_edge_failures,
		    trace.physical_event_edge_owned_boxes,
		    trace.physical_event_edge_owned_roots,
		    reconstructed_contacts, reconstructed_boxes,
		    reconstructed_roots, trace.physical_event_regular,
		    trace.stored_physical_events,
		    trace.physical_event_complete,
		    trace.prepared_production_selected,
		    trace.prepared_production_fallback, trace_hits,
		    production_result.segments, endpoint_error,
		    endpoint_tolerance);
		for (size_t edge_index = 0;
			edge_index < trace.stored_edges; ++edge_index) {
		    const struct rt_brep_trace_edge &edge =
			trace.edges[edge_index];
		    if (edge.distance > 1.0e-3 * scale)
			continue;
		    std::printf("  SME %zu e=%d f=%d/%d d=%.17g t=%.17g "
			"sector=%d line=%d state=%d/%d dir=%d tol=%.17g\n",
			edge_index, edge.edge_index, edge.face_index[0],
			edge.face_index[1], edge.distance, edge.ray_dist,
			edge.sector_valid, edge.line_state_valid,
			edge.line_before_state, edge.line_after_state,
			edge.line_transition_direction, edge.edge_tolerance);
		}
		for (size_t box_index = 0;
			box_index < trace.stored_surface_boxes; ++box_index) {
		    const struct rt_brep_trace_surface_box &box =
			trace.surface_boxes[box_index];
		    std::printf("  SMB %zu f=%d s=%d t=[%.17g,%.17g] "
			"d=%d sign=%d\n", box_index, box.face_index,
			box.span_index, box.t_min, box.t_max,
			box.disposition, box.determinant_sign);
		}
		for (size_t root_index = 0;
			root_index < trace.stored_local_roots; ++root_index) {
		    const struct rt_brep_trace_local_root &root =
			trace.local_roots[root_index];
		    std::printf("  SMR %zu f=%d s=%d t=%.17g trim=%d "
			"class=%d dir=%d nd=%.17g\n", root_index,
			root.face_index, root.span_index, root.dist,
			root.trim_status, root.hit_class, root.direction,
			root.normal_dot);
		}
		failures++;
	    }
	}
    }
    return failures;
}


static int
check_brep_vertex_fan_similarity(const ON_Brep &base, int target_vertex,
    const ON_3dPoint &target_point, const ON_3dVector &base_inward,
    const struct bn_tol *tol)
{
    struct similarity_case {
	const char *name;
	double scale;
	ON_3dVector translation;
	ON_3dVector axis;
	double angle;
    } cases[] = {
	{"translated", 1.0, ON_3dVector(-31.25, 47.5, 103.75),
	    ON_3dVector(1.0, 0.0, 0.0), 0.0},
	{"rotated-translated", 1.0, ON_3dVector(13.0, -17.0, 29.0),
	    ON_3dVector(1.0, -2.0, 0.5), 0.731},
	{"small-similarity", 0.01, ON_3dVector(1.25, -2.5, 5.0),
	    ON_3dVector(-0.3, 1.0, 0.7), -1.113},
	{"large-similarity", 1.0e4,
	    ON_3dVector(1.0e6, -2.0e6, 3.0e6),
	    ON_3dVector(2.0, 0.25, -1.0), 2.017}
    };
    int failures = 0;
    for (size_t case_index = 0;
	    case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
	const similarity_case &test = cases[case_index];
	const ON_Xform xform = cobb_axis_angle_similarity_transform(test.scale,
	    test.translation, test.axis, test.angle);
	ON_Brep *variant = new ON_Brep(base);
	if (!variant->Transform(xform)) {
	    std::printf("FAIL: vertex-fan %s transform\n", test.name);
	    delete variant;
	    failures++;
	    continue;
	}
	for (int vertex_index = 0; vertex_index < variant->m_V.Count();
		++vertex_index)
	    if (ON_IsValid(base.m_V[vertex_index].m_tolerance) &&
		    base.m_V[vertex_index].m_tolerance >= 0.0)
		variant->m_V[vertex_index].m_tolerance =
		    test.scale * base.m_V[vertex_index].m_tolerance;
	for (int edge_index = 0; edge_index < variant->m_E.Count();
		++edge_index)
	    if (ON_IsValid(base.m_E[edge_index].m_tolerance) &&
		    base.m_E[edge_index].m_tolerance >= 0.0)
		variant->m_E[edge_index].m_tolerance =
		    test.scale * base.m_E[edge_index].m_tolerance;

	struct bn_tol case_tol = *tol;
	case_tol.dist = test.scale * tol->dist;
	case_tol.dist_sq = case_tol.dist * case_tol.dist;
	struct rt_i *case_rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
	if (!case_rtip) {
	    std::printf("FAIL: vertex-fan %s rt_i construction\n",
		test.name);
	    delete variant;
	    failures++;
	    continue;
	}
	case_rtip->rti_tol = case_tol;
	struct resource case_resource = {};
	rt_init_resource(&case_resource, 0, case_rtip);
	struct rt_brep_internal variant_internal = {};
	variant_internal.magic = RT_BREP_INTERNAL_MAGIC;
	variant_internal.brep = variant;
	struct rt_db_internal variant_intern;
	RT_DB_INTERNAL_INIT(&variant_intern);
	variant_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	variant_intern.idb_type = ID_BREP;
	variant_intern.idb_meth = &OBJ[ID_BREP];
	variant_intern.idb_ptr = &variant_internal;
	struct soltab *stp = prep_solid(case_rtip, &variant_intern, ID_BREP);
	if (!stp) {
	    std::printf("FAIL: vertex-fan %s BREP prep\n", test.name);
	    delete variant_internal.brep;
	    rt_clean_resource_basic(case_rtip, &case_resource);
	    BU_PTBL_SET(&case_rtip->rti_resources, 0, NULL);
	    rt_i_destroy(case_rtip);
	    failures++;
	    continue;
	}

	const ON_3dPoint transformed_target =
	    cobb_transform_point(xform, target_point);
	ON_3dVector transformed_inward =
	    cobb_transform_vector(xform, base_inward);
	if (!transformed_inward.Unitize()) {
	    std::printf("FAIL: vertex-fan %s transformed direction\n",
		test.name);
	    failures++;
	} else {
	    for (int reverse_ray = 0; reverse_ray <= 1; ++reverse_ray) {
		const ON_3dVector direction = reverse_ray ?
		    -transformed_inward : transformed_inward;
		const ON_3dPoint ray_origin = transformed_target -
		    20.0 * test.scale * direction;
		sampled_ray ray;
		VSET(ray.origin, ray_origin.x, ray_origin.y, ray_origin.z);
		VSET(ray.direction, direction.x, direction.y, direction.z);
		struct rt_brep_shot_trace trace;
		const int trace_hits = shoot_brep_trace(stp, case_rtip,
		    &case_resource, ray, trace);
		size_t vertex_events = 0;
		size_t target_events = 0;
		bool invalid_event = false;
		const int expected_target_direction = reverse_ray ?
		    RT_BREP_TRACE_LEAVING : RT_BREP_TRACE_ENTERING;
		for (size_t event_index = 0;
			event_index < trace.stored_physical_events;
			++event_index) {
		    const struct rt_brep_trace_physical_event &event =
			trace.physical_events[event_index];
		    if (event.certificate != RT_BREP_TRACE_EVENT_VERTEX_FAN)
			continue;
		    vertex_events++;
		    if (event.vertex_index == target_vertex) {
			target_events++;
			if (event.direction != expected_target_direction)
			    invalid_event = true;
		    }
		    if (!brep_trace_vertex_event_owned(trace, *variant,
			    event))
			invalid_event = true;
		}
		if (!brep_trace_fixed_workspaces_match(trace) ||
			trace.prepared_vertex_records != 12 ||
			trace.supported_vertex_records != 12 ||
			trace.physical_event_vertex_attempts != 1 ||
			trace.physical_event_vertex_candidates != 2 ||
			trace.physical_event_vertex != 2 ||
			trace.physical_event_vertex_certified != 1 ||
			trace.physical_event_vertex_failures ||
			trace.physical_event_vertex_winding_ambiguous ||
			trace.physical_event_vertex_owned_roots < 2 ||
			vertex_events != 2 || target_events != 1 ||
			invalid_event || trace.physical_event_complete != 1 ||
			trace.physical_event_material_segments != 1 ||
			trace.prepared_production_fallback !=
			RT_BREP_PREPARED_FALLBACK_NONE ||
			trace.prepared_production_selected != 1 ||
			trace_hits != 2 || trace.final_segments != 1) {
		    std::printf("FAIL: vertex-fan %s reverse=%d "
			"records=%zu/%zu candidates/events/cert/fail="
			"%zu/%zu/%zu/%zu winding=%zu/%zu owned=%zu/%zu "
			"ledger=%zu/%zu target=%zu complete=%zu selected=%zu "
			"fallback=%d hits=%d final=%zu\n", test.name,
			reverse_ray, trace.prepared_vertex_records,
			trace.supported_vertex_records,
			trace.physical_event_vertex_candidates,
			trace.physical_event_vertex,
			trace.physical_event_vertex_certified,
			trace.physical_event_vertex_failures,
			trace.physical_event_vertex_winding_checks,
			trace.physical_event_vertex_winding_ambiguous,
			trace.physical_event_vertex_owned_boxes,
			trace.physical_event_vertex_owned_roots,
			trace.stored_physical_events, vertex_events,
			target_events, trace.physical_event_complete,
			trace.prepared_production_selected,
			trace.prepared_production_fallback, trace_hits,
			trace.final_segments);
		    for (size_t box_index = 0;
			    box_index < trace.stored_surface_boxes; ++box_index) {
			const struct rt_brep_trace_surface_box &box =
			    trace.surface_boxes[box_index];
			std::printf("  SB %zu f=%d t=[%.17g,%.17g] d=%d s=%d\n",
			    box_index, box.face_index, box.t_min, box.t_max,
			    box.disposition, box.determinant_sign);
		    }
		    for (size_t root_index = 0;
			    root_index < trace.stored_local_roots; ++root_index) {
			const struct rt_brep_trace_local_root &root =
			    trace.local_roots[root_index];
			std::printf("  SR %zu f=%d t=%.17g class=%d trim=%d "
			    "dir=%d nd=%.17g\n", root_index,
			    root.face_index, root.dist, root.hit_class,
			    root.trim_status, root.direction, root.normal_dot);
		    }
		    failures++;
		}
	    }
	}
	failures += check_brep_manifold_edge_similarity_case(test.name,
	    test.scale, xform, stp, case_rtip, &case_resource);
	free_solid(stp);
	rt_clean_resource_basic(case_rtip, &case_resource);
	BU_PTBL_SET(&case_rtip->rti_resources, 0, NULL);
	rt_i_destroy(case_rtip);
    }
    if (!failures)
	std::printf("Vertex/edge similarity invariance: PASS cases=%zu\n",
	    sizeof(cases) / sizeof(cases[0]));
    return failures;
}


static int
check_brep_manifold_edge_events(const ON_Brep &brep,
    struct soltab *implicit_stp, struct soltab *brep_stp,
    struct rt_i *rtip, struct resource *resource)
{
    struct edge_case {
	const char *name;
	ON_3dVector direction;
	size_t candidates;
	size_t transitions;
	size_t contacts;
	size_t regular;
	ON_3dPoint endpoint[2];
	bool compare_implicit;
    } cases[] = {
	{"convex-concave transitions", ON_3dVector(1.0, 1.0, 0.0),
	    2, 2, 0, 0,
	    {ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(2.0, 2.0, 0.0)},
	    true},
	{"concave contact", ON_3dVector(1.0, -1.0, 0.0),
	    3, 2, 1, 0,
	    {ON_3dPoint(0.0, 4.0, 0.0), ON_3dPoint(4.0, 0.0, 0.0)},
	    false},
	{"mixed regular edge", ON_3dVector(0.25, 0.5, 1.0),
	    1, 1, 0, 1,
	    {ON_3dPoint(1.5, 1.0, -2.0), ON_3dPoint(2.0, 2.0, 0.0)},
	    true}
    };
    int failures = 0;
    double maximum_oracle_error = 0.0;
    const ON_3dPoint edge_point(2.0, 2.0, 0.0);
    for (size_t case_index = 0;
	    case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
	if (!cases[case_index].direction.Unitize())
	    return failures + 1;
	for (int reverse_ray = 0; reverse_ray <= 1; ++reverse_ray) {
	    const ON_3dVector direction = reverse_ray ?
		-cases[case_index].direction : cases[case_index].direction;
	    const ON_3dPoint origin = edge_point - 10.0 * direction;
	    sampled_ray ray;
	    VSET(ray.origin, origin.x, origin.y, origin.z);
	    VSET(ray.direction, direction.x, direction.y, direction.z);
	    struct rt_brep_shot_trace trace;
	    const int trace_hits = shoot_brep_trace(brep_stp, rtip,
		resource, ray, trace);
	    ray_result implicit_result;
	    if (cases[case_index].compare_implicit)
		implicit_result = shoot_solid(implicit_stp, rtip, resource,
		    ray.origin, ray.direction);
	    const ray_result production_result = shoot_solid(brep_stp,
		rtip, resource, ray.origin, ray.direction);
	    const double first_parameter =
		(cases[case_index].endpoint[0] - origin) * direction;
	    const double second_parameter =
		(cases[case_index].endpoint[1] - origin) * direction;
	    const double expected_in = std::min(first_parameter,
		second_parameter);
	    const double expected_out = std::max(first_parameter,
		second_parameter);
	    double oracle_error = DBL_MAX;
	    if (production_result.segments == 1) {
		oracle_error = std::max(fabs(expected_in -
		    production_result.in_dist), fabs(expected_out -
		    production_result.out_dist));
		maximum_oracle_error = std::max(maximum_oracle_error,
		    oracle_error);
	    }
	    const bool implicit_matches = !cases[case_index].compare_implicit ||
		(implicit_result.segments == 1 &&
		 fabs(expected_in - implicit_result.in_dist) <= 1.0e-7 &&
		 fabs(expected_out - implicit_result.out_dist) <= 1.0e-7);
	    size_t edge_events = 0;
	    size_t regular_events = 0;
	    size_t reconstructed_contacts = 0;
	    size_t reconstructed_roots = 0;
	    size_t reconstructed_boxes = 0;
	    bool invalid_event = false;
	    for (size_t event_index = 0;
		    event_index < trace.stored_physical_events;
		    ++event_index) {
		const struct rt_brep_trace_physical_event &event =
		    trace.physical_events[event_index];
		if (event.certificate ==
			RT_BREP_TRACE_EVENT_MANIFOLD_EDGE) {
		    edge_events++;
		    if (event.source_kind !=
			    RT_BREP_TRACE_EVENT_SOURCE_MANIFOLD_EDGE ||
			    event.edge_index < 0 ||
			    event.edge_index >= brep.m_E.Count() ||
			    event.vertex_index != -1 ||
			    !event.source_box_count || event.hit_class != 4)
			invalid_event = true;
		    const struct rt_brep_trace_edge *edge =
			brep_trace_edge(trace, event.edge_index);
		    size_t event_roots = 0;
		    size_t event_boxes = 0;
		    if (!edge || !brep_trace_edge_cluster_owned(trace, *edge,
			    &event, event_roots, event_boxes))
			invalid_event = true;
		    reconstructed_roots += event_roots;
		    reconstructed_boxes += event_boxes;
		} else if (event.certificate ==
			RT_BREP_TRACE_EVENT_REGULAR_INTERIOR) {
		    regular_events++;
		} else {
		    invalid_event = true;
		}
	    }
	    for (size_t edge_index = 0;
		    edge_index < trace.stored_edges; ++edge_index) {
		const struct rt_brep_trace_edge &edge = trace.edges[edge_index];
		const double exact_tolerance = 1.0e-9 * std::max(1.0,
		    std::max(fabs(edge.ray_dist),
		    fabs((double)edge.edge_tolerance)));
		if (edge.distance > exact_tolerance || !edge.line_state_valid ||
			edge.line_before_state != edge.line_after_state)
		    continue;
		size_t contact_roots = 0;
		size_t contact_boxes = 0;
		if (!brep_trace_edge_cluster_owned(trace, edge, NULL,
			contact_roots, contact_boxes))
		    invalid_event = true;
		reconstructed_contacts++;
		reconstructed_roots += contact_roots;
		reconstructed_boxes += contact_boxes;
	    }
	    if (!brep_trace_fixed_workspaces_match(trace) ||
		    trace.physical_event_edge_attempts != 1 ||
		    trace.physical_event_edge_candidates !=
			cases[case_index].candidates ||
		    trace.physical_event_edge !=
			cases[case_index].transitions ||
		    trace.physical_event_edge_contacts !=
			cases[case_index].contacts ||
		    reconstructed_contacts != cases[case_index].contacts ||
		    trace.physical_event_edge_certified != 1 ||
		    trace.physical_event_edge_failures ||
		    !trace.physical_event_edge_owned_boxes ||
		    !trace.physical_event_edge_owned_roots ||
		    reconstructed_boxes !=
			trace.physical_event_edge_owned_boxes ||
		    reconstructed_roots !=
			trace.physical_event_edge_owned_roots ||
		    trace.physical_event_regular != cases[case_index].regular ||
		    edge_events != cases[case_index].transitions ||
		    regular_events != cases[case_index].regular ||
		    invalid_event || trace.physical_event_complete != 1 ||
		    trace.physical_event_material_segments != 1 ||
		    trace.stored_physical_events != 2 ||
		    trace.prepared_production_fallback !=
			RT_BREP_PREPARED_FALLBACK_NONE ||
		    trace.prepared_production_selected != 1 ||
		    trace_hits != 2 || !implicit_matches ||
		    production_result.segments != 1 || oracle_error > 1.0e-7) {
		std::printf("FAIL: manifold edge %s reverse=%d "
		    "attempt/candidate/event/contact/cert/fail="
		    "%zu/%zu/%zu/%zu/%zu/%zu owned=%zu/%zu "
		    "regular=%zu events=%zu/%zu complete=%zu segments=%zu "
		    "selected=%zu fallback=%d hits=%d implicit=%d compare=%d "
		    "production=%d "
		    "error=%.17g\n", cases[case_index].name, reverse_ray,
		    trace.physical_event_edge_attempts,
		    trace.physical_event_edge_candidates,
		    trace.physical_event_edge,
		    trace.physical_event_edge_contacts,
		    trace.physical_event_edge_certified,
		    trace.physical_event_edge_failures,
		    trace.physical_event_edge_owned_boxes,
		    trace.physical_event_edge_owned_roots,
		    trace.physical_event_regular, trace.stored_physical_events,
		    edge_events, trace.physical_event_complete,
		    trace.physical_event_material_segments,
		    trace.prepared_production_selected,
		    trace.prepared_production_fallback, trace_hits,
		    implicit_result.segments,
		    cases[case_index].compare_implicit,
		    production_result.segments,
		    oracle_error);
		for (size_t edge_index = 0;
			edge_index < trace.stored_edges; ++edge_index) {
		    const struct rt_brep_trace_edge &edge =
			trace.edges[edge_index];
		    if (edge.distance > 1.0e-6)
			continue;
		    std::printf("  ME %zu e=%d f=%d/%d d=%.17g t=%.17g "
			"sector=%d line=%d state=%d/%d dir=%d\n",
			edge_index, edge.edge_index, edge.face_index[0],
			edge.face_index[1], edge.distance, edge.ray_dist,
			edge.sector_valid, edge.line_state_valid,
			edge.line_before_state, edge.line_after_state,
			edge.line_transition_direction);
		}
		for (size_t box_index = 0;
			box_index < trace.stored_surface_boxes; ++box_index) {
		    const struct rt_brep_trace_surface_box &box =
			trace.surface_boxes[box_index];
		    std::printf("  MB %zu f=%d s=%d t=[%.17g,%.17g] "
			"d=%d sign=%d\n", box_index, box.face_index,
			box.span_index, box.t_min, box.t_max,
			box.disposition, box.determinant_sign);
		}
		for (size_t root_index = 0;
			root_index < trace.stored_local_roots; ++root_index) {
		    const struct rt_brep_trace_local_root &root =
			trace.local_roots[root_index];
		    std::printf("  MR %zu f=%d s=%d t=%.17g trim=%d "
			"class=%d dir=%d nd=%.17g\n", root_index,
			root.face_index, root.span_index, root.dist,
			root.trim_status, root.hit_class, root.direction,
			root.normal_dot);
		}
		failures++;
	    }
	}
    }

    /* A line on one incident face-sector boundary is not an edge transition
     * theorem.  Even if other evidence could make an even stream, the exact
     * ambiguous encounter must retain whole-ray fallback. */
    ON_3dVector boundary_direction(1.0, 0.0, 0.25);
    if (!boundary_direction.Unitize())
	return failures + 1;
    const ON_3dPoint boundary_origin = edge_point -
	10.0 * boundary_direction;
    sampled_ray boundary_ray;
    VSET(boundary_ray.origin, boundary_origin.x, boundary_origin.y,
	boundary_origin.z);
    VSET(boundary_ray.direction, boundary_direction.x, boundary_direction.y,
	boundary_direction.z);
    struct rt_brep_shot_trace boundary_trace;
    (void)shoot_brep_trace(brep_stp, rtip, resource, boundary_ray,
	boundary_trace);
    size_t exact_ambiguous = 0;
    for (size_t edge_index = 0;
	    edge_index < boundary_trace.stored_edges; ++edge_index) {
	const struct rt_brep_trace_edge &edge =
	    boundary_trace.edges[edge_index];
	const double exact_tolerance = 1.0e-9 *
	    std::max(1.0, fabs(edge.ray_dist));
	if (edge.distance <= exact_tolerance && edge.sector_valid &&
		!edge.line_state_valid)
	    exact_ambiguous++;
    }
    const bool rejected_by_edge =
	boundary_trace.physical_event_edge_attempts == 1 &&
	!boundary_trace.physical_event_edge_certified &&
	boundary_trace.physical_event_edge_failures == 1 &&
	boundary_trace.prepared_production_fallback ==
	    RT_BREP_PREPARED_FALLBACK_UNCERTIFIED;
    const bool rejected_before_edge =
	!boundary_trace.physical_event_edge_attempts &&
	!boundary_trace.physical_event_edge_certified &&
	!boundary_trace.physical_event_edge_failures &&
	boundary_trace.prepared_production_fallback ==
	    RT_BREP_PREPARED_FALLBACK_SURFACE_BOXES;
    if (!exact_ambiguous || (!rejected_by_edge && !rejected_before_edge) ||
	    boundary_trace.physical_event_complete ||
	    boundary_trace.stored_physical_events ||
	    boundary_trace.prepared_production_selected) {
	std::printf("FAIL: manifold edge sector-boundary fallback "
	    "ambiguous=%zu attempt/cert/fail=%zu/%zu/%zu "
	    "events=%zu complete=%zu selected=%zu fallback=%d\n",
	    exact_ambiguous, boundary_trace.physical_event_edge_attempts,
	    boundary_trace.physical_event_edge_certified,
	    boundary_trace.physical_event_edge_failures,
	    boundary_trace.stored_physical_events,
	    boundary_trace.physical_event_complete,
	    boundary_trace.prepared_production_selected,
	    boundary_trace.prepared_production_fallback);
	failures++;
    }
    if (!failures)
	std::printf("Manifold edge transitions: PASS cases=%zu "
	    "fallback=1 oracle-error=%.3g\n",
	    sizeof(cases) / sizeof(cases[0]), maximum_oracle_error);
    return failures;
}


static int
check_brep_concave_vertex_fan(const struct bn_tol *tol, struct rt_i *rtip,
    struct resource *resource)
{
    point2d_t vertices[6] = {
	{0.0, 0.0}, {4.0, 0.0}, {4.0, 2.0},
	{2.0, 2.0}, {2.0, 4.0}, {0.0, 4.0}
    };
    struct line_seg segments[6] = {};
    void *segment_pointers[6] = {};
    int reverse[6] = {};
    for (int segment = 0; segment < 6; ++segment) {
	segments[segment].magic = CURVE_LSEG_MAGIC;
	segments[segment].start = segment;
	segments[segment].end = (segment + 1) % 6;
	segment_pointers[segment] = &segments[segment];
    }
    struct rt_sketch_internal sketch = {};
    sketch.magic = RT_SKETCH_INTERNAL_MAGIC;
    VSET(sketch.V, 0.0, 0.0, -2.0);
    VSET(sketch.u_vec, 1.0, 0.0, 0.0);
    VSET(sketch.v_vec, 0.0, 1.0, 0.0);
    sketch.vert_count = 6;
    sketch.verts = vertices;
    sketch.curve.count = 6;
    sketch.curve.reverse = reverse;
    sketch.curve.segment = segment_pointers;

    struct rt_extrude_internal extrude = {};
    extrude.magic = RT_EXTRUDE_INTERNAL_MAGIC;
    VSET(extrude.V, 0.0, 0.0, -2.0);
    VSET(extrude.h, 0.0, 0.0, 4.0);
    VSET(extrude.u_vec, 1.0, 0.0, 0.0);
    VSET(extrude.v_vec, 0.0, 1.0, 0.0);
    extrude.skt = &sketch;
    struct rt_db_internal extrude_intern;
    RT_DB_INTERNAL_INIT(&extrude_intern);
    extrude_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    extrude_intern.idb_type = ID_EXTRUDE;
    extrude_intern.idb_meth = &OBJ[ID_EXTRUDE];
    extrude_intern.idb_ptr = &extrude;

    ON_Brep *brep = ON_Brep::New();
    OBJ[ID_EXTRUDE].ft_brep(&brep, &extrude_intern, tol);
    const ON_3dPoint target_point(2.0, 2.0, 2.0);
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
    if (!brep || !brep->IsSolid() || target_vertex < 0 ||
	    brep->m_V[target_vertex].m_ei.Count() != 3) {
	std::printf("FAIL: concave vertex-fan extrusion solid=%d vertex=%d "
	    "valence=%d\n", brep && brep->IsSolid(), target_vertex,
	    target_vertex >= 0 ? brep->m_V[target_vertex].m_ei.Count() : -1);
	delete brep;
	return 1;
    }

    struct soltab *implicit_stp = prep_solid(rtip, &extrude_intern,
	ID_EXTRUDE);
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
    if (!implicit_stp || !brep_stp) {
	std::printf("FAIL: concave vertex-fan prep implicit=%d brep=%d\n",
	    implicit_stp != NULL, brep_stp != NULL);
	free_solid(implicit_stp);
	free_solid(brep_stp);
	if (!brep_stp)
	    delete brep_internal.brep;
	return 1;
    }

    ON_3dVector inward(-0.5, -0.5, -1.0);
    inward.Unitize();
    int failures = 0;
    double maximum_oracle_error = 0.0;
    for (int reverse_ray = 0; reverse_ray <= 1; ++reverse_ray) {
	const ON_3dVector direction = reverse_ray ? -inward : inward;
	const ON_3dPoint ray_origin = target_point - 20.0 * direction;
	sampled_ray ray;
	VSET(ray.origin, ray_origin.x, ray_origin.y, ray_origin.z);
	VSET(ray.direction, direction.x, direction.y, direction.z);
	struct rt_brep_shot_trace trace;
	const int trace_hits = shoot_brep_trace(brep_stp, rtip, resource,
	    ray, trace);
	const ray_result implicit_result = shoot_solid(implicit_stp, rtip,
	    resource, ray.origin, ray.direction);
	const ray_result production_result = shoot_solid(brep_stp, rtip,
	    resource, ray.origin, ray.direction);
	double oracle_error = DBL_MAX;
	if (implicit_result.segments == 1 && production_result.segments == 1) {
	    oracle_error = std::max(fabs(implicit_result.in_dist -
		production_result.in_dist), fabs(implicit_result.out_dist -
		production_result.out_dist));
	    maximum_oracle_error = std::max(maximum_oracle_error,
		oracle_error);
	}
	size_t vertex_events = 0;
	size_t target_events = 0;
	bool invalid_event = false;
	const int expected_direction = reverse_ray ?
	    RT_BREP_TRACE_LEAVING : RT_BREP_TRACE_ENTERING;
	for (size_t event_index = 0;
		event_index < trace.stored_physical_events; ++event_index) {
	    const struct rt_brep_trace_physical_event &event =
		trace.physical_events[event_index];
	    if (event.certificate != RT_BREP_TRACE_EVENT_VERTEX_FAN)
		continue;
	    vertex_events++;
	    if (event.vertex_index == target_vertex) {
		target_events++;
		if (event.direction != expected_direction)
		    invalid_event = true;
	    }
	    if (event.source_box_count < 3 ||
		    !brep_trace_vertex_event_owned(trace, *brep, event))
		invalid_event = true;
	}
	if (!brep_trace_fixed_workspaces_match(trace) ||
		trace.physical_event_vertex_attempts != 1 ||
		trace.physical_event_vertex_candidates != 2 ||
		trace.physical_event_vertex != 2 ||
		trace.physical_event_vertex_certified != 1 ||
		trace.physical_event_vertex_failures ||
		trace.physical_event_vertex_winding_ambiguous ||
		trace.physical_event_vertex_owned_boxes < 6 ||
		trace.physical_event_vertex_owned_roots != 6 ||
		vertex_events != 2 || target_events != 1 || invalid_event ||
		trace.physical_event_complete != 1 ||
		trace.physical_event_material_segments != 1 ||
		trace.prepared_production_fallback !=
		RT_BREP_PREPARED_FALLBACK_NONE ||
		trace.prepared_production_selected != 1 ||
		trace_hits != 2 || implicit_result.segments != 1 ||
		production_result.segments != 1 ||
		oracle_error > 1.0e-7) {
	    std::printf("FAIL: concave vertex-fan reverse=%d "
		"records=%zu/%zu attempt/candidate/event/cert/fail="
		"%zu/%zu/%zu/%zu/%zu winding=%zu ambiguous=%zu "
		"owned=%zu/%zu events=%zu complete=%zu selected=%zu "
		"fallback=%d hits=%d implicit/production=%d/%d error=%.17g\n",
		reverse_ray, trace.prepared_vertex_records,
		trace.supported_vertex_records,
		trace.physical_event_vertex_attempts,
		trace.physical_event_vertex_candidates,
		trace.physical_event_vertex,
		trace.physical_event_vertex_certified,
		trace.physical_event_vertex_failures,
		trace.physical_event_vertex_winding_checks,
		trace.physical_event_vertex_winding_ambiguous,
		trace.physical_event_vertex_owned_boxes,
		trace.physical_event_vertex_owned_roots, vertex_events,
		trace.physical_event_complete,
		trace.prepared_production_selected,
		trace.prepared_production_fallback, trace_hits,
		implicit_result.segments, production_result.segments,
		oracle_error);
	    for (size_t box_index = 0;
		    box_index < trace.stored_surface_boxes; ++box_index) {
		const struct rt_brep_trace_surface_box &box =
		    trace.surface_boxes[box_index];
		std::printf("  CB %zu f=%d s=%d t=[%.17g,%.17g] d=%d sign=%d\n",
		    box_index, box.face_index, box.span_index, box.t_min,
		    box.t_max, box.disposition, box.determinant_sign);
	    }
	    for (size_t root_index = 0;
		    root_index < trace.stored_local_roots; ++root_index) {
		const struct rt_brep_trace_local_root &root =
		    trace.local_roots[root_index];
		std::printf("  CR %zu f=%d s=%d t=%.17g trim=%d class=%d "
		    "dir=%d nd=%.17g uv=(%.17g,%.17g)\n", root_index,
		    root.face_index, root.span_index, root.dist,
		    root.trim_status, root.hit_class, root.direction,
		    root.normal_dot, root.uv[0], root.uv[1]);
	    }
	    failures++;
	}
    }
    failures += check_brep_vertex_fan_similarity(*brep, target_vertex,
	target_point, inward, tol);
    failures += check_brep_manifold_edge_events(*brep, implicit_stp,
	brep_stp, rtip, resource);
    free_solid(implicit_stp);
    free_solid(brep_stp);
    if (!failures)
	std::printf("Concave vertex-fan transition: PASS oracle-error=%.3g\n",
	    maximum_oracle_error);
    return failures;
}


static int
check_cobb_endpoint_moving_seam_corpus(const struct bn_tol *tol,
    struct rt_i *trace_rtip, struct resource *trace_resource)
{
    int failures = 0;
    const double radius = 10.0;
    const ON_3dPoint origin(0.0, 0.0, 0.0);
    ON_Brep *pristine = ON_Brep_CobbSphereSewn(radius, origin);
    if (!pristine) {
	std::printf("FAIL: endpoint-moving Cobb pristine construction\n");
	return 1;
    }

    struct rt_ell_internal sphere;
    struct rt_db_internal sphere_intern;
    point_t center = VINIT_ZERO;
    init_sphere_internal(sphere, sphere_intern, center, radius);
    prepared_model implicit_model;
    if (!prep_partition_model(implicit_model, &sphere_intern,
	    "cobb_endpoint_oracle.s", tol)) {
	std::printf("FAIL: endpoint-moving Cobb implicit preparation\n");
	delete pristine;
	return 1;
    }

    const double gap_ratios[] = {0.25, 0.9, 1.1};
    const double clearance_ratios[] = {2.0, 0.9, 0.25, 0.0, -0.25};
    size_t rays = 0;
    size_t endpoint_certificates = 0;
    size_t inside_gap_proofs = 0;
    size_t legacy_one_hit = 0;
    size_t certified_pairs = 0;
    size_t selected = 0;
    size_t oracle_differences = 0;
    size_t uncertainty_differences = 0;
    size_t outside_uncertainty_differences = 0;
    size_t below_envelope_leaks = 0;
    size_t above_envelope_pairs = 0;
    size_t exhausted_gap_proofs = 0;
    size_t maximum_gap_cells = 0;
    double maximum_calibration_error = 0.0;
    double maximum_outside_error = 0.0;
    double maximum_edge_vertex_gap = 0.0;
    double maximum_lift_vertex_gap = 0.0;
    size_t endpoint_tolerance_rejections = 0;
    size_t endpoint_similarity_cases = 0;
    size_t minimum_similarity_cells = (size_t)-1;
    size_t maximum_similarity_cells = 0;

    for (size_t ratio_index = 0;
	    ratio_index < sizeof(gap_ratios) / sizeof(gap_ratios[0]);
	    ++ratio_index) {
	for (int sign = -1; sign <= 1; sign += 2) {
	    cobb_seam_frame frame;
	    int minimum_edge_index = pristine->m_E.Count();
	    for (int side = 0; side < 4; ++side) {
		int edge_index = -1;
		if (cobb_target_edge(pristine, frame.face_index, side,
			edge_index) && edge_index < minimum_edge_index) {
		    minimum_edge_index = edge_index;
		    frame.side_index = side;
		}
	    }
	    double measured_gap = 0.0;
	    double applied_displacement = 0.0;
	    const double target_gap = gap_ratios[ratio_index] * tol->dist;
	    ON_Brep *variant = cobb_endpoint_moving_seam_variant(pristine,
		origin, sign * target_gap, frame, measured_gap,
		applied_displacement);
	    const double calibration_error = variant ?
		fabs(measured_gap - target_gap) : INFINITY;
	    double edge_vertex_gap = INFINITY;
	    double lift_vertex_gap = INFINITY;
	    const bool endpoint_contract = variant &&
		cobb_edge_endpoint_contract(variant, frame.edge_index,
		    tol->dist, edge_vertex_gap, lift_vertex_gap);
	    maximum_calibration_error = std::max(maximum_calibration_error,
		calibration_error);
	    if (endpoint_contract) {
		maximum_edge_vertex_gap = std::max(maximum_edge_vertex_gap,
		    edge_vertex_gap);
		maximum_lift_vertex_gap = std::max(maximum_lift_vertex_gap,
		    lift_vertex_gap);
	    }
	    if (!variant || !endpoint_contract || !variant->IsValid() ||
		    !variant->IsSolid() ||
		    calibration_error > 1.0e-3 * target_gap) {
		std::printf("FAIL: endpoint-moving Cobb construction sign=%d "
		    "g/T=%.3g measured=%.17g target=%.17g move=%.17g "
		    "endpoint=%d edge/trim-gap=%.17g/%.17g\n",
		    sign, gap_ratios[ratio_index], measured_gap, target_gap,
		    applied_displacement, endpoint_contract, edge_vertex_gap,
		    lift_vertex_gap);
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
		    "cobb_endpoint_moving.s", tol)) {
		std::printf("FAIL: endpoint-moving Cobb prep sign=%d g/T=%.3g\n",
		    sign, gap_ratios[ratio_index]);
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
		std::printf("FAIL: endpoint-moving Cobb trace prep sign=%d "
		    "g/T=%.3g\n", sign, gap_ratios[ratio_index]);
		delete trace_internal.brep;
		free_prepared_model(variant_model);
		delete variant;
		failures++;
		continue;
	    }

	    partition_result forward_implicit;
	    partition_result forward_variant;
	    for (size_t clearance_index = 0; clearance_index <
		    sizeof(clearance_ratios) / sizeof(clearance_ratios[0]);
		    ++clearance_index) {
		const double clearance = clearance_ratios[clearance_index] *
		    tol->dist;
		for (int reverse = 0; reverse <= 1; ++reverse) {
		    const sampled_ray ray = cobb_seam_grazing_ray(frame, origin,
			radius, clearance, reverse != 0);
		    const partition_result implicit_result = shoot_partitions(
			implicit_model, ray);
		    const partition_result variant_result = shoot_partitions(
			variant_model, ray);
		    struct rt_brep_shot_trace trace;
		    (void)shoot_brep_trace(trace_stp, trace_rtip,
			trace_resource, ray, trace);
		    const struct rt_brep_trace_edge *target_edge =
			brep_trace_edge(trace, frame.edge_index);
		    if (target_edge) {
			maximum_gap_cells = std::max(maximum_gap_cells,
			    target_edge->discrepancy_bound_cells);
			if (target_edge->discrepancy_bound_exhausted)
			    exhausted_gap_proofs++;
		    }
		    if (target_edge &&
			target_edge->discrepancy_endpoints_certified)
			endpoint_certificates++;
		    if (target_edge && target_edge->discrepancy_bounded &&
			    !target_edge->discrepancy_bound_exhausted &&
			    target_edge->discrepancy_authorized &&
			    target_edge->discrepancy_proof_class ==
			    RT_BREP_SEAM_GAP_INSIDE)
			inside_gap_proofs++;
		    if (!target_edge || !target_edge->correspondence_supported ||
			    !target_edge->discrepancy_endpoints_certified ||
			    !target_edge->discrepancy_bounded ||
			    target_edge->discrepancy_bound_exhausted ||
			    !target_edge->discrepancy_authorized ||
			    target_edge->discrepancy_proof_class !=
			    RT_BREP_SEAM_GAP_INSIDE ||
			    !target_edge->discrepancy_bound_cells) {
			std::printf("FAIL: endpoint-moving Cobb proof "
			    "sign=%d g/T=%.3g h/T=%.3g reverse=%d "
			    "edge=%d correspondence/endpoints=%d/%d "
			    "proof=%d bounded/authorized=%d/%d cells=%zu\n",
			    sign,
			    gap_ratios[ratio_index],
			    clearance_ratios[clearance_index], reverse,
			    target_edge != NULL, target_edge ?
			    target_edge->correspondence_supported : -1,
			    target_edge ?
			    target_edge->discrepancy_endpoints_certified : -1,
			    target_edge ?
			    target_edge->discrepancy_proof_class : -1,
			    target_edge ?
			    target_edge->discrepancy_bounded : -1,
			    target_edge ?
			    target_edge->discrepancy_authorized : -1,
			    target_edge ?
			    target_edge->discrepancy_bound_cells : 0);
			failures++;
		    }
		    if (trace.final_hits == 1)
			legacy_one_hit++;
		    if (trace.physical_event_seam_certified) {
			certified_pairs++;
			if (!brep_trace_seam_event_stream_valid(trace,
				target_edge, variant, implicit_result, tol->dist,
				false, 2.5e-4)) {
			    std::printf("FAIL: endpoint-moving Cobb certified "
				"stream sign=%d g/T=%.3g h/T=%.3g reverse=%d\n",
				sign, gap_ratios[ratio_index],
				clearance_ratios[clearance_index], reverse);
			    std::printf("  oracle=[%.17g %.17g] events=%zu "
				"roots/boxes=%zu/%zu contact=%zu/%zu/%zu\n",
				implicit_result.intervals[0].in_dist,
				implicit_result.intervals[0].out_dist,
				trace.stored_physical_events,
				trace.stored_local_roots,
				trace.stored_surface_boxes,
				trace.physical_event_seam_contact_pairs,
				trace.physical_event_seam_contact_boxes,
				trace.physical_event_seam_contact_roots);
			    for (size_t event_index = 0;
				event_index < trace.stored_physical_events;
				++event_index) {
				const struct rt_brep_trace_physical_event &event =
				    trace.physical_events[event_index];
				std::printf("  event[%zu] t=[%.17g %.17g %.17g] "
				    "cert/dir/face/span=%d/%d/%d/%d\n",
				    event_index, event.t_min, event.dist,
				    event.t_max, event.certificate,
				    event.direction, event.face_index,
				    event.span_index);
			    }
			    failures++;
			}
		    }
		    if (trace.prepared_production_selected)
			selected++;
		    if (gap_ratios[ratio_index] > 1.0 &&
			trace.physical_event_seam_certified)
			above_envelope_pairs++;

		    double endpoint_error = 0.0;
		    const bool same = partition_results_match(implicit_result,
			variant_result, tol->dist, endpoint_error);
		    if (std::isfinite(endpoint_error))
			maximum_outside_error = std::max(maximum_outside_error,
			    endpoint_error);
		    const bool within_uncertainty = fabs(clearance) <=
			2.0 * (tol->dist + measured_gap);
		    if (!same) {
			oracle_differences++;
			if (within_uncertainty)
			    uncertainty_differences++;
			else
			    outside_uncertainty_differences++;
		    }
		    if (gap_ratios[ratio_index] < 1.0 && clearance > 0.0 &&
			    implicit_result.partitions &&
			    !variant_result.partitions)
			below_envelope_leaks++;
		    if (clearance <= 0.0 &&
			    (trace.closure_candidates ||
			     trace.physical_event_seam_certified)) {
			std::printf("FAIL: endpoint-moving Cobb contact/exterior "
			    "closure sign=%d g/T=%.3g h/T=%.3g reverse=%d "
			    "closure=%zu certified=%zu\n", sign,
			    gap_ratios[ratio_index],
			    clearance_ratios[clearance_index], reverse,
			    trace.closure_candidates,
			    trace.physical_event_seam_certified);
			failures++;
		    }
		    if ((clearance_index != 3 &&
			(!partition_result_valid(implicit_result, ray.direction) ||
			 !partition_result_valid(variant_result, ray.direction))) ||
			    trace.final_segments != variant_result.partitions) {
			std::printf("FAIL: endpoint-moving Cobb invalid stream "
			    "sign=%d g/T=%.3g h/T=%.3g reverse=%d "
			    "segments=%zu/%zu\n", sign,
			    gap_ratios[ratio_index],
			    clearance_ratios[clearance_index], reverse,
			    trace.final_segments, variant_result.partitions);
			failures++;
		    }
		    if (!reverse) {
			forward_implicit = implicit_result;
			forward_variant = variant_result;
		    } else if (forward_implicit.partitions !=
			    implicit_result.partitions ||
			    forward_variant.partitions !=
			    variant_result.partitions ||
			    fabs(partition_chord(forward_implicit) -
			    partition_chord(implicit_result)) > tol->dist ||
			    fabs(partition_chord(forward_variant) -
			    partition_chord(variant_result)) > tol->dist) {
			std::printf("FAIL: endpoint-moving Cobb reversal "
			    "sign=%d g/T=%.3g h/T=%.3g\n", sign,
			    gap_ratios[ratio_index],
			    clearance_ratios[clearance_index]);
			failures++;
		    }
		    rays++;
		}
	    }

	    free_solid(trace_stp);
	    free_prepared_model(variant_model);
	    delete variant;
	}
    }


    /* The endpoint/topology certificate and the resulting physical pair are
     * similarity invariants.  Exercise the one newly certified endpoint-
     * moving state independently of the endpoint-preserving classifier
     * matrix, including ray reversal and extreme scale/translation. */
    {
	cobb_seam_frame frame;
	int minimum_edge_index = pristine->m_E.Count();
	for (int side = 0; side < 4; ++side) {
	    int edge_index = -1;
	    if (cobb_target_edge(pristine, frame.face_index, side, edge_index) &&
		    edge_index < minimum_edge_index) {
		minimum_edge_index = edge_index;
		frame.side_index = side;
	    }
	}
	double measured_gap = 0.0;
	double applied_displacement = 0.0;
	ON_Brep *base = cobb_endpoint_moving_seam_variant(pristine, origin,
	    -0.9 * tol->dist, frame, measured_gap, applied_displacement);
	struct endpoint_transform_case {
	    const char *name;
	    double scale;
	    ON_3dVector translation;
	    ON_3dVector axis;
	    double angle;
	} transform_cases[] = {
	    {"translated", 1.0, ON_3dVector(-31.25, 47.5, 103.75),
		ON_3dVector(0.0, 0.0, 1.0), 0.0},
	    {"oblique-rotated-translated", 1.0,
		ON_3dVector(-19.0, 23.0, 41.0),
		ON_3dVector(1.0, -2.0, 0.5), 0.731},
	    {"small-similarity", 0.01,
		ON_3dVector(1.25, -2.5, 5.0),
		ON_3dVector(-0.3, 1.0, 0.7), -1.113},
	    {"large-similarity", 1.0e4,
		ON_3dVector(1.0e6, -2.0e6, 3.0e6),
		ON_3dVector(2.0, 0.25, -1.0), 2.017}
	};
	if (!base) {
	    std::printf("FAIL: endpoint-moving similarity construction\n");
	    failures++;
	} else {
	    for (size_t case_index = 0; case_index <
		    sizeof(transform_cases) / sizeof(transform_cases[0]);
		    ++case_index) {
		const endpoint_transform_case &test = transform_cases[case_index];
		const ON_Xform xform = cobb_axis_angle_similarity_transform(
		    test.scale, test.translation, test.axis, test.angle);
		ON_Brep *variant = new ON_Brep(*base);
		bool transformed = variant->Transform(xform);
		for (int vertex_index = 0;
			transformed && vertex_index < variant->m_V.Count();
			++vertex_index)
		    variant->m_V[vertex_index].m_tolerance =
			base->m_V[vertex_index].m_tolerance * test.scale;
		for (int edge_index = 0;
			transformed && edge_index < variant->m_E.Count();
			++edge_index)
		    variant->m_E[edge_index].m_tolerance =
			base->m_E[edge_index].m_tolerance * test.scale;
		struct bn_tol case_tol = *tol;
		case_tol.dist = tol->dist * test.scale;
		case_tol.dist_sq = case_tol.dist * case_tol.dist;
		double edge_vertex_gap = 0.0;
		double lift_vertex_gap = 0.0;
		if (!transformed || !variant->IsValid() || !variant->IsSolid() ||
			!cobb_edge_endpoint_contract(variant, frame.edge_index,
			    case_tol.dist, edge_vertex_gap, lift_vertex_gap)) {
		    std::printf("FAIL: endpoint-moving similarity %s "
			"geometry endpoint=%.17g/%.17g\n", test.name,
			edge_vertex_gap, lift_vertex_gap);
		    delete variant;
		    failures++;
		    continue;
		}

		struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
		if (!rtip) {
		    std::printf("FAIL: endpoint-moving similarity %s rt_i\n",
			test.name);
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
		    std::printf("FAIL: endpoint-moving similarity %s prep\n",
			test.name);
		    failures++;
		} else {
		    for (int reverse = 0; reverse <= 1; ++reverse) {
			const sampled_ray base_ray = cobb_seam_grazing_ray(frame,
			    origin, radius, 0.25 * tol->dist, reverse != 0);
			const partition_result base_oracle = shoot_partitions(
			    implicit_model, base_ray);
			partition_result oracle = base_oracle;
			if (oracle.partitions == 1) {
			    oracle.intervals[0].in_dist *= test.scale;
			    oracle.intervals[0].out_dist *= test.scale;
			}
			const ON_3dPoint transformed_origin =
			    cobb_transform_point(xform, ON_3dPoint(base_ray.origin));
			ON_3dVector transformed_direction = cobb_transform_vector(
			    xform, ON_3dVector(base_ray.direction));
			sampled_ray ray;
			const bool direction_valid = transformed_direction.Unitize();
			VSET(ray.origin, transformed_origin.x,
			    transformed_origin.y, transformed_origin.z);
			VSET(ray.direction, transformed_direction.x,
			    transformed_direction.y, transformed_direction.z);
			struct rt_brep_shot_trace trace;
			(void)shoot_brep_trace(stp, rtip, &resource, ray, trace);
			const struct rt_brep_trace_edge *target_edge =
			    brep_trace_edge(trace, frame.edge_index);
			if (!direction_valid || !target_edge ||
				!target_edge->correspondence_supported ||
				!target_edge->discrepancy_endpoints_certified ||
				!target_edge->discrepancy_bounded ||
				target_edge->discrepancy_bound_exhausted ||
				!target_edge->discrepancy_authorized ||
				target_edge->discrepancy_proof_class !=
				    RT_BREP_SEAM_GAP_INSIDE ||
				!trace.physical_event_seam_certified ||
				!brep_trace_seam_event_stream_valid(trace,
				    target_edge, variant, oracle, case_tol.dist,
				    false, 2.5e-4)) {
			    std::printf("FAIL: endpoint-moving similarity %s "
				"reverse=%d edge/endpoints/bounded/authorized="
				"%d/%d/%d/%d proof=%d certified=%zu cells=%zu\n",
				test.name, reverse, target_edge != NULL,
				target_edge ?
				target_edge->discrepancy_endpoints_certified : -1,
				target_edge ?
				target_edge->discrepancy_bounded : -1,
				target_edge ?
				target_edge->discrepancy_authorized : -1,
				target_edge ?
				target_edge->discrepancy_proof_class : -1,
				trace.physical_event_seam_certified,
				target_edge ?
				target_edge->discrepancy_bound_cells : 0);
			    std::printf("  seam attempt/root/closure/continuation/"
				"cert/fail=%zu/%zu/%zu/%zu/%zu/%zu "
				"roots/boxes/final/selected=%zu/%zu/%zu/%zu "
				"sector/state/ray-dot=%d/%d/%.17g\n",
				trace.physical_event_seam_attempts,
				trace.physical_event_seam_root_candidates,
				trace.physical_event_seam_closure_candidates,
				trace.physical_event_seam_continuation_candidates,
				trace.physical_event_seam_certified,
				trace.physical_event_seam_failures,
				trace.stored_local_roots,
				trace.stored_surface_boxes, trace.final_segments,
				trace.prepared_production_selected,
				target_edge ? target_edge->sector_valid : -1,
				target_edge ? target_edge->closest_state : -1,
				target_edge ? target_edge->ray_edge_dot : NAN);
			    failures++;
			} else {
			    endpoint_similarity_cases++;
			    minimum_similarity_cells = std::min(
				minimum_similarity_cells,
				target_edge->discrepancy_bound_cells);
			    maximum_similarity_cells = std::max(
				maximum_similarity_cells,
				target_edge->discrepancy_bound_cells);
			}
		    }
		    free_solid(stp);
		}
		rt_clean_resource_basic(rtip, &resource);
		BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
		rt_i_destroy(rtip);
	    }
	}
	delete base;
    }


    /* A large edge tolerance or the opposite endpoint's vertex tolerance
     * must not make the independent endpoint oracle accept an under-declared
     * endpoint.  Do not send this intentionally invalid metadata through
     * BREP prep; malformed-model prep hardening is a separate concern. */
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
	cobb_seam_frame frame;
	int minimum_edge_index = pristine->m_E.Count();
	for (int side = 0; side < 4; ++side) {
	    int edge_index = -1;
	    if (cobb_target_edge(pristine, frame.face_index, side, edge_index) &&
		    edge_index < minimum_edge_index) {
		minimum_edge_index = edge_index;
		frame.side_index = side;
	    }
	}
	double measured_gap = 0.0;
	double applied_displacement = 0.0;
	ON_Brep *variant = cobb_endpoint_moving_seam_variant(pristine, origin,
	    2.0 * tol->dist, frame, measured_gap, applied_displacement);
	if (!variant || frame.edge_index < 0 ||
		frame.edge_index >= variant->m_E.Count()) {
	    std::printf("FAIL: endpoint-moving under-declared construction\n");
	    delete variant;
	    failures++;
	    continue;
	}
	const int vertex_index =
	    variant->m_E[frame.edge_index].m_vi[endpoint];
	if (vertex_index < 0 || vertex_index >= variant->m_V.Count()) {
	    std::printf("FAIL: endpoint-moving under-declared vertex\n");
	    delete variant;
	    failures++;
	    continue;
	}
	variant->m_V[vertex_index].m_tolerance = 0.5 * tol->dist;
	double edge_vertex_gap = 0.0;
	double lift_vertex_gap = 0.0;
	if (cobb_edge_endpoint_contract(variant, frame.edge_index, tol->dist,
		edge_vertex_gap, lift_vertex_gap)) {
	    std::printf("FAIL: endpoint-moving under-declared endpoint=%d "
		"edge/trim-gap=%.17g/%.17g\n", endpoint,
		edge_vertex_gap, lift_vertex_gap);
	    failures++;
	} else {
	    endpoint_tolerance_rejections++;
	}
	delete variant;
    }


    if (rays != 60 || endpoint_certificates != rays ||
	    inside_gap_proofs != rays || exhausted_gap_proofs ||
	    !maximum_gap_cells || certified_pairs < 2 || legacy_one_hit > 16 ||
	    selected < certified_pairs || oracle_differences > 46 ||
	    uncertainty_differences > 46 || outside_uncertainty_differences ||
	    below_envelope_leaks || endpoint_tolerance_rejections != 2 ||
	    endpoint_similarity_cases != 8) {
	std::printf("FAIL: endpoint-moving Cobb ratchet rays=%zu/60 "
	    "certified/above=%zu/%zu endpoints/inside=%zu/%zu exhausted/cells="
	    "%zu/%zu legacy/selected=%zu/%zu differences=%zu/%zu/%zu "
	    "leaks=%zu endpoint-rejections=%zu/2 similarity=%zu/8\n", rays,
	    certified_pairs,
	    above_envelope_pairs,
	    endpoint_certificates, inside_gap_proofs, exhausted_gap_proofs,
	    maximum_gap_cells, legacy_one_hit, selected, oracle_differences,
	    uncertainty_differences, outside_uncertainty_differences,
	    below_envelope_leaks, endpoint_tolerance_rejections,
	    endpoint_similarity_cases);
	failures++;
    }
    std::printf("Cobb endpoint-moving seam trend: rays=%zu "
	"endpoint/inside=%zu/%zu "
	"legacy-one-hit=%zu certified=%zu selected=%zu "
	"oracle-differences=%zu uncertainty=%zu outside=%zu leaks=%zu "
	"gap-exhausted=%zu max-gap-cells=%zu max-oracle-error=%.3g "
	"max-calibration=%.3g endpoint-edge/lift=%.3g/%.3g "
	"endpoint-rejections=%zu similarity=%zu cells=%zu/%zu\n", rays,
	endpoint_certificates, inside_gap_proofs, legacy_one_hit,
	certified_pairs, selected,
	oracle_differences, uncertainty_differences,
	outside_uncertainty_differences, below_envelope_leaks,
	exhausted_gap_proofs, maximum_gap_cells, maximum_outside_error,
	maximum_calibration_error, maximum_edge_vertex_gap,
	maximum_lift_vertex_gap, endpoint_tolerance_rejections,
	endpoint_similarity_cases, minimum_similarity_cells,
	maximum_similarity_cells);
    free_prepared_model(implicit_model);
    delete pristine;
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
    size_t prepared_partition_promotions = 0;
    size_t prepared_seam_pairs = 0;
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
	    "discrepancy_measured,correspondence_screened,"
	    "correspondence_supported,correspondence_cells,"
	    "correspondence_depth,correspondence_exhausted,"
	    "discrepancy_sample_authorized,discrepancy_proof_class,"
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
		    const bool invalid_discrepancy_bound = target_edge &&
			(!target_edge->discrepancy_bounded ||
			target_edge->discrepancy_bound_exhausted ||
			target_edge->discrepancy_proof_class !=
			RT_BREP_SEAM_GAP_INSIDE ||
			target_edge->discrepancy_lower_bound >
			measured_gap + edge_distance_limit ||
			target_edge->discrepancy_upper_bound <
			measured_gap - edge_distance_limit ||
			!(target_edge->discrepancy_upper_bound <
			target_edge->edge_tolerance));
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
			    !target_edge->discrepancy_sample_authorized ||
			    !target_edge->correspondence_screened ||
			    !target_edge->correspondence_supported ||
			    !target_edge->correspondence_cells ||
			    target_edge->correspondence_exhausted ||
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
			    "bound=%.17g/%.17g proof=%d sample=%d authorized=%d "
			    "spans=%zu within=%d/%d sector=%d state=%d/%d "
			    "lifts=%.17g/%.17g ray-edge=%.17g\n", sign,
			    gap_ratios[ratio_index],
			    clearance_ratios[clearance_index], reverse,
			    target_edge ? target_edge->distance : INFINITY,
			    fabs(clearance), edge_distance_limit,
			    target_edge ? target_edge->edge_tolerance : INFINITY,
			    target_edge ?
				target_edge->discrepancy_lower_bound : INFINITY,
			    target_edge ?
				target_edge->discrepancy_upper_bound : INFINITY,
			    target_edge ?
				target_edge->discrepancy_proof_class : -1,
			    target_edge ?
				target_edge->discrepancy_sample_authorized : -1,
			    target_edge ? target_edge->discrepancy_authorized : -1,
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
		    if (trace.physical_event_seam_certified) {
			if (!trace.prepared_production_selected ||
				!brep_trace_seam_event_stream_valid(trace,
				    target_edge, variant, implicit_result,
				    tol->dist)) {
			    std::printf("FAIL: bowed Cobb seam event stream "
				"sign=%d g/T=%.3g h/T=%.3g reverse=%d "
				"selected=%zu events=%zu/%zu complete=%zu "
				"failures=%zu ownership=%zu\n", sign,
				gap_ratios[ratio_index],
				clearance_ratios[clearance_index], reverse,
				trace.prepared_production_selected,
				trace.physical_event_seam,
				trace.stored_physical_events,
				trace.physical_event_complete,
				trace.physical_event_seam_failures,
				trace.physical_event_seam_ownership_failures);
			    failures++;
			} else {
			    prepared_seam_pairs++;
			}
		    }
		    if (trace.physical_event_seam_ownership_failures &&
			    (trace.prepared_production_selected ||
			     trace.physical_event_seam ||
			     trace.physical_event_seam_certified)) {
			std::printf("FAIL: bowed Cobb unowned seam work published "
			    "sign=%d g/T=%.3g h/T=%.3g reverse=%d "
			    "ownership=%zu selected/events/certified=%zu/%zu/%zu\n",
			    sign, gap_ratios[ratio_index],
			    clearance_ratios[clearance_index], reverse,
			    trace.physical_event_seam_ownership_failures,
			    trace.prepared_production_selected,
			    trace.physical_event_seam,
			    trace.physical_event_seam_certified);
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
			if (trace.prepared_production_selected && prepared_same &&
				same) {
			    prepared_partition_promotions++;
			    classification = "promotion";
			} else if (prepared_same && !same) {
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
				"%d,%d,%d,%zu,%zu,%d,%d,%d,%d,%d,%zu,%d,"
				"%.17g,%.17g,"
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
				edge.correspondence_screened,
				edge.correspondence_supported,
				edge.correspondence_cells,
				edge.correspondence_depth,
				edge.correspondence_exhausted,
				edge.discrepancy_sample_authorized,
				edge.discrepancy_proof_class,
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
    if (root_events.physical_event_complete !=
	    root_events.prepared_production_selected) {
	std::printf("FAIL: prepared Cobb/event-ledger selection=%zu/%zu\n",
	    root_events.prepared_production_selected,
	    root_events.physical_event_complete);
	failures++;
    }
    std::printf("Cobb prepared partition changes: promotions=%zu "
	"improvements=%zu regressions=%zu ambiguous=%zu "
	"max-changed-oracle-error=%.3g\n", prepared_partition_promotions,
	prepared_partition_improvements, prepared_partition_regressions,
	prepared_partition_ambiguous, maximum_prepared_oracle_error);
    if (prepared_partition_promotions != 2 ||
	    prepared_partition_regressions || prepared_partition_ambiguous) {
	std::printf("FAIL: prepared Cobb partitions have %zu regressions and "
	    "%zu ambiguous changes; promotions=%zu/2\n",
	    prepared_partition_regressions, prepared_partition_ambiguous,
	    prepared_partition_promotions);
	failures++;
    }
    if (prepared_seam_pairs != 22 ||
	    root_events.prepared_production_selected != 124 ||
	    root_events.physical_event_seam != 44 ||
	    root_events.physical_event_seam_certified != 22 ||
	    root_events.physical_event_seam_edge_only_candidates != 14 ||
	    root_events.physical_event_seam_ownership_failures ||
	    root_events.physical_event_seam_witness_failures ||
	    root_events.physical_event_seam_box_failures ||
	    root_events.physical_event_seam_root_coverage_failures ||
	    root_events.physical_event_seam_witness_boxes != 4 ||
	    root_events.physical_event_seam_witness_roots != 4 ||
	    root_events.physical_event_seam_contact_pairs != 6 ||
	    root_events.physical_event_seam_contact_boxes != 12 ||
	    root_events.physical_event_seam_contact_roots != 12 ||
	    root_events.physical_event_seam_contact_miss_roots) {
	std::printf("FAIL: prepared Cobb seam certification pairs=%zu/22 "
	    "selected=%zu/124 "
	    "events=%zu/44 certified=%zu/22 edge-only=%zu/14 "
	    "ownership=%zu witness=%zu box/root=%zu/%zu "
	    "witness-box/root=%zu/%zu contact=%zu/%zu/%zu/%zu\n",
	    prepared_seam_pairs,
	    root_events.prepared_production_selected,
	    root_events.physical_event_seam,
	    root_events.physical_event_seam_certified,
	    root_events.physical_event_seam_edge_only_candidates,
	    root_events.physical_event_seam_ownership_failures,
	    root_events.physical_event_seam_witness_failures,
	    root_events.physical_event_seam_box_failures,
	    root_events.physical_event_seam_root_coverage_failures,
	    root_events.physical_event_seam_witness_boxes,
	    root_events.physical_event_seam_witness_roots,
	    root_events.physical_event_seam_contact_pairs,
	    root_events.physical_event_seam_contact_boxes,
	    root_events.physical_event_seam_contact_roots,
	    root_events.physical_event_seam_contact_miss_roots);
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
	    prepared_partition_promotions + prepared_partition_improvements +
	    prepared_partition_regressions + prepared_partition_ambiguous) {
	std::printf("FAIL: prepared Cobb event accounting failures=%zu "
	    "overflow=%zu near-miss-stage=%zu changes=%zu/%zu\n",
	    root_events.local_event_failures,
	    root_events.local_event_overflow,
	    root_events.local_candidate_semantic_stage[0],
	    root_events.local_event_final_mismatches,
	    prepared_partition_promotions + prepared_partition_improvements +
	    prepared_partition_regressions + prepared_partition_ambiguous);
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
    const bool report_cobb_oblique = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--cobb-oblique-report");
    const bool affine_only = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--affine-only");
    const bool interval_only = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--interval-only");
    const bool local_root_only = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--local-root-only");
    const bool grazing_root_only = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--grazing-root-only");
    const bool trim_interval_only = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--trim-interval-only");
    const bool source_union_only = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--source-union-only");
    const bool fold_only = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--fold-only");
    const bool core_only = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--core-only");
    const bool directed_only = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--directed-only");
    const bool crofton_only = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--crofton-only");
    const bool seam_only = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--seam-only");
    const bool endpoint_only = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--endpoint-only");
    const bool nonisoparametric_only = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--nonisoparametric-only");
    const bool contact_trim_only = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--contact-trim-only");
    const bool defect_only = argc == 2 &&
	BU_STR_EQUAL(argv[1], "--defect-only");
    if (argc != 1 && !report_grazing && !report_cobb &&
	    !report_cobb_oblique && !affine_only &&
	    !interval_only && !local_root_only && !grazing_root_only &&
	    !trim_interval_only &&
	    !source_union_only &&
	    !fold_only && !core_only &&
	    !directed_only &&
	    !crofton_only && !seam_only && !endpoint_only &&
	    !nonisoparametric_only && !contact_trim_only && !defect_only)
	bu_exit(1, "Usage: %s [--grazing-report|--cobb-report|"
	    "--cobb-oblique-report|"
	    "--affine-only|--interval-only|--local-root-only|"
	    "--grazing-root-only|"
	    "--trim-interval-only|"
	    "--source-union-only|--fold-only|"
	    "--core-only|"
	    "--directed-only|--crofton-only|--seam-only|--endpoint-only|"
	    "--nonisoparametric-only|--contact-trim-only|--defect-only]\n",
	    argv[0]);
    if (interval_only) {
	const int interval_failures = check_brep_interval_enclosures() +
	    check_brep_local_root_solver() +
	    check_brep_weighted_local_root_solver() +
	    check_brep_fold_interval_classifier() +
	    check_brep_source_union_solver() +
	    check_brep_trim_interval_solver();
	return interval_failures ? 1 : 0;
    }
    if (local_root_only)
	return (check_brep_local_root_solver() +
	    check_brep_weighted_local_root_solver()) ? 1 : 0;
    if (trim_interval_only)
	return check_brep_trim_interval_solver() ? 1 : 0;
    if (source_union_only)
	return check_brep_source_union_solver() ? 1 : 0;
    if (fold_only)
	return check_brep_fold_solver() ? 1 : 0;
    const bool split_core = directed_only || grazing_root_only ||
	crofton_only || seam_only || endpoint_only;
    const bool run_directed = !split_core || directed_only;
    const bool run_grazing_root = !split_core || directed_only ||
	grazing_root_only;
    const bool run_crofton = !split_core || crofton_only;
    const bool run_seam = !split_core || seam_only;
    const bool run_endpoint = !split_core || endpoint_only;

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

    if (nonisoparametric_only) {
	const int nonisoparametric_failures =
	    check_cobb_nonisoparametric_oblique(&rtip->rti_tol);
	rt_clean_resource_basic(rtip, &resp);
	BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
	rt_i_destroy(rtip);
	return nonisoparametric_failures ? 1 : 0;
    }

    if (contact_trim_only) {
	const int contact_trim_failures =
	    check_cobb_contact_trim_oblique(&rtip->rti_tol);
	rt_clean_resource_basic(rtip, &resp);
	BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
	rt_i_destroy(rtip);
	return contact_trim_failures ? 1 : 0;
    }

    if (defect_only) {
	const int defect_failures = check_cobb_isolated_defect_corpus(
	    &rtip->rti_tol, rtip, &resp);
	rt_clean_resource_basic(rtip, &resp);
	BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
	rt_i_destroy(rtip);
	return defect_failures ? 1 : 0;
    }

    if (report_cobb_oblique) {
	const int oblique_failures = check_cobb_oblique_contact_trend(
	    &rtip->rti_tol, true);
	rt_clean_resource_basic(rtip, &resp);
	BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
	rt_i_destroy(rtip);
	return oblique_failures ? 1 : 0;
    }

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
    if (!core_only && !split_core) {
	failures += check_brep_interval_enclosures();
	failures += check_brep_local_root_solver();
	failures += check_brep_weighted_local_root_solver();
	failures += check_brep_fold_interval_classifier();
	failures += check_brep_source_union_solver();
	failures += check_brep_trim_interval_solver();
	failures += check_brep_fold_solver();
    }
    if (run_directed) {
	for (size_t repeat = 0; repeat < 16; ++repeat) {
	    for (size_t i = 0; i < sizeof(rays) / sizeof(rays[0]); ++i)
		failures += check_ray(rays[i].label, implicit_stp, brep_stp,
		    rtip, &resp, rays[i].origin, rays[i].direction,
		    rays[i].expected_in, rays[i].expected_out);
	}

	failures += check_grazing_ratchet(implicit_stp, brep_stp, rtip,
	    &resp, radius);

	point_t small_center = {1.25, -2.5, 5.0};
	failures += check_transformed_sphere(rtip, &resp,
	    "small-translated", small_center, 0.01);
	point_t large_center = {1.0e6, -2.0e6, 3.0e6};
	failures += check_transformed_sphere(rtip, &resp,
	    "large-translated", large_center, 1.0e4);

	if (report_grazing)
	    grazing_report(implicit_stp, brep_stp, rtip, &resp, radius);
    }
    if (run_grazing_root) {
	const ON_3dPoint grazing_center(0.0, 0.0, 0.0);
	const ON_3dVector grazing_radial(2.0, 3.0, 6.0);
	const ON_3dVector grazing_tangent(3.0, -2.0, 0.0);
	failures += check_grazing_local_root_certificate_trend(brep_stp,
	    implicit_stp, brep_stp, rtip, &resp, radius, "Sphere",
	    grazing_center, grazing_radial, grazing_tangent, 1.0, 0, true,
	    false, false, false);
	failures += check_torus_grazing_local_root_certificate_trend(rtip,
	    &resp);
	failures += check_torus_status2_similarity(&rtip->rti_tol);
    }

    free_solid(brep_stp);
    free_solid(implicit_stp);

    if (run_directed)
	failures += check_sphere_adaptive_similarity(&rtip->rti_tol);

    if (run_crofton) {
	point_t sphere_min = {-radius, -radius, -radius};
	point_t sphere_max = {radius, radius, radius};
	failures += check_shared_crofton_fixture("sphere", &ell_intern,
	    &rtip->rti_tol, sphere_min, sphere_max,
	    4.0 * M_PI * radius * radius,
	    (4.0 / 3.0) * M_PI * radius * radius * radius, 32000);
	failures += check_shared_primitive_corpus(&rtip->rti_tol);
	failures += check_brep_leaf_csg_corpus(&rtip->rti_tol);
	failures += check_cobb_sphere_corpus(&rtip->rti_tol);
	failures += check_crofton_sphere(&ell_intern, &rtip->rti_tol, radius);
    }

    if (!core_only && !split_core)
	failures += check_ellipsoid_adaptive_affine(&rtip->rti_tol);

    if (run_seam) {
	failures += check_brep_edge_sector_box(&rtip->rti_tol, rtip, &resp);
	failures += check_brep_edge_sector_concave(&rtip->rti_tol, rtip,
	    &resp);
	failures += check_brep_edge_sector_seam(&rtip->rti_tol, rtip, &resp);
	failures += check_brep_vertex_fan_transition(&rtip->rti_tol, rtip,
	    &resp);
	failures += check_brep_concave_vertex_fan(&rtip->rti_tol, rtip,
	    &resp);
	failures += check_cobb_classifier_invariance(&rtip->rti_tol);
	failures += check_cobb_oblique_contact_trend(&rtip->rti_tol, false);
	failures += check_cobb_nonisoparametric_oblique(&rtip->rti_tol);
	failures += check_cobb_contact_trim_oblique(&rtip->rti_tol);
	failures += check_cobb_ambiguous_correspondence(&rtip->rti_tol, rtip,
	    &resp);
	failures += check_cobb_discrepancy_bound_budget(&rtip->rti_tol, rtip,
	    &resp);
	failures += check_cobb_tolerance_metadata(&rtip->rti_tol, rtip,
	    &resp);
	failures += check_cobb_bowed_seam_corpus(&rtip->rti_tol, report_cobb,
	    rtip, &resp);
	failures += check_cobb_isolated_defect_corpus(&rtip->rti_tol, rtip,
	    &resp);
    }

    if (run_endpoint)
	failures += check_cobb_endpoint_moving_seam_corpus(&rtip->rti_tol,
	    rtip, &resp);

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

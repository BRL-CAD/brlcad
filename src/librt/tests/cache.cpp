/*                           C A C H E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2026 United States Government as represented by
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

/* Stress tests for the librt cache mechanism. */

#include "common.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/avs.h"
#include "bu/env.h"
#include "bu/hash.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/str.h"
#include "raytrace.h"
#include "../librt_private.h"

const char *RTC_PREFIX = "rt_cache_test";

static void
add_brep_sph(struct db_i *dbip, const char *name, point_t *v, double r, long int test_num)
{
    struct directory *dp;
    struct rt_db_internal intern;
    ON_3dPoint vp(*v);
    ON_Sphere sph(vp, r);
    struct rt_brep_internal* bi;

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BREP;
    intern.idb_meth = &OBJ[ID_BREP];

    BU_ALLOC(intern.idb_ptr, struct rt_brep_internal);
    bi = (struct rt_brep_internal *)intern.idb_ptr;
    bi->magic = RT_BREP_INTERNAL_MAGIC;
    bi->brep =  ON_BrepSphere(sph);

    dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	rt_db_free_internal(&intern);
	bu_exit(1, "Test %ld: cannot add %s to directory\n", test_num, name);
    }
    if (rt_db_put_internal(dp, dbip, &intern) < 0) {
	rt_db_free_internal(&intern);
	bu_exit(1, "Test %ld: database write error, aborting\n", test_num);
    }
    rt_db_free_internal(&intern);
}

static void
cp_brep_sph(struct db_i *dbip, const char *oname, const char *nname, long int test_num)
{
    struct directory *odp, *ndp;
    struct bu_external external;
    int idb_type = ID_BREP;

    odp = db_lookup(dbip, oname, LOOKUP_QUIET);
    if (odp == RT_DIR_NULL) {
	bu_exit(1, "Test %ld: cannot copy %s - object not found\n", test_num, oname);
    }

    if (db_get_external(&external, odp, dbip)) {
	bu_exit(1, "Test %ld: db_get_external cannot get bu_external form of %s\n", test_num, oname);
    }

    ndp = db_diradd(dbip, nname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&idb_type);

    ndp->d_flags = odp->d_flags;

    if (db_put_external(&external, ndp, dbip) < 0) {
	bu_exit(1, "Test %ld: db_put_external cannot write %s to  %s\n", test_num, oname, nname);
    }

    bu_free_external(&external);
}

/* Make a comb with all the objects in obj_argv */
static void
add_comb(struct db_i *dbip, const char *name, int obj_argc, const char **obj_argv, long int test_num)
{
    int i;
    struct rt_db_internal intern;
    struct directory *dp;
    struct rt_comb_internal *comb;
    struct rt_tree_array *tree_list;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_meth = &OBJ[ID_COMBINATION];
    intern.idb_type = ID_COMBINATION;
    BU_ALLOC(intern.idb_ptr, struct rt_comb_internal);
    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_COMB_INTERNAL_INIT(comb);
    tree_list = (struct rt_tree_array *)bu_calloc(obj_argc, sizeof(struct rt_tree_array), "tree list");
    for (i = 0; i < obj_argc; i++) {
	union tree *tp;
	BU_GET(tp, union tree);
	RT_TREE_INIT(tp);
	tree_list[i].tl_op = OP_UNION;
	tree_list[i].tl_tree = tp;
	tp->tr_l.tl_op = OP_DB_LEAF;
	tp->tr_l.tl_name = bu_strdup(obj_argv[i]);
	tp->tr_l.tl_mat = (matp_t)NULL;

    }
    comb->tree = (union tree *)db_mkgift_tree(tree_list, obj_argc);
    dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_COMB, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	rt_db_free_internal(&intern);
	bu_exit(1, "Test %ld: cannot add %s to directory\n", test_num, name);
    }
    if (rt_db_put_internal(dp, dbip, &intern) < 0) {
	rt_db_free_internal(&intern);
	bu_exit(1, "Test %ld: Database write error creating comb, aborting\n", test_num);
    }
}

static size_t
cache_count(const char *cache_dir, int ignore_temp)
{
    size_t cache_objects = 0;
    struct bu_vls wpath = BU_VLS_INIT_ZERO;

    /* We need to find all cache objects */
    char **obj_dirs = NULL;
    bu_vls_sprintf(&wpath, "%s/objects", cache_dir);
    size_t objdir_cnt = bu_file_list(bu_vls_cstr(&wpath), "[a-zA-z0-9]*", &obj_dirs);
    for (size_t i = 0; i < objdir_cnt; i++) {
	/* Find and remove all files in the obj dir */
	bu_vls_sprintf(&wpath, "%s/objects/%s", cache_dir, obj_dirs[i]);
	if (!ignore_temp) {
	    cache_objects += bu_file_list(bu_vls_cstr(&wpath), "[a-zA-z0-9]*", NULL);
	} else {
	    char **objs = NULL;
	    size_t objs_cnt = bu_file_list(bu_vls_cstr(&wpath), "[a-zA-z0-9]*", &objs);
	    for (size_t j = 0; j < objs_cnt; j++) {
		// Temporary objects will have '.' chars in the name - skip them,
		// since another process might be working on one
		if (!strchr(objs[j], '.')) {
		    cache_objects++;
		}
	    }
	    bu_argv_free(objs_cnt, objs);
	}
    }
    bu_argv_free(objdir_cnt, obj_dirs);
    bu_vls_free(&wpath);
    return cache_objects;
}


static void
cache_cleanup(struct bu_vls *cache_dir_vls)
{
    const char *cache_dir = bu_vls_cstr(cache_dir_vls);
    struct bu_vls wpath = BU_VLS_INIT_ZERO;

    /* Zap the format file first (that's the easy one) */
    bu_vls_sprintf(&wpath, "%s/format", cache_dir);
    bu_file_delete(bu_vls_cstr(&wpath));

    /* Now, we need to find and eliminate any cache objects */
    char **obj_dirs = NULL;
    bu_vls_sprintf(&wpath, "%s/objects", cache_dir);
    size_t objdir_cnt = bu_file_list(bu_vls_cstr(&wpath), "[a-zA-z0-9]*", &obj_dirs);
    for (size_t i = 0; i < objdir_cnt; i++) {
	/* Find and remove all files in the obj dir */
	char **objs = NULL;
	bu_vls_sprintf(&wpath, "%s/objects/%s", cache_dir, obj_dirs[i]);
	size_t objs_cnt = bu_file_list(bu_vls_cstr(&wpath), "[a-zA-z0-9]*", &objs);
	for (size_t j = 0; j < objs_cnt; j++) {
	    bu_vls_sprintf(&wpath, "%s/objects/%s/%s", cache_dir, obj_dirs[i], objs[j]);
	    if (!bu_file_delete(bu_vls_cstr(&wpath))) {
		bu_exit(1, "Unable to remove the object %s\n", bu_vls_cstr(&wpath));
	    }
	}
	bu_argv_free(objs_cnt, objs);

	/* Emptied the dir, now remove it */
	bu_vls_sprintf(&wpath, "%s/objects/%s", cache_dir, obj_dirs[i]);
	if (!bu_file_delete(bu_vls_cstr(&wpath))) {
	    bu_exit(1, "Unable to remove the directory %s\n", bu_vls_cstr(&wpath));
	}
    }
    bu_argv_free(objdir_cnt, obj_dirs);

    /* That should be everything - remove the objects dir and the cache dir */
    bu_vls_sprintf(&wpath, "%s/objects", cache_dir);
    if (!bu_file_delete(bu_vls_cstr(&wpath))) {
	bu_exit(1, "Unable to remove the directory %s\n", bu_vls_cstr(&wpath));
    }
    if (!bu_file_delete(cache_dir)) {
	bu_exit(1, "Unable to remove the directory %s\n", bu_vls_cstr(&wpath));
    }

    bu_vls_free(&wpath);
}

static struct db_i *
create_test_g_file(long int test_num, const char *gfile)
{
    struct db_i *dbip = DBI_NULL;

    if (bu_file_exists(gfile, NULL)) {
	bu_exit(1, "Test %ld: Stale .g file %s exists\n", test_num, gfile);
    }

    dbip = db_create(gfile, BRLCAD_DB_FORMAT_LATEST);

    if (dbip == DBI_NULL) {
	bu_exit(1, "Test %ld: unable to create test file %s\n", test_num, gfile);
    }

    RT_CK_DBI(dbip);

    return dbip;
}

static struct rt_i *
build_rtip(long int test_num, const char *gfile, const char *objname, int stage_num, int do_parallel, int ncpus, struct resource *resp, int brep_depth = -1)
{
    struct rt_i *rtip = RTI_NULL;

    rtip = rt_dirbuild(gfile, NULL, 0);

    if (rtip == RTI_NULL) {
	bu_exit(1, "Test %ld: failed to rt_dirbuild in stage %d\n", test_num, stage_num);
    }

    if (brep_depth >= 0 && !_rt_brep_set_surface_tree_depth(rtip, brep_depth)) {
	bu_exit(1, "Test %ld: failed to set BREP depth %d in stage %d\n",
		test_num, brep_depth, stage_num);
    }

    if (!do_parallel) {
	if (rt_gettree(rtip, objname) < 0) {
	    bu_exit(1, "Test %ld: rt_getree in stage %d failed\n", test_num, stage_num);
	}
	rt_prep(rtip);
    } else {
	if (rt_gettrees(rtip, 1, (const char **)&objname, ncpus) < 0) {
	    bu_exit(1, "Test %ld: rt_getrees in stage %d failed\n", test_num, stage_num);
	}
	/* We're doing parallel prep, need one resource per thread */
	for (int i = 0; i < ncpus; i++) {
	    rt_init_resource(&resp[i], i, rtip);
	}
	rt_prep_parallel(rtip, ncpus);
    }

    return rtip;
}


static struct rt_brep_prep_stats
read_brep_prep_stats(long int test_num, struct rt_i *rtip, const char *objname,
	int expected_depth, int stage_num)
{
    struct rt_brep_prep_stats stats = {};
    struct soltab *stp = rt_find_solid(rtip, objname);

    if (!stp || !_rt_brep_prep_stats(stp, &stats)) {
	bu_exit(1, "Test %ld: cannot read BREP prep statistics in stage %d\n",
		test_num, stage_num);
    }
    if (stats.surface_tree_depth_limit != expected_depth) {
	bu_exit(1, "Test %ld: expected BREP depth %d in stage %d, found %d\n",
		test_num, expected_depth, stage_num,
		stats.surface_tree_depth_limit);
    }
    return stats;
}


static struct rt_brep_prep_stats
brep_prep_stats(long int test_num, struct rt_i *rtip, const char *objname,
	int expected_depth, int stage_num)
{
    const struct rt_brep_prep_stats stats = read_brep_prep_stats(test_num,
	rtip, objname, expected_depth, stage_num);
    if (!stats.bezier_spans || !stats.packed_surface_controls ||
	    stats.packed_surface_control_values !=
		4 * stats.packed_surface_controls) {
	bu_exit(1, "Test %ld: incomplete packed BREP controls in stage %d\n",
		test_num, stage_num);
    }
    if (!(stats.routing_capability_flags &
	    RT_BREP_ROUTE_CAP_PACKED_CONTROLS) ||
	    !(stats.routing_risk_flags & RT_BREP_ROUTE_RISK_UNQUALIFIED) ||
	    !stats.routing_unqualified_faces ||
	    stats.routing_feature_cells != stats.surface_tree_leaves ||
	    !stats.routing_correction_cells ||
	    !stats.routing_feature_cell_span_bindings ||
	    stats.routing_complete_cell_span_packs >
		stats.routing_cell_span_packs ||
	    (stats.routing_complete_cell_span_packs &&
	     (!stats.routing_cell_span_control_intervals ||
	      !stats.routing_cell_span_subpatch_boxes)) ||
	    stats.routing_cell_span_pack_used_bytes >
		stats.routing_cell_span_pack_budget_bytes ||
	    !stats.routing_correction_bvh_nodes) {
	bu_exit(1, "Test %ld: incomplete BREP routing metadata in stage %d\n",
		test_num, stage_num);
    }
    const size_t retained_sum = stats.retained_core_bytes +
	stats.retained_brep_bytes + stats.retained_surface_tree_bytes +
	stats.retained_curve_tree_bytes + stats.retained_edge_bytes +
	stats.retained_surface_bytes + stats.retained_hierarchy_bytes +
	stats.retained_routing_bytes + stats.retained_vertex_bytes;
    const size_t retained_unused_sum =
	stats.retained_surface_tree_unused_capacity_bytes +
	stats.retained_curve_tree_unused_capacity_bytes +
	stats.retained_edge_unused_capacity_bytes +
	stats.retained_surface_unused_capacity_bytes +
	stats.retained_hierarchy_unused_capacity_bytes +
	stats.retained_routing_unused_capacity_bytes +
	stats.retained_vertex_unused_capacity_bytes;
    const size_t retained_routing_unused_sum =
	stats.retained_routing_feature_cell_unused_capacity_bytes +
	stats.retained_routing_binding_unused_capacity_bytes +
	stats.retained_routing_pack_unused_capacity_bytes +
	stats.retained_routing_control_unused_capacity_bytes +
	stats.retained_routing_subpatch_unused_capacity_bytes +
	stats.retained_routing_bvh_unused_capacity_bytes;
    if (!stats.retained_bytes || retained_sum != stats.retained_bytes ||
	    retained_unused_sum != stats.retained_unused_capacity_bytes ||
	    retained_routing_unused_sum !=
		stats.retained_routing_unused_capacity_bytes ||
	    stats.retained_routing_binding_unused_capacity_bytes ||
	    stats.retained_routing_pack_unused_capacity_bytes ||
	    stats.retained_unused_capacity_bytes > stats.retained_bytes ||
	    stats.retained_used_bytes != stats.retained_bytes -
		stats.retained_unused_capacity_bytes) {
	bu_exit(1, "Test %ld: inconsistent BREP retained-byte accounting in "
	    "stage %d\n", test_num, stage_num);
    }
    return stats;
}


static void
check_cached_brep_stats(long int test_num,
	const struct rt_brep_prep_stats &fresh,
	const struct rt_brep_prep_stats &cached, int stage_num)
{
    if (cached.surface_tree_build_microseconds != 0 ||
	    cached.surface_tree_maximum_depth != fresh.surface_tree_maximum_depth ||
	    cached.surface_tree_nodes != fresh.surface_tree_nodes ||
	    cached.surface_tree_leaves != fresh.surface_tree_leaves ||
	    cached.curve_trees != fresh.curve_trees ||
	    cached.curve_tree_leaves != fresh.curve_tree_leaves ||
	    cached.bezier_spans != fresh.bezier_spans ||
	    cached.packed_surface_controls != fresh.packed_surface_controls ||
	    cached.packed_surface_control_values !=
		fresh.packed_surface_control_values ||
	    cached.routing_capability_flags != fresh.routing_capability_flags ||
	    cached.routing_risk_flags != fresh.routing_risk_flags ||
	    cached.routing_seeded_faces != fresh.routing_seeded_faces ||
	    cached.routing_seeded_spans != fresh.routing_seeded_spans ||
	    cached.routing_unqualified_faces != fresh.routing_unqualified_faces ||
	    cached.routing_unsupported_faces != fresh.routing_unsupported_faces ||
	    cached.routing_reparameterized_faces !=
		fresh.routing_reparameterized_faces ||
	    cached.routing_singular_spans != fresh.routing_singular_spans ||
	    cached.routing_periodic_faces != fresh.routing_periodic_faces ||
	    cached.routing_inner_trim_faces != fresh.routing_inner_trim_faces ||
	    cached.routing_boundary_faces != fresh.routing_boundary_faces ||
	    cached.routing_nonmanifold_faces != fresh.routing_nonmanifold_faces ||
	    cached.routing_seam_faces != fresh.routing_seam_faces ||
	    cached.routing_feature_cells != fresh.routing_feature_cells ||
	    cached.routing_seeded_cells != fresh.routing_seeded_cells ||
	    cached.routing_unsupported_cells !=
		fresh.routing_unsupported_cells ||
	    cached.routing_trim_boundary_cells !=
		fresh.routing_trim_boundary_cells ||
	    cached.routing_domain_boundary_cells !=
		fresh.routing_domain_boundary_cells ||
	    cached.routing_correction_cells != fresh.routing_correction_cells ||
	    cached.routing_feature_cell_span_bindings !=
		fresh.routing_feature_cell_span_bindings ||
	    cached.routing_cell_span_packs != fresh.routing_cell_span_packs ||
	    cached.routing_complete_cell_span_packs !=
		fresh.routing_complete_cell_span_packs ||
	    cached.routing_cell_span_control_intervals !=
		fresh.routing_cell_span_control_intervals ||
	    cached.routing_cell_span_subpatch_boxes !=
		fresh.routing_cell_span_subpatch_boxes ||
	    cached.routing_cell_span_pack_budget_bytes !=
		fresh.routing_cell_span_pack_budget_bytes ||
	    cached.routing_cell_span_pack_used_bytes !=
		fresh.routing_cell_span_pack_used_bytes ||
	    cached.routing_correction_bvh_nodes !=
		fresh.routing_correction_bvh_nodes) {
	bu_log("Test %ld: BREP cache stats stage %d "
	    "build=%zu/%zu depth=%d/%d nodes=%zu/%zu leaves=%zu/%zu "
	    "curves=%zu/%zu curve-leaves=%zu/%zu spans=%zu/%zu "
	    "controls=%zu/%zu values=%zu/%zu flags=%u/%u risks=%u/%u "
	    "faces=%zu/%zu seeded-spans=%zu/%zu cells=%zu/%zu "
	    "seeded-cells=%zu/%zu unsupported-cells=%zu/%zu "
	    "trim-cells=%zu/%zu domain-cells=%zu/%zu correction=%zu/%zu "
	    "bindings=%zu/%zu packs=%zu/%zu complete=%zu/%zu "
	    "intervals=%zu/%zu subpatches=%zu/%zu bvh=%zu/%zu\n",
	    test_num, stage_num,
	    fresh.surface_tree_build_microseconds,
	    cached.surface_tree_build_microseconds,
	    fresh.surface_tree_maximum_depth,
	    cached.surface_tree_maximum_depth,
	    fresh.surface_tree_nodes, cached.surface_tree_nodes,
	    fresh.surface_tree_leaves, cached.surface_tree_leaves,
	    fresh.curve_trees, cached.curve_trees,
	    fresh.curve_tree_leaves, cached.curve_tree_leaves,
	    fresh.bezier_spans, cached.bezier_spans,
	    fresh.packed_surface_controls, cached.packed_surface_controls,
	    fresh.packed_surface_control_values,
	    cached.packed_surface_control_values,
	    fresh.routing_capability_flags, cached.routing_capability_flags,
	    fresh.routing_risk_flags, cached.routing_risk_flags,
	    fresh.routing_seeded_faces, cached.routing_seeded_faces,
	    fresh.routing_seeded_spans, cached.routing_seeded_spans,
	    fresh.routing_feature_cells, cached.routing_feature_cells,
	    fresh.routing_seeded_cells, cached.routing_seeded_cells,
	    fresh.routing_unsupported_cells, cached.routing_unsupported_cells,
	    fresh.routing_trim_boundary_cells,
	    cached.routing_trim_boundary_cells,
	    fresh.routing_domain_boundary_cells,
	    cached.routing_domain_boundary_cells,
	    fresh.routing_correction_cells, cached.routing_correction_cells,
	    fresh.routing_feature_cell_span_bindings,
	    cached.routing_feature_cell_span_bindings,
	    fresh.routing_cell_span_packs, cached.routing_cell_span_packs,
	    fresh.routing_complete_cell_span_packs,
	    cached.routing_complete_cell_span_packs,
	    fresh.routing_cell_span_control_intervals,
	    cached.routing_cell_span_control_intervals,
	    fresh.routing_cell_span_subpatch_boxes,
	    cached.routing_cell_span_subpatch_boxes,
	    fresh.routing_correction_bvh_nodes,
	    cached.routing_correction_bvh_nodes);
	bu_exit(1, "Test %ld: cached BREP prep statistics differ in stage %d\n",
		test_num, stage_num);
    }
}


static void
check_brep_payload_corruption_rejected(long int test_num,
	struct rt_i *rtip, const char *objname)
{
    struct soltab *source = rt_find_solid(rtip, objname);
    struct directory *dp = db_lookup(rtip->rti_dbip, objname, LOOKUP_QUIET);
    struct rt_db_internal intern;
    struct bu_external payload = BU_EXTERNAL_INIT_ZERO;
    size_t version = (size_t)-1;
    if (!source || dp == RT_DIR_NULL ||
	    rt_db_get_internal(&intern, dp, rtip->rti_dbip, NULL) < 0)
	bu_exit(1, "Test %ld: cannot prepare BREP corruption input\n",
	    test_num);
    if (rt_obj_prep_serialize(source, &intern, &payload, &version) ||
	    payload.ext_nbytes < SIZEOF_NETWORK_DOUBLE || !payload.ext_buf) {
	rt_db_free_internal(&intern);
	bu_exit(1, "Test %ld: cannot serialize BRP6 corruption input\n",
	    test_num);
    }

    /* Routing feature cells are the final BRP6 records.  Changing the least
     * significant byte of the last UV-domain endpoint preserves a readable
     * payload while requiring exact reconstruction validation to reject it. */
    payload.ext_buf[payload.ext_nbytes - 1] ^= 1u;
    struct soltab corrupted = RT_SOLTAB_INIT_ZERO;
    corrupted.l.magic = RT_SOLTAB_MAGIC;
    BU_LIST_INIT(&corrupted.l2);
    corrupted.st_rtip = rtip;
    corrupted.st_dp = source->st_dp;
    corrupted.st_meth = source->st_meth;
    corrupted.st_id = source->st_id;
    if (!rt_obj_prep_serialize(&corrupted, &intern, &payload, &version) ||
	    corrupted.st_specific) {
	bu_free_external(&payload);
	rt_db_free_internal(&intern);
	bu_exit(1, "Test %ld: corrupt BRP6 routing cell was accepted\n",
	    test_num);
    }
    bu_free_external(&payload);
    rt_db_free_internal(&intern);
}


static void
add_brep_tor(struct db_i *dbip, const char *name, point_t *v,
    double major_radius, double minor_radius, long int test_num)
{
    struct directory *dp;
    struct rt_db_internal intern;
    const ON_Plane plane(ON_3dPoint(*v), ON_3dVector::ZAxis);
    const ON_Torus torus(plane, major_radius, minor_radius);
    ON_RevSurface *revolution = torus.RevSurfaceForm(NULL);
    if (revolution) {
	revolution->SetDomain(0, 0.0, 2.0 * ON_PI * major_radius);
	revolution->SetDomain(1, 0.0, 2.0 * ON_PI * minor_radius);
    }

	/* Exercise reconstruction of the derived status-2 PolyCurve piece table
	 * and independently converted prepared spans, not only the direct-arc
	 * profile path. */
    ON_Curve *source = revolution ? revolution->m_curve : NULL;
    if (revolution)
	revolution->m_curve = NULL;
    ON_PolyCurve *profile = source ? new ON_PolyCurve(4) : NULL;
    for (int segment_index = 0; profile && segment_index < 4;
	    ++segment_index) {
	const ON_Interval source_domain = source->Domain();
	const ON_Interval piece_domain(source_domain.ParameterAt(
	    0.25 * segment_index), source_domain.ParameterAt(
	    0.25 * (segment_index + 1)));
	ON_Curve *segment = source->DuplicateCurve();
	if (!segment || !segment->Trim(piece_domain)) {
	    delete segment;
	    delete profile;
	    profile = NULL;
	    break;
	}
	if (segment_index % 2) {
	    ON_NurbsCurve *nurbs = segment->NurbsCurve();
	    delete segment;
	    segment = nurbs;
	}
	if (!segment || !profile->Append(segment)) {
	    delete segment;
	    delete profile;
	    profile = NULL;
	    break;
	}
    }
    delete source;
    if (profile && (!profile->SetDomain(-37.0, 91.0) ||
	    !profile->IsValid())) {
	delete profile;
	profile = NULL;
    }
    if (revolution)
	revolution->m_curve = profile;
    ON_Brep *brep = revolution ?
	ON_BrepRevSurface(revolution, false, false, NULL) : NULL;
    if (!brep && revolution)
	delete revolution;
    if (!brep)
	bu_exit(1, "Test %ld: cannot construct BREP torus %s\n", test_num,
	    name);

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BREP;
    intern.idb_meth = &OBJ[ID_BREP];
    BU_ALLOC(intern.idb_ptr, struct rt_brep_internal);
    struct rt_brep_internal *bi =
	(struct rt_brep_internal *)intern.idb_ptr;
    bi->magic = RT_BREP_INTERNAL_MAGIC;
    bi->brep = brep;

    dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID,
	(void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	rt_db_free_internal(&intern);
	bu_exit(1, "Test %ld: cannot add %s to directory\n", test_num,
	    name);
    }
    if (rt_db_put_internal(dp, dbip, &intern) < 0) {
	rt_db_free_internal(&intern);
	bu_exit(1, "Test %ld: database write error, aborting\n", test_num);
    }
    rt_db_free_internal(&intern);
}


static int
test_brep_unsupported_cell_cache(long int test_num, const char *gfile,
	const char *objname)
{
    const int depths[] = {3, BREP_MAX_FT_DEPTH};
    const size_t depth_count = sizeof(depths) / sizeof(depths[0]);
    struct rt_brep_prep_stats fresh[depth_count] = {};
    struct bu_vls cache_dir = BU_VLS_INIT_ZERO;
    struct resource resp = {};

    bu_vls_sprintf(&cache_dir, "%s_dir_%ld_unsupported", RTC_PREFIX,
	test_num);
    const char *cache_path = bu_dir(NULL, 0, BU_DIR_CURR,
	bu_vls_cstr(&cache_dir), NULL);
    if (bu_file_exists(cache_path, NULL))
	bu_exit(1, "Test %ld: stale test cache directory %s exists\n",
		test_num, cache_path);
    if (bu_setenv("LIBRT_CACHE", cache_path, 1) != 0)
	bu_exit(1, "Test %ld: failed to set LIBRT_CACHE\n", test_num);

    for (size_t i = 0; i < depth_count; ++i) {
	const int stage = (int)i + 1;
	struct rt_i *rtip = build_rtip(test_num, gfile, objname, stage, 0, 1,
	    &resp, depths[i]);
	fresh[i] = read_brep_prep_stats(test_num, rtip, objname, depths[i],
	    stage);
	if (!fresh[i].surface_tree_build_microseconds ||
		!fresh[i].routing_unsupported_cells ||
		fresh[i].routing_feature_cells != fresh[i].surface_tree_leaves) {
	    bu_exit(1, "Test %ld: imported BREP depth %d did not exercise "
		"unsupported routing-cell construction\n", test_num,
		depths[i]);
	}
	if (cache_count(bu_vls_cstr(&cache_dir), 0) != i + 1)
	    bu_exit(1, "Test %ld: imported BREP depth %d did not create its "
		"cache variant\n", test_num, depths[i]);
	rt_clean(rtip);
	rt_i_destroy(rtip);
    }

    for (size_t i = 0; i < depth_count; ++i) {
	const int stage = (int)(depth_count + i + 1);
	struct rt_i *rtip = build_rtip(test_num, gfile, objname, stage, 0, 1,
	    &resp, depths[i]);
	const struct rt_brep_prep_stats cached = read_brep_prep_stats(test_num,
	    rtip, objname, depths[i], stage);
	check_cached_brep_stats(test_num, fresh[i], cached, stage);
	if (!cached.routing_unsupported_cells ||
		cache_count(bu_vls_cstr(&cache_dir), 0) != depth_count) {
	    bu_exit(1, "Test %ld: imported BREP depth %d cache reload was "
		"incomplete\n", test_num, depths[i]);
	}
	rt_clean(rtip);
	rt_i_destroy(rtip);
    }

    cache_cleanup(&cache_dir);
    bu_vls_free(&cache_dir);
    bu_log("Test %ld: PASSED\n", test_num);
    return 0;
}


static int
test_brep_depth_variants(long int test_num)
{
    const int shallow_depth = 3;
    const int deep_depth = BREP_MAX_FT_DEPTH;
    const char *objname = "tor_0.s";
    struct bu_vls cache_dir = BU_VLS_INIT_ZERO;
    struct bu_vls gfile = BU_VLS_INIT_ZERO;
    struct db_i *dbip;
    point_t v = VINIT_ZERO;
    struct resource resp = {};

    bu_vls_sprintf(&cache_dir, "%s_dir_%ld_depth", RTC_PREFIX, test_num);
    bu_vls_sprintf(&gfile, "%s_%ld_depth.g", RTC_PREFIX, test_num);
    const char *cache_path = bu_dir(NULL, 0, BU_DIR_CURR,
	bu_vls_cstr(&cache_dir), NULL);
    if (bu_setenv("LIBRT_CACHE", cache_path, 1) != 0)
	bu_exit(1, "Test %ld: failed to set LIBRT_CACHE\n", test_num);
    if (bu_file_exists(cache_path, NULL))
	bu_exit(1, "Test %ld: stale test cache directory %s exists\n",
		test_num, cache_path);

    dbip = create_test_g_file(test_num, bu_vls_cstr(&gfile));
    add_brep_tor(dbip, objname, &v, 3.0, 1.0, test_num);
    db_close(dbip);

    struct rt_i *rtip = build_rtip(test_num, bu_vls_cstr(&gfile), objname,
	1, 0, 1, &resp, shallow_depth);
    const struct rt_brep_prep_stats shallow_fresh = brep_prep_stats(test_num,
	rtip, objname, shallow_depth, 1);
    if (!(shallow_fresh.routing_capability_flags &
	    RT_BREP_ROUTE_CAP_DEGREE_THEOREM) ||
	    !shallow_fresh.routing_feature_cell_span_bindings)
	bu_exit(1, "Test %ld: shallow status-2 interval routing was not "
	    "prepared\n", test_num);
    check_brep_payload_corruption_rejected(test_num, rtip, objname);
    if (cache_count(bu_vls_cstr(&cache_dir), 0) != 1)
	bu_exit(1, "Test %ld: shallow prep did not create one cache object\n",
		test_num);
    rt_clean(rtip);
    rt_i_destroy(rtip);

    rtip = build_rtip(test_num, bu_vls_cstr(&gfile), objname,
	2, 0, 1, &resp, deep_depth);
    const struct rt_brep_prep_stats deep_fresh = brep_prep_stats(test_num,
	rtip, objname, deep_depth, 2);
    if (!(deep_fresh.routing_capability_flags &
	    RT_BREP_ROUTE_CAP_DEGREE_THEOREM) ||
	    !deep_fresh.routing_feature_cell_span_bindings)
	bu_exit(1, "Test %ld: deep status-2 interval routing was not "
	    "prepared\n", test_num);
    if (cache_count(bu_vls_cstr(&cache_dir), 0) != 2)
	bu_exit(1, "Test %ld: depth variants did not create two cache objects\n",
		test_num);
    rt_clean(rtip);
    rt_i_destroy(rtip);

    rtip = build_rtip(test_num, bu_vls_cstr(&gfile), objname,
	3, 0, 1, &resp, shallow_depth);
    const struct rt_brep_prep_stats shallow_cached = brep_prep_stats(test_num,
	rtip, objname, shallow_depth, 3);
    check_cached_brep_stats(test_num, shallow_fresh, shallow_cached, 3);
    rt_clean(rtip);
    rt_i_destroy(rtip);

    rtip = build_rtip(test_num, bu_vls_cstr(&gfile), objname,
	4, 0, 1, &resp, deep_depth);
    const struct rt_brep_prep_stats deep_cached = brep_prep_stats(test_num,
	rtip, objname, deep_depth, 4);
    check_cached_brep_stats(test_num, deep_fresh, deep_cached, 4);
    if (cache_count(bu_vls_cstr(&cache_dir), 0) != 2)
	bu_exit(1, "Test %ld: cache reload changed the variant object count\n",
		test_num);
    rt_clean(rtip);
    rt_i_destroy(rtip);

    cache_cleanup(&cache_dir);
    bu_file_delete(bu_vls_cstr(&gfile));
    bu_vls_free(&cache_dir);
    bu_vls_free(&gfile);
    bu_log("Test %ld: PASSED\n", test_num);
    return 0;
}


static int
test_brep_depth_subprocess(int ac, char *av[])
{
    long int test_num;
    long int process_num;
    int depth;
    const char *gfile;
    const char *cache_dir;
    const char *objname;
    struct resource resp = {};

    if (ac != 6)
	bu_exit(1, "rt_cache BREP depth subprocess invoked incorrectly");

    sscanf(av[0], "%ld", &test_num);
    sscanf(av[1], "%ld", &process_num);
    sscanf(av[2], "%d", &depth);
    gfile = av[3];
    cache_dir = av[4];
    objname = av[5];

    if (bu_setenv("LIBRT_CACHE",
	    bu_dir(NULL, 0, BU_DIR_CURR, cache_dir, NULL), 1) != 0)
	bu_exit(1, "Test %ld(process %ld): failed to set LIBRT_CACHE\n",
	    test_num, process_num);

    /* The first prep races other writers for one of two depth-qualified
     * names.  The second must be able to consume the completed object without
     * observing a partial payload or the other depth's payload. */
    struct rt_i *first = build_rtip(test_num, gfile, objname,
	process_num * 1000 + 1, 0, 1, &resp, depth);
    const struct rt_brep_prep_stats first_stats = brep_prep_stats(test_num,
	first, objname, depth, process_num * 1000 + 1);
    rt_clean(first);
    rt_i_destroy(first);

    struct rt_i *second = build_rtip(test_num, gfile, objname,
	process_num * 1000 + 2, 0, 1, &resp, depth);
    const struct rt_brep_prep_stats second_stats = brep_prep_stats(test_num,
	second, objname, depth, process_num * 1000 + 2);
    check_cached_brep_stats(test_num, first_stats, second_stats,
	process_num * 1000 + 2);
    rt_clean(second);
    rt_i_destroy(second);

    bu_log("Test %ld(process %ld, depth %d): PASSED\n", test_num,
	process_num, depth);
    return 0;
}

static int
test_subprocess(int ac, char *av[])
{
    long int test_num;
    long int process_num;
    long int expected;
    struct rt_i *rtip_stage_1, *rtip_stage_2;
    const char *gfile;
    const char *cache_dir;
    const char *cname;
    size_t ncpus = (bu_avail_cpus() > MAX_PSW) ? MAX_PSW : bu_avail_cpus();
    struct resource *resp = (struct resource *)bu_calloc(ncpus+1, sizeof(struct resource), "resources");

    if (ac != 6) {
	bu_exit(1, "rt_cache subprocess command invoked incorrectly");
    }

    sscanf(av[0], "%ld", &test_num);
    sscanf(av[1], "%ld", &process_num);
    sscanf(av[2], "%ld", &expected);
    gfile = av[3];
    cache_dir = av[4];
    cname = av[5];

    bu_setenv("LIBRT_CACHE", bu_dir(NULL, 0, BU_DIR_CURR, cache_dir, NULL), 1);

    rtip_stage_1 = build_rtip(test_num, gfile, cname, process_num*1000 + 1, 1, (int)ncpus, resp);

    // Confirm the presence of the expected number of file(s) in the cache
    size_t cc = cache_count(cache_dir, 1);
    if (cc != (size_t)expected) {
	bu_exit(1, "Test %ld(process %ld): expected %ld cache object(s), found %zu\n", test_num, process_num, expected, cc);
    }

    rt_clean(rtip_stage_1);
    rt_i_destroy(rtip_stage_1);

    /*** Now, do it again with the cache definitely in place */
    rtip_stage_2 = build_rtip(test_num, gfile, cname, process_num*1000 + 2, 1, (int)ncpus, resp);
    rt_clean(rtip_stage_2);
    rt_i_destroy(rtip_stage_2);

    bu_log("Test %ld(process %ld): PASSED\n", test_num, process_num);

    bu_free(resp, "resp");

    return 0;
}

struct subprocess_data {
    char *pname;
    long int test_num;
    long int process_num;
    long int expected;
    char *gfile;
    char *cache_dir;
    char *cname;
    int brep_depth;
    int result;
    struct bu_vls *sd_result;
};

static void
subprocess_launcher(int id, void *data)
{
    char *line = (char *)bu_calloc(MAXPATHLEN+501, sizeof(char), "resultstr");
    const char *av[9];
    int ind = (!id) ? 0 : (id - 1);
    struct subprocess_data **sda = (struct subprocess_data **)data;
    struct subprocess_data *sd = sda[ind];
    struct bu_vls tn = BU_VLS_INIT_ZERO;
    struct bu_vls pn = BU_VLS_INIT_ZERO;
    struct bu_vls en = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&tn, "%ld", sd->test_num);
    bu_vls_sprintf(&pn, "%ld", sd->process_num);
    bu_vls_sprintf(&en, "%ld", sd->expected);

    av[0] = sd->pname;
    av[1] = "-1";
    av[2] = bu_vls_cstr(&tn);
    av[3] = bu_vls_cstr(&pn);
    av[4] = bu_vls_cstr(&en);
    av[5] = sd->gfile;
    av[6] = sd->cache_dir;
    av[7] = sd->cname;
    av[8] = NULL;

    sd->result = 0;

    struct bu_process *p = NULL;
    bu_process_create(&p, (const char **)av, BU_PROCESS_OUT_EQ_ERR);
    bu_vls_printf(sd->sd_result, "Test %ld(process %ld): running...\n", sd->test_num, sd->process_num);

    while (bu_process_read_n(p, BU_PROCESS_STDOUT, MAXPATHLEN+501, (char *)line) > 0) {
	bu_vls_printf(sd->sd_result, "%s\n", line);
	memset(line, 0, MAXPATHLEN+501);
    }

    if (bu_process_wait_n(&p, 120)) {
	bu_vls_printf(sd->sd_result, "Test %ld(process %ld): rt_cache_subprocess failed\n", sd->test_num, sd->process_num);
	sd->result = 1;
    }

    bu_free(line, "line");
}


static void
brep_depth_subprocess_launcher(int id, void *data)
{
    char *line = (char *)bu_calloc(MAXPATHLEN+501, sizeof(char), "resultstr");
    const char *av[9];
    int ind = (!id) ? 0 : (id - 1);
    struct subprocess_data **sda = (struct subprocess_data **)data;
    struct subprocess_data *sd = sda[ind];
    struct bu_vls tn = BU_VLS_INIT_ZERO;
    struct bu_vls pn = BU_VLS_INIT_ZERO;
    struct bu_vls dn = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&tn, "%ld", sd->test_num);
    bu_vls_sprintf(&pn, "%ld", sd->process_num);
    bu_vls_sprintf(&dn, "%d", sd->brep_depth);

    av[0] = sd->pname;
    av[1] = "-2";
    av[2] = bu_vls_cstr(&tn);
    av[3] = bu_vls_cstr(&pn);
    av[4] = bu_vls_cstr(&dn);
    av[5] = sd->gfile;
    av[6] = sd->cache_dir;
    av[7] = sd->cname;
    av[8] = NULL;

    sd->result = 0;
    struct bu_process *p = NULL;
    bu_process_create(&p, (const char **)av, BU_PROCESS_OUT_EQ_ERR);
    bu_vls_printf(sd->sd_result,
	"Test %ld(process %ld, depth %d): running...\n", sd->test_num,
	sd->process_num, sd->brep_depth);
    while (bu_process_read_n(p, BU_PROCESS_STDOUT, MAXPATHLEN+501,
	    (char *)line) > 0) {
	bu_vls_printf(sd->sd_result, "%s\n", line);
	memset(line, 0, MAXPATHLEN+501);
    }
    if (bu_process_wait_n(&p, 120)) {
	bu_vls_printf(sd->sd_result,
	    "Test %ld(process %ld, depth %d): rt_cache subprocess failed\n",
	    sd->test_num, sd->process_num, sd->brep_depth);
	sd->result = 1;
    }

    bu_vls_free(&tn);
    bu_vls_free(&pn);
    bu_vls_free(&dn);
    bu_free(line, "line");
}


static int
test_brep_concurrent_depth_variants(char *rp, long int test_num)
{
    const int shallow_depth = 3;
    const int deep_depth = BREP_MAX_FT_DEPTH;
    const int subprocess_count = 8;
    const char *objname = "sph_0.s";
    struct bu_vls cache_dir = BU_VLS_INIT_ZERO;
    struct bu_vls gfile = BU_VLS_INIT_ZERO;
    point_t v = VINIT_ZERO;
    struct resource resp = {};

    bu_vls_sprintf(&cache_dir, "%s_dir_%ld_depth_concurrent", RTC_PREFIX,
	test_num);
    bu_vls_sprintf(&gfile, "%s_%ld_depth_concurrent.g", RTC_PREFIX,
	test_num);
    const char *cache_path = bu_dir(NULL, 0, BU_DIR_CURR,
	bu_vls_cstr(&cache_dir), NULL);
    if (bu_file_exists(cache_path, NULL))
	bu_exit(1, "Test %ld: stale test cache directory %s exists\n",
	    test_num, cache_path);

    struct db_i *dbip = create_test_g_file(test_num, bu_vls_cstr(&gfile));
    add_brep_sph(dbip, objname, &v, 1.0, test_num);
    db_close(dbip);

    /* Establish independent, uncached references before racing writers. */
    if (bu_setenv("LIBRT_CACHE", "off", 1) != 0)
	bu_exit(1, "Test %ld: failed to disable LIBRT_CACHE\n", test_num);
    struct rt_i *rtip = build_rtip(test_num, bu_vls_cstr(&gfile), objname,
	1, 0, 1, &resp, shallow_depth);
    const struct rt_brep_prep_stats shallow_fresh = brep_prep_stats(test_num,
	rtip, objname, shallow_depth, 1);
    rt_clean(rtip);
    rt_i_destroy(rtip);
    rtip = build_rtip(test_num, bu_vls_cstr(&gfile), objname,
	2, 0, 1, &resp, deep_depth);
    const struct rt_brep_prep_stats deep_fresh = brep_prep_stats(test_num,
	rtip, objname, deep_depth, 2);
    rt_clean(rtip);
    rt_i_destroy(rtip);

    struct subprocess_data **sdata = (struct subprocess_data **)bu_calloc(
	subprocess_count, sizeof(struct subprocess_data *),
	"BREP depth launch data array");
    for (int i = 0; i < subprocess_count; ++i) {
	sdata[i] = (struct subprocess_data *)bu_calloc(1,
	    sizeof(struct subprocess_data), "BREP depth launch data");
	sdata[i]->pname = bu_strdup(rp);
	sdata[i]->test_num = test_num;
	sdata[i]->process_num = i;
	sdata[i]->gfile = bu_strdup(bu_vls_cstr(&gfile));
	sdata[i]->cache_dir = bu_strdup(bu_vls_cstr(&cache_dir));
	sdata[i]->cname = bu_strdup(objname);
	sdata[i]->brep_depth = (i % 2) ? deep_depth : shallow_depth;
	BU_GET(sdata[i]->sd_result, struct bu_vls);
	bu_vls_init(sdata[i]->sd_result);
    }
    bu_parallel(brep_depth_subprocess_launcher, subprocess_count,
	(void *)sdata);
    for (int i = 0; i < subprocess_count; ++i) {
	if (sdata[i]->result)
	    bu_exit(1, "Test %ld: depth subprocess failure:\n%s", test_num,
		bu_vls_cstr(sdata[i]->sd_result));
	bu_log("%s", bu_vls_cstr(sdata[i]->sd_result));
	bu_free(sdata[i]->pname, "program name");
	bu_free(sdata[i]->gfile, "gfile");
	bu_free(sdata[i]->cache_dir, "cache directory");
	bu_free(sdata[i]->cname, "object name");
	bu_vls_free(sdata[i]->sd_result);
	BU_PUT(sdata[i]->sd_result, struct bu_vls);
	bu_free(sdata[i], "BREP depth launch data");
    }
    bu_free(sdata, "BREP depth launch data array");

    if (cache_count(bu_vls_cstr(&cache_dir), 0) != 2)
	bu_exit(1, "Test %ld: concurrent depth prep did not leave exactly two "
	    "complete cache objects\n", test_num);

    /* Validate both published variants against uncached construction after all
     * writers have exited.  A depth alias or partial publication is therefore
     * observable even if an individual racing process safely fell back. */
    if (bu_setenv("LIBRT_CACHE", cache_path, 1) != 0)
	bu_exit(1, "Test %ld: failed to set LIBRT_CACHE\n", test_num);
    rtip = build_rtip(test_num, bu_vls_cstr(&gfile), objname,
	3, 0, 1, &resp, shallow_depth);
    const struct rt_brep_prep_stats shallow_cached = brep_prep_stats(test_num,
	rtip, objname, shallow_depth, 3);
    check_cached_brep_stats(test_num, shallow_fresh, shallow_cached, 3);
    rt_clean(rtip);
    rt_i_destroy(rtip);
    rtip = build_rtip(test_num, bu_vls_cstr(&gfile), objname,
	4, 0, 1, &resp, deep_depth);
    const struct rt_brep_prep_stats deep_cached = brep_prep_stats(test_num,
	rtip, objname, deep_depth, 4);
    check_cached_brep_stats(test_num, deep_fresh, deep_cached, 4);
    rt_clean(rtip);
    rt_i_destroy(rtip);
    if (cache_count(bu_vls_cstr(&cache_dir), 0) != 2)
	bu_exit(1, "Test %ld: concurrent cache reload changed the object count\n",
	    test_num);

    cache_cleanup(&cache_dir);
    bu_file_delete(bu_vls_cstr(&gfile));
    bu_vls_free(&cache_dir);
    bu_vls_free(&gfile);
    bu_log("Test %ld: PASSED\n", test_num);
    return 0;
}

static int
test_cache_payload_compression(long int test_num)
{
    const size_t source_bytes = 1024 * 1024;
    struct bu_external payload = BU_EXTERNAL_INIT_ZERO;
    payload.ext_nbytes = source_bytes;
    payload.ext_buf = (uint8_t *)bu_calloc(source_bytes, 1,
	"cache compression test payload");
    for (size_t i = 0; i < source_bytes; ++i)
	payload.ext_buf[i] = (uint8_t)(i % 17);

    if (!_rt_cache_compress_external(&payload) ||
	    payload.ext_nbytes <= sizeof(uint32_t) ||
	    payload.ext_nbytes >= source_bytes) {
	const size_t compressed_bytes = payload.ext_nbytes;
	bu_free_external(&payload);
	bu_exit(1, "Test %ld: cache payload did not use the actual LZ4 output "
	    "size (%zu source bytes, %zu stored bytes)\n", test_num,
	    source_bytes, compressed_bytes);
    }

    const size_t compressed_bytes = payload.ext_nbytes;
    bu_free_external(&payload);
    bu_log("Test %ld: PASSED (%zu source bytes, %zu compressed bytes)\n",
	test_num, source_bytes, compressed_bytes);
    return 0;
}


static void
cache_test_object_path(long int test_num, const char *cache_dir,
	struct bu_vls *object_path)
{
    char **obj_dirs = NULL;
    char **objects = NULL;
    struct bu_vls path = BU_VLS_INIT_ZERO;
    size_t objdir_cnt;
    size_t object_cnt;

    bu_vls_sprintf(&path, "%s/objects", cache_dir);
    objdir_cnt = bu_file_list(bu_vls_cstr(&path), "[a-zA-Z0-9]*",
	&obj_dirs);
    if (objdir_cnt != 1)
	bu_exit(1, "Test %ld: expected one cache object directory, found %zu\n",
	    test_num, objdir_cnt);

    bu_vls_sprintf(&path, "%s/objects/%s", cache_dir, obj_dirs[0]);
    object_cnt = bu_file_list(bu_vls_cstr(&path), "[a-zA-Z0-9]*",
	&objects);
    if (object_cnt != 1)
	bu_exit(1, "Test %ld: expected one cache object, found %zu\n",
	    test_num, object_cnt);

    bu_vls_sprintf(object_path, "%s/objects/%s/%s", cache_dir,
	obj_dirs[0], objects[0]);
    bu_argv_free(object_cnt, objects);
    bu_argv_free(objdir_cnt, obj_dirs);
    bu_vls_free(&path);
}


static void
cache_test_read_file(long int test_num, const char *path,
	struct bu_external *external)
{
    FILE *fp;
    long int file_bytes = bu_file_size(path);

    BU_EXTERNAL_INIT(external);
    if (file_bytes <= 0)
	bu_exit(1, "Test %ld: cannot size cache object %s\n", test_num, path);
    external->ext_nbytes = (size_t)file_bytes;
    external->ext_buf = (uint8_t *)bu_malloc(external->ext_nbytes,
	"cache test file");
    fp = fopen(path, "rb");
    if (!fp || fread(external->ext_buf, 1, external->ext_nbytes, fp) !=
	    external->ext_nbytes) {
	if (fp)
	    fclose(fp);
	bu_free_external(external);
	bu_exit(1, "Test %ld: cannot read cache object %s\n", test_num, path);
    }
    fclose(fp);
}


static void
cache_test_write_file(long int test_num, const char *path,
	const struct bu_external *external, size_t bytes)
{
    FILE *fp;

    if (!external || !external->ext_buf || !bytes ||
	    bytes > external->ext_nbytes)
	bu_exit(1, "Test %ld: invalid cache test write for %s\n", test_num,
	    path);
    fp = fopen(path, "wb");
    if (!fp)
	bu_exit(1, "Test %ld: cannot open cache object %s for writing\n",
	    test_num, path);
    if (fwrite(external->ext_buf, 1, bytes, fp) != bytes) {
	fclose(fp);
	bu_exit(1, "Test %ld: cannot write cache object %s\n", test_num,
	    path);
    }
    if (fclose(fp))
	bu_exit(1, "Test %ld: cannot close cache object %s\n", test_num,
	    path);
}


static uint8_t *
cache_test_attribute_value(struct bu_external *attributes, const char *name)
{
    uint8_t *cursor = attributes->ext_buf;
    uint8_t *limit = cursor + attributes->ext_nbytes;

    while (cursor < limit && *cursor) {
	char *attribute_name = (char *)cursor;
	uint8_t *terminator = (uint8_t *)memchr(cursor, '\0',
	    (size_t)(limit - cursor));
	if (!terminator)
	    return NULL;
	cursor = terminator + 1;
	if (cursor >= limit)
	    return NULL;
	uint8_t *value = cursor;
	terminator = (uint8_t *)memchr(cursor, '\0',
	    (size_t)(limit - cursor));
	if (!terminator)
	    return NULL;
	if (BU_STR_EQUAL(attribute_name, name))
	    return value;
	cursor = terminator + 1;
    }
    return NULL;
}


static void
cache_test_refresh_payload_hash(long int test_num,
	struct db5_raw_internal *raw_internal)
{
    uint8_t *hash_value = cache_test_attribute_value(
	&raw_internal->attributes, "rt_cache::payload_hash128");
    const bu_h128_t hash = bu_data_hash128(raw_internal->body.ext_buf,
	raw_internal->body.ext_nbytes);
    char encoded[33] = {0};

    if (!hash_value || strlen((const char *)hash_value) != 32)
	bu_exit(1, "Test %ld: cannot locate cache payload hash attribute\n",
	    test_num);
    snprintf(encoded, sizeof(encoded), "%016llx%016llx",
	(unsigned long long)hash.w[0], (unsigned long long)hash.w[1]);
    memcpy(hash_value, encoded, 32);
}


enum cache_corruption_kind {
    CACHE_TRUNCATED_DB5 = 0,
    CACHE_INVALID_OUTPUT_SIZE,
    CACHE_INVALID_LZ4,
    CACHE_MALFORMED_ATTRIBUTES,
    CACHE_WRONG_PREP_VERSION,
    CACHE_WRONG_EMBEDDED_NAME,
    CACHE_PAYLOAD_HASH_MISMATCH,
    CACHE_CORRUPTION_COUNT
};


static int
test_cache_corruption_recovery(long int test_num)
{
    const char *objname = "sph_0.s";
    const char *mutation_names[CACHE_CORRUPTION_COUNT] = {
	"truncated DB5 object",
	"invalid declared output size",
	"invalid LZ4 stream",
	"malformed attributes",
	"wrong prep version",
	"wrong embedded object name",
	"payload hash mismatch"
    };
    struct bu_vls cache_dir = BU_VLS_INIT_ZERO;
    struct bu_vls cache_path = BU_VLS_INIT_ZERO;
    struct bu_vls object_path = BU_VLS_INIT_ZERO;
    struct bu_vls gfile = BU_VLS_INIT_ZERO;
    struct bu_external valid = BU_EXTERNAL_INIT_ZERO;
    struct resource resp = {};
    point_t v = VINIT_ZERO;

    bu_vls_sprintf(&cache_dir, "%s_dir_%ld_corrupt", RTC_PREFIX, test_num);
    bu_vls_sprintf(&gfile, "%s_%ld_corrupt.g", RTC_PREFIX, test_num);
    bu_vls_strcpy(&cache_path, bu_dir(NULL, 0, BU_DIR_CURR,
	bu_vls_cstr(&cache_dir), NULL));
    if (bu_file_exists(bu_vls_cstr(&cache_path), NULL))
	bu_exit(1, "Test %ld: stale test cache directory %s exists\n",
	    test_num, bu_vls_cstr(&cache_path));
    if (bu_setenv("LIBRT_CACHE", bu_vls_cstr(&cache_path), 1) != 0)
	bu_exit(1, "Test %ld: failed to set LIBRT_CACHE\n", test_num);

    struct db_i *dbip = create_test_g_file(test_num, bu_vls_cstr(&gfile));
    add_brep_sph(dbip, objname, &v, 1.0, test_num);
    db_close(dbip);

    struct rt_i *rtip = build_rtip(test_num, bu_vls_cstr(&gfile), objname,
	1, 0, 1, &resp);
    const struct rt_brep_prep_stats initial = brep_prep_stats(test_num,
	rtip, objname, BREP_MAX_FT_DEPTH, 1);
    if (!initial.surface_tree_build_microseconds)
	bu_exit(1, "Test %ld: initial BREP prep unexpectedly used cache\n",
	    test_num);
    rt_clean(rtip);
    rt_i_destroy(rtip);

    cache_test_object_path(test_num, bu_vls_cstr(&cache_dir), &object_path);
    cache_test_read_file(test_num, bu_vls_cstr(&object_path), &valid);

    for (int mutation = 0; mutation < CACHE_CORRUPTION_COUNT; ++mutation) {
	struct bu_external corrupt = BU_EXTERNAL_INIT_ZERO;
	struct db5_raw_internal raw_internal;
	size_t written_bytes = valid.ext_nbytes;
	const int fresh_stage = 2 + mutation * 2;
	const int cached_stage = fresh_stage + 1;

	corrupt.ext_nbytes = valid.ext_nbytes;
	corrupt.ext_buf = (uint8_t *)bu_malloc(corrupt.ext_nbytes,
	    "corrupt cache test object");
	memcpy(corrupt.ext_buf, valid.ext_buf, corrupt.ext_nbytes);
	if (db5_get_raw_internal_ptr(&raw_internal, corrupt.ext_buf) == NULL)
	    bu_exit(1, "Test %ld: cannot parse valid cache object\n", test_num);

	switch (mutation) {
	    case CACHE_TRUNCATED_DB5:
		written_bytes--;
		break;
	    case CACHE_INVALID_OUTPUT_SIZE: {
		/* UINT32_MAX has the same byte representation in either order. */
		uint32_t invalid_size = UINT32_MAX;
		memcpy(raw_internal.body.ext_buf, &invalid_size,
		    sizeof(invalid_size));
		cache_test_refresh_payload_hash(test_num, &raw_internal);
		break;
	    }
	    case CACHE_INVALID_LZ4:
		memset(raw_internal.body.ext_buf + SIZEOF_NETWORK_LONG, 0xff,
		    raw_internal.body.ext_nbytes - SIZEOF_NETWORK_LONG);
		cache_test_refresh_payload_hash(test_num, &raw_internal);
		break;
	    case CACHE_MALFORMED_ATTRIBUTES:
		raw_internal.attributes.ext_buf[
		    raw_internal.attributes.ext_nbytes - 1] = 'X';
		break;
	    case CACHE_WRONG_PREP_VERSION: {
		uint8_t *version = cache_test_attribute_value(
		    &raw_internal.attributes, "rt_cache::version");
		if (!version || !*version)
		    bu_exit(1, "Test %ld: cannot locate prep version attribute\n",
			test_num);
		version[0] = version[0] == '0' ? '1' : '0';
		break;
	    }
	    case CACHE_WRONG_EMBEDDED_NAME:
		raw_internal.name.ext_buf[0] =
		    raw_internal.name.ext_buf[0] == '0' ? '1' : '0';
		break;
	    case CACHE_PAYLOAD_HASH_MISMATCH:
		raw_internal.body.ext_buf[SIZEOF_NETWORK_LONG] ^= 0x01;
		break;
	    default:
		bu_exit(1, "Test %ld: unknown cache corruption case\n",
		    test_num);
	}

	cache_test_write_file(test_num, bu_vls_cstr(&object_path), &corrupt,
	    written_bytes);
	rtip = build_rtip(test_num, bu_vls_cstr(&gfile), objname,
	    fresh_stage, 0, 1, &resp);
	const struct rt_brep_prep_stats fresh = brep_prep_stats(test_num,
	    rtip, objname, BREP_MAX_FT_DEPTH, fresh_stage);
	if (!fresh.surface_tree_build_microseconds)
	    bu_exit(1, "Test %ld: %s was accepted instead of rebuilt\n",
		test_num, mutation_names[mutation]);
	rt_clean(rtip);
	rt_i_destroy(rtip);

	if (cache_count(bu_vls_cstr(&cache_dir), 0) != 1 ||
		bu_file_size(bu_vls_cstr(&object_path)) !=
		(long int)valid.ext_nbytes)
	    bu_exit(1, "Test %ld: %s was not replaced in place\n",
		test_num, mutation_names[mutation]);

	struct bu_external healed = BU_EXTERNAL_INIT_ZERO;
	cache_test_read_file(test_num, bu_vls_cstr(&object_path), &healed);
	if (healed.ext_nbytes == written_bytes &&
		!memcmp(healed.ext_buf, corrupt.ext_buf, written_bytes))
	    bu_exit(1, "Test %ld: %s remained after fresh prep\n", test_num,
		mutation_names[mutation]);
	bu_free_external(&healed);
	bu_free_external(&corrupt);

	/* A second prep must consume the repaired object, proving that recovery
	 * did not merely fall back while leaving a poisoned same-name file. */
	rtip = build_rtip(test_num, bu_vls_cstr(&gfile), objname,
	    cached_stage, 0, 1, &resp);
	const struct rt_brep_prep_stats cached = brep_prep_stats(test_num,
	    rtip, objname, BREP_MAX_FT_DEPTH, cached_stage);
	check_cached_brep_stats(test_num, fresh, cached, cached_stage);
	rt_clean(rtip);
	rt_i_destroy(rtip);
	bu_log("Test %ld: recovered from %s\n", test_num,
	    mutation_names[mutation]);
    }

    bu_free_external(&valid);
    cache_cleanup(&cache_dir);
    bu_file_delete(bu_vls_cstr(&gfile));
    bu_vls_free(&cache_dir);
    bu_vls_free(&cache_path);
    bu_vls_free(&object_path);
    bu_vls_free(&gfile);
    bu_log("Test %ld: PASSED\n", test_num);
    return 0;
}


static int
test_cache_format_migration(long int test_num)
{
    const char *objname = "sph_0.s";
    struct bu_vls cache_dir = BU_VLS_INIT_ZERO;
    struct bu_vls cache_path = BU_VLS_INIT_ZERO;
    struct bu_vls format_path = BU_VLS_INIT_ZERO;
    struct bu_vls gfile = BU_VLS_INIT_ZERO;
    struct resource resp = {};
    point_t v = VINIT_ZERO;
    int format = 0;

    bu_vls_sprintf(&cache_dir, "%s_dir_%ld_migrate", RTC_PREFIX, test_num);
    bu_vls_sprintf(&gfile, "%s_%ld_migrate.g", RTC_PREFIX, test_num);
    bu_vls_strcpy(&cache_path, bu_dir(NULL, 0, BU_DIR_CURR,
	bu_vls_cstr(&cache_dir), NULL));
    if (bu_file_exists(bu_vls_cstr(&cache_path), NULL))
	bu_exit(1, "Test %ld: stale test cache directory %s exists\n",
	    test_num, bu_vls_cstr(&cache_path));
    if (bu_setenv("LIBRT_CACHE", bu_vls_cstr(&cache_path), 1) != 0)
	bu_exit(1, "Test %ld: failed to set LIBRT_CACHE\n", test_num);

    struct db_i *dbip = create_test_g_file(test_num, bu_vls_cstr(&gfile));
    add_brep_sph(dbip, objname, &v, 1.0, test_num);
    db_close(dbip);

    struct rt_i *rtip = build_rtip(test_num, bu_vls_cstr(&gfile), objname,
	1, 0, 1, &resp);
    (void)brep_prep_stats(test_num, rtip, objname, BREP_MAX_FT_DEPTH, 1);
    rt_clean(rtip);
    rt_i_destroy(rtip);
    if (cache_count(bu_vls_cstr(&cache_dir), 0) != 1)
	bu_exit(1, "Test %ld: failed to establish cache object\n", test_num);

    bu_vls_sprintf(&format_path, "%s/format", bu_vls_cstr(&cache_dir));
    FILE *fp = fopen(bu_vls_cstr(&format_path), "wb");
    if (!fp)
	bu_exit(1, "Test %ld: cannot open old cache format marker\n",
	    test_num);
    if (fprintf(fp, "3\n") <= 0) {
	fclose(fp);
	bu_exit(1, "Test %ld: cannot write old cache format marker\n",
	    test_num);
    }
    if (fclose(fp))
	bu_exit(1, "Test %ld: cannot close old cache format marker\n",
	    test_num);

    rtip = build_rtip(test_num, bu_vls_cstr(&gfile), objname,
	2, 0, 1, &resp);
    const struct rt_brep_prep_stats migrated = brep_prep_stats(test_num,
	rtip, objname, BREP_MAX_FT_DEPTH, 2);
    if (!migrated.surface_tree_build_microseconds)
	bu_exit(1, "Test %ld: format-3 object was not invalidated\n", test_num);
    rt_clean(rtip);
    rt_i_destroy(rtip);

    fp = fopen(bu_vls_cstr(&format_path), "rb");
    if (!fp)
	bu_exit(1, "Test %ld: cannot read migrated cache format marker\n",
	    test_num);
    if (fscanf(fp, "%d", &format) != 1) {
	fclose(fp);
	bu_exit(1, "Test %ld: cache did not migrate from format 3 to 4\n",
	    test_num);
    }
    if (fclose(fp) || format != 4)
	bu_exit(1, "Test %ld: cache did not migrate from format 3 to 4\n",
	    test_num);
    if (cache_count(bu_vls_cstr(&cache_dir), 0) != 1)
	bu_exit(1, "Test %ld: migrated cache did not contain one object\n",
	    test_num);

    rtip = build_rtip(test_num, bu_vls_cstr(&gfile), objname,
	3, 0, 1, &resp);
    const struct rt_brep_prep_stats cached = brep_prep_stats(test_num,
	rtip, objname, BREP_MAX_FT_DEPTH, 3);
    check_cached_brep_stats(test_num, migrated, cached, 3);
    rt_clean(rtip);
    rt_i_destroy(rtip);

    cache_cleanup(&cache_dir);
    bu_file_delete(bu_vls_cstr(&gfile));
    bu_vls_free(&cache_dir);
    bu_vls_free(&cache_path);
    bu_vls_free(&format_path);
    bu_vls_free(&gfile);
    bu_log("Test %ld: PASSED\n", test_num);
    return 0;
}


/* Core test routine.  Check that the cache object is created, that only the
 * correct number of object(s) is/are created, and that a second rtip can be
 * successfully created after the cache is initialized. */
static int
test_cache(char *rp, long int test_num, long int obj_cnt, int do_parallel, int different_content, int subprocess_cnt, int comb_cnt)
{
    struct bu_vls cache_dir = BU_VLS_INIT_ZERO;
    struct bu_vls gfile = BU_VLS_INIT_ZERO;
    struct rt_i *rtip_stage_1, *rtip_stage_2;
    struct db_i *dbip;
    const char *oname_root = "sph_";
    struct bu_vls cname = BU_VLS_INIT_ZERO;
    point_t v = VINIT_ZERO;
    double r = 1.0;
    size_t ncpus = 1;
    if (do_parallel) {
	ncpus =	(bu_avail_cpus() > MAX_PSW) ? MAX_PSW : bu_avail_cpus();
    }

    bu_vls_sprintf(&cache_dir, "%s_dir_%ld_%ld", RTC_PREFIX, test_num, obj_cnt);
    bu_vls_sprintf(&gfile, "%s_%ld_%ld.g", RTC_PREFIX, test_num, obj_cnt);

    const char *cache_path = bu_dir(NULL, 0, BU_DIR_CURR, bu_vls_cstr(&cache_dir), NULL);
    if (bu_setenv("LIBRT_CACHE", cache_path, 1) != 0)
	bu_exit(1, "Test %ld: failed to set LIBRT_CACHE\n", test_num);

    if (bu_file_exists(cache_path, NULL)) {
	bu_exit(1, "Test %ld: stale test cache directory %s exists\n", test_num, cache_path);
    }

    dbip = create_test_g_file(test_num, bu_vls_cstr(&gfile));

    bu_vls_sprintf(&cname, "sph_0.s");

    // Unit sphere at the origin
    add_brep_sph(dbip, bu_vls_cstr(&cname), &v, r, test_num);

    // If we're doing multiple objects, put them in a comb
    if (obj_cnt > 1) {
	int oc = obj_cnt;
	char **ov = (char **)bu_calloc(oc+1, sizeof(char  *), "object array");

	ov[0] = bu_strdup("sph_0.s");

	for (long int i = 1; i < obj_cnt; i++) {
	    bu_vls_sprintf(&cname, "%s%ld.s", oname_root, i);
	    if (different_content) {
		// For different content, make a series of larger spheres along
		// the Y axis
		r = 2 * r;
		v[1] = v[1] + (2*r);
		bu_vls_sprintf(&cname, "%s%ld.s", oname_root, i);
		add_brep_sph(dbip, bu_vls_cstr(&cname), &v, r, test_num);
	    } else {
		// For same content, exactly duplicate the original sphere
		// (don't want new UUID from openNURBS in data structure, for
		// example, so we can't just make more unit spheres - need to
		// specifically copy original) */
		cp_brep_sph(dbip, ov[0], bu_vls_cstr(&cname), test_num);
	    }
	    ov[i] = bu_strdup(bu_vls_cstr(&cname));
	}
	bu_vls_sprintf(&cname, "%s%ld.c", oname_root, test_num);

	add_comb(dbip, bu_vls_cstr(&cname), oc, (const char **)ov, test_num);

	if (comb_cnt > 0) {
	    struct bu_vls obj_comb = BU_VLS_INIT_ZERO;
	    char **ovc = (char **)bu_calloc(comb_cnt+1, sizeof(char  *), "object array");
	    const char *ocname = NULL;
	    bu_vls_sprintf(&obj_comb, "%s", bu_vls_cstr(&cname));
	    ocname = bu_vls_cstr(&obj_comb);

	    for (long int i = 0; i < comb_cnt; i++) {
		bu_vls_sprintf(&cname, "%s%ld_child%ld.c", oname_root, test_num, i);
		ovc[i] = bu_strdup(bu_vls_cstr(&cname));
		add_comb(dbip, bu_vls_cstr(&cname), 1, (const char **)&ocname, test_num);
	    }
	    bu_vls_sprintf(&cname, "%s%ld-top.c", oname_root, test_num);
	    add_comb(dbip, bu_vls_cstr(&cname), comb_cnt, (const char **)ovc, test_num);

	    for (long int i = 0; i < comb_cnt; i++) {
		bu_free(ovc[i], "free string");
	    }
	    bu_free(ovc, "free string array");
	    ovc = NULL;
	}

	for (long int i = 0; i < obj_cnt; i++) {
	    bu_free(ov[i], "free string");
	}
	bu_free(ov, "free string array");
	ov = NULL;
    }

    db_close(dbip);

    if (!subprocess_cnt) {
	struct resource *resp = (struct resource *)bu_calloc(ncpus+1, sizeof(struct resource), "resources");
	rtip_stage_1 = build_rtip(test_num, bu_vls_cstr(&gfile), bu_vls_cstr(&cname), 1, do_parallel, (int)ncpus, resp);

	// Confirm the presence of the expected number of file(s) in the cache
	size_t cc = cache_count(bu_vls_cstr(&cache_dir), 0);
	long int expected = (different_content) ? obj_cnt : 1;
	if (cc != (size_t)expected) {
	    bu_exit(1, "Test %ld: expected %ld cache object(s), found %zu\n", test_num, expected, cc);
	}

	rt_clean(rtip_stage_1);
	rt_i_destroy(rtip_stage_1);

	/*** Now, do it again with the cache in place */
	rtip_stage_2 = build_rtip(test_num, bu_vls_cstr(&gfile), bu_vls_cstr(&cname), 2, do_parallel, (int)ncpus, resp);
	rt_clean(rtip_stage_2);
	rt_i_destroy(rtip_stage_2);
	bu_free(resp, "resp");
    } else {
	long int expected = (different_content) ? obj_cnt : 1;
	struct subprocess_data **sdata = (struct subprocess_data **)bu_calloc(subprocess_cnt, sizeof(struct subprocess_data *), "launch data array");
	for (long int i = 0; i < subprocess_cnt; i++) {
	    sdata[i] = (struct subprocess_data *)bu_calloc(1, sizeof(struct subprocess_data), "launch data");
	    sdata[i]->pname = bu_strdup(rp);
	    sdata[i]->test_num = test_num;
	    sdata[i]->process_num = i;
	    sdata[i]->expected = expected;
	    sdata[i]->gfile = bu_strdup(bu_vls_cstr(&gfile));
	    sdata[i]->cache_dir = bu_strdup(bu_vls_cstr(&cache_dir));
	    sdata[i]->cname = bu_strdup(bu_vls_cstr(&cname));
	    BU_GET(sdata[i]->sd_result, struct bu_vls);
	    bu_vls_init(sdata[i]->sd_result);
	}
	bu_parallel(subprocess_launcher, subprocess_cnt, (void *)sdata);
	for (long int i = 0; i < subprocess_cnt; i++) {
	    if (sdata[i]->result) {
		bu_exit(1, "Test %ld: subprocess failure:\n%s", test_num, bu_vls_cstr(sdata[i]->sd_result));
	    } else {
		bu_log("%s", bu_vls_cstr(sdata[i]->sd_result));
	    }
	    bu_free(sdata[i]->pname, "gfile");
	    bu_free(sdata[i]->gfile, "gfile");
	    bu_free(sdata[i]->cache_dir, "cache_dir");
	    bu_free(sdata[i]->cname, "cname");
	    bu_vls_free(sdata[i]->sd_result);
	    BU_PUT(sdata[i]->sd_result, struct bu_vls);
	}
    }

    /* All done - scrub out the temporary cache */
    cache_cleanup(&cache_dir);

    /* Clear the .g file */
    bu_file_delete(bu_vls_cstr(&gfile));

    bu_vls_free(&cache_dir);
    bu_vls_free(&gfile);
    bu_vls_free(&cname);
    return 0;
}


const char *rt_cache_test_usage =
"Usage: rt_cache 1             (Single object serial test)\n"
"       rt_cache 2             (Single object parallel test)\n"
"       rt_cache 3 [obj_count] (Multiple identical object serial test)\n"
"       rt_cache 4 [obj_count] (Multiple identical object parallel test)\n"
"       rt_cache 5 [obj_count] (Multiple distinct object serial test)\n"
"       rt_cache 6 [obj_count] (Multiple distinct object parallel test)\n"
"       rt_cache 7 [obj_count] (Multiple distinct objects, multiple instances in tree parallel test)\n"
"       rt_cache 8             (BREP depth-aware cache variants test)\n"
"       rt_cache 9             (Concurrent BREP depth-aware cache stress test)\n"
"       rt_cache 10 file.g obj (Imported unsupported-cell depth-cache test)\n"
"       rt_cache 11            (Exact compressed-payload size test)\n"
"       rt_cache 12            (Corrupt-entry rejection and repair test)\n"
"       rt_cache 13            (Format-3 to format-4 migration test)\n"
"       rt_cache 20 [obj_count] [subprocess_count] (Multiple process identical objects test)\n"
"       rt_cache 21 [obj_count] [subprocess_count] (Multiple process distinct objects test)\n";

int
main(int ac, char *av[])
{
    long int subprocess_cnt = 2;
    long int obj_cnt = 1;
    long int test_num = 0;
    char *rp = bu_file_realpath(av[0], NULL);
    size_t maxcpus = (bu_avail_cpus() > MAX_PSW) ? MAX_PSW : bu_avail_cpus();

    bu_setprogname(av[0]);

    ac--; av++;

    if (ac < 1) {
	bu_exit(1, "%s", rt_cache_test_usage);
    }

    sscanf(av[0], "%ld", &test_num);

    if (test_num == -1) {
	ac--; av++;
	return test_subprocess(ac, av);
    }
    if (test_num == -2) {
	ac--; av++;
	return test_brep_depth_subprocess(ac, av);
    }

    if (ac > 3) {
	bu_exit(1, "%s", rt_cache_test_usage);
    }

    if (test_num < 3 && ac > 1) {
	bu_exit(1, "%s", rt_cache_test_usage);
    }

    if (((test_num >= 3 && test_num <= 7) || test_num >= 20) && ac >= 2) {
	sscanf(av[1], "%ld", &obj_cnt);
    }

    if (test_num > 19 && ac >= 3) {
	sscanf(av[2], "%ld", &subprocess_cnt);

	// TODO - see if we can use the bu_debug |= BU_DEBUG_PARALLEL trick from
	// the libbu semaphore.c unit test to push this harder... - we may not
	// need to be capped at the CPU count

	if (subprocess_cnt > (long int)maxcpus) {
	    bu_log("Requested %ld subprocesses, but only %zd CPUs available - throttling to %zd\n", subprocess_cnt, maxcpus, maxcpus);
	    subprocess_cnt = (long int)maxcpus;
	}
    }

    switch (test_num) {
	case 1:
	    /* Serial prep API, 1 object */
	    return test_cache(rp, test_num, 1, 0, 0, 0, 0);
	    break;
	case 2:
	    /* Parallel prep API, 1 object */
	    return test_cache(rp, test_num, 1, 1, 0, 0, 0);
	    break;
	case 3:
	    /* Serial prep API, multiple objects, identical content */
	    return test_cache(rp, test_num, obj_cnt, 0, 0, 0, 0);
	    break;
	case 4:
	    /* Parallel prep API, multiple objects, identical content */
	    return test_cache(rp, test_num, obj_cnt, 1, 0, 0, 0);
	    break;
	case 5:
	    /* Serial prep API, multiple objects, different content */
	    return test_cache(rp, test_num, obj_cnt, 0, 1, 0, 0);
	    break;
	case 6:
	    /* Parallel prep API, multiple objects, different content */
	    return test_cache(rp, test_num, obj_cnt, 1, 1, 0, 0);
	case 7:
	    /* Parallel prep API, multiple objects, non-unique content, multiple instances in tree */
	    return test_cache(rp, test_num, obj_cnt, 1, 1, 0, 5);
	case 8:
	    /* Distinct BREP prep depths coexist and reload exactly. */
	    return test_brep_depth_variants(test_num);
	case 9:
	    /* Race same-object shallow and deep BRP6 publication and reload. */
	    return test_brep_concurrent_depth_variants(rp, test_num);
	case 10:
	    /* Reload a real imported BREP containing unsupported leaf cells. */
	    if (ac != 3)
		bu_exit(1, "%s", rt_cache_test_usage);
	    return test_brep_unsupported_cell_cache(test_num, av[1], av[2]);
	case 11:
	    /* Store only the bytes actually produced by LZ4. */
	    return test_cache_payload_compression(test_num);
	case 12:
	    /* Reject malformed cache layers, replace, and reload the repair. */
	    return test_cache_corruption_recovery(test_num);
	case 13:
	    /* Invalidate and rebuild pre-integrity format-3 cache objects. */
	    return test_cache_format_migration(test_num);
	case 20:
	    /* Multiple objects, same content, multi-process */
	    return test_cache(rp, test_num, obj_cnt, 1, 0, subprocess_cnt, 0);
	case 21:
	    /* Multiple objects, different content, multi-process */
	    return test_cache(rp, test_num, obj_cnt, 1, 1, subprocess_cnt, 0);
	default:
	    break;
    }

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

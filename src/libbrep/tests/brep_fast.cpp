/*                         B R E P _ F A S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */

#include "common.h"

#include <cstring>
#include <vector>

#include "brep/cdt.h"
#include "bv/vlist.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/brep.h"

struct fast_output {
    std::vector<int> faces;
    std::vector<fastf_t> normals;
    std::vector<fastf_t> points;
    struct brep_cdt_fast_report report = {};
};

struct wire_output {
    std::vector<int> commands;
    std::vector<fastf_t> points;
    struct rt_brep_draw_report report = {};
};

static int
run_wire(wire_output *output, struct rt_db_internal *intern, size_t workers,
	size_t max_points, size_t max_working_bytes = 0)
{
    struct bu_list vhead;
    BU_LIST_INIT(&vhead);
    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct rt_brep_draw_options options;
    rt_brep_draw_options_default(&options);
    options.max_workers = workers;
    options.max_points = max_points;
    if (max_working_bytes)
	options.max_working_bytes = max_working_bytes;

    int ret = rt_brep_plot_ex(&vhead, intern, &ttol, &tol, NULL, &options,
	&output->report);
    const struct bv_vlist *vlist;
    for (BU_LIST_FOR(vlist, bv_vlist, &vhead)) {
	for (size_t i = 0; i < vlist->nused; i++) {
	    output->commands.push_back(vlist->cmd[i]);
	    output->points.insert(output->points.end(), vlist->pt[i],
		vlist->pt[i] + 3);
	}
    }
    BV_FREE_VLIST(&rt_vlfree, &vhead);
    return ret;
}

static int
run_shaded(wire_output *output, struct directory *dp,
	struct rt_db_internal *intern)
{
    struct bu_list vhead;
    BU_LIST_INIT(&vhead);
    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    int ret = rt_brep_plot_poly(&vhead, dp, intern, &ttol, &tol, NULL);
    const struct bv_vlist *vlist;
    for (BU_LIST_FOR(vlist, bv_vlist, &vhead)) {
	for (size_t i = 0; i < vlist->nused; i++) {
	    output->commands.push_back(vlist->cmd[i]);
	    output->points.insert(output->points.end(), vlist->pt[i],
		vlist->pt[i] + 3);
	}
    }
    BV_FREE_VLIST(&rt_vlfree, &vhead);
    return ret;
}

static int
run_fast(fast_output *output, const ON_Brep *brep, size_t workers,
	size_t max_points, size_t max_working_bytes = 0,
	size_t max_triangles = 0, bool adaptive_quality = true)
{
    int *faces = NULL;
    int face_count = 0;
    vect_t *normals = NULL;
    point_t *points = NULL;
    int point_count = 0;
    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct brep_cdt_fast_options options;
    brep_cdt_fast_options_default(&options);
    options.max_workers = workers;
    options.max_points = max_points;
    if (max_working_bytes)
	options.max_working_bytes = max_working_bytes;
    if (max_triangles)
	options.max_triangles = max_triangles;
    options.adaptive_quality = adaptive_quality ? 1 : 0;

    int ret = brep_cdt_fast_ex(&faces, &face_count, &normals, &points,
	&point_count, brep, -1, &ttol, &tol, &options, &output->report);
    if (faces)
	output->faces.assign(faces, faces + (size_t)face_count * 3);
    if (normals) {
	const fastf_t *values = (const fastf_t *)normals;
	output->normals.assign(values, values + (size_t)face_count * 9);
    }
    if (points) {
	const fastf_t *values = (const fastf_t *)points;
	output->points.assign(values, values + (size_t)point_count * 3);
    }

    bu_free(faces, "fast test faces");
    bu_free(normals, "fast test normals");
    bu_free(points, "fast test points");
    return ret;
}

int
main(int argc, const char **argv)
{
    if (argc != 3)
	return 2;

    struct db_i *dbip = db_open(argv[1], DB_OPEN_READONLY);
    if (dbip == DBI_NULL || db_dirbuild(dbip) < 0)
	return 2;
    struct directory *dp = db_lookup(dbip, argv[2], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	db_close(dbip);
	return 2;
    }

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0 ||
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	db_close(dbip);
	return 2;
    }
    struct rt_brep_internal *bi = (struct rt_brep_internal *)intern.idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    std::vector<ON_Interval> domains;
    for (int i = 0; i < bi->brep->m_F.Count(); i++) {
	const ON_Surface *surface = bi->brep->m_F[i].SurfaceOf();
	domains.push_back(surface->Domain(0));
	domains.push_back(surface->Domain(1));
    }
    std::vector<void *> trim_users;
    for (int i = 0; i < bi->brep->m_T.Count(); i++)
	trim_users.push_back(bi->brep->m_T[i].m_trim_user.p);

    fast_output serial;
    fast_output parallel;
    const size_t working_budget = (size_t)64 * 1024 * 1024;
    if (run_fast(&serial, bi->brep, 1, 16 * 1024 * 1024) !=
	    BREP_CDT_FAST_OK ||
	    run_fast(&parallel, bi->brep, 4, 16 * 1024 * 1024,
		working_budget) !=
	    BREP_CDT_FAST_OK) {
	rt_db_free_internal(&intern);
	db_close(dbip);
	return 1;
    }

    bool same = serial.faces == parallel.faces &&
	serial.normals == parallel.normals && serial.points == parallel.points;
    bool complete = serial.report.failed_faces == 0 &&
	parallel.report.failed_faces == 0 &&
	serial.report.completed_faces == bi->brep->m_F.Count() &&
	parallel.report.completed_faces == bi->brep->m_F.Count();
    bool working_bounded = parallel.report.peak_working_bytes > 0 &&
	parallel.report.peak_working_bytes <= working_budget;
    bool adaptive_reported = serial.report.triangle_budget > 0 &&
	serial.report.triangle_budget <= (size_t)256 * 1024 &&
	serial.report.refinement_passes == parallel.report.refinement_passes &&
	serial.report.approximated_faces ==
	parallel.report.approximated_faces;

    fast_output limited;
    int limit_ret = run_fast(&limited, bi->brep, 4, 1);
    bool limited_cleanly = limit_ret == BREP_CDT_FAST_LIMIT &&
	limited.report.hit_point_limit && limited.faces.empty() &&
	limited.normals.empty() && limited.points.empty();

    fast_output triangle_targeted;
    int triangle_target_ret = run_fast(&triangle_targeted, bi->brep, 4,
	16 * 1024 * 1024, 0, 1);
    bool authoritative_boundaries_retained =
	triangle_target_ret == BREP_CDT_FAST_OK &&
	triangle_targeted.report.triangle_budget == 1 &&
	triangle_targeted.report.triangle_budget_limited_faces > 0 &&
	!triangle_targeted.faces.empty();

    wire_output wire_serial;
    wire_output wire_parallel;
    int wire_serial_ret = run_wire(&wire_serial, &intern, 1,
	4 * 1024 * 1024);
    int wire_parallel_ret = run_wire(&wire_parallel, &intern, 4,
	4 * 1024 * 1024);
    bool wire_same = wire_serial_ret == RT_BREP_DRAW_OK &&
	wire_parallel_ret == RT_BREP_DRAW_OK &&
	wire_serial.commands == wire_parallel.commands &&
	wire_serial.points == wire_parallel.points &&
	!wire_serial.commands.empty();

    wire_output wire_limited;
    int wire_limit_ret = run_wire(&wire_limited, &intern, 4, 1);
    bool wire_limited_cleanly = wire_limit_ret == RT_BREP_DRAW_LIMIT &&
	wire_limited.report.hit_point_limit && wire_limited.commands.empty() &&
	wire_limited.points.empty();

    wire_output wire_approximated;
    int wire_approximated_ret = run_wire(&wire_approximated, &intern, 1,
	4 * 1024 * 1024, 1);
    bool wire_approximated_cleanly = true;
    if (wire_approximated.report.requested_surface_cues > 0) {
	wire_approximated_cleanly =
	    wire_approximated_ret == RT_BREP_DRAW_PARTIAL &&
	    wire_approximated.report.approximated_surface_cues > 0 &&
	    wire_approximated.report.completed_edges ==
		wire_approximated.report.requested_edges &&
	    !wire_approximated.report.hit_memory_limit &&
	    !wire_approximated.commands.empty();
    }

    wire_output shaded;
    int shaded_ret = run_shaded(&shaded, dp, &intern);
    std::vector<int> expected_commands;
    std::vector<fastf_t> expected_shaded_points;
    for (size_t triangle = 0; triangle < serial.faces.size() / 3;
	    triangle++) {
	const int *indices = &serial.faces[triangle * 3];
	const fastf_t *triangle_normals = &serial.normals[triangle * 9];
	expected_commands.push_back(BV_VLIST_TRI_START);
	expected_shaded_points.insert(expected_shaded_points.end(),
	    triangle_normals, triangle_normals + 3);
	for (int corner = 0; corner < 3; corner++) {
	    expected_commands.push_back(BV_VLIST_TRI_VERTNORM);
	    expected_shaded_points.insert(expected_shaded_points.end(),
		triangle_normals + corner * 3,
		triangle_normals + corner * 3 + 3);
	    expected_commands.push_back(corner ? BV_VLIST_TRI_DRAW :
		BV_VLIST_TRI_MOVE);
	    expected_shaded_points.insert(expected_shaded_points.end(),
		&serial.points[(size_t)indices[corner] * 3],
		&serial.points[(size_t)indices[corner] * 3 + 3]);
	}
	expected_commands.push_back(BV_VLIST_TRI_END);
	expected_shaded_points.insert(expected_shaded_points.end(),
	    &serial.points[(size_t)indices[0] * 3],
	    &serial.points[(size_t)indices[0] * 3 + 3]);
    }
    bool shaded_matches_fast = shaded_ret == BRLCAD_OK &&
	shaded.commands == expected_commands &&
	shaded.points == expected_shaded_points;

    bool unchanged = true;
    size_t domain_index = 0;
    for (int i = 0; i < bi->brep->m_F.Count(); i++) {
	const ON_Surface *surface = bi->brep->m_F[i].SurfaceOf();
	unchanged = unchanged &&
	    surface->Domain(0) == domains[domain_index++] &&
	    surface->Domain(1) == domains[domain_index++];
    }
    for (int i = 0; i < bi->brep->m_T.Count(); i++)
	unchanged = unchanged &&
	    bi->brep->m_T[i].m_trim_user.p == trim_users[(size_t)i];

    rt_db_free_internal(&intern);
    db_close(dbip);
    return (same && complete && working_bounded && adaptive_reported &&
	unchanged && limited_cleanly && authoritative_boundaries_retained &&
	wire_same && wire_limited_cleanly &&
	wire_approximated_cleanly &&
	shaded_matches_fast) ? 0 : 1;
}

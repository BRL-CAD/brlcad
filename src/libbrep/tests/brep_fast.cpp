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
#include "raytrace.h"
#include "rt/geom.h"

struct fast_output {
    std::vector<int> faces;
    std::vector<fastf_t> normals;
    std::vector<fastf_t> points;
    struct brep_cdt_fast_report report = {};
};

static int
run_fast(fast_output *output, const ON_Brep *brep, size_t workers,
	size_t max_points)
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
    if (run_fast(&serial, bi->brep, 1, 16 * 1024 * 1024) !=
	    BREP_CDT_FAST_OK ||
	    run_fast(&parallel, bi->brep, 4, 16 * 1024 * 1024) !=
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

    fast_output limited;
    int limit_ret = run_fast(&limited, bi->brep, 4, 1);
    bool limited_cleanly = limit_ret == BREP_CDT_FAST_LIMIT &&
	limited.report.hit_point_limit && limited.faces.empty() &&
	limited.normals.empty() && limited.points.empty();

    rt_db_free_internal(&intern);
    db_close(dbip);
    return (same && complete && unchanged && limited_cleanly) ? 0 : 1;
}

/*                 B R E P _ C D T _ C O N T R A C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */

#include "common.h"

#include <limits>
#include <vector>

#include "brep/cdt.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/brep.h"

struct mesh_output {
    std::vector<int> faces;
    std::vector<fastf_t> vertices;
};

static bool
mesh_get(mesh_output &output, struct ON_Brep_CDT_State *state)
{
    int *faces = NULL;
    fastf_t *vertices = NULL;
    int face_count = 0;
    int vertex_count = 0;
    if (ON_Brep_CDT_Mesh(&faces, &face_count, &vertices, &vertex_count,
	    NULL, NULL, NULL, NULL, state, 0, NULL) < 0)
	return false;
    output.faces.assign(faces, faces + (size_t)face_count * 3);
    output.vertices.assign(vertices, vertices + (size_t)vertex_count * 3);
    bu_free(faces, "contract test faces");
    bu_free(vertices, "contract test vertices");
    return true;
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
    struct rt_brep_internal *bi =
	(struct rt_brep_internal *)intern.idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(bi->brep,
	dp->d_namep);
    struct brep_cdt_diagnostic diagnostic;
    bool initial = ON_Brep_CDT_Status(state) == -1 &&
	ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0 &&
	diagnostic.result == BREP_CDT_RESULT_UNATTEMPTED;

    struct bg_tess_tol tolerance = BG_TESS_TOL_INIT_ZERO;
    tolerance.rel = 0.05;
    ON_Brep_CDT_Tol_Set(state, &tolerance);
    bool first_ok = ON_Brep_CDT_Tessellate(state, 0, NULL) == 0 &&
	ON_Brep_CDT_Status(state) == 0 &&
	ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0 &&
	diagnostic.result == BREP_CDT_RESULT_SUCCESS;
    mesh_output first;
    first_ok = first_ok && mesh_get(first, state) && !first.faces.empty();

    bool second_ok = ON_Brep_CDT_Tessellate(state, 0, NULL) == 0;
    mesh_output second;
    second_ok = second_ok && mesh_get(second, state) &&
	first.faces == second.faces && first.vertices == second.vertices;

    ON_Brep_CDT_Tol_Set(state, &tolerance);
    bool invalidated = ON_Brep_CDT_Status(state) == -1 &&
	ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0 &&
	diagnostic.result == BREP_CDT_RESULT_UNATTEMPTED;

    tolerance.abs = std::numeric_limits<fastf_t>::quiet_NaN();
    ON_Brep_CDT_Tol_Set(state, &tolerance);
    bool invalid_tolerance = ON_Brep_CDT_Tessellate(state, 0, NULL) == -1 &&
	ON_Brep_CDT_Status(state) == -3 &&
	ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0 &&
	diagnostic.result == BREP_CDT_RESULT_INVALID_TOLERANCE;

    tolerance = BG_TESS_TOL_INIT_ZERO;
    tolerance.rel = 0.05;
    ON_Brep_CDT_Tol_Set(state, &tolerance);
    int bad_face = bi->brep->m_F.Count();
    bool invalid_face = ON_Brep_CDT_Tessellate(state, 1, &bad_face) == -1 &&
	ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0 &&
	diagnostic.result == BREP_CDT_RESULT_INVALID_BREP;

    ON_Brep_CDT_Destroy(state);
    rt_db_free_internal(&intern);
    db_close(dbip);

    ON_Brep invalid_brep;
    state = ON_Brep_CDT_Create(&invalid_brep, "invalid");
    bool invalid_input = ON_Brep_CDT_Tessellate(state, 0, NULL) == -1 &&
	ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0 &&
	diagnostic.result == BREP_CDT_RESULT_INVALID_BREP &&
	diagnostic.stage == BREP_CDT_STAGE_TOPOLOGY;
    ON_Brep_CDT_Destroy(state);

    return initial && first_ok && second_ok && invalidated &&
	invalid_tolerance && invalid_face && invalid_input ? 0 : 1;
}


/*                     B O T _ V D B . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "common.h"

#include "bg/trimesh.h"
#include "bu.h"
#include "ged.h"
#include "raytrace.h"
#include "wdb.h"


static void
check(bool condition, const char *message)
{
    if (!condition)
	bu_exit(EXIT_FAILURE, "%s\n", message);
}


static size_t
bot_faces(struct ged *gedp, const char *name)
{
    struct directory *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    check(dp != RT_DIR_NULL, "Expected BoT was not written");

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    check(rt_db_get_internal(&intern, dp, gedp->dbip, NULL) == ID_BOT,
	"Could not read output BoT");

    struct rt_bot_internal *bot = (struct rt_bot_internal *)intern.idb_ptr;
    check(bg_trimesh_solid2(bot->num_vertices, bot->num_faces, bot->vertices,
	    bot->faces, NULL) == 0, "Output mesh is not closed");
    check(rt_bot_inside_out(bot) == 0, "Output mesh has incorrect winding");
    size_t faces = bot->num_faces;
    rt_db_free_internal(&intern);
    return faces;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    check(argc == 2, "Expected a path for the test database");
    struct rt_wdb *wdbp = wdb_fopen(argv[1]);
    check(wdbp != NULL, "Could not create test database");

    fastf_t vertices[] = {
	0, 0, 0, 100, 0, 0, 100, 100, 0, 0, 100, 0,
	0, 0, 100, 100, 0, 100, 100, 100, 100, 0, 100, 100
    };
    int faces[] = {
	0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
	1, 2, 6, 1, 6, 5, 2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7,
	0, 2, 1
    };
    const size_t box_vertices = sizeof(vertices) / sizeof(vertices[0]) / ELEMENTS_PER_POINT;
    const size_t box_faces = 12;
    fastf_t shell_vertices[2 * box_vertices * ELEMENTS_PER_POINT];
    int shell_faces[2 * box_faces * ELEMENTS_PER_POINT];
    for (size_t i = 0; i < box_vertices * ELEMENTS_PER_POINT; i++) {
	shell_vertices[i] = vertices[i];
	shell_vertices[box_vertices * ELEMENTS_PER_POINT + i] = 25.0 + 0.5 * vertices[i];
    }
    // Reverse the inner box to make it a sealed cavity.
    for (size_t face = 0; face < box_faces; face++) {
	size_t outer = face * ELEMENTS_PER_POINT;
	size_t inner = (box_faces + face) * ELEMENTS_PER_POINT;
	VMOVE(shell_faces + outer, faces + outer);
	VSET(shell_faces + inner, faces[outer] + box_vertices,
	    faces[outer + Z] + box_vertices, faces[outer + Y] + box_vertices);
    }
    check(mk_bot(wdbp, "shell.bot", RT_BOT_SOLID, RT_BOT_CCW, 0,
	    2 * box_vertices, 2 * box_faces, shell_vertices, shell_faces,
	    NULL, NULL) == 0, "Could not create the hollow box");
    // A duplicate triangle makes the second mesh non-manifold.
    check(mk_bot(wdbp, "duplicate.bot", RT_BOT_SOLID, RT_BOT_CCW, 0, 8, 13,
	    vertices, faces, NULL, NULL) == 0, "Could not create the test defect");
    wdb_close(wdbp);

    struct ged *gedp = ged_open("db", argv[1], 1);
    check(gedp != NULL, "Could not open test database");
    const char *remesh[] = {"bot", "remesh", "--vdb", "duplicate.bot", "fixed.bot"};
    int status = ged_exec_bot(gedp, sizeof(remesh) / sizeof(remesh[0]), remesh);
    check(status == BRLCAD_OK, bu_vls_cstr(gedp->ged_result_str));
    check(bot_faces(gedp, "fixed.bot") > 0, "Remeshing returned an empty mesh");

    const char *in_place[] = {"bot", "remesh", "--vdb", "duplicate.bot"};
    status = ged_exec_bot(gedp, sizeof(in_place) / sizeof(in_place[0]), in_place);
    check(status == BRLCAD_OK, bu_vls_cstr(gedp->ged_result_str));
    check(bot_faces(gedp, "duplicate.bot") > 0, "Remeshing did not persist in place");

    const char *exterior[] = {"bot", "exterior", "--flood-fill", "--voxel-size", "5",
	"shell.bot", "outer.bot"};
    status = ged_exec_bot(gedp, sizeof(exterior) / sizeof(exterior[0]), exterior);
    check(status == BRLCAD_OK, bu_vls_cstr(gedp->ged_result_str));
    check(bot_faces(gedp, "outer.bot") == box_faces, "Flood fill did not isolate the outside faces");
    ged_close(gedp);
    return EXIT_SUCCESS;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

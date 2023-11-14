/*                   T E S S E L L A T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @file libged/facetize/tessellate.cpp
 *
 * Primary management of the process of getting Manifold mesh inputs from other
 * BRL-CAD data.  Usually this just means calling ft_tessellate and translating
 * the result into a Manifold, but there are a variety other scenarios as well.
 */

#include "common.h"

#include <chrono>
#include <thread>

#include "bu/process.h"

#include "../ged_private.h"
#include "./ged_facetize.h"

int
manifold_tessellate(void **out, struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, void *data)
{
    if (!out || !tsp || !ip || !data)
	return -1;

    struct _ged_facetize_state *s = (struct _ged_facetize_state *)data;
    struct directory *dp = DB_PATH_CURR_DIR(pathp);
    char *path_str = db_path_to_string(pathp);
    bu_log("Tessellate %s\n", path_str);
    bu_free(path_str, "path string");

    // In order to control the process of generating a suitable input mesh,
    // we run the tessellation logic itself in a separate process.

    /* Build up a temp file path to use for writing out ip */
    // TODO - incorporate a text printing of the ip pointer address, so we have
    // instance uniqueness in the name as well in case we save this file for
    // later use...
    char tmpfil[MAXPATHLEN];
    bu_dir(tmpfil, MAXPATHLEN, BU_DIR_TEMP, bu_temp_file_name(NULL, 0), dp->d_namep, "_tess.g", NULL);

    // Create the temporary database
    struct db_i *dbip = db_create(tmpfil, BRLCAD_DB_FORMAT_LATEST);
    if (!dbip) {
	bu_log("Unable to create temp database %s\n", tmpfil);
	return -1;
    }

    // Write the object in question
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
    wdb_put_internal(wdbp, dp->d_namep, ip, 1.0);

    // Close the temporary .g file
    db_close(dbip);

    // Build up the path to the tessellation exec name
    char tess_exec[MAXPATHLEN];
    bu_dir(tess_exec, MAXPATHLEN, BU_DIR_BIN, "ged_tessellate", BU_DIR_EXT, NULL);

    // Build up the command to run
    struct bu_vls abs_str = BU_VLS_INIT_ZERO;
    struct bu_vls rel_str = BU_VLS_INIT_ZERO;
    struct bu_vls norm_str = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&abs_str, "%0.17f", tsp->ts_ttol->abs);
    bu_vls_sprintf(&rel_str, "%0.17f", tsp->ts_ttol->rel);
    bu_vls_sprintf(&norm_str, "%0.17f", tsp->ts_ttol->norm);
    char *tess_cmd[11] = {NULL};
    tess_cmd[0] = tess_exec;
    tess_cmd[1] = "--abs";
    tess_cmd[2] = bu_vls_cstr(&abs_str);
    tess_cmd[3] = "--rel";
    tess_cmd[4] = bu_vls_cstr(&rel_str);
    tess_cmd[5] = "--norm";
    tess_cmd[6] = bu_vls_cstr(&norm_str);
    tess_cmd[7] = tmpfil;
    tess_cmd[8] = dp->d_namep;
    tess_cmd[9] = "--default";

    // There are a number of methods that can be tried.  We try them in priority
    // order, timing out if one of them goes too long.
    int rc = 0;
    int method_flags = s->method_flags;
    while (rc) {
	if (!method_flags)
	    break;
	bool have_method = false;
	if (!have_method && method_flags & FACETIZE_NMG) {
	    method_flags = method_flags & ~(FACETIZE_NMG);
	    tess_cmd[9] = "--default";
	    have_method = true;
	}
	if (!have_method && method_flags & FACETIZE_CONTINUATION) {
	    method_flags = method_flags & ~(FACETIZE_CONTINUATION);
	    tess_cmd[9] = "--CM";
	    have_method = true;
	}
	if (!have_method && method_flags & FACETIZE_SPSR) {
	    method_flags = method_flags & ~(FACETIZE_SPSR);
	    tess_cmd[9] = "--SPSR";
	}

	int aborted = 0, timeout = 0;
	int64_t start = bu_gettime();
	int64_t elapsed = 0;
	fastf_t seconds = 0.0;
	struct bu_process *p = NULL;
	bu_process_exec(&p, tess_cmd[0], 10, tess_cmd, 0, 0);
	while (p && (bu_process_pid(p) != -1)) {
	    std::this_thread::sleep_for(std::chrono::milliseconds(100));
	    elapsed = bu_gettime() - start;
	    seconds = elapsed / 1000000.0;
	    if (seconds > s->max_time) {
		bu_terminate(bu_process_pid(p));
		timeout = 1;
	    }
	}
	int w_rc = bu_process_wait(&aborted, p, 0);
	rc = (timeout) ? -1 : w_rc;
    }

    if (rc) {
	// If we tried all the methods and didn't get any successes, we have an
	// error.  We can either terminate the whole conversion based on this
	// failure, or ignore this object and process the remainder of the
	// tree.  The latter will be incorrect if this object was supposed to
	// materially contribute to the final object shape, but for some
	// applications like visualization there are cases where "something is
	// better than nothing"
	return -1;
    }

    // If we succeeded, tmpfil should now hold a BoT that will be a suitable
    // basis for a Manifold.
    if (!bu_file_exists(tmpfil, NULL)) {
    	bu_log("Unable to locate tessellation result database %s\n", tmpfil);
	return -1;
    }

    // TODO - it may be worth doing this "on the fly" for larger meshes, to
    // save our in-memory footprint during the boolean process...  save the
    // temp .g files, stash the paths for later lookup, and clean them all up
    // at the end...
    int fsize = bu_file_size(tmpfil);

    struct rt_bot_internal *bot = NULL;
    struct db_i *dbip = db_open(tmpfil, DB_OPEN_READONLY);
    if (!dbip) {
	bu_log("Unable to open tessellation database %s for result reading\n", tmpfil);
	return -1;
    }




    manifold::Mesh bot_mesh;
    for (size_t j = 0; j < nbot->num_vertices ; j++)
	bot_mesh.vertPos.push_back(glm::vec3(nbot->vertices[3*j], nbot->vertices[3*j+1], nbot->vertices[3*j+2]));
    for (size_t j = 0; j < nbot->num_faces; j++)
	bot_mesh.triVerts.push_back(glm::vec3(nbot->faces[3*j], nbot->faces[3*j+1], nbot->faces[3*j+2]));

    manifold::Manifold bot_manifold = manifold::Manifold(bot_mesh);
    if (bot_manifold.Status() != manifold::Manifold::Error::NoError) {
	// Urk - we got a mesh, but it's no good for a Manifold(??)
	if (nbot->vertices)
	    bu_free(nbot->vertices, "verts");
	if (nbot->faces)
	    bu_free(nbot->faces, "faces");
	BU_FREE(nbot, struct rt_bot_internal);
	nbot = NULL;
    }

    if (status >= 0) {
	// Passed - return the manifold
	(*out) = new manifold::Manifold(bot_manifold);
	return 0;
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


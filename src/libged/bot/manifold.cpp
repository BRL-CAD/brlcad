/*                     M A N I F O L D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2023 United States Government as represented by
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
/** @file libged/bot/manifold.cpp
 *
 * The LIBGED bot manifold subcommand.
 *
 * Checks if a BoT is manifold according to the Manifold library.  If
 * not, and if an output object name is specified, it will attempt
 * various repair operations to try and produce a manifold output
 * using the specified test bot as an input.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "manifold/manifold.h"
#ifdef USE_ASSETIMPORT
#include "manifold/meshIO.h"
#endif

#include "geogram/basic/process.h"
#include <geogram/basic/command_line.h>
#include <geogram/basic/command_line_args.h>
#include "geogram/mesh/mesh.h"
#include "geogram/mesh/mesh_geometry.h"
#include "geogram/mesh/mesh_preprocessing.h"
#include "geogram/mesh/mesh_repair.h"
#include "geogram/mesh/mesh_fill_holes.h"

#include "bu/cmd.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "bg/chull.h"
#include "bg/trimesh.h"
#include "rt/geom.h"
#include "wdb.h"

#include "./ged_bot.h"
#include "../ged_private.h"

static struct rt_bot_internal *
manifold_process(struct rt_bot_internal *bot, int repair)
{
    if (!bot)
	return NULL;

    manifold::Mesh bot_mesh;
    for (size_t j = 0; j < bot->num_vertices ; j++)
	bot_mesh.vertPos.push_back(glm::vec3(bot->vertices[3*j], bot->vertices[3*j+1], bot->vertices[3*j+2]));
    if (bot->orientation == RT_BOT_CW) {
	for (size_t j = 0; j < bot->num_faces; j++)
	    bot_mesh.triVerts.push_back(glm::vec3(bot->faces[3*j], bot->faces[3*j+2], bot->faces[3*j+1]));
    } else {
	for (size_t j = 0; j < bot->num_faces; j++)
	    bot_mesh.triVerts.push_back(glm::vec3(bot->faces[3*j], bot->faces[3*j+1], bot->faces[3*j+2]));
    }

    manifold::MeshGL bot_gl(bot_mesh);
    if (!bot_gl.NumTri()) {
	// GetMeshGL failed, cannot process.
	return NULL;
    }

    if (!bot_gl.Merge()) {
	// BoT is already manifold
	return bot;
    }

    // If we're not going to try and repair it, it's just a
    // non-manifold mesh report at this point.
    if (!repair)
	return NULL;

    manifold::Manifold omanifold(bot_gl);
    if (omanifold.Status() != manifold::Manifold::Error::NoError) {
	// MeshGL.Merge() failed to produce a manifold mesh"
	return NULL;
    }

    // Original wasn't manifold, but the Manifold Merge() function was
    // able to produce a variation that is.  Make a new BoT
    manifold::Mesh omesh = omanifold.GetMesh();
    struct rt_bot_internal *nbot;
    BU_GET(nbot, struct rt_bot_internal);
    nbot->magic = RT_BOT_INTERNAL_MAGIC;
    nbot->mode = RT_BOT_SOLID;
    nbot->orientation = RT_BOT_CCW;
    nbot->thickness = NULL;
    nbot->face_mode = (struct bu_bitv *)NULL;
    nbot->bot_flags = 0;
    nbot->num_vertices = (int)omesh.vertPos.size();
    nbot->num_faces = (int)omesh.triVerts.size();
    nbot->vertices = (double *)calloc(omesh.vertPos.size()*3, sizeof(double));;
    nbot->faces = (int *)calloc(omesh.triVerts.size()*3, sizeof(int));
    for (size_t j = 0; j < omesh.vertPos.size(); j++) {
	nbot->vertices[3*j] = omesh.vertPos[j].x;
	nbot->vertices[3*j+1] = omesh.vertPos[j].y;
	nbot->vertices[3*j+2] = omesh.vertPos[j].z;
    }
    for (size_t j = 0; j < omesh.triVerts.size(); j++) {
	nbot->faces[3*j] = omesh.triVerts[j].x;
	nbot->faces[3*j+1] = omesh.triVerts[j].y;
	nbot->faces[3*j+2] = omesh.triVerts[j].z;
    }

    return nbot;
}

static struct rt_bot_internal *
geogram_mesh_repair(struct rt_bot_internal *bot)
{
    if (!bot)
	return NULL;

    // Use the default hole filling algorithm
    GEO::CmdLine::set_arg("algo:hole_filling", "loop_split");
    GEO::CmdLine::set_arg("algo:nn_search", "BNN");

    GEO::Mesh gm;
    gm.vertices.assign_points((double *)bot->vertices, 3, bot->num_vertices);
    for (size_t i = 0; i < bot->num_faces; i++) {
	GEO::index_t f = gm.facets.create_polygon(3);
	gm.facets.set_vertex(f, 0, bot->faces[3*i+0]);
	gm.facets.set_vertex(f, 1, bot->faces[3*i+1]);
	gm.facets.set_vertex(f, 2, bot->faces[3*i+2]);
    }

    // After the initial raw load, do a repair pass to set up
    // Geogram's internal mesh data
    double epsilon = 1e-6 * (0.01 * GEO::bbox_diagonal(gm));

    GEO::mesh_repair(gm, GEO::MeshRepairMode(GEO::MESH_REPAIR_DEFAULT), epsilon);

    // Per the geobox "mesh repair" function, we need to do some
    // small connected component removal ahead of the fill_holes
    // call  - that was the behavior difference observed between
    // the raw bot manifold run and exporting the mesh into geobox
    // for processing
    double area = GEO::Geom::mesh_area(gm,3);
    double min_comp_area = 0.03 * area;
    if (min_comp_area > 0.0) {
	double nb_f_removed = gm.facets.nb();
	GEO::remove_small_connected_components(gm, min_comp_area);
	nb_f_removed -= gm.facets.nb();
	if(nb_f_removed > 0 || nb_f_removed < 0) {
	    GEO::mesh_repair(gm, GEO::MESH_REPAIR_DEFAULT, epsilon);
	}
    }

    // Do the hole filling.
    //
    // TODO: Right now we're basically trying to fill in ALL holes (1e30 is a
    // value used in the Geogram code for a large hole size. That large a value
    // may not be a good default for automatic processing - we might be better
    // off starting with something like a percentage of the surface area of the
    // mesh, so we don't end up "repairing" something with a giant hole in such
    // a way that it is unlikely to represent the original modeling intent.  We
    // could then let the user override that default with something more
    // extreme manually if they deem the result useful.
    GEO::fill_holes(gm, 1e30);

    // Make sure we're still repaired post filling
    GEO::mesh_repair(gm, GEO::MeshRepairMode(GEO::MESH_REPAIR_DEFAULT));

    // Post repair, make sure mesh is still a triangle mesh
    gm.facets.triangulate();

    // Once Geogram is done with it, ask Manifold what it thinks
    // of the result - if Manifold doesn't think we're there, then
    // the results won't fly for boolean evaluation and we go ahead
    // and reject.
    manifold::Mesh gmm;
    for(GEO::index_t v = 0; v < gm.vertices.nb(); v++) {
	double gm_v[3];
	const double *p = gm.vertices.point_ptr(v);
	for (int i = 0; i < 3; i++) {
	    gm_v[i] = p[i];
	}
	gmm.vertPos.push_back(glm::vec3(gm_v[0], gm_v[1], gm_v[2]));
    }
    for (GEO::index_t f = 0; f < gm.facets.nb(); f++) {
	double tri_verts[3];
	for (int i = 0; i < 3; i++) {
	    tri_verts[i] = gm.facets.vertex(f, i);
	}
	// TODO - CW vs CCW orientation handling?
	gmm.triVerts.push_back(glm::vec3(tri_verts[0], tri_verts[1], tri_verts[2]));
    }

#if 1
    manifold::Manifold gmanifold(gmm);
    if (gmanifold.Status() != manifold::Manifold::Error::NoError) {
	// Repair failed
	return NULL;
    }
    // Output is manifold, make a new bot
    manifold::Mesh omesh = gmanifold.GetMesh();
#else
    // output the mesh, good or bad (used for debugging)
    manifold::Mesh omesh = gmm;
#endif

    struct rt_bot_internal *nbot;
    BU_GET(nbot, struct rt_bot_internal);
    nbot->magic = RT_BOT_INTERNAL_MAGIC;
    nbot->mode = RT_BOT_SOLID;
    nbot->orientation = RT_BOT_CCW;
    nbot->thickness = NULL;
    nbot->face_mode = (struct bu_bitv *)NULL;
    nbot->bot_flags = 0;
    nbot->num_vertices = (int)omesh.vertPos.size();
    nbot->num_faces = (int)omesh.triVerts.size();
    nbot->vertices = (double *)calloc(omesh.vertPos.size()*3, sizeof(double));;
    nbot->faces = (int *)calloc(omesh.triVerts.size()*3, sizeof(int));
    for (size_t j = 0; j < omesh.vertPos.size(); j++) {
	nbot->vertices[3*j] = omesh.vertPos[j].x;
	nbot->vertices[3*j+1] = omesh.vertPos[j].y;
	nbot->vertices[3*j+2] = omesh.vertPos[j].z;
    }
    for (size_t j = 0; j < omesh.triVerts.size(); j++) {
	nbot->faces[3*j] = omesh.triVerts[j].x;
	nbot->faces[3*j+1] = omesh.triVerts[j].y;
	nbot->faces[3*j+2] = omesh.triVerts[j].z;
    }
    return nbot;
}

//#define IN_PLACE_REPAIR

extern "C" int
_bot_cmd_manifold(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot manifold <objname> [repaired_obj_name]";
    const char *purpose_string = "Check if Manifold thinks the BoT is a manifold mesh.  If not, and a repaired_obj_name has been supplied, attempt to produce a manifold output using objname's geometry as an input.  If successful, the resulting manifold geometry will be written to repaired_obj_name.";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    // GEO::initialize needs to be called once.  TODO - this probably
    // should happen once for all of libged, if we use geogram in
    // multiple commands...
    static int geo_inited = 0;
    if (!geo_inited) {
	GEO::initialize();
	geo_inited = 1;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

#ifdef IN_PLACE_REPAIR
    if (argc != 1) {
#else
    if (argc != 1 && argc != 2) {
#endif
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }

    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
    if (bot->mode != RT_BOT_SOLID) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s is a non-solid BoT, skipping", gb->dp->d_namep);
	return BRLCAD_ERROR;
    }

#ifdef IN_PLACE_REPAIR
    int repair_flag = 1;
#else
    int repair_flag = (argc == 2) ?  1 : 0;
#endif

    struct rt_bot_internal *mbot = manifold_process(bot, repair_flag);

    // If we're already manifold, there's nothing to do
    if (mbot && mbot == bot) {
	bu_vls_printf(gb->gedp->ged_result_str, "BoT %s is manifold", gb->dp->d_namep);
	return BRLCAD_OK;
    }

    // If we don't have a repair target, just report non-manifold
    if (!mbot && !repair_flag) {
    	bu_vls_printf(gb->gedp->ged_result_str, "BoT %s is NOT manifold", gb->dp->d_namep);
	return BRLCAD_ERROR;
    }

    // If we're repairing and Manifold Merge was able to fix it, go with that
    if (mbot && repair_flag) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BOT;
	intern.idb_meth = &OBJ[ID_BOT];
	intern.idb_ptr = (void *)mbot;

#ifdef IN_PLACE_REPAIR
	const char *rname = gb->dp->d_namep;
	struct directory *dp = gb->dp;
#else
	const char *rname = argv[1];

	struct directory *dp = db_diradd(gb->gedp->dbip, rname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
	if (dp == RT_DIR_NULL) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Failed to write out new BoT %s", rname);
	    return BRLCAD_ERROR;
	}
#endif

	if (rt_db_put_internal(dp, gb->gedp->dbip, &intern, &rt_uniresource) < 0) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Failed to write out new BoT %s", rname);
	    return BRLCAD_ERROR;
	}

	bu_vls_printf(gb->gedp->ged_result_str, "Repair completed successfully (Manifold Merge op, minimal changes) and written to %s", rname);
	return BRLCAD_OK;
    }

    // Not manifold, not fixed by the first pass, and we want to repair it.
    // Time to attempt mesh repair
    mbot = geogram_mesh_repair(bot);

    if (!mbot) {
	bu_vls_printf(gb->gedp->ged_result_str, "Unable to repair BoT %s", gb->dp->d_namep);
	return BRLCAD_ERROR;
    }

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = (void *)mbot;


#ifdef IN_PLACE_REPAIR
    const char *rname = gb->dp->d_namep;
    struct directory *dp = gb->dp;
#else
    const char *rname = argv[1];
    struct directory *dp = db_diradd(gb->gedp->dbip, rname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str, "Failed to write out new BoT %s", rname);
	return BRLCAD_ERROR;
    }
#endif

    if (rt_db_put_internal(dp, gb->gedp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "Failed to write out new BoT %s", rname);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gb->gedp->ged_result_str, "Repair completed successfully (Geogram hole fill) and written to %s", rname);
    return BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


/*                       R E P A I R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2024 United States Government as represented by
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
/** @file repair.cpp
 *
 * Try repairing a BoT to produce a manifold volume
 *
 * TODO - investigate:
 *
 * https://github.com/wjakob/instant-meshes (see also https://github.com/Volumental/instant-meshes)
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

#ifdef USE_GEOGRAM
#include "geogram/basic/process.h"
#include <geogram/basic/command_line.h>
#include <geogram/basic/command_line_args.h>
#include "geogram/mesh/mesh.h"
#include "geogram/mesh/mesh_geometry.h"
#include "geogram/mesh/mesh_preprocessing.h"
#include "geogram/mesh/mesh_repair.h"
#include "geogram/mesh/mesh_fill_holes.h"
#endif

#include "rt/defines.h"
#include "rt/geom.h"
#include "rt/primitives/bot.h"

int
rt_bot_repair(struct rt_bot_internal **obot, struct rt_bot_internal *bot)
{
    if (!bot || !obot)
	return -1;

    // Unless we produce something, obot will be NULL
    *obot = NULL;

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
	return -1;
    }

    if (!bot_gl.Merge()) {
	// BoT is already manifold
	return 1;
    }

    manifold::Manifold omanifold(bot_gl);
    if (omanifold.Status() == manifold::Manifold::Error::NoError) {
	// MeshGL.Merge() produced a manifold mesh"
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
	*obot = nbot;
	return 0;
    }

#ifdef USE_GEOGRAM
    // Geogram libraries like to print a lot - shut down
    // the I/O channels until we can clear the logger
    int serr = -1;
    int sout = -1;
    int stderr_stashed = -1;
    int stdout_stashed = -1;
    int fnull = open("/dev/null", O_WRONLY);
    if (fnull == -1) {
	/* https://gcc.gnu.org/ml/gcc-patches/2005-05/msg01793.html */
	fnull = open("nul", O_WRONLY);
    }
    if (fnull != -1) {
	serr = fileno(stderr);
	sout = fileno(stdout);
	stderr_stashed = dup(serr);
	stdout_stashed = dup(sout);
	dup2(fnull, serr);
	dup2(fnull, sout);
	close(fnull);
    }

    // Make sure geogram is initialized
    GEO::initialize();

     // Quell logging messages
    GEO::Logger::instance()->unregister_all_clients();

    // Put I/O channels back where they belong
    if (fnull != -1) {
	fflush(stderr);
	dup2(stderr_stashed, serr);
	close(stderr_stashed);
	fflush(stdout);
	dup2(stdout_stashed, sout);
	close(stdout_stashed);
    }

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
	return -1;
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

    *obot = nbot;
    return 0;
#else
    // Without geogram, we can't repair (TODO - implement other options
    // like OpenMesh hole filling, maybe Instant Meshes?)
    return -1;
#endif
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


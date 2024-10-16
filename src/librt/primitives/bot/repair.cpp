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
 * Routines related to repairing BoTs
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
#include <unordered_set>
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

#include "bg/trimesh.h"
#include "rt/defines.h"
#include "rt/geom.h"
#include "rt/primitives/bot.h"

int
rt_bot_repair(struct rt_bot_internal **obot, struct rt_bot_internal *bot, struct rt_bot_repair_info *settings)
{
    if (!bot || !obot || !settings)
	return -1;

    // Unless we produce something, obot will be NULL
    *obot = NULL;

    manifold::MeshGL64 bot_mesh;
    for (size_t j = 0; j < bot->num_vertices ; j++) {
	bot_mesh.vertProperties.insert(bot_mesh.vertProperties.end(), bot->vertices[3*j+0]);
	bot_mesh.vertProperties.insert(bot_mesh.vertProperties.end(), bot->vertices[3*j+1]);
	bot_mesh.vertProperties.insert(bot_mesh.vertProperties.end(), bot->vertices[3*j+2]);
    }
    if (bot->orientation == RT_BOT_CW) {
	for (size_t j = 0; j < bot->num_faces; j++) {
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j+2]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j+1]);
	}
    } else {
	for (size_t j = 0; j < bot->num_faces; j++) {
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j+1]);
	    bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[3*j+2]);
	}
    }

    int num_vertices = (int)bot->num_vertices;
    int num_faces = (int)bot->num_faces;
    int bg_not_solid = bg_trimesh_solid2(num_vertices, num_faces, bot->vertices, bot->faces, NULL);

    if (!bot_mesh.Merge() && !bg_not_solid) {
	// BoT is already manifold
	return 1;
    }

    manifold::Manifold omanifold(bot_mesh);
    if (omanifold.Status() == manifold::Manifold::Error::NoError) {
	// MeshGL.Merge() produced a manifold mesh"
	manifold::MeshGL64 omesh = omanifold.GetMeshGL64();
	struct rt_bot_internal *nbot;
	BU_GET(nbot, struct rt_bot_internal);
	nbot->magic = RT_BOT_INTERNAL_MAGIC;
	nbot->mode = RT_BOT_SOLID;
	nbot->orientation = RT_BOT_CCW;
	nbot->thickness = NULL;
	nbot->face_mode = (struct bu_bitv *)NULL;
	nbot->bot_flags = 0;
	nbot->num_vertices = (int)omesh.vertProperties.size()/3;
	nbot->num_faces = (int)omesh.triVerts.size()/3;
	nbot->vertices = (double *)calloc(omesh.vertProperties.size(), sizeof(double));;
	nbot->faces = (int *)calloc(omesh.triVerts.size(), sizeof(int));
	for (size_t j = 0; j < omesh.vertProperties.size(); j++)
	    nbot->vertices[j] = omesh.vertProperties[j];
	for (size_t j = 0; j < omesh.triVerts.size(); j++)
	    nbot->faces[j] = omesh.triVerts[j];
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

    // To try to to fill in ALL holes we default to 1e30, which is a
    // value used in the Geogram code for a large hole size.
    double hole_size = 1e30;

    // Stash the bounding box diagonal
    double bbox_diag = GEO::bbox_diagonal(gm);

    // See if the settings override the default
    area = GEO::Geom::mesh_area(gm,3);
    if (!NEAR_ZERO(settings->max_hole_area, SMALL_FASTF)) {
	hole_size = settings->max_hole_area;
    } else if (!NEAR_ZERO(settings->max_hole_area_percent, SMALL_FASTF)) {
	hole_size = area * (settings->max_hole_area_percent/100.0);
    }

    // Do the hole filling.
    GEO::fill_holes(gm, hole_size);

    // Make sure we're still repaired post filling
    GEO::mesh_repair(gm, GEO::MeshRepairMode(GEO::MESH_REPAIR_DEFAULT));

    // Post repair, make sure mesh is still a triangle mesh
    gm.facets.triangulate();

    // Sanity check the area - shouldn't go down, and if it went up by more
    // than 3x it's concerning - that's a lot of new area even for a swiss
    // cheese mesh.  Can revisit reporting failure if we hit a legit case
    // like that, but we also want to know if something went badly wrong with
    // the hole filling itself and crazy new geometry was added...
    double new_area = GEO::Geom::mesh_area(gm,3);
    if (new_area < area) {
	bu_log("Mesh area decreased after hole filling - error\n");
	return -1;
    }
    if (new_area > 3*area) {
	bu_log("Mesh area more than tripled after hole filling.  At the moment this is considered an error - if a legitimate case exists where this is expected behavior, please report it upstream to the BRL-CAD developers.\n");
	return -1;
    }

    // Sanity check the bounding box diagonal - should be very close to the
    // original value
    double new_bbox_diag = GEO::bbox_diagonal(gm);
    if (!NEAR_EQUAL(bbox_diag, new_bbox_diag, BN_TOL_DIST)) {
	bu_log("Mesh bounding box is different after hole filling - error\n");
	return -1;
    }

    // Once Geogram is done with it, ask Manifold what it thinks
    // of the result - if Manifold doesn't think we're there, then
    // the results won't fly for boolean evaluation and we go ahead
    // and reject.
    manifold::MeshGL64 gmm;
    for(GEO::index_t v = 0; v < gm.vertices.nb(); v++) {
	const double *p = gm.vertices.point_ptr(v);
	for (int i = 0; i < 3; i++)
	    gmm.vertProperties.insert(gmm.vertProperties.end(), p[i]);
    }
    for (GEO::index_t f = 0; f < gm.facets.nb(); f++) {
	for (int i = 0; i < 3; i++) {
	    // TODO - CW vs CCW orientation handling?
	    gmm.triVerts.insert(gmm.triVerts.end(), gm.facets.vertex(f, i));
	}
    }

#if 1
    manifold::Manifold gmanifold(gmm);
    if (gmanifold.Status() != manifold::Manifold::Error::NoError) {
	// Repair failed
	return -1;
    }
    // Output is manifold, make a new bot
    manifold::MeshGL64 omesh = gmanifold.GetMeshGL64();
#else
    // output the mesh, good or bad (used for debugging)
    manifold::MeshGL64 omesh = gmm;
#endif

    struct rt_bot_internal *nbot;
    BU_GET(nbot, struct rt_bot_internal);
    nbot->magic = RT_BOT_INTERNAL_MAGIC;
    nbot->mode = RT_BOT_SOLID;
    nbot->orientation = RT_BOT_CCW;
    nbot->thickness = NULL;
    nbot->face_mode = (struct bu_bitv *)NULL;
    nbot->bot_flags = 0;
    nbot->num_vertices = (int)omesh.vertProperties.size()/3;
    nbot->num_faces = (int)omesh.triVerts.size()/3;
    nbot->vertices = (double *)calloc(omesh.vertProperties.size(), sizeof(double));;
    nbot->faces = (int *)calloc(omesh.triVerts.size(), sizeof(int));
    for (size_t j = 0; j < omesh.vertProperties.size(); j++)
	nbot->vertices[j] = omesh.vertProperties[j];
    for (size_t j = 0; j < omesh.triVerts.size(); j++)
	nbot->faces[j] = omesh.triVerts[j];

    *obot = nbot;
    return 0;
#else
    // Without geogram, we can't repair (TODO - implement other options
    // like OpenMesh hole filling)
    return -1;
#endif
}


struct rt_bot_internal *
rt_bot_remove_faces(struct bu_ptbl *rm_face_indices, const struct rt_bot_internal *orig_bot)
{
    if (!rm_face_indices || !BU_PTBL_LEN(rm_face_indices))
	return NULL;


    std::unordered_set<size_t> rm_indices;
    for (size_t i = 0; i < BU_PTBL_LEN(rm_face_indices); i++) {
	int ind = (int)(long)BU_PTBL_GET(rm_face_indices, i);
	rm_indices.insert(ind);
    }

    int *nfaces = (int *)bu_calloc(orig_bot->num_faces * 3, sizeof(int), "new faces array");
    size_t nfaces_ind = 0;
    for (size_t i = 0; i < orig_bot->num_faces; i++) {
	if (rm_indices.find(i) != rm_indices.end())
	    continue;
	nfaces[3*nfaces_ind + 0] = orig_bot->faces[3*i+0];
	nfaces[3*nfaces_ind + 1] = orig_bot->faces[3*i+1];
	nfaces[3*nfaces_ind + 2] = orig_bot->faces[3*i+2];
	nfaces_ind++;
    }

    // Having built a faces array with the specified triangles removed, we now
    // garbage collect to produce re-indexed face and point arrays with just the
    // active data (vertices may be no longer active in the BoT depending on
    // which faces were removed.
    int *nfacesarray = NULL;
    point_t *npointsarray = NULL;
    int npntcnt = 0;
    int new_num_faces = bg_trimesh_3d_gc(&nfacesarray, &npointsarray, &npntcnt, nfaces, nfaces_ind, (const point_t *)orig_bot->vertices);

    if (new_num_faces < 3) {
	new_num_faces = 0;
	npntcnt = 0;
	bu_free(nfacesarray, "nfacesarray");
	nfacesarray = NULL;
	bu_free(npointsarray, "npointsarray");
	npointsarray = NULL;
    }

    // Done with the nfaces array
    bu_free(nfaces, "free unmapped new faces array");

    // Make the new rt_bot_internal
    struct rt_bot_internal *bot = NULL;
    BU_GET(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = orig_bot->mode;
    bot->orientation = orig_bot->orientation;
    bot->bot_flags = orig_bot->bot_flags;
    bot->num_vertices = npntcnt;
    bot->num_faces = new_num_faces;
    bot->vertices = (fastf_t *)npointsarray;
    bot->faces = nfacesarray;

    // TODO - need to properly rebuild these arrays as well, if orig_bot has them - bg_trimesh_3d_gc only
    // handles the vertices themselves
    bot->thickness = NULL;
    bot->face_mode = NULL;
    bot->normals = NULL;
    bot->face_normals = NULL;
    bot->uvs = NULL;
    bot->face_uvs = NULL;

    return bot;
}

struct rt_bot_internal *
rt_bot_dup(const struct rt_bot_internal *obot)
{
    if (!obot)
	return NULL;

    struct rt_bot_internal *bot = NULL;
    BU_GET(bot, struct rt_bot_internal);
    bot->magic = obot->magic;
    bot->mode = obot->mode;
    bot->orientation = obot->orientation;
    bot->bot_flags = obot->bot_flags;

    bot->num_faces = obot->num_faces;
    bot->faces = (int *)bu_malloc(obot->num_faces * sizeof(int)*3, "bot faces");
    memcpy(bot->faces, obot->faces, obot->num_faces * sizeof(int)*3);

    bot->num_vertices = obot->num_vertices;
    bot->vertices = (fastf_t*)bu_malloc(obot->num_vertices * sizeof(fastf_t)*3, "bot verts");
    memcpy(bot->vertices, obot->vertices, obot->num_vertices * sizeof(fastf_t)*3);

    if (obot->thickness) {
	bot->thickness = (fastf_t*)bu_malloc(obot->num_faces * sizeof(fastf_t), "bot thicknesses");
	memcpy(bot->thickness, obot->thickness, obot->num_faces * sizeof(fastf_t));
    }

    if (obot->face_mode) {
	bot->face_mode = (struct bu_bitv *)bu_malloc(obot->num_faces * sizeof(struct bu_bitv), "bot face_mode");
	memcpy(bot->face_mode, obot->face_mode, obot->num_faces * sizeof(struct bu_bitv));
    }

    if (obot->normals && obot->num_normals) {
	bot->num_normals = obot->num_normals;
	bot->normals = (fastf_t*)bu_malloc(obot->num_normals * sizeof(fastf_t)*3, "bot normals");
	memcpy(bot->normals, obot->normals, obot->num_normals * sizeof(fastf_t)*3);
    }

    if (obot->face_normals && obot->num_face_normals) {
	bot->num_face_normals = obot->num_face_normals;
	bot->face_normals = (int*)bu_malloc(obot->num_face_normals * sizeof(int)*3, "bot face normals");
	memcpy(bot->face_normals, obot->face_normals, obot->num_face_normals * sizeof(int)*3);
    }

    if (obot->num_uvs && obot->uvs) {
	bot->num_uvs = obot->num_uvs;
	bot->uvs = (fastf_t*)bu_malloc(obot->num_uvs * sizeof(fastf_t)*3, "bot uvs");
	memcpy(bot->uvs, obot->uvs, obot->num_uvs * sizeof(fastf_t)*3);
    }

    if (obot->num_face_uvs && obot->face_uvs) {
	bot->num_face_uvs = obot->num_face_uvs;
	bot->face_uvs = (int*)bu_malloc(obot->num_face_uvs * sizeof(int)*3, "bot face_uvs");
	memcpy(bot->face_uvs, obot->face_uvs, obot->num_face_uvs * sizeof(int)*3);
    }

    return bot;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


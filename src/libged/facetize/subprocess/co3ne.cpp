/*                      C O 3 N E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @file libged/facetize/tessellate/spsr.cpp
 *
 * Tessellation interface to Geogram Concurrent Co-Cones surface reconstruction
 * algorithm.
 *
 */

#include "common.h"

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
#include "geogram/points/co3ne.h"
#include "geogram/points/nn_search.h"
#endif

#include "bg/spsr.h"
#include "../../ged_private.h"
#include "./tessellate.h"

int
co3ne_mesh(struct rt_bot_internal **obot, struct db_i *dbip, struct rt_pnts_internal *pnts, tess_opts *s)
{
    if (!obot || !dbip || !pnts || !s)
	return BRLCAD_ERROR;

#ifdef USE_GEOGRAM

#if 1
    // Geogram libraries like to print a lot - shut down
    // the I/O channels until we can clear the logger
    int serr;
    int sout;
    int stderr_stashed;
    int stdout_stashed;
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
#endif

    // Make sure geogram is initialized
    GEO::initialize();

#if 1
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
	fnull = -1;
    }
#endif
    GEO::CmdLine::set_arg("algo:nn_search", "ANN");
    GEO::CmdLine::set_arg("co3ne:max_N_angle", "60.0");

    GEO::Mesh gm;
    GEO::Attribute<double> normals;
    normals.bind_if_is_defined(gm.vertices.attributes(), "normal");
    if(!normals.is_bound())
	normals.create_vector_attribute(gm.vertices.attributes(), "normal", 3);

    point_t *input_points_3d = (point_t *)bu_calloc(pnts->count, sizeof(point_t), "points");
    vect_t *input_normals_3d = (vect_t *)bu_calloc(pnts->count, sizeof(vect_t), "normals");
    struct pnt_normal *pn;
    struct pnt_normal *pl = (struct pnt_normal *)pnts->point;
    int i = 0;
    for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	VMOVE(input_points_3d[i], pn->v);
	VMOVE(input_normals_3d[i], pn->n);
	i++;
    }

    gm.vertices.assign_points((double *)input_points_3d, 3, pnts->count);

    i = 0;
    for(GEO::index_t v=0; v < gm.vertices.nb(); ++v) {
	normals[3*v] = input_normals_3d[i][X];
	normals[3*v+1] = input_normals_3d[i][Y];
	normals[3*v+2] = input_normals_3d[i][Z];
	i++;
    }

    // maximum distance used to connect neighbors with triangles
    double search_dist = 0.05 * GEO::bbox_diagonal(gm);

    GEO::Co3Ne_reconstruct(gm, search_dist);

    // Once Geogram is done with it, ask Manifold what it thinks
    // of the result - if Manifold doesn't think we're there, then
    // the results won't fly for boolean evaluation and we go ahead
    // and reject.
    manifold::MeshGL64 gmm;
    for(GEO::index_t v = 0; v < gm.vertices.nb(); v++) {
	const double *p = gm.vertices.point_ptr(v);
	for (i = 0; i < 3; i++)
	    gmm.vertProperties.insert(gmm.vertProperties.end(), p[i]);
    }
    for (GEO::index_t f = 0; f < gm.facets.nb(); f++) {
	double tri_verts[3];
	// TODO - CW vs CCW orientation handling?
	// TODO - is this correct orientation??
	tri_verts[0] = gm.facets.vertex(f, 0);
	tri_verts[1] = gm.facets.vertex(f, 2);
	tri_verts[2] = gm.facets.vertex(f, 1);
	for (i = 0; i < 3; i++)
	    gmm.triVerts.insert(gmm.triVerts.end(), tri_verts[i]);
    }

    manifold::Manifold gmanifold(gmm);
    if (gmanifold.Status() != manifold::Manifold::Error::NoError) {
	bu_log("Geogram result not manifold\n");
	return BRLCAD_ERROR;
    }

    // Output is manifold, make a new bot
    manifold::MeshGL64 omesh = gmanifold.GetMeshGL64();

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

    /* do decimation */
    {
	double feature_size = (s->co3ne_options.feature_size > 0) ? s->co3ne_options.feature_size : search_dist;
	double d_feature_size = (s->co3ne_options.d_feature_size > 0) ? s->co3ne_options.d_feature_size : 1.5 * feature_size  ;

	bu_log("CO3NE: decimating with feature size %g\n", d_feature_size);

	struct rt_bot_internal *dbot = _tess_facetize_decimate(nbot, d_feature_size);

	if (nbot == dbot || !dbot->num_vertices || !dbot->num_faces) {
	    bu_log("decimation failed\n");
	    return BRLCAD_ERROR;
	}

	*obot = dbot;
    }

    return BRLCAD_OK;
#else
    return BRLCAD_ERROR;
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


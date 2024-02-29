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

#ifdef USE_GEOGRAM
#include "geogram/basic/process.h"
#include <geogram/basic/command_line.h>
#include <geogram/basic/command_line_args.h>
#include "geogram/mesh/mesh.h"
#include "geogram/mesh/mesh_geometry.h"
#include "geogram/mesh/mesh_preprocessing.h"
#include "geogram/points/co3ne.h"
#endif

#include "bg/spsr.h"
#include "../../ged_private.h"
#include "./tessellate.h"
#include "../tess_opts.h"

int
co3ne_mesh(struct rt_bot_internal **obot, struct db_i *dbip, struct rt_pnts_internal *pnts, struct tess_opts *s)
{
    if (!obot || !dbip || !pnts || !s)
	return BRLCAD_ERROR;

#ifdef USE_GEOGRAM
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

    GEO::Co3Ne_reconstruct(gm, 0.0); // maximum distance used to connect neighbors with triangles - should we use ANN to get that value?  need to check what geogram does...

#if 0

    /* Check validity - do not return an invalid BoT */
    {
	int not_solid = bg_trimesh_solid2(bot->num_vertices, bot->num_faces, (fastf_t *)bot->vertices, (int *)bot->faces, NULL);
	if (not_solid) {
	    if (bot->vertices) bu_free(bot->vertices, "verts");
	    if (bot->faces) bu_free(bot->faces, "verts");
	    ret = BRLCAD_ERROR;
	    bu_log("SPSR: facetization failed, final BoT was not solid\n");
	    goto ged_facetize_spsr_memfree;
	}
    }

    *obot = bot;
#endif
    return BRLCAD_ERROR;
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


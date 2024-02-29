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


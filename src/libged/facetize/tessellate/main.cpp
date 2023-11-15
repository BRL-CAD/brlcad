/*                        M A I N . C P P
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
/** @file libged/facetize/tessellate/main.cpp
 *
 * Because the process of turning implicit solids into manifold meshes
 * has a wide variety of difficulties associated with it, we run the
 * actual per-primitive conversion as a sub-process managed by the
 * facetize command.
 */

#include "common.h"

#include "bu/app.h"
#include "bu/opt.h"

#include "ged.h"
#include "./tessellate.h"

int
main(int argc, const char **argv)
{
    if (!argc || !argv)
	return BRLCAD_ERROR;

    bu_setprogname(argv[0]);

    struct ged *gedp = ged_open("db", argv[1], 1);
    if (!gedp)
	return BRLCAD_ERROR;

    struct directory *dp = db_lookup(gedp->dbip, argv[2], LOOKUP_QUIET);
    if (!dp)
	return BRLCAD_ERROR;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, gedp->dbip, NULL, &rt_uniresource) < 0) {
	bu_log("rt_db_get_internal failed for %s\n", argv[2]);
	return BRLCAD_ERROR;
    }

    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct rt_bot_internal *bot = NULL;
    struct rt_bot_internal *obot = NULL;
    manifold::Mesh bot_mesh;
    manifold::Manifold bot_manifold;
    int propVal;
    int ret = 0;

    switch (intern.idb_minor_type) {
	// If we've got no-volume objects, they get an empty Manifold -
	// they can be safely treated as a no-op in any of the booleans
	case ID_ANNOT:
	case ID_BINUNIF:
	case ID_CONSTRAINT:
	case ID_DATUM:
	case ID_GRIP:
	case ID_JOINT:
	case ID_MATERIAL:
	case ID_PNTS:
	case ID_SCRIPT:
	case ID_SKETCH:
	    return BRLCAD_OK;
	case ID_HALF:
	    // Halfspace objects get a large arb.
	    ret = half_to_bot(&obot, &intern, &ttol, &tol);
	    break;
	case ID_BOT:
	    bot = (struct rt_bot_internal *)(intern.idb_ptr);
	    propVal = (int)rt_bot_propget(bot, "type");
	    // Surface meshes are zero volume, and thus no-op
	    if (propVal == RT_BOT_SURFACE)
		return BRLCAD_OK;
	    // Plate mode BoTs need an explicit volume representation
	    if (propVal == RT_BOT_PLATE || propVal == RT_BOT_PLATE_NOCOS) {
		ret = plate_eval(&obot, bot, &ttol, &tol);
	    }
	    // Volumetric bot - if it can be manifold we're good, but if
	    // not we need to try and repair it.
	    for (size_t j = 0; j < bot->num_vertices ; j++)
		bot_mesh.vertPos.push_back(glm::vec3(bot->vertices[3*j], bot->vertices[3*j+1], bot->vertices[3*j+2]));
	    for (size_t j = 0; j < bot->num_faces; j++)
		bot_mesh.triVerts.push_back(glm::vec3(bot->faces[3*j], bot->faces[3*j+1], bot->faces[3*j+2]));
	    bot_manifold = manifold::Manifold(bot_mesh);
	    if (bot_manifold.Status() != manifold::Manifold::Error::NoError) {
		// Nope - try repairing
		ret = bot_repair(&obot, bot, &ttol, &tol);
	    }
	case ID_BREP:
	    // TODO - need to handle plate mode NURBS the way we handle plate mode BoTs
	default:
	    break;
    }

    if (ret == BRLCAD_OK && obot) {
	// TODO - have output bot?  write it to disk and return
	return BRLCAD_OK;
    }

    // If we got this far, it's not a special case - see if we can do "normal"
    // tessellation
    int status = -1;
    struct rt_bot_internal *nbot = NULL;
    if (intern.idb_meth) {
	struct model *m = nmg_mm();
	struct nmgregion *r1 = (struct nmgregion *)NULL;
	// Try the NMG routines (primary means of CSG implicit -> explicit mesh conversion)
	// TODO - needs to be separate process...
	if (!BU_SETJUMP) {
	    status = intern.idb_meth->ft_tessellate(&r1, m, &intern, &ttol, &tol);
	} else {
	    BU_UNSETJUMP;
	    status = -1;
	    nbot = NULL;
	} BU_UNSETJUMP;
	if (status > -1) {
	    // NMG reports success, now get a BoT
	    if (!BU_SETJUMP) {
		nbot = (struct rt_bot_internal *)nmg_mdl_to_bot(m, &RTG.rtg_vlfree, &tol);
	    } else {
		BU_UNSETJUMP;
		nbot = NULL;
	    } BU_UNSETJUMP;
	    if (nbot) {
		// We got a BoT, now see if we can get a Manifold
		for (size_t j = 0; j < nbot->num_vertices ; j++)
		    bot_mesh.vertPos.push_back(glm::vec3(nbot->vertices[3*j], nbot->vertices[3*j+1], nbot->vertices[3*j+2]));
		for (size_t j = 0; j < nbot->num_faces; j++)
		    bot_mesh.triVerts.push_back(glm::vec3(nbot->faces[3*j], nbot->faces[3*j+1], nbot->faces[3*j+2]));
		bot_manifold = manifold::Manifold(bot_mesh);
		if (bot_manifold.Status() != manifold::Manifold::Error::NoError) {
		    // Urk - we got an NMG mesh, but it's no good for a Manifold(??)
		    if (nbot->vertices)
			bu_free(nbot->vertices, "verts");
		    if (nbot->faces)
			bu_free(nbot->faces, "faces");
		    BU_FREE(nbot, struct rt_bot_internal);
		    nbot = NULL;
		}
	    }
	}
    }

    if (status >= 0)
	return BRLCAD_OK;

    // TODO - nothing worked - try fallback methods, if enabled
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


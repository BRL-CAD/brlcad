/*                   P R E P R O C E S S . C P P
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
/** @file libged/facetize.cpp
 *
 * Logic for cleaning up or otherwise preparing geometry for facetize
 * that is known to be problematic for the standard routines.  Examples
 * include:
 *
 * pnts are non-volumetric and can be ignored
 * Half planes (need to be represented with arb8s for boolean purposes)
 * Non-manifold meshes, non-plate - need to attempt repair
 * Non-manifold meshes, plate-mode - need offset surfaces
 *
 * Probably others...
 *
 */

#include "common.h"

#include "bu/opt.h"

#include "../ged_private.h"
#include "./ged_facetize.h"

#if 0
// TODO - cline has a facetize routine, so this is probably not needed.  However, it might
// be usable as a fallback option if the cline facetize routine fails.
static int
cline_to_pipe(void **out, struct rt_db_internal *ip)
{
    struct rt_pipe_internal pipe;
    int ret = rt_cline_to_pipe(&pipe, ip);
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_PIPE;
    intern.idb_ptr = &pipe;
    intern.idb_meth = &OBJ[ID_PIPE];

    struct nmgregion *r1 = NULL;
    struct model *m = nmg_mm();
    if (!intern.idb_meth->ft_tessellate(&r1, m, &intern, ttol, tol)) {
	(*out) = new manifold::Manifold();
	return 1;
    }

    struct rt_bot_internal *sbot = (struct rt_bot_internal *)nmg_mdl_to_bot(m, &RTG.rtg_vlfree, tol);
    if (!sbot) {
    	(*out) = new manifold::Manifold();
	return 1;
    }

    manifold::Mesh pipe_m;
    for (size_t j = 0; j < sbot->num_vertices ; j++)
	pipe_m.vertPos.push_back(glm::vec3(sbot->vertices[3*j], sbot->vertices[3*j+1], sbot->vertices[3*j+2]));
    for (size_t j = 0; j < sbot->num_faces; j++)
	pipe_m.triVerts.push_back(glm::vec3(sbot->faces[3*j], sbot->faces[3*j+1], sbot->faces[3*j+2]));

    manifold::Manifold pipe_manifold(pipe_m);
    if (pipe_manifold.Status() != manifold::Manifold::Error::NoError) {
	bu_log("cline conversion - cannot define manifold from pipe facetization\n");
	(*out) = new manifold::Manifold();
	return 1;
    }

    (*out) = new manifold::Manifold(pipe_manifold);
    return 0;
}
#endif

int
bot_repair(void **out, struct rt_bot_internal *bot, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    if (!out || !bot || !ttol || !tol)
	return -1;
    return 0;
}

int
_pre_tess_clbk(void **out, struct db_tree_state *tsp, const struct db_full_path *UNUSED(pathp), struct rt_db_internal *ip, void *data)
{
    if (!out || !tsp || !ip || !data)
	return -1;

    struct _ged_facetize_state *s = (struct _ged_facetize_state *)data;
    struct facetize_maps *fm = (struct facetize_maps *)s->iptr;

    manifold::Manifold *hm = NULL;

    struct rt_bot_internal *bot = NULL;
    int propVal = 0;
    manifold::Mesh bot_mesh;
    manifold::Manifold bot_manifold;

    switch (ip->idb_minor_type) {
	// If we've got no-volume objects, they get an empty manifold_mesh -
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
	    (*out) = new manifold::Manifold();
	    return 0;
	case ID_HALF:
	    // Halfspace objects get a large arb.  Really should based this on
	    // the total scene size, but maybe punt and just make it just below
	    // the limits of the world size?
	    // Construct the arb
	    // facetize the arb
	    // nmg->bot
	    // bot->manifold
	    // (*out) = manifold;
	    if (fm)
		fm->half_spaces.insert(hm);
	    return 0;
	case ID_BOT:
	    bot = (struct rt_bot_internal *)(ip->idb_ptr);
	    propVal = (int)rt_bot_propget(bot, "type");
	    // Surface meshes are zero volume, and thus no-op
	    if (propVal == RT_BOT_SURFACE) {
		(*out) = new manifold::Manifold();
		return 0;
	    }
	    // Plate mode BoTs need an explicit volume representation
	    if (propVal == RT_BOT_PLATE || propVal == RT_BOT_PLATE_NOCOS) {
		return plate_eval(out, bot, tsp->ts_ttol, tsp->ts_tol);
	    }
	    // Volumetric bot - make a manifold.  There is no need to stage through NMG - what
	    // we care about is whether this mesh is something Manifold's boolean can handle,
	    // or whether we need to try to repair it.
	    for (size_t j = 0; j < bot->num_vertices ; j++)
		bot_mesh.vertPos.push_back(glm::vec3(bot->vertices[3*j], bot->vertices[3*j+1], bot->vertices[3*j+2]));
	    for (size_t j = 0; j < bot->num_faces; j++)
		bot_mesh.triVerts.push_back(glm::vec3(bot->faces[3*j], bot->faces[3*j+1], bot->faces[3*j+2]));
	    bot_manifold = manifold::Manifold(bot_mesh);
	    if (bot_manifold.Status() != manifold::Manifold::Error::NoError) {
		// Nope - try repairing
		return bot_repair(out, bot, tsp->ts_ttol, tsp->ts_tol);
	    }
	    // Passed - return the manifold
	    (*out) = new manifold::Manifold(bot_manifold);
	    return 0;
	case ID_BREP:
	    // TODO - need to handle plate mode NURBS the way we handle plate mode BoTs
	default:
	    // If we're not dealing with one of the above types, proceed
	    // normally
	    return -1;
    }

}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


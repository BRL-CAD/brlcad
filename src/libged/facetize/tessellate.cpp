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

#include "bu/opt.h"

#include "../ged_private.h"
#include "./ged_facetize.h"

// We use an arbn to define the enclosed volume, and facetize that
static int
half_to_manifold(void **out, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    plane_t equations[7];
    // First, bound the volume to its limits
    HSET(equations[0], -1, 0, 0, FLT_MAX);
    HSET(equations[1], 1, 0, 0, FLT_MAX);
    HSET(equations[2], 0, -1, 0, FLT_MAX);
    HSET(equations[3], 0, 1, 0, FLT_MAX);
    HSET(equations[4], 0, 0, -1, FLT_MAX);
    HSET(equations[5], 0, 0, 1, FLT_MAX);

    // Then introduce the half plane
    struct rt_half_internal *h = (struct rt_half_internal *)ip->idb_ptr;
    HMOVE(equations[6], h->eqn);

    struct rt_arbn_internal arbn;
    arbn.magic = RT_ARBN_INTERNAL_MAGIC;
    arbn.neqn = 7;
    arbn.eqn = equations;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_ARBN;
    intern.idb_ptr = &arbn;
    intern.idb_meth = &OBJ[ID_ARBN];

    struct nmgregion *r1 = NULL;
    struct model *m = nmg_mm();
    if (!intern.idb_meth->ft_tessellate(&r1, m, &intern, ttol, tol)) {
	(*out) = new manifold::Manifold();
	return 1;
    }

    struct rt_bot_internal *hbot = (struct rt_bot_internal *)nmg_mdl_to_bot(m, &RTG.rtg_vlfree, tol);
    if (!hbot) {
    	(*out) = new manifold::Manifold();
	return 1;
    }

    manifold::Mesh half_m;
    for (size_t j = 0; j < hbot->num_vertices ; j++)
	half_m.vertPos.push_back(glm::vec3(hbot->vertices[3*j], hbot->vertices[3*j+1], hbot->vertices[3*j+2]));
    for (size_t j = 0; j < hbot->num_faces; j++)
	half_m.triVerts.push_back(glm::vec3(hbot->faces[3*j], hbot->faces[3*j+1], hbot->faces[3*j+2]));

    manifold::Manifold half_manifold(half_m);
    if (half_manifold.Status() != manifold::Manifold::Error::NoError) {
	bu_log("half->arbn->bot conversion failed - cannot define manifold from half facetization\n");
	(*out) = new manifold::Manifold();
	return 1;
    }

    (*out) = new manifold::Manifold(half_manifold);
    return 0;
}

// TODO - this is a hard problem, without a truly general solution - we need to try to handle
// "almost correct" meshes, but can't do much with complete garbage.  Potential resources
// to investigate:
//
// https://github.com/wjakob/instant-meshes (also https://github.com/Volumental/instant-meshes)
// https://github.com/BrunoLevy/geogram (hole filling, Co3Ne)
//
int
bot_repair(void **out, struct rt_bot_internal *bot, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    if (!out || !bot || !ttol || !tol)
	return -1;
    return 0;
}

int
manifold_tessellate(void **out, struct db_tree_state *tsp, const struct db_full_path *UNUSED(pathp), struct rt_db_internal *ip, void *data)
{
    if (!out || !tsp || !ip || !data)
	return -1;

    struct _ged_facetize_state *s = (struct _ged_facetize_state *)data;
    struct facetize_maps *fm = (struct facetize_maps *)s->iptr;

    struct rt_bot_internal *bot = NULL;
    int propVal = 0, ret = -1;
    manifold::Mesh bot_mesh;
    manifold::Manifold bot_manifold;

    switch (ip->idb_minor_type) {
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
	    (*out) = new manifold::Manifold();
	    return 0;
	case ID_HALF:
	    // Halfspace objects get a large arb.
	    ret = half_to_manifold(out, ip, tsp->ts_ttol, tsp->ts_tol);
	    if (*out)
		fm->half_spaces.insert((manifold::Manifold *)*out);
	    return ret;
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
	    break;
    }

    // If we got this far, it's not a special case - see if we can do "normal"
    // tessellation
    int status = -1;
    struct rt_bot_internal *nbot = NULL;
    if (ip->idb_meth) {
	struct model *m = nmg_mm();
	struct nmgregion *r1 = (struct nmgregion *)NULL;
	// Try the NMG routines (primary means of CSG implicit -> explicit mesh conversion)
	if (!BU_SETJUMP) {
	    status = ip->idb_meth->ft_tessellate(&r1, m, ip, tsp->ts_ttol, tsp->ts_tol);
	} else {
	    BU_UNSETJUMP;
	    status = -1;
	    nbot = NULL;
	} BU_UNSETJUMP;
	if (status > -1) {
	    // NMG reports success, now get a BoT
	    if (!BU_SETJUMP) {
		nbot = (struct rt_bot_internal *)nmg_mdl_to_bot(m, &RTG.rtg_vlfree, tsp->ts_tol);
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

    if (status >= 0) {
	// Passed - return the manifold
	(*out) = new manifold::Manifold(bot_manifold);
	return 0;
    }

    // Nothing worked - try fallback methods, if enabled
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


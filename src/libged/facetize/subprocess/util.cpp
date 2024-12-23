/*                        U T I L . C P P
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
/** @file libged/facetize/tessellate/util.cpp
 *
 * facetize command.
 *
 */

#include "common.h"

#include "bu/ptbl.h"
#include "rt/search.h"
#include "../../ged_private.h"
#include "../ged_facetize.h"
#include "./tessellate.h"

struct rt_bot_internal *
_tess_facetize_decimate(struct rt_bot_internal *bot, fastf_t feature_size)
{
    size_t success = 0;
    /* these are static for longjmp */
    static struct rt_bot_internal *nbot;
    static struct rt_bot_internal *obot;

    obot = bot;
    nbot = NULL;

    BU_ALLOC(nbot, struct rt_bot_internal);
    nbot->magic = RT_BOT_INTERNAL_MAGIC;
    nbot->mode = RT_BOT_SOLID;
    nbot->orientation = RT_BOT_UNORIENTED;
    nbot->thickness = (fastf_t *)NULL;
    nbot->face_mode = (struct bu_bitv *)NULL;
    nbot->num_faces = bot->num_faces;
    nbot->num_vertices = bot->num_vertices;
    nbot->faces = (int *)bu_calloc(nbot->num_faces*3, sizeof(int), "copy of faces array");
    nbot->vertices = (fastf_t *)bu_calloc(nbot->num_vertices*3, sizeof(fastf_t), "copy of faces array");
    memcpy(nbot->faces, bot->faces, nbot->num_faces*3*sizeof(int));
    memcpy(nbot->vertices, bot->vertices, nbot->num_vertices*3*sizeof(fastf_t));

    if (!BU_SETJUMP) {
	/* try */
	success = rt_bot_decimate_gct(nbot, feature_size);
    } else {
	/* catch */
	BU_UNSETJUMP;
	/* Failed - free the working copy */
	bu_free(nbot->faces, "free faces");
	bu_free(nbot->vertices, "free vertices");
	bu_free(nbot, "free bot");
	return obot;
    } BU_UNSETJUMP;

    if (success) {
	/* Success - free the old BoT, return the new one */
	bu_free(obot->faces, "free faces");
	bu_free(obot->vertices, "free vertices");
	bu_free(obot, "free bot");
	return nbot;
    } else {
	/* Failed - free the working copy */
	bu_free(nbot->faces, "free faces");
	bu_free(nbot->vertices, "free vertices");
	bu_free(nbot, "free bot");
	return obot;
    }
}

int
_tess_facetize_write_bot(struct db_i *dbip, struct rt_bot_internal *bot, const char *name, const char *method)
{
    /* Export BOT as a new solid */
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = (void *)bot;

    // Delete a conflicting object name, if present
    struct directory *odp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (odp) {
	db_delete(dbip, odp);
	db_dirdelete(dbip, odp);
    }

    bu_avs_init_empty(&intern.idb_avs);
    if (method)
	(void)bu_avs_add(&intern.idb_avs, FACETIZE_METHOD_ATTR, method);

    struct directory *dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	bu_log("Cannot add %s to directory\n", name);
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	bu_log("Failed to write %s to database\n", name);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

bool
bot_is_manifold(struct rt_bot_internal *bot)
{
    if (!bot)
	return false;

    manifold::MeshGL64 bot_mesh;
    manifold::Manifold bot_manifold;

    // We have a BoT, but make sure we can get a Manifold before we accept it
    for (size_t j = 0; j < bot->num_vertices*3 ; j++)
	bot_mesh.vertProperties.insert(bot_mesh.vertProperties.end(), bot->vertices[j]);
    for (size_t j = 0; j < bot->num_faces*3; j++)
	bot_mesh.triVerts.insert(bot_mesh.triVerts.end(), bot->faces[j]);
    bot_manifold = manifold::Manifold(bot_mesh);
    if (bot_manifold.Status() != manifold::Manifold::Error::NoError)
	return false;
    return true;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


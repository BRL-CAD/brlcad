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

extern "C" {
#include "fort.h"
}

#include "bu/cmd.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "bg/chull.h"
#include "bg/trimesh.h"
#include "rt/geom.h"
#include "wdb.h"

#include "./ged_bot.h"
#include "../ged_private.h"

extern "C" int
_bot_cmd_manifold(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot manifold <objname> [outobj]";
    const char *purpose_string = "Check if Manifold thinks the BoT is a manifold mesh.  If not, if an outobj name has been supplied apply MeshGL Merge to see if can be automatically repaired.  If it can be, the result will be written to outobj.";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    if (argc != 1 && argc != 2) {
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
	bu_vls_printf(gb->gedp->ged_result_str, "%s: GetMeshGL failed, cannot process.", gb->dp->d_namep);
	return BRLCAD_ERROR;
    }
    if (!bot_gl.Merge()) {
	bu_vls_printf(gb->gedp->ged_result_str, "BoT %s is manifold", gb->dp->d_namep);
	return BRLCAD_ERROR;
    }
    manifold::Manifold omanifold(bot_gl);
    if (omanifold.Status() != manifold::Manifold::Error::NoError) {
	// Tried, but output isn't manifold - no dice.
	bu_vls_printf(gb->gedp->ged_result_str, "%s: MeshGL.Merge() failed to produce a manifold mesh - repair failed", gb->dp->d_namep);
	return BRLCAD_ERROR;
    }

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

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = (void *)nbot;

    if (rt_db_put_internal(gb->dp, gb->gedp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "Failed to write out new BoT %s", gb->dp->d_namep);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gb->gedp->ged_result_str, "Repair complete, written to %s", gb->dp->d_namep);
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


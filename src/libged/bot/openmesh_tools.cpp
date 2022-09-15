/*                  O P E N M E S H _ T O O L S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2022 United States Government as represented by
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
 /** @file libged/bot/openmesh_tools.cpp
  *
  * Bot subcommands using openmesh tools and features
  * based on remesh.cpp
  *
  */

#include "common.h"

#ifdef BUILD_OPENMESH_TOOLS

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push /* start new diagnostic pragma */
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined(__clang__)
#  pragma clang diagnostic push /* start new diagnostic pragma */
#  pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#include <OpenMesh/Core/Mesh/PolyMeshT.hh>
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop /* end ignoring warnings */
#elif defined(__clang__)
#  pragma clang diagnostic pop /* end ignoring warnings */
#endif

#endif /* BUILD_OPENMESH_TOOLS */


#include "vmath.h"
#include "bu/str.h"
#include "rt/db5.h"
#include "rt/db_internal.h"
#include "rt/db_io.h"
#include "rt/geom.h"
#include "rt/wdb.h"
#include "ged/commands.h"
#include "ged/database.h"
#include "ged/objects.h"
#include "./ged_bot.h"


#ifdef BUILD_OPENMESH_TOOLS

static bool
bot_om(struct ged* gedp, struct rt_bot_internal* bot)
{
    if (!gedp || !bot)
	return false;
    bu_vls_printf(gedp->ged_result_str, "command recognized\n");
    return true;
}

#else /* BUILD_OPENMESH_TOOLS */

static bool
bot_om(struct ged* gedp, struct rt_bot_internal* UNUSED(bot))
{
    bu_vls_printf(gedp->ged_result_str,
	"WARNING: BoT OpenMesh subcommands are unavailable.\n"
	"BRL-CAD needs to be compiled with OpenMesh support.\n"
	"(cmake -DBRLCAD_ENABLE_OPENVDB=ON or set -DOPENMESH_ROOT=/path/to/openmesh)\n");
    return false;
}

#endif /* BUILD_OPENMESH_TOOLS */


extern "C" int
_bot_cmd_om(void* bs, int argc, const char** argv)
{
    const char* usage_string = "bot [options] openmesh [subcommand]? <objname> <output_bot>";
    const char* purpose_string = "Do <something> to the BoT using the OpenMesh library and output to <output_bot>";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info* gb = (struct _ged_bot_info*)bs;

    argc--; argv++;

    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct ged* gedp = gb->gedp;
    const char* input_bot_name = gb->dp->d_namep;
    const char* output_bot_name;
    struct directory* dp_input;
    struct directory* dp_output;
    struct rt_bot_internal* input_bot;

    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    dp_input = dp_output = RT_DIR_NULL;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%s\n%s", usage_string, purpose_string);
	return BRLCAD_HELP;
    }

    /* check that we are using a version 5 database */
    if (db_version(gedp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str,
	    "ERROR: Unable to remesh the current (v%d) database.\n"
	    "Use \"dbupgrade\" to upgrade this database to the current version.\n",
	    db_version(gedp->dbip));
	return BRLCAD_ERROR;
    }

    if (argc > 3) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: unexpected arguments encountered\n");
	bu_vls_printf(gedp->ged_result_str, "%s\n%s", usage_string, purpose_string);
	return BRLCAD_ERROR;
    }

    output_bot_name = input_bot_name;
    if (argc > 1)
	output_bot_name = (char*)argv[1];

    if (!BU_STR_EQUAL(input_bot_name, output_bot_name)) {
	GED_CHECK_EXISTS(gedp, output_bot_name, LOOKUP_QUIET, BRLCAD_ERROR);
    }

    if (gb->intern->idb_major_type != DB5_MAJORTYPE_BRLCAD || gb->intern->idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a BOT primitive\n", input_bot_name);
	return BRLCAD_ERROR;
    }

    input_bot = (struct rt_bot_internal*)gb->intern->idb_ptr;
    RT_BOT_CK_MAGIC(input_bot);

    bu_log("INPUT BoT has %zu vertices and %zu faces\n", input_bot->num_vertices, input_bot->num_faces);

    /* TODO: stash a backup if overwriting the original */

    bool ok = bot_om(gedp, input_bot);
    if (!ok) {
	return BRLCAD_ERROR;
    }

    bu_log("OUTPUT BoT has %zu vertices and %zu faces\n", input_bot->num_vertices, input_bot->num_faces);

    if (BU_STR_EQUAL(input_bot_name, output_bot_name)) {
	dp_output = dp_input;
    }
    else {
	GED_DB_DIRADD(gedp, dp_output, output_bot_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void*)&gb->intern->idb_type, BRLCAD_ERROR);
    }

    GED_DB_PUT_INTERNAL(gedp, dp_output, gb->intern, gedp->ged_wdbp->wdb_resp, BRLCAD_ERROR);

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


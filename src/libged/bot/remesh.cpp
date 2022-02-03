/*                     B O T _ R E M E S H . C P P
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
/** @file libged/bot_remesh.cpp
 *
 * The bot "remesh" sub-command.
 *
 */

#include "common.h"

#ifdef OPENVDB_ABI_VERSION_NUMBER
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/VolumeToMesh.h>
#  include <openvdb/tools/MeshToVolume.h>
#endif /* OPENVDB_ABI_VERSION_NUMBER */

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


#ifdef OPENVDB_ABI_VERSION_NUMBER

struct botDataAdapter {
    struct rt_bot_internal *bot;

    size_t polygonCount() const {
	return bot->num_faces;
    };
    size_t pointCount() const {
	return bot->num_vertices;
    };
    size_t vertexCount(size_t) const {
	return 3;
    };
    void getIndexSpacePoint(size_t n, size_t v, openvdb::Vec3d& pos) const {
	int idx = bot->faces[(n*3)+v];
	pos[X] = bot->vertices[(idx*3)+X];
	pos[Y] = bot->vertices[(idx*3)+Y];
	pos[Z] = bot->vertices[(idx*3)+Z];
	return;
    };

    /* constructor */
    botDataAdapter(struct rt_bot_internal *bip) : bot(bip) {}
};


static bool
bot_remesh(struct ged *UNUSED(gedp), struct rt_bot_internal *bot, double voxelSize)
{
    const float exteriorBandWidth = 10.0;
    const float interiorBandWidth = std::numeric_limits<float>::max();

    struct botDataAdapter bda(bot);

    openvdb::initialize();

    bu_log("...voxelizing");

    openvdb::math::Transform::Ptr xform = openvdb::math::Transform::createLinearTransform(voxelSize);
    openvdb::FloatGrid::Ptr bot2vol = openvdb::tools::meshToVolume<openvdb::FloatGrid, botDataAdapter>(bda, *xform, exteriorBandWidth, interiorBandWidth);

#if 0
    openvdb::io::File file("mesh.vdb");
    openvdb::GridPtrVec grids;
    grids.push_back(bot2vol);
    file.write(grids);
    file.close();
    return false;
#endif

    bu_log("...devoxelizing");

    std::vector<openvdb::Vec3s> points;
    std::vector<openvdb::Vec3I> triangles;
    std::vector<openvdb::Vec4I> quadrilaterals;
    openvdb::tools::volumeToMesh<openvdb::FloatGrid>(*bot2vol, points, triangles, quadrilaterals);

    bu_log("...storing");

    if (bot->vertices) {
	bu_free(bot->vertices, "vertices");
	bot->num_vertices = 0;
    }
    if (bot->faces) {
	bu_free(bot->faces, "faces");
	bot->num_faces = 0;
    }
    if (bot->normals) {
	bu_free(bot->normals, "normals");
    }
    if (bot->face_normals) {
	bu_free(bot->face_normals, "face normals");
    }

    bot->num_vertices = points.size();
    bot->vertices = (fastf_t *)bu_malloc(bot->num_vertices * ELEMENTS_PER_POINT * sizeof(fastf_t), "vertices");
    for (size_t i = 0; i < points.size(); i++) {
	bot->vertices[(i*3)+X] = points[i].x();
	bot->vertices[(i*3)+Y] = points[i].y();
	bot->vertices[(i*3)+Z] = points[i].z();
    }
    bot->num_faces = triangles.size() + (quadrilaterals.size() * 2);
    bot->faces = (int *)bu_malloc(bot->num_faces * 3 * sizeof(int), "triangles");
    for (size_t i = 0; i < triangles.size(); i++) {
	bot->faces[(i*3)+X] = triangles[i].x();
	bot->faces[(i*3)+Y] = triangles[i].y();
	bot->faces[(i*3)+Z] = triangles[i].z();
    }
    size_t ntri = triangles.size();
    for (size_t i = 0; i < quadrilaterals.size(); i++) {
	bot->faces[((ntri+i)*3)+X] = quadrilaterals[i][0];
	bot->faces[((ntri+i)*3)+Y] = quadrilaterals[i][1];
	bot->faces[((ntri+i)*3)+Z] = quadrilaterals[i][2];

	bot->faces[((ntri+i+1)*3)+X] = quadrilaterals[i][0];
	bot->faces[((ntri+i+1)*3)+Y] = quadrilaterals[i][2];
	bot->faces[((ntri+i+1)*3)+Z] = quadrilaterals[i][3];
    }

    bu_log("...done!\n");

    return (points.size() > 0);
}

#else /* OPENVDB_ABI_VERSION_NUMBER */

static bool
bot_remesh(struct ged *gedp, struct rt_bot_internal *UNUSED(bot), double UNUSED(voxelSize))
{
    bu_vls_printf(gedp->ged_result_str,
		  "WARNING: BoT remeshing is unavailable.\n"
		  "BRL-CAD needs to be compiled with OpenVDB support.\n"
		  "(cmake -DBRLCAD_ENABLE_OPENVDB=ON)\n");
    return false;
}

#endif /* OPENVDB_ABI_VERSION_NUMBER */




extern "C" int
_bot_cmd_remesh(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] remesh <objname> <output_bot>";
    const char *purpose_string = "Store a remeshed version of the BoT in object <output_bot>";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct ged *gedp = gb->gedp;
    const char *input_bot_name = gb->dp->d_namep;
    const char *output_bot_name;
    struct directory *dp_input;
    struct directory *dp_output;
    struct rt_bot_internal *input_bot;

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
	output_bot_name = (char *)argv[1];

    if (!BU_STR_EQUAL(input_bot_name, output_bot_name)) {
	GED_CHECK_EXISTS(gedp, output_bot_name, LOOKUP_QUIET, BRLCAD_ERROR);
    }

    if (gb->intern->idb_major_type != DB5_MAJORTYPE_BRLCAD || gb->intern->idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a BOT primitive\n", input_bot_name);
	return BRLCAD_ERROR;
    }

    input_bot = (struct rt_bot_internal *)gb->intern->idb_ptr;
    RT_BOT_CK_MAGIC(input_bot);

    bu_log("INPUT BoT has %zu vertices and %zu faces\n", input_bot->num_vertices, input_bot->num_faces);

    /* TODO: stash a backup if overwriting the original */

    bool ok = bot_remesh(gedp, input_bot, 50);
    if (!ok) {
	return BRLCAD_ERROR;
    }

    bu_log("OUTPUT BoT has %zu vertices and %zu faces\n", input_bot->num_vertices, input_bot->num_faces);

    if (BU_STR_EQUAL(input_bot_name, output_bot_name)) {
	dp_output = dp_input;
    } else {
	GED_DB_DIRADD(gedp, dp_output, output_bot_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&gb->intern->idb_type, BRLCAD_ERROR);
    }

    GED_DB_PUT_INTERNAL(gedp, dp_output, gb->intern, gedp->ged_wdbp->wdb_resp, BRLCAD_ERROR);

    return BRLCAD_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

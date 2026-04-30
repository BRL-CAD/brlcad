/*                     B O T _ R E M E S H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2026 United States Government as represented by
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

#include "manifold/manifold.h"

#include "vmath.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bg/trimesh.h"
#include "rt/db5.h"
#include "rt/db_internal.h"
#include "rt/db_io.h"
#include "rt/geom.h"
#include "rt/wdb.h"
#include "ged/commands.h"
#include "ged/database.h"
#include "ged/objects.h"
#include "../ged_private.h"
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
bot_remesh_vdb(struct ged *UNUSED(gedp), struct rt_bot_internal *bot, double voxelSize)
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
bot_remesh_vdb(struct ged *gedp, struct rt_bot_internal *UNUSED(bot), double UNUSED(voxelSize))
{
    bu_vls_printf(gedp->ged_result_str,
		  "WARNING: BoT remeshing is unavailable.\n"
		  "BRL-CAD needs to be compiled with OpenVDB support.\n"
		  "(cmake -DBRLCAD_ENABLE_OPENVDB=ON)\n");
    return false;
}

#endif /* OPENVDB_ABI_VERSION_NUMBER */

static int
bot_remesh_geogram(struct rt_bot_internal **obot, struct ged *gedp, struct rt_bot_internal *bot)
{
    int *ifaces = bot->faces;
    int n_ifaces = (int)bot->num_faces;
    point_t *ipnts = (point_t *)bot->vertices;
    int n_ipnts = (int)bot->num_vertices;

    struct bg_trimesh_remesh_opts opts = BG_TRIMESH_REMESH_OPTS_DEFAULT;

    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;

    int ret = bg_trimesh_remesh(&ofaces, &n_ofaces, &opnts, &n_opnts,
				ifaces, n_ifaces, ipnts, n_ipnts, &opts);
    if (ret != 0 || !ofaces || !opnts) {
	bu_free(ofaces, "ofaces");
	bu_free(opnts, "opnts");
	bu_vls_printf(gedp->ged_result_str, "Remesh failed\n");
	return BRLCAD_ERROR;
    }

    /* Check if the remeshed result is a solid. */
    int bmode = RT_BOT_SURFACE;
    {
	manifold::MeshGL gmm;
	for (int i = 0; i < n_opnts; i++) {
	    gmm.vertProperties.push_back(opnts[i][X]);
	    gmm.vertProperties.push_back(opnts[i][Y]);
	    gmm.vertProperties.push_back(opnts[i][Z]);
	}
	for (int i = 0; i < n_ofaces; i++) {
	    gmm.triVerts.push_back((uint32_t)ofaces[3*i+0]);
	    gmm.triVerts.push_back((uint32_t)ofaces[3*i+1]);
	    gmm.triVerts.push_back((uint32_t)ofaces[3*i+2]);
	}
	manifold::Manifold gmanifold(gmm);
	if (gmanifold.Status() == manifold::Manifold::Error::NoError)
	    bmode = RT_BOT_SOLID;
    }

    struct rt_bot_internal *nbot;
    BU_GET(nbot, struct rt_bot_internal);
    nbot->magic = RT_BOT_INTERNAL_MAGIC;
    nbot->mode = bmode;
    nbot->orientation = RT_BOT_CCW;
    nbot->thickness = NULL;
    nbot->face_mode = (struct bu_bitv *)NULL;
    nbot->bot_flags = 0;
    nbot->num_vertices = n_opnts;
    nbot->num_faces = n_ofaces;
    nbot->vertices = (double *)calloc((size_t)n_opnts * 3, sizeof(double));
    nbot->faces = (int *)calloc((size_t)n_ofaces * 3, sizeof(int));

    for (int i = 0; i < n_opnts; i++) {
	nbot->vertices[3*i+0] = opnts[i][X];
	nbot->vertices[3*i+1] = opnts[i][Y];
	nbot->vertices[3*i+2] = opnts[i][Z];
    }
    for (int i = 0; i < n_ofaces * 3; i++)
	nbot->faces[i] = ofaces[i];

    bu_free(ofaces, "ofaces");
    bu_free(opnts, "opnts");

    *obot = nbot;
    bu_vls_sprintf(gedp->ged_result_str, "remeshed\n");
    return BRLCAD_OK;
}



extern "C" int
_bot_cmd_remesh(void *bs, int argc, const char **argv)
{
    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    struct ged *gedp = gb->gedp;

    const char *usage_string = "bot [options] remesh <objname> <output_bot>\n";
    const char *purpose_string = "Store a remeshed version of the BoT in object <output_bot>";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    /* check that we are using a version 5 database */
    if (db_version(gedp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str,
		      "ERROR: Unable to remesh the current (v%d) database.\n"
		      "Use \"dbupgrade\" to upgrade this database to the current version.\n",
		      db_version(gedp->dbip));
	return BRLCAD_ERROR;
    }

    int print_help = 0;
    int use_vdb = 0;
    struct bu_vls output_bot_name = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[7];
    BU_OPT(d[0], "h", "help",                  "",         NULL,  &print_help,      "Print help");
    BU_OPT(d[1], "o", "output",           "oname",  &bu_opt_vls,  &output_bot_name, "Name to use for output BoT");
    BU_OPT(d[2],  "", "vdb",                   "",         NULL,  &use_vdb,         "Use OpenVDB based remeshing");
    BU_OPT_NULL(d[3]);


    argc--; argv++;

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (print_help || !argc) {
	_ged_cmd_help(gedp, usage_string, d);
	bu_vls_free(&output_bot_name);
	return GED_HELP;
    }

    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	bu_vls_free(&output_bot_name);
	return BRLCAD_ERROR;
    }

    const char *input_bot_name = gb->dp->d_namep;
    if (gb->intern->idb_major_type != DB5_MAJORTYPE_BRLCAD || gb->intern->idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a BOT primitive\n", input_bot_name);
	bu_vls_free(&output_bot_name);
	return BRLCAD_ERROR;
    }

    struct directory *dp_input = RT_DIR_NULL;
    struct directory *dp_output = RT_DIR_NULL;

    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    if ((bu_vls_strlen(&output_bot_name) && argc > 1) || argc > 2) {
	bu_vls_printf(gedp->ged_result_str, "Unexpected arguments\n");
	bu_vls_free(&output_bot_name);
	return BRLCAD_ERROR;
    }

    if (!bu_vls_strlen(&output_bot_name) && argc == 2)
	bu_vls_printf(&output_bot_name, "%s", argv[1]);

    // If we've got no specified output, we're overwriting
    if (!bu_vls_strlen(&output_bot_name))
	bu_vls_printf(&output_bot_name, "%s", input_bot_name);


    if (!BU_STR_EQUAL(input_bot_name, bu_vls_cstr(&output_bot_name))) {
	GED_CHECK_EXISTS(gedp, bu_vls_cstr(&output_bot_name), LOOKUP_QUIET, BRLCAD_ERROR);
    }

    struct rt_bot_internal *input_bot = (struct rt_bot_internal *)gb->intern->idb_ptr;
    RT_BOT_CK_MAGIC(input_bot);

    bu_log("INPUT BoT has %zu vertices and %zu faces\n", input_bot->num_vertices, input_bot->num_faces);

    /* TODO: stash a backup if overwriting the original */

    if (use_vdb) {
	bool ok = bot_remesh_vdb(gedp, input_bot, 50);
	if (!ok) {
	    bu_vls_free(&output_bot_name);
	    return BRLCAD_ERROR;
	}
    } else {
	struct rt_bot_internal *obot = NULL;
	if (bot_remesh_geogram(&obot, gedp, input_bot) != BRLCAD_OK || !obot) {
	    bu_vls_free(&output_bot_name);
	    return BRLCAD_ERROR;
	}

	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BOT;
	intern.idb_meth = &OBJ[ID_BOT];
	intern.idb_ptr = (void *)obot;

	const char *rname = bu_vls_cstr(&output_bot_name);
	struct directory *dp = db_diradd(gedp->dbip, rname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
	if (dp == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Failed to write out new BoT %s\n", rname);
	    rt_db_free_internal(&intern);
	    bu_vls_free(&output_bot_name);
	    return BRLCAD_ERROR;
	}

	if (rt_db_put_internal(dp, gedp->dbip, &intern) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Failed to write out new BoT %s\n", rname);
	    rt_db_free_internal(&intern);
	    bu_vls_free(&output_bot_name);
	    return BRLCAD_ERROR;
	}

	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "Remesh complete\n");
	return BRLCAD_OK;
    }

    bu_log("OUTPUT BoT has %zu vertices and %zu faces\n", input_bot->num_vertices, input_bot->num_faces);

    if (BU_STR_EQUAL(input_bot_name, bu_vls_cstr(&output_bot_name))) {
	dp_output = dp_input;
    } else {
	GED_DB_DIRADD(gedp, dp_output, bu_vls_cstr(&output_bot_name), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&gb->intern->idb_type, BRLCAD_ERROR);
    }

    GED_DB_PUT_INTERN(gedp, dp_output, gb->intern, BRLCAD_ERROR);

    bu_vls_free(&output_bot_name);

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


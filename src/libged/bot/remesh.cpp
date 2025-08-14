/*                     B O T _ R E M E S H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2025 United States Government as represented by
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

#include "geogram/basic/process.h"
#include <geogram/basic/command_line.h>
#include <geogram/basic/command_line_args.h>
#include "geogram/mesh/mesh.h"
#include "geogram/mesh/mesh_geometry.h"
#include "geogram/mesh/mesh_preprocessing.h"
#include "geogram/mesh/mesh_remesh.h"

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

static void
geogram_to_manifold(manifold::MeshGL *gmm, GEO::Mesh &gm)
{
    for (GEO::index_t v = 0; v < gm.vertices.nb(); v++) {
	const double *p = gm.vertices.point_ptr(v);
	for (int i = 0; i < 3; i++)
	    gmm->vertProperties.insert(gmm->vertProperties.end(), p[i]);
    }
    for (GEO::index_t f = 0; f < gm.facets.nb(); f++) {
	for (int i = 0; i < 3; i++) {
	    // TODO - CW vs CCW orientation handling?
	    gmm->triVerts.insert(gmm->triVerts.end(), gm.facets.vertex(f, i));
	}
    }
}

static int
bot_remesh_geogram(struct rt_bot_internal **obot, struct ged *gedp, struct rt_bot_internal *bot)
{
    // Geogram libraries like to print a lot - shut down
    // the I/O channels until we can clear the logger
    int serr = -1;
    int sout = -1;
    int stderr_stashed = -1;
    int stdout_stashed = -1;
    int fnull = open("/dev/null", O_WRONLY);
    if (fnull == -1) {
	/* https://gcc.gnu.org/ml/gcc-patches/2005-05/msg01793.html */
	fnull = open("nul", O_WRONLY);
    }
    if (fnull != -1) {
	serr = fileno(stderr);
	sout = fileno(stdout);
	stderr_stashed = dup(serr);
	stdout_stashed = dup(sout);
	dup2(fnull, serr);
	dup2(fnull, sout);
	close(fnull);
    }

    // Make sure geogram is initialized
    GEO::initialize();

    // Quell logging messages
    GEO::Logger::instance()->unregister_all_clients();

    // Put I/O channels back where they belong
    if (fnull != -1) {
	fflush(stderr);
	dup2(stderr_stashed, serr);
	close(stderr_stashed);
	fflush(stdout);
	dup2(stdout_stashed, sout);
	close(stdout_stashed);
    }

    GEO::CmdLine::import_arg_group("standard");
    GEO::CmdLine::import_arg_group("algo");
    GEO::CmdLine::import_arg_group("remesh");

    // Target ten times the original vert count
    fastf_t nb_pts = bot->num_vertices * 10;
    std::string nbpts = std::to_string(nb_pts);
    GEO::CmdLine::set_arg("remesh:nb_pts", nbpts.c_str());

    // Initialize the Geogram mesh
    GEO::Mesh gm;
    gm.vertices.assign_points((double *)bot->vertices, 3, bot->num_vertices);
    for (size_t i = 0; i < bot->num_faces; i++) {
	GEO::index_t f = gm.facets.create_polygon(3);
	gm.facets.set_vertex(f, 0, bot->faces[3*i+0]);
	gm.facets.set_vertex(f, 1, bot->faces[3*i+1]);
	gm.facets.set_vertex(f, 2, bot->faces[3*i+2]);
    }

    // After the initial raw load, do a repair pass to set up
    // Geogram's internal mesh data
    double bbox_diag = GEO::bbox_diagonal(gm);
    double epsilon = 1e-6 * (0.01 * bbox_diag);
    GEO::mesh_repair(gm, GEO::MeshRepairMode(GEO::MESH_REPAIR_DEFAULT), epsilon);

    // https://github.com/BrunoLevy/geogram/wiki/Remeshing
    GEO::compute_normals(gm);
    set_anisotropy(gm, 2*0.02);
    GEO::Mesh remesh;
    GEO::remesh_smooth(gm, remesh, nb_pts);

    // See if we have a solid
    manifold::MeshGL gmm;
    geogram_to_manifold(&gmm, gm);
    manifold::Manifold gmanifold(gmm);
    int bmode = RT_BOT_SURFACE;
    if (gmanifold.Status() == manifold::Manifold::Error::NoError)
	bmode = RT_BOT_SOLID;

    struct rt_bot_internal *nbot;
    BU_GET(nbot, struct rt_bot_internal);
    nbot->magic = RT_BOT_INTERNAL_MAGIC;
    nbot->mode = bmode;
    nbot->orientation = RT_BOT_CCW;
    nbot->thickness = NULL;
    nbot->face_mode = (struct bu_bitv *)NULL;
    nbot->bot_flags = 0;
    nbot->num_vertices = (int)remesh.vertices.nb();
    nbot->num_faces = (int)remesh.facets.nb();
    nbot->vertices = (double *)calloc(nbot->num_vertices*3, sizeof(double));
    nbot->faces = (int *)calloc(nbot->num_faces*3, sizeof(int));

    int j = 0;
    for(GEO::index_t v = 0; v < remesh.vertices.nb(); v++) {
	double gm_v[3];
	const double *p = remesh.vertices.point_ptr(v);
	for (int i = 0; i < 3; i++)
	    gm_v[i] = p[i];
	nbot->vertices[3*j] = gm_v[0];
	nbot->vertices[3*j+1] = gm_v[1];
	nbot->vertices[3*j+2] = gm_v[2];
	j++;
    }

    j = 0;
    for (GEO::index_t f = 0; f < remesh.facets.nb(); f++) {
	double tri_verts[3];
	for (int i = 0; i < 3; i++)
	    tri_verts[i] = remesh.facets.vertex(f, i);
	// TODO - CW vs CCW orientation handling?
	nbot->faces[3*j] = tri_verts[0];
	nbot->faces[3*j+1] = tri_verts[1];
	nbot->faces[3*j+2] = tri_verts[2];
	j++;
    }

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
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

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

	if (rt_db_put_internal(dp, gedp->dbip, &intern, &rt_uniresource) < 0) {
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

    GED_DB_PUT_INTERNAL(gedp, dp_output, gb->intern, wdbp->wdb_resp, BRLCAD_ERROR);

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


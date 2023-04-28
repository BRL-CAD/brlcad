/*                  S M O O T H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2023 United States Government as represented by
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
 /** @file libged/bot/smooth.cpp
  *
  * Smooth a BoT using OpenMesh
  * based on remesh.cpp
  *
  */

#include "common.h"

#include <vector>
#include <unordered_map>

#include "vmath.h"
#include "bu/str.h"

#include "rt/db5.h"
#include "rt/db_internal.h"
#include "rt/db_io.h"
#include "rt/geom.h"
#include "rt/wdb.h"

#ifdef BUILD_OPENMESH_TOOLS
// Getting this definition from opennurbs_subd.h
#ifdef EdgeAttributes
#  undef EdgeAttributes
#endif

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push /* start new diagnostic pragma */
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#elif defined(__clang__)
#  pragma clang diagnostic push /* start new diagnostic pragma */
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wdocumentation"
#endif
#include "OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh"
#include "OpenMesh/Core/Utils/Property.hh"
#include "OpenMesh/Tools/Smoother/JacobiLaplaceSmootherT.hh"
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop /* end ignoring warnings */
#elif defined(__clang__)
#  pragma clang diagnostic pop /* end ignoring warnings */
#endif

#endif /* BUILD_OPENMESH_TOOLS */

#include "ged/commands.h"
#include "ged/database.h"
#include "ged/objects.h"
#include "./ged_bot.h"


#ifdef BUILD_OPENMESH_TOOLS

typedef OpenMesh::TriMesh_ArrayKernelT<>  TriMesh;

static struct rt_bot_internal *
bot_smooth(struct ged *gedp, struct rt_bot_internal *input_bot, int component, int continuity, double max_lerr, double max_aerr, int iterations)
{
    if (!gedp || !input_bot)
	return NULL;

    OpenMesh::TriMeshT<TriMesh> trimesh;
    std::unordered_map<size_t, OpenMesh::VertexHandle> vMap;

    /* vertices */
    const size_t num_v = input_bot->num_vertices;
    for (size_t i = 0; i < num_v; i++) {
	OpenMesh::Vec3f v(input_bot->vertices[3*i], input_bot->vertices[3*i + 1], input_bot->vertices[3*i + 2]);
	OpenMesh::VertexHandle handle = trimesh.add_vertex(v);
	vMap[i] = handle;
    }

    /* faces */
    const size_t num_f = input_bot->num_faces;
    for (size_t i = 0; i < num_f; i++) {
	std::vector<OpenMesh::VertexHandle> indices;
	for (int j = 0; j < 3; j++) {
	    OpenMesh::VertexHandle &handle = vMap[input_bot->faces[3*i + j]];
            indices.push_back(handle);
	}
	trimesh.add_face(indices);
    }

    OpenMesh::Smoother::SmootherT<TriMesh>::Component scpt;
    switch (component) {
	case 1:
	    scpt = OpenMesh::Smoother::SmootherT<TriMesh>::Component::Normal;
	    break;
	case 2:
	    scpt = OpenMesh::Smoother::SmootherT<TriMesh>::Component::Tangential_and_Normal;
	    break;
	default:
	    scpt = OpenMesh::Smoother::SmootherT<TriMesh>::Component::Tangential;
    }

    OpenMesh::Smoother::SmootherT<TriMesh>::Continuity scont;
    switch (continuity) {
	case 1:
	    scont = OpenMesh::Smoother::SmootherT<TriMesh>::Continuity::C1;
	    break;
	case 2:
	    scont = OpenMesh::Smoother::SmootherT<TriMesh>::Continuity::C2;
	    break;
	default:
	    scont = OpenMesh::Smoother::SmootherT<TriMesh>::Continuity::C0;
    }

    /* smooth */
    OpenMesh::Smoother::JacobiLaplaceSmootherT<TriMesh> smoother(trimesh);
    smoother.initialize(scpt, scont);
    if (max_lerr > 0)
	smoother.set_relative_local_error(max_lerr);
    if (max_aerr > 0)
	smoother.set_absolute_local_error(max_aerr);
    smoother.smooth(iterations);

    /* convert mesh back to bot */
    struct rt_bot_internal *obot = NULL;
    BU_ALLOC(obot, struct rt_bot_internal);
    obot->magic = RT_BOT_INTERNAL_MAGIC;
    obot->mode = RT_BOT_SOLID;
    obot->orientation = RT_BOT_UNORIENTED; // TODO
    obot->thickness = (fastf_t *)NULL;
    obot->face_mode = (struct bu_bitv *)NULL;


    /* Count active vertices and build a map from handles to vertex indices */
    size_t i = 0;
    int fcnt = 0;
    std::map<int, size_t> rMap;

    // Use a face iterator to iterate over all the faces and build a point map
    for (TriMesh::ConstFaceIter f_it = trimesh.faces_begin(); f_it != trimesh.faces_end(); ++f_it) {
	const OpenMesh::FaceHandle fh = *f_it;
	fcnt++;
	for (TriMesh::ConstFaceVertexIter v_it = trimesh.cfv_begin(fh); v_it.is_valid(); ++v_it) {
	    const OpenMesh::VertexHandle &handle = *v_it;
	    if (rMap.find(handle.idx()) == rMap.end()) {
		rMap[handle.idx()] = i;
		i++;
	    }
	}
    }

    /* Allocate vertex index array */
    obot->num_vertices = i;
    obot->vertices = (fastf_t*)bu_malloc(obot->num_vertices * 3 * sizeof(fastf_t), "vertices");
    obot->num_faces = fcnt;
    obot->faces = (int*)bu_malloc((obot->num_faces + 1) * 3 * sizeof(int), "triangles");

    /* Retrieve coordinate values */
    for (TriMesh::ConstFaceIter f_it = trimesh.faces_begin(); f_it != trimesh.faces_end(); ++f_it) {
	const OpenMesh::FaceHandle fh = *f_it;
	for (TriMesh::ConstFaceVertexIter v_it = trimesh.cfv_begin(fh); v_it.is_valid(); ++v_it) {
	    const OpenMesh::VertexHandle &handle = *v_it;
	    if (rMap.find(handle.idx()) != rMap.end()) {
		i = rMap[handle.idx()];
		TriMesh::Point p = trimesh.point(handle);
		obot->vertices[3*i+0] = p[0];
		obot->vertices[3*i+1] = p[1];
		obot->vertices[3*i+2] = p[2];
	    }
	}
    }

    /* Retrieve face vertex index references */
    fcnt = 0;
    for (TriMesh::ConstFaceIter f_it = trimesh.faces_begin(); f_it != trimesh.faces_end(); ++f_it) {
	const OpenMesh::FaceHandle fh = *f_it;
	int j = 0;
	for (TriMesh::ConstFaceVertexIter v_it = trimesh.cfv_begin(fh); v_it.is_valid(); ++v_it) {
	    const OpenMesh::VertexHandle &handle = *v_it;
	    int ind = (int)rMap[handle.idx()];
	    obot->faces[fcnt*3 + j] = ind;
	    j++;
	}
	fcnt++;
    }

    return obot;
}

#else /* BUILD_OPENMESH_TOOLS */

static bool
bot_smooth(struct ged *gedp, struct rt_bot_internal *UNUSED(input_bot), int UNUSED(component), int UNUSED(continuity))
{
    bu_vls_printf(gedp->ged_result_str,
	"WARNING: BoT OpenMesh subcommands are unavailable.\n"
	"BRL-CAD needs to be compiled with OpenMesh support.\n"
	"(cmake -DBRLCAD_ENABLE_OPENVDB=ON or set -DOPENMESH_ROOT=/path/to/openmesh)\n");
    return false;
}

#endif /* BUILD_OPENMESH_TOOLS */

static void
smooth_usage(struct bu_vls *str, const char *cmd, struct bu_opt_desc *d) {
    char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(str, "Usage: %s [options] input_bot [output_name]\n", cmd);
    if (option_help) {
	bu_vls_printf(str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }

    bu_vls_printf(str, "Available continuity options are:\n");
    bu_vls_printf(str, "    0 (C0: shape is continuous, but not the tangent - default)\n");
    bu_vls_printf(str, "    1 (C1: shape and tangent are continuous)\n");
    bu_vls_printf(str, "    2 (C2: preserve curvature)\n\n");

    bu_vls_printf(str, "Available component direction options are:\n");
    bu_vls_printf(str, "    0 (smooth in tangential direction - default)\n");
    bu_vls_printf(str, "    1 (smooth in normal direction)\n");
    bu_vls_printf(str, "    2 (smooth in both tangential and normal direction)\n");
}

extern "C" int
_bot_cmd_smooth(void* bs, int argc, const char** argv)
{
    struct _ged_bot_info* gb = (struct _ged_bot_info*)bs;
    struct ged* gedp = gb->gedp;
    struct db_i *dbip = gedp->dbip;
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    const char* usage_string = "bot [options] smooth [smooth_options] <objname> [output_name]";
    const char* purpose_string = "Smooth the BoT using OpenMesh's Jacobi Laplace smoother";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    int print_help = 0;
    int continuity = 2;
    int direction = 0;
    double max_lerror = 0;
    double max_aerror = 0;
    int iterations = 3;

    struct bu_opt_desc d[7];
    BU_OPT(d[0], "h", "help",              "",         NULL,      &print_help, "Print help");
    BU_OPT(d[1], "c", "continuity",       "#",  &bu_opt_int,      &continuity, "C0 (0), C1 (1) or C2 (2) continuity");
    BU_OPT(d[2], "d", "direction",        "#",  &bu_opt_int,      &direction,  "Tangential (0), Normal (1) or both (2)");
    BU_OPT(d[3], "e", "max-local-error",  "#",  &bu_opt_fastf_t,  &max_lerror, "Maximum local error");
    BU_OPT(d[4], "E", "max-abs-error",    "#",  &bu_opt_fastf_t,  &max_aerror, "Maximum absolute error");
    BU_OPT(d[5], "I", "iterations",       "#",  &bu_opt_int,      &iterations, "Number of times to apply smoothing");
    BU_OPT_NULL(d[6]);

    // We know we're the smooth command - start processing args
    argc--; argv++;

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (print_help || !argc) {
	smooth_usage(gedp->ged_result_str, "bot smooth", d);
	return GED_HELP;
    }

    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    const char* input_bot_name = gb->dp->d_namep;
    struct bu_vls output_bot_name = BU_VLS_INIT_ZERO;
    if (argc > 1) {
	bu_vls_sprintf(&output_bot_name, "%s", argv[1]);
    } else {
	bu_vls_sprintf(&output_bot_name, "%s-out_smooth.bot", input_bot_name);
    }

    GED_CHECK_EXISTS(gedp, bu_vls_cstr(&output_bot_name), LOOKUP_QUIET, BRLCAD_ERROR);

    struct rt_bot_internal *input_bot = (struct rt_bot_internal*)gb->intern->idb_ptr;
    RT_BOT_CK_MAGIC(input_bot);

    bu_log("INPUT BoT has %zu vertices and %zu faces\n", input_bot->num_vertices, input_bot->num_faces);

    struct rt_bot_internal *output_bot = bot_smooth(gedp, input_bot, continuity, direction, max_lerror, max_aerror, iterations);
    if (!output_bot) {
	bu_vls_free(&output_bot_name);
	return BRLCAD_ERROR;
    }

    bu_log("OUTPUT BoT has %zu vertices and %zu faces\n", output_bot->num_vertices, output_bot->num_faces);

    /* Export BOT as a new solid */
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = (void *)output_bot;

    struct directory *dp = db_diradd(dbip, bu_vls_cstr(&output_bot_name), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_free(&output_bot_name);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&output_bot_name);

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

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


/*                  S U B D I V I D E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2024 United States Government as represented by
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
 /** @file libged/bot/subdivide.cpp
  *
  * Subdivide a BoT using OpenMesh
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
#  pragma clang diagnostic ignored "-Wunused-variable"
#endif
#include "OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh"
#include "OpenMesh/Core/Utils/Property.hh"
#include "OpenMesh/Tools/Subdivider/Uniform/CatmullClarkT.hh"  // I believe this algorithm needs Quads?
#include "OpenMesh/Tools/Subdivider/Uniform/LoopT.hh"
#include "OpenMesh/Tools/Subdivider/Uniform/MidpointT.hh"
#include "OpenMesh/Tools/Subdivider/Uniform/ModifiedButterFlyT.hh"
#include "OpenMesh/Tools/Subdivider/Uniform/Sqrt3InterpolatingSubdividerLabsikGreinerT.hh"
#include "OpenMesh/Tools/Subdivider/Uniform/Sqrt3T.hh"
#include "OpenMesh/Tools/Subdivider/Uniform/SubdividerT.hh"
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
bot_subd(struct ged *gedp, struct rt_bot_internal *input_bot, int alg, int level)
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

    /* initialize subdivider */
    switch (alg) {
	case 1:
	    {
		OpenMesh::Subdivider::Uniform::LoopT<TriMesh> divider;
		divider.attach(trimesh);
		divider(level);
		divider.detach();
		break;
	    }
	case 2:
	    {
		OpenMesh::Subdivider::Uniform::Sqrt3T<TriMesh> divider;
		divider.attach(trimesh);
		divider(level);
		divider.detach();
		break;
	    }
	case 3:
	    {
		OpenMesh::Subdivider::Uniform::InterpolatingSqrt3LGT<TriMesh> divider;
		divider.attach(trimesh);
		divider(level);
		divider.detach();
		break;
	    }
	case 4:
	    {
		OpenMesh::Subdivider::Uniform::ModifiedButterflyT<TriMesh> divider;
		divider.attach(trimesh);
		divider(level);
		divider.detach();
		break;
	    }
	case 5:
	    {
		OpenMesh::Subdivider::Uniform::MidpointT<TriMesh> divider;
		divider.attach(trimesh);
		divider(level);
		divider.detach();
		break;
	    }

	default:
	    {
		OpenMesh::Subdivider::Uniform::LoopT<TriMesh> divider;
		divider.attach(trimesh);
		divider(level);
		divider.detach();
		break;
	    }
    }

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

static struct rt_bot_internal *
bot_subd(struct ged *gedp, struct rt_bot_internal *UNUSED(input_bot), int UNUSED(alg), int UNUSED(level))

{
    bu_vls_printf(gedp->ged_result_str,
	"WARNING: BoT OpenMesh subcommands are unavailable.\n"
	"BRL-CAD needs to be compiled with OpenMesh support.\n"
	"(cmake -DBRLCAD_ENABLE_OPENVDB=ON or set -DOPENMESH_ROOT=/path/to/openmesh)\n");
    return NULL;
}

#endif /* BUILD_OPENMESH_TOOLS */

static void
subd_usage(struct bu_vls *str, const char *cmd, struct bu_opt_desc *d) {
    char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(str, "Usage: %s [options] input_bot [output_name]\n", cmd);
    if (option_help) {
	bu_vls_printf(str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
    bu_vls_printf(str, "Available subdivision algorithm types are:\n");
    bu_vls_printf(str, "    1 (Loop - default)\n");
    bu_vls_printf(str, "    2 (Sqrt3)\n");
    bu_vls_printf(str, "    3 (Sqrt3 Interpolating)\n");
    bu_vls_printf(str, "    4 (Modified Butterfly)\n");
    bu_vls_printf(str, "    5 (Midpoint)\n");
    bu_vls_printf(str, "\n\n Note that the number of triangles generated grows *very* rapidly with increasing iteration counts - it is recommended that users start with small refinements on the target bot to get a feel for the tradeoff between triangle count and smoothness.\n");
}

extern "C" int
_bot_cmd_subd(void* bs, int argc, const char** argv)
{
    struct _ged_bot_info* gb = (struct _ged_bot_info*)bs;
    struct ged* gedp = gb->gedp;
    struct db_i *dbip = gedp->dbip;
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    const char* usage_string = "bot [options] subd [subd_options] <objname> [output_name]";
    const char* purpose_string = "Subdivide the BoT; default algorithm is Loop";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    int alg_id = 0;
    int print_help = 0;
    int level = 2;

    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",       "",         NULL,  &print_help, "Print help");
    BU_OPT(d[1], "A", "algorithm", "#",  &bu_opt_int,  &alg_id,     "Subdivision algorithm to use");
    BU_OPT(d[2], "l", "level",     "#",  &bu_opt_int,  &level,      "# of subdivision refinement iterations");
    BU_OPT_NULL(d[3]);

    // We know we're the subd command - start processing args
    argc--; argv++;

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (print_help || !argc) {
	subd_usage(gedp->ged_result_str, "bot subd", d);
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
	bu_vls_sprintf(&output_bot_name, "%s-out_subd.bot", input_bot_name);
    }

    GED_CHECK_EXISTS(gedp, bu_vls_cstr(&output_bot_name), LOOKUP_QUIET, BRLCAD_ERROR);

    struct rt_bot_internal *input_bot = (struct rt_bot_internal*)gb->intern->idb_ptr;
    RT_BOT_CK_MAGIC(input_bot);

    bu_log("INPUT BoT has %zu vertices and %zu faces\n", input_bot->num_vertices, input_bot->num_faces);

    struct rt_bot_internal *output_bot = bot_subd(gedp, input_bot, alg_id, level);
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


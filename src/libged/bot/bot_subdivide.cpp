/*                B O T _ S U B D I V I D E . C P P
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
 /** @file libged/bot/bot_subdivide.cpp
  *
  * Subdivide a BoT using OpenMesh
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
#include <OpenMesh/Tools/Subdivider/Uniform/SubdividerT.hh>
//#include <OpenMesh/Tools/Subdivider/Uniform/CatmullClarkT.hh>
#include <OpenMesh/Tools/Subdivider/Uniform/LoopT.hh>
#include <OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <vector>
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

typedef OpenMesh::TriMesh_ArrayKernelT<>  MyMesh;
//typedef OpenMesh::PolyMesh_ArrayKernelT<> MyMesh;   // needed for catmullclark?

static bool
bot_subd(struct ged* gedp, struct rt_bot_internal* bot, int alg)
{
    if (!gedp || !bot)
	return false;

    /* convert bot to OpenMesh 'mesh' */
    MyMesh mesh;
    /* generate vertices */
    const size_t num_v = bot->num_vertices;
    std::vector<MyMesh::VertexHandle> vhandle(num_v);
    for (size_t i = 0; i < num_v; i++) {
	//bu_vls_printf(gedp->ged_result_str, "vert[%d]: (%d, %d, %d)\n", i, bot->vertices[3 * i], bot->vertices[3 * i + 1], bot->vertices[3 * i + 2]);
	vhandle[i] = mesh.add_vertex(MyMesh::Point(bot->vertices[3 * i], bot->vertices[3 * i + 1], bot->vertices[3 * i + 2]));
    }
    /* generate faces */
    const size_t num_f = bot->num_faces;
    std::vector<MyMesh::VertexHandle> face_vhandles(3);
    for (size_t i = 0; i < num_f; i++) {
	//bu_vls_printf(gedp->ged_result_str, "face[%d]: (%d, %d, %d)\n", i, bot->faces[3 * i], bot->faces[3 * i + 1], bot->faces[3 * i + 2]);
	std::vector<MyMesh::VertexHandle> face_vhandles(3);
	face_vhandles[0] = vhandle[bot->faces[3 * i    ]];
	face_vhandles[1] = vhandle[bot->faces[3 * i + 1]];
	face_vhandles[2] = vhandle[bot->faces[3 * i + 2]];
	mesh.add_face(face_vhandles);
    }

    /* initialize subdivider */
    switch (alg) {
    case 1:
    {
	OpenMesh::Subdivider::Uniform::LoopT<MyMesh> divider;
	divider.attach(mesh);
	divider(2); // TODO: have option to set division step depth
	divider.detach();
	break;
    }
    case 0:
    default:
	//OpenMesh::Subdivider::Uniform::CatmullClarkT<MyMesh> catmull;
	//catmull.attach(mesh);
	//catmull(2); // TODO: have option to set division step depth
	//catmull.detach();
	break;
    }

    //OpenMesh::Subdivider::Uniform::CatmullClarkT<PolyMesh> catmull;
    //catmull.attach(mesh);
    //catmull(2);	// TODO: have option to set division step depth?
    //catmull.detach();

    /* convert mesh back to bot */
    bot->num_vertices = mesh.n_vertices();
    bot->vertices = (fastf_t*)bu_malloc(bot->num_vertices * ELEMENTS_PER_POINT * sizeof(fastf_t), "vertices");
    bot->num_faces = mesh.n_faces();
    bot->faces = (int*)bu_malloc(bot->num_faces * 3 * sizeof(int), "triangles");
    size_t i = 0;
    // use an edge iterator to iterate over all the edges
    for (MyMesh::EdgeIter eit = mesh.edges_begin(); eit != mesh.edges_end(); ++eit) {
	// check for boundary.  (one halfedge won't be valid if boundary)
	// note: you have to dereference the edge iterator
	if (!mesh.is_boundary(*eit)) {
	    MyMesh::VertexHandle vh = mesh.to_vertex_handle(mesh.halfedge_handle(*eit, 0));
	    MyMesh::Point p = mesh.point(vh);
	    bot->vertices[3 * i    ] = p[0];
	    bot->vertices[3 * i + 1] = p[1];
	    bot->vertices[3 * i + 2] = p[2];

	    /*MyMesh::FaceHandle fh = mesh.face_handle(mesh.halfedge_handle(*eit, 0));
	    MyMesh::Face f = mesh.face(fh);

	    MyMesh::Normal face1Norm = mesh.normal(fh);*/
	    
	    i++;
	}   // else is boundary. One of the half edges won't be valid
    }

    return true;
}

#else /* BUILD_OPENMESH_TOOLS */

static bool
bot_subd(struct ged* gedp, struct rt_bot_internal* UNUSED(bot), int UNUSED(alg))
{
    bu_vls_printf(gedp->ged_result_str,
	"WARNING: BoT OpenMesh subcommands are unavailable.\n"
	"BRL-CAD needs to be compiled with OpenMesh support.\n"
	"(cmake -DBRLCAD_ENABLE_OPENVDB=ON or set -DOPENMESH_ROOT=/path/to/openmesh)\n");
    return false;
}

#endif /* BUILD_OPENMESH_TOOLS */


extern "C" int
_bot_cmd_subd(void* bs, int argc, const char** argv)
{
    const char* usage_string = "bot [options] subd [algorithm] <objname> [output_name]";
    const char* purpose_string = "Subdivide the BoT; default algorithm is CatmullClark";
    const char* input_bot_name = NULL;
    const char* output_bot_name = NULL;
    struct directory* dp_input;
    struct directory* dp_output;
    struct rt_bot_internal* input_bot;

    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info* gb = (struct _ged_bot_info*)bs;
    struct ged* gedp = gb->gedp;

    /* check for incorrect invocation
     * in the form "subd [algorithm] input_bot_name [output_bot_name]"
     */
    if (argc < 2 || argc > 4) {
	bu_vls_printf(gedp->ged_result_str, "%s\n%s", usage_string, purpose_string);
	return BRLCAD_HELP;
    }

    /* strip argv[0] "subd" and continue */
    argc--; argv++;

    /* TODO: add a way to accomodate a user specifying a subdivison algorithm */

    if (_bot_obj_setup(gb, argv[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    dp_input = dp_output = RT_DIR_NULL;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* check that we are using a version 5 database */
    if (db_version(gedp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str,
	    "ERROR: Unable to remesh the current (v%d) database.\n"
	    "Use \"dbupgrade\" to upgrade this database to the current version.\n",
	    db_version(gedp->dbip));
	return BRLCAD_ERROR;
    }

    input_bot_name = gb->dp->d_namep;
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

    bool ok = bot_subd(gedp, input_bot, 0);
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

    /* TODO: FIXME - breaks if input_name == output_name */
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


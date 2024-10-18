/*                      P L A T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2024 United States Government as represented by
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
/** @file plate.cpp
 *
 * Given a plate mode bot, represent its volumetric thickness with
 * individual CSG primitives, then tessellate and combine them to
 * make an evaluated representation of the plate mode volume.
 */

#include "common.h"

#include <map>
#include <set>
#include <unordered_set>

// flag to enable writing debugging output in case of boolean failure
#define CHECK_INTERMEDIATES 1

#include "manifold/manifold.h"
#ifdef CHECK_INTERMEDIATES
#  include "manifold/meshIO.h"
#endif

#include "vmath.h"
#include "bu/time.h"
#include "bn/tol.h"
#include "bg/defines.h"
#include "rt/defines.h"
#include "rt/db5.h"
#include "rt/db_internal.h"
#include "rt/functab.h"
#include "rt/global.h"
#include "rt/nmg_conv.h"
#include "rt/primitives/bot.h"

static bool
bot_face_normal(vect_t *n, struct rt_bot_internal *bot, int i)
{
    vect_t a,b;

    /* sanity */
    if (!n || !bot || i < 0 || (size_t)i > bot->num_faces ||
	    bot->faces[i*3+2] < 0 || (size_t)bot->faces[i*3+2] > bot->num_vertices) {
	return false;
    }

    VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
    VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
    VCROSS(*n, a, b);
    VUNITIZE(*n);
    if (bot->orientation == RT_BOT_CW) {
	VREVERSE(*n, *n);
    }

    return true;
}

int
rt_bot_plate_to_vol(struct rt_bot_internal **obot, struct rt_bot_internal *bot, int round_outer_edges, int quiet_mode)
{
    if (!obot || !bot)
	return 1;

    if (bot->mode != RT_BOT_PLATE)
	return 1;

    // Check for at least 1 non-zero thickness, or there's no volume to define
    bool have_solid = false;
    for (size_t i = 0; i < bot->num_faces; i++) {
	if (bot->thickness[i] > VUNITIZE_TOL) {
	    have_solid = true;
	    break;
	}
    }

    // If there's no volume, there's nothing to produce
    if (!have_solid) {
	*obot = NULL;
	return 0;
    }

    // OK, we have volume.  Now we need to build up the manifold definition
    // using unioned CSG elements
    manifold::Manifold c;

    // Collect the active vertices and edges
    std::set<int> verts;
    std::map<int, double> verts_thickness;
    std::map<int, int> verts_fcnt;
    std::map<std::pair<int, int>, double> edges_thickness;
    std::map<std::pair<int, int>, int> edges_fcnt;
    std::map<std::pair<int, int>, int> edges_fmode;
    std::set<std::pair<int, int>> edges;
    for (size_t i = 0; i < bot->num_faces; i++) {
	point_t eind;
	// face_mode is a view dependent reporting option, where the full thickness is appended to the
	// hit point reported from the BoT.  Because a volumetric BoT cannot exhibit view dependent
	// behavior, we instead use the full thickness to encompass the maximal volume that might be
	// claimed by the plate mode raytracing depending on the direction of the ray.  This will change
	// the shotline thickness reported for any given ray, but provides a volume representative of
	// the space the plate mode may claim as part of its solidity.
	double fthickness = 0.5*bot->thickness[i];
	if (NEAR_ZERO(fthickness, SMALL_FASTF))
	    continue;
	for (int j = 0; j < 3; j++) {
	    verts.insert(bot->faces[i*3+j]);
	    verts_thickness[bot->faces[i*3+j]] += fthickness;
	    verts_fcnt[bot->faces[i*3+j]]++;
	    eind[j] = bot->faces[i*3+j];
	}
	int e11, e12, e21, e22, e31, e32;
	e11 = (eind[0] < eind[1]) ? eind[0] : eind[1];
	e12 = (eind[1] < eind[0]) ? eind[0] : eind[1];
	e21 = (eind[1] < eind[2]) ? eind[1] : eind[2];
	e22 = (eind[2] < eind[1]) ? eind[1] : eind[2];
	e31 = (eind[2] < eind[0]) ? eind[2] : eind[0];
	e32 = (eind[0] < eind[2]) ? eind[2] : eind[0];
	edges.insert(std::make_pair(e11, e12));
	edges.insert(std::make_pair(e21, e22));
	edges.insert(std::make_pair(e31, e32));
	edges_thickness[std::make_pair(e11, e12)] += fthickness;
	edges_thickness[std::make_pair(e21, e22)] += fthickness;
	edges_thickness[std::make_pair(e31, e32)] += fthickness;
	edges_fcnt[std::make_pair(e11, e12)]++;
	edges_fcnt[std::make_pair(e21, e22)]++;
	edges_fcnt[std::make_pair(e31, e32)]++;
    }

    // Any edge with only one face associated with it is an outer
    // edge.  Vertices on those edges are exterior vertices
    std::unordered_set<int> exterior_verts;
    std::set<std::pair<int, int>>::iterator e_it;
    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
	if (edges_fcnt[*e_it] == 1) {
	    exterior_verts.insert(e_it->first);
	    exterior_verts.insert(e_it->second);
	}
    }

    std::set<int>::iterator v_it;
    if (!quiet_mode)
	bu_log("Processing %zd vertices... \n" , verts.size());
    for (v_it = verts.begin(); v_it != verts.end(); v_it++) {

	if (!round_outer_edges) {
	    if (exterior_verts.find(*v_it) != exterior_verts.end())
		continue;
	}

	manifold::Manifold left = c;

	point_t v;
	double r = ((double)verts_thickness[*v_it]/(double)(verts_fcnt[*v_it]));
	// Make a sph at the vertex point with a radius based on the thickness
	VMOVE(v, &bot->vertices[3**v_it]);

	manifold::Manifold sph = manifold::Manifold::Sphere(r, 8);
	manifold::Manifold right = sph.Translate(glm::vec3(v[0], v[1], v[2]));

	try {
	    c = left.Boolean(right, manifold::OpType::Add);
#if defined(CHECK_INTERMEDIATES)
	    c.GetMesh();
#endif
	} catch (const std::exception &e) {
	    if (!quiet_mode) {
		bu_log("Vertices - manifold boolean op failure\n");
		std::cerr << e.what() << "\n";
	    }
#if defined(CHECK_INTERMEDIATES)
	    manifold::ExportMesh(std::string("left.glb"), left.GetMesh(), {});
	    manifold::ExportMesh(std::string("right.glb"), right.GetMesh(), {});
	    bu_exit(EXIT_FAILURE, "halting on boolean failure");
#endif
	    return -1;
	}
    }
    if (!quiet_mode)
	bu_log("Processing %zd vertices... done.\n" , verts.size());

    size_t ecnt = 0;
    int64_t start = bu_gettime();
    int64_t elapsed = 0;
    fastf_t seconds = 0.0;
    if (!quiet_mode)
	bu_log("Processing %zd edges... \n" , edges.size());
    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {

	if (!round_outer_edges) {
	    if (edges_fcnt[*e_it] == 1)
		continue;
	}

	manifold::Manifold left = c;

	// Make an rcc along the edge with a radius based on the thickness
	double r = ((double)edges_thickness[*e_it]/(double)(edges_fcnt[*e_it]));
	point_t base, v;
	VMOVE(base, &bot->vertices[3*(*e_it).first]);
	VMOVE(v, &bot->vertices[3*(*e_it).second]);

	vect_t edge;
	VSUB2(edge, v, base);

	// Create a cylinder at the origin in the +z direction
	manifold::Manifold origin_cyl = manifold::Manifold::Cylinder(MAGNITUDE(edge), r, r, 8);

	// Move it into position
	glm::vec3 evec(-1*edge[0], -1*edge[1], edge[2]);
	manifold::Manifold rotated_cyl = origin_cyl.Transform(manifold::RotateUp(evec));
	manifold::Manifold right = rotated_cyl.Translate(glm::vec3(base[0], base[1], base[2]));

	try {
	    c = left.Boolean(right, manifold::OpType::Add);
#if defined(CHECK_INTERMEDIATES)
	    c.GetMesh();
#endif
	} catch (const std::exception &e) {
	    if (!quiet_mode) {
		bu_log("Edges - manifold boolean op failure\n");
		std::cerr << e.what() << "\n";
	    }
#if defined(CHECK_INTERMEDIATES)
	    manifold::ExportMesh(std::string("left.glb"), left.GetMesh(), {});
	    manifold::ExportMesh(std::string("right.glb"), right.GetMesh(), {});
	    bu_exit(EXIT_FAILURE, "halting on boolean failure");
#endif
	    return -1;
	}

	ecnt++;
	elapsed = bu_gettime() - start;
	seconds = elapsed / 1000000.0;

	if (seconds > 5) {
	    start = bu_gettime();
	    if (!quiet_mode)
		bu_log("Processed %zd of %zd edges\n", ecnt, edges.size());
	}
    }
    if (!quiet_mode)
	bu_log("Processing %zd edges... done.\n" , edges.size());

    // Now, handle the primary arb faces
    size_t fcnt = 0;
    start = bu_gettime();
    if (!quiet_mode)
	bu_log("Processing %zd faces...\n" , bot->num_faces);
    for (size_t i = 0; i < bot->num_faces; i++) {
	double fthickness = 0.5*bot->thickness[i];
	if (NEAR_ZERO(fthickness, SMALL_FASTF))
	    continue;
	point_t pnts[6];
	point_t pf[3];
	vect_t pv1[3], pv2[3];
	vect_t n = VINIT_ZERO;
	bot_face_normal(&n, bot, i);

	for (int j = 0; j < 3; j++) {
	    VMOVE(pf[j], &bot->vertices[bot->faces[i*3+j]*3]);
	    VSCALE(pv1[j], n, fthickness);
	    VSCALE(pv2[j], n, -1*fthickness);
	}

	for (int j = 0; j < 3; j++) {
	    point_t npnt1;
	    point_t npnt2;
	    VADD2(npnt1, pf[j], pv1[j]);
	    VADD2(npnt2, pf[j], pv2[j]);
	    VMOVE(pnts[j], npnt1);
	    VMOVE(pnts[j+3], npnt2);
	}

	// Use arb6 point order
	fastf_t pts[3*6];
	/* 1 */ pts[0] = pnts[4][X]; pts[1] = pnts[4][Y]; pts[2] = pnts[4][Z];
	/* 2 */ pts[3] = pnts[3][X]; pts[4] = pnts[3][Y]; pts[5] = pnts[3][Z];
	/* 3 */ pts[6] = pnts[0][X]; pts[7] = pnts[0][Y]; pts[8] = pnts[0][Z];
	/* 4 */ pts[9] = pnts[1][X]; pts[10] = pnts[1][Y]; pts[11] = pnts[1][Z];
	/* 5 */ pts[12] = pnts[5][X]; pts[13] = pnts[5][Y]; pts[14] = pnts[5][Z];
	/* 6 */ pts[15] = pnts[2][X]; pts[16] = pnts[2][Y]; pts[17] = pnts[2][Z];

	int faces[24];
	faces[ 0] = 0; faces[ 1] = 1; faces[ 2] = 4;  // 1 2 5
	faces[ 3] = 2; faces[ 4] = 3; faces[ 5] = 5;  // 3 4 6
	faces[ 6] = 1; faces[ 7] = 0; faces[ 8] = 3;  // 2 1 4
	faces[ 9] = 3; faces[10] = 2; faces[11] = 1;  // 4 3 2
	faces[12] = 3; faces[13] = 0; faces[14] = 4;  // 4 1 5
	faces[15] = 4; faces[16] = 5; faces[17] = 3;  // 5 6 4
	faces[18] = 5; faces[19] = 4; faces[20] = 1;  // 6 5 2
	faces[21] = 1; faces[22] = 2; faces[23] = 5;  // 2 3 6

#if 0
	bu_log("title {face}\n");
	bu_log("units mm\n");
	struct bu_vls vstr = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&vstr, "put {face.bot} bot mode volume orient rh flags {} V { ");
	for (int il = 0; il < 6; il++) {
	    bu_vls_printf(&vstr, " { %g %g %g } ", V3ARGS(((point_t *)pts)[il]));
	}
	bu_vls_printf(&vstr, "} F { ");
	for (int il = 0; il < 8; il++) {
	    bu_vls_printf(&vstr, " { %d %d %d } ", faces[il*3], faces[il*3+1], faces[il*3+2]);
	}
	bu_vls_printf(&vstr, "}\n");
	bu_log("%s\n", bu_vls_cstr(&vstr));
	bu_vls_free(&vstr);

	bu_exit(EXIT_FAILURE, "test");
#endif

	manifold::Mesh arb_m;
	for (size_t j = 0; j < 6; j++)
	    arb_m.vertPos.push_back(glm::vec3(pts[3*j], pts[3*j+1], pts[3*j+2]));
	for (size_t j = 0; j < 8; j++)
	    arb_m.triVerts.push_back(glm::ivec3(faces[3*j], faces[3*j+1], faces[3*j+2]));

	manifold::Manifold left = c;
	manifold::Manifold right(arb_m);

	try {
	    c = left.Boolean(right, manifold::OpType::Add);
#if defined(CHECK_INTERMEDIATES)
	    c.GetMesh();
#endif
	} catch (const std::exception &e) {
	    if (!quiet_mode) {
		bu_log("Faces - manifold boolean op failure\n");
		std::cerr << e.what() << "\n";
	    }
#if defined(CHECK_INTERMEDIATES)
	    manifold::ExportMesh(std::string("left.glb"), left.GetMesh(), {});
	    manifold::ExportMesh(std::string("right.glb"), right.GetMesh(), {});
	    bu_exit(EXIT_FAILURE, "halting on boolean failure");
#endif
	    return -1;
	}

	fcnt++;
	elapsed = bu_gettime() - start;
	seconds = elapsed / 1000000.0;

	if (seconds > 5) {
	    start = bu_gettime();
	    if (!quiet_mode)
		bu_log("Processed %zd of %zd faces\n", fcnt, bot->num_faces);
	}
    }
    if (!quiet_mode)
	bu_log("Processing %zd faces... done.\n" , bot->num_faces);

    manifold::Mesh rmesh = c.GetMesh();
    struct rt_bot_internal *rbot;
    BU_GET(rbot, struct rt_bot_internal);
    rbot->magic = RT_BOT_INTERNAL_MAGIC;
    rbot->mode = RT_BOT_SOLID;
    rbot->orientation = RT_BOT_CCW;
    rbot->thickness = NULL;
    rbot->face_mode = (struct bu_bitv *)NULL;
    rbot->bot_flags = 0;
    rbot->num_vertices = (int)rmesh.vertPos.size();
    rbot->num_faces = (int)rmesh.triVerts.size();
    rbot->vertices = (double *)calloc(rmesh.vertPos.size()*3, sizeof(double));;
    rbot->faces = (int *)calloc(rmesh.triVerts.size()*3, sizeof(int));
    for (size_t j = 0; j < rmesh.vertPos.size(); j++) {
	rbot->vertices[3*j] = rmesh.vertPos[j].x;
	rbot->vertices[3*j+1] = rmesh.vertPos[j].y;
	rbot->vertices[3*j+2] = rmesh.vertPos[j].z;
    }
    for (size_t j = 0; j < rmesh.triVerts.size(); j++) {
	rbot->faces[3*j] = rmesh.triVerts[j].x;
	rbot->faces[3*j+1] = rmesh.triVerts[j].y;
	rbot->faces[3*j+2] = rmesh.triVerts[j].z;
    }
    *obot = rbot;
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

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

#include "manifold/manifold.h"

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

#define MAX_CYL_STEPS 100
static int
edge_cyl(point_t **verts, int **faces, int *vert_cnt, int *face_cnt, point_t p1, point_t p2, fastf_t r)
{
    if (!verts || !faces || !vert_cnt || !face_cnt || NEAR_ZERO(r, SMALL_FASTF))
	return -1;

    int nsegs = 8;
    vect_t h;
    VSUB2(h, p2, p1);
    if (MAGNITUDE(h) < VUNITIZE_TOL)
	return -1;

    // Find axis to use for point generation on circles
    vect_t cross1, cross2, xaxis, yaxis;
    bn_vec_ortho(cross1, h);
    VUNITIZE(cross1);
    VCROSS(cross2, cross1, h);
    VUNITIZE(cross2);
    VSCALE(xaxis, cross1, r);
    VSCALE(yaxis, cross2, r);

    // Figure out the step we take for each ring in the cylinder
    double seg_len = M_PI * r * r / (double)nsegs;
    fastf_t e_len = MAGNITUDE(h);
    int steps = 1;
    fastf_t h_len = (e_len < 2*seg_len) ? e_len : -1.0;
    if (h_len < 0) {
	steps = (int)(e_len / seg_len);
	// Ideally we want the height of the triangles up the cylinder to be on
	// the same order as the segment size, but that's likely to make for a
	// huge number of triangles if we have a long edge.
	if (steps > MAX_CYL_STEPS)
	    steps = MAX_CYL_STEPS;
	h_len = e_len / (double)steps;
    }
    vect_t h_step;
    VMOVE(h_step, h);
    VUNITIZE(h_step);
    VSCALE(h_step, h_step, h_len);

    // Generated the vertices
    (*verts) = (point_t *)bu_calloc((steps+1) * nsegs + 2, sizeof(point_t), "verts");
    for (int i = 0; i <= steps; i++) {
	for (int j = 0; j < nsegs; j++) {
	    double alpha = M_2PI * (double)(2*j+1)/(double)(2*nsegs);
	    double sin_alpha = sin(alpha);
	    double cos_alpha = cos(alpha);
	    /* vertex geometry */
	    VJOIN3((*verts)[i*nsegs+j], p1, (double)i, h_step, cos_alpha, xaxis, sin_alpha, yaxis);
	}
    }

    // The two center points of the end caps are the last two points
    VMOVE((*verts)[(steps+1) * nsegs], p1);
    VMOVE((*verts)[(steps+1) * nsegs + 1], p2);

    // Next, we define the faces.  The two end caps each have one triangle for each segment.
    // Each step defines 2*nseg triangles.
    (*faces) = (int *)bu_calloc(steps * 2*nsegs + 2*nsegs, 3*sizeof(int), "triangles");

    // For the steps, we process in quads - each segment gets two triangles
    for (int i = 0; i < steps; i++) {
	for (int j = 0; j < nsegs; j++) {
	    int pnts[4];
	    pnts[0] = nsegs * i + j;
	    pnts[1] = (j < nsegs - 1) ? nsegs * i + j + 1 : nsegs * i;
	    pnts[2] = nsegs * (i + 1) + j;
	    pnts[3] = (j < nsegs - 1) ? nsegs * (i + 1) + j + 1 : nsegs * (i + 1);
	    int offset = 3 * (i * nsegs * 2 + j * 2);
	    (*faces)[offset + 0] = pnts[0];
	    (*faces)[offset + 1] = pnts[2];
	    (*faces)[offset + 2] = pnts[1];
	    (*faces)[offset + 3] = pnts[2];
	    (*faces)[offset + 4] = pnts[3];
	    (*faces)[offset + 5] = pnts[1];
	}
    }

    // Define the end caps.  The first set of triangles uses the base
    // point (stored at verts[steps*nsegs] and the points of the first
    // circle (stored at the beginning of verts)
    int offset = 3 * (steps * nsegs * 2);
    for (int j = 0; j < nsegs; j++){
	int pnts[3];
	pnts[0] = (steps+1) * nsegs;
	pnts[1] = j;
	pnts[2] = (j < nsegs - 1) ? j + 1 : 0;
	(*faces)[offset + 3*j + 0] = pnts[0];
	(*faces)[offset + 3*j + 1] = pnts[1];
	(*faces)[offset + 3*j + 2] = pnts[2];
    }
    // The second set of cap triangles uses the second edge point
    // point (stored at verts[steps*nsegs+1] and the points of the last
    // circle (stored at the end of verts = (steps-1) * nsegs)
    offset = 3 * ((steps * nsegs * 2) + nsegs);
    for (int j = 0; j < nsegs; j++){
	int pnts[3];
	pnts[0] = (steps+1) * nsegs + 1;
	pnts[1] = steps * nsegs + j;
	pnts[2] = (j < nsegs - 1) ? steps * nsegs + j + 1 : steps * nsegs;
	(*faces)[offset + 3*j + 0] = pnts[0];
	(*faces)[offset + 3*j + 1] = pnts[2];
	(*faces)[offset + 3*j + 2] = pnts[1];
    }

    (*vert_cnt) = (steps+1) * nsegs + 2;
    (*face_cnt) = steps * 2*nsegs + 2*nsegs;

#if 0
    bu_log("title {edge}\n");
    bu_log("units mm\n");
    struct bu_vls vstr = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vstr, "put {edge.bot} bot mode volume orient rh flags {} V { ");
    for (int i = 0; i < vert_cnt; i++) {
	bu_vls_printf(&vstr, " { %g %g %g } ", V3ARGS((*verts)[i]));
    }
    bu_vls_printf(&vstr, "} F { ");
    for (int i = 0; i < face_cnt; i++) {
	bu_vls_printf(&vstr, " { %d %d %d } ", faces[i*3], faces[i*3+1], faces[i*3+2]);
    }
    bu_vls_printf(&vstr, "}\n");
    bu_log("%s\n", bu_vls_cstr(&vstr));
    bu_vls_free(&vstr);

    bu_exit(EXIT_FAILURE, "test");
#endif

    return 0;
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
    std::set<std::pair<int, int>> edges;
    for (size_t i = 0; i < bot->num_faces; i++) {
	point_t eind;
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

	point_t v;
	double r = ((double)verts_thickness[*v_it]/(double)(verts_fcnt[*v_it]));
	// Make a sph at the vertex point with a radius based on the thickness
	VMOVE(v, &bot->vertices[3**v_it]);

	manifold::Manifold sph = manifold::Manifold::Sphere(r, 8);
	manifold::Manifold right = sph.Translate(la::vec3(v[0], v[1], v[2]));

	try {
	    c += right;
	} catch (const std::exception &e) {
	    if (!quiet_mode) {
		bu_log("Vertices - manifold boolean op failure\n");
		std::cerr << e.what() << "\n";
	    }
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

	// Make an rcc along the edge a radius based on the thickness
	double r = ((double)edges_thickness[*e_it]/(double)(edges_fcnt[*e_it]));
	point_t base, v;
	VMOVE(base, &bot->vertices[3*(*e_it).first]);
	VMOVE(v, &bot->vertices[3*(*e_it).second]);

	point_t *vertices = NULL;
	int *faces = NULL;
	int vert_cnt, face_cnt;
	if (edge_cyl(&vertices, &faces, &vert_cnt, &face_cnt, base, v, r))
	    continue;

	manifold::MeshGL64 rcc_m;
	for (int j = 0; j < vert_cnt; j++) {
	    rcc_m.vertProperties.insert(rcc_m.vertProperties.end(), vertices[j][X]);
	    rcc_m.vertProperties.insert(rcc_m.vertProperties.end(), vertices[j][Y]);
	    rcc_m.vertProperties.insert(rcc_m.vertProperties.end(), vertices[j][Z]);
	}
	for (int j = 0; j < face_cnt; j++) {
	    rcc_m.triVerts.insert(rcc_m.triVerts.end(), faces[3*j]);
	    rcc_m.triVerts.insert(rcc_m.triVerts.end(), faces[3*j+1]);
	    rcc_m.triVerts.insert(rcc_m.triVerts.end(), faces[3*j+2]);
	}

	if (vertices)
	    bu_free(vertices, "verts");
	if (faces)
	    bu_free(faces, "faces");

	manifold::Manifold right(rcc_m);
	try {
	    c += right;
	} catch (const std::exception &e) {
	    if (!quiet_mode) {
		bu_log("Edges - manifold boolean op failure\n");
		std::cerr << e.what() << "\n";
	    }
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

	manifold::Mesh arb_m;
	for (size_t j = 0; j < 6; j++)
	    arb_m.vertPos.push_back(la::vec3(pts[3*j], pts[3*j+1], pts[3*j+2]));
	for (size_t j = 0; j < 8; j++)
	    arb_m.triVerts.push_back(la::ivec3(faces[3*j], faces[3*j+1], faces[3*j+2]));

	manifold::Manifold right(arb_m);

	try {
	    c += right;
	} catch (const std::exception &e) {
	    if (!quiet_mode) {
		bu_log("Faces - manifold boolean op failure\n");
		std::cerr << e.what() << "\n";
	    }
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

    manifold::MeshGL64 rmesh = c.GetMeshGL64();
    struct rt_bot_internal *rbot;
    BU_GET(rbot, struct rt_bot_internal);
    rbot->magic = RT_BOT_INTERNAL_MAGIC;
    rbot->mode = RT_BOT_SOLID;
    rbot->orientation = RT_BOT_CCW;
    rbot->thickness = NULL;
    rbot->face_mode = (struct bu_bitv *)NULL;
    rbot->bot_flags = 0;
    rbot->num_vertices = (int)rmesh.vertProperties.size()/3;
    rbot->num_faces = (int)rmesh.triVerts.size()/3;
    rbot->vertices = (double *)calloc(rmesh.vertProperties.size(), sizeof(double));;
    rbot->faces = (int *)calloc(rmesh.triVerts.size(), sizeof(int));
    for (size_t j = 0; j < rmesh.vertProperties.size(); j++)
	rbot->vertices[j] = rmesh.vertProperties[j];
    for (size_t j = 0; j < rmesh.triVerts.size(); j++)
	rbot->faces[j] = rmesh.triVerts[j];
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

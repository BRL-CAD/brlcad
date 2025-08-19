/*                      P L A T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2025 United States Government as represented by
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
 * Given a plate mode bot, represent its volumetric thickness with individual
 * CSG primitives, then tessellate and combine them to make an evaluated
 * representation of the plate mode volume.
 *
 * NOTE: older Manifold versions would ignore "error" empty manifolds and
 * proceed with the boolean by treating them as empty inputs, but newer
 * versions propagate the error - this means we need to catch cases where our
 * inputs wind up not giving us a manifold (feeding in degenerate edge
 * segments, for example) and NOT use them. If we don't catch them, we wind up
 * wiping out some of our output geometry with the errors.
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
#include "bg/trimesh.h"
#include "rt/defines.h"
#include "rt/db5.h"
#include "rt/db_internal.h"
#include "rt/functab.h"
#include "rt/global.h"
#include "rt/nmg_conv.h"
#include "rt/primitives/bot.h"

// TODO - newer Manifold has a way to generate cylinders, added for a regression
// test related to this code - we may be able to replace edge_cyl with the Manifold
// version and simplify our own code.

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

static struct rt_bot_internal *
collapse_faces(struct rt_bot_internal *bot, fastf_t working_tol)
{
    if (NEAR_ZERO(working_tol, VUNITIZE_TOL))
	return bot;

    // Do the initial decimation
    int *ofaces = NULL;
    int n_ofaces = 0;
    struct bg_trimesh_decimation_settings s = BG_TRIMESH_DECIMATION_SETTINGS_INIT;
    s.feature_size = working_tol;
    int ret = bg_trimesh_decimate(&ofaces, &n_ofaces, bot->faces, (int)bot->num_faces, (point_t *)bot->vertices, (int)bot->num_vertices, &s);
    if (bu_vls_strlen(&s.msgs)) {
	//bu_log("%s", bu_vls_cstr(&s.msgs));
    }
    bu_vls_free(&s.msgs);

    // If didn't work, we're done
    if (ret != BRLCAD_OK) {
	bu_free(ofaces, "ofaces");
	return bot;
    }

    // bg_trimesh_decimate will filter out degenerate faces, but we still need
    // to trim out any unused points.
    int *gcfaces = NULL;
    point_t *opnts = NULL;
    int n_opnts;
    int n_gcfaces = bg_trimesh_3d_gc(&gcfaces, &opnts, &n_opnts, ofaces, n_ofaces, (point_t *)bot->vertices);

    // If garbage collection didn't work, we're done
    if (n_gcfaces != n_ofaces) {
	bu_free(gcfaces, "gcfaces");
	bu_free(opnts , "opnts");
	bu_free(ofaces, "ofaces");
	return bot;
    }

    // We have our new input bot
    //bu_log("New input BoT has %d vertices and %d faces, merge_tol = %f\n", n_opnts, n_gcfaces, s.feature_size);

    // Indices may be updated after gc, so the old array is obsolete
    bu_free(ofaces, "ofaces");

    // New input bot
    struct rt_bot_internal *input_bot;
    BU_ALLOC(input_bot, struct rt_bot_internal);
    input_bot->magic = RT_BOT_INTERNAL_MAGIC;
    input_bot->mode = input_bot->mode;
    // We're not mapping plate mode thickness info at the moment, so we
    // can't persist a plate mode type
    if (input_bot->mode == RT_BOT_PLATE || input_bot->mode == RT_BOT_PLATE_NOCOS)
	input_bot->mode = RT_BOT_SURFACE;
    input_bot->orientation = input_bot->orientation;
    // We changed the faces, but we still need to set their thicknesses
    input_bot->thickness = (fastf_t *)bu_calloc(n_ofaces, sizeof(fastf_t), "thickness array");
    for (int i = 0; i < n_ofaces; i++)
	input_bot->thickness[i] = bot->thickness[0];
    input_bot->face_mode = NULL; // Face mode doesn't matter here - we can only extrude using one method
    input_bot->num_faces = n_ofaces;
    input_bot->num_vertices = n_opnts;
    input_bot->faces = gcfaces;
    input_bot->vertices = (fastf_t *)opnts;

    return input_bot;
}

int
rt_bot_plate_to_vol(struct rt_bot_internal **obot, struct rt_bot_internal *input_bot, int round_outer_edges, int quiet_mode, fastf_t max_area_delta, fastf_t min_tri_threshold)
{
    double mtol = std::numeric_limits<float>::min();
    struct rt_bot_internal *bot = input_bot;
    if (!obot || !bot)
	return 1;

    if (bot->mode != RT_BOT_PLATE)
	return 1;

    // Check for at least 1 non-zero thickness, or there's no volume to define.
    // While we're at it see if we have variable thickness - if we do, we can't
    // simplify the BoT up front.
    bool have_solid = false;
    bool is_uniform = true;
    for (size_t i = 0; i < bot->num_faces; i++) {
	if (bot->thickness[i] > VUNITIZE_TOL)
	    have_solid = true;
	if (!NEAR_EQUAL(bot->thickness[i], bot->thickness[0], VUNITIZE_TOL))
	    is_uniform = false;
    }

    // If there's no volume, there's nothing to produce
    if (!have_solid) {
	*obot = NULL;
	return 0;
    }

    // If we have a non-zero max_area_delta and uniform thickness,
    // pre-process the mesh to try and filter out near degenerate inputs.
    // Those can have serious boolean evaluation performance implications and
    // contribute virtually nothing to the final shape (such features also
    // seriously inflate triangle counts for essentially no gain.)
    if (is_uniform && !NEAR_ZERO(max_area_delta, VUNITIZE_TOL)) {

	// Calculate our initial tolerance using one tenth of the length of the
	// diagonal of the bounding box as a starting point, and tighten vertex
	// merging criteria from there if the change in area is larger than the
	// user allowed maximum.
	point_t bbmin = VINIT_ZERO;
	point_t bbmax = VINIT_ZERO;
	if (!bg_trimesh_aabb(&bbmin, &bbmax, bot->faces, bot->num_faces, (const point_t *)bot->vertices, bot->num_vertices)) {

	    fastf_t working_tol = DIST_PNT_PNT(bbmax, bbmin) * 0.1;

	    fastf_t orig_area = bg_trimesh_area(bot->faces, bot->num_faces, (const point_t *)bot->vertices, bot->num_vertices);
	    fastf_t narea = 0;
	    fastf_t pdelta = 1.0;

	    while (pdelta > min_tri_threshold) {
		struct rt_bot_internal *cbot = collapse_faces(bot, working_tol);
		if (cbot == input_bot)
		    break;

		// We have a new candidate - check its area and the triangle delta
		narea = bg_trimesh_area(cbot->faces, cbot->num_faces, (const point_t *)cbot->vertices, cbot->num_vertices);
		pdelta = ((fastf_t)bot->num_faces - (fastf_t)cbot->num_faces)/(fastf_t)bot->num_faces;


		// If we satisfy the criteria, declare victory
		if (pdelta > min_tri_threshold && NEAR_EQUAL(orig_area, narea, max_area_delta)) {
		    if (!quiet_mode) {
			bu_log("face cnt: %zd -> %zd:  Δ: -%g%%\n", bot->num_faces, cbot->num_faces, 100*pdelta);
			bu_log("area: %g -> %g:  Δ: %g%%\n", orig_area, narea, 100*(orig_area - narea)/orig_area);
		    }
		    bot = cbot;
		    break;
		}

		// One way or the other, we're not using cbot - free it up
		rt_bot_internal_free(cbot);
		bu_free(cbot, "cbot");

		// If we're not getting enough reduction in face cnt, give up
		if (pdelta < .2)
		   break;

		// If we don't have a winner yet, but we haven't hit the
		// stopping criteria, continue
		working_tol *= 0.5;
	    }
	}
    }

    // OK, we have volume.  Now we need to build up the manifold definition
    // using unioned CSG elements
    manifold::Manifold c;
    c.SetTolerance(mtol);

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
	VMOVE(v, &bot->vertices[3*(*v_it)]);

	manifold::Manifold sph = manifold::Manifold::Sphere(r, 8);
	manifold::Manifold right = sph.Translate(linalg::vec<fastf_t, 3>(v[0], v[1], v[2]));

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
	rcc_m.tolerance = mtol;
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
	// If we couldn't make the cylinder, skip
	if (right.Status() != manifold::Manifold::Error::NoError) {
	    bu_log("Warning - failed to generate manifold edge cyl - skipping!\n");
	    continue;
	}

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

	// To minimize coplanarity with neighboring faces if the source BoT is
	// planar, bump all points out very slightly from the arb center point.
	point_t pcenter = VINIT_ZERO;
	for (size_t j = 0; j < 6; j++) {
	    point_t apnt;
	    VMOVE(apnt, ((point_t *)pts)[j]);
	    VADD2(pcenter, pcenter, apnt);
	}
	VSCALE(pcenter, pcenter, 1.0/6.0);

	for (size_t j = 0; j < 6; j++) {
	    point_t apnt;
	    vect_t bumpv;
	    VMOVE(apnt, ((point_t *)pts)[j]);
	    VSUB2(bumpv, apnt, pcenter);
	    VUNITIZE(bumpv);
	    VSCALE(bumpv, bumpv, 1 + VUNITIZE_TOL);
	    VADD2(((point_t *)pts)[j], apnt, bumpv);
	}

	int faces[24];
	faces[ 0] = 0; faces[ 1] = 1; faces[ 2] = 4;  // 1 2 5
	faces[ 3] = 2; faces[ 4] = 3; faces[ 5] = 5;  // 3 4 6
	faces[ 6] = 1; faces[ 7] = 0; faces[ 8] = 3;  // 2 1 4
	faces[ 9] = 3; faces[10] = 2; faces[11] = 1;  // 4 3 2
	faces[12] = 3; faces[13] = 0; faces[14] = 4;  // 4 1 5
	faces[15] = 4; faces[16] = 5; faces[17] = 3;  // 5 6 4
	faces[18] = 5; faces[19] = 4; faces[20] = 1;  // 6 5 2
	faces[21] = 1; faces[22] = 2; faces[23] = 5;  // 2 3 6

	manifold::MeshGL64 arb_m;
	for (size_t j = 0; j < 6; j++) {
	    arb_m.vertProperties.insert(arb_m.vertProperties.end(), pts[3*j]);
	    arb_m.vertProperties.insert(arb_m.vertProperties.end(), pts[3*j+1]);
	    arb_m.vertProperties.insert(arb_m.vertProperties.end(), pts[3*j+2]);
	}
	for (size_t j = 0; j < 8; j++) {
	    arb_m.triVerts.insert(arb_m.triVerts.end(), faces[3*j]);
	    arb_m.triVerts.insert(arb_m.triVerts.end(), faces[3*j+1]);
	    arb_m.triVerts.insert(arb_m.triVerts.end(), faces[3*j+2]);
	}
	manifold::Manifold right(arb_m);
	// If we couldn't make the arb, skip
	if (right.Status() != manifold::Manifold::Error::NoError) {
	    bu_log("Warning - failed to generate manifold face arb - skipping!\n");
	    continue;
	}

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

    if (c.Status() != manifold::Manifold::Error::NoError) {
	bu_log("Boolean op failure!\n");
	// TODO - if debugging, re-run to generate specific failure
	// inputs
	return -1;
    }


    manifold::MeshGL64 rmesh;
    rmesh.tolerance = mtol;
    rmesh = c.GetMeshGL64();

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

    if (!quiet_mode)
	bu_log("Extrusion has %ld vertices and %ld faces\n", rbot->num_vertices, rbot->num_faces);

    if (input_bot != bot) {
	rt_bot_internal_free(bot);
	bu_free(bot, "bot");
    }

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

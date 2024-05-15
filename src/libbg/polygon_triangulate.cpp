/*         P O L Y G O N _ T R I A N G U L A T E . C P P
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
/** @file polygon_triangulate.cpp
 *
 * libbg wrapper functions for Polygon Triangulation codes
 *
 */

#include "common.h"

#include <inttypes.h>
#include "bio.h"

#include <array>
#include <map>
#include <set>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "clipper.hpp"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif
#include "./earcut.hpp"
#ifdef USE_DETRIA
#include "detria.hpp"
#endif
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

#include "delaunator.hpp"

#ifndef USE_DETRIA
#include "poly2tri/poly2tri.h"
#endif

#define PLOT_PREFIX_STR bg_plot3_ // needed by RTree.h and plot3.h

#include "RTree.h"

#include "bu/malloc.h"
#include "bn/rand.h"
#include "bv/plot3.h"
#include "bg/polygon.h"
#include "bg/plane.h"

extern int polyline_2d_plot3(const char *fname, std::vector<std::pair<int64_t, int64_t>> &xy, fastf_t scale, struct bu_color *c);

int
xy_ind_lookup(
	std::unordered_map<int64_t, std::unordered_map<int64_t, std::unordered_set<int>>> &bins,
	std::unordered_map<int, int> collapsed_pts,
	int64_t xc, int64_t yc
	)
{
    long pind = -1;

    if (bins.find(xc) != bins.end()) {
	std::unordered_map<int64_t, std::unordered_set<int>> &yset = bins[xc];
	if (yset.find(yc) != yset.end()) {
	    std::unordered_set<int> &pind_set = yset[yc];
	    if (pind_set.size() == 1) {
		pind = *pind_set.begin();
	    } else {
		std::unordered_set<int>::iterator ps_it;
		for (ps_it = pind_set.begin(); ps_it != pind_set.end(); ps_it++) {
		    size_t candidate = *ps_it;
		    if (collapsed_pts.find(candidate) != collapsed_pts.end())
			continue;
		    pind = candidate;
		    break;
		}
	    }
	}
    }

    return pind;
}

struct line_steiner_ctx {
    std::unordered_set<size_t> *steiner_excluded;
    std::vector<std::pair<double,double>> *lines;
    point_t tpnt;
    double distsq_tol;
};

static bool
LSteinClbk(size_t data, void *ctx)
{
    struct line_steiner_ctx *c = (struct line_steiner_ctx *)ctx;
    point2d_t P0, P1;
    vect2d_t dir;
    V2SET(P0, (*c->lines)[2*data+0].first, (*c->lines)[2*data+0].second);
    V2SET(P1, (*c->lines)[2*data+1].first, (*c->lines)[2*data+1].second);
    V2SUB2(dir, P1, P0);
    double dsq = MAG2SQ(dir);
    if (!(dsq > 0.0))
	return true;
    double d1sq = DIST_PNT2_PNT2_SQ(c->tpnt, P0);
    double d2sq = DIST_PNT2_PNT2_SQ(c->tpnt, P1);
    double t;
    if (d1sq < d2sq) {
	vect2d_t s;
	V2SUB2(s, c->tpnt, P0);
	t = V2DOT(s, dir)/dsq;
    } else if (d2sq > d1sq) {
	vect2d_t s;
	V2SUB2(s, c->tpnt, P1);
	t = 1.0 + V2DOT(s, dir)/dsq;
    } else {
	vect2d_t s;
	V2SUB2(s, c->tpnt, P0);
	t = V2DOT(s, dir)/dsq;
    }
    if (t < 0 || t > 1)
	return true;
    double dirlensq = V2DOT(dir, dir);
    if (!(dirlensq > 0.0))
	return true;
    vect2d_t closest;
    point_t ra, rb;
    t /= dirlensq;
    V2SCALE(ra, P0, (1.0 - t));
    V2SCALE(rb, P1, t);
    V2ADD2(closest, ra, rb);
    double cdsq = DIST_PNT2_PNT2_SQ(closest, c->tpnt);
    if (cdsq < c->distsq_tol) {
	c->steiner_excluded->insert(data);
	//bu_log("excluded\n");
    }
    return true;
}

// TODO - clipper2 is released now - https://github.com/AngusJohnson/Clipper2
// Need to look at updating our bundled clipper to that version...
int
bg_poly2tri_test(int **faces, int *num_faces, point2d_t **out_pts, int *num_outpts,
	    const int *poly, const size_t poly_pnts,
	    const int **holes_array, const size_t *holes_npts, const size_t nholes,
	    const int *steiner, const size_t steiner_npts,
	    const point2d_t *pts)
{
    // Sanity
    if (!faces || !num_faces || !poly || !poly_pnts)
	return BRLCAD_ERROR;
    if (nholes && (!holes_npts || !holes_array))
	return BRLCAD_ERROR;
    if (steiner_npts && !steiner)
	return BRLCAD_ERROR;

    // The poly2tri algorithm requires:
    // 1. no repeat points within epsilon
    // 2. simple polygons only, for both holes and outer boundary
    // 3. interior holes must not touch other holes, nor touch the outer boundary
    //
    // None of these conditions are guaranteed by the input data, so we need to
    // rework as required.  (Most use cases for this algorithm prefer to
    // generate a "close" mesh that ignores repeat points and resolves hole
    // intersections rather than simply failing.)
    //
    // If the output points array is supplied, we will need to generate a list
    // of "active" points that were used to form the successful mesh.  If not,
    // we are returning indices to the original points, accepting any minor
    // flaws that may be introduced by using the original point locations to
    // realize the mesh.

    // 1.  The first order of business is to get a working set of points.  We do not
    // assume that all points in pts are active, so we interrogate the polygons, holes
    // and steiner points to figure out which ones we need to worry about.
    std::unordered_set<int> initial_pnt_indices;
    for (size_t i = 0; i < poly_pnts; i++)
	initial_pnt_indices.insert(poly[i]);
    for (size_t i = 0; i < steiner_npts; i++)
	initial_pnt_indices.insert(steiner[i]);
    for (size_t i = 0; i < nholes; i++) {
	for (size_t j = 0; j < holes_npts[i]; j++)
	    initial_pnt_indices.insert(holes_array[i][j]);
    }

    // 2.  Now that we know the points, we need to identify the closest points and
    // determine if any must be merged.  We also need to determine a good integer
    // approximation to use for clipper when we make sure our hole polygons aren't
    // going to cause trouble.  "Good" answers for this problem are data dependent,
    // so we'll try to find an answer without hard coding scale values.  Building
    // on experience with the POPBuffer LoD work, we'll try integer clamping based
    // on seed data from the 3D scene:
    //
    // a.  Find the bounding box for the active points
    point_t bbmin, bbmax;
    VSETALL(bbmin, INFINITY);
    VSETALL(bbmax, -INFINITY);
    std::unordered_set<int>::iterator p_it;
    for (p_it = initial_pnt_indices.begin(); p_it != initial_pnt_indices.end(); p_it++) {
	V2MINMAX(bbmin, bbmax, pts[*p_it]);
    }
    double deltaX = fabs(bbmax[X] - bbmin[X]);
    double deltaY = fabs(bbmax[Y] - bbmin[Y]);
    double delta = (deltaX < deltaY) ? deltaX : deltaY;
    //bu_log("bbmin: %f %f %f  bbmax: %f %f %f\n", V3ARGS(bbmin), V3ARGS(bbmax));

    // b.  Bin the points into a series of nested unordered maps of sets based
    // on snapping the x,y coordinates to an integer.  We want to preserve as
    // much information as we can, so clipper will produce useful new points if
    // it needs to do intersection calculations on lines.  However, there is a
    // limit to how far we can "scale" a floating point number into integer
    // space before we overflow, and before that limit is reached we will
    // create numbers too large to calculate with.  We need to respect these
    // constraints as hard limits - how far we can push depends on when our
    // current working data set will experience an overflow.

    // First, define the ultimate upper limit of the data type we want to use.
    // Per the clipper library's documentation:
    //
    // "It's important to note that path coordinates can't use quite the full
    // 64 bits (63bit signed integers) because the library needs to perform
    // coordinate addition and subtraction. Consequently coordinates must be 62
    // bit signed integers (±4.6 *10 ^18)"
    //
    // Rather than hard coding the number 62, we express this limit
    // semantically to make it clearer what it represents:
    int64_t log2max = log2(std::numeric_limits<int64_t>::max()) - 1;

    // Next, use the largest absolute value we will need to be concerned with
    // in the current data set to find a bound maximum.  That will tell us how
    // much we can scale without pushing our coordinates above the limit
    double bnmax = fabs(bbmin[X]);
    bnmax = (fabs(bbmin[Y]) > bnmax) ? fabs(bbmin[Y]) : bnmax;
    bnmax = (fabs(bbmax[X]) > bnmax) ? fabs(bbmax[X]) : bnmax;
    bnmax = (fabs(bbmax[Y]) > bnmax) ? fabs(bbmax[Y]) : bnmax;

    // Need to increment by one to be sure we are defining a size that will
    // hold all the values we need.
    int64_t log2boundmax = (int64_t)log2(bnmax) + 1;

    // The difference between the current data max and the ultimate coordinate
    // size limit tells us how far we can scale for this particular data set.
    int64_t log2_limit = log2max - log2boundmax;
    double scale = pow(2, log2_limit);
    //bu_log("log2_limit: %" PRId64 "\n", log2_limit);
    //bu_log("scale: %f\n", scale);

    // Having established our limits, scale and snap the points
    std::unordered_map<size_t, size_t> orig_to_snapped;
    std::vector<std::pair<int64_t,int64_t>> snapped_pts;
    std::unordered_map<int64_t, std::unordered_map<int64_t, std::unordered_set<int>>> ubins;
    for (p_it = initial_pnt_indices.begin(); p_it != initial_pnt_indices.end(); p_it++) {
	int64_t lx = static_cast<int64_t>(pts[*p_it][X]* scale);
	int64_t ly = static_cast<int64_t>(pts[*p_it][Y]* scale);
	snapped_pts.push_back(std::make_pair(lx,ly));
	orig_to_snapped[*p_it] = snapped_pts.size() - 1;
	ubins[lx][ly].insert(*p_it);
#if 0
	double lx_restore = static_cast<double>(lx / scale);
	double ly_restore = static_cast<double>(ly / scale);
	bu_log("%d: %f,%f -> %" PRId64 ", %" PRId64 " -> %f %f\n", *p_it, pts[*p_it][X], pts[*p_it][Y], lx, ly, lx_restore, ly_restore);
#endif
    }

    // c.  Now we need to identify any post-binning collapsed points.
    std::unordered_map<int, int> collapsed_pts;
    std::unordered_map<int64_t, std::unordered_map<int64_t, std::unordered_set<int>>>::iterator b_it;
    for (b_it = ubins.begin(); b_it != ubins.end(); b_it++) {
	std::unordered_map<int64_t, std::unordered_set<int>>::iterator bb_it;
	for (bb_it = b_it->second.begin(); bb_it != b_it->second.end(); bb_it++) {
	    if (bb_it->second.size() < 2)
		continue;
	    // If we have binned points, we need to point them all to a
	    // single point for the purposes of meshing.  Look for the
	    // point with the closest value to the average of all the binned
	    // points, and use that.
	    std::unordered_set<int>::iterator bp_it;
	    point2d_t pavg = V2INIT_ZERO;
	    for (bp_it = bb_it->second.begin(); bp_it != bb_it->second.end(); bp_it++) {
		pavg[X] += pts[*bp_it][X];
		pavg[Y] += pts[*bp_it][Y];
	    }
	    fastf_t pdistsq = INFINITY;
	    int closest = 0;
	    for (bp_it = bb_it->second.begin(); bp_it != bb_it->second.end(); bp_it++) {
		fastf_t lsq = DIST_PNT2_PNT2_SQ(pavg, pts[*bp_it]);
		if (lsq < pdistsq) {
		    closest = *bp_it;
		    pdistsq = lsq;
		}
	    }
	    for (bp_it = bb_it->second.begin(); bp_it != bb_it->second.end(); bp_it++) {
		// Note - we don't want to loop the closest point to itself
		if (*bp_it != closest) {
		    collapsed_pts[*bp_it] = closest;
		}
	    }
	    bu_log("collapsed: %d\n", closest);
	}
    }

    // TODO - although we want to avoid arbitrarily close but not identical points, we also
    // need to avoid touching polygon loops - see https://github.com/raptor/clip2tri/blob/master/clip2tri/clip2tri.cpp#L160
    // and https://github.com/greenm01/poly2tri/issues/90
    //
    // Need to add the adjustment from that clip2tri routine *before* we do the Clipper evaluation


    // d.  Having produce suitably unique points, we now need to perturb them
    // to avoid colinearity - we don't want clipper removing points added to
    // straight lines to increase triangle counts, and poly2tri doesn't do well
    // with truly colinear points.
    std::unordered_map<int64_t, std::unordered_map<int64_t, std::unordered_set<int>>> pbins;
    std::vector<std::pair<int64_t,int64_t>> psnapped_pts;
    std::unordered_map<int64_t, std::unordered_set<int64_t>> ucheck;
    float *prand;
    bn_rand_init(prand, 0);
    bu_log("deltaX: %f deltaY: %f\n", deltaX, deltaY);
    int64_t dXb = (int64_t)log2((int)deltaX);
    int64_t dYb = (int64_t)log2((int)deltaY);
    //bu_log("log2boundmax: %" PRId64 " dXb: %" PRId64 " dYb: %" PRId64 "\n", log2boundmax, dXb, dYb);
    // How much to perturb the points is a key parameter - too much and we get
    // range problems with Clipper and meshing issues as relative positioning
    // changes, but if we're too small Poly2Tri can crash due to issues like
    // colinearity.  For the moment, try to base this on the X and Y parametric
    // space dimensions.
    fastf_t delta_spaceX = pow(2, dXb+log2boundmax+1) * deltaX;
    fastf_t delta_spaceY = pow(2, dYb+log2boundmax+1) * deltaY;
    bu_log("deltaX(%" PRId64 "): %f  deltaY(%" PRId64 "): %f\n", dXb, delta_spaceX, dYb, delta_spaceY);
    for (b_it = ubins.begin(); b_it != ubins.end(); b_it++) {
	std::unordered_map<int64_t, std::unordered_set<int>>::iterator bb_it;
	for (bb_it = b_it->second.begin(); bb_it != b_it->second.end(); bb_it++) {
	    int inf_loop_guard = 0;
	    int64_t slx = b_it->first;
	    int64_t sly = bb_it->first;
	    //bu_log("slx: %" PRId64 " sly: %" PRId64 " \n", slx, sly);
	    int64_t lx = (int64_t)(slx + (int64_t)(bn_rand_half(prand) * delta_spaceX));
	    int64_t ly = (int64_t)(sly + (int64_t)(bn_rand_half(prand) * delta_spaceY));
	    //bu_log("lx: %" PRId64 " ly: %" PRId64 " \n", lx, ly);
	    // We need to preserve the point uniqueness - keep perturbing until we
	    // produce a point we've not already seen
	    while (inf_loop_guard < 100000 && ucheck.find(lx) != ucheck.end() && ucheck[lx].find(ly) != ucheck[lx].end()) {
		lx = slx + (bn_rand_half(prand) * delta_spaceX);
		ly = sly + (bn_rand_half(prand) * delta_spaceY);
		bu_log("uniq: lx, ly: %" PRId64 ",%" PRId64 "\n", lx, ly);
		inf_loop_guard++;
	    }
	    ucheck[lx].insert(ly);
	    //bu_log("lx: %" PRId64 " ly: %" PRId64 " \n", lx, ly);
	    psnapped_pts.push_back(std::make_pair(lx,ly));
	    std::unordered_set<int>::iterator pind_it;
	    for (pind_it = bb_it->second.begin(); pind_it != bb_it->second.end(); pind_it++) {
		orig_to_snapped[*pind_it] = psnapped_pts.size() - 1;
	    }
	    pbins[lx][ly].insert(bb_it->second.begin(), bb_it->second.end());
	}
    }

    ubins.clear();

    // 3.  Having found suitable integer points, assemble versions of the polygons
    // using the new indices.  These will be the clipper inputs.  (NOTE:  we can
    // filter out duplicate points here if we need to, but first we're going to
    // see if clipper will handle that for us...)
    ClipperLib::Clipper clipper;

    ClipperLib::Path outer_polygon;
    outer_polygon.resize(poly_pnts);
    for (size_t i = 0; i < poly_pnts; i++) {
	size_t pind = poly[i];
	if (collapsed_pts.find(pind) != collapsed_pts.end())
	    pind = collapsed_pts[pind];
	outer_polygon[i].X = (ClipperLib::long64)(psnapped_pts[orig_to_snapped[pind]].first);
	outer_polygon[i].Y = (ClipperLib::long64)(psnapped_pts[orig_to_snapped[pind]].second);
    }
    try {
	clipper.AddPath(outer_polygon, ClipperLib::ptSubject, true);
    } catch (...) {
	bu_log("Clipper: failed to add outer polygon\n");
	return BRLCAD_ERROR;
    }

    // Add any holes
    for (size_t hn = 0; hn < nholes; hn++) {
	size_t hpcnt = holes_npts[hn];
	const int *harray = holes_array[hn];
	ClipperLib::Path hole_polygon;
	hole_polygon.resize(hpcnt);
	for (size_t i = 0; i < hpcnt; i++) {
	    size_t pind = harray[i];
	    if (collapsed_pts.find(pind) != collapsed_pts.end())
		pind = collapsed_pts[pind];
	    hole_polygon[i].X = (ClipperLib::long64)(psnapped_pts[orig_to_snapped[pind]].first);
	    hole_polygon[i].Y = (ClipperLib::long64)(psnapped_pts[orig_to_snapped[pind]].second);
	}
	try {
	    clipper.AddPath(hole_polygon, ClipperLib::ptClip, true);
	} catch (...) {
	    bu_log("Clipper: failed to add hole polygon %zd\n", hn);
	    return BRLCAD_ERROR;
	}
    }

    // 4.  Feed the integer-based outer polygon and hole polygons to clipper to
    // resolve any overlapping hole polygons or other problematic loop info.
    ClipperLib::PolyTree eval_polys;
    clipper.Execute(ClipperLib::ctDifference, eval_polys, ClipperLib::pftEvenOdd, ClipperLib::pftEvenOdd);

    // We're not going to tie the poly2tri algorithm to the Clipper data types,
    // so recast the data.
    std::map<int, std::vector<std::pair<int64_t,int64_t>>> outer_loops;
    std::map<int, std::map<int, std::vector<std::pair<int64_t,int64_t>>>> hole_loops;
    bool new_pnts = false;
    int outer_loop_cnt = 0;
    int hole_loop_cnt = 0;
    ClipperLib::PolyNode *polynode = eval_polys.GetFirst();
    while (polynode) {
	ClipperLib::Path &path = polynode->Contour;
	if (!polynode->IsHole()) {
	    hole_loop_cnt = 0;
	    outer_loop_cnt++;
	} else {
	    hole_loop_cnt++;
	}
	//bu_log("%d(%d)\n", outer_loop_cnt, hole_loop_cnt);
	for (size_t i = 0; i < path.size(); i++) {
	    int64_t xcoord = path[i].X;
	    int64_t ycoord = path[i].Y;
	    if (!polynode->IsHole()) {
		outer_loops[outer_loop_cnt-1].push_back(std::make_pair(xcoord, ycoord));
	    } else {
		hole_loops[outer_loop_cnt-1][hole_loop_cnt-1].push_back(std::make_pair(xcoord, ycoord));
	    }
	    // The points clipper returns are integer space points. Most will
	    // map back to our original points.  If we do have actual new
	    // points, we need to know - those will eventually need to be
	    // translated back to floating point numbers, which will require
	    // the ability to create a new output point array.
	    if (xy_ind_lookup(pbins, collapsed_pts, xcoord, ycoord) == -1)
		new_pnts = true;
	}
	polynode = polynode->GetNext();
    }
#if 0
    size_t num_contours = eval_polys.Total();
    bu_log("Clipper evaluated contours: %zd\n", num_contours);
#endif

    // If we get back new points from clipper but we're not supposed to be
    // returning a points array, error out.
    if (new_pnts && (!out_pts || !num_outpts))
	return BRLCAD_ERROR;


    // 4.  We have (slightly) shifted the points defining the polylines (and
    // for that matter, the input data doesn't guarantee that the steiner
    // points weren't on line segments.)  We need to exclude any steiner points
    // that are on a line segment from consideration.
    //
    // First, assemble all the lines from the outer loops and holes
    std::map<int, std::vector<std::pair<int64_t,int64_t>>>::iterator o_it;
    std::vector<std::pair<double,double>> lines;
    for (o_it = outer_loops.begin(); o_it != outer_loops.end(); o_it++) {
#if 0
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fname, "outer_loop_%d.plot3", o_it->first);
	unsigned char rgb[3] = {255, 0, 0};
	struct bu_color c = BU_COLOR_INIT_ZERO;
	bu_color_from_rgb_chars(&c, rgb);
	polyline_2d_plot3(bu_vls_cstr(&fname), o_it->second, scale, &c);
	bu_vls_free(&fname);
#endif
	for (size_t i = 0; i < o_it->second.size(); i++) {
	    double xcd = o_it->second[i].first / scale;
	    double ycd = o_it->second[i].second / scale;
	    //bu_log("xcd, ycd: %f %f\n", xcd, ycd);
	    lines.push_back(std::make_pair(xcd, ycd));
	}
	if (hole_loops.find(o_it->first) == hole_loops.end())
	    continue;
	std::map<int, std::vector<std::pair<int64_t,int64_t>>>::iterator h_it;
	for (h_it = hole_loops[o_it->first].begin(); h_it != hole_loops[o_it->first].end(); h_it++) {
#if 0
	    bu_vls_sprintf(&fname, "hole_loop_%d-%d.plot3", o_it->first, h_it->first);
	    unsigned char hrgb[3] = {0, 255, 0};
	    struct bu_color hc = BU_COLOR_INIT_ZERO;
	    bu_color_from_rgb_chars(&hc, hrgb);
	    polyline_2d_plot3(bu_vls_cstr(&fname), h_it->second, scale, &hc);
	    bu_vls_free(&fname);
#endif
	    for (size_t i = 0; i < h_it->second.size(); i++) {
		double xcd = h_it->second[i].first / scale;
		double ycd = h_it->second[i].second / scale;
		lines.push_back(std::make_pair(xcd, ycd));
	    }
	}
    }

    // Next, assemble an RTree of the lines so we can quickly identify which
    // ones we need to check each steiner point against
    RTree<size_t, double, 2> rtree_2d;
    for (size_t i = 0; i < lines.size()/2; i++) {
	double tMin[2], tMax[2];
	tMin[0] = (lines[2*i+0].first < lines[2*i+1].first) ? lines[2*i+0].first : lines[2*i+1].first;
	tMin[1] = (lines[2*i+0].second < lines[2*i+1].second) ? lines[2*i+0].second : lines[2*i+1].second;
	tMax[0] = (lines[2*i+0].first > lines[2*i+1].first) ? lines[2*i+0].first : lines[2*i+1].first;
	tMax[1] = (lines[2*i+0].second > lines[2*i+1].second) ? lines[2*i+0].second : lines[2*i+1].second;
	rtree_2d.Insert(tMin, tMax, i);
    }

    // Find steiner points that are too close to polylines and add them
    // to the excluded set
    std::unordered_set<size_t> steiner_excluded;
    struct line_steiner_ctx lctx;
    lctx.steiner_excluded = &steiner_excluded;
    lctx.lines = &lines;
    lctx.distsq_tol = 0.001*delta;
    for (size_t s = 0; s < steiner_npts; s++) {
	int64_t xc = psnapped_pts[orig_to_snapped[steiner[s]]].first;
	int64_t yc = psnapped_pts[orig_to_snapped[steiner[s]]].second;
	double xcd = xc / scale;
	double ycd = yc / scale;
	V2SET(lctx.tpnt, xcd, ycd);

	double fMin[2];
	double fMax[2];
	fMin[0] = xcd - 10*SMALL_FASTF;
	fMin[1] = xcd + 10*SMALL_FASTF;
	fMax[0] = ycd - 10*SMALL_FASTF;
	fMax[1] = ycd + 10*SMALL_FASTF;

	rtree_2d.Search(fMin, fMax, LSteinClbk, (void *)&lctx);
    }

#ifndef USE_DETRIA
    // 5.  The output of the above process is fed to the poly2tri algorithm,
    // along with the snapped steiner points.
    std::map<p2t::Point *, long> p2t_to_ind;
    std::vector<p2t::CDT *> cdts;
    for (o_it = outer_loops.begin(); o_it != outer_loops.end(); o_it++) {

	std::vector<p2t::Point*> outer_polyline;
	for (size_t i = 0; i < o_it->second.size(); i++) {
	    int64_t xc = o_it->second[i].first;
	    int64_t yc = o_it->second[i].second;

	    double xcd = xc / scale;
	    double ycd = yc / scale;
	    p2t::Point *p = new p2t::Point(xcd, ycd);
	    outer_polyline.push_back(p);

	    long pind = xy_ind_lookup(pbins, collapsed_pts, xc, yc);
	    if (pind == -1) {
		new_pnts = true;
		bu_log("(O) Wait, what?  Need new point????\n");
		//bu_log("xc, yc: %" PRId64 ",%" PRId64 "\n", xc, yc);
		//bu_log("%d: x,y: %f,%f\n", o_it->first, xcd, ycd);
		//pind = xy_ind_lookup(pbins, collapsed_pts, xc, yc);
	    } else {
		p2t_to_ind[p] = pind;
	    }
	}

	p2t::CDT *cdt = new p2t::CDT(outer_polyline);

	if (hole_loops.find(o_it->first) != hole_loops.end()) {
	    std::map<int, std::vector<std::pair<int64_t,int64_t>>>::iterator h_it;
	    for (h_it = hole_loops[o_it->first].begin(); h_it != hole_loops[o_it->first].end(); h_it++) {
		std::vector<p2t::Point*> polyline;
		for (size_t i = 0; i < h_it->second.size(); i++) {
		    int64_t xc = h_it->second[i].first;
		    int64_t yc = h_it->second[i].second;
		    //bu_log("xc, yc: %" PRId64 ",%" PRId64 "\n", xc, yc);
		    double xcd = xc / scale;
		    double ycd = yc / scale;
		    //bu_log("%d->h(%d): x,y: %f,%f\n", o_it->first, h_it->first, xcd, ycd);
		    p2t::Point *p = new p2t::Point(xcd, ycd);
		    polyline.push_back(p);

		    long pind = xy_ind_lookup(pbins, collapsed_pts, xc, yc);
		    if (pind == -1) {
			new_pnts = true;
			bu_log("(H) Wait, what?  Need new point????\n");
		    } else {
			p2t_to_ind[p] = pind;
		    }
		    p2t_to_ind[p] = pind;
		}
		cdt->AddHole(polyline);
	    }
	}

	for (size_t s = 0; s < steiner_npts; s++) {
	    if (steiner_excluded.find(s) != steiner_excluded.end())
		continue;
	    int64_t xc = psnapped_pts[orig_to_snapped[steiner[s]]].first;
	    int64_t yc = psnapped_pts[orig_to_snapped[steiner[s]]].second;
	    double xcd = xc / scale;
	    double ycd = yc / scale;
	    p2t::Point *p = new p2t::Point(xcd, ycd);
	    cdt->AddPoint(p);

	    long pind = xy_ind_lookup(pbins, collapsed_pts, xc, yc);
	    if (pind == -1) {
		new_pnts = true;
		bu_log("(S) Wait, what?  Need new point????\n");
	    } else {
		p2t_to_ind[p] = pind;
	    }
	    p2t_to_ind[p] = pind;
	}

	try {
	    cdt->Triangulate(true, -1);
	}
	catch (...) {
	    delete cdt;
	    cdt = NULL;
	}

	if (!cdt)
	    continue;

	// If we didn't get any triangles, we don't need this cdt
	if (!cdt->GetTriangles().size()) {
	    delete cdt;
	} else {
	    cdts.push_back(cdt);
	}
    }

    // If all the CDT attempts failed, we have a problem
    if (!cdts.size()) {
	bu_log("Poly2Tri CDT failed!\n");
	return BRLCAD_ERROR;
    }
#endif

    // If the loops need new points (??) bug we don't have out_pnts, error out.
    if (new_pnts && (!out_pts || !num_outpts))
	return BRLCAD_ERROR;

#ifndef USE_DETRIA
    // 6. Unpack the Poly2Tri faces into a C container
    std::unordered_set<p2t::Point *> p2t_active_pnts;
    std::unordered_map<p2t::Point *, size_t> npt_map;
    size_t total_tris = 0;
    for (size_t i = 0; i < cdts.size(); i++) {
	p2t::CDT *cdt = cdts[i];
	std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
	//bu_log("got %zd triangles\n", tris.size());
	total_tris += tris.size();

	if (!new_pnts)
	    continue;
	for (size_t j = 0; j < tris.size(); j++) {
	    p2t::Triangle *t = tris[j];
	    for (size_t k = 0; k < 3; k++) {
		p2t_active_pnts.insert(t->GetPoint(k));
	    }
	}
    }
    if (new_pnts) {
	size_t ind = 0;
	std::unordered_set<p2t::Point *>::iterator pa_it;
	for (pa_it = p2t_active_pnts.begin(); pa_it != p2t_active_pnts.end(); pa_it++) {
	    npt_map[*pa_it] = ind;
	    ind++;
	}
	(*num_outpts) = (int) p2t_active_pnts.size();
	(*out_pts) = (point2d_t *)bu_calloc(*num_outpts, sizeof(point2d_t), "new pnts array");
	ind = 0;
	for (pa_it = p2t_active_pnts.begin(); pa_it != p2t_active_pnts.end(); pa_it++) {
	    p2t::Point *p = *pa_it;
	    //bu_log("p2t point %zd: %f %f\n", ind, p->x, p->y);
	    fastf_t py = p->y - (bbmax[X] - p->x)/(bbmax[X] - bbmin[X]);
	    V2SET((*out_pts)[ind], p->x, py);
	    //bu_log("p2t point %zd descaled: %f %f\n", ind, p->x, py);
	    ind++;
	}
    }
    (*num_faces) = (int)total_tris;
    int *nfaces = (int *)bu_calloc(*num_faces * 3, sizeof(int), "faces array");
    int total_face_ind = 0;
    for (size_t i = 0; i < cdts.size(); i++) {
	p2t::CDT *cdt = cdts[i];
	std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
	//bu_log("got %zd triangles\n", tris.size());

	if (new_pnts) {
	    for (size_t j = 0; j < tris.size(); j++) {
		p2t::Triangle *t = tris[j];
		nfaces[3*total_face_ind] = npt_map[t->GetPoint(0)];
		nfaces[3*total_face_ind+1] = npt_map[t->GetPoint(1)];
		nfaces[3*total_face_ind+2] = npt_map[t->GetPoint(2)];
		total_face_ind++;
	    }
	} else {
	    for (size_t j = 0; j < tris.size(); j++) {
		p2t::Triangle *t = tris[j];
		for (size_t k = 0; k < 3; k++) {
		    p2t::Point *p = t->GetPoint(k);
		    if (p2t_to_ind[p] == -1) {
			bu_log("T1 Point map error??: %f, %f\n", p->x, p->y);
		    }
		}
		nfaces[3*total_face_ind] = p2t_to_ind[t->GetPoint(0)];
		nfaces[3*total_face_ind+1] = p2t_to_ind[t->GetPoint(1)];
		nfaces[3*total_face_ind+2] = p2t_to_ind[t->GetPoint(2)];
		total_face_ind++;
	    }
	}

	delete cdt;
    }
    (*faces) = nfaces;
#endif
    return 0;
}

#ifdef USE_DETRIA
int
bg_detria(int **faces, int *num_faces, point2d_t **out_pts, int *num_outpts,
	    const int *poly, const size_t poly_pnts,
	    const int **holes_array, const size_t *holes_npts, const size_t nholes,
	    const int *steiner, const size_t steiner_npts,
	    const point2d_t *pts)
{
    std::unordered_map<int, int> det2pts, pts2det;
    std::set<int> active_pts;

    // Find active points
    for (size_t i = 0; i < poly_pnts; i++)
	active_pts.insert(poly[i]);
    for (size_t i = 0; i < nholes; i++) {
	for (size_t j = 0; j < holes_npts[i]; j++)
	    active_pts.insert(holes_array[i][j]);
    }
    // Note - for detria, all points added that are not part of a polygon are
    // steiner points - so the only thing we need to do with them is flag them
    // as active in the set.
    for (size_t i = 0; i < steiner_npts; i++)
	active_pts.insert(steiner[i]);

    // Set up a points array holding the active points
    std::vector<detria::PointD> tpnts;
    std::set<int>::iterator a_it;
    for (a_it = active_pts.begin(); a_it != active_pts.end(); a_it++) {
	detria::PointD npt;
	npt.x = pts[*a_it][X];
	npt.y = pts[*a_it][Y];
	tpnts.push_back(npt);
	int detind = (int)tpnts.size() - 1;
	det2pts[detind] = *a_it;
	pts2det[*a_it] = detind;
    }

    // Let the triangulation structure know about the points
    detria::Triangulation<detria::PointD, int> tri;
    tri.setPoints(tpnts);

    // Outer polygon is defined first
    std::vector<int> outer_polyline;
    for (size_t i = 0; i < poly_pnts; i++)
	outer_polyline.push_back(pts2det[poly[i]]);
    tri.addOutline(outer_polyline);

    // Next are the holes
    std::vector<std::vector<int>> inner_holes;
    for (size_t i = 0; i < nholes; i++) {
	std::vector<int> hv;
	for (size_t j = 0; j < holes_npts[i]; j++)
	    hv.push_back(holes_array[i][j]);
	tri.addHole(hv);
    }

    // Run the core triangulation routine
    bool tri_success = false;
    {
	try {
	    tri_success = tri.triangulate(true);
	}
	catch (...) {
	    return 1;
	}
    }

    // Did we succeed?
    if (!tri_success) {
	return 1;
    }

    // Should the result triangles be in CW order?
    bool cwTriangles = true;

    // Count the number of interior triangles so we can allocate an output array
    int tri_cnt = 0;
    tri.forEachTriangle([&](const detria::Triangle<int> &){tri_cnt++;}, cwTriangles);

    (*num_faces) = tri_cnt;
    int *nfaces = (int *)bu_calloc(*num_faces * 3, sizeof(int), "faces array");

    // We may want to report the number of unique active points, so
    // track this as we iterate the triangles
    active_pts.clear();

    int tri_ind = 0;
    tri.forEachTriangle([&](const detria::Triangle<int> triangle)
    {
        // `triangle` contains the point indices
	nfaces[3*tri_ind] = det2pts[triangle.x];
	nfaces[3*tri_ind+1] = det2pts[triangle.y];
	nfaces[3*tri_ind+2] = det2pts[triangle.z];
	active_pts.insert(nfaces[3*tri_ind]);
	active_pts.insert(nfaces[3*tri_ind+1]);
	active_pts.insert(nfaces[3*tri_ind+2]);
	tri_ind++;
    }, cwTriangles);

    (*faces) = nfaces;

    // We're not generating a new points array, so if out_pts exists set it to the
    // input points array
    if (out_pts) {
	(*out_pts) = (point2d_t *)pts;
    }
    if (num_outpts) {
	(*num_outpts) = (int)active_pts.size();
    }

    return 0;
}
#endif

#ifndef USE_DETRIA
int
bg_poly2tri(int **faces, int *num_faces, point2d_t **out_pts, int *num_outpts,
	    const int *poly, const size_t poly_pnts,
	    const int **holes_array, const size_t *holes_npts, const size_t nholes,
	    const int *steiner, const size_t steiner_npts,
	    const point2d_t *pts)
{
    std::map<p2t::Point *, size_t> p2t_to_ind;
    std::set<p2t::Point *> p2t_pnts;
    std::set<p2t::Point *>::iterator p_it;

    // Outer polygon is defined first
    std::vector<p2t::Point*> outer_polyline;
    for (size_t i = 0; i < poly_pnts; i++) {
	p2t::Point *p = new p2t::Point(pts[poly[i]][X], pts[poly[i]][Y]);
	outer_polyline.push_back(p);
	p2t_to_ind[p] = poly[i];
	p2t_pnts.insert(p);
    }

    // Initialize the cdt structure
    p2t::CDT *cdt = new p2t::CDT(outer_polyline);

    // Add holes, if any
    for (size_t h = 0; h < nholes; h++) {
	std::vector<p2t::Point*> polyline;
	for (size_t i = 0; i < holes_npts[h]; i++) {
	    p2t::Point *p = new p2t::Point(pts[holes_array[h][i]][X], pts[holes_array[h][i]][Y]);
	    polyline.push_back(p);
	    p2t_to_ind[p] = holes_array[h][i];
	    p2t_pnts.insert(p);
	}
	cdt->AddHole(polyline);
    }

    // Add steiner points, if any
    for (size_t s = 0; s < steiner_npts; s++) {
	p2t::Point *p = new p2t::Point(pts[steiner[s]][X], pts[steiner[s]][Y]);
	p2t_to_ind[p] = steiner[s];
	p2t_pnts.insert(p);
	cdt->AddPoint(p);
    }

    // Run the core triangulation routine
    {
	try {
	    cdt->Triangulate(true, -1);
	}
	catch (...) {
	    delete cdt;
	    return 1;
	}
    }

    // If we didn't get any triangles, fail
    if (!cdt->GetTriangles().size()) {
	delete cdt;
	return 1;
    }

    // Unpack the Poly2Tri faces into a C container
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();

    (*num_faces) = (int)tris.size();
    int *nfaces = (int *)bu_calloc(*num_faces * 3, sizeof(int), "faces array");
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	nfaces[3*i] = p2t_to_ind[t->GetPoint(0)];
	nfaces[3*i+1] = p2t_to_ind[t->GetPoint(1)];
	nfaces[3*i+2] = p2t_to_ind[t->GetPoint(2)];
    }
    (*faces) = nfaces;

    // We're not generating a new points array, so if out_pts exists set it to the
    // input points array
    if (out_pts) {
	(*out_pts) = (point2d_t *)pts;
    }
    if (num_outpts) {
	// Build the set of unique points - even if num_outpts != npts the
	// caller will be able to tell if all points were used, even though
	// out_pts won't contain just the used set of points.
	std::set<p2t::Point *> p2t_active_pnts;
	for (size_t i = 0; i < tris.size(); i++) {
	    p2t::Triangle *t = tris[i];
	    for (size_t j = 0; j < 3; j++) {
		p2t_active_pnts.insert(t->GetPoint(j));
	    }
	}

	(*num_outpts) = (int) p2t_active_pnts.size();
    }

    // Cleanup
    delete cdt;

    return 0;
}
#endif


extern "C" int
bg_nested_poly_triangulate(int **faces, int *num_faces, point2d_t **out_pts, int *num_outpts,
			      const int *poly, const size_t poly_pnts,
			      const int **holes_array, const size_t *holes_npts, const size_t nholes,
			      const int *steiner, const size_t steiner_npts,
			      const point2d_t *pts, const size_t npts, triangulation_t type)
{
    if (npts < 3 || poly_pnts < 3) return 1;
    if (!faces || !num_faces || !pts || !poly) return 1;

    if (nholes > 0) {
	if (!holes_array || !holes_npts) return 1;
    }

    //if (type == TRI_DELAUNAY && (!out_pts || !num_outpts)) return 1;

    if (type == TRI_ANY || type == TRI_CONSTRAINED_DELAUNAY) {
#ifdef USE_DETRIA
	int detria_ret = bg_detria(faces, num_faces, out_pts, num_outpts, poly, poly_pnts, holes_array, holes_npts, nholes, steiner, steiner_npts, pts);
	return detria_ret;
#else
	int p2t_ret = bg_poly2tri(faces, num_faces, out_pts, num_outpts, poly, poly_pnts, holes_array, holes_npts, nholes, steiner, steiner_npts, pts);
	return p2t_ret;
#endif
    }

    if (type == TRI_DELAUNAY) {
	std::vector<double> coords;
	for (size_t i = 0; i < npts; i++) {
	    coords.push_back(pts[i][X]);
	    coords.push_back(pts[i][Y]);
	}
	delaunator::Delaunator d(coords);

	(*num_faces) = d.triangles.size()/3;
	(*faces) = (int *)bu_calloc(d.triangles.size(), sizeof(int), "faces");

	for (size_t i = 0; i < d.triangles.size()/3; i++) {
	    (*faces)[3*i] = (int)d.triangles[3*i];
	    (*faces)[3*i+1] = (int)d.triangles[3*i+1];
	    (*faces)[3*i+2] = (int)d.triangles[3*i+2];
	}

	return 0;
    }

    if (type == TRI_EAR_CLIPPING) {

	if (steiner_npts) return 1;

	/* Set up for ear clipping */
	using Coord = fastf_t;
	using N = uint32_t;
	using Point = std::array<Coord, 2>;
	std::vector<std::vector<Point>> polygon;
	std::vector<Point> outer_polygon;
	for (size_t i = 0; i < poly_pnts; i++) {
	    Point np;
	    np[0] = pts[poly[i]][X];
	    np[1] = pts[poly[i]][Y];
	    outer_polygon.push_back(np);
	}
	polygon.push_back(outer_polygon);

	if (holes_array) {
	    for (size_t i = 0; i < nholes; i++) {
		std::vector<Point> hole_polygon;
		for (size_t j = 0; j < holes_npts[i]; j++) {
		    Point np;
		    np[0] = pts[holes_array[i][j]][X];
		    np[1] = pts[holes_array[i][j]][Y];
		    hole_polygon.push_back(np);
		}
		polygon.push_back(hole_polygon);
	    }
	}

	std::vector<N> indices = mapbox::earcut<N>(polygon);

	if (indices.size() < 3) {
	    return 1;
	}

	(*num_faces) = indices.size()/3;
	(*faces) = (int *)bu_calloc(indices.size(), sizeof(int), "faces");

	for (size_t i = 0; i < indices.size()/3; i++) {
	    (*faces)[3*i] = (int)indices[3*i];
	    (*faces)[3*i+1] = (int)indices[3*i+1];
	    (*faces)[3*i+2] = (int)indices[3*i+2];
	}

	return 0;
    }

    /* Unimplemented type specified */
    return -1;
}

extern "C" int
bg_poly_triangulate(int **faces, int *num_faces, point2d_t **out_pts, int *num_outpts,
	const int *steiner, const size_t steiner_pnts,
	const point2d_t *pts, const size_t npts, triangulation_t type)
{
    int ret;

    if (type == TRI_DELAUNAY && (!out_pts || !num_outpts)) return 1;

    int *verts_ind = NULL;
    verts_ind = (int *)bu_calloc(npts, sizeof(int), "vert indices");
    for (size_t i = 0; i < npts; i++) {
	verts_ind[i] = (int)i;
    }

    ret = bg_nested_poly_triangulate(faces, num_faces, out_pts, num_outpts, verts_ind, npts, NULL, NULL, 0, steiner, steiner_pnts, pts, npts, type);

    bu_free(verts_ind, "vert indices");
    return ret;
}

extern "C" void
bg_tri_plot_2d(const char *filename, const int *faces, int num_faces, const point2d_t *pnts, int r, int g, int b)
{
    FILE* plot_file = fopen(filename, "wb");
    pl_color(plot_file, r, g, b);

    for (int k = 0; k < num_faces; k++) {
	point_t p1, p2, p3;
	VSET(p1, pnts[faces[3*k]][X], pnts[faces[3*k]][Y], 0);
	VSET(p2, pnts[faces[3*k+1]][X], pnts[faces[3*k+1]][Y], 0);
	VSET(p3, pnts[faces[3*k+2]][X], pnts[faces[3*k+2]][Y], 0);

	pdv_3move(plot_file, p1);
	pdv_3cont(plot_file, p2);
	pdv_3move(plot_file, p1);
	pdv_3cont(plot_file, p3);
	pdv_3move(plot_file, p2);
	pdv_3cont(plot_file, p3);
    }
    fclose(plot_file);
}

extern "C" int
bg_polygon_triangulate(int **faces, int *num_faces, point_t **out_pts, int *num_outpts,
	struct bg_polygon *p, triangulation_t type)
{
    if (!faces || !num_faces || !out_pts || !num_outpts || !p)
	return -1;

    // Fit the outer contour to get a 2D plane (bg_polygon is in principle a 3D data structure)
    point_t pcenter;
    vect_t  pnorm;
    plane_t pl;
    if (bg_fit_plane(&pcenter, &pnorm, p->contour[0].num_points, p->contour[0].point)) {
	return -1;
    }
    bg_plane_pt_nrml(&pl, pcenter, pnorm);

    // Count all points
    int pnt_cnt = 0;
    for (size_t i = 0; i < p->num_contours; ++i) {
	pnt_cnt += p->contour[i].num_points;
    }

    // Translate the bg_polygon into bg_nested_poly_triangulate inputs
    point2d_t *pnts_2d = (point2d_t *)bu_calloc(pnt_cnt, sizeof(point2d_t), "projected points");
    int *ocontour = NULL;
    int ocontour_cnt = 0;
    int **holes_array = (int **)bu_calloc(p->num_contours - 1, sizeof(int *), "holes");
    size_t *holes_npts = (size_t *)bu_calloc(p->num_contours - 1, sizeof(size_t), "holes_cnt");
    int curr_pnt = 0;
    for (size_t i = 0; i < p->num_contours; ++i) {
	int *cpnts = (int *)bu_calloc(p->contour[i].num_points, sizeof(int), "point indices");
	if (i > 0) {
	    holes_array[i-1] = cpnts;
	    holes_npts[i-1] = p->contour[i].num_points;
	} else {
	    ocontour = cpnts;
	    ocontour_cnt = p->contour[i].num_points;
	}
	for (size_t j = 0; j < p->contour[i].num_points; ++j) {
	    vect2d_t p2d;
	    bg_plane_closest_pt(&p2d[0], &p2d[1], pl, p->contour[i].point[j]);
	    V2MOVE(pnts_2d[curr_pnt], p2d);
	    cpnts[j] = curr_pnt;
	    curr_pnt++;
	}
    }

    int *tri_faces = NULL;
    int tri_num_faces = 0;
    point2d_t *tri_out_pts = NULL;
    int tri_num_outpts = 0;
    int ret = bg_nested_poly_triangulate(&tri_faces, &tri_num_faces, &tri_out_pts, &tri_num_outpts, ocontour, ocontour_cnt, (const int **)holes_array, (const size_t *)holes_npts, p->num_contours - 1, NULL, 0, pnts_2d, pnt_cnt, type); 


    // Translate 2D plane points into 3D points
    point_t *pnts_3d = (point_t *)bu_calloc(pnt_cnt, sizeof(point_t), "3D points");
    for (int i = 0; i < pnt_cnt; i++) {
	bg_plane_pt_at(&pnts_3d[i], pl, pnts_2d[i][0], pnts_2d[i][1]);
    }

    // Assign outputs
    *faces = tri_faces;
    *num_faces = tri_num_faces;
    *out_pts = pnts_3d;
    *num_outpts = tri_num_outpts;

    // Clean up 2D and translation arrays
    bu_free(ocontour, "free ocontour");
    for (size_t i = 0; i < p->num_contours - 1; i++) {
	bu_free(holes_array[i], "free holes array");
    }
    bu_free(holes_npts, "hole cnts");
    bu_free(pnts_2d, "2d pnts");

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

/*                        P L O T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file libged/brep/plot.cpp
 *
 * Visualization logic for individual subcomponents of brep objects.
 *
 */

#include "common.h"

#include <set>

#include "opennurbs.h"
#include "bu/cmd.h"
#include "../ged_private.h"
#include "./ged_brep.h"

/********************************************************************************
 * Auxiliary functions
 ********************************************************************************/

using namespace brlcad;


void
plotpoint(const ON_3dPoint &point, struct bu_list *vlfree, struct bv_vlblock *vbp, const int red = 255, const int green = 255, const int blue = 0)
{
    struct bu_list *vhead;
    ON_3dPoint pointsize(4.0,0,0);
    vhead = bv_vlblock_find(vbp, red, green, blue);
    BV_ADD_VLIST(vlfree, vhead, pointsize, BV_VLIST_POINT_SIZE);
    BV_ADD_VLIST(vlfree, vhead, point, BV_VLIST_POINT_DRAW);
    return;
}

void
plottrim(const ON_BrepTrim &trim, struct bu_list *vlfree, struct bv_vlblock *vbp, int plotres, bool dim3d, const int red = 255, const int green = 255, const int blue = 0)
{
    struct bu_list *vhead;
    ON_2dPoint from, to;
    const ON_Surface *surf = trim.SurfaceOf();

    ON_TextLog tl(stderr);

    vhead = bv_vlblock_find(vbp, red, green, blue);

    const ON_Curve* trimCurve = trim.TrimCurveOf();
    ON_Interval dom = trimCurve->Domain();
    //trimCurve->Dump(tl);

    fastf_t pt[3];
    for (int k = 0; k <= plotres; ++k) {
	ON_3dPoint p = trimCurve->PointAt(dom.ParameterAt((double)k / plotres));
	if (dim3d) {
	    p = surf->PointAt(p.x, p.y);
	}
	VMOVE(pt, p);

	if (k != 0) {
	    BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_LINE_DRAW);
	} else {
	    BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_LINE_MOVE);
	}
    }
 
#if 0
    fastf_t pt1[3], pt2[3];
    for (int k = 1; k <= plotres; k++) {
	ON_3dPoint p = trimCurve->PointAt(dom.ParameterAt((double) (k - 1) / (double) plotres));
	if (dim3d) {
	    p = surf->PointAt(p.x, p.y);
	}
	VMOVE(pt1, p);
	p = trimCurve->PointAt(dom.ParameterAt((double) k / (double) plotres));
	if (dim3d) {
	    p = surf->PointAt(p.x, p.y);
	}
	VMOVE(pt2, p);
	BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
    }
#endif

    return;
}

#define LINE_PLOT(p1, p2) pdv_3move(brep_plot_file(), p1); pdv_3line(brep_plot_file(), p1, p2)
void
plotcurve(const ON_Curve &curve, struct bu_list *vlfree, struct bv_vlblock *vbp, int plotres, const int red = 255, const int green = 255, const int blue = 0)
{
    struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    ON_2dPoint from, to;

    vhead = bv_vlblock_find(vbp, red, green, blue);

    if (curve.IsLinear()) {
	/*
	   ON_BrepVertex& v1 = face.Brep()->m_V[trim.m_vi[0]];
	   ON_BrepVertex& v2 = face.Brep()->m_V[trim.m_vi[1]];
	   VMOVE(pt1, v1.Point());
	   VMOVE(pt2, v2.Point());
	   LINE_PLOT(pt1, pt2);
	   */

	int knotcnt = curve.SpanCount();
	fastf_t *knots = new fastf_t[knotcnt + 1];

	curve.GetSpanVector(knots);
	for (int i = 1; i <= knotcnt; i++) {
	    ON_3dPoint p = curve.PointAt(knots[i - 1]);
	    VMOVE(pt1, p);
	    p = curve.PointAt(knots[i]);
	    VMOVE(pt2, p);
	    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
	}

    } else {
	ON_Interval dom = curve.Domain();
	// XXX todo: dynamically sample the curve
	for (int i = 1; i <= plotres; i++) {
	    ON_3dPoint p = curve.PointAt(dom.ParameterAt((double) (i - 1)
			/ (double)plotres));
	    VMOVE(pt1, p);
	    p = curve.PointAt(dom.ParameterAt((double) i / (double)plotres));
	    VMOVE(pt2, p);
	    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
	}
    }
    return;
}



void plotcurveonsurface(const ON_Curve *curve,
	const ON_Surface *surface,
	struct bu_list *vlfree,
	struct bv_vlblock *vbp,
	int plotres,
	const int red = 255,
	const int green = 255,
	const int blue = 0)
{
    if (curve->Dimension() != 2)
	return;
    struct bu_list *vhead;
    vhead = bv_vlblock_find(vbp, red, green, blue);

    for (int i = 0; i <= plotres; i++) {
	ON_2dPoint pt2d;
	ON_3dPoint pt3d;
	ON_3dPoint pt1, pt2;
	pt2d = curve->PointAt(curve->Domain().ParameterAt((double)i/plotres));
	pt3d = surface->PointAt(pt2d.x, pt2d.y);
	pt1 = pt2;
	pt2 = pt3d;
	if (i != 0) {
	    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
	}
    }
    return;
}

void
plotsurface(const ON_Surface &surf, struct bu_list *vlfree, struct bv_vlblock *vbp, int isocurveres, int gridres, const int red = 200, const int green = 200, const int blue = 200)
{
    struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    ON_2dPoint from, to;
    fastf_t hsv[3];
    unsigned char fill_rgb[3];

    VSET(fill_rgb,(unsigned char)red,(unsigned char)green,(unsigned char)blue);
    bu_rgb_to_hsv(fill_rgb,hsv);
    // simply fill with 50% lightness/value of outline
    hsv[2] = hsv[2] * 0.5;
    bu_hsv_to_rgb(hsv,fill_rgb);


    vhead = bv_vlblock_find(vbp, red, green, blue);

    ON_Interval udom = surf.Domain(0);
    ON_Interval vdom = surf.Domain(1);

    for (int u = 0; u <= gridres; u++) {
	if (u == 0 || u == gridres) {
	    vhead = bv_vlblock_find(vbp, red, green, blue);
	} else {
	    vhead = bv_vlblock_find(vbp, (int)(fill_rgb[0]), (int)(fill_rgb[1]), (int)(fill_rgb[2]));
	}
	for (int v = 1; v <= isocurveres; v++) {
	    ON_3dPoint p = surf.PointAt(udom.ParameterAt((double)u/(double)gridres), vdom.ParameterAt((double)(v-1)/(double)isocurveres));
	    VMOVE(pt1, p);
	    p = surf.PointAt(udom.ParameterAt((double)u/(double)gridres), vdom.ParameterAt((double)v/(double)isocurveres));
	    VMOVE(pt2, p);
	    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
	}
    }

    for (int v = 0; v <= gridres; v++) {
	if (v == 0 || v == gridres) {
	    vhead = bv_vlblock_find(vbp, red, green, blue);
	} else {
	    vhead = bv_vlblock_find(vbp, (int)(fill_rgb[0]), (int)(fill_rgb[1]), (int)(fill_rgb[2]));
	}
	for (int u = 1; u <= isocurveres; u++) {
	    ON_3dPoint p = surf.PointAt(udom.ParameterAt((double)(u-1)/(double)isocurveres), vdom.ParameterAt((double)v/(double)gridres));
	    VMOVE(pt1, p);
	    p = surf.PointAt(udom.ParameterAt((double)u/(double)isocurveres), vdom.ParameterAt((double)v/(double)gridres));
	    VMOVE(pt2, p);
	    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
	}
    }
    return;
}

void
plotface(const ON_BrepFace &face, struct bu_list *vlfree, struct bv_vlblock *vbp, int plotres, bool dim3d, const int red = 255, const int green = 255, const int blue = 0)
{
    struct bu_list *vhead;
    const ON_Surface* surf = face.SurfaceOf();
    fastf_t umin, umax;
    fastf_t pt1[3], pt2[3];
    ON_2dPoint from, to;

    ON_TextLog tl(stderr);

    vhead = bv_vlblock_find(vbp, red, green, blue);

    surf->GetDomain(0, &umin, &umax);
    for (int i = 0; i < face.LoopCount(); i++) {
	const ON_BrepLoop* loop = face.Loop(i);
	// for each trim
	for (int j = 0; j < loop->m_ti.Count(); j++) {
	    const ON_BrepTrim& trim = face.Brep()->m_T[loop->m_ti[j]];
	    const ON_Curve* trimCurve = trim.TrimCurveOf();
	    //trimCurve->Dump(tl);

	    ON_Interval dom = trimCurve->Domain();
	    // XXX todo: dynamically sample the curve
	    for (int k = 1; k <= plotres; k++) {
		ON_3dPoint p = trimCurve->PointAt(dom.ParameterAt((double) (k - 1) / (double) plotres));
		if (dim3d)
		    p = surf->PointAt(p.x, p.y);
		VMOVE(pt1, p);
		p = trimCurve->PointAt(dom.ParameterAt((double) k / (double) plotres));
		if (dim3d)
		    p = surf->PointAt(p.x, p.y);
		VMOVE(pt2, p);
		BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
		BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
	    }
	}
    }

    return;
}

static void
plotUVDomain2d(ON_BrepFace *face, struct bu_list *vlfree, struct bv_vlblock *vbp)
{
    struct bu_list *vhead;
    const ON_Surface* surf = face->SurfaceOf();
    fastf_t umin, umax, urange;
    fastf_t vmin, vmax, vrange;
    fastf_t pt1[3], pt2[3];
    ON_2dPoint from, to;

    ON_TextLog tl(stderr);

    vhead = bv_vlblock_find(vbp, PURERED);

    double width, height;
    ON_BoundingBox loop_bb;
    ON_BoundingBox trim_bb;
#ifndef RESETDOMAIN
    if (face->GetSurfaceSize(&width, &height)) {
	face->SetDomain(0, 0.0, width);
	face->SetDomain(1, 0.0, height);
    }
#endif
    surf->GetDomain(0, &umin, &umax);
    surf->GetDomain(1, &vmin, &vmax);
    // add a little offset so we can see the boundary curves
    urange = umax - umin;
    vrange = vmax - vmin;
    umin = umin - 0.01*urange;
    vmin = vmin - 0.01*vrange;
    umax = umax + 0.01*urange;
    vmax = vmax + 0.01*vrange;

    //umin
    VSET(pt1, umin, vmin, 0.0);
    VSET(pt2, umin, vmax, 0.0);
    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
    // umax
    VSET(pt1, umax, vmin, 0.0);
    VSET(pt2, umax, vmax, 0.0);
    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
    //vmin
    VSET(pt1, umin, vmin, 0.0);
    VSET(pt2, umax, vmin, 0.0);
    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
    //vmax
    VSET(pt1, umin, vmax, 0.0);
    VSET(pt2, umax, vmax, 0.0);
    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);

    return;
}

/* a binary predicate for std:list implemented as a function */
static bool
near_equal(double first, double second)
{
    /* FIXME: arbitrary nearness tolerance */
    return NEAR_EQUAL(first, second, 1e-6);
}

static void
drawisoUCheckForTrim(const SurfaceTree* st, struct bu_list *vlfree, struct bv_vlblock *vbp, fastf_t from, fastf_t to, fastf_t v, int UNUSED(curveres))
{
    struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    std::list<const BRNode*> m_trims_right;
    std::list<fastf_t> trim_hits;

    vhead = bv_vlblock_find(vbp, YELLOW);

    const ON_Surface *surf = st->getSurface();
    const CurveTree *ctree = st->m_ctree;
    fastf_t umin, umax;
    surf->GetDomain(0, &umin, &umax);

    m_trims_right.clear();

    fastf_t tol = 0.001;
    ON_2dPoint pt;
    pt.x = umin;
    pt.y = v;

    if (ctree != NULL) {
	m_trims_right.clear();
	ctree->getLeavesRight(m_trims_right, pt, tol);
    }

    int cnt = 1;
    //bu_log("V - %f\n", pt.x);
    trim_hits.clear();
    for (std::list<const BRNode*>::const_iterator i = m_trims_right.begin(); i != m_trims_right.end(); i++, cnt++) {
	const BRNode* br = *i;

	point_t bmin, bmax;
	if (!br->m_Horizontal) {
	    br->GetBBox(bmin, bmax);
	    if (((bmin[Y] - tol) <= pt[Y]) && (pt[Y] <= (bmax[Y]+tol))) { //if check trim and in BBox
		fastf_t u = br->getCurveEstimateOfU(pt[Y], tol);
		trim_hits.push_back(u);
		//bu_log("%d U %d - %f pt %f, %f bmin %f, %f  bmax %f, %f\n", br->m_face->m_face_index, cnt, u, pt.x, pt.y, bmin[X], bmin[Y], bmax[X], bmax[Y]);
	    }
	}
    }
    trim_hits.sort();
    trim_hits.unique(near_equal);

    int hit_cnt = trim_hits.size();
    cnt = 1;
    //bu_log("\tdrawisoUCheckForTrim: hit_cnt %d from center  %f %f 0.0 to center %f %f 0.0\n", hit_cnt, from, v , to, v);

    if ((hit_cnt > 0) && ((hit_cnt % 2) == 0)) {
	/*
	   if ((hit_cnt % 2) != 0) {
	//bu_log("V - %f\n", pt.y);
	if (!trim_hits.empty()) {
	fastf_t end = trim_hits.front();
	trim_hits.pop_front();
	//bu_log("\tfrom - %f, to - %f\n", from, to);
	fastf_t deltax = (end - from) / 50.0;
	if (deltax > 0.001) {
	for (fastf_t x = from; x < end && x < to; x = x + deltax) {
	ON_3dPoint p = surf->PointAt(x, pt.y);
	VMOVE(pt1, p);
	if (x + deltax > end) {
	if (x + deltax > to) {
	p = surf->PointAt(to, pt.y);
	} else {
	p = surf->PointAt(end, pt.y);
	}
	} else {
	if (x + deltax > to) {
	p = surf->PointAt(to, pt.y);
	} else {
	p = surf->PointAt(x + deltax, pt.y);
	}
	}
	VMOVE(pt2, p);

	//bu_log(
	//		"\t\t%d from center  %f %f 0.0 to center %f %f 0.0\n",
	//		cnt++, x, v, x + deltax, v);

	BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
	}
	}
	}
	}
	*/
	while (!trim_hits.empty()) {
	    fastf_t start = trim_hits.front();
	    if (start < from) {
		start = from;
	    }
	    trim_hits.pop_front();
	    fastf_t end = trim_hits.front();
	    if (end > to) {
		end = to;
	    }
	    trim_hits.pop_front();
	    //bu_log("\tfrom - %f, to - %f\n", from, to);
	    fastf_t deltax = (end - start) / 50.0;
	    if (deltax > 0.001) {
		for (fastf_t x = start; x < end; x = x + deltax) {
		    ON_3dPoint p = surf->PointAt(x, pt.y);
		    VMOVE(pt1, p);
		    if (x + deltax > end) {
			p = surf->PointAt(end, pt.y);
		    } else {
			p = surf->PointAt(x + deltax, pt.y);
		    }
		    VMOVE(pt2, p);

		    //				bu_log(
		    //						"\t\t%d from center  %f %f 0.0 to center %f %f 0.0\n",
		    //						cnt++, x, v, x + deltax, v);

		    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
		    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
		}
	    }
	}
    }

    return;
}


static void
drawisoVCheckForTrim(const SurfaceTree* st, struct bu_list *vlfree, struct bv_vlblock *vbp, fastf_t from, fastf_t to, fastf_t u, int UNUSED(curveres))
{
    struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    std::list<const BRNode*> m_trims_above;
    std::list<fastf_t> trim_hits;

    vhead = bv_vlblock_find(vbp, YELLOW);

    const ON_Surface *surf = st->getSurface();
    const CurveTree *ctree = st->m_ctree;
    fastf_t vmin, vmax;
    surf->GetDomain(1, &vmin, &vmax);

    m_trims_above.clear();

    fastf_t tol = 0.001;
    ON_2dPoint pt;
    pt.x = u;
    pt.y = vmin;

    if (ctree != NULL) {
	m_trims_above.clear();
	ctree->getLeavesAbove(m_trims_above, pt, tol);
    }

    int cnt = 1;
    trim_hits.clear();
    for (std::list<const BRNode*>::const_iterator i = m_trims_above.begin(); i != m_trims_above.end(); i++, cnt++) {
	const BRNode* br = *i;

	point_t bmin, bmax;
	if (!br->m_Vertical) {
	    br->GetBBox(bmin, bmax);

	    if (((bmin[X] - tol) <= pt[X]) && (pt[X] <= (bmax[X]+tol))) { //if check trim and in BBox
		fastf_t v = br->getCurveEstimateOfV(pt[X], tol);
		trim_hits.push_back(v);
		//bu_log("%d V %d - %f pt %f, %f bmin %f, %f  bmax %f, %f\n", br->m_face->m_face_index, cnt, v, pt.x, pt.y, bmin[X], bmin[Y], bmax[X], bmax[Y]);
	    }
	}
    }
    trim_hits.sort();
    trim_hits.unique(near_equal);

    size_t hit_cnt = trim_hits.size();
    cnt = 1;

    //bu_log("\tdrawisoVCheckForTrim: hit_cnt %d from center  %f %f 0.0 to center %f %f 0.0\n", hit_cnt, u, from, u, to);

    if ((hit_cnt > 0) && ((hit_cnt % 2) == 0)) {
	/*
	   if ((hit_cnt % 2) != 0) { //odd starting inside
	//bu_log("V - %f\n", pt.y);
	if (!trim_hits.empty()) {
	fastf_t end = trim_hits.front();
	trim_hits.pop_front();
	//bu_log("\tfrom - %f, to - %f\n", from, to);
	fastf_t deltay = (end - from) / 50.0;
	if (deltay > 0.001) {
	for (fastf_t y = from; y < end && y < to; y = y + deltay) {
	ON_3dPoint p = surf->PointAt(pt.x, y);
	VMOVE(pt1, p);
	if (y + deltay > end) {
	if (y + deltay > to) {
	p = surf->PointAt(pt.x, to);
	} else {
	p = surf->PointAt(pt.x, end);
	}
	} else {
	if (y + deltay > to) {
	p = surf->PointAt(pt.x, to);
	} else {
	p = surf->PointAt(pt.x, y + deltay);
	}
	}
	VMOVE(pt2, p);

	//bu_log(
	//		"\t\t%d from center  %f %f 0.0 to center %f %f 0.0\n",
	//		cnt++, u, y, u, y + deltay);

	BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
	}
	}

	}
	}
	*/
	while (!trim_hits.empty()) {
	    fastf_t start = trim_hits.front();
	    trim_hits.pop_front();
	    if (start < from) {
		start = from;
	    }
	    fastf_t end = trim_hits.front();
	    trim_hits.pop_front();
	    if (end > to) {
		end = to;
	    }
	    //bu_log("\tfrom - %f, to - %f\n", from, to);
	    fastf_t deltay = (end - start) / 50.0;
	    if (deltay > 0.001) {
		for (fastf_t y = start; y < end; y = y + deltay) {
		    ON_3dPoint p = surf->PointAt(pt.x, y);
		    VMOVE(pt1, p);
		    if (y + deltay > end) {
			p = surf->PointAt(pt.x, end);
		    } else {
			p = surf->PointAt(pt.x, y + deltay);
		    }
		    VMOVE(pt2, p);

		    //bu_log("\t\t%d from center  %f %f 0.0 to center %f %f 0.0\n",
		    //		cnt++, u, y, u, y + deltay);

		    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
		    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
		}
	    }
	}
    }
    return;
}


static void
drawisoU(const SurfaceTree* st, struct bu_list *vlfree, struct bv_vlblock *vbp, fastf_t from, fastf_t to, fastf_t v, int curveres)
{
    struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    fastf_t deltau = (to - from) / curveres;
    const ON_Surface *surf = st->getSurface();

    vhead = bv_vlblock_find(vbp, YELLOW);
    for (fastf_t u = from; u < to; u = u + deltau) {
	ON_3dPoint p = surf->PointAt(u, v);
	//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y, p.x, p.y, p.z);
	VMOVE(pt1, p);
	if (u + deltau > to) {
	    p = surf->PointAt(to, v);
	} else {
	    p = surf->PointAt(u + deltau, v);
	}
	//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y+deltay, p.x, p.y, p.z);
	VMOVE(pt2, p);
	BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
    }
}


static void
drawisoV(const SurfaceTree* st, struct bu_list *vlfree, struct bv_vlblock *vbp, fastf_t from, fastf_t to, fastf_t u, int curveres)
{
    struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    fastf_t deltav = (to - from) / curveres;
    const ON_Surface *surf = st->getSurface();

    vhead = bv_vlblock_find(vbp, YELLOW);
    for (fastf_t v = from; v < to; v = v + deltav) {
	ON_3dPoint p = surf->PointAt(u, v);
	//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y, p.x, p.y, p.z);
	VMOVE(pt1, p);
	if (v + deltav > to) {
	    p = surf->PointAt(u, to);
	} else {
	    p = surf->PointAt(u, v + deltav);
	}
	//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y+deltay, p.x, p.y, p.z);
	VMOVE(pt2, p);
	BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
    }
}



static void
drawBBNode(const SurfaceTree* st, struct bu_list *vlfree, struct bv_vlblock *vbp, const BBNode * node)
{
    if (node->isLeaf()) {
	//draw leaf
	if (node->m_trimmed) {
	    return; // nothing to do node is trimmed
	} else if (node->m_checkTrim) { // node may contain trim check all corners
	    fastf_t u =  node->m_u[0];
	    fastf_t v = node->m_v[0];
	    fastf_t from = u;
	    fastf_t to = node->m_u[1];
	    //bu_log("drawBBNode: node %x uvmin center %f %f 0.0, uvmax center %f %f 0.0\n", node, node->m_u[0], node->m_v[0], node->m_u[1], node->m_v[1]);

	    drawisoUCheckForTrim(st, vlfree, vbp, from, to, v, 3); //bottom
	    v = node->m_v[1];
	    drawisoUCheckForTrim(st, vlfree, vbp, from, to, v, 3); //top
	    from = node->m_v[0];
	    to = node->m_v[1];
	    drawisoVCheckForTrim(st, vlfree, vbp, from, to, u, 3); //left
	    u = node->m_u[1];
	    drawisoVCheckForTrim(st, vlfree, vbp, from, to, u, 3); //right

	    return;
	} else { // fully untrimmed just draw bottom and right edges
	    fastf_t u =  node->m_u[0];
	    fastf_t v = node->m_v[0];
	    fastf_t from = u;
	    fastf_t to = node->m_u[1];
	    drawisoU(st, vlfree, vbp, from, to, v, 10); //bottom
	    from = v;
	    to = node->m_v[1];
	    drawisoV(st, vlfree, vbp, from, to, u, 10); //right
	    return;
	}
    } else {
	if (!node->get_children().empty()) {
	    for (std::vector<BBNode*>::const_iterator childnode = node->get_children().begin(); childnode
		    != node->get_children().end(); childnode++) {
		drawBBNode(st, vlfree, vbp, *childnode);
	    }
	}
    }
}

static void
plotFaceFromSurfaceTree(const SurfaceTree* st, struct bu_list *vlfree, struct bv_vlblock *vbp, int UNUSED(isocurveres), int UNUSED(gridres))
{
    const BBNode *root = st->getRootNode();
    drawBBNode(st, vlfree, vbp, root);
}

FILE*
brep_plot_file(const char *pname = NULL)
{
    static FILE* plot = NULL;

    if (plot != NULL) {
	(void)fclose(plot);
	plot = NULL;
    }
    if (pname == NULL) {
	pname = "out.plot3";
    }
    plot = fopen(pname, "wb");
    point_t min, max;
    VSET(min, -2048, -2048, -2048);
    VSET(max, 2048, 2048, 2048);
    pdv_3space(plot, min, max);

    return plot;
}

#define ARB_FACE(valp, a, b, c, d)			\
    BV_ADD_VLIST(vlfree, vhead, valp[a], BV_VLIST_LINE_MOVE);	\
    BV_ADD_VLIST(vlfree, vhead, valp[b], BV_VLIST_LINE_DRAW);	\
    BV_ADD_VLIST(vlfree, vhead, valp[c], BV_VLIST_LINE_DRAW);	\
    BV_ADD_VLIST(vlfree, vhead, valp[d], BV_VLIST_LINE_DRAW);

#define BB_PLOT(min, max) {		\
    fastf_t pt[8][3];			\
    VSET(pt[0], max[X], min[Y], min[Z]);	\
    VSET(pt[1], max[X], max[Y], min[Z]);	\
    VSET(pt[2], max[X], max[Y], max[Z]);	\
    VSET(pt[3], max[X], min[Y], max[Z]);	\
    VSET(pt[4], min[X], min[Y], min[Z]);	\
    VSET(pt[5], min[X], max[Y], min[Z]);	\
    VSET(pt[6], min[X], max[Y], max[Z]);	\
    VSET(pt[7], min[X], min[Y], max[Z]);	\
    ARB_FACE(pt, 0, 1, 2, 3);		\
    ARB_FACE(pt, 4, 0, 3, 7);		\
    ARB_FACE(pt, 5, 4, 7, 6);		\
    ARB_FACE(pt, 1, 5, 6, 2);		\
}

static unsigned int
plotsurfaceleafs(const SurfaceTree* surf, struct bu_list *vlfree, struct bv_vlblock *vbp, bool dim3d)
{
    struct bu_list *vhead;
    fastf_t min[3] = VINIT_ZERO;
    fastf_t max[3];
    std::list<const BBNode*> leaves;
    surf->getLeaves(leaves);

    ON_TextLog tl(stderr);

    vhead = bv_vlblock_find(vbp, PURERED);
    BV_ADD_VLIST(vlfree, vhead, min, BV_VLIST_LINE_MOVE);
    vhead = bv_vlblock_find(vbp, BLUE);
    BV_ADD_VLIST(vlfree, vhead, min, BV_VLIST_LINE_MOVE);
    vhead = bv_vlblock_find(vbp, MAGENTA);
    BV_ADD_VLIST(vlfree, vhead, min, BV_VLIST_LINE_MOVE);

    for (std::list<const BBNode*>::const_iterator i = leaves.begin(); i != leaves.end(); i++) {
	const BBNode* bb = *i;
	if (bb->m_trimmed) {
	    vhead = bv_vlblock_find(vbp, PURERED);
	} else if (bb->m_checkTrim) {
	    vhead = bv_vlblock_find(vbp, BLUE);
	} else {
	    vhead = bv_vlblock_find(vbp, MAGENTA);
	}
	if (dim3d) {
	    bb->GetBBox(min, max);
	} else {
	    VSET(min, bb->m_u[0]+0.001, bb->m_v[0]+0.001, 0.0);
	    VSET(max, bb->m_u[1]-0.001, bb->m_v[1]-0.001, 0.0);
	}
	BB_PLOT(min, max);
    }

    return leaves.size();
}


static void
plottrimleafs(const SurfaceTree* st, struct bu_list *vlfree, struct bv_vlblock *vbp, bool dim3d)
{
    struct bu_list *vhead;
    vect_t min = VINIT_ZERO;
    vect_t max;
    std::list<const BRNode*> leaves;
    st->m_ctree->getLeaves(leaves);

    ON_TextLog tl(stderr);

    vhead = bv_vlblock_find(vbp, PURERED);
    BV_ADD_VLIST(vlfree, vhead, min, BV_VLIST_LINE_MOVE);
    vhead = bv_vlblock_find(vbp, BLUE);
    BV_ADD_VLIST(vlfree, vhead, min, BV_VLIST_LINE_MOVE);
    vhead = bv_vlblock_find(vbp, MAGENTA);
    BV_ADD_VLIST(vlfree, vhead, min, BV_VLIST_LINE_MOVE);

    for (std::list<const BRNode*>::const_iterator i = leaves.begin(); i != leaves.end(); i++) {
	const BRNode* bb = *i;
	if (bb->m_XIncreasing) {
	    vhead = bv_vlblock_find(vbp, GREEN);
	} else {
	    vhead = bv_vlblock_find(vbp, BLUE);
	}
	bb->GetBBox(min, max);
	if (dim3d) {
	    const ON_Surface *surf = st->getSurface();

	    ON_3dPoint p1 = surf->PointAt(min[0], min[1]);
	    ON_3dPoint p2 = surf->PointAt(max[0], max[1]);
	    VMOVE(min, p1);
	    VMOVE(max, p2);
	}
	BB_PLOT(min, max);
    }

    return;
}

static void
plottrimdirection(const ON_BrepFace &face, struct bu_list *vlfree, struct bv_vlblock *vbp, int plotres)
{
    struct bu_list *vhead;
    const ON_Surface* surf = face.SurfaceOf();
    fastf_t umin, umax;
    fastf_t pt1[3], pt2[3];
    ON_2dPoint from, to;

    ON_TextLog tl(stderr);

    vhead = bv_vlblock_find(vbp, GREEN);

    surf->GetDomain(0, &umin, &umax);
    for (int i = 0; i < face.LoopCount(); i++) {
	const ON_BrepLoop* loop = face.Loop(i);
	// for each trim
	for (int j = 0; j < loop->m_ti.Count(); j++) {
	    const ON_BrepTrim& trim = face.Brep()->m_T[loop->m_ti[j]];
	    const ON_Curve* trimCurve = trim.TrimCurveOf();
	    //trimCurve->Dump(tl);

	    int knotcnt = trimCurve->SpanCount();
	    fastf_t *knots = new fastf_t[knotcnt + 1];

	    trimCurve->GetSpanVector(knots);
	    for (int k = 1; k <= knotcnt; k++) {
		fastf_t dist = knots[k] - knots[k-1];
		fastf_t step = dist/plotres;
		for (fastf_t t=knots[k-1]+step; t<=knots[k]; t=t+step) {
		    ON_3dPoint p = trimCurve->PointAt(t);
		    p = surf->PointAt(p.x, p.y);
		    ON_3dPoint prev = trimCurve->PointAt(t-step*0.1);
		    prev = surf->PointAt(prev.x, prev.y);
		    ON_3dVector N = surf->NormalAt(p.x, p.y);
		    N.Unitize();
		    ON_3dVector tan = p - prev;
		    tan.Unitize();
		    prev = p - tan;
		    ON_3dVector A = ON_CrossProduct(tan, N);
		    A.Unitize();
		    ON_3dVector B = ON_CrossProduct(N, tan);
		    B.Unitize();
		    ON_3dPoint a = prev + A;
		    ON_3dPoint b = prev + B;
		    VMOVE(pt1, p);
		    VMOVE(pt2, a);
		    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
		    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
		    VMOVE(pt2, b);
		    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
		    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
		}
	    }
	}
    }

    return;
}

static void
plotsurfacenormals(const ON_Surface &surf, struct bu_list *vlfree, struct bv_vlblock *vbp, int gridres)
{
    struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    ON_2dPoint from, to;

    vhead = bv_vlblock_find(vbp, GREEN);

    ON_Interval udom = surf.Domain(0);
    ON_Interval vdom = surf.Domain(1);

    for (int u = 0; u <= gridres; u++) {
	for (int v = 1; v <= gridres; v++) {
	    ON_3dPoint p = surf.PointAt(udom.ParameterAt((double)u/(double)gridres), vdom.ParameterAt((double)(v-1)/(double)gridres));
	    ON_3dVector n = surf.NormalAt(udom.ParameterAt((double)u/(double)gridres), vdom.ParameterAt((double)(v-1)/(double)gridres));
	    n.Unitize();
	    VMOVE(pt1, p);
	    VSCALE(pt2, n, surf.BoundingBox().Diagonal().Length()*0.1);
	    VADD2(pt2, pt1, pt2);
	    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
	}
    }
    return;
}


static void
plotsurfaceknots(ON_Surface &surf, struct bu_list *vlfree, struct bv_vlblock *vbp, bool dim3d)
{
    struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    ON_2dPoint from, to;
    int spanu_cnt = surf.SpanCount(0);
    int spanv_cnt = surf.SpanCount(1);
    fastf_t *spanu = NULL;
    fastf_t *spanv = NULL;
    spanu = new fastf_t[spanu_cnt+1];
    spanv = new fastf_t[spanv_cnt+1];

#ifndef RESETDOMAIN
    double width, height;
    if (surf.GetSurfaceSize(&width, &height)) {
	surf.SetDomain(0, 0.0, width);
	surf.SetDomain(1, 0.0, height);

    }
#endif

    surf.GetSpanVector(0, spanu);
    surf.GetSpanVector(1, spanv);

    vhead = bv_vlblock_find(vbp, GREEN);

    ON_Interval udom = surf.Domain(0);
    ON_Interval vdom = surf.Domain(1);

    if (dim3d) {
	for (int u = 0; u <= spanu_cnt; u++) {
	    for (int v = 0; v <= spanv_cnt; v++) {
		ON_3dPoint p = surf.PointAt(spanu[u], spanv[v]);
		ON_3dVector n = surf.NormalAt(spanu[u], spanv[v]);
		n.Unitize();
		VMOVE(pt1, p);
		VSCALE(pt2, n, 3.0);
		VADD2(pt2, pt1, pt2);
		BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
		BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
	    }
	}
    } else {
	for (int u = 0; u <= spanu_cnt; u++) {
	    for (int v = 0; v <= spanv_cnt; v++) {
		VSET(pt1, spanu[u], spanv[v], 0.0);
		BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_POINT_DRAW);
	    }
	}
    }
    return;
}

static void
plot_nurbs_cv(struct bu_list *vlfree, struct bv_vlblock *vbp, int ucount, int vcount, const ON_NurbsSurface *ns)
{
    struct bu_list *vhead;
    vhead = bv_vlblock_find(vbp, PEACH);
    ON_3dPoint cp;
    fastf_t pt1[3], pt2[3];
    int i, j, k, temp;
    for (k = 0; k < 2; k++) {
	/* two times i loop */

	for (i = 0; i < ucount; ++i) {
	    if (k == 1)
		ns->GetCV(0, i, cp);	   /* i < ucount */
	    else
		ns->GetCV(i, 0, cp);       /* i < vcount */

	    VMOVE(pt1, cp);
	    for (j = 0; j < vcount; ++j) {
		if (k == 1)
		    ns->GetCV(j, i, cp);
		else
		    ns->GetCV(i, j, cp);

		VMOVE(pt2, cp);
		BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
		BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
		VMOVE(pt1, cp);
		BV_ADD_VLIST(vlfree, vhead, cp, BV_VLIST_POINT_DRAW);
	    }
	}
	temp  = ucount;
	ucount = vcount;
	vcount = temp;
    }
}

struct _ged_brep_iplot {
    struct _ged_brep_info *gb;
    struct bu_vls *vls;
    const struct bu_cmdtab *cmds;
};

static int
_brep_plot_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_brep_iplot *gb = (struct _ged_brep_iplot *)bs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gb->vls, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->vls, "%s\n", ps);
	return 1;
    }
    return 0;
}

// C2 - 2D parameter curves
extern "C" int
_brep_cmd_curve_2d_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot C2 [[index][index-index]]";
    const char *purpose_string = "2D parameter space geometric curves";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    int plotres = gib->gb->plotres;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_C2.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int ci = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_Curve* curve = brep->m_C2[ci];

	if (!curve->IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "curve %d is not valid, skipping", ci);
	    continue;
	}

	if (color) {
	    plotcurve(*curve, vlfree, vbp, plotres, (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	} else {
	    plotcurve(*curve, vlfree, vbp, plotres);
	}
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_C2_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// C3 - 3D edge curves
extern "C" int
_brep_cmd_curve_3d_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot C3 [[index][index-index]]";
    const char *purpose_string = "3D parameter space geometric curves";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    int plotres = gib->gb->plotres;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_C3.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int ci = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_Curve* curve = brep->m_C3[ci];
	if (!curve->IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "curve %d is not valid, skipping", ci);
	    continue;
	}
	if (color) {
	    plotcurve(*curve, vlfree, vbp, plotres, (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	} else {
	    plotcurve(*curve, vlfree, vbp, plotres);
	}
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_C3_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// E - topological edges
extern "C" int
_brep_cmd_edge_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot E [[index][index-index]]";
    const char *purpose_string = "topological 3D edges";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    int plotres = gib->gb->plotres;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_E.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int ei = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_BrepEdge &edge = brep->m_E[ei];
	if (!edge.IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "edge %d is not valid, skipping", ei);
	    continue;
	}
	const ON_Curve* curve = edge.EdgeCurveOf();
	if (!curve->IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "curve %d associated with edge %d is not valid, skipping", edge.m_c3i, ei);
	    continue;
	}
	if (color) {
	    plotcurve(*curve, vlfree, vbp, plotres, (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	} else {
	    plotcurve(*curve, vlfree, vbp, plotres);
	}
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_E_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;

}

// F - topological faces
extern "C" int
_brep_cmd_face_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot F [[index][index-index]]";
    const char *purpose_string = "topological faces";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    int plotres = gib->gb->plotres;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int fi = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_BrepFace& face = brep->m_F[fi];
	if (!face.IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "face %d is not valid, skipping", fi);
	    continue;
	}

	if (color) {
	    plotface(face, vlfree, vbp, plotres, true, (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	} else {
	    plotface(face, vlfree, vbp, plotres, true);
	}
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_F_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// F2d - topological faces in parametric space
extern "C" int
_brep_cmd_face_2d_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot F2d [[index][index-index]]";
    const char *purpose_string = "topological faces in parametric space";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    int plotres = gib->gb->plotres;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int fi = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_BrepFace& face = brep->m_F[fi];
	if (!face.IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "face %d is not valid, skipping", fi);
	    continue;
	}

	plotUVDomain2d(brep->Face(fi), vlfree, vbp);
	if (color) {
	    plotface(face, vlfree, vbp, plotres, false, (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	} else {
	    plotface(face, vlfree, vbp, plotres, false);
	}
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_F2d_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// FSBB - face surface bounding boxes
extern "C" int
_brep_cmd_face_surface_bbox_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot FSBB [[index][index-index]]";
    const char *purpose_string = "face surface bounding boxes";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int fi = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	ON_BrepFace *face = brep->Face(fi);
	if (!face->IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "face %d is not valid, skipping", fi);
	    continue;
	}

	const ON_Surface *s = face->SurfaceOf();
	double surface_width,surface_height;
	if (s->GetSurfaceSize(&surface_width,&surface_height)) {
	    // reparameterization of the face's surface and transforms the "u"
	    // and "v" coordinates of all the face's parameter space trimming
	    // curves to minimize distortion in the map from parameter space to 3d..
	    face->SetDomain(0, 0.0, surface_width);
	    face->SetDomain(1, 0.0, surface_height);
	}
	const SurfaceTree st(face);
	unsigned int lcnt = plotsurfaceleafs(&st, vlfree, vbp, true);
	bu_vls_printf(gib->vls, "Face: %d contains %d SBBs", fi, lcnt);
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_SBB_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// FSBB2d - surface bounding boxes in parametric space
extern "C" int
_brep_cmd_face_surface_bbox_2d_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot FSBB2d [[index][index-index]]";
    const char *purpose_string = "2D parameter space surface bounding boxes";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int fi = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	ON_BrepFace *face = brep->Face(fi);
	if (!face->IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "face %d is not valid, skipping", fi);
	    continue;
	}

	const ON_Surface *s = face->SurfaceOf();
	double surface_width,surface_height;
	if (s->GetSurfaceSize(&surface_width,&surface_height)) {
	    // reparameterization of the face's surface and transforms the "u"
	    // and "v" coordinates of all the face's parameter space trimming
	    // curves to minimize distortion in the map from parameter space to 3d..
	    face->SetDomain(0, 0.0, surface_width);
	    face->SetDomain(1, 0.0, surface_height);
	}
	const SurfaceTree st(face);
	unsigned int lcnt = plotsurfaceleafs(&st, vlfree, vbp, false);
	bu_vls_printf(gib->vls, "Face: %d contains %d SBBs", fi, lcnt);
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_SBB_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// FTBB - trim bboxes in 3D
extern "C" int
_brep_cmd_face_trim_bbox_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot FTBB [[index][index-index]]";
    const char *purpose_string = "face trim bounding boxes in 3D";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int fi = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_BrepFace& face = brep->m_F[fi];
	if (!face.IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "face %d is not valid, skipping", fi);
	    continue;
	}

	const SurfaceTree st(&face);
	plottrimleafs(&st, vlfree, vbp, true);
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_TBB_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// FTBB2d - face trim bboxes in 2D 
extern "C" int
_brep_cmd_face_trim_bbox_2d_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot FTBB2d [[index][index-index]]";
    const char *purpose_string = "trim bounding boxes in parametric space";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int fi = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_BrepFace& face = brep->m_F[fi];

	if (!face.IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "face %d is not valid, skipping", fi);
	    continue;
	}

	const SurfaceTree st(&face);
	plottrimleafs(&st, vlfree, vbp, false);
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_TBB_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// FTD - trim direction
extern "C" int
_brep_cmd_face_trim_direction_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot FTD [[index][index-index]]";
    const char *purpose_string = "face trim direction";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    int plotres = gib->gb->plotres;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int fi = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_BrepFace& face = brep->m_F[fi];
	if (!face.IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "face %d is not valid, skipping", fi);
	    continue;
	}
	plottrimdirection(face, vlfree, vbp, plotres);
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_FTD_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;

}

// I - isosurfaces
extern "C" int
_brep_cmd_isosurface_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot I [[index][index-index]]";
    const char *purpose_string = "isosurfaces";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    int plotres = gib->gb->plotres;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int fi = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_BrepFace &face = brep->m_F[fi];
	if (!face.IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "face %d is not valid, skipping", fi);
	    continue;
	}

	const SurfaceTree st(&face, true, 0);
	plotface(face, vlfree, vbp, plotres, true);
	plotFaceFromSurfaceTree(&st, vlfree, vbp, plotres, plotres);
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_I_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// L - topological trimming loops in 3D
extern "C" int
_brep_cmd_loop_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot L [[index][index-index]]";
    const char *purpose_string = "topological trimming loops in 3D";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    int plotres = gib->gb->plotres;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_L.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int li = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_BrepLoop* loop = &(brep->m_L[li]);

	if (!loop->IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "loop %d is not valid, skipping", li);
	    continue;
	}

	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim& trim = brep->m_T[loop->m_ti[ti]];
	    if (color) {
		plottrim(trim, vlfree, vbp, plotres, true, (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	    } else {
		plottrim(trim, vlfree, vbp, plotres, true);
	    }
	}
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_L_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// L2d - 2D parameter space topological trimming loops
extern "C" int
_brep_cmd_loop_2d_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot L2d [[index][index-index]]";
    const char *purpose_string = "2D parameter space topological trimming loops";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    int plotres = gib->gb->plotres;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_L.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int li = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_BrepLoop* loop = &(brep->m_L[li]);
	if (!loop->IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "loop %d is not valid, skipping", li);
	    continue;
	}

	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim& trim = brep->m_T[loop->m_ti[ti]];
	    if (color) {
		plottrim(trim, vlfree, vbp, plotres, false, (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	    } else {
		plottrim(trim, vlfree, vbp, plotres, false);
	    }
	}
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_L2d_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// S - surfaces
extern "C" int
_brep_cmd_surface_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot S [[index][index-index]]";
    const char *purpose_string = "untrimmed surfaces";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    int plotres = gib->gb->plotres;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_S.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int si = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_Surface *surf = brep->m_S[si];
	if (!surf->IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "surface %d is not valid, skipping", si);
	    continue;
	}

	if (color) {
	    plotsurface(*surf, vlfree, vbp, plotres, 10, (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	} else {
	    plotsurface(*surf, vlfree, vbp, plotres, 10);
	}
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_S_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}



// SCV - surface control vertex mesh
extern "C" int
_brep_cmd_surface_control_verts_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot SCV [[index][index-index]]";
    const char *purpose_string = "surface control vertex mesh";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_S.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int si = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_Surface *surf = brep->m_S[si];
	if (!surf->IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "surface %d is not valid, skipping", si);
	    continue;
	}

	ON_NurbsSurface *ns = ON_NurbsSurface::New();
	surf->GetNurbForm(*ns, 0.0);
	int ucount, vcount;
	ucount = ns->m_cv_count[0];
	vcount = ns->m_cv_count[1];
	plot_nurbs_cv(vlfree, vbp, ucount, vcount, ns);
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_SCV_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// SK - surface knots in 3D
extern "C" int
_brep_cmd_surface_knot_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot SK [[index][index-index]]";
    const char *purpose_string = "surface knots in 3D";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_S.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int si = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	ON_Surface *surf = brep->m_S[si];
	if (!surf->IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "surface %d is not valid, skipping", si);
	    continue;
	}
	plotsurfaceknots(*surf, vlfree, vbp, true);
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_SK_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// SK2d - surface knots in parametric space
extern "C" int
_brep_cmd_surface_knot_2d_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot SK2d [[index][index-index]]";
    const char *purpose_string = "surface knots in parametric space";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_S.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int si = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	ON_Surface *surf = brep->m_S[si];
	if (!surf->IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "surface %d is not valid, skipping", si);
	    continue;
	}
	plotsurfaceknots(*surf, vlfree, vbp, false);
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_SK2d_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// SN - surface normals
extern "C" int
_brep_cmd_surface_normal_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot SN [[index][index-index]]";
    const char *purpose_string = "surface normals";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    int plotres = gib->gb->plotres;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_S.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int si = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	ON_Surface *surf = brep->m_S[si];
	if (!surf->IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "surface %d is not valid, skipping", si);
	    continue;
	}

	plotsurfaceknots(*surf, vlfree, vbp, true);
	plotsurfacenormals(*surf, vlfree, vbp, plotres);
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_SN_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// SUV - surface uv bounds plot
extern "C" int
_brep_cmd_surface_uv_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot SUV [options] [[index][index-index]]";
    const char *purpose_string = "surface uv bounds";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    // TODO - define options that allow specification of U min/max and V min/max intervals

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_S.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int si = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_Surface *surf = brep->m_S[si];
	if (!surf->IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "surface %d is not valid, skipping", si);
	    continue;
	}

	ON_Interval U = surf->Domain(0);
	ON_Interval V = surf->Domain(1);
	struct bu_list *vhead;
	fastf_t pt1[3], pt2[3];
	fastf_t delta = U.Length()/1000.0;

	vhead = bv_vlblock_find(vbp, YELLOW);
	for (int i = 0; i < 2; i++) {
	    fastf_t v = V.m_t[i];
	    for (fastf_t u = U.m_t[0]; u < U.m_t[1]; u = u + delta) {
		ON_3dPoint p = surf->PointAt(u, v);
		//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y, p.x, p.y, p.z);
		VMOVE(pt1, p);
		if (u + delta > U.m_t[1]) {
		    p = surf->PointAt(U.m_t[1], v);
		} else {
		    p = surf->PointAt(u + delta, v);
		}
		//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y+deltay, p.x, p.y, p.z);
		VMOVE(pt2, p);
		BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
		BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
	    }
	}
	delta = V.Length()/1000.0;
	for (int i = 0; i < 2; i++) {
	    fastf_t u = U.m_t[i];
	    for (fastf_t v = V.m_t[0]; v < V.m_t[1]; v = v + delta) {
		ON_3dPoint p = surf->PointAt(u, v);
		//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y, p.x, p.y, p.z);
		VMOVE(pt1, p);
		if (v + delta > V.m_t[1]) {
		    p = surf->PointAt(u,V.m_t[1]);
		} else {
		    p = surf->PointAt(u, v + delta);
		}
		//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y+deltay, p.x, p.y, p.z);
		VMOVE(pt2, p);
		BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
		BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
	    }
	}
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_SUV_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// SUVP - surface uv point plot
extern "C" int
_brep_cmd_surface_uv_point_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot SUVP index u v";
    const char *purpose_string = "surface 3D point at uv coordinates";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;

    if (argc != 3) {
	bu_vls_printf(gib->vls, "%s", usage_string);
	return GED_ERROR;
    }

    int si;
    double u, v;

    ON_wString wstr;
    ON_TextLog tl(wstr);

    if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&si) < 0) {
	bu_vls_printf(gib->vls, "invalid surface specifier: %s", argv[0]);
	return GED_ERROR;
    }

    if (si < 0 || si >= brep->m_S.Count()) {
	bu_vls_printf(gib->vls, "surface id %d is not valid", si);
	return GED_ERROR;
    }

    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&u) < 0) {
	bu_vls_printf(gib->vls, "invalid u coordinate specifier: %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[2], (void *)&v) < 0) {
	bu_vls_printf(gib->vls, "invalid u coordinate specifier: %s", argv[2]);
	return GED_ERROR;
    }

    unsigned char rgb[3];
    bu_color_to_rgb_chars(color, rgb);

    const ON_Surface *surf = brep->m_S[si];
    if (!surf->IsValid(NULL)) {
	bu_vls_printf(gib->vls, "surface %d is not valid, skipping", si);
	return GED_ERROR;
    }

    if (color) {
	plotpoint(surf->PointAt(u, v), vlfree, vbp, (int)rgb[0], (int)rgb[1], (int)rgb[2]);
    } else {
	plotpoint(surf->PointAt(u, v), vlfree, vbp, GREEN);
    }
    bu_vls_printf(gib->vls, "%s", ON_String(wstr).Array());

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_SUVP_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// T - topological trims in 3D
extern "C" int
_brep_cmd_trim_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot T [[index][index-index]]";
    const char *purpose_string = "topological trims in 3D";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    int plotres = gib->gb->plotres;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_T.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int ti = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_BrepTrim &trim = brep->m_T[ti];
	if (!trim.IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "trim %d is not valid, skipping", ti);
	    continue;
	}

	if (color) {
	    plottrim(trim, vlfree, vbp, plotres, true, (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	} else {
	    plottrim(trim, vlfree, vbp, plotres, true);
	}
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_T_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// T2d - 2D topological trims
extern "C" int
_brep_cmd_trim_2d_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot T2d [[index][index-index]]";
    const char *purpose_string = "2D parameter space topological trims";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    int plotres = gib->gb->plotres;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_T.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int ti = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_BrepTrim &trim = brep->m_T[ti];
	if (!trim.IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "trim %d is not valid, skipping", ti);
	    continue;
	}

	if (color) {
	    plottrim(trim, vlfree, vbp, plotres, false, (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	} else {
	    plottrim(trim, vlfree, vbp, plotres, false);
	}
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_T_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// V - 3D vertices
extern "C" int
_brep_cmd_vertex_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot V [[index][index-index]]";
    const char *purpose_string = "3D vertices";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_V.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;

    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {

	int vi = *e_it;

	unsigned char rgb[3];
	bu_color_to_rgb_chars(color, rgb);

	const ON_BrepVertex &vertex = brep->m_V[vi];
	if (!vertex.IsValid(NULL)) {
	    bu_vls_printf(gib->vls, "vertex %d is not valid, skipping", vi);
	    continue;
	}
	if (color) {
	    plotpoint(vertex.Point(), vlfree, vbp, (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	} else {
	    plotpoint(vertex.Point(), vlfree, vbp, GREEN);
	}
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_V_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// CDT - face triangulation
extern "C" int
_brep_cmd_face_cdt_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot CDT [[index][index-index]]";
    const char *purpose_string = "triangulation of face in 3D";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    //struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    const char *solid_name = gib->gb->solid_name.c_str();
    const struct bg_tess_tol *ttol = (const struct bg_tess_tol *)&gib->gb->gedp->ged_wdbp->wdb_ttol;
    const struct bn_tol *tol = &gib->gb->gedp->ged_wdbp->wdb_tol;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
	brep_facecdt_plot(gib->vls, solid_name, ttol, tol, brep, NULL, vbp, vlfree, *e_it, 0, -1);
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_CDT_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// CDT2d - face triangulation in parametric space
extern "C" int
_brep_cmd_face_cdt_2d_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot CDT2d [[index][index-index]]";
    const char *purpose_string = "triangulation of face in parametric space";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    //struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    const char *solid_name = gib->gb->solid_name.c_str();
    const struct bg_tess_tol *ttol = (const struct bg_tess_tol *)&gib->gb->gedp->ged_wdbp->wdb_ttol;
    const struct bn_tol *tol = &gib->gb->gedp->ged_wdbp->wdb_tol;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
	brep_facecdt_plot(gib->vls, solid_name, ttol, tol, brep, NULL, vbp, vlfree, *e_it, 2, -1);
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_CDT2d_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// CDTM2d - face triangulation ??
extern "C" int
_brep_cmd_face_cdt_m2d_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot CDTM2d [[index][index-index]]";
    const char *purpose_string = "Triangulation of face ??";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    //struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    const char *solid_name = gib->gb->solid_name.c_str();
    const struct bg_tess_tol *ttol = (const struct bg_tess_tol *)&gib->gb->gedp->ged_wdbp->wdb_ttol;
    const struct bn_tol *tol = &gib->gb->gedp->ged_wdbp->wdb_tol;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
	brep_facecdt_plot(gib->vls, solid_name, ttol, tol, brep, NULL, vbp, vlfree, *e_it, 3, -1);
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_CDTm2d_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// CDTP2d - face triangulation points?
extern "C" int
_brep_cmd_face_cdt_p2d_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot CDTP2d [[index][index-index]]";
    const char *purpose_string = "face triangulation points?";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    //struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    const char *solid_name = gib->gb->solid_name.c_str();
    const struct bg_tess_tol *ttol = (const struct bg_tess_tol *)&gib->gb->gedp->ged_wdbp->wdb_ttol;
    const struct bn_tol *tol = &gib->gb->gedp->ged_wdbp->wdb_tol;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
	brep_facecdt_plot(gib->vls, solid_name, ttol, tol, brep, NULL, vbp, vlfree, *e_it, 4, -1);
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_CDTp2d_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// CDTW - face triangulation wireframe in 3D
extern "C" int
_brep_cmd_face_cdt_wireframe_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot CDTW [[index][index-index]]";
    const char *purpose_string = "face triangulation wireframe in 3D";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    //struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    const char *solid_name = gib->gb->solid_name.c_str();
    const struct bg_tess_tol *ttol = (const struct bg_tess_tol *)&gib->gb->gedp->ged_wdbp->wdb_ttol;
    const struct bn_tol *tol = &gib->gb->gedp->ged_wdbp->wdb_tol;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    std::set<int>::iterator e_it;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
	brep_facecdt_plot(gib->vls, solid_name, ttol, tol, brep, NULL, vbp, vlfree, *e_it, 1, -1);
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_CDTw_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// CDTN - new face triangulation
extern "C" int
_brep_cmd_face_cdt2_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot CDTN [[index][index-index]]";
    const char *purpose_string = "(Debug) new triangulation of face in 3D";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    //struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    const char *solid_name = gib->gb->solid_name.c_str();
    const struct bg_tess_tol *ttol = (const struct bg_tess_tol *)&gib->gb->gedp->ged_wdbp->wdb_ttol;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }

    struct bu_color c;
    bu_color_from_str(&c, "255/255/0");

    ON_Brep_CDT_State *s_cdt = ON_Brep_CDT_Create((void *)brep, solid_name);

    struct bg_tess_tol cdttol = BG_TESS_TOL_INIT_ZERO;
    cdttol.abs = ttol->abs;
    cdttol.rel = ttol->rel;
    cdttol.norm = ttol->norm;
    ON_Brep_CDT_Tol_Set(s_cdt, &cdttol);

    if (elements.size()) {
	int face_cnt = 0;
	int *faces = (int *)bu_calloc(elements.size()+1, sizeof(int), "face array");
	std::set<int>::iterator e_it;
	for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
	    faces[face_cnt] = *e_it;
	    face_cnt++;
	}
	ON_Brep_CDT_Tessellate(s_cdt, face_cnt, faces);
	bu_free(faces, "free face array");
    } else {
	// If we have nothing, show all
	ON_Brep_CDT_Tessellate(s_cdt, 0, NULL);
    }

    ON_Brep_CDT_VList(vbp, vlfree, &c, 0, s_cdt);
    ON_Brep_CDT_Destroy(s_cdt);

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_CDTw_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// CDTN2d - new face triangulation in parametric space
extern "C" int
_brep_cmd_face_cdt2_2d_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot CDTN2d [[index][index-index]]";
    const char *purpose_string = "(Debug) new triangulation of face in parametric space";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    //struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    const char *solid_name = gib->gb->solid_name.c_str();
    const struct bg_tess_tol *ttol = (const struct bg_tess_tol *)&gib->gb->gedp->ged_wdbp->wdb_ttol;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    struct bu_color c;
    bu_color_from_str(&c, "255/255/0");
    int face_cnt = 0;
    int *faces = (int *)bu_calloc(elements.size()+1, sizeof(int), "face array");
    std::set<int>::iterator e_it;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
	faces[face_cnt] = *e_it;
	face_cnt++;
    }
    ON_Brep_CDT_State *s_cdt = ON_Brep_CDT_Create((void *)brep, solid_name);

    struct bg_tess_tol cdttol = BG_TESS_TOL_INIT_ZERO;
    cdttol.abs = ttol->abs;
    cdttol.rel = ttol->rel;
    cdttol.norm = ttol->norm;
    ON_Brep_CDT_Tol_Set(s_cdt, &cdttol);

    ON_Brep_CDT_Tessellate(s_cdt, face_cnt, faces);
    ON_Brep_CDT_VList(vbp, vlfree, &c, 2, s_cdt);
    ON_Brep_CDT_Destroy(s_cdt);
    bu_free(faces, "free face array");

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_CDT2d_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

// CDTNW - new face triangulation wireframe in 3D
extern "C" int
_brep_cmd_face_cdt2_wireframe_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname1> plot CDTNW [[index][index-index]]";
    const char *purpose_string = "(Debug) new face triangulation wireframe in 3D";
    if (_brep_plot_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--;argv++;

    struct _ged_brep_iplot *gib = (struct _ged_brep_iplot *)bs;
    const ON_Brep *brep = ((struct rt_brep_internal *)(gib->gb->intern.idb_ptr))->brep;
    //struct bu_color *color = gib->gb->color;
    struct bv_vlblock *vbp = gib->gb->vbp;
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    const char *solid_name = gib->gb->solid_name.c_str();
    const struct bg_tess_tol *ttol = (const struct bg_tess_tol *)&gib->gb->gedp->ged_wdbp->wdb_ttol;

    std::set<int> elements;
    if (_brep_indices(elements, gib->vls, argc, argv) != GED_OK) {
	return GED_ERROR;
    }
    // If we have nothing, report all
    if (!elements.size()) {
	for (int i = 0; i < brep->m_F.Count(); i++) {
	    elements.insert(i);
	}
    }

    struct bu_color c;
    bu_color_from_str(&c, "255/255/0");
    int face_cnt = 0;
    int *faces = (int *)bu_calloc(elements.size()+1, sizeof(int), "face array");
    std::set<int>::iterator e_it;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
	faces[face_cnt] = *e_it;
	face_cnt++;
    }
    ON_Brep_CDT_State *s_cdt = ON_Brep_CDT_Create((void *)brep, solid_name);

    struct bg_tess_tol cdttol = BG_TESS_TOL_INIT_ZERO;
    cdttol.abs = ttol->abs;
    cdttol.rel = ttol->rel;
    cdttol.norm = ttol->norm;
    ON_Brep_CDT_Tol_Set(s_cdt, &cdttol);

    ON_Brep_CDT_Tessellate(s_cdt, face_cnt, faces);
    ON_Brep_CDT_VList(vbp, vlfree, &c, 1, s_cdt);
    ON_Brep_CDT_Destroy(s_cdt);
    bu_free(faces, "free face array");

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "_BC_CDTw_%s", gib->gb->solid_name.c_str());
    _ged_cvt_vlblock_to_solids(gib->gb->gedp, vbp, bu_vls_cstr(&sname), 0);
    bu_vls_free(&sname);

    return GED_OK;
}

static void
_brep_plot_help(struct _ged_brep_iplot *bs, int argc, const char **argv)
{
    struct _ged_brep_iplot *gb = (struct _ged_brep_iplot *)bs;
    if (!argc || !argv) {
	bu_vls_printf(gb->vls, "brep [options] <objname> plot <subcommand> [args]\n");
	bu_vls_printf(gb->vls, "Available subcommands:\n");
	const struct bu_cmdtab *ctp = NULL;
	int ret;
	const char *helpflag[2];
	helpflag[1] = PURPOSEFLAG;
	for (ctp = gb->cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    bu_vls_printf(gb->vls, "  %s\t\t", ctp->ct_name);
	    helpflag[0] = ctp->ct_name;
	    bu_cmd(gb->cmds, 2, helpflag, 0, (void *)gb, &ret);
	}
    } else {
	int ret;
	const char *helpflag[2];
	helpflag[0] = argv[0];
	helpflag[1] = HELPFLAG;
	bu_cmd(gb->cmds, 2, helpflag, 0, (void *)gb, &ret);
    }
}

const struct bu_cmdtab _brep_plot_cmds[] = {
    { "C2",          _brep_cmd_curve_2d_plot},
    { "C3",          _brep_cmd_curve_3d_plot},
    { "E",           _brep_cmd_edge_plot},
    { "F",           _brep_cmd_face_plot},
    { "F2d",         _brep_cmd_face_2d_plot},
    { "FSBB",        _brep_cmd_face_surface_bbox_plot},
    { "FSBB2d",      _brep_cmd_face_surface_bbox_2d_plot},
    { "FTBB",        _brep_cmd_face_trim_bbox_plot},
    { "FTBB2d",      _brep_cmd_face_trim_bbox_2d_plot},
    { "FTD",         _brep_cmd_face_trim_direction_plot},
    { "I",           _brep_cmd_isosurface_plot},
    { "L",           _brep_cmd_loop_plot},
    { "L2d",         _brep_cmd_loop_2d_plot},
    { "S",           _brep_cmd_surface_plot},
    { "SCV",         _brep_cmd_surface_control_verts_plot},
    { "SK",          _brep_cmd_surface_knot_plot},
    { "SK2d",        _brep_cmd_surface_knot_2d_plot},
    { "SN",          _brep_cmd_surface_normal_plot},
    { "SUV",         _brep_cmd_surface_uv_plot},
    { "SUVP",        _brep_cmd_surface_uv_point_plot},
    { "T",           _brep_cmd_trim_plot},
    { "T2d",         _brep_cmd_trim_2d_plot},
    { "V",           _brep_cmd_vertex_plot},
    { "CDT",         _brep_cmd_face_cdt_plot},
    { "CDT2d",       _brep_cmd_face_cdt_2d_plot},
    { "CDTm2d",      _brep_cmd_face_cdt_m2d_plot},
    { "CDTp2d",      _brep_cmd_face_cdt_p2d_plot},
    { "CDTw",        _brep_cmd_face_cdt_wireframe_plot},
    { "CDTn",        _brep_cmd_face_cdt2_plot},
    { "CDTn2d",      _brep_cmd_face_cdt2_2d_plot},
    { "CDTnw",       _brep_cmd_face_cdt2_wireframe_plot},
    { (char *)NULL,      NULL}
};

int
brep_plot(struct _ged_brep_info *gb, int argc, const char **argv)
{
    struct _ged_brep_iplot gib;
    gib.gb = gb;
    gib.vls = gb->gedp->ged_result_str;
    gib.cmds = _brep_plot_cmds;

    const ON_Brep *brep = ((struct rt_brep_internal *)(gb->intern.idb_ptr))->brep;
    if (brep == NULL) {
	bu_vls_printf(gib.vls, "attempting to plot, but no ON_Brep data present\n");
	return GED_ERROR;
    }

    if (!argc) {
	_brep_plot_help(&gib, 0, NULL);
	return GED_OK;
    }

    if (argc > 1 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	argc--;argv++;
	argc--;argv++;
	_brep_plot_help(&gib, argc, argv);
	return GED_OK;
    }

    // Must have valid subcommand to process
    if (bu_cmd_valid(_brep_plot_cmds, argv[0]) != BRLCAD_OK) {
	bu_vls_printf(gib.vls, "invalid subcommand \"%s\" specified\n", argv[0]);
	_brep_plot_help(&gib, 0, NULL);
	return GED_ERROR;
    }

    int ret;
    if (bu_cmd(_brep_plot_cmds, argc, argv, 0, (void *)&gib, &ret) == BRLCAD_OK) {
	return ret;
    }
    return GED_ERROR;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

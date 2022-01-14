/*                  D E B U G _ P L O T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file debug_plot.cpp
 *
 * DebugPlot implementation. Currently borrows code from librt to
 * handle the creation of vlists for brep geometry and conversion of
 * those vlists to unix plot files.
 */

#include <iostream>
#include <sstream>
#include <string>
#include "bu/log.h"
#include "bn.h"
#include "raytrace.h"
#include "vmath.h"
#include "debug_plot.h"
#include "brep_except.h"

static unsigned char surface1_color[] = {0, 0, 62};
static unsigned char surface2_color[] = {62, 0, 0};
static unsigned char surface1_highlight_color[] = {56, 56, 255};
static unsigned char surface2_highlight_color[] = {255, 56, 56};

static unsigned char tangent_color[] = {255, 255, 255};
static unsigned char transverse_color[] = {255, 255, 0};
static unsigned char overlap_color[] = {0, 255, 0};

static unsigned char accepted_outerloop_color[] = {0, 255, 0};
static unsigned char accepted_innerloop_color[] = {255, 0, 0};
static unsigned char unknown_outerloop_color[] = {158, 158, 0};
static unsigned char unknown_innerloop_color[] = {158, 158, 0};
static unsigned char rejected_outerloop_color[] = {0, 62, 0};
static unsigned char rejected_innerloop_color[] = {62, 0, 0};

DebugPlot::DebugPlot(const char *basename) :
    prefix(basename),
    have_surfaces(false),
    brep1_surf_count(0),
    brep2_surf_count(0),
    linked_curve_count(0)
{
    BU_LIST_INIT(&vlist_free_list);
}

DebugPlot::~DebugPlot()
{
    struct bv_vlist *vp;
    while (BU_LIST_WHILE(vp, bv_vlist, &vlist_free_list)) {
	BU_LIST_DEQUEUE(&(vp->l));
	bu_free((char *)vp, "bv_vlist");
    }
}

int
DebugPlot::SurfacePairs(void)
{
    return (int)intersecting_surfaces.size();
}

int
DebugPlot::IntersectingIsocurves(int ssx_idx)
{
    int max_isocsx_idx = (int)ssx_isocsx_events.size() - 1;
    if (ssx_idx < 0 || ssx_idx > max_isocsx_idx) {
	std::cerr << "DebugPlot::IntersectingIsocurves passed invalid ssx index.\n";
	return 0;
    }
    return (int)ssx_isocsx_events[ssx_idx].size();
}

int
DebugPlot::LinkedCurves(void)
{
    return linked_curve_count;
}

HIDDEN void
rt_vlist_to_uplot(FILE *fp, const struct bu_list *vhead)
{
    struct bv_vlist *vp;

    for (BU_LIST_FOR(vp, bv_vlist, vhead)) {
	 int i;
	 int nused = vp->nused;
	 const int *cmd = vp->cmd;
	 point_t *pt = vp->pt;

	for (i = 0; i < nused; i++, cmd++, pt++) {
	    switch (*cmd) {
		case BV_VLIST_POLY_START:
		case BV_VLIST_TRI_START:
		    break;
		case BV_VLIST_POLY_MOVE:
		case BV_VLIST_LINE_MOVE:
		case BV_VLIST_TRI_MOVE:
		    pdv_3move(fp, *pt);
		    break;
		case BV_VLIST_POLY_DRAW:
		case BV_VLIST_POLY_END:
		case BV_VLIST_LINE_DRAW:
		case BV_VLIST_TRI_DRAW:
		case BV_VLIST_TRI_END:
		    pdv_3cont(fp, *pt);
		    break;
		default:
		    bu_log("rt_vlist_to_uplot: unknown vlist cmd x%x\n",
			   *cmd);
	    }
	}
    }
}

HIDDEN void
write_plot_to_file(
    const char *filename,
    const struct bu_list *vhead,
    const unsigned char *color)
{
    FILE *fp = fopen(filename, "w");
    const unsigned char *lcolor = color;
    unsigned char clr[] = {255, 0, 0};

    if (!lcolor) {
	lcolor = clr;
    }

    pl_linmod(fp, "solid");
    pl_color(fp, lcolor[0], lcolor[1], lcolor[2]);
    rt_vlist_to_uplot(fp, vhead);

    fclose(fp);
}

void
DebugPlot::WriteLog()
{
    std::ostringstream filename;

    // First write out empty plots of different colors, we use this to
    // "erase" unwanted overlays when running the dplot command.
    struct bu_list vhead;
    BU_LIST_INIT(&vhead);
    point_t origin = {0.0, 0.0, 0.0};
    BV_ADD_VLIST(&vlist_free_list, &vhead, origin, BV_VLIST_LINE_MOVE);

    filename << prefix << "_empty0.plot3";
    write_plot_to_file(filename.str().c_str(), &vhead, tangent_color);
    filename.str("");
    filename << prefix << "_empty1.plot3";
    write_plot_to_file(filename.str().c_str(), &vhead, transverse_color);
    filename.str("");
    filename << prefix << "_empty2.plot3";
    write_plot_to_file(filename.str().c_str(), &vhead, overlap_color);
    filename.str("");
    filename << prefix << "_empty3.plot3";
    write_plot_to_file(filename.str().c_str(), &vhead, surface1_color);
    filename.str("");
    filename << prefix << "_empty4.plot3";
    write_plot_to_file(filename.str().c_str(), &vhead, surface2_color);
    filename.str("");
    filename << prefix << "_empty5.plot3";
    write_plot_to_file(filename.str().c_str(), &vhead, surface1_highlight_color);
    filename.str("");
    filename << prefix << "_empty6.plot3";
    write_plot_to_file(filename.str().c_str(), &vhead, surface2_highlight_color);
    filename.str("");
    filename << prefix << "_empty7.plot3";
    write_plot_to_file(filename.str().c_str(), &vhead, accepted_outerloop_color);
    filename.str("");
    filename << prefix << "_empty8.plot3";
    write_plot_to_file(filename.str().c_str(), &vhead, accepted_innerloop_color);
    filename.str("");
    filename << prefix << "_empty9.plot3";
    write_plot_to_file(filename.str().c_str(), &vhead, unknown_outerloop_color);
    filename.str("");
    filename << prefix << "_empty10.plot3";
    write_plot_to_file(filename.str().c_str(), &vhead, unknown_innerloop_color);
    filename.str("");
    filename << prefix << "_empty11.plot3";
    write_plot_to_file(filename.str().c_str(), &vhead, rejected_outerloop_color);
    filename.str("");
    filename << prefix << "_empty12.plot3";
    write_plot_to_file(filename.str().c_str(), &vhead, rejected_innerloop_color);

    // create dplot log file
    filename.str("");
    filename << prefix << ".dplot";
    FILE *fp = fopen(filename.str().c_str(), "w");

    // write out surface-surface intersections
    fprintf(fp, "surfaces %d %d\n", brep1_surf_count, brep2_surf_count);
    for (size_t i = 0; i < intersecting_surfaces.size(); ++i) {
	std::pair<int, int> intersecting = intersecting_surfaces[i];

	int events, b1_isocurves, intersecting_isocurves, b1_clipped, b2_clipped;

	try {
	    events = ssx_events.at(i);
	} catch (std::out_of_range &e) {
	    events = 0;
	}
	try {
	    std::pair<int, int> ccount = ssx_clipped_curves.at(i);
	    b1_clipped = ccount.first;
	    b2_clipped = ccount.second;
	} catch (std::out_of_range &e) {
	    b1_clipped = b2_clipped = 0;
	}
	try {
	    b1_isocurves = ssx_isocsx_brep1_curves.at(i);
	} catch (std::out_of_range &e) {
	    b1_isocurves = 0;
	}
	try {
	    intersecting_isocurves = (int)ssx_isocsx_events.at(i).size();
	} catch (std::out_of_range &e) {
	    intersecting_isocurves = 0;
	}

	// b1si b2si finalevents b1ccurves b2ccurves b1_isocurve_xs total_isocurve_xs isocsx0_event0 ...
	fprintf(fp, "ssx %d %d %d %d %d %d %d", intersecting.first,
		intersecting.second, events, b1_clipped, b2_clipped,
		b1_isocurves, intersecting_isocurves);

	if (ssx_isocsx_events.size() > i) {
	    for (size_t j = 0; j < ssx_isocsx_events[i].size(); ++j) {
		fprintf(fp, " %d", ssx_isocsx_events[i][j]);
	    }
	}
	fprintf(fp, "\n");
    }

    // write out linked curve count
    if (linked_curve_count > 0) {
	fprintf(fp, "linkedcurves %d\n", linked_curve_count);
    }

    // write out split faces
    size_t split_faces = split_face_outerloop_curves.size();
    if (split_faces > 0) {
	fprintf(fp, "splitfaces %d\n", (int)split_faces);
    }
    for (size_t i = 0; i < split_faces; ++i) {
	fprintf(fp, "splitface %d %d %d\n", (int)i,
		split_face_outerloop_curves[i],
		split_face_innerloop_curves[i]);
    }
    fclose(fp);
    BV_FREE_VLIST(&vlist_free_list, &vhead);
}

HIDDEN double
find_next_t(const ON_Curve* crv, double start_t, double step, double max_dist)
{
    ON_Interval dom = crv->Domain();
    ON_3dPoint prev_pt = crv->PointAt(dom.ParameterAt(start_t));
    ON_3dPoint next_pt;

    // ensure that (start + step) < 1.0
    if (start_t + step > 1.0) {
	step = 1.0 - start_t - BN_TOL_DIST;
    }
    
    // reduce step until next point is within tolerance
    while (step > BN_TOL_DIST) {
	next_pt = crv->PointAt(dom.ParameterAt(start_t + step));

	if (prev_pt.DistanceTo(next_pt) <= max_dist) {
	    return start_t + step;
	}
	step /= 2.0;
    }
    // if we couldn't find a point within tolerance, give up and jump
    // to end of domain
    return 1.0;
}

void
DebugPlot::Plot3DCurve(
    const ON_Curve *crv,
    const char *filename,
    unsigned char *color,
    struct bu_list *vlist /* = NULL */)
{
    struct bu_list vhead_tmp;
    BU_LIST_INIT(&vhead_tmp);

    struct bu_list *vhead = &vhead_tmp;
    if (vlist) {
	vhead = vlist;
    }

    ON_Interval crv_dom = crv->Domain();

    // Insert first point.
    point_t pt1;
    ON_3dPoint p;
    p = crv->PointAt(crv_dom.ParameterAt(0.0));
    VMOVE(pt1, p);
    BV_ADD_VLIST(&vlist_free_list, vhead, pt1, BV_VLIST_LINE_MOVE);

    /* Dynamic sampling approach - start with an initial guess
     * for the next point of one tenth of the domain length
     * further down the domain from the previous value.  Set a
     * maximum physical distance between points of 100 times
     * the model tolerance.  Reduce the increment until the
     * tolerance is satisfied, then add the point and use it
     * as the starting point for the next calculation until
     * the whole domain is finished.  Perhaps it would be more
     * ideal to base the tolerance on some fraction of the
     * curve bounding box dimensions?
     */
    double t = 0.0;
    while (t < 1.0) {
	t = find_next_t(crv, t, 0.1, BN_TOL_DIST * 100);
	p = crv->PointAt(crv_dom.ParameterAt(t));
	VMOVE(pt1, p);

	BV_ADD_VLIST(&vlist_free_list, vhead, pt1, BV_VLIST_LINE_DRAW);
    }

    if (!vlist) {
	write_plot_to_file(filename, vhead, color);
	BV_FREE_VLIST(&vlist_free_list, vhead);
    }
}

void
DebugPlot::Plot3DCurveFrom2D(
    const ON_Surface *surf,
    const ON_Curve *crv,
    const char *filename,
    unsigned char *color,
    bool decorate /* = false */)
{
    struct bu_list vhead;
    BU_LIST_INIT(&vhead);

    ON_Interval crv_dom = crv->Domain();

    ON_3dPoint p, uv;

    // Insert first point.
    point_t pt1, first_pt, last_pt, prev_pt;
    ON_3dVector normal;
    uv = crv->PointAt(crv_dom.ParameterAt(0.0));
    surf->EvNormal(uv.x, uv.y, p, normal);
    VMOVE(first_pt, p);

    uv = crv->PointAt(crv_dom.ParameterAt(1.0));
    surf->EvNormal(uv.x, uv.y, p, normal);
    VMOVE(last_pt, p);

    bool closed = false;
    if (VNEAR_EQUAL(first_pt, last_pt, BN_TOL_DIST)) {
	closed = true;
    }

    VMOVE(pt1, first_pt);
    BV_ADD_VLIST(&vlist_free_list, &vhead, pt1, BV_VLIST_LINE_MOVE);

    /* Dynamic sampling approach - start with an initial guess
     * for the next point of one tenth of the domain length
     * further down the domain from the previous value.  Set a
     * maximum physical distance between points of 100 times
     * the model tolerance.  Reduce the increment until the
     * tolerance is satisfied, then add the point and use it
     * as the starting point for the next calculation until
     * the whole domain is finished.  Perhaps it would be more
     * ideal to base the tolerance on some fraction of the
     * curve bounding box dimensions?
     */
    double t = 0.0;
    bool first = true;
    double min_mag = BN_TOL_DIST * 10.0;
    double mag_tan = min_mag;
    vect_t tangent = {0.0, 0.0, min_mag};
    vect_t perp, barb;
    while (t < 1.0) {
	t = find_next_t(crv, t, 0.1, BN_TOL_DIST * 100);

	uv = crv->PointAt(crv_dom.ParameterAt(t));
	surf->EvNormal(uv.x, uv.y, p, normal);
	VMOVE(prev_pt, pt1);
	VMOVE(pt1, p);

	if (first || !VNEAR_EQUAL(pt1, prev_pt, BN_TOL_DIST)) {
	    VSUB2(tangent, pt1, prev_pt);
	}

	if (decorate && first) {
	    first = false;

	    mag_tan = DIST_PNT_PNT(prev_pt, pt1);
	    mag_tan = FMAX(mag_tan, min_mag);

	    VUNITIZE(tangent);
	    VCROSS(perp, tangent, normal);

	    if (!closed) {
		VSCALE(tangent, tangent, mag_tan);
		VUNITIZE(perp);
		VSCALE(perp, perp, mag_tan);

		VADD3(barb, prev_pt, tangent, perp);
		BV_ADD_VLIST(&vlist_free_list, &vhead, barb, BV_VLIST_LINE_DRAW);
		BV_ADD_VLIST(&vlist_free_list, &vhead, prev_pt, BV_VLIST_LINE_MOVE);

		VSCALE(perp, perp, -1.0);
		VADD3(barb, prev_pt, tangent, perp);
		BV_ADD_VLIST(&vlist_free_list, &vhead, barb, BV_VLIST_LINE_DRAW);
		BV_ADD_VLIST(&vlist_free_list, &vhead, prev_pt, BV_VLIST_LINE_MOVE);
	    }
	}
	BV_ADD_VLIST(&vlist_free_list, &vhead, pt1, BV_VLIST_LINE_DRAW);
    }
    if (decorate) {
	VUNITIZE(tangent);
	VSCALE(tangent, tangent, -mag_tan);

	VCROSS(perp, tangent, normal);
	VUNITIZE(perp);
	VSCALE(perp, perp, mag_tan);

	VADD2(barb, pt1, perp);
	if (!closed) {
	    VADD2(barb, barb, tangent);
	}
	BV_ADD_VLIST(&vlist_free_list, &vhead, barb, BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(&vlist_free_list, &vhead, pt1, BV_VLIST_LINE_MOVE);

	VSCALE(perp, perp, -1.0);
	VADD2(barb, pt1, perp);
	if (!closed) {
	    VADD2(barb, barb, tangent);
	}
	BV_ADD_VLIST(&vlist_free_list, &vhead, barb, BV_VLIST_LINE_DRAW);
    }

    write_plot_to_file(filename, &vhead, color);
    BV_FREE_VLIST(&vlist_free_list, &vhead);
}

void
DebugPlot::PlotBoundaryIsocurves(
    struct bu_list *vlist,
    const ON_Surface &surf,
    int knot_dir)
{
    int surf_dir = 1 - knot_dir;
    int knot_count = surf.SpanCount(surf_dir) + 1;

    double *surf_knots = new double[knot_count];
    surf.GetSpanVector(surf_dir, surf_knots);

    // knots that can be boundaries of Bezier patches
    ON_SimpleArray<double> surf_bknots;
    surf_bknots.Append(surf_knots[0]);
    for (int i = 1; i < knot_count; i++) {
	if (surf_knots[i] > *(surf_bknots.Last())) {
	    surf_bknots.Append(surf_knots[i]);
	}
    }
    delete[] surf_knots;

    if (surf.IsClosed(surf_dir)) {
	surf_bknots.Remove();
    }

    for (int i = 0; i < surf_bknots.Count(); i++) {
	ON_Curve *surf_boundary_iso = surf.IsoCurve(knot_dir, surf_bknots[i]);
	Plot3DCurve(surf_boundary_iso, NULL, NULL, vlist);
	delete surf_boundary_iso;
    }
}

void
DebugPlot::PlotSurface(
    const ON_Surface &surf,
    const char *filename,
    unsigned char *color)
{
    struct bu_list vhead;
    BU_LIST_INIT(&vhead);

    PlotBoundaryIsocurves(&vhead, surf, 0);
    PlotBoundaryIsocurves(&vhead, surf, 1);

    write_plot_to_file(filename, &vhead, color);

    BV_FREE_VLIST(&vlist_free_list, &vhead);
}


void
DebugPlot::Surfaces(const ON_Brep *brep1, const ON_Brep *brep2)
{
    if (!brep1 || !brep2) {
	std::cerr << "error: dplot_surfaces: NULL args\n";
	return;
    }

    brep1_surf_count = brep1->m_S.Count();
    for (int i = 0; i < brep1->m_S.Count(); i++) {
	ON_Surface *surf = brep1->m_S[i];
	std::ostringstream filename;
	filename << prefix << "_brep1_surface" << i << ".plot3";
	PlotSurface(*surf, filename.str().c_str(), surface1_color);
    }

    brep2_surf_count = brep2->m_S.Count();
    for (int i = 0; i < brep2->m_S.Count(); i++) {
	ON_Surface *surf = brep2->m_S[i];
	std::ostringstream filename;
	filename << prefix << "_brep2_surface" << i << ".plot3";
	PlotSurface(*surf, filename.str().c_str(), surface2_color);
    }
    have_surfaces = true;
}

int get_subcurve_inside_faces(const ON_Brep *brep1, const ON_Brep *brep2, int face_i1, int face_i2, ON_SSX_EVENT *event);

void
DebugPlot::SSX(
    const ON_ClassArray<ON_SSX_EVENT> &events,
    const ON_Brep *brep1, int brep1_surf,
    const ON_Brep *brep2, int brep2_surf)
{
    ON_Surface *surf;
    std::ostringstream filename;

    // create highlighted plot of brep1 surface if it doesn't exist
    filename << prefix << "_highlight_brep1_surface" << brep1_surf << ".plot3";
    if (!bu_file_exists(filename.str().c_str(), NULL)) {
	surf = brep1->m_S[brep1_surf];
	PlotSurface(*surf, filename.str().c_str(), surface1_highlight_color);
    }

    // create highlighted plot of brep2 surface if it doesn't exist
    filename.str("");
    filename << prefix << "_highlight_brep2_surface" << brep2_surf << ".plot3";
    if (!bu_file_exists(filename.str().c_str(), NULL)) {
	surf = brep2->m_S[brep2_surf];
	PlotSurface(*surf, filename.str().c_str(), surface2_highlight_color);
    }

    // create plot of the intersections between these surfaces
    surf = brep1->m_S[brep1_surf];
    size_t ssx_idx = intersecting_surfaces.size();
    int plot_count = 0;
    for (int i = 0; i < events.Count(); ++i) {
	filename.str("");
	filename << prefix << "_ssx" << ssx_idx << "_event" << plot_count <<
	    ".plot3";

	if (events[i].m_type == ON_SSX_EVENT::ssx_tangent) {
	    Plot3DCurveFrom2D(surf, events[i].m_curveA,
		    filename.str().c_str(), tangent_color, true);
	    ++plot_count;
	} else if (events[i].m_type == ON_SSX_EVENT::ssx_transverse) {
	    Plot3DCurveFrom2D(surf, events[i].m_curveA,
		    filename.str().c_str(), transverse_color, true);
	    ++plot_count;
	} else if (events[i].m_type == ON_SSX_EVENT::ssx_overlap) {
	    Plot3DCurveFrom2D(surf, events[i].m_curveA,
		    filename.str().c_str(), overlap_color, true);
	    ++plot_count;
	}
    }
    // stash surface indices and event count
    std::pair<int, int> ssx_pair(brep1_surf, brep2_surf);
    intersecting_surfaces.push_back(ssx_pair);
    ssx_events.push_back(plot_count);
}

void
DebugPlot::IsoCSX(
    const ON_SimpleArray<ON_X_EVENT> &events,
    const ON_Curve *isocurve,
    bool is_brep1_iso) // is the isocurve from brep1?
{
    size_t ssx_idx = intersecting_surfaces.size();

    // create plot of the intersections between the curve and surface
    while (ssx_isocsx_events.size() < (ssx_idx + 1)) {
	ssx_isocsx_events.push_back(std::vector<int>());
    }
    size_t isocsx_idx = ssx_isocsx_events[ssx_idx].size();
    int plot_count = 0;
    for (int i = 0; i < events.Count(); ++i) {
	if (events[i].m_type == ON_X_EVENT::csx_overlap) {
	    std::ostringstream filename;
	    filename << prefix << "_ssx" << ssx_idx << "_isocsx" << isocsx_idx
		<< "_event" << plot_count++ << ".plot3";

	    try {
		ON_Curve *event_curve = sub_curve(isocurve, events[i].m_a[0],
			events[i].m_a[1]);
		Plot3DCurve(event_curve, filename.str().c_str(), overlap_color);
	    } catch (InvalidInterval &e) {
		std::cerr << "error: IsoCSX event contains degenerate interval\n";
	    }
	}
    }
    if (plot_count) {
	// create highlighted plot of isocurve if it doesn't already exist
	std::ostringstream filename;
	filename << prefix << "_highlight_ssx" << ssx_idx << "_isocurve" <<
	    isocsx_idx << ".plot3";
	if (!bu_file_exists(filename.str().c_str(), NULL)) {
	    if (is_brep1_iso) {
		Plot3DCurve(isocurve, filename.str().c_str(),
			surface1_highlight_color);
	    } else {
		Plot3DCurve(isocurve, filename.str().c_str(),
			surface2_highlight_color);
	    }
	}

	// remember event count for this isocsx
	ssx_isocsx_events[ssx_idx].push_back(plot_count);

	// remember how many events are for brep1 isocurve and brep2 surface,
	if (is_brep1_iso) {
	    while (ssx_isocsx_brep1_curves.size() < (ssx_idx + 1)) {
		ssx_isocsx_brep1_curves.push_back(0);
	    }
	    ++ssx_isocsx_brep1_curves[ssx_idx];
	}
    }
}

void
DebugPlot::ClippedFaceCurves(
    const ON_Surface *surf1,
    const ON_Surface *surf2,
    const ON_SimpleArray<ON_Curve *> &face1_curves,
    const ON_SimpleArray<ON_Curve *> &face2_curves)
{
    // plot clipped tangent/transverse/overlap curves
    size_t ssx_idx = intersecting_surfaces.size() - 1;
    for (int i = 0; i < face1_curves.Count(); ++i) {
	std::ostringstream filename;
	filename << prefix << "_ssx" << ssx_idx << "_brep1face_clipped_curve" << i << ".plot3";
	Plot3DCurveFrom2D(surf1, face1_curves[i], filename.str().c_str(),
		surface1_highlight_color, true);
    }
    for (int i = 0; i < face2_curves.Count(); ++i) {
	std::ostringstream filename;
	filename << prefix << "_ssx" << ssx_idx << "_brep2face_clipped_curve" << i << ".plot3";
	Plot3DCurveFrom2D(surf2, face2_curves[i], filename.str().c_str(),
		surface2_highlight_color, true);
    }

    while (ssx_clipped_curves.size() < (ssx_idx + 1)) {
	ssx_clipped_curves.push_back(std::pair<int, int>(0, 0));
    }
    std::pair<int, int> counts(face1_curves.Count(), face2_curves.Count());
    ssx_clipped_curves[ssx_idx] = counts;
}

struct TrimmedFace {
    // curve segments in the face's outer loop
    ON_SimpleArray<ON_Curve *> m_outerloop;
    // several inner loops, each has some curves
    std::vector<ON_SimpleArray<ON_Curve *> > m_innerloop;
    const ON_BrepFace *m_face;
    enum {
	UNKNOWN = -1,
	NOT_BELONG = 0,
	BELONG = 1
    } m_belong_to_final;
    bool m_rev;

    // Default constructor
    TrimmedFace()
    {
	m_face = NULL;
	m_belong_to_final = UNKNOWN;
	m_rev = false;
    }

    // Destructor
    ~TrimmedFace()
    {
	// Delete the curve segments if it's not belong to the result.
	if (m_belong_to_final != BELONG) {
	    for (int i = 0; i < m_outerloop.Count(); i++) {
		if (m_outerloop[i]) {
		    delete m_outerloop[i];
		    m_outerloop[i] = NULL;
		}
	    }
	    for (unsigned int i = 0; i < m_innerloop.size(); i++) {
		for (int j = 0; j < m_innerloop[i].Count(); j++) {
		    if (m_innerloop[i][j]) {
			delete m_innerloop[i][j];
			m_innerloop[i][j] = NULL;
		    }
		}
	    }
	}
    }

    TrimmedFace *Duplicate() const
    {
	TrimmedFace *out = new TrimmedFace();
	out->m_face = m_face;
	for (int i = 0; i < m_outerloop.Count(); i++) {
	    if (m_outerloop[i]) {
		out->m_outerloop.Append(m_outerloop[i]->Duplicate());
	    }
	}
	out->m_innerloop = m_innerloop;
	for (unsigned int i = 0; i < m_innerloop.size(); i++) {
	    for (int j = 0; j < m_innerloop[i].Count(); j++) {
		if (m_innerloop[i][j]) {
		    out->m_innerloop[i][j] = m_innerloop[i][j]->Duplicate();
		}
	    }
	}
	return out;
    }
};

void
DebugPlot::SplitFaces(
    const ON_ClassArray<ON_SimpleArray<TrimmedFace *> > &split_faces)
{
    for (int i = 0; i < split_faces.Count(); ++i) {
	for (int j = 0; j < split_faces[i].Count(); ++j) {
	    TrimmedFace *face = split_faces[i][j];

	    unsigned char *outerloop_color = unknown_outerloop_color;
	    unsigned char *innerloop_color = unknown_innerloop_color;
	    switch (face->m_belong_to_final) {
		case TrimmedFace::NOT_BELONG:
		    outerloop_color = rejected_outerloop_color;
		    innerloop_color = rejected_innerloop_color;
		    break;
		case TrimmedFace::BELONG:
		    outerloop_color = accepted_outerloop_color;
		    innerloop_color = accepted_innerloop_color;
		    break;
		default:
		    outerloop_color = unknown_outerloop_color;
		    innerloop_color = unknown_innerloop_color;
	    }

	    int split_face_count = split_face_outerloop_curves.size();
	    for (int k = 0; k < face->m_outerloop.Count(); ++k) {
		std::ostringstream filename;
		filename << prefix << "_split_face" << split_face_count <<
		    "_outerloop_curve" << k << ".plot3";

		Plot3DCurveFrom2D(face->m_face->SurfaceOf(),
			face->m_outerloop[k], filename.str().c_str(),
			outerloop_color);
	    }
	    split_face_outerloop_curves.push_back(face->m_outerloop.Count());

	    int innerloop_count = 0;
	    for (size_t k = 0; k < face->m_innerloop.size(); ++k) {
		for (int l = 0; l < face->m_innerloop[k].Count(); ++l) {
		    std::ostringstream filename;
		    filename << prefix << "_split_face" << split_face_count <<
			"_innerloop_curve" << innerloop_count++ << ".plot3";

		    Plot3DCurveFrom2D(face->m_face->SurfaceOf(),
			    face->m_innerloop[k][l], filename.str().c_str(),
			    innerloop_color);
		}
	    }
	    split_face_innerloop_curves.push_back(innerloop_count);
	}
    }
}

void append_to_polycurve(ON_Curve *curve, ON_PolyCurve &polycurve);

struct SSICurve {
    ON_Curve *m_curve;

    SSICurve()
    {
	m_curve = NULL;
    }

    SSICurve(ON_Curve *curve)
    {
	m_curve = curve;
    }

    SSICurve *Duplicate() const
    {
	SSICurve *out = new SSICurve();
	if (out != NULL) {
	    *out = *this;
	    out->m_curve = m_curve->Duplicate();
	}
	return out;
    }
};

struct LinkedCurve {
private:
    ON_Curve *m_curve;	// an explicit storage of the whole curve
public:
    // The curves contained in this LinkedCurve, including
    // the information needed by the connectivity graph
    ON_SimpleArray<SSICurve> m_ssi_curves;

    // Default constructor
    LinkedCurve()
    {
	m_curve = NULL;
    }

    ~LinkedCurve()
    {
	if (m_curve) {
	    delete m_curve;
	}
	m_curve = NULL;
    }

    LinkedCurve &operator= (const LinkedCurve &_lc)
    {
	m_curve = _lc.m_curve ? _lc.m_curve->Duplicate() : NULL;
	m_ssi_curves = _lc.m_ssi_curves;
	return *this;
    }

    ON_3dPoint PointAtStart() const
    {
	if (m_ssi_curves.Count()) {
	    return m_ssi_curves[0].m_curve->PointAtStart();
	} else {
	    return ON_3dPoint::UnsetPoint;
	}
    }

    ON_3dPoint PointAtEnd() const
    {
	if (m_ssi_curves.Count()) {
	    return m_ssi_curves.Last()->m_curve->PointAtEnd();
	} else {
	    return ON_3dPoint::UnsetPoint;
	}
    }

    bool IsClosed() const
    {
	if (m_ssi_curves.Count() == 0) {
	    return false;
	}
	return PointAtStart().DistanceTo(PointAtEnd()) < ON_ZERO_TOLERANCE;
    }

    bool IsValid() const
    {
	// Check whether the curve has "gaps".
	for (int i = 1; i < m_ssi_curves.Count(); i++) {
	    if (m_ssi_curves[i].m_curve->PointAtStart().DistanceTo(m_ssi_curves[i - 1].m_curve->PointAtEnd()) >= ON_ZERO_TOLERANCE) {
		bu_log("The LinkedCurve is not valid.\n");
		return false;
	    }
	}
	return true;
    }

    bool Reverse()
    {
	ON_SimpleArray<SSICurve> new_array;
	for (int i = m_ssi_curves.Count() - 1; i >= 0; i--) {
	    if (!m_ssi_curves[i].m_curve->Reverse()) {
		return false;
	    }
	    new_array.Append(m_ssi_curves[i]);
	}
	m_ssi_curves = new_array;
	return true;
    }

    void Append(const LinkedCurve &lc)
    {
	m_ssi_curves.Append(lc.m_ssi_curves.Count(), lc.m_ssi_curves.Array());
    }

    void Append(const SSICurve &sc)
    {
	m_ssi_curves.Append(sc);
    }

    void AppendCurvesToArray(ON_SimpleArray<ON_Curve *> &arr) const
    {
	for (int i = 0; i < m_ssi_curves.Count(); i++) {
	    arr.Append(m_ssi_curves[i].m_curve->Duplicate());
	}
    }

    const ON_Curve *Curve()
    {
	if (m_curve != NULL) {
	    return m_curve;
	}
	if (m_ssi_curves.Count() == 0 || !IsValid()) {
	    return NULL;
	}
	ON_PolyCurve *polycurve = new ON_PolyCurve;
	for (int i = 0; i < m_ssi_curves.Count(); i++) {
	    append_to_polycurve(m_ssi_curves[i].m_curve->Duplicate(), *polycurve);
	}
	m_curve = polycurve;
	return m_curve;
    }

    const ON_3dPoint PointAt(double t)
    {
	const ON_Curve *c = Curve();
	if (c == NULL) {
	    return ON_3dPoint::UnsetPoint;
	}
	return c->PointAt(t);
    }

    const ON_Interval Domain()
    {
	const ON_Curve *c = Curve();
	if (c == NULL) {
	    return ON_Interval::EmptyInterval;
	}
	return c->Domain();
    }

    ON_Curve *SubCurve(double t1, double t2)
    {
	const ON_Curve *c = Curve();
	if (c == NULL) {
	    return NULL;
	}
	try {
	    return sub_curve(c, t1, t2);
	} catch (InvalidInterval &e) {
	    bu_log("%s", e.what());
	    return NULL;
	}
    }
};

void
DebugPlot::LinkedCurves(
    const ON_Surface *surf,
    ON_ClassArray<LinkedCurve> &linked_curves)
{
    for (int i = 0; i < linked_curves.Count(); ++i) {
	const ON_Curve *linked_curve = linked_curves[i].Curve();

	std::ostringstream filename;
	filename << prefix << "_linked_curve" << linked_curve_count++ << ".plot3";

	Plot3DCurveFrom2D(surf, linked_curve, filename.str().c_str(), transverse_color);
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

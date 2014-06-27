/*                  D E B U G _ P L O T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
#include "bu.h"
#include "bn.h"
#include "plot3.h"
#include "raytrace.h"
#include "vmath.h"
#include "debug_plot.h"

#define DEFAULT_PLOT_RES 100
#define DEFAULT_GRID_RES 10

static unsigned char surface_color[] = {0, 0, 62};
static unsigned char surface_highlight_color[] = {56, 56, 255};

static unsigned char curve_highlight_color[] = {255, 255, 255};

static unsigned char tangent_color[] = {255, 0, 0};
static unsigned char transverse_color[] = {255, 255, 0};
static unsigned char overlap_color[] = {0, 255, 0};

DebugPlot::DebugPlot(const char *basename) :
    prefix(basename),
    have_surfaces(false),
    brep1_surf_count(0),
    brep2_surf_count(0)
{
    BU_LIST_INIT(&vlist_free_list);
}

HIDDEN void
write_plot_to_file(
    const char *filename,
    const struct bu_list *vhead,
    const unsigned char *color)
{
    FILE *fp = fopen(filename, "w");

    pl_linmod(fp, "solid");
    pl_color(fp, color[0], color[1], color[2]);
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
    BN_ADD_VLIST(&vlist_free_list, &vhead, origin, BN_VLIST_LINE_MOVE);

    filename << prefix << "_empty0.plot3";
    write_plot_to_file(filename.str().c_str(), &vhead, tangent_color);
    filename.str("");
    filename << prefix << "_empty1.plot3";
    write_plot_to_file(filename.str().c_str(), &vhead, transverse_color);
    filename.str("");
    filename << prefix << "_empty2.plot3";
    write_plot_to_file(filename.str().c_str(), &vhead, overlap_color);

    // create dplot log file
    filename.str("");
    filename << prefix << ".dplot";
    FILE *fp = fopen(filename.str().c_str(), "w");

    // write out surface counts
    fprintf(fp, "surfaces %d %d\n", brep1_surf_count, brep2_surf_count);

    // write out surface-surface intersections
    for (size_t i = 0; i < intersecting_surfaces.size(); ++i) {
	std::pair<int, int> pair = intersecting_surfaces[i];
	// b1si b2si finalevents b1_isocurve_xs total_isocurve_xs isocsx0_event0 ...
	fprintf(fp, "ssx %d %d %d %d %d", pair.first, pair.second,
		ssx_events[i], (int)ssx_isocsx_brep1_curves[i],
		(int)ssx_isocsx_events[i].size());
	for (size_t j = 0; j < ssx_isocsx_events[i].size(); ++j) {
	    fprintf(fp, " %d", ssx_isocsx_events[i][j]);
	}
	fprintf(fp, "\n");
    }
    fclose(fp);
}

HIDDEN void
rt_vlist_to_uplot(FILE *fp, const struct bu_list *vhead)
{
    register struct bn_vlist *vp;

    for (BU_LIST_FOR(vp, bn_vlist, vhead)) {
	register int i;
	register int nused = vp->nused;
	register const int *cmd = vp->cmd;
	register point_t *pt = vp->pt;

	for (i = 0; i < nused; i++, cmd++, pt++) {
	    switch (*cmd) {
		case BN_VLIST_POLY_START:
		case BN_VLIST_TRI_START:
		    break;
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_LINE_MOVE:
		case BN_VLIST_TRI_MOVE:
		    pdv_3move(fp, *pt);
		    break;
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_POLY_END:
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_TRI_DRAW:
		case BN_VLIST_TRI_END:
		    pdv_3cont(fp, *pt);
		    break;
		default:
		    bu_log("rt_vlist_to_uplot: unknown vlist cmd x%x\n",
			   *cmd);
	    }
	}
    }
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
    unsigned char *color)
{
    struct bu_list vhead;
    BU_LIST_INIT(&vhead);

    ON_Interval crv_dom = crv->Domain();

    // Insert first point.
    point_t pt1;
    ON_3dPoint p;
    p = crv->PointAt(crv_dom.ParameterAt(0.0));
    VMOVE(pt1, p);
    BN_ADD_VLIST(&vlist_free_list, &vhead, pt1, BN_VLIST_LINE_MOVE);

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

	BN_ADD_VLIST(&vlist_free_list, &vhead, pt1, BN_VLIST_LINE_DRAW);
    }

    write_plot_to_file(filename, &vhead, color);
}

void
DebugPlot::Plot3DCurveFrom2D(
    const ON_Surface *surf,
    const ON_Curve *crv,
    const char *filename,
    unsigned char *color)
{
    struct bu_list vhead;
    BU_LIST_INIT(&vhead);

    ON_Interval crv_dom = crv->Domain();
    ON_Interval surf_udom = surf->Domain(0);
    ON_Interval surf_vdom = surf->Domain(1);

    ON_3dPoint p, uv, tmp_uv;
#define UV_AT_T(_t) \
    (tmp_uv = crv->PointAt(crv_dom.ParameterAt((_t))), \
    ON_3dPoint(surf_udom.ParameterAt(tmp_uv.x), surf_vdom.ParameterAt(tmp_uv.y), 0.0))

    // Insert first point.
    point_t pt1, first_pt, last_pt, prev_pt;
    ON_3dVector normal;
    uv = UV_AT_T(0.0);
    surf->EvNormal(uv.x, uv.y, p, normal);
    VMOVE(first_pt, p);

    uv = UV_AT_T(1.0);
    surf->EvNormal(uv.x, uv.y, p, normal);
    VMOVE(last_pt, p);

    bool closed = false;
    if (VNEAR_EQUAL(first_pt, last_pt, BN_TOL_DIST)) {
	closed = true;
    }

    VMOVE(pt1, first_pt);
    BN_ADD_VLIST(&vlist_free_list, &vhead, pt1, BN_VLIST_LINE_MOVE);

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

	uv = UV_AT_T(t);
	surf->EvNormal(uv.x, uv.y, p, normal);
	VMOVE(prev_pt, pt1);
	VMOVE(pt1, p);

	if (first || !VNEAR_EQUAL(pt1, prev_pt, BN_TOL_DIST)) {
	    VSUB2(tangent, pt1, prev_pt);
	}

	if (first) {
	    first = false;

	    mag_tan = DIST_PT_PT(prev_pt, pt1);
	    mag_tan = FMAX(mag_tan, min_mag);

	    VUNITIZE(tangent);
	    VCROSS(perp, tangent, normal);

	    if (!closed) {
		VSCALE(tangent, tangent, mag_tan);
		VUNITIZE(perp);
		VSCALE(perp, perp, mag_tan);

		VADD3(barb, prev_pt, tangent, perp);
		BN_ADD_VLIST(&vlist_free_list, &vhead, barb, BN_VLIST_LINE_DRAW);
		BN_ADD_VLIST(&vlist_free_list, &vhead, prev_pt, BN_VLIST_LINE_MOVE);

		VSCALE(perp, perp, -1.0);
		VADD3(barb, prev_pt, tangent, perp);
		BN_ADD_VLIST(&vlist_free_list, &vhead, barb, BN_VLIST_LINE_DRAW);
		BN_ADD_VLIST(&vlist_free_list, &vhead, prev_pt, BN_VLIST_LINE_MOVE);
	    }
	}
	BN_ADD_VLIST(&vlist_free_list, &vhead, pt1, BN_VLIST_LINE_DRAW);
    }
    VUNITIZE(tangent);
    VSCALE(tangent, tangent, -mag_tan);

    VCROSS(perp, tangent, normal);
    VUNITIZE(perp);
    VSCALE(perp, perp, mag_tan);

    VADD2(barb, pt1, perp);
    if (!closed) {
	VADD2(barb, barb, tangent);
    }
    BN_ADD_VLIST(&vlist_free_list, &vhead, barb, BN_VLIST_LINE_DRAW);
    BN_ADD_VLIST(&vlist_free_list, &vhead, pt1, BN_VLIST_LINE_MOVE);

    VSCALE(perp, perp, -1.0);
    VADD2(barb, pt1, perp);
    if (!closed) {
	VADD2(barb, barb, tangent);
    }
    BN_ADD_VLIST(&vlist_free_list, &vhead, barb, BN_VLIST_LINE_DRAW);

    write_plot_to_file(filename, &vhead, color);
}

#if 0

void
plot_point(ON_3dPoint pt, double diameter, const char *prefix, unsigned char *color)
{
    ON_3dPoint start, end;
    double offset = diameter / 2.0;

    start = end = pt;
    start.x -= offset;
    end.x += offset;
    ON_LineCurve xline(start, end);

    start = end = pt;
    start.y -= offset;
    end.y += offset;
    ON_LineCurve yline(start, end);

    start = end = pt;
    start.z -= offset;
    end.z += offset;
    ON_LineCurve zline(start, end);

    plot_curve_to_file(&xline, prefix, color);
    plot_curve_to_file(&yline, prefix, color);
    plot_curve_to_file(&zline, prefix, color);
}
#endif

void
DebugPlot::PlotSurface(
    const ON_Surface &surf,
    int isocurveres,
    int gridres,
    const char *filename,
    unsigned char *color)
{
    struct bu_list vhead;
    BU_LIST_INIT(&vhead);

    ON_Interval udom = surf.Domain(0);
    ON_Interval vdom = surf.Domain(1);

    fastf_t pt1[3], pt2[3];
    for (int u = 0; u <= gridres; u++) {
	for (int v = 1; v <= isocurveres; v++) {
	    ON_3dPoint p = surf.PointAt(
		    udom.ParameterAt((double)u/(double)gridres),
		    vdom.ParameterAt((double)(v-1)/(double)isocurveres));
	    VMOVE(pt1, p);
	    p = surf.PointAt(
		    udom.ParameterAt((double)u/(double)gridres),
		    vdom.ParameterAt((double)v/(double)isocurveres));
	    VMOVE(pt2, p);
	    BN_ADD_VLIST(&vlist_free_list, &vhead, pt1, BN_VLIST_LINE_MOVE);
	    BN_ADD_VLIST(&vlist_free_list, &vhead, pt2, BN_VLIST_LINE_DRAW);
	}
    }

    for (int v = 0; v <= gridres; v++) {
	for (int u = 1; u <= isocurveres; u++) {
	    ON_3dPoint p = surf.PointAt(
		    udom.ParameterAt((double)(u-1)/(double)isocurveres),
		    vdom.ParameterAt((double)v/(double)gridres));
	    VMOVE(pt1, p);
	    p = surf.PointAt(
		    udom.ParameterAt((double)u/(double)isocurveres),
		    vdom.ParameterAt((double)v/(double)gridres));
	    VMOVE(pt2, p);
	    BN_ADD_VLIST(&vlist_free_list, &vhead, pt1, BN_VLIST_LINE_MOVE);
	    BN_ADD_VLIST(&vlist_free_list, &vhead, pt2, BN_VLIST_LINE_DRAW);
	}
    }
    write_plot_to_file(filename, &vhead, color);
    return;
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
	PlotSurface(*surf, DEFAULT_PLOT_RES, DEFAULT_GRID_RES,
		filename.str().c_str(), surface_color);
    }

    brep2_surf_count = brep2->m_S.Count();
    for (int i = 0; i < brep2->m_S.Count(); i++) {
	ON_Surface *surf = brep2->m_S[i];
	std::ostringstream filename;
	filename << prefix << "_brep2_surface" << i << ".plot3";
	PlotSurface(*surf, DEFAULT_PLOT_RES, DEFAULT_GRID_RES,
		filename.str().c_str(), surface_color);
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
	PlotSurface(*surf, DEFAULT_PLOT_RES, DEFAULT_GRID_RES,
		filename.str().c_str(), surface_highlight_color);
    }

    // create highlighted plot of brep2 surface if it doesn't exist
    filename.str("");
    filename << prefix << "_highlight_brep2_surface" << brep2_surf << ".plot3";
    if (!bu_file_exists(filename.str().c_str(), NULL)) {
	surf = brep2->m_S[brep2_surf];
	PlotSurface(*surf, DEFAULT_PLOT_RES, DEFAULT_GRID_RES,
		filename.str().c_str(), surface_highlight_color);
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
		    filename.str().c_str(), tangent_color);
	    ++plot_count;
	} else if (events[i].m_type == ON_SSX_EVENT::ssx_transverse) {
	    Plot3DCurveFrom2D(surf, events[i].m_curveA,
		    filename.str().c_str(), transverse_color);
	    ++plot_count;
	} else if (events[i].m_type == ON_SSX_EVENT::ssx_overlap) {
	    Plot3DCurveFrom2D(surf, events[i].m_curveA,
		    filename.str().c_str(), overlap_color);
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
    const ON_Surface *surf,
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

	    ON_Curve *event_curve = sub_curve(isocurve, events[i].m_a[0],
		    events[i].m_a[1]);

	    Plot3DCurve(event_curve, filename.str().c_str(), overlap_color);
	}
    }
    if (plot_count) {
	// create highlighted plot of isocurve if it doesn't already exist
	std::ostringstream filename;
	filename << prefix << "_highlight_ssx" << ssx_idx << "_isocurve" <<
	    isocsx_idx << ".plot3";
	if (!bu_file_exists(filename.str().c_str(), NULL)) {
	    Plot3DCurve(isocurve, filename.str().c_str(), curve_highlight_color);
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

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

/*                    D E B U G _ P L O T . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file debug_plot.h
 *
 * Class used to write out unix plot files representing brep geometry
 * and intersection events from various stages of brep boolean
 * evaluation.
 *
 * Also outputs a parsable log file with implicit information about
 * the output plot files.
 */

#ifndef DEBUG_PLOT_H
#define DEBUG_PLOT_H
#include "bu/list.h"
#include "raytrace.h"

struct TrimmedFace;
struct LinkedCurve;

class DebugPlot {
public:
    // prefix all dplot output files with 'basename'
    DebugPlot(const char *basename);

    ~DebugPlot();

    // write plot files for all surfaces of two breps
    void Surfaces(const ON_Brep *brep1, const ON_Brep *brep2);

    // record a surface surface intersection
    void SSX(
	const ON_ClassArray<ON_SSX_EVENT> &events,
	const ON_Brep *brep1, int brep1_surf,
	const ON_Brep *brep2, int brep2_surf);

    // record surface surface isocurve intersections
    void IsoCSX(
	const ON_SimpleArray<ON_X_EVENT> &events,
	const ON_Curve *isocurve,
	bool is_brep1_iso);

    // record clipped surface-surface intersection curves
    void ClippedFaceCurves(
	const ON_Surface *surf1,
	const ON_Surface *surf2,
	const ON_SimpleArray<ON_Curve *> &face1_curves,
	const ON_SimpleArray<ON_Curve *> &face2_curves);

    // write out the log file that the dplot command references to
    // navigate the generated plot files
    void WriteLog();

    // get the number of intersecting surface pairs recorded
    int SurfacePairs();

    // get the number of isocurves that intersect either surface in
    // given surface pair
    int IntersectingIsocurves(int ssx_idx);

    // get the number of linked curves recorded
    int LinkedCurves();

    void SplitFaces(
	const ON_ClassArray<ON_SimpleArray<TrimmedFace *> > &split_faces);

    void LinkedCurves(
	const ON_Surface *surf,
	ON_ClassArray<LinkedCurve> &linked_curves);

    void
    Plot3DCurveFrom2D(
	const ON_Surface *surf,
	const ON_Curve *crv,
	const char *filename,
	unsigned char *color,
	bool decorate = false
	);

    void
    Plot3DCurve(
	const ON_Curve *crv,
	const char *filename,
	unsigned char *color,
	struct bu_list *vlist = NULL);

private:
    struct bu_list vlist_free_list;
    std::string prefix;
    bool have_surfaces;
    int brep1_surf_count;
    int brep2_surf_count;
    int linked_curve_count;
    std::vector< std::pair<int, int> > intersecting_surfaces; // ssx surface index pairs
    std::vector<int> ssx_events; // num final events of each ssx
    std::vector< std::vector<int> > ssx_isocsx_events; // num events for each isocsx of each ssx
    std::vector<int> ssx_isocsx_brep1_curves; // num ssx isocsx events using brep1 isocurves
    std::vector< std::pair<int, int> > ssx_clipped_curves; // num clipped intersection curves
    std::vector<int> split_face_outerloop_curves;
    std::vector<int> split_face_innerloop_curves;

    void PlotSurface(
	const ON_Surface &surf,
	const char *filename,
	unsigned char *color);

    void
    PlotBoundaryIsocurves(
	struct bu_list *vlist,
	const ON_Surface &surf,
	int knot_dir);
};

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

/*                    D E B U G _ P L O T . H
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
#include "bu.h"
#include "raytrace.h"

class DebugPlot {
public:
    // prefix all dplot output files with 'basename'
    DebugPlot(const char *basename);

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

    // write out the log file that the dplot command references to
    // navigate the generated plot files
    void WriteLog();

private:
    struct bu_list vlist_free_list;
    std::string prefix;
    bool have_surfaces;
    int brep1_surf_count;
    int brep2_surf_count;
    std::vector< std::pair<int, int> > intersecting_surfaces; // ssx surface index pairs
    std::vector<int> ssx_events; // num final events of each ssx
    std::vector< std::vector<int> > ssx_isocsx_events; // num events for each isocsx of each ssx
    std::vector<int> ssx_isocsx_brep1_curves; // num ssx isocsx events using brep1 isocurves

    void PlotSurface(
	const ON_Surface &surf,
	const char *filename,
	unsigned char *color);

    void
    Plot3DCurveFrom2D(
	const ON_Surface *surf,
	const ON_Curve *crv,
	const char *filename,
	unsigned char *color);

    void
    Plot3DCurve(
	const ON_Curve *crv,
	const char *filename,
	unsigned char *color,
	struct bu_list *vlist = NULL);

    void
    PlotBoundaryIsocurves(
	struct bu_list *vlist,
	const ON_Surface &surf,
	int knot_dir);
};

#if 0

void
plot_point(
    ON_3dPoint pt,
    double diameter,
    const char *prefix = "curve",
    unsigned char *color = NULL);
#endif

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

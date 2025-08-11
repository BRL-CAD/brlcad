/*                 D E C I M A T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2025 United States Government as represented by
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
/** @file decimate.cpp
 *
 * libbg decimation methods.  Right now the only method implemented hardly
 * warrants the name - it is a simple snapping/collapsing of vertices based on
 * a specified distance tolerance and removal of newly degenerate faces.
 *
 * The use case that motivated trying it is simplifying nasty plate mode BoTs
 * ahead of extrusion.
 *
 */

#include "common.h"

#include <inttypes.h>

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "bu/malloc.h"
#include "bu/log.h"
#include "bn/tol.h"
#include "bg/trimesh.h"

int trimesh_decimate_simple(
	int **ofaces, int *n_ofaces,
	int *ifaces, int n_ifaces, point_t *p, int n_p, struct bg_trimesh_decimation_settings *s)
{
    if (!ofaces || !n_ofaces || !ifaces || !n_ifaces || !p || !n_p || !s)
	return BRLCAD_ERROR;

    if (s->method != BG_TRIMESH_DECIMATION_METHOD_DEFAULT)
	return BRLCAD_ERROR;

    // Find the active points in the mesh - don't assume all points in p are active
    std::unordered_set<int> active_pnts;
    for (int i = 0; i < n_ifaces*3; i++)
	active_pnts.insert(ifaces[i]);

    // Bound the active points
    point_t bbmin, bbmax;
    VSETALL(bbmin, INFINITY);
    VSETALL(bbmax, -INFINITY);
    std::unordered_set<int>::iterator p_it;
    for (p_it = active_pnts.begin(); p_it != active_pnts.end(); p_it++) {
	VMINMAX(bbmin, bbmax, p[*p_it]);
    }
    // Find the largest numerical value
    double maxval = fabs(bbmin[0]);
    maxval = std::max(maxval, fabs(bbmin[1]));
    maxval = std::max(maxval, fabs(bbmin[2]));
    maxval = std::max(maxval, fabs(bbmax[0]));
    maxval = std::max(maxval, fabs(bbmax[1]));
    maxval = std::max(maxval, fabs(bbmax[2]));

    // Calculate a default size to try if we don't have a sane input - default
    // to 1/100 of the bbox diagonal
    double dfeature_size = DIST_PNT_PNT(bbmax, bbmin) * 0.01;

    // Too small and this won't work - don't go below VUNITIZE
    // TODO - is this right?  Not sure I've got the scaling behaving the way it
    // really should...
    fastf_t feature_size = (s->feature_size < VUNITIZE_TOL) ? dfeature_size: s->feature_size;
    s->feature_size = feature_size;
    double scale = 1/feature_size * 10;

    // Make sure we can handle the maximum value at the current feature size -
    // if not, we can't bin the numbers
    // TODO - is this right?
    int64_t log10max = log10(std::numeric_limits<int64_t>::max()) - 1;
    int64_t log10maxval = log10(fabs(maxval));
    int64_t log10scale = log10(fabs(scale));
    if ((log10maxval + log10scale) > log10max) {
	return BRLCAD_ERROR;
    }

    // We will be binning the points into a series of nested unordered sets
    // based on snapping the x,y,z coordinates to integers.
    std::unordered_map<int, int64_t> xint;
    std::unordered_map<int, int64_t> yint;
    std::unordered_map<int, int64_t> zint;
    std::unordered_map<int64_t, std::unordered_map<int64_t, std::unordered_map<int64_t, std::unordered_set<int>>>> pbins;
    // First, snap point values
    for (p_it = active_pnts.begin(); p_it != active_pnts.end(); p_it++) {
	xint[*p_it] = static_cast<int64_t>(std::round(p[*p_it][X] * scale));
	yint[*p_it] = static_cast<int64_t>(std::round(p[*p_it][Y] * scale));
	zint[*p_it] = static_cast<int64_t>(std::round(p[*p_it][Z] * scale));
	//bu_log("%d: %ld %ld %ld\n", *p_it, xint[*p_it], yint[*p_it], zint[*p_it]);
    }
    // Second, assign point indices
    for (p_it = active_pnts.begin(); p_it != active_pnts.end(); p_it++) {
	pbins[xint[*p_it]][yint[*p_it]][zint[*p_it]].insert(*p_it);
    }

    // For each of the bins with more than one point, find the point closest to the average and use that as our
    // target output point for the bin.
    size_t old_merged = 0;
    size_t mtgt_cnt = 0;
    std::unordered_map<int, int> old_to_new;
    std::unordered_map<int64_t, std::unordered_map<int64_t, std::unordered_map<int64_t, std::unordered_set<int>>>>::iterator px_it;
    for (px_it = pbins.begin(); px_it != pbins.end(); ++px_it) {
	std::unordered_map<int64_t, std::unordered_map<int64_t, std::unordered_set<int>>>::iterator  py_it;
	for (py_it = px_it->second.begin(); py_it != px_it->second.end(); ++py_it) {
	    std::unordered_map<int64_t, std::unordered_set<int>>::iterator  pz_it;
	    for (pz_it = py_it->second.begin(); pz_it != py_it->second.end(); ++pz_it) {
		std::unordered_set<int> &cbin = pz_it->second;
		if (cbin.size() == 1)
		    old_to_new[*cbin.begin()] = *cbin.begin();
		if (cbin.size() > 1) {
		    old_merged += cbin.size() - 1;
		    mtgt_cnt++;
		    // Multiple points - find the average value.
		    point_t vavg = VINIT_ZERO;
		    std::unordered_set<int>::iterator b_it;
		    for (b_it = cbin.begin(); b_it != cbin.end(); ++b_it) {
			VADD2(vavg, vavg, p[*b_it]);
		    }
		    VSCALE(vavg, vavg, 1/cbin.size());
		    // Find the actual point closest to the average
		    int cavg = *cbin.begin();
		    double dsqd = DIST_PNT_PNT_SQ(vavg, p[*cbin.begin()]);
		    for (b_it = cbin.begin(); b_it != cbin.end(); ++b_it) {
			double ndsqd = DIST_PNT_PNT_SQ(vavg, p[*b_it]);
			if (ndsqd < dsqd) {
			    dsqd = ndsqd;
			    cavg = *b_it;
			}
		    }
		    // Point all the points in the bin to the closest average point
		    for (b_it = cbin.begin(); b_it != cbin.end(); ++b_it) {
			old_to_new[*b_it] = cavg;
		    }
		}
	    }
	}
    }

    // Iterate over faces, assigning any that are not degenerate with the new mapping to a new face array.
    std::vector<int> nfaces;
    for (int i = 0; i < n_ifaces; i++) {
	int f_ind[3];
	f_ind[0] = old_to_new[ifaces[3*i+0]];
	f_ind[1] = old_to_new[ifaces[3*i+1]];
	f_ind[2] = old_to_new[ifaces[3*i+2]];

	if (f_ind[0] == f_ind[1])
	    continue;
	if (f_ind[0] == f_ind[2])
	    continue;
	if (f_ind[1] == f_ind[2])
	    continue;

	// Not degenerate - assign
	nfaces.push_back(f_ind[0]);
	nfaces.push_back(f_ind[1]);
	nfaces.push_back(f_ind[2]);
    }

    if (old_merged)
	bu_vls_printf(&s->msgs, "redirected %zd pnts to %zd new targets\n", old_merged, mtgt_cnt);
    if (n_ifaces != (int)(nfaces.size()/3))
	bu_vls_printf(&s->msgs, "removed %d faces\n", n_ifaces - (int)(nfaces.size()/3));

    if (!nfaces.size())
	return BRLCAD_ERROR;

    (*ofaces) = (int *)bu_calloc(nfaces.size(), sizeof(int), "ofaces");
    for (size_t i = 0; i < nfaces.size(); i++)
	(*ofaces)[i] = nfaces[i];
    *n_ofaces = (int)(nfaces.size()/3);

    return BRLCAD_OK;
}

int bg_trimesh_decimate(
	int **ofaces, int *n_ofaces,
	int *ifaces, int n_ifaces, point_t *p, int n_p, struct bg_trimesh_decimation_settings *s)
{
    if (!ofaces || !n_ofaces || !ifaces || !n_ifaces || !p || !n_p || !s)
	return BRLCAD_ERROR;

    if (s->method != BG_TRIMESH_DECIMATION_METHOD_DEFAULT)
	return BRLCAD_ERROR;

    return trimesh_decimate_simple(ofaces, n_ofaces, ifaces, n_ifaces, p, n_p, s);
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

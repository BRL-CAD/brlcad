/*                 T R I M E S H _ F U S E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2024 United States Government as represented by
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
/** @file trimesh_fuse.cpp
 *
 * Bin close vertices into buckets, merge vertices in the same bucket,
 * remove degenerate faces.
 */

#include "common.h"

#include <inttypes.h>
#include <limits>
#include <map>
#include <set>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bg/trimesh.h"

extern "C" int
bg_trimesh_fuse(int **of, int *ofcnt, fastf_t **ov, int *ovcnt,  int *f, int fcnt, fastf_t *v, int vcnt, fastf_t bsize)
{
    if (!of || !ofcnt || !f || !ov || !ovcnt || fcnt < 0 || !v || vcnt < 0 || bsize < SMALL_FASTF)
	return -1;

    // Bin the points into a series of nested unordered maps of sets based on
    // snapping the x,y coordinates to an integer.  There is a maximum limit
    // to the integer size, so we check both the supplied bsize and the upper
    // limit.  If bsize can't be handled, we go with the available maximum
    // limit.
    std::unordered_map<int64_t, std::unordered_map<int64_t, std::unordered_map<int64_t, std::set<int>>>> ind_bin;
    int64_t log10max = log10(std::numeric_limits<int64_t>::max()) - 1;
    int64_t log10bsize = (int64_t)log10(1/bsize) + 1;
    int log_limit = (log10max < log10bsize) ? log10max : log10bsize;
    double scale = pow(10, log_limit);
    std::vector<int64_t> sverts;
    for (int i = 0; i < vcnt*3; i++) {
	int64_t sval = static_cast<int64_t>(v[i]*scale);
	sverts.push_back(sval);
    }
    for (int i = 0; i < vcnt; i++)
	ind_bin[sverts[3*i]][sverts[3*i+1]][sverts[3*i+2]].insert(i);

    // Calculate the average point values for the bins with more than one point,
    // and build up a map of input vertex indexes to the snapped results.
    std::unordered_map<int, int> ptmap;
    std::unordered_map<int, point_t> avg_pt;
    std::unordered_map<int64_t, std::unordered_map<int64_t, std::unordered_map<int64_t, std::set<int>>>>::iterator x_it;
    std::unordered_map<int64_t, std::unordered_map<int64_t, std::set<int>>>::iterator y_it;
    std::unordered_map<int64_t, std::set<int>>::iterator z_it;
    for (x_it = ind_bin.begin(); x_it != ind_bin.end(); x_it++) {
	for (y_it = x_it->second.begin(); y_it != x_it->second.end(); y_it++) {
	    for (z_it = y_it->second.begin(); z_it != y_it->second.end(); z_it++) {
		std::set<int>::iterator p_ind_lowest = z_it->second.begin();
		if (z_it->second.size() < 1) {
		    ptmap[*p_ind_lowest] = *p_ind_lowest;
		    continue;
		}
		point_t pavg = VINIT_ZERO;
		std::set<int>::iterator p_ind;
		for (p_ind = z_it->second.begin(); p_ind != z_it->second.end(); p_ind++) {
		    ptmap[*p_ind] = *p_ind_lowest;
		    pavg[X] += v[3*(*p_ind)+0];
		    pavg[Y] += v[3*(*p_ind)+1];
		    pavg[Z] += v[3*(*p_ind)+2];
		}
		VSCALE(pavg, pavg, 1/(fastf_t)z_it->second.size());
		avg_pt[*p_ind_lowest][X] = pavg[X];
		avg_pt[*p_ind_lowest][Y] = pavg[Y];
		avg_pt[*p_ind_lowest][Z] = pavg[Z];
	    }
	}
    }

    // Rebuild the faces array using snapped vertices
    // Remove any triangle that is now degenerate
    std::vector<int> fmapped;
    std::unordered_set<int> used_verts;
    for (int i = 0; i < fcnt; i++) {
	int verts[3];
	verts[0] = ptmap[f[3*i+0]];
	verts[1] = ptmap[f[3*i+1]];
	verts[2] = ptmap[f[3*i+2]];
	if (verts[0] == verts[1] || verts[0] == verts[2] || verts[1] == verts[2])
	    continue;
	for (int j = 0; j < 3; j++) {
	    used_verts.insert(verts[j]);
	    fmapped.push_back(verts[j]);
	}
    }

    // Rebuild output mesh with just needed verts and faces
    std::unordered_map<int, int> old_to_new;
    (*ov) = (fastf_t *)bu_calloc(used_verts.size()*3, sizeof(fastf_t), "output verts");
    std::unordered_set<int>::iterator uv_it;
    int oind = 0;
    for (uv_it = used_verts.begin(); uv_it != used_verts.end(); uv_it++) {
	if (avg_pt.find(*uv_it) != avg_pt.end()) {
	    (*ov)[3*oind+0] = avg_pt[*uv_it][X];
	    (*ov)[3*oind+1] = avg_pt[*uv_it][Y];
	    (*ov)[3*oind+2] = avg_pt[*uv_it][Z];
	} else {
	    (*ov)[3*oind+0] = v[3*(*uv_it)+0];
	    (*ov)[3*oind+1] = v[3*(*uv_it)+1];
	    (*ov)[3*oind+2] = v[3*(*uv_it)+2];
	}
	old_to_new[*uv_it] = oind;
	oind++;
    }
    (*ovcnt) = (int)used_verts.size();
    (*of) = (int *)bu_calloc(fmapped.size(), sizeof(int), "output faces");
    for (size_t i = 0; i < fmapped.size(); i++)
	(*of)[i] = old_to_new[fmapped[i]];
    (*ofcnt) = (int)(fmapped.size()/3);

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


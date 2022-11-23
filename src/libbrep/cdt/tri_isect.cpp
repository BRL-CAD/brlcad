/*              C D T _ T R I _ I S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2022 United States Government as represented by
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
/** @addtogroup libbrep */
/** @{ */
/** @file cdt_ovlps.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 * This file defines triangle intersection logic specifically
 * tailored for the needs of CDT overlap resolution.
 *
 */

#include "common.h"
#include <queue>
#include <numeric>
#include <random>
#include <sstream>
#include <iomanip>

#include "bu/str.h"
#include "bg/chull.h"
#include "bg/tri_pt.h"
#include "bg/tri_tri.h"
#include "./cdt.h"

// Four orders below the minimum involved triangle edge length by default...
#define TRI_ISECT_TOL_FACTOR 0.0001

// default to .3 of the minimum involved edge length
#define TRI_NEAR_EDGE_TOL 0.3

class tri_isect_t {
    public:

	tri_isect_t(triangle_t &tri1, triangle_t &tri2, int m)
	{
	    fmesh1 = tri1.m;
	    fmesh2 = tri2.m;
	    t1 = tri1;
	    t2 = tri2;
	    mode = m;

	    // Set up the ON_Lines for the triangles
	    t1_lines[0] = ON_Line(*fmesh1->pnts[t1.v[0]], *fmesh1->pnts[t1.v[1]]);
	    t1_lines[1] = ON_Line(*fmesh1->pnts[t1.v[1]], *fmesh1->pnts[t1.v[2]]);
	    t1_lines[2] = ON_Line(*fmesh1->pnts[t1.v[2]], *fmesh1->pnts[t1.v[0]]);

	    t2_lines[0] = ON_Line(*fmesh2->pnts[t2.v[0]], *fmesh2->pnts[t2.v[1]]);
	    t2_lines[1] = ON_Line(*fmesh2->pnts[t2.v[1]], *fmesh2->pnts[t2.v[2]]);
	    t2_lines[2] = ON_Line(*fmesh2->pnts[t2.v[2]], *fmesh2->pnts[t2.v[0]]);

	    elen_min = DBL_MAX;
	    for (int i = 0; i < 3; i++) {
		elen_min = (elen_min > t1_lines[i].Length()) ? t1_lines[i].Length() : elen_min;
		elen_min = (elen_min > t2_lines[i].Length()) ? t2_lines[i].Length() : elen_min;
	    }
	}

	cdt_mesh_t *fmesh1;
	triangle_t t1;
	cdt_mesh_t *fmesh2;
	triangle_t t2;

	ON_3dPoint ipt_1;
	ON_3dPoint ipt_2;

	bool isect_basic();

	bool isect_tri_near_verts(double etol);
	bool isect_edge_only(double etol);

	bool edge_midpoints_inside(int ind);

	bool isect_tri_near_verts() {
	    return isect_tri_near_verts(TRI_ISECT_TOL_FACTOR * elen_min);
	}
	bool isect_edge_only() {
	    return isect_edge_only(TRI_NEAR_EDGE_TOL * elen_min);
	}

	void plot(const char *fname);

	ON_Line t1_lines[3];
	ON_Line t2_lines[3];
	double elen_min;
	double lt[4];
	ON_Line t1_nedge, t2_nedge;
	ON_Line t1_fedges[2];
	ON_Line t2_fedges[2];

	edge_t t1_e, t2_e;

    private:
	int mode;
	bool find_intersecting_edges(double etol);

};

void
tri_isect_t::plot(const char *fname)
{
    FILE *plotfile = fopen(fname, "w");
    double fpnt_r = -1.0;
    double pnt_r = -1.0;
    pl_color(plotfile, 0, 0, 255);
    fmesh1->plot_tri(t1, NULL, plotfile, 0, 0, 0);
    pnt_r = fmesh1->tri_pnt_r(t1.ind);
    fpnt_r = (pnt_r > fpnt_r) ? pnt_r : fpnt_r;
    pl_color(plotfile, 255, 0, 0);
    fmesh2->plot_tri(t2, NULL, plotfile, 0, 0, 0);
    pnt_r = fmesh2->tri_pnt_r(t2.ind);
    fpnt_r = (pnt_r > fpnt_r) ? pnt_r : fpnt_r;
    pl_color(plotfile, 255, 255, 255);
    plot_pnt_3d(plotfile, &ipt_1, fpnt_r, 0);
    plot_pnt_3d(plotfile, &ipt_2, fpnt_r, 0);
    point_t isectpt1, isectpt2;
    VSET(isectpt1, ipt_1.x, ipt_1.y, ipt_1.z);
    VSET(isectpt2, ipt_2.x, ipt_2.y, ipt_2.z);
    pdv_3move(plotfile, isectpt1);
    pdv_3cont(plotfile, isectpt2);
    fclose(plotfile);
}

bool
tri_isect_t::isect_basic()
{
    int coplanar = 0;
    point_t T1_V[3];
    point_t T2_V[3];
    point_t isectpt1 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};
    point_t isectpt2 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};

    VSET(T1_V[0], fmesh1->pnts[t1.v[0]]->x, fmesh1->pnts[t1.v[0]]->y, fmesh1->pnts[t1.v[0]]->z);
    VSET(T1_V[1], fmesh1->pnts[t1.v[1]]->x, fmesh1->pnts[t1.v[1]]->y, fmesh1->pnts[t1.v[1]]->z);
    VSET(T1_V[2], fmesh1->pnts[t1.v[2]]->x, fmesh1->pnts[t1.v[2]]->y, fmesh1->pnts[t1.v[2]]->z);
    VSET(T2_V[0], fmesh2->pnts[t2.v[0]]->x, fmesh2->pnts[t2.v[0]]->y, fmesh2->pnts[t2.v[0]]->z);
    VSET(T2_V[1], fmesh2->pnts[t2.v[1]]->x, fmesh2->pnts[t2.v[1]]->y, fmesh2->pnts[t2.v[1]]->z);
    VSET(T2_V[2], fmesh2->pnts[t2.v[2]]->x, fmesh2->pnts[t2.v[2]]->y, fmesh2->pnts[t2.v[2]]->z);
    int isect = bg_tri_tri_isect_with_line(T1_V[0], T1_V[1], T1_V[2], T2_V[0], T2_V[1], T2_V[2], &coplanar, &isectpt1, &isectpt2);

    if (!isect) {
	// No intersection - we're done
	return false;
    }

    ipt_1 = ON_3dPoint(isectpt1[X], isectpt1[Y], isectpt1[Z]);
    ipt_2 = ON_3dPoint(isectpt2[X], isectpt2[Y], isectpt2[Z]);
    if (ipt_1.DistanceTo(ipt_2) < ON_ZERO_TOLERANCE) {
	// Intersection is a single point - not volumetric
	return false;
    }

    return true;
}


bool
tri_isect_t::isect_tri_near_verts(double etol)
{
    // See if we're intersecting very close to triangle vertices
    bool vert_isect = false;
    double t1_i1_vdists[3];
    double t1_i2_vdists[3];
    double t2_i1_vdists[3];
    double t2_i2_vdists[3];

    for (int i = 0; i < 3; i++) {
	t1_i1_vdists[i] = fmesh1->pnts[t1.v[i]]->DistanceTo(ipt_1);
	t2_i1_vdists[i] = fmesh2->pnts[t2.v[i]]->DistanceTo(ipt_1);
    	t1_i2_vdists[i] = fmesh1->pnts[t1.v[i]]->DistanceTo(ipt_2);
	t2_i2_vdists[i] = fmesh2->pnts[t2.v[i]]->DistanceTo(ipt_2);
    }
    for (int i = 0; i < 3; i++) {
	if (t1_i1_vdists[i] < etol && t1_i2_vdists[i] < etol) {
	    vert_isect = true;
	}
	if (t2_i1_vdists[i] < etol && t2_i2_vdists[i] < etol) {
	    vert_isect = true;
	}
    }

    return vert_isect;
}
bool
tri_isect_t::find_intersecting_edges(double etol)
{
    double p1d1[3], p1d2[3];
    double p2d1[3], p2d2[3];
    for (int i = 0; i < 3; i++) {
	p1d1[i] = ipt_1.DistanceTo(t1_lines[i].ClosestPointTo(ipt_1));
	p2d1[i] = ipt_2.DistanceTo(t1_lines[i].ClosestPointTo(ipt_2));
	p1d2[i] = ipt_1.DistanceTo(t2_lines[i].ClosestPointTo(ipt_1));
	p2d2[i] = ipt_2.DistanceTo(t2_lines[i].ClosestPointTo(ipt_2));
    }

    int fedgecnt = 0;
    bool nedge_1 = false;
    for (int i = 0; i < 3; i++) {
	if (NEAR_ZERO(p1d1[i], etol) &&  NEAR_ZERO(p2d1[i], etol)) {
	    //std::cout << "edge-only intersect - e1\n";
	    t1_nedge = t1_lines[i];
	    int v2 = (i < 2) ? i + 1 : 0;
	    t1_e = edge_t(t1.v[i], t1.v[v2]);
	    nedge_1 = true;
	} else {
	    if (fedgecnt < 2) {
		t1_fedges[fedgecnt] = t1_lines[i];
		fedgecnt++;
	    }
	}
    }

    fedgecnt = 0;
    bool nedge_2 = false;
    for (int i = 0; i < 3; i++) {
	if (NEAR_ZERO(p1d2[i], etol) &&  NEAR_ZERO(p2d2[i], etol)) {
	    //std::cout << "edge-only intersect - e1\n";
	    t2_nedge = t2_lines[i];
	    int v2 = (i < 2) ? i + 1 : 0;
	    t2_e = edge_t(t2.v[i], t2.v[v2]);
	    nedge_2 = true;
	} else {
	    if (fedgecnt < 2) {
		t2_fedges[fedgecnt] = t2_lines[i];
		fedgecnt++;
	    }
	}
    }

    if (!nedge_1 || !nedge_2) {
	return false;
    }

    // If the edges aren't properly aligned and we need the closest points
    double llen = -DBL_MAX;

    t1_nedge.ClosestPointTo(t2_nedge.PointAt(0), &(lt[0]));
    t1_nedge.ClosestPointTo(t2_nedge.PointAt(1), &(lt[1]));
    t2_nedge.ClosestPointTo(t1_nedge.PointAt(0), &(lt[2]));
    t2_nedge.ClosestPointTo(t1_nedge.PointAt(1), &(lt[3]));

    double ll[4];
    ll[0] = t1_nedge.PointAt(0).DistanceTo(t2_nedge.PointAt(lt[2]));
    ll[1] = t1_nedge.PointAt(1).DistanceTo(t2_nedge.PointAt(lt[3]));
    ll[2] = t2_nedge.PointAt(0).DistanceTo(t1_nedge.PointAt(lt[0]));
    ll[3] = t2_nedge.PointAt(1).DistanceTo(t1_nedge.PointAt(lt[1]));
    for (int i = 0; i < 4; i++) {
	llen = (ll[i] > llen) ? ll[i] : llen;
    }
    // Max dist from line too large, not an edge only intersect
    if ( llen > etol ) {
	return false;
    }

    return true;
}

int
near_edge_process(double t, double vtol)
{
    if (t > 0  && t < 1 && !NEAR_ZERO(t, vtol) && !NEAR_EQUAL(t, 1, vtol)) {
	return 1;
    }
    return 0;
}

/*****************************************************************************
 * We're only concerned with specific categories of intersections between
 * triangles, so filter accordingly.
 * Return 0 if no intersection, 1 if coplanar intersection, 2 if non-coplanar
 * intersection.
 *****************************************************************************/
int
tri_isect(
	triangle_t &t1,
       	triangle_t &t2,
	int mode
	)
{
    tri_isect_t tri_isection(t1, t2, mode);

    if (!tri_isection.isect_basic()) {
	return 0;
    }

#if 0
    if (TRICHK(fmesh1, fmesh2, t1, t2)) {
	fmesh1->tri_plot(t1, "t1.plot3");
	fmesh2->tri_plot(t2, "t2.plot3");
	std::cout << "working problem tri\n";
    }
#endif

    // tri_isection.plot("tri_isect.plot3");

    // This category of intersection isn't one that warrants an intersection return.
    if (tri_isection.isect_tri_near_verts()) {
	return 0;
    }

    return 1;
}

/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


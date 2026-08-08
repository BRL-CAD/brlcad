/*                       C O B B . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Based off of code from Ayam:
 *
 * This software is copyrighted (c) 1998-2012 by
 * Randolf Schultz (randolf.schultz@gmail.com).
 * All rights reserved.
 *
 * The author hereby grants permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 *
 * IN NO EVENT SHALL THE AUTHOR OR DISTRIBUTORS BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
 * DERIVATIVES THEREOF, EVEN IF THE AUTHOR HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHOR AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
 * IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHOR AND DISTRIBUTORS HAVE
 * NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file cobb.cpp
 *
 * J. E. Cobb's six-patch rational Bezier sphere, with both the historical
 * independent-face topology and a shared solid topology for seam tests.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "brep/cobb.h"
#include "opennurbs.h"


ON_BezierSurface *
ON_Brep_CobbSphereFace(double rotation_x, double rotation_z)
{
    const double t = sqrt(3.0);
    const double d = sqrt(2.0);
    const double s = sqrt(6.0);
    const double cvs[5][5][4] = {
	{
	    {4.0*(1.0-t), 4.0*(1.0-t), 4.0*(1.0-t), 4.0*(3.0-t)},
	    {-d, d*(t-4.0), d*(t-4.0), d*(3.0*t-2.0)},
	    {0.0, 4.0*(1.0-2.0*t)/3.0, 4.0*(1.0-2.0*t)/3.0,
		4.0*(5.0-t)/3.0},
	    {d, d*(t-4.0), d*(t-4.0), d*(3.0*t-2.0)},
	    {4.0*(t-1.0), 4.0*(1.0-t), 4.0*(1.0-t), 4.0*(3.0-t)}
	},
	{
	    {d*(t-4.0), -d, d*(t-4.0), d*(3.0*t-2.0)},
	    {(2.0-3.0*t)/2.0, (2.0-3.0*t)/2.0, -(t+6.0)/2.0,
		(t+6.0)/2.0},
	    {0.0, d*(2.0*t-7.0)/3.0, -5.0*s/3.0, d*(t+6.0)/3.0},
	    {(3.0*t-2.0)/2.0, (2.0-3.0*t)/2.0, -(t+6.0)/2.0,
		(t+6.0)/2.0},
	    {d*(4.0-t), -d, d*(t-4.0), d*(3.0*t-2.0)}
	},
	{
	    {4.0*(1.0-2.0*t)/3.0, 0.0, 4.0*(1.0-2.0*t)/3.0,
		4.0*(5.0-t)/3.0},
	    {d*(2.0*t-7.0)/3.0, 0.0, -5.0*s/3.0, d*(t+6.0)/3.0},
	    {0.0, 0.0, 4.0*(t-5.0)/3.0, 4.0*(5.0*t-1.0)/9.0},
	    {-d*(2.0*t-7.0)/3.0, 0.0, -5.0*s/3.0, d*(t+6.0)/3.0},
	    {-4.0*(1.0-2.0*t)/3.0, 0.0, 4.0*(1.0-2.0*t)/3.0,
		4.0*(5.0-t)/3.0}
	},
	{
	    {d*(t-4.0), d, d*(t-4.0), d*(3.0*t-2.0)},
	    {(2.0-3.0*t)/2.0, -(2.0-3.0*t)/2.0, -(t+6.0)/2.0,
		(t+6.0)/2.0},
	    {0.0, -d*(2.0*t-7.0)/3.0, -5.0*s/3.0, d*(t+6.0)/3.0},
	    {(3.0*t-2.0)/2.0, -(2.0-3.0*t)/2.0, -(t+6.0)/2.0,
		(t+6.0)/2.0},
	    {d*(4.0-t), d, d*(t-4.0), d*(3.0*t-2.0)}
	},
	{
	    {4.0*(1.0-t), -4.0*(1.0-t), 4.0*(1.0-t), 4.0*(3.0-t)},
	    {-d, -d*(t-4.0), d*(t-4.0), d*(3.0*t-2.0)},
	    {0.0, -4.0*(1.0-2.0*t)/3.0, 4.0*(1.0-2.0*t)/3.0,
		4.0*(5.0-t)/3.0},
	    {d, -d*(t-4.0), d*(t-4.0), d*(3.0*t-2.0)},
	    {4.0*(t-1.0), -4.0*(1.0-t), 4.0*(1.0-t), 4.0*(3.0-t)}
	}
    };

    ON_BezierSurface *surface = new ON_BezierSurface(3, true, 5, 5);
    surface->ReserveCVCapacity(100);
    for (int i = 0; i < 5; ++i) {
	for (int j = 0; j < 5; ++j)
	    surface->SetCV(i, j, ON_4dPoint(cvs[i][j][0], cvs[i][j][1],
		cvs[i][j][2], cvs[i][j][3]));
    }

    const ON_3dPoint center(0.0, 0.0, 0.0);
    surface->Rotate(rotation_x * ON_PI / 180.0,
	ON_3dVector(1.0, 0.0, 0.0), center);
    surface->Rotate(rotation_z * ON_PI / 180.0,
	ON_3dVector(0.0, 0.0, 1.0), center);
    return surface;
}


static ON_NurbsSurface *
cobb_nurbs_face(double rotation_x, double rotation_z, double radius,
	const ON_3dPoint &origin)
{
    ON_BezierSurface *bezier = ON_Brep_CobbSphereFace(rotation_x,
	rotation_z);
    if (!bezier)
	return NULL;
    ON_NurbsSurface *surface = ON_NurbsSurface::New();
    if (!bezier->GetNurbForm(*surface)) {
	delete surface;
	delete bezier;
	return NULL;
    }
    delete bezier;
    surface->Scale(radius);
    surface->Translate(ON_3dVector(origin.x, origin.y, origin.z));
    return surface;
}


static bool
cobb_face_outward(const ON_Surface *surface, const ON_3dPoint &origin)
{
    const ON_Interval u = surface->Domain(0);
    const ON_Interval v = surface->Domain(1);
    ON_3dPoint point;
    ON_3dVector du;
    ON_3dVector dv;
    if (!surface->Ev1Der(u.Mid(), v.Mid(), point, du, dv))
	return true;
    return ON_CrossProduct(du, dv) * (point - origin) > 0.0;
}


ON_Brep *
ON_Brep_CobbSphereUnsewn(double radius, const ON_3dPoint &origin)
{
    if (!(radius > 0.0) || !std::isfinite(radius) || !origin.IsValid())
	return NULL;

    const double rotations[6][2] = {
	{0.0, 0.0}, {90.0, 0.0}, {180.0, 0.0},
	{270.0, 0.0}, {90.0, 90.0}, {90.0, 270.0}
    };
    ON_Brep *brep = ON_Brep::New();
    for (int i = 0; i < 6; ++i) {
	ON_NurbsSurface *surface = cobb_nurbs_face(rotations[i][0],
	    rotations[i][1], radius, origin);
	if (!surface) {
	    delete brep;
	    return NULL;
	}
	ON_BrepFace *face = brep->NewFace(*surface);
	if (!face) {
	    delete surface;
	    delete brep;
	    return NULL;
	}
	face->m_bRev = !cobb_face_outward(surface, origin);
	delete surface;
    }
    brep->Standardize();
    brep->Compact();
    return brep;
}


ON_Brep *
ON_Brep_CobbSphereSewn(double radius, const ON_3dPoint &origin)
{
    if (!(radius > 0.0) || !std::isfinite(radius) || !origin.IsValid())
	return NULL;

    const double rotations[6][2] = {
	{0.0, 0.0}, {90.0, 0.0}, {180.0, 0.0},
	{270.0, 0.0}, {90.0, 90.0}, {90.0, 270.0}
    };
    const double coordinate_scale = std::max(std::max(radius, 1.0),
	std::max(std::max(fabs(origin.x), fabs(origin.y)), fabs(origin.z)));
    const double corner_tolerance = std::max(ON_ZERO_TOLERANCE,
	128.0 * std::numeric_limits<double>::epsilon() * coordinate_scale);
    ON_Brep *brep = ON_Brep::New();
    for (int face_index = 0; face_index < 6; ++face_index) {
	ON_NurbsSurface *surface = cobb_nurbs_face(rotations[face_index][0],
	    rotations[face_index][1], radius, origin);
	if (!surface) {
	    delete brep;
	    return NULL;
	}

	const ON_Interval u = surface->Domain(0);
	const ON_Interval v = surface->Domain(1);
	const ON_3dPoint corners[4] = {
	    surface->PointAt(u.Min(), v.Min()),
	    surface->PointAt(u.Max(), v.Min()),
	    surface->PointAt(u.Max(), v.Max()),
	    surface->PointAt(u.Min(), v.Max())
	};
	int vid[4] = {-1, -1, -1, -1};
	for (int corner = 0; corner < 4; ++corner) {
	    for (int vertex = 0; vertex < brep->m_V.Count(); ++vertex) {
		if (corners[corner].DistanceTo(brep->m_V[vertex].Point()) <=
			corner_tolerance) {
		    vid[corner] = vertex;
		    break;
		}
	    }
	}

	/* Oriented outer-loop corner pairs for south, east, north, west. */
	const int side_corners[4][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
	int eid[4] = {-1, -1, -1, -1};
	bool reversed[4] = {false, false, false, false};
	for (int side = 0; side < 4; ++side) {
	    const int from = vid[side_corners[side][0]];
	    const int to = vid[side_corners[side][1]];
	    if (from < 0 || to < 0)
		continue;
	    for (int edge = 0; edge < brep->m_E.Count(); ++edge) {
		const int edge_from = brep->m_E[edge].m_vi[0];
		const int edge_to = brep->m_E[edge].m_vi[1];
		if ((edge_from == from && edge_to == to) ||
			(edge_from == to && edge_to == from)) {
		    eid[side] = edge;
		    reversed[side] = edge_from != from;
		    break;
		}
	    }
	}

	const bool outward = cobb_face_outward(surface, origin);
	ON_BrepFace *face = brep->NewFace(surface, vid, eid, reversed);
	if (!face) {
	    delete surface;
	    delete brep;
	    return NULL;
	}
	face->m_bRev = !outward;
	for (int corner = 0; corner < 4; ++corner)
	    brep->m_V[vid[corner]].m_tolerance = 0.0;
	for (int side = 0; side < 4; ++side)
	    brep->m_E[eid[side]].m_tolerance = 0.0;
    }

    for (int trim = 0; trim < brep->m_T.Count(); ++trim) {
	brep->m_T[trim].m_tolerance[0] = 0.0;
	brep->m_T[trim].m_tolerance[1] = 0.0;
    }
    brep->Standardize();
    brep->Compact();
    return brep;
}


double
ON_Brep_CobbSphereMaxRadialError(const ON_Brep *brep, double radius,
	const ON_3dPoint &origin, int samples_per_direction)
{
    if (!brep || !(radius > 0.0) || !std::isfinite(radius) ||
	    !origin.IsValid() || samples_per_direction < 2)
	return std::numeric_limits<double>::infinity();

    double maximum_error = 0.0;
    for (int si = 0; si < brep->m_S.Count(); ++si) {
	const ON_Surface *surface = brep->m_S[si];
	if (!surface)
	    return std::numeric_limits<double>::infinity();
	const ON_Interval u = surface->Domain(0);
	const ON_Interval v = surface->Domain(1);
	for (int i = 0; i < samples_per_direction; ++i) {
	    const double up = u.ParameterAt((double)i /
		(double)(samples_per_direction - 1));
	    for (int j = 0; j < samples_per_direction; ++j) {
		const double vp = v.ParameterAt((double)j /
		    (double)(samples_per_direction - 1));
		const double sample_radius = surface->PointAt(up, vp).
		    DistanceTo(origin);
		maximum_error = std::max(maximum_error,
		    fabs(sample_radius - radius));
	    }
	}
    }
    return maximum_error;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

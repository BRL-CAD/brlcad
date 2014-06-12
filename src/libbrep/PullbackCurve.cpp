/*               P U L L B A C K C U R V E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2009-2014 United States Government as represented by
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
/** @file step/PullbackCurve.cpp
 *
 * Pull curve back into UV space from 3D space
 *
 */

#include "common.h"

#include "vmath.h"
#include "dvec.h"

#include <assert.h>
#include <vector>
#include <list>
#include <limits>
#include <set>
#include <map>

#include "brep.h"

/* interface header */
#include "PullbackCurve.h"


#define RANGE_HI 0.55
#define RANGE_LO 0.45
#define UNIVERSAL_SAMPLE_COUNT 1001

/* FIXME: duplicated with opennurbs_ext.cpp */
class BSpline
{
public:
    int p; // degree
    int m; // num_knots-1
    int n; // num_samples-1 (aka number of control points)
    std::vector<double> params;
    std::vector<double> knots;
    ON_2dPointArray controls;
};

bool
isFlat(const ON_2dPoint& p1, const ON_2dPoint& m, const ON_2dPoint& p2, double flatness)
{
    ON_Line line = ON_Line(ON_3dPoint(p1), ON_3dPoint(p2));
    return line.DistanceTo(ON_3dPoint(m)) <= flatness;
}

void
utah_ray_planes(const ON_Ray &r, ON_3dVector &p1, double &p1d, ON_3dVector &p2, double &p2d)
{
    ON_3dPoint ro(r.m_origin);
    ON_3dVector rdir(r.m_dir);
    double rdx, rdy, rdz;
    double rdxmag, rdymag, rdzmag;

    rdx = rdir.x;
    rdy = rdir.y;
    rdz = rdir.z;

    rdxmag = fabs(rdx);
    rdymag = fabs(rdy);
    rdzmag = fabs(rdz);

    if (rdxmag > rdymag && rdxmag > rdzmag)
	p1 = ON_3dVector(rdy, -rdx, 0);
    else
	p1 = ON_3dVector(0, rdz, -rdy);
    p1.Unitize();

    p2 = ON_CrossProduct(p1, rdir);

    p1d = -(p1 * ro);
    p2d = -(p2 * ro);
}


enum seam_direction
seam_direction(ON_2dPoint uv1, ON_2dPoint uv2)
{
    if (NEAR_EQUAL(uv1.x, 0.0, PBC_TOL) && NEAR_EQUAL(uv2.x, 0.0, PBC_TOL)) {
	return WEST_SEAM;
    } else if (NEAR_EQUAL(uv1.x, 1.0, PBC_TOL) && NEAR_EQUAL(uv2.x, 1.0, PBC_TOL)) {
	return EAST_SEAM;
    } else if (NEAR_EQUAL(uv1.y, 0.0, PBC_TOL) && NEAR_EQUAL(uv2.y, 0.0, PBC_TOL)) {
	return SOUTH_SEAM;
    } else if (NEAR_EQUAL(uv1.y, 1.0, PBC_TOL) && NEAR_EQUAL(uv2.y, 1.0, PBC_TOL)) {
	return NORTH_SEAM;
    } else {
	return UNKNOWN_SEAM_DIRECTION;
    }
}


ON_BOOL32
GetDomainSplits(
	const ON_Surface *surf,
	const ON_Interval &u_interval,
	const ON_Interval &v_interval,
	ON_Interval domSplits[2][2]
        )
{
    ON_3dPoint min, max;
    ON_Interval dom[2];
    double length[2];

    // initialize intervals
    for (int i = 0; i < 2; i++) {
	for (int j = 0; j < 2; j++) {
	    domSplits[i][j] = ON_Interval::EmptyInterval;
	}
    }

    min[0] = u_interval.m_t[0];
    min[1] = v_interval.m_t[0];
    max[0] = u_interval.m_t[1];
    max[1] = v_interval.m_t[1];

    for (int i=0; i<2; i++) {
	if (surf->IsClosed(i)) {
	    dom[i] = surf->Domain(i);
	    length[i] = dom[i].Length();
	    if ((max[i] - min[i]) >= length[i]) {
		domSplits[i][0] = dom[i];
	    } else {
		double minbound = min[i];
		double maxbound = max[i];
		while (minbound < dom[i].m_t[0]) {
		    minbound += length[i];
		    maxbound += length[i];
		}
		while (minbound > dom[i].m_t[1]) {
		    minbound -= length[i];
		    maxbound -= length[i];
		}
		if (maxbound > dom[i].m_t[1]) {
		    domSplits[i][0].m_t[0] = minbound;
		    domSplits[i][0].m_t[1] = dom[i].m_t[1];
		    domSplits[i][1].m_t[0] = dom[i].m_t[0];
		    domSplits[i][1].m_t[1] = maxbound - length[i];
		} else {
		    domSplits[i][0].m_t[0] = minbound;
		    domSplits[i][0].m_t[1] = maxbound;
		}
	    }
	} else {
	    domSplits[i][0].m_t[0] = min[i];
	    domSplits[i][0].m_t[1] = max[i];
	}
    }

    return true;
}


/*
 * Wrapped OpenNURBS 'EvNormal()' because it fails when at surface singularity
 * but not on edge of domain. If fails and at singularity this wrapper will
 * reevaluate at domain edge.
 */
ON_BOOL32
surface_EvNormal( // returns false if unable to evaluate
	 const ON_Surface *surf,
         double s, double t, // evaluation parameters (s,t)
         ON_3dPoint& point,  // returns value of surface
         ON_3dVector& normal, // unit normal
         int side,       // optional - determines which side to evaluate from
                         //         0 = default
                         //         1 from NE quadrant
                         //         2 from NW quadrant
                         //         3 from SW quadrant
                         //         4 from SE quadrant
         int* hint       // optional - evaluation hint (int[2]) used to speed
                         //            repeated evaluations
         )
{
    ON_BOOL32 rc = false;

    if (!(rc=surf->EvNormal(s, t, point, normal, side, hint))) {
	side = IsAtSingularity(surf, s, t, PBC_SEAM_TOL);// 0 = south, 1 = east, 2 = north, 3 = west
	if (side >= 0) {
	    ON_Interval u = surf->Domain(0);
	    ON_Interval v = surf->Domain(1);
	    if (side == 0) {
		rc=surf->EvNormal(u.m_t[0], v.m_t[0], point, normal, side, hint);
	    } else if (side == 1) {
		rc=surf->EvNormal(u.m_t[1], v.m_t[1], point, normal, side, hint);
	    } else if (side == 2) {
		rc=surf->EvNormal(u.m_t[1], v.m_t[1], point, normal, side, hint);
	    } else if (side == 3) {
		rc=surf->EvNormal(u.m_t[0], v.m_t[0], point, normal, side, hint);
	    }
	}
    }
    return rc;
}


ON_BOOL32
surface_GetBoundingBox(
	const ON_Surface *surf,
	const ON_Interval &u_interval,
	const ON_Interval &v_interval,
        ON_BoundingBox& bbox,
	ON_BOOL32 bGrowBox
        )
{
    /* TODO: Address for threading
     * defined static for first time thru initialization, will
     * have to do something else here for multiple threads
     */
    static ON_RevSurface *rev_surface = ON_RevSurface::New();
    static ON_NurbsSurface *nurbs_surface = ON_NurbsSurface::New();
    static ON_Extrusion *extr_surface = new ON_Extrusion();
    static ON_PlaneSurface *plane_surface = new ON_PlaneSurface();
    static ON_SumSurface *sum_surface = ON_SumSurface::New();
    static ON_SurfaceProxy *proxy_surface = new ON_SurfaceProxy();

    ON_Interval domSplits[2][2] = { { ON_Interval::EmptyInterval, ON_Interval::EmptyInterval }, { ON_Interval::EmptyInterval, ON_Interval::EmptyInterval }};
    if (!GetDomainSplits(surf,u_interval,v_interval,domSplits)) {
	return false;
    }

    bool growcurrent = bGrowBox;
    for (int i=0; i<2; i++) {
	if (domSplits[0][i] == ON_Interval::EmptyInterval)
	    continue;

	for (int j=0; j<2; j++) {
	    if (domSplits[1][j] != ON_Interval::EmptyInterval) {
		if (dynamic_cast<ON_RevSurface * >(const_cast<ON_Surface *>(surf)) != NULL) {
		    *rev_surface = *dynamic_cast<ON_RevSurface * >(const_cast<ON_Surface *>(surf));
		    if (rev_surface->Trim(0, domSplits[0][i]) && rev_surface->Trim(1, domSplits[1][j])) {
			if (!rev_surface->GetBoundingBox(bbox, growcurrent)) {
			    return false;
			}
			growcurrent = true;
		    }
		} else if (dynamic_cast<ON_NurbsSurface * >(const_cast<ON_Surface *>(surf)) != NULL) {
		    *nurbs_surface = *dynamic_cast<ON_NurbsSurface * >(const_cast<ON_Surface *>(surf));
		    if (nurbs_surface->Trim(0, domSplits[0][i]) && nurbs_surface->Trim(1, domSplits[1][j])) {
			if (!nurbs_surface->GetBoundingBox(bbox, growcurrent)) {
			    return false;
			}
		    }
		    growcurrent = true;
		} else if (dynamic_cast<ON_Extrusion * >(const_cast<ON_Surface *>(surf)) != NULL) {
		    *extr_surface = *dynamic_cast<ON_Extrusion * >(const_cast<ON_Surface *>(surf));
		    if (extr_surface->Trim(0, domSplits[0][i]) && extr_surface->Trim(1, domSplits[1][j])) {
			if (!extr_surface->GetBoundingBox(bbox, growcurrent)) {
			    return false;
			}
		    }
		    growcurrent = true;
		} else if (dynamic_cast<ON_PlaneSurface * >(const_cast<ON_Surface *>(surf)) != NULL) {
		    *plane_surface = *dynamic_cast<ON_PlaneSurface * >(const_cast<ON_Surface *>(surf));
		    if (plane_surface->Trim(0, domSplits[0][i]) && plane_surface->Trim(1, domSplits[1][j])) {
			if (!plane_surface->GetBoundingBox(bbox, growcurrent)) {
			    return false;
			}
		    }
		    growcurrent = true;
		} else if (dynamic_cast<ON_SumSurface * >(const_cast<ON_Surface *>(surf)) != NULL) {
		    *sum_surface = *dynamic_cast<ON_SumSurface * >(const_cast<ON_Surface *>(surf));
		    if (sum_surface->Trim(0, domSplits[0][i]) && sum_surface->Trim(1, domSplits[1][j])) {
			if (!sum_surface->GetBoundingBox(bbox, growcurrent)) {
			    return false;
			}
		    }
		    growcurrent = true;
		} else if (dynamic_cast<ON_SurfaceProxy * >(const_cast<ON_Surface *>(surf)) != NULL) {
		    *proxy_surface = *dynamic_cast<ON_SurfaceProxy * >(const_cast<ON_Surface *>(surf));
		    if (proxy_surface->Trim(0, domSplits[0][i]) && proxy_surface->Trim(1, domSplits[1][j])) {
			if (!proxy_surface->GetBoundingBox(bbox, growcurrent)) {
			    return false;
			}
		    }
		    growcurrent = true;
		} else {
		    std::cerr << "Unknown Surface Type" << std::endl;
		}
	    }
	}
    }

    return true;
}


ON_BOOL32
face_GetBoundingBox(
	const ON_BrepFace &face,
        ON_BoundingBox& bbox,
	ON_BOOL32 bGrowBox
        )
{
    const ON_Surface *surf = face.SurfaceOf();

    // may be a smaller trimmed subset of surface so worth getting
    // face boundary
    bool growcurrent = bGrowBox;
    ON_3dPoint min, max;
    for (int li = 0; li < face.LoopCount(); li++) {
	for (int ti = 0; ti < face.Loop(li)->TrimCount(); ti++) {
	    ON_BrepTrim *trim = face.Loop(li)->Trim(ti);
	    trim->GetBoundingBox(min, max, growcurrent);
	    growcurrent = true;
	}
    }

    ON_Interval u_interval(min.x, max.x);
    ON_Interval v_interval(min.y, max.y);
    if (!surface_GetBoundingBox(surf, u_interval,v_interval,bbox,growcurrent)) {
	return false;
    }

    return true;
}


ON_BOOL32
surface_GetIntervalMinMaxDistance(
        const ON_3dPoint& p,
        ON_BoundingBox &bbox,
        double &min_distance,
        double &max_distance
        )
{
	min_distance = bbox.MinimumDistanceTo(p);
	max_distance = bbox.MaximumDistanceTo(p);
	return true;
}


ON_BOOL32
surface_GetIntervalMinMaxDistance(
	const ON_Surface *surf,
        const ON_3dPoint& p,
	const ON_Interval &u_interval,
	const ON_Interval &v_interval,
        double &min_distance,
        double &max_distance
        )
{
    ON_BoundingBox bbox;

    if (surface_GetBoundingBox(surf,u_interval,v_interval,bbox, false)) {
	min_distance = bbox.MinimumDistanceTo(p);

	max_distance = bbox.MaximumDistanceTo(p);
	return true;
    }
    return false;
}


double
surface_GetOptimalNormalUSplit(const ON_Surface *surf, const ON_Interval &u_interval, const ON_Interval &v_interval,double tol)
{
    ON_3dVector normal[4];
    double u = u_interval.Mid();

    if ((normal[0] = surf->NormalAt(u_interval.m_t[0],v_interval.m_t[0])) &&
	(normal[2] = surf->NormalAt(u_interval.m_t[0],v_interval.m_t[1]))) {
	double step = u_interval.Length()/2.0;
	double stepdir = 1.0;
	u = u_interval.m_t[0] + stepdir * step;

	while (step > tol) {
	    if ((normal[1] = surf->NormalAt(u,v_interval.m_t[0])) &&
		(normal[3] = surf->NormalAt(u,v_interval.m_t[1]))) {
		    double udot_1 = normal[0] * normal[1];
		    double udot_2 = normal[2] * normal[3];
		    if ((udot_1 < 0.0) || (udot_2 < 0.0)) {
			stepdir = -1.0;
		    } else {
			stepdir = 1.0;
		    }
		    step = step / 2.0;
		    u = u + stepdir * step;
	    }
	}
    }
    return u;
}


double
surface_GetOptimalNormalVSplit(const ON_Surface *surf, const ON_Interval &u_interval, const ON_Interval &v_interval,double tol)
{
    ON_3dVector normal[4];
    double v = v_interval.Mid();

    if ((normal[0] = surf->NormalAt(u_interval.m_t[0],v_interval.m_t[0])) &&
	(normal[1] = surf->NormalAt(u_interval.m_t[1],v_interval.m_t[0]))) {
	double step = v_interval.Length()/2.0;
	double stepdir = 1.0;
	v = v_interval.m_t[0] + stepdir * step;

	while (step > tol) {
	    if ((normal[2] = surf->NormalAt(u_interval.m_t[0],v)) &&
		(normal[3] = surf->NormalAt(u_interval.m_t[1],v))) {
		double vdot_1 = normal[0] * normal[2];
		double vdot_2 = normal[1] * normal[3];
		if ((vdot_1 < 0.0) || (vdot_2 < 0.0)) {
		    stepdir = -1.0;
		} else {
		    stepdir = 1.0;
		}
		step = step / 2.0;
		v = v + stepdir * step;
	    }
	}
    }
    return v;
}


//forward for cyclic
double surface_GetClosestPoint3dFirstOrderByRange(const ON_Surface *surf,const ON_3dPoint& p,const ON_Interval& u_range,
        const ON_Interval& v_range,double current_closest_dist,ON_2dPoint& p2d,ON_3dPoint& p3d,double same_point_tol, double within_distance_tol,int level);

double surface_GetClosestPoint3dFirstOrderSubdivision(const ON_Surface *surf,
        const ON_3dPoint& p, const ON_Interval &u_interval, double u, const ON_Interval &v_interval, double v,
        double current_closest_dist, ON_2dPoint& p2d, ON_3dPoint& p3d,
        double same_point_tol, double within_distance_tol, int level)
{
    double min_distance, max_distance;
    ON_Interval new_u_interval = u_interval;
    ON_Interval new_v_interval = v_interval;

    for (int iu = 0; iu < 2; iu++) {
	new_u_interval.m_t[iu] = u_interval.m_t[iu];
	new_u_interval.m_t[1 - iu] = u;
	for (int iv = 0; iv < 2; iv++) {
	    new_v_interval.m_t[iv] = v_interval.m_t[iv];
	    new_v_interval.m_t[1 - iv] = v;
	    if (surface_GetIntervalMinMaxDistance(surf, p, new_u_interval,new_v_interval, min_distance, max_distance)) {
		double distance = DBL_MAX;
		if ((min_distance < current_closest_dist) && NEAR_ZERO(min_distance,within_distance_tol)) {
		    /////////////////////////////////////////
		    // Could check normals and CV angles here
		    /////////////////////////////////////////
		    ON_3dVector normal[4];
		    if ((normal[0] = surf->NormalAt(new_u_interval.m_t[0],new_v_interval.m_t[0])) &&
			(normal[1] = surf->NormalAt(new_u_interval.m_t[1],new_v_interval.m_t[0])) &&
			(normal[2] = surf->NormalAt(new_u_interval.m_t[0],new_v_interval.m_t[1])) &&
			(normal[3] = surf->NormalAt(new_u_interval.m_t[1],new_v_interval.m_t[1]))) {

			    double udot_1 = normal[0] * normal[1];
			    double udot_2 = normal[2] * normal[3];
			    double vdot_1 = normal[0] * normal[2];
			    double vdot_2 = normal[1] * normal[3];

			    if ((udot_1 < 0.0) || (udot_2 < 0.0) || (vdot_1 < 0.0) || (vdot_2 < 0.0)) {
				double u_split,v_split;
				if ((udot_1 < 0.0) || (udot_2 < 0.0)) {
				    //get optimal U split
				    u_split = surface_GetOptimalNormalUSplit(surf,new_u_interval,new_v_interval,same_point_tol);
				} else {
				    u_split = new_u_interval.Mid();
				}
				if ((vdot_1 < 0.0) || (vdot_2 < 0.0)) {
				    //get optimal V split
				    v_split = surface_GetOptimalNormalVSplit(surf,new_u_interval,new_v_interval,same_point_tol);
				} else {
				    v_split = new_v_interval.Mid();
				}
				distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,new_u_interval,u_split,new_v_interval,v_split,current_closest_dist,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_closest_dist) {
				    current_closest_dist = distance;
				    if (current_closest_dist < same_point_tol)
					return current_closest_dist;
				}
			    } else {
				distance = surface_GetClosestPoint3dFirstOrderByRange(surf,p,new_u_interval,new_v_interval,current_closest_dist,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_closest_dist) {
				    current_closest_dist = distance;
				    if (current_closest_dist < same_point_tol)
					return current_closest_dist;
				}
			    }
		    }
		}
	    }
	}
    }
    return current_closest_dist;
}


double
surface_GetClosestPoint3dFirstOrderByRange(
	const ON_Surface *surf,
        const ON_3dPoint& p,
        const ON_Interval& u_range,
        const ON_Interval& v_range,
        double current_closest_dist,
        ON_2dPoint& p2d,
        ON_3dPoint& p3d,
        double same_point_tol,
        double within_distance_tol,
        int level
        )
{
    ON_3dPoint p0;
    ON_2dPoint p2d0;
    ON_3dVector ds, dt, dss, dst, dtt;
    ON_2dPoint working_p2d;
    ON_3dPoint working_p3d;
    ON_3dPoint P;
    ON_3dVector Ds, Dt, Dss, Dst, Dtt;
    bool notdone = true;
    double previous_distance = DBL_MAX;
    double distance;
    int errcnt = 0;

    p2d0.x = u_range.Mid();
    p2d0.y = v_range.Mid();

    while (notdone && (surf->Ev2Der(p2d0.x, p2d0.y, p0, ds, dt, dss, dst, dtt))) {
	if ((distance = p0.DistanceTo(p)) >= previous_distance) {
	    if (++errcnt <= 10) {
		p2d0 = (p2d0 + working_p2d) / 2.0;
		continue;
	    } else {
		///////////////////////////
		// Don't Subdivide just not getting any closer
		///////////////////////////
		/*
		double distance =
		        surface_GetClosestPoint3dFirstOrderSubdivision(surf, p,
		                u_range, u_range.Mid(), v_range, v_range.Mid(),
		                current_closest_dist, p2d, p3d, tol, level++);
		if (distance < current_closest_dist) {
		    current_closest_dist = distance;
		    if (current_closest_dist < tol)
			return current_closest_dist;
		}
		*/
		break;
	    }
	} else {
	    if (distance < current_closest_dist) {
		current_closest_dist = distance;
		p3d = p0;
		p2d = p2d0;
		if (current_closest_dist < same_point_tol)
		    return current_closest_dist;
	    }
	    previous_distance = distance;
	    working_p3d = p0;
	    working_p2d = p2d0;
	    errcnt = 0;
	}
	ON_3dVector N = ON_CrossProduct(ds, dt);
	N.Unitize();
	ON_Plane plane(p0, N);
	ON_3dPoint q = plane.ClosestPointTo(p);
	ON_2dVector pullback;
	ON_3dVector vector = q - p0;
	double vlength = vector.Length();

	if (vlength > 0.0) {
	    if (ON_Pullback3dVector(vector, 0.0, ds, dt, dss, dst, dtt,
		    pullback)) {
		p2d0 = p2d0 + pullback;
		if (!u_range.Includes(p2d0.x, false)) {
		    int i = (u_range.m_t[0] <= u_range.m_t[1]) ? 0 : 1;
		    p2d0.x =
			    (p2d0.x < u_range.m_t[i]) ?
			            u_range.m_t[i] : u_range.m_t[1 - i];
		}
		if (!v_range.Includes(p2d0.y, false)) {
		    int i = (v_range.m_t[0] <= v_range.m_t[1]) ? 0 : 1;
		    p2d0.y =
			    (p2d0.y < v_range.m_t[i]) ?
			            v_range.m_t[i] : v_range.m_t[1 - i];
		}
	    } else {
		///////////////////////////
		// Subdivide and go again
		///////////////////////////
		notdone = false;
		distance =
		        surface_GetClosestPoint3dFirstOrderSubdivision(surf, p,
		                u_range, u_range.Mid(), v_range, v_range.Mid(),
		                current_closest_dist, p2d, p3d, same_point_tol, within_distance_tol, level++);
		if (distance < current_closest_dist) {
		    current_closest_dist = distance;
		    if (current_closest_dist < same_point_tol)
			return current_closest_dist;
		}
		break;
	    }
	} else {
	    // can't get any closer
	    notdone = false;
	    break;
	}
    }
    if (previous_distance < current_closest_dist) {
	current_closest_dist = previous_distance;
	p3d = working_p3d;
	p2d = working_p2d;
    }

    return current_closest_dist;
}


bool surface_GetClosestPoint3dFirstOrder(
	const ON_Surface *surf,
        const ON_3dPoint& p,
        ON_2dPoint& p2d,
        ON_3dPoint& p3d,
        double &current_distance,
        int quadrant,	// optional - determines which quadrant to evaluate from
				//         0 = default
				//         1 from NE quadrant
				//         2 from NW quadrant
				//         3 from SW quadrant
				//         4 from SE quadrant
        double same_point_tol,
        double within_distance_tol
        )
{
    ON_3dPoint p0;
    ON_2dPoint p2d0;
    ON_3dVector ds, dt, dss, dst, dtt;
    ON_3dVector T, K;
    bool rc = false;

    static const ON_Surface *prev_surface = NULL;
    static int prev_u_spancnt = 0;
    static int u_spancnt = 0;
    static int v_spancnt = 0;
    static double *uspan = NULL;
    static double *vspan = NULL;
    static ON_BoundingBox **bbox = NULL;
    static double umid = 0.0;
    static int umid_index = 0;
    static double vmid = 0.0;
    static int vmid_index = 0;

    current_distance = DBL_MAX;

    int prec = std::cerr.precision();
    std::cerr.precision(15);


    if (prev_surface != surf) {
	if (uspan)
	    delete [] uspan;
	if (vspan)
	    delete [] vspan;
	if (bbox) {
	    for( int i = 0 ; i < (prev_u_spancnt + 2) ; i++ )
		delete [] bbox[i] ;
	    delete [] bbox;
	}
	u_spancnt = prev_u_spancnt = surf->SpanCount(0);
	v_spancnt = surf->SpanCount(1);
	// adding 2 here because going to divide at midpoint
	uspan = new double[u_spancnt + 2];
	vspan = new double[v_spancnt + 2];
	bbox = new ON_BoundingBox *[(u_spancnt + 2)];
	for( int i = 0 ; i < (u_spancnt + 2) ; i++ )
	    bbox[i] = new ON_BoundingBox [v_spancnt + 2];

	if (surf->GetSpanVector(0, uspan) && surf->GetSpanVector(1, vspan)) {
	    prev_surface = surf;
	    umid = surf->Domain(0).Mid();
	    umid_index = u_spancnt/2;
	    for (int u_span_index = 0; u_span_index < u_spancnt + 1;u_span_index++) {
		if (NEAR_EQUAL(uspan[u_span_index],umid,same_point_tol)) {
		    umid_index = u_span_index;
		    break;
		} else if (uspan[u_span_index] > umid) {
		    for (u_span_index = u_spancnt + 1; u_span_index > 0;u_span_index--) {
			if (uspan[u_span_index-1] < umid) {
			    uspan[u_span_index] = umid;
			    umid_index = u_span_index;
			    u_spancnt++;
			    u_span_index = u_spancnt+1;
			    break;
			} else {
			    uspan[u_span_index] = uspan[u_span_index-1];
			}
		    }
		}
	    }
	    vmid = surf->Domain(1).Mid();
	    vmid_index = v_spancnt/2;
	    for (int v_span_index = 0; v_span_index < v_spancnt + 1;v_span_index++) {
		if (NEAR_EQUAL(vspan[v_span_index],vmid,same_point_tol)) {
		    vmid_index = v_span_index;
		    break;
		} else if (vspan[v_span_index] > vmid) {
		    for (v_span_index = v_spancnt + 1; v_span_index > 0;v_span_index--) {
			if (vspan[v_span_index-1] < vmid) {
			    vspan[v_span_index] = vmid;
			    vmid_index = v_span_index;
			    v_spancnt++;
			    v_span_index = v_spancnt+1;
			    break;
			} else {
			    vspan[v_span_index] = vspan[v_span_index-1];
			}
		    }
		}
	    }
	    for (int u_span_index = 1; u_span_index < u_spancnt + 1;
		    u_span_index++) {
		for (int v_span_index = 1; v_span_index < v_spancnt + 1;
			v_span_index++) {
		    ON_Interval u_interval(uspan[u_span_index - 1],
			    uspan[u_span_index]);
		    ON_Interval v_interval(vspan[v_span_index - 1],
			    vspan[v_span_index]);

		    if (!surface_GetBoundingBox(surf,u_interval,v_interval,bbox[u_span_index-1][v_span_index-1], false)) {
			std::cerr << "Error computing bounding box for surface interval" << std::endl;
		    }
		}
	    }
	} else {
	    prev_surface = NULL;
	}
    }
    if (prev_surface == surf) {
	if (quadrant == 0) {
	    for (int u_span_index = 1; u_span_index < u_spancnt + 1;
		    u_span_index++) {
		for (int v_span_index = 1; v_span_index < v_spancnt + 1;
			v_span_index++) {
		    ON_Interval u_interval(uspan[u_span_index - 1],
			    uspan[u_span_index]);
		    ON_Interval v_interval(vspan[v_span_index - 1],
			    vspan[v_span_index]);
		    double min_distance,max_distance;

		    int level = 1;
		    if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
			    /////////////////////////////////////////
			    // Could check normals and CV angles here
			    /////////////////////////////////////////
			    double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
			    if (distance < current_distance) {
				current_distance = distance;
				if (current_distance < same_point_tol) {
				    rc = true;
				    goto cleanup;
				}
			    }
			}
		    }
		}
	    }
	    if (current_distance < within_distance_tol) {
		rc = true;
		goto cleanup;
	    }
	} else if (quadrant == 1) {
	    if (surf->IsClosed(0)) { //NE,SE,NW.SW
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    } else { //NE,NW,SW,SE
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    }
	    if (current_distance < within_distance_tol) {
		rc = true;
		goto cleanup;
	    }
	} else if (quadrant == 2) {
	    if (surf->IsClosed(0)) { // NW,SW,NE,SE
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    } else { // NW,NE,SW,SE
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    }
	    if (current_distance < within_distance_tol) {
		rc = true;
		goto cleanup;
	    }
	} else if (quadrant == 3) {
	    if (surf->IsClosed(0)) { // SW,NW,SE,NE
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    } else { // SW,SE,NW,NE
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    }
	    if (current_distance < within_distance_tol) {
		rc = true;
		goto cleanup;
	    }
	} else if (quadrant == 4) {
	    if (surf->IsClosed(0)) { // SE,NE,SW,NW
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    } else { // SE,SW,NE,NW
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
				uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
				vspan[v_span_index]);
			double min_distance,max_distance;

			int level = 1;
			if (surface_GetIntervalMinMaxDistance(p,bbox[u_span_index-1][v_span_index-1],min_distance,max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance,within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf,p,u_interval,u_interval.Mid(),v_interval,v_interval.Mid(),current_distance,p2d,p3d,same_point_tol,within_distance_tol,level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    }
	    if (current_distance < within_distance_tol) {
		rc = true;
		goto cleanup;
	    }
	}
    }
cleanup:
    std::cerr.precision(prec);
    return rc;
}


bool trim_GetClosestPoint3dFirstOrder(
	const ON_BrepTrim& trim,
        const ON_3dPoint& p,
        ON_2dPoint& p2d,
        double& t,
        double& distance,
        const ON_Interval* interval,
        double same_point_tol,
        double within_distance_tol
        )
{
    bool rc = false;
    const ON_Surface *surf = trim.SurfaceOf();

    double t0 = interval->Mid();
    ON_3dPoint p3d;
    ON_3dPoint p0;
    ON_3dVector ds,dt,dss,dst,dtt;
    ON_3dVector T,K;
    int prec = std::cerr.precision();
    ON_BoundingBox tight_bbox;
    std::vector<ON_BoundingBox> bbox;
    std::cerr.precision(15);

    ON_Curve *c = trim.Brep()->m_C2[trim.m_c2i];
    ON_NurbsCurve N;
    if ( 0 == c->GetNurbForm(N) )
      return false;
    if ( N.m_order < 2 || N.m_cv_count < N.m_order )
      return false;

    p2d = trim.PointAt(t);
    int quadrant = 0;     // optional - determines which quadrant to evaluate from
                          //         0 = default
                          //         1 from NE quadrant
                          //         2 from NW quadrant
                          //         3 from SW quadrant
                          //         4 from SE quadrant
    ON_Interval u_interval = surf->Domain(0);
    ON_Interval v_interval = surf->Domain(1);
    if (p2d.y > v_interval.Mid()) {
	// North quadrants -> 1 or 2;
	if (p2d.x > u_interval.Mid()) {
	    quadrant = 1; // NE
	} else {
	    quadrant = 2; //NW
	}
    } else {
	// South quadrants -> 3 or 4;
	if (p2d.x > u_interval.Mid()) {
	    quadrant = 4; // SE
	} else {
	    quadrant = 3; //SW
	}
    }
    if (surface_GetClosestPoint3dFirstOrder(surf,p,p2d,p3d,distance,quadrant,same_point_tol,within_distance_tol)) {
	ON_BezierCurve B;
	bool bGrowBox = false;
	ON_3dVector d1,d2;
	double max_dist_to_closest_pt = DBL_MAX;
	ON_Interval *span_interval = new ON_Interval[N.m_cv_count - N.m_order + 1];
	double *min_distance = new double[N.m_cv_count - N.m_order + 1];
	double *max_distance = new double[N.m_cv_count - N.m_order + 1];
	bool *skip = new bool[N.m_cv_count - N.m_order + 1];
	bbox.resize(N.m_cv_count - N.m_order + 1);
	for ( int span_index = 0; span_index <= N.m_cv_count - N.m_order; span_index++ )
	{
	    skip[span_index] = true;
	  if ( !(N.m_knot[span_index + N.m_order-2] < N.m_knot[span_index + N.m_order-1]) )
	    continue;

	  // check for span out of interval
	  int i = (interval->m_t[0] <= interval->m_t[1]) ? 0 : 1;
	  if ( N.m_knot[span_index + N.m_order-2] > interval->m_t[1-i] )
	    continue;
	  if ( N.m_knot[span_index + N.m_order-1] < interval->m_t[i] )
	    continue;

	  if ( !N.ConvertSpanToBezier( span_index, B ) )
	    continue;
	  ON_Interval bi = B.Domain();
	  if ( !B.GetTightBoundingBox(tight_bbox,bGrowBox,NULL) )
	    continue;
	  bbox[span_index] = tight_bbox;
	  d1 = tight_bbox.m_min - p2d;
	  d2 = tight_bbox.m_max - p2d;
	  min_distance[span_index] = tight_bbox.MinimumDistanceTo(p2d);

	  if (min_distance[span_index] > max_dist_to_closest_pt) {
	      max_distance[span_index] = DBL_MAX;
	      continue;
	  }
	  skip[span_index] = false;
	  span_interval[span_index].m_t[0] = ((N.m_knot[span_index + N.m_order-2]) < interval->m_t[i]) ? interval->m_t[i] : N.m_knot[span_index + N.m_order-2];
	  span_interval[span_index].m_t[1] = ((N.m_knot[span_index + N.m_order-1]) > interval->m_t[1 -i]) ? interval->m_t[1 -i] : (N.m_knot[span_index + N.m_order-1]);
	  ON_3dPoint d1sq(d1.x*d1.x,d1.y*d1.y,0.0),d2sq(d2.x*d2.x,d2.y*d2.y,0.0);
	  double distancesq;
	  if (d1sq.x < d2sq.x) {
	    if (d1sq.y < d2sq.y) {
		if ((d1sq.x + d2sq.y) < (d2sq.x + d1sq.y)) {
		    distancesq = d1sq.x + d2sq.y;
		} else {
		    distancesq = d2sq.x + d1sq.y;
		}
	    } else {
		if ((d1sq.x + d1sq.y) < (d2sq.x + d2sq.y)) {
		    distancesq = d1sq.x + d1sq.y;
		} else {
		    distancesq = d2sq.x + d2sq.y;
		}
	    }
	  } else {
	    if (d1sq.y < d2sq.y) {
		if ((d1sq.x + d1sq.y) < (d2sq.x + d2sq.y)) {
		    distancesq = d1sq.x + d1sq.y;
		} else {
		    distancesq = d2sq.x + d2sq.y;
		}
	    } else {
		if ((d1sq.x + d2sq.y) < (d2sq.x + d1sq.y)) {
		    distancesq = d1sq.x + d2sq.y;
		} else {
		    distancesq = d2sq.x + d1sq.y;
		}
	    }
	  }
	  max_distance[span_index] = sqrt(distancesq);
	  if (max_distance[span_index] < max_dist_to_closest_pt) {
	      max_dist_to_closest_pt = max_distance[span_index];
	  }
	  if (max_distance[span_index] < min_distance[span_index]) {
	      // should only be here for near equal fuzz
	      min_distance[span_index] = max_distance[span_index];
	  }
	}
	for ( int span_index = 0; span_index <= N.m_cv_count - N.m_order; span_index++ )
	{

	  if ( skip[span_index] )
	    continue;

	  if (min_distance[span_index] > max_dist_to_closest_pt) {
	      skip[span_index] = true;
	      continue;
	  }

	}

	ON_3dPoint q;
	ON_3dPoint point;
	double closest_distance = DBL_MAX;
	double closestT = DBL_MAX;
	for ( int span_index = 0; span_index <= N.m_cv_count - N.m_order; span_index++ )
	{
	    if (skip[span_index]) {
		continue;
	    }
	    t0 = span_interval[span_index].Mid();
	    bool closestfound = false;
	    bool notdone = true;
	    double distance = DBL_MAX;
	    double previous_distance = DBL_MAX;
	    ON_3dVector firstDervative, secondDervative;
	    while (notdone
		    && trim.Ev2Der(t0, point, firstDervative, secondDervative)
		    && ON_EvCurvature(firstDervative, secondDervative, T, K)) {
		ON_Line line(point, point + 100.0 * T);
		q = line.ClosestPointTo(p2d);
		double delta_t = (firstDervative * (q - point))
		        / (firstDervative * firstDervative);
		double new_t0 = t0 + delta_t;
		if (!span_interval[span_index].Includes(new_t0, false)) {
		    // limit to interval
		    int i = (span_interval[span_index].m_t[0] <= span_interval[span_index].m_t[1]) ? 0 : 1;
		    new_t0 =
			    (new_t0 < span_interval[span_index].m_t[i]) ?
				    span_interval[span_index].m_t[i] : span_interval[span_index].m_t[1 - i];
		}
		delta_t = new_t0 - t0;
		t0 = new_t0;
		point = trim.PointAt(t0);
		distance = point.DistanceTo(p2d);
		if (distance < previous_distance) {
		    closestfound = true;
		    closestT = t0;
		    previous_distance = distance;
		    if (fabs(delta_t) < same_point_tol) {
			notdone = false;
		    }
		} else {
		    notdone = false;
		}
	    }
	    if (closestfound && (distance < closest_distance)) {
		closest_distance = distance;
		rc = true;
		t = closestT;
	    }
	}
	delete [] span_interval;
	delete [] min_distance;
	delete [] max_distance;
	delete [] skip;

    }
    std::cerr.precision(prec);

    return rc;
}


bool
toUV(PBCData& data, ON_2dPoint& out_pt, double t, double knudge = 0.0)
{
    ON_3dPoint pointOnCurve = data.curve->PointAt(t);
    ON_3dPoint knudgedPointOnCurve = data.curve->PointAt(t + knudge);

    ON_2dPoint uv;
    if (data.surftree->getSurfacePoint((const ON_3dPoint&)pointOnCurve, uv, (const ON_3dPoint&)knudgedPointOnCurve, BREP_EDGE_MISS_TOLERANCE) > 0) {
	out_pt.Set(uv.x, uv.y);
	return true;
    } else {
	return false;
    }
}


bool
toUV(brlcad::SurfaceTree *surftree, const ON_Curve *curve, ON_2dPoint& out_pt, double t, double knudge = 0.0)
{
    if (!surftree)
	return false;

    const ON_Surface *surf = surftree->getSurface();
    if (!surf)
	return false;

    ON_3dPoint pointOnCurve = curve->PointAt(t);
    ON_3dPoint knudgedPointOnCurve = curve->PointAt(t + knudge);
    ON_3dVector dt;
    curve->Ev1Der(t, pointOnCurve, dt);
    ON_3dVector tangent = curve->TangentAt(t);
    //data.surf->GetClosestPoint(pointOnCurve, &a, &b, 0.0001);
    ON_Ray r(pointOnCurve, tangent);
    plane_ray pr;
    brep_get_plane_ray(r, pr);
    ON_3dVector p1;
    double p1d;
    ON_3dVector p2;
    double p2d;

    utah_ray_planes(r, p1, p1d, p2, p2d);

    VMOVE(pr.n1, p1);
    pr.d1 = p1d;
    VMOVE(pr.n2, p2);
    pr.d2 = p2d;

    try {
	pt2d_t uv;
	ON_2dPoint uv2d = surftree->getClosestPointEstimate(knudgedPointOnCurve);
	move(uv, uv2d);

	ON_3dVector dir = surf->NormalAt(uv[0], uv[1]);
	dir.Reverse();
	ON_Ray ray(pointOnCurve, dir);
	brep_get_plane_ray(ray, pr);
	//know use this as guess to iterate to closer solution
	pt2d_t Rcurr;
	pt2d_t new_uv;
	ON_3dPoint pt;
	ON_3dVector su, sv;
#ifdef SHOW_UNUSED
	bool found = false;
#endif
	fastf_t Dlast = MAX_FASTF;
	for (int i = 0; i < 10; i++) {
	    brep_r(surf, pr, uv, pt, su, sv, Rcurr);
	    fastf_t d = v2mag(Rcurr);
	    if (d < BREP_INTERSECTION_ROOT_EPSILON) {
		TRACE1("R:" << ON_PRINT2(Rcurr));
#ifdef SHOW_UNUSED
		found = true;
		break;
#endif
	    } else if (d > Dlast) {
#ifdef SHOW_UNUSED
		found = false; //break;
#endif
		break;
		//return brep_edge_check(found, sbv, face, surf, ray, hits);
	    }
	    brep_newton_iterate(pr, Rcurr, su, sv, uv, new_uv);
	    move(uv, new_uv);
	    Dlast = d;
	}

///////////////////////////////////////
	out_pt.Set(uv[0], uv[1]);
	return true;
    } catch (...) {
	return false;
    }
}


double
randomPointFromRange(PBCData& data, ON_2dPoint& out, double lo, double hi)
{
    assert(lo < hi);
    double random_pos = drand48() * (RANGE_HI - RANGE_LO) + RANGE_LO;
    double newt = random_pos * (hi - lo) + lo;
    assert(toUV(data, out, newt));
    return newt;
}


bool
sample(PBCData& data,
       double t1,
       double t2,
       const ON_2dPoint& p1,
       const ON_2dPoint& p2)
{
    ON_2dPoint m;
    double t = randomPointFromRange(data, m, t1, t2);
    if (!data.segments.empty()) {
	ON_2dPointArray * samples = data.segments.back();
	if (isFlat(p1, m, p2, data.flatness)) {
	    samples->Append(p2);
	} else {
	    sample(data, t1, t, p1, m);
	    sample(data, t, t2, m, p2);
	}
	return true;
    }
    return false;
}


void
generateKnots(BSpline& bspline)
{
    int num_knots = bspline.m + 1;
    bspline.knots.resize(num_knots);
    for (int i = 0; i <= bspline.p; i++) {
	bspline.knots[i] = 0.0;
    }
    for (int i = bspline.m - bspline.p; i <= bspline.m; i++) {
	bspline.knots[i] = 1.0;
    }
    for (int i = 1; i <= bspline.n - bspline.p; i++) {
	bspline.knots[bspline.p + i] = (double)i / (bspline.n - bspline.p + 1.0);
    }
}


int
getKnotInterval(BSpline& bspline, double u)
{
    int k = 0;
    while (u >= bspline.knots[k]) k++;
    k = (k == 0) ? k : k - 1;
    return k;
}

ON_NurbsCurve*
interpolateLocalCubicCurve(ON_2dPointArray &Q)
{
    int num_samples = Q.Count();
    int num_segments = Q.Count() - 1;
    int qsize = num_samples + 4;
    std::vector < ON_2dVector > qarray(qsize);

    for (int i = 1; i < Q.Count(); i++) {
	qarray[i + 1] = Q[i] - Q[i - 1];
    }
    qarray[1] = 2.0 * qarray[2] - qarray[3];
    qarray[0] = 2.0 * qarray[1] - qarray[2];

    qarray[num_samples + 1] = 2 * qarray[num_samples] - qarray[num_samples - 1];
    qarray[num_samples + 2] = 2 * qarray[num_samples + 1] - qarray[num_samples];
    qarray[num_samples + 3] = 2 * qarray[num_samples + 2] - qarray[num_samples + 1];

    std::vector < ON_2dVector > T(num_samples);
    std::vector<double> A(num_samples);
    for (int k = 0; k < num_samples; k++) {
	ON_3dVector a = ON_CrossProduct(qarray[k], qarray[k + 1]);
	ON_3dVector b = ON_CrossProduct(qarray[k + 2], qarray[k + 3]);
	double alength = a.Length();
	if (NEAR_ZERO(alength, PBC_TOL)) {
	    A[k] = 1.0;
	} else {
	    A[k] = (a.Length()) / (a.Length() + b.Length());
	}
	T[k] = (1.0 - A[k]) * qarray[k + 1] + A[k] * qarray[k + 2];
	T[k].Unitize();
    }
    std::vector < ON_2dPointArray > P(num_samples - 1);
    ON_2dPointArray control_points;
    control_points.Append(Q[0]);
    for (int i = 1; i < num_samples; i++) {
	ON_2dPoint P0 = Q[i - 1];
	ON_2dPoint P3 = Q[i];
	ON_2dVector T0 = T[i - 1];
	ON_2dVector T3 = T[i];

	double a, b, c;

	ON_2dVector vT0T3 = T0 + T3;
	ON_2dVector dP0P3 = P3 - P0;
	a = 16.0 - vT0T3.Length() * vT0T3.Length();
	b = 12.0 * (dP0P3 * vT0T3);
	c = -36.0 * dP0P3.Length() * dP0P3.Length();

	double alpha = (-b + sqrt(b * b - 4.0 * a * c)) / (2.0 * a);

	ON_2dPoint P1 = P0 + (1.0 / 3.0) * alpha * T0;
	control_points.Append(P1);
	ON_2dPoint P2 = P3 - (1.0 / 3.0) * alpha * T3;
	control_points.Append(P2);
	P[i - 1].Append(P0);
	P[i - 1].Append(P1);
	P[i - 1].Append(P2);
	P[i - 1].Append(P3);
    }
    control_points.Append(Q[num_samples - 1]);

    std::vector<double> u(num_segments + 1);
    u[0] = 0.0;
    for (int k = 0; k < num_segments; k++) {
	u[k + 1] = u[k] + 3.0 * (P[k][1] - P[k][0]).Length();
    }
    int degree = 3;
    int n = control_points.Count();
    int p = degree;
    int m = n + p - 1;
    int dimension = 2;
    ON_NurbsCurve* c = ON_NurbsCurve::New(dimension, false, degree + 1, n);
    c->ReserveKnotCapacity(m);
    for (int i = 0; i < degree; i++) {
	c->SetKnot(i, 0.0);
    }
    for (int i = 1; i < num_segments; i++) {
	double knot_value = u[i] / u[num_segments];
	c->SetKnot(degree + 2 * (i - 1), knot_value);
	c->SetKnot(degree + 2 * (i - 1) + 1, knot_value);
    }
    for (int i = m - p; i < m; i++) {
	c->SetKnot(i, 1.0);
    }

    // insert the control points
    for (int i = 0; i < n; i++) {
	ON_3dPoint pnt = control_points[i];
	c->SetCV(i, pnt);
    }
    return c;
}

ON_NurbsCurve*
interpolateLocalCubicCurve(const ON_3dPointArray &Q)
{
    int num_samples = Q.Count();
    int num_segments = Q.Count() - 1;
    int qsize = num_samples + 3;
    std::vector<ON_3dVector> qarray(qsize + 1);
    ON_3dVector *q = &qarray[1];

    for (int i = 1; i < Q.Count(); i++) {
	q[i] = Q[i] - Q[i - 1];
    }
    q[0] = 2.0 * q[1] - q[2];
    q[-1] = 2.0 * q[0] - q[1];

    q[num_samples] = 2 * q[num_samples - 1] - q[num_samples - 2];
    q[num_samples + 1] = 2 * q[num_samples] - q[num_samples - 1];
    q[num_samples + 2] = 2 * q[num_samples + 1] - q[num_samples];

    std::vector<ON_3dVector> T(num_samples);
    std::vector<double> A(num_samples);
    for (int k = 0; k < num_samples; k++) {
	ON_3dVector avec = ON_CrossProduct(q[k - 1], q[k]);
	ON_3dVector bvec = ON_CrossProduct(q[k + 1], q[k + 2]);
	double alength = avec.Length();
	if (NEAR_ZERO(alength, PBC_TOL)) {
	    A[k] = 1.0;
	} else {
	    A[k] = (avec.Length()) / (avec.Length() + bvec.Length());
	}
	T[k] = (1.0 - A[k]) * q[k] + A[k] * q[k + 1];
	T[k].Unitize();
    }
    std::vector<ON_3dPointArray> P(num_samples - 1);
    ON_3dPointArray control_points;
    control_points.Append(Q[0]);
    for (int i = 1; i < num_samples; i++) {
	ON_3dPoint P0 = Q[i - 1];
	ON_3dPoint P3 = Q[i];
	ON_3dVector T0 = T[i - 1];
	ON_3dVector T3 = T[i];

	double a, b, c;

	ON_3dVector vT0T3 = T0 + T3;
	ON_3dVector dP0P3 = P3 - P0;
	a = 16.0 - vT0T3.Length() * vT0T3.Length();
	b = 12.0 * (dP0P3 * vT0T3);
	c = -36.0 * dP0P3.Length() * dP0P3.Length();

	double alpha = (-b + sqrt(b * b - 4.0 * a * c)) / (2.0 * a);

	ON_3dPoint P1 = P0 + (1.0 / 3.0) * alpha * T0;
	control_points.Append(P1);
	ON_3dPoint P2 = P3 - (1.0 / 3.0) * alpha * T3;
	control_points.Append(P2);
	P[i - 1].Append(P0);
	P[i - 1].Append(P1);
	P[i - 1].Append(P2);
	P[i - 1].Append(P3);
    }
    control_points.Append(Q[num_samples - 1]);

    std::vector<double> u(num_segments + 1);
    u[0] = 0.0;
    for (int k = 0; k < num_segments; k++) {
	u[k + 1] = u[k] + 3.0 * (P[k][1] - P[k][0]).Length();
    }
    int degree = 3;
    int n = control_points.Count();
    int p = degree;
    int m = n + p - 1;
    int dimension = 3;
    ON_NurbsCurve* c = ON_NurbsCurve::New(dimension, false, degree + 1, n);
    c->ReserveKnotCapacity(m);
    for (int i = 0; i < degree; i++) {
	c->SetKnot(i, 0.0);
    }
    for (int i = 1; i < num_segments; i++) {
	double knot_value = u[i] / u[num_segments];
	c->SetKnot(degree + 2 * (i - 1), knot_value);
	c->SetKnot(degree + 2 * (i - 1) + 1, knot_value);
    }
    for (int i = m - p; i < m; i++) {
	c->SetKnot(i, 1.0);
    }

    // insert the control points
    for (int i = 0; i < n; i++) {
	ON_3dPoint pnt = control_points[i];
	c->SetCV(i, pnt);
    }
    return c;
}


ON_NurbsCurve*
newNURBSCurve(BSpline& spline, int dimension = 3)
{
    // we now have everything to complete our spline
    ON_NurbsCurve* c = ON_NurbsCurve::New(dimension,
					  false,
					  spline.p + 1,
					  spline.n + 1);
    c->ReserveKnotCapacity(spline.knots.size() - 2);
    for (unsigned int i = 1; i < spline.knots.size() - 1; i++) {
	c->m_knot[i - 1] = spline.knots[i];
    }

    for (int i = 0; i < spline.controls.Count(); i++) {
	c->SetCV(i, ON_3dPoint(spline.controls[i]));
    }

    return c;
}


ON_Curve*
interpolateCurve(ON_2dPointArray &samples)
{
    ON_NurbsCurve* nurbs;

    if (samples.Count() == 2)
	// build a line
	return new ON_LineCurve(samples[0], samples[1]);

    // local vs. global interpolation for large point sampled curves
    nurbs = interpolateLocalCubicCurve(samples);

    return nurbs;
}


int
IsAtSeam(const ON_Surface *surf, int dir, double u, double v, double tol)
{
    int rc = 0;
    if (!surf->IsClosed(dir))
	return rc;

    double p = (dir) ? v : u;
    if (NEAR_EQUAL(p, surf->Domain(dir)[0], tol) || NEAR_EQUAL(p, surf->Domain(dir)[1], tol))
	rc += (dir + 1);

    return rc;
}

/*
 *  Similar to openNURBS's surf->IsAtSeam() function but uses tolerance to do a near check versus
 *  the floating point equality used by openNURBS.
 * rc = 0 Not on seam, 1 on East/West seam(umin/umax), 2 on North/South seam(vmin/vmax), 3 seam on both U/V boundaries
 */
int
IsAtSeam(const ON_Surface *surf, double u, double v, double tol)
{
    int rc = 0;
    int i;
    for (i = 0; i < 2; i++) {
	rc += IsAtSeam(surf, i, u, v, tol);
    }

    return rc;
}

int
IsAtSeam(const ON_Surface *surf, int dir, const ON_2dPoint &pt, double tol)
{
    int rc = 0;
    ON_2dPoint unwrapped_pt = UnwrapUVPoint(surf,pt,tol);
    rc = IsAtSeam(surf,dir,unwrapped_pt.x,unwrapped_pt.y,tol);

    return rc;
}

/*
 *  Similar to IsAtSeam(surf,u,v,tol) function but takes a ON_2dPoint
 *  and unwraps any closed seam extents before passing on IsAtSeam(surf,u,v,tol)
 */
int
IsAtSeam(const ON_Surface *surf, const ON_2dPoint &pt, double tol)
{
    int rc = 0;
    ON_2dPoint unwrapped_pt = UnwrapUVPoint(surf,pt,tol);
    rc = IsAtSeam(surf,unwrapped_pt.x,unwrapped_pt.y,tol);

    return rc;
}


int
IsAtSingularity(const ON_Surface *surf, double u, double v, double tol)
{
    // 0 = south, 1 = east, 2 = north, 3 = west
    //std::cerr << "IsAtSingularity = u, v - " << u << ", " << v << std::endl;
    //std::cerr << "surf->Domain(0) - " << surf->Domain(0)[0] << ", " << surf->Domain(0)[1] << std::endl;
    //std::cerr << "surf->Domain(1) - " << surf->Domain(1)[0] << ", " << surf->Domain(1)[1] << std::endl;
    if (NEAR_EQUAL(u, surf->Domain(0)[0], tol)) {
	if (surf->IsSingular(3))
	    return 3;
    } else if (NEAR_EQUAL(u, surf->Domain(0)[1], tol)) {
	if (surf->IsSingular(1))
	    return 1;
    }
    if (NEAR_EQUAL(v, surf->Domain(1)[0], tol)) {
	if (surf->IsSingular(0))
	    return 0;
    } else if (NEAR_EQUAL(v, surf->Domain(1)[1], tol)) {
	if (surf->IsSingular(2))
	    return 2;
    }
    return -1;
}

int
IsAtSingularity(const ON_Surface *surf, const ON_2dPoint &pt, double tol)
{
    int rc = 0;
    ON_2dPoint unwrapped_pt = UnwrapUVPoint(surf,pt,tol);
    rc = IsAtSingularity(surf,unwrapped_pt.x,unwrapped_pt.y,tol);

    return rc;
}

ON_2dPointArray *
pullback_samples(PBCData* data,
		 double t,
		 double s)
{
    if (!data)
	return NULL;

    const ON_Curve* curve = data->curve;
    const ON_Surface* surf = data->surf;
    ON_2dPointArray *samples = new ON_2dPointArray();
    int numKnots = curve->SpanCount();
    double *knots = new double[numKnots + 1];
    curve->GetSpanVector(knots);

    int istart = 0;
    while (t >= knots[istart])
	istart++;

    if (istart > 0) {
	istart--;
	knots[istart] = t;
    }

    int istop = numKnots;
    while (s <= knots[istop])
	istop--;

    if (istop < numKnots) {
	istop++;
	knots[istop] = s;
    }

    int samplesperknotinterval;
    int degree = curve->Degree();

    if (degree > 1) {
	samplesperknotinterval = 3 * degree;
    } else {
	samplesperknotinterval = 18 * degree;
    }
    ON_2dPoint pt;
    ON_3dPoint p = ON_3dPoint::UnsetPoint;
    ON_3dPoint p3d = ON_3dPoint::UnsetPoint;
    double distance;
    for (int i = istart; i <= istop; i++) {
	if (i <= numKnots / 2) {
	    if (i > 0) {
		double delta = (knots[i] - knots[i - 1]) / (double) samplesperknotinterval;
		for (int j = 1; j < samplesperknotinterval; j++) {
		    p = curve->PointAt(knots[i - 1] + j * delta);
		    p3d = ON_3dPoint::UnsetPoint;
		    if (surface_GetClosestPoint3dFirstOrder(surf,p,pt,p3d,distance,0,BREP_SAME_POINT_TOLERANCE,BREP_EDGE_MISS_TOLERANCE)) {
			samples->Append(pt);
		    }
		}
	    }
	    p = curve->PointAt(knots[i]);
	    p3d = ON_3dPoint::UnsetPoint;
	    if (surface_GetClosestPoint3dFirstOrder(surf,p,pt,p3d,distance,0,BREP_SAME_POINT_TOLERANCE,BREP_EDGE_MISS_TOLERANCE)) {
		samples->Append(pt);
	    }
	} else {
	    if (i > 0) {
		double delta = (knots[i] - knots[i - 1]) / (double) samplesperknotinterval;
		for (int j = 1; j < samplesperknotinterval; j++) {
		    p = curve->PointAt(knots[i - 1] + j * delta);
		    p3d = ON_3dPoint::UnsetPoint;
		    if (surface_GetClosestPoint3dFirstOrder(surf,p,pt,p3d,distance,0,BREP_SAME_POINT_TOLERANCE,BREP_EDGE_MISS_TOLERANCE)) {
			samples->Append(pt);
		    }
		}
		p = curve->PointAt(knots[i]);
		p3d = ON_3dPoint::UnsetPoint;
		if (surface_GetClosestPoint3dFirstOrder(surf,p,pt,p3d,distance,0,BREP_SAME_POINT_TOLERANCE,BREP_EDGE_MISS_TOLERANCE)) {
		    samples->Append(pt);
		}
	    }
	}
    }
    delete[] knots;
    return samples;
}


/*
 *  Unwrap 2D UV point values to within actual surface UV. Points often wrap around the closed seam.
 */
ON_2dPoint
UnwrapUVPoint(const ON_Surface *surf,const ON_2dPoint &pt, double tol)
{
    ON_2dPoint p = pt;
    for (int i=0; i<2; i++) {
      if (!surf->IsClosed(i))
        continue;
      while (p[i] < surf->Domain(i).m_t[0] - tol) {
	  double length = surf->Domain(i).Length();
	  if (i<=0) {
	      p.x = p.x + length;
	  } else {
	      p.y = p.y + length;
	  }
      }
      while (p[i] >= surf->Domain(i).m_t[1] + tol) {
	  double length = surf->Domain(i).Length();
	  if (i<=0) {
	      p.x = p.x - length;
	  } else {
	      p.y = p.y - length;
	  }
      }
    }

    return p;
}


double
DistToNearestClosedSeam(const ON_Surface *surf,const ON_2dPoint &pt)
{
    double dist = -1.0;
    ON_2dPoint unwrapped_pt = UnwrapUVPoint(surf,pt);
    for (int i=0; i<2; i++) {
      if (!surf->IsClosed(i))
        continue;
      dist = fabs(unwrapped_pt[i] - surf->Domain(i)[0]);
      V_MIN(dist,fabs(surf->Domain(i)[1]-unwrapped_pt[i]));
    }
    return dist;
}


void
GetClosestExtendedPoint(const ON_Surface *surf,ON_2dPoint &pt,ON_2dPoint &prev_pt, double tol) {
    if (surf->IsClosed(0)) {
	double length = surf->Domain(0).Length();
	double delta=pt.x-prev_pt.x;
	while (fabs(delta) > length/2.0) {
	    if (delta > length/2.0) {
		pt.x = pt.x - length;
		delta=pt.x-prev_pt.x;
	    } else {
		pt.x = pt.x + length;
		delta=pt.x - prev_pt.x;
	    }
	}
    }

    if (surf->IsClosed(1)) {
	double length = surf->Domain(1).Length();
	double delta=pt.y-prev_pt.y;
	while (fabs(delta) > length/2.0) {
	    if (delta > length/2.0) {
		pt.y = pt.y - length;
		delta=pt.y - prev_pt.y;
	    } else {
		pt.y = pt.y + length;
		delta=pt.y -prev_pt.y;
	    }
	}
    }
}


/*
 *  Simple check to determine if two consecutive points pulled back from 3d curve sampling
 *  to 2d UV parameter space crosses the seam of the closed UV. The assumption here is that
 *  the sampling of the 3d curve is a small fraction of the UV domain.
 *
 *  // dir  - 0 = not crossing, 1 = south/east bound, 2 = north/west bound
 */
bool
ConsecutivePointsCrossClosedSeam(const ON_Surface *surf,const ON_2dPoint &pt,const ON_2dPoint &prev_pt, int &udir, int &vdir, double tol)
{
    bool rc = false;

    /*
     * if one of the points is at a seam then not crossing
     */
    int dir =0;
    ON_2dPoint unwrapped_pt = UnwrapUVPoint(surf,pt);
    ON_2dPoint unwrapped_prev_pt = UnwrapUVPoint(surf,prev_pt);

    if (!IsAtSeam(surf,dir,pt,tol) && !IsAtSeam(surf,dir,prev_pt,tol)) {
	udir = vdir = 0;
	if (surf->IsClosed(0)) {
	    double delta=unwrapped_pt.x-unwrapped_prev_pt.x;
	    if (fabs(delta) > surf->Domain(0).Length()/2.0) {
		if (delta < 0.0) {
		    udir = 1; // east bound
		} else {
		    udir= 2; // west bound
		}
		rc = true;
	    }
	}
    }
    dir = 1;
    if (!IsAtSeam(surf,dir,pt,tol) && !IsAtSeam(surf,dir,prev_pt,tol)) {
	if (surf->IsClosed(1)) {
	    double delta=unwrapped_pt.y-unwrapped_prev_pt.y;
	    if (fabs(delta) > surf->Domain(1).Length()/2.0) {
		if (delta < 0.0) {
		    vdir = 2; // north bound
		} else {
		    vdir= 1; // south bound
		}
		rc = true;
	    }
	}
    }

    return rc;
}

/*
 *  If within UV tolerance to a seam force force to actually seam value so surface
 *  seam function can be used.
 */
void
ForceToClosestSeam(const ON_Surface *surf, ON_2dPoint &pt, double tol)
{
    int seam;
    ON_2dPoint unwrapped_pt = UnwrapUVPoint(surf,pt,tol);
    ON_2dVector wrap = ON_2dVector::ZeroVector;
    ON_Interval dom[2] = { ON_Interval::EmptyInterval, ON_Interval::EmptyInterval };
    double length[2] = { ON_UNSET_VALUE, ON_UNSET_VALUE};

    for (int i=0; i<2; i++) {
	dom[i] = surf->Domain(i);
	length[i] = dom[i].Length();
	if (!surf->IsClosed(i))
	    continue;
	if (pt[i] > (dom[i].m_t[1] + tol)) {
	  ON_2dPoint p = pt;
	  while (p[i] > (dom[i].m_t[1] + tol)) {
	      p[i] -= length[i];
	      wrap[i] += 1.0;
	  }
	} else if (pt[i] < (dom[i].m_t[0] - tol)) {
	  ON_2dPoint p = pt;
	  while (p[i] < (dom[i].m_t[0] - tol)) {
	      p[i] += length[i];
	      wrap[i] -= 1.0;
	  }
	}
	wrap[i] = wrap[i] * length[i];
    }

    if ((seam=IsAtSeam(surf, unwrapped_pt, tol)) > 0) {
	if (seam == 1) { // east/west seam
	    if (fabs(unwrapped_pt.x - dom[0].m_t[0]) < length[0]/2.0) {
		unwrapped_pt.x = dom[0].m_t[0]; // on east swap to west seam
	    } else {
		unwrapped_pt.x = dom[0].m_t[1]; // on west swap to east seam
	    }
	} else if (seam == 2) { // north/south seam
	    if (fabs(unwrapped_pt.y - dom[1].m_t[0]) < length[1]/2.0) {
		unwrapped_pt.y = dom[1].m_t[0]; // on north swap to south seam
	    } else {
		unwrapped_pt.y = dom[1].m_t[1]; // on south swap to north seam
	    }
	} else { //on both seams
	    if (fabs(unwrapped_pt.x - dom[0].m_t[0]) < length[0]/2.0) {
		unwrapped_pt.x = dom[0].m_t[0]; // on east swap to west seam
	    } else {
		unwrapped_pt.x = dom[0].m_t[1]; // on west swap to east seam
	    }
	    if (fabs(pt.y - dom[1].m_t[0]) < length[1]/2.0) {
		unwrapped_pt.y = dom[1].m_t[0]; // on north swap to south seam
	    } else {
		unwrapped_pt.y = dom[1].m_t[1]; // on south swap to north seam
	    }
	}
    }
    pt = unwrapped_pt + wrap;
}


/*
 *  If point lies on a seam(s) swap to opposite side of UV.
 *  hint = 1 swap E/W, 2 swap N/S or default 3 swap both
 */
void
SwapUVSeamPoint(const ON_Surface *surf, ON_2dPoint &p, int hint)
{
    int seam;
    ON_Interval dom[2];
    dom[0] = surf->Domain(0);
    dom[1] = surf->Domain(1);

    if ((seam=surf->IsAtSeam(p.x,p.y)) > 0) {
	if (seam == 1) { // east/west seam
	    if (fabs(p.x - dom[0].m_t[0]) > dom[0].Length()/2.0) {
		p.x = dom[0].m_t[0]; // on east swap to west seam
	    } else {
		p.x = dom[0].m_t[1]; // on west swap to east seam
	    }
	} else if (seam == 2) { // north/south seam
	    if (fabs(p.y - dom[1].m_t[0]) > dom[1].Length()/2.0) {
		p.y = dom[1].m_t[0]; // on north swap to south seam
	    } else {
		p.y = dom[1].m_t[1]; // on south swap to north seam
	    }
	} else { //on both seams check hint 1=east/west only, 2=north/south, 3 = both
	    if (hint == 1) {
		if (fabs(p.x - dom[0].m_t[0]) > dom[0].Length()/2.0) {
		    p.x = dom[0].m_t[0]; // on east swap to west seam
		} else {
		    p.x = dom[0].m_t[1]; // on west swap to east seam
		}
	    } else if (hint == 2) {
		if (fabs(p.y - dom[1].m_t[0]) > dom[1].Length()/2.0) {
		    p.y = dom[1].m_t[0]; // on north swap to south seam
		} else {
		    p.y = dom[1].m_t[1]; // on south swap to north seam
		}
	    } else {
		if (fabs(p.x - dom[0].m_t[0]) > dom[0].Length()/2.0) {
		    p.x = dom[0].m_t[0]; // on east swap to west seam
		} else {
		    p.x = dom[0].m_t[1]; // on west swap to east seam
		}
		if (fabs(p.y - dom[1].m_t[0]) > dom[1].Length()/2.0) {
		    p.y = dom[1].m_t[0]; // on north swap to south seam
		} else {
		    p.y = dom[1].m_t[1]; // on south swap to north seam
		}
	    }
	}
    }
}


/*
 *  Find where Pullback of 3d curve crosses closed seam of surface UV
 */
bool
Find3DCurveSeamCrossing(PBCData &data,double t0,double t1, double offset,double &seam_t,ON_2dPoint &from,ON_2dPoint &to,double tol)
{
    bool rc = true;
    const ON_Surface *surf = data.surf;

    // quick bail out is surface not closed
    if (surf->IsClosed(0) || surf->IsClosed(1)) {
	ON_2dPoint p0_2d = ON_2dPoint::UnsetPoint;
	ON_2dPoint p1_2d = ON_2dPoint::UnsetPoint;
	ON_3dPoint p0_3d = data.curve->PointAt(t0);
	ON_3dPoint p1_3d = data.curve->PointAt(t1);
	ON_Interval dom[2];
	double p0_distance;
	double p1_distance;

	dom[0] = surf->Domain(0);
	dom[1] = surf->Domain(1);

	ON_3dPoint check_pt_3d = ON_3dPoint::UnsetPoint;
	int udir=0;
	int vdir=0;
	if (surface_GetClosestPoint3dFirstOrder(surf,p0_3d,p0_2d,check_pt_3d,p0_distance,0,BREP_SAME_POINT_TOLERANCE,BREP_EDGE_MISS_TOLERANCE) &&
		surface_GetClosestPoint3dFirstOrder(surf,p1_3d,p1_2d,check_pt_3d,p1_distance,0,BREP_SAME_POINT_TOLERANCE,BREP_EDGE_MISS_TOLERANCE) ) {
	    if (ConsecutivePointsCrossClosedSeam(surf,p0_2d,p1_2d,udir,vdir,tol)) {
		ON_2dPoint p_2d;
		//lets check to see if p0 || p1 are already on a seam
		int seam0=0;
		if ((seam0 = IsAtSeam(surf,p0_2d, tol)) > 0) {
		    ForceToClosestSeam(surf, p0_2d, tol);
		}
		int seam1 = 0;
		if ((seam1 = IsAtSeam(surf,p1_2d, tol)) > 0) {
		    ForceToClosestSeam(surf, p1_2d, tol);
		}
		if (seam0 > 0 ) {
		    if (seam1 > 0) { // both p0 & p1 on seam shouldn't happen report error and return false
			rc = false;
		    } else { // just p0 on seam
			from = to = p0_2d;
			seam_t = t0;
			SwapUVSeamPoint(surf, to);
		    }
		} else if (seam1 > 0) { // only p1 on seam
		    from = to = p1_2d;
		    seam_t = t1;
		    SwapUVSeamPoint(surf, from);
		} else { // crosses the seam somewhere in between the two points
		    bool seam_not_found = true;
		    while(seam_not_found) {
			double d0 = DistToNearestClosedSeam(surf,p0_2d);
			double d1 = DistToNearestClosedSeam(surf,p1_2d);
			if ((d0 > 0.0) && (d1 > 0.0)) {
			    double t = t0 + (t1 - t0)*(d0/(d0+d1));
			    int seam;
			    ON_3dPoint p_3d = data.curve->PointAt(t);
			    double distance;

			    if (surface_GetClosestPoint3dFirstOrder(surf,p_3d,p_2d,check_pt_3d,distance,0,BREP_SAME_POINT_TOLERANCE,BREP_EDGE_MISS_TOLERANCE)) {
				if ((seam=IsAtSeam(surf,p_2d, tol)) > 0) {
				    ForceToClosestSeam(surf, p_2d, tol);
				    from = to = p_2d;
				    seam_t = t;
				    if (p0_2d.DistanceTo(p_2d) < p1_2d.DistanceTo(p_2d)) {
					SwapUVSeamPoint(surf, to);
				    } else {
					SwapUVSeamPoint(surf, from);
				    }
				    seam_not_found=false;
				    rc = true;
				} else {
				    if (ConsecutivePointsCrossClosedSeam(surf,p0_2d,p_2d,udir,vdir,tol)) {
					p1_2d = p_2d;
					t1 = t;
				    } else if (ConsecutivePointsCrossClosedSeam(surf,p_2d,p1_2d,udir,vdir,tol)) {
					p0_2d = p_2d;
					t0=t;
				    } else {
					seam_not_found=false;
					rc = false;
				    }
				}
			    } else {
				seam_not_found=false;
				rc = false;
			    }
			} else {
			    seam_not_found=false;
			    rc = false;
			}
		    }
		}
	    }
	}
    }

    return rc;
}


/*
 *  Find where 2D trim curve crosses closed seam of surface UV
 */
bool
FindTrimSeamCrossing(const ON_BrepTrim &trim,double t0,double t1,double &seam_t,ON_2dPoint &from,ON_2dPoint &to,double tol)
{
    bool rc = true;
    const ON_Surface *surf = trim.SurfaceOf();

    // quick bail out is surface not closed
    if (surf->IsClosed(0) || surf->IsClosed(1)) {
	ON_2dPoint p0 = trim.PointAt(t0);
	ON_2dPoint p1 = trim.PointAt(t1);
	ON_Interval dom[2];
	dom[0] = surf->Domain(0);
	dom[1] = surf->Domain(1);

	p0 = UnwrapUVPoint(surf,p0);
	p1 = UnwrapUVPoint(surf,p1);

        int udir=0;
        int vdir=0;
	if (ConsecutivePointsCrossClosedSeam(surf,p0,p1,udir,vdir,tol)) {
	    ON_2dPoint p;
	    //lets check to see if p0 || p1 are already on a seam
	    int seam0=0;
	    if ((seam0 = IsAtSeam(surf,p0, tol)) > 0) {
		ForceToClosestSeam(surf, p0, tol);
	    }
	    int seam1 = 0;
	    if ((seam1 = IsAtSeam(surf,p1, tol)) > 0) {
		ForceToClosestSeam(surf, p1, tol);
	    }
	    if (seam0 > 0 ) {
		if (seam1 > 0) { // both p0 & p1 on seam shouldn't happen report error and return false
		    rc = false;
		} else { // just p0 on seam
		    from = to = p0;
		    seam_t = t0;
		    SwapUVSeamPoint(surf, to);
		}
	    } else if (seam1 > 0) { // only p1 on seam
		from = to = p1;
		seam_t = t1;
		SwapUVSeamPoint(surf, from);
	    } else { // crosses the seam somewhere in between the two points
		bool seam_not_found = true;
		while (seam_not_found) {
		    double d0 = DistToNearestClosedSeam(surf,p0);
		    double d1 = DistToNearestClosedSeam(surf,p1);
		    if ((d0 > tol) && (d1 > tol)) {
			double t = t0 + (t1 - t0)*(d0/(d0+d1));
			int seam;
			p = trim.PointAt(t);
			if ((seam=IsAtSeam(surf,p, tol)) > 0) {
			    ForceToClosestSeam(surf, p, tol);
			    from = to = p;
			    seam_t = t;

			    if (p0.DistanceTo(p) < p1.DistanceTo(p)) {
				SwapUVSeamPoint(surf, to);
			    } else {
				SwapUVSeamPoint(surf, from);
			    }
			    seam_not_found=false;
			    rc = true;
			} else {
			    if (ConsecutivePointsCrossClosedSeam(surf,p0,p,udir,vdir,tol)) {
				p1 = p;
				t1 = t;
			    } else if (ConsecutivePointsCrossClosedSeam(surf,p,p1,udir,vdir,tol)) {
				p0 = p;
				t0=t;
			    } else {
				seam_not_found=false;
				rc = false;
			    }
			}
		    } else {
			seam_not_found=false;
			rc = false;
		    }
		}
	    }
	}
    }

    return rc;
}


void
pullback_samples_from_closed_surface(PBCData* data,
		 double t,
		 double s)
{
    if (!data)
	return;

    if (!data->surf || !data->curve)
	return;

    const ON_Curve* curve= data->curve;
    const ON_Surface *surf = data->surf;

    ON_2dPointArray *samples= new ON_2dPointArray();
    size_t numKnots = curve->SpanCount();
    double *knots = new double[numKnots+1];

    curve->GetSpanVector(knots);

    size_t istart = 0;
    while ((istart < (numKnots+1)) && (t >= knots[istart]))
	istart++;

    if (istart > 0) {
	knots[--istart] = t;
    }

    size_t istop = numKnots;
    while ((istop > 0) && (s <= knots[istop]))
	istop--;

    if (istop < numKnots) {
	knots[++istop] = s;
    }

    size_t degree = curve->Degree();
    size_t samplesperknotinterval=18*degree;

    ON_2dPoint pt;
    ON_2dPoint prev_pt;
    double prev_t = knots[istart];
    double offset = 0.0;
    double delta;
    for (size_t i=istart; i<istop; i++) {
	delta = (knots[i+1] - knots[i])/(double)samplesperknotinterval;
	if (i <= numKnots/2) {
	    offset = PBC_FROM_OFFSET;
	} else {
	    offset = -PBC_FROM_OFFSET;
	}
	for (size_t j=0; j<=samplesperknotinterval; j++) {
	    if ((j == samplesperknotinterval) && (i < istop - 1))
		continue;

	    double curr_t = knots[i]+j*delta;
	    if (curr_t < (s-t)/2.0) {
		offset = PBC_FROM_OFFSET;
	    } else {
		offset = -PBC_FROM_OFFSET;
	    }
	    ON_3dPoint p = curve->PointAt(curr_t);
	    ON_3dPoint p3d = ON_3dPoint::UnsetPoint;
	    double distance;
	    if (surface_GetClosestPoint3dFirstOrder(surf,p,pt,p3d,distance,0,BREP_SAME_POINT_TOLERANCE,BREP_EDGE_MISS_TOLERANCE)) {
		if (IsAtSeam(surf,pt,PBC_SEAM_TOL) > 0) {
		    ForceToClosestSeam(surf, pt, PBC_SEAM_TOL);
		}
		if ((i == istart) && (j == 0)) {
		    // first point just append and set reference in prev_pt
		    samples->Append(pt);
		    prev_pt = pt;
		    prev_t = curr_t;
		    continue;
		}
		int udir= 0;
		int vdir= 0;
		if (ConsecutivePointsCrossClosedSeam(surf,pt,prev_pt,udir,vdir,PBC_SEAM_TOL)) {
		    int pt_seam = surf->IsAtSeam(pt.x,pt.y);
		    int prev_pt_seam = surf->IsAtSeam(prev_pt.x,prev_pt.y);
		    if ( pt_seam > 0) {
			if ((prev_pt_seam > 0) && (samples->Count() == 1)) {
			    samples->Empty();
			    SwapUVSeamPoint(surf, prev_pt,pt_seam);
			    samples->Append(prev_pt);
			} else {
			    if (pt_seam == 3) {
				if (prev_pt_seam == 1) {
				    pt.x = prev_pt.x;
				    if (ConsecutivePointsCrossClosedSeam(surf,pt,prev_pt,udir,vdir,PBC_SEAM_TOL)) {
					SwapUVSeamPoint(surf, pt, 2);
				    }
				} else if (prev_pt_seam == 2) {
				    pt.y = prev_pt.y;
				    if (ConsecutivePointsCrossClosedSeam(surf,pt,prev_pt,udir,vdir,PBC_SEAM_TOL)) {
					SwapUVSeamPoint(surf, pt, 1);
				    }
				}
			    } else {
				SwapUVSeamPoint(surf, pt, prev_pt_seam);
			    }
			}
		    } else if (prev_pt_seam > 0) {
			if (samples->Count() == 1) {
			    samples->Empty();
			    SwapUVSeamPoint(surf, prev_pt);
			    samples->Append(prev_pt);
			}
		    } else if (data->curve->IsClosed()) {
			ON_2dPoint from,to;
			double seam_t;
			if (Find3DCurveSeamCrossing(*data,prev_t,curr_t,offset,seam_t,from,to,PBC_TOL)) {
			    samples->Append(from);
			    data->segments.push_back(samples);
			    samples= new ON_2dPointArray();
			    samples->Append(to);
			    prev_pt = to;
			    prev_t = seam_t;
			} else {
			    std::cout << "Can not find seam crossing...." << std::endl;
			}
		    }
		}
		samples->Append(pt);

		prev_pt = pt;
		prev_t = curr_t;
	    }
	}
    }
    delete [] knots;

    if (samples != NULL) {
	data->segments.push_back(samples);

	int numsegs = data->segments.size();

	if (numsegs > 1) {
	    if (curve->IsClosed()) {
		ON_2dPointArray *reordered_samples= new ON_2dPointArray();
		// must have walked over seam but have closed curve so reorder stitching
		int seg = 0;
		for (std::list<ON_2dPointArray *>::reverse_iterator rit=data->segments.rbegin(); rit!=data->segments.rend(); ++seg) {
		    samples = *rit;
		    if (seg < numsegs-1) { // since end points should be repeated
			reordered_samples->Append(samples->Count()-1,(const ON_2dPoint *)samples->Array());
		    } else {
			reordered_samples->Append(samples->Count(),(const ON_2dPoint *)samples->Array());
		    }
		    data->segments.erase((++rit).base());
		    rit = data->segments.rbegin();
		    delete samples;
		}
		data->segments.clear();
		data->segments.push_back(reordered_samples);

	    } else {
		//punt for now
	    }
	}
    }

    return;
}


PBCData *
pullback_samples(const ON_Surface* surf,
		 const ON_Curve* curve,
		 double tolerance,
		 double flatness)
{
    if (!surf)
	return NULL;

    PBCData *data = new PBCData;
    data->tolerance = tolerance;
    data->flatness = flatness;
    data->curve = curve;
    data->surf = surf;
    data->surftree = NULL;

    double tmin, tmax;
    data->curve->GetDomain(&tmin, &tmax);

    if (surf->IsClosed(0) || surf->IsClosed(1)) {
	if ((tmin < 0.0) && (tmax > 0.0)) {
	    ON_2dPoint uv = ON_2dPoint::UnsetPoint;
	    ON_3dPoint p = curve->PointAt(0.0);
	    ON_3dPoint p3d = ON_3dPoint::UnsetPoint;
	    double distance;
	    int quadrant = 0; // optional - 0 = default, 1 from NE quadrant, 2 from NW quadrant, 3 from SW quadrant, 4 from SE quadrant
	    if (surface_GetClosestPoint3dFirstOrder(surf,p,uv,p3d,distance,quadrant,BREP_SAME_POINT_TOLERANCE,BREP_EDGE_MISS_TOLERANCE)) {
		if (IsAtSeam(surf, uv, PBC_SEAM_TOL) > 0) {
		    ON_2dPointArray *samples1 = pullback_samples(data, tmin, 0.0);
		    ON_2dPointArray *samples2 = pullback_samples(data, 0.0, tmax);
		    if (samples1 != NULL) {
			data->segments.push_back(samples1);
		    }
		    if (samples2 != NULL) {
			data->segments.push_back(samples2);
		    }
		} else {
		    ON_2dPointArray *samples = pullback_samples(data, tmin, tmax);
		    if (samples != NULL) {
			data->segments.push_back(samples);
		    }
		}
	    } else {
		std::cerr << "pullback_samples:Error: cannot evaluate curve at parameter 0.0" << std::endl;
		delete data;
		return NULL;
	    }
	} else {
	    pullback_samples_from_closed_surface(data, tmin, tmax);
	}
    } else {
	ON_2dPointArray *samples = pullback_samples(data, tmin, tmax);
	if (samples != NULL) {
	    data->segments.push_back(samples);
	}
    }
    return data;
}


ON_Curve*
refit_edge(const ON_BrepEdge* edge, double UNUSED(tolerance))
{
    double edge_tolerance = 0.01;
    ON_Brep *brep = edge->Brep();
#ifdef SHOW_UNUSED
    ON_3dPoint start = edge->PointAtStart();
    ON_3dPoint end = edge->PointAtEnd();
#endif

    ON_BrepTrim& trim1 = brep->m_T[edge->m_ti[0]];
    ON_BrepTrim& trim2 = brep->m_T[edge->m_ti[1]];
    ON_BrepFace *face1 = trim1.Face();
    ON_BrepFace *face2 = trim2.Face();
    const ON_Surface *surface1 = face1->SurfaceOf();
    const ON_Surface *surface2 = face2->SurfaceOf();
    bool removeTrimmed = false;
    brlcad::SurfaceTree *st1 = new brlcad::SurfaceTree(face1, removeTrimmed);
    brlcad::SurfaceTree *st2 = new brlcad::SurfaceTree(face2, removeTrimmed);

    ON_Curve *curve = brep->m_C3[edge->m_c3i];
    double t0, t1;
    curve->GetDomain(&t0, &t1);
    ON_Plane plane;
    curve->FrameAt(t0, plane);
#ifdef SHOW_UNUSED
    ON_3dPoint origin = plane.Origin();
    ON_3dVector xaxis = plane.Xaxis();
    ON_3dVector yaxis = plane.Yaxis();
    ON_3dVector zaxis = plane.zaxis;
    ON_3dPoint px = origin + xaxis;
    ON_3dPoint py = origin + yaxis;
    ON_3dPoint pz = origin + zaxis;
#endif

    int numKnots = curve->SpanCount();
    double *knots = new double[numKnots + 1];
    curve->GetSpanVector(knots);

    int samplesperknotinterval;
    int degree = curve->Degree();

    if (degree > 1) {
	samplesperknotinterval = 3 * degree;
    } else {
	samplesperknotinterval = 18 * degree;
    }
    ON_2dPoint pt;
    double t = 0.0;
    ON_3dPoint pointOnCurve;
    ON_3dPoint knudgedPointOnCurve;
    for (int i = 0; i <= numKnots; i++) {
	if (i <= numKnots / 2) {
	    if (i > 0) {
		double delta = (knots[i] - knots[i - 1]) / (double) samplesperknotinterval;
		for (int j = 1; j < samplesperknotinterval; j++) {
		    t = knots[i - 1] + j * delta;
		    pointOnCurve = curve->PointAt(t);
		    knudgedPointOnCurve = curve->PointAt(t + PBC_TOL);

		    ON_3dPoint point = pointOnCurve;
		    ON_3dPoint knudgepoint = knudgedPointOnCurve;
		    ON_3dPoint ps1;
		    ON_3dPoint ps2;
		    bool found = false;
		    double dist;
		    while (!found) {
			ON_2dPoint uv;
			if (st1->getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
			    ps1 = surface1->PointAt(uv.x, uv.y);
			    if (st2->getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
				ps2 = surface2->PointAt(uv.x, uv.y);
			    }
			}
			dist = ps1.DistanceTo(ps2);
			if (NEAR_ZERO(dist, PBC_TOL)) {
			    point = ps1;
			    found = true;
			} else {
			    ON_3dVector v1 = ps1 - point;
			    ON_3dVector v2 = ps2 - point;
			    knudgepoint = point;
			    ON_3dVector deltav = v1 + v2;
			    if (NEAR_ZERO(deltav.Length(), PBC_TOL)) {
				found = true; // as close as we are going to get
			    } else {
				point = point + v1 + v2;
			    }
			}
		    }
		}
	    }
	    t = knots[i];
	    pointOnCurve = curve->PointAt(t);
	    knudgedPointOnCurve = curve->PointAt(t + PBC_TOL);
	    ON_3dPoint point = pointOnCurve;
	    ON_3dPoint knudgepoint = knudgedPointOnCurve;
	    ON_3dPoint ps1;
	    ON_3dPoint ps2;
	    bool found = false;
	    double dist;

	    while (!found) {
		ON_2dPoint uv;
		if (st1->getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
		    ps1 = surface1->PointAt(uv.x, uv.y);
		    if (st2->getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
			ps2 = surface2->PointAt(uv.x, uv.y);
		    }
		}
		dist = ps1.DistanceTo(ps2);
		if (NEAR_ZERO(dist, PBC_TOL)) {
		    point = ps1;
		    found = true;
		} else {
		    ON_3dVector v1 = ps1 - point;
		    ON_3dVector v2 = ps2 - point;
		    knudgepoint = point;
		    ON_3dVector deltav = v1 + v2;
		    if (NEAR_ZERO(deltav.Length(), PBC_TOL)) {
			found = true; // as close as we are going to get
		    } else {
			point = point + v1 + v2;
		    }
		}
	    }
	} else {
	    if (i > 0) {
		double delta = (knots[i] - knots[i - 1]) / (double) samplesperknotinterval;
		for (int j = 1; j < samplesperknotinterval; j++) {
		    t = knots[i - 1] + j * delta;
		    pointOnCurve = curve->PointAt(t);
		    knudgedPointOnCurve = curve->PointAt(t - PBC_TOL);

		    ON_3dPoint point = pointOnCurve;
		    ON_3dPoint knudgepoint = knudgedPointOnCurve;
		    ON_3dPoint ps1;
		    ON_3dPoint ps2;
		    bool found = false;
		    double dist;

		    while (!found) {
			ON_2dPoint uv;
			if (st1->getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
			    ps1 = surface1->PointAt(uv.x, uv.y);
			    if (st2->getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
				ps2 = surface2->PointAt(uv.x, uv.y);
			    }
			}
			dist = ps1.DistanceTo(ps2);
			if (NEAR_ZERO(dist, PBC_TOL)) {
			    point = ps1;
			    found = true;
			} else {
			    ON_3dVector v1 = ps1 - point;
			    ON_3dVector v2 = ps2 - point;
			    knudgepoint = point;
			    ON_3dVector deltav = v1 + v2;
			    if (NEAR_ZERO(deltav.Length(), PBC_TOL)) {
				found = true; // as close as we are going to get
			    } else {
				point = point + v1 + v2;
			    }
			}
		    }
		}
		t = knots[i];
		pointOnCurve = curve->PointAt(t);
		knudgedPointOnCurve = curve->PointAt(t - PBC_TOL);
		ON_3dPoint point = pointOnCurve;
		ON_3dPoint knudgepoint = knudgedPointOnCurve;
		ON_3dPoint ps1;
		ON_3dPoint ps2;
		bool found = false;
		double dist;

		while (!found) {
		    ON_2dPoint uv;
		    if (st1->getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
			ps1 = surface1->PointAt(uv.x, uv.y);
			if (st2->getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
			    ps2 = surface2->PointAt(uv.x, uv.y);
			}
		    }
		    dist = ps1.DistanceTo(ps2);
		    if (NEAR_ZERO(dist, PBC_TOL)) {
			point = ps1;
			found = true;
		    } else {
			ON_3dVector v1 = ps1 - point;
			ON_3dVector v2 = ps2 - point;
			knudgepoint = point;
			ON_3dVector deltav = v1 + v2;
			if (NEAR_ZERO(deltav.Length(), PBC_TOL)) {
			    found = true; // as close as we are going to get
			} else {
			    point = point + v1 + v2;
			}
		    }
		}
	    }
	}
    }
    delete [] knots;


    return NULL;
}


bool
has_singularity(const ON_Surface *surf)
{
    bool ret = false;
    if (UNLIKELY(!surf)) return ret;
    // 0 = south, 1 = east, 2 = north, 3 = west
    for (int i = 0; i < 4; i++) {
	if (surf->IsSingular(i)) {
	    /*
	      switch (i) {
	      case 0:
	      std::cout << "Singular South" << std::endl;
	      break;
	      case 1:
	      std::cout << "Singular East" << std::endl;
	      break;
	      case 2:
	      std::cout << "Singular North" << std::endl;
	      break;
	      case 3:
	      std::cout << "Singular West" << std::endl;
	      }
	    */
	    ret = true;
	}
    }
    return ret;
}


bool is_closed(const ON_Surface *surf)
{
    bool ret = false;
    if (UNLIKELY(!surf)) return ret;
    // dir 0 = "s", 1 = "t"
    for (int i = 0; i < 2; i++) {
	if (surf->IsClosed(i)) {
//			switch (i) {
//			case 0:
//				std::cout << "Closed in U" << std::endl;
//				break;
//			case 1:
//				std::cout << "Closed in V" << std::endl;
//			}
	    ret = true;
	}
    }
    return ret;
}


bool
check_pullback_closed(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator d = pbcs.begin();
    if ((*d) == NULL || (*d)->surf == NULL)
	return false;

    const ON_Surface *surf = (*d)->surf;

    //TODO:
    // 0 = U, 1 = V
    if (surf->IsClosed(0) && surf->IsClosed(1)) {
	//TODO: need to check how torus UV looks to determine checks
	std::cerr << "Is this some kind of torus????" << std::endl;
    } else if (surf->IsClosed(0)) {
	//check_pullback_closed_U(pbcs);
	std::cout << "check closed in U" << std::endl;
    } else if (surf->IsClosed(1)) {
	//check_pullback_closed_V(pbcs);
	std::cout << "check closed in V" << std::endl;
    }
    return true;
}


bool
check_pullback_singular_east(std::list<PBCData*> &pbcs)
{
    std::list<PBCData *>::iterator cs = pbcs.begin();
    if ((*cs) == NULL || (*cs)->surf == NULL)
	return false;

    const ON_Surface *surf = (*cs)->surf;
    double umin, umax;
    ON_2dPoint *prev = NULL;

    surf->GetDomain(0, &umin, &umax);
    std::cout << "Umax: " << umax << std::endl;
    while (cs != pbcs.end()) {
	PBCData *data = (*cs);
	std::list<ON_2dPointArray *>::iterator si = data->segments.begin();
	int segcnt = 0;
	while (si != data->segments.end()) {
	    ON_2dPointArray *samples = (*si);
	    std::cerr << std::endl << "Segment:" << ++segcnt << std::endl;
	    if (true) {
		int ilast = samples->Count() - 1;
		std::cerr << std::endl << 0 << "- " << (*samples)[0].x << ", " << (*samples)[0].y << std::endl;
		std::cerr << ilast << "- " << (*samples)[ilast].x << ", " << (*samples)[ilast].y << std::endl;
	    } else {
		for (int i = 0; i < samples->Count(); i++) {
		    if (NEAR_EQUAL((*samples)[i].x, umax, PBC_TOL)) {
			if (prev != NULL) {
			    std::cerr << "prev - " << prev->x << ", " << prev->y << std::endl;
			}
			std::cerr << i << "- " << (*samples)[i].x << ", " << (*samples)[i].y << std::endl << std::endl;
		    }
		    prev = &(*samples)[i];
		}
	    }
	    si ++;
	}
	cs++;
    }
    return true;
}


bool
check_pullback_singular(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator d = pbcs.begin();
    if ((*d) == NULL || (*d)->surf == NULL)
	return false;

    const ON_Surface *surf = (*d)->surf;
    int cnt = 0;

    for (int i = 0; i < 4; i++) {
	if (surf->IsSingular(i)) {
	    cnt++;
	}
    }
    if (cnt > 2) {
	//TODO: I don't think this makes sense but check out
	std::cerr << "Is this some kind of sickness????" << std::endl;
	return false;
    } else if (cnt == 2) {
	if (surf->IsSingular(0) && surf->IsSingular(2)) {
	    std::cout << "check singular North-South" << std::endl;
	} else if (surf->IsSingular(1) && surf->IsSingular(2)) {
	    std::cout << "check singular East-West" << std::endl;
	} else {
	    //TODO: I don't think this makes sense but check out
	    std::cerr << "Is this some kind of sickness????" << std::endl;
	    return false;
	}
    } else {
	if (surf->IsSingular(0)) {
	    std::cout << "check singular South" << std::endl;
	} else if (surf->IsSingular(1)) {
	    std::cout << "check singular East" << std::endl;
	    if (check_pullback_singular_east(pbcs)) {
		return true;
	    }
	} else if (surf->IsSingular(2)) {
	    std::cout << "check singular North" << std::endl;
	} else if (surf->IsSingular(3)) {
	    std::cout << "check singular West" << std::endl;
	}
    }
    return true;
}


#ifdef _DEBUG_TESTING_
void
print_pullback_data(std::string str, std::list<PBCData*> &pbcs, bool justendpoints)
{
    std::list<PBCData*>::iterator cs = pbcs.begin();
    int trimcnt = 0;
    if (justendpoints) {
	// print out endpoints before
	std::cerr << "EndPoints " << str << ":" << std::endl;
	while (cs != pbcs.end()) {
	    PBCData *data = (*cs);
	    if (!data || !data->surf)
		continue;

	    const ON_Surface *surf = data->surf;
	    std::list<ON_2dPointArray *>::iterator si = data->segments.begin();
	    int segcnt = 0;
	    while (si != data->segments.end()) {
		ON_2dPointArray *samples = (*si);
		std::cerr << std::endl << "  Segment:" << ++segcnt << std::endl;
		int ilast = samples->Count() - 1;
		std::cerr << "    T:" << ++trimcnt << std::endl;
		int i = 0;
		int singularity = IsAtSingularity(surf, (*samples)[i], PBC_SEAM_TOL);
		int seam = IsAtSeam(surf, (*samples)[i], PBC_SEAM_TOL);
		std::cerr << "--------";
		if ((seam > 0) && (singularity >= 0)) {
		    std::cerr << " S/S  " << (*samples)[i].x << ", " << (*samples)[i].y;
		} else if (seam > 0) {
		    std::cerr << " Seam " << (*samples)[i].x << ", " << (*samples)[i].y;
		} else if (singularity >= 0) {
		    std::cerr << " Sing " << (*samples)[i].x << ", " << (*samples)[i].y;
		} else {
		    std::cerr << "      " << (*samples)[i].x << ", " << (*samples)[i].y;
		}
		ON_3dPoint p = surf->PointAt((*samples)[i].x, (*samples)[i].y);
		std::cerr << "  (" << p.x << ", " << p.y << ", " << p.z << ") " << std::endl;

		i = ilast;
		singularity = IsAtSingularity(surf, (*samples)[i], PBC_SEAM_TOL);
		seam = IsAtSeam(surf, (*samples)[i], PBC_SEAM_TOL);
		std::cerr << "        ";
		if ((seam > 0) && (singularity >= 0)) {
		    std::cerr << " S/S  " << (*samples)[i].x << ", " << (*samples)[i].y << std::endl;
		} else if (seam > 0) {
		    std::cerr << " Seam " << (*samples)[i].x << ", " << (*samples)[i].y << std::endl;
		} else if (singularity >= 0) {
		    std::cerr << " Sing " << (*samples)[i].x << ", " << (*samples)[i].y << std::endl;
		} else {
		    std::cerr << "      " << (*samples)[i].x << ", " << (*samples)[i].y << std::endl;
		}
		p = surf->PointAt((*samples)[i].x, (*samples)[i].y);
		std::cerr << "  (" << p.x << ", " << p.y << ", " << p.z << ") " << std::endl;
		si++;
	    }
	    cs++;
	}
    } else {
	// print out all points
	trimcnt = 0;
	cs = pbcs.begin();
	std::cerr << str << ":" << std::endl;
	while (cs != pbcs.end()) {
	    PBCData *data = (*cs);
	    if (!data || !data->surf)
		continue;

	    const ON_Surface *surf = data->surf;
	    std::list<ON_2dPointArray *>::iterator si = data->segments.begin();
	    int segcnt = 0;
	    std::cerr << "2d surface domain: " << std::endl;
	    std::cerr << "in rpp rpp" << surf->Domain(0).m_t[0] << " " << surf->Domain(0).m_t[1] << " " << surf->Domain(1).m_t[0] << " " << surf->Domain(1).m_t[1] << " 0.0 0.01"  << std::endl;
	    while (si != data->segments.end()) {
		ON_2dPointArray *samples = (*si);
		std::cerr << std::endl << "  Segment:" << ++segcnt << std::endl;
		std::cerr << "    T:" << ++trimcnt << std::endl;
		for (int i = 0; i < samples->Count(); i++) {
		    int singularity = IsAtSingularity(surf, (*samples)[i], PBC_SEAM_TOL);
		    int seam = IsAtSeam(surf, (*samples)[i], PBC_SEAM_TOL);
		    if (_debug_print_mged2d_points_) {
			std::cerr << "in pt_" << _debug_point_count_++ << " sph " << (*samples)[i].x << " " << (*samples)[i].y << " 0.0 0.1000"  << std::endl;
		    } else if (_debug_print_mged3d_points_) {
			ON_3dPoint p = surf->PointAt((*samples)[i].x, (*samples)[i].y);
			std::cerr << "in pt_" << _debug_point_count_++ << " sph " << p.x << " " << p.y << " " << p.z << " 0.1000"  << std::endl;
		    } else {
			if (i == 0) {
			    std::cerr << "--------";
			} else {
			    std::cerr << "        ";
			}
			if ((seam > 0) && (singularity >= 0)) {
			    std::cerr << " S/S  " << (*samples)[i].x << ", " << (*samples)[i].y;
			} else if (seam > 0) {
			    std::cerr << " Seam " << (*samples)[i].x << ", " << (*samples)[i].y;
			} else if (singularity >= 0) {
			    std::cerr << " Sing " << (*samples)[i].x << ", " << (*samples)[i].y;
			} else {
			    std::cerr << "      " << (*samples)[i].x << ", " << (*samples)[i].y;
			}
			ON_3dPoint p = surf->PointAt((*samples)[i].x, (*samples)[i].y);
			std::cerr << "  (" << p.x << ", " << p.y << ", " << p.z << ") " << std::endl;
		    }
		}
		si++;
	    }
	    cs++;
	}
    }
    /////
}
#endif


bool
resolve_seam_segment_from_prev(const ON_Surface *surface, ON_2dPointArray &segment, ON_2dPoint *prev = NULL)
{
    bool complete = false;
    double umin, umax, umid;
    double vmin, vmax, vmid;

    surface->GetDomain(0, &umin, &umax);
    surface->GetDomain(1, &vmin, &vmax);
    umid = (umin + umax) / 2.0;
    vmid = (vmin + vmax) / 2.0;

    for (int i = 0; i < segment.Count(); i++) {
	int singularity = IsAtSingularity(surface, segment[i], PBC_SEAM_TOL);
	int seam = IsAtSeam(surface, segment[i], PBC_SEAM_TOL);
	if ((seam > 0)) {
	    if (prev != NULL) {
		//std::cerr << " at seam " << seam << " but has prev" << std::endl;
		//std::cerr << "    prev: " << prev->x << ", " << prev->y << std::endl;
		//std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
		switch (seam) {
		    case 1: //east/west
			if (prev->x < umid) {
			    segment[i].x = umin;
			} else {
			    segment[i].x = umax;
			}
			break;
		    case 2: //north/south
			if (prev->y < vmid) {
			    segment[i].y = vmin;
			} else {
			    segment[i].y = vmax;
			}
			break;
		    case 3: //both
			if (prev->x < umid) {
			    segment[i].x = umin;
			} else {
			    segment[i].x = umax;
			}
			if (prev->y < vmid) {
			    segment[i].y = vmin;
			} else {
			    segment[i].y = vmax;
			}
		}
		prev = &segment[i];
	    } else {
		//std::cerr << " at seam and no prev" << std::endl;
		complete = false;
	    }
	} else {
	    if (singularity < 0) {
		prev = &segment[i];
	    } else {
		prev = NULL;
	    }
	}
    }
    return complete;
}


bool
resolve_seam_segment_from_next(const ON_Surface *surface, ON_2dPointArray &segment, ON_2dPoint *next = NULL)
{
    bool complete = false;
    double umin, umax, umid;
    double vmin, vmax, vmid;

    surface->GetDomain(0, &umin, &umax);
    surface->GetDomain(1, &vmin, &vmax);
    umid = (umin + umax) / 2.0;
    vmid = (vmin + vmax) / 2.0;

    if (next != NULL) {
	complete = true;
	for (int i = segment.Count() - 1; i >= 0; i--) {
	    int singularity = IsAtSingularity(surface, segment[i], PBC_SEAM_TOL);
	    int seam = IsAtSeam(surface, segment[i], PBC_SEAM_TOL);

	    if ((seam > 0)) {
		if (next != NULL) {
		    switch (seam) {
			case 1: //east/west
			    if (next->x < umid) {
				segment[i].x = umin;
			    } else {
				segment[i].x = umax;
			    }
			    break;
			case 2: //north/south
			    if (next->y < vmid) {
				segment[i].y = vmin;
			    } else {
				segment[i].y = vmax;
			    }
			    break;
			case 3: //both
			    if (next->x < umid) {
				segment[i].x = umin;
			    } else {
				segment[i].x = umax;
			    }
			    if (next->y < vmid) {
				segment[i].y = vmin;
			    } else {
				segment[i].y = vmax;
			    }
		    }
		    next = &segment[i];
		} else {
		    //std::cerr << " at seam and no prev" << std::endl;
		    complete = false;
		}
	    } else {
		if (singularity < 0) {
		    next = &segment[i];
		} else {
		    next = NULL;
		}
	    }
	}
    }
    return complete;
}


bool
resolve_seam_segment(const ON_Surface *surface, ON_2dPointArray &segment, bool &u_resolved, bool &v_resolved)
{
    ON_2dPoint *prev = NULL;
    bool complete = false;
    double umin, umax, umid;
    double vmin, vmax, vmid;
    int prev_seam = 0;

    surface->GetDomain(0, &umin, &umax);
    surface->GetDomain(1, &vmin, &vmax);
    umid = (umin + umax) / 2.0;
    vmid = (vmin + vmax) / 2.0;

    for (int i = 0; i < segment.Count(); i++) {
	int singularity = IsAtSingularity(surface, segment[i], PBC_SEAM_TOL);
	int seam = IsAtSeam(surface, segment[i], PBC_SEAM_TOL);

	if ((seam > 0)) {
	    if (prev != NULL) {
		//std::cerr << " at seam " << seam << " but has prev" << std::endl;
		//std::cerr << "    prev: " << prev->x << ", " << prev->y << std::endl;
		//std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
		switch (seam) {
		    case 1: //east/west
			if (prev->x < umid) {
			    segment[i].x = umin;
			} else {
			    segment[i].x = umax;
			}
			break;
		    case 2: //north/south
			if (prev->y < vmid) {
			    segment[i].y = vmin;
			} else {
			    segment[i].y = vmax;
			}
			break;
		    case 3: //both
			if (prev->x < umid) {
			    segment[i].x = umin;
			} else {
			    segment[i].x = umax;
			}
			if (prev->y < vmid) {
			    segment[i].y = vmin;
			} else {
			    segment[i].y = vmax;
			}
		}
		prev = &segment[i];
	    } else {
		//std::cerr << " at seam and no prev" << std::endl;
		complete = false;
	    }
	    prev_seam = seam;
	} else {
	    if (singularity < 0) {
		prev = &segment[i];
		prev_seam = 0;
	    } else {
		prev = NULL;
	    }
	}
    }
    prev_seam = 0;
    if ((!complete) && (prev != NULL)) {
	complete = true;
	for (int i = segment.Count() - 2; i >= 0; i--) {
	    int singularity = IsAtSingularity(surface, segment[i], PBC_SEAM_TOL);
	    int seam = IsAtSeam(surface, segment[i], PBC_SEAM_TOL);
	    if ((seam > 0)) {
		if (prev != NULL) {
		    //std::cerr << " at seam " << seam << " but has prev" << std::endl;
		    //std::cerr << "    prev: " << prev->x << ", " << prev->y << std::endl;
		    //std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
		    switch (seam) {
			case 1: //east/west
			    if (prev->x < umid) {
				segment[i].x = umin;
			    } else {
				segment[i].x = umax;
			    }
			    break;
			case 2: //north/south
			    if (prev->y < vmid) {
				segment[i].y = vmin;
			    } else {
				segment[i].y = vmax;
			    }
			    break;
			case 3: //both
			    if (prev->x < umid) {
				segment[i].x = umin;
			    } else {
				segment[i].x = umax;
			    }
			    if (prev->y < vmid) {
				segment[i].y = vmin;
			    } else {
				segment[i].y = vmax;
			    }
		    }
		    prev = &segment[i];
		} else {
		    //std::cerr << " at seam and no prev" << std::endl;
		    complete = false;
		}
	    } else {
		if (singularity < 0) {
		    prev = &segment[i];
		} else {
		    prev = NULL;
		}
	    }
	}
    }
    return complete;
}


/*
 * number_of_seam_crossings
 */
int
number_of_seam_crossings(std::list<PBCData*> &pbcs)
{
    int rc = 0;
    std::list<PBCData*>::iterator cs;

    cs = pbcs.begin();
    while (cs != pbcs.end()) {
	PBCData *data = (*cs);
	if (!data || !data->surf)
	    continue;

	const ON_Surface *surf = data->surf;
	std::list<ON_2dPointArray *>::iterator si = data->segments.begin();
	ON_2dPoint *pt = NULL;
	ON_2dPoint *prev_pt = NULL;
	ON_2dPoint *next_pt = NULL;
	while (si != data->segments.end()) {
	    ON_2dPointArray *samples = (*si);
	    for (int i = 0; i < samples->Count(); i++) {
		pt = &(*samples)[i];
		if (!IsAtSeam(surf,*pt,PBC_SEAM_TOL)) {
		    if (prev_pt == NULL) {
			prev_pt = pt;
		    } else {
			next_pt = pt;
		    }
		    int udir=0;
		    int vdir=0;
		    if (next_pt != NULL) {
			if (ConsecutivePointsCrossClosedSeam(surf,*prev_pt,*next_pt,udir,vdir,PBC_SEAM_TOL)) {
			    rc++;
			}
			prev_pt = next_pt;
			next_pt = NULL;
		    }
		}
	    }
	    if (si != data->segments.end())
		si++;
	}

	if (cs != pbcs.end())
	    cs++;
    }

    return rc;
}


/*
 * if current and previous point on seam make sure they are on same seam
 */
bool
check_for_points_on_same_seam(std::list<PBCData*> &pbcs)
{

    std::list<PBCData*>::iterator cs = pbcs.begin();
    ON_2dPoint *prev_pt = NULL;
    int prev_seam = 0;
    while ( cs != pbcs.end()) {
	PBCData *data = (*cs);
	const ON_Surface *surf = data->surf;
	std::list<ON_2dPointArray *>::iterator seg = data->segments.begin();
	while (seg != data->segments.end()) {
	    ON_2dPointArray *points = (*seg);
	    for (int i=0; i < points->Count(); i++) {
		ON_2dPoint *pt = points->At(i);
		int seam = IsAtSeam(surf,*pt,PBC_SEAM_TOL);
		if (seam > 0) {
		    if (prev_seam > 0) {
			if ((seam == 1) && ((prev_seam % 2) == 1)) {
			    pt->x = prev_pt->x;
			} else if ((seam == 2) && (prev_seam > 1)) {
			    pt->y = prev_pt->y;
			} else if (seam == 3) {
			    if ((prev_seam % 2) == 1) {
				pt->x = prev_pt->x;
			    }
			    if (prev_seam > 1) {
				pt->y = prev_pt->y;
			    }
			}
		    }
		    prev_seam = seam;
		    prev_pt = pt;
		}
	    }
	    seg++;
	}
	cs++;
    }
    return true;
}


/*
 * extend_pullback_at_shared_3D_curve_seam
 */
bool
extend_pullback_at_shared_3D_curve_seam(std::list<PBCData*> &pbcs)
{
    const ON_Curve *next_curve = NULL;
    std::set<const ON_Curve *> set;
    std::map<const ON_Curve *,int> map;
    std::list<PBCData*>::iterator cs = pbcs.begin();

    while ( cs != pbcs.end()) {
	PBCData *data = (*cs++);
	const ON_Curve *curve = data->curve;
	const ON_Surface *surf = data->surf;

	if (cs != pbcs.end()) {
	    PBCData *nextdata = (*cs);
	    next_curve = nextdata->curve;
	}

	if (curve == next_curve) {
	    std::cerr << "Consecutive seam usage" << std::endl;
	    //find which direction we need to extend
	    if (surf->IsClosed(0) && !surf->IsClosed(1)) {
		double length = surf->Domain(0).Length();
		std::list<ON_2dPointArray *>::iterator seg = data->segments.begin();
		while (seg != data->segments.end()) {
		    ON_2dPointArray *points = (*seg);
		    for (int i=0; i < points->Count(); i++) {
			points->At(i)->x = points->At(i)->x + length;
		    }
		    seg++;
		}
	    } else if (!surf->IsClosed(0) && surf->IsClosed(1)) {
		double length = surf->Domain(1).Length();
		std::list<ON_2dPointArray *>::iterator seg = data->segments.begin();
		while (seg != data->segments.end()) {
		    ON_2dPointArray *points = (*seg);
		    for (int i=0; i < points->Count(); i++) {
			points->At(i)->y = points->At(i)->y + length;
		    }
		    seg++;
		}
	    } else {
		std::cerr << "both directions" << std::endl;
	    }
	}
	next_curve = NULL;
    }
    return true;
}


/*
 * shift_closed_curve_split_over_seam
 */
bool
shift_single_curve_loop_straddled_over_seam(std::list<PBCData*> &pbcs)
{
    if (pbcs.size() == 1) { // single curve for this loop
	std::list<PBCData*>::iterator cs;

	PBCData *data = pbcs.front();
	if (!data || !data->surf)
	    return false;

	const ON_Surface *surf = data->surf;
	ON_Interval udom = surf->Domain(0);
	ON_Interval vdom = surf->Domain(1);
	std::list<ON_2dPointArray *>::iterator si = data->segments.begin();
	ON_2dPoint pt;
	ON_2dPoint prev_pt;
	if (data->curve->IsClosed()) {
	    int numseamcrossings = number_of_seam_crossings(pbcs);
	    if (numseamcrossings == 1) {
		ON_2dPointArray part1,part2;
		ON_2dPointArray* curr_point_array = &part2;
		while (si != data->segments.end()) {
		    ON_2dPointArray *samples = (*si);
		    for (int i = 0; i < samples->Count(); i++) {
			pt = (*samples)[i];
			if (i == 0) {
			    prev_pt = pt;
			    curr_point_array->Append(pt);
			    continue;
			}
			int udir= 0;
			int vdir= 0;
			if (ConsecutivePointsCrossClosedSeam(surf,pt,prev_pt,udir,vdir,PBC_SEAM_TOL)) {
			    if (surf->IsAtSeam(pt.x,pt.y) > 0) {
				SwapUVSeamPoint(surf, pt);
				curr_point_array->Append(pt);
				curr_point_array = &part1;
				SwapUVSeamPoint(surf, pt);
			    } else if (surf->IsAtSeam(prev_pt.x,prev_pt.y) > 0) {
				SwapUVSeamPoint(surf, prev_pt);
				curr_point_array->Append(prev_pt);
			    } else {
				std::cerr << "shift_single_curve_loop_straddled_over_seam(): Error expecting to see seam in sample points" << std::endl;
			    }
			}
			curr_point_array->Append(pt);
			prev_pt = pt;
		    }
		    samples->Empty();
		    samples->Append(part1.Count(),part1.Array());
		    samples->Append(part2.Count(),part2.Array());
		    if (si != data->segments.end())
			si++;
		}
	    }
	}
    }
    return true;
}


/*
 * extend_over_seam_crossings - all incoming points assumed to be within UV bounds
 */
bool
extend_over_seam_crossings(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator cs;
    ON_2dPoint *pt = NULL;
    ON_2dPoint *prev_non_seam_pt = NULL;
    ON_2dPoint *prev_pt = NULL;
    ON_2dVector curr_uv_offsets = ON_2dVector::ZeroVector;

    ///// Loop through and fix any seam ambiguities
    cs = pbcs.begin();
    while (cs != pbcs.end()) {
	PBCData *data = (*cs);
	if (!data || !data->surf)
	    continue;

	const ON_Surface *surf = data->surf;
	ON_Interval udom = surf->Domain(0);
	double ulength = udom.Length();
	ON_Interval vdom = surf->Domain(1);
	double vlength = vdom.Length();
	std::list<ON_2dPointArray *>::iterator si = data->segments.begin();
	while (si != data->segments.end()) {
	    ON_2dPointArray *samples = (*si);
	    for (int i = 0; i < samples->Count(); i++) {
		pt = &(*samples)[i];

		if (prev_pt != NULL) {
		    GetClosestExtendedPoint(surf,*pt,*prev_pt,PBC_SEAM_TOL);
		}
		prev_pt = pt;
	    }
	    if (si != data->segments.end())
		si++;
	}
	if (cs != pbcs.end())
	    cs++;
    }

    return true;
}


/*
 * run through curve loop to determine correct start/end
 * points resolving ambiguities when point lies on a seam or
 * singularity
 */
bool
resolve_pullback_seams(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator cs;

    ///// Loop through and fix any seam ambiguities
    ON_2dPoint *prev = NULL;
    ON_2dPoint *next = NULL;
    bool u_resolved = false;
    bool v_resolved = false;
    cs = pbcs.begin();
    while (cs != pbcs.end()) {
	PBCData *data = (*cs);
	if (!data || !data->surf)
	    continue;

	const ON_Surface *surf = data->surf;
	double umin, umax;
	double vmin, vmax;
	surf->GetDomain(0, &umin, &umax);
	surf->GetDomain(1, &vmin, &vmax);

	std::list<ON_2dPointArray *>::iterator si = data->segments.begin();
	while (si != data->segments.end()) {
	    ON_2dPointArray *samples = (*si);
	    if (resolve_seam_segment(surf, *samples,u_resolved,v_resolved)) {
		// Found a starting point
		//1) walk back up with resolved next point
		next = (*samples).First();
		std::list<PBCData*>::reverse_iterator rcs(cs);
		rcs--;
		std::list<ON_2dPointArray *>::reverse_iterator rsi(si);
		while (rcs != pbcs.rend()) {
		    PBCData *rdata = (*rcs);
		    while (rsi != rdata->segments.rend()) {
			ON_2dPointArray *rsamples = (*rsi);
			// first try and resolve on own merits
			if (!resolve_seam_segment(surf, *rsamples,u_resolved,v_resolved)) {
			    resolve_seam_segment_from_next(surf, *rsamples, next);
			}
			next = (*rsamples).First();
			rsi++;
		    }
		    rcs++;
		    if (rcs != pbcs.rend()) {
			rdata = (*rcs);
			rsi = rdata->segments.rbegin();
		    }
		}

		//2) walk rest of way down with resolved prev point
		if (samples->Count() > 1)
		    prev = &(*samples)[samples->Count() - 1];
		else
		    prev = NULL;
		si++;
		std::list<PBCData*>::iterator current(cs);
		while (cs != pbcs.end()) {
		    while (si != data->segments.end()) {
			samples = (*si);
			// first try and resolve on own merits
			if (!resolve_seam_segment(surf, *samples,u_resolved,v_resolved)) {
			    resolve_seam_segment_from_prev(surf, *samples, prev);
			}
			if (samples->Count() > 1)
			    prev = &(*samples)[samples->Count() - 1];
			else
			    prev = NULL;
			si++;
		    }
		    cs++;
		    if (cs != pbcs.end()) {
			data = (*cs);
			si = data->segments.begin();
		    }
		}
		// make sure to wrap back around with previous
		cs = pbcs.begin();
		data = (*cs);
		si = data->segments.begin();
		while ((cs != pbcs.end()) && (cs != current)) {
		    while (si != data->segments.end()) {
			samples = (*si);
			// first try and resolve on own merits
			if (!resolve_seam_segment(surf, *samples,u_resolved,v_resolved)) {
			    resolve_seam_segment_from_prev(surf, *samples, prev);
			}
			if (samples->Count() > 1)
			    prev = &(*samples)[samples->Count() - 1];
			else
			    prev = NULL;
			si++;
		    }
		    cs++;
		    if (cs != pbcs.end()) {
			data = (*cs);
			si = data->segments.begin();
		    }
		}
	    }
	    if (si != data->segments.end())
		si++;
	}
	if (cs != pbcs.end())
	    cs++;
    }
    return true;
}


/*
 * run through curve loop to determine correct start/end
 * points resolving ambiguities when point lies on a seam or
 * singularity
 */
bool
resolve_pullback_singularities(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator cs = pbcs.begin();

    ///// Loop through and fix any seam ambiguities
    ON_2dPoint *prev = NULL;
    bool complete = false;
    int checkcnt = 0;

    prev = NULL;
    complete = false;
    checkcnt = 0;
    while (!complete && (checkcnt < 2)) {
	cs = pbcs.begin();
	complete = true;
	checkcnt++;
	//std::cerr << "Checkcnt - " << checkcnt << std::endl;
	while (cs != pbcs.end()) {
	    int singularity;
	    prev = NULL;
	    PBCData *data = (*cs);
	    if (!data || !data->surf)
		continue;

	    const ON_Surface *surf = data->surf;
	    std::list<ON_2dPointArray *>::iterator si = data->segments.begin();
	    while (si != data->segments.end()) {
		ON_2dPointArray *samples = (*si);
		for (int i = 0; i < samples->Count(); i++) {
		    // 0 = south, 1 = east, 2 = north, 3 = west
		    if ((singularity = IsAtSingularity(surf, (*samples)[i], PBC_SEAM_TOL)) >= 0) {
			if (prev != NULL) {
			    //std::cerr << " at singularity " << singularity << " but has prev" << std::endl;
			    //std::cerr << "    prev: " << prev->x << ", " << prev->y << std::endl;
			    //std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
			    switch (singularity) {
				case 0: //south
				    (*samples)[i].x = prev->x;
				    (*samples)[i].y = surf->Domain(1)[0];
				    break;
				case 1: //east
				    (*samples)[i].y = prev->y;
				    (*samples)[i].x = surf->Domain(0)[1];
				    break;
				case 2: //north
				    (*samples)[i].x = prev->x;
				    (*samples)[i].y = surf->Domain(1)[1];
				    break;
				case 3: //west
				    (*samples)[i].y = prev->y;
				    (*samples)[i].x = surf->Domain(0)[0];
			    }
			    prev = NULL;
			    //std::cerr << "    curr now: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
			} else {
			    //std::cerr << " at singularity " << singularity << " and no prev" << std::endl;
			    //std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
			    complete = false;
			}
		    } else {
			prev = &(*samples)[i];
		    }
		}
		if (!complete) {
		    //std::cerr << "Lets work backward:" << std::endl;
		    for (int i = samples->Count() - 2; i >= 0; i--) {
			// 0 = south, 1 = east, 2 = north, 3 = west
			if ((singularity = IsAtSingularity(surf, (*samples)[i], PBC_SEAM_TOL)) >= 0) {
			    if (prev != NULL) {
				//std::cerr << " at singularity " << singularity << " but has prev" << std::endl;
				//std::cerr << "    prev: " << prev->x << ", " << prev->y << std::endl;
				//std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
				switch (singularity) {
				    case 0: //south
					(*samples)[i].x = prev->x;
					(*samples)[i].y = surf->Domain(1)[0];
					break;
				    case 1: //east
					(*samples)[i].y = prev->y;
					(*samples)[i].x = surf->Domain(0)[1];
					break;
				    case 2: //north
					(*samples)[i].x = prev->x;
					(*samples)[i].y = surf->Domain(1)[1];
					break;
				    case 3: //west
					(*samples)[i].y = prev->y;
					(*samples)[i].x = surf->Domain(0)[0];
				}
				prev = NULL;
				//std::cerr << "    curr now: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
			    } else {
				//std::cerr << " at singularity " << singularity << " and no prev" << std::endl;
				//std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
				complete = false;
			    }
			} else {
			    prev = &(*samples)[i];
			}
		    }
		}
		si++;
	    }
	    cs++;
	}
    }

    return true;
}


void
remove_consecutive_intersegment_duplicates(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator cs = pbcs.begin();
    while (cs != pbcs.end()) {
	PBCData *data = (*cs);
	std::list<ON_2dPointArray *>::iterator si = data->segments.begin();
	while (si != data->segments.end()) {
	    ON_2dPointArray *samples = (*si);
	    if (samples->Count() == 0) {
		si = data->segments.erase(si);
	    } else {
		for (int i = 0; i < samples->Count() - 1; i++) {
		    while ((i < (samples->Count() - 1)) && (*samples)[i].DistanceTo((*samples)[i + 1]) < 1e-9) {
			samples->Remove(i + 1);
		    }
		}
		si++;
	    }
	}
	if (data->segments.empty()) {
	    cs = pbcs.erase(cs);
	} else {
	    cs++;
	}
    }
}


bool
check_pullback_data(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator d = pbcs.begin();

    if ((*d) == NULL || (*d)->surf == NULL)
	return false;

    const ON_Surface *surf = (*d)->surf;
    bool singular = has_singularity(surf);
    bool closed = is_closed(surf);

    if (singular) {
	if (!resolve_pullback_singularities(pbcs)) {
	    std::cerr << "Error: Can not resolve singular ambiguities." << std::endl;
	}
    }

    if (closed) {
	// check for same 3D curve use
	if (!check_for_points_on_same_seam(pbcs)) {
	    std::cerr << "Error: Can not extend pullback at shared 3D curve seam." << std::endl;
	    return false;
	}
	// check for same 3D curve use
	if (!extend_pullback_at_shared_3D_curve_seam(pbcs)) {
	    std::cerr << "Error: Can not extend pullback at shared 3D curve seam." << std::endl;
	    return false;
	}
	if (!shift_single_curve_loop_straddled_over_seam(pbcs)) {
	    std::cerr << "Error: Can not resolve seam ambiguities." << std::endl;
	    return false;
	}
	if (!resolve_pullback_seams(pbcs)) {
	    std::cerr << "Error: Can not resolve seam ambiguities." << std::endl;
	    return false;
	}
	if (!extend_over_seam_crossings(pbcs)) {
	    std::cerr << "Error: Can not resolve seam ambiguities." << std::endl;
	    return false;
	}
    }

    // consecutive duplicates within segment will cause problems in curve fit
    remove_consecutive_intersegment_duplicates(pbcs);

    return true;
}


int
check_pullback_singularity_bridge(const ON_Surface *surf, const ON_2dPoint &p1, const ON_2dPoint &p2)
{
    if (has_singularity(surf)) {
	int is, js;
	if (((is = IsAtSingularity(surf, p1, PBC_SEAM_TOL)) >= 0) && ((js = IsAtSingularity(surf, p2, PBC_SEAM_TOL)) >= 0)) {
	    //create new singular trim
	    if (is == js) {
		return is;
	    }
	}
    }
    return -1;
}


int
check_pullback_seam_bridge(const ON_Surface *surf, const ON_2dPoint &p1, const ON_2dPoint &p2)
{
    if (is_closed(surf)) {
	int is, js;
	if (((is = IsAtSeam(surf, p1, PBC_SEAM_TOL)) > 0) && ((js = IsAtSeam(surf, p2, PBC_SEAM_TOL)) > 0)) {
	    //create new seam trim
	    if (is == js) {
		// need to check if seam 3d points are equal
		double endpoint_distance = p1.DistanceTo(p2);
		double t0, t1;

		int dir = is - 1;
		surf->GetDomain(dir, &t0, &t1);
		if (endpoint_distance > 0.5 * (t1 - t0)) {
		    return is;
		}
	    }
	}
    }
    return -1;
}


ON_Curve*
pullback_curve(const brlcad::SurfaceTree* surfacetree,
	       const ON_Surface* surf,
	       const ON_Curve* curve,
	       double tolerance,
	       double flatness)
{
    PBCData data;
    data.tolerance = tolerance;
    data.flatness = flatness;
    data.curve = curve;
    data.surf = surf;
    data.surftree = (brlcad::SurfaceTree*)surfacetree;
    ON_2dPointArray samples;
    data.segments.push_back(&samples);

    // Step 1 - adaptively sample the curve
    double tmin, tmax;
    data.curve->GetDomain(&tmin, &tmax);
    ON_2dPoint& start = samples.AppendNew(); // new point is added to samples and returned
    if (!toUV(data, start, tmin, 0.001)) {
	return NULL;    // fails if first point is out of tolerance!
    }

    ON_2dPoint uv;
    ON_3dPoint p = curve->PointAt(tmin);
    ON_3dPoint from = curve->PointAt(tmin + 0.0001);
    brlcad::SurfaceTree *st = (brlcad::SurfaceTree *)surfacetree;
    if (st->getSurfacePoint((const ON_3dPoint&)p, uv, (const ON_3dPoint&)from) < 0) {
	std::cerr << "Error: Can not get surface point." << std::endl;
    }

    ON_2dPoint p1, p2;

#ifdef SHOW_UNUSED
    if (!data.surf)
	return NULL;

    const ON_Surface *surf = data.surf;
#endif

    if (toUV(data, p1, tmin, PBC_TOL) && toUV(data, p2, tmax, -PBC_TOL)) {
#ifdef SHOW_UNUSED
	ON_3dPoint a = surf->PointAt(p1.x, p1.y);
	ON_3dPoint b = surf->PointAt(p2.x, p2.y);
#endif

	p = curve->PointAt(tmax);
	from = curve->PointAt(tmax - 0.0001);
	if (st->getSurfacePoint((const ON_3dPoint&)p, uv, (const ON_3dPoint&)from) < 0) {
	    std::cerr << "Error: Can not get surface point." << std::endl;
	}

	if (!sample(data, tmin, tmax, p1, p2)) {
	    return NULL;
	}

	for (int i = 0; i < samples.Count(); i++) {
	    std::cerr << samples[i].x << ", " << samples[i].y << std::endl;
	}
    } else {
	return NULL;
    }

    return interpolateCurve(samples);
}


ON_Curve*
pullback_seam_curve(enum seam_direction seam_dir,
		    const brlcad::SurfaceTree* surfacetree,
		    const ON_Surface* surf,
		    const ON_Curve* curve,
		    double tolerance,
		    double flatness)
{
    PBCData data;
    data.tolerance = tolerance;
    data.flatness = flatness;
    data.curve = curve;
    data.surf = surf;
    data.surftree = (brlcad::SurfaceTree*)surfacetree;
    ON_2dPointArray samples;
    data.segments.push_back(&samples);

    // Step 1 - adaptively sample the curve
    double tmin, tmax;
    data.curve->GetDomain(&tmin, &tmax);
    ON_2dPoint& start = samples.AppendNew(); // new point is added to samples and returned
    if (!toUV(data, start, tmin, 0.001)) {
	return NULL;    // fails if first point is out of tolerance!
    }

    ON_2dPoint p1, p2;

    if (toUV(data, p1, tmin, PBC_TOL) && toUV(data, p2, tmax, -PBC_TOL)) {
	if (!sample(data, tmin, tmax, p1, p2)) {
	    return NULL;
	}

	for (int i = 0; i < samples.Count(); i++) {
	    if (seam_dir == NORTH_SEAM) {
		samples[i].y = 1.0;
	    } else if (seam_dir == EAST_SEAM) {
		samples[i].x = 1.0;
	    } else if (seam_dir == SOUTH_SEAM) {
		samples[i].y = 0.0;
	    } else if (seam_dir == WEST_SEAM) {
		samples[i].x = 0.0;
	    }
	    std::cerr << samples[i].x << ", " << samples[i].y << std::endl;
	}
    } else {
	return NULL;
    }

    return interpolateCurve(samples);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

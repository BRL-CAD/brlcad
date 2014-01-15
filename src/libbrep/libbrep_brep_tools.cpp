/*          L I B B R E P _ B R E P _ T O O L S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file libbrep_brep_tools.cpp
 *
 * Utility routines for working with geometry.
 */

#include "common.h"

#include <vector>
#include <iostream>

#include "opennurbs.h"
#include "bu.h"
#include "libbrep_brep_tools.h"

bool ON_NearZero(double val, double epsilon) {
    return (val > -epsilon) && (val < epsilon);
}

double ON_Curve_Get_Tangent(int direction, const ON_Curve* curve, double min, double max, double zero_tol) {
    double mid;
    bool tanmin;
    ON_3dVector tangent;

    // first check end points
    tangent = curve->TangentAt(max);
    if (direction == 1 && ON_NearZero(tangent.y, zero_tol)) return max;
    if (direction == 0 && ON_NearZero(tangent.x, zero_tol)) return max;
    tangent = curve->TangentAt(min);
    if (direction == 1 && ON_NearZero(tangent.y, zero_tol)) return min;
    if (direction == 0 && ON_NearZero(tangent.y, zero_tol)) return min;

    tanmin = (tangent[direction] < 0.0);
    while ((max-min) > zero_tol) {
	mid = (max + min)/2.0;
	tangent = curve->TangentAt(mid);
	if (ON_NearZero(tangent[direction], zero_tol)) {
	    return mid;
	}
	if ((tangent[direction] < 0.0) == tanmin) {
	    min = mid;
	} else {
	    max = mid;
	}
    }
    return min;
}

double ON_Curve_Get_Horizontal_Tangent(const ON_Curve* curve, double min, double max, double zero_tol) {
    return ON_Curve_Get_Tangent(1, curve, min, max, zero_tol);
}

double ON_Curve_Get_Vertical_Tangent(const ON_Curve* curve, double min, double max, double zero_tol) {
    return ON_Curve_Get_Tangent(0, curve, min, max, zero_tol);
}

int ON_Curve_Has_Tangent(const ON_Curve* curve, double ct_min, double ct_max, double t_tol) {

    bool tanx1, tanx2, x_changed;
    bool tany1, tany2, y_changed;
    bool slopex, slopey;
    double xdelta, ydelta;
    ON_3dVector tangent1, tangent2;
    ON_3dPoint p1, p2;
    ON_Interval t(ct_min, ct_max);

    tangent1 = curve->TangentAt(t[0]);
    tangent2 = curve->TangentAt(t[1]);

    tanx1 = (tangent1[0] < 0.0);
    tanx2 = (tangent2[0] < 0.0);
    tany1 = (tangent1[1] < 0.0);
    tany2 = (tangent2[1] < 0.0);

    x_changed =(tanx1 != tanx2);
    y_changed =(tany1 != tany2);

    if (x_changed && y_changed) return 3; //horz & vert
    if (x_changed) return 1;//need to get vertical tangent
    if (y_changed) return 2;//need to find horizontal tangent

    p1 = curve->PointAt(t[0]);
    p2 = curve->PointAt(t[1]);

    xdelta = (p2[0] - p1[0]);
    slopex = (xdelta < 0.0);
    ydelta = (p2[1] - p1[1]);
    slopey = (ydelta < 0.0);

    // If we have no slope change
    // in x or y, we have a tangent line
    if (ON_NearZero(xdelta, t_tol) || ON_NearZero(ydelta, t_tol)) return 0;

    if ((slopex != tanx1) || (slopey != tany1)) return 3;

    return 0;
}

bool ON_Surface_IsFlat(ON_Plane *frames, double f_tol)
{
    double Ndot=1.0;

    for(int i=0; i<8; i++) {
	for( int j=i+1; j<9; j++) {
	    if ((Ndot = Ndot * frames[i].zaxis * frames[j].zaxis) < f_tol) {
		return false;
	    }
	}
    }

    return true;
}

bool ON_Surface_IsFlat_U(ON_Plane *frames, double f_tol)
{
    // check surface normals in U direction
    double Ndot = 1.0;
    if ((Ndot=frames[0].zaxis * frames[1].zaxis) < f_tol) {
	return false;
    } else if ((Ndot=Ndot * frames[2].zaxis * frames[3].zaxis) < f_tol) {
	return false;
    } else if ((Ndot=Ndot * frames[5].zaxis * frames[7].zaxis) < f_tol) {
	return false;
    } else if ((Ndot=Ndot * frames[6].zaxis * frames[8].zaxis) < f_tol) {
	return false;
    }

    // check for U twist within plane
    double Xdot = 1.0;
    if ((Xdot=frames[0].xaxis * frames[1].xaxis) < f_tol) {
	return false;
    } else if ((Xdot=Xdot * frames[2].xaxis * frames[3].xaxis) < f_tol) {
	return false;
    } else if ((Xdot=Xdot * frames[5].xaxis * frames[7].xaxis) < f_tol) {
	return false;
    } else if ((Xdot=Xdot * frames[6].xaxis * frames[8].xaxis) < f_tol) {
	return false;
    }

    return true;
}

bool ON_Surface_IsFlat_V(ON_Plane *frames, double f_tol)
{
    // check surface normals in V direction
    double Ndot = 1.0;
    if ((Ndot=frames[0].zaxis * frames[3].zaxis) < f_tol) {
	return false;
    } else if ((Ndot=Ndot * frames[1].zaxis * frames[2].zaxis) < f_tol) {
	return false;
    } else if ((Ndot=Ndot * frames[5].zaxis * frames[6].zaxis) < f_tol) {
	return false;
    } else if ((Ndot=Ndot * frames[7].zaxis * frames[8].zaxis) < f_tol) {
	return false;
    }

    // check for V twist within plane
    double Xdot = 1.0;
    if ((Xdot=frames[0].xaxis * frames[3].xaxis) < f_tol) {
	return false;
    } else if ((Xdot=Xdot * frames[1].xaxis * frames[2].xaxis) < f_tol) {
	return false;
    } else if ((Xdot=Xdot * frames[5].xaxis * frames[6].xaxis) < f_tol) {
	return false;
    } else if ((Xdot=Xdot * frames[7].xaxis * frames[8].xaxis) < f_tol) {
	return false;
    }

    return true;
}

bool ON_Surface_IsStraight(ON_Plane *frames, double s_tol)
{
    double Xdot=1.0;

    for(int i=0; i<8; i++) {
	for( int j=i+1; j<9; j++) {
	    if ((Xdot = Xdot * frames[0].xaxis * frames[1].xaxis) < s_tol) {
		return false;
	    }
	}
    }

    return true;
}

/**
   \brief Create surfaces and store their pointers in the t* arguments.

   For any pre-existing surface passed as one of the t* args, this is a no-op.

   @param t1 Pointer to pointer addressing first surface
   @param t2 Pointer to pointer addressing second surface
   @param t3 Pointer to pointer addressing third surface
   @param t4 Pointer to pointer addressing fourth surface
*/
void ON_Surface_Create_Scratch_Surfaces(
    ON_Surface **t1,
    ON_Surface **t2,
    ON_Surface **t3,
    ON_Surface **t4)
{
    if (!(*t1)) {
	ON_NurbsSurface *nt1 = ON_NurbsSurface::New();
	(*t1)= (ON_Surface *)(nt1);
    }
    if (!(*t2)) {
	ON_NurbsSurface *nt2 = ON_NurbsSurface::New();
	(*t2)= (ON_Surface *)(nt2);
    }
    if (!(*t3)) {
	ON_NurbsSurface *nt3 = ON_NurbsSurface::New();
	(*t3)= (ON_Surface *)(nt3);
    }
    if (!(*t4)) {
	ON_NurbsSurface *nt4 = ON_NurbsSurface::New();
	(*t4)= (ON_Surface *)(nt4);
    }
}

bool ON_Surface_SubSurface(
    const ON_Surface *srf,
    ON_Interval *u_val,
    ON_Interval *v_val,
    ON_Surface **t1,
    ON_Surface **t2,
    ON_Surface **t3,
    ON_Surface **t4,
    ON_Surface **result
    )
{
    bool split = true;
    ON_Surface **target;
    int last_split = 0;
    int t1_del, t2_del, t3_del;

    // Make sure we have intervals with non-zero lengths
    if ((u_val->Length() <= ON_ZERO_TOLERANCE) || (v_val->Length() <= ON_ZERO_TOLERANCE))
	return false;

    // If we have the original surface domain, just return true
    if ((fabs(u_val->Min() - srf->Domain(0).m_t[0]) <= ON_ZERO_TOLERANCE) &&
	(fabs(u_val->Max() - srf->Domain(0).m_t[1]) <= ON_ZERO_TOLERANCE) &&
	(fabs(v_val->Min() - srf->Domain(1).m_t[0]) <= ON_ZERO_TOLERANCE) &&
	(fabs(v_val->Max() - srf->Domain(1).m_t[1]) <= ON_ZERO_TOLERANCE)) {
	(*result) = (ON_Surface *)srf;
	return true;
    }
    if (fabs(u_val->Min() - srf->Domain(0).m_t[0]) > ON_ZERO_TOLERANCE) last_split = 1;
    if (fabs(u_val->Max() - srf->Domain(0).m_t[1]) > ON_ZERO_TOLERANCE) last_split = 2;
    if (fabs(v_val->Min() - srf->Domain(1).m_t[0]) > ON_ZERO_TOLERANCE) last_split = 3;
    if (fabs(v_val->Max() - srf->Domain(1).m_t[1]) > ON_ZERO_TOLERANCE) last_split = 4;
    t1_del = (*t1) ? (0) : (1);
    t2_del = (*t2) ? (0) : (1);
    t3_del = (*t3) ? (0) : (1);
    ON_Surface *ssplit = (ON_Surface *)srf;
    ON_Surface_Create_Scratch_Surfaces(t1, t2, t3, t4);
    if (fabs(u_val->Min() - srf->Domain(0).m_t[0]) > ON_ZERO_TOLERANCE) {
	if (last_split == 1) {target = t4;} else {target = t2;}
	split = ssplit->Split(0, u_val->Min(), *t1, *target);
	ssplit = *target;
    }
    if ((fabs(u_val->Max() - srf->Domain(0).m_t[1]) > ON_ZERO_TOLERANCE) && split) {
	if (last_split == 2) {target = t4;} else {target = t1;}
	split = ssplit->Split(0, u_val->Max(), *target, *t3);
	ssplit = *target;
    }
    if ((fabs(v_val->Min() - srf->Domain(1).m_t[0]) > ON_ZERO_TOLERANCE) && split) {
	if (last_split == 3) {target = t4;} else {target = t3;}
	split = ssplit->Split(1, v_val->Min(), *t2, *target);
	ssplit = *target;
    }
    if ((fabs(v_val->Max() - srf->Domain(1).m_t[1]) > ON_ZERO_TOLERANCE) && split) {
	split = ssplit->Split(1, v_val->Max(), *t4, *t2);
    }
    (*result) = *t4;
    if (t1_del) delete *t1;
    if (t2_del) delete *t2;
    if (t3_del) delete *t3;
    (*result)->SetDomain(0,u_val->Min(), u_val->Max());
    (*result)->SetDomain(1,v_val->Min(), v_val->Max());
    return split;
}

bool ON_Surface_Quad_Split(
    const ON_Surface *surf,
    const ON_Interval& u,
    const ON_Interval& v,
    double upt,
    double vpt,
    ON_Surface **q0,
    ON_Surface **q1,
    ON_Surface **q2,
    ON_Surface **q3)
{
    bool split_success = true;
    ON_Surface *north = NULL;
    ON_Surface *south = NULL;

    // upt and vpt must be within their respective domains
    if (!u.Includes(upt, true) || !v.Includes(vpt, true)) return false;

    // All four output surfaces should be NULL - the point of this function is to create them
    if ((*q0) || (*q1) || (*q2) || (*q3)) {
	bu_log("ON_Surface_Quad_Split was supplied non-NULL surfaces as output targets: q0: %p, q1: %p, q2: %p, q3: %p\n",
	       static_cast<void *>(*q0), static_cast<void *>(*q1), static_cast<void *>(*q2), static_cast<void *>(*q3));
	return false;
    }

    // First, get the north and south pieces
    split_success = surf->Split(1, vpt, south, north);
    if (!split_success || !south || !north) {
	delete south;
	delete north;
	return false;
    }

    // Split the south pieces to get q0 and q1
    split_success = south->Split(0, upt, (*q0), (*q1));
    if (!split_success || !(*q0) || !(*q1)) {
	delete south;
	delete north;
	if (*q0) delete (*q0);
	if (*q1) delete (*q1);
	return false;
    }

    // Split the north pieces to get q2 and q3
    split_success = north->Split(0, upt, (*q3), (*q2));
    if (!split_success || !(*q3) || !(*q2)) {
	delete south;
	delete north;
	if (*q0) delete (*q0);
	if (*q1) delete (*q1);
	if (*q2) delete (*q2);
	if (*q3) delete (*q3);
	return false;
    }

    delete south;
    delete north;

    return true;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

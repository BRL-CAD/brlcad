/*          L I B B R E P _ B R E P _ T O O L S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#include <vector>
#include <iostream>

#include "opennurbs.h"

// For any pre-existing surface passed as one of the t* args, this is a no-op
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



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

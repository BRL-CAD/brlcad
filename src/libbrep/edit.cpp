/*                        E D I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 1994-2023 United States Government as represented by
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
/** @file edit.cpp
 *
 * Implementation of edit support for brep.
 *
 */

#include "brep/edit.h"
#include "bu/log.h"

void *brep_create()
{
    ON_Brep* brep = new ON_Brep();
    return (void *)brep;
}

ON_NurbsCurve *brep_make_curve(int argc, const char **argv)
{
    ON_NurbsCurve *curve = new ON_NurbsCurve(3, true, 3, 4);
    curve->SetCV(0, ON_3dPoint(-0.1, -1.5, 0));
    curve->SetCV(1, ON_3dPoint(0.1, -0.5, 0));
    curve->SetCV(2, ON_3dPoint(0.1, 0.5, 0));
    curve->SetCV(3, ON_3dPoint(-0.1, 1.5, 0));
    curve->SetKnot(0, 0);
    curve->SetKnot(1, 0);
    curve->SetKnot(2, 0.5);
    curve->SetKnot(3, 1);
    curve->SetKnot(4, 1);
    if (argc == 3)
        curve->Translate(ON_3dVector(atof(argv[0]), atof(argv[1]), atof(argv[2])));
    return curve;
}


// TODO: add more options about knot vector
ON_NurbsCurve *brep_in_curve(int argc, const char **argv)
{
    bool is_rational = atoi(argv[0]);
    int order = atoi(argv[1]);
    int cv_count = atoi(argv[2]);
    int dim = 3;
    if (argc != 3 + cv_count * (dim + is_rational))
    {
        return NULL;
    }

    ON_NurbsCurve *curve = new ON_NurbsCurve(dim, is_rational, order, cv_count);
    
    for (int i = 0; i < cv_count; i++)
    {
    curve->SetCV(i, ON_3dPoint(atof(argv[3 + i * (dim + is_rational)]), atof(argv[4 + i * (dim + is_rational)]), atof(argv[5 + i * (dim + is_rational)])));
    if (is_rational)
        curve->SetWeight(i, atof(argv[6 + i * (dim + is_rational)]));
    }

    // make uniform knot vector
    curve->MakeClampedUniformKnotVector();
    return curve;
}

ON_NurbsCurve *brep_get_nurbs_curve(ON_Brep* brep, int curve_id)
{
    if(curve_id<0 || curve_id>=brep->m_C3.Count())
    {
		bu_log("curve_id is out of range\n");
        return NULL;
    }
    ON_NurbsCurve *curve = dynamic_cast<ON_NurbsCurve *>(brep->m_C3[curve_id]);

    if (!curve)
    {
		bu_log("curve %d is not a NURBS curve\n", curve_id);
        return NULL;
    }
    return curve;
}

bool brep_curve_move_cv(ON_Brep* brep, int curve_id, int cv_id, ON_4dPoint point)
{
    ON_NurbsCurve *curve = brep_get_nurbs_curve(brep, curve_id);
    if (!curve)
    {
        return false;
    }
    return curve->SetCV(cv_id, point);
}

bool brep_curve_reverse(ON_Brep* brep, int curve_id)
{
    ON_NurbsCurve *curve = brep_get_nurbs_curve(brep, curve_id);
    if (!curve)
    {
        return false;
    }
    return curve->Reverse();
}

bool brep_curve_insert_knot(ON_Brep* brep, int curve_id, double knot, int multiplicity)
{
    ON_NurbsCurve *curve = brep_get_nurbs_curve(brep, curve_id);
    if (!curve)
    {
        return false;
    }
    return curve->InsertKnot(knot, multiplicity);
}

bool brep_curve_trim(ON_Brep* brep, int curve_id, double t0, double t1)
{
    ON_NurbsCurve *curve = brep_get_nurbs_curve(brep, curve_id);
    if (!curve)
    {
        return false;
    }
    ON_Interval interval(t0, t1);
    return curve->Trim(interval);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

/*           S U R F A C E I N T E R S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
/** @file surfaceintersect.cpp
 *
 */

/* Until further notice this code is in a state of heavy flux as part of
 * GSoC 2009 as such it would be very foolish to write anything that
 * depends on it right now
 * This code is written and maintained by Joe Doliner jdoliner@gmail.com */

#include "surfaceintersect.h"

/**
 *        step
 *
 * @brief advances s1, s2, t1, t2 along the curve of intersection of the two surfaces
 * by a distance of step size.
 */
void Step(
	ON_Surface *surf1,
	ON_Surface *surf2,
	double *s1,
	double *s2,
	double *t1,
	double *t2,
	double stepsize
	)
{
    ON_3dVector Norm1 = surf1->NormalAt(*s1, *t1);
    ON_3dVector Norm2 = surf2->NormalAt(*s2, *t2);
    ON_3dVector step = ON_CrossProduct(Norm1, Norm2);
    double Magnitude = ON_ArrayMagnitude(3, step);
    double vec[3] = {0.0, 0.0, 0.0};
    ON_3dVector stepscaled =  vec;
    ON_ArrayScale(3, sqrt(stepsize/Magnitude), step, stepscaled);
    ON_3dPoint value1, value2;
    ON_3dVector ds1, ds2, dt1, dt2; /* the partial derivatives */
    assert(surf1->Ev1Der(*s1, *t1, value1, ds1, dt1));
    assert(surf2->Ev1Der(*s2, *t2, value2, ds2, dt2));
    double delta_s1, delta_s2, delta_t1, delta_t2;
    ON_DecomposeVector(stepscaled, ds1, dt1, &delta_s1, &delta_t1);
    ON_DecomposeVector(stepscaled, ds2, dt2, &delta_s2, &delta_t2);
    *s1 += delta_s1;
    *s2 += delta_s2;
    *t1 += delta_t1;
    *t2 += delta_t2;
}

void WalkIntersection(
	ON_Surface *surf1,
	ON_Surface *surf2,
	double s1,
	double s2,
	double t1,
	double t2,
	double stepsize,
	ON_BezierCurve *out
	)
{
    ON_Polyline intersectionPoints;
    while (surf1->Domain(0).Includes(s1, true) && surf1->Domain(1).Includes(t1, true) && surf2->Domain(0).Includes(s2, true) && surf2->Domain(1).Includes(t2, true) && !intersectionPoints.IsClosed(stepsize)) {
	intersectionPoints.Append(surf1->PointAt(s1, t1));
	Step(surf1, surf2, &s1, &s2, &t1, &t2, stepsize);
    }
    *out = ON_BezierCurve(intersectionPoints);
}

int main()
{
    return 0;
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

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
#define MIN(a, b) (((a) > (b)) ? (a) : (b))

/**
 *       ClosestValue
 *
 * @brief returns the value that is closest to the given value but in the given interval
 */
double ClosestValue(
	double value,
	ON_Interval interval
	)
{
    if (interval.Includes(value, true)) {
	return value;
    } else if (value < interval.Min()) {
	return interval.Min();
    } else {
	return interval.Max();
    }
}

/**
 *        Push
 *
 * @brief updates s and t using an X,Y,Z vector
 */
void Push(
	ON_Surface *surf,
	double *s,
	double *t,
	ON_3dVector vec
	)
{
    ON_3dVector ds, dt;
    ON_3dPoint value;
    assert(surf->Ev1Der(*s, *t, value, ds, dt));
    double delta_s, delta_t;
    ON_DecomposeVector(vec, ds, dt, &delta_s, &delta_t);
    *s += delta_s;
    *t += delta_t;
}

/**
 *        Step
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
    Push(surf1, s1, t1, stepscaled);
    Push(surf2, s2, t2, stepscaled);
}

/**
 *        Jiggle
 *
 * @brief uses newtonesque method to jiggle the points on the surfaces about and find a closer
 * point returns the new distance between the points
 */
double Jiggle(
	ON_Surface *surf1,
	ON_Surface *surf2,
	double *s1,
	double *s2,
	double *t1,
	double *t2,
	double stepsize
	)
{
    ON_3dPoint p1 = surf1->PointAt(*s1, *t1);
    ON_3dPoint p2 = surf2->PointAt(*s2, *t2);
    ON_3dVector Norm1 = surf1->NormalAt(*s1, *t1);
    ON_3dVector Norm2 = surf2->NormalAt(*s2, *t2);
    ON_3dVector p1p2 = p2 - p1, p2p1 = p1 - p2;
    ON_3dVector p1p2orth, p1p2prl, p2p1orth, p2p1prl;
    VPROJECT(p1p2, Norm1, p1p2prl, p1p2orth);
    VPROJECT(p2p1, Norm2, p2p1prl, p2p1orth);
    Push(surf1, s1, t1, p1p2orth / 2);
    Push(surf2, s2, t2, p2p1orth / 2);
    return surf1->PointAt(*s1, *t1).DistanceTo(surf2->PointAt(*s2, *t2));
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
    double olds1, olds2, oldt1, oldt2;
    while (surf1->Domain(0).Includes(s1, true) && surf1->Domain(1).Includes(t1, true) && surf2->Domain(0).Includes(s2, true) && surf2->Domain(1).Includes(t2, true) && !intersectionPoints.IsClosed(stepsize)) {
	intersectionPoints.Append(surf1->PointAt(s1, t1));
	olds1 = s1;
	olds2 = s2;
	oldt1 = t1;
	oldt2 = t2;
	Step(surf1, surf2, &s1, &s2, &t1, &t2, stepsize);
    }
    if (!intersectionPoints.IsClosed(stepsize)) {
	/* we stepped off the edge of our domain, we need to get a point right on the edge */
	double news1, news2, newt1, newt2;
	news1 = ClosestValue(s1, surf1->Domain(0));
	news2 = ClosestValue(s2, surf2->Domain(1));
	newt1 = ClosestValue(t1, surf1->Domain(0));
	newt2 = ClosestValue(t2, surf2->Domain(1));
	news1 = olds1 + MIN((news1 - olds1) / (s1 - olds1), (newt1 - oldt1) / (t1 - oldt1)) * (s1 - olds1);
	news2 = olds2 + MIN((news2 - olds2) / (s2 - olds2), (newt2 - oldt2) / (t2 - oldt2)) * (s2 - olds2);
	newt1 = oldt1 + MIN((news1 - olds1) / (s1 - olds1), (newt1 - oldt1) / (t1 - oldt1)) * (t1 - oldt1);
	newt2 = oldt2 + MIN((news2 - olds2) / (s2 - olds2), (newt2 - oldt2) / (t2 - oldt2)) * (t2 - oldt2);
	double newstep = MIN(surf1->PointAt(olds1, oldt1).DistanceTo(surf1->PointAt(news1, newt1)), surf2->PointAt(olds2, oldt2).DistanceTo(surf2->PointAt(news2, newt2)));
	Step(surf1, surf2, &s1, &s2, &t1, &t2, newstep);
	intersectionPoints.Append(surf1->PointAt(s1, t1));
    }
    *out = ON_BezierCurve(intersectionPoints);
}

bool GetStartPoints(
	ON_Surface *surf1,
	ON_Surface *surf2,
	ON_2dPointArray start_points1,
	ON_2dPointArray start_points2,
	double tol
	)
{
    bool return_value;
    if (surf1->BoundingBox().IsDisjoint(surf2->BoundingBox())) {
	return_value = false;
    } else if (surf1->IsPlanar(NULL, tol) && surf2->IsPlanar(NULL, tol)) {
	if (!surf1->BoundingBox().IsDisjoint(surf2->BoundingBox())) {
	    start_points1.Append(ON_2dPoint(surf1->Domain(0).Mid(), surf1->Domain(1).Mid()));
	    start_points2.Append(ON_2dPoint(surf2->Domain(0).Mid(), surf2->Domain(1).Mid()));
	} else {
	    return_value = false;
	}
    } else {
	ON_Surface *N1, *S1, *N2, *S2, *Parts1[4], *Parts2[4]; /* = {SW, SE, NW, NE} */
	assert(surf1->Split(0, surf1->Domain(0).Mid(), S1, N1));
	assert(surf2->Split(0, surf2->Domain(0).Mid(), S2, N2));
	assert(S1->Split(1, S1->Domain(1).Mid(), Parts1[0], Parts1[1]));
	assert(N1->Split(1, N1->Domain(1).Mid(), Parts1[2], Parts1[3]));
	assert(S2->Split(1, S2->Domain(1).Mid(), Parts2[0], Parts2[1]));
	assert(N2->Split(1, N2->Domain(1).Mid(), Parts2[2], Parts2[3]));
	int i,j;
	return_value = false;
	for (i = 0; i < 4; i++) {
	    for (j = 0; j < 4; j++) {
		return_value = return_value && GetStartPoints(Parts1[i], Parts2[j], start_points1, start_points2, tol);
	    }
	}
    }
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

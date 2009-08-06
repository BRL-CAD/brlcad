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
#define safesqrt(x) (((x) >= 0) ? sqrt(x) : (-(sqrt(-x))))

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
	const ON_Surface *surf,
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
	const ON_Surface *surf1,
	const ON_Surface *surf2,
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
    ON_ArrayScale(3, safesqrt(stepsize/Magnitude), step, stepscaled);
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
	const ON_Surface *surf1,
	const ON_Surface *surf2,
	double *s1,
	double *s2,
	double *t1,
	double *t2
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

/**
 *         FaceLoopIntersect
 *
 * @brief Intersects a new curve in the face's domain with the
 *  existing loops returns the number of intersections
 */
int FaceLoopIntersect(
	ON_BrepFace& face,
	ON_Curve *curve,
	ON_SimpleArray<ON_X_EVENT>& x,
	double tol
	)
{
    int i, j;
    int rv = 0;
    for (i = 0; i < face.LoopCount(); i++) {
	ON_BrepLoop* loop = face.Loop(i);
	for (j = 0; j < loop->TrimCount(); j++) {
	    ON_BrepTrim *trim = loop->Trim(j);
	    rv += trim->IntersectCurve(curve, x, tol, tol);
	}
    }
}



/**
 *        SplitTrim
 *
 * @brief Split's a trim at a point, and replaces the references to that trim
 * with the pieces
 */
void SplitTrim(
	ON_BrepTrim *trim,
	double t
	)
{
    ON_Curve *left, *right;
    bool rv = trim->Split(t, left, right);
    if (rv) {
	trim->Brep()->AddTrimCurve(left);
	trim->Brep()->AddTrimCurve(right);
    }
    /* now we need to remove the original trim */
    /* it's not quite clear what we'll be doing after that at this point */
}

/**
 *        TrimCompareStart
 *
 * @brief sorts the trims by their start points, uses a pretty convoluted method to get a 
 * complete ordering on R^3, but you don't actually need to understand the ordering,
 * any ordering would work just as well.
 */
int TrimCompareStart(
	const ON_BrepTrim *a,
	const ON_BrepTrim *b
	)
{
    ON_3dVector A = ON_3dVector(a->Face()->SurfaceOf()->PointAt(a->PointAtStart().x, a->PointAtStart().y));
    ON_3dVector B = ON_3dVector(b->Face()->SurfaceOf()->PointAt(b->PointAtStart().x, b->PointAtStart().y));
    if (VAPPROXEQUAL(A, B, 1e-9)) {
	return 0;
    }
    else if (A.x < B.x) {
	return -1;
    } else if (A.x > B.x) {
	return 1;
    } else if (A.y < B.y) {
	return -1;
    } else if (A.y > B.y) {
	return 1;
    } else if (A.z < B.z) {
	return -1;
    } else if (A.z > B.z) {
	return 1;
    } else {
	return 0; /* we should have already done this... but just in case */
    }
}

/**
 *          TrimCompareEnd
 *
 * @brief same as the function above but this one does things with end points
 * it's a bit :( that we need to duplicate this code.
 */
int TrimCompareEnd(
	const ON_BrepTrim *a,
	const ON_BrepTrim *b
	)
{
    ON_3dVector A = ON_3dVector(a->Face()->SurfaceOf()->PointAt(a->PointAtEnd().x, a->PointAtEnd().y));
    ON_3dVector B = ON_3dVector(b->Face()->SurfaceOf()->PointAt(b->PointAtEnd().x, b->PointAtEnd().y));
    if (VAPPROXEQUAL(A, B, 1e-9)) {
	return 0;
    }
    else if (A.x < B.x) {
	return -1;
    } else if (A.x > B.x) {
	return 1;
    } else if (A.y < B.y) {
	return -1;
    } else if (A.y > B.y) {
	return 1;
    } else if (A.z < B.z) {
	return -1;
    } else if (A.z > B.z) {
	return 1;
    } else {
	return 0; /* we should have already done this... but just in case */
    }
}

/**
 *        MakeLoops
 *
 * @brief Makes loops out of the new trims
 */
void MakeLoops(
	ON_SimpleArray<ON_BrepTrim> trims,
	double tol
	)
{

}

/**
 *        ReconstructX_Events
 *
 * @brief Walks an intersection list, the list should include every intersection the
 * curve and its twin have with the trims of their respective faces
 */
void  ReconstructX_Events(
	ON_SimpleArray<ON_X_EVENT>& x,
	bool& isactive /* active points must map to active points in on both surfaces */
	)
{
    int i;
    for (i = 0; i < x.Count(); i++) {
	if (isactive) {
	} else {
	}
    }
}

/**
 *        IsClosed
 *
 * @check if a 2dPointarrray is closed to be closed an array must have >2 points in it, have the first and last points within tol of one another
 * and have at least one point not within tol of either of them.
 */
bool IsClosed(
	const ON_2dPointArray l,
	double tol
	)
{
    if (l.Count() < 3) {
	return false;
    } else if (V2APPROXEQUAL(l[0], l[l.Count() - 1], tol)) {
	int i;
	for (i = 1; i < l.Count() - 1; i++) {
	    if (!V2APPROXEQUAL(l[0], l[i], tol) && !V2APPROXEQUAL(l[l.Count() - 1], l[i], tol)) {
		return true;
	    }
	}
    } else {
	return false;
    }
}

/**
 *        WalkIntersection
 *
 * @brief walks the intersection between 2 brepfaces,
 * returns lines segmented by the trimming curves
 */

void WalkIntersection(
	ON_Surface *surf1,
	ON_Surface *surf2,
	double s1,
	double s2,
	double t1,
	double t2,
	double stepsize,
	double tol,
	ON_Curve **out1,
	ON_Curve **out2
	)
{
    ON_2dPointArray intersectionPoints1, intersectionPoints2;
    double olds1, olds2, oldt1, oldt2;
    double inits1 = s1, inits2 = s2, initt1 = t1, initt2 = t2;
    double distance;
    int passes;
    /* this function is meant to be called with an arbitrary point on the curve
       and return the entire curve, regardless of which point was passed in,
       this means that if the intersection doesn't happen to be a loop,
       we need to walk both directions from the initial point. */
    for (passes = 0; passes < 2; passes++) {
	while (surf1->Domain(0).Includes(s1, true) && surf1->Domain(1).Includes(t1, true) && surf2->Domain(0).Includes(s2, true) && surf2->Domain(1).Includes(t2, true) && !(IsClosed(intersectionPoints1, stepsize) && IsClosed(intersectionPoints2, stepsize))) {
	    do {
		distance = Jiggle(surf1, surf2, &s1, &s2, &t1, &t2);
	    } while (distance > tol);
	    intersectionPoints1.Append(ON_2dPoint(s1, t1));
	    intersectionPoints2.Append(ON_2dPoint(s2, t2));
	    olds1 = s1;
	    olds2 = s2;
	    oldt1 = t1;
	    oldt2 = t2;
	    if (passes == 0) {
		Step(surf1, surf2, &s1, &s2, &t1, &t2, stepsize);
	    } else {
		Step(surf1, surf2, &s1, &s2, &t1, &t2, -stepsize);
	    }
	}
	if (!(IsClosed(intersectionPoints1, stepsize) && IsClosed(intersectionPoints2, stepsize))) {
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
	    intersectionPoints1.Append(ON_2dPoint(s1, t1));
	    intersectionPoints2.Append(ON_2dPoint(s2, t2));
	}
	if (IsClosed(intersectionPoints1, stepsize) && IsClosed(intersectionPoints2, stepsize)) {
	    break; /* we're done, no second pass required */
	} else {
	    intersectionPoints1.Reverse(); /* this is a bit cute, we are assured to hit this part on the second pass, so we either reverse 0 or 2 times */
	    intersectionPoints2.Reverse();
	    s1 = inits1;
	    s2 = inits2;
	    t1 = initt1;
	    t2 = initt2;
	}
    }
    *out1 = (ON_Curve *) new ON_BezierCurve((ON_2dPointArray) intersectionPoints1);
    *out2 = (ON_Curve *) new ON_BezierCurve((ON_2dPointArray) intersectionPoints2);
}

/**
 *        GetStartPointsInternal
 *
 * @brief Subdibivides the surface recursively to zoom in on intersection
 * points internal to the surfaces
 */

bool GetStartPoints(
	ON_Surface *surf1,
	ON_Surface *surf2,
	ON_2dPointArray& start_points1,
	ON_2dPointArray& start_points2,
	double tol
	)
{
    bool return_value;
    if (surf1->BoundingBox().IsDisjoint(surf2->BoundingBox())) {
	return_value = false;
    } else if (surf1->IsPlanar(NULL, tol) && surf2->IsPlanar(NULL, tol)) {
	if (!surf1->BoundingBox().IsDisjoint(surf2->BoundingBox())) {
	    double distance, s1 = surf1->Domain(0).Mid(), s2 = surf2->Domain(0).Mid(), t1 = surf1->Domain(1).Mid(), t2 = surf2->Domain(1).Mid();
	    do {
		distance = Jiggle(surf1, surf2, &s1, &s2, &t1, &t2);
	    } while (distance > tol);
	    start_points1.Append(ON_2dPoint(s1, t1));
	    start_points2.Append(ON_2dPoint(s2, t2));
	    return_value = true;
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
    return return_value;
}

/**
 *        GetStartPointsEdges
 *
 * @brief Find starting points that are on the edges of the surfaces
 */
bool GetStartPointsEdges(
	ON_Surface *surf1,
	ON_Surface *surf2,
	ON_2dPointArray& start_points1,
	ON_2dPointArray& start_points2,
	double tol
	)
{
    bool rv = false;
    ON_Interval intervals[4]; /* {s1, t1, s2, t2} */
    intervals[0] = surf1->Domain(0);
    intervals[1] = surf1->Domain(1);
    intervals[2] = surf2->Domain(0);
    intervals[3] = surf2->Domain(1);
    ON_Surface *surfaces[2] = {surf1, surf2};
    ON_2dPointArray out[2] = {start_points1, start_points2};
    ON_SimpleArray<ON_X_EVENT> x;
    surf1->IsoCurve(1, intervals[0].Min())->IntersectSurface(surf2, x, tol, tol);
    surf1->IsoCurve(1, intervals[0].Max())->IntersectSurface(surf2, x, tol, tol);
    surf1->IsoCurve(0, intervals[1].Min())->IntersectSurface(surf2, x, tol, tol);
    surf1->IsoCurve(0, intervals[1].Max())->IntersectSurface(surf2, x, tol, tol);
    surf2->IsoCurve(1, intervals[2].Min())->IntersectSurface(surf1, x, tol, tol);
    surf2->IsoCurve(1, intervals[2].Max())->IntersectSurface(surf1, x, tol, tol);
    surf2->IsoCurve(0, intervals[3].Min())->IntersectSurface(surf1, x, tol, tol);
    surf2->IsoCurve(0, intervals[3].Max())->IntersectSurface(surf1, x, tol, tol);
    int curve; /* the surface the curves come from */
    int dir; /* the parameter that varies in the iso curve */
    int min_or_max; /* 0 = min, 1 = max */
    for (curve = 0; curve < 2; curve++) {
	for (dir = 0; dir < 2; dir++) {
	    for (min_or_max = 0; min_or_max < 2; min_or_max++) {
		if (min_or_max == 0) {
		    surfaces[curve]->IsoCurve(1 - dir, intervals[dir + (2 * curve)].Min())->IntersectSurface(surfaces[curve], x, tol, tol);
		} else {
		    surfaces[curve]->IsoCurve(1 - dir, intervals[dir + (2 * curve)].Max())->IntersectSurface(surfaces[curve], x, tol, tol);
		}
		int i;
		for (i = 0; i < x.Count(); i++) {
		    rv = true; /* if we get here it means we've found a point */
		    if (dir == 0) {
			if (min_or_max == 0) {
			    out[curve].Append(ON_2dPoint(x[i].m_a[0], intervals[dir + (2 * curve)].Min()));
			} else {
			    out[curve].Append(ON_2dPoint(x[i].m_a[0], intervals[dir + (2 * curve)].Min()));
			}
		    }
		    else {
			if (min_or_max == 0) {
			    out[curve].Append(ON_2dPoint(x[i].m_a[0], intervals[(1 - dir) + (2 * curve)].Min()));
			} else {
			    out[curve].Append(ON_2dPoint(x[i].m_a[0], intervals[(1 - dir) + (2 * curve)].Min()));
			}
		    }
		    out[1 - curve].Append(ON_2dPoint(x[i].m_b[0], x[i].m_b[1])); 
		}
	    }
	}
    }
    return rv;
}


/**
 *        SurfaceSurfaceIntersect
 *
 * @brief finds, as bezier curves in the surfaces's parameter spaces the curves of intersection of the two surfaces
 */

bool SurfaceSurfaceIntersect(
	ON_Surface *surf1,
	ON_Surface *surf2,
	double stepsize,
	double tol,
	ON_ClassArray<ON_Curve>& curves1,
	ON_ClassArray<ON_Curve>& curves2
	)
{
    ON_2dPointArray start_points1, start_points2;
    bool rv = GetStartPoints(surf1, surf2, start_points1, start_points2, tol);
    if (!rv) {
	return false;
    }
    int i, j;
    ON_Curve *out1, *out2;
    ON_2dPoint start1, start2;
    for (i = 0; i < start_points1.Count(); i++) {
	start1 = *start_points1.First();
	start2 = *start_points2.First();
	WalkIntersection(surf1, surf2, start1[0], start2[0], start1[1], start2[1], stepsize, tol, &out1, &out2);
	start_points1.Remove(0);
	start_points2.Remove(0);
	for (j = 1; j < start_points1.Count(); j++) {
	    ON_3dPoint p1 = surf1->PointAt(start_points1[j][0], start_points1[j][1]);
	    ON_3dPoint p2 = surf2->PointAt(start_points2[j][0], start_points2[j][1]);
	    double dummy;
	    if (false /* we need to rewrite this test */) {
		start_points1.Remove(j);
		start_points2.Remove(j);
		j--;
	    }
	}
	/* append the found curves to curves1 and curves2 */
    }
    return true;
}

ON_Surface*
Surface(const ON_3dPoint& SW, const ON_3dPoint& SE, const ON_3dPoint& NE, const ON_3dPoint& NW)
{
    ON_NurbsSurface* pNurbsSurface = new ON_NurbsSurface(3, // dimension
							 FALSE, // not rational
							 2, // u order
							 2, // v order,
							 2, // number of control vertices in u
							 2 // number of control verts in v
	);
    pNurbsSurface->SetCV(0, 0, SW);
    pNurbsSurface->SetCV(1, 0, SE);
    pNurbsSurface->SetCV(1, 1, NE);
    pNurbsSurface->SetCV(0, 1, NW);
    // u knots
    pNurbsSurface->SetKnot(0, 0, 0.0);
    pNurbsSurface->SetKnot(0, 1, 1.0);
    // v knots
    pNurbsSurface->SetKnot(1, 0, 0.0);
    pNurbsSurface->SetKnot(1, 1, 1.0);

    return pNurbsSurface;
}

int main()
{
    ON_Surface *surf1 = Surface(ON_3dPoint(1.0, 1.0, 1.0), ON_3dPoint(-1.0, 1.0, 1.0), ON_3dPoint(-1.0, -1.0, -1.0), ON_3dPoint(1.0, -1.0, -1.0));
    ON_Surface *surf2 = Surface(ON_3dPoint(1.0, -1.0, 1.0), ON_3dPoint(-1.0, -1.0, 1.0), ON_3dPoint(-1.0, 1.0, -1.0), ON_3dPoint(1.0, 1.0, -1.0));
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

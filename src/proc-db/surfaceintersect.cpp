/*             S U R F A C E I N T E R S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2009-2011 United States Government as represented by
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
/** @{ */
/** @file proc-db/surfaceintersect.cpp
 *
 * This code was originally written by Joe Doliner: jdoliner@gmail.com
 */

#include "surfaceintersect.h"

/* implementation system headers */
#include <assert.h>


#define SI_MIN(a, b) (((a) > (b)) ? (a) : (b))


/**
 * ClosestValue
 *
 * @brief returns the value that is closest to the given value but in
 * the given interval
 */
double
ClosestValue(double value, ON_Interval interval)
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
 * Push
 *
 * @brief updates s and t using an X, Y, Z vector
 */
void
Push(const ON_Surface *surf, double *s, double *t, ON_3dVector vec)
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
 * Step
 *
 * @brief advances s1, s2, t1, t2 along the curve of intersection of
 * the two surfaces by a distance of step size.
 */
void
Step(
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
    /* double vec[3] = {0.0, 0.0, 0.0}; */
    ON_3dVector stepscaled;
    ON_ArrayScale(3, stepsize/Magnitude, step, stepscaled);
    Push(surf1, s1, t1, stepscaled);
    Push(surf2, s2, t2, stepscaled);
}


/**
 * Jiggle
 *
 * @brief uses newtonesque method to jiggle the points on the surfaces
 * about and find a closer * point returns the new distance between
 * the points.
 */
double
Jiggle(
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
 * SplitTrim
 *
 * @brief Split's a trim at a point, and replaces the references to
 * that trim with the pieces
 */
void
SplitTrim(ON_BrepTrim *trim, double t)
{
    ON_Curve *left, *right;
    bool rv = trim->Split(t, left, right);

    if (rv) {
	int lefti = trim->Brep()->AddTrimCurve(left);
	int righti = trim->Brep()->AddTrimCurve(right);
	trim->Loop()->m_ti.Remove(trim->m_trim_index);
	trim->Loop()->m_ti.Insert(lefti, trim->m_trim_index);
	trim->Loop()->m_ti.Insert(righti, trim->m_trim_index);
    }
}


/**
 * ShatterLoop
 *
 * @brief after slicing a loop up in to pieces, this destroys the loop
 * itself and drops the pieces into an array
 */
void
ShatterLoop(ON_BrepLoop *loop, ON_SimpleArray<ON_Curve*> curves)
{
    int i;
    for (i = 0; i < loop->TrimCount(); i++) {
	curves.Append(loop->Trim(i)->TrimCurveOf()->Duplicate());
    }

    /* deletes the loop as well as curves used only by this loop */
    loop->Brep()->DeleteLoop(*loop, true);
}


/**
 * Compare_X_Parameter
 *
 * @brief Compares two ON_X_EVENTS by the value of the parameter of the
 * first curve.
 */
int
Compare_X_Parameter(const ON_X_EVENT *a, const ON_X_EVENT *b)
{
    if (a->m_a[0] < b->m_a[0]) {
	return -1;
    } else if (a->m_a[0] > b->m_a[0]) {
	return 1;
    } else {
	return 0;
    }
}


/**
 * Curve_Compare_start
 *
 * @brief Compares the start points of the curve profiles
 */
int
Curve_Compare_start(ON_Curve *const *a, ON_Curve *const *b)
{
    ON_3dVector A = ON_2dVector((*a)->PointAtStart().x, (*a)->PointAtStart().y);
    ON_3dVector B = ON_2dVector((*b)->PointAtStart().x, (*b)->PointAtStart().y);

    if (V2NEAR_EQUAL(A, B, 1e-9)) {
	return 0;
    } else if (A.x < B.x) {
	return -1;
    } else if (A.x > B.x) {
	return 1;
    } else if (A.y < B.y) {
	return -1;
    } else if (A.y > B.y) {
	return 1;
    }

    /* we should have already done this... but just in case */
    return 0;
}


/**
 * Curve_Compare_end
 *
 * @brief Compares the end points of the curve profiles
 */
int
Curve_Compare_end(const ON_Curve **a, const ON_Curve **b)
{
    ON_3dVector A = ON_2dVector((*a)->PointAtEnd().x, (*a)->PointAtEnd().y);
    ON_3dVector B = ON_2dVector((*b)->PointAtEnd().x, (*b)->PointAtEnd().y);
    if (V2NEAR_EQUAL(A, B, 1e-9)) {
	return 0;
    } else if (A.x < B.x) {
	return -1;
    } else if (A.x > B.x) {
	return 1;
    } else if (A.y < B.y) {
	return -1;
    } else if (A.y > B.y) {
	return 1;
    }

    /* we should have already done this... but just in case */
    return 0;
}


/**
 * Face_X_Event::Face_X_Event
 *
 * @brief create a new unintialized Face_X_Event
 */
Face_X_Event::Face_X_Event()
{}


/**
 * Face_X_Event::Face_X_Event
 *
 * @brief create a new Face_X_Event using a set of given values
 */
Face_X_Event::Face_X_Event(ON_BrepFace *f1, ON_BrepFace *f2, ON_Curve *c1, ON_Curve *c2)
{
    this->face1 = f1;
    this->face2 = f2;
    this->curve1 = c1;
    this->curve2 = c2;
    this->loop_flags1.SetCapacity(face1->LoopCount());
    this->loop_flags1.SetCount(face1->LoopCount());
    this->loop_flags2.SetCapacity(face2->LoopCount());
    this->loop_flags2.SetCount(face2->LoopCount());
}


/**
 * Face_X_Event::Render_Curves
 *
 * @brief Renders the Curves in the Face_X_Event as the different
 * curves it is segmented in to This assumes the convention that to
 * the left of a curve is below.
 */
int
Face_X_Event::Render_Curves()
{
    /* the curve can be active or inactive in either face */
    bool active1 = false, active2 = false;
    double last = 0.0;
    int i;

    /* Now we step through the X events activating and deactivating
     * the curve Note the curve is always curve1 in the event, while
     * the trim is always curve 2
     */
    for (i = 0; i < x.Count(); i++) {
	ON_X_EVENT event = x[i];
	if (active1 && active2) {
	    /* to be deactived the curve must pass from below a curve to above it */
	    if (event.m_dirA[0] == event.from_below_dir && event.m_dirA[1] == event.to_above_dir) {
		ON_Curve *new_curve1 = curve1->Duplicate();
		ON_Curve *new_curve2 = curve2->Duplicate();
		new_curve1->Trim(ON_Interval(last, event.m_a[0]));
		new_curve2->Trim(ON_Interval(last, event.m_a[0]));
		new_curves1.Append(new_curve1);
		new_curves2.Append(new_curve2);
		last = event.m_a[0];
		if (event.m_user.i == 0) { /* event.m_user tells us which of the twins the event happened in */
		    active1 = false;
		} else if (event.m_user.i == 1) {
		    active2 = false;
		} else {
		    assert(0);
		}
	    } else if (event.m_dirA[0] == event.from_above_dir && event.m_dirA[1] == event.to_below_dir) {

		/* this would be an activating event, except that both
		 * curves are already active in this case we assume
		 * that the user forgot to remove the outer curve,
		 * thus we reset last
		 */
		last = event.m_a[0];
	    }
	} else {
	    /* one of the curves is inactive */
	    if (event.m_dirA[0] == event.from_above_dir && event.m_dirA[1] == event.to_below_dir) {
		last = event.m_a[0];
		if (event.m_user.i == 0) {
		    active1 = true;
		} else if (event.m_user.i == 1) {
		    active2 = true;
		} else {
		    assert(0);
		}
	    } else {
		/* do nothing */
	    }
	}
    }

    return 0; /* XXX - unused */
}

/**
 * CurveCurveIntersect
 *
 * @brief Intersect 2 curves appending ON_X_EVENTS to the array x for
 * the intersections returns the number of ON_X_EVENTS appended
 *
 * This is not a great implementation of this function it's limited in
 * that it will only find point intersections, not overlaps. Overlaps,
 * will come out as long strings of points, and will probably take a
 * long time to compute.
 */
int
CurveCurveIntersect(
    const ON_Curve *curve1,
    const ON_Curve *curve2,
    ON_SimpleArray<ON_X_EVENT>& x,
    double tol
    )
{
    int rv = 0;
    ON_BoundingBox bbox1, bbox2;
    curve1->GetTightBoundingBox(bbox1);
    curve2->GetTightBoundingBox(bbox2);
    if (bbox1.IsDisjoint(bbox2)) {
	return 0;
    } else {
	ON_Interval domain1 = curve1->Domain(), domain2 = curve2->Domain();
	if (bbox1.Diagonal().Length() + bbox2.Diagonal().Length() > tol) {
	    ON_Curve *left1, *right1, *left2, *right2;
	    curve1->Split(domain1.Mid(), left1, right1), curve2->Split(domain2.Mid(), left2, right2);
	    rv += CurveCurveIntersect(left1, left2, x, tol);
	    rv += CurveCurveIntersect(left1, right2, x, tol);
	    rv += CurveCurveIntersect(right1, left2, x, tol);
	    rv += CurveCurveIntersect(right2, right2, x, tol);
	} else {
	    /* the curves are contained within a bounding box that's smaller than the tolerance */
	    ON_X_EVENT *newx = new ON_X_EVENT();
	    newx->m_type = newx->ccx_point;
	    newx->m_A[0] = curve1->PointAt(domain1.Mid());
	    newx->m_B[0] = curve2->PointAt(domain2.Mid());
	    newx->m_a[0] = domain1.Mid();
	    newx->m_b[0] = domain2.Mid();

	    /* Documentation for these routines being what it is for these intersections
	     * it's not really clear what these should be
	     newx.m_cnodeA[0] = ;
	     newx.m_nodeA_t[0] = ;
	     newx.m_cnodeB[0] = ;
	     newx.m_nodeB_t[0] = ;
	     newx.m_x_eventsn = ; this one we could do, but it doesn't seem worth the trouble.
	    */
	    x.Append(*newx);
	    rv++;
	}
    }
    return rv;
}


/**
 * SetCurveSurveIntersectionDir
 *
 * @brief Sets the Dir fields on an intersection event, this function
 * is 'below' a curve refers to the portion to the right of the curve
 * wrt N
 */
bool
SetCurveCurveIntersectionDir(
    ON_3dVector N,
    int xcount,
    ON_X_EVENT* xevent,
    ON_Curve *curve1,
    ON_Curve *curve2
    )
{
    bool rv = true;
    int i;
    for (i = 0; i < xcount; i++) {
	ON_X_EVENT event = xevent[i];
	if (event.m_type != event.ccx_point) {
	    continue;
	}
	ON_3dVector tangent1 = curve1->TangentAt(event.m_a[0]);
	ON_3dVector tangent2 = curve2->TangentAt(event.m_b[0]);
	ON_3dVector cross = ON_CrossProduct(tangent1, tangent2);
	double dot = ON_DotProduct(N, cross);
	if (dot > 0) {
	    event.m_dirA[0] = event.from_above_dir;
	    event.m_dirA[1] = event.to_below_dir;
	    event.m_dirB[0] = event.from_below_dir;
	    event.m_dirB[1] = event.to_above_dir;
	} else if (dot > 0) {
	    event.m_dirA[0] = event.from_below_dir;
	    event.m_dirA[1] = event.to_above_dir;
	    event.m_dirB[0] = event.from_above_dir;
	    event.m_dirB[0] = event.to_below_dir;
	} else {
	    event.m_dirA[0] = event.no_x_dir;
	    event.m_dirA[1] = event.no_x_dir;
	    event.m_dirB[0] = event.no_x_dir;
	    event.m_dirB[1] = event.no_x_dir;
	    rv = false;
	}
    }
    return rv;
}


/**
 * Face_X_Event::Get_ON_X_Events()
 *
 * @brief Gets all of the intersections between either of the new
 * curves and the trims of the faces, stores them in the x field in
 * the class
 */
int
Face_X_Event::Get_ON_X_Events(double tol)
{
    ON_SimpleArray<ON_X_EVENT> out;
    x.Empty();
    ON_BrepFace *faces[2] = {face1, face2};
    ON_Curve *curves[2] = {curve1, curve2};
    ON_ClassArray<bool> *loop_flags[2]= {&loop_flags1, &loop_flags2};

    int i, j, k, l;
    for (i = 0; i < 2; i++) {

	ON_BrepFace *face = faces[i];
	for (j = 0; j < face->LoopCount(); j++) {

	    ON_BrepLoop* loop = face->Loop(j);
	    for (k = 0; k < loop->TrimCount(); k++) {

		ON_BrepTrim *trim = loop->Trim(k);
		out.Empty();

		/* It's worth noting that trims are always curve2 in intersections */
		int new_xs = CurveCurveIntersect(curves[i], trim->TrimCurveOf(), out, tol);
		if (new_xs) {
		    loop_flags[i][j] = true; /* flag loop j for destruction */
		}

		/* record in m_user which curve this intersection came from */
		for (l = 0; l < new_xs; l++) {
		    out[l].m_user.i = i;
		}

		SetCurveCurveIntersectionDir(ON_3dVector(0.0, 0.0, 1.0), new_xs, out.Array(), curves[0], curves[1]);
		x.Append(new_xs, out.Array());
	    }
	}
    }
    if (x.Count()) {
	x.QuickSort(Compare_X_Parameter);
    }
    return x.Count();
}


/**
 * MakeLoops
 *
 * @brief Makes loops out of the new trims and old trims by matching
 * them end to end old trims were previously in trim loops, new trims
 * come from the intersection the difference being that every new_trim
 * must be used, while some old_trims will be discarded.
 *
 * Note: destroys the arrays it's given.
 */
int
MakeLoops(
    ON_BrepFace *face,
    ON_SimpleArray<ON_Curve*>& new_trims,
    ON_SimpleArray<ON_Curve*>& old_trims,
    double tol
    )
{
    int i;
    ON_SimpleArray<ON_Curve*> trims[2] = {new_trims, old_trims};
    for (i = 0; i < 2; i++) {
	trims[i].QuickSort(Curve_Compare_start);
    }

    ON_SimpleArray<ON_Curve*> loop;
    while (new_trims.Count() != 0) {

	loop.Append(*new_trims.First());
	new_trims.Remove(0);
	while (!V2NEAR_EQUAL((*loop.First())->PointAtStart(), (*loop.Last())->PointAtEnd(), tol)) {

	    /* bit hacky, makes a curve we can use to search for the
	     * arrays for matching trims
	     */
	    ON_LineCurve search_line = ON_LineCurve((*loop.Last())->PointAtEnd(), (*loop.First())->PointAtStart());
	    ON_Curve *search_curve = (ON_Curve *) &search_line;
	    int next_curve; /* -1 is the not found value */

	    for (i = 0; i < 2; i++) {
		next_curve = -1;
		next_curve = new_trims.BinarySearch(&search_curve, Curve_Compare_start);
		if (next_curve != -1) {
		    loop.Append(trims[i][next_curve]);
		    trims[i].Remove(next_curve);
		    break;
		}
	    }

	    if (next_curve == -1) {
		/* we didn't find anything in either array */
		assert(0);
	    }
	}

	/* When we get here, loop will be a fully closed loop of
	 * trims.
	 */
	ON_Brep *brep = face->Brep();
	ON_BrepLoop *new_loop = &brep->NewLoop(ON_BrepLoop::unknown, *face);
	for (i = 0; i < loop.Count(); i++) {
	    new_loop->m_ti.Append(brep->AddTrimCurve(loop[i]));
	}

	int ldir = brep->LoopDirection(*new_loop);
	switch (ldir) {
	    case 1:
		new_loop->m_type = ON_BrepLoop::outer;
		break;
	    case -1:
		new_loop->m_type = ON_BrepLoop::inner;
		break;
	    default:
		assert(0); /* we should absolutely never get here */
	}
    }

    return 0;/* XXX - unused */
}

/**
 * IsClosed
 *
 * @brief check if a 2dPointarrray is closed. To be closed an array
 * must have >2 points in it, have the first and last points within
 * tol of one another and have at least one point not within tol of
 * either of them.
 */
bool
IsClosed(const ON_2dPointArray l, double tol)
{
    if (l.Count() < 3) {
	return false;
    } else if (V2NEAR_EQUAL(l[0], l[l.Count() - 1], tol)) {
	int i;
	for (i = 1; i < l.Count() - 1; i++) {
	    if (!V2NEAR_EQUAL(l[0], l[i], tol) && !V2NEAR_EQUAL(l[l.Count() - 1], l[i], tol)) {
		return true;
	    }
	}
    } else {
	return false;
    }
    return false;
}


/**
 * WalkIntersection
 *
 * @brief walks the intersection between 2 brepfaces, returns lines
 * segmented by the trimming curves
 */
void
WalkIntersection(
    const ON_Surface *surf1,
    const ON_Surface *surf2,
    double s1,
    double s2,
    double t1,
    double t2,
    double stepsize,
    double tol,
    ON_Curve *&out1,
    ON_Curve *&out2
    )
{
    ON_2dPointArray intersectionPoints1, intersectionPoints2;
    double inits1 = s1, inits2 = s2, initt1 = t1, initt2 = t2;
    double distance;
    int passes;

    /* this function is meant to be called with an arbitrary point on
     * the curve and return the entire curve, regardless of which
     * point was passed in, this means that if the intersection
     * doesn't happen to be a loop, we need to walk both directions
     * from the initial point.
     */
    for (passes = 0; passes < 2; passes++) {
	while (surf1->Domain(0).Includes(s1, true) && surf1->Domain(1).Includes(t1, true) && surf2->Domain(0).Includes(s2, true) && surf2->Domain(1).Includes(t2, true) && !(IsClosed(intersectionPoints1, stepsize) && IsClosed(intersectionPoints2, stepsize))) {
	    do {
		distance = Jiggle(surf1, surf2, &s1, &s2, &t1, &t2);
	    } while (distance > tol);

	    intersectionPoints1.Append(ON_2dPoint(s1, t1));
	    intersectionPoints2.Append(ON_2dPoint(s2, t2));
	    if (passes == 0) {
		Step(surf1, surf2, &s1, &s2, &t1, &t2, stepsize);
	    } else {
		Step(surf1, surf2, &s1, &s2, &t1, &t2, -stepsize);
	    }
	}

	if (IsClosed(intersectionPoints1, stepsize) && IsClosed(intersectionPoints2, stepsize)) {
	    break; /* we're done, no second pass required */
	} else {
	    /* this is a bit cute, we are assured to hit this part on
	     * the second pass, so we either reverse 0 or 2 times.
	     */
	    intersectionPoints1.Reverse();
	    intersectionPoints2.Reverse();
	    s1 = inits1;
	    s2 = inits2;
	    t1 = initt1;
	    t2 = initt2;

	    if (passes == 0) {
		/* to avoid duplicates of the starting point */
		Step(surf1, surf2, &s1, &s2, &t1, &t2, -stepsize);
	    }
	}
    }
    ON_BezierCurve *bezier1 = new ON_BezierCurve((ON_2dPointArray) intersectionPoints1);
    ON_BezierCurve *bezier2 = new ON_BezierCurve((ON_2dPointArray) intersectionPoints2);
    out1 = ON_NurbsCurve::New(*bezier1);
    out2 = ON_NurbsCurve::New(*bezier2);
}


/**
 * GetStartPointsInternal
 *
 * @brief Subdibivides the surface recursively to zoom in on
 * intersection points internal to the surfaces.
 */
bool
GetStartPointsInternal(
    const ON_Surface *surf1,
    const ON_Surface *surf2,
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

	int i, j;
	return_value = false;
	for (i = 0; i < 4; i++) {
	    for (j = 0; j < 4; j++) {
		return_value = return_value && GetStartPointsInternal(Parts1[i], Parts2[j], start_points1, start_points2, tol);
	    }
	}
    }
    return return_value;
}


/**
 * GetStartPointsEdges
 *
 * @brief Find starting points that are on the edges of the surfaces
 */
bool
GetStartPointsEdges(
    const ON_Surface *surf1,
    const ON_Surface *surf2,
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

    const ON_Surface *surfaces[2] = {surf1, surf2};
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
		    } else {
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
 * FaceFaceIntersect
 *
 * @brief finds the intersection curves of two faces and returns them
 * as Face_X_Events.
 */
int
FaceFaceIntersect(
    ON_BrepFace *face1,
    ON_BrepFace *face2,
    double stepsize,
    double tol,
    ON_ClassArray<Face_X_Event>& x
    )
{
    int init_count = x.Count();
    const ON_Surface *surf1 = face1->SurfaceOf(), *surf2 = face2->SurfaceOf();
    ON_2dPointArray start_points1, start_points2;

    bool rv_edges = GetStartPointsEdges(surf1, surf2, start_points1, start_points2, tol);
    bool rv_internal = GetStartPointsInternal(surf1, surf2, start_points1, start_points2, tol);

    if (!(rv_edges || rv_internal)) {
	return false;
    }

    int i, j;
    ON_Curve *out1, *out2;
    ON_2dPoint start1, start2;

    for (i = 0; i < start_points1.Count(); i++) {
	start1 = *start_points1.First();
	start2 = *start_points2.First();
	WalkIntersection(surf1, surf2, start1[0], start2[0], start1[1], start2[1], stepsize, tol, out1, out2);

	start_points1.Remove(0);
	start_points2.Remove(0);

	for (j = 0; j < start_points1.Count(); j++) {
	    double dummy;
	    if (out1->GetClosestPoint(ON_3dPoint(start_points1[j]), &dummy, stepsize) && out2->GetClosestPoint(ON_3dPoint(start_points2[j]), &dummy, stepsize)) {
		/* start points j lie on the curve so we delete them */
		start_points1.Remove(j);
		start_points2.Remove(j);
		j--;
	    }
	}
	Face_X_Event newevent = Face_X_Event(face1, face2, out1, out2);
	x.Append(newevent);
    }
    return x.Count() - init_count;
}


/**
 * BrepBrepIntersect
 *
 * @brief calls SurfaceSurfaceIntersect on the {m_S}X{m_S} then
 * intersects the results with the trim curves of the brepfaces.
 */
bool
BrepBrepIntersect(
    ON_Brep *brep1,
    ON_Brep *brep2,
    double stepsize,
    double tol
    )
{
    int i, j, k, l;

    /* the new curves we get from the actual intersection */
    ON_ClassArray<ON_SimpleArray<ON_Curve*> > intersection_curves1, intersection_curves2;

    /* the new curves we get from destroying the old trim_loops */
    ON_ClassArray<ON_SimpleArray<ON_Curve*> > trim_curves1, trim_curves2;

    /* initialization for brep1's arrays */
    intersection_curves1.SetCapacity(brep1->m_F.Count());
    intersection_curves1.SetCount(brep1->m_F.Count());
    trim_curves1.SetCapacity(brep1->m_F.Count());
    trim_curves1.SetCount(brep1->m_F.Count());

    /* initialization for brep2's arrays */
    intersection_curves2.SetCapacity(brep2->m_F.Count());
    intersection_curves2.SetCount(brep2->m_F.Count());
    trim_curves2.SetCapacity(brep2->m_F.Count());
    trim_curves2.SetCount(brep2->m_F.Count());

    /* flags for which trims need to be shattered */
    ON_ClassArray<ON_SimpleArray<bool> > loop_flags1, loop_flags2;
    loop_flags1.SetCapacity(brep1->m_F.Count());
    loop_flags1.SetCount(brep1->m_F.Count());
    loop_flags2.SetCapacity(brep2->m_F.Count());
    loop_flags2.SetCount(brep2->m_F.Count());

    /* initialize the loop_flag arrays */
    for (i = 0; i < brep1->m_F.Count(); i++) {
	ON_BrepFace *face1 = &brep1->m_F[i];
	loop_flags1[i].SetCapacity(face1->LoopCount());
	loop_flags1[i].SetCount(face1->LoopCount());
	loop_flags1[i].MemSet(false);
    }

    for (j = 0; j < brep2->m_F.Count(); j++) {
	ON_BrepFace *face2 = &brep2->m_F[j];
	loop_flags2[j].SetCapacity(face2->LoopCount());
	loop_flags2[j].SetCount(face2->LoopCount());
	loop_flags2[j].MemSet(false);
    }

    /* first we intersect all of the Faces and record the
     * intersectiosn in Face_X_Events.
     */
    ON_ClassArray<Face_X_Event> x;
    for (i = 0; i < brep1->m_F.Count(); i++) {

	ON_BrepFace *face1 = &brep1->m_F[i];
	for (j = 0; j < brep2->m_F.Count(); j++) {

	    ON_BrepFace *face2 = &brep2->m_F[j];
	    x.Empty();
	    FaceFaceIntersect(face1, face2, stepsize, tol, x);

	    for (k = 0; k < x.Count(); k++) {

		Face_X_Event event_ij = x[k];
		event_ij.Get_ON_X_Events(tol);
		int new_curves = event_ij.Render_Curves();

		for (l = 0; l < new_curves; l++) {
		    intersection_curves1[i].Append(event_ij.new_curves1[l]);
		    intersection_curves2[j].Append(event_ij.new_curves2[l]);
		}
		for (l = 0; l < event_ij.loop_flags1.Count(); l++) {
		    loop_flags1[i][l] = loop_flags1[i][l] && event_ij.loop_flags1[l];
		}
		for (l = 0; l < event_ij.loop_flags2.Count(); l++) {
		    loop_flags2[j][l] = loop_flags2[j][l] && event_ij.loop_flags2[l];
		}
	    }
	}
    }

    /* Now we shatter the loops */
    for (i = 0; i < brep1->m_F.Count(); i++) {
	ON_BrepFace *face1 = &brep1->m_F[i];
	for (j = 0; j < loop_flags1[i].Count(); j++) {
	    if (loop_flags1[i][j]) {
		/* time to destroy a loop */
		ShatterLoop(face1->Loop(j), trim_curves1[i]);
	    }
	}
    }

    for (i = 0; i < brep2->m_F.Count(); i++) {
	ON_BrepFace *face2 = &brep2->m_F[i];
	for (j = 0; j < loop_flags2[i].Count(); j++) {
	    if (loop_flags2[i][j]) {
		/* time to destroy a loop */
		ShatterLoop(face2->Loop(j), trim_curves2[i]);
	    }
	}
    }

    /* We now have all of the trims we need to reconstruct the loops */
    for (i = 0; i < brep1->m_F.Count(); i++) {
	MakeLoops(&brep1->m_F[i], intersection_curves1[i], trim_curves1[i], tol);
    }
    for (i = 0; i < brep2->m_F.Count(); i++) {
	MakeLoops(&brep2->m_F[i], intersection_curves2[i], trim_curves2[i], tol);
    }

    /* XXX - unused */
    return false;
}

namespace si {

    enum {
	A, B, C, D, E, F, G, H
    };

    enum {
	AB, BC, CD, AD, EF, FG, GH, EH
    };

    enum {
	ABCD, FEHG
    };
} /* end namespace */
using namespace si;


ON_Curve*
TwistedCubeEdgeCurve(const ON_3dPoint& from, const ON_3dPoint& to)
{
    // creates a 3d line segment to be used as a 3d curve in an ON_Brep
    ON_Curve* c3d = new ON_LineCurve(from, to);
    c3d->SetDomain(0.0, 1.0); // XXX is this UV bounds?
    return c3d;
}

void
MakeTwistedCubeEdge(ON_Brep& brep, int from, int to, int curve)
{
    ON_BrepVertex& v0 = brep.m_V[from];
    ON_BrepVertex& v1 = brep.m_V[to];
    ON_BrepEdge& edge = brep.NewEdge(v0, v1, curve);
    edge.m_tolerance = 0.0; // exact!
}

void
MakeTwistedCubeEdges1(ON_Brep& brep)
{
    MakeTwistedCubeEdge(brep, A, B, AB);
    MakeTwistedCubeEdge(brep, B, C, BC);
    MakeTwistedCubeEdge(brep, C, D, CD);
    MakeTwistedCubeEdge(brep, A, D, AD);
}

void
MakeTwistedCubeEdges2(ON_Brep& brep)
{
    MakeTwistedCubeEdge(brep, E, F, EF);
    MakeTwistedCubeEdge(brep, F, G, FG);
    MakeTwistedCubeEdge(brep, G, si::H, GH);
    MakeTwistedCubeEdge(brep, E, si::H, EH);
}

ON_Surface*
TwistedCubeSideSurface(const ON_3dPoint& SW, const ON_3dPoint& SE, const ON_3dPoint& NE, const ON_3dPoint& NW)
{
    ON_NurbsSurface* pNurbsSurface = new ON_NurbsSurface(3, // dimension
							 0, // not rational
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

ON_Curve*
TwistedCubeTrimmingCurve(const ON_Surface& s,
			 int side // 0 = SW to SE, 1 = SE to NE, 2 = NE to NW, 3 = NW, SW
    )
{
    // a trimming curve is a 2d curve whose image lies in the surface's
    // domain. The "active" portion of the surface is to the left of the
    // trimming curve (looking down the orientation of the curve). An
    // outer trimming loop consists of a simple closed curve running
    // counter-clockwise around the region it trims

    ON_2dPoint from, to;
    double u0, u1, v0, v1;
    s.GetDomain(0, &u0, &u1);
    s.GetDomain(1, &v0, &v1);

    switch (side) {
	case 0:
	    from.x = u0; from.y = v0;
	    to.x   = u1; to.y   = v0;
	    break;
	case 1:
	    from.x = u1; from.y = v0;
	    to.x   = u1; to.y   = v1;
	    break;
	case 2:
	    from.x = u1; from.y = v1;
	    to.x   = u0; to.y   = v1;
	    break;
	case 3:
	    from.x = u0; from.y = v1;
	    to.x   = u0; to.y   = v0;
	    break;
	default:
	    return NULL;
    }
    ON_Curve* c2d = new ON_LineCurve(from, to);
    c2d->SetDomain(0.0, 1.0);
    return c2d;
}


int // return value not used?
MakeTwistedCubeTrimmingLoop(ON_Brep& brep,
			    ON_BrepFace& face,
			    int UNUSED(v0), int UNUSED(v1), int UNUSED(v2), int UNUSED(v3), // indices of corner vertices
			    int e0, int eo0, // edge index + orientation w.r.t surface trim
			    int e1, int eo1,
			    int e2, int eo2,
			    int e3, int eo3)
{
    // get a reference to the surface
    const ON_Surface& srf = *brep.m_S[face.m_si];

    ON_BrepLoop& loop = brep.NewLoop(ON_BrepLoop::outer, face);

    // create the trimming curves running counter-clockwise around the
    // surface's domain, start at the south side
    ON_Curve* c2;
    int c2i, ei = 0, bRev3d = 0;
    ON_2dPoint q;

    // flags for isoparametric curves
    ON_Surface::ISO iso = ON_Surface::not_iso;

    for (int side = 0; side < 4; side++) {
	// side: 0=south, 1=east, 2=north, 3=west
	c2 = TwistedCubeTrimmingCurve(srf, side);
	c2i = brep.m_C2.Count();
	brep.m_C2.Append(c2);

	switch (side) {
	    case 0:
		ei = e0;
		bRev3d = (eo0 == -1);
		iso = ON_Surface::S_iso;
		break;
	    case 1:
		ei = e1;
		bRev3d = (eo1 == -1);
		iso = ON_Surface::E_iso;
		break;
	    case 2:
		ei = e2;
		bRev3d = (eo2 == -1);
		iso = ON_Surface::N_iso;
		break;
	    case 3:
		ei = e3;
		bRev3d = (eo3 == -1);
		iso = ON_Surface::W_iso;
		break;
	}

	ON_BrepTrim& trim = brep.NewTrim(brep.m_E[ei], bRev3d, loop, c2i);
	trim.m_iso = iso;

	// the type gives metadata on the trim type in this case, "mated"
	// means the trim is connected to an edge, is part of an
	// outer/inner/slit loop, no other trim from the same edge is
	// connected to the edge, and at least one trim from a different
	// loop is connected to the edge
	trim.m_type = ON_BrepTrim::mated; // i.e. this b-rep is closed, so
	// all trims have mates

	// not convinced these shouldn't be set with a member function
	trim.m_tolerance[0] = 0.0; // exact
	trim.m_tolerance[1] = 0.0; //
    }
    return loop.m_loop_index;
}

void
MakeTwistedCubeFace(ON_Brep& brep,
		    int surf,
		    int orientation,
		    int v0, int v1, int v2, int v3, // the indices of corner vertices
		    int e0, int eo0, // edge index + orientation
		    int e1, int eo1,
		    int e2, int eo2,
		    int e3, int eo3)
{
    ON_BrepFace& face = brep.NewFace(surf);
    MakeTwistedCubeTrimmingLoop(brep,
				face,
				v0, v1, v2, v3,
				e0, eo0,
				e1, eo1,
				e2, eo2,
				e3, eo3);
    // should the normal be reversed?
    face.m_bRev = (orientation == -1);
}

void
MakeTwistedCubeFaces1(ON_Brep& brep)
{
    MakeTwistedCubeFace(brep,
			ABCD, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			A, B, C, D, // indices of vertices listed in order
			AB, +1, // south edge, orientation w.r.t. trimming curve?
			BC, +1, // east edge, orientation w.r.t. trimming curve?
			CD, +1,
			AD, -1);
}

void
MakeTwistedCubeFaces2(ON_Brep& brep)
{
    MakeTwistedCubeFace(brep,
			FEHG, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			F, E, si::H, G, // indices of vertices listed in order
			EF, -1, // south edge, orientation w.r.t. trimming curve?
			EH, +1, // east edge, orientation w.r.t. trimming curve?
			GH, -1,
			FG, -1);
}

ON_Brep*
MakeTwistedSquare1(ON_TextLog& error_log)
{
    ON_3dPoint point[8] = {
	ON_3dPoint(1.0,  1.0, 1.0), // Point A
	ON_3dPoint(-1.0, -1.0, 1.0), // Point B
	ON_3dPoint(-1.0, -1.0, -1.0), // Point C
	ON_3dPoint(1.0, 1.0, -1.0), // Point D
    };

    ON_Brep* brep = new ON_Brep();

    // create eight vertices located at the eight points
    for (int i = 0; i < 4; i++) {
	ON_BrepVertex& v = brep->NewVertex(point[i]);
	v.m_tolerance = 0.0;
	// this example uses exact tolerance... reference
	// ON_BrepVertex for definition of non-exact data
    }

    // create 3d curve geometry - the orientations are arbitrarily
    // chosen so that the end vertices are in alphabetical order
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[A], point[B])); // AB
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[B], point[C])); // BC
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[C], point[D])); // CD
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[A], point[D])); // AD

    // create the 12 edges the connect the corners
    MakeTwistedCubeEdges1(*brep);

    // create the 3d surface geometry. the orientations are arbitrary so
    // some normals point into the cube and other point out... not sure why
    brep->m_S.Append(TwistedCubeSideSurface(point[A], point[B], point[C], point[D]));

    // create the faces
    MakeTwistedCubeFaces1(*brep);

    if (!brep->IsValid()) {
	error_log.Print("Twisted cube b-rep is not valid!\n");
	delete brep;
	brep = NULL;
    }

    return brep;
}


ON_Brep*
MakeTwistedSquare2(ON_TextLog& error_log)
{
    ON_3dPoint point[8] = {
	ON_3dPoint(1.0, -1.0, 1.0), // Point E
	ON_3dPoint(-1.0, 1.0, 1.0), // Point F
	ON_3dPoint(-1.0, -1.0, 1.0), // Point G
	ON_3dPoint(1.0, -1.0, -1.0), // Point H
    };

    ON_Brep* brep = new ON_Brep();

    // create eight vertices located at the eight points
    for (int i = 0; i < 4; i++) {
	ON_BrepVertex& v = brep->NewVertex(point[i]);
	v.m_tolerance = 0.0;
	// this example uses exact tolerance... reference
	// ON_BrepVertex for definition of non-exact data
    }

    // create 3d curve geometry - the orientations are arbitrarily
    // chosen so that the end vertices are in alphabetical order
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[E], point[F])); // EF
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[F], point[G])); // FG
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[G], point[si::H])); // GH
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[E], point[si::H])); // EH

    // create the 12 edges the connect the corners
    MakeTwistedCubeEdges2(*brep);

    // create the 3d surface geometry. the orientations are arbitrary so
    // some normals point into the cube and other point out... not sure why
    brep->m_S.Append(TwistedCubeSideSurface(point[E], point[F], point[G], point[si::H]));

    // create the faces
    MakeTwistedCubeFaces2(*brep);

    if (!brep->IsValid()) {
	error_log.Print("Twisted cube b-rep is not valid!\n");
	delete brep;
	brep = NULL;
    }

    return brep;
}


void
printPoints(struct rt_brep_internal* bi)
{
    ON_TextLog tl(stdout);
    ON_Brep* brep = bi->brep;

    if (brep) {
	const int count = brep->m_V.Count();
	for (int i = 0; i < count; i++) {
	    ON_BrepVertex& bv = brep->m_V[i];
	    bv.Dump(tl);
	}
    } else {
	bu_log("brep was NULL!\n");
    }
}


int
main()
{
    ON_3dPointArray pts1, pts2;
    pts1.Append(ON_3dPoint(1.0, 1.0, 0.0));
    pts1.Append(ON_3dPoint(-1.0, -1.0, 0.0));
    pts2.Append(ON_3dPoint(-1.0, 1.0, 0.0));
    pts2.Append(ON_3dPoint(1.0, -1.0, 0.0));
    ON_BezierCurve *bezier1 = new ON_BezierCurve(pts1);
    ON_BezierCurve *bezier2 = new ON_BezierCurve(pts2);
    ON_Curve *curve1 = ON_NurbsCurve::New(*bezier1);
    ON_Curve *curve2 = ON_NurbsCurve::New(*bezier2);
    ON_SimpleArray<ON_X_EVENT> x;
    CurveCurveIntersect(curve1, curve2, x, 1e-9);
    ON_Brep brep1 = ON_Brep(), brep2 = ON_Brep();
    ON_Surface *surf1 = TwistedCubeSideSurface(ON_3dPoint(1, 1, 1), ON_3dPoint(-1, -1, 1), ON_3dPoint(-1, -1, -1), ON_3dPoint(1, 1, -1));
    ON_Surface *surf2 = TwistedCubeSideSurface(ON_3dPoint(1, -1, 1), ON_3dPoint(-1, 1, 1), ON_3dPoint(-1, 1, -1), ON_3dPoint(1, -1, -1));

    brep1.Create(surf1);
    brep2.Create(surf2);
    BrepBrepIntersect(&brep1, &brep2, 1e-3, 1e-9);
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

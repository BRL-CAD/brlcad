#include "bltInt.h"

typedef double TriDiagonalMatrix[3];
typedef struct {
    double b, c, d;
} Cubic2D;

typedef struct {
    double b, c, d, e, f;
} Quint2D;

/*
 * Quadratic spline parameters
 */
#define E1	param[0]
#define E2	param[1]
#define V1	param[2]
#define V2	param[3]
#define W1	param[4]
#define W2	param[5]
#define Z1	param[6]
#define Z2	param[7]
#define Y1	param[8]
#define Y2	param[9]

static Tcl_CmdProc SplineCmd;

/*
 * -----------------------------------------------------------------------
 *
 * Search --
 *
 *	Conducts a binary search for a value.  This routine is called
 *	only if key is between x(0) and x(len - 1).
 *
 * Results:
 *	Returns the index of the largest value in xtab for which
 *	x[i] < key.
 *
 * -----------------------------------------------------------------------
 */
static int
Search(points, nPoints, key, foundPtr)
    Point2D points[];		/* Contains the abscissas of the data
				 * points of interpolation. */
    int nPoints;			/* Dimension of x. */
    double key;			/* Value whose relative position in
				 * x is to be located. */
    int *foundPtr;		/* (out) Returns 1 if s is found in
				 * x and 0 otherwise. */
{
    int high, low, mid;

    low = 0;
    high = nPoints - 1;

    while (high >= low) {
	mid = (high + low) / 2;
	if (key > points[mid].x) {
	    low = mid + 1;
	} else if (key < points[mid].x) {
	    high = mid - 1;
	} else {
	    *foundPtr = 1;
	    return mid;
	}
    }
    *foundPtr = 0;
    return low;
}

/*
 *-----------------------------------------------------------------------
 *
 * QuadChoose --
 *
 *	Determines the case needed for the computation of the parame-
 *	ters of the quadratic spline.
 *
 * Results:
 * 	Returns a case number (1-4) which controls how the parameters
 * 	of the quadratic spline are evaluated.
 *
 *-----------------------------------------------------------------------
 */
static int
QuadChoose(p, q, m1, m2, epsilon)
    Point2D *p;			/* Coordinates of one of the points of
				 * interpolation */
    Point2D *q;			/* Coordinates of one of the points of
				 * interpolation */
    double m1;			/* Derivative condition at point P */
    double m2;			/* Derivative condition at point Q */
    double epsilon;		/* Error tolerance used to distinguish
				 * cases when m1 or m2 is relatively
				 * close to the slope or twice the
				 * slope of the line segment joining
				 * the points P and Q.  If
				 * epsilon is not 0.0, then epsilon
				 * should be greater than or equal to
				 * machine epsilon.  */
{
    double slope;

    /* Calculate the slope of the line joining P and Q. */
    slope = (q->y - p->y) / (q->x - p->x);

    if (slope != 0.0) {
	double relerr;
	double mref, mref1, mref2, prod1, prod2;

	prod1 = slope * m1;
	prod2 = slope * m2;

	/* Find the absolute values of the slopes slope, m1, and m2. */
	mref = FABS(slope);
	mref1 = FABS(m1);
	mref2 = FABS(m2);

	/*
	 * If the relative deviation of m1 or m2 from slope is less than
	 * epsilon, then choose case 2 or case 3.
	 */
	relerr = epsilon * mref;
	if ((FABS(slope - m1) > relerr) && (FABS(slope - m2) > relerr) &&
	    (prod1 >= 0.0) && (prod2 >= 0.0)) {
	    double prod;

	    prod = (mref - mref1) * (mref - mref2);
	    if (prod < 0.0) {
		/*
		 * l1, the line through (x1,y1) with slope m1, and l2,
		 * the line through (x2,y2) with slope m2, intersect
		 * at a point whose abscissa is between x1 and x2.
		 * The abscissa becomes a knot of the spline.
		 */
		return 1;
	    }
	    if (mref1 > (mref * 2.0)) {
		if (mref2 <= ((2.0 - epsilon) * mref)) {
		    return 3;
		}
	    } else if (mref2 <= (mref * 2.0)) {
		/*
		 * Both l1 and l2 cross the line through
		 * (x1+x2)/2.0,y1 and (x1+x2)/2.0,y2, which is the
		 * midline of the rectangle formed by P and Q or both
		 * m1 and m2 have signs different than the sign of
		 * slope, or one of m1 and m2 has opposite sign from
		 * slope and l1 and l2 intersect to the left of x1 or
		 * to the right of x2.  The point (x1+x2)/2. is a knot
		 * of the spline.
		 */
		return 2;
	    } else if (mref1 <= ((2.0 - epsilon) * mref)) {
		/*
		 * In cases 3 and 4, sign(m1)=sign(m2)=sign(slope).
		 * Either l1 or l2 crosses the midline, but not both.
		 * Choose case 4 if mref1 is greater than
		 * (2.-epsilon)*mref; otherwise, choose case 3.
		 */
		return 3;
	    }
	    /*
	     * If neither l1 nor l2 crosses the midline, the spline
	     * requires two knots between x1 and x2.
	     */
	    return 4;
	} else {
	    /*
	     * The sign of at least one of the slopes m1 or m2 does not
	     * agree with the sign of *slope*.
	     */
	    if ((prod1 < 0.0) && (prod2 < 0.0)) {
		return 2;
	    } else if (prod1 < 0.0) {
		if (mref2 > ((epsilon + 1.0) * mref)) {
		    return 1;
		} else {
		    return 2;
		}
	    } else if (mref1 > ((epsilon + 1.0) * mref)) {
		return 1;
	    } else {
		return 2;
	    }
	}
    } else if ((m1 * m2) >= 0.0) {
	return 2;
    } else {
	return 1;
    }
}

/*
 * -----------------------------------------------------------------------
 *
 * QuadCases --
 *
 *	Computes the knots and other parameters of the spline on the
 *	interval PQ.
 *
 *
 * On input--
 *
 *	P and Q are the coordinates of the points of interpolation.
 *
 *	m1 is the slope at P.
 *
 *	m2 is the slope at Q.
 *
 *	ncase controls the number and location of the knots.
 *
 *
 * On output--
 *
 *	(v1,v2),(w1,w2),(z1,z2), and (e1,e2) are the coordinates of
 *	the knots and other parameters of the spline on P.
 *	(e1,e2) and Q are used only if ncase=4.
 *
 * -----------------------------------------------------------------------
 */
static void
QuadCases(p, q, m1, m2, param, which)
    Point2D *p, *q;
    double m1, m2;
    double param[];
    int which;
{
    if ((which == 3) || (which == 4)) {	/* Parameters used in both 3 and 4 */
	double mbar1, mbar2, mbar3, c1, d1, h1, j1, k1;

	c1 = p->x + (q->y - p->y) / m1;
	d1 = q->x + (p->y - q->y) / m2;
	h1 = c1 * 2.0 - p->x;
	j1 = d1 * 2.0 - q->x;
	mbar1 = (q->y - p->y) / (h1 - p->x);
	mbar2 = (p->y - q->y) / (j1 - q->x);

	if (which == 4) {	/* Case 4. */
	    Y1 = (p->x + c1) / 2.0;
	    V1 = (p->x + Y1) / 2.0;
	    V2 = m1 * (V1 - p->x) + p->y;
	    Z1 = (d1 + q->x) / 2.0;
	    W1 = (q->x + Z1) / 2.0;
	    W2 = m2 * (W1 - q->x) + q->y;
	    mbar3 = (W2 - V2) / (W1 - V1);
	    Y2 = mbar3 * (Y1 - V1) + V2;
	    Z2 = mbar3 * (Z1 - V1) + V2;
	    E1 = (Y1 + Z1) / 2.0;
	    E2 = mbar3 * (E1 - V1) + V2;
	} else {		/* Case 3. */
	    k1 = (p->y - q->y + q->x * mbar2 - p->x * mbar1) / (mbar2 - mbar1);
	    if (FABS(m1) > FABS(m2)) {
		Z1 = (k1 + p->x) / 2.0;
	    } else {
		Z1 = (k1 + q->x) / 2.0;
	    }
	    V1 = (p->x + Z1) / 2.0;
	    V2 = p->y + m1 * (V1 - p->x);
	    W1 = (q->x + Z1) / 2.0;
	    W2 = q->y + m2 * (W1 - q->x);
	    Z2 = V2 + (W2 - V2) / (W1 - V1) * (Z1 - V1);
	}
    } else if (which == 2) {	/* Case 2. */
	Z1 = (p->x + q->x) / 2.0;
	V1 = (p->x + Z1) / 2.0;
	V2 = p->y + m1 * (V1 - p->x);
	W1 = (Z1 + q->x) / 2.0;
	W2 = q->y + m2 * (W1 - q->x);
	Z2 = (V2 + W2) / 2.0;
    } else {			/* Case 1. */
	double ztwo;

	Z1 = (p->y - q->y + m2 * q->x - m1 * p->x) / (m2 - m1);
	ztwo = p->y + m1 * (Z1 - p->x);
	V1 = (p->x + Z1) / 2.0;
	V2 = (p->y + ztwo) / 2.0;
	W1 = (Z1 + q->x) / 2.0;
	W2 = (ztwo + q->y) / 2.0;
	Z2 = V2 + (W2 - V2) / (W1 - V1) * (Z1 - V1);
    }
}

static int
QuadSelect(p, q, m1, m2, epsilon, param)
    Point2D *p, *q;
    double m1, m2;
    double epsilon;
    double param[];
{
    int ncase;

    ncase = QuadChoose(p, q, m1, m2, epsilon);
    QuadCases(p, q, m1, m2, param, ncase);
    return ncase;
}

/*
 * -----------------------------------------------------------------------
 *
 * QuadGetImage --
 *
 * -----------------------------------------------------------------------
 */
INLINE static double
QuadGetImage(p1, p2, p3, x1, x2, x3)
    double p1, p2, p3;
    double x1, x2, x3;
{
    double A, B, C;
    double y;

    A = x1 - x2;
    B = x2 - x3;
    C = x1 - x3;

    y = (p1 * (A * A) + p2 * 2.0 * B * A + p3 * (B * B)) / (C * C);
    return y;
}

/*
 * -----------------------------------------------------------------------
 *
 * QuadSpline --
 *
 *	Finds the image of a point in x.
 *
 *	On input
 *
 *	x	Contains the value at which the spline is evaluated.
 *	leftX, leftY
 *		Coordinates of the left-hand data point used in the
 *		evaluation of x values.
 *	rightX, rightY
 *		Coordinates of the right-hand data point used in the
 *		evaluation of x values.
 *	Z1, Z2, Y1, Y2, E2, W2, V2
 *		Parameters of the spline.
 *	ncase	Controls the evaluation of the spline by indicating
 *		whether one or two knots were placed in the interval
 *		(xtabs,xtabs1).
 *
 * Results:
 *	The image of the spline at x.
 *
 * -----------------------------------------------------------------------
 */
static void
QuadSpline(intp, left, right, param, ncase)
    Point2D *intp;		/* Value at which spline is evaluated */
    Point2D *left;		/* Point to the left of the data point to
				 * be evaluated */
    Point2D *right;		/* Point to the right of the data point to
				 * be evaluated */
    double param[];		/* Parameters of the spline */
    int ncase;			/* Controls the evaluation of the
				 * spline by indicating whether one or
				 * two knots were placed in the
				 * interval (leftX,rightX) */
{
    double y;

    if (ncase == 4) {
	/*
	 * Case 4:  More than one knot was placed in the interval.
	 */

	/*
	 * Determine the location of data point relative to the 1st knot.
	 */
	if (Y1 > intp->x) {
	    y = QuadGetImage(left->y, V2, Y2, Y1, intp->x, left->x);
	} else if (Y1 < intp->x) {
	    /*
	     * Determine the location of the data point relative to
	     * the 2nd knot.
	     */
	    if (Z1 > intp->x) {
		y = QuadGetImage(Y2, E2, Z2, Z1, intp->x, Y1);
	    } else if (Z1 < intp->x) {
		y = QuadGetImage(Z2, W2, right->y, right->x, intp->x, Z1);
	    } else {
		y = Z2;
	    }
	} else {
	    y = Y2;
	}
    } else {

	/*
	 * Cases 1, 2, or 3:
	 *
	 * Determine the location of the data point relative to the
	 * knot.
	 */
	if (Z1 < intp->x) {
	    y = QuadGetImage(Z2, W2, right->y, right->x, intp->x, Z1);
	} else if (Z1 > intp->x) {
	    y = QuadGetImage(left->y, V2, Z2, Z1, intp->x, left->x);
	} else {
	    y = Z2;
	}
    }
    intp->y = y;
}

/*
 * -----------------------------------------------------------------------
 *
 * QuadSlopes --
 *
 * 	Calculates the derivative at each of the data points.  The
 * 	slopes computed will insure that an osculatory quadratic
 * 	spline will have one additional knot between two adjacent
 * 	points of interpolation.  Convexity and monotonicity are
 * 	preserved wherever these conditions are compatible with the
 * 	data.
 *
 * Results:
 *	The output array "m" is filled with the derivates at each
 *	data point.
 *
 * -----------------------------------------------------------------------
 */
static void
QuadSlopes(points, m, nPoints)
    Point2D points[];
    double *m;			/* (out) To be filled with the first
				 * derivative at each data point. */
    int nPoints;		/* Number of data points (dimension of
				 * x, y, and m). */
{
    double xbar, xmid, xhat, ydif1, ydif2;
    double yxmid;
    double m1, m2;
    double m1s, m2s;
    register int i, n, l;

    m1s = m2s = m1 = m2 = 0;
    for (l = 0, i = 1, n = 2; i < (nPoints - 1); l++, i++, n++) {
	/*
	 * Calculate the slopes of the two lines joining three
	 * consecutive data points.
	 */
	ydif1 = points[i].y - points[l].y;
	ydif2 = points[n].y - points[i].y;
	m1 = ydif1 / (points[i].x - points[l].x);
	m2 = ydif2 / (points[n].x - points[i].x);
	if (i == 1) {
	    m1s = m1, m2s = m2;	/* Save slopes of starting point */
	}
	/*
	 * If one of the preceding slopes is zero or if they have opposite
	 * sign, assign the value zero to the derivative at the middle
	 * point.
	 */
	if ((m1 == 0.0) || (m2 == 0.0) || ((m1 * m2) <= 0.0)) {
	    m[i] = 0.0;
	} else if (FABS(m1) > FABS(m2)) {
	    /*
	     * Calculate the slope by extending the line with slope m1.
	     */
	    xbar = ydif2 / m1 + points[i].x;
	    xhat = (xbar + points[n].x) / 2.0;
	    m[i] = ydif2 / (xhat - points[i].x);
	} else {
	    /*
	     * Calculate the slope by extending the line with slope m2.
	     */
	    xbar = -ydif1 / m2 + points[i].x;
	    xhat = (points[l].x + xbar) / 2.0;
	    m[i] = ydif1 / (points[i].x - xhat);
	}
    }

    /* Calculate the slope at the last point, x(n). */
    i = nPoints - 2;
    n = nPoints - 1;
    if ((m1 * m2) < 0.0) {
	m[n] = m2 * 2.0;
    } else {
	xmid = (points[i].x + points[n].x) / 2.0;
	yxmid = m[i] * (xmid - points[i].x) + points[i].y;
	m[n] = (points[n].y - yxmid) / (points[n].x - xmid);
	if ((m[n] * m2) < 0.0) {
	    m[n] = 0.0;
	}
    }

    /* Calculate the slope at the first point, x(0). */
    if ((m1s * m2s) < 0.0) {
	m[0] = m1s * 2.0;
    } else {
	xmid = (points[0].x + points[1].x) / 2.0;
	yxmid = m[1] * (xmid - points[1].x) + points[1].y;
	m[0] = (yxmid - points[0].y) / (xmid - points[0].x);
	if ((m[0] * m1s) < 0.0) {
	    m[0] = 0.0;
	}
    }

}

/*
 * -----------------------------------------------------------------------
 *
 * QuadEval --
 *
 * 	QuadEval controls the evaluation of an osculatory quadratic
 * 	spline.  The user may provide his own slopes at the points of
 * 	interpolation or use the subroutine 'QuadSlopes' to calculate
 * 	slopes which are consistent with the shape of the data.
 *
 * ON INPUT--
 *   	intpPts	must be a nondecreasing vector of points at which the
 *		spline will be evaluated.
 *   	origPts	contains the abscissas of the data points to be
 *		interpolated. xtab must be increasing.
 *   	y	contains the ordinates of the data points to be
 *		interpolated.
 *   	m 	contains the slope of the spline at each point of
 *		interpolation.
 *   	nPoints	number of data points (dimension of xtab and y).
 *   	numEval is the number of points of evaluation (dimension of
 *		xval and yval).
 *   	epsilon 	is a relative error tolerance used in subroutine
 *		'QuadChoose' to distinguish the situation m(i) or
 *		m(i+1) is relatively close to the slope or twice
 *		the slope of the linear segment between xtab(i) and
 *		xtab(i+1).  If this situation occurs, roundoff may
 *		cause a change in convexity or monotonicity of the
 *   		resulting spline and a change in the case number
 *		provided by 'QuadChoose'.  If epsilon is not equal to zero,
 *		then epsilon should be greater than or equal to machine
 *		epsilon.
 * ON OUTPUT--
 * 	yval 	contains the images of the points in xval.
 *   	err 	is one of the following error codes:
 *      	0 - QuadEval ran normally.
 *      	1 - xval(i) is less than xtab(1) for at least one
 *		    i or xval(i) is greater than xtab(num) for at
 *		    least one i. QuadEval will extrapolate to provide
 *		    function values for these abscissas.
 *      	2 - xval(i+1) < xval(i) for some i.
 *
 *
 *  QuadEval calls the following subroutines or functions:
 *      Search
 *      QuadCases
 *      QuadChoose
 *      QuadSpline
 * -----------------------------------------------------------------------
 */
static int
QuadEval(origPts, nOrigPts, intpPts, nIntpPts, m, epsilon)
    Point2D origPts[];
    int nOrigPts;
    Point2D intpPts[];
    int nIntpPts;
    double *m;			/* Slope of the spline at each point
				 * of interpolation. */
    double epsilon;		/* Relative error tolerance (see choose) */
{
    int error;
    register int i, j;
    double param[10];
    int ncase;
    int start, end;
    int l, p;
    register int n;
    int found;

    /* Initialize indices and set error result */
    error = 0;
    l = nOrigPts - 1;
    p = l - 1;
    ncase = 1;

    /*
     * Determine if abscissas of new vector are non-decreasing.
     */
    for (j = 1; j < nIntpPts; j++) {
	if (intpPts[j].x < intpPts[j - 1].x) {
	    return 2;
	}
    }
    /*
     * Determine if any of the points in xval are LESS than the
     * abscissa of the first data point.
     */
    for (start = 0; start < nIntpPts; start++) {
	if (intpPts[start].x >= origPts[0].x) {
	    break;
	}
    }
    /*
     * Determine if any of the points in xval are GREATER than the
     * abscissa of the l data point.
     */
    for (end = nIntpPts - 1; end >= 0; end--) {
	if (intpPts[end].x <= origPts[l].x) {
	    break;
	}
    }

    if (start > 0) {
	error = 1;		/* Set error value to indicate that
				 * extrapolation has occurred. */
	/*
	 * Calculate the images of points of evaluation whose abscissas
	 * are less than the abscissa of the first data point.
	 */
	ncase = QuadSelect(origPts, origPts + 1, m[0], m[1], epsilon, param);
	for (j = 0; j < (start - 1); j++) {
	    QuadSpline(intpPts + j, origPts, origPts + 1, param, ncase);
	}
	if (nIntpPts == 1) {
	    return error;
	}
    }
    if ((nIntpPts == 1) && (end != (nIntpPts - 1))) {
	goto noExtrapolation;
    }
    
    /*
     * Search locates the interval in which the first in-range
     * point of evaluation lies.
     */

    i = Search(origPts, nOrigPts, intpPts[start].x, &found);
    
    n = i + 1;
    if (n >= nOrigPts) {
	n = nOrigPts - 1;
	i = nOrigPts - 2;
    }
    /*
     * If the first in-range point of evaluation is equal to one
     * of the data points, assign the appropriate value from y.
     * Continue until a point of evaluation is found which is not
     * equal to a data point.
     */
    if (found) {
	do {
	    intpPts[start].y = origPts[i].y;
	    start++;
	    if (start >= nIntpPts) {
		return error;
	    }
	} while (intpPts[start - 1].x == intpPts[start].x);
	
	for (;;) {
	    if (intpPts[start].x < origPts[n].x) {
		break;	/* Break out of for-loop */
	    }
	    if (intpPts[start].x == origPts[n].x) {
		do {
		    intpPts[start].y = origPts[n].y;
		    start++;
		    if (start >= nIntpPts) {
			return error;
		    }
		} while (intpPts[start].x == intpPts[start - 1].x);
	    }
	    i++;
	    n++;
	}
    }
    /*
     * Calculate the images of all the points which lie within
     * range of the data.
     */
    if ((i > 0) || (error != 1)) {
	ncase = QuadSelect(origPts + i, origPts + n, m[i], m[n], 
			   epsilon, param);
    }
    for (j = start; j <= end; j++) {
	/*
	 * If xx(j) - x(n) is negative, do not recalculate
	 * the parameters for this section of the spline since
	 * they are already known.
	 */
	if (intpPts[j].x == origPts[n].x) {
	    intpPts[j].y = origPts[n].y;
	    continue;
	} else if (intpPts[j].x > origPts[n].x) {
	    double delta;
	    
	    /* Determine that the routine is in the correct part of
	       the spline. */
	    do {
		i++, n++;
		delta = intpPts[j].x - origPts[n].x;
	    } while (delta > 0.0);
	    
	    if (delta < 0.0) {
		ncase = QuadSelect(origPts + i, origPts + n, m[i], 
			   m[n], epsilon, param);
	    } else if (delta == 0.0) {
		intpPts[j].y = origPts[n].y;
		continue;
	    }
	}
	QuadSpline(intpPts + j, origPts + i, origPts + n, param, ncase);
    }
    
    if (end == (nIntpPts - 1)) {
	return error;
    }
    if ((n == l) && (intpPts[end].x != origPts[l].x)) {
	goto noExtrapolation;
    }

    error = 1;			/* Set error value to indicate that
				 * extrapolation has occurred. */
    ncase = QuadSelect(origPts + p, origPts + l, m[p], m[l], epsilon, param);

  noExtrapolation:
    /*
     * Calculate the images of the points of evaluation whose
     * abscissas are greater than the abscissa of the last data point.
     */
    for (j = (end + 1); j < nIntpPts; j++) {
	QuadSpline(intpPts + j, origPts + p, origPts + l, param, ncase);
    }
    return error;
}

/*
 * -----------------------------------------------------------------------
 *
 *		  Shape preserving quadratic splines
 *		   by D.F.Mcallister & J.A.Roulier
 *		    Coded by S.L.Dodd & M.Roulier
 *			 N.C.State University
 *
 * -----------------------------------------------------------------------
 */
/*
 * Driver routine for quadratic spline package
 * On input--
 *   X,Y    Contain n-long arrays of data (x is increasing)
 *   XM     Contains m-long array of x values (increasing)
 *   eps    Relative error tolerance
 *   n      Number of input data points
 *   m      Number of output data points
 * On output--
 *   work   Contains the value of the first derivative at each data point
 *   ym     Contains the interpolated spline value at each data point
 */
int
Blt_QuadraticSpline(origPts, nOrigPts, intpPts, nIntpPts)
    Point2D origPts[];
    int nOrigPts;
    Point2D intpPts[];
    int nIntpPts;
{
    double epsilon;
    double *work;
    int result;

    work = Blt_Malloc(nOrigPts * sizeof(double));
    assert(work);

    epsilon = 0.0;		/* TBA: adjust error via command-line option */
    /* allocate space for vectors used in calculation */
    QuadSlopes(origPts, work, nOrigPts);
    result = QuadEval(origPts, nOrigPts, intpPts, nIntpPts, work, epsilon);
    Blt_Free(work);
    if (result > 1) {
	return FALSE;
    }
    return TRUE;
}

/*
 * ------------------------------------------------------------------------
 *
 * Reference:
 *	Numerical Analysis, R. Burden, J. Faires and A. Reynolds.
 *	Prindle, Weber & Schmidt 1981 pp 112
 *
 * Parameters:
 *	origPts - vector of points, assumed to be sorted along x.
 *	intpPts - vector of new points.
 *
 * ------------------------------------------------------------------------
 */
int
Blt_NaturalSpline(origPts, nOrigPts, intpPts, nIntpPts)
    Point2D origPts[];
    int nOrigPts;
    Point2D intpPts[];
    int nIntpPts;
{
    Cubic2D *eq;
    Point2D *iPtr, *endPtr;
    TriDiagonalMatrix *A;
    double *dx;		/* vector of deltas in x */
    double x, dy, alpha;
    int isKnot;
    register int i, j, n;

    dx = Blt_Malloc(sizeof(double) * nOrigPts);
    /* Calculate vector of differences */
    for (i = 0, j = 1; j < nOrigPts; i++, j++) {
	dx[i] = origPts[j].x - origPts[i].x;
	if (dx[i] < 0.0) {
	    return 0;
	}
    }
    n = nOrigPts - 1;		/* Number of intervals. */
    A = Blt_Malloc(sizeof(TriDiagonalMatrix) * nOrigPts);
    if (A == NULL) {
	Blt_Free(dx);
	return 0;
    }
    /* Vectors to solve the tridiagonal matrix */
    A[0][0] = A[n][0] = 1.0;
    A[0][1] = A[n][1] = 0.0;
    A[0][2] = A[n][2] = 0.0;

    /* Calculate the intermediate results */
    for (i = 0, j = 1; j < n; j++, i++) {
	alpha = 3.0 * ((origPts[j + 1].y / dx[j]) - (origPts[j].y / dx[i]) - 
		       (origPts[j].y / dx[j]) + (origPts[i].y / dx[i]));
	A[j][0] = 2 * (dx[j] + dx[i]) - dx[i] * A[i][1];
	A[j][1] = dx[j] / A[j][0];
	A[j][2] = (alpha - dx[i] * A[i][2]) / A[j][0];
    }
    eq = Blt_Malloc(sizeof(Cubic2D) * nOrigPts);

    if (eq == NULL) {
	Blt_Free(A);
	Blt_Free(dx);
	return FALSE;
    }
    eq[0].c = eq[n].c = 0.0;
    for (j = n, i = n - 1; i >= 0; i--, j--) {
	eq[i].c = A[i][2] - A[i][1] * eq[j].c;
	dy = origPts[i+1].y - origPts[i].y;
	eq[i].b = (dy) / dx[i] - dx[i] * (eq[j].c + 2.0 * eq[i].c) / 3.0;
	eq[i].d = (eq[j].c - eq[i].c) / (3.0 * dx[i]);
    }
    Blt_Free(A);
    Blt_Free(dx);

    endPtr = intpPts + nIntpPts;
    /* Now calculate the new values */
    for (iPtr = intpPts; iPtr < endPtr; iPtr++) {
	iPtr->y = 0.0;
	x = iPtr->x;

	/* Is it outside the interval? */
	if ((x < origPts[0].x) || (x > origPts[n].x)) {
	    continue;
	}
	/* Search for the interval containing x in the point array */
	i = Search(origPts, nOrigPts, x, &isKnot);
	if (isKnot) {
	    iPtr->y = origPts[i].y;
	} else {
	    i--;
	    x -= origPts[i].x;
	    iPtr->y = origPts[i].y + 
		x * (eq[i].b + x * (eq[i].c + x * eq[i].d));
	}
    }
    Blt_Free(eq);
    return TRUE;
}

static Blt_OpSpec splineOps[] =
{
    {"natural", 1, (Blt_Op)Blt_NaturalSpline, 6, 6, "x y splx sply",},
    {"quadratic", 1, (Blt_Op)Blt_QuadraticSpline, 6, 6, "x y splx sply",},
};
static int nSplineOps = sizeof(splineOps) / sizeof(Blt_OpSpec);

/*ARGSUSED*/
static int
SplineCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_Op proc;
    Blt_Vector *x, *y, *splX, *splY;
    double *xArr, *yArr;
    register int i;
    Point2D *origPts, *intpPts;
    int nOrigPts, nIntpPts;
    
    proc = Blt_GetOp(interp, nSplineOps, splineOps, BLT_OP_ARG1, argc, argv,0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    if ((Blt_GetVector(interp, argv[2], &x) != TCL_OK) ||
	(Blt_GetVector(interp, argv[3], &y) != TCL_OK) ||
	(Blt_GetVector(interp, argv[4], &splX) != TCL_OK)) {
	return TCL_ERROR;
    }
    nOrigPts = Blt_VecLength(x);
    if (nOrigPts < 3) {
	Tcl_AppendResult(interp, "length of vector \"", argv[2], "\" is < 3",
	    (char *)NULL);
	return TCL_ERROR;
    }
    for (i = 1; i < nOrigPts; i++) {
	if (Blt_VecData(x)[i] < Blt_VecData(x)[i - 1]) {
	    Tcl_AppendResult(interp, "x vector \"", argv[2],
		"\" must be monotonically increasing", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    /* Check that all the data points aren't the same. */
    if (Blt_VecData(x)[i - 1] <= Blt_VecData(x)[0]) {
	Tcl_AppendResult(interp, "x vector \"", argv[2],
	 "\" must be monotonically increasing", (char *)NULL);
	return TCL_ERROR;
    }
    if (nOrigPts != Blt_VecLength(y)) {
	Tcl_AppendResult(interp, "vectors \"", argv[2], "\" and \"", argv[3],
	    " have different lengths", (char *)NULL);
	return TCL_ERROR;
    }
    nIntpPts = Blt_VecLength(splX);
    if (Blt_GetVector(interp, argv[5], &splY) != TCL_OK) {
	/*
	 * If the named vector to hold the ordinates of the spline
	 * doesn't exist, create one the same size as the vector
	 * containing the abscissas.
	 */
	if (Blt_CreateVector(interp, argv[5], nIntpPts, &splY) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else if (nIntpPts != Blt_VecLength(splY)) {
	/*
	 * The x and y vectors differ in size. Make the number of ordinates
	 * the same as the number of abscissas.
	 */
	if (Blt_ResizeVector(splY, nIntpPts) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    origPts = Blt_Malloc(sizeof(Point2D) * nOrigPts);
    if (origPts == NULL) {
	Tcl_AppendResult(interp, "can't allocate \"", Blt_Itoa(nOrigPts), 
		"\" points", (char *)NULL);
	return TCL_ERROR;
    }
    intpPts = Blt_Malloc(sizeof(Point2D) * nIntpPts);
    if (intpPts == NULL) {
	Tcl_AppendResult(interp, "can't allocate \"", Blt_Itoa(nIntpPts), 
		"\" points", (char *)NULL);
	Blt_Free(origPts);
	return TCL_ERROR;
    }
    xArr = Blt_VecData(x);
    yArr = Blt_VecData(y);
    for (i = 0; i < nOrigPts; i++) {
	origPts[i].x = xArr[i];
	origPts[i].y = yArr[i];
    }
    xArr = Blt_VecData(splX);
    yArr = Blt_VecData(splY);
    for (i = 0; i < nIntpPts; i++) {
	intpPts[i].x = xArr[i];
	intpPts[i].y = yArr[i];
    }
    if (!(*proc) (origPts, nOrigPts, intpPts, nIntpPts)) {
	Tcl_AppendResult(interp, "error generating spline for \"", 
		Blt_NameOfVector(splY), "\"", (char *)NULL);
	Blt_Free(origPts);
	Blt_Free(intpPts);
	return TCL_ERROR;
    }
    yArr = Blt_VecData(splY);
    for (i = 0; i < nIntpPts; i++) {
	yArr[i] = intpPts[i].y;
    }
    Blt_Free(origPts);
    Blt_Free(intpPts);

    /* Finally update the vector. The size of the vector hasn't
     * changed, just the data. Reset the vector using TCL_STATIC to
     * indicate this. */
    if (Blt_ResetVector(splY, Blt_VecData(splY), Blt_VecLength(splY),
	    Blt_VecSize(splY), TCL_STATIC) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

int
Blt_SplineInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec =
    {"spline", SplineCmd,};

    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}


#define SQR(x)	((x)*(x))

typedef struct {
    double t;			/* Arc length of interval. */
    double x;			/* 2nd derivative of X with respect to T */
    double y;			/* 2nd derivative of Y with respect to T */
} CubicSpline;


/*
 * The following two procedures solve the special linear system which arise
 * in cubic spline interpolation. If x is assumed cyclic ( x[i]=x[n+i] ) the
 * equations can be written as (i=0,1,...,n-1):
 *     m[i][0] * x[i-1] + m[i][1] * x[i] + m[i][2] * x[i+1] = b[i] .
 * In matrix notation one gets A * x = b, where the matrix A is tridiagonal
 * with additional elements in the upper right and lower left position:
 *   A[i][0] = A_{i,i-1}  for i=1,2,...,n-1    and    m[0][0] = A_{0,n-1} ,
 *   A[i][1] = A_{i, i }  for i=0,1,...,n-1
 *   A[i][2] = A_{i,i+1}  for i=0,1,...,n-2    and    m[n-1][2] = A_{n-1,0}.
 * A should be symmetric (A[i+1][0] == A[i][2]) and positive definite.
 * The size of the system is given in n (n>=1).
 *
 * In the first procedure the Cholesky decomposition A = C^T * D * C
 * (C is upper triangle with unit diagonal, D is diagonal) is calculated.
 * Return TRUE if decomposition exist.
 */
static int 
SolveCubic1(A, n)
    TriDiagonalMatrix A[];
    int n;
{
    int i;
    double m_ij, m_n, m_nn, d;

    if (n < 1) {
	return FALSE;		/* Dimension should be at least 1 */
    }
    d = A[0][1];		/* D_{0,0} = A_{0,0} */
    if (d <= 0.0) {
	return FALSE;		/* A (or D) should be positive definite */
    }
    m_n = A[0][0];		/*  A_{0,n-1}  */
    m_nn = A[n - 1][1];		/* A_{n-1,n-1} */
    for (i = 0; i < n - 2; i++) {
	m_ij = A[i][2];		/*  A_{i,1}  */
	A[i][2] = m_ij / d;	/* C_{i,i+1} */
	A[i][0] = m_n / d;	/* C_{i,n-1} */
	m_nn -= A[i][0] * m_n;	/* to get C_{n-1,n-1} */
	m_n = -A[i][2] * m_n;	/* to get C_{i+1,n-1} */
	d = A[i + 1][1] - A[i][2] * m_ij;	/* D_{i+1,i+1} */
	if (d <= 0.0) {
	    return FALSE;	/* Elements of D should be positive */
	}
	A[i + 1][1] = d;
    }
    if (n >= 2) {		/* Complete last column */
	m_n += A[n - 2][2];	/* add A_{n-2,n-1} */
	A[n - 2][0] = m_n / d;	/* C_{n-2,n-1} */
	A[n - 1][1] = d = m_nn - A[n - 2][0] * m_n;	/* D_{n-1,n-1} */
	if (d <= 0.0) {
	    return FALSE;
	}
    }
    return TRUE;
}

/*
 * The second procedure solves the linear system, with the Cholesky
 * decomposition calculated above (in m[][]) and the right side b given
 * in x[]. The solution x overwrites the right side in x[].
 */
static void 
SolveCubic2(A, spline, nIntervals)
    TriDiagonalMatrix A[];
    CubicSpline spline[];
    int nIntervals;
{
    int i;
    double x, y;
    int n, m;

    n = nIntervals - 2;
    m = nIntervals - 1;

    /* Division by transpose of C : b = C^{-T} * b */
    x = spline[m].x;
    y = spline[m].y;
    for (i = 0; i < n; i++) {
	spline[i + 1].x -= A[i][2] * spline[i].x; /* C_{i,i+1} * x(i) */
	spline[i + 1].y -= A[i][2] * spline[i].y; /* C_{i,i+1} * x(i) */
	x -= A[i][0] * spline[i].x;	/* C_{i,n-1} * x(i) */
	y -= A[i][0] * spline[i].y; /* C_{i,n-1} * x(i) */
    }
    if (n >= 0) {
	/* C_{n-2,n-1} * x_{n-1} */
	spline[m].x = x - A[n][0] * spline[n].x; 
	spline[m].y = y - A[n][0] * spline[n].y; 
    }
    /* Division by D: b = D^{-1} * b */
    for (i = 0; i < nIntervals; i++) {
	spline[i].x /= A[i][1];
	spline[i].y /= A[i][1];
    }

    /* Division by C: b = C^{-1} * b */
    x = spline[m].x;
    y = spline[m].y;
    if (n >= 0) {
	/* C_{n-2,n-1} * x_{n-1} */
	spline[n].x -= A[n][0] * x;	
	spline[n].y -= A[n][0] * y;	
    }
    for (i = (n - 1); i >= 0; i--) {
	/* C_{i,i+1} * x_{i+1} + C_{i,n-1} * x_{n-1} */
	spline[i].x -= A[i][2] * spline[i + 1].x + A[i][0] * x;
	spline[i].y -= A[i][2] * spline[i + 1].y + A[i][0] * y;
    }
}

/*
 * Find second derivatives (x''(t_i),y''(t_i)) of cubic spline interpolation
 * through list of points (x_i,y_i). The parameter t is calculated as the
 * length of the linear stroke. The number of points must be at least 3.
 * Note: For CLOSED_CONTOURs the first and last point must be equal.
 */
static CubicSpline *
CubicSlopes(points, nPoints, isClosed, unitX, unitY)
    Point2D points[];
    int nPoints;		/* Number of points (nPoints>=3) */
    int isClosed;		/* CLOSED_CONTOUR or OPEN_CONTOUR  */
    double unitX, unitY;	/* Unit length in x and y (norm=1) */
{
    CubicSpline *spline;
    register CubicSpline *s1, *s2;
    int n, i;
    double norm, dx, dy;
    TriDiagonalMatrix *A;	/* The tri-diagonal matrix is saved here. */
    
    spline = Blt_Malloc(sizeof(CubicSpline) * nPoints);
    if (spline == NULL) {
	return NULL;
    }
    A = Blt_Malloc(sizeof(TriDiagonalMatrix) * nPoints);
    if (A == NULL) {
	Blt_Free(spline);
	return NULL;
    }
    /*
     * Calculate first differences in (dxdt2[i], y[i]) and interval lengths
     * in dist[i]:
     */
    s1 = spline;
    for (i = 0; i < nPoints - 1; i++) {
	s1->x = points[i+1].x - points[i].x;
	s1->y = points[i+1].y - points[i].y;

	/*
	 * The Norm of a linear stroke is calculated in "normal coordinates"
	 * and used as interval length:
	 */
	dx = s1->x / unitX;
	dy = s1->y / unitY;
	s1->t = sqrt(dx * dx + dy * dy);

	s1->x /= s1->t;	/* first difference, with unit norm: */
	s1->y /= s1->t;	/*   || (dxdt2[i], y[i]) || = 1      */
	s1++;
    }

    /*
     * Setup linear System:  Ax = b
     */
    n = nPoints - 2;		/* Without first and last point */
    if (isClosed) {
	/* First and last points must be equal for CLOSED_CONTOURs */
	spline[nPoints - 1].t = spline[0].t;
	spline[nPoints - 1].x = spline[0].x;
	spline[nPoints - 1].y = spline[0].y;
	n++;			/* Add last point (= first point) */
    }
    s1 = spline, s2 = s1 + 1;
    for (i = 0; i < n; i++) {
	/* Matrix A, mainly tridiagonal with cyclic second index 
	   ("j = j+n mod n") 
	*/
	A[i][0] = s1->t;	/* Off-diagonal element A_{i,i-1} */
	A[i][1] = 2.0 * (s1->t + s2->t);	/* A_{i,i} */
	A[i][2] = s2->t;	/* Off-diagonal element A_{i,i+1} */

	/* Right side b_x and b_y */
	s1->x = (s2->x - s1->x) * 6.0;
	s1->y = (s2->y - s1->y) * 6.0;

	/* 
	 * If the linear stroke shows a cusp of more than 90 degree,
	 * the right side is reduced to avoid oscillations in the
	 * spline: 
	 */
	/*
	 * The Norm of a linear stroke is calculated in "normal coordinates"
	 * and used as interval length:
	 */
	dx = s1->x / unitX;
	dy = s1->y / unitY;
	norm = sqrt(dx * dx + dy * dy) / 8.5;
	if (norm > 1.0) {
	    /* The first derivative will not be continuous */
	    s1->x /= norm;
	    s1->y /= norm;
	}
	s1++, s2++;
    }

    if (!isClosed) {
	/* Third derivative is set to zero at both ends */
	A[0][1] += A[0][0];	/* A_{0,0}     */
	A[0][0] = 0.0;		/* A_{0,n-1}   */
	A[n-1][1] += A[n-1][2]; /* A_{n-1,n-1} */
	A[n-1][2] = 0.0;	/* A_{n-1,0}   */
    }
    /* Solve linear systems for dxdt2[] and y[] */

    if (SolveCubic1(A, n)) {	/* Cholesky decomposition */
	SolveCubic2(A, spline, n); /* A * dxdt2 = b_x */
    } else {			/* Should not happen, but who knows ... */
	Blt_Free(A);
	Blt_Free(spline);
	return NULL;
    }
    /* Shift all second derivatives one place right and update the ends. */
    s2 = spline + n, s1 = s2 - 1;
    for (/* empty */; s2 > spline; s2--, s1--) {
	s2->x = s1->x;
	s2->y = s1->y;
    }
    if (isClosed) {
	spline[0].x = spline[n].x;
	spline[0].y = spline[n].y;
    } else {
	/* Third derivative is 0.0 for the first and last interval. */
	spline[0].x = spline[1].x; 
	spline[0].y = spline[1].y; 
	spline[n + 1].x = spline[n].x;
	spline[n + 1].y = spline[n].y;
    }
    Blt_Free( A);
    return spline;
}


/*
 * Calculate interpolated values of the spline function (defined via p_cntr
 * and the second derivatives dxdt2[] and dydt2[]). The number of tabulated
 * values is n. On an equidistant grid n_intpol values are calculated.
 */
static int
CubicEval(origPts, nOrigPts, intpPts, nIntpPts, spline)
    Point2D origPts[];
    int nOrigPts;
    Point2D intpPts[];
    int nIntpPts;
    CubicSpline spline[];
{
    double t, tSkip, tMax;
    Point2D p, q;
    double d, hx, dx0, dx01, hy, dy0, dy01;
    register int i, j, count;

    /* Sum the lengths of all the segments (intervals). */
    tMax = 0.0;
    for (i = 0; i < nOrigPts - 1; i++) {
	tMax += spline[i].t;
    }

    /* Need a better way of doing this... */

    /* The distance between interpolated points */
    tSkip = (1. - 1e-7) * tMax / (nIntpPts - 1);
    
    t = 0.0;			/* Spline parameter value. */
    q = origPts[0];
    count = 0;

    intpPts[count++] = q;	/* First point. */
    t += tSkip;
    
    for (i = 0, j = 1; j < nOrigPts; i++, j++) {
	d = spline[i].t;	/* Interval length */
	p = q;
	q = origPts[i+1];
	hx = (q.x - p.x) / d;
	hy = (q.y - p.y) / d;
	dx0 = (spline[j].x + 2 * spline[i].x) / 6.0;
	dy0 = (spline[j].y + 2 * spline[i].y) / 6.0;
	dx01 = (spline[j].x - spline[i].x) / (6.0 * d);
	dy01 = (spline[j].y - spline[i].y) / (6.0 * d);
	while (t <= spline[i].t) { /* t in current interval ? */
	    p.x += t * (hx + (t - d) * (dx0 + t * dx01));
	    p.y += t * (hy + (t - d) * (dy0 + t * dy01));
	    intpPts[count++] = p;
	    t += tSkip;
	}
	/* Parameter t relative to start of next interval */
	t -= spline[i].t;
    }
    return count;
}

/*
 * Generate a cubic spline curve through the points (x_i,y_i) which are
 * stored in the linked list p_cntr.
 * The spline is defined as a 2d-function s(t) = (x(t),y(t)), where the
 * parameter t is the length of the linear stroke.
 */
int
Blt_NaturalParametricSpline(origPts, nOrigPts, extsPtr, isClosed, 
	intpPts, nIntpPts)
    Point2D origPts[];
    int nOrigPts;
    Extents2D *extsPtr;
    int isClosed;
    Point2D *intpPts;
    int nIntpPts;
{
    double unitX, unitY;	/* To define norm (x,y)-plane */
    CubicSpline *spline;
    int result;

    if (nOrigPts < 3) {
	return 0;
    }
    if (isClosed) {
	origPts[nOrigPts].x = origPts[0].x;
	origPts[nOrigPts].y = origPts[0].y;
	nOrigPts++;
    }
    /* Width and height of the grid is used at unit length (2d-norm) */
    unitX = extsPtr->right - extsPtr->left;
    unitY = extsPtr->bottom - extsPtr->top;

    if (unitX < FLT_EPSILON) {
	unitX = FLT_EPSILON;
    }
    if (unitY < FLT_EPSILON) {
	unitY = FLT_EPSILON;
    }
    /* Calculate parameters for cubic spline: 
     *		t     = arc length of interval.
     *		dxdt2 = second derivatives of x with respect to t, 
     *		dydt2 = second derivatives of y with respect to t, 
     */
    spline = CubicSlopes(origPts, nOrigPts, isClosed, unitX, unitY);
    if (spline == NULL) {
	return 0;
    }
    result= CubicEval(origPts, nOrigPts, intpPts, nIntpPts, spline);
    Blt_Free(spline);
    return result;
}

static void
CatromCoeffs(p, a, b, c, d)
    Point2D *p;
    Point2D *a, *b, *c, *d;
{
    a->x = -p[0].x + 3.0 * p[1].x - 3.0 * p[2].x + p[3].x;
    b->x = 2.0 * p[0].x - 5.0 * p[1].x + 4.0 * p[2].x - p[3].x;
    c->x = -p[0].x + p[2].x;
    d->x = 2.0 * p[1].x;
    a->y = -p[0].y + 3.0 * p[1].y - 3.0 * p[2].y + p[3].y;
    b->y = 2.0 * p[0].y - 5.0 * p[1].y + 4.0 * p[2].y - p[3].y;
    c->y = -p[0].y + p[2].y;
    d->y = 2.0 * p[1].y;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ParametricCatromSpline --
 *
 *	Computes a spline based upon the data points, returning a new
 *	(larger) coordinate array or points.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
int
Blt_CatromParametricSpline(points, nPoints, intpPts, nIntpPts)
    Point2D *points;
    int nPoints;
    Point2D *intpPts;
    int nIntpPts;
{
    register int i;
    Point2D *origPts;
    double t;
    int interval;
    Point2D a, b, c, d;

    assert(nPoints > 0);
    /*
     * The spline is computed in screen coordinates instead of data
     * points so that we can select the abscissas of the interpolated
     * points from each pixel horizontally across the plotting area.
     */
    origPts = Blt_Malloc((nPoints + 4) * sizeof(Point2D));
    memcpy(origPts + 1, points, sizeof(Point2D) * nPoints);

    origPts[0] = origPts[1];
    origPts[nPoints + 2] = origPts[nPoints + 1] = origPts[nPoints];

    for (i = 0; i < nIntpPts; i++) {
	interval = (int)intpPts[i].x;
	t = intpPts[i].y;
	assert(interval < nPoints);
	CatromCoeffs(origPts + interval, &a, &b, &c, &d);
	intpPts[i].x = (d.x + t * (c.x + t * (b.x + t * a.x))) / 2.0;
	intpPts[i].y = (d.y + t * (c.y + t * (b.y + t * a.y))) / 2.0;
    }
    Blt_Free(origPts);
    return 1;
}

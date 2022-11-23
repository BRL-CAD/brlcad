/*                       T R I _ T R I . C
 * BRL-CAD
 *
 * Published in 2012 by the United States Government.
 * This work is in the public domain.
 *
 */
/* Triangle/triangle intersection test routine,
 * by Tomas Moller, 1997.
 * See article "A Fast Triangle-Triangle Intersection Test",
 * Journal of Graphics Tools, 2(2), 1997
 * updated: 2001-06-20 (added line of intersection)
 *
 * Updated June 1999: removed the divisions -- a little faster now!
 * Updated October 1999: added {} to CROSS and SUB macros
 *
 * Calculate whether two coplanar triangles intersect:
 *
 * int bg_tri_tri_isect_coplanar(point_t V0, point_t V1, point_t V2,
 *                               point_t U0, point_t U1, point_t U2,
 *                               int area_flag)
 * parameters: vertices of triangle 1: V0, V1, V2
 *             vertices of triangle 2: U0, U1, U2
 *             flag to tell function to require non-zero area: area_flag
 * result    : returns 1 if the triangles intersect, otherwise 0
 *
 * Calculate whether two triangles intersect:
 *
 * int bg_tri_tri_isect(point_t V0, point_t V1, point_t V2,
 *                      point_t U0, point_t U1, point_t U2)
 * parameters: vertices of triangle 1: V0, V1, V2
 *             vertices of triangle 2: U0, U1, U2
 * result    : returns 1 if the triangles intersect, otherwise 0
 *
 * This version computes the line of intersection as well (if they
 * are not coplanar):
 *
 * int bg_tri_tri_isect_with_line(point_t V0, point_t V1, point_t V2,
 *			          point_t U0, point_t U1, point_t U2,
 *			          int *coplanar, point_t *isectpt1,
 *			          point_t *isectpt2);
 *
 * coplanar returns whether the tris are coplanar
 * isectpt1, isectpt2 are the endpoints of the line of intersection
 *
 * License:  public domain (from Moller's collection of public domain code at
 * http://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/code/)
 *
 * The changes made for BRL-CAD integration were to use the point_t
 * data type instead of fastf_t arrays and use vmath's vector macros
 * instead of the locally defined versions.  The function names were
 * changed to bg_tri_tri_isect and bg_tri_tri_isect_with_line.
 * A number of minor changes were made for C89 compatibility.  BRL-CAD's
 * NEAR_ZERO macro was used in place of exact floating point comparisons.
 *
 */

#include "common.h"
#include <math.h>
#include "vmath.h"
#include "bg/plane.h"
#include "bn/tol.h"
#include "bg/tri_tri.h"

/* if USE_EPSILON_TEST is true then we do a check:
   if |dv|<EPSILON then dv=0.0;
   else no check is done (which is less robust)
*/
#define USE_EPSILON_TEST 1
#define EPSILON 0.000001

/* sort so that a<=b */
#define SORT(a, b)       \
    if (a>b) {    \
	fastf_t c_tmp; \
	c_tmp=a;     \
	a=b;     \
	b=c_tmp;     \
    }

#define SORT2(a, b, smallest)       \
    if (a>b) {      \
	fastf_t c_tmp;    \
	c_tmp=a;        \
	a=b;        \
	b=c_tmp;        \
	smallest=1; \
    }             \
    else smallest=0;


/* this edge to edge test is based on Franklin Antonio's gem:
   "Faster Line Sebgent Intersection", in Graphics Gems III,
   pp. 199-202 */
#define EDGE_EDGE_TEST(V0, U0, U1)                      \
    Bx=U0[i0]-U1[i0];                                   \
    By=U0[i1]-U1[i1];                                   \
    Cx=V0[i0]-U0[i0];                                   \
    Cy=V0[i1]-U0[i1];                                   \
    f=Ay*Bx-Ax*By;                                      \
    d=By*Cx-Bx*Cy;                                      \
    if ((f>0 && d>=0 && d<=f) || (f<0 && d<=0 && d>=f)) { \
	e=Ax*Cy-Ay*Cx;                                    \
	if (f>0) {                                           \
	    if (e>=0 && e<=f) return 1;                      \
	} else {                                                 \
	    if (e<=0 && e>=f) return 1;                      \
	}                                                 \
    }

#define EDGE_EDGE_TEST_AREA(V0, U0, U1)                 \
    Bx=U0[i0]-U1[i0];                                   \
    By=U0[i1]-U1[i1];                                   \
    Cx=V0[i0]-U0[i0];                                   \
    Cy=V0[i1]-U0[i1];                                   \
    f=Ay*Bx-Ax*By;                                      \
    d=By*Cx-Bx*Cy;                                      \
    if ((f>0 && d>0 && d<f && !NEAR_EQUAL(d, f, EPSILON)) || (f<0 && d<0 && d>f && !NEAR_EQUAL(d, f, EPSILON))) {                                                   \
	e=Ax*Cy-Ay*Cx;                                     \
	if (f>EPSILON) {                                                 \
	    if (e>EPSILON && e<f && !NEAR_EQUAL(e, f, EPSILON)) return 1;                          \
	} else {                                                 \
	    if (e<EPSILON && e>f && !NEAR_EQUAL(e, f, EPSILON)) return 1;                          \
	}                                                 \
    }


#define EDGE_AGAINST_TRI_EDGES(V0, V1, U0, U1, U2) \
    {                                              \
	fastf_t Ax, Ay, Bx, By, Cx, Cy, e, d, f;             \
	Ax=V1[i0]-V0[i0];                            \
	Ay=V1[i1]-V0[i1];                            \
	/* test edge U0, U1 against V0, V1 */          \
	EDGE_EDGE_TEST(V0, U0, U1);                    \
	/* test edge U1, U2 against V0, V1 */          \
	EDGE_EDGE_TEST(V0, U1, U2);                    \
	/* test edge U2, U1 against V0, V1 */          \
	EDGE_EDGE_TEST(V0, U2, U0);                    \
    }


#define EDGE_AGAINST_TRI_EDGES_AREA(V0, V1, U0, U1, U2) \
    {                                                   \
	fastf_t Ax, Ay, Bx, By, Cx, Cy, e, d, f;                  \
	Ax=V1[i0]-V0[i0];                                 \
	Ay=V1[i1]-V0[i1];                                 \
	/* test edge U0, U1 against V0, V1 */               \
	EDGE_EDGE_TEST_AREA(V0, U0, U1);                    \
	/* test edge U1, U2 against V0, V1 */               \
	EDGE_EDGE_TEST_AREA(V0, U1, U2);                    \
	/* test edge U2, U1 against V0, V1 */               \
	EDGE_EDGE_TEST_AREA(V0, U2, U0);                    \
    }


#define POINT_IN_TRI(V0, U0, U1, U2)           \
    {                                           \
	fastf_t a, b, c, d0, d1, d2;                     \
	/* is T1 completely inside T2? */          \
	/* check if V0 is inside tri(U0, U1, U2) */ \
	a=U1[i1]-U0[i1];                          \
	b=-(U1[i0]-U0[i0]);                       \
	c=-a*U0[i0]-b*U0[i1];                     \
	d0=a*V0[i0]+b*V0[i1]+c;                   \
					    \
	a=U2[i1]-U1[i1];                          \
	b=-(U2[i0]-U1[i0]);                       \
	c=-a*U1[i0]-b*U1[i1];                     \
	d1=a*V0[i0]+b*V0[i1]+c;                   \
					    \
	a=U0[i1]-U2[i1];                          \
	b=-(U0[i0]-U2[i0]);                       \
	c=-a*U2[i0]-b*U2[i1];                     \
	d2=a*V0[i0]+b*V0[i1]+c;                   \
	if (d0*d1>0.0) {                                         \
	    if (d0*d2>0.0) return 1;                 \
	}                                         \
    }

int bg_tri_tri_isect_coplanar(point_t V0, point_t V1, point_t V2,
			      point_t U0, point_t U1, point_t U2, int area_flag)
{
    int ret;
    fastf_t A[3];
    short i0, i1;
    point_t E1, E2, N;
    plane_t P1, P2;
    static const struct bn_tol tol = {
	BN_TOL_MAGIC, EPSILON, EPSILON*EPSILON, 1e-6, 1-1e-6
    };

    /* compute plane of triangle (V0, V1, V2) */
    ret = bg_make_plane_3pnts(P1, V0, V1, V2, &tol);
    if (ret) return -1;
    /* compute plane of triangle (U0, U1, U2) */
    ret = bg_make_plane_3pnts(P2, U0, U1, U2, &tol);
    if (ret) return -1;
    /* verify that triangles are coplanar */
    if (bg_coplanar(P1, P2, &tol) <= 0) return -1;

    /* first project onto an axis-aligned plane, that maximizes the area */
    /* of the triangles, compute indices: i0, i1. */
    VSUB2(E1, V1, V0);
    VSUB2(E2, V2, V0);
    VCROSS(N, E1, E2);

    A[0]=fabs(N[0]);
    A[1]=fabs(N[1]);
    A[2]=fabs(N[2]);
    if (A[0]>A[1]) {
	if (A[0]>A[2]) {
	    i0=1;      /* A[0] is greatest */
	    i1=2;
	} else {
	    i0=0;      /* A[2] is greatest */
	    i1=1;
	}
    } else {
	/* A[0]<=A[1] */
	if (A[2]>A[1]) {
	    i0=0;      /* A[2] is greatest */
	    i1=1;
	} else {
	    i0=0;      /* A[1] is greatest */
	    i1=2;
	}
    }

    /* test all edges of triangle 1 against the edges of triangle 2 */
    if (!area_flag) {
	EDGE_AGAINST_TRI_EDGES(V0, V1, U0, U1, U2);
	EDGE_AGAINST_TRI_EDGES(V1, V2, U0, U1, U2);
	EDGE_AGAINST_TRI_EDGES(V2, V0, U0, U1, U2);
    } else {
	EDGE_AGAINST_TRI_EDGES_AREA(V0, V1, U0, U1, U2);
	EDGE_AGAINST_TRI_EDGES_AREA(V1, V2, U0, U1, U2);
	EDGE_AGAINST_TRI_EDGES_AREA(V2, V0, U0, U1, U2);
    }

    /* finally, test if tri1 is totally contained in tri2 or vice versa */
    POINT_IN_TRI(V0, U0, U1, U2);
    POINT_IN_TRI(U0, V0, V1, V2);

    return 0;
}

static int
_bg_point_in_tri(short i0, short i1, point_t V0, point_t U0, point_t U1, point_t U2) {
    fastf_t a, b, c, d0, d1, d2;
    /* is T1 completely inside T2? */
    /* check if V0 is inside tri(U0, U1, U2) */
    a=U1[i1]-U0[i1];
    b=-(U1[i0]-U0[i0]);
    c=-a*U0[i0]-b*U0[i1];
    d0=a*V0[i0]+b*V0[i1]+c;

    a=U2[i1]-U1[i1];
    b=-(U2[i0]-U1[i0]);
    c=-a*U1[i0]-b*U1[i1];
    d1=a*V0[i0]+b*V0[i1]+c;

    a=U0[i1]-U2[i1];
    b=-(U0[i0]-U2[i0]);
    c=-a*U2[i0]-b*U2[i1];
    d2=a*V0[i0]+b*V0[i1]+c;
    if (d0*d1>0.0) {
	if (d0*d2>0.0) return 1;
    }

    return 0;
}

int bg_tri_tri_isect_coplanar2(point_t V0, point_t V1, point_t V2,
			      point_t U0, point_t U1, point_t U2, int area_flag)
{
    int ret;
    fastf_t A[3];
    short i0, i1;
    point_t E1, E2, N;
    plane_t P1, P2;
    static const struct bn_tol tol = {
	BN_TOL_MAGIC, EPSILON, EPSILON*EPSILON, 1e-6, 1-1e-6
    };

    /* compute plane of triangle (V0, V1, V2) */
    ret = bg_make_plane_3pnts(P1, V0, V1, V2, &tol);
    if (ret) return -1;
    /* compute plane of triangle (U0, U1, U2) */
    ret = bg_make_plane_3pnts(P2, U0, U1, U2, &tol);
    if (ret) return -1;
    /* verify that triangles are coplanar */
    if (bg_coplanar(P1, P2, &tol) <= 0) return -1;

    /* first project onto an axis-aligned plane, that maximizes the area */
    /* of the triangles, compute indices: i0, i1. */
    VSUB2(E1, V1, V0);
    VSUB2(E2, V2, V0);
    VCROSS(N, E1, E2);

    A[0]=fabs(N[0]);
    A[1]=fabs(N[1]);
    A[2]=fabs(N[2]);
    if (A[0]>A[1]) {
	if (A[0]>A[2]) {
	    i0=1;      /* A[0] is greatest */
	    i1=2;
	} else {
	    i0=0;      /* A[2] is greatest */
	    i1=1;
	}
    } else {
	/* A[0]<=A[1] */
	if (A[2]>A[1]) {
	    i0=0;      /* A[2] is greatest */
	    i1=1;
	} else {
	    i0=0;      /* A[1] is greatest */
	    i1=2;
	}
    }

    /* test all edges of triangle 1 against the edges of triangle 2 */
    if (!area_flag) {
	EDGE_AGAINST_TRI_EDGES(V0, V1, U0, U1, U2);
	EDGE_AGAINST_TRI_EDGES(V1, V2, U0, U1, U2);
	EDGE_AGAINST_TRI_EDGES(V2, V0, U0, U1, U2);
    } else {
	EDGE_AGAINST_TRI_EDGES_AREA(V0, V1, U0, U1, U2);
	EDGE_AGAINST_TRI_EDGES_AREA(V1, V2, U0, U1, U2);
	EDGE_AGAINST_TRI_EDGES_AREA(V2, V0, U0, U1, U2);
    }

    /* finally, test if tri1 is totally contained in tri2 or vice versa */
    int p1inside = 0;
    p1inside += _bg_point_in_tri(i0, i1, V0, U0, U1, U2);
    p1inside += _bg_point_in_tri(i0, i1, V1, U0, U1, U2);
    p1inside += _bg_point_in_tri(i0, i1, V2, U0, U1, U2);
    if (p1inside == 3) {
	return 1;
    }

    int p2inside = 0;
    p2inside += _bg_point_in_tri(i0, i1, U0, V0, V1, V2);
    p2inside += _bg_point_in_tri(i0, i1, U1, V0, V1, V2);
    p2inside += _bg_point_in_tri(i0, i1, U2, V0, V1, V2);

    if (p2inside == 3) {
	return 1;
    }

    return 0;
}

/* Internal coplanar test used by more general routines.  Separate from
 * the external function because this version of the test can assume
 * coplanar and does not need to test for area-only intersections. */
int coplanar_tri_tri(point_t N, point_t V0, point_t V1, point_t V2,
		     point_t U0, point_t U1, point_t U2)
{
    fastf_t A[3];
    short i0, i1;

    /* first project onto an axis-aligned plane, that maximizes the area */
    /* of the triangles, compute indices: i0, i1. */
    A[0]=fabs(N[0]);
    A[1]=fabs(N[1]);
    A[2]=fabs(N[2]);
    if (A[0]>A[1]) {
	if (A[0]>A[2]) {
	    i0=1;      /* A[0] is greatest */
	    i1=2;
	} else {
	    i0=0;      /* A[2] is greatest */
	    i1=1;
	}
    } else {
	/* A[0]<=A[1] */
	if (A[2]>A[1]) {
	    i0=0;      /* A[2] is greatest */
	    i1=1;
	} else {
	    i0=0;      /* A[1] is greatest */
	    i1=2;
	}
    }

    /* test all edges of triangle 1 against the edges of triangle 2 */
    EDGE_AGAINST_TRI_EDGES(V0, V1, U0, U1, U2);
    EDGE_AGAINST_TRI_EDGES(V1, V2, U0, U1, U2);
    EDGE_AGAINST_TRI_EDGES(V2, V0, U0, U1, U2);

    /* finally, test if tri1 is totally contained in tri2 or vice versa */
    POINT_IN_TRI(V0, U0, U1, U2);
    POINT_IN_TRI(U0, V0, V1, V2);

    return 0;
}


#define NEWCOMPUTE_INTERVALS(VV0, VV1, VV2, D0, D1, D2, D0D1, D0D2, A, B, C, X0, X1) \
    { \
	if (D0D1>0.0f) { \
	    /* here we know that D0D2<=0.0 */ \
	    /* that is D0, D1 are on the same side, D2 on the other or on the plane */ \
	    A=VV2; B=(VV0-VV2)*D2; C=(VV1-VV2)*D2; X0=D2-D0; X1=D2-D1; \
	} else if (D0D2>0.0f) { \
	    /* here we know that d0d1<=0.0 */ \
	    A=VV1; B=(VV0-VV1)*D1; C=(VV2-VV1)*D1; X0=D1-D0; X1=D1-D2; \
	} else if (D1*D2>0.0f || !NEAR_ZERO(D0, SMALL_FASTF)) { \
	    /* here we know that d0d1<=0.0 or that D0!=0.0 */ \
	    A=VV0; B=(VV1-VV0)*D0; C=(VV2-VV0)*D0; X0=D0-D1; X1=D0-D2; \
	} else if (!NEAR_ZERO(D1, SMALL_FASTF)) { \
	    A=VV1; B=(VV0-VV1)*D1; C=(VV2-VV1)*D1; X0=D1-D0; X1=D1-D2; \
	} else if (!NEAR_ZERO(D2, SMALL_FASTF)) { \
	    A=VV2; B=(VV0-VV2)*D2; C=(VV1-VV2)*D2; X0=D2-D0; X1=D2-D1; \
	} else { \
	    /* triangles are coplanar */ \
	    return coplanar_tri_tri(N1, V0, V1, V2, U0, U1, U2); \
	} \
    }


int bg_tri_tri_isect(point_t V0, point_t V1, point_t V2,
		     point_t U0, point_t U1, point_t U2)
{
    point_t E1, E2;
    point_t N1, N2;
    fastf_t d1, d2;
    fastf_t du0, du1, du2, dv0, dv1, dv2;
    fastf_t D[3];
    fastf_t isect1[2], isect2[2];
    fastf_t du0du1, du0du2, dv0dv1, dv0dv2;
    short index;
    fastf_t vp0, vp1, vp2;
    fastf_t up0, up1, up2;
    fastf_t bb, cc, max;
    /* parameters for triangle 1 interval computation */
    fastf_t a, b, c, tri_x0, tri_x1;
    /* parameters for triangle 2 interval computation */
    fastf_t d, e, f, tri_y0, tri_y1;

    fastf_t xx, yy, xxyy, tmp;

    /* compute plane equation of triangle(V0, V1, V2) */
    VSUB2(E1, V1, V0);
    VSUB2(E2, V2, V0);
    VCROSS(N1, E1, E2);
    d1=-VDOT(N1, V0);
    /* plane equation 1: N1.X+d1=0 */

    /* put U0, U1, U2 into plane equation 1 to compute signed distances to the plane*/
    du0=VDOT(N1, U0)+d1;
    du1=VDOT(N1, U1)+d1;
    du2=VDOT(N1, U2)+d1;

    /* coplanarity robustness check */
#if USE_EPSILON_TEST
    if (fabs(du0)<EPSILON) du0=0.0;
    if (fabs(du1)<EPSILON) du1=0.0;
    if (fabs(du2)<EPSILON) du2=0.0;
#endif
    du0du1=du0*du1;
    du0du2=du0*du2;

    if (du0du1>0.0f && du0du2>0.0f) /* same sign on all of them + not equal 0 ? */
	return 0;                    /* no intersection occurs */

    /* compute plane of triangle (U0, U1, U2) */
    VSUB2(E1, U1, U0);
    VSUB2(E2, U2, U0);
    VCROSS(N2, E1, E2);
    d2=-VDOT(N2, U0);
    /* plane equation 2: N2.X+d2=0 */

    /* put V0, V1, V2 into plane equation 2 */
    dv0=VDOT(N2, V0)+d2;
    dv1=VDOT(N2, V1)+d2;
    dv2=VDOT(N2, V2)+d2;

#if USE_EPSILON_TEST
    if (fabs(dv0)<EPSILON) dv0=0.0;
    if (fabs(dv1)<EPSILON) dv1=0.0;
    if (fabs(dv2)<EPSILON) dv2=0.0;
#endif

    dv0dv1=dv0*dv1;
    dv0dv2=dv0*dv2;

    if (dv0dv1>0.0f && dv0dv2>0.0f) /* same sign on all of them + not equal 0 ? */
	return 0;                    /* no intersection occurs */

    /* compute direction of intersection line */
    VCROSS(D, N1, N2);

    /* compute and index to the largest component of D */
    max=(float)fabs(D[0]);
    index=0;
    bb=(float)fabs(D[1]);
    cc=(float)fabs(D[2]);
    if (bb>max) max=bb, index=1;
    if (cc>max) index=2;

    /* this is the simplified projection onto L*/
    vp0=V0[index];
    vp1=V1[index];
    vp2=V2[index];

    up0=U0[index];
    up1=U1[index];
    up2=U2[index];

    /* compute interval for triangle 1 */
    NEWCOMPUTE_INTERVALS(vp0, vp1, vp2, dv0, dv1, dv2, dv0dv1, dv0dv2, a, b, c, tri_x0, tri_x1);

    /* compute interval for triangle 2 */
    NEWCOMPUTE_INTERVALS(up0, up1, up2, du0, du1, du2, du0du1, du0du2, d, e, f, tri_y0, tri_y1);

    xx=tri_x0*tri_x1;
    yy=tri_y0*tri_y1;
    xxyy=xx*yy;

    tmp=a*xxyy;
    isect1[0]=tmp+b*tri_x1*yy;
    isect1[1]=tmp+c*tri_x0*yy;

    tmp=d*xxyy;
    isect2[0]=tmp+e*xx*tri_y1;
    isect2[1]=tmp+f*xx*tri_y0;

    SORT(isect1[0], isect1[1]);
    SORT(isect2[0], isect2[1]);

    if (isect1[1]<isect2[0] || isect2[1]<isect1[0]) return 0;
    return 1;
}


void calc_isect2(point_t VTX0, point_t VTX1, point_t VTX2, fastf_t VV0, fastf_t VV1,
		 fastf_t VV2, fastf_t D0, fastf_t D1, fastf_t D2, fastf_t *isect0,
		 fastf_t *isect1, point_t isectpoint0, point_t isectpoint1)
{
    fastf_t tmp=D0/(D0-D1);
    point_t diff;
    *isect0=VV0+(VV1-VV0)*tmp;
    VSUB2(diff, VTX1, VTX0);
    VSCALE(diff, diff, tmp);
    VADD2(isectpoint0, diff, VTX0);
    tmp=D0/(D0-D2);
    *isect1=VV0+(VV2-VV0)*tmp;
    VSUB2(diff, VTX2, VTX0);
    VSCALE(diff, diff, tmp);
    VADD2(isectpoint1, VTX0, diff);
}


int compute_intervals_isectline(point_t VERT0, point_t VERT1, point_t VERT2,
				fastf_t VV0, fastf_t VV1, fastf_t VV2, fastf_t D0, fastf_t D1, fastf_t D2,
				fastf_t D0D1, fastf_t D0D2, fastf_t *isect0, fastf_t *isect1,
				point_t isectpoint0, point_t isectpoint1)
{
    if (D0D1>0.0f) {
	/* here we know that D0D2<=0.0 */
	/* that is D0, D1 are on the same side, D2 on the other or on the plane */
	calc_isect2(VERT2, VERT0, VERT1, VV2, VV0, VV1, D2, D0, D1, isect0, isect1, isectpoint0, isectpoint1);
    } else if (D0D2>0.0f) {
	/* here we know that d0d1<=0.0 */
	calc_isect2(VERT1, VERT0, VERT2, VV1, VV0, VV2, D1, D0, D2, isect0, isect1, isectpoint0, isectpoint1);
    } else if (D1*D2>0.0f || !NEAR_ZERO(D0, SMALL_FASTF)) {
	/* here we know that d0d1<=0.0 or that D0!=0.0 */
	calc_isect2(VERT0, VERT1, VERT2, VV0, VV1, VV2, D0, D1, D2, isect0, isect1, isectpoint0, isectpoint1);
    } else if (!NEAR_ZERO(D1, SMALL_FASTF)) {
	calc_isect2(VERT1, VERT0, VERT2, VV1, VV0, VV2, D1, D0, D2, isect0, isect1, isectpoint0, isectpoint1);
    } else if (!NEAR_ZERO(D2, SMALL_FASTF)) {
	calc_isect2(VERT2, VERT0, VERT1, VV2, VV0, VV1, D2, D0, D1, isect0, isect1, isectpoint0, isectpoint1);
    } else {
	/* triangles are coplanar */
	return 1;
    }
    return 0;
}


int bg_tri_tri_isect_with_line(point_t V0, point_t V1, point_t V2,
			       point_t U0, point_t U1, point_t U2,
			       int *coplanar, point_t *isectpt1, point_t *isectpt2)
{
    point_t E1, E2;
    point_t N1, N2;
    fastf_t d1, d2;
    fastf_t du0, du1, du2, dv0, dv1, dv2;
    fastf_t D[3];
    fastf_t isect1[2] = {0, 0};
    fastf_t isect2[2] = {0, 0};
    point_t isectpointA1 = VINIT_ZERO;
    point_t isectpointA2 = VINIT_ZERO;
    point_t isectpointB1 = VINIT_ZERO;
    point_t isectpointB2 = VINIT_ZERO;
    fastf_t du0du1, du0du2, dv0dv1, dv0dv2;
    short index;
    fastf_t vp0, vp1, vp2;
    fastf_t up0, up1, up2;
    fastf_t b, c, max;
    int smallest1, smallest2;

    /* compute plane equation of triangle(V0, V1, V2) */
    VSUB2(E1, V1, V0);
    VSUB2(E2, V2, V0);
    VCROSS(N1, E1, E2);
    d1=-VDOT(N1, V0);
    /* plane equation 1: N1.X+d1=0 */

    /* put U0, U1, U2 into plane equation 1 to compute signed distances to the plane*/
    du0=VDOT(N1, U0)+d1;
    du1=VDOT(N1, U1)+d1;
    du2=VDOT(N1, U2)+d1;

    /* coplanarity robustness check */
#if USE_EPSILON_TEST
    if (fabs(du0)<EPSILON) du0=0.0;
    if (fabs(du1)<EPSILON) du1=0.0;
    if (fabs(du2)<EPSILON) du2=0.0;
#endif
    du0du1=du0*du1;
    du0du2=du0*du2;

    if (du0du1>0.0f && du0du2>0.0f) /* same sign on all of them + not equal 0 ? */
	return 0;                    /* no intersection occurs */

    /* compute plane of triangle (U0, U1, U2) */
    VSUB2(E1, U1, U0);
    VSUB2(E2, U2, U0);
    VCROSS(N2, E1, E2);
    d2=-VDOT(N2, U0);
    /* plane equation 2: N2.X+d2=0 */

    /* put V0, V1, V2 into plane equation 2 */
    dv0=VDOT(N2, V0)+d2;
    dv1=VDOT(N2, V1)+d2;
    dv2=VDOT(N2, V2)+d2;

#if USE_EPSILON_TEST
    if (fabs(dv0)<EPSILON) dv0=0.0;
    if (fabs(dv1)<EPSILON) dv1=0.0;
    if (fabs(dv2)<EPSILON) dv2=0.0;
#endif

    dv0dv1=dv0*dv1;
    dv0dv2=dv0*dv2;

    if (dv0dv1>0.0f && dv0dv2>0.0f) /* same sign on all of them + not equal 0 ? */
	return 0;                    /* no intersection occurs */

    /* compute direction of intersection line */
    VCROSS(D, N1, N2);

    /* compute and index to the largest component of D */
    max=fabs(D[0]);
    index=0;
    b=fabs(D[1]);
    c=fabs(D[2]);
    if (b>max) max=b, index=1;
    if (c>max) index=2;

    /* this is the simplified projection onto L*/
    vp0=V0[index];
    vp1=V1[index];
    vp2=V2[index];

    up0=U0[index];
    up1=U1[index];
    up2=U2[index];

    /* compute interval for triangle 1 */
    *coplanar=compute_intervals_isectline(V0, V1, V2, vp0, vp1, vp2, dv0, dv1, dv2,
					  dv0dv1, dv0dv2, &isect1[0], &isect1[1], isectpointA1, isectpointA2);
    if (*coplanar) return coplanar_tri_tri(N1, V0, V1, V2, U0, U1, U2);


    /* compute interval for triangle 2 */
    compute_intervals_isectline(U0, U1, U2, up0, up1, up2, du0, du1, du2,
				du0du1, du0du2, &isect2[0], &isect2[1], isectpointB1, isectpointB2);

    SORT2(isect1[0], isect1[1], smallest1);
    SORT2(isect2[0], isect2[1], smallest2);

    if (isect1[1]<isect2[0] || isect2[1]<isect1[0]) return 0;

    /* at this point, we know that the triangles intersect */

    if (isect2[0]<isect1[0]) {
	if (smallest1==0) { VMOVE(*isectpt1, isectpointA1); } else { VMOVE(*isectpt1, isectpointA2); }

	if (isect2[1]<isect1[1]) {
	    if (smallest2==0) { VMOVE(*isectpt2, isectpointB2); } else { VMOVE(*isectpt2, isectpointB1); }
	} else {
	    if (smallest1==0) { VMOVE(*isectpt2, isectpointA2); } else { VMOVE(*isectpt2, isectpointA1); }
	}
    } else {
	if (smallest2==0) { VMOVE(*isectpt1, isectpointB1); } else { VMOVE(*isectpt1, isectpointB2); }

	if (isect2[1]>isect1[1]) {
	    if (smallest1==0) { VMOVE(*isectpt2, isectpointA2); } else { VMOVE(*isectpt2, isectpointA1); }
	} else {
	    if (smallest2==0) { VMOVE(*isectpt2, isectpointB2); } else { VMOVE(*isectpt2, isectpointB1); }
	}
    }
    return 1;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

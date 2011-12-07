/*                 T R I _ I N T E R S E C T . C
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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

/** @file libgcv/tri_intersect.c
 *
 * Intersect 2 triangles using a modified Möller routine.
 */

#include "common.h"

#include <math.h>	/* ceil */
#include <string.h>	/* memcpy */

#include "vmath.h"
#include "bn.h"
#include "soup.h"
#include "tri_intersect.h"


/**********************************************************************/
/* stuff from the moller97 paper */


void
gcv_fisect2(
    point_t VTX0, point_t VTX1, point_t VTX2,
    fastf_t VV0, fastf_t VV1, fastf_t VV2,
    fastf_t D0, fastf_t D1, fastf_t D2,
    fastf_t *isect0, fastf_t *isect1, 
    point_t *isectpoint0, point_t *isectpoint1)
{
    fastf_t tmp=D0/(D0-D1);
    fastf_t diff[3];

    *isect0=VV0+(VV1-VV0)*tmp;
    VSUB2(diff, VTX1, VTX0);
    VSCALE(diff, diff, tmp);
    VADD2(*isectpoint0, diff, VTX0);

    tmp=D0/(D0-D2);
    *isect1=VV0+(VV2-VV0)*tmp;
    VSUB2(diff, VTX2, VTX0);
    VSCALE(diff, diff, tmp);
    VADD2(*isectpoint1, VTX0, diff);
}


int
gcv_compute_intervals_isectline(struct face_s *f,
			    fastf_t VV0, fastf_t VV1, fastf_t VV2, fastf_t D0, fastf_t D1, fastf_t D2,
			    fastf_t D0D1, fastf_t D0D2, fastf_t *isect0, fastf_t *isect1,
			    point_t *isectpoint0, point_t *isectpoint1,
			    const struct bn_tol *tol)
{
    if(D0D1>0.0f)
	/* here we know that D0D2<=0.0 */
	/* that is D0, D1 are on the same side, D2 on the other or on the plane */
	gcv_fisect2(f->vert[2], f->vert[0], f->vert[1], VV2, VV0, VV1, D2, D0, D1, isect0, isect1, isectpoint0, isectpoint1);
    else if(D0D2>0.0f)
	/* here we know that d0d1<=0.0 */
	gcv_fisect2(f->vert[1], f->vert[0], f->vert[2], VV1, VV0, VV2, D1, D0, D2, isect0, isect1, isectpoint0, isectpoint1);
    else if(D1*D2>0.0f || !NEAR_ZERO(D0, tol->dist))
	/* here we know that d0d1<=0.0 or that D0!=0.0 */
	gcv_fisect2(f->vert[0], f->vert[1], f->vert[2], VV0, VV1, VV2, D0, D1, D2, isect0, isect1, isectpoint0, isectpoint1);
    else if(!NEAR_ZERO(D1, tol->dist))
	gcv_fisect2(f->vert[1], f->vert[0], f->vert[2], VV1, VV0, VV2, D1, D0, D2, isect0, isect1, isectpoint0, isectpoint1);
    else if(!NEAR_ZERO(D2, tol->dist))
	gcv_fisect2(f->vert[2], f->vert[0], f->vert[1], VV2, VV0, VV1, D2, D0, D1, isect0, isect1, isectpoint0, isectpoint1);
    else
	/* triangles are coplanar */
	return 1;
    return 0;
}


int
gcv_edge_edge_test(point_t V0, point_t U0, point_t U1, fastf_t Ax, fastf_t Ay, int i0, int i1)
{
    fastf_t Bx, By, Cx, Cy, e, d, f;

    Bx = U0[i0] - U1[i0];
    By = U0[i1] - U1[i1];
    Cx = V0[i0] - U0[i0];
    Cy = V0[i1] - U0[i1];
    f = Ay * Bx - Ax * By;
    d = By * Cx - Bx * Cy;
    if ((f > 0 && d >= 0 && d <= f) || (f < 0 && d <= 0 && d >= f)) {
	e = Ax * Cy - Ay * Cx;
	if (f > 0) {
	    if (e >= 0 && e <= f)
		return 1;
	} else if (e <= 0 && e >= f)
	    return 1;
    }
    return 0;
}


int
gcv_edge_against_tri_edges(point_t V0, point_t V1, point_t U0, point_t U1, point_t U2, int i0, int i1)
{
    fastf_t Ax, Ay;
    Ax=V1[i0]-V0[i0];
    Ay=V1[i1]-V0[i1];
    /* test edge U0, U1 against V0, V1 */
    if(gcv_edge_edge_test(V0, U0, U1, Ax, Ay, i0, i1)) return 1;
    /* test edge U1, U2 against V0, V1 */
    if(gcv_edge_edge_test(V0, U1, U2, Ax, Ay, i0, i1)) return 1;
    /* test edge U2, U1 against V0, V1 */
    if(gcv_edge_edge_test(V0, U2, U0, Ax, Ay, i0, i1)) return 1;
    return 0;
}


int
gcv_point_in_tri(point_t V0, point_t U0, point_t U1, point_t U2, int i0, int i1)
{
    fastf_t a, b, c, d0, d1, d2;
    /* is T1 completly inside T2? */
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
    if(d0*d1>0.0)
	if(d0*d2>0.0) return 1;
    return 0;
}


int
gcv_coplanar_tri_tri(vect_t N, vect_t V0, vect_t V1, vect_t V2, vect_t U0, vect_t U1, vect_t U2)
{
    vect_t A;
    short i0, i1;
    /* first project onto an axis-aligned plane, that maximizes the area */
    /* of the triangles, compute indices: i0, i1. */
    A[0]=fabs(N[0]);
    A[1]=fabs(N[1]);
    A[2]=fabs(N[2]);
    if(A[0]>A[1]) {
	if(A[0]>A[2]) {
	    i0=1;      /* A[0] is greatest */
	    i1=2;
	} else {
	    i0=0;      /* A[2] is greatest */
	    i1=1;
	}
    } else {  /* A[0]<=A[1] */
	if(A[2]>A[1]) {
	    i0=0;      /* A[2] is greatest */
	    i1=1;
	} else {
	    i0=0;      /* A[1] is greatest */
	    i1=2;
	}
    }

    /* test all edges of triangle 1 against the edges of triangle 2 */
    if(gcv_edge_against_tri_edges(V0, V1, U0, U1, U2, i0, i1))return 1;
    if(gcv_edge_against_tri_edges(V1, V2, U0, U1, U2, i0, i1))return 1;
    if(gcv_edge_against_tri_edges(V2, V0, U0, U1, U2, i0, i1))return 1;

    /* finally, test if tri1 is totally contained in tri2 or vice versa */
    if(gcv_point_in_tri(V0, U0, U1, U2, i0, i1))return 1;
    if(gcv_point_in_tri(U0, V0, V1, V2, i0, i1))return 1;

    return 0;
}


int
gcv_tri_tri_intersect_with_isectline(struct soup_s *UNUSED(left), struct soup_s *UNUSED(right), struct face_s *lf, struct face_s *rf, int *coplanar, point_t *isectpt, const struct bn_tol *tol)
{
    vect_t D, isectpointA1={0}, isectpointA2={0}, isectpointB1={0}, isectpointB2={0};
    fastf_t d1, d2, du0, du1, du2, dv0, dv1, dv2, du0du1, du0du2, dv0dv1, dv0dv2, vp0, vp1, vp2, up0, up1, up2, b, c, max, isect1[2]={0, 0}, isect2[2]={0, 0};
    int i, smallest1=0, smallest2=0;

    /* compute plane equation of triangle(lf->vert[0], lf->vert[1], lf->vert[2]) */
    d1=-VDOT(lf->plane, lf->vert[0]);
    /* plane equation 1: lf->plane.X+d1=0 */

    /* put rf->vert[0], rf->vert[1], rf->vert[2] into plane equation 1 to compute signed distances to the plane*/
    du0=VDOT(lf->plane, rf->vert[0])+d1;
    du1=VDOT(lf->plane, rf->vert[1])+d1;
    du2=VDOT(lf->plane, rf->vert[2])+d1;

    du0du1=du0*du1;
    du0du2=du0*du2;

    if(du0du1>0.0f && du0du2>0.0f) /* same sign on all of them + not equal 0 ? */
	return 0;                    /* no intersection occurs */

    /* compute plane of triangle (rf->vert[0], rf->vert[1], rf->vert[2]) */
    d2=-VDOT(rf->plane, rf->vert[0]);
    /* plane equation 2: rf->plane.X+d2=0 */

    /* put lf->vert[0], lf->vert[1], lf->vert[2] into plane equation 2 */
    dv0=VDOT(rf->plane, lf->vert[0])+d2;
    dv1=VDOT(rf->plane, lf->vert[1])+d2;
    dv2=VDOT(rf->plane, lf->vert[2])+d2;

    dv0dv1=dv0*dv1;
    dv0dv2=dv0*dv2;

    if(dv0dv1>0.0f && dv0dv2>0.0f) /* same sign on all of them + not equal 0 ? */
	return 0;                    /* no intersection occurs */

    /* compute direction of intersection line */
    VCROSS(D, lf->plane, rf->plane);

    /* compute and i to the largest component of D */
    max=fabs(D[0]);
    i=0;
    b=fabs(D[1]);
    c=fabs(D[2]);
    if(b>max) max=b, i=1;
    if(c>max) max=c, i=2;

    /* this is the simplified projection onto L*/
    vp0=lf->vert[0][i];
    vp1=lf->vert[1][i];
    vp2=lf->vert[2][i];

    up0=rf->vert[0][i];
    up1=rf->vert[1][i];
    up2=rf->vert[2][i];

    /* compute interval for triangle 1 */
    *coplanar=gcv_compute_intervals_isectline(lf, vp0, vp1, vp2, dv0, dv1, dv2, dv0dv1, dv0dv2, &isect1[0], &isect1[1], &isectpointA1, &isectpointA2, tol);
    if(*coplanar)
	return gcv_coplanar_tri_tri(lf->plane, lf->vert[0], lf->vert[1], lf->vert[2], rf->vert[0], rf->vert[1], rf->vert[2]);

    /* compute interval for triangle 2 */
    gcv_compute_intervals_isectline(rf, up0, up1, up2, du0, du1, du2, du0du1, du0du2, &isect2[0], &isect2[1], &isectpointB1, &isectpointB2, tol);

    /* sort so that a<=b */
    smallest1 = smallest2 = 0;
#define SORT2(a, b, smallest) if(a>b) { fastf_t _c; _c=a; a=b; b=_c; smallest=1; }
    SORT2(isect1[0], isect1[1], smallest1);
    SORT2(isect2[0], isect2[1], smallest2);
#undef SORT2

    if(isect1[1]<isect2[0] || isect2[1]<isect1[0])
	return 0;

    /* at this point, we know that the triangles intersect */

    if (isect2[0] < isect1[0]) {
	if (smallest1 == 0)
	    VMOVE(isectpt[0], isectpointA1)
	else
	    VMOVE(isectpt[0], isectpointA2)
		if (isect2[1] < isect1[1])
		    if (smallest2 == 0)
			VMOVE(isectpt[1], isectpointB2)
		    else
			VMOVE(isectpt[1], isectpointB1)
		else if (smallest1 == 0)
		    VMOVE(isectpt[1], isectpointA2)
		else
		    VMOVE(isectpt[1], isectpointA1)
    } else {
	if (smallest2 == 0)
	    VMOVE(isectpt[0], isectpointB1)
	else
	    VMOVE(isectpt[0], isectpointB2)
		if (isect2[1] > isect1[1])
		    if (smallest1 == 0)
			VMOVE(isectpt[1], isectpointA2)
		    else
			VMOVE(isectpt[1], isectpointA1)
		else if (smallest2 == 0)
		    VMOVE(isectpt[1], isectpointB2)
		else
		    VMOVE(isectpt[1], isectpointB1)
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

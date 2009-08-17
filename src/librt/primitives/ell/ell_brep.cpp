/*                    E L L _ B R E P . C P P
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
/** @file ell_brep.cpp
 *
 * Convert a Generalized Ellipsoid to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"

ON_NurbsSurface *
brep_ell_surf(const matp_t m)
{
    register fastf_t *op;
    ON_NurbsSurface *surf;

    surf = ON_NurbsSurface::New(3, true, 3, 3, 9, 5);

    // u knots
    surf->InsertKnot(0, 0.0, 2); 
    surf->SetKnot(0, 2, 0.25);
    surf->SetKnot(0, 3, 0.25);
    surf->SetKnot(0, 4, 0.5); 
    surf->SetKnot(0, 5, 0.5);
    surf->SetKnot(0, 6, 0.75); 
    surf->SetKnot(0, 7, 0.75);
    surf->SetKnot(0, 8, 1.0);
    surf->SetKnot(0, 9, 1.0); 
    // v knots
    surf->InsertKnot(1,0.0,2);
    surf->SetKnot(1, 2, 0.5); 
    surf->SetKnot(1, 3, 0.5);
    surf->SetKnot(1, 4, 1.0);
    surf->SetKnot(1, 5, 1.0); 

/* Inspired by MAT4X4PNT */
#define M(x, y, z, w)	{ \
	*op++ = m[ 0]*(x) + m[ 1]*(y) + m[ 2]*(z) + m[ 3]*(w);\
	*op++ = m[ 4]*(x) + m[ 5]*(y) + m[ 6]*(z) + m[ 7]*(w);\
	*op++ = m[ 8]*(x) + m[ 9]*(y) + m[10]*(z) + m[11]*(w);\
	*op++ = m[12]*(x) + m[13]*(y) + m[14]*(z) + m[15]*(w); }

    op = surf->m_cv;

    M(   0     ,   0     ,-1.0     , 1.0     );
    M( M_SQRT1_2 ,   0     ,-M_SQRT1_2 , M_SQRT1_2 );
    M( 1.0     ,   0     ,   0     , 1.0     );
    M( M_SQRT1_2 ,   0     , M_SQRT1_2 , M_SQRT1_2 );
    M(   0     ,   0     , 1.0     , 1.0     );

    M(   0     ,   0     ,-M_SQRT1_2 , M_SQRT1_2 );
    M( 0.5     ,-0.5     ,-0.5     , 0.5     );
    M( M_SQRT1_2 ,-M_SQRT1_2 ,   0     , M_SQRT1_2 );
    M( 0.5     ,-0.5     , 0.5     , 0.5     );
    M(   0     ,   0     , M_SQRT1_2 , M_SQRT1_2 );

    M(   0     ,   0     ,-1.0     , 1.0     );
    M(   0     ,-M_SQRT1_2 ,-M_SQRT1_2 , M_SQRT1_2 );
    M(   0     ,-1.0     ,   0     , 1.0     );
    M(   0     ,-M_SQRT1_2 , M_SQRT1_2 , M_SQRT1_2 );
    M(   1     ,   0     , 1.0     , 1.0     );

    M(   0     ,   0     ,-M_SQRT1_2 , M_SQRT1_2 );
    M(-0.5     ,-0.5     ,-0.5     , 0.5     );
    M(-M_SQRT1_2 ,-M_SQRT1_2 ,   0     , M_SQRT1_2 );
    M(-0.5     ,-0.5     , 0.5     , 0.5     );
    M(   0     ,   0     , M_SQRT1_2 , M_SQRT1_2 );

    M(   0     ,   0     ,-1.0     , 1.0     );
    M(-M_SQRT1_2 ,   0     ,-M_SQRT1_2 , M_SQRT1_2 );
    M(-1.0     ,   0     ,   0     , 1.0     );
    M(-M_SQRT1_2 ,   0     , M_SQRT1_2 , M_SQRT1_2 );
    M(   0     ,   0     , 1.0     , 1.0     );

    M(   0     ,   0     ,-M_SQRT1_2 , M_SQRT1_2 );
    M(-0.5     , 0.5     ,-0.5     , 0.5     );
    M(-M_SQRT1_2 , M_SQRT1_2 ,   0     , M_SQRT1_2 );
    M(-0.5     , 0.5     , 0.5     , 0.5     );
    M(   0     ,   0     , M_SQRT1_2 , M_SQRT1_2 );

    M(   0     ,   0     ,-1.0     , 1.0     );
    M(   0     , M_SQRT1_2 ,-M_SQRT1_2 , M_SQRT1_2 );
    M(   0     , 1.0     ,   0     , 1.0     );
    M(   0     , M_SQRT1_2 , M_SQRT1_2 , M_SQRT1_2 );
    M(   0     ,   0     , 1.0     , 1.0     );

    M(   0     ,   0     ,-M_SQRT1_2 , M_SQRT1_2 );
    M( 0.5     , 0.5     ,-0.5     , 0.5     );
    M( M_SQRT1_2 , M_SQRT1_2 ,   0     , M_SQRT1_2 );
    M( 0.5     , 0.5     , 0.5     , 0.5     );
    M(   0     ,   0     , M_SQRT1_2 , M_SQRT1_2 );

    M(   0     ,   0     ,-1.0     , 1.0     );
    M( M_SQRT1_2 ,   0     ,-M_SQRT1_2 , M_SQRT1_2 );
    M( 1.0     ,   0     ,   0     , 1.0     );
    M( M_SQRT1_2 ,   0     , M_SQRT1_2 , M_SQRT1_2 );
    M(   0     ,   0     , 1.0     , 1.0     );

    return surf;
}

/**
 *			R T _ E L L _ B R E P
 */
extern "C" void
rt_ell_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    mat_t	R;
    mat_t	S;
    mat_t	invR;
    mat_t	invS;
    mat_t		invRinvS;
    mat_t		invRoS;
    mat_t	unit2model;
    mat_t	xlate;
    vect_t	Au, Bu, Cu;	/* A, B, C with unit length */
    fastf_t	Alen, Blen, Clen;
    fastf_t	invAlen, invBlen, invClen;
    fastf_t	magsq_a, magsq_b, magsq_c;
    fastf_t	f;
    fastf_t		radius;
    struct rt_ell_internal	*eip;

    *b = NULL; 

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_ell_internal *)ip->idb_ptr;
    RT_ELL_CK_MAGIC(eip);

    /* Validate that |A| > 0, |B| > 0, |C| > 0 */
    magsq_a = MAGSQ( eip->a );
    magsq_b = MAGSQ( eip->b );
    magsq_c = MAGSQ( eip->c );
    if ( magsq_a < tol->dist || magsq_b < tol->dist || magsq_c < tol->dist ) {
	bu_log("rt_ell_tess():  zero length A, B, or C vector\n");
	return;
    }

    /* Create unit length versions of A, B, C */
    invAlen = 1.0/(Alen = sqrt(magsq_a));
    VSCALE( Au, eip->a, invAlen );
    invBlen = 1.0/(Blen = sqrt(magsq_b));
    VSCALE( Bu, eip->b, invBlen );
    invClen = 1.0/(Clen = sqrt(magsq_c));
    VSCALE( Cu, eip->c, invClen );

    /* Validate that A.B == 0, B.C == 0, A.C == 0 (check dir only) */
    f = VDOT( Au, Bu );
    if ( ! NEAR_ZERO(f, tol->dist) )  {
	bu_log("ell():  A not perpendicular to B, f=%f\n", f);
	return;
    }
    f = VDOT( Bu, Cu );
    if ( ! NEAR_ZERO(f, tol->dist) )  {
	bu_log("ell():  B not perpendicular to C, f=%f\n", f);
	return;
    }
    f = VDOT( Au, Cu );
    if ( ! NEAR_ZERO(f, tol->dist) )  {
	bu_log("ell():  A not perpendicular to C, f=%f\n", f);
	return;
    }

    {
	vect_t	axb;
	VCROSS( axb, Au, Bu );
	f = VDOT( axb, Cu );
	if ( f < 0 )  {
	    VREVERSE( Cu, Cu );
	    VREVERSE( eip->c, eip->c );
	}
    }

    /* Compute R and Rinv matrices */
    MAT_IDN( R );
    VMOVE( &R[0], Au );
    VMOVE( &R[4], Bu );
    VMOVE( &R[8], Cu );
    bn_mat_trn( invR, R ); /* inv of rot mat is trn */

    /* Compute S and invS matrices */
    /* invS is just 1/diagonal elements */
    MAT_IDN( S );
    S[ 0] = invAlen;
    S[ 5] = invBlen;
    S[10] = invClen;
    bn_mat_inv( invS, S );

    /* invRinvS, for converting points from unit sphere to model */
    bn_mat_mul( invRinvS, invR, invS );

    /* invRoS, for converting normals from unit sphere to model */
    bn_mat_mul( invRoS, invR, S );

    /* Compute radius of bounding sphere */
    radius = Alen;
    if ( Blen > radius )
	radius = Blen;
    if ( Clen > radius )
	radius = Clen;

    MAT_IDN( xlate );
    MAT_DELTAS_VEC( xlate, eip->v );
    bn_mat_mul( unit2model, xlate, invRinvS );

    /* Create brep with one face*/
    *b = ON_Brep::New();
    (*b)->NewFace(*brep_ell_surf(unit2model));
//    (*b)->Standardize();
 //   (*b)->Compact();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


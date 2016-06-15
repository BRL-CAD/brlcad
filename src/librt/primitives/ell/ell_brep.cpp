/*                    E L L _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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

extern "C" void
rt_ell_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct rt_ell_internal *eip;

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_ell_internal *)ip->idb_ptr;
    RT_ELL_CK_MAGIC(eip);

    // For Algorithms used in rotation and translation
    // Please refer to ell.c
    // X' = S(R(X - V))
    // X = invR(invS(X')) + V = invRinvS(X') + V;
    // where R(X) = (A/(|A|))
    //              (B/(|B|)) . X
    //              (C/(|C|))
    //
    // and S(X) = (1/|A|   0     0 )
    //            (0     1/|B|   0 ) . X
    //            (0      0   1/|C|)

    // Parameters for Rotate and Translate
    vect_t Au, Bu, Cu;    /* A, B, C with unit length */
    mat_t R;
    mat_t Rinv;
    mat_t Sinv;
    mat_t invRinvS;
    fastf_t magsq_a, magsq_b, magsq_c;
    fastf_t f;

    magsq_a = MAGSQ(eip->a);
    magsq_b = MAGSQ(eip->b);
    magsq_c = MAGSQ(eip->c);

    if (magsq_a < tol->dist_sq || magsq_b < tol->dist_sq || magsq_c < tol->dist_sq) {
    bu_log("rt_ell_brep():  ell zero length A(%g), B(%g), or C(%g) vector\n",
	   magsq_a, magsq_b, magsq_c);
    }

    f = 1.0/sqrt(magsq_a);
    VSCALE(Au, eip->a, f);
    f = 1.0/sqrt(magsq_b);
    VSCALE(Bu, eip->b, f);
    f = 1.0/sqrt(magsq_c);
    VSCALE(Cu, eip->c, f);

    MAT_IDN(R);
    VMOVE(&R[0], Au);
    VMOVE(&R[4], Bu);
    VMOVE(&R[8], Cu);
    bn_mat_trn(Rinv, R);

    MAT_IDN(Sinv);
    Sinv[0] = MAGNITUDE(eip->a);
    Sinv[5] = MAGNITUDE(eip->b);
    Sinv[10] = MAGNITUDE(eip->c);
    bn_mat_mul(invRinvS, Rinv, Sinv);

    point_t origin;
    VSET(origin, 0, 0, 0);
    ON_Sphere sph(origin, 1);

    // Get the NURBS form of the surface
    ON_NurbsSurface *ellcurvedsurf = ON_NurbsSurface::New();
    sph.GetNurbForm(*ellcurvedsurf);

    // Scale, rotate and translate control points
    for (int i = 0; i < ellcurvedsurf->CVCount(0); i++) {
	for (int j = 0; j < ellcurvedsurf->CVCount(1); j++) {
	    point_t cvpt;
	    ON_4dPoint ctrlpt;
	    ellcurvedsurf->GetCV(i, j, ctrlpt);
	    MAT3X3VEC(cvpt, invRinvS, ctrlpt);
	    point_t scale_v;
	    VSCALE(scale_v, eip->v, ctrlpt.w);
	    VADD2(cvpt, scale_v, cvpt);
	    ON_4dPoint newpt = ON_4dPoint(cvpt[0], cvpt[1], cvpt[2], ctrlpt.w);
	    ellcurvedsurf->SetCV(i, j, newpt);
	}
    }

    ellcurvedsurf->SetDomain(0, 0.0, 1.0);
    ellcurvedsurf->SetDomain(1, 0.0, 1.0);

    // Make final BREP structure
    (*b)->m_S.Append(ellcurvedsurf);
    int surfindex = (*b)->m_S.Count();
    (*b)->NewFace(surfindex - 1);
    int faceindex = (*b)->m_F.Count();
    (*b)->NewOuterLoop(faceindex-1);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

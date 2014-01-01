/*                S U P E R E L L _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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
/** @file superell_brep.cpp
 *
 * Convert a Superquadratic Ellipsoid to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"

extern "C" {
    void rt_ell_brep(ON_Brep **b, struct rt_db_internal *ip, const struct bn_tol *tol);
}

/**
 * R T _ S U P E R E L L _ B R E P
 */
extern "C" void
rt_superell_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct rt_superell_internal *sip;

    RT_CK_DB_INTERNAL(ip);
    sip = (struct rt_superell_internal *)ip->idb_ptr;
    RT_SUPERELL_CK_MAGIC(sip);

    // First, create brep of an ellipsoid
    struct rt_ell_internal *eip;
    BU_ALLOC(eip, struct rt_ell_internal);
    eip->magic = RT_ELL_INTERNAL_MAGIC;
    VMOVE(eip->v, sip->v);
    VMOVE(eip->a, sip->a);
    VMOVE(eip->b, sip->b);
    VMOVE(eip->c, sip->c);

    struct rt_db_internal tmp_internal;
    RT_DB_INTERNAL_INIT(&tmp_internal);
    tmp_internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    tmp_internal.idb_ptr = (genptr_t)eip;
    tmp_internal.idb_minor_type = ID_ELL;
    tmp_internal.idb_meth = &OBJ[ID_ELL];

    ON_Brep *tmp_brep = ON_Brep::New();
    rt_ell_brep(&tmp_brep, &tmp_internal, tol);

    ON_NurbsSurface *surf = ON_NurbsSurface::New();
    tmp_brep->m_S[0]->GetNurbForm(*surf);

    // Calculate the weight value
    // See: Joe F. Thompson, B. K. Soni, N. P. Weatherill. Handbook of Grid Generation.
    // Ch.30.3.5 Superellipse to NURBS Curve.
    const fastf_t cos45 = cos(ON_PI/4.0);
    fastf_t weight_e = (pow(cos45, sip->e) - 0.5) / (1 - pow(cos45, sip->e));
    fastf_t weight_n = (pow(cos45, sip->n) - 0.5) / (1 - pow(cos45, sip->n));
    // When the center (origin) is used as a control point
    if (weight_e < 0) {
	weight_e = (pow(cos45, sip->e) - 0.5) / pow(cos45, sip->e);
    }
    if (weight_n < 0) {
	weight_n = (pow(cos45, sip->n) - 0.5) / pow(cos45, sip->n);
    }

    // Change the weight of the control points. rt_ell_brep() creates a NURBS surface with 9*5
    // control points.
    // When both e and n <= 2, this can generate an ideal b-rep for superell, otherwise cannot.
    if (sip->e <= 2 && sip->n <= 2) {
	for (int i = 0; i < surf->CVCount(0); i++) {
	    fastf_t tmp_weight = 0.0;
	    switch (i) {
	    case 0:
	    case 4:
	    case 8: // the XZ plane
	    case 2:
	    case 6: // the YZ plane
		tmp_weight = 1.0;
		break;
	    case 1:
	    case 3:
	    case 5:
	    case 7:
		tmp_weight = weight_e;
		break;
	    default:
		bu_log("Should not reach here!\n");
	    }
	    for (int j = 0; j < surf->CVCount(1); j++) {
		fastf_t new_weight = 0.0;
		switch (j) {
		case 0:
		case 2:
		case 4:
		    new_weight = tmp_weight;
		    break;
		case 1:
		case 3:
		    new_weight = tmp_weight * weight_n;
		    break;
		default:
		    bu_log("Should not reach here!\n");
		}
		ON_3dPoint ctrlpt;
		surf->GetCV(i, j, ctrlpt);
		ON_4dPoint newctrlpt(ctrlpt[0]*new_weight, ctrlpt[1]*new_weight, ctrlpt[2]*new_weight, new_weight);
		surf->SetCV(i, j, newctrlpt);
	    }
	}

	surf->SetDomain(0, 0.0, 1.0);
	surf->SetDomain(1, 0.0, 1.0);

	// Make final BREP structure
	(*b)->m_S.Append(surf);
	int surfindex = (*b)->m_S.Count();
	(*b)->NewFace(surfindex - 1);
	int faceindex = (*b)->m_F.Count();
	(*b)->NewOuterLoop(faceindex-1);
    } else {
	// When one or more than one of e and n are greater than 2, generate 8 surfaces
	// independently to represent the superell

	// First, we create a b-rep superell centered at the origin, with radius 1.
	ON_NurbsSurface *surface[8];

	surface[0] = ON_NurbsSurface::New(3, true, 3, 3, 3, 3);
	surface[0]->SetKnot(0, 0, 0);
	surface[0]->SetKnot(0, 1, 0);
	surface[0]->SetKnot(0, 2, 1);
	surface[0]->SetKnot(0, 3, 1);
	surface[0]->SetKnot(1, 0, 0);
	surface[0]->SetKnot(1, 1, 0);
	surface[0]->SetKnot(1, 2, 1);
	surface[0]->SetKnot(1, 3, 1);

	// Surface 1
	surface[0]->SetCV(0, 0, ON_4dPoint(0, 0, -1, 1));
	surface[0]->SetCV(0, 2, ON_4dPoint(1, 0, 0, 1));
	if (weight_e > 0) {
	    surface[0]->SetCV(1, 0, ON_4dPoint(0, 0, -weight_e, weight_e));
	    surface[0]->SetCV(1, 2, ON_4dPoint(weight_e, weight_e, 0, weight_e));
	} else {
	    surface[0]->SetCV(1, 0, ON_4dPoint(0, 0, weight_e, -weight_e));
	    surface[0]->SetCV(1, 2, ON_4dPoint(0, 0, 0, -weight_e));
	}
	surface[0]->SetCV(2, 0, ON_4dPoint(0, 0, -1, 1));
	surface[0]->SetCV(2, 2, ON_4dPoint(0, 1, 0, 1));

	if (weight_n >= 0) {
	    surface[0]->SetCV(0, 1, ON_4dPoint(weight_n, 0, -weight_n, weight_n));
	    surface[0]->SetCV(1, 1, ON_4dPoint(0, 0, -fabs(weight_n*weight_e), fabs(weight_n*weight_e)));
	    surface[0]->SetCV(2, 1, ON_4dPoint(0, weight_n, -weight_n, weight_n));
	} else {
	    // use the center as a control point
	    surface[0]->SetCV(0, 1, ON_4dPoint(0, 0, 0, -weight_n));
	    if (weight_e > 0) {
		surface[0]->SetCV(1, 1, ON_4dPoint(0, 0, 0, -weight_n*weight_e));
	    } else {
		surface[0]->SetCV(1, 1, ON_4dPoint(0, 0, 0, -weight_n));
	    }
	    surface[0]->SetCV(2, 1, ON_4dPoint(0, 0, 0, -weight_n));
	}

	surface[0]->SetDomain(0, 0.0, 1.0);
	surface[0]->SetDomain(1, 0.0, 1.0);

	ON_3dPoint axis_a(0, 0, -1);
	ON_3dPoint axis_b(0, 0, 0);
	ON_3dVector axis;
	axis = axis_a - axis_b;

	// Surface 2
	surface[1] = surface[0]->Duplicate();
	surface[1]->Rotate(ON_PI/2.0, axis, axis_b);

	// Surface 3
	surface[2] = surface[0]->Duplicate();
	surface[2]->Rotate(ON_PI, axis, axis_b);

	// Surface 4
	surface[3] = surface[0]->Duplicate();
	surface[3]->Rotate(ON_PI*3.0/2.0, axis, axis_b);

	axis_a = ON_3dPoint(0, 1, 0);
	ON_3dVector axis2;
	axis2 = axis_a - axis_b;

	// Surface 5
	surface[4] = surface[0]->Duplicate();
	surface[4]->Rotate(ON_PI, axis2, axis_b);

	// Surface 6
	surface[5] = surface[4]->Duplicate();
	surface[5]->Rotate(ON_PI/2.0, axis, axis_b);

	// Surface 7
	surface[6] = surface[4]->Duplicate();
	surface[6]->Rotate(ON_PI, axis, axis_b);

	// Surface 8
	surface[7] = surface[4]->Duplicate();
	surface[7]->Rotate(ON_PI*3.0/2.0, axis, axis_b);

	// Do the transformations and add elements to the b-rep object
	for (int i = 0; i < 8; i++) {
	    mat_t R, invR, invS, invRinvS;
	    MAT_IDN(R);
	    VMOVE(&R[0], sip->a);
	    VUNITIZE(&R[0]);
	    VMOVE(&R[4], sip->b);
	    VUNITIZE(&R[4]);
	    VMOVE(&R[8], sip->c);
	    VUNITIZE(&R[8]);
	    bn_mat_trn(invR, R);

	    MAT_IDN(invS);
	    invS[0] = MAGNITUDE(sip->a);
	    invS[5] = MAGNITUDE(sip->b);
	    invS[10] = MAGNITUDE(sip->c);
	    bn_mat_mul(invRinvS, invR, invS);

	    for (int j = 0; j < surface[j]->CVCount(0); j++) {
		for (int k = 0; k < surface[k]->CVCount(1); k++) {
		    point_t cvpt;
		    ON_4dPoint ctrlpt;
		    surface[i]->GetCV(j, k, ctrlpt);
		    MAT3X3VEC(cvpt, invRinvS, ctrlpt);
		    point_t scale_v;
		    VSCALE(scale_v, eip->v, ctrlpt.w);
		    VADD2(cvpt, scale_v, cvpt);
		    ON_4dPoint newpt = ON_4dPoint(cvpt[0], cvpt[1], cvpt[2], ctrlpt.w);
		    surface[i]->SetCV(j, k, newpt);
		}
	    }

	    (*b)->m_S.Append(surface[i]);
	    int surfindex = (*b)->m_S.Count();
	    (*b)->NewFace(surfindex - 1);
	    int faceindex = (*b)->m_F.Count();
	    (*b)->NewOuterLoop(faceindex-1);
	}
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

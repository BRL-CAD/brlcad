/*                    E L L _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2012 United States Government as represented by
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
    BU_GET(eip, struct rt_ell_internal);
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
    tmp_internal.idb_meth = &rt_functab[ID_ELL];

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

    // Change the weight of the control points. rt_ell_brep() creates a NURBS surface with 9*5
    // control points.
    for (int i = 0; i < surf->CVCount(0); i++) {
	fastf_t tmp_weight;
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
	    fastf_t new_weight;
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
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

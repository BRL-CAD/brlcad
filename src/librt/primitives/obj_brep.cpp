/*                    O B J _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include "raytrace.h"
#include "brep.h"

#include "../librt_private.h"


static int
brep_make_affine_safe(ON_Brep *brep)
{
    int i;

    if (!brep)
	return -1;

    /* Analytic OpenNURBS surfaces and curves generally preserve only
     * similarities.  Convert them to their exact rational NURBS forms before
     * applying an arbitrary affine transform.  Surface parameterizations are
     * preserved, so the existing trim curves remain valid. */
    for (i = 0; i < brep->m_S.Count(); i++) {
	ON_Surface *old_surface = brep->m_S[i];
	if (!old_surface || dynamic_cast<ON_NurbsSurface *>(old_surface))
	    continue;

	ON_NurbsSurface *nurbs = ON_NurbsSurface::New();
	if (old_surface->GetNurbForm(*nurbs, 0.0) <= 0) {
	    delete nurbs;
	    return -1;
	}
	brep->m_S[i] = nurbs;
	for (int fi = 0; fi < brep->m_F.Count(); fi++) {
	    if (brep->m_F[fi].m_si == i)
		brep->m_F[fi].SetProxySurface(nurbs);
	}
	delete old_surface;
    }

    for (i = 0; i < brep->m_C3.Count(); i++) {
	ON_Curve *old_curve = brep->m_C3[i];
	if (!old_curve || dynamic_cast<ON_NurbsCurve *>(old_curve))
	    continue;

	ON_NurbsCurve *nurbs = ON_NurbsCurve::New();
	if (old_curve->GetNurbForm(*nurbs, 0.0) <= 0) {
	    delete nurbs;
	    return -1;
	}
	brep->m_C3[i] = nurbs;
	for (int ei = 0; ei < brep->m_E.Count(); ei++) {
	    if (brep->m_E[ei].m_c3i == i)
		brep->m_E[ei].ChangeEdgeCurve(i);
	}
	delete old_curve;
    }

    return 0;
}


extern "C" int
rt_obj_brep(ON_Brep **b, struct rt_db_internal *ip, const struct bn_tol *tol)
{
    mat_t nonuniform_mat;
    struct bn_tol body_tol;
    const struct bn_tol *brep_tol = tol;
    int have_nonuniform;
    int id;

    if (!b || !ip)
	return -1;
    RT_CK_DB_INTERNAL(ip);
    if (tol) BN_CK_TOL(tol);

    id = ip->idb_minor_type;
    if (id < 0 || !OBJ[id].ft_brep)
	return -2;

    have_nonuniform = _rt_nonuniform_attr_get(nonuniform_mat, ip);
    if (have_nonuniform < 0)
	return -3;
    if (have_nonuniform > 0 && tol) {
	_rt_nonuniform_tolerances(NULL, &body_tol, NULL, tol, nonuniform_mat);
	brep_tol = &body_tol;
    }

    OBJ[id].ft_brep(b, ip, brep_tol);
    if (!*b)
	return -4;

    if (have_nonuniform > 0) {
	if (brep_make_affine_safe(*b) < 0)
	    return -5;
	ON_Xform transform(nonuniform_mat);
	if (!(*b)->Transform(transform))
	    return -6;
    }

    return 0;
}

/*                   N U R B _ B R E P . C P P
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
/** @file nurb_poly.c
 *
 * Convert old NURBS to new NURBS
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "nurb.h"
#include "brep.h"


extern "C" void
rt_nurb_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *)
{
    int i, j, k;
    struct rt_nurb_internal *nip;

    RT_CK_DB_INTERNAL(ip);
    nip = (struct rt_nurb_internal *)ip->idb_ptr;
    RT_NURB_CK_MAGIC(nip);

    ON_TextLog log(stderr);
    *b = ON_Brep::New();

    for (i = 0; i < nip->nsrf; i++) {
	struct face_g_snurb *surface = nip->srfs[i];
	NMG_CK_SNURB(surface);

	ON_NurbsSurface *nurb = ON_NurbsSurface::New(3, true, surface->order[0], surface->order[1], surface->s_size[0], surface->s_size[1]);

	/* set 'u' knots */
	/* skip first and last (duplicates?) */
	for (j = 1; j < surface->u.k_size - 1; j++) {
	    nurb->SetKnot(0, j-1, surface->u.knots[j]);
	    /* bu_log("u knot %d is %f\n", j-1, surface->u.knots[j]); */
	}
	/* set 'v' knots */
	/* skip first and last (duplicates?) */
	for (j = 1; j < surface->v.k_size - 1; j++) {
	    nurb->SetKnot(1, j-1, surface->v.knots[j]);
	    /* bu_log("v knot %d is %f\n", j-1, surface->u.knots[j]); */
	}

	/* set control points */
	for (j = 0; j < surface->s_size[0]; j++) {
	    for (k = 0; k < surface->s_size[1]; k++) {
		ON_3dPoint point = &RT_NURB_GET_CONTROL_POINT(surface, j, k);
		nurb->SetCV(k, j, point);
	    }
	}

	/* nurb->Dump(log); */
	bu_log("NURBS surface %d %s valid\n", i, nurb->IsValid(&log) ? "is" : "is not");

	(*b)->m_S.Append(nurb);
	int sindex = (*b)->m_S.Count();
	(*b)->NewFace(sindex - 1);
	int findex = (*b)->m_F.Count();
	(*b)->NewOuterLoop(findex - 1);
    }

    bu_log("BREP object %s a single surface\n", (*b)->IsSurface() ? "is" : "is not");
    bu_log("BREP object %s valid\n", (*b)->IsValid(&log) ? "is" : "is not");
    bu_log("BREP object %s valid topology\n", (*b)->IsValidTopology(&log) ? "is" : "is not");
    bu_log("BREP object %s valid geometry\n", (*b)->IsValidGeometry(&log) ? "is" : "is not");
    bu_log("BREP object %s solid\n", (*b)->IsSolid() ? "is" : "is not");
    bu_log("BREP object %s manifold\n", (*b)->IsManifold() ? "is" : "is not");
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

/*                   N U R B _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2009 United States Government as represented by
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
rt_nurb_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    int i, j, k;
    struct rt_nurb_internal *nip;
    point_t min_pt, max_pt;
    point_t center;

    RT_CK_DB_INTERNAL(ip);
    nip = (struct rt_nurb_internal *)ip->idb_ptr;
    RT_NURB_CK_MAGIC(nip);

#if 0    
    for (i = 0; i < nip->nsrf; i++) {
	point_t min_bound, max_bound;
	NMG_CK_SNURB(nip->srfs[i]);
	rt_nurb_s_bound(nip->srfs[i], min_bound, max_bound);

	VMINMAX(min_pt, max_pt, min_bound);
	VMINMAX(min_pt, max_pt, max_bound);
    }

    VADD2SCALE(center, max_pt, min_pt, 0.5);

    /* create a bounding sphere for now */
    // bu_log("DEBUG: min_pt is (%f,%f,%f); max_pt is (%f,%f,%f); radius is %f\n", V3ARGS(min_pt), V3ARGS(max_pt), DIST_PT_PT(min_pt, max_pt) * 0.5);

    ON_Sphere sph(center, DIST_PT_PT(min_pt, max_pt) * 0.5);
    *b = ON_BrepSphere(sph);

#else

    *b = ON_Brep::New();

    for (i = 0; i < nip->nsrf; i++) {
	struct face_g_snurb *surface = nip->srfs[i];
	NMG_CK_SNURB(surface);

	ON_NurbsSurface *nurb = ON_NurbsSurface::New(3, true, surface->order[0], surface->order[1], surface->s_size[0], surface->s_size[1]);

	/* set 'u' knots */
	for (j = 0; j < surface->u.k_size; j++) {
	    nurb->SetKnot(0, j, surface->u.knots[j]);
	}
	/* set 'v' knots */
	for (j = 0; j < surface->v.k_size; j++) {
	    nurb->SetKnot(1, j, surface->v.knots[j]);
	}

	/* set control points */
	for (j = 0; j < surface->s_size[0]; j++) {
	    for (k = 0; k < surface->s_size[1]; k++) {
		ON_3dPoint point = &RT_NURB_GET_CONTROL_POINT(surface, j, k);
		nurb->SetCV(k, j, point);
	    }
	}

	ON_TextLog log(stderr);
	nurb->Dump(log);
	bu_log("NURBS surface %s valid", nurb->IsValid(&log) ? "is" : "is not");

	(*b)->m_S.Append(nurb);
	int sindex = (*b)->m_S.Count();
	ON_BrepFace& face = (*b)->NewFace(sindex - 1);
	int findex = (*b)->m_F.Count();
	ON_BrepLoop* loop = (*b)->NewOuterLoop(findex - 1);
    }

#endif
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

/*                B S P L I N E _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2009-2026 United States Government as represented by
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
/** @file bspline_brep.cpp
 *
 * Convert old NURBS to new NURBS
 *
 */

#include "common.h"

#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"


extern "C" void
rt_nurb_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *)
{
    int i, j, k;
    struct rt_nurb_internal *nip;

    RT_CK_DB_INTERNAL(ip);
    nip = (struct rt_nurb_internal *)ip->idb_ptr;
    RT_NURB_CK_MAGIC(nip);

    for (i = 0; i < nip->nsrf; i++) {
	struct face_g_snurb *surface = nip->srfs[i];
	NMG_CK_SNURB(surface);

	int is_rational = RT_NURB_IS_PT_RATIONAL(surface->pt_type);
	int ncoords = RT_NURB_EXTRACT_COORDS(surface->pt_type);

	ON_NurbsSurface *nurb = ON_NurbsSurface::New(3, is_rational ? true : false,
						     surface->order[0], surface->order[1],
						     surface->s_size[0], surface->s_size[1]);

	/*
	 * openNURBS uses n+p-2 knots (omitting the redundant first and last
	 * endpoint of the BRL-CAD clamped knot vector, which stores n+p knots).
	 * Skip index 0 and index k_size-1.
	 */
	for (j = 1; j < surface->u.k_size - 1; j++)
	    nurb->SetKnot(0, j-1, surface->u.knots[j]);
	for (j = 1; j < surface->v.k_size - 1; j++)
	    nurb->SetKnot(1, j-1, surface->v.knots[j]);

	/*
	 * Set control points.  s_size[0] is the u-direction count and
	 * s_size[1] is the v-direction count.  ON_NurbsSurface::SetCV(i, j)
	 * uses i for u and j for v, matching our outer/inner loop variables.
	 * For rational surfaces the BRL-CAD snurb stores homogeneous coords
	 * (X*w, Y*w, Z*w, w), which is the same convention as ON_4dPoint.
	 */
	for (j = 0; j < surface->s_size[0]; j++) {
	    for (k = 0; k < surface->s_size[1]; k++) {
		fastf_t *cv = &RT_NURB_GET_CONTROL_POINT(surface, j, k);
		if (is_rational) {
		    nurb->SetCV(j, k, ON_4dPoint(cv[0], cv[1], cv[2],
						 (ncoords >= 4) ? cv[3] : 1.0));
		} else {
		    nurb->SetCV(j, k, ON_3dPoint(cv[0], cv[1], cv[2]));
		}
	    }
	}

	(*b)->m_S.Append(nurb);
	int sindex = (*b)->m_S.Count();
	int findex_before = (*b)->m_F.Count();
	(*b)->NewFace(sindex - 1);
	int findex_after = (*b)->m_F.Count();
	if (findex_after == findex_before) {
	    bu_log("rt_nurb_brep: failed to create face for surface %d, skipping\n", i);
	    continue;
	}
	(*b)->NewOuterLoop(findex_after - 1);
    }

    if (!(*b)->IsValid(NULL)) {
	ON_TextLog log(stderr);
	bu_log("rt_nurb_brep: resulting BREP is not valid:\n");
	(*b)->IsValid(&log);
    }
}


extern "C" int
rt_nurb_to_brep(struct rt_db_internal *ip)
{
    struct rt_brep_internal *bi;
    ON_Brep *brep;
    struct bn_tol tol;

    RT_CK_DB_INTERNAL(ip);

    if (ip->idb_type != ID_BSPLINE) {
	bu_log("rt_nurb_to_brep: called with non-bspline internal (type %d)\n",
	       ip->idb_type);
	return -1;
    }

    tol.magic = BN_TOL_MAGIC;
    tol.dist = BN_TOL_DIST;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = SMALL_FASTF;
    tol.para = 1.0 - tol.perp;

    brep = ON_Brep::New();
    rt_nurb_brep(&brep, ip, &tol);

    if (!brep->IsValid(NULL)) {
	ON_TextLog log(stderr);
	bu_log("rt_nurb_to_brep: converted BREP is not valid for '%s':\n",
	       ip->idb_meth->ft_label);
	brep->IsValid(&log);
	delete brep;
	return -1;
    }

    /* free the old BSPLINE internal in-place */
    ip->idb_meth->ft_ifree(ip);

    BU_ALLOC(bi, struct rt_brep_internal);
    bi->magic = RT_BREP_INTERNAL_MAGIC;
    bi->brep = brep;

    ip->idb_ptr = (void *)bi;
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_BREP;
    ip->idb_meth = &OBJ[ID_BREP];

    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

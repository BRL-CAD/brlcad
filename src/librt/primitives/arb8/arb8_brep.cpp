/*                    A R B 8 _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file arb8_brep.cpp
 *
 * Convert an Arbitrary Regular Polyhedron with as many as 8 vertices
 * to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/arb8.h"
#include "nmg.h"
#include "brep.h"

extern "C" {
    extern int rt_arb_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol);
    extern void rt_nmg_brep(ON_Brep **bi, const struct rt_db_internal *ip, const struct bn_tol *tol);
}


extern "C" void
rt_arb_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    const struct rt_arb_internal *arb =
	(const struct rt_arb_internal *)ip->idb_ptr;
    struct rt_arb_internal repaired;
    int validation_issues = 0;
    (void)rt_arb_validate(NULL, arb, tol, &validation_issues);
    if (validation_issues & RT_ARB_VALIDATE_NONSTANDARD) {
	memset(&repaired, 0, sizeof(repaired));
	if (rt_arb_repair(&repaired, arb, tol, 0) < 0)
	    return;
	arb = &repaired;
	(void)rt_arb_validate(NULL, arb, tol, &validation_issues);
    }
    if (validation_issues & ~RT_ARB_VALIDATE_CONCAVE)
	return;

    struct rt_db_internal *tmp_internal;
    struct bg_tess_tol ttmptol = BG_TESS_TOL_INIT_TOL;

    BU_ALLOC(tmp_internal, struct rt_db_internal);
    RT_DB_INTERNAL_INIT(tmp_internal);

    const struct bg_tess_tol *ttol = &ttmptol;
    struct model *arbm = nmg_mm();
    struct nmgregion *arbr = NULL;

    tmp_internal->idb_ptr = (void *)arb;
    const int tess_result = rt_arb_tess(&arbr, arbm, tmp_internal, ttol, tol);
    if (tess_result == 0 && arbr != NULL) {
	tmp_internal->idb_ptr = (void *)arbm;
	rt_nmg_brep(b, tmp_internal, tol);
    }

    FREE_MODEL(arbm);
    bu_free(tmp_internal, "free temporary rt_db_internal");
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

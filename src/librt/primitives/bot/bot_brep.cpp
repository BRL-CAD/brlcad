/*                    B O T _ B R E P . C P P
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
/** @file bot_brep.cpp
 *
 * Convert a BOT primitive to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rt/geom.h"
#include "nmg.h"
#include "brep.h"

extern "C" {
    extern int rt_bot_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol);
    extern void rt_nmg_brep(ON_Brep **bi, const struct rt_db_internal *ip, const struct bn_tol *tol);
}


extern "C" void
rt_bot_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    if (b == NULL || *b == NULL || ip == NULL || tol == NULL)
	return;

    RT_CK_DB_INTERNAL(ip);
    const struct rt_bot_internal *bot =
	(const struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    /* Surface BOTs have no volume, and plate BOTs describe per-triangle
     * thickness semantics that a bare copy of their triangle faces does not
     * preserve.  In particular, sending a large surface BOT through
     * rt_bot_tess would manufacture a two-sided NMG and spend unbounded work
     * on something that cannot be a semantically equivalent solid B-Rep. */
    if (bot->mode != RT_BOT_SOLID) {
	(*b)->Destroy();
	return;
    }

    struct rt_db_internal *tmp_internal;
    struct bg_tess_tol ttmptol = BG_TESS_TOL_INIT_TOL;
    struct bn_tol topology_tol = *tol;

    /* A solid BoT already supplies explicit face topology.  The general NMG
     * tessellator's modeling-tolerance collinearity test is appropriate when
     * approximating analytic geometry, but applying a coarse distance
     * tolerance here can silently discard nonzero mesh triangles and open an
     * otherwise closed shell.  Keep its numerical floor while respecting any
     * tighter tolerance requested by the caller. */
    if (topology_tol.dist <= 0.0 || topology_tol.dist > RT_LEN_TOL)
	topology_tol.dist = RT_LEN_TOL;
    topology_tol.dist_sq = topology_tol.dist * topology_tol.dist;

    BU_ALLOC(tmp_internal, struct rt_db_internal);
    RT_DB_INTERNAL_INIT(tmp_internal);

    const struct bg_tess_tol *ttol = &ttmptol;
    struct model *botm = nmg_mm();
    struct nmgregion *botr = NULL;

    tmp_internal->idb_ptr = (void *)ip->idb_ptr;
    const int tess_result = rt_bot_tess(&botr, botm, tmp_internal, ttol,
	&topology_tol);
    if (tess_result == 0 && botr != NULL) {
	tmp_internal->idb_ptr = (void *)botm;
	rt_nmg_brep(b, tmp_internal, tol);
    } else {
	(*b)->Destroy();
    }

    FREE_MODEL(botm);
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

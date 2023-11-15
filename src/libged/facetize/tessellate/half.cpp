/*                        H A L F . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @file libged/facetize/tessellate/half.cpp
 *
 * When we're doing booleans, infinite half spaces aren't usable
 * directly for mesh operations  Need to represent as a BoT.
 */

#include "common.h"

#include "../../ged_private.h"
#include "./tessellate.h"

// We use an arbn to define the enclosed volume, and facetize that
int
half_to_bot(struct rt_bot_internal **out, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    plane_t equations[7];
    // First, bound the volume to its limits
    HSET(equations[0], -1, 0, 0, FLT_MAX);
    HSET(equations[1], 1, 0, 0, FLT_MAX);
    HSET(equations[2], 0, -1, 0, FLT_MAX);
    HSET(equations[3], 0, 1, 0, FLT_MAX);
    HSET(equations[4], 0, 0, -1, FLT_MAX);
    HSET(equations[5], 0, 0, 1, FLT_MAX);

    // Then introduce the half plane
    struct rt_half_internal *h = (struct rt_half_internal *)ip->idb_ptr;
    HMOVE(equations[6], h->eqn);

    struct rt_arbn_internal arbn;
    arbn.magic = RT_ARBN_INTERNAL_MAGIC;
    arbn.neqn = 7;
    arbn.eqn = equations;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_ARBN;
    intern.idb_ptr = &arbn;
    intern.idb_meth = &OBJ[ID_ARBN];

    struct nmgregion *r1 = NULL;
    struct model *m = nmg_mm();
    if (intern.idb_meth->ft_tessellate(&r1, m, &intern, ttol, tol)) {
	(*out) = NULL;
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *hbot = (struct rt_bot_internal *)nmg_mdl_to_bot(m, &RTG.rtg_vlfree, tol);
    (*out) = hbot;
    return (!hbot) ? BRLCAD_ERROR : BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


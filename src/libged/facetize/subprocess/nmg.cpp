/*                        N M G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @file libged/facetize/tessellate/nmg.cpp
 *
 * Invoke the standard librt/libnmg routines to generate a mesh from
 * an implicit solid.
 */

#include "common.h"

#include <sstream>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/opt.h"
#include "rt/primitives/bot.h"
#include "ged.h"
#include "./tessellate.h"

int
_nmg_tessellate(struct rt_bot_internal **nbot, struct rt_db_internal *intern, tess_opts *s)
{
    int status = -1;
    struct bu_list *vlfree = &RTG.rtg_vlfree;

    if (!nbot || !intern || !intern->idb_meth || !s)
	return BRLCAD_ERROR;

    // TODO - get the nmg debug option from nmg_options and set it
#if 0
    if (bu_vls_strlen(&nmg_debug_str)) {
	long l = std::stol(bu_vls_cstr(&nmg_debug_str), nullptr, 16);
	nmg_debug |= l;
    }
#endif


    (*nbot) = NULL;

    struct model *m = nmg_mm();
    struct nmgregion *r1 = (struct nmgregion *)NULL;
    // Try the NMG routines (primary means of CSG implicit -> explicit mesh conversion)
    if (!BU_SETJUMP) {
	status = intern->idb_meth->ft_tessellate(&r1, m, intern, &s->nmg_options.ttol, &s->nmg_options.tol);
    } else {
	BU_UNSETJUMP;
	status = -1;
    } BU_UNSETJUMP;

    if (status <= -1)
	return BRLCAD_ERROR;

    // NMG reports success, now get a BoT
    if (!BU_SETJUMP) {
	(*nbot) = (struct rt_bot_internal *)nmg_mdl_to_bot(m, vlfree, &s->nmg_options.tol);
    } else {
	BU_UNSETJUMP;
	(*nbot) = NULL;
    } BU_UNSETJUMP;

    if (!(*nbot))
	return BRLCAD_ERROR;

    if (!bot_is_manifold(*nbot)) {
	bu_log("We got an NMG mesh, but it's no good for a Manifold(!?)\n");
	if ((*nbot)->vertices)
	    bu_free((*nbot)->vertices, "verts");
	if ((*nbot)->faces)
	    bu_free((*nbot)->faces, "faces");
	BU_FREE((*nbot), struct rt_bot_internal);
	(*nbot) = NULL;
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


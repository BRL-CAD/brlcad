/*                         W H O . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/who.cpp
 *
 * The who command.
 *
 */

#include "common.h"

#include <set>

#include "bu/malloc.h"
#include "bu/str.h"
#include "ged.h"

/*
 * List the db objects currently drawn
 *
 * Usage:
 * who
 *
 * TODO - like draw2, this needs to be aware of whether we're using local or shared
 * grp sets - if we're adaptive plotting, for example.
 */
extern "C" int
ged_who2_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    struct bu_ptbl *sg = gedp->ged_gvp->gv_db_grps;
    for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
	bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&cg->s_name));
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


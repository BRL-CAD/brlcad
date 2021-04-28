/*                         E R A S E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/erase.cpp
 *
 * The erase command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>


#include "../ged_private.h"

/*
 * Erase objects from the display.
 *
 */
extern "C" int
ged_erase2_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[object(s)]";
    const char *cmdName = argv[0];
    struct bview *v = gedp->ged_gvp;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Check that we have a view */
    if (!v) {
	bu_vls_printf(gedp->ged_result_str, "No current view defined in GED, nothing to erase from");
	return GED_ERROR;
    }

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmdName, usage);
	return GED_HELP;
    }

    /* skip past cmd */
    argc--; argv++;

    int rmcnt = 0;
    for (size_t i = 0; i < (size_t)argc; ++i) {
	for (size_t j = 0; i < BU_PTBL_LEN(gedp->ged_gvp->gv_scene_objs); j++) {
	    struct bview_scene_obj *s = (struct bview_scene_obj *)BU_PTBL_GET(v->gv_scene_objs, j);
	    if (BU_STR_EQUAL(argv[i], bu_vls_cstr(&s->s_uuid))) {
		bu_ptbl_rm(gedp->ged_gvp->gv_scene_objs, (long *)s);
		bview_scene_obj_free(s);
		BU_PUT(s, struct bview_scene_obj);
		rmcnt++;
		break;
	    }
	}
    }

    if (rmcnt != argc) {
	bu_vls_printf(gedp->ged_result_str, "%d objects specified for removal, but only removed %d", argc, rmcnt);
	return GED_ERROR;
    }

    return GED_OK;
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

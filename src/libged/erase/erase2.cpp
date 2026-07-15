/*                         E R A S E . C P P
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
/** @file libged/erase.cpp
 *
 * The erase command.
 *
 */

#include "common.h"

#include <set>
#include <unordered_map>

#include <stdlib.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "bu/sort.h"
#include "bv/view_sets.h"
#include "ged/database.h"
#include "ged/view.h"

#include "../dbi.h"
#include "../ged_private.h"

/*
 * Erase objects from the scene.  In many ways this is the most complex
 * operation we need to perform on a displayed set of objects.  When subsets of
 * objects are erased, the top level "who" objects need to be split into new
 * objects, with each new "who" object being the highest level child instance
 * available in the hierarchy that is still fully drawn.
 */
extern "C" int
ged_erase2_core(struct ged *gedp, int argc, const char *argv[])
{
    const char *cmdName = argv[0];
    struct bview *v = gedp->ged_gvp;
    int operand_index;
    struct ged_erase_new_args args = {NULL, -1};

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    operand_index = bu_cmd_schema_parse(&ged_erase_new_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str,
		"Usage: %s [--view name] [--mode number] object ...", cmdName);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

    /* Erase may operate on a particular independent view.  Resolve it before
     * the no-object help case so an explicitly invalid view remains an error. */
    if (args.view) {
	v = bv_set_find_view(&gedp->ged_views, args.view);
	if (!v) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", args.view);
	    return BRLCAD_ERROR;
	}

	if (!v->independent) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s is not an independent view, and as such does not support specifying db objects for display in only this view.  To change the view's status, the command 'view independent %s 1' may be applied.\n", args.view, args.view);
	    return BRLCAD_ERROR;
	}
    }

    /* Check that we have a view */
    if (!v) {
	bu_vls_printf(gedp->ged_result_str, "No view specified and no current view defined in GED, nothing to erase from");
	return BRLCAD_ERROR;
    }

    /* must be wanting help */
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s [--view name] [--mode number] object ...", cmdName);
	return GED_HELP;
    }

    if (!gedp->dbi_state)
	return BRLCAD_OK;

    DbiState *dbis = (DbiState *)gedp->dbi_state;
    BViewState *bvs = dbis->get_view_state(v);
    bvs->erase_path(args.mode, argc, argv);

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

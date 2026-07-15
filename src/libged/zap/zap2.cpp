/*                         Z A P . C P P
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
/** @file libged/zap.cpp
 *
 * The zap command.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "../ged_private.h"
#include "../dbi.h"
#include "zap.h"

static const struct bu_cmd_schema *
zap_schema_for_command(const char *command)
{
    return BU_STR_EQUAL(command, "Z") ? &ged_Z_cmd_schema : &ged_zap_cmd_schema;
}

static void
zap_show_help(struct ged *gedp, const char *command, const struct bu_cmd_schema *schema)
{
    char *option_help = bu_cmd_schema_describe(schema);

    bu_vls_printf(gedp->ged_result_str, "Usage: %s [options]\n", command);
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "Options:\n%s", option_help);
	bu_free(option_help, "zap native option help");
    }
}

/*
 * Erase all currently displayed geometry
 *
 * Usage:
 * zap
 *
 */
extern "C" int
ged_zap2_core(struct ged *gedp, int argc, const char *argv[])
{
    struct zap_args args = {0, NULL, 0, 0, 0, 0};
    const struct bu_cmd_schema *schema;
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    struct bview *v = NULL;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    schema = zap_schema_for_command(argv[0]);
    if (bu_cmd_schema_parse_complete(schema, &args, gedp->ged_result_str,
	argc - 1, argv + 1) < 0) {
	zap_show_help(gedp, argv[0], schema);
	return BRLCAD_ERROR;
    }


    if (args.print_help) {
	zap_show_help(gedp, argv[0], schema);
	return GED_HELP;
    }

    if (!args.clear_solid_objs && !args.clear_view_objs) {
	args.clear_solid_objs = 1;
	args.clear_view_objs = 1;
    }

    if (!args.clear_all_views && args.view && args.view[0]) {

	if (args.shared_only) {
	    bu_vls_printf(gedp->ged_result_str, "Zap scope defined as view %s - not clearing shared data\n", args.view);
	    return BRLCAD_ERROR;
	}

	v = bv_set_find_view(&gedp->ged_views, args.view);
	if (!v) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", args.view);
	    return BRLCAD_ERROR;
	}

	int flags = BV_LOCAL_OBJS;
	if (args.clear_solid_objs) {
	    flags |= BV_DB_OBJS;
	    if (gedp->dbi_state) {
		DbiState *dbis = (DbiState *)gedp->dbi_state;
		BViewState *bvs = dbis->get_view_state(v);
		bvs->clear();
	    }
	}

	if (args.clear_view_objs)
	    flags |= BV_VIEW_OBJS;

	if (!bv_clear(v, flags))
	    v->gv_s->gv_cleared = 1;

	return BRLCAD_OK;
    }

    // Clear everything
    int ret = BRLCAD_OK;
    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	v = (struct bview *)BU_PTBL_GET(views, i);
	if (v->independent && !args.clear_all_views)
	    continue;
	int flags = 0;
	if (args.clear_solid_objs) {
	    flags |= BV_DB_OBJS;
	    if (gedp->dbi_state) {
		DbiState *dbis = (DbiState *)gedp->dbi_state;
		BViewState *bvs = dbis->get_view_state(v);
		bvs->clear();
	    }
	}
	if (args.clear_view_objs)
	    flags |= BV_VIEW_OBJS;
	int nret = bv_clear(v, flags);
	int lret = 1;
	if (!args.shared_only) {
	    flags |= BV_LOCAL_OBJS;
	    lret = bv_clear(v, flags);
	}
	if (!nret || !lret)
	    v->gv_s->gv_cleared = 1;

	ret = BRLCAD_OK;
    }
    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

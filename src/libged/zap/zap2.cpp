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
#include "ged/bsg_ged_draw.h"
#include "../ged_private.h"


static int
zap_draw_db_scope(struct ged *gedp, struct bsg_view *v)
{
    struct ged_draw_transaction txn =
	ged_draw_transaction_make(GED_DRAW_TXN_CLEAR_SCOPE, NULL);
    txn.view = v;
    return ged_draw_apply_transaction(gedp, &txn, NULL);
}


static int
zap_has_independent_views(struct ged *gedp)
{
    if (!gedp)
	return 0;

    struct bu_ptbl *views = bsg_set_views(&gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	struct bsg_view *v = (struct bsg_view *)BU_PTBL_GET(views, i);
	if (v && bsg_view_is_independent(v))
	    return 1;
    }

    return 0;
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
    int print_help = 0;
    struct bu_vls cvls = BU_VLS_INIT_ZERO;
    int clear_view_objs = 0;
    int clear_solid_objs = 0;
    int clear_all_views = 0;
    int shared_only = 0;
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    const char *usage = "zap [options]\n";
    struct bsg_view *v = NULL;

    argc-=(argc>0); argv+=(argc>0); /* done with command name argv[0] */

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bu_opt_desc d[8];
    BU_OPT(d[0],  "h", "help",         "",        NULL,       &print_help, "Print help and exit");
    BU_OPT(d[1],  "?", "",             "",        NULL,       &print_help, "");
    BU_OPT(d[2],  "V", "view",     "name", &bu_opt_vls,             &cvls, "clear data specific to this view");
    BU_OPT(d[3],  "S", "shared",       "",        NULL,      &shared_only, "clear only data shared across views");
    BU_OPT(d[4],  "v", "view-objs",    "",        NULL,  &clear_view_objs, "clear non-solid based view features");
    BU_OPT(d[5],  "g", "solid-objs",   "",        NULL, &clear_solid_objs, "clear solid based view features");
    BU_OPT(d[6],  "",  "all",          "",        NULL,  &clear_all_views, "clear shared and independent views");
    BU_OPT_NULL(d[7]);

    int opt_ret = bu_opt_parse(NULL, argc, argv, d);
    argc = opt_ret;

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return GED_HELP;
    }

    if (argc) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_ERROR;
    }

    if (!clear_solid_objs && !clear_view_objs) {
	clear_solid_objs = 1;
	clear_view_objs = 1;
    }

    if (!clear_all_views && bu_vls_strlen(&cvls)) {

	if (shared_only) {
	    bu_vls_printf(gedp->ged_result_str, "Zap scope defined as view %s - not clearing shared data\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}

	v = bsg_set_find_view(&gedp->ged_views, bu_vls_cstr(&cvls));
	if (!v) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}

	int flags = BSG_SOURCE_LOCAL;
	if (clear_solid_objs && bsg_view_is_independent(v)) {
	    flags |= BSG_SOURCE_DB;
	    zap_draw_db_scope(gedp, v);
	}

	if (clear_view_objs)
	    flags |= BSG_SOURCE_VIEW;

	if (!bsg_clear(v, flags))
	    bsg_view_set_cleared(v, 1);

	bu_vls_free(&cvls);
	return BRLCAD_OK;
    }

    // Clear everything
    int ret = BRLCAD_OK;
    int cleared_shared_db = 0;
    int full_canonical_clear = clear_solid_objs && clear_view_objs &&
	!shared_only &&
	(clear_all_views || !zap_has_independent_views(gedp));
    if (clear_solid_objs && !full_canonical_clear)
	cleared_shared_db = zap_draw_db_scope(gedp, NULL);

    struct bu_ptbl *views = bsg_set_views(&gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	v = (struct bsg_view *)BU_PTBL_GET(views, i);
	if (bsg_view_is_independent(v) && !clear_all_views)
	    continue;
	int flags = 0;
	if (clear_solid_objs) {
	    flags |= BSG_SOURCE_DB;
	    if (!full_canonical_clear &&
		    (bsg_view_is_independent(v) || !cleared_shared_db))
		zap_draw_db_scope(gedp, bsg_view_is_independent(v) ? v : NULL);
	}
	if (clear_view_objs)
	    flags |= BSG_SOURCE_VIEW;
	int nret = bsg_clear(v, flags);
	int lret = 1;
	if (!shared_only) {
	    flags |= BSG_SOURCE_LOCAL;
	    lret = bsg_clear(v, flags);
	}
	if (!nret || !lret)
	    bsg_view_set_cleared(v, 1);

	ret = BRLCAD_OK;
    }

    if (full_canonical_clear) {
	struct ged_draw_transaction txn =
	    ged_draw_transaction_make(GED_DRAW_TXN_CLEAR, NULL);
	(void)ged_draw_apply_transaction(gedp, &txn, NULL);
    }

    bu_vls_free(&cvls);
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

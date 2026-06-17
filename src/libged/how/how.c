/*                         H O W . C
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
/** @file libged/how.c
 *
 * The how command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/str.h"
#include "dm.h"
#include "ged/bsg_ged_draw.h"
#include "../ged_private.h"

struct how_find_ctx {
    const char *path;
    struct bsg_view *view;
    const struct ged_draw_group_record *match;
    struct ged_draw_group_record rec;
};

static int
how_group_match_cb(const struct ged_draw_group_record *rec, void *ud)
{
    struct how_find_ctx *ctx = (struct how_find_ctx *)ud;
    if (!ctx || !rec || !rec->path || !ctx->path)
	return 1;
    if (rec->is_overlay || !rec->visible || rec->shape_count <= 0)
	return 1;
    if (!ged_draw_group_record_in_view(rec, ctx->view))
	return 1;

    if (BU_STR_EQUAL(rec->path, ctx->path)) {
	ctx->rec = *rec;
	ctx->match = &ctx->rec;
	return 0;
    }

    size_t n = strlen(ctx->path);
    if (strlen(rec->path) > n && !bu_strncmp(rec->path, ctx->path, n) &&
	    rec->path[n] == '/') {
	ctx->rec = *rec;
	ctx->match = &ctx->rec;
	return 0;
    }

    return 1;
}

/*
 * Returns "how" an object is being displayed.
 *
 * Usage:
 * how [-b] object
 *
 */
int
ged_how_core(struct ged *gedp, int argc, const char *argv[])
{
    int both = 0;
    static const char *usage = "[-b] object";
    const char *obj_arg = NULL;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (3 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (argc == 3 &&
	argv[1][0] == '-' &&
	argv[1][1] == 'b') {
	both = 1;
    } else {
	if (argc != 2) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
	}
    }

    obj_arg = both ? argv[2] : argv[1];
    struct how_find_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.path = obj_arg;
    ctx.view = gedp->ged_gvp;
    ged_draw_foreach_group_record(gedp, how_group_match_cb, &ctx);
    if (!ctx.match)
	goto not_found;

    int dmode = (int)ctx.match->draw_mode;
    fastf_t transparency = ctx.match->transparency;
    if (dmode == _GED_HIDDEN_LINE) {
	if (both)
	    bu_vls_printf(gedp->ged_result_str, "%d 1", _GED_HIDDEN_LINE);
	else
	    bu_vls_printf(gedp->ged_result_str, "%d", _GED_HIDDEN_LINE);
    } else {
	if (both)
	    bu_vls_printf(gedp->ged_result_str, "%d %g", dmode, transparency);
	else
	    bu_vls_printf(gedp->ged_result_str, "%d", dmode);
    }
    return BRLCAD_OK;

not_found:
    bu_vls_printf(gedp->ged_result_str, "-1");

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_HOW_COMMANDS(X, XID) \
    X(how, ged_how_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_HOW_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_how", 1, GED_HOW_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

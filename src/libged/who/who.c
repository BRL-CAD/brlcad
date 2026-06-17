/*                         W H O . C
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
/** @file libged/who.c
 *
 * The who command.
 *
 */

#include <string.h>

#include "ged.h"
#include "ged/bsg_ged_draw.h"
#include "../ged_private.h"

extern int ged_who2_core(struct ged *gedp, int argc, const char **argv);
extern int ged_who_solids_core(struct ged *gedp, int argc, const char **argv);

struct who_record_ctx {
    struct ged *gedp;
};

static int
who_record_cb(const struct ged_draw_group_record *rec, void *ud)
{
    struct who_record_ctx *c = (struct who_record_ctx *)ud;
    if (!rec || !c)
	return 1;
    if (!rec->is_overlay)
	return 1;
    if (!rec->visible || rec->shape_count <= 0)
	return 1;
    if (!rec->path || !*rec->path)
	return 1;
    bu_vls_printf(c->gedp->ged_result_str, "%s ", rec->path);
    return 1;
}


static void
who_append_real_paths(struct ged *gedp, struct bsg_view *view)
{
    struct bu_vls paths = BU_VLS_INIT_ZERO;
    ged_draw_list_paths(gedp, view, -1, 0, &paths);

    const char *start = bu_vls_cstr(&paths);
    while (start && *start) {
	const char *end = strchr(start, '\n');
	size_t len = end ? (size_t)(end - start) : strlen(start);
	if (len > 0)
	    bu_vls_printf(gedp->ged_result_str, "%.*s ", (int)len, start);
	if (!end)
	    break;
	start = end + 1;
    }

    bu_vls_free(&paths);
}


static int
who_legacy_selector(const char *arg)
{
    if (!arg)
	return 0;

    return BU_STR_EQUAL(arg, "r") || BU_STR_EQUAL(arg, "real") ||
	BU_STR_EQUAL(arg, "p") || BU_STR_EQUAL(arg, "phony") ||
	BU_STR_EQUAL(arg, "b") || BU_STR_EQUAL(arg, "both");
}

/*
 * List the objects currently prepped for drawing
 *
 * Usage:
 * who [r(eal)|p(hony)|b(oth)]
 * who solids [level]
 *
 */
int
ged_who_core(struct ged *gedp, int argc, const char *argv[])
{
    if (argc > 1 && (BU_STR_EQUAL(argv[1], "solids") || BU_STR_EQUAL(argv[1], "report")))
	return ged_who_solids_core(gedp, argc, argv);

    if (argc != 2 || !who_legacy_selector(argv[1]))
	return ged_who2_core(gedp, argc, argv);


    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    int list_real = 1;
    int list_overlays = 0;
    switch (argv[1][0]) {
	case 'b':
	    list_real = 1;
	    list_overlays = 1;
	    break;
	case 'p':
	    list_real = 0;
	    list_overlays = 1;
	    break;
	case 'r':
	default:
	    list_real = 1;
	    list_overlays = 0;
	    break;
    }

    if (list_real)
	who_append_real_paths(gedp, gedp->ged_gvp);

    if (list_overlays) {
	struct who_record_ctx ctx = {gedp};
	ged_draw_foreach_group_record(gedp, who_record_cb, &ctx);
    }

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_WHO_COMMANDS(X, XID) \
    X(who, ged_who_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_WHO_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_who", 1, GED_WHO_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

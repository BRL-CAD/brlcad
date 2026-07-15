/*                       A U T O V I E W . C P P
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
/** @file libged/autoview.cpp
 *
 * The autoview command.
 *
 */

#include "common.h"

#include "bu/cmdschema.h"
#include "dm.h"
#include "../ged_private.h"


struct autoview2_args {
    int print_help;
    int all_view_objects;
    fastf_t scale;
    struct bu_vls view;
};

static const struct bu_cmd_option autoview2_options[] = {
    BU_CMD_FLAG("h", "help", struct autoview2_args, print_help, "Print help and exit"),
    BU_CMD_FLAG(NULL, "all-objs", struct autoview2_args, all_view_objects,
	"Bound all non-faceplate view objects"),
    BU_CMD_NUMBER("s", "scale", struct autoview2_args, scale, "number",
	"Set view scale (model scale relative to view size)"),
    BU_CMD_VLS_APPEND("V", "view", struct autoview2_args, view, "name",
	"Specify view to adjust"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand autoview2_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_PATH, 0, BU_CMD_COUNT_UNLIMITED,
	"Object or full path to frame", "ged.db_path"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema autoview2_schema = {
    "autoview", "Frame displayed geometry or named objects", autoview2_options,
    autoview2_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


static int
_autoview_arg_is_num(const char *s, fastf_t *v)
{
    return bu_cmd_number_from_str(v, s);
}


static void
autoview2_usage(struct ged *gedp)
{
    char *option_help = bu_cmd_schema_describe(&autoview2_schema);

    bu_vls_printf(gedp->ged_result_str, "Usage: %s [options] [object ...]\n", autoview2_schema.name);
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "Options:\n%s\n", option_help);
	bu_free(option_help, "autoview native option help");
    }
}

/*
 * Auto-adjust the view so that geometry is framed within the view.  By
 * default all displayed geometry is framed, but if a list of objects
 * (or full paths) is supplied the view is instead centered and sized to
 * frame only those objects.
 *
 * Usage:
 * autoview [options] [object ...]
 *
 * TODO - like draw2, this needs to be aware of whether we're using local or shared
 * grp sets - if we're adaptive plotting, for example.
 *
 */
extern "C" int
ged_autoview2_core(struct ged *gedp, int argc, const char *argv[])
{
    /* default, 0.5 model scale == 2.0 view factor */
    fastf_t factor = BV_AUTOVIEW_SCALE_DEFAULT;
    struct autoview2_args args = {0, 0, -1.0, BU_VLS_INIT_ZERO};
    const char **pargv = NULL;
    int operand_index;
    int object_count;

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview *v = gedp->ged_gvp;

    pargv = (const char **)bu_calloc((size_t)argc, sizeof(*pargv), "autoview native argv");
    memcpy(pargv, argv, (size_t)argc * sizeof(*pargv));
    operand_index = bu_cmd_schema_parse_complete(&autoview2_schema, &args,
	gedp->ged_result_str, argc - 1, pargv + 1);
    if (operand_index < 0) {
	autoview2_usage(gedp);
	bu_vls_free(&args.view);
	bu_free((void *)pargv, "autoview native argv");
	return BRLCAD_ERROR;
    }
    object_count = argc - 1 - operand_index;
    argv = pargv + 1 + operand_index;

    if (args.print_help) {
	autoview2_usage(gedp);
	bu_vls_free(&args.view);
	bu_free((void *)pargv, "autoview native argv");
	return BRLCAD_OK;
    }


    if (bu_vls_strlen(&args.view)) {
	v = bv_set_find_view(&gedp->ged_views, bu_vls_cstr(&args.view));
	if (!v) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&args.view));
	    bu_vls_free(&args.view);
	    bu_free((void *)pargv, "autoview native argv");
	    return BRLCAD_ERROR;
	}
    }
    bu_vls_free(&args.view);


    /* Backward compatibility: a lone leading numeric argument historically
     * set the view scale.  Honor that only when -s/--scale was not supplied
     * and the token does not name an existing object, so an object with a
     * numeric name still wins. */

    if (args.scale < 0.0 && object_count > 0) {
	fastf_t sval;
	if (_autoview_arg_is_num(argv[0], &sval) &&
	    (!gedp->dbip || db_lookup(gedp->dbip, argv[0], LOOKUP_QUIET) == RT_DIR_NULL)) {
	    args.scale = sval;
	    object_count--;
	    argv++;
	}
    }

    if (args.scale > 0.0)
	factor = 1.0 / args.scale;

    if (object_count > 0) {
	/* Frame only the named objects.  Bound them directly from the
	 * database (they need not be displayed). */
	point_t min, max;
	if (rt_obj_bounds(gedp->ged_result_str, gedp->dbip, object_count, argv, 0, min, max) != BRLCAD_OK) {
	    bu_free((void *)pargv, "autoview native argv");
	    return BRLCAD_ERROR;
	}
	bv_autoview_bounds(v, factor, min, max);
    } else {
	// libbv has the nuts and bolts
	bv_autoview(v, factor, args.all_view_objects);
    }

    bu_free((void *)pargv, "autoview native argv");
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

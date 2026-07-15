/*                      A N N O T A T E . C
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
/** @file libged/annotate.c
 *
 * The annotate command.
 *
 * Examples:
 *
 *   annotate
 *            [text {string}]
 *            [as label|leader|angular|radial|dimension|table|note|box|axes|plane [named {name}]]
 *            [on|for {object1} [and {object2}] [and {...}]]
 *            [to|thru|at {point|plane} [offset {distance|vector}]]
 *            [align auto|model|view]
 *            [position auto|fixed|absolute|relative]
 *            [visible always|never|rendering|wireframe]
 *
 *            [help]
 *            [list {name}]
 *            [get {key} from {name}]
 *            [set {key=value} on {name}]
 *
 *   annotate help
 *   annotate text "Hello, World!"
 *   annotate for all.g
 *   annotate as label named my.note for all.g text "This is a tank."
 *   annotate as box text "This geometry is unclassified."
 *
 * DESIGN OPTIONS TO CONSIDER:
 *
 * Types: text (see Text), leader, angular, radial, aligned, ordinate, linear
 *
 * Extended types: note, label, table, dimension, tolerance, box (see Box), axes, plane
 *
 * Orientation: auto, horizontal, vertical, above, below, inline
 *
 * Align: auto, model, view
 *
 * Text: fontname, fontsize, fontstyle (regular, italic, bold), linespacing, justification
 *   Justification:
 *     undefined
 *     left
 *     center
 *     right
 *     bottom
 *     middle
 *     top
 *     bottomleft
 *     bottomcenter
 *     bottomright
 *     middleleft
 *     middlecenter
 *     middleright
 *     topleft
 *     topcenter
 *     topright
 *
 * Decoration: color, leader line, box (see Box) around target, box around annotation
 *
 * Box: empty, hatch, gradient, solid
 *
 * Placement: position (auto/fixed/relative/absolute), scale, orientation/rotation/twist, head/tail
 *   auto is automatic static placement
 *   fixed is 2d coordinate relative to view
 *   absolute is 2d coordinate relative to world center or global 3d position
 *   relative is 2d coordinate relative to view center or 3d offset distance from target center
 *
 * Leader: linelength, linewidth, type (no head, arrow head, round head, square head)
 *
 * Visibility: auto, wireframe, render, both
 *
 * linearformat: "%.2f"
 * linearunits: mm, m, in, ft, etc.
 *
 * angularformat: "%.2f"
 * angularunits: degrees, radians
 *
 * struct parameters:
 * type
 * plane
 * point list
 * text?
 * color?
 * visibility?
 *
 */

#include "common.h"

#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "ged.h"


struct annotate_args {
    const char *name;
    fastf_t point[3];
};

static const struct bu_cmd_arg_shape annotate_point_shape =
    BU_CMD_ARG_SHAPE(BU_CMD_ARG_SHAPE_TOKEN_SEQUENCE, 3, 3, "XYZ components");

static int
annotate_point_consume(struct bu_vls *msg, size_t argc, const char **argv,
	void *storage)
{
    fastf_t *point = (fastf_t *)storage;

    if (argc != 3) {
	if (msg)
	    bu_vls_printf(msg, "annotation point requires X Y Z components");
	return -1;
    }
    for (size_t i = 0; i < argc; i++) {
	char *end = NULL;
	double value;

	errno = 0;
	value = strtod(argv[i], &end);
	if (errno == ERANGE || !end || *end || !isfinite(value) ||
	    (sizeof(fastf_t) == sizeof(float) && (value > FLT_MAX || value < -FLT_MAX))) {
	    if (msg)
		bu_vls_printf(msg, "invalid annotation point component: %s", argv[i]);
	    return -1;
	}
	if (point)
	    point[i] = (fastf_t)value;
    }
    return 0;
}

static const struct bu_cmd_option annotate_schema_options[] = {
    BU_CMD_STRING("n", NULL, struct annotate_args, name, "name", "Annotation name"),
    BU_CMD_OPTION_SHAPED("p", NULL, "p", struct annotate_args, point,
	BU_CMD_VALUE_VECTOR, "x y z", "Annotation point", BU_CMD_ARG_REQUIRED,
	&annotate_point_shape, annotate_point_consume),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand annotate_schema_operands[] = {
    BU_CMD_OPERAND("objects", BU_CMD_VALUE_DB_PATH, 1, BU_CMD_COUNT_UNLIMITED,
	"Objects to annotate", "ged.db_path"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema annotate_cmd_schema = {
    "annotate", "Create an annotation for objects", annotate_schema_options,
    annotate_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


void
annotate_help(struct bu_vls *result, const char *cmd)
{
    static const char *usage = "[object(s)] [-n name] [-p x y z]";

    bu_vls_printf(result, "Usage: %s %s", cmd, usage);
}


int
ged_annotate_core(struct ged *gedp, int argc, const char *argv[])
{
    char **object_argv;
    const char *argv0 = argv[0];
    struct bu_vls objects = BU_VLS_INIT_ZERO;
    struct annotate_args args = {NULL, {0.0, 0.0, 0.0}};
    int object_count = 0;
    int i;
    int operand_index;

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	annotate_help(gedp->ged_result_str, argv0);
	return GED_HELP;
    }

    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);


    operand_index = bu_cmd_schema_parse_complete(&annotate_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	annotate_help(gedp->ged_result_str, argv0);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

    /* stash objects, quoting them if they include spaces */
    for (i = 0; i < argc; i++) {
	bu_log("DEBUG: argv[%d] == %s\n", i, argv[i]);

	object_count++;
	if (i != 0)
	    bu_vls_putc(&objects, ' ');
	bu_vls_from_argv(&objects, 1, &(argv[i]));
    }
    bu_log("DEBUG: objects: [%s]\n", bu_vls_addr(&objects));

    object_argv = (char **)bu_calloc(object_count+1, sizeof(char *), "alloc object argv");
    bu_argv_from_string(object_argv, object_count, bu_vls_addr(&objects));
    for (i = 0; i < object_count; i++) {
	bu_log("DEBUG: stashed [%s]\n", object_argv[i]);
    }

    bu_vls_free(&objects);
    bu_free((void *)object_argv, "ged_annotate_core");

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_ANNOTATE_COMMANDS(X, XID) \
    X(annotate, ged_annotate_core, GED_CMD_DEFAULT, &annotate_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_ANNOTATE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_annotate", 1, GED_ANNOTATE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

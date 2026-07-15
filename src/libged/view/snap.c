/*                         S N A P . C
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
/** @file libged/snap.c
 *
 * Logic for snapping points to visual elements.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmdschema.h"
#include "bu/vls.h"
#include "bv/snap.h"
#include "dm.h"
#include "../ged_private.h"
#include "./ged_view.h"

struct ged_view_snap_args {
    int print_help;
    fastf_t tolerance;
    int use_grid;
    int use_lines;
    int use_object_keypoints;
    int use_object_lines;
};

static int
ged_view_snap_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    size_t operands;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help"))
	return 0;
    operands = bu_cmd_schema_operand_count(schema, argc, argv);
    if ((bu_cmd_schema_option_present(schema, argc, argv, "tol") && !operands) ||
	operands == 2 || operands == 3)
	return 0;
    bu_cmd_validate_result_clear(result);
    result->state = operands < 2 ? BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_INVALID;
    result->token_start = cursor_arg < argc ? cursor_arg : argc;
    result->token_end = result->token_start;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = BU_CMD_VALUE_NUMBER;
    result->hint = "two screen coordinates or three model coordinates required";
    return 0;
}

static const struct bu_cmd_option ged_view_snap_options[] = {
    BU_CMD_FLAG("h", "help", struct ged_view_snap_args, print_help, "Print help and exit"),
    {"t", "tol", "tol", "number", "Set or report snapping tolerance scale factor",
	BU_CMD_VALUE_NUMBER, offsetof(struct ged_view_snap_args, tolerance), NULL, NULL,
	NULL, NULL, 0, 0, NULL, BU_CMD_ARG_OPTIONAL, NULL, NULL, NULL},
    BU_CMD_FLAG("g", "grid", struct ged_view_snap_args, use_grid, "Snap to the view grid"),
    BU_CMD_FLAG("l", "lines", struct ged_view_snap_args, use_lines, "Snap to view lines"),
    BU_CMD_FLAG("o", "obj-keypt", struct ged_view_snap_args, use_object_keypoints,
	"Snap to drawn object keypoints"),
    BU_CMD_FLAG("w", "obj-lines", struct ged_view_snap_args, use_object_lines,
	"Snap to drawn object lines"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand ged_view_snap_operands[] = {
    BU_CMD_OPERAND("coordinate", BU_CMD_VALUE_NUMBER, 0, 3,
	"Two screen or three model coordinates", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema ged_view_snap_schema = {
    "snap", "Snap a point to view elements", ged_view_snap_options,
    ged_view_snap_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(ged_view_snap_validate, NULL)
};


static void
ged_view_snap_usage(struct ged *gedp)
{
    char *option_help = bu_cmd_schema_describe(&ged_view_snap_schema);

    bu_vls_printf(gedp->ged_result_str, "Usage: snap [options] x y [z]\n");
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "Options:\n%s\n", option_help);
	bu_free(option_help, "view snap native option help");
    }
}


int
ged_view_snap(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_view_snap_args args = {0, 0.0, 0, 0, 0, 0};
    point2d_t view_pt_2d = {0.0, 0.0};
    point_t view_pt = VINIT_ZERO;
    const char **pargv = NULL;
    int operand_index;
    int tolerance_has_value = 0;

    /* must be wanting help */

    if (argc < 2) {
	ged_view_snap_usage(gedp);
	return BRLCAD_OK;
    }

    for (int i = 1; i < argc;) {
	int span = bu_cmd_schema_option_span(&ged_view_snap_schema,
	    (size_t)(argc - i), argv + i);
	if (span < 0)
	    break;
	if (!span) {
	    i++;
	    continue;
	}
	if ((BU_STR_EQUAL(argv[i], "-t") || BU_STR_EQUAL(argv[i], "--tol")) && span > 1)
	    tolerance_has_value = 1;
	i += span;
    }
    pargv = (const char **)bu_calloc((size_t)argc, sizeof(*pargv), "view snap native argv");
    memcpy(pargv, argv, (size_t)argc * sizeof(*pargv));
    operand_index = bu_cmd_schema_parse_complete(&ged_view_snap_schema, &args,
	gedp->ged_result_str, argc - 1, pargv + 1);
    if (operand_index < 0) {
	ged_view_snap_usage(gedp);
	bu_free((void *)pargv, "view snap native argv");
	return BRLCAD_ERROR;
    }
    argc = argc - 1 - operand_index;
    argv = pargv + 1 + operand_index;


    if (args.print_help) {
	ged_view_snap_usage(gedp);
	bu_free((void *)pargv, "view snap native argv");
	return BRLCAD_OK;
    }

    /* Handle tolerance */
    if (bu_cmd_schema_option_present(&ged_view_snap_schema, (size_t)(argc + operand_index),
	pargv + 1, "tol")) {
	if (tolerance_has_value) {
	    gedp->ged_gvp->gv_s->gv_snap_tol_factor = args.tolerance;
	    if (!argc) {
		bu_vls_printf(gedp->ged_result_str, "%g", gedp->ged_gvp->gv_s->gv_snap_tol_factor);
		bu_free((void *)pargv, "view snap native argv");
		return BRLCAD_OK;
	    }
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%g", gedp->ged_gvp->gv_s->gv_snap_tol_factor);
	    bu_free((void *)pargv, "view snap native argv");
	    return BRLCAD_OK;
	}
    }

    /* We may get a 2D screen point or a 3D model space point.  Either
     * should work - whatever we get, set up both points so we have
     * the necessary inputs for any of the options. */
    if (argc == 2) {
	point2d_t p2d = {0.0, 0.0};
	point_t p = VINIT_ZERO;
	point_t vp = VINIT_ZERO;
	if (!bu_cmd_number_from_str(&p2d[X], argv[0])) {
	    bu_vls_printf(gedp->ged_result_str, "problem reading X value %s\n", argv[0]);
	    bu_free((void *)pargv, "view snap native argv");
	    return BRLCAD_ERROR;
	}
	if (!bu_cmd_number_from_str(&p2d[Y], argv[1])) {
	    bu_vls_printf(gedp->ged_result_str, "problem reading Y value %s\n", argv[1]);
	    bu_free((void *)pargv, "view snap native argv");
	    return BRLCAD_ERROR;
	}
	V2MOVE(view_pt_2d, p2d);
	VSET(vp, p[0], p[1], 0);
	MAT4X3PNT(p, gedp->ged_gvp->gv_view2model, vp);
	VMOVE(view_pt, p);
    }
    /* We may get a 3D point instead */
    if (argc == 3) {
	point_t p = VINIT_ZERO;
	point_t vp = VINIT_ZERO;
	if (bu_cmd_vector3_from_argv(p, (size_t)argc, argv) != argc) {
	    bu_vls_sprintf(gedp->ged_result_str, "three finite model coordinates required");
	    bu_free((void *)pargv, "view snap native argv");
	    return BRLCAD_ERROR;
	}
	MAT4X3PNT(vp, gedp->ged_gvp->gv_model2view, p);
	V2SET(view_pt_2d, vp[0], vp[1]);
	VMOVE(view_pt, p);
    }

    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (args.use_grid) {
	// Grid operates on view space points
	bv_snap_grid_2d(gedp->ged_gvp, &view_pt_2d[X], &view_pt_2d[Y]);
    }

    if (args.use_lines) {
	point_t out_pt = VINIT_ZERO;
	point_t vp = VINIT_ZERO;
	// It's OK if we have no lines close enough to snap to -
	// in that case just pass back the view pt.  If we do
	// have a snap, update the output
	if (bv_snap_lines_3d(&out_pt, gedp->ged_gvp, &view_pt) == BRLCAD_OK) {
	    MAT4X3PNT(vp, gedp->ged_gvp->gv_model2view, out_pt);
	    V2SET(view_pt_2d, vp[0], vp[1]);
	    VMOVE(view_pt, out_pt);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "no lines close enough for snapping");
	    bu_free((void *)pargv, "view snap native argv");
	    return BRLCAD_OK;
	}
    }

    bu_vls_printf(gedp->ged_result_str, "%g %g %g\n", V3ARGS(view_pt));
    bu_vls_printf(gedp->ged_result_str, "%g %g\n", view_pt_2d[X], view_pt_2d[Y]);

    bu_free((void *)pargv, "view snap native argv");
    return BRLCAD_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

/*                         K N O B . C
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
/** @file libged/view/knob.c
 *
 * The view knob subcommand.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmdschema.h"
#include "../ged_private.h"
#include "./ged_view.h"


struct ged_view_knob_args {
    int print_help;
    int incr_flag;
    int view_flag;
    int model_flag;
    const char *origin;
};

static const struct bu_cmd_value_keyword ged_view_knob_origins[] = {
    {"v", NULL, "View origin (default)"},
    {"m", NULL, "Model origin"},
    {"e", NULL, "Eye point"},
    {"k", NULL, "Current keypoint"},
    {NULL, NULL, NULL}
};

static const char * const ged_view_knob_actions[] = {
    "zap", "zero", "clear", "stop", "calibrate",
    "x", "y", "z", "X", "Y", "Z", "S",
    "ax", "ay", "az", "aX", "aY", "aZ", "aS",
    NULL
};

static int
ged_view_knob_action_is_reset(const char *action)
{
    return BU_STR_EQUAL(action, "zap") || BU_STR_EQUAL(action, "zero") ||
	BU_STR_EQUAL(action, "clear") || BU_STR_EQUAL(action, "stop");
}


static int
ged_view_knob_action_is_value(const char *action)
{
    for (size_t i = 5; ged_view_knob_actions[i]; i++)
	if (BU_STR_EQUAL(action, ged_view_knob_actions[i]))
	    return 1;
    return 0;
}


static void
ged_view_knob_validation_result(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, bu_cmd_value_t type,
	const char *hint)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = type;
    result->hint = hint;
}


/* The action stream intentionally lives in the command schema rather than
 * bv_knobs_cmd_process: this makes completion, highlighting, help, and the
 * execution preflight agree on the same alternating action/value grammar. */
static int
ged_view_knob_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    size_t i = 0;
    int option_phase = 1;
    int want_value = 0;

    flat.validation.custom_validate = NULL;
    if (bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result) != 0 ||
	result->state == BU_CMD_VALIDATE_INVALID)
	return 0;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help"))
	return 0;
    /* The ordinary schema owns option prefixes and option arguments. */
	if ((result->expected & BU_CMD_EXPECT_OPTION_ARG) ||
	(cursor_arg < argc && (result->expected & BU_CMD_EXPECT_OPTION)))
	return 0;

    /* Parse every committed positional word up to the cursor.  This mirrors
     * the native option scanner so interspersed options never become actions. */
    while (i < cursor_arg) {
	int span = 0;
	if (option_phase && BU_STR_EQUAL(argv[i], "--")) {
	    option_phase = 0;
	    i++;
	    continue;
	}
	if (option_phase)
	    span = bu_cmd_schema_option_span(schema, argc - i, argv + i);
	if (span > 0) {
	    i += (size_t)span;
	    continue;
	}
	if (want_value) {
	    fastf_t value;
	    if (!bu_cmd_number_from_str(&value, argv[i])) {
		ged_view_knob_validation_result(result, BU_CMD_VALIDATE_INVALID, i,
		    BU_CMD_VALUE_NUMBER, "finite numeric knob value required");
		return 0;
	    }
	    want_value = 0;
	    i++;
	    continue;
	}
	if (ged_view_knob_action_is_reset(argv[i]) ||
	    BU_STR_EQUAL(argv[i], "calibrate")) {
	    i++;
	    continue;
	}
	if (!ged_view_knob_action_is_value(argv[i])) {
	    ged_view_knob_validation_result(result, BU_CMD_VALIDATE_INVALID, i,
		BU_CMD_VALUE_STRING, "known knob action required");
	    bu_cmd_keyword_candidates(result, ged_view_knob_actions, argv[i]);
	    return 0;
	}
	want_value = 1;
	i++;
    }

    if (cursor_arg < argc) {
	const char *current = argv[cursor_arg];
	if (want_value) {
	    fastf_t value;
	    ged_view_knob_validation_result(result,
		bu_cmd_number_from_str(&value, current) ? BU_CMD_VALIDATE_VALID :
		BU_CMD_VALIDATE_INVALID, cursor_arg, BU_CMD_VALUE_NUMBER,
		"finite numeric knob value required");
	    return 0;
	}
	if (ged_view_knob_action_is_reset(current) ||
	    BU_STR_EQUAL(current, "calibrate")) {
	    ged_view_knob_validation_result(result, BU_CMD_VALIDATE_VALID,
		cursor_arg, BU_CMD_VALUE_STRING, "knob action");
	    bu_cmd_keyword_candidates(result, ged_view_knob_actions, current);
	    return 0;
	}
	if (ged_view_knob_action_is_value(current)) {
	    ged_view_knob_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE,
		cursor_arg, BU_CMD_VALUE_NUMBER, "numeric knob value required");
	    return 0;
	}
	ged_view_knob_validation_result(result, BU_CMD_VALIDATE_INVALID,
	    cursor_arg, BU_CMD_VALUE_STRING, "known knob action required");
	bu_cmd_keyword_candidates(result, ged_view_knob_actions, current);
	return 0;
    }

    if (want_value) {
	ged_view_knob_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE,
	    cursor_arg, BU_CMD_VALUE_NUMBER, "numeric knob value required");
	return 0;
    }
    ged_view_knob_validation_result(result, BU_CMD_VALIDATE_VALID, cursor_arg,
	BU_CMD_VALUE_STRING, "knob action may follow");
    bu_cmd_keyword_candidates(result, ged_view_knob_actions, "");
    return 0;
}


static const char * const ged_view_knob_coord_options[] = {
    "view-coords", "model-coords", NULL
};
static const struct bu_cmd_constraint ged_view_knob_constraints[] = {
    BU_CMD_CONSTRAINT_OPTIONS(ged_view_knob_coord_options, 0, 1,
	"--view-coords and --model-coords cannot be used together"),
    BU_CMD_CONSTRAINT_NULL
};
static const struct bu_cmd_option ged_view_knob_options[] = {
    BU_CMD_FLAG("h", "help", struct ged_view_knob_args, print_help,
	"Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_FLAG("i", "incr", struct ged_view_knob_args, incr_flag,
	"Interpret values as increments"),
    BU_CMD_FLAG("v", "view-coords", struct ged_view_knob_args, view_flag,
	"Manipulate the view using view coordinates"),
    BU_CMD_FLAG("m", "model-coords", struct ged_view_knob_args, model_flag,
	"Manipulate the view using model coordinates"),
    BU_CMD_KEYWORD_VALUES("o", "origin", struct ged_view_knob_args, origin,
	"v|m|e|k", "Set rotation origin", ged_view_knob_origins),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand ged_view_knob_operands[] = {
    BU_CMD_OPERAND("action", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Knob action/value stream", NULL),
    BU_CMD_OPERAND_NULL
};
GED_EXPORT const struct bu_cmd_schema ged_view_knob_schema = {
    "knob", "Apply low-level view controls", ged_view_knob_options,
    ged_view_knob_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(ged_view_knob_validate, ged_view_knob_constraints)
};


static void
ged_view_knob_usage(struct ged *gedp)
{
    char *option_help = bu_cmd_schema_describe(&ged_view_knob_schema);

    bu_vls_printf(gedp->ged_result_str,
	"Usage: knob [options] [action [value] ...]\n"
	"Actions without values: zap, zero, clear, stop, calibrate\n"
	"Actions with a finite numeric value: x, y, z, X, Y, Z, S, ax, ay, az, aX, aY, aZ, aS\n");
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "Options:\n%s\n", option_help);
	bu_free(option_help, "view knob native option help");
    }
}

static void
print_knob_vals(struct bu_vls *o, struct bview *v)
{
    if (!o || !v)
	return;

    bu_vls_printf(o, "rate - rotation:\n");
    bu_vls_printf(o, " x = %f\n", v->k.rot_v[X]);
    bu_vls_printf(o, " y = %f\n", v->k.rot_v[Y]);
    bu_vls_printf(o, " z = %f\n", v->k.rot_v[Z]);
    bu_vls_printf(o, "rate - translation (skew):\n");
    bu_vls_printf(o, " X = %f\n", v->k.tra_v[X]);
    bu_vls_printf(o, " Y = %f\n", v->k.tra_v[Y]);
    bu_vls_printf(o, " Z = %f\n", v->k.tra_v[Z]);
    bu_vls_printf(o, "rate - scale:\n");
    bu_vls_printf(o, " S = %f\n", v->k.sca);

    bu_vls_printf(o, "absolute - rotation:\n");
    bu_vls_printf(o, " ax = %f\n", v->k.rot_v_abs[X]);
    bu_vls_printf(o, " ay = %f\n", v->k.rot_v_abs[Y]);
    bu_vls_printf(o, " az = %f\n", v->k.rot_v_abs[Z]);
    bu_vls_printf(o, "absolute - translation (skew):\n");
    bu_vls_printf(o, " aX = %f\n", v->k.tra_v_abs[X]);
    bu_vls_printf(o, " aY = %f\n", v->k.tra_v_abs[Y]);
    bu_vls_printf(o, " aZ = %f\n", v->k.tra_v_abs[Z]);
    bu_vls_printf(o, "absolute - scale:\n");
    bu_vls_printf(o, " aS = %f\n", v->gv_a_scale);
}

int
ged_knob_core(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_view_knob_args args = {0, 0, 0, 0, NULL};
    const char **pargv = NULL;
    int operand_index;
    int action_argc;
    const char **action_argv;

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    /* Make sure the view coordinate conversion values match the database */
    struct bview *v = gedp->ged_gvp;
    v->gv_local2base = (gedp->dbip) ? gedp->dbip->dbi_local2base : 1.0;
    v->gv_base2local = (gedp->dbip) ? gedp->dbip->dbi_base2local : 1.0;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    pargv = (const char **)bu_calloc((size_t)argc, sizeof(*pargv),
	"view knob native argv");
    memcpy(pargv, argv, (size_t)argc * sizeof(*pargv));
    operand_index = bu_cmd_schema_parse_complete(&ged_view_knob_schema, &args,
	gedp->ged_result_str, argc - 1, pargv + 1);
    if (operand_index < 0) {
	ged_view_knob_usage(gedp);
	bu_free((void *)pargv, "view knob native argv");
	return BRLCAD_ERROR;
    }
    action_argc = argc - 1 - operand_index;
    action_argv = pargv + 1 + operand_index;

    if (args.print_help) {
	ged_view_knob_usage(gedp);
	bu_free((void *)pargv, "view knob native argv");
	return BRLCAD_OK;
    }

    if (!action_argc) {
	// print current values
	print_knob_vals(gedp->ged_result_str, gedp->ged_gvp);
	bu_free((void *)pargv, "view knob native argv");
	return BRLCAD_OK;
    }

    // Make sure model flag is set, if that's what either the user
    // options or view settings indicate we should use
    if (!args.model_flag && !args.view_flag)
	args.model_flag = (v->gv_coord == 'm') ? 1 : 0;

    int do_tran = 0;
    int do_rot = 0;
    vect_t tvec;
    vect_t rvec;
    VSETALL(tvec, 0.0);
    VSETALL(rvec, 0.0);

    for (int ai = 0; ai < action_argc; ai++) {
	const char *cmd = action_argv[ai];

	if (ged_view_knob_action_is_reset(cmd)) {
	    // Per MGED, this command seems to reset the rate entries
	    // but not the absolute entries.
	    bv_knobs_reset(&v->k, BV_KNOBS_RATE);
	    continue;
	}
	if (BU_STR_EQUAL(cmd, "calibrate")) {
	    /* Reset BOTH model and view absolute translation baselines and their
	     * corresponding last arrays so switching coord systems after a
	     * calibrate does not introduce stale deltas. */
	    VSETALL(v->k.tra_v_abs, 0.0);
	    VSETALL(v->k.tra_v_abs_last, 0.0);
	    VSETALL(v->k.tra_m_abs, 0.0);
	    VSETALL(v->k.tra_m_abs_last, 0.0);
	    continue;
	}

	/* The schema has already enforced the action/value alternation.  Retain
	 * this defensive check so direct callers cannot reach libbv with bad data
	 * if the schema is changed in the future. */
	fastf_t f = 0.0;
	if (++ai >= action_argc || !bu_cmd_number_from_str(&f, action_argv[ai])) {
	    bu_vls_printf(gedp->ged_result_str, "finite numeric knob value required\n");
	    bu_free((void *)pargv, "view knob native argv");
	    return BRLCAD_ERROR;
	}

	// Process the actual command
	int kp_ret = bv_knobs_cmd_process(
		&rvec, &do_rot, &tvec, &do_tran,
		v, cmd, f,
		args.origin ? args.origin[0] : 'v', args.model_flag, args.incr_flag);

	if (kp_ret != BRLCAD_OK) {
	    ged_view_knob_usage(gedp);
	    bu_free((void *)pargv, "view knob native argv");
	    return BRLCAD_ERROR;
	}
    }

    if (do_tran) {
	bv_knobs_tran(v, tvec, args.model_flag);
    }

    if (do_rot) {
	// Note - we don't (currently) support 'o' coords here, so the obj_rot matrix is always NULL.
	char origin = args.origin ? args.origin[0] : 'v';
	bv_knobs_rot(v, rvec, origin, (args.model_flag ? 'm' : 'v'), NULL,
	    (origin == 'k') ? gedp->ged_gvp->gv_keypoint : NULL);
    }

    bv_update_rate_flags(v);

    bu_free((void *)pargv, "view knob native argv");
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

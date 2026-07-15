/*                          Q R A Y . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2026 United States Government as represented by
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
/** @addtogroup libged */

#include "common.h"

#include <string.h>


#include "bu/cmdschema.h"
#include "bu/color.h"
#include "vmath.h"
#include "ged.h"

#include "../qray.h"


static void qray_print_fmts(struct ged *gedp);
static void qray_print_vars(struct ged *gedp);
static int qray_get_fmt_index(struct ged *gedp, char c);
static int qray_execute(struct ged *gedp, int argc, const char *argv[]);


static void
qray_usage(struct ged *gedp, const char *argv0)
{
    bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", argv0);
    bu_vls_printf(gedp->ged_result_str, " qray vars			print a list of all variables (i.e. var = val)\n");
    bu_vls_printf(gedp->ged_result_str, " qray basename [str]		set or get basename for query ray primitives\n");
    bu_vls_printf(gedp->ged_result_str, " qray effects [t|g|b]		set or get effects (i.e. text, graphical or both)\n");
    bu_vls_printf(gedp->ged_result_str, " qray echo [0|1]		set or get command echo\n");
    bu_vls_printf(gedp->ged_result_str, " qray oddcolor [r g b]		set or get color of odd partitions\n");
    bu_vls_printf(gedp->ged_result_str, " qray evencolor [r g b]		set or get color of even partitions\n");
    bu_vls_printf(gedp->ged_result_str, " qray voidcolor [r g b]		set or get color of void areas\n");
    bu_vls_printf(gedp->ged_result_str, " qray overlapcolor [r g b]	set or get color of overlap areas\n");
    bu_vls_printf(gedp->ged_result_str, " qray fmt [r|h|p|f|m|o|g [str]]	set or get format string(s)\n");
    bu_vls_printf(gedp->ged_result_str, " qray script [str]		set or get the nirt script string\n");
    bu_vls_printf(gedp->ged_result_str, " qray [help]			print this help message\n");
}


static void
qray_print_fmts(struct ged *gedp)
{
    int i;

    for (i = 0; gedp->i->ged_gdp->gd_qray_fmts[i].type != (char)0; ++i)
	bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_addr(&gedp->i->ged_gdp->gd_qray_fmts[i].fmt));
}


static void
qray_print_vars(struct ged *gedp)
{
    bu_vls_printf(gedp->ged_result_str, "basename = %s\n", bu_vls_addr(&gedp->i->ged_gdp->gd_qray_basename));
    bu_vls_printf(gedp->ged_result_str, "script = %s\n", bu_vls_addr(&gedp->i->ged_gdp->gd_qray_script));
    bu_vls_printf(gedp->ged_result_str, "effects = %c\n", gedp->i->ged_gdp->gd_qray_effects);
    bu_vls_printf(gedp->ged_result_str, "echo = %d\n", gedp->i->ged_gdp->gd_qray_cmd_echo);
    bu_vls_printf(gedp->ged_result_str, "oddcolor = %d %d %d\n",
		  gedp->i->ged_gdp->gd_qray_odd_color.r, gedp->i->ged_gdp->gd_qray_odd_color.g, gedp->i->ged_gdp->gd_qray_odd_color.b);
    bu_vls_printf(gedp->ged_result_str, "evencolor = %d %d %d\n",
		  gedp->i->ged_gdp->gd_qray_even_color.r, gedp->i->ged_gdp->gd_qray_even_color.g, gedp->i->ged_gdp->gd_qray_even_color.b);
    bu_vls_printf(gedp->ged_result_str, "voidcolor = %d %d %d\n",
		  gedp->i->ged_gdp->gd_qray_void_color.r, gedp->i->ged_gdp->gd_qray_void_color.g, gedp->i->ged_gdp->gd_qray_void_color.b);
    bu_vls_printf(gedp->ged_result_str, "overlapcolor = %d %d %d\n",
		  gedp->i->ged_gdp->gd_qray_overlap_color.r, gedp->i->ged_gdp->gd_qray_overlap_color.g, gedp->i->ged_gdp->gd_qray_overlap_color.b);

    qray_print_fmts(gedp);
}


static int
qray_get_fmt_index(struct ged *gedp,
		   char c)
{
    int i;

    for (i = 0; gedp->i->ged_gdp->gd_qray_fmts[i].type != (char)0; ++i)
	if (c == gedp->i->ged_gdp->gd_qray_fmts[i].type)
	    return i;

    return -1;
}


static int
qray_color_command(struct ged *gedp, const char *name, struct ged_qray_color *color,
	int argc, const char *argv[])
{
    unsigned char rgb[3] = {0, 0, 0};

    if (!gedp || !name || !color || argc < 0 || (argc && !argv))
	return BRLCAD_ERROR;
    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "%d %d %d", color->r, color->g, color->b);
	return BRLCAD_OK;
    }
    if (bu_rgb_from_argv(rgb, (size_t)argc, (const char * const *)argv) != argc) {
	bu_vls_printf(gedp->ged_result_str, "qray %s - bad RGB value", name);
	return BRLCAD_ERROR;
    }
    color->r = rgb[RED];
    color->g = rgb[GRN];
    color->b = rgb[BLU];
    return BRLCAD_OK;
}


static int
qray_execute(struct ged *gedp,
	 int argc,
	 const char *argv[])
{
    if (!gedp || argc <= 0 || !argv)
	return BRLCAD_ERROR;
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	qray_usage(gedp, argv[0]);
	return GED_HELP;
    }

    /* catch bug introduced pre 7.26.0 where .mgedrc ends up with qray
     * lines containing "A database is not open!".  we detect to
     * report a more meaningful error message.
     */
    if ((argc >= 4
	 && BU_STR_EQUAL(argv[3], "A database is not open!"))
	|| (argc == 7
	    && BU_STR_EQUAL(argv[2], "A")
	    && BU_STR_EQUAL(argv[3], "database")
	    && BU_STR_EQUAL(argv[4], "is")
	    && BU_STR_EQUAL(argv[5], "not")
	    && BU_STR_EQUAL(argv[6], "open!"))) {
	bu_vls_printf(gedp->ged_result_str, "WARNING: Corrupt qray line encountered.\n");
	return BRLCAD_ERROR;
    }

    if (argc > 6) {
	qray_usage(gedp, argv[0]);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "fmt")) {
	int i;

	if (argc == 2) {
	    /* get all format strings */
	    qray_print_fmts(gedp);
	    return BRLCAD_OK;
	} else if (argc == 3) {
	    /* get particular format string */
	    if ((i = qray_get_fmt_index(gedp, *argv[2])) < 0) {
		bu_vls_printf(gedp->ged_result_str, "qray: unrecognized format type: '%s'\n", argv[2]);
		qray_usage(gedp, argv[0]);

		return BRLCAD_ERROR;
	    }

	    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&gedp->i->ged_gdp->gd_qray_fmts[i].fmt));
	    return BRLCAD_OK;
	} else if (argc == 4) {
	    /* set value */
	    if ((i = qray_get_fmt_index(gedp, *argv[2])) < 0) {
		bu_vls_printf(gedp->ged_result_str, "qray: unrecognized format type: '%s'\n", argv[2]);
		qray_usage(gedp, argv[0]);

		return BRLCAD_ERROR;
	    }

	    bu_vls_trunc(&gedp->i->ged_gdp->gd_qray_fmts[i].fmt, 0);
	    bu_vls_printf(&gedp->i->ged_gdp->gd_qray_fmts[i].fmt, "%s", argv[3]);
	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'qray fmt' command accepts 0, 1 or 2 arguments\n");
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "basename")) {
	if (argc == 2) {
	    /* get value */
	    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&gedp->i->ged_gdp->gd_qray_basename));

	    return BRLCAD_OK;
	} else if (argc == 3) {
	    /* set value */
	    bu_vls_strcpy(&gedp->i->ged_gdp->gd_qray_basename, argv[2]);
	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'qray basename' command accepts 0 or 1 argument\n");
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "script")) {
	if (argc == 2) {
	    /* get value */
	    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&gedp->i->ged_gdp->gd_qray_script));

	    return BRLCAD_OK;
	} else if (argc == 3) {
	    /* set value */
	    bu_vls_strcpy(&gedp->i->ged_gdp->gd_qray_script, argv[2]);
	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'qray scripts' command accepts 0 or 1 argument\n");
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "effects")) {
	if (argc == 2) {
	    /* get value */
	    bu_vls_printf(gedp->ged_result_str, "%c", gedp->i->ged_gdp->gd_qray_effects);

	    return BRLCAD_OK;
	} else if (argc == 3) {
	    /* set value */
	    if (*argv[2] != 't' && *argv[2] != 'g' && *argv[2] != 'b') {
		bu_vls_printf(gedp->ged_result_str, "qray effects: bad value - %s", argv[2]);

		return BRLCAD_ERROR;
	    }

	    gedp->i->ged_gdp->gd_qray_effects = *argv[2];

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'qray effects' command accepts 0 or 1 argument\n");
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "echo")) {
	if (argc == 2) {
	    /* get value */
	    if (gedp->i->ged_gdp->gd_qray_cmd_echo)
		bu_vls_printf(gedp->ged_result_str, "1");
	    else
		bu_vls_printf(gedp->ged_result_str, "0");

	    return BRLCAD_OK;
	} else if (argc == 3) {
	    /* set value */
	    int ival;

	    if (sscanf(argv[2], "%d", &ival) < 1) {
		bu_vls_printf(gedp->ged_result_str, "qray echo: bad value - %s", argv[2]);

		return BRLCAD_ERROR;
	    }

	    if (ival)
		gedp->i->ged_gdp->gd_qray_cmd_echo = 1;
	    else
		gedp->i->ged_gdp->gd_qray_cmd_echo = 0;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'qray echo' command accepts 0 or 1 argument\n");
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "oddcolor")) {
	return qray_color_command(gedp, "oddcolor", &gedp->i->ged_gdp->gd_qray_odd_color,
	    argc - 2, argv + 2);
    }

    if (BU_STR_EQUAL(argv[1], "evencolor")) {
	return qray_color_command(gedp, "evencolor", &gedp->i->ged_gdp->gd_qray_even_color,
	    argc - 2, argv + 2);
    }

    if (BU_STR_EQUAL(argv[1], "voidcolor")) {
	return qray_color_command(gedp, "voidcolor", &gedp->i->ged_gdp->gd_qray_void_color,
	    argc - 2, argv + 2);
    }

    if (BU_STR_EQUAL(argv[1], "overlapcolor")) {
	return qray_color_command(gedp, "overlapcolor", &gedp->i->ged_gdp->gd_qray_overlap_color,
	    argc - 2, argv + 2);
    }

    if (BU_STR_EQUAL(argv[1], "vars")) {
	qray_print_vars(gedp);
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[1], "help")) {
	qray_usage(gedp, argv[0]);
	return GED_HELP;
    }

    bu_vls_printf(gedp->ged_result_str, "qray: unrecognized command: '%s'\n", argv[1]);
    qray_usage(gedp, argv[0]);

    return BRLCAD_ERROR;
}

/** @} */


#include "../include/plugin.h"

static const struct bu_cmd_operand qray_string_operands[] = {
    BU_CMD_OPERAND("value", BU_CMD_VALUE_STRING, 0, 1, "Optional new value", NULL),
    BU_CMD_OPERAND_NULL
};
static const char * const qray_effect_keywords[] = {"t", "g", "b", NULL};
static const struct bu_cmd_operand qray_effect_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("effects", BU_CMD_VALUE_KEYWORD, 0, 1,
	"Text, graphical, or both", NULL, qray_effect_keywords),
    BU_CMD_OPERAND_NULL
};
static const char * const qray_bool_keywords[] = {"0", "1", NULL};
static const struct bu_cmd_operand qray_bool_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("enabled", BU_CMD_VALUE_KEYWORD, 0, 1,
	"Boolean setting", NULL, qray_bool_keywords),
    BU_CMD_OPERAND_NULL
};
static int
qray_color_schema_validate(const struct bu_cmd_schema *UNUSED(schema), size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    return bu_cmd_rgb_optional_validate(argc, argv, cursor_arg, result);
}
static const struct bu_cmd_operand qray_color_operands[] = {
    BU_CMD_OPERAND_SHAPED("color", BU_CMD_VALUE_COLOR, 0, 3, NULL,
	"Optional packed or red, green, and blue components", NULL, &bu_cmd_rgb_arg_shape),
    BU_CMD_OPERAND_NULL
};
static const char * const qray_fmt_keywords[] = {"r", "h", "p", "f", "m", "o", "g", NULL};
static const struct bu_cmd_operand qray_fmt_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("format_type", BU_CMD_VALUE_KEYWORD, 0, 1,
	"Ray format type", NULL, qray_fmt_keywords),
    BU_CMD_OPERAND("format", BU_CMD_VALUE_STRING, 0, 1, "Optional format string", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema qray_root_schema = {
    "qray", "Configure query-ray reporting", NULL, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema qray_vars_schema = {
    "vars", "Print all qray variables", NULL, NULL,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema qray_help_schema = {
    "help", "Print qray help", NULL, NULL,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
#define QRAY_SCHEMA(_id, _name, _help, _operands, _validation) \
    static const struct bu_cmd_schema _id##_schema = { \
	_name, _help, NULL, _operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, _validation \
    }
QRAY_SCHEMA(qray_basename, "basename", "Query or set the result basename", qray_string_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
QRAY_SCHEMA(qray_script, "script", "Query or set the nirt script", qray_string_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
QRAY_SCHEMA(qray_effects, "effects", "Query or set ray effects", qray_effect_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
QRAY_SCHEMA(qray_echo, "echo", "Query or set command echo", qray_bool_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
QRAY_SCHEMA(qray_oddcolor, "oddcolor", "Query or set odd-partition color", qray_color_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(qray_color_schema_validate, NULL));
QRAY_SCHEMA(qray_evencolor, "evencolor", "Query or set even-partition color", qray_color_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(qray_color_schema_validate, NULL));
QRAY_SCHEMA(qray_voidcolor, "voidcolor", "Query or set void color", qray_color_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(qray_color_schema_validate, NULL));
QRAY_SCHEMA(qray_overlapcolor, "overlapcolor", "Query or set overlap color", qray_color_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(qray_color_schema_validate, NULL));
QRAY_SCHEMA(qray_fmt, "fmt", "Query or set ray format strings", qray_fmt_operands,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL));
#undef QRAY_SCHEMA


static int
qray_tree_execute(void *data, int argc, const char *argv[])
{
    const char **full_argv = NULL;
    int ret = BRLCAD_ERROR;

    if (!data || argc < 1 || !argv)
	return BRLCAD_ERROR;
    full_argv = (const char **)bu_calloc((size_t)argc + 1, sizeof(*full_argv),
	"qray native tree argv");
    full_argv[0] = "qray";
    for (int i = 0; i < argc; i++)
	full_argv[i + 1] = argv[i];
    ret = qray_execute((struct ged *)data, argc + 1, full_argv);
    bu_free((void *)full_argv, "qray native tree argv");
    return ret;
}


static const struct bu_cmd_tree_node qray_subcommands[] = {
    BU_CMD_TREE_NODE(&qray_vars_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, qray_tree_execute),
    BU_CMD_TREE_NODE(&qray_help_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, qray_tree_execute),
    BU_CMD_TREE_NODE(&qray_basename_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, qray_tree_execute),
    BU_CMD_TREE_NODE(&qray_script_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, qray_tree_execute),
    BU_CMD_TREE_NODE(&qray_effects_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, qray_tree_execute),
    BU_CMD_TREE_NODE(&qray_echo_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, qray_tree_execute),
    BU_CMD_TREE_NODE(&qray_oddcolor_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, qray_tree_execute),
    BU_CMD_TREE_NODE(&qray_evencolor_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, qray_tree_execute),
    BU_CMD_TREE_NODE(&qray_voidcolor_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, qray_tree_execute),
    BU_CMD_TREE_NODE(&qray_overlapcolor_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, qray_tree_execute),
    BU_CMD_TREE_NODE(&qray_fmt_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, qray_tree_execute),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree ged_qray_tree = {
    &qray_root_schema, qray_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};


int
ged_qray_core(struct ged *gedp, int argc, const char *argv[])
{
    const struct bu_cmd_tree_node *node = NULL;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int ret = BRLCAD_ERROR;

    if (!gedp || argc <= 0 || !argv)
	return BRLCAD_ERROR;
    if (argc == 1)
	return qray_execute(gedp, argc, argv);
    /* Preserve the historical corrupt-.mgedrc diagnosis before ordinary
     * syntax validation sees the malformed words. */
    if ((argc >= 4 && BU_STR_EQUAL(argv[3], "A database is not open!")) ||
	(argc == 7 && BU_STR_EQUAL(argv[2], "A") && BU_STR_EQUAL(argv[3], "database") &&
	 BU_STR_EQUAL(argv[4], "is") && BU_STR_EQUAL(argv[5], "not") && BU_STR_EQUAL(argv[6], "open!")))
	return qray_execute(gedp, argc, argv);
    node = bu_cmd_tree_find_subcommand(&ged_qray_tree, argv[1]);
    if (!node)
	return qray_execute(gedp, argc, argv);
    bu_vls_trunc(gedp->ged_result_str, 0);
    if (bu_cmd_schema_parse_complete(node->schema, NULL, &msg, argc - 2, argv + 2) < 0) {
	if (bu_vls_strlen(&msg))
	    bu_vls_vlscat(gedp->ged_result_str, &msg);
	qray_usage(gedp, argv[0]);
	bu_vls_free(&msg);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&msg);
    if (bu_cmd_tree_dispatch(&ged_qray_tree, gedp, argc - 1, argv + 1, &ret) == 0)
	return ret;
    return BRLCAD_ERROR;
}


static int
ged_qray_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &ged_qray_tree, input, cursor_pos, result);
}


static int
ged_qray_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &ged_qray_tree, input, analysis);
}


static char *
ged_qray_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&ged_qray_tree);
}


static int
ged_qray_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&ged_qray_tree, msgs);
}


static const struct ged_cmd_grammar ged_qray_grammar = {
    "qray", "Configure query-ray reporting", ged_qray_grammar_validate,
    ged_qray_grammar_analyze, ged_qray_grammar_json, ged_qray_grammar_lint
};

#define GED_QRAY_COMMANDS(X, XID) \
    X(qray, ged_qray_core, GED_CMD_DEFAULT, &ged_qray_grammar) \

GED_DECLARE_COMMAND_SET_WITH_GRAMMAR(GED_QRAY_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_GRAMMAR("libged_qray", 1, GED_QRAY_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

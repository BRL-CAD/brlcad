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

#include "ged.h"
#include "bu/cmdschema.h"

#include "../ged_private.h"
#include "who_solids.h"

extern int ged_who2_core(struct ged *gedp, int argc, const char **argv);
extern int ged_who_solids_core(struct ged *gedp, int argc, const char **argv);
extern const struct bu_cmd_schema ged_who_new_schema;

struct who_legacy_args {
    int print_help;
};

static const char * const who_real_aliases[] = {"r", NULL};
static const char * const who_phony_aliases[] = {"p", NULL};
static const char * const who_both_aliases[] = {"b", NULL};
static const struct bu_cmd_value_keyword who_display_kinds[] = {
    {"real", who_real_aliases, "List real database objects"},
    {"phony", who_phony_aliases, "List phony display objects"},
    {"both", who_both_aliases, "List real and phony objects"},
    {NULL, NULL, NULL}
};
static const struct bu_cmd_option who_legacy_options[] = {
    BU_CMD_FLAG("h", "help", struct who_legacy_args, print_help,
	"Print command help"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand who_legacy_operands[] = {
    BU_CMD_OPERAND_KEYWORD_VALUES("display_kind", BU_CMD_VALUE_KEYWORD, 0, 1,
	"real, phony, or both", NULL, who_display_kinds),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand who_solids_operands[] = {
    BU_CMD_OPERAND("detail_level", BU_CMD_VALUE_INTEGER, 0, 1,
	"Report detail level", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_option who_legacy_solids_options[] = {
    BU_CMD_FLAG("h", "help", struct ged_solid_report_args, print_help,
	"Print command help"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema who_legacy_schema = {
    "who", "List top-level displayed objects", who_legacy_options,
    who_legacy_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema who_legacy_solids_schema = {
    "solids", "Report displayed solid internals", who_legacy_solids_options,
    who_solids_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema who_tree_root_schema = {
    "who", "List objects currently displayed", NULL, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const char * const who_report_aliases[] = {"report", NULL};
static const struct bu_cmd_tree_node who_legacy_subcommands[] = {
    BU_CMD_TREE_NODE(&who_legacy_solids_schema, who_report_aliases, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree_node who_new_subcommands[] = {
    BU_CMD_TREE_NODE(&ged_who_solids_schema, who_report_aliases, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree who_legacy_tree = {
    &who_tree_root_schema, who_legacy_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};
static const struct bu_cmd_tree who_new_tree = {
    &who_tree_root_schema, who_new_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};
static const struct ged_cmd_native_form ged_who_native_forms[] = {
    {"legacy", &who_legacy_schema, NULL},
    {"legacy_solids", NULL, &who_legacy_tree},
    {"new", &ged_who_new_schema, NULL},
    {"new_solids", NULL, &who_new_tree},
    {NULL, NULL, NULL}
};

static int
who_solids_input(size_t argc, const char * const *argv)
{
    const char *arg;
    size_t len;

    if (argc < 2 || !argv || !argv[1])
	return 0;
    arg = argv[1];
    if (BU_STR_EQUAL(arg, "solids") || BU_STR_EQUAL(arg, "report"))
	return 1;
    len = strlen(arg);
    if (!len)
	return 0;
    if (arg[0] == 's' && strncmp("solids", arg, len) == 0)
	return 1;
    return len > 1 && strncmp("report", arg, len) == 0;
}

static const struct ged_cmd_native_form *
ged_who_select_native_form(const struct ged *gedp, size_t argc,
	const char * const *argv)
{
    int solids = who_solids_input(argc, argv);
    int new_form = gedp && gedp->new_cmd_forms;

    if (new_form)
	return solids ? &ged_who_native_forms[3] : &ged_who_native_forms[2];
    return solids ? &ged_who_native_forms[1] : &ged_who_native_forms[0];
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
    static const char *usage =
	"Usage:\n"
	"  who [real|phony|both]\n"
	"  who solids [level]";

    struct who_legacy_args args = {0};
    int operand_index;

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    if (argc > 1 && (BU_STR_EQUAL(argv[1], "solids") || BU_STR_EQUAL(argv[1], "report")))
	return ged_who_solids_core(gedp, argc, argv);

    if (gedp->new_cmd_forms)
	return ged_who2_core(gedp, argc, argv);

    struct display_list *gdlp;
    int skip_real, skip_phony;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    operand_index = bu_cmd_schema_parse_complete(&who_legacy_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", usage);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bu_vls_printf(gedp->ged_result_str, "%s", usage);
	return GED_HELP;
    }

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);

    skip_real = 0;
    skip_phony = 1;
    if (argc == 2) {
	switch (argv[1][0]) {
	    case 'b':
		skip_real = 0;
		skip_phony = 0;
		break;
	    case 'p':
		skip_real = 1;
		skip_phony = 0;
		break;
	    case 'r':
		skip_real = 0;
		skip_phony = 1;
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "%s", usage);
		return BRLCAD_ERROR;
	}
    }

    for (BU_LIST_FOR(gdlp, display_list, (struct bu_list *)ged_dl(gedp))) {
	if (((struct directory *)gdlp->dl_dp)->d_addr == RT_DIR_PHONY_ADDR) {
	    if (skip_phony) continue;
	} else {
	    if (skip_real) continue;
	}

	bu_vls_printf(gedp->ged_result_str, "%s ", bu_vls_addr(&gdlp->dl_path));
    }

    return BRLCAD_OK;
}


#include "../include/plugin.h"

static int
ged_who_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_native_forms_validate(gedp, ged_who_native_forms,
	ged_who_select_native_form, input, cursor_pos, result);
}

static int
ged_who_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_native_forms_analyze(gedp, ged_who_native_forms,
	ged_who_select_native_form, input, analysis);
}

static char *
ged_who_grammar_json(void)
{
    return ged_cmd_native_forms_describe_json("who", "List objects currently displayed",
	ged_who_native_forms);
}

static int
ged_who_grammar_lint(struct bu_vls *msgs)
{
    return ged_cmd_native_forms_lint("who", ged_who_native_forms, msgs);
}

static const struct ged_cmd_grammar ged_who_grammar = {
    "who", "List objects currently displayed", ged_who_grammar_validate,
    ged_who_grammar_analyze, ged_who_grammar_json, ged_who_grammar_lint
};

#define GED_WHO_COMMANDS(X, XID, N, NID, G, GID) \
    G(who, ged_who_core, GED_CMD_DEFAULT, &ged_who_grammar) \

GED_DECLARE_COMMAND_SET_WITH_MIXED_SCHEMA(GED_WHO_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_MIXED_SCHEMA("libged_who", 1, GED_WHO_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

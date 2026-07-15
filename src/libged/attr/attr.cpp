/*                       A T T R . C P P
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
/** @file libged/attr.cpp
 *
 * The attr command.
 *
 */

#include "common.h"

#include "bu/cmdschema.h"

#include <utility>
#include <string>
#include <set>
#include <cstdlib>

#include "rt/cmd.h"
#include "ged/database.h"

extern "C" int
ged_attr_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "{[-c sep_char] set|get|show|rm|append|sort|list|copy|standardize} object [key [value] ... ]";
    const char *cmd_name = argv[0];

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return GED_HELP;
    }

    /* Verify that this wdb supports lookup operations
       (non-null dbip) */
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);

    int ret = rt_cmd_attr(gedp->ged_result_str, gedp->dbip, argc, argv);

    if (ret & GED_HELP) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
    }

    if (ret & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    return ret;
}

#include "../include/plugin.h"

static const struct bu_cmd_option attr_schema_options[] = {
    BU_CMD_VALUE_UNBOUND("c", NULL, "c", BU_CMD_VALUE_INTEGER, "sep_char",
	"Separate output fields with this character"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand attr_object_operands[] = {
    BU_CMD_OPERAND("object_pattern", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Object name or pattern", "ged.db_object"),
    BU_CMD_OPERAND("attributes", BU_CMD_VALUE_STRING, 0, BU_CMD_COUNT_UNLIMITED,
	"Attribute names or name/value pairs", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand attr_copy_operands[] = {
    BU_CMD_OPERAND("object_pattern", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Object name or pattern", "ged.db_object"),
    BU_CMD_OPERAND("source_attribute", BU_CMD_VALUE_STRING, 1, 1,
	"Attribute to copy", NULL),
    BU_CMD_OPERAND("destination_attribute", BU_CMD_VALUE_STRING, 1, 1,
	"New attribute name", NULL),
    BU_CMD_OPERAND_NULL
};
static const char * const attr_sort_keywords[] = {"case", "nocase", "value", "value-nocase", NULL};
static const struct bu_cmd_operand attr_sort_operands[] = {
    BU_CMD_OPERAND("object_pattern", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Object name or pattern", "ged.db_object"),
    BU_CMD_OPERAND_KEYWORDS("sort_order", BU_CMD_VALUE_KEYWORD, 0, 1,
	"Sort order", NULL, attr_sort_keywords),
    BU_CMD_OPERAND_NULL
};
#define ATTR_SCHEMA(_id, _name, _help, _ops) \
    static const struct bu_cmd_schema _id##_schema = { \
	_name, _help, NULL, _ops, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL} \
    }
ATTR_SCHEMA(attr_set, "set", "Set attribute/value pairs", attr_object_operands);
ATTR_SCHEMA(attr_get, "get", "Get attribute values", attr_object_operands);
ATTR_SCHEMA(attr_show, "show", "Show attributes", attr_object_operands);
ATTR_SCHEMA(attr_rm, "rm", "Remove attributes", attr_object_operands);
ATTR_SCHEMA(attr_append, "append", "Append to attribute values", attr_object_operands);
ATTR_SCHEMA(attr_sort, "sort", "Sort attributes", attr_sort_operands);
ATTR_SCHEMA(attr_list, "list", "List standard attributes", attr_object_operands);
ATTR_SCHEMA(attr_copy, "copy", "Copy an attribute", attr_copy_operands);
ATTR_SCHEMA(attr_standardize, "standardize", "Standardize attribute names", attr_object_operands);
#undef ATTR_SCHEMA
static const struct bu_cmd_schema attr_root_schema = {
    "attr", "Inspect or modify database attributes", attr_schema_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};
static const struct bu_cmd_tree_node attr_subcommands[] = {
    BU_CMD_TREE_NODE(&attr_set_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&attr_get_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&attr_show_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&attr_rm_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&attr_append_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&attr_sort_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&attr_list_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&attr_copy_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&attr_standardize_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree attr_tree = {
    &attr_root_schema, attr_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};

static int
attr_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &attr_tree, input, cursor_pos, result);
}

static int
attr_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &attr_tree, input, analysis);
}

static char *
attr_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&attr_tree);
}

static int
attr_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&attr_tree, msgs);
}

static const struct ged_cmd_grammar attr_grammar = {
    "attr", "Inspect or modify database attributes", attr_grammar_validate,
    attr_grammar_analyze, attr_grammar_json, attr_grammar_lint
};

#define GED_ATTR_COMMANDS(X, XID, NX, NXID, GX, GXID) \
    GX(attr, ged_attr_core, GED_CMD_DEFAULT, &attr_grammar) \

GED_DECLARE_COMMAND_SET_WITH_MIXED_SCHEMA(GED_ATTR_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_MIXED_SCHEMA("libged_attr", 1, GED_ATTR_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

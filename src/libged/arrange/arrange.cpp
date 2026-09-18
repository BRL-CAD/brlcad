/*                         A R R A N G E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file libged/arrange/arrange.cpp
 *
 * Dispatch for object arrangement styles.
 */

#include "common.h"

#include <cstring>

#include "bu/cmdschema.h"

#include "arrange_private.h"
#include "../ged_private.h"

namespace {

using StyleHandler = int (*)(struct ged *, int, const char *[]);

struct StyleEntry {
    const char *name;
    StyleHandler handler;
    const char *summary;
};

const StyleEntry styles[] = {
    {"nest", arrange::ged_arrange_nest,
        "pack arbitrary 2D silhouettes or 3D objects into a container"}
};

void
print_help(struct ged *gedp)
{
    bu_vls_printf(gedp->ged_result_str,
        "Usage: arrange <style> [style-options] ...\n"
        "       arrange help [style]\n\n"
        "Available styles:\n");
    for (const StyleEntry &style : styles)
        bu_vls_printf(gedp->ged_result_str, "  %-8s %s\n", style.name,
            style.summary);
    bu_vls_printf(gedp->ged_result_str,
        "\nPlanned style names include grid, hex, tri, row, col, polar, and spiral.\n");
}

} // namespace

extern "C" int
ged_arrange_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
        print_help(gedp);
        return GED_HELP;
    }

    const char *style_name = argv[1];
    if (BU_STR_EQUAL(style_name, "help")) {
        if (argc == 2) {
            print_help(gedp);
            return GED_HELP;
        }
        style_name = argv[2];
        for (const StyleEntry &style : styles) {
            if (BU_STR_EQUAL(style_name, style.name)) {
                const char *help_argv[] = {style.name, "--help"};
                return style.handler(gedp, 2, help_argv);
            }
        }
        bu_vls_printf(gedp->ged_result_str,
            "arrange: unknown style '%s'\n", style_name);
        print_help(gedp);
        return BRLCAD_ERROR;
    }

    for (const StyleEntry &style : styles) {
        if (BU_STR_EQUAL(style_name, style.name))
            return style.handler(gedp, argc - 1, argv + 1);
    }

    bu_vls_printf(gedp->ged_result_str,
        "arrange: unknown style '%s'\n", style_name);
    print_help(gedp);
    return BRLCAD_ERROR;
}

#include "../include/plugin.h"

static const struct bu_cmd_schema arrange_root_schema =
    BU_CMD_SCHEMA_BOUND("arrange", "Arrange objects by style", NULL, NULL,
        BU_CMD_PARSE_OPTIONS_FIRST, NULL, NULL, NULL, NULL);
static const struct bu_cmd_tree_node arrange_nodes[] = {
    BU_CMD_TREE_NODE(&arrange::ged_arrange_nest_schema, NULL, NULL,
        BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree arrange_tree =
    BU_CMD_TREE(&arrange_root_schema, arrange_nodes, BU_CMD_TREE_CHILD_AFTER_OPTIONS);

static int
arrange_grammar_validate(struct ged *gedp, const char *input, size_t cursor,
    struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &arrange_tree, input, cursor, result);
}
static int
arrange_grammar_analyze(struct ged *gedp, const char *input,
    struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &arrange_tree, input, analysis);
}
static char *
arrange_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&arrange_tree);
}
static int
arrange_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&arrange_tree, msgs);
}
GED_CMD_TREE_HELP(arrange_grammar_help, arrange_tree)
static const struct ged_cmd_grammar arrange_grammar = {
    "arrange", "Arrange objects by style", arrange_grammar_validate,
    arrange_grammar_analyze, arrange_grammar_json, arrange_grammar_lint, NULL,
    arrange_grammar_help
};

#define GED_ARRANGE_COMMANDS(X, XID) \
    X(arrange, ged_arrange_core, GED_CMD_DEFAULT, &arrange_grammar)

GED_DECLARE_COMMAND_SET_WITH_GRAMMAR(GED_ARRANGE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_GRAMMAR("libged_arrange", 1, GED_ARRANGE_COMMANDS)

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */


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

#define GED_ARRANGE_COMMANDS(X, XID) \
    X(arrange, ged_arrange_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_ARRANGE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_arrange", 1, GED_ARRANGE_COMMANDS)

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


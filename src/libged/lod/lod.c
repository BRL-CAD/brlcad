/*                           L O D . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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
/** @file lod.c
 *
 * Level of Detail drawing configuration command.
 *
 */

#include "common.h"

#include "../ged_private.h"

int
ged_lod_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    return ged_view_lod_core(gedp, gedp->ged_gvp, argc, argv);
}


static const struct ged_cmd_native_form ged_lod_native_forms[] = {
    {"action_grammar", &ged_view_lod_schema, NULL},
    {NULL, NULL, NULL}
};


static const struct ged_cmd_native_form *
ged_lod_select_native_form(const struct ged *UNUSED(gedp), size_t UNUSED(argc),
	const char * const *UNUSED(argv))
{
    return &ged_lod_native_forms[0];
}


static int
ged_lod_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_native_forms_validate(gedp, ged_lod_native_forms,
	ged_lod_select_native_form, input, cursor_pos, result);
}


static int
ged_lod_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_native_forms_analyze(gedp, ged_lod_native_forms,
	ged_lod_select_native_form, input, analysis);
}


static int
ged_lod_grammar_lint(struct bu_vls *msgs)
{
    return ged_cmd_native_forms_lint("lod", ged_lod_native_forms, msgs);
}


static const struct ged_cmd_grammar ged_lod_grammar = {
    "lod", "Manage level-of-detail drawing settings", ged_lod_grammar_validate,
    ged_lod_grammar_analyze, ged_view_lod_grammar_json, ged_lod_grammar_lint
};


#include "../include/plugin.h"

#define GED_LOD_COMMANDS(X, XID, N, NID, G, GID) \
    G(lod, ged_lod_core, GED_CMD_DEFAULT, &ged_lod_grammar) \

GED_DECLARE_COMMAND_SET_WITH_MIXED_SCHEMA(GED_LOD_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_MIXED_SCHEMA("libged_lod", 1, GED_LOD_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

/*             E R A S E _ C O M P L E T I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */
/** @file erase_completion.cpp
 *
 * Command-owned completion grammar for erase.
 */
#include "common.h"
#include "bu/cmdschema.h"
#include "ged.h"
#include "../ged_private.h"

static const struct bu_cmd_form erase_form_rows[] = {
    BU_CMD_FORM_SCHEMA("legacy", "Legacy erase behavior", &ged_erase_legacy_schema),
    BU_CMD_FORM_SCHEMA("current", "Current erase behavior", &ged_erase_new_schema),
    BU_CMD_FORM_NULL
};

static const struct bu_cmd_form *
ged_erase_form_select(const struct bu_cmd_forms *forms, size_t UNUSED(argc),
	const char * const *UNUSED(argv), void *context)
{
    const struct ged *gedp = (const struct ged *)context;
    return gedp && gedp->new_cmd_forms ? &forms->forms[1] : &forms->forms[0];
}

static const struct bu_cmd_forms erase_forms =
    BU_CMD_FORMS("erase", "Erase database paths from the display",
	erase_form_rows, ged_erase_form_select);

static int
ged_erase_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_native_forms_validate(gedp, &erase_forms, input,
	cursor_pos, result);
}

static int
ged_erase_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_native_forms_analyze(gedp, &erase_forms, input, analysis);
}

static char *
ged_erase_grammar_json(void)
{
    return ged_cmd_native_forms_describe_json(&erase_forms);
}

static int
ged_erase_grammar_lint(struct bu_vls *msgs)
{
    return ged_cmd_native_forms_lint(&erase_forms, msgs);
}

static char *
ged_erase_grammar_help(const char *invocation)
{
    return ged_cmd_native_forms_help(&erase_forms, invocation);
}

extern "C" GED_EXPORT const struct ged_cmd_grammar ged_erase_grammar = {
    "erase", "Erase database paths from the display", ged_erase_grammar_validate,
    ged_erase_grammar_analyze, ged_erase_grammar_json, ged_erase_grammar_lint,
    NULL, ged_erase_grammar_help
};

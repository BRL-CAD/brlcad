/*            S E L E C T _ C O M P L E T I O N . C P P
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
/** @file select_completion.cpp
 *
 * Command-owned completion grammar for select and rselect.
 */
#include "common.h"
#include "bu/cmdschema.h"
#include "ged.h"
#include "../ged_private.h"

static const struct bu_cmd_form select_form_rows[] = {
    BU_CMD_FORM_SCHEMA("legacy", "Legacy screen-rectangle selection",
	&ged_select_legacy_schema),
    BU_CMD_FORM_TREE("current", "Selection-set subcommands", &ged_select_new_tree),
    BU_CMD_FORM_NULL
};

static const struct bu_cmd_form rselect_form_rows[] = {
    BU_CMD_FORM_SCHEMA("legacy", "Select from the current rectangle",
	&ged_rselect_legacy_schema),
    BU_CMD_FORM_NULL
};

static const struct bu_cmd_form *
ged_select_form_select(const struct bu_cmd_forms *forms, size_t UNUSED(argc),
	const char * const *UNUSED(argv), void *context)
{
    const struct ged *gedp = (const struct ged *)context;
    return gedp && gedp->new_cmd_forms ? &forms->forms[1] : &forms->forms[0];
}

static const struct bu_cmd_form *
ged_rselect_form_select(const struct bu_cmd_forms *forms, size_t UNUSED(argc),
	const char * const *UNUSED(argv), void *UNUSED(context))
{
    return &forms->forms[0];
}

static const struct bu_cmd_forms select_forms =
    BU_CMD_FORMS("select", "Manage display selection and selection sets",
	select_form_rows, ged_select_form_select);
static const struct bu_cmd_forms rselect_forms =
    BU_CMD_FORMS("rselect", "Select from the current screen rectangle",
	rselect_form_rows, ged_rselect_form_select);

static int
ged_select_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_native_forms_validate(gedp, &select_forms, input,
	cursor_pos, result);
}

static int
ged_select_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_native_forms_analyze(gedp, &select_forms, input, analysis);
}

static char *
ged_select_grammar_json(void)
{
    return ged_cmd_native_forms_describe_json(&select_forms);
}

static int
ged_select_grammar_lint(struct bu_vls *msgs)
{
    return ged_cmd_native_forms_lint(&select_forms, msgs);
}

static char *
ged_select_grammar_help(const char *invocation)
{
    return ged_cmd_native_forms_help(&select_forms, invocation);
}

static int
ged_rselect_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_native_forms_validate(gedp, &rselect_forms, input,
	cursor_pos, result);
}

static int
ged_rselect_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_native_forms_analyze(gedp, &rselect_forms, input, analysis);
}

static char *
ged_rselect_grammar_json(void)
{
    return ged_cmd_native_forms_describe_json(&rselect_forms);
}

static int
ged_rselect_grammar_lint(struct bu_vls *msgs)
{
    return ged_cmd_native_forms_lint(&rselect_forms, msgs);
}

static char *
ged_rselect_grammar_help(const char *invocation)
{
    return ged_cmd_native_forms_help(&rselect_forms, invocation);
}

extern "C" GED_EXPORT const struct ged_cmd_grammar ged_select_grammar = {
    "select", "Manage display selection and selection sets", ged_select_grammar_validate,
    ged_select_grammar_analyze, ged_select_grammar_json, ged_select_grammar_lint,
    NULL, ged_select_grammar_help
};

extern "C" GED_EXPORT const struct ged_cmd_grammar ged_rselect_grammar = {
    "rselect", "Select using the current rubber-band region", ged_rselect_grammar_validate,
    ged_rselect_grammar_analyze, ged_rselect_grammar_json,
    ged_rselect_grammar_lint, NULL, ged_rselect_grammar_help
};

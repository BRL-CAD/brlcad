/* Command-owned completion grammar for select and rselect. */
#include "common.h"
#include "bu/cmdschema.h"
#include "ged.h"
#include "../ged_private.h"

static const struct ged_cmd_native_form select_forms[] = {
    {"legacy", &ged_select_legacy_schema, NULL},
    {"new", NULL, &ged_select_new_tree},
    {NULL, NULL, NULL}
};

static const struct ged_cmd_native_form rselect_forms[] = {
    {"legacy", &ged_rselect_legacy_schema, NULL},
    {NULL, NULL, NULL}
};

static const struct ged_cmd_native_form *
ged_select_form_select(const struct ged *gedp, size_t UNUSED(argc), const char * const *UNUSED(argv))
{
    return gedp && gedp->new_cmd_forms ? &select_forms[1] : &select_forms[0];
}

static const struct ged_cmd_native_form *
ged_rselect_form_select(const struct ged *UNUSED(gedp), size_t UNUSED(argc), const char * const *UNUSED(argv))
{
    return &rselect_forms[0];
}

static int
ged_select_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_native_forms_validate(gedp, select_forms, ged_select_form_select,
	input, cursor_pos, result);
}

static int
ged_select_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_native_forms_analyze(gedp, select_forms, ged_select_form_select,
	input, analysis);
}

static char *
ged_select_grammar_json(void)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    char *legacy = bu_cmd_schema_describe_json(&ged_select_legacy_schema);
    char *new_form = bu_cmd_tree_describe_json(&ged_select_new_tree);
    bu_vls_strcat(&out, "{\"name\":\"select\",\"kind\":\"grammar_adapter\",");
    bu_vls_strcat(&out, "\"parse_policy\":\"context_selected_form\",");
    bu_vls_strcat(&out, "\"help\":\"Manage display selection and selection sets\",");
    bu_vls_printf(&out, "\"forms\":{\"legacy\":%s,\"new\":%s}}",
	legacy ? legacy : "{}", new_form ? new_form : "{}");
    if (legacy) bu_free(legacy, "select legacy schema JSON");
    if (new_form) bu_free(new_form, "select new schema JSON");
    return bu_vls_strdup(&out);
}

static int
ged_select_grammar_lint(struct bu_vls *msgs)
{
    int failures = 0;
    if (!ged_select_legacy_schema.options || !ged_select_legacy_schema.operands ||
	!ged_select_new_schema.options) {
	if (msgs) bu_vls_strcat(msgs, "select: native command forms are incomplete\n");
	failures++;
    }
    return failures + bu_cmd_tree_lint(&ged_select_new_tree, msgs);
}

static int
ged_rselect_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_native_forms_validate(gedp, rselect_forms, ged_rselect_form_select,
	input, cursor_pos, result);
}

static int
ged_rselect_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_native_forms_analyze(gedp, rselect_forms, ged_rselect_form_select,
	input, analysis);
}

static char *
ged_rselect_grammar_json(void)
{
    return bu_cmd_schema_describe_json(&ged_rselect_legacy_schema);
}

static int
ged_rselect_grammar_lint(struct bu_vls *msgs)
{
    if (ged_rselect_legacy_schema.options) return 0;
    if (msgs) bu_vls_strcat(msgs, "rselect: native schema is incomplete\n");
    return 1;
}

extern "C" GED_EXPORT const struct ged_cmd_grammar ged_select_grammar = {
    "select", "Manage display selection and selection sets", ged_select_grammar_validate,
    ged_select_grammar_analyze, ged_select_grammar_json, ged_select_grammar_lint
};

extern "C" GED_EXPORT const struct ged_cmd_grammar ged_rselect_grammar = {
    "rselect", "Select using the current rubber-band region", ged_rselect_grammar_validate,
    ged_rselect_grammar_analyze, ged_rselect_grammar_json, ged_rselect_grammar_lint
};

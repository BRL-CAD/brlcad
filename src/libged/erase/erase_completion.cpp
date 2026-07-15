/* Command-owned completion grammar for erase. */
#include "common.h"
#include "bu/cmdschema.h"
#include "ged.h"
#include "../ged_private.h"

static const struct ged_cmd_native_form erase_forms[] = {
    {"legacy", &ged_erase_legacy_schema, NULL},
    {"new", &ged_erase_new_schema, NULL},
    {NULL, NULL, NULL}
};

static const struct ged_cmd_native_form *
ged_erase_form_select(const struct ged *gedp, size_t UNUSED(argc), const char * const *UNUSED(argv))
{
    return gedp && gedp->new_cmd_forms ? &erase_forms[1] : &erase_forms[0];
}

static int
ged_erase_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_native_forms_validate(gedp, erase_forms, ged_erase_form_select,
	input, cursor_pos, result);
}

static int
ged_erase_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_native_forms_analyze(gedp, erase_forms, ged_erase_form_select,
	input, analysis);
}

static char *
ged_erase_grammar_json(void)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    char *legacy = bu_cmd_schema_describe_json(&ged_erase_legacy_schema);
    char *new_form = bu_cmd_schema_describe_json(&ged_erase_new_schema);
    bu_vls_strcat(&out, "{\"name\":\"erase\",\"kind\":\"grammar_adapter\",");
    bu_vls_strcat(&out, "\"parse_policy\":\"context_selected_form\",");
    bu_vls_strcat(&out, "\"help\":\"Erase database paths from the display\",");
    bu_vls_printf(&out, "\"forms\":{\"legacy\":%s,\"new\":%s}}",
	legacy ? legacy : "{}", new_form ? new_form : "{}");
    if (legacy) bu_free(legacy, "erase legacy schema JSON");
    if (new_form) bu_free(new_form, "erase new schema JSON");
    return bu_vls_strdup(&out);
}

static int
ged_erase_grammar_lint(struct bu_vls *msgs)
{
    int failures = 0;
    if (!ged_erase_legacy_schema.options || !ged_erase_legacy_schema.operands ||
	!ged_erase_legacy_schema.validation.custom_validate) {
	if (msgs) bu_vls_strcat(msgs, "erase: legacy native form is incomplete\n");
	failures++;
    }
    if (!ged_erase_new_schema.options || !ged_erase_new_schema.operands ||
	ged_erase_new_schema.parse_policy != BU_CMD_PARSE_INTERSPERSED) {
	if (msgs) bu_vls_strcat(msgs, "erase: new native form is incomplete\n");
	failures++;
    }
    return failures;
}

extern "C" GED_EXPORT const struct ged_cmd_grammar ged_erase_grammar = {
    "erase", "Erase database paths from the display", ged_erase_grammar_validate,
    ged_erase_grammar_analyze, ged_erase_grammar_json, ged_erase_grammar_lint
};

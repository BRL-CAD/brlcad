/* Command-owned completion grammar for view and view2. */
#include "common.h"
#include "bu/cmdschema.h"
#include "ged.h"
#include "../ged_private.h"

static int
ged_view_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &ged_view_tree, input, cursor_pos, result);
}

static int
ged_view2_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &ged_view2_tree, input, cursor_pos, result);
}

static int
ged_view_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &ged_view_tree, input, analysis);
}

static int
ged_view2_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &ged_view2_tree, input, analysis);
}

static char *ged_view_grammar_json(void) { return bu_cmd_tree_describe_json(&ged_view_tree); }
static char *ged_view2_grammar_json(void) { return bu_cmd_tree_describe_json(&ged_view2_tree); }
static int ged_view_grammar_lint(struct bu_vls *msgs) { return bu_cmd_tree_lint(&ged_view_tree, msgs); }
static int ged_view2_grammar_lint(struct bu_vls *msgs) { return bu_cmd_tree_lint(&ged_view2_tree, msgs); }

extern "C" GED_EXPORT const struct ged_cmd_grammar ged_view_grammar = {
    "view", "Inspect and manipulate named views", ged_view_grammar_validate,
    ged_view_grammar_analyze, ged_view_grammar_json, ged_view_grammar_lint
};

extern "C" GED_EXPORT const struct ged_cmd_grammar ged_view2_grammar = {
    "view2", "Inspect and manipulate named views", ged_view2_grammar_validate,
    ged_view2_grammar_analyze, ged_view2_grammar_json, ged_view2_grammar_lint
};

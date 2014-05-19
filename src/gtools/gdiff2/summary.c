/*                     G D I F F 2 . C
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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

#include "./gdiff2.h"


/*******************************************************************/
/* Output generators for diff log */
/*******************************************************************/

static void
avs_log(struct bu_vls *attr_log, const struct bu_attribute_value_set *avs, const char *title, const char *offset_title, const char *offset_entry)
{
    struct bu_attribute_value_pair *avpp;
    if (avs->count > 0) {
	bu_vls_printf(attr_log, "%s%s:\n", offset_title, title);
	for (BU_AVS_FOR(avpp, avs)) {
	    bu_vls_printf(attr_log, "%s%s\n", offset_entry, avpp->name);
	}
    }
}

static void
params_summary(struct bu_vls *attr_log, const struct diff_result_container *result)
{
    struct bu_attribute_value_pair *avpp;
    const struct bu_attribute_value_set *orig_only = &result->internal_orig_only;
    const struct bu_attribute_value_set *new_only = &result->internal_new_only;
    const struct bu_attribute_value_set *orig_diff = &result->internal_orig_diff;
    const struct bu_attribute_value_set *new_diff = &result->internal_new_diff;

    avs_log(attr_log, orig_only, "Parameters removed:", "   ", "      ");
    if (new_only->count > 0) {
	bu_vls_printf(attr_log, "   Parameters added:\n");
	for (BU_AVS_FOR(avpp, new_only)) {
	    bu_vls_printf(attr_log, "      %s: %s\n", avpp->name, avpp->value);
	}
    }
    if (orig_diff->count > 0) {
	bu_vls_printf(attr_log, "   Parameters changed:\n");
	for (BU_AVS_FOR(avpp, orig_diff)) {
	    bu_vls_printf(attr_log, "      %s: \"%s\" -> \"%s\"\n", avpp->name, avpp->value, bu_avs_get(new_diff, avpp->name));
	}
    }
}

static void
params3_summary(struct bu_vls *attr_log, const struct diff3_result_container *result)
{
    struct bu_attribute_value_pair *avpp;

    /* Report removed parameters */
    avs_log(attr_log, &result->param_removed_left_only, "Parameters removed in left", "   ", "      ");
    avs_log(attr_log, &result->param_removed_right_only, "Parameters removed in right", "   ", "      ");
    avs_log(attr_log, &result->param_removed_both, "Parameters removed in both left and right", "   ", "      ");

    /* Report added parameters */
    avs_log(attr_log, &result->param_added_left_only, "Parameters added in left", "   ", "      ");
    avs_log(attr_log, &result->param_added_right_only, "Parameters added in right", "   ", "      ");
    avs_log(attr_log, &result->param_added_both, "Parameters added in both left and right", "   ", "      ");

    /* Parameter conflicts in additions */
    if ((&result->param_added_conflict_left)->count > 0) {
	bu_vls_printf(attr_log, "   CONFLICT: Parameters added in both left and right with different values:\n");
	for (BU_AVS_FOR(avpp, &result->param_added_conflict_left)) {
	    bu_vls_printf(attr_log, "      %s: (%s,%s)\n", avpp->name, bu_avs_get(&result->param_added_conflict_left, avpp->name), bu_avs_get(&result->param_added_conflict_right, avpp->name));
	}
    }

    /* Report changed parameters */
    avs_log(attr_log, &result->param_changed_left_only, "Parameters changed in left", "   ", "      ");
    avs_log(attr_log, &result->param_changed_right_only, "Parameters changed in right", "   ", "      ");
    avs_log(attr_log, &result->param_changed_both, "Parameters changed to the same value in both left and right", "   ", "      ");

    /* Parameter conflicts in changes */
    if ((&result->param_changed_conflict_ancestor)->count > 0) {
	bu_vls_printf(attr_log, "   CONFLICT: Parameters changed in both left and right with different values:\n");
	for (BU_AVS_FOR(avpp, &result->param_added_conflict_left)) {
	    bu_vls_printf(attr_log, "      %s: (%s,%s)\n", avpp->name, bu_avs_get(&result->param_changed_conflict_ancestor, avpp->name), bu_avs_get(&result->param_changed_conflict_left, avpp->name), bu_avs_get(&result->param_changed_conflict_right, avpp->name));
	}
    }

}

static void
attrs_summary(struct bu_vls *attr_log, const struct diff_result_container *result)
{
    struct bu_attribute_value_pair *avpp;
    const struct bu_attribute_value_set *orig_only = &result->additional_orig_only;
    const struct bu_attribute_value_set *new_only = &result->additional_new_only;
    const struct bu_attribute_value_set *orig_diff = &result->additional_orig_diff;
    const struct bu_attribute_value_set *new_diff = &result->additional_new_diff;

    avs_log(attr_log, orig_only, "Attributes removed:", "   ", "      ");
    if (new_only->count > 0) {
	bu_vls_printf(attr_log, "   Attributes added:\n");
	for (BU_AVS_FOR(avpp, new_only)) {
	    bu_vls_printf(attr_log, "      %s: %s\n", avpp->name, avpp->value);
	}
    }
    if (orig_diff->count > 0) {
	bu_vls_printf(attr_log, "   Attribute parameters changed:\n");
	for (BU_AVS_FOR(avpp, orig_diff)) {
	    bu_vls_printf(attr_log, "      %s: \"%s\" -> \"%s\"\n", avpp->name, avpp->value, bu_avs_get(new_diff, avpp->name));
	}
    }
}

static void
attrs3_summary(struct bu_vls *attr_log, const struct diff3_result_container *result)
{
    struct bu_attribute_value_pair *avpp;

    /* Report removed attributes */
    avs_log(attr_log, &result->attribute_removed_left_only, "Attributes removed in left", "   ", "      ");
    avs_log(attr_log, &result->attribute_removed_right_only, "Attributes removed in right", "   ", "      ");
    avs_log(attr_log, &result->attribute_removed_both, "Attributes removed in both left and right", "   ", "      ");

    /* Report added attributes */
    avs_log(attr_log, &result->attribute_added_left_only, "Attributes added in left", "   ", "      ");
    avs_log(attr_log, &result->attribute_added_right_only, "Attributes added in right", "   ", "      ");
    avs_log(attr_log, &result->attribute_added_both, "Attributes added in both left and right", "   ", "      ");

    /* Attribute conflicts in additions */
    if ((&result->attribute_added_conflict_left)->count > 0) {
	bu_vls_printf(attr_log, "   CONFLICT: Attributes added in both left and right with different values:\n");
	for (BU_AVS_FOR(avpp, &result->attribute_added_conflict_left)) {
	    bu_vls_printf(attr_log, "      %s: (%s,%s)\n", avpp->name, bu_avs_get(&result->attribute_added_conflict_left, avpp->name), bu_avs_get(&result->attribute_added_conflict_right, avpp->name));
	}
    }

    /* Report changed attributes */
    avs_log(attr_log, &result->attribute_changed_left_only, "Attributes changed in left", "   ", "      ");
    avs_log(attr_log, &result->attribute_changed_right_only, "Attributes changed in right", "   ", "      ");
    avs_log(attr_log, &result->attribute_changed_both, "Attributes changed to the same value in both left and right", "   ", "      ");

    /* Attribute conflicts in changes */
    if ((&result->attribute_changed_conflict_ancestor)->count > 0) {
	bu_vls_printf(attr_log, "   CONFLICT: Attributes changed in both left and right with different values:\n");
	for (BU_AVS_FOR(avpp, &result->attribute_added_conflict_left)) {
	    bu_vls_printf(attr_log, "      %s: (%s,%s)\n", avpp->name, bu_avs_get(&result->attribute_changed_conflict_ancestor, avpp->name), bu_avs_get(&result->attribute_changed_conflict_left, avpp->name), bu_avs_get(&result->attribute_changed_conflict_right, avpp->name));
	}
    }

}

static int
print_dp(struct bu_vls *diff_log, const struct bu_ptbl *dptable, const int cnt, const struct directory *dp, const int line_len)
{
    int local_line_len = line_len;

    if (local_line_len + strlen(dp->d_namep) > 80) {
	bu_vls_printf(diff_log, "\n");
	local_line_len = 0;
    }

    bu_vls_printf(diff_log, "%s", dp->d_namep);
    local_line_len += strlen(dp->d_namep);

    if (local_line_len > 79 || (cnt+1) == (int)BU_PTBL_LEN(dptable)) {
	bu_vls_printf(diff_log, "\n");
	local_line_len = 0;
    } else {
	bu_vls_printf(diff_log, ", ");
	local_line_len += 2;
    }

    return local_line_len;
}


static void
print_tbl_dp(struct bu_vls *diff_log, const struct bu_ptbl *dptable, const char *lead, const int flag)
{
    int i = 0;
    int line_len = 0;
    if (flag > 0) {
	if ((int)BU_PTBL_LEN(dptable) > 0) {
	    bu_vls_printf(diff_log, "\n%s:\n", lead);
	}
	for (i = 0; i < (int)BU_PTBL_LEN(dptable); i++) {
	    struct directory *dp = (struct directory *)BU_PTBL_GET(dptable, i);
	    line_len = print_dp(diff_log, dptable, i, dp, line_len);
	}
    }
}

static void
print_tbl_diff(struct bu_vls *diff_log, const struct bu_ptbl *difftable, const char *lead, const int flag)
{
    int i = 0;
    int line_len = 0;
    if (flag > 0) {
	if ((int)BU_PTBL_LEN(difftable) > 0) {
	    bu_vls_printf(diff_log, "\n%s:\n", lead);
	}
	for (i = 0; i < (int)BU_PTBL_LEN(difftable); i++) {
	    struct diff_result_container *result = (struct diff_result_container *)BU_PTBL_GET(difftable, i);
	    const struct directory *dp = result->dp_orig;
	    if (!dp) dp = result->dp_new;
	    line_len = print_dp(diff_log, difftable, i, dp, line_len);
	}
    }
}

static void
print_tbl_diff3(struct bu_vls *diff_log, const struct bu_ptbl *difftable, const char *lead, const int flag)
{
    int i = 0;
    int line_len = 0;
    if (flag > 0) {
	if ((int)BU_PTBL_LEN(difftable) > 0) {
	    bu_vls_printf(diff_log, "\n%s:\n", lead);
	}
	for (i = 0; i < (int)BU_PTBL_LEN(difftable); i++) {
	    struct diff3_result_container *result = (struct diff3_result_container *)BU_PTBL_GET(difftable, i);
	    const struct directory *dp = result->ancestor_dp;
	    if (!dp) dp = result->left_dp;
	    if (!dp) dp = result->right_dp;
	    line_len = print_dp(diff_log, difftable, i, dp, line_len);
	}
    }
}

void
diff_summarize(struct bu_vls *diff_log, const struct diff_results *results, struct diff_state *state)
{
    int i = 0;

    struct bu_ptbl *added;
    struct bu_ptbl *removed;
    struct bu_ptbl *changed;
    struct bu_ptbl *unchanged;

    if (state->verbosity < 1 || !results)
	return;

    added = results->added;
    removed = results->removed;
    changed = results->changed;
    unchanged = results->unchanged;

    if (state->verbosity == 1) {
	print_tbl_dp(diff_log, added, "Objects added", state->return_added);
	print_tbl_dp(diff_log, removed, "Objects removed", state->return_removed);
	print_tbl_diff(diff_log, removed, "Objects changed", state->return_changed);
	print_tbl_dp(diff_log, unchanged, "Objects unchanged", state->return_unchanged);
    }
    if (state->verbosity > 1) {
	print_tbl_dp(diff_log, added, "Objects added", state->return_added);
	print_tbl_dp(diff_log, removed, "Objects removed", state->return_removed);
	if (state->return_changed > 0) {
	    for (i = 0; i < (int)BU_PTBL_LEN(changed); i++) {
		struct diff_result_container *result = (struct diff_result_container *)BU_PTBL_GET(changed, i);
		const struct directory *dp = result->dp_orig;
		if (!dp) dp = result->dp_new;
		bu_vls_printf(diff_log, "%s was changed:\n", dp->d_namep);
		params_summary(diff_log, result);
		attrs_summary(diff_log, result);
		bu_vls_printf(diff_log, "\n");
	    }
	}
	print_tbl_dp(diff_log, unchanged, "Objects unchanged", state->return_unchanged);
    }
    bu_vls_printf(diff_log, "\n");
}


void
diff3_summarize(struct bu_vls *diff_log, const struct diff3_results *results, struct diff_state *state)
{
    int i = 0;

    if (state->verbosity < 1 || !results)
	return;

    /*struct bu_ptbl *unchanged          = results->unchanged;         */ /* directory pointers (ancestor dbip) */
    /*struct bu_ptbl *removed_left_only  = results->removed_left_only; */ /* directory pointers (ancestor dbip) */
    /*struct bu_ptbl *removed_right_only = results->removed_right_only;*/ /* directory pointers (ancestor dbip) */
    /*struct bu_ptbl *removed_both       = results->removed_both;      */ /* directory pointers (ancestor dbip) */
    /*struct bu_ptbl *changed            = results->changed;  */          /* containers */
    /*struct bu_ptbl *conflict           = results->conflict; */          /* containers */

    if (state->return_added && ((int)BU_PTBL_LEN(results->added_left_only) > 0 || (int)BU_PTBL_LEN(results->added_right_only) > 0 || (int)BU_PTBL_LEN(results->added_both) > 0)) {
	bu_vls_printf(diff_log, "\nObjects added:\n");
    }
    print_tbl_dp(diff_log, results->added_left_only, "From left", state->return_added);
    print_tbl_dp(diff_log, results->added_right_only, "From right", state->return_added);
    print_tbl_dp(diff_log, results->added_both, "From left and right, identically", state->return_added);
    print_tbl_diff3(diff_log, results->added_merged, "From left and right, merged information", state->return_added);
    if (state->verbosity > 10) {
	for (i = 0; i < (int)BU_PTBL_LEN(results->added_merged); i++) {
	    struct diff3_result_container *result = (struct diff3_result_container *)BU_PTBL_GET(results->added_merged, i);
	    params3_summary(diff_log, result);
	    attrs3_summary(diff_log, result);
	}
    }
    if (state->return_removed > 0) {
    }
    if (state->return_changed > 0) {
    }
    if (state->return_unchanged > 0) {
    }

    bu_vls_printf(diff_log, "\n");
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

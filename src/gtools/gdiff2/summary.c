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

static struct diff_avp *
diff_ptbl_get(struct bu_ptbl *avp_array, const char *key)
{
    int i = 0;
    for (i = 0; i < (int)BU_PTBL_LEN(avp_array); i++) {
	struct diff_avp *avp = (struct diff_avp *)BU_PTBL_GET(avp_array, i);
	if (BU_STR_EQUAL(avp->name, key)) return avp;
    }
    return NULL;
}

void
diff_attrs_print(struct diff_result *dr, struct diff_state *state, struct bu_vls *diff_log) {
    if (state->use_params == 1) {
	struct diff_avp *minor_type = diff_ptbl_get(dr->param_diffs, "DB5_MINORTYPE");
	if ((!minor_type) || minor_type->state == DIFF_UNCHANGED || state->verbosity > 3) {
	    int i = 0;
	    for (i = 0; i < (int)BU_PTBL_LEN(dr->param_diffs); i++) {
		struct diff_avp *avp = (struct diff_avp *)BU_PTBL_GET(dr->param_diffs, i);
		if (state->return_added && avp->state == DIFF_ADDED) {
		    bu_vls_printf(diff_log, "A \"%s\" %s(p): %s\n", dr->obj_name, avp->name, avp->right_value);
		}
		if (state->return_removed && avp->state == DIFF_REMOVED) {
		    bu_vls_printf(diff_log, "D \"%s\" %s(p): %s\n", dr->obj_name, avp->name, avp->left_value);
		}
		if (state->return_changed && avp->state == DIFF_CHANGED) {
		    bu_vls_printf(diff_log, "- \"%s\" %s(p): %s\n+ \"%s\" %s(p): %s\n", dr->obj_name, avp->name, avp->left_value, dr->obj_name, avp->name, avp->right_value);
		}
		if (state->return_unchanged && avp->state == DIFF_UNCHANGED) {
		    bu_vls_printf(diff_log, "  \"%s\" %s(p): %s\n", dr->obj_name, avp->name, avp->left_value);
		}
	    }
	} else {
	    bu_vls_printf(diff_log, "M \"%s\" %s->%s\n", dr->obj_name, minor_type->left_value, minor_type->right_value);
	}

    }
    if (state->use_attrs == 1) {
	int i = 0;
	for (i = 0; i < (int)BU_PTBL_LEN(dr->attr_diffs); i++) {
	    struct diff_avp *avp = (struct diff_avp *)BU_PTBL_GET(dr->attr_diffs, i);
	    if (state->return_added && avp->state == DIFF_ADDED) {
		bu_vls_printf(diff_log, "A \"%s\" %s(a): %s\n", dr->obj_name, avp->name, avp->right_value);
	    }
	    if (state->return_removed && avp->state == DIFF_REMOVED) {
		bu_vls_printf(diff_log, "D \"%s\" %s(a): %s\n", dr->obj_name, avp->name, avp->left_value);
	    }
	    if (state->return_changed && avp->state == DIFF_CHANGED) {
		bu_vls_printf(diff_log, "- \"%s\" %s(a): %s\n+ \"%s\" %s(a): %s\n", dr->obj_name, avp->name, avp->left_value, dr->obj_name, avp->name, avp->right_value);
	    }
	    if (state->return_unchanged && avp->state == DIFF_UNCHANGED) {
		bu_vls_printf(diff_log, "  \"%s\" %s(a): %s\n", dr->obj_name, avp->name, avp->left_value);
	    }
	}
    }
}

void
diff_summarize(struct bu_vls *diff_log, const struct bu_ptbl *results, struct diff_state *state)
{
    int i = 0;
    struct bu_vls added_list = BU_VLS_INIT_ZERO;
    struct bu_vls removed_list = BU_VLS_INIT_ZERO;
    struct bu_vls changed_list = BU_VLS_INIT_ZERO;
    struct bu_vls unchanged_list = BU_VLS_INIT_ZERO;
    struct bu_vls added_tmp = BU_VLS_INIT_ZERO;
    struct bu_vls removed_tmp = BU_VLS_INIT_ZERO;
    struct bu_vls changed_tmp = BU_VLS_INIT_ZERO;
    struct bu_vls unchanged_tmp = BU_VLS_INIT_ZERO;


    if (state->verbosity < 1 || !results)
	return;

    if (state->verbosity == 1) {
	if (state->return_added == 1) bu_vls_sprintf(&added_list, "Added:\n");
	if (state->return_removed == 1) bu_vls_sprintf(&removed_list, "Removed:\n");
	if (state->return_changed == 1) bu_vls_sprintf(&changed_list, "Changed:\n");
	if (state->return_unchanged == 1) bu_vls_sprintf(&unchanged_list, "Unchanged:\n");
    }

    for (i = 0; i < (int)BU_PTBL_LEN(results); i++) {
	struct diff_result *dr = (struct diff_result *)BU_PTBL_GET(results, i);
	if (dr->param_state == DIFF_ADDED && state->return_added == 1) {
	    switch (state->verbosity) {
		case 1:
		    if (strlen(bu_vls_addr(&added_tmp)) + strlen(dr->obj_name) + 2 > 80) {
			bu_vls_printf(&added_list, "%s\n", bu_vls_addr(&added_tmp));
			bu_vls_sprintf(&added_tmp, "%s", dr->obj_name);
		    } else {
			if (strlen(bu_vls_addr(&added_tmp)) > 0) bu_vls_printf(&added_tmp, ", ");
			bu_vls_printf(&added_tmp, "%s", dr->obj_name);
		    }
		    break;
		case 2:
		case 3:
		    bu_vls_printf(diff_log, "A %s\n", dr->obj_name);
		    break;
		case 4:
		    diff_attrs_print(dr, state, diff_log);
		    break;
		default:
		    break;
	    }
	}
	if (dr->param_state == DIFF_REMOVED && state->return_removed == 1) {
	    switch (state->verbosity) {
		case 1:
		    if (strlen(bu_vls_addr(&removed_tmp)) + strlen(dr->obj_name) + 2 > 80) {
			bu_vls_printf(&removed_list, "%s\n", bu_vls_addr(&removed_tmp));
			bu_vls_sprintf(&removed_tmp, "%s", dr->obj_name);
		    } else {
			if (strlen(bu_vls_addr(&removed_tmp)) > 0) bu_vls_printf(&removed_tmp, ", ");
			bu_vls_printf(&removed_tmp, "%s", dr->obj_name);
		    }
		    break;
		case 2:
		case 3:
		    bu_vls_printf(diff_log, "D %s\n", dr->obj_name);
		    break;
		case 4:
		    diff_attrs_print(dr, state, diff_log);
		    break;
		default:
		    break;
	    }
	}
	if (((dr->param_state == DIFF_UNCHANGED && state->use_params) || !state->use_params) &&
		((dr->attr_state == DIFF_UNCHANGED && state->use_attrs) || !state->use_attrs) &&
	       	state->return_unchanged == 1) {
		switch (state->verbosity) {
		    case 1:
			if (strlen(bu_vls_addr(&unchanged_tmp)) + strlen(dr->obj_name) + 2 > 80) {
			    bu_vls_printf(&unchanged_list, "%s\n", bu_vls_addr(&unchanged_tmp));
			    bu_vls_sprintf(&unchanged_tmp, "%s", dr->obj_name);
			} else {
			    if (strlen(bu_vls_addr(&unchanged_tmp)) > 0) bu_vls_printf(&unchanged_tmp, ", ");
			    bu_vls_printf(&unchanged_tmp, "%s", dr->obj_name);
			}
			break;
		    case 2:
		    case 3:
			bu_vls_printf(diff_log, "  %s\n", dr->obj_name);
			break;
		    case 4:
			diff_attrs_print(dr, state, diff_log);
			break;
		    default:
			break;
		}
	}

	if (((dr->param_state != DIFF_UNCHANGED && state->use_params) || (dr->attr_state != DIFF_UNCHANGED && state->use_attrs)) && dr->param_state != DIFF_ADDED && dr->param_state != DIFF_REMOVED && state->return_changed == 1) {
	    switch (state->verbosity) {
		case 1:

		    if (strlen(bu_vls_addr(&changed_tmp)) + strlen(dr->obj_name) + 2 > 80) {
			bu_vls_printf(&changed_list, "%s\n", bu_vls_addr(&changed_tmp));
			bu_vls_sprintf(&changed_tmp, "%s", dr->obj_name);
		    } else {
			if (strlen(bu_vls_addr(&changed_tmp)) > 0) bu_vls_printf(&changed_tmp, ", ");
			bu_vls_printf(&changed_tmp, "%s", dr->obj_name);
		    }
		    break;
		case 2:
		    bu_vls_printf(diff_log, "M %s\n", dr->obj_name);
		    break;
		case 3:
		case 4:
		    diff_attrs_print(dr, state, diff_log);
		    break;
		default:
		    break;
	    }
	}
    }

    if (state->verbosity == 1) {
	bu_vls_printf(&added_list, "%s\n", bu_vls_addr(&added_tmp));
	bu_vls_printf(&removed_list, "%s\n", bu_vls_addr(&removed_tmp));
	bu_vls_printf(&changed_list, "%s\n", bu_vls_addr(&changed_tmp));
	bu_vls_printf(&unchanged_list, "%s\n", bu_vls_addr(&unchanged_tmp));
	if (state->return_added == 1) bu_vls_printf(diff_log, "%s\n", bu_vls_addr(&added_list));
	if (state->return_removed == 1) bu_vls_printf(diff_log, "%s\n", bu_vls_addr(&removed_list));
	if (state->return_changed == 1) bu_vls_printf(diff_log, "%s\n", bu_vls_addr(&changed_list));
	if (state->return_unchanged == 1) bu_vls_printf(diff_log, "%s\n", bu_vls_addr(&unchanged_list));
    }
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

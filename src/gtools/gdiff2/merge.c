/*                     M E R G E . C
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

static int
dp_copy(struct db_i *merged_dbip, struct db_i *source_dbip, const struct directory *source_dp, const char *new_name)
{
    struct directory *new_dp;
    if (source_dp->d_major_type != DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	struct rt_db_internal ip;
	if ((new_dp = db_diradd(merged_dbip, new_name, RT_DIR_PHONY_ADDR, 0, source_dp->d_flags, (genptr_t)&source_dp->d_minor_type)) == RT_DIR_NULL) {
	    return -1;
	}
	if (rt_db_get_internal(&ip, source_dp, source_dbip, NULL, &rt_uniresource) < 0) {
	    return -1;
	}
	if (rt_db_put_internal(new_dp, merged_dbip, &ip, &rt_uniresource) < 0) {
	    return -1;
	}
    }
    return 0;
}

static int
avp_diff(struct diff_result *dr)
{
    int left_right_diff = 0;
    int left_diff = 0;
    int right_diff = 0;
    int ret = 0;
    int i = 0;
    for (i = 0; i < (int)BU_PTBL_LEN(dr->param_diffs); i++) {
	struct diff_avp *avp = (struct diff_avp *)BU_PTBL_GET(dr->param_diffs, i);
	if (!BU_STR_EQUAL(avp->left_value, avp->right_value)) left_right_diff = 1;
	if (!BU_STR_EQUAL(avp->ancestor_value, avp->left_value)) left_diff = 1;
	if (!BU_STR_EQUAL(avp->ancestor_value, avp->right_value)) right_diff = 1;
    }
    if (left_right_diff) ret = 3;
    if (!left_diff && right_diff) ret = 2;
    if (left_diff && !right_diff) ret = 1;
    if ((left_diff && right_diff) && !left_right_diff) ret = 1;
    return ret;
}

static int
dp_changed_copy(struct db_i *merged_dbip, struct diff_result *dr,
       	struct db_i *left_dbip, struct db_i *ancestor_dbip, struct db_i *right_dbip)
{
    int i = 0;
    struct directory *new_dp;
    struct rt_db_internal ip;
    struct bu_attribute_value_set *obj_avs;
    BU_GET(obj_avs, struct bu_attribute_value_set);
    BU_AVS_INIT(obj_avs);
    for (i = 0; i < (int)BU_PTBL_LEN(dr->attr_diffs); i++) {
	struct diff_avp *avp = (struct diff_avp *)BU_PTBL_GET(dr->attr_diffs, i);
	if (avp->state == DIFF_ADDED) {
	    if (avp->left_value) {
		bu_avs_add(obj_avs, avp->name, avp->left_value);
	    } else {
		bu_avs_add(obj_avs, avp->name, avp->right_value);
	    }
	}
	if (avp->state == DIFF_UNCHANGED) {
	    bu_avs_add(obj_avs, avp->name, avp->ancestor_value);
	}
	if (avp->state == DIFF_CHANGED) {
	    if (!BU_STR_EQUAL(avp->ancestor_value, avp->left_value))
		bu_avs_add(obj_avs, avp->name, avp->left_value);
	    if (!BU_STR_EQUAL(avp->ancestor_value, avp->right_value))
		bu_avs_add(obj_avs, avp->name, avp->right_value);
	}
	if (avp->state == DIFF_CONFLICT) {
	    if (avp->ancestor_value) {
		struct bu_vls attr_name_ancestor = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&attr_name_ancestor, "CONFLICT(%s).ancestor", avp->name);
		bu_avs_add(obj_avs, bu_vls_addr(&attr_name_ancestor), avp->left_value);
		bu_vls_free(&attr_name_ancestor);
	    }
	    if (avp->left_value) {
		struct bu_vls attr_name_ancestor = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&attr_name_ancestor, "CONFLICT(%s).left", avp->name);
		bu_avs_add(obj_avs, bu_vls_addr(&attr_name_ancestor), avp->ancestor_value);
		bu_vls_free(&attr_name_ancestor);
	    }
	    if (avp->right_value) {
		struct bu_vls attr_name_ancestor = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&attr_name_ancestor, "CONFLICT(%s).right", avp->name);
		bu_avs_add(obj_avs, bu_vls_addr(&attr_name_ancestor), avp->right_value);
		bu_vls_free(&attr_name_ancestor);
	    }
	}
    }
    switch (avp_diff(dr)) {
	case 0:
	    if ((new_dp = db_diradd(merged_dbip, dr->dp_ancestor->d_namep, RT_DIR_PHONY_ADDR, 0, dr->dp_ancestor->d_flags, (genptr_t)&dr->dp_ancestor->d_minor_type)) == RT_DIR_NULL) {
		return -1;
	    }
	    if (rt_db_get_internal(&ip, dr->dp_ancestor, ancestor_dbip, NULL, &rt_uniresource) < 0) {
		return -1;
	    }
	    bu_avs_merge(&ip.idb_avs, obj_avs);
	    if (rt_db_put_internal(new_dp, merged_dbip, &ip, &rt_uniresource) < 0) {
		return -1;
	    }
	case 1:
	    if ((new_dp = db_diradd(merged_dbip, dr->dp_left->d_namep, RT_DIR_PHONY_ADDR, 0, dr->dp_left->d_flags, (genptr_t)&dr->dp_left->d_minor_type)) == RT_DIR_NULL) {
		return -1;
	    }
	    if (rt_db_get_internal(&ip, dr->dp_left, left_dbip, NULL, &rt_uniresource) < 0) {
		return -1;
	    }
	    bu_avs_merge(&ip.idb_avs, obj_avs);
	    if (rt_db_put_internal(new_dp, merged_dbip, &ip, &rt_uniresource) < 0) {
		return -1;
	    }
	    break;
	case 2:
	    if ((new_dp = db_diradd(merged_dbip, dr->dp_right->d_namep, RT_DIR_PHONY_ADDR, 0, dr->dp_right->d_flags, (genptr_t)&dr->dp_right->d_minor_type)) == RT_DIR_NULL) {
		return -1;
	    }
	    if (rt_db_get_internal(&ip, dr->dp_right, right_dbip, NULL, &rt_uniresource) < 0) {
		return -1;
	    }
	    bu_avs_merge(&ip.idb_avs, obj_avs);
	    if (rt_db_put_internal(new_dp, merged_dbip, &ip, &rt_uniresource) < 0) {
		return -1;
	    }
	    break;
	default:
	    if (dr->dp_left != RT_DIR_NULL) {
		struct directory *new_dp_left;
		struct bu_vls left_name = BU_VLS_INIT_ZERO;
		struct rt_db_internal ip_left;
		bu_vls_sprintf(&left_name, "CONFLICT(%s).left", dr->dp_left->d_namep);
		if ((new_dp_left = db_diradd(merged_dbip, bu_vls_addr(&left_name), RT_DIR_PHONY_ADDR, 0, dr->dp_left->d_flags, (genptr_t)&dr->dp_left->d_minor_type)) == RT_DIR_NULL) {
		    return -1;
		}
		if (rt_db_get_internal(&ip_left, dr->dp_left, left_dbip, NULL, &rt_uniresource) < 0) {
		    return -1;
		}
		bu_avs_free(&ip_left.idb_avs);
		bu_avs_merge(&ip_left.idb_avs, obj_avs);
		if (rt_db_put_internal(new_dp_left, merged_dbip, &ip_left, &rt_uniresource) < 0) {
		    return -1;
		}

	    }
	    if (dr->dp_right != RT_DIR_NULL) {
		struct directory *new_dp_right;
		struct bu_vls right_name = BU_VLS_INIT_ZERO;
		struct rt_db_internal ip_right;
		bu_vls_sprintf(&right_name, "CONFLICT(%s).right", dr->dp_right->d_namep);
		if ((new_dp_right = db_diradd(merged_dbip, bu_vls_addr(&right_name), RT_DIR_PHONY_ADDR, 0, dr->dp_right->d_flags, (genptr_t)&dr->dp_right->d_minor_type)) == RT_DIR_NULL) {
		    return -1;
		}
		if (rt_db_get_internal(&ip_right, dr->dp_right, right_dbip, NULL, &rt_uniresource) < 0) {
		    return -1;
		}
		bu_avs_free(&ip_right.idb_avs);
		bu_avs_merge(&ip_right.idb_avs, obj_avs);
		if (rt_db_put_internal(new_dp_right, merged_dbip, &ip_right, &rt_uniresource) < 0) {
		    return -1;
		}
	    }
	    if (dr->dp_ancestor != RT_DIR_NULL) {
		struct directory *new_dp_ancestor;
		struct bu_vls ancestor_name = BU_VLS_INIT_ZERO;
		struct rt_db_internal ip_ancestor;
		bu_vls_sprintf(&ancestor_name, "CONFLICT(%s).ancestor", dr->dp_ancestor->d_namep);
		if ((new_dp_ancestor = db_diradd(merged_dbip, bu_vls_addr(&ancestor_name), RT_DIR_PHONY_ADDR, 0, dr->dp_ancestor->d_flags, (genptr_t)&dr->dp_ancestor->d_minor_type)) == RT_DIR_NULL) {
		    return -1;
		}
		if (rt_db_get_internal(&ip_ancestor, dr->dp_ancestor, ancestor_dbip, NULL, &rt_uniresource) < 0) {
		    return -1;
		}
		bu_avs_free(&ip_ancestor.idb_avs);
		bu_avs_merge(&ip_ancestor.idb_avs, obj_avs);
		if (rt_db_put_internal(new_dp_ancestor, merged_dbip, &ip_ancestor, &rt_uniresource) < 0) {
		    return -1;
		}
	    }
	    break;
    }

    bu_avs_free(obj_avs);
    BU_PUT(obj_avs, struct bu_attribute_value_set);
    return 0;
}

int
diff3_merge(struct db_i *left_dbip, struct db_i *ancestor_dbip, struct db_i *right_dbip,
       struct diff_state *state, struct bu_ptbl *results)
{
    int i = 0;
    struct db_i *merged_dbip;
    if (!state->merge) return 0;
    if (strlen(bu_vls_addr(state->merge_file)) == 0) return -1;
    if (bu_file_exists(bu_vls_addr(state->merge_file), NULL)) return -1;
    bu_log("Merging into %s\n", bu_vls_addr(state->merge_file));

    if ((merged_dbip = db_create(bu_vls_addr(state->merge_file), 5)) == DBI_NULL) return -1;

    for (i = 0; i < (int)BU_PTBL_LEN(results); i++) {
	int added = 0;
	int removed = 0;
	int changed = 0;
	int conflict = 0;
	int unchanged = 0;

	struct diff_result *dr = (struct diff_result *)BU_PTBL_GET(results, i);

	if (dr->param_state == DIFF_ADDED) {added = 1;}
	if (dr->param_state == DIFF_REMOVED) {removed = 1;}
	if (dr->param_state & DIFF_CONFLICT || dr->attr_state & DIFF_CONFLICT) {conflict = 1;}
	if (!conflict && !added && !removed) {
	    if (dr->param_state != DIFF_UNCHANGED && dr->param_state != DIFF_EMPTY) changed = 1;
	    if (dr->attr_state != DIFF_UNCHANGED && dr->attr_state != DIFF_EMPTY) changed = 1;
	}
	if (!conflict && !added && !removed && !changed) {
	    unchanged = 1;
	}

	if (added) {
	    if (dr->dp_left != RT_DIR_NULL) {
		if (dp_copy(merged_dbip, left_dbip, dr->dp_left, dr->dp_left->d_namep) < 0) {return -1;}
	    } else {
		if (dp_copy(merged_dbip, right_dbip, dr->dp_right, dr->dp_right->d_namep) < 0) {return -1;}
	    }
	    /* Handle attributes */
	}
	if (conflict || changed) {
	    if (dp_changed_copy(merged_dbip, dr, left_dbip, ancestor_dbip, right_dbip) < 0) {return -1;}
		/* Conflict at the parameter level - need multiple db objects */
	}
	if (unchanged) {
	    if (dp_copy(merged_dbip, ancestor_dbip, dr->dp_ancestor, dr->dp_ancestor->d_namep) < 0) {return -1;}
	}
    }

    return 0;
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

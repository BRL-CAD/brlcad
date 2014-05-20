/*                     U T I L I T Y . C
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

int
attr_obj_diff(struct diff_result_container *result, const struct db_i *left, const struct db_i *right, const struct directory *dp_left, const struct directory *dp_right, const struct bn_tol *diff_tol) {
    struct bu_external before_ext;
    struct bu_external after_ext;
    struct db5_raw_internal before_raw;
    struct db5_raw_internal after_raw;
    struct bu_attribute_value_set before_avs;
    struct bu_attribute_value_set after_avs;

    if (!(dp_left->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY && dp_right->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY)) return -1;

    BU_EXTERNAL_INIT(&before_ext);
    BU_EXTERNAL_INIT(&after_ext);
    if (db_get_external(&before_ext, dp_left, left) < 0 || db5_get_raw_internal_ptr(&before_raw, before_ext.ext_buf) == NULL) {return -1;}
    if (db_get_external(&after_ext, dp_right, right) < 0 || db5_get_raw_internal_ptr(&after_raw, after_ext.ext_buf) == NULL) {return -1;}
    /* Parse out the attributes */
    if (db5_import_attributes(&before_avs, &before_raw.attributes) < 0 || db5_import_attributes(&after_avs, &after_raw.attributes) < 0) {
	bu_free_external(&before_ext);
	bu_free_external(&after_ext);
	return -1;
    }
    result->internal_diff = 0;
    result->attribute_diff = db_avs_diff(&(result->additional_new_only), &(result->additional_orig_only),
	    &(result->additional_orig_diff), &(result->additional_new_diff), &(result->additional_shared),
	    &before_avs, &after_avs, diff_tol);

    bu_free_external(&before_ext);
    bu_free_external(&after_ext);

    bu_avs_free(&before_avs);
    bu_avs_free(&after_avs);
    return result->attribute_diff;

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

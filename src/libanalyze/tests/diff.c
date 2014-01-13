/*                    T E S T _ D I F F . C
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

#include "common.h"
#include <string.h>
#include "analyze.h"

/* Debugging function for printing *everything* 

void
gdiff_print(void *result)
{
    struct bu_vls tmp = BU_VLS_INIT_ZERO;
    struct directory *dp = diff_info_dp(result, DIFF_ORIG);
    if (diff_type(result) == DIFF_ADDED) {
	dp = diff_info_dp(result, DIFF_NEW);
    }
    bu_log("\n\n%s(%d): (internal type: %d)\n", dp->d_namep, diff_status(result), diff_type(result));
    bu_vls_sprintf(&tmp, "Internal parameters: shared (%s)", dp->d_namep);
    bu_avs_print(diff_result(result, DIFF_SHARED_PARAM), bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Internal parameters: orig_only (%s)", dp->d_namep);
    bu_avs_print(diff_result(result, DIFF_ORIG_ONLY_PARAM), bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Internal parameters: new_only (%s)", dp->d_namep);
    bu_avs_print(diff_result(result, DIFF_NEW_ONLY_PARAM), bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Internal parameters: orig_diff (%s)", dp->d_namep);
    bu_avs_print(diff_result(result, DIFF_CHANGED_ORIG_PARAM), bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Internal parameters: new_diff (%s)", dp->d_namep);
    bu_avs_print(diff_result(result, DIFF_CHANGED_NEW_PARAM), bu_vls_addr(&tmp));

    bu_vls_sprintf(&tmp, "Additional parameters: shared (%s)", dp->d_namep);
    bu_avs_print(diff_result(result, DIFF_SHARED_ATTR), bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Additional parameters: orig_only (%s)", dp->d_namep);
    bu_avs_print(diff_result(result, DIFF_ORIG_ONLY_ATTR), bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Additional parameters: new_only (%s)", dp->d_namep);
    bu_avs_print(diff_result(result, DIFF_NEW_ONLY_ATTR), bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Additional parameters: orig_diff (%s)", dp->d_namep);
    bu_avs_print(diff_result(result, DIFF_CHANGED_ORIG_ATTR), bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Additional parameters: new_diff (%s)", dp->d_namep);
    bu_avs_print(diff_result(result, DIFF_CHANGED_NEW_ATTR), bu_vls_addr(&tmp));
}


void
diff_summarize(struct bu_ptbl *results_table)
{
    int i = 0;
    for (i = 0; i < (int)BU_PTBL_LEN(results_table); i++) {
	gdiff_print(BU_PTBL_GET(results_table, i));
    }
}
*/

void
attrs_summary(struct bu_vls *attr_log, void *result)
{
    struct bu_attribute_value_pair *avpp;
    struct bu_attribute_value_set *orig_only = diff_result(result, DIFF_ORIG_ONLY_ATTR);
    struct bu_attribute_value_set *new_only = diff_result(result, DIFF_NEW_ONLY_ATTR);
    struct bu_attribute_value_set *orig_diff = diff_result(result, DIFF_CHANGED_ORIG_ATTR);
    struct bu_attribute_value_set *new_diff = diff_result(result, DIFF_CHANGED_NEW_ATTR);
    if (orig_only->count > 0) {
	bu_vls_printf(attr_log, "   Attributes removed:\n");
	for (BU_AVS_FOR(avpp, orig_only)) {
	    bu_vls_printf(attr_log, "      %s\n", avpp->name);
	}
    }
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

void
diff_summarize(struct bu_vls *diff_log, struct bu_ptbl *results_table)
{
    int i = 0;
    struct bu_vls params = BU_VLS_INIT_ZERO;
    struct bu_vls added = BU_VLS_INIT_ZERO;
    struct bu_vls removed = BU_VLS_INIT_ZERO;
    struct bu_vls typechanged = BU_VLS_INIT_ZERO;
    struct bu_vls same = BU_VLS_INIT_ZERO;
    struct bu_attribute_value_pair *avpp;
    for (i = 0; i < (int)BU_PTBL_LEN(results_table); i++) {
	void *result = BU_PTBL_GET(results_table, i);
	struct directory *dp = diff_info_dp(result, DIFF_ORIG);
	if (diff_type(result) == DIFF_ADDED) {
	    dp = diff_info_dp(result, DIFF_NEW);
	}
	switch(diff_type(result)) {
	    case DIFF_NONE:
		break;
	    case DIFF_REMOVED:
		bu_vls_printf(&removed, "%s was removed.\n\n", dp->d_namep);
		break;
	    case DIFF_ADDED:
		bu_vls_printf(&added, "%s was added.\n\n", dp->d_namep);
		break;
	    case DIFF_TYPECHANGE:
		bu_vls_printf(&typechanged, "%s changed from type \"%s\" to type \"%s\".\n", dp->d_namep, diff_info_intern(result, DIFF_ORIG)->idb_meth->ft_label, diff_info_intern(result, DIFF_NEW)->idb_meth->ft_label);
		attrs_summary(&typechanged, result);
		bu_vls_printf(&typechanged, "\n");
		break;
	    case DIFF_BINARY:
		bu_vls_printf(&params, "%s exhibits a binary difference.\n", dp->d_namep);
		attrs_summary(&params, result);
		bu_vls_printf(&params, "\n");
		break;
	    case DIFF:
		bu_vls_printf(&params, "%s changed:\n", dp->d_namep);
		for (BU_AVS_FOR(avpp, diff_result(result, DIFF_CHANGED_ORIG_PARAM))) {
		    bu_vls_printf(&params, "   %s: \"%s\" -> \"%s\"\n", avpp->name, avpp->value, bu_avs_get(diff_result(result, DIFF_CHANGED_NEW_PARAM), avpp->name));
		}
		attrs_summary(&params, result);
		bu_vls_printf(&params, "\n");
		break;
	    default:
		break;
	}
    }
    bu_vls_trunc(diff_log, 0);
    if (strlen(bu_vls_addr(&same)) > 0) bu_vls_printf(diff_log, "%s", bu_vls_addr(&same));
    if (strlen(bu_vls_addr(&removed)) > 0) bu_vls_printf(diff_log, "%s", bu_vls_addr(&removed));
    if (strlen(bu_vls_addr(&added)) > 0) bu_vls_printf(diff_log, "%s", bu_vls_addr(&added));
    if (strlen(bu_vls_addr(&typechanged)) > 0) bu_vls_printf(diff_log, "%s", bu_vls_addr(&typechanged));
    if (strlen(bu_vls_addr(&params)) > 0) bu_vls_printf(diff_log, "%s", bu_vls_addr(&params));

}

int
main(int argc, char **argv)
{
    struct bu_ptbl *results;
    struct db_i *dbip1 = DBI_NULL;
    struct db_i *dbip2 = DBI_NULL;
    struct bu_vls diff_log = BU_VLS_INIT_ZERO;

    if (argc != 3) {
	bu_log("Error - please specify two .g files\n");
	bu_exit(EXIT_FAILURE, NULL);
    }

    if (!bu_file_exists(argv[1], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[1]);
    }

    if (!bu_file_exists(argv[2], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[2]);
    }

    if ((dbip1 = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[1]);
    }
    RT_CK_DBI(dbip1);
    if (db_dirbuild(dbip1) < 0) {
	db_close(dbip1);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
    }

    if ((dbip2 = db_open(argv[2], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[2]);
    }
    RT_CK_DBI(dbip2);
    if (db_dirbuild(dbip2) < 0) {
	db_close(dbip1);
	db_close(dbip2);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
    }


    results = diff_dbip(dbip1, dbip2);
    diff_summarize(&diff_log, results);
    diff_free_ptbl(results);

    bu_log("%s", bu_vls_addr(&diff_log));

    db_close(dbip1);
    db_close(dbip2);
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

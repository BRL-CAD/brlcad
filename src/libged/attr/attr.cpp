/*                       A T T R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/attr.cpp
 *
 * The attr command.
 *
 */

#include "common.h"

#include <utility>
#include <string>
#include <set>
#include <cstdlib>

#include "bu/getopt.h"
#include "bu/path.h"
#include "bu/sort.h"
#include "../ged_private.h"


typedef enum {
    ATTR_APPEND,
    ATTR_COPY,
    ATTR_GET,
    ATTR_RM,
    ATTR_SET,
    ATTR_SHOW,
    ATTR_SORT,
    ATTR_LIST,
    ATTR_UNKNOWN
} attr_cmd_t;


/*
 * avs attribute comparison function, e.g. for bu_sort
 */
HIDDEN int
attr_cmp(const void *p1, const void *p2, void *UNUSED(arg))
{
    return bu_strcmp(((struct bu_attribute_value_pair *)p1)->name,
		     ((struct bu_attribute_value_pair *)p2)->name);
}


HIDDEN int
attr_cmp_nocase(const void *p1, const void *p2, void *UNUSED(arg))
{
    return bu_strcasecmp(((struct bu_attribute_value_pair *)p1)->name,
			 ((struct bu_attribute_value_pair *)p2)->name);
}


HIDDEN int
attr_cmp_value(const void *p1, const void *p2, void *UNUSED(arg))
{
    return bu_strcmp(((struct bu_attribute_value_pair *)p1)->value,
		     ((struct bu_attribute_value_pair *)p2)->value);
}


HIDDEN int
attr_cmp_value_nocase(const void *p1, const void *p2, void *UNUSED(arg))
{
    return bu_strcasecmp(((struct bu_attribute_value_pair *)p1)->value,
			 ((struct bu_attribute_value_pair *)p2)->value);
}


HIDDEN int
attr_pretty_print(struct ged *gedp, struct directory *dp, const char *name)
{
    if (dp->d_flags & RT_DIR_COMB) {
	if (dp->d_flags & RT_DIR_REGION) {
	    bu_vls_printf(gedp->ged_result_str, "%s region:\n", name);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%s combination:\n", name);
	}
    } else if (dp->d_flags & RT_DIR_SOLID) {
	struct rt_db_internal intern;
	GED_DB_GET_INTERNAL(gedp, &intern, dp, (fastf_t *)NULL, &rt_uniresource, GED_ERROR);
	bu_vls_printf(gedp->ged_result_str, "%s %s:\n", name, intern.idb_meth->ft_label);
	rt_db_free_internal(&intern);
    } else {
	switch (dp->d_major_type) {
	    case DB5_MAJORTYPE_ATTRIBUTE_ONLY:
		bu_vls_printf(gedp->ged_result_str, "%s global:\n", name);
		break;
	    case DB5_MAJORTYPE_BINARY_MIME:
		bu_vls_printf(gedp->ged_result_str, "%s binary(mime):\n", name);
		break;
	    case DB5_MAJORTYPE_BINARY_UNIF:
		bu_vls_printf(gedp->ged_result_str, "%s %s:\n", name,
			      binu_types[dp->d_minor_type]);
		break;
	}
    }

    return GED_OK;
}


HIDDEN attr_cmd_t
attr_cmd(const char* arg)
{
    /* sub-commands */
    const char APPEND[] = "append";
    const char COPY[]   = "copy";
    const char GET[]    = "get";
    const char LIST[]   = "list";
    const char RM[]     = "rm";
    const char SET[]    = "set";
    const char SHOW[]   = "show";
    const char SORT[]   = "sort";

    /* in one user's predicted order of frequency: */
    if (BU_STR_EQUIV(SHOW, arg))
	return ATTR_SHOW;
    else if (BU_STR_EQUIV(SET, arg))
	return ATTR_SET;
    else if (BU_STR_EQUIV(SORT, arg))
	return ATTR_SORT;
    else if (BU_STR_EQUIV(RM, arg))
	return ATTR_RM;
    else if (BU_STR_EQUIV(APPEND, arg))
	return ATTR_APPEND;
    else if (BU_STR_EQUIV(GET, arg))
	return ATTR_GET;
    else if (BU_STR_EQUIV(LIST, arg))
	return ATTR_LIST;
    else if (BU_STR_EQUIV(COPY, arg))
	return ATTR_COPY;
    else
	return ATTR_UNKNOWN;
}

struct avsncmp {
    bool operator () (const std::pair<std::string, std::string> &p_left, const std::pair<std::string, std::string> &p_right) const
    {
	long l1, l2;
	char *endptr = NULL;
	if (p_left.first != p_right.first) {
	    return p_left < p_right;
	}
	errno = 0;
	endptr = NULL;
	l1 = strtol(p_left.second.c_str(), &endptr, 0);
	if (endptr != NULL && strlen(endptr) > 0) {
	    return p_left < p_right;
	}
	errno = 0;
	endptr = NULL;
	l2 = strtol(p_right.second.c_str(), &endptr, 0);
	if (endptr != NULL && strlen(endptr) > 0) {
	    return p_left < p_right;
	}
	return l1 < l2;
    }
};

static int
attr_list(struct ged *gedp, size_t path_cnt, struct directory **paths, int argc, const char **argv)
{
    if (argc > 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: attr list obj_pattern [key_pattern [value_pattern]]");
	return GED_HELP;
    }
    struct bu_attribute_value_pair *avpp;
    std::set<std::pair<std::string, std::string>, avsncmp> uniq_avp;
    for (size_t i = 0; i < path_cnt; i++) {
	struct directory *dp = paths[i];
	struct bu_attribute_value_set lavs;
	bu_avs_init_empty(&lavs);
	if (db5_get_attributes(gedp->ged_wdbp->dbip, &lavs, dp)) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
	    bu_avs_free(&lavs);
	    return GED_ERROR;
	}
	size_t j = 0;
	for (j = 0, avpp = lavs.avp; j < lavs.count; j++, avpp++) {
	    /* If we have a key-only filter, filter printing based on matching
	     * to that filter.  If we also have a value filter, print the value as well according to value
	     * matches. */
	    if (!argc) {
		uniq_avp.insert(std::make_pair(std::string(avpp->name), std::string("")));
		continue;
	    }
	    if (argc == 1) {
		if (!bu_path_match(argv[0], avpp->name, 0)) {
		    uniq_avp.insert(std::make_pair(std::string(avpp->name), std::string("")));
		}
		continue;
	    }
	    if (argc == 2) {
		if (!bu_path_match(argv[0], avpp->name, 0) && !bu_path_match(argv[1], avpp->value, 0)) {
		    uniq_avp.insert(std::make_pair(std::string(avpp->name), std::string(avpp->value)));
		}
		continue;
	    }
	}
	bu_avs_free(&lavs);
    }
    /* List all the attributes. */
    std::set<std::pair<std::string, std::string>>::iterator u_it;
    for (u_it = uniq_avp.begin(); u_it != uniq_avp.end(); u_it++) {
	if (!argc) {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", u_it->first.c_str());
	    continue;
	}
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", u_it->first.c_str());
	    continue;
	}
	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%s=%s\n", u_it->first.c_str(), u_it->second.c_str());
	    continue;
	}
    }

    return GED_OK;
}
 


HIDDEN void
attr_print(struct ged *gedp, struct bu_attribute_value_set *avs, int argc, const char **argv)
{
    size_t max_attr_name_len  = 0;
    struct bu_attribute_value_pair *avpp;
    size_t i, j;
    /* If we don't have a specified set, do everything */
    /* find the max_attr_name_len  */
    if (argc == 0 || !argv) {
	for (j = 0, avpp = avs->avp; j < avs->count; j++, avpp++) {
	    size_t len = strlen(avpp->name);
	    if (len > max_attr_name_len) max_attr_name_len = len;
	}
	for (i = 0, avpp = avs->avp; i < avs->count; i++, avpp++) {
	    size_t len_diff = 0;
	    size_t count = 0;
	    bu_vls_printf(gedp->ged_result_str, "\t%s", avpp->name);
	    len_diff = max_attr_name_len - strlen(avpp->name);
	    while (count < (len_diff) + 1) {
		bu_vls_printf(gedp->ged_result_str, " ");
		count++;
	    }
	    bu_vls_printf(gedp->ged_result_str, "%s\n", avpp->value);
	}
    } else {
	for (i = 0; i < (size_t)argc; i++) {
	    size_t len = strlen(argv[i]);
	    if (len > max_attr_name_len) max_attr_name_len = len;
	}
	for (i = 0; i < (size_t)argc; i++) {
	    const char *aval = bu_avs_get(avs, argv[i]);
	    size_t len_diff = 0;
	    size_t count = 0;
	    if (!aval) {
		bu_vls_sprintf(gedp->ged_result_str, "ERROR: attribute %s not found\n", argv[i]);
		return;
	    }
	    bu_vls_printf(gedp->ged_result_str, "\t%s", argv[i]);
	    len_diff = max_attr_name_len - strlen(argv[i]);
	    while (count < (len_diff) + 1) {
		bu_vls_printf(gedp->ged_result_str, " ");
		count++;
	    }
	    bu_vls_printf(gedp->ged_result_str, "%s\n", aval);
	}
    }
}


extern "C" int
ged_attr_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = GED_OK;
    size_t i;
    struct directory *dp;
    struct bu_attribute_value_pair *avpp;
    static const char *usage = "{[-c sep_char] set|get|show|rm|append|sort|list|copy} object [key [value] ... ]";
    attr_cmd_t scmd;
    struct directory **paths = NULL;
    size_t path_cnt = 0;
    int opt;
    int c_sep = -1;
    const char *cmd_name = argv[0];

    /* sort types */
    const char CASE[]         = "case";
    const char NOCASE[]       = "nocase";
    const char VALUE[]        = "value";
    const char VALUE_NOCASE[] = "value-nocase";

    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return GED_HELP;
    }

    bu_optind = 1;      /* re-init bu_getopt() */
    while ((opt = bu_getopt(argc, (char * const *)argv, "c:")) != -1) {
	switch (opt) {
	    case 'c':
		c_sep = (int)bu_optarg[0];
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Unrecognized option - %c", opt);
		return GED_ERROR;
	}
    }

    argc -= bu_optind - 1;
    argv += bu_optind - 1;

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return GED_ERROR;
    }

    /* Verify that this wdb supports lookup operations
       (non-null dbip) */
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);

    /* this is only valid for v5 databases */
    if (db_version(gedp->ged_wdbp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str, "Attributes are not available for this database format.\nPlease upgrade your database format using \"dbupgrade\" to enable attributes.");
	return GED_ERROR;
    }

    scmd = attr_cmd(argv[1]);

    path_cnt = db_ls(gedp->ged_wdbp->dbip, DB_LS_HIDDEN, argv[2], &paths);

    if (path_cnt == 0) {
	bu_vls_printf(gedp->ged_result_str, "Cannot locate objects matching %s\n", argv[2]);
	ret = GED_ERROR;
	goto ged_attr_core_memfree;
    }

    if (scmd == ATTR_SORT) {
	for (i = 0; i < path_cnt; i++) {
	    /* for pretty printing */
	    size_t j = 0;
	    size_t max_attr_value_len = 0;

	    struct bu_attribute_value_set avs;
	    bu_avs_init_empty(&avs);

	    dp = paths[i];

	    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
		bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
		ret = GED_ERROR;
		goto ged_attr_core_memfree;
	    }
	    bu_sort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), attr_cmp, NULL);
	    /* get a jump on calculating name and value lengths */
	    for (j = 0, avpp = avs.avp; j < avs.count; j++, avpp++) {
		if (avpp->value) {
		    size_t len = strlen(avpp->value);
		    if (len > max_attr_value_len)
			max_attr_value_len = len;
		}
	    }

	    /* pretty print */
	    if ((attr_pretty_print(gedp, dp, argv[2])) != GED_OK) {
		ret = GED_ERROR;
		goto ged_attr_core_memfree;
	    }
	    if (argc == 3) {
		/* just list the already sorted attribute-value pairs */
		attr_print(gedp, &avs, 0, NULL);
	    } else {
		/* argv[3] is the sort type: 'case', 'nocase', 'value', 'value-nocase' */
		if (BU_STR_EQUIV(argv[3], NOCASE)) {
		    bu_sort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), attr_cmp_nocase, NULL);
		} else if (BU_STR_EQUIV(argv[3], VALUE)) {
		    bu_sort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), attr_cmp_value, NULL);
		} else if (BU_STR_EQUIV(argv[3], VALUE_NOCASE)) {
		    bu_sort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), attr_cmp_value_nocase, NULL);
		} else if (BU_STR_EQUIV(argv[3], CASE)) {
		    ; /* don't need to do anything since this is the existing (default) sort */
		}
		/* just list the already sorted attribute-value pairs */
		attr_print(gedp, &avs, 0, NULL);
	    }
	    bu_avs_free(&avs);
	}
    } else if (scmd == ATTR_GET) {
	if (path_cnt == 1) {
	    struct bu_attribute_value_set avs;
	    bu_avs_init_empty(&avs);

	    dp = paths[0];

	    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
		bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
		ret = GED_ERROR;
		goto ged_attr_core_memfree;
	    }
	    bu_sort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), attr_cmp, NULL);

	    if (argc == 3) {
		/* just list all the attributes */
		for (i = 0, avpp = avs.avp; i < avs.count; i++, avpp++) {
		    if (c_sep == -1)
			bu_vls_printf(gedp->ged_result_str, "%s {%s} ", avpp->name, avpp->value);
		    else {
			if (i == 0)
			    bu_vls_printf(gedp->ged_result_str, "%s%c%s", avpp->name, (char)c_sep, avpp->value);
			else
			    bu_vls_printf(gedp->ged_result_str, "%c%s%c%s", (char)c_sep, avpp->name, (char)c_sep, avpp->value);
		    }
		}
	    } else {
		const char *val;
		int do_separators = argc-4; /* if more than one attribute */

		for (i = 3; i < (size_t)argc; i++) {
		    val = bu_avs_get(&avs, argv[i]);
		    if (!val) {
			bu_vls_printf(gedp->ged_result_str,
				"Object %s does not have a %s attribute\n",
				dp->d_namep,
				argv[i]);
			bu_avs_free(&avs);
			ret = GED_ERROR;
			goto ged_attr_core_memfree;
		    }
		    if (do_separators) {
			if (c_sep == -1)
			    bu_vls_printf(gedp->ged_result_str, "{%s} ", val);
			else {
			    if (i == 3)
				bu_vls_printf(gedp->ged_result_str, "%s", val);
			    else
				bu_vls_printf(gedp->ged_result_str, "%c%s", (char)c_sep, val);
			}
		    } else {
			bu_vls_printf(gedp->ged_result_str, "%s", val);
		    }
		}
	    }
	    bu_avs_free(&avs);
	} else {
	    for (i = 0; i < path_cnt; i++) {
		size_t j = 0;
		struct bu_vls obj_vals = BU_VLS_INIT_ZERO;
		struct bu_attribute_value_set avs;
		bu_avs_init_empty(&avs);
		dp = paths[i];

		if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
		    bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
		    ret = GED_ERROR;
		    goto ged_attr_core_memfree;
		}
		bu_sort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), attr_cmp, NULL);

		if (argc == 3) {
		    /* just list all the attributes */
		    for (j = 0, avpp = avs.avp; j < avs.count; j++, avpp++) {
			if (c_sep == -1)
			    bu_vls_printf(&obj_vals, "%s {%s} ", avpp->name, avpp->value);
			else {
			    if (j == 0)
				bu_vls_printf(&obj_vals, "%s%c%s", avpp->name, (char)c_sep, avpp->value);
			    else
				bu_vls_printf(&obj_vals, "%c%s%c%s", (char)c_sep, avpp->name, (char)c_sep, avpp->value);
			}
		    }
		} else {
		    const char *val;
		    int do_separators = argc-4; /* if more than one attribute */

		    for (j = 3; j < (size_t)argc; j++) {
			val = bu_avs_get(&avs, argv[j]);
			if (val) {
			    if (do_separators) {
				if (c_sep == -1)
				    bu_vls_printf(&obj_vals, "{%s} ", val);
				else {
				    if (j == 0)
					bu_vls_printf(&obj_vals, "%s", val);
				    else
					bu_vls_printf(&obj_vals, "%c%s", (char)c_sep, val);
				}
			    } else {
				bu_vls_printf(&obj_vals, "%s", val);
			    }
			}
		    }
		}
		if (strlen(bu_vls_addr(&obj_vals)) > 0) {
		    bu_vls_printf(gedp->ged_result_str, "%s: ", dp->d_namep);
		    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&obj_vals));

		    if (i < path_cnt-1) {
			bu_vls_printf(gedp->ged_result_str, "\n");
		    }
		}

		bu_vls_free(&obj_vals);
		bu_avs_free(&avs);
	    }
	}
    } else if (scmd == ATTR_LIST) {

	return attr_list(gedp, path_cnt, paths, argc - 3, &(argv[3]));

    } else if (scmd == ATTR_COPY) {
	const char *oattr, *nattr;

	if (argc != 5) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	    return GED_ERROR;
	}
	oattr = argv[3];
	nattr = argv[4];

	for (i = 0; i < path_cnt; i++) {
	    const char *val;
	    struct bu_attribute_value_set lavs;
	    bu_avs_init_empty(&lavs);
	    dp = paths[i];
	    if (db5_get_attributes(gedp->ged_wdbp->dbip, &lavs, dp)) {
		bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
		ret = GED_ERROR;
		goto ged_attr_core_memfree;
	    }
	    val = bu_avs_get(&lavs, oattr);
	    if (val) {
		(void)bu_avs_add(&lavs, nattr, val);
		db5_standardize_avs(&lavs);
		if (db5_update_attributes(dp, &lavs, gedp->ged_wdbp->dbip)) {
		    bu_vls_printf(gedp->ged_result_str,
			    "Error: failed to update attributes\n");
		    bu_avs_free(&lavs);
		    ret = GED_ERROR;
		    goto ged_attr_core_memfree;
		}
		/* lavs is freed by db5_update_attributes() */
	    } else {
		bu_avs_free(&lavs);
	    }
	}

    } else if (scmd == ATTR_SET) {
	GED_CHECK_READ_ONLY(gedp, GED_ERROR);
	/* setting attribute/value pairs */
	if ((argc - 3) % 2) {
	    bu_vls_printf(gedp->ged_result_str,
		    "Error: attribute names and values must be in pairs!!!\n");
	    ret = GED_ERROR;
	    goto ged_attr_core_memfree;
	}
	for (i = 0; i < path_cnt; i++) {
	    size_t j = 3;
	    struct bu_attribute_value_set avs;
	    bu_avs_init_empty(&avs);
	    dp = paths[i];

	    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
		bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
		ret = GED_ERROR;
		goto ged_attr_core_memfree;
	    }
	    bu_sort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), attr_cmp, NULL);
	    while (j < (size_t)argc) {
		if (BU_STR_EQUAL(argv[j], "region") && BU_STR_EQUAL(argv[j+1], "R")) {
		    dp->d_flags |= RT_DIR_REGION;
		}
		(void)bu_avs_add(&avs, argv[j], argv[j+1]);
		j += 2;
	    }
	    db5_standardize_avs(&avs);
	    if (db5_update_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
		bu_vls_printf(gedp->ged_result_str,
			"Error: failed to update attributes\n");
		bu_avs_free(&avs);
		ret = GED_ERROR;
		goto ged_attr_core_memfree;
	    }
	    /* avs is freed by db5_update_attributes() */
	}

    } else if (scmd == ATTR_RM) {
	GED_CHECK_READ_ONLY(gedp, GED_ERROR);
	for (i = 0; i < path_cnt; i++) {
	    size_t j = 3;
	    struct bu_attribute_value_set avs;
	    bu_avs_init_empty(&avs);
	    dp = paths[i];

	    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
		bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
		ret = GED_ERROR;
		goto ged_attr_core_memfree;
	    }
	    bu_sort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), attr_cmp, NULL);

	    while (j < (size_t)argc) {
		if (BU_STR_EQUAL(argv[j], "region")) {
		    dp->d_flags = dp->d_flags & ~(RT_DIR_REGION);
		}
		(void)bu_avs_remove(&avs, argv[j]);
		j++;
	    }
	    if (db5_replace_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
		bu_vls_printf(gedp->ged_result_str,
			"Error: failed to update attributes\n");
		bu_avs_free(&avs);
		ret = GED_ERROR;
		goto ged_attr_core_memfree;
	    }
	    /* avs is freed by db5_replace_attributes() */
	}

    } else if (scmd == ATTR_APPEND) {
	GED_CHECK_READ_ONLY(gedp, GED_ERROR);
	if ((argc-3) % 2) {
	    bu_vls_printf(gedp->ged_result_str,
			  "Error: attribute names and values must be in pairs!!!\n");
	    ret = GED_ERROR;
	    goto ged_attr_core_memfree;
	}
	for (i = 0; i < path_cnt; i++) {
	    size_t j = 3;
	    struct bu_attribute_value_set avs;
	    bu_avs_init_empty(&avs);
	    dp = paths[i];

	    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
		bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
		ret = GED_ERROR;
		goto ged_attr_core_memfree;
	    }
	    bu_sort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), attr_cmp, NULL);

	    while (j < (size_t)argc) {
		const char *old_val;
		if (BU_STR_EQUAL(argv[j], "region") && BU_STR_EQUAL(argv[j+1], "R")) {
		    dp->d_flags |= RT_DIR_REGION;
		}
		old_val = bu_avs_get(&avs, argv[j]);
		if (!old_val) {
		    (void)bu_avs_add(&avs, argv[j], argv[j+1]);
		} else {
		    struct bu_vls vls = BU_VLS_INIT_ZERO;

		    bu_vls_strcat(&vls, old_val);
		    bu_vls_strcat(&vls, argv[j+1]);
		    bu_avs_add_vls(&avs, argv[j], &vls);
		    bu_vls_free(&vls);
		}

		j += 2;
	    }
	    if (db5_replace_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
		bu_vls_printf(gedp->ged_result_str,
			"Error: failed to update attributes\n");
		bu_avs_free(&avs);
		ret = GED_ERROR;
		goto ged_attr_core_memfree;
	    }

	    /* avs is freed by db5_replace_attributes() */
	}
    } else if (scmd == ATTR_SHOW) {
	for (i = 0; i < path_cnt; i++) {
	    /* for pretty printing */
	    size_t max_attr_value_len = 0;

	    size_t j = 0;
	    struct bu_attribute_value_set avs;
	    bu_avs_init_empty(&avs);
	    dp = paths[i];

	    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
		bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
		ret = GED_ERROR;
		goto ged_attr_core_memfree;
	    }

	    /* get a jump on calculating name and value lengths */
	    for (j = 0, avpp = avs.avp; j < avs.count; j++, avpp++) {
		if (avpp->value) {
		    size_t len = strlen(avpp->value);
		    if (len > max_attr_value_len)
			max_attr_value_len = len;
		}
	    }

	    /* pretty print */
	    if ((attr_pretty_print(gedp, dp, dp->d_namep)) != GED_OK) {
		ret = GED_ERROR;
		goto ged_attr_core_memfree;
	    }

	    if (argc == 3) {
		/* just display all attributes */
		attr_print(gedp, &avs, 0, NULL);
	    } else {
		/* show just the specified attributes */
		attr_print(gedp, &avs, argc - 3, argv + 3);
	    }
	}
    } else {
	bu_vls_printf(gedp->ged_result_str, "ERROR: unrecognized attr subcommand %s\n", argv[1]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);

	ret = GED_ERROR;
	goto ged_attr_core_memfree;
    }

ged_attr_core_memfree:

    bu_free(paths, "db_ls paths");

    return ret;
}



#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl attr_cmd_impl = { "attr", ged_attr_core, GED_CMD_DEFAULT };
    const struct ged_cmd attr_pcmd = { &attr_cmd_impl };
    const struct ged_cmd *attr_cmds[] = { &attr_pcmd,  NULL };

    static const struct ged_plugin pinfo = { GED_API,  attr_cmds, 1 };

    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
    {
	return &pinfo;
    }
}
#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

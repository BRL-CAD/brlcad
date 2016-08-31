/*                         A T T R . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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
/** @file libged/attr.c
 *
 * The attr command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/getopt.h"
#include "bu/sort.h"
#include "./ged_private.h"


typedef enum {
    ATTR_APPEND,
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
    else
	return ATTR_UNKNOWN;
}


HIDDEN void
attr_print(struct ged *gedp, struct bu_attribute_value_set *avs,
	   const size_t max_attr_name_len)
{
    struct bu_attribute_value_pair *avpp;
    size_t i;

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
}


int
ged_attr(struct ged *gedp, int argc, const char *argv[])
{
    size_t i;
    struct directory *dp;
    struct bu_attribute_value_pair *avpp;
    static const char *usage = "{[-c sep_char] set|get|show|rm|append|sort|list} object [key [value] ... ]";
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
	return GED_ERROR;
    }

    if (scmd == ATTR_SORT) {
	for (i = 0; i < path_cnt; i++) {
	    /* for pretty printing */
	    size_t j = 0;
	    size_t max_attr_name_len  = 0;
	    size_t max_attr_value_len = 0;

	    struct bu_attribute_value_set avs;
	    bu_avs_init_empty(&avs);

	    dp = paths[i];

	    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
		bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
		return GED_ERROR;
	    }
	    bu_sort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), attr_cmp, NULL);
	    /* get a jump on calculating name and value lengths */
	    for (j = 0, avpp = avs.avp; j < avs.count; j++, avpp++) {
		size_t len = strlen(avpp->name);
		if (len > max_attr_name_len)
		    max_attr_name_len = len;
		if (avpp->value) {
		    len = strlen(avpp->value);
		    if (len > max_attr_value_len)
			max_attr_value_len = len;
		}
	    }

	    /* pretty print */
	    if ((attr_pretty_print(gedp, dp, argv[2])) != GED_OK) {
		return GED_ERROR;
	    }
	    if (argc == 3) {
		/* just list the already sorted attribute-value pairs */
		attr_print(gedp, &avs, max_attr_name_len);
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
		attr_print(gedp, &avs, max_attr_name_len);
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
		return GED_ERROR;
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
		int do_separators=argc-4; /* if more than one attribute */

		for (i = 3; i < (size_t)argc; i++) {
		    val = bu_avs_get(&avs, argv[i]);
		    if (!val) {
			bu_vls_printf(gedp->ged_result_str,
				"Object %s does not have a %s attribute\n",
				dp->d_namep,
				argv[i]);
			bu_avs_free(&avs);
			return GED_ERROR;
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
		    return GED_ERROR;
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
		    int do_separators=argc-4; /* if more than one attribute */

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
	struct bu_attribute_value_set avs;
	bu_avs_init_empty(&avs);

	for (i = 0; i < path_cnt; i++) {
	    struct bu_attribute_value_set lavs;
	    bu_avs_init_empty(&lavs);
	    dp = paths[i];
	    if (db5_get_attributes(gedp->ged_wdbp->dbip, &lavs, dp)) {
		bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
		return GED_ERROR;
	    }
	    bu_avs_merge(&avs, &lavs);
	    bu_avs_free(&lavs);
	}
	/* Now that we have them all, sort */
	bu_sort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), attr_cmp, NULL);
	/* list all the attributes */
	for (i = 0, avpp = avs.avp; i < avs.count; i++, avpp++) {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", avpp->name);
	}
	bu_avs_free(&avs);
    } else if (scmd == ATTR_SET) {
	GED_CHECK_READ_ONLY(gedp, GED_ERROR);
	/* setting attribute/value pairs */
	if ((argc - 3) % 2) {
	    bu_vls_printf(gedp->ged_result_str,
		    "Error: attribute names and values must be in pairs!!!\n");
	    return GED_ERROR;
	}
	for (i = 0; i < path_cnt; i++) {
	    size_t j = 3;
	    struct bu_attribute_value_set avs;
	    bu_avs_init_empty(&avs);
	    dp = paths[i];

	    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
		bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
		return GED_ERROR;
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
		return GED_ERROR;
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
		return GED_ERROR;
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
		return GED_ERROR;
	    }
	    /* avs is freed by db5_replace_attributes() */
	}

    } else if (scmd == ATTR_APPEND) {
	GED_CHECK_READ_ONLY(gedp, GED_ERROR);
	if ((argc-3) % 2) {
	    bu_vls_printf(gedp->ged_result_str,
			  "Error: attribute names and values must be in pairs!!!\n");
	    return GED_ERROR;
	}
	for (i = 0; i < path_cnt; i++) {
	    size_t j = 3;
	    struct bu_attribute_value_set avs;
	    bu_avs_init_empty(&avs);
	    dp = paths[i];

	    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
		bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
		return GED_ERROR;
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
		return GED_ERROR;
	    }

	    /* avs is freed by db5_replace_attributes() */
	}
    } else if (scmd == ATTR_SHOW) {
	for (i = 0; i < path_cnt; i++) {
	    /* for pretty printing */
	    size_t max_attr_name_len  = 0;
	    size_t max_attr_value_len = 0;

	    size_t j = 0;
	    size_t tabs1 = 0;
	    struct bu_attribute_value_set avs;
	    bu_avs_init_empty(&avs);
	    dp = paths[i];

	    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
		bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
		return GED_ERROR;
	    }

	    /* get a jump on calculating name and value lengths */
	    for (j = 0, avpp = avs.avp; j < avs.count; j++, avpp++) {
		size_t len = strlen(avpp->name);
		if (len > max_attr_name_len)
		    max_attr_name_len = len;
		if (avpp->value) {
		    len = strlen(avpp->value);
		    if (len > max_attr_value_len)
			max_attr_value_len = len;
		}
	    }

	    /* pretty print */
	    if ((attr_pretty_print(gedp, dp, dp->d_namep)) != GED_OK) {
		return GED_ERROR;
	    }

	    if (argc == 3) {
		/* just display all attributes */
		attr_print(gedp, &avs, max_attr_name_len);
	    } else {
		const char *val;
		size_t len;

		/* show just the specified attributes */
		for (j = 0; j < (size_t)argc; j++) {
		    len = strlen(argv[j]);
		    if (len > max_attr_name_len) {
			max_attr_name_len = len;
		    }
		}
		tabs1 = 2 + max_attr_name_len/8;
		for (j = 3; j < (size_t)argc; j++) {
		    size_t tabs2;
		    size_t k;
		    const char *c;

		    val = bu_avs_get(&avs, argv[j]);
		    if (!val && path_cnt == 1) {
			bu_vls_printf(gedp->ged_result_str,
				"Object %s does not have a %s attribute\n",
				dp->d_namep,
				argv[j]);
			bu_avs_free(&avs);
			return GED_ERROR;
		    } else {
			if (val) {
			    bu_vls_printf(gedp->ged_result_str, "\t%s", argv[j]);
			    len = strlen(val);
			    tabs2 = tabs1 - 1 - len/8;
			    for (k = 0; k < tabs2; k++) {
				bu_vls_putc(gedp->ged_result_str, '\t');
			    }
			    c = val;
			    while (*c) {
				bu_vls_putc(gedp->ged_result_str, *c);
				if (*c == '\n') {
				    for (k = 0; k < tabs1; k++) {
					bu_vls_putc(gedp->ged_result_str, '\t');
				    }
				}
				c++;
			    }
			    bu_vls_putc(gedp->ged_result_str, '\n');
			}
		    }
		}
	    }
	}
    } else {
	bu_vls_printf(gedp->ged_result_str, "ERROR: unrecognized attr subcommand %s\n", argv[1]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);

	return GED_ERROR;
    }

    return GED_OK;
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

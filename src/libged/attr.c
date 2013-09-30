/*                         A T T R . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2013 United States Government as represented by
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

#include "./ged_private.h"


/*
 * avs attribute comparison function, e.g. for qsort
 */
int
_ged_cmpattr(const void *p1, const void *p2)
{
    return bu_strcmp(((struct bu_attribute_value_pair *)p1)->name,
		     ((struct bu_attribute_value_pair *)p2)->name);
}

int
_ged_cmpattr_nocase(const void *p1, const void *p2)
{
    return bu_strcasecmp(((struct bu_attribute_value_pair *)p1)->name,
			 ((struct bu_attribute_value_pair *)p2)->name);
}

int
_ged_cmpattr_value(const void *p1, const void *p2)
{
    return bu_strcmp(((struct bu_attribute_value_pair *)p1)->value,
		     ((struct bu_attribute_value_pair *)p2)->value);
}

int
_ged_cmpattr_value_nocase(const void *p1, const void *p2)
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

typedef enum {
    ATTR_APPEND,
    ATTR_GET,
    ATTR_RM,
    ATTR_SET,
    ATTR_SHOW,
    ATTR_SORT,
    ATTR_UNKNOWN
} _attr_subcmd_t;

_attr_subcmd_t
_get_subcmd(const char* arg)
{
    /* sub-commands */
    const char APPEND[] = "append";
    const char GET[]    = "get";
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
    else
      return ATTR_UNKNOWN;
}

void
_list_attrs(struct ged *gedp, struct bu_attribute_value_set *avs,
	    const int max_attr_name_len, const int max_attr_value_len)
{
    struct bu_attribute_value_pair *avpp;
    size_t i;

    for (i = 0, avpp = avs->avp; i < avs->count; i++, avpp++) {
	bu_vls_printf(gedp->ged_result_str,
		      "\t%-*.*s"
		      "\t%-*.*s\n",
		      max_attr_name_len, max_attr_name_len, avpp->name,
		      max_attr_value_len, max_attr_value_len, avpp->value
		      );
    }
}

int
ged_attr(struct ged *gedp, int argc, const char *argv[])
{
    size_t i;
    struct directory *dp;
    struct bu_attribute_value_set avs;
    struct bu_attribute_value_pair *avpp;
    static const char *usage = "{set|get|show|rm|append|sort} object [key [value] ... ]";
    _attr_subcmd_t scmd;

    /* sort types */
    const char CASE[]         = "case";
    const char NOCASE[]       = "nocase";
    const char VALUE[]        = "value";
    const char VALUE_NOCASE[] = "value-nocase";

    /* for pretty printing */
    int max_attr_name_len  = 0;
    int max_attr_value_len = 0;

    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
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

    GED_DB_LOOKUP(gedp, dp, argv[2], LOOKUP_QUIET, GED_ERROR);

    bu_avs_init_empty(&avs);
    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
	bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
	return GED_ERROR;
    }

    scmd = _get_subcmd(argv[1]);

    if ((scmd == ATTR_SHOW && argc == 3) || scmd == ATTR_SORT) {
	/* get a jump on calculating name and value lengths */
	for (i = 0, avpp = avs.avp; i < avs.count; i++, avpp++) {
	    int len = (int)strlen(avpp->name);
	    if (len > max_attr_name_len)
		max_attr_name_len = len;
	    if (avpp->value) {
		len = (int)strlen(avpp->value);
		if (len > max_attr_value_len)
		    max_attr_value_len = len;
	    }
	}
    }

    /* default: sort attribute-value set array by attribute name (case sensitive) */
    qsort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), _ged_cmpattr);

    if (scmd == ATTR_SORT) {
	/* pretty print */
	if ((attr_pretty_print(gedp, dp, argv[2])) != GED_OK)
	    return GED_ERROR;

	if (argc == 3) {
	    /* just list the already sorted attribute-value pairs */
	    _list_attrs(gedp, &avs, max_attr_name_len, max_attr_value_len);
	} else {
	    /* argv[3] is the sort type: 'case', 'nocase', 'value', 'value-nocase' */
	    if (BU_STR_EQUIV(argv[3], NOCASE)) {
		qsort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), _ged_cmpattr_nocase);
	    } else if (BU_STR_EQUIV(argv[3], VALUE)) {
		qsort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), _ged_cmpattr_value);
	    } else if (BU_STR_EQUIV(argv[3], VALUE_NOCASE)) {
		qsort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), _ged_cmpattr_value_nocase);
	    } else if (BU_STR_EQUIV(argv[3], CASE)) {
	      ; /* don't need to do anything since this is the existing (default) sort */
	    }
	    /* just list the already sorted attribute-value pairs */
	    _list_attrs(gedp, &avs, max_attr_name_len, max_attr_value_len);
	}
    } else if (scmd == ATTR_GET) {
	if (argc == 3) {
	    /* just list all the attributes */
           for (i = 0, avpp = avs.avp; i < avs.count; i++, avpp++) {
	       bu_vls_printf(gedp->ged_result_str, "%s {%s} ", avpp->name, avpp->value);
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
		    bu_vls_printf(gedp->ged_result_str, "{%s} ", val);
		} else {
		    bu_vls_printf(gedp->ged_result_str, "%s", val);
		}
	    }
	}

	bu_avs_free(&avs);

    } else if (scmd == ATTR_SET) {
	GED_CHECK_READ_ONLY(gedp, GED_ERROR);
	/* setting attribute/value pairs */
	if ((argc - 3) % 2) {
	    bu_vls_printf(gedp->ged_result_str,
			  "Error: attribute names and values must be in pairs!!!\n");
	    bu_avs_free(&avs);
	    return GED_ERROR;
	}

	i = 3;
	while (i < (size_t)argc) {
	    if (BU_STR_EQUAL(argv[i], "region") && BU_STR_EQUAL(argv[i+1], "R")) {
		dp->d_flags |= RT_DIR_REGION;
	    }
	    (void)bu_avs_add(&avs, argv[i], argv[i+1]);
	    i += 2;
	}
	db5_standardize_avs(&avs);
	if (db5_update_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
	    bu_vls_printf(gedp->ged_result_str,
			  "Error: failed to update attributes\n");
	    bu_avs_free(&avs);
	    return GED_ERROR;
	}

	/* avs is freed by db5_update_attributes() */

    } else if (scmd == ATTR_RM) {
	GED_CHECK_READ_ONLY(gedp, GED_ERROR);
	i = 3;
	while (i < (size_t)argc) {
	    if (BU_STR_EQUAL(argv[i], "region")) {
		dp->d_flags = dp->d_flags & ~(RT_DIR_REGION);
	    }
	    (void)bu_avs_remove(&avs, argv[i]);
	    i++;
	}
	if (db5_replace_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
	    bu_vls_printf(gedp->ged_result_str,
			  "Error: failed to update attributes\n");
	    bu_avs_free(&avs);
	    return GED_ERROR;
	}

	/* avs is freed by db5_replace_attributes() */

    } else if (scmd == ATTR_APPEND) {
	GED_CHECK_READ_ONLY(gedp, GED_ERROR);
	if ((argc-3) % 2) {
	    bu_vls_printf(gedp->ged_result_str,
			  "Error: attribute names and values must be in pairs!!!\n");
	    bu_avs_free(&avs);
	    return GED_ERROR;
	}
	i = 3;
	while (i < (size_t)argc) {
	    const char *old_val;
	    if (BU_STR_EQUAL(argv[i], "region") && BU_STR_EQUAL(argv[i+1], "R")) {
		dp->d_flags |= RT_DIR_REGION;
	    }
	    old_val = bu_avs_get(&avs, argv[i]);
	    if (!old_val) {
		(void)bu_avs_add(&avs, argv[i], argv[i+1]);
	    } else {
		struct bu_vls vls = BU_VLS_INIT_ZERO;

		bu_vls_strcat(&vls, old_val);
		bu_vls_strcat(&vls, argv[i+1]);
		bu_avs_add_vls(&avs, argv[i], &vls);
		bu_vls_free(&vls);
	    }

	    i += 2;
	}
	if (db5_replace_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
	    bu_vls_printf(gedp->ged_result_str,
			  "Error: failed to update attributes\n");
	    bu_avs_free(&avs);
	    return GED_ERROR;
	}

	/* avs is freed by db5_replace_attributes() */

    } else if (scmd == ATTR_SHOW) {
	int tabs1 = 0;

	/* pretty print */
	if ((attr_pretty_print(gedp, dp, argv[2])) != GED_OK)
	    return GED_ERROR;

	if (argc == 3) {
	    /* just display all attributes */
	    _list_attrs(gedp, &avs, max_attr_name_len, max_attr_value_len);
	} else {
	    const char *val;
	    int len;

	    /* show just the specified attributes */
	    for (i = 0; i < (size_t)argc; i++) {
		len = (int)strlen(argv[i]);
		if (len > max_attr_name_len) {
		    max_attr_name_len = len;
		}
	    }
	    tabs1 = 2 + max_attr_name_len/8;
	    for (i = 3; i < (size_t)argc; i++) {
		int tabs2;
		int k;
		const char *c;

		val = bu_avs_get(&avs, argv[i]);
		if (!val) {
		    bu_vls_printf(gedp->ged_result_str,
				  "Object %s does not have a %s attribute\n",
				  dp->d_namep,
				  argv[i]);
		    bu_avs_free(&avs);
		    return GED_ERROR;
		}
		bu_vls_printf(gedp->ged_result_str, "\t%s", argv[i]);
		len = (int)strlen(val);
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

    } else {
	bu_vls_printf(gedp->ged_result_str, "ERROR: unrecognized attr subcommand %s\n", argv[1]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

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

/*                         L S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/ls.c
 *
 * The ls command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/sort.h"
#include "bu/units.h"

#include "../ged_private.h"

static void
vls_long_dpp(struct ged *gedp,
	     struct directory **list_of_names,
	     int num_in_list,
	     int aflag,		/* print all objects */
	     int cflag,		/* print combinations */
	     int rflag,		/* print regions */
	     int sflag,		/* print solids */
	     int hflag,		/* use human readable units for size */
	     int ssflag)        /* sort by object size */
{
    int i;
    int isComb=0, isRegion=0;
    int isSolid=0;
    const char *type=NULL;
    size_t max_nam_len = 0;
    size_t max_type_len = 0;
    struct directory *dp;

    if (!ssflag) {
	bu_sort((void *)list_of_names,
		(unsigned)num_in_list, (unsigned)sizeof(struct directory *),
		cmpdirname, NULL);
    } else {
	bu_sort((void *)list_of_names,
		(unsigned)num_in_list, (unsigned)sizeof(struct directory *),
		cmpdlen, NULL);
    }

    for (i = 0; i < num_in_list; i++) {
	size_t len;

	dp = list_of_names[i];
	len = strlen(dp->d_namep);
	if (len > max_nam_len)
	    max_nam_len = len;

	if (dp->d_flags & RT_DIR_REGION)
	    len = 6; /* "region" */
	else if (dp->d_flags & RT_DIR_COMB)
	    len = 4; /* "comb" */
	else if (dp->d_flags & RT_DIR_SOLID) {
	    struct rt_db_internal intern;
	    len = 9; /* "primitive" */
	    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) >= 0) {
		len = strlen(intern.idb_meth->ft_label);
		rt_db_free_internal(&intern);
	    }
	} else {
	    switch (list_of_names[i]->d_major_type) {
		case DB5_MAJORTYPE_ATTRIBUTE_ONLY:
		    len = 6;
		    break;
		case DB5_MAJORTYPE_BINARY_MIME:
		    len = strlen("binary (mime)");
		    break;
		case DB5_MAJORTYPE_BINARY_UNIF:
		    len = strlen(rt_binunif_typestr(list_of_names[i]));
		    break;
	    }
	}

	if (len > max_type_len)
	    max_type_len = len;
    }

    /*
     * i - tracks the list item
     */
    for (i = 0; i < num_in_list; ++i) {
	dp = list_of_names[i];

	if (dp->d_flags & RT_DIR_COMB) {
	    isComb = 1;
	    isSolid = 0;
	    type = "comb";

	    if (dp->d_flags & RT_DIR_REGION) {
		isRegion = 1;
		type = "region";
	    } else
		isRegion = 0;
	} else if (dp->d_flags & RT_DIR_SOLID) {
	    struct rt_db_internal intern;
	    type = "primitive";
	    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) >= 0) {
		type = intern.idb_meth->ft_label;
		rt_db_free_internal(&intern);
	    }
	    isComb = isRegion = 0;
	    isSolid = 1;
	} else {
	    switch (dp->d_major_type) {
		case DB5_MAJORTYPE_ATTRIBUTE_ONLY:
		    isSolid = 0;
		    type = "global";
		    break;
		case DB5_MAJORTYPE_BINARY_MIME:
		    isSolid = 0;
		    isRegion = 0;
		    type = "binary(mime)";
		    break;
		case DB5_MAJORTYPE_BINARY_UNIF:
		    isSolid = 0;
		    isRegion = 0;
		    type = rt_binunif_typestr(dp);
		    break;
	    }
	}

	/* print list item i */
	if (aflag ||
	    (!cflag && !rflag && !sflag) ||
	    (cflag && isComb) ||
	    (rflag && isRegion) ||
	    (sflag && isSolid)) {
	    bu_vls_printf(gedp->ged_result_str, "%s", dp->d_namep);
	    bu_vls_spaces(gedp->ged_result_str, (int)(max_nam_len - strlen(dp->d_namep)));
	    bu_vls_printf(gedp->ged_result_str, " %s", type);
	    if (type)
	       bu_vls_spaces(gedp->ged_result_str, (int)(max_type_len - strlen(type)));
	    bu_vls_printf(gedp->ged_result_str,  " %2d %2d ", dp->d_major_type, dp->d_minor_type);
	    if (!hflag) {
		bu_vls_printf(gedp->ged_result_str,  "%ld\n", (long)(dp->d_len));
	    } else {
		char hlen[6] = { '\0' };
		(void)bu_humanize_number(hlen, 5, (int64_t)dp->d_len, "",
			BU_HN_AUTOSCALE,
			BU_HN_B | BU_HN_NOSPACE | BU_HN_DECIMAL);
		bu_vls_printf(gedp->ged_result_str,  " %s\n", hlen);
	    }
	}
    }
}


/**
 * Given a pointer to a list of pointers to names and the number of names
 * in that list, sort and print that list on the same line.
 */
static void
vls_line_dpp(struct ged *gedp,
	     struct directory **list_of_names,
	     int num_in_list,
	     int aflag,	/* print all objects */
	     int cflag,	/* print combinations */
	     int rflag,	/* print regions */
	     int sflag,	/* print solids */
	     int ssflag) /* sort by size */
{
    int i;
    int isComb, isRegion;
    int isSolid;

    if (!ssflag) {
	bu_sort((void *)list_of_names,
		(unsigned)num_in_list, (unsigned)sizeof(struct directory *),
		cmpdirname, NULL);
    } else {
	bu_sort((void *)list_of_names,
		(unsigned)num_in_list, (unsigned)sizeof(struct directory *),
		cmpdlen, NULL);
    }

    /*
     * i - tracks the list item
     */
    for (i = 0; i < num_in_list; ++i) {
	if (list_of_names[i]->d_flags & RT_DIR_COMB) {
	    isComb = 1;
	    isSolid = 0;

	    if (list_of_names[i]->d_flags & RT_DIR_REGION)
		isRegion = 1;
	    else
		isRegion = 0;
	} else {
	    isComb = isRegion = 0;
	    isSolid = 1;
	}

	/* print list item i */
	if (aflag ||
	    (!cflag && !rflag && !sflag) ||
	    (cflag && isComb) ||
	    (rflag && isRegion) ||
	    (sflag && isSolid)) {
	    bu_vls_printf(gedp->ged_result_str,  "%s ", list_of_names[i]->d_namep);
	    _ged_results_add(gedp->ged_results, list_of_names[i]->d_namep);
	}
    }
}

struct _ged_ls_data {
    int aflag;	   /* print all objects without formatting */
    int cflag;	   /* print combinations */
    int rflag;	   /* print regions */
    int sflag;	   /* print solids */
    int lflag;	   /* use long format */
    int qflag;	   /* quiet flag - do a quiet lookup */
    int hflag;	   /* use human readable units for size in long format */
    int ssflag;	   /* sort by size in long format */
    int or_flag;   /* flag for "one attribute match is sufficient" mode */
    int attr_flag; /* operands are attribute name/value pairs */
    int print_help;
    struct bu_ptbl *results_obj;
    struct bu_ptbl *results_fullpath;
    int dir_flags;
};

/* select objects based on attributes and flags */
int
_ged_ls_attr_objs(struct ged *gedp, struct _ged_ls_data *ls, int argc, const char *argv[])
{
    int i;
    struct bu_attribute_value_set avs;
    int op;

    if ((argc < 2) || (argc%2 != 0)) {
	/* should be even number of name/value pairs */
	bu_log("Error: ls -A option expects even number of 'name value' pairs\n\n");
	return BRLCAD_ERROR;
    }

    op = (ls->or_flag) ? 2 : 1;

    bu_avs_init(&avs, argc, "wdb_ls_cmd avs");
    for (i = 0; i < argc; i += 2) {
	if (ls->or_flag) {
	    bu_avs_add_nonunique(&avs, (char *)argv[i], (char *)argv[i+1]);
	} else {
	    bu_avs_add(&avs, (char *)argv[i], (char *)argv[i+1]);
	}
    }

    ls->results_obj = db_lookup_by_attr(gedp->dbip, ls->dir_flags, &avs, op);
    bu_avs_free(&avs);

    return BRLCAD_OK;
}


/* select objects based on name patterns and flags */
void
_ged_ls_named_objs(struct ged *gedp, struct _ged_ls_data *ls, int argc, const char *argv[])
{
    int i, lq;

    lq = (ls->qflag) ? LOOKUP_QUIET : LOOKUP_NOISY;

    for (i = 0; i < argc; i++) {
	int is_path = 0;
	const char *pc = argv[i];
	while(*pc != '\0' && !is_path) {
	    is_path = (*pc == '/');
	    pc++;
	}
	/* If this is (potentially) a path, handle as a path, else as an object name */

	if (is_path) {
	    /* TODO - for now, just do a db_lookup on the full path, but need to rework
	     * the printing logic and formatting to properly deal with paths */
	    struct directory *dp = db_lookup(gedp->dbip, argv[i], lq);
	    if (dp != RT_DIR_NULL && ((dp->d_flags & ls->dir_flags) != 0)) {
		bu_ptbl_ins(ls->results_obj, (long *)dp);
	    }

	} else {
	    struct directory *dp = db_lookup(gedp->dbip, argv[i], lq);
	    if (dp != RT_DIR_NULL && ((dp->d_flags & ls->dir_flags) != 0)) {
		bu_ptbl_ins(ls->results_obj, (long *)dp);
	    }
	}
    }
}

void
_ged_ls_data_init(struct _ged_ls_data *d)
{
    if (!d) return;
    d->aflag = 0;
    d->cflag = 0;
    d->rflag = 0;
    d->sflag = 0;
    d->lflag = 0;
    d->qflag = 0;
    d->hflag = 0;
    d->ssflag = 0;
    d->or_flag = 0;
    d->attr_flag = 0;
    d->print_help = 0;
    d->results_obj = NULL;
    d->results_fullpath = NULL;
    d->dir_flags = 0;
}


static const struct bu_arg_option ls_options[] = {
    BU_ARG_FLAG("h", "help", struct _ged_ls_data, print_help,
	"Print help and exit"),
    BU_ARG_FLAG("a", "all", struct _ged_ls_data, aflag,
	"Do not ignore hidden objects"),
    BU_ARG_FLAG("c", "combs", struct _ged_ls_data, cflag,
	"List combinations"),
    BU_ARG_FLAG("r", "regions", struct _ged_ls_data, rflag, "List regions"),
    BU_ARG_FLAG("p", "primitives", struct _ged_ls_data, sflag,
	"List primitives"),
    BU_ARG_ALIAS("s", NULL, "primitives", 0),
    BU_ARG_FLAG("q", "quiet", struct _ged_ls_data, qflag,
	"Suppress informational output messages during database lookup"),
    BU_ARG_FLAG("l", NULL, struct _ged_ls_data, lflag,
	"Use long reporting format"),
    BU_ARG_FLAG("H", "human-readable", struct _ged_ls_data, hflag,
	"When printing in long format, use human-readable object sizes"),
    BU_ARG_FLAG("S", "sort", struct _ged_ls_data, ssflag,
	"Sort by object size"),
    BU_ARG_FLAG("A", "attributes", struct _ged_ls_data, attr_flag,
	"Treat operands as attribute name/value pairs"),
    BU_ARG_FLAG("o", "or", struct _ged_ls_data, or_flag,
	"In attribute mode, match any attribute pair"),
    BU_ARG_OPTION_NULL
};
static const struct bu_arg_operand ls_help_operands[] = {
    BU_ARG_OPERAND("raw_arguments", BU_ARG_VALUE_RAW, 0,
	BU_ARG_COUNT_UNLIMITED, "Arguments ignored when requesting help", NULL),
    BU_ARG_OPERAND_NULL
};
static const struct bu_arg_operand ls_attribute_roles[] = {
    BU_ARG_OPERAND("attribute", BU_ARG_VALUE_STRING, 1, 1,
	"Attribute name", NULL),
    BU_ARG_OPERAND("value", BU_ARG_VALUE_STRING, 1, 1,
	"Attribute value", NULL),
    BU_ARG_OPERAND_NULL
};
static const struct bu_arg_operand_group ls_attribute_groups[] = {
    BU_ARG_OPERAND_GROUP("attribute_value_group", ls_attribute_roles, 1,
	BU_ARG_COUNT_UNLIMITED, "Repeated attribute name/value pairs"),
    BU_ARG_OPERAND_GROUP_NULL
};
#define LS_OBJECT_OPERANDS(_name, _provider) \
    static const struct bu_arg_operand _name[] = { \
	BU_ARG_OPERAND("objects", BU_ARG_VALUE_DB_PATH, 0, \
	    BU_ARG_COUNT_UNLIMITED, "Object or path patterns", _provider), \
	BU_ARG_OPERAND_NULL \
    }
LS_OBJECT_OPERANDS(ls_default_operands, "ged.db_path_or_pattern");
LS_OBJECT_OPERANDS(ls_all_operands, "ged.db_path_any_hidden_or_pattern");
LS_OBJECT_OPERANDS(ls_comb_operands, "ged.db_path_combinations_or_pattern");
LS_OBJECT_OPERANDS(ls_primitive_operands, "ged.db_path_primitives_or_pattern");
LS_OBJECT_OPERANDS(ls_region_operands, "ged.db_path_regions_or_pattern");
#undef LS_OBJECT_OPERANDS
static const char * const ls_help_option[] = {"help", NULL};
static const char * const ls_attribute_option[] = {"attributes", NULL};
static const char * const ls_region_option[] = {"regions", NULL};
static const char * const ls_primitive_option[] = {"primitives", NULL};
static const char * const ls_comb_option[] = {"combs", NULL};
static const char * const ls_all_option[] = {"all", NULL};
static const struct bu_arg_schema_case ls_cases[] = {
    BU_ARG_SCHEMA_CASE("help", "Display command help",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, ls_help_option, ls_help_operands, NULL),
    BU_ARG_SCHEMA_CASE("attributes", "Match attribute name/value pairs",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, ls_attribute_option, NULL,
	ls_attribute_groups),
    BU_ARG_SCHEMA_CASE("regions", "List region patterns",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, ls_region_option, ls_region_operands, NULL),
    BU_ARG_SCHEMA_CASE("primitives", "List primitive patterns",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, ls_primitive_option,
	ls_primitive_operands, NULL),
    BU_ARG_SCHEMA_CASE("combinations", "List combination patterns",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, ls_comb_option, ls_comb_operands, NULL),
    BU_ARG_SCHEMA_CASE("all", "List patterns including hidden objects",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, ls_all_option, ls_all_operands, NULL),
    BU_ARG_SCHEMA_CASE_DEFAULT("objects", "List object or path patterns",
	ls_default_operands, NULL),
    BU_ARG_SCHEMA_CASE_NULL
};
static const struct bu_arg_schema ls_cmd_schema =
    BU_ARG_SCHEMA_CASED("ls", "List database objects", ls_options,
	BU_ARG_PARSE_INTERSPERSED, ls_cases);
static const struct bu_arg_schema t_cmd_schema =
    BU_ARG_SCHEMA_CASED("t", "List database objects", ls_options,
	BU_ARG_PARSE_INTERSPERSED, ls_cases);


static void
ls_show_help(struct ged *gedp, const char *command)
{
    (void)ged_cmd_help_append(gedp->ged_result_str, command, command);
}

/**
 * List objects in this database
 */
int
ged_ls_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct directory **dirp0 = (struct directory **)NULL;
    struct _ged_ls_data ls;
    int operand_count;
    int operand_index;
    const struct bu_arg_schema *schema;
    const char *command = argv[0];

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize */
    _ged_ls_data_init(&ls);
    bu_vls_trunc(gedp->ged_result_str, 0);
    ged_results_clear(gedp->ged_results);

    schema = BU_STR_EQUAL(command, "t") ? &t_cmd_schema : &ls_cmd_schema;
    argc--; argv++;
    operand_index = bu_arg_schema_parse_complete(schema, &ls,
	gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	if (bu_arg_schema_option_present(schema, argc, argv, "attributes"))
	    bu_vls_printf(gedp->ged_result_str, "attribute value required\n");
	ls_show_help(gedp, command);
	return BRLCAD_ERROR;
    }
    operand_count = argc - operand_index;
    argv += operand_index;
    if (ls.print_help) {
	ls_show_help(gedp, command);
	return BRLCAD_OK;
    }
    argc = operand_count;

    /* Set object type filter via flags */
    ls.dir_flags = 0;
    if (ls.aflag) ls.dir_flags = -1;
    if (ls.cflag) ls.dir_flags = RT_DIR_COMB;
    if (ls.sflag) ls.dir_flags = RT_DIR_SOLID;
    if (ls.rflag) ls.dir_flags = RT_DIR_REGION;
    if (!ls.dir_flags) ls.dir_flags = -1 ^ RT_DIR_HIDDEN;

    /* create list of selected objects from database */
    if (ls.attr_flag) {

	/* In this scenario we're only going to get object names, and db_lookup_by_attr will provide
	 * the table for us, so don't init either of them.  */
	if (_ged_ls_attr_objs(gedp, &ls, argc, argv) != BRLCAD_OK) {
	    return BRLCAD_ERROR;
	}

    } else {

	/* Object name results are possible both with and without arguments -
	 * init that table regardless */
	BU_ALLOC(ls.results_obj, struct bu_ptbl);
	bu_ptbl_init(ls.results_obj, 128, "object name results");

	if (argc > 0) {

	    /* If we have arguments we might also have full path results - set
	     * up the fullpath table */
	    BU_ALLOC(ls.results_fullpath, struct bu_ptbl);
	    bu_ptbl_init(ls.results_fullpath, 128, "full path results");

	    _ged_ls_named_objs(gedp, &ls, argc, argv);

	} else {

	    /* No guidance at all - just list all names. Walk the directory
	     * list adding pointers (to the directory entries) to the tbl.
	     */
	    FOR_ALL_DIRECTORY_START(dp, gedp->dbip)
		if (!ls.aflag && (dp->d_flags & RT_DIR_HIDDEN)) continue;
		if (((dp->d_flags & ls.dir_flags) != 0)) {
		    bu_ptbl_ins(ls.results_obj, (long *)dp);
		}
	    FOR_ALL_DIRECTORY_END;
	}
    }

    dirp0 = (struct directory **)ls.results_obj->buffer;
    if (ls.lflag)
	vls_long_dpp(gedp, dirp0, (int)BU_PTBL_LEN(ls.results_obj), ls.aflag, ls.cflag, ls.rflag, ls.sflag, ls.hflag, ls.ssflag);
    else if (ls.aflag || ls.cflag || ls.rflag || ls.sflag)
	vls_line_dpp(gedp, dirp0, (int)BU_PTBL_LEN(ls.results_obj), ls.aflag, ls.cflag, ls.rflag, ls.sflag, ls.ssflag);
    else {
	_ged_vls_col_pr4v(gedp->ged_result_str, dirp0, (int)BU_PTBL_LEN(ls.results_obj), 0, ls.ssflag, ged_terminal_columns(gedp));
	_ged_results_add(gedp->ged_results, bu_vls_addr(gedp->ged_result_str));
    }

    if (ls.results_obj) {
	bu_ptbl_free(ls.results_obj);
	bu_free((void *)ls.results_obj, "object name results");
    }

    if (ls.results_fullpath) {
	bu_ptbl_free(ls.results_fullpath);
	bu_free((void *)ls.results_fullpath, "full path results");
    }

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_LS_COMMANDS(X, XID) \
    X(ls,  ged_ls_core,   GED_CMD_DEFAULT, &ls_cmd_schema) \
    X(t,   ged_ls_core,   GED_CMD_DEFAULT, &t_cmd_schema)

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_LS_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_ls", 1, GED_LS_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

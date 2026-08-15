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
#include "bu/opt.h"
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


#define LS_OPTIONS(args) \
    BU_OPT_FLAG(args, "h", "help", print_help, "Print help and exit"), \
    BU_OPT_FLAG(args, "a", "all", aflag, "Do not ignore hidden objects"), \
    BU_OPT_FLAG(args, "c", "combs", cflag, "List combinations"), \
    BU_OPT_FLAG(args, "r", "regions", rflag, "List regions"), \
    BU_OPT_FLAG(args, "p", "primitives", sflag, "List primitives"), \
    BU_OPT_FLAG(args, "s", NULL, sflag, "List primitives"), \
    BU_OPT_FLAG(args, "q", "quiet", qflag, \
	"Suppress informational output messages during database lookup"), \
    BU_OPT_FLAG(args, "l", NULL, lflag, "Use long reporting format"), \
    BU_OPT_FLAG(args, "H", "human-readable", hflag, \
	"When printing in long format, use human-readable object sizes"), \
    BU_OPT_FLAG(args, "S", "sort", ssflag, "Sort by object size"), \
    BU_OPT_FLAG(args, "A", "attributes", attr_flag, \
	"Treat operands as attribute name/value pairs"), \
    BU_OPT_FLAG(args, "o", "or", or_flag, \
	"In attribute mode, match any attribute pair"),

BU_OPT_DESC_BUILDER(ls_options, struct _ged_ls_data, LS_OPTIONS);

static const ged_opt_rule ls_opt_rules[] = {
    GED_RULE_ALIAS("s", "primitives"),
    GED_RULE_WHEN_HELP("help", "Display command help", "raw_arguments:raw*"),
    GED_RULE_WHEN_HELP("attributes", "Match attribute name/value pairs",
	"(attribute:string value:string)+"),
    GED_RULE_OTHERWISE_HELP("List object or path patterns",
	"objects:path@ged.db_path_or_pattern*"),
    GED_RULE_DB_PATHS("objects", GED_OPT_DB_GEOMETRY, "all", GED_OPT_DB_ANY_HIDDEN),
    GED_RULE_DB_TYPE("objects", "regions", GED_OPT_DB_REGIONS),
    GED_RULE_DB_TYPE("objects", "primitives", GED_OPT_DB_PRIMITIVES),
    GED_RULE_DB_TYPE("objects", "combs", GED_OPT_DB_COMBINATIONS),
    GED_RULE_NULL
};

static const ged_opt_spec ls_opt_spec =
    GED_OPT_FORMS("ls", "List database objects", ls_options,
	ls_opt_rules);

static const ged_opt_spec t_opt_spec =
    GED_OPT_FORMS("t", "List database objects", ls_options,
	ls_opt_rules);


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
    int operand_count = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize */
    _ged_ls_data_init(&ls);
    bu_vls_trunc(gedp->ged_result_str, 0);
    ged_results_clear(gedp->ged_results);

    operand_count = bu_opt_parse_build(gedp->ged_result_str, argc - 1,
	argv + 1, ls_options, &ls);
    if (operand_count < 0) {
	ls_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
    }
    if (ls.print_help) {
	ls_show_help(gedp, argv[0]);
	return BRLCAD_OK;
    }
    if (ls.attr_flag && (operand_count < 2 || operand_count % 2)) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", operand_count % 2 ?
	    "attribute value required" :
	    "at least one attribute name/value pair required");
	ls_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
    }
    argv += 1;
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
	_ged_vls_col_pr4v(gedp->ged_result_str, dirp0, (int)BU_PTBL_LEN(ls.results_obj), 0, ls.ssflag);
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
    X(ls,  ged_ls_core,   GED_CMD_DEFAULT, &ls_opt_spec) \
    X(t,   ged_ls_core,   GED_CMD_DEFAULT, &t_opt_spec)

GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC(GED_LS_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_OPT_SPEC("libged_ls", 1, GED_LS_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

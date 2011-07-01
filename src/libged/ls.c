/*                         L S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


#define RT_TERMINAL_WIDTH 80
#define RT_COLUMNS ((RT_TERMINAL_WIDTH + V4_MAXNAME - 1) / V4_MAXNAME)


/**
 * G E D _ G E T S P A C E
 *
 * This routine walks through the directory entry list and mallocs
 * enough space for pointers to hold the number of entries specified
 * by the argument if > 0.
 *
 */
struct directory **
_ged_getspace(struct db_i *dbip,
	      size_t num_entries)
{
    struct directory **dir_basep;

    if (num_entries == 0)
	num_entries = db_directory_size(dbip);

    /* Allocate and cast num_entries worth of pointers */
    dir_basep = (struct directory **) bu_calloc((num_entries+1), sizeof(struct directory *), "_ged_getspace *dir[]");
    return dir_basep;
}


/**
 * _ G E D _ C M P D I R N A M E
 *
 * Given two pointers to pointers to directory entries, do a string
 * compare on the respective names and return that value.
 */
static int
cmpdirname(const genptr_t a, const genptr_t b)
{
    struct directory **dp1, **dp2;

    dp1 = (struct directory **)a;
    dp2 = (struct directory **)b;
    return bu_strcmp((*dp1)->d_namep, (*dp2)->d_namep);
}


/**
 * _ G E D _ V L S _ C O L _ P R 4 V
 *
 * Given a pointer to a list of pointers to names and the number of
 * names in that list, sort and print that list in column order over
 * four columns.
 */
void
_ged_vls_col_pr4v(struct bu_vls *vls,
		  struct directory **list_of_names,
		  size_t num_in_list,
		  int no_decorate)
{
    size_t lines, i, j, k, this_one;
    size_t namelen;
    size_t maxnamelen;	/* longest name in list */
    size_t cwidth;	/* column width */
    size_t numcol;	/* number of columns */

    qsort((genptr_t)list_of_names,
	  (unsigned)num_in_list, (unsigned)sizeof(struct directory *),
	  (int (*)(const void *, const void *))cmpdirname);

    /*
     * Traverse the list of names, find the longest name and set the
     * the column width and number of columns accordingly.  If the
     * longest name is greater than 80 characters, the number of
     * columns will be one.
     */
    maxnamelen = 0;
    for (k=0; k < num_in_list; k++) {
	namelen = strlen(list_of_names[k]->d_namep);
	if (namelen > maxnamelen)
	    maxnamelen = namelen;
    }

    if (maxnamelen <= 16)
	maxnamelen = 16;
    cwidth = maxnamelen + 4;

    if (cwidth > 80)
	cwidth = 80;
    numcol = RT_TERMINAL_WIDTH / cwidth;

    /*
     * For the number of (full and partial) lines that will be needed,
     * print in vertical format.
     */
    lines = (num_in_list + (numcol - 1)) / numcol;
    for (i=0; i < lines; i++) {
	for (j=0; j < numcol; j++) {
	    this_one = j * lines + i;
	    bu_vls_printf(vls, "%s", list_of_names[this_one]->d_namep);
	    namelen = strlen(list_of_names[this_one]->d_namep);

	    /*
	     * Region and ident checks here....  Since the code has
	     * been modified to push and sort on pointers, the
	     * printing of the region and ident flags must be delayed
	     * until now.  There is no way to make the decision on
	     * where to place them before now.
	     */
	    if (!no_decorate && list_of_names[this_one]->d_flags & RT_DIR_COMB) {
		bu_vls_putc(vls, '/');
		namelen++;
	    }

	    if (!no_decorate && list_of_names[this_one]->d_flags & RT_DIR_REGION) {
		bu_vls_putc(vls, 'R');
		namelen++;
	    }

	    /*
	     * Size check (partial lines), and line termination.  Note
	     * that this will catch the end of the lines that are full
	     * too.
	     */
	    if (this_one + lines >= num_in_list) {
		bu_vls_putc(vls, '\n');
		break;
	    } else {
		/*
		 * Pad to next boundary as there will be another entry
		 * to the right of this one.
		 */
		while (namelen++ < cwidth)
		    bu_vls_putc(vls, ' ');
	    }
	}
    }
}


static void
vls_long_dpp(struct ged *gedp,
	     struct directory **list_of_names,
	     int num_in_list,
	     int aflag,		/* print all objects */
	     int cflag,		/* print combinations */
	     int rflag,		/* print regions */
	     int sflag)		/* print solids */
{
    int i;
    int isComb=0, isRegion=0;
    int isSolid=0;
    const char *type=NULL;
    size_t max_nam_len = 0;
    size_t max_type_len = 0;
    struct directory *dp;

    qsort((genptr_t)list_of_names,
	  (unsigned)num_in_list, (unsigned)sizeof(struct directory *),
	  (int (*)(const void *, const void *))cmpdirname);

    for (i=0; i < num_in_list; i++) {
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
	    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) >= 0) {
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
		    len = strlen(binu_types[list_of_names[i]->d_minor_type]);
		    break;
	    }
	}

	if (len > max_type_len)
	    max_type_len = len;
    }

    /*
     * i - tracks the list item
     */
    for (i=0; i < num_in_list; ++i) {
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
	    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) >= 0) {
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
		    type = binu_types[dp->d_minor_type];
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
	    bu_vls_spaces(gedp->ged_result_str, (int)(max_type_len - strlen(type)));
	    bu_vls_printf(gedp->ged_result_str,  " %2d %2d %ld\n",
			  dp->d_major_type, dp->d_minor_type, (long)(dp->d_len));
	}
    }
}


/**
 * G E D _ V L S _ L I N E _ D P P
 *
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
	     int sflag)	/* print solids */
{
    int i;
    int isComb, isRegion;
    int isSolid;

    qsort((genptr_t)list_of_names,
	  (unsigned)num_in_list, (unsigned)sizeof(struct directory *),
	  (int (*)(const void *, const void *))cmpdirname);

    /*
     * i - tracks the list item
     */
    for (i=0; i < num_in_list; ++i) {
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
	}
    }
}


/**
 * List objects in this database
 */
int
ged_ls(struct ged *gedp, int argc, const char *argv[])
{
    struct bu_vls vls;
    struct directory *dp;
    size_t i;
    int c;
    int aflag = 0;		/* print all objects without formatting */
    int cflag = 0;		/* print combinations */
    int rflag = 0;		/* print regions */
    int sflag = 0;		/* print solids */
    int lflag = 0;		/* use long format */
    int qflag = 0;		/* quiet flag - do a quiet lookup */
    int attr_flag = 0;		/* arguments are attribute name/value pairs */
    int or_flag = 0;		/* flag indicating that any one attribute match is sufficient
				 * default is all attributes must match.
				 */
    struct directory **dirp;
    struct directory **dirp0 = (struct directory **)NULL;
    static const char *usage = "[-A name/value pairs] OR [-acrslopq] object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_vls_init(&vls);

    bu_optind = 1;	/* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "acrslopqA")) != -1) {
	switch (c) {
	    case 'A':
		attr_flag = 1;
		break;
	    case 'o':
		or_flag = 1;
		break;
	    case 'a':
		aflag = 1;
		break;
	    case 'c':
		cflag = 1;
		break;
	    case 'q':
		qflag = 1;
		break;
	    case 'r':
		rflag = 1;
		break;
	    case 's':
	    case 'p':
		sflag = 1;
		break;
	    case 'l':
		lflag = 1;
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Unrecognized option - %c", c);
		return GED_ERROR;
	}
    }
    /* skip options processed plus command name */
    argc -= bu_optind;
    argv += bu_optind;

    /* create list of selected objects from database */
    if (attr_flag) {
	/* select objects based on attributes */
	struct bu_ptbl *tbl;
	struct bu_attribute_value_set avs;
	int dir_flags;
	int op;
	if ((argc < 2) || (argc%2 != 0)) {
	    /* should be even number of name/value pairs */
	    bu_log("ls -A option expects even number of 'name value' pairs\n");
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return TCL_ERROR;
	}

	if (or_flag) {
	    op = 2;
	} else {
	    op = 1;
	}

	dir_flags = 0;
	if (aflag) dir_flags = -1;
	if (cflag) dir_flags = RT_DIR_COMB;
	if (sflag) dir_flags = RT_DIR_SOLID;
	if (rflag) dir_flags = RT_DIR_REGION;
	if (!dir_flags) dir_flags = -1 ^ RT_DIR_HIDDEN;

	bu_avs_init(&avs, argc, "wdb_ls_cmd avs");
	for (i = 0; i < (size_t)argc; i += 2) {
	    if (or_flag) {
		bu_avs_add_nonunique(&avs, (char *)argv[i], (char *)argv[i+1]);
	    } else {
		bu_avs_add(&avs, (char *)argv[i], (char *)argv[i+1]);
	    }
	}

	tbl = db_lookup_by_attr(gedp->ged_wdbp->dbip, dir_flags, &avs, op);
	bu_avs_free(&avs);

	dirp = _ged_getspace(gedp->ged_wdbp->dbip, BU_PTBL_LEN(tbl));
	dirp0 = dirp;
	for (i=0; i<BU_PTBL_LEN(tbl); i++) {
	    *dirp++ = (struct directory *)BU_PTBL_GET(tbl, i);
	}

	bu_ptbl_free(tbl);
	bu_free((char *)tbl, "wdb_ls_cmd ptbl");
    } else if (argc > 0) {
	/* Just list specified names */
	dirp = _ged_getspace(gedp->ged_wdbp->dbip, argc);
	dirp0 = dirp;
	/*
	 * Verify the names, and add pointers to them to the array.
	 */
	for (i = 0; i < (size_t)argc; i++) {
	    if (qflag) {
		dp = db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_QUIET);
	    } else {
		dp = db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_NOISY);
	    }
	    if (dp  == RT_DIR_NULL)
		continue;
	    *dirp++ = dp;
	}
    } else {
	/* Full table of contents */
	dirp = _ged_getspace(gedp->ged_wdbp->dbip, 0);	/* Enough for all */
	dirp0 = dirp;
	/*
	 * Walk the directory list adding pointers (to the directory
	 * entries) to the array.
	 */
	for (i = 0; i < RT_DBNHASH; i++)
	    for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		if (!aflag && (dp->d_flags & RT_DIR_HIDDEN))
		    continue;
		*dirp++ = dp;
	    }
    }

    if (lflag)
	vls_long_dpp(gedp, dirp0, (int)(dirp - dirp0), aflag, cflag, rflag, sflag);
    else if (aflag || cflag || rflag || sflag)
	vls_line_dpp(gedp, dirp0, (int)(dirp - dirp0), aflag, cflag, rflag, sflag);
    else
	_ged_vls_col_pr4v(gedp->ged_result_str, dirp0, (int)(dirp - dirp0), 0);

    bu_free((genptr_t)dirp0, "_ged_getspace dp[]");

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

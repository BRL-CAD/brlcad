/*                       W D B _ O B J . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2014 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/wdb_obj.c
 *
 * A quasi-object-oriented database interface.
 *
 * A database object contains the attributes and methods for
 * controlling a BRL-CAD database.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <assert.h>
#include "bio.h"

#include "tcl.h"


#include "bn.h"
#include "bu/cmd.h"
#include "vmath.h"
#include "db.h"
#include "rtgeom.h"
#include "wdb.h"
#include "mater.h"
#include "obj.h"

#include "./ged_private.h"


#define WDB_TCL_READ_ERR { \
	Tcl_AppendResult(wdbp->wdb_interp, "Database read error, aborting.\n", (char *)NULL); \
    }

#define WDB_TCL_READ_ERR_return { \
	WDB_TCL_READ_ERR; \
	return TCL_ERROR; }

#define WDB_TCL_WRITE_ERR { \
	Tcl_AppendResult(wdbp->wdb_interp, "Database write error, aborting.\n", (char *)NULL); \
	WDB_TCL_ERROR_RECOVERY_SUGGESTION; }

#define WDB_TCL_WRITE_ERR_return { \
	WDB_TCL_WRITE_ERR; \
	return TCL_ERROR; }

#define WDB_TCL_ALLOC_ERR { \
	Tcl_AppendResult(wdbp->wdb_interp, "\
An error has occurred while adding a new object to the database.\n", (char *)NULL); \
	WDB_TCL_ERROR_RECOVERY_SUGGESTION; }

#define WDB_TCL_ALLOC_ERR_return { \
	WDB_TCL_ALLOC_ERR; \
	return TCL_ERROR; }

#define WDB_TCL_DELETE_ERR(_name) { \
	Tcl_AppendResult(wdbp->wdb_interp, "An error has occurred while deleting '", _name, \
			 "' from the database.\n", (char *)NULL);\
	WDB_TCL_ERROR_RECOVERY_SUGGESTION; }

#define WDB_TCL_DELETE_ERR_return (_name) {  \
	WDB_TCL_DELETE_ERR(_name); \
	return TCL_ERROR;  }

/* A verbose message to attempt to soothe and advise the user */
#define WDB_TCL_ERROR_RECOVERY_SUGGESTION\
    Tcl_AppendResult(wdbp->wdb_interp, "\
The in-memory table of contents may not match the status of the on-disk\n\
database.  The on-disk database should still be intact.  For safety, \n\
you should exit now, and resolve the I/O problem, before continuing.\n", (char *)NULL)

#define WDB_READ_ERR { \
	bu_log("Database read error, aborting\n"); }

#define WDB_READ_ERR_return { \
	WDB_READ_ERR; \
	return;  }

#define WDB_WRITE_ERR { \
	bu_log("Database write error, aborting.\n"); \
	WDB_ERROR_RECOVERY_SUGGESTION; }

#define WDB_WRITE_ERR_return { \
	WDB_WRITE_ERR; \
	return;  }

/* For errors from db_diradd() or db_alloc() */
#define WDB_ALLOC_ERR { \
	bu_log("\nAn error has occurred while adding a new object to the database.\n"); \
	WDB_ERROR_RECOVERY_SUGGESTION; }

#define WDB_ALLOC_ERR_return { \
	WDB_ALLOC_ERR; \
	return;  }

/* A verbose message to attempt to soothe and advise the user */
#define WDB_ERROR_RECOVERY_SUGGESTION\
    bu_log(WDB_ERROR_RECOVERY_MESSAGE)

#define WDB_ERROR_RECOVERY_MESSAGE "\
The in-memory table of contents may not match the status of the on-disk\n\
database.  The on-disk database should still be intact.  For safety, \n\
you should exit now, and resolve the I/O problem, before continuing.\n"

#define WDB_TCL_CHECK_READ_ONLY \
    if (wdbp->wdb_interp) { \
	if (wdbp->dbip->dbi_read_only) { \
	    Tcl_AppendResult(wdbp->wdb_interp, "Sorry, this database is READ-ONLY\n", (char *)NULL); \
	    return TCL_ERROR; \
	} \
    } else { \
	bu_log("Sorry, this database is READ-ONLY\n"); \
    }

struct wdb_killtree_data {
    Tcl_Interp *interp;
    int notify;
};


/**
 * structure used by the dbconcat command for keeping tract of where
 * objects are being copied to and from, and what type of affix to
 * use.
 */
struct concat_data {
    int copy_mode;
    struct db_i *old_dbip;
    struct db_i *new_dbip;
    struct bu_vls affix;
};


#define NO_AFFIX 1<<0
#define AUTO_PREFIX 1<<1
#define AUTO_SUFFIX 1<<2
#define CUSTOM_PREFIX 1<<3
#define CUSTOM_SUFFIX 1<<4
#define V4_MAXNAME NAMESIZE

/* Defined in wdb_cmd_std.c */
extern int wdb_comb_std_tcl(void *data,
			    int argc,
			    const char *argv[]);

/* Defined in dg_obj.c */
extern void dgo_impending_wdb_close(struct rt_wdb *wdbp);
extern void dgo_zapall(struct rt_wdb *wdbp);
extern void dgo_eraseobjall_callback(struct db_i *dbip,
				     struct directory *dp,
				     int notify);
extern void dgo_notifyWdb(struct rt_wdb *wdbp);

/* wdb_track.c */
extern int wdb_track_cmd(void *data, int argc, const char *argv[]);

/* wdb_importFg4Section.c */
extern int wdb_importFg4Section_cmd(void *data, int argc, const char *argv[]);

static int pathListNoLeaf = 0;


/**
 * Given two pointers to pointers to directory entries, do a string compare
 * on the respective names and return that value.
 */
int
wdb_cmpdirname(const void *a, const void *b, void *UNUSED(arg))
{
    struct directory **dp1, **dp2;

    dp1 = (struct directory **)a;
    dp2 = (struct directory **)b;
    return bu_strcmp((*dp1)->d_namep, (*dp2)->d_namep);
}


#define RT_TERMINAL_WIDTH 80
#define RT_COLUMNS ((RT_TERMINAL_WIDTH + V4_MAXNAME - 1) / V4_MAXNAME)

void
wdb_vls_col_item(struct bu_vls *str,
		 const char *cp,
		 int *ccp,		/* column count pointer */
		 int *clp)		/* column length pointer */
{
    /* Output newline if last column printed. */
    if (*ccp >= RT_COLUMNS || (*clp+V4_MAXNAME-1) >= RT_TERMINAL_WIDTH) {
	/* line now full */
	bu_vls_putc(str, '\n');
	*ccp = 0;
    } else if (*ccp != 0) {
	/* Space over before starting new column */
	do {
	    bu_vls_putc(str, ' ');
	    ++*clp;
	}  while ((*clp % V4_MAXNAME) != 0);
    }
    /* Output string and save length for next tab. */
    *clp = 0;
    while (*cp != '\0') {
	bu_vls_putc(str, *cp);
	++cp;
	++*clp;
    }
    ++*ccp;
}


void
wdb_vls_col_eol(struct bu_vls *str,
		int *ccp,
		int *clp)
{
    if (*ccp != 0)		/* partial line */
	bu_vls_putc(str, '\n');
    *ccp = 0;
    *clp = 0;
}


/**
 * Given a pointer to a list of pointers to names and the number of
 * names in that list, sort and print that list in column order over
 * four columns.
 */
void
wdb_vls_col_pr4v(struct bu_vls *vls,
		 struct directory **list_of_names,
		 int num_in_list,
		 int no_decorate)
{
    int lines, i, j, k, namelen, this_one;
    int maxnamelen;	/* longest name in list */
    int cwidth;		/* column width */
    int numcol;		/* number of columns */

    bu_sort((void *)list_of_names,
	    (unsigned)num_in_list, (unsigned)sizeof(struct directory *),
	    wdb_cmpdirname, NULL);

    /*
     * Traverse the list of names, find the longest name and set the
     * the column width and number of columns accordingly.
     * If the longest name is greater than 80 characters, the number of columns
     * will be one.
     */
    maxnamelen = 0;
    for (k = 0; k < num_in_list; k++) {
	namelen = (int)strlen(list_of_names[k]->d_namep);
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
    for (i = 0; i < lines; i++) {
	for (j = 0; j < numcol; j++) {
	    this_one = j * lines + i;
	    bu_vls_printf(vls, "%s", list_of_names[this_one]->d_namep);
	    namelen = (int)strlen(list_of_names[this_one]->d_namep);

	    /*
	     * Region and ident checks here....  Since the code
	     * has been modified to push and sort on pointers,
	     * the printing of the region and ident flags must
	     * be delayed until now.  There is no way to make the
	     * decision on where to place them before now.
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
	     * Size check (partial lines), and line termination.
	     * Note that this will catch the end of the lines
	     * that are full too.
	     */
	    if (this_one + lines >= num_in_list) {
		bu_vls_putc(vls, '\n');
		break;
	    } else {
		/*
		 * Pad to next boundary as there will be
		 * another entry to the right of this one.
		 */
		while (namelen++ < cwidth)
		    bu_vls_putc(vls, ' ');
	    }
	}
    }
}


void
wdb_vls_long_dpp(struct rt_wdb *wdbp,
		 struct bu_vls *vls,
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
    int max_nam_len = 0;
    int max_type_len = 0;
    struct directory *dp;

    bu_sort((void *)list_of_names,
	    (unsigned)num_in_list, (unsigned)sizeof(struct directory *),
	    wdb_cmpdirname, NULL);

    for (i = 0; i < num_in_list; i++) {
	int len;

	dp = list_of_names[i];
	len = (int)strlen(dp->d_namep);
	if (len > max_nam_len)
	    max_nam_len = len;

	if (dp->d_flags & RT_DIR_REGION)
	    len = 6; /* "region" */
	else if (dp->d_flags & RT_DIR_COMB)
	    len = 4; /* "comb" */
	else if (dp->d_flags & RT_DIR_SOLID) {
	    struct rt_db_internal intern;
	    len = 9; /* "primitive" */
	    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) >= 0) {
		len = strlen(intern.idb_meth->ft_label);
		rt_db_free_internal(&intern);
	    }
	} else {
	    switch (list_of_names[i]->d_major_type) {
		case DB5_MAJORTYPE_ATTRIBUTE_ONLY:
		    len = 6;
		    break;
		case DB5_MAJORTYPE_BINARY_MIME:
		    len = (int)strlen("binary(mime)");
		    break;
		case DB5_MAJORTYPE_BINARY_UNIF:
		    len = (int)strlen(binu_types[list_of_names[i]->d_minor_type]);
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
	if (list_of_names[i]->d_flags & RT_DIR_COMB) {
	    isComb = 1;
	    isSolid = 0;
	    type = "comb";

	    if (list_of_names[i]->d_flags & RT_DIR_REGION) {
		isRegion = 1;
		type = "region";
	    } else
		isRegion = 0;
	} else if (list_of_names[i]->d_flags & RT_DIR_SOLID) {
	    struct rt_db_internal intern;
	    type = "primitive";
	    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) >= 0) {
		type = intern.idb_meth->ft_label;
		rt_db_free_internal(&intern);
	    }
	    isComb = isRegion = 0;
	    isSolid = 1;
	} else {
	    switch (list_of_names[i]->d_major_type) {
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
		    type = binu_types[list_of_names[i]->d_minor_type];
		    break;
	    }
	}

	/* print list item i */
	dp = list_of_names[i];
	if (aflag ||
	    (!cflag && !rflag && !sflag) ||
	    (cflag && isComb) ||
	    (rflag && isRegion) ||
	    (sflag && isSolid)) {
	    bu_vls_printf(vls, "%s", dp->d_namep);
	    bu_vls_spaces(vls, (int)(max_nam_len - strlen(dp->d_namep)));
	    bu_vls_printf(vls, " %s", type);
	    if (type)
	       bu_vls_spaces(vls, (int)(max_type_len - strlen(type)));
	    bu_vls_printf(vls,  " %2d %2d %ld\n",
			  dp->d_major_type, dp->d_minor_type, (long)(dp->d_len));
	}
    }
}


/**
 *@brief
 * Given a pointer to a list of pointers to names and the number of
 * names in that list, sort and print that list on the same line.
 */
void
wdb_vls_line_dpp(struct rt_wdb *UNUSED(wdbp),
		 struct bu_vls *vls,
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

    bu_sort((void *)list_of_names,
	    (unsigned)num_in_list, (unsigned)sizeof(struct directory *),
	    wdb_cmpdirname, NULL);

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
	    bu_vls_printf(vls,  "%s ", list_of_names[i]->d_namep);
	}
    }
}


/**
 * This routine walks through the directory entry list and mallocs
 * enough space for pointers to hold:
 *
 * a) all of the entries if called with an argument of 0, or
 * b) the number of entries specified by the argument if > 0.
 */
struct directory **
wdb_getspace(struct db_i *dbip,
	     size_t num_entries)
{
    struct directory **dir_basep;

    if (num_entries == 0)
	num_entries = db_directory_size(dbip);

    /* Allocate and cast num_entries worth of pointers */
    dir_basep = (struct directory **) bu_calloc((num_entries+1), sizeof(struct directory *),
						"wdb_getspace *dir[]");
    return dir_basep;
}


void
wdb_do_list(struct db_i *dbip,
	    struct bu_vls *outstrp,
	    struct directory *dp,
	    int verbose)
{
    int id;
    struct rt_db_internal intern;

    RT_CK_DBI(dbip);

    if (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	/* this is the _GLOBAL object */
	struct bu_attribute_value_set avs;
	struct bu_attribute_value_pair *avp;

	bu_vls_strcat(outstrp, dp->d_namep);
	bu_vls_strcat(outstrp, ": global attributes object\n");
	bu_avs_init_empty(&avs);
	if (db5_get_attributes(dbip, &avs, dp)) {
	    bu_log("Cannot get attributes for %s\n", dp->d_namep);
	    return;
	}
	for (BU_AVS_FOR (avp, &avs)) {
	    if (BU_STR_EQUAL(avp->name, "units")) {
		double conv;
		const char *str;

		conv = atof(avp->value);
		bu_vls_strcat(outstrp, "\tunits: ");
		if ((str=bu_units_string(conv)) == NULL) {
		    bu_vls_strcat(outstrp, "Unrecognized units\n");
		} else {
		    bu_vls_strcat(outstrp, str);
		    bu_vls_putc(outstrp, '\n');
		}
	    } else {
		bu_vls_putc(outstrp, '\t');
		bu_vls_strcat(outstrp, avp->name);
		bu_vls_strcat(outstrp, ": ");
		bu_vls_strcat(outstrp, avp->value);
		bu_vls_putc(outstrp, '\n');
	    }
	}
    } else {

	if ((id = rt_db_get_internal(&intern, dp, dbip,
				     (fastf_t *)NULL, &rt_uniresource)) < 0) {
	    bu_log("rt_db_get_internal(%s) failure\n", dp->d_namep);
	    return;
	}

	if (OBJ[id].ft_describe) {
	    int ret;
	    bu_vls_printf(outstrp, "%s:  ", dp->d_namep);
	    ret = OBJ[id].ft_describe(outstrp, &intern, verbose, dbip->dbi_base2local, &rt_uniresource, dbip);
	    if (ret < 0)
		bu_log("%s: describe error\n", dp->d_namep);
	} else {
	    bu_log("%s: list support unimplemented\n", dp->d_namep);
	}

	rt_db_free_internal(&intern);
    }
}


/*
 * Add an instance of object 'objp' to combination 'name'.
 * If the combination does not exist, it is created.
 * region_flag is 1 (region), or 0 (group).
 *
 * Preserves the GIFT semantics.
 */
struct directory *
wdb_combadd(struct db_i *dbip,
	    struct directory *objp,
	    const char *combname,
	    int region_flag,	/* true if adding region */
	    int relation,	/* = UNION, SUBTRACT, INTERSECT */
	    int ident,		/* "Region ID" */
	    int air,		/* Air code */
	    struct rt_wdb *wdbp)
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    union tree *tp;
    struct rt_tree_array *tree_list;
    size_t node_count;
    size_t actual_count;

    /*
     * Check to see if we have to create a new combination
     */
    if ((dp = db_lookup(dbip,  combname, LOOKUP_QUIET)) == RT_DIR_NULL) {
	int flags;

	if (region_flag)
	    flags = RT_DIR_REGION | RT_DIR_COMB;
	else
	    flags = RT_DIR_COMB;

	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_COMBINATION;
	intern.idb_meth = &OBJ[ID_COMBINATION];

	/* Update the in-core directory */
	dp = db_diradd(dbip, combname, RT_DIR_PHONY_ADDR, 0, flags, (void *)&intern.idb_type);
	if (dp == RT_DIR_NULL) {
	    bu_log("An error has occurred while adding '%s' to the database.\n", combname);
	    return RT_DIR_NULL;
	}

	BU_ALLOC(comb, struct rt_comb_internal);
	intern.idb_ptr = (void *)comb;
	RT_COMB_INTERNAL_INIT(comb);

	if (region_flag) {
	    struct bu_vls tmp_vls;

	    comb->region_flag = 1;
	    comb->region_id = ident;
	    comb->aircode = air;
	    comb->los = wdbp->wdb_los_default;
	    comb->GIFTmater = wdbp->wdb_mat_default;
	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls,
			  "Creating region with attrs: region_id=%d, air=%d, los=%d, material_id=%d\n",
			  ident, air,
			  wdbp->wdb_los_default,
			  wdbp->wdb_mat_default);
	    bu_log("%s", bu_vls_addr(&tmp_vls));
	    bu_vls_free(&tmp_vls);
	} else {
	    comb->region_flag = 0;
	}
	RT_GET_TREE(tp, &rt_uniresource);
	tp->tr_l.tl_op = OP_DB_LEAF;
	tp->tr_l.tl_name = bu_strdup(objp->d_namep);
	tp->tr_l.tl_mat = (matp_t)NULL;
	comb->tree = tp;

	if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	    bu_log("Failed to write %s", dp->d_namep);
	    return RT_DIR_NULL;
	}
	return dp;
    } else if (!(dp->d_flags & RT_DIR_COMB)) {
	bu_log("%s exists, but is not a combination\n", combname);
	return RT_DIR_NULL;
    }

    /* combination exists, add a new member */
    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_log("read error, aborting\n");
	return RT_DIR_NULL;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (region_flag && !comb->region_flag) {
	bu_log("%s: not a region\n", combname);
	return RT_DIR_NULL;
    }

    if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	db_non_union_push(comb->tree, &rt_uniresource);
	if (db_ck_v4gift_tree(comb->tree) < 0) {
	    bu_log("Cannot flatten tree for editing\n");
	    rt_db_free_internal(&intern);
	    return RT_DIR_NULL;
	}
    }

    /* make space for an extra leaf */
    node_count = db_tree_nleaves(comb->tree) + 1;
    tree_list = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list");

    /* flatten tree */
    if (comb->tree) {
	actual_count = 1 + (struct rt_tree_array *)db_flatten_tree(tree_list, comb->tree, OP_UNION, 1, &rt_uniresource) - tree_list;
	BU_ASSERT_SIZE_T(actual_count, ==, node_count);
	comb->tree = TREE_NULL;
    }

    /* insert new member at end */
    switch (relation) {
	case '+':
	    tree_list[node_count - 1].tl_op = OP_INTERSECT;
	    break;
	case '-':
	    tree_list[node_count - 1].tl_op = OP_SUBTRACT;
	    break;
	default:
	    if (relation != 'u') {
		bu_log("unrecognized relation (assume UNION)\n");
	    }
	    tree_list[node_count - 1].tl_op = OP_UNION;
	    break;
    }

    /* make new leaf node, and insert at end of list */
    RT_GET_TREE(tp, &rt_uniresource);
    tree_list[node_count-1].tl_tree = tp;
    tp->tr_l.tl_op = OP_DB_LEAF;
    tp->tr_l.tl_name = bu_strdup(objp->d_namep);
    tp->tr_l.tl_mat = (matp_t)NULL;

    /* rebuild the tree */
    comb->tree = (union tree *)db_mkgift_tree(tree_list, node_count, &rt_uniresource);

    /* and finally, write it out */
    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	bu_log("Failed to write %s\n", dp->d_namep);
	return RT_DIR_NULL;
    }

    bu_free((char *)tree_list, "combadd: tree_list");

    return dp;
}


/* cyclic dependency, need to declare */
void wdb_identitize(struct directory *dp, struct db_i *dbip);

static void
wdb_do_identitize(struct db_i *dbip,
		  struct rt_comb_internal *UNUSED(comb),
		  union tree *comb_leaf,
		  void *UNUSED(user_ptr1),
		  void *UNUSED(user_ptr2),
		  void *UNUSED(user_ptr3),
		  void *UNUSED(user_ptr4))
{
    struct directory *dp;

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    if (!comb_leaf->tr_l.tl_mat) {
	comb_leaf->tr_l.tl_mat = (matp_t)bu_malloc(sizeof(mat_t), "tl_mat");
    }
    MAT_IDN(comb_leaf->tr_l.tl_mat);
    if ((dp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_NOISY)) == RT_DIR_NULL)
	return;

    wdb_identitize(dp, dbip);
}


/*
 * Traverses an objects paths, setting all member matrices == identity
 *
 */
void
wdb_identitize(struct directory *dp,
	       struct db_i *dbip)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    if (dp->d_flags & RT_DIR_SOLID)
	return;
    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_log("Database read error, aborting\n");
	return;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;
    if (comb->tree) {
	db_tree_funcleaf(dbip, comb, comb->tree, wdb_do_identitize,
			 (void *)NULL, (void *)NULL, (void *)NULL, (void *)NULL);
	if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	    bu_log("Cannot write modified combination (%s) to database\n", dp->d_namep);
	    return;
	}
    }
}


/*
 * This routine walks through the directory entry list and mallocs enough
 * space for pointers to hold:
 * a) all of the entries if called with an argument of 0, or
 * b) the number of entries specified by the argument if > 0.
 */
static struct directory **
wdb_dir_getspace(struct db_i *dbip,
		 int num_entries)
{
    struct directory *dp;
    int i;
    struct directory **dir_basep;

    if (num_entries < 0) {
	bu_log("dir_getspace: was passed %d, used 0\n",
	       num_entries);
	num_entries = 0;
    }
    if (num_entries == 0) {
	/* Set num_entries to the number of entries */
	for (i = 0; i < RT_DBNHASH; i++)
	    for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw)
		num_entries++;
    }

    /* Allocate and cast num_entries worth of pointers */
    dir_basep = (struct directory **) bu_malloc((num_entries+1) * sizeof(struct directory *),
						"dir_getspace *dir[]");
    return dir_basep;
}


/*
 * Summarize the contents of the directory by categories
 * (solid, comb, region).  If flag is != 0, it is interpreted
 * as a request to print all the names in that category (e.g., RT_DIR_SOLID).
 */
static void
wdb_dir_summary(struct db_i *dbip,
		int flag)
{
    struct directory *dp;
    int i;
    static int sol, comb, reg;
    struct directory **dirp;
    struct directory **dirp0 = (struct directory **)NULL;
    struct bu_vls vls;

    bu_vls_init(&vls);

    sol = comb = reg = 0;
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_flags & RT_DIR_SOLID)
		sol++;
	    if (dp->d_flags & RT_DIR_COMB) {
		if (dp->d_flags & RT_DIR_REGION)
		    reg++;
		else
		    comb++;
	    }
	}
    }

    bu_vls_printf(&vls, "Summary:\n");
    bu_vls_printf(&vls, "  %5d primitives\n", sol);
    bu_vls_printf(&vls, "  %5d region; %d non-region combinations\n", reg, comb);
    bu_vls_printf(&vls, "  %5d total objects\n\n", sol+reg+comb);

    if (flag == 0) {
	bu_log("%s", bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return;
    }

    /* Print all names matching the flags parameter */
    /* THIS MIGHT WANT TO BE SEPARATED OUT BY CATEGORY */

    dirp = wdb_dir_getspace(dbip, 0);
    dirp0 = dirp;
    /*
     * Walk the directory list adding pointers (to the directory entries
     * of interest) to the array
     */
    for (i = 0; i < RT_DBNHASH; i++)
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw)
	    if (dp->d_flags & flag)
		*dirp++ = dp;

    wdb_vls_col_pr4v(&vls, dirp0, (int)(dirp - dirp0), 0);
    bu_log("%s", bu_vls_addr(&vls));
    bu_vls_free(&vls);
    bu_free((void *)dirp0, "dir_getspace");
}


static union tree *
wdb_pathlist_leaf_func(struct db_tree_state *UNUSED(tsp),
		       const struct db_full_path *pathp,
		       struct rt_db_internal *ip,
		       void *client_data)
{
    Tcl_Interp *interp = (Tcl_Interp *)client_data;
    const char *str;

    RT_CK_FULL_PATH(pathp);
    RT_CK_DB_INTERNAL(ip);

    if (pathListNoLeaf) {
	struct db_full_path pp;
	db_full_path_init(&pp);
	db_dup_full_path(&pp, pathp);
	--pp.fp_len;
	str = db_path_to_string(&pp);
	Tcl_AppendElement(interp, str);
	db_free_full_path(&pp);
    } else {
	str = db_path_to_string(pathp);
	Tcl_AppendElement(interp, str);
    }

    bu_free((void *)str, "path string");
    return TREE_NULL;
}


int
wdb_bot_decimate_cmd(struct rt_wdb *wdbp,
		     int argc,
		     const char *argv[])
{
    int c;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct directory *dp;
    fastf_t max_chord_error=-1.0;
    fastf_t max_normal_error=-1.0;
    fastf_t min_edge_length=-1.0;

    if (argc < 5 || argc > 9) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_bot_decimate %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* process args */
    bu_optind = 1;
    bu_opterr = 0;
    while ((c=bu_getopt(argc, (char * const *)argv, "c:n:e:")) != -1) {
	switch (c) {
	    case 'c':
		max_chord_error = atof(bu_optarg);
		if (max_chord_error < 0.0) {
		    Tcl_AppendResult(wdbp->wdb_interp,
				     "Maximum chord error cannot be less than zero",
				     (char *)NULL);
		    return TCL_ERROR;
		}
		break;
	    case 'n':
		max_normal_error = atof(bu_optarg);
		if (max_normal_error < 0.0) {
		    Tcl_AppendResult(wdbp->wdb_interp,
				     "Maximum normal error cannot be less than zero",
				     (char *)NULL);
		    return TCL_ERROR;
		}
		break;
	    case 'e':
		min_edge_length = atof(bu_optarg);
		if (min_edge_length < 0.0) {
		    Tcl_AppendResult(wdbp->wdb_interp,
				     "minimum edge length cannot be less than zero",
				     (char *)NULL);
		    return TCL_ERROR;
		}
		break;
	    default:
		{
		    struct bu_vls vls;

		    bu_vls_init(&vls);
		    bu_vls_printf(&vls, "helplib_alias wdb_bot_decimate %s",
				  argv[0]);
		    Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
		    bu_vls_free(&vls);
		    return TCL_ERROR;
		}
	}
    }

    argc -= bu_optind;
    argv += bu_optind;

    /* make sure new solid does not already exist */
    dp = db_lookup(wdbp->dbip, argv[0], LOOKUP_QUIET);
    if (dp != RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, argv[0], " already exists!!\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* make sure current solid does exist */
    dp = db_lookup(wdbp->dbip, argv[1], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, argv[1], " Does not exist\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* import the current solid */
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, wdbp->dbip, NULL, wdbp->wdb_resp) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Failed to get internal form of ", argv[1],
			 "\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* make sure this is a BOT solid */
    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	Tcl_AppendResult(wdbp->wdb_interp, argv[1], " is not a BOT solid\n", (char *)NULL);
	rt_db_free_internal(&intern);
	return TCL_ERROR;
    }

    bot = (struct rt_bot_internal *)intern.idb_ptr;

    RT_BOT_CK_MAGIC(bot);

    /* convert maximum error and edge length to mm */
    max_chord_error = max_chord_error * wdbp->dbip->dbi_local2base;
    min_edge_length = min_edge_length * wdbp->dbip->dbi_local2base;

    /* do the decimation */
    if (rt_bot_decimate(bot, max_chord_error, max_normal_error, min_edge_length) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Decimation Error\n", (char *)NULL);
	rt_db_free_internal(&intern);
	return TCL_ERROR;
    }

    /* save the result to the database */
    if (wdb_put_internal(wdbp, argv[0], &intern, 1.0) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Failed to write decimated BOT back to database\n", (char *)NULL);
	return TCL_ERROR;
    }

    return TCL_OK;
}


/*
 * Usage:
 * procname
 */
static int
wdb_bot_decimate_tcl(void *clientData,
		     int argc,
		     const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_bot_decimate_cmd(wdbp, argc-1, argv+1);
}


int
wdb_move_arb_edge_cmd(struct rt_wdb *wdbp,
		      int argc,
		      const char *argv[])
{
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    plane_t planes[6];  /* ARBs defining plane equations */
    int arb_type;
    int edge;
    int bad_edge_id = 0;
    point_t pt;
    double scan[3];
    struct bu_vls error_msg;

    if (argc != 4) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_move_arb_edge %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (wdbp->dbip == 0) {
	Tcl_AppendResult(wdbp->wdb_interp,
			 "db does not support lookup operations",
			 (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_tcl_import_from_path(wdbp->wdb_interp, &intern, argv[1], wdbp) == TCL_ERROR)
	return TCL_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ARB8) {
	Tcl_AppendResult(wdbp->wdb_interp, "Object not an ARB", (char *)NULL);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    if (sscanf(argv[2], "%d", &edge) != 1) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad edge - %s", argv[2]);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }
    edge -= 1;

    if (sscanf(argv[3], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad point - %s", argv[3]);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }
    /* convert double to fastf_t */
    VMOVE(pt, scan);

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    arb_type = rt_arb_std_type(&intern, &wdbp->wdb_tol);

    /* check the arb type */
    switch (arb_type) {
	case ARB4:
	    if (edge < 0 || 4 < edge)
		bad_edge_id = 1;
	    break;
	case ARB5:
	    if (edge < 0 || 8 < edge)
		bad_edge_id = 1;
	    break;
	case ARB6:
	    if (edge < 0 || 9 < edge)
		bad_edge_id = 1;
	    break;
	case ARB7:
	    if (edge < 0 || 11 < edge)
		bad_edge_id = 1;
	    break;
	case ARB8:
	    if (edge < 0 || 11 < edge)
		bad_edge_id = 1;
	    break;
	default:
	    Tcl_AppendResult(wdbp->wdb_interp, "unrecognized arb type", (char *)NULL);
	    rt_db_free_internal(&intern);

	    return TCL_ERROR;
    }

    /* check the edge id */
    if (bad_edge_id) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad edge - %s", argv[2]);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    bu_vls_init(&error_msg);
    if (rt_arb_calc_planes(&error_msg, arb, arb_type, planes, &wdbp->wdb_tol)) {
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&error_msg), (char *)0);
	rt_db_free_internal(&intern);
	bu_vls_free(&error_msg);

	return TCL_ERROR;
    }

    if (rt_arb_edit(&error_msg, arb, arb_type, edge, pt, planes, &wdbp->wdb_tol)) {
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&error_msg), (char *)0);
	rt_db_free_internal(&intern);
	bu_vls_free(&error_msg);

	return TCL_ERROR;
    }
    bu_vls_free(&error_msg);

    {
	int i;
	struct bu_vls vls;

	bu_vls_init(&vls);

	for (i = 0; i < 8; ++i) {
	    bu_vls_printf(&vls, "V%d {%g %g %g} ",
			  i + 1,
			  arb->pt[i][X],
			  arb->pt[i][Y],
			  arb->pt[i][Z]);
	}

	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
    }

    rt_db_free_internal(&intern);
    return TCL_OK;
}


/**
 * Move an arb's edge so that it intersects the given point. The new
 * vertices are returned via interp result.
 *
 * Usage:
 * procname move_arb_face arb face pt
 */
static int
wdb_move_arb_edge_tcl(void *clientData,
		      int argc,
		      const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_move_arb_edge_cmd(wdbp, argc-1, argv+1);
}


int
wdb_move_arb_face_cmd(struct rt_wdb *wdbp,
		      int argc,
		      const char *argv[])
{
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    fastf_t planes[7][4];		/* ARBs defining plane equations */
    int arb_type;
    int face;
    struct bu_vls error_msg;

    /* intentionally double for scan */
    double pt[3];

    if (argc != 4) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_move_arb_face %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (wdbp->dbip == 0) {
	Tcl_AppendResult(wdbp->wdb_interp,
			 "db does not support lookup operations",
			 (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_tcl_import_from_path(wdbp->wdb_interp, &intern, argv[1], wdbp) == TCL_ERROR)
	return TCL_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ARB8) {
	Tcl_AppendResult(wdbp->wdb_interp, "Object not an ARB", (char *)NULL);
	rt_db_free_internal(&intern);

	return TCL_OK;
    }

    if (sscanf(argv[2], "%d", &face) != 1) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad face - %s", argv[2]);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    /*XXX need better checking of the face */
    face -= 1;
    if (face < 0 || 5 < face) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad face - %s", argv[2]);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    if (sscanf(argv[3], "%lf %lf %lf", &pt[X], &pt[Y], &pt[Z]) != 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad point - %s", argv[3]);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    arb_type = rt_arb_std_type(&intern, &wdbp->wdb_tol);

    bu_vls_init(&error_msg);
    if (rt_arb_calc_planes(&error_msg, arb, arb_type, planes, &wdbp->wdb_tol)) {
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&error_msg), (char *)0);
	rt_db_free_internal(&intern);
	bu_vls_free(&error_msg);

	return TCL_ERROR;
    }
    bu_vls_free(&error_msg);

    /* change D of planar equation */
    planes[face][3] = VDOT(&planes[face][0], pt);

    /* calculate new points for the arb */
    (void)rt_arb_calc_points(arb, arb_type, (const plane_t *)planes, &wdbp->wdb_tol);

    {
	int i;
	struct bu_vls vls;

	bu_vls_init(&vls);

	for (i = 0; i < 8; ++i) {
	    bu_vls_printf(&vls, "V%d {%g %g %g} ",
			  i + 1,
			  arb->pt[i][X],
			  arb->pt[i][Y],
			  arb->pt[i][Z]);
	}

	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
    }

    rt_db_free_internal(&intern);
    return TCL_OK;
}


/**
 * Move an arb's face so that its plane intersects the given
 * point. The new vertices are returned via interp result.
 *
 * Usage:
 * procname move_arb_face arb face pt
 */
static int
wdb_move_arb_face_tcl(void *clientData,
		      int argc,
		      const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_move_arb_face_cmd(wdbp, argc-1, argv+1);
}


static short int rt_arb_vertices[5][24] = {
    { 1, 2, 3, 0, 1, 2, 4, 0, 2, 3, 4, 0, 1, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* arb4 */
    { 1, 2, 3, 4, 1, 2, 5, 0, 2, 3, 5, 0, 3, 4, 5, 0, 1, 4, 5, 0, 0, 0, 0, 0 },	/* arb5 */
    { 1, 2, 3, 4, 2, 3, 6, 5, 1, 5, 6, 4, 1, 2, 5, 0, 3, 4, 6, 0, 0, 0, 0, 0 },	/* arb6 */
    { 1, 2, 3, 4, 5, 6, 7, 0, 1, 4, 5, 0, 2, 3, 7, 6, 1, 2, 6, 5, 4, 3, 7, 5 },	/* arb7 */
    { 1, 2, 3, 4, 5, 6, 7, 8, 1, 5, 8, 4, 2, 3, 7, 6, 1, 2, 6, 5, 4, 3, 7, 8 }	/* arb8 */
};


int
wdb_rotate_arb_face_cmd(struct rt_wdb *wdbp,
			int argc,
			const char *argv[])
{
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    fastf_t planes[7][4];		/* ARBs defining plane equations */
    int arb_type;
    int face;
    int vi;
    int i;
    int pnt5;		/* special arb7 case */
    struct bu_vls error_msg;

    /* intentionally double for scan */
    double pt[3];

    if (argc != 5) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_move_arb_face %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (wdbp->dbip == 0) {
	Tcl_AppendResult(wdbp->wdb_interp,
			 "db does not support lookup operations",
			 (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_tcl_import_from_path(wdbp->wdb_interp, &intern, argv[1], wdbp) == TCL_ERROR)
	return TCL_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ARB8) {
	Tcl_AppendResult(wdbp->wdb_interp, "Object not an ARB", (char *)NULL);
	rt_db_free_internal(&intern);

	return TCL_OK;
    }

    if (sscanf(argv[2], "%d", &face) != 1) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad face - %s", argv[2]);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    /*XXX need better checking of the face */
    face -= 1;
    if (face < 0 || 5 < face) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad face - %s", argv[2]);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    if (sscanf(argv[3], "%d", &vi) != 1) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad vertex index - %s", argv[2]);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }


    /*XXX need better checking of the vertex index */
    vi -= 1;
    if (vi < 0 || 7 < vi) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad vertex - %s", argv[2]);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    if (sscanf(argv[4], "%lf %lf %lf", &pt[X], &pt[Y], &pt[Z]) != 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "bad point - %s", argv[3]);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	rt_db_free_internal(&intern);

	return TCL_ERROR;
    }

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    arb_type = rt_arb_std_type(&intern, &wdbp->wdb_tol);

    bu_vls_init(&error_msg);
    if (rt_arb_calc_planes(&error_msg, arb, arb_type, planes, &wdbp->wdb_tol)) {
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&error_msg), (char *)0);
	rt_db_free_internal(&intern);
	bu_vls_free(&error_msg);

	return TCL_ERROR;
    }
    bu_vls_free(&error_msg);

    /* check if point 5 is in the face */
    pnt5 = 0;
    for (i = 0; i < 4; i++) {
	if (rt_arb_vertices[arb_type-4][face*4+i]==5)
	    pnt5=1;
    }

    /* special case for arb7 */
    if (arb_type == ARB7  && pnt5)
	vi = 4;

    {
	/* Apply incremental changes */
	vect_t tempvec;
	vect_t work;
	fastf_t *plane;
	mat_t rmat;

	bn_mat_angles(rmat, pt[X], pt[Y], pt[Z]);

	plane = &planes[face][0];
	VMOVE(work, plane);
	MAT4X3VEC(plane, rmat, work);

	/* point notation of fixed vertex */
	VMOVE(tempvec, arb->pt[vi]);

	/* set D of planar equation to anchor at fixed vertex */
	planes[face][3]=VDOT(plane, tempvec);
    }

    /* calculate new points for the arb */
    (void)rt_arb_calc_points(arb, arb_type, (const plane_t *)planes, &wdbp->wdb_tol);

    {
	struct bu_vls vls;

	bu_vls_init(&vls);

	for (i = 0; i < 8; ++i) {
	    bu_vls_printf(&vls, "V%d {%g %g %g} ",
			  i + 1,
			  arb->pt[i][X],
			  arb->pt[i][Y],
			  arb->pt[i][Z]);
	}

	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
    }

    rt_db_free_internal(&intern);
    return TCL_OK;
}


/**
 * Rotate an arb's face to the given point. The new vertices are
 * returned via interp result.
 *
 * Usage:
 * procname rotate_arb_face arb face pt
 */
static int
wdb_rotate_arb_face_tcl(void *clientData,
			int argc,
			const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_rotate_arb_face_cmd(wdbp, argc-1, argv+1);
}


int
wdb_decode_dbip(const char *dbip_string, struct db_i **dbipp)
{
    if (sscanf(dbip_string, "%p", (void **)dbipp) != 1) {
	return GED_ERROR;
    }

    /* Could core dump */
    RT_CK_DBI(*dbipp);

    return TCL_OK;
}


/**
 * @brief
 * Open/Create the database and build the in memory directory.
 */
struct db_i *
wdb_prep_dbip(const char *filename)
{
    struct db_i *dbip;

    /* open database */
    if (((dbip = db_open(filename, DB_OPEN_READWRITE)) == DBI_NULL) &&
	((dbip = db_open(filename, DB_OPEN_READONLY)) == DBI_NULL)) {

	/*
	 * Check to see if we can access the database
	 */
	if (bu_file_exists(filename, NULL) && !bu_file_readable(filename)) {
	    bu_log("wdb_prep_dbip: %s is not readable\n", filename);

	    return DBI_NULL;
	}

	/* db_create does a db_dirbuild */
	if ((dbip = db_create(filename, 5)) == DBI_NULL) {
	    bu_log("wdb_prep_dbip: failed to create %s\n", filename);

	    if (dbip == DBI_NULL)
		bu_log("wdb_prep_dbip: no database is currently opened!");

	    return DBI_NULL;
	}
    } else
	/* --- Scan geometry database and build in-memory directory --- */
	db_dirbuild(dbip);


    return dbip;
}


/****************** Database Object Methods ********************/

int
wdb_reopen_cmd(struct rt_wdb *wdbp,
	       int argc,
	       const char *argv[])
{
    struct db_i *dbip;
    struct bu_vls vls;

    /* get database filename */
    if (argc == 1) {
	Tcl_AppendResult(wdbp->wdb_interp, wdbp->dbip->dbi_filename, (char *)NULL);
	return TCL_OK;
    }

    /* set database filename */
    if (argc == 2) {
	if ((dbip = wdb_prep_dbip(argv[1])) == DBI_NULL) {
	    return TCL_ERROR;
	}

	/* XXXnotify observers */
	/* notify drawable geometry objects associated with this database */
	dgo_zapall(wdbp);

	/* close current database */
	db_close(wdbp->dbip);

	wdbp->dbip = dbip;

	Tcl_AppendResult(wdbp->wdb_interp, wdbp->dbip->dbi_filename, (char *)NULL);
	return TCL_OK;
    }

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias wdb_reopen %s", argv[0]);
    Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
}


/**
 *
 * @par Usage:
 * procname open [filename]
 */
static int
wdb_reopen_tcl(void *clientData,
	       int argc,
	       const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_reopen_cmd(wdbp, argc-1, argv+1);
}


int
wdb_match_cmd(struct rt_wdb *wdbp,
	      int UNUSED(argc),
	      const char *argv[])
{
    struct bu_vls matches;

    RT_CK_WDB(wdbp);

    /* Verify that this wdb supports lookup operations
       (non-null dbip) */
    if (wdbp->dbip == 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "this database does not support lookup operations");
	return TCL_ERROR;
    }

    bu_vls_init(&matches);
    for (++argv; *argv != NULL; ++argv) {
	if (db_regexp_match_all(&matches, wdbp->dbip, *argv) > 0)
	    bu_vls_strcat(&matches, " ");
    }
    bu_vls_trimspace(&matches);
    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&matches), (char *)NULL);
    bu_vls_free(&matches);
    return TCL_OK;
}


/**
 *@brief
 * Returns (via interp) a list (possibly empty) of all matches to the
 * (possibly wildcard-containing) arguments given.  Does *NOT* return
 * tokens that do not match anything, unlike the "expand" command.
 */

static int
wdb_match_tcl(void *clientData,
	      int argc,
	      const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_match_cmd(wdbp, argc-1, argv+1);
}


int
wdb_get_cmd(struct rt_wdb *wdbp,
	    int argc,
	    const char *argv[])
{
    int status;
    struct rt_db_internal intern;

    if (argc < 2 || argc > 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_get %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* Verify that this wdb supports lookup operations
       (non-null dbip) */
    if (wdbp->dbip == 0) {
	Tcl_AppendResult(wdbp->wdb_interp,
			 "db does not support lookup operations",
			 (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_tcl_import_from_path(wdbp->wdb_interp, &intern, argv[1], wdbp) == TCL_ERROR)
	return TCL_ERROR;

    {
	struct bu_vls logstr;

	bu_vls_init(&logstr);

	if (!intern.idb_meth->ft_get)
	    return TCL_ERROR;

	status = intern.idb_meth->ft_get(&logstr, &intern, argv[2]);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&logstr), (char *)NULL);
	bu_vls_free(&logstr);
    }

    rt_db_free_internal(&intern);
    return status;
}


/**
 * For use with Tcl, this routine accepts as its first argument the name
 * of an object in the database.  If only one argument is given, this routine
 * then fills the result string with the (minimal) attributes of the item.
 * If a second, optional, argument is provided, this function looks up the
 * property with that name of the item given, and returns it as the result
 * string.
 *
 *
 * NOTE: This is called directly by gdiff/g_diff.c
 */
int
wdb_get_tcl(void *clientData,
	    int argc,
	    const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_get_cmd(wdbp, argc-1, argv+1);
}


int
wdb_get_type_cmd(struct rt_wdb *wdbp,
		 int argc,
		 const char *argv[])
{
    struct rt_db_internal intern;
    int type;

    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_get_type %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (wdbp->dbip == 0) {
	Tcl_AppendResult(wdbp->wdb_interp,
			 "db does not support lookup operations",
			 (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_tcl_import_from_path(wdbp->wdb_interp, &intern, argv[1], wdbp) == TCL_ERROR)
	return TCL_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD) {
	Tcl_AppendResult(wdbp->wdb_interp, "unknown", (char *)NULL);
	rt_db_free_internal(&intern);

	return TCL_OK;
    }

    switch (intern.idb_minor_type) {
	case DB5_MINORTYPE_BRLCAD_TOR:
	    Tcl_AppendResult(wdbp->wdb_interp, "tor", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_TGC:
	    Tcl_AppendResult(wdbp->wdb_interp, "tgc", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_ELL:
	    Tcl_AppendResult(wdbp->wdb_interp, "ell", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_ARB8:
	    type = rt_arb_std_type(&intern, &wdbp->wdb_tol);

	    switch (type) {
		case 4:
		    Tcl_AppendResult(wdbp->wdb_interp, "arb4", (char *)NULL);
		    break;
		case 5:
		    Tcl_AppendResult(wdbp->wdb_interp, "arb5", (char *)NULL);
		    break;
		case 6:
		    Tcl_AppendResult(wdbp->wdb_interp, "arb6", (char *)NULL);
		    break;
		case 7:
		    Tcl_AppendResult(wdbp->wdb_interp, "arb7", (char *)NULL);
		    break;
		case 8:
		    Tcl_AppendResult(wdbp->wdb_interp, "arb8", (char *)NULL);
		    break;
		default:
		    Tcl_AppendResult(wdbp->wdb_interp, "invalid", (char *)NULL);
		    break;
	    }

	    break;
	case DB5_MINORTYPE_BRLCAD_ARS:
	    Tcl_AppendResult(wdbp->wdb_interp, "ars", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_HALF:
	    Tcl_AppendResult(wdbp->wdb_interp, "half", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_REC:
	    Tcl_AppendResult(wdbp->wdb_interp, "rec", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_POLY:
	    Tcl_AppendResult(wdbp->wdb_interp, "poly", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_BSPLINE:
	    Tcl_AppendResult(wdbp->wdb_interp, "spline", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_SPH:
	    Tcl_AppendResult(wdbp->wdb_interp, "sph", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_NMG:
	    Tcl_AppendResult(wdbp->wdb_interp, "nmg", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_EBM:
	    Tcl_AppendResult(wdbp->wdb_interp, "ebm", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_VOL:
	    Tcl_AppendResult(wdbp->wdb_interp, "vol", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_ARBN:
	    Tcl_AppendResult(wdbp->wdb_interp, "arbn", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_PIPE:
	    Tcl_AppendResult(wdbp->wdb_interp, "pipe", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_PARTICLE:
	    Tcl_AppendResult(wdbp->wdb_interp, "part", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_RPC:
	    Tcl_AppendResult(wdbp->wdb_interp, "rpc", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_RHC:
	    Tcl_AppendResult(wdbp->wdb_interp, "rhc", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_EPA:
	    Tcl_AppendResult(wdbp->wdb_interp, "epa", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_EHY:
	    Tcl_AppendResult(wdbp->wdb_interp, "ehy", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_ETO:
	    Tcl_AppendResult(wdbp->wdb_interp, "eto", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_GRIP:
	    Tcl_AppendResult(wdbp->wdb_interp, "grip", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_JOINT:
	    Tcl_AppendResult(wdbp->wdb_interp, "joint", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_HF:
	    Tcl_AppendResult(wdbp->wdb_interp, "hf", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_DSP:
	    Tcl_AppendResult(wdbp->wdb_interp, "dsp", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_SKETCH:
	    Tcl_AppendResult(wdbp->wdb_interp, "sketch", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_EXTRUDE:
	    Tcl_AppendResult(wdbp->wdb_interp, "extrude", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_SUBMODEL:
	    Tcl_AppendResult(wdbp->wdb_interp, "submodel", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_CLINE:
	    Tcl_AppendResult(wdbp->wdb_interp, "cline", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_BOT:
	    Tcl_AppendResult(wdbp->wdb_interp, "bot", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_COMBINATION:
	    Tcl_AppendResult(wdbp->wdb_interp, "comb", (char *)NULL);
	    break;
	case DB5_MINORTYPE_BRLCAD_BREP:
	    Tcl_AppendResult(wdbp->wdb_interp, "brep", (char *)NULL);
	    break;
	default:
	    Tcl_AppendResult(wdbp->wdb_interp, "other", (char *)NULL);
	    break;
    }

    rt_db_free_internal(&intern);
    return TCL_OK;
}


int
wdb_get_type_tcl(void *clientData,
		 int argc,
		 const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_get_type_cmd(wdbp, argc-1, argv+1);
}


int
wdb_put_cmd(struct rt_wdb *wdbp,
	    int argc,
	    const char *argv[])
{
    struct rt_db_internal intern;
    const struct rt_functab *ftp;
    int i;
    const char *name;
    char type[16];

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_put %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    name = argv[1];

    /* Verify that this wdb supports lookup operations (non-null dbip).
     * stdout/file wdb objects don't, but can still be written to.
     * If not, just skip the lookup test and write the object
     */
    if (wdbp->dbip && db_lookup(wdbp->dbip, argv[1], LOOKUP_QUIET) != RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, argv[1], " already exists",
			 (char *)NULL);
	return TCL_ERROR;
    }

    RT_DB_INTERNAL_INIT(&intern);

    /* place values in type at 0-14, place null terminator at 15 */
    for (i = 0; argv[2][i] != 0 && i < 15; i++) {
	type[i] = isupper((int)argv[2][i]) ? tolower((int)argv[2][i]) : argv[2][i];
    }
    type[i] = '\0';

    ftp = rt_get_functab_by_label(type);
    if (ftp == NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, type,
			 " is an unknown object type.",
			 (char *)NULL);
	return TCL_ERROR;
    }

    RT_CK_FUNCTAB(ftp);

    if (ftp->ft_make) {
	ftp->ft_make(ftp, &intern);
    } else {
	rt_generic_make(ftp, &intern);
    }

    {
	struct bu_vls logstr;

	bu_vls_init(&logstr);

	if (!ftp->ft_adjust || ftp->ft_adjust(&logstr, &intern, argc-3, argv+3) == GED_ERROR) {
	    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&logstr), (char *)NULL);
	    bu_vls_free(&logstr);
	    rt_db_free_internal(&intern);
	    return TCL_ERROR;
	}

	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&logstr), (char *)NULL);
	bu_vls_free(&logstr);
    }

    if (wdb_put_internal(wdbp, name, &intern, 1.0) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "wdb_put_internal(", argv[1],
			 ") failure", (char *)NULL);
	rt_db_free_internal(&intern);
	return TCL_ERROR;
    }

    rt_db_free_internal(&intern);
    return TCL_OK;
}


int
wdb_adjust_cmd(struct rt_wdb *wdbp,
	       int argc,
	       const char *argv[])
{
    struct directory *dp;
    int status;
    const char *name;
    struct rt_db_internal intern;

    if (argc < 4) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_adjust %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }
    name = argv[1];

    /* Verify that this wdb supports lookup operations (non-null dbip) */
    RT_CK_DBI(wdbp->dbip);

    dp = db_lookup(wdbp->dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, name, ": not found",
			 (char *)NULL);
	return TCL_ERROR;
    }

    status = rt_db_get_internal(&intern, dp, wdbp->dbip, (matp_t)NULL, &rt_uniresource);
    if (status < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "rt_db_get_internal(", name,
			 ") failure", (char *)NULL);
	return TCL_ERROR;
    }
    RT_CK_DB_INTERNAL(&intern);

    /* Find out what type of object we are dealing with and tweak it. */
    RT_CK_FUNCTAB(intern.idb_meth);

    {
	struct bu_vls logstr;

	bu_vls_init(&logstr);

	if (!intern.idb_meth->ft_adjust)
	    return TCL_ERROR;

	status = intern.idb_meth->ft_adjust(&logstr, &intern, argc-2, argv+2);

	if (status == GED_OK && wdb_put_internal(wdbp, name, &intern, 1.0) < 0) {
	    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&logstr), (char *)NULL);
	    bu_vls_free(&logstr);
	    Tcl_AppendResult(wdbp->wdb_interp, "wdb_export(", name,
			     ") failure", (char *)NULL);
	    rt_db_free_internal(&intern);
	    return TCL_ERROR;
	}

	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&logstr), (char *)NULL);
	bu_vls_free(&logstr);
    }

    /* notify observers */
    bu_observer_notify(wdbp->wdb_interp, &wdbp->wdb_observers, bu_vls_addr(&wdbp->wdb_name));

    return status;
}


/**
 *@brief
 * For use with Tcl, this routine accepts as its first argument an item in
 * the database; as its remaining arguments it takes the properties that
 * need to be changed and their values.
 *
 * @par Example of adjust operation on a solid:
 *	.inmem adjust LIGHT V { -46 -13 5 }
 *
 * @par Example of adjust operation on a combination:
 *	.inmem adjust light.r rgb { 255 255 255 }
 */

static int
wdb_adjust_tcl(void *clientData,
	       int argc,
	       const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_adjust_cmd(wdbp, argc-1, argv+1);
}


int
wdb_form_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    const struct rt_functab *ftp;

    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_form %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((ftp = rt_get_functab_by_label(argv[1])) == NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, "There is no geometric object type \"",
			 argv[1], "\".", (char *)NULL);
	return TCL_ERROR;
    }

    {
	int ret;
	struct bu_vls logstr;

	bu_vls_init(&logstr);

	if (!ftp->ft_form)
	    return TCL_ERROR;

	ret = ftp->ft_form(&logstr, ftp);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&logstr), (char *)NULL);
	bu_vls_free(&logstr);

	return ret;
    }
}


static int
wdb_form_tcl(void *clientData,
	     int argc,
	     const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_form_cmd(wdbp, argc-1, argv+1);
}


int
wdb_tops_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    struct directory *dp;
    int i;
    struct directory **dirp;
    struct directory **dirp0 = (struct directory **)NULL;
    struct bu_vls vls;
    int c;

    int aflag = 0;
    int hflag = 0;
    int pflag = 0;

    int no_decorate = 0;

    RT_CK_WDB(wdbp);
    RT_CK_DBI(wdbp->dbip);

    /* process any options */
    bu_optind = 1;	/* re-init bu_getopt() */

    while ((c = bu_getopt(argc, (char * const *)argv, "ahnp")) != -1) {
	switch (c) {
	    case 'a':
		aflag = 1;
		break;
	    case 'h':
		hflag = 1;
		break;
	    case 'n':
		no_decorate = 1;
		break;
	    case 'p':
		pflag = 1;
		break;
	    default:
		break;
	}
    }

    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    /* Can this be executed only sometimes?
       Perhaps a "dirty bit" on the database? */
    db_update_nref(wdbp->dbip, &rt_uniresource);

    /*
     * Find number of possible entries and allocate memory
     */
    dirp = wdb_dir_getspace(wdbp->dbip, 0);
    dirp0 = dirp;

    if (db_version(wdbp->dbip) < 5) {
	for (i = 0; i < RT_DBNHASH; i++)
	    for (dp = wdbp->dbip->dbi_Head[i];
		 dp != RT_DIR_NULL;
		 dp = dp->d_forw) {
		if (dp->d_nref == 0)
		    *dirp++ = dp;
	    }
    } else {
	for (i = 0; i < RT_DBNHASH; i++)
	    for (dp = wdbp->dbip->dbi_Head[i];
		 dp != RT_DIR_NULL;
		 dp = dp->d_forw) {

		if (dp->d_nref == 0 &&
		    (aflag ||
		     (hflag && (dp->d_flags & RT_DIR_HIDDEN)) ||
		     (pflag && dp->d_addr == RT_DIR_PHONY_ADDR) ||
		     (!aflag && !hflag && !pflag &&
		      !(dp->d_flags & RT_DIR_HIDDEN) &&
		      (dp->d_addr != RT_DIR_PHONY_ADDR))))
		    *dirp++ = dp;
	    }
    }

    bu_vls_init(&vls);
    wdb_vls_col_pr4v(&vls, dirp0, (int)(dirp - dirp0), no_decorate);
    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)0);
    bu_vls_free(&vls);
    bu_free((void *)dirp0, "wdb_tops_cmd: wdb_dir_getspace");

    return TCL_OK;
}


/**
 * NON-PARALLEL because of rt_uniresource
 */
static int
wdb_tops_tcl(void *clientData,
	     int argc,
	     const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_tops_cmd(wdbp, argc-1, argv+1);
}


/**
 *@brief
 * Called when the named proc created by rt_gettrees() is destroyed.
 */
static void
wdb_deleteProc_rt(void *clientData)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;

    RT_AP_CHECK(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    rt_free_rti(rtip);
    ap->a_rt_i = (struct rt_i *)NULL;

    bu_free((void *)ap, "struct application");
}


int
wdb_rt_gettrees_cmd(struct rt_wdb *wdbp,
		    int argc,
		    const char *argv[])
{
    struct rt_i *rtip;
    struct application *ap;
    struct resource *resp;
    const char *newprocname;

    RT_CK_WDB(wdbp);
    RT_CK_DBI(wdbp->dbip);

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_rt_gettrees %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    rtip = rt_new_rti(wdbp->dbip);
    newprocname = argv[1];

    /* Delete previous proc (if any) to release all that memory, first */
    (void)Tcl_DeleteCommand(wdbp->wdb_interp, newprocname);

    while (argc > 2 && argv[2][0] == '-') {
	if (BU_STR_EQUAL(argv[2], "-i")) {
	    rtip->rti_dont_instance = 1;
	    argc--;
	    argv++;
	    continue;
	}
	if (BU_STR_EQUAL(argv[2], "-u")) {
	    rtip->useair = 1;
	    argc--;
	    argv++;
	    continue;
	}
	break;
    }

    if (argc-2 < 1) {
	Tcl_AppendResult(wdbp->wdb_interp,
			 "rt_gettrees(): no geometry has been specified ", (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_gettrees(rtip, argc-2, (const char **)&argv[2], 1) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp,
			 "rt_gettrees() returned error", (char *)NULL);
	rt_free_rti(rtip);
	return TCL_ERROR;
    }

    /* Establish defaults for this rt_i */
    rtip->rti_hasty_prep = 1;	/* Tcl isn't going to fire many rays */

    /*
     * In case of multiple instances of the library, make sure that
     * each instance has a separate resource structure,
     * because the bit vector lengths depend on # of solids.
     * And the "overwrite" sequence in Tcl is to create the new
     * proc before running the Tcl_CmdDeleteProc on the old one,
     * which in this case would trash rt_uniresource.
     * Once on the rti_resources list, rt_clean() will clean 'em up.
     */
    BU_ALLOC(resp, struct resource);
    rt_init_resource(resp, 0, rtip);
    BU_ASSERT_PTR(BU_PTBL_GET(&rtip->rti_resources, 0), !=, NULL);

    BU_ALLOC(ap, struct application);
    RT_APPLICATION_INIT(ap);
    ap->a_magic = RT_AP_MAGIC;
    ap->a_resource = resp;
    ap->a_rt_i = rtip;
    ap->a_purpose = "Conquest!";

    rt_ck(rtip);

    /* Instantiate the proc, with clientData of wdb */
    /* Beware, returns a "token", not TCL_OK. */
    (void)Tcl_CreateCommand(wdbp->wdb_interp, newprocname, rt_tcl_rt,
			    (ClientData)ap, wdb_deleteProc_rt);

    /* Return new function name as result */
    Tcl_AppendResult(wdbp->wdb_interp, newprocname, (char *)NULL);

    return TCL_OK;
}


/**
 *@brief
 * Given an instance of a database and the name of some treetops,
 * create a named "ray-tracing" object (proc) which will respond to
 * subsequent operations.
 * Returns new proc name as result.
 *
 * @par Example:
 *	.inmem rt_gettrees .rt all.g light.r
 */
static int
wdb_rt_gettrees_tcl(void *clientData,
		    int argc,
		    const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_rt_gettrees_cmd(wdbp, argc-1, argv+1);
}


struct showmats_data {
    Tcl_Interp *smd_interp;
    int smd_count;
    char *smd_child;
    mat_t smd_mat;
};


static void
Do_showmats(struct db_i *dbip,
	    struct rt_comb_internal *UNUSED(comb),
	    union tree *comb_leaf,
	    void *user_ptr1,
	    void *UNUSED(user_ptr2),
	    void *UNUSED(user_ptr3),
	    void *UNUSED(user_ptr4))
{
    struct showmats_data *smdp;

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    smdp = (struct showmats_data *)user_ptr1;

    if (!BU_STR_EQUAL(comb_leaf->tr_l.tl_name, smdp->smd_child))
	return;

    smdp->smd_count++;
    if (smdp->smd_count > 1) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "\n\tOccurrence #%d:\n", smdp->smd_count);
	Tcl_AppendResult(smdp->smd_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
    }

    bn_tcl_mat_print(smdp->smd_interp, "", comb_leaf->tr_l.tl_mat);
    if (smdp->smd_count == 1) {
	mat_t tmp_mat;
	if (comb_leaf->tr_l.tl_mat) {
	    bn_mat_mul(tmp_mat, smdp->smd_mat, comb_leaf->tr_l.tl_mat);
	    MAT_COPY(smdp->smd_mat, tmp_mat);
	}
    }
}


int
wdb_showmats_cmd(struct rt_wdb *wdbp,
		 int argc,
		 const char *argv[])
{
    struct showmats_data sm_data;
    const char *parent;
    struct directory *dp;
    int max_count=1;
    char *argv1;

    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_showmats %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    sm_data.smd_interp = wdbp->wdb_interp;
    MAT_IDN(sm_data.smd_mat);

    argv1 = bu_strdup(argv[1]);
    parent = strtok(argv1, "/");
    while ((sm_data.smd_child = strtok((char *)NULL, "/")) != NULL) {
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;

	if ((dp = db_lookup(wdbp->dbip, parent, LOOKUP_NOISY)) == RT_DIR_NULL)
	    return TCL_ERROR;

	Tcl_AppendResult(wdbp->wdb_interp, parent, "\n", (char *)NULL);

	if (!(dp->d_flags & RT_DIR_COMB)) {
	    Tcl_AppendResult(wdbp->wdb_interp, "\tThis is not a combination\n", (char *)NULL);
	    break;
	}

	if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0)
	    WDB_TCL_READ_ERR_return;
	comb = (struct rt_comb_internal *)intern.idb_ptr;

	sm_data.smd_count = 0;

	if (comb->tree)
	    db_tree_funcleaf(wdbp->dbip, comb, comb->tree, Do_showmats,
			     (void *)&sm_data, (void *)NULL, (void *)NULL, (void *)NULL);
	rt_db_free_internal(&intern);

	if (!sm_data.smd_count) {
	    Tcl_AppendResult(wdbp->wdb_interp, sm_data.smd_child, " is not a member of ",
			     parent, "\n", (char *)NULL);
	    return TCL_ERROR;
	}
	if (sm_data.smd_count > max_count)
	    max_count = sm_data.smd_count;

	parent = sm_data.smd_child;
    }
    Tcl_AppendResult(wdbp->wdb_interp, parent, "\n", (char *)NULL);
    bu_free(argv1, "free strdup");

    if (max_count > 1)
	Tcl_AppendResult(wdbp->wdb_interp, "\nAccumulated matrix (using first occurrence of each object):\n", (char *)NULL);
    else
	Tcl_AppendResult(wdbp->wdb_interp, "\nAccumulated matrix:\n", (char *)NULL);

    bn_tcl_mat_print(wdbp->wdb_interp, "", sm_data.smd_mat);

    return TCL_OK;
}


static int
wdb_showmats_tcl(void *clientData,
		 int argc,
		 const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_showmats_cmd(wdbp, argc-1, argv+1);
}


int
wdb_shells_cmd(struct rt_wdb *wdbp,
	       int argc,
	       const char *argv[])
{
    struct directory *old_dp, *new_dp;
    struct rt_db_internal old_intern, new_intern;
    struct model *m_tmp, *m;
    struct nmgregion *r_tmp, *r;
    struct shell *s_tmp, *s;
    int shell_count=0;
    struct bu_vls shell_name;
    long **trans_tbl;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_shells %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((old_dp = db_lookup(wdbp->dbip, argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return TCL_ERROR;

    if (rt_db_get_internal(&old_intern, old_dp, wdbp->dbip, bn_mat_identity, &rt_uniresource) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "rt_db_get_internal() error\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (old_intern.idb_type != ID_NMG) {
	Tcl_AppendResult(wdbp->wdb_interp, "Object is not an NMG!!!\n", (char *)NULL);
	return TCL_ERROR;
    }

    m = (struct model *)old_intern.idb_ptr;
    NMG_CK_MODEL(m);

    bu_vls_init(&shell_name);
    for (BU_LIST_FOR (r, nmgregion, &m->r_hd)) {
	for (BU_LIST_FOR (s, shell, &r->s_hd)) {
	    s_tmp = nmg_dup_shell(s, &trans_tbl, &wdbp->wdb_tol);
	    bu_free((void *)trans_tbl, "trans_tbl");

	    m_tmp = nmg_mmr();
	    r_tmp = BU_LIST_FIRST(nmgregion, &m_tmp->r_hd);

	    BU_LIST_DEQUEUE(&s_tmp->l);
	    BU_LIST_APPEND(&r_tmp->s_hd, &s_tmp->l);
	    s_tmp->r_p = r_tmp;
	    nmg_m_reindex(m_tmp, 0);
	    nmg_m_reindex(m, 0);

	    bu_vls_printf(&shell_name, "shell.%d", shell_count);
	    while (db_lookup(wdbp->dbip, bu_vls_addr(&shell_name), 0) != RT_DIR_NULL) {
		bu_vls_trunc(&shell_name, 0);
		shell_count++;
		bu_vls_printf(&shell_name, "shell.%d", shell_count);
	    }

	    /* Export NMG as a new solid */
	    RT_DB_INTERNAL_INIT(&new_intern);
	    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	    new_intern.idb_type = ID_NMG;
	    new_intern.idb_meth = &OBJ[ID_NMG];
	    new_intern.idb_ptr = (void *)m_tmp;

	    new_dp = db_diradd(wdbp->dbip, bu_vls_addr(&shell_name), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&new_intern.idb_type);
	    if (new_dp == RT_DIR_NULL) {
		WDB_TCL_ALLOC_ERR_return;
	    }

	    /* make sure the geometry/bounding boxes are up to date */
	    nmg_rebound(m_tmp, &wdbp->wdb_tol);


	    if (rt_db_put_internal(new_dp, wdbp->dbip, &new_intern, &rt_uniresource) < 0) {
		/* Free memory */
		nmg_km(m_tmp);
		Tcl_AppendResult(wdbp->wdb_interp, "rt_db_put_internal() failure\n", (char *)NULL);
		return TCL_ERROR;
	    }
	    /* Internal representation has been freed by rt_db_put_internal */
	    new_intern.idb_ptr = (void *)NULL;
	}
    }
    bu_vls_free(&shell_name);

    return TCL_OK;
}


static int
wdb_shells_tcl(void *clientData,
	       int argc,
	       const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_shells_cmd(wdbp, argc-1, argv+1);
}


int
wdb_dump_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    struct rt_wdb *op;
    int ret;

    RT_CK_WDB(wdbp);
    RT_CK_DBI(wdbp->dbip);

    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_dump %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((op = wdb_fopen(argv[1])) == RT_WDB_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, "dump:  ", argv[1],
			 ": cannot create", (char *)NULL);
	return TCL_ERROR;
    }

    ret = db_dump(op, wdbp->dbip);
    wdb_close(op);

    if (ret < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "dump ", argv[1],
			 ": db_dump() error", (char *)NULL);
	return TCL_ERROR;
    }

    return TCL_OK;
}


/**
 *@brief
 * Write the current state of a database object out to a file.
 *
 * @par Example:
 *	.inmem dump "/tmp/foo.g"
 */
static int
wdb_dump_tcl(void *clientData,
	     int argc,
	     const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_dump_cmd(wdbp, argc-1, argv+1);
}


int
wdb_stub_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    if (argc != 1) {
	struct bu_vls vls;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_%s %s", argv[0], argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    Tcl_AppendResult(wdbp->wdb_interp, "%s: no database is currently opened!", argv[0], (char *)NULL);
    return TCL_ERROR;
}


/**
 * Stub command callback for commands that only exist after a database
 * is opened (e.g., db).
 *
 * @returns false
 */
static int
wdb_stub_tcl(void *clientData,
	     int argc,
	     const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_stub_cmd(wdbp, argc-1, argv+1);
}


int
wdb_dbip_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    struct bu_vls vls;

    bu_vls_init(&vls);

    if (argc != 1) {
	bu_vls_printf(&vls, "helplib_alias wdb_dbip %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_printf(&vls, "%p", (void *)wdbp->dbip);
    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
}


/**
 *
 * @par Usage:
 * procname dbip
 *
 * @return database objects dbip.
 */
static int
wdb_dbip_tcl(void *clientData,
	     int argc,
	     const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_dbip_cmd(wdbp, argc-1, argv+1);
}


int
wdb_ls_cmd(struct rt_wdb *wdbp,
	   int argc,
	   const char *argv[])
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
    int attr_flag = 0;	/* arguments are attribute name/value pairs */
    int or_flag = 0;	/* flag indicating that any one attribute match is sufficient
			 * default is all attributes must match.
			 */
    struct directory **dirp;
    struct directory **dirp0 = (struct directory **)NULL;
    const char *cmdname = argv[0];

    bu_vls_init(&vls);

    if (argc < 1) {
	bu_vls_printf(&vls, "helplib_alias wdb_ls %s", cmdname);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_optind = 1;	/* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "acrslopA")) != -1) {
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
		bu_vls_printf(&vls, "Unrecognized option - %c", c);
		Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);
		return TCL_ERROR;
	}
    }
    /* skip options processed plus command name, should just leave object names */
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
	    bu_vls_printf(&vls, "helplib_alias wdb_ls %s", cmdname);
	    Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
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
		bu_avs_add_nonunique(&avs, argv[i], argv[i+1]);
	    } else {
		bu_avs_add(&avs, argv[i], argv[i+1]);
	    }
	}

	tbl = db_lookup_by_attr(wdbp->dbip, dir_flags, &avs, op);
	bu_avs_free(&avs);

	dirp = wdb_getspace(wdbp->dbip, BU_PTBL_LEN(tbl));
	dirp0 = dirp;
	for (i = 0; i < BU_PTBL_LEN(tbl); i++) {
	    *dirp++ = (struct directory *)BU_PTBL_GET(tbl, i);
	}

	bu_ptbl_free(tbl);
	bu_free((char *)tbl, "wdb_ls_cmd ptbl");
    } else if (argc > 0) {
	/* Just list specified names */
	dirp = wdb_getspace(wdbp->dbip, argc);
	dirp0 = dirp;
	/*
	 * Verify the names, and add pointers to them to the array.
	 */
	for (i = 0; i < (size_t)argc; i++) {
	    if ((dp = db_lookup(wdbp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL)
		continue;
	    *dirp++ = dp;
	}
    } else {
	/* Full table of contents */
	dirp = wdb_getspace(wdbp->dbip, 0);	/* Enough for all */
	dirp0 = dirp;
	/*
	 * Walk the directory list adding pointers (to the directory
	 * entries) to the array.
	 */
	for (i = 0; i < RT_DBNHASH; i++)
	    for (dp = wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		if (!aflag && (dp->d_flags & RT_DIR_HIDDEN))
		    continue;
		*dirp++ = dp;
	    }
    }

    if (lflag)
	wdb_vls_long_dpp(wdbp, &vls, dirp0, (int)(dirp - dirp0),
			 aflag, cflag, rflag, sflag);
    else if (aflag || cflag || rflag || sflag)
	wdb_vls_line_dpp(wdbp, &vls, dirp0, (int)(dirp - dirp0),
			 aflag, cflag, rflag, sflag);
    else
	wdb_vls_col_pr4v(&vls, dirp0, (int)(dirp - dirp0), 0);

    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    bu_free((void *)dirp0, "wdb_getspace dp[]");

    return TCL_OK;
}


/**
 *
 * Usage:
 * procname ls [args]
 *
 * @return list objects in this database object.
 */
static int
wdb_ls_tcl(void *clientData,
	   int argc,
	   const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_ls_cmd(wdbp, argc-1, argv+1);
}


int
wdb_pathsum_cmd(struct rt_wdb *wdbp,
		int argc,
		const char *argv[])
{
    int i, pos_in;
    struct _ged_trace_data gtd;
    struct ged ged;
    char *argv1;

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias %s%s %s", "wdb_", argv[0], argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /*XXX Temporary */
    GED_INIT(&ged, wdbp);

    /*
     * paths are matched up to last input member
     * ANY path the same up to this point is considered as matching
     */

    /* initialize gtd */
    gtd.gtd_gedp = &ged;
    gtd.gtd_flag = _GED_CPEVAL;
    gtd.gtd_prflag = 0;

    pos_in = 1;

    /* find out which command was entered */
    if (BU_STR_EQUAL(argv[0], "paths")) {
	/* want to list all matching paths */
	gtd.gtd_flag = _GED_LISTPATH;
    }
    if (BU_STR_EQUAL(argv[0], "listeval")) {
	/* want to list evaluated solid[s] */
	gtd.gtd_flag = _GED_LISTEVAL;
    }

    argv1 = bu_strdup(argv[1]);
    if (argc == 2 && strchr(argv1, '/')) {
	const char *tok;
	gtd.gtd_objpos = 0;

	tok = strtok(argv1, "/");
	while (tok) {
	    if ((gtd.gtd_obj[gtd.gtd_objpos++] = db_lookup(wdbp->dbip, tok, LOOKUP_NOISY)) == RT_DIR_NULL) {

		/* release any allocated memory */
		ged_free(&ged);
		bu_free(argv1, "free strdup");

		return TCL_ERROR;
	    }
	    tok = strtok((char *)NULL, "/");
	}
    } else {
	gtd.gtd_objpos = argc-1;

	/* build directory pointer array for desired path */
	for (i = 0; i < gtd.gtd_objpos; i++) {
	    if ((gtd.gtd_obj[i] = db_lookup(wdbp->dbip, argv[pos_in+i], LOOKUP_NOISY)) == RT_DIR_NULL) {

		/* release any allocated memory */
		ged_free(&ged);
		bu_free(argv1, "free strdup");

		return TCL_ERROR;
	    }
	}
    }
    bu_free(argv1, "free strdup");

    MAT_IDN(gtd.gtd_xform);

    _ged_trace(gtd.gtd_obj[0], 0, bn_mat_identity, &gtd, 1);

    if (gtd.gtd_prflag == 0) {
	/* path not found */
	Tcl_AppendResult(wdbp->wdb_interp, "PATH:  ", (char *)NULL);
	for (i = 0; i < gtd.gtd_objpos; i++)
	    Tcl_AppendResult(wdbp->wdb_interp, "/", gtd.gtd_obj[i]->d_namep, (char *)NULL);

	Tcl_AppendResult(wdbp->wdb_interp, "  NOT FOUND\n", (char *)NULL);
    }

    /* release any allocated memory */
    ged_free(&ged);

    return TCL_OK;
}


/**
 *@brief
 * Common code for several direct db methods: listeval, paths
 * Also used as support routine for "l" (list) command.
 *
 * 1.  produces path for purposes of matching
 * 2.  gives all paths matching the input path OR
 * 3.  gives a summary of all paths matching the input path
 * including the final parameters of the solids at the bottom
 * of the matching paths
 *
 * Usage:
 * procname (WDB_LISTEVAL|paths) args(s)
 */
static int
wdb_pathsum_tcl(void *clientData,
		int argc,
		const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_pathsum_cmd(wdbp, argc-1, argv+1);
}


int
wdb_list_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    struct directory *dp;
    int arg;
    struct bu_vls str;
    int id;
    int recurse = 0;
    char *listeval = "listeval";
    struct rt_db_internal intern;

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_list %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (argc > 1 && BU_STR_EQUAL(argv[1], "-r")) {
	recurse = 1;

	/* skip past used args */
	--argc;
	++argv;
    }

    /* skip past used args */
    --argc;
    ++argv;

    bu_vls_init(&str);

    for (arg = 0; arg < argc; arg++) {
	if (recurse) {
	    const char *tmp_argv[3];

	    tmp_argv[0] = listeval;
	    tmp_argv[1] = argv[arg];
	    tmp_argv[2] = (char *)NULL;

	    wdb_pathsum_cmd(wdbp, 2, tmp_argv);
	} else if (strchr(argv[arg], '/')) {
	    struct db_tree_state ts;
	    struct db_full_path path;

	    db_full_path_init(&path);
	    ts = wdbp->wdb_initial_tree_state;     /* struct copy */
	    ts.ts_dbip = wdbp->dbip;
	    ts.ts_resp = &rt_uniresource;
	    MAT_IDN(ts.ts_mat);

	    if (db_follow_path_for_state(&ts, &path, argv[arg], 1))
		continue;

	    dp = DB_FULL_PATH_CUR_DIR(&path);

	    if ((id = rt_db_get_internal(&intern, dp, wdbp->dbip, ts.ts_mat, &rt_uniresource)) < 0) {
		Tcl_AppendResult(wdbp->wdb_interp, "rt_db_get_internal(", dp->d_namep,
				 ") failure", (char *)NULL);
		continue;
	    }

	    db_free_full_path(&path);

	    bu_vls_printf(&str, "%s:  ", argv[arg]);

	    if (!OBJ[id].ft_describe || OBJ[id].ft_describe(&str, &intern, 99, wdbp->dbip->dbi_base2local, &rt_uniresource, wdbp->dbip) < 0)
		Tcl_AppendResult(wdbp->wdb_interp, dp->d_namep, ": describe error", (char *)NULL);

	    rt_db_free_internal(&intern);
	} else {
	    if ((dp = db_lookup(wdbp->dbip, argv[arg], LOOKUP_NOISY)) == RT_DIR_NULL)
		continue;

	    wdb_do_list(wdbp->dbip, &str, dp, 99);	/* very verbose */
	}
    }

    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&str), (char *)NULL);
    bu_vls_free(&str);

    return TCL_OK;
}


/**
 *
 * Usage:
 * procname l [-r] arg(s)
 *
 *@brief
 * List object information, verbose.
 */
static int
wdb_list_tcl(void *clientData, int argc, const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_list_cmd(wdbp, argc-1, argv+1);
}


static void
wdb_scrape_escapes_AppendResult(Tcl_Interp *interp, const char *str)
{
    char buf[2];
    buf[1] = '\0';

    while (*str) {
	buf[0] = *str;
	if (*str != '\\') {
	    Tcl_AppendResult(interp, buf, NULL);
	} else if (*(str+1) == '\\') {
	    Tcl_AppendResult(interp, buf, NULL);
	    ++str;
	}
	if (*str == '\0')
	    break;
	++str;
    }
}


int
wdb_expand_cmd(struct rt_wdb *wdbp,
	       int argc,
	       const char *argv[])
{
    const char *pattern;
    struct directory *dp;
    int i, whicharg;
    int regexp, nummatch, thismatch, backslashed;

    if (argc < 1) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_expand %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    nummatch = 0;
    backslashed = 0;
    for (whicharg = 1; whicharg < argc; whicharg++) {
	/* If * ? or [ are present, this is a regular expression */
	pattern = argv[whicharg];
	regexp = 0;
	do {
	    if ((*pattern == '*' || *pattern == '?' || *pattern == '[') &&
		!backslashed) {
		regexp = 1;
		break;
	    }
	    if (*pattern == '\\' && !backslashed)
		backslashed = 1;
	    else
		backslashed = 0;
	} while (*pattern++);

	/* If it isn't a regexp, copy directly and continue */
	if (regexp == 0) {
	    if (nummatch > 0)
		Tcl_AppendResult(wdbp->wdb_interp, " ", NULL);
	    wdb_scrape_escapes_AppendResult(wdbp->wdb_interp, argv[whicharg]);
	    ++nummatch;
	    continue;
	}

	/* Search for pattern matches.
	 * If any matches are found, we do not have to worry about
	 * '\' escapes since the match coming from dp->d_namep will be
	 * clean. In the case of no matches, just copy the argument
	 * directly.
	 */

	pattern = argv[whicharg];
	thismatch = 0;
	for (i = 0; i < RT_DBNHASH; i++) {
	    for (dp = wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		if (bu_fnmatch(pattern, dp->d_namep, 0) != 0)
		    continue;
		/* Successful match */
		if (nummatch == 0)
		    Tcl_AppendResult(wdbp->wdb_interp, dp->d_namep, NULL);
		else
		    Tcl_AppendResult(wdbp->wdb_interp, " ", dp->d_namep, NULL);
		++nummatch;
		++thismatch;
	    }
	}
	if (thismatch == 0) {
	    if (nummatch > 0)
		Tcl_AppendResult(wdbp->wdb_interp, " ", NULL);
	    wdb_scrape_escapes_AppendResult(wdbp->wdb_interp, argv[whicharg]);
	}
    }

    return TCL_OK;
}


/**
 * @brief
 * Performs wildcard expansion (matched to the database elements)
 * on its given arguments.  The result is returned via interp result.
 *
 * @par Usage:
 * procname expand [args]
 */
static int
wdb_expand_tcl(void *clientData, int argc, const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_expand_cmd(wdbp, argc-1, argv+1);
}


int
wdb_kill_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    struct directory *dp;
    int i;
    int is_phony;
    int verbose = LOOKUP_NOISY;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_kill %s", argv[0]);
	if (wdbp->wdb_interp) {
	    Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	}
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* skip past "-f" */
    if (argc > 1 && BU_STR_EQUAL(argv[1], "-f")) {
	verbose = LOOKUP_QUIET;
	argc--;
	argv++;
    }

    for (i = 1; i < argc; i++) {
	if ((dp = db_lookup(wdbp->dbip,  argv[i], verbose)) != RT_DIR_NULL) {
	    is_phony = (dp->d_addr == RT_DIR_PHONY_ADDR);

	    /* don't worry about phony objects */
	    if (is_phony)
		continue;

	    /* notify drawable geometry objects associated with this database object */
	    if (i == argc-1)
		dgo_eraseobjall_callback(wdbp->dbip, dp, 1 /* notify other interested observers */);
	    else
		dgo_eraseobjall_callback(wdbp->dbip, dp, 0);

	    if (db_delete(wdbp->dbip, dp) || db_dirdelete(wdbp->dbip, dp)) {
		/* Abort kill processing on first error */
		Tcl_AppendResult(wdbp->wdb_interp, "an error occurred while deleting ", argv[i], (char *)NULL);
		return TCL_ERROR;
	    }
	}
    }

    return TCL_OK;
}


/*
 * Usage:
 * procname kill arg(s)
 */
static int
wdb_kill_tcl(void *clientData,
	     int argc,
	     const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_kill_cmd(wdbp, argc-1, argv+1);
}


int
wdb_killall_cmd(struct rt_wdb *wdbp,
		int argc,
		const char *argv[])
{
    int i, k;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int ret;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_killall %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    ret = TCL_OK;

    /* Examine all COMB nodes */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (!(dp->d_flags & RT_DIR_COMB))
		continue;

	    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		Tcl_AppendResult(wdbp->wdb_interp, "rt_db_get_internal(", dp->d_namep,
				 ") failure", (char *)NULL);
		ret = TCL_ERROR;
		continue;
	    }
	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    RT_CK_COMB(comb);

	    for (k = 1; k < argc; k++) {
		int code;

		code = db_tree_del_dbleaf(&(comb->tree), argv[k], &rt_uniresource, 0);
		if (code == -1)
		    continue;	/* not found */
		if (code == -2)
		    continue;	/* empty tree */
		if (code < 0) {
		    Tcl_AppendResult(wdbp->wdb_interp, "  ERROR_deleting ",
				     dp->d_namep, "/", argv[k],
				     "\n", (char *)NULL);
		    ret = TCL_ERROR;
		} else {
		    Tcl_AppendResult(wdbp->wdb_interp, "deleted ",
				     dp->d_namep, "/", argv[k],
				     "\n", (char *)NULL);
		}
	    }

	    if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
		Tcl_AppendResult(wdbp->wdb_interp,
				 "ERROR: Unable to write new combination into database.\n",
				 (char *)NULL);
		ret = TCL_ERROR;
		continue;
	    }
	}
    }

    if (ret != TCL_OK) {
	Tcl_AppendResult(wdbp->wdb_interp,
			 "KILL skipped because of earlier errors.\n",
			 (char *)NULL);
	return ret;
    }

    /* ALL references removed...now KILL the object[s] */
    /* reuse argv[] */
    argv[0] = "kill";
    return wdb_kill_cmd(wdbp, argc, argv);
}


static void
wdb_killtree_callback(struct db_i *dbip,
		      struct directory *dp,
		      void *ptr) {
    struct wdb_killtree_data *ktdp = (struct wdb_killtree_data *)ptr;
    Tcl_Interp *interp = ktdp->interp;

    if (dbip == DBI_NULL)
	return;

    Tcl_AppendResult(interp, "KILL ", (dp->d_flags & RT_DIR_COMB) ? "COMB" : "Solid",
		     ":  ", dp->d_namep, "\n", (char *)NULL);

    /* notify drawable geometry objects associated with this database object */
    dgo_eraseobjall_callback(dbip, dp, ktdp->notify);

    if (db_delete(dbip, dp) || db_dirdelete(dbip, dp)) {
	Tcl_AppendResult(interp, "an error occurred while deleting ", dp->d_namep, "\n", (char *)NULL);
    }
}


/**
 * @brief
 * Kill object[s] and remove all references to the object[s].
 *
 * Usage:
 * procname killall arg(s)
 */
static int
wdb_killall_tcl(void *clientData,
		int argc,
		const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_killall_cmd(wdbp, argc-1, argv+1);
}


int
wdb_killtree_cmd(struct rt_wdb *wdbp,
		 int argc,
		 const char *argv[])
{
    struct directory *dp;
    int i;
    struct wdb_killtree_data ktd;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_killtree %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    ktd.interp = wdbp->wdb_interp;
    ktd.notify = 0;

    for (i = 1; i < argc; i++) {
	if ((dp = db_lookup(wdbp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;

	/* ignore phony objects */
	if (dp->d_addr == RT_DIR_PHONY_ADDR)
	    continue;

	db_functree(wdbp->dbip, dp,
		    wdb_killtree_callback, wdb_killtree_callback,
		    wdbp->wdb_resp, (void *)&ktd);
    }

    dgo_notifyWdb(wdbp);

    return TCL_OK;
}


/**
 * @brief
 * Kill all paths belonging to an object.
 *
 * Usage:
 * procname killtree arg(s)
 */
static int
wdb_killtree_tcl(void *clientData,
		 int argc,
		 const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_killtree_cmd(wdbp, argc-1, argv+1);
}


/**
 * guts to the 'cp' command, used to shallow-copy an object
 *
 */
int
wdb_copy_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    struct directory *proto;
    struct directory *dp;
    struct bu_external external;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc != 3) {
	struct bu_vls vls;

	if (!wdbp->wdb_interp) {
	    return TCL_ERROR;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_copy %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((proto = db_lookup(wdbp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return TCL_ERROR;

    if (db_lookup(wdbp->dbip, argv[2], LOOKUP_QUIET) != RT_DIR_NULL) {
	if (wdbp->wdb_interp) {
	    Tcl_AppendResult(wdbp->wdb_interp, argv[2], ":  already exists", (char *)NULL);
	} else {
	    bu_log("%s: already exists\n", argv[2]);
	}
	return TCL_ERROR;
    }

    if (db_get_external(&external, proto, wdbp->dbip)) {
	if (wdbp->wdb_interp) {
	    Tcl_AppendResult(wdbp->wdb_interp, "Database read error, aborting", (char *)NULL);
	} else {
	    bu_log("Database read error, aborting\n");
	}
	return TCL_ERROR;
    }

    dp = db_diradd(wdbp->dbip, argv[2], RT_DIR_PHONY_ADDR, 0, proto->d_flags, (void *)&proto->d_minor_type);
    if (dp == RT_DIR_NULL) {
	if (wdbp->wdb_interp) {
	    Tcl_AppendResult(wdbp->wdb_interp, "An error has occurred while adding a new object to the database.", (char *)NULL);
	} else {
	    bu_log("An error has occurred while adding a new object to the database.");
	}
	return TCL_ERROR;
    }

    if (db_put_external(&external, dp, wdbp->dbip) < 0) {
	bu_free_external(&external);
	if (wdbp->wdb_interp) {
	    Tcl_AppendResult(wdbp->wdb_interp, "Database write error, aborting", (char *)NULL);
	} else {
	    bu_log("Database write error, aborting\n");
	}
	return TCL_ERROR;
    }
    bu_free_external(&external);

    return TCL_OK;
}


/**
 * @par Usage:
 * procname cp from to
 */
static int
wdb_copy_tcl(void *clientData,
	     int argc,
	     const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_copy_cmd(wdbp, argc-1, argv+1);
}


int
wdb_move_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc != 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_move %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((dp = db_lookup(wdbp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return TCL_ERROR;

    if (db_lookup(wdbp->dbip, argv[2], LOOKUP_QUIET) != RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, argv[2], ":  already exists", (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Database read error, aborting", (char *)NULL);
	return TCL_ERROR;
    }

    /* Change object name in the in-memory directory. */
    if (db_rename(wdbp->dbip, dp, argv[2]) < 0) {
	rt_db_free_internal(&intern);
	Tcl_AppendResult(wdbp->wdb_interp, "error in db_rename to ", argv[2],
			 ", aborting", (char *)NULL);
	return TCL_ERROR;
    }

    /* Re-write to the database.  New name is applied on the way out. */
    if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Database write error, aborting", (char *)NULL);
	return TCL_ERROR;
    }

    return TCL_OK;
}


/**
 * @brief
 * Rename an object.
 *
 * @par Usage:
 * procname mv from to
 */
static int
wdb_move_tcl(void *clientData,
	     int argc,
	     const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_move_cmd(wdbp, argc-1, argv+1);
}


int
wdb_move_all_cmd(struct rt_wdb *wdbp,
		 int argc,
		 const char *argv[])
{
    int i;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct bu_ptbl stack;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc != 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_moveall %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (db_version(wdbp->dbip) < 5 && (int)strlen(argv[2]) > NAMESIZE) {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "ERROR: name length limited to %d characters in v4 databases\n", NAMESIZE);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
	return TCL_ERROR;
    }


    /* rename the record itself */
    if ((dp = db_lookup(wdbp->dbip, argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return TCL_ERROR;

    if (db_lookup(wdbp->dbip, argv[2], LOOKUP_QUIET) != RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, argv[2], ":  already exists", (char *)NULL);
	return TCL_ERROR;
    }

    /* if this was a sketch, we need to look for all the extrude
     * objects that might use it.
     *
     * This has to be done here, before we rename the (possible) sketch object
     * because the extrude will do a rt_db_get on the sketch when we call
     * rt_db_get_internal on it.
     */
    if (dp->d_major_type == DB5_MAJORTYPE_BRLCAD && \
	dp->d_minor_type == DB5_MINORTYPE_BRLCAD_SKETCH) {

	struct directory *dirp;

	for (i = 0; i < RT_DBNHASH; i++) {
	    for (dirp = wdbp->dbip->dbi_Head[i]; dirp != RT_DIR_NULL; dirp = dirp->d_forw) {

		if (dirp->d_major_type == DB5_MAJORTYPE_BRLCAD && \
		    dirp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE) {
		    struct rt_extrude_internal *extrude;

		    if (rt_db_get_internal(&intern, dirp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
			bu_log("Can't get extrude %s?\n", dirp->d_namep);
			continue;
		    }
		    extrude = (struct rt_extrude_internal *)intern.idb_ptr;
		    RT_EXTRUDE_CK_MAGIC(extrude);

		    if (BU_STR_EQUAL(extrude->sketch_name, argv[1])) {
			bu_free(extrude->sketch_name, "sketch name");
			extrude->sketch_name = bu_strdup(argv[2]);

			if (rt_db_put_internal(dirp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
			    bu_log("oops\n");
			}
		    }
		}
	    }
	}
    }

    /* Change object name in the directory. */
    if (db_rename(wdbp->dbip, dp, argv[2]) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "error in rename to ", argv[2],
			 ", aborting", (char *)NULL);
	return TCL_ERROR;
    }

    /* Change name in the file */
    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Database read error, aborting", (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Database write error, aborting", (char *)NULL);
	return TCL_ERROR;
    }

    bu_ptbl_init(&stack, 64, "combination stack for wdb_mvall_cmd");


    /* Examine all COMB nodes */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    union tree *comb_leaf;
	    int done=0;
	    int changed=0;

	    if (!(dp->d_flags & RT_DIR_COMB))
		continue;

	    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0)
		continue;
	    comb = (struct rt_comb_internal *)intern.idb_ptr;

	    bu_ptbl_reset(&stack);
	    /* visit each leaf in the combination */
	    comb_leaf = comb->tree;
	    if (comb_leaf) {
		while (!done) {
		    while (comb_leaf->tr_op != OP_DB_LEAF) {
			bu_ptbl_ins(&stack, (long *)comb_leaf);
			comb_leaf = comb_leaf->tr_b.tb_left;
		    }

		    if (BU_STR_EQUAL(comb_leaf->tr_l.tl_name, argv[1])) {
			bu_free(comb_leaf->tr_l.tl_name, "comb_leaf->tr_l.tl_name");
			comb_leaf->tr_l.tl_name = bu_strdup(argv[2]);
			changed = 1;
		    }

		    if (BU_PTBL_END(&stack) < 1) {
			done = 1;
			break;
		    }
		    comb_leaf = (union tree *)BU_PTBL_GET(&stack, BU_PTBL_END(&stack)-1);
		    if (comb_leaf->tr_op != OP_DB_LEAF) {
			bu_ptbl_rm(&stack, (long *)comb_leaf);
			comb_leaf = comb_leaf->tr_b.tb_right;
		    }
		}
	    }

	    if (changed) {
		if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource)) {
		    bu_ptbl_free(&stack);
		    rt_db_free_internal(&intern);
		    Tcl_AppendResult(wdbp->wdb_interp,
				     "Database write error, aborting",
				     (char *)NULL);
		    return TCL_ERROR;
		}
	    } else
		rt_db_free_internal(&intern);
	}
    }

    bu_ptbl_free(&stack);
    return TCL_OK;
}


/**
 * @brief
 * Rename all occurrences of an object
 *
 * @par Usage:
 * procname mvall from to
 */
static int
wdb_move_all_tcl(void *clientData, int argc, const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_move_all_cmd(wdbp, argc-1, argv+1);
}


/**
 * find a new unique name given a list of previously used names, and
 * the type of naming mode that described what type of affix to use.
 */
static char *
get_new_name(const char *name,
	     struct db_i *dbip,
	     Tcl_HashTable *name_tbl,
	     Tcl_HashTable *used_names_tbl,
	     struct concat_data *cc_data)
{
    struct bu_vls new_name;
    struct bu_vls prev_name;
    Tcl_HashEntry *ptr = NULL;
    char *aname = NULL;
    char *ret_name = NULL;
    int new_entry = 0;
    long num = 0;
    long repeat = 0;

    RT_CK_DBI(dbip);
    BU_ASSERT(name_tbl);
    BU_ASSERT(used_names_tbl);
    BU_ASSERT(cc_data);

    if (!name) {
	bu_log("WARNING: encountered NULL name, renaming to \"UNKNOWN\"\n");
	name = "UNKNOWN";
    }

    ptr = Tcl_CreateHashEntry(name_tbl, name, &new_entry);

    if (!new_entry) {
	return (char *)Tcl_GetHashValue(ptr);
    }

    bu_vls_init(&new_name);
    bu_vls_init(&prev_name);

    do {
	/* iterate until we find an object name that is not in
	 * use, trying to accommodate the user's requested affix
	 * naming mode.
	 */

	bu_vls_trunc(&new_name, 0);

	if (cc_data->copy_mode & NO_AFFIX) {
	    if (num > 0 && cc_data->copy_mode & CUSTOM_PREFIX) {
		/* auto-increment prefix */
		bu_vls_printf(&new_name, "%ld_", num);
	    }
	    bu_vls_strcat(&new_name, name);
	    if (num > 0 && cc_data->copy_mode & CUSTOM_SUFFIX) {
		/* auto-increment suffix */
		bu_vls_printf(&new_name, "_%ld", num);
	    }
	} else if (cc_data->copy_mode & CUSTOM_SUFFIX) {
	    /* use custom suffix */
	    bu_vls_strcpy(&new_name, name);
	    if (num > 0) {
		bu_vls_printf(&new_name, "_%ld_", num);
	    }
	    bu_vls_vlscat(&new_name, &cc_data->affix);
	} else if (cc_data->copy_mode & CUSTOM_PREFIX) {
	    /* use custom prefix */
	    bu_vls_vlscat(&new_name, &cc_data->affix);
	    if (num > 0) {
		bu_vls_printf(&new_name, "_%ld_", num);
	    }
	    bu_vls_strcat(&new_name, name);
	} else if (cc_data->copy_mode & AUTO_SUFFIX) {
	    /* use auto-incrementing suffix */
	    bu_vls_strcat(&new_name, name);
	    bu_vls_printf(&new_name, "_%ld", num);
	} else if (cc_data->copy_mode & AUTO_PREFIX) {
	    /* use auto-incrementing prefix */
	    bu_vls_printf(&new_name, "%ld_", num);
	    bu_vls_strcat(&new_name, name);
	} else {
	    /* no custom suffix/prefix specified, use suffix */
	    if (num > 0) {
		bu_vls_printf(&new_name, "_%ld", num);
	    }
	    bu_vls_strcpy(&new_name, name);
	}

	/* make sure it fits for v4 */
	if (db_version(cc_data->old_dbip) < 5) {
	    if (bu_vls_strlen(&new_name) > V4_MAXNAME) {
		bu_log("ERROR: generated new name [%s] is too long (%ld > %d)\n", bu_vls_addr(&new_name), bu_vls_strlen(&new_name), V4_MAXNAME);
	    }
	    return NULL;
	}
	aname = bu_vls_addr(&new_name);

	num++;

	/* basic infinite loop sanity check */
	if (bu_vls_strcmp(&new_name, &prev_name) == 0) {
	    if (repeat > 100) {
		bu_log("ERROR: we seem to be stuck in an infinite loop generating a new name (%s)\n", bu_vls_addr(&new_name));
		return NULL;
	    }
	    repeat++;
	} else {
	    bu_vls_trunc(&prev_name, 0);
	    bu_vls_vlscat(&prev_name, &new_name);
	}

    } while (db_lookup(dbip, aname, LOOKUP_QUIET) != RT_DIR_NULL ||
	     Tcl_FindHashEntry(used_names_tbl, aname) != NULL);

    /* if they didn't get what they asked for, warn them */
    if (num > 1) {
	if (cc_data->copy_mode & NO_AFFIX) {
	    bu_log("WARNING: unable to import [%s] without an affix, imported as [%s]\n", name, bu_vls_addr(&new_name));
	} else if (cc_data->copy_mode & CUSTOM_SUFFIX) {
	    bu_log("WARNING: unable to import [%s] as [%s%s], imported as [%s]\n", name, name, bu_vls_addr(&cc_data->affix), bu_vls_addr(&new_name));
	} else if (cc_data->copy_mode & CUSTOM_PREFIX) {
	    bu_log("WARNING: unable to import [%s] as [%s%s], imported as [%s]\n", name, bu_vls_addr(&cc_data->affix), name, bu_vls_addr(&new_name));
	}
    }

    /* we should now have a unique name.  store it in the hash */
    ret_name = bu_vls_strgrab(&new_name);
    Tcl_SetHashValue(ptr, (ClientData)ret_name);
    (void)Tcl_CreateHashEntry(used_names_tbl, ret_name, &new_entry);
    bu_vls_free(&new_name);

    return ret_name;
}


static void
adjust_names(
    Tcl_Interp *interp,
    union tree *trp,
    struct db_i *dbip,
    Tcl_HashTable *name_tbl,
    Tcl_HashTable *used_names_tbl,
    struct concat_data *cc_data)
{
    char *new_name;

    if (trp == NULL) {
	return;
    }

    switch (trp->tr_op) {
	case OP_DB_LEAF:
	    new_name = get_new_name(trp->tr_l.tl_name, dbip,
				    name_tbl, used_names_tbl, cc_data);
	    if (new_name) {
		bu_free(trp->tr_l.tl_name, "leaf name");
		trp->tr_l.tl_name = bu_strdup(new_name);
	    }
	    break;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    adjust_names(interp, trp->tr_b.tb_left, dbip,
			 name_tbl, used_names_tbl, cc_data);
	    adjust_names(interp, trp->tr_b.tb_right, dbip,
			 name_tbl, used_names_tbl, cc_data);
	    break;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    adjust_names(interp, trp->tr_b.tb_left, dbip,
			 name_tbl, used_names_tbl, cc_data);
	    break;
    }
}


static int
copy_object(
    Tcl_Interp *interp,
    struct directory *input_dp,
    struct db_i *input_dbip,
    struct db_i *curr_dbip,
    Tcl_HashTable *name_tbl,
    Tcl_HashTable *used_names_tbl,
    struct concat_data *cc_data)
{
    struct rt_db_internal ip;
    struct rt_extrude_internal *extr;
    struct rt_dsp_internal *dsp;
    struct rt_comb_internal *comb;
    struct directory *new_dp;
    char *new_name;

    if (rt_db_get_internal(&ip, input_dp, input_dbip, NULL, &rt_uniresource) < 0) {
	Tcl_AppendResult(interp, "Failed to get internal form of object (", input_dp->d_namep,
			 ") - aborting!!!\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (ip.idb_major_type == DB5_MAJORTYPE_BRLCAD) {
	/* adjust names of referenced object in any object that reference other objects */
	switch (ip.idb_minor_type) {
	    case DB5_MINORTYPE_BRLCAD_COMBINATION:
		comb = (struct rt_comb_internal *)ip.idb_ptr;
		RT_CK_COMB(comb);
		adjust_names(interp, comb->tree, curr_dbip, name_tbl, used_names_tbl, cc_data);
		break;
	    case DB5_MINORTYPE_BRLCAD_EXTRUDE:
		extr = (struct rt_extrude_internal *)ip.idb_ptr;
		RT_EXTRUDE_CK_MAGIC(extr);

		new_name = get_new_name(extr->sketch_name, curr_dbip, name_tbl, used_names_tbl, cc_data);
		if (new_name) {
		    bu_free(extr->sketch_name, "sketch name");
		    extr->sketch_name = bu_strdup(new_name);
		}
		break;
	    case DB5_MINORTYPE_BRLCAD_DSP:
		dsp = (struct rt_dsp_internal *)ip.idb_ptr;
		RT_DSP_CK_MAGIC(dsp);

		if (dsp->dsp_datasrc == RT_DSP_SRC_OBJ) {
		    /* This dsp references a database object, may need to change its name */
		    new_name = get_new_name(bu_vls_addr(&dsp->dsp_name), curr_dbip,
					    name_tbl, used_names_tbl, cc_data);
		    if (new_name) {
			bu_vls_free(&dsp->dsp_name);
			bu_vls_strcpy(&dsp->dsp_name, new_name);
		    }
		}
		break;
	}
    }

    new_name = get_new_name(input_dp->d_namep, curr_dbip, name_tbl, used_names_tbl, cc_data);
    if (!new_name) {
	new_name = input_dp->d_namep;
    }
    new_dp = db_diradd(curr_dbip, new_name, RT_DIR_PHONY_ADDR, 0, input_dp->d_flags, (void *)&input_dp->d_minor_type);
    if (new_dp == RT_DIR_NULL) {
	Tcl_AppendResult(interp, "Failed to add new object name (", new_name,
			 ") to directory - aborting!!\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_db_put_internal(new_dp, curr_dbip, &ip, &rt_uniresource) < 0) {
	Tcl_AppendResult(interp, "Failed to write new object (", new_name,
			 ") to database - aborting!!\n", (char *)NULL);
	return TCL_ERROR;
    }

    return TCL_OK;
}


int
wdb_concat_cmd(struct rt_wdb *wdbp,
	       int argc,
	       const char *argv[])
{
    struct db_i *newdbp;
    struct directory *dp;
    Tcl_HashTable name_tbl;
    Tcl_HashTable used_names_tbl;
    Tcl_HashEntry *ptr;
    Tcl_HashSearch search;
    struct concat_data cc_data;
    const char *oldfile;
    struct bu_vls vls;

    WDB_TCL_CHECK_READ_ONLY;

    /* expecting either 3 or 4 args */
    if ((argc < 3) ||
	(argc > 4) ||
	(argc > 1 && argv[1][0] == '-' && argc != 4)) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_concat %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    memset(&cc_data, 0, sizeof(struct concat_data));
    bu_vls_init(&cc_data.affix);
    cc_data.copy_mode = 0;

    if (argv[1][0] == '-') {
	/* specified suffix or prefix explicitly */

	oldfile = argv[2];

	if (argv[1][1] == 'p') {

	    cc_data.copy_mode |= AUTO_PREFIX;

	    if (BU_STR_EQUAL(argv[3], "/")) {
		cc_data.copy_mode = NO_AFFIX | CUSTOM_PREFIX;
	    } else {
		(void)bu_vls_strcpy(&cc_data.affix, argv[3]);
		cc_data.copy_mode |= CUSTOM_PREFIX;
	    }

	} else if (argv[1][1] == 's') {

	    cc_data.copy_mode |= AUTO_SUFFIX;

	    if (BU_STR_EQUAL(argv[3], "/")) {
		cc_data.copy_mode = NO_AFFIX | CUSTOM_SUFFIX;
	    } else {
		(void)bu_vls_strcpy(&cc_data.affix, argv[3]);
		cc_data.copy_mode |= CUSTOM_SUFFIX;
	    }

	} else {
	    bu_vls_free(&cc_data.affix);

	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "helplib_alias wdb_concat %s", argv[0]);
	    Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

    } else {
	/* no prefix/suffix preference, use prefix */

	oldfile = argv[1];

	cc_data.copy_mode |= AUTO_PREFIX;

	if (BU_STR_EQUAL(argv[2], "/")) {
	    cc_data.copy_mode = NO_AFFIX | CUSTOM_PREFIX;
	} else {
	    (void)bu_vls_strcpy(&cc_data.affix, argv[2]);
	    cc_data.copy_mode |= CUSTOM_PREFIX;
	}

    }

    if (db_version(wdbp->dbip) < 5) {
	if (bu_vls_strlen(&cc_data.affix) > V4_MAXNAME-1) {
	    bu_log("ERROR: affix [%s] is too long for v%d\n", bu_vls_addr(&cc_data.affix), db_version(wdbp->dbip));
	    bu_vls_free(&cc_data.affix);
	    return TCL_ERROR;
	}
    }

    /* open the input file */
    if ((newdbp = db_open(oldfile, DB_OPEN_READONLY)) == DBI_NULL) {
	bu_vls_free(&cc_data.affix);
	perror(oldfile);
	Tcl_AppendResult(wdbp->wdb_interp, "%s: Can't open ", argv[0], oldfile, (char *)NULL);
	return TCL_ERROR;
    }

    if (db_version(newdbp) > 4 && db_version(wdbp->dbip) < 5) {
	bu_vls_free(&cc_data.affix);
	Tcl_AppendResult(wdbp->wdb_interp, argv[0], ": databases are incompatible, use dbupgrade on ",
			 wdbp->dbip->dbi_filename, " first",
			 (char *)NULL);
	return TCL_ERROR;
    }

    db_dirbuild(newdbp);

    cc_data.new_dbip = newdbp;
    cc_data.old_dbip = wdbp->dbip;

    /* visit each directory pointer in the input database */
    Tcl_InitHashTable(&name_tbl, TCL_STRING_KEYS);
    Tcl_InitHashTable(&used_names_tbl, TCL_STRING_KEYS);

    FOR_ALL_DIRECTORY_START(dp, newdbp) {
	if (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	    /* skip GLOBAL object */
	    continue;
	}
	copy_object(wdbp->wdb_interp, dp, newdbp, wdbp->dbip, &name_tbl, &used_names_tbl, &cc_data);
    } FOR_ALL_DIRECTORY_END;

    bu_vls_free(&cc_data.affix);
    rt_mempurge(&(newdbp->dbi_freep));

    /* Free all the directory entries, and close the input database */
    db_close(newdbp);

    db_sync(wdbp->dbip);	/* force changes to disk */

    /* Free the Hash tables */
    ptr = Tcl_FirstHashEntry(&name_tbl, &search);
    while (ptr) {
	bu_free((char *)Tcl_GetHashValue(ptr), "new name");
	ptr = Tcl_NextHashEntry(&search);
    }
    Tcl_DeleteHashTable(&name_tbl);
    Tcl_DeleteHashTable(&used_names_tbl);

    return TCL_OK;
}


/**
 * @brief
 * Concatenate another GED file into the current file.
 *
 * Usage:
 * procname concat file.g prefix
 */
static int
wdb_concat_tcl(void *clientData,
	       int argc,
	       const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_concat_cmd(wdbp, argc-1, argv+1);
}


int
wdb_copyeval_cmd(struct rt_wdb *wdbp,
		 int argc,
		 const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal internal, new_int;
    mat_t start_mat;
    int id;
    int i;
    int endpos;
    struct _ged_trace_data gtd;
    struct ged ged;
    char *argv2;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 3 || 27 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_copyeval %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /*XXX Temporary */
    GED_INIT(&ged, wdbp);

    /* initialize gtd */
    gtd.gtd_gedp = &ged;
    gtd.gtd_flag = _GED_CPEVAL;
    gtd.gtd_prflag = 0;

    /* check if new solid name already exists in description */
    if (db_lookup(wdbp->dbip, argv[1], LOOKUP_QUIET) != RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, argv[1], ": already exists\n", (char *)NULL);

	/* release any allocated memory */
	ged_free(&ged);

	return TCL_ERROR;
    }

    MAT_IDN(start_mat);

    /* build directory pointer array for desired path */
    argv2 = bu_strdup(argv[2]);
    if (argc == 3 && strchr(argv2, '/')) {
	const char *tok;

	endpos = 0;

	tok = strtok(argv2, "/");
	while (tok) {
	    if ((gtd.gtd_obj[endpos++] = db_lookup(wdbp->dbip, tok, LOOKUP_NOISY)) == RT_DIR_NULL) {
		/* release any allocated memory */
		ged_free(&ged);
		bu_free(argv2, "free strdup");

		return TCL_ERROR;
	    }
	    tok = strtok((char *)NULL, "/");
	}
    } else {
	for (i = 2; i < argc; i++) {
	    if ((gtd.gtd_obj[i-2] = db_lookup(wdbp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
		/* release any allocated memory */
		ged_free(&ged);
		bu_free(argv2, "free strdup");

		return TCL_ERROR;
	    }
	}
	endpos = argc - 2;
    }
    bu_free(argv2, "free strdup");

    gtd.gtd_objpos = endpos - 1;

    /* Make sure that final component in path is a solid */
    if ((id = rt_db_get_internal(&internal, gtd.gtd_obj[endpos - 1], wdbp->dbip, bn_mat_identity, &rt_uniresource)) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "import failure on ",
			 argv[argc-1], "\n", (char *)NULL);

	/* release any allocated memory */
	ged_free(&ged);

	return TCL_ERROR;
    }

    if (id >= ID_COMBINATION) {
	rt_db_free_internal(&internal);
	Tcl_AppendResult(wdbp->wdb_interp, "final component on path must be a solid!!!\n", (char *)NULL);

	/* release any allocated memory */
	ged_free(&ged);

	return TCL_ERROR;
    }

    _ged_trace(gtd.gtd_obj[0], 0, start_mat, &gtd, 1);

    if (gtd.gtd_prflag == 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "PATH:  ", (char *)NULL);

	for (i = 0; i < gtd.gtd_objpos; i++)
	    Tcl_AppendResult(wdbp->wdb_interp, "/", gtd.gtd_obj[i]->d_namep, (char *)NULL);

	Tcl_AppendResult(wdbp->wdb_interp, "  NOT FOUND\n", (char *)NULL);
	rt_db_free_internal(&internal);

	/* release any allocated memory */
	ged_free(&ged);

	return TCL_ERROR;
    }

    /* Have found the desired path - wdb_xform is the transformation matrix */
    /* wdb_xform matrix calculated in wdb_trace() */

    /* create the new solid */
    RT_DB_INTERNAL_INIT(&new_int);
    if (rt_generic_xform(&new_int, gtd.gtd_xform,
			 &internal, 0, wdbp->dbip, &rt_uniresource)) {
	rt_db_free_internal(&internal);
	Tcl_AppendResult(wdbp->wdb_interp, "wdb_copyeval_cmd: rt_generic_xform failed\n", (char *)NULL);

	/* release any allocated memory */
	ged_free(&ged);

	return TCL_ERROR;
    }

    dp = db_diradd(wdbp->dbip, argv[1], RT_DIR_PHONY_ADDR, 0, gtd.gtd_obj[endpos-1]->d_flags, (void *)&new_int.idb_type);

    /* release any allocated memory */
    ged_free(&ged);

    if (dp == RT_DIR_NULL) {
	rt_db_free_internal(&internal);
	rt_db_free_internal(&new_int);
	WDB_TCL_ALLOC_ERR_return;
    }

    if (rt_db_put_internal(dp, wdbp->dbip, &new_int, &rt_uniresource) < 0) {
	rt_db_free_internal(&internal);
	rt_db_free_internal(&new_int);
	WDB_TCL_WRITE_ERR_return;
    }
    rt_db_free_internal(&internal);
    rt_db_free_internal(&new_int);

    return TCL_OK;
}


/**
 *
 *
 * Usage:
 * procname copyeval new_solid path_to_solid
 */
static int
wdb_copyeval_tcl(void *clientData,
		 int argc,
		 const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_copyeval_cmd(wdbp, argc-1, argv+1);
}


struct dir_check_stuff {
    struct db_i *main_dbip;
    struct rt_wdb *wdbp;
    struct directory **dup_dirp;
};


void
wdb_dir_check5(struct db_i *input_dbip,
	       const struct db5_raw_internal *rip,
	       off_t UNUSED(addr),
	       void *ptr)
{
    char *name;
    struct directory *dupdp;
    struct bu_vls local;
    struct dir_check_stuff *dcsp = (struct dir_check_stuff *)ptr;

    if (dcsp->main_dbip == DBI_NULL)
	return;

    RT_CK_DBI(input_dbip);
    RT_CK_RIP(rip);

    if (rip->h_dli == DB5HDR_HFLAGS_DLI_HEADER_OBJECT) return;
    if (rip->h_dli == DB5HDR_HFLAGS_DLI_FREE_STORAGE) return;

    name = (char *)rip->name.ext_buf;

    if (name == (char *)NULL) return;

    /* do not compare _GLOBAL */
    if (rip->major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY &&
	rip->minor_type == 0)
	return;

    /* Add the prefix, if any */
    bu_vls_init(&local);
    if (db_version(dcsp->main_dbip) < 5) {
	if (dcsp->wdbp->wdb_ncharadd > 0) {
	    bu_vls_strncpy(&local, bu_vls_addr(&dcsp->wdbp->wdb_prestr), dcsp->wdbp->wdb_ncharadd);
	    bu_vls_strcat(&local, name);
	} else {
	    bu_vls_strncpy(&local, name, V4_MAXNAME);
	}
	bu_vls_trunc(&local, V4_MAXNAME);
    } else {
	if (dcsp->wdbp->wdb_ncharadd > 0) {
	    (void)bu_vls_vlscat(&local, &dcsp->wdbp->wdb_prestr);
	    (void)bu_vls_strcat(&local, name);
	} else {
	    (void)bu_vls_strcat(&local, name);
	}
    }

    /* Look up this new name in the existing (main) database */
    if ((dupdp = db_lookup(dcsp->main_dbip, bu_vls_addr(&local), LOOKUP_QUIET)) != RT_DIR_NULL) {
	/* Duplicate found, add it to the list */
	dcsp->wdbp->wdb_num_dups++;
	*dcsp->dup_dirp++ = dupdp;
    }

    bu_vls_free(&local);

    return;
}


/**
 *@brief
 * Check a name against the global directory.
 */
int
wdb_dir_check(struct db_i *input_dbip, const char *name, off_t UNUSED(laddr), size_t UNUSED(len), int UNUSED(flags), void *ptr)
{
    struct directory *dupdp;
    struct bu_vls local;
    struct dir_check_stuff *dcsp = (struct dir_check_stuff *)ptr;

    if (dcsp->main_dbip == DBI_NULL)
	return 0;

    RT_CK_DBI(input_dbip);

    /* Add the prefix, if any */
    bu_vls_init(&local);
    if (db_version(dcsp->main_dbip) < 5) {
	if (dcsp->wdbp->wdb_ncharadd > 0) {
	    bu_vls_strncpy(&local, bu_vls_addr(&dcsp->wdbp->wdb_prestr), dcsp->wdbp->wdb_ncharadd);
	    bu_vls_strcat(&local, name);
	} else {
	    bu_vls_strncpy(&local, name, V4_MAXNAME);
	}
	bu_vls_trunc(&local, V4_MAXNAME);
    } else {
	if (dcsp->wdbp->wdb_ncharadd > 0) {
	    bu_vls_vlscat(&local, &dcsp->wdbp->wdb_prestr);
	    bu_vls_strcat(&local, name);
	} else {
	    bu_vls_strcat(&local, name);
	}
    }

    /* Look up this new name in the existing (main) database */
    if ((dupdp = db_lookup(dcsp->main_dbip, bu_vls_addr(&local), LOOKUP_QUIET)) != RT_DIR_NULL) {
	/* Duplicate found, add it to the list */
	dcsp->wdbp->wdb_num_dups++;
	*dcsp->dup_dirp++ = dupdp;
    }
    bu_vls_free(&local);
    return 0;
}


int
wdb_dup_cmd(struct rt_wdb *wdbp,
	    int argc,
	    const char *argv[])
{
    struct db_i *newdbp = DBI_NULL;
    struct directory **dirp0 = (struct directory **)NULL;
    struct bu_vls vls;
    struct dir_check_stuff dcs;

    if (argc < 2 || 3 < argc) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_dup %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_trunc(&wdbp->wdb_prestr, 0);
    if (argc == 3)
	(void)bu_vls_strcpy(&wdbp->wdb_prestr, argv[2]);

    wdbp->wdb_num_dups = 0;
    if (db_version(wdbp->dbip) < 5) {
	if ((wdbp->wdb_ncharadd = bu_vls_strlen(&wdbp->wdb_prestr)) > 12) {
	    wdbp->wdb_ncharadd = 12;
	    bu_vls_trunc(&wdbp->wdb_prestr, 12);
	}
    } else {
	wdbp->wdb_ncharadd = bu_vls_strlen(&wdbp->wdb_prestr);
    }

    /* open the input file */
    if ((newdbp = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[1]);
	Tcl_AppendResult(wdbp->wdb_interp, "dup: Can't open ", argv[1], (char *)NULL);
	return TCL_ERROR;
    }

    Tcl_AppendResult(wdbp->wdb_interp, "\n*** Comparing ",
		     wdbp->dbip->dbi_filename,
		     "  with ", argv[1], " for duplicate names\n", (char *)NULL);
    if (wdbp->wdb_ncharadd) {
	Tcl_AppendResult(wdbp->wdb_interp, "  For comparison, all names in ",
			 argv[1], " were prefixed with:  ",
			 bu_vls_addr(&wdbp->wdb_prestr), "\n", (char *)NULL);
    }

    /* Get array to hold names of duplicates */
    if ((dirp0 = wdb_getspace(wdbp->dbip, 0)) == (struct directory **) 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "f_dup: unable to get memory\n", (char *)NULL);
	db_close(newdbp);
	return TCL_ERROR;
    }

    /* Scan new database for overlaps */
    dcs.main_dbip = wdbp->dbip;
    dcs.wdbp = wdbp;
    dcs.dup_dirp = dirp0;
    if (db_version(newdbp) < 5) {
	if (db_scan(newdbp, wdb_dir_check, 0, (void *)&dcs) < 0) {
	    Tcl_AppendResult(wdbp->wdb_interp, "dup: db_scan failure", (char *)NULL);
	    bu_free((void *)dirp0, "wdb_getspace array");
	    db_close(newdbp);
	    return TCL_ERROR;
	}
    } else {
	if (db5_scan(newdbp, wdb_dir_check5, (void *)&dcs) < 0) {
	    Tcl_AppendResult(wdbp->wdb_interp, "dup: db_scan failure", (char *)NULL);
	    bu_free((void *)dirp0, "wdb_getspace array");
	    db_close(newdbp);
	    return TCL_ERROR;
	}
    }
    rt_mempurge(&(newdbp->dbi_freep));        /* didn't really build a directory */

    bu_vls_init(&vls);
    wdb_vls_col_pr4v(&vls, dirp0, (int)(dcs.dup_dirp - dirp0), 0);
    bu_vls_printf(&vls, "\n -----  %d duplicate names found  -----", wdbp->wdb_num_dups);
    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    bu_free((void *)dirp0, "wdb_getspace array");
    db_close(newdbp);

    return TCL_OK;
}


/**
 * @par Usage:
 * procname dup file.g [prefix]
 */
static int
wdb_dup_tcl(void *clientData, int argc, const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_dup_cmd(wdbp, argc-1, argv+1);
}


int
wdb_group_cmd(struct rt_wdb *wdbp,
	      int argc,
	      const char *argv[])
{
    struct directory *dp;
    int i;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_group %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* get objects to add to group */
    for (i = 2; i < argc; i++) {
	if ((dp = db_lookup(wdbp->dbip, argv[i], LOOKUP_NOISY)) != RT_DIR_NULL) {
	    if (wdb_combadd(wdbp->dbip, dp, argv[1], 0,
			    WMOP_UNION, 0, 0, wdbp) == RT_DIR_NULL)
		return TCL_ERROR;
	}  else
	    Tcl_AppendResult(wdbp->wdb_interp, "skip member ", argv[i], "\n", (char *)NULL);
    }
    return TCL_OK;
}


/**
 * @par Usage:
 * procname g groupname object1 object2 .... objectn
 */
static int
wdb_group_tcl(void *clientData,
	      int argc,
	      const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_group_cmd(wdbp, argc-1, argv+1);
}


int
wdb_remove_cmd(struct rt_wdb *wdbp,
	       int argc,
	       const char *argv[])
{
    struct directory *dp;
    int i;
    int num_deleted;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int ret;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_remove %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((dp = db_lookup(wdbp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return TCL_ERROR;

    if ((dp->d_flags & RT_DIR_COMB) == 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "rm: ", dp->d_namep,
			 " is not a combination", (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Database read error, aborting", (char *)NULL);
	return TCL_ERROR;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    /* Process each argument */
    num_deleted = 0;
    ret = TCL_OK;
    for (i = 2; i < argc; i++) {
	if (db_tree_del_dbleaf(&(comb->tree), argv[i], &rt_uniresource, 0) < 0) {
	    Tcl_AppendResult(wdbp->wdb_interp, "  ERROR_deleting ",
			     dp->d_namep, "/", argv[i],
			     "\n", (char *)NULL);
	    ret = TCL_ERROR;
	} else {
	    Tcl_AppendResult(wdbp->wdb_interp, "deleted ",
			     dp->d_namep, "/", argv[i],
			     "\n", (char *)NULL);
	    num_deleted++;
	}
    }

    if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Database write error, aborting", (char *)NULL);
	return TCL_ERROR;
    }

    return ret;
}


/**
 * @brief
 * Remove members from a combination.
 *
 * @par Usage:
 * procname remove comb object(s)
 */
static int
wdb_remove_tcl(void *clientData, int argc, const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_remove_cmd(wdbp, argc-1, argv+1);
}


int
wdb_region_cmd(struct rt_wdb *wdbp,
	       int argc,
	       const char *argv[])
{
    struct directory *dp;
    int i;
    int ident, air;
    char oper;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 4) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_region %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    ident = wdbp->wdb_item_default;
    air = wdbp->wdb_air_default;

    /* Check for even number of arguments */
    if (argc & 01) {
	Tcl_AppendResult(wdbp->wdb_interp, "error in number of args!", (char *)NULL);
	return TCL_ERROR;
    }

    if (db_lookup(wdbp->dbip, argv[1], LOOKUP_QUIET) == RT_DIR_NULL) {
	/* will attempt to create the region */
	if (wdbp->wdb_item_default) {
	    struct bu_vls tmp_vls;

	    wdbp->wdb_item_default++;
	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "Defaulting item number to %d\n",
			  wdbp->wdb_item_default);
	    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);
	}
    }

    /* Get operation and solid name for each solid */
    for (i = 2; i < argc; i += 2) {
	if (argv[i][1] != '\0') {
	    Tcl_AppendResult(wdbp->wdb_interp, "bad operation: ", argv[i],
			     " skip member: ", argv[i+1], "\n", (char *)NULL);
	    continue;
	}
	oper = argv[i][0];
	if ((dp = db_lookup(wdbp->dbip,  argv[i+1], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    Tcl_AppendResult(wdbp->wdb_interp, "skipping ", argv[i+1], "\n", (char *)NULL);
	    continue;
	}

	if (oper != WMOP_UNION && oper != WMOP_SUBTRACT && oper != WMOP_INTERSECT) {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "bad operation: %c skip member: %s\n",
			  oper, dp->d_namep);
	    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);
	    continue;
	}

	/* Adding region to region */
	if (dp->d_flags & RT_DIR_REGION) {
	    Tcl_AppendResult(wdbp->wdb_interp, "Note: ", dp->d_namep,
			     " is a region\n", (char *)NULL);
	}

	if (wdb_combadd(wdbp->dbip, dp,
			argv[1], 1, oper, ident, air, wdbp) == RT_DIR_NULL) {
	    Tcl_AppendResult(wdbp->wdb_interp, "error in combadd", (char *)NULL);
	    return TCL_ERROR;
	}
    }

    if (db_lookup(wdbp->dbip, argv[1], LOOKUP_QUIET) == RT_DIR_NULL) {
	/* failed to create region */
	if (wdbp->wdb_item_default > 1)
	    wdbp->wdb_item_default--;
	return TCL_ERROR;
    }

    return TCL_OK;
}


/**
 * @par Usage:
 * procname r rname object(s)
 */
static int
wdb_region_tcl(void *clientData, int argc, const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_region_cmd(wdbp, argc-1, argv+1);
}


int
wdb_comb_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    struct directory *dp;
    const char *comb_name;
    int i;
    char oper;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 4) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_comb %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* Check for odd number of arguments */
    if (argc & 01) {
	Tcl_AppendResult(wdbp->wdb_interp, "error in number of args!", (char *)NULL);
	return TCL_ERROR;
    }

    /* Save combination name, for use inside loop */
    comb_name = argv[1];
    dp = db_lookup(wdbp->dbip, comb_name, LOOKUP_QUIET);
    if (dp != RT_DIR_NULL) {
	if (!(dp->d_flags & RT_DIR_COMB)) {
	    Tcl_AppendResult(wdbp->wdb_interp,
			     "ERROR: ", comb_name,
			     " is not a combination", (char *)0);
	    return TCL_ERROR;
	}
    }

    /* Get operation and solid name for each solid */
    for (i = 2; i < argc; i += 2) {
	if (argv[i][1] != '\0') {
	    Tcl_AppendResult(wdbp->wdb_interp, "bad operation: ", argv[i],
			     " skip member: ", argv[i+1], "\n", (char *)NULL);
	    continue;
	}
	oper = argv[i][0];
	dp = db_lookup(wdbp->dbip,  argv[i+1], LOOKUP_NOISY);
	if (dp == RT_DIR_NULL) {
	    Tcl_AppendResult(wdbp->wdb_interp, "skipping ", argv[i+1], "\n", (char *)NULL);
	    continue;
	}

	if (oper != WMOP_UNION && oper != WMOP_SUBTRACT && oper != WMOP_INTERSECT) {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "bad operation: %c skip member: %s\n",
			  oper, dp->d_namep);
	    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);
	    continue;
	}

	if (wdb_combadd(wdbp->dbip, dp, comb_name, 0, oper, 0, 0, wdbp) == RT_DIR_NULL) {
	    Tcl_AppendResult(wdbp->wdb_interp, "error in combadd", (char *)NULL);
	    return TCL_ERROR;
	}
    }

    if (db_lookup(wdbp->dbip, comb_name, LOOKUP_QUIET) == RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, "Error:  ", comb_name,
			 " not created", (char *)NULL);
	return TCL_ERROR;
    }

    return TCL_OK;
}


/**
 * @brief
 * Create or add to the end of a combination, with one or more solids,
 * with explicitly specified operations.
 *
 * @par Usage:
 * procname comb comb_name opr1 sol1 opr2 sol2 ... oprN solN
 */
static int
wdb_comb_tcl(void *clientData, int argc, const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_comb_cmd(wdbp, argc-1, argv+1);
}


static void
wdb_find_ref(struct db_i *UNUSED(dbip),
	     struct rt_comb_internal *UNUSED(comb),
	     union tree *comb_leaf,
	     void *object,
	     void *comb_name_ptr,
	     void *user_ptr3,
	     void *UNUSED(user_ptr4))
{
    char *obj_name;
    char *comb_name;
    Tcl_Interp *interp = (Tcl_Interp *)user_ptr3;

    RT_CK_TREE(comb_leaf);

    obj_name = (char *)object;
    if (!BU_STR_EQUAL(comb_leaf->tr_l.tl_name, obj_name))
	return;

    comb_name = (char *)comb_name_ptr;

    Tcl_AppendElement(interp, comb_name);
}


HIDDEN union tree *
facetize_region_end(struct db_tree_state *UNUSED(tsp), const struct db_full_path *UNUSED(pathp), union tree *curtree, void *client_data)
{
    struct bu_list vhead;
    union tree **facetize_tree;

    facetize_tree = (union tree **)client_data;
    BU_LIST_INIT(&vhead);

    if (curtree->tr_op == OP_NOP) return curtree;

    if (*facetize_tree) {
	union tree *tr;
	BU_ALLOC(tr, union tree);
	RT_TREE_INIT(tr);
	tr->tr_op = OP_UNION;
	tr->tr_b.tb_regionp = REGION_NULL;
	tr->tr_b.tb_left = *facetize_tree;
	tr->tr_b.tb_right = curtree;
	*facetize_tree = tr;
    } else {
	*facetize_tree = curtree;
    }

    /* Tree has been saved, and will be freed later */
    return TREE_NULL;
}


int
wdb_facetize_cmd(struct rt_wdb *wdbp,
		 int argc,
		 const char *argv[])
{
    int i;
    int c;
    const char *newname;
    struct rt_db_internal intern;
    struct directory *dp;
    int failed;
    int nmg_use_tnurbs = 0;
    struct db_tree_state init_state;
    struct db_i *dbip;
    union tree *facetize_tree;
    struct model *nmg_model;

    /* static due to longjmp, keep gcc happy */
    static int triangulate;
    static int make_bot;

    if (argc < 3) {
	Tcl_AppendResult(wdbp->wdb_interp,
			 "Usage: ",
			 argv[0],
			 " new_object old_object [old_object2 old_object3 ...]\n",
			 (char *)NULL);
	return TCL_ERROR;
    }

    dbip = wdbp->dbip;
    RT_CHECK_DBI(dbip);

    db_init_db_tree_state(&init_state, dbip, wdbp->wdb_resp);

    /* Establish tolerances */
    init_state.ts_ttol = &wdbp->wdb_ttol;
    init_state.ts_tol = &wdbp->wdb_tol;

    /* Initial values for options, must be reset each time */
    triangulate = 0;
    make_bot = 1;

    /* Parse options. */
    bu_optind = 1;		/* re-init bu_getopt() */
    while ((c=bu_getopt(argc, (char * const *)argv, "ntT")) != -1) {
	switch (c) {
	    case 'n':
		make_bot = 0;
		break;
	    case 'T':
		triangulate = 1;
		break;
	    case 't':
		nmg_use_tnurbs = 1;
		break;
	    default:
		{
		    struct bu_vls tmp_vls;

		    bu_vls_init(&tmp_vls);
		    bu_vls_printf(&tmp_vls, "option '%c' unknown\n", c);
		    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&tmp_vls),
				     "Usage: facetize [-ntT] object(s)\n",
				     "\t-n make NMG primitives rather than BOT's\n",
				     "\t-t Perform CSG-to-tNURBS conversion\n",
				     "\t-T enable triangulator\n", (char *)NULL);
		    bu_vls_free(&tmp_vls);
		}
		break;
	}
    }
    argc -= bu_optind;
    argv += bu_optind;
    if (argc < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "facetize: missing argument\n", (char *)NULL);
	return TCL_ERROR;
    }

    newname = argv[0];
    argv++;
    argc--;
    if (argc < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "facetize: missing argument\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (db_lookup(dbip, newname, LOOKUP_QUIET) != RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, "error: solid '", newname,
			 "' already exists, aborting\n", (char *)NULL);
	return TCL_ERROR;
    }

    {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls,
		      "facetize:  tessellating primitives with tolerances a=%g, r=%g, n=%g\n",
		      wdbp->wdb_ttol.abs, wdbp->wdb_ttol.rel, wdbp->wdb_ttol.norm);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
    }
    facetize_tree = (union tree *)0;
    nmg_model = nmg_mm();
    init_state.ts_m = &nmg_model;

    i = db_walk_tree(dbip, argc, (const char **)argv,
		     1,
		     &init_state,
		     0,			/* take all regions */
		     facetize_region_end,
		     nmg_use_tnurbs ?
		     nmg_booltree_leaf_tnurb :
		     nmg_booltree_leaf_tess,
		     (void *)&facetize_tree
	);


    if (i < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "facetize: error in db_walk_tree()\n", (char *)NULL);
	/* Destroy NMG */
	nmg_km(nmg_model);
	return TCL_ERROR;
    }

    if (facetize_tree) {
	/* Now, evaluate the boolean tree into ONE region */
	Tcl_AppendResult(wdbp->wdb_interp, "facetize:  evaluating boolean expressions\n", (char *)NULL);

	if (BU_SETJUMP) {
	    BU_UNSETJUMP;
	    Tcl_AppendResult(wdbp->wdb_interp, "WARNING: facetization failed!!!\n", (char *)NULL);
	    db_free_tree(facetize_tree, &rt_uniresource);
	    facetize_tree = (union tree *)NULL;
	    nmg_km(nmg_model);
	    nmg_model = (struct model *)NULL;
	    return TCL_ERROR;
	}

	failed = nmg_boolean(facetize_tree, nmg_model, &wdbp->wdb_tol, &rt_uniresource);
	BU_UNSETJUMP;
    } else
	failed = 1;

    if (failed) {
	Tcl_AppendResult(wdbp->wdb_interp, "facetize:  no resulting region, aborting\n", (char *)NULL);
	db_free_tree(facetize_tree, &rt_uniresource);
	facetize_tree = (union tree *)NULL;
	nmg_km(nmg_model);
	nmg_model = (struct model *)NULL;
	return TCL_ERROR;
    }
    /* New region remains part of this nmg "model" */
    NMG_CK_REGION(facetize_tree->tr_d.td_r);
    Tcl_AppendResult(wdbp->wdb_interp, "facetize:  ", facetize_tree->tr_d.td_name,
		     "\n", (char *)NULL);

    /* Triangulate model, if requested */
    if (triangulate && !make_bot) {
	Tcl_AppendResult(wdbp->wdb_interp, "facetize:  triangulating resulting object\n", (char *)NULL);
	if (BU_SETJUMP) {
	    BU_UNSETJUMP;
	    Tcl_AppendResult(wdbp->wdb_interp, "WARNING: triangulation failed!!!\n", (char *)NULL);
	    db_free_tree(facetize_tree, &rt_uniresource);
	    facetize_tree = (union tree *)NULL;
	    nmg_km(nmg_model);
	    nmg_model = (struct model *)NULL;
	    return TCL_ERROR;
	}
	nmg_triangulate_model(nmg_model, &wdbp->wdb_tol);
	BU_UNSETJUMP;
    }

    if (make_bot) {
	struct rt_bot_internal *bot;
	struct nmgregion *r;
	struct shell *s;

	Tcl_AppendResult(wdbp->wdb_interp, "facetize:  converting to BOT format\n", (char *)NULL);

	r = BU_LIST_FIRST(nmgregion, &nmg_model->r_hd);
	s = BU_LIST_FIRST(shell, &r->s_hd);
	bot = (struct rt_bot_internal *)nmg_bot(s, &wdbp->wdb_tol);
	nmg_km(nmg_model);
	nmg_model = (struct model *)NULL;

	/* Export BOT as a new solid */
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BOT;
	intern.idb_meth = &OBJ[ID_BOT];
	intern.idb_ptr = (void *) bot;
    } else {

	Tcl_AppendResult(wdbp->wdb_interp, "facetize:  converting NMG to database format\n", (char *)NULL);

	/* Export NMG as a new solid */
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_NMG;
	intern.idb_meth = &OBJ[ID_NMG];
	intern.idb_ptr = (void *)nmg_model;
	nmg_model = (struct model *)NULL;
    }

    dp = db_diradd(dbip, newname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, "Cannot add ", newname, " to directory\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Failed to write ", newname, " to database\n", (char *)NULL);
	rt_db_free_internal(&intern);
	return TCL_ERROR;
    }

    facetize_tree->tr_d.td_r = (struct nmgregion *)NULL;

    /* Free boolean tree, and the regions in it */
    db_free_tree(facetize_tree, &rt_uniresource);
    facetize_tree = (union tree *)NULL;

    return TCL_OK;
}


int
wdb_find_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    int i, k;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb=(struct rt_comb_internal *)NULL;
    struct bu_vls vls;
    int c;
    int aflag = 0;		/* look at all objects */

    if (argc < 2) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_find %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_optind = 1;	/* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "a")) != -1) {
	switch (c) {
	    case 'a':
		aflag = 1;
		break;
	    default:
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "Unrecognized option - %c", c);
		Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);
		return TCL_ERROR;
	}
    }
    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    /* Examine all COMB nodes */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (!(dp->d_flags & RT_DIR_COMB) ||
		(!aflag && (dp->d_flags & RT_DIR_HIDDEN)))
		continue;

	    if (rt_db_get_internal(&intern,
				   dp,
				   wdbp->dbip,
				   (fastf_t *)NULL,
				   &rt_uniresource) < 0) {
		Tcl_AppendResult(wdbp->wdb_interp, "Database read error, aborting", (char *)NULL);
		return TCL_ERROR;
	    }

	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    for (k = 1; k < argc; k++)
		db_tree_funcleaf(wdbp->dbip,
				 comb,
				 comb->tree,
				 wdb_find_ref,
				 (void *)argv[k],
				 (void *)dp->d_namep,
				 (void *)wdbp->wdb_interp,
				 (void *)NULL);

	    rt_db_free_internal(&intern);
	}
    }

    return TCL_OK;
}


static int
wdb_facetize_tcl(void *clientData, int argc, const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_facetize_cmd(wdbp, argc-1, argv+1);
}


/**
 * Usage:
 * procname find object(s)
 */
static int
wdb_find_tcl(void *clientData, int argc, const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_find_cmd(wdbp, argc-1, argv+1);
}


struct wdb_id_names {
    struct bu_list l;
    struct bu_vls name;		/**< name associated with region id */
};


struct wdb_id_to_names {
    struct bu_list l;
    int id;				/**< starting id (i.e. region id or air code) */
    struct wdb_id_names headName;	/**< head of list of names */
};


int
wdb_rmap_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    int i;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct wdb_id_to_names headIdName;
    struct wdb_id_to_names *itnp;
    struct wdb_id_names *inp;
    struct bu_vls vls;

    if (argc != 1) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_rmap %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (db_version(wdbp->dbip) < 5) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%s is not available prior to version 5 of the .g file format\n", argv[0]);
	Tcl_SetResult(wdbp->wdb_interp, bu_vls_addr(&vls), TCL_VOLATILE);
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    BU_LIST_INIT(&headIdName.l);

    /* For all regions not hidden */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    int found = 0;

	    if (!(dp->d_flags & RT_DIR_REGION) ||
		(dp->d_flags & RT_DIR_HIDDEN))
		continue;

	    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		bu_vls_init(&vls);
		bu_vls_strcat(&vls, "Database read error, aborting");
		Tcl_SetResult(wdbp->wdb_interp, bu_vls_addr(&vls), TCL_VOLATILE);
		bu_vls_free(&vls);
		return TCL_ERROR;
	    }

	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    /* check to see if the region id or air code matches one in our list */
	    for (BU_LIST_FOR (itnp, wdb_id_to_names, &headIdName.l)) {
		if ((comb->region_id == itnp->id) ||
		    (comb->aircode != 0 && -comb->aircode == itnp->id)) {
		    /* add region name to our name list for this region */
		    BU_ALLOC(inp, struct wdb_id_names);
		    bu_vls_init(&inp->name);
		    bu_vls_strcpy(&inp->name, dp->d_namep);
		    BU_LIST_INSERT(&itnp->headName.l, &inp->l);
		    found = 1;
		    break;
		}
	    }

	    if (!found) {
		/* create new id_to_names node */
		BU_ALLOC(itnp, struct wdb_id_to_names);
		if (0 < comb->region_id)
		    itnp->id = comb->region_id;
		else
		    itnp->id = -comb->aircode;
		BU_LIST_INSERT(&headIdName.l, &itnp->l);
		BU_LIST_INIT(&itnp->headName.l);

		/* add region name to our name list for this region */
		BU_ALLOC(inp, struct wdb_id_names);
		bu_vls_init(&inp->name);
		bu_vls_strcpy(&inp->name, dp->d_namep);
		BU_LIST_INSERT(&itnp->headName.l, &inp->l);
	    }

	    rt_db_free_internal(&intern);
	}
    }

    bu_vls_init(&vls);

    /* place data in a dynamic tcl string */
    while (BU_LIST_WHILE (itnp, wdb_id_to_names, &headIdName.l)) {
	/* add this id to the list */
	bu_vls_printf(&vls, "%d {", itnp->id);

	/* start sublist of names associated with this id */
	while (BU_LIST_WHILE (inp, wdb_id_names, &itnp->headName.l)) {
	    /* add the this name to this sublist */
	    if (strchr(bu_vls_addr(&inp->name), ' ')) {
		bu_vls_printf(&vls, "\"%s\" ", bu_vls_addr(&inp->name));
	    } else {
		bu_vls_printf(&vls, "%s ", bu_vls_addr(&inp->name));
	    }

	    BU_LIST_DEQUEUE(&inp->l);
	    bu_vls_free(&inp->name);
	    bu_free((void *)inp, "rmap: inp");
	}
	bu_vls_strcat(&vls, "} ");

	BU_LIST_DEQUEUE(&itnp->l);
	bu_free((void *)itnp, "rmap: itnp");

    }
    bu_vls_trimspace(&vls);

    Tcl_SetResult(wdbp->wdb_interp, bu_vls_addr(&vls), TCL_VOLATILE);
    bu_vls_free(&vls);

    return TCL_OK;
}


/**
 * Usage:
 * procname rmap
 */
static int
wdb_rmap_tcl(void *clientData, int argc, const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_rmap_cmd(wdbp, argc-1, argv+1);
}


int
wdb_which_cmd(struct rt_wdb *wdbp,
	      int argc,
	      const char *argv[])
{
    int i, j;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct wdb_id_to_names headIdName;
    struct wdb_id_to_names *itnp;
    struct wdb_id_names *inp;
    int isAir;
    int sflag;


    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_%s %s", argv[0], argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "whichair"))
	isAir = 1;
    else
	isAir = 0;

    if (BU_STR_EQUAL(argv[1], "-s")) {
	--argc;
	++argv;

	if (argc < 2) {
	    struct bu_vls vls;

	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "helplib_alias wdb_%s %s", argv[-1], argv[-1]);
	    Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	sflag = 1;
    } else {
	sflag = 0;
    }

    BU_LIST_INIT(&headIdName.l);

    /* Build list of id_to_names */
    for (j = 1; j < argc; j++) {
	int n;
	int start, end;
	int range;
	int k;

	n = sscanf(argv[j], "%d%*[:-]%d", &start, &end);
	switch (n) {
	    case 1:
		for (BU_LIST_FOR (itnp, wdb_id_to_names, &headIdName.l))
		    if (itnp->id == start)
			break;

		/* id not found */
		if (BU_LIST_IS_HEAD(itnp, &headIdName.l)) {
		    BU_ALLOC(itnp, struct wdb_id_to_names);
		    itnp->id = start;
		    BU_LIST_INSERT(&headIdName.l, &itnp->l);
		    BU_LIST_INIT(&itnp->headName.l);
		}

		break;
	    case 2:
		if (start < end)
		    range = end - start + 1;
		else if (end < start) {
		    range = start - end + 1;
		    start = end;
		} else
		    range = 1;

		for (k = 0; k < range; ++k) {
		    int id = start + k;

		    for (BU_LIST_FOR (itnp, wdb_id_to_names, &headIdName.l))
			if (itnp->id == id)
			    break;

		    /* id not found */
		    if (BU_LIST_IS_HEAD(itnp, &headIdName.l)) {
			BU_ALLOC(itnp, struct wdb_id_to_names);
			itnp->id = id;
			BU_LIST_INSERT(&headIdName.l, &itnp->l);
			BU_LIST_INIT(&itnp->headName.l);
		    }
		}

		break;
	}
    }

    /* Examine all COMB nodes */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (!(dp->d_flags & RT_DIR_REGION))
		continue;

	    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		Tcl_AppendResult(wdbp->wdb_interp, "Database read error, aborting", (char *)NULL);
		return TCL_ERROR;
	    }
	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    /* check to see if the region id or air code matches one in our list */
	    for (BU_LIST_FOR (itnp, wdb_id_to_names, &headIdName.l)) {
		if ((!isAir && comb->region_id == itnp->id) ||
		    (isAir && comb->aircode == itnp->id)) {
		    /* add region name to our name list for this region */
		    BU_ALLOC(inp, struct wdb_id_names);
		    bu_vls_init(&inp->name);
		    bu_vls_strcpy(&inp->name, dp->d_namep);
		    BU_LIST_INSERT(&itnp->headName.l, &inp->l);
		    break;
		}
	    }

	    rt_db_free_internal(&intern);
	}
    }

    /* place data in interp and free memory */
    while (BU_LIST_WHILE (itnp, wdb_id_to_names, &headIdName.l)) {
	if (!sflag) {
	    struct bu_vls vls;

	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "Region[s] with %s %d:\n",
			  isAir ? "air code" : "ident", itnp->id);
	    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);
	}

	while (BU_LIST_WHILE (inp, wdb_id_names, &itnp->headName.l)) {
	    if (sflag)
		Tcl_AppendElement(wdbp->wdb_interp, bu_vls_addr(&inp->name));
	    else
		Tcl_AppendResult(wdbp->wdb_interp, "   ", bu_vls_addr(&inp->name),
				 "\n", (char *)NULL);

	    BU_LIST_DEQUEUE(&inp->l);
	    bu_vls_free(&inp->name);
	    bu_free((void *)inp, "which: inp");
	}

	BU_LIST_DEQUEUE(&itnp->l);
	bu_free((void *)itnp, "which: itnp");
    }

    return TCL_OK;
}


/**
 * Usage:
 * procname whichair/whichid [-s] id(s)
 */
static int
wdb_which_tcl(void *clientData, int argc, const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_which_cmd(wdbp, argc-1, argv+1);
}


int
wdb_title_cmd(struct rt_wdb *wdbp,
	      int argc,
	      const char *argv[])
{
    struct bu_vls title;
    int bad = 0;

    RT_CK_WDB(wdbp);
    RT_CK_DBI(wdbp->dbip);

    if (argc < 1) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_title %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* get title */
    if (argc == 1) {
	Tcl_AppendResult(wdbp->wdb_interp, wdbp->dbip->dbi_title, (char *)NULL);
	return TCL_OK;
    }

    WDB_TCL_CHECK_READ_ONLY;

    /* set title */
    bu_vls_init(&title);
    bu_vls_from_argv(&title, argc-1, (const char **)argv+1);

    if (db_update_ident(wdbp->dbip, bu_vls_addr(&title), wdbp->dbip->dbi_local2base) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Error: unable to change database title");
	bad = 1;
    }

    bu_vls_free(&title);
    return bad ? TCL_ERROR : TCL_OK;
}


/**
 * @brief
 * Change or return the database title.
 *
 * Usage:
 * procname title [description]
 */
static int
wdb_title_tcl(void *clientData,
	      int argc,
	      const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_title_cmd(wdbp, argc-1, argv+1);
}


static int
wdb_list_children(struct rt_wdb *wdbp,
		  struct directory *dp)
{
    size_t i;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    if (!(dp->d_flags & RT_DIR_COMB))
	return TCL_OK;

    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Database read error, aborting", (char *)NULL);
	return TCL_ERROR;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb->tree) {
	struct bu_vls vls;
	size_t node_count;
	size_t actual_count;
	struct rt_tree_array *rt_tree_array;

	if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	    db_non_union_push(comb->tree, &rt_uniresource);
	    if (db_ck_v4gift_tree(comb->tree) < 0) {
		Tcl_AppendResult(wdbp->wdb_interp, "Cannot flatten tree for listing", (char *)NULL);
		return TCL_ERROR;
	    }
	}
	node_count = db_tree_nleaves(comb->tree);
	if (node_count > 0) {
	    rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count,
							      sizeof(struct rt_tree_array), "tree list");
	    actual_count = (struct rt_tree_array *)db_flatten_tree(
		rt_tree_array, comb->tree, OP_UNION,
		1, &rt_uniresource) - rt_tree_array;
	    BU_ASSERT_SIZE_T(actual_count, ==, node_count);
	    comb->tree = TREE_NULL;
	} else {
	    actual_count = 0;
	    rt_tree_array = NULL;
	}

	bu_vls_init(&vls);
	for (i = 0; i < actual_count; i++) {
	    char op;

	    switch (rt_tree_array[i].tl_op) {
		case OP_UNION:
		    op = 'u';
		    break;
		case OP_INTERSECT:
		    op = '+';
		    break;
		case OP_SUBTRACT:
		    op = '-';
		    break;
		default:
		    op = '?';
		    break;
	    }

	    bu_vls_printf(&vls, "{%c %s} ", op, rt_tree_array[i].tl_tree->tr_l.tl_name);
	    db_free_tree(rt_tree_array[i].tl_tree, &rt_uniresource);
	}
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)0);
	bu_vls_free(&vls);

	if (rt_tree_array)
	    bu_free((char *)rt_tree_array, "printnode: rt_tree_array");
    }
    rt_db_free_internal(&intern);

    return TCL_OK;
}


int
wdb_lt_cmd(struct rt_wdb *wdbp,
	   int argc,
	   const char *argv[])
{
    struct directory *dp;
    struct bu_vls vls;

    if (argc != 2)
	goto bad;

    if ((dp = db_lookup(wdbp->dbip, argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	goto bad;

    return wdb_list_children(wdbp, dp);

bad:
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias wdb_lt %s", argv[0]);
    Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
}


/*
 * Usage:
 * procname lt object
 */
static int
wdb_lt_tcl(void *clientData,
	   int argc,
	   const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_lt_cmd(wdbp, argc-1, argv+1);
}


int
wdb_version_cmd(struct rt_wdb *wdbp,
		int argc,
		const char *argv[])
{
    struct bu_vls vls;

    bu_vls_init(&vls);

    if (argc != 1) {
	bu_vls_printf(&vls, "helplib_alias wdb_version %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_printf(&vls, "%d", db_version(wdbp->dbip));
    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)0);
    bu_vls_free(&vls);

    return TCL_OK;
}


/**
 * Usage:
 * procname version
 */
static int
wdb_version_tcl(void *clientData,
		int argc,
		const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_version_cmd(wdbp, argc-1, argv+1);
}


/**
 *@brief
 * NON-PARALLEL due to rt_uniresource
 */
void
wdb_print_node(struct rt_wdb *wdbp,
	       struct directory *dp,
	       int pathpos,
	       int indentSize,
	       char prefix,
	       int cflag,
	       int displayDepth,
	       int currdisplayDepth)
{
    size_t i;
    struct directory *nextdp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;


    if (cflag && !(dp->d_flags & RT_DIR_COMB))
	return;

    for (i = 0; i < (size_t)pathpos; i++)
	if (indentSize < 0) {
	    Tcl_AppendResult(wdbp->wdb_interp, "\t", (char *)NULL);
	} else {
	    int j;
	    for (j = 0; j < indentSize; j++) {
		Tcl_AppendResult(wdbp->wdb_interp, " ", (char *)NULL);
	    }
	}

    if (prefix) {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "%c ", prefix);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
    }

    Tcl_AppendResult(wdbp->wdb_interp, dp->d_namep, (char *)NULL);
    /* Output Comb and Region flags (-F?) */
    if (dp->d_flags & RT_DIR_COMB)
	Tcl_AppendResult(wdbp->wdb_interp, "/", (char *)NULL);
    if (dp->d_flags & RT_DIR_REGION)
	Tcl_AppendResult(wdbp->wdb_interp, "R", (char *)NULL);

    Tcl_AppendResult(wdbp->wdb_interp, "\n", (char *)NULL);

    if (!(dp->d_flags & RT_DIR_COMB))
	return;

    /*
     * This node is a combination (e.g., a directory).
     * Process all the arcs (e.g., directory members).
     */

    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Database read error, aborting", (char *)NULL);
	return;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb->tree) {
	size_t node_count;
	size_t actual_count;
	struct rt_tree_array *rt_tree_array;

	if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	    db_non_union_push(comb->tree, &rt_uniresource);
	    if (db_ck_v4gift_tree(comb->tree) < 0) {
		Tcl_AppendResult(wdbp->wdb_interp, "Cannot flatten tree for listing", (char *)NULL);
		return;
	    }
	}
	node_count = db_tree_nleaves(comb->tree);
	if (node_count > 0) {
	    rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count,
							      sizeof(struct rt_tree_array), "tree list");
	    actual_count = (struct rt_tree_array *)db_flatten_tree(
		rt_tree_array, comb->tree, OP_UNION,
		1, &rt_uniresource) - rt_tree_array;
	    BU_ASSERT_SIZE_T(actual_count, ==, node_count);
	    comb->tree = TREE_NULL;
	} else {
	    actual_count = 0;
	    rt_tree_array = NULL;
	}

	for (i = 0; i < actual_count; i++) {
	    char op;

	    switch (rt_tree_array[i].tl_op) {
		case OP_UNION:
		    op = 'u';
		    break;
		case OP_INTERSECT:
		    op = '+';
		    break;
		case OP_SUBTRACT:
		    op = '-';
		    break;
		default:
		    op = '?';
		    break;
	    }

	    if ((nextdp = db_lookup(wdbp->dbip, rt_tree_array[i].tl_tree->tr_l.tl_name, LOOKUP_NOISY)) == RT_DIR_NULL) {
		int j;
		struct bu_vls tmp_vls;

		for (j = 0; j < pathpos + 1; j++)
		    Tcl_AppendResult(wdbp->wdb_interp, "\t", (char *)NULL);

		bu_vls_init(&tmp_vls);
		bu_vls_printf(&tmp_vls, "%c ", op);
		Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);

		Tcl_AppendResult(wdbp->wdb_interp, rt_tree_array[i].tl_tree->tr_l.tl_name, "\n", (char *)NULL);
	    } else {
		if (currdisplayDepth < displayDepth) {
		    wdb_print_node(wdbp, nextdp, pathpos+1, indentSize, op, cflag, displayDepth, currdisplayDepth+1);
		}
	    }
	    db_free_tree(rt_tree_array[i].tl_tree, &rt_uniresource);
	}
	if (rt_tree_array) bu_free((char *)rt_tree_array, "printnode: rt_tree_array");
    }
    rt_db_free_internal(&intern);
}


/**
 * Usage:
 * procname track args
 */
static int
wdb_track_tcl(void *clientData,
	      int argc,
	      const char *argv[]) {
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_track_cmd(wdbp, argc-1, argv+1);
}


static void
wdb_pr_mater(const struct mater *mp,
	     int *ccp,
	     int *clp)
{
    char buf[128];
    struct bu_vls vls;

    bu_vls_init(&vls);

    (void)sprintf(buf, "%5d..%d", mp->mt_low, mp->mt_high);
    wdb_vls_col_item(&vls, buf, ccp, clp);
    (void)sprintf(buf, "%3d, %3d, %3d", mp->mt_r, mp->mt_g, mp->mt_b);
    wdb_vls_col_item(&vls, buf, ccp, clp);
    wdb_vls_col_eol(&vls, ccp, clp);

    bu_log("%s", bu_vls_addr(&vls));
    bu_vls_free(&vls);
}


int
wdb_prcolor_cmd(struct rt_wdb *wdbp,
		int argc,
		const char *argv[])
{
    const struct mater *mp;
    int col_count = 0;
    int col_len = 0;

    if (argc != 1) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_prcolor %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (rt_material_head() == MATER_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, "none", (char *)NULL);
	return TCL_OK;
    }

    for (mp = rt_material_head(); mp != MATER_NULL; mp = mp->mt_forw)
	wdb_pr_mater(mp, &col_count, &col_len);

    return TCL_OK;
}


/*
 * Usage:
 * procname prcolor
 */
static int
wdb_prcolor_tcl(void *clientData, int argc, const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_prcolor_cmd(wdbp, argc-1, argv+1);
}


int
wdb_tol_cmd(struct rt_wdb *wdbp,
	    int argc,
	    const char *argv[])
{
    struct bu_vls vls;
    double f;

    if (argc < 1 || ((argc-1)%2 != 0)) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_tol %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* print all tolerance settings */
    if (argc == 1) {
	Tcl_AppendResult(wdbp->wdb_interp, "Current tolerance settings are:\n", (char *)NULL);
	Tcl_AppendResult(wdbp->wdb_interp, "Tessellation tolerances:\n", (char *)NULL);

	if (wdbp->wdb_ttol.abs > 0.0) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "\tabs %g mm\n", wdbp->wdb_ttol.abs);
	    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);
	} else {
	    Tcl_AppendResult(wdbp->wdb_interp, "\tabs None\n", (char *)NULL);
	}

	if (wdbp->wdb_ttol.rel > 0.0) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "\trel %g (%g%%)\n",
			  wdbp->wdb_ttol.rel, wdbp->wdb_ttol.rel * 100.0);
	    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);
	} else {
	    Tcl_AppendResult(wdbp->wdb_interp, "\trel None\n", (char *)NULL);
	}

	if (wdbp->wdb_ttol.norm > 0.0) {
	    int deg, min;
	    double sec;

	    bu_vls_init(&vls);
	    sec = wdbp->wdb_ttol.norm * RAD2DEG;
	    deg = (int)(sec);
	    sec = (sec - (double)deg) * 60;
	    min = (int)(sec);
	    sec = (sec - (double)min) * 60;

	    bu_vls_printf(&vls, "\tnorm %g degrees (%d deg %d min %g sec)\n",
			  wdbp->wdb_ttol.norm * RAD2DEG, deg, min, sec);
	    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);
	} else {
	    Tcl_AppendResult(wdbp->wdb_interp, "\tnorm None\n", (char *)NULL);
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "Calculational tolerances:\n");
	bu_vls_printf(&vls,
		      "\tdistance = %g mm\n\tperpendicularity = %g (cosine of %g degrees)",
		      wdbp->wdb_tol.dist, wdbp->wdb_tol.perp,
		      acos(wdbp->wdb_tol.perp)*RAD2DEG);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    /* get the specified tolerance */
    if (argc == 2) {
	int status = TCL_OK;

	bu_vls_init(&vls);

	switch (argv[1][0]) {
	    case 'a':
		if (wdbp->wdb_ttol.abs > 0.0)
		    bu_vls_printf(&vls, "%g", wdbp->wdb_ttol.abs);
		else
		    bu_vls_printf(&vls, "None");
		break;
	    case 'r':
		if (wdbp->wdb_ttol.rel > 0.0)
		    bu_vls_printf(&vls, "%g", wdbp->wdb_ttol.rel);
		else
		    bu_vls_printf(&vls, "None");
		break;
	    case 'n':
		if (wdbp->wdb_ttol.norm > 0.0)
		    bu_vls_printf(&vls, "%g", wdbp->wdb_ttol.norm);
		else
		    bu_vls_printf(&vls, "None");
		break;
	    case 'd':
		bu_vls_printf(&vls, "%g", wdbp->wdb_tol.dist);
		break;
	    case 'p':
		bu_vls_printf(&vls, "%g", wdbp->wdb_tol.perp);
		break;
	    default:
		bu_vls_printf(&vls, "unrecognized tolerance type - %s", argv[1]);
		status = TCL_ERROR;
		break;
	}

	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return status;
    }

    /* skip the command name */
    argc--;
    argv++;

    /* iterate over the pairs of tolerance values */
    while (argc > 0) {

	/* set the specified tolerance(s) */
	if (sscanf(argv[1], "%lf", &f) != 1) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "bad tolerance - %s", argv[1]);
	    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    return TCL_ERROR;
	}

	/* clamp negative to zero */
	if (f < 0.0) {
	    Tcl_AppendResult(wdbp->wdb_interp, "negative tolerance clamped to 0.0\n", (char *)NULL);
	    f = 0.0;
	}

	switch (argv[0][0]) {
	    case 'a':
		/* Absolute tol */
		if (f < wdbp->wdb_tol.dist) {
		    bu_vls_init(&vls);
		    bu_vls_printf(&vls, "absolute tolerance cannot be less than distance tolerance, clamped to %f\n", wdbp->wdb_tol.dist);
		    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
		    bu_vls_free(&vls);
		}
		wdbp->wdb_ttol.abs = f;
		break;
	    case 'r':
		if (f >= 1.0) {
		    Tcl_AppendResult(wdbp->wdb_interp,
				     "relative tolerance must be between 0 and 1, not changed\n",
				     (char *)NULL);
		    return TCL_ERROR;
		}
		/* Note that a value of 0.0 will disable relative tolerance */
		wdbp->wdb_ttol.rel = f;
		break;
	    case 'n':
		/* Normal tolerance, in degrees */
		if (f > 90.0) {
		    Tcl_AppendResult(wdbp->wdb_interp,
				     "Normal tolerance must be less than 90.0 degrees\n",
				     (char *)NULL);
		    return TCL_ERROR;
		}
		/* Note that a value of 0.0 or 360.0 will disable this tol */
		wdbp->wdb_ttol.norm = f * DEG2RAD;
		break;
	    case 'd':
		/* Calculational distance tolerance */
		wdbp->wdb_tol.dist = f;
		wdbp->wdb_tol.dist_sq = wdbp->wdb_tol.dist * wdbp->wdb_tol.dist;
		break;
	    case 'p':
		/* Calculational perpendicularity tolerance */
		if (f > 1.0) {
		    Tcl_AppendResult(wdbp->wdb_interp,
				     "Calculational perpendicular tolerance must be from 0 to 1\n",
				     (char *)NULL);
		    return TCL_ERROR;
		}
		wdbp->wdb_tol.perp = f;
		wdbp->wdb_tol.para = 1.0 - f;
		break;
	    default:
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "unrecognized tolerance type - %s", argv[0]);
		Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	argc-=2;
	argv+=2;
    }

    return TCL_OK;
}


/**
 * Usage:
 * procname tol [abs|rel|norm|dist|perp [#]]
 *
 *@n abs # sets absolute tolerance.  # > 0.0
 *@n rel # sets relative tolerance.  0.0 < # < 1.0
 *@n norm # sets normal tolerance, in degrees.
 *@n dist # sets calculational distance tolerance
 *@n perp # sets calculational normal tolerance.
 *
 */
static int
wdb_tol_tcl(void *clientData,
	    int argc,
	    const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_tol_cmd(wdbp, argc-1, argv+1);
}


void
wdb_pull_comb(struct db_i *dbip,
	  struct directory *dp,
	  void *mp);

/* This restores the matrix transformation at a combination by taking leaf matrix transformations, inverting
 * and storing the changes at the combinations.
 */
static void
wdb_pull_comb_mat(struct db_i *dbip, struct rt_comb_internal *UNUSED(comb), union tree *comb_leaf, void *mp, void *UNUSED(usr_ptr2),
	      void *UNUSED(usr_ptr3), void *UNUSED(usr_ptr4))
{
    struct directory *dp;
    mat_t inv_mat;
    matp_t mat = (matp_t)mp;

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    if ((dp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_NOISY)) == RT_DIR_NULL)
	return;

    if (!comb_leaf->tr_l.tl_mat) {
	comb_leaf->tr_l.tl_mat = (matp_t)bu_malloc(sizeof(mat_t), "tl_mat");
	MAT_COPY(comb_leaf->tr_l.tl_mat, mat);

	return;
    }

    /* invert the matrix transformation of the leaf and store new matrix  at comb */
    bn_mat_inverse(inv_mat, comb_leaf->tr_l.tl_mat );

    /* multiply inverse and store at combination */
    bn_mat_mul2(inv_mat,mat);
    MAT_COPY(comb_leaf->tr_l.tl_mat, mat);
    wdb_pull_comb(dbip, dp, mp);
}


/* pull_comb(): This routine enters a comb restoring the matrix transformation at the combination
 *              calls and calls pull_comb_mat() which updates current matrix transformation and moves up tree.
 * Note:        the generic pointer points to a matrix if successful or a string if unsuccessful.
 */
void
wdb_pull_comb(struct db_i *dbip,
	  struct directory *dp,
	  void *mp)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    matp_t mat = (matp_t)mp;
    mat_t m;
    mat_t invMat;

    if (dp->d_flags & RT_DIR_SOLID)
	return;
    if (rt_db_get_internal(&intern, dp, dbip, m, &rt_uniresource) < 0) {
	Tcl_AppendResult(dbip->dbi_wdbp->wdb_interp,"Database read error, aborting\n");
	return;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;

    /* checks if matrix pointer is valid */
    if (mat == NULL) {
	mat = (matp_t)bu_malloc(sizeof(mat_t), "cur_mat");
	MAT_IDN(mat);
    }

    bn_mat_inverse(invMat, mat);
    bn_mat_mul2(mat,m);
    MAT_COPY(mat, m);/* updates current matrix pointer */

    if (comb->tree) {
	db_tree_funcleaf(dbip, comb, comb->tree, wdb_pull_comb_mat,
			 &m, (void *)NULL, (void *)NULL, (void *)NULL);

	if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	    Tcl_AppendResult(dbip->dbi_wdbp->wdb_interp,"Cannot write modified combination (%s) to database\n", dp->d_namep);
	    return;
	}
    }
}


/**
 * @brief
 * This routine takes the internal database representation of a leaf node or
 * primitive object and builds  a matrix transformation, closest to this node's,
 * sets these values to default and returns matrix.
 */
static void
wdb_pull_leaf(struct db_i *UNUSED(dbip),
	  struct directory *UNUSED(dp),
	  void *UNUSED(mp))
{
    return;
}


int
wdb_pull_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    struct directory *dp;
    struct resource *resp;
    mat_t mat;
    int c;
    int debug;
    static const char *usage = "object";

    resp = &rt_uniresource;
    rt_init_resource( &rt_uniresource, 0, NULL );

    WDB_TCL_CHECK_READ_ONLY;

    /* must be wanting help */
    if (argc == 1) {
	Tcl_AppendResult(wdbp->wdb_interp,"helplib_alias wdb_pull Usage: %s %s", argv[0], usage);
	return TCL_ERROR;
    }

    if (argc != 2) {
	Tcl_AppendResult(wdbp->wdb_interp, "Usage: %s %s", argv[0], usage);
	return TCL_ERROR;
    }

    debug = RT_G_DEBUG;

    /* get directory pointer for arg */
    if ((dp = db_lookup(wdbp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return GED_ERROR;

    /* Checks whether the object is a primitive.*/
    if (dp->d_flags & RT_DIR_SOLID) {
	Tcl_AppendResult(wdbp->wdb_interp,"Attempt to pull primitive, aborting.\n");
	return TCL_ERROR;
    }

    /* Parse options */
    bu_optind = 1;	/* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "d")) != -1) {
	switch (c) {
	   case 'd':
		RTG.debug |= DEBUG_TREEWALK;
		break;
	  case '?':
	  default:
		Tcl_AppendResult(wdbp->wdb_interp, "pull: usage pull [-d] root \n");
		break;
	}
    }

    /*
     * uses a no frills walk routine recursively moving up the tree
     * from the leaves performing the necessary matrix transformations moving up
     * right to the the head of the tree pulling objects.
     * All new changes are immediately written to database
     */
    db_functree(wdbp->dbip, dp, wdb_pull_comb, wdb_pull_leaf, resp, &mat);

    RTG.debug = debug;


   return TCL_OK;
}

/**
 * @brief
 * The pull command is used to move matrices from combinations
 * and leaves up to the head of the tree defined by the object.
 *
 * the -d flag turns on the treewalker debugging output.
 *
 * Usage:
 * procname pull object(s)
 */
int
wdb_pull_tcl(void *clientData,
             int argc,
             const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_pull_cmd(wdbp, argc-1, argv+1);
}


/** structure to hold all solids that have been pushed. */
struct wdb_push_id {
    uint32_t magic;
    struct wdb_push_id *forw, *back;
    struct directory *pi_dir;
    mat_t pi_mat;
};


#define WDB_MAGIC_PUSH_ID 0x50495323
#define FOR_ALL_WDB_PUSH_SOLIDS(_p, _phead) \
    for (_p=_phead.forw; _p!=&_phead; _p=_p->forw)

struct wdb_push_data {
    Tcl_Interp *interp;
    struct wdb_push_id pi_head;
    int push_error;
};


/**
 * This routine must be prepared to run in parallel.
 *
 * @brief
 * This routine is called once for eas leaf (solid) that is to
 * be pushed.  All it does is build at push_id linked list.  The
 * linked list could be handled by bu_list macros but it is simple
 * enough to do hear with out them.
 */
static union tree *
wdb_push_leaf(struct db_tree_state *tsp,
	      const struct db_full_path *pathp,
	      struct rt_db_internal *ip,
	      void *client_data)
{
    union tree *curtree;
    struct directory *dp;
    struct wdb_push_id *pip;
    struct wdb_push_data *wpdp = (struct wdb_push_data *)client_data;

    RT_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    RT_CK_RESOURCE(tsp->ts_resp);

    dp = pathp->fp_names[pathp->fp_len-1];

    if (RT_G_DEBUG&DEBUG_TREEWALK) {
	char *sofar = db_path_to_string(pathp);

	Tcl_AppendResult(wpdp->interp, "wdb_push_leaf(",
			 ip->idb_meth->ft_name,
			 ") path='", sofar, "'\n", (char *)NULL);
	bu_free((void *)sofar, "path string");
    }
/*
 * XXX - This will work but is not the best method.  dp->d_uses tells us
 * if this solid (leaf) has been seen before.  If it hasn't just add
 * it to the list.  If it has, search the list to see if the matrices
 * match and do the "right" thing.
 *
 * (There is a question as to whether dp->d_uses is reset to zero
 * for each tree walk.  If it is not, then d_uses is NOT a safe
 * way to check and this method will always work.)
 */
    bu_semaphore_acquire(RT_SEM_WORKER);
    FOR_ALL_WDB_PUSH_SOLIDS(pip, wpdp->pi_head) {
	if (pip->pi_dir == dp) {
	    if (!bn_mat_is_equal(pip->pi_mat,
				 tsp->ts_mat, tsp->ts_tol)) {
		char *sofar = db_path_to_string(pathp);

		Tcl_AppendResult(wpdp->interp, "wdb_push_leaf: matrix mismatch between '", sofar,
				 "' and prior reference.\n", (char *)NULL);
		bu_free((void *)sofar, "path string");
		wpdp->push_error = 1;
	    }

	    bu_semaphore_release(RT_SEM_WORKER);
	    RT_GET_TREE(curtree, tsp->ts_resp);
	    curtree->tr_op = OP_NOP;
	    return curtree;
	}
    }
/*
 * This is the first time we have seen this solid.
 */
    BU_ALLOC(pip, struct wdb_push_id);
    pip->magic = WDB_MAGIC_PUSH_ID;
    pip->pi_dir = dp;
    MAT_COPY(pip->pi_mat, tsp->ts_mat);
    pip->back = wpdp->pi_head.back;
    wpdp->pi_head.back = pip;
    pip->forw = &wpdp->pi_head;
    pip->back->forw = pip;
    bu_semaphore_release(RT_SEM_WORKER);
    RT_GET_TREE(curtree, tsp->ts_resp);
    curtree->tr_op = OP_NOP;
    return curtree;
}
/**
 * @brief
 * A null routine that does nothing.
 */
static union tree *
wdb_push_region_end(struct db_tree_state *UNUSED(tsp),
		    const struct db_full_path *UNUSED(pathp),
		    union tree *curtree,
		    void *UNUSED(client_data))
{
    return curtree;
}


int
wdb_push_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    struct wdb_push_data *wpdp;
    struct wdb_push_id *pip;
    struct rt_db_internal es_int;
    int i;
    int ncpu;
    int c;
    int old_debug;
    int push_error;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_push %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    RT_CHECK_DBI(wdbp->dbip);

    BU_ALLOC(wpdp, struct wdb_push_data);
    wpdp->interp = wdbp->wdb_interp;
    wpdp->push_error = 0;
    wpdp->pi_head.magic = WDB_MAGIC_PUSH_ID;
    wpdp->pi_head.forw = wpdp->pi_head.back = &wpdp->pi_head;
    wpdp->pi_head.pi_dir = (struct directory *) 0;

    old_debug = RT_G_DEBUG;

    /* Initial values for options, must be reset each time */
    ncpu = 1;

    /* Parse options */
    bu_optind = 1;	/* re-init bu_getopt() */
    while ((c=bu_getopt(argc, (char * const *)argv, "P:d")) != -1) {
	switch (c) {
	    case 'P':
		ncpu = atoi(bu_optarg);
		if (ncpu<1) ncpu = 1;
		break;
	    case 'd':
		RTG.debug |= DEBUG_TREEWALK;
		break;
	    case '?':
	    default:
		Tcl_AppendResult(wdbp->wdb_interp, "push: usage push [-P processors] [-d] root [root2 ...]\n", (char *)NULL);
		break;
	}
    }

    argc -= bu_optind;
    argv += bu_optind;

    /*
     * build a linked list of solids with the correct
     * matrix to apply to each solid.  This will also
     * check to make sure that a solid is not pushed in two
     * different directions at the same time.
     */
    i = db_walk_tree(wdbp->dbip, argc, (const char **)argv,
		     ncpu,
		     &wdbp->wdb_initial_tree_state,
		     0,				/* take all regions */
		     wdb_push_region_end,
		     wdb_push_leaf, (void *)wpdp);

    /*
     * If there was any error, then just free up the solid
     * list we just built.
     */
    if (i < 0 || wpdp->push_error) {
	while (wpdp->pi_head.forw != &wpdp->pi_head) {
	    pip = wpdp->pi_head.forw;
	    pip->forw->back = pip->back;
	    pip->back->forw = pip->forw;
	    bu_free((void *)pip, "Push ident");
	}
	RTG.debug = old_debug;
	bu_free((void *)wpdp, "wdb_push_tcl: wpdp");
	Tcl_AppendResult(wdbp->wdb_interp,
			 "push:\tdb_walk_tree failed or there was a solid moving\n\tin two or more directions",
			 (char *)NULL);
	return TCL_ERROR;
    }
/*
 * We've built the push solid list, now all we need to do is apply
 * the matrix we've stored for each solid.
 */
    FOR_ALL_WDB_PUSH_SOLIDS(pip, wpdp->pi_head) {
	if (rt_db_get_internal(&es_int, pip->pi_dir, wdbp->dbip, pip->pi_mat, &rt_uniresource) < 0) {
	    Tcl_AppendResult(wdbp->wdb_interp, "f_push: Read error fetching '",
			     pip->pi_dir->d_namep, "'\n", (char *)NULL);
	    wpdp->push_error = -1;
	    continue;
	}
	RT_CK_DB_INTERNAL(&es_int);

	if (rt_db_put_internal(pip->pi_dir, wdbp->dbip, &es_int, &rt_uniresource) < 0) {
	    Tcl_AppendResult(wdbp->wdb_interp, "push(", pip->pi_dir->d_namep,
			     "): solid export failure\n", (char *)NULL);
	}
	rt_db_free_internal(&es_int);
    }

    /*
     * Now use the wdb_identitize() tree walker to turn all the
     * matrices in a combination to the identity matrix.
     * It would be nice to use db_tree_walker() but the tree
     * walker does not give us all combinations, just regions.
     * This would work if we just processed all matrices backwards
     * from the leaf (solid) towards the root, but all in all it
     * seems that this is a better method.
     */

    while (argc > 0) {
	struct directory *db;
	db = db_lookup(wdbp->dbip, *argv++, 0);
	if (db)
	    wdb_identitize(db, wdbp->dbip);
	--argc;
    }

    /*
     * Free up the solid table we built.
     */
    while (wpdp->pi_head.forw != &wpdp->pi_head) {
	pip = wpdp->pi_head.forw;
	pip->forw->back = pip->back;
	pip->back->forw = pip->forw;
	bu_free((void *)pip, "Push ident");
    }

    RTG.debug = old_debug;
    push_error = wpdp->push_error;
    bu_free((void *)wpdp, "wdb_push_tcl: wpdp");

    return push_error ? TCL_ERROR : TCL_OK;
}


/**
 * @brief
 * The push command is used to move matrices from combinations
 * down to the solids. At some point, it is worth while thinking
 * about adding a limit to have the push go only N levels down.
 *
 * the -d flag turns on the treewalker debugging output.
 * the -P flag allows for multi-processor tree walking (not useful)
 *
 * Usage:
 * procname push object(s)
 */
static int
wdb_push_tcl(void *clientData,
	     int argc,
	     const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_push_cmd(wdbp, argc-1, argv+1);
}


static void
increment_uses(struct db_i *UNUSED(db_ip),
	       struct directory *dp,
	       void *UNUSED(ptr))
{
    RT_CK_DIR(dp);

    dp->d_uses++;
}


static void
increment_nrefs(struct db_i *UNUSED(db_ip),
		struct directory *dp,
		void *UNUSED(ptr))
{
    RT_CK_DIR(dp);

    dp->d_nref++;
}


struct object_use
{
    struct bu_list l;
    struct directory *dp;
    mat_t xform;
    int used;
};


static void
Free_uses(struct db_i *dbip)
{
    int i;

    for (i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;
	struct object_use *use;

	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (!(dp->d_flags & (RT_DIR_SOLID | RT_DIR_COMB)))
		continue;

	    while (BU_LIST_NON_EMPTY(&dp->d_use_hd)) {
		use = BU_LIST_FIRST(object_use, &dp->d_use_hd);
		if (!use->used) {
		    if (use->dp->d_un.file_offset >= 0) {
			/* was written to disk */
			if (db_delete(dbip, use->dp) != 0)
			    bu_log("Free_uses: db_delete failed!\n");
		    }
		    if (db_dirdelete(dbip, use->dp) < 0) {
		      bu_log("Free_uses: db_dirdelete failed!\n");
		    }
		}
		BU_LIST_DEQUEUE(&use->l);
		bu_free((void *)use, "Free_uses: use");
	    }

	}
    }

}


static void
Make_new_name(struct db_i *dbip,
	      struct directory *dp,
	      void *UNUSED(ptr))
{
    struct object_use *use;
    int use_no;
    int digits;
    int suffix_start;
    int name_length;
    int j;
    char format_v4[25], format_v5[25];
    struct bu_vls name_v5;
    char name_v4[NAMESIZE+1];
    char *name;

    /* only one use and not referenced elsewhere, nothing to do */
    if (dp->d_uses < 2 && dp->d_uses == dp->d_nref)
	return;

    /* check if already done */
    if (BU_LIST_NON_EMPTY(&dp->d_use_hd))
	return;

    digits = log10((double)dp->d_uses) + 2.0;
    snprintf(format_v5, 25, "%%s_%%0%dd", digits);
    snprintf(format_v4, 25, "_%%0%dd", digits);

    name_length = (int)strlen(dp->d_namep);
    if (name_length + digits + 1 > NAMESIZE - 1)
	suffix_start = NAMESIZE - digits - 2;
    else
	suffix_start = name_length;

    bu_vls_init(&name_v5);

    j = 0;
    for (use_no=0; use_no<dp->d_uses; use_no++) {
	j++;
	BU_ALLOC(use, struct object_use);

	/* set xform for this object_use to all zeros */
	MAT_ZERO(use->xform);
	use->used = 0;
	if (db_version(dbip) < 5) {
	    NAMEMOVE(dp->d_namep, name_v4);
	    name_v4[NAMESIZE] = '\0';                /* ensure null termination */
	}

	/* Add an entry for the original at the end of the list
	 * This insures that the original will be last to be modified
	 * If original were modified earlier, copies would be screwed-up
	 */
	if (use_no == dp->d_uses-1 && dp->d_uses == dp->d_nref)
	    use->dp = dp;
	else {
	    if (db_version(dbip) < 5) {
		snprintf(&name_v4[suffix_start], NAMESIZE-suffix_start, format_v4, j);
		name = name_v4;
	    } else {
		bu_vls_trunc(&name_v5, 0);
		bu_vls_printf(&name_v5, format_v5, dp->d_namep, j);
		name = bu_vls_addr(&name_v5);
	    }

	    /* Insure that new name is unique */
	    while (db_lookup(dbip, name, 0) != RT_DIR_NULL) {
		j++;
		if (db_version(dbip) < 5) {
		    snprintf(&name_v4[suffix_start], NAMESIZE-suffix_start, format_v4, j);
		    name = name_v4;
		} else {
		    bu_vls_trunc(&name_v5, 0);
		    bu_vls_printf(&name_v5, format_v5, dp->d_namep, j);
		    name = bu_vls_addr(&name_v5);
		}
	    }

	    /* Add new name to directory */
	    use->dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, dp->d_flags, (void *)&dp->d_minor_type);
	    if (use->dp == RT_DIR_NULL) {
		WDB_ALLOC_ERR_return;
	    }
	}

	/* Add new directory pointer to use list for this object */
	BU_LIST_INSERT(&dp->d_use_hd, &use->l);
    }

    bu_vls_free(&name_v5);
}


static struct directory *
Copy_solid(struct db_i *dbip,
	   struct directory *dp,
	   mat_t xform,
	   struct rt_wdb *wdbp)
{
    struct directory *found;
    struct rt_db_internal sol_int;
    struct object_use *use;

    RT_CK_DIR(dp);

    if (!(dp->d_flags & RT_DIR_SOLID)) {
	Tcl_AppendResult(wdbp->wdb_interp, "Copy_solid: ", dp->d_namep,
			 " is not a solid!!!!\n", (char *)NULL);
	return RT_DIR_NULL;
    }

    /* If no transformation is to be applied, just use the original */
    if (bn_mat_is_identity(xform)) {
	/* find original in the list */
	for (BU_LIST_FOR (use, object_use, &dp->d_use_hd)) {
	    if (use->dp == dp && use->used == 0) {
		use->used = 1;
		return dp;
	    }
	}
    }

    /* Look for a copy that already has this transform matrix */
    for (BU_LIST_FOR (use, object_use, &dp->d_use_hd)) {
	if (bn_mat_is_equal(xform, use->xform, &wdbp->wdb_tol)) {
	    /* found a match, no need to make another copy */
	    use->used = 1;
	    return use->dp;
	}
    }

    /* get a fresh use */
    found = RT_DIR_NULL;
    for (BU_LIST_FOR (use, object_use, &dp->d_use_hd)) {
	if (use->used)
	    continue;

	found = use->dp;
	use->used = 1;
	MAT_COPY(use->xform, xform);
	break;
    }

    if (found == RT_DIR_NULL && dp->d_nref == 1 && dp->d_uses == 1) {
	/* only one use, take it */
	found = dp;
    }

    if (found == RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, "Ran out of uses for solid ",
			 dp->d_namep, "\n", (char *)NULL);
	return RT_DIR_NULL;
    }

    if (rt_db_get_internal(&sol_int, dp, dbip, xform, &rt_uniresource) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Cannot import solid ",
			 dp->d_namep, "\n", (char *)NULL);
	return RT_DIR_NULL;
    }

    RT_CK_DB_INTERNAL(&sol_int);
    if (rt_db_put_internal(found, dbip, &sol_int, &rt_uniresource) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Cannot write copy solid (", found->d_namep,
			 ") to database\n", (char *)NULL);
	return RT_DIR_NULL;
    }

    return found;
}


static struct directory *Copy_object(struct db_i *dbip, struct directory *dp, fastf_t *xform, struct rt_wdb *wdbp);

HIDDEN void
Do_copy_membs(struct db_i *dbip,
	      struct rt_comb_internal *UNUSED(comb),
	      union tree *comb_leaf,
	      void *user_ptr1,
	      void *UNUSED(user_ptr2),
	      void *user_ptr3,
	      void *UNUSED(user_ptr4))
{
    struct directory *dp;
    struct directory *dp_new;
    mat_t new_xform;
    matp_t xform;
    struct rt_wdb *wdbp;

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    dp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	return;

    xform = (matp_t)user_ptr1;
    wdbp = (struct rt_wdb *)user_ptr3;

    /* apply transform matrix for this arc */
    if (comb_leaf->tr_l.tl_mat) {
	bn_mat_mul(new_xform, xform, comb_leaf->tr_l.tl_mat);
    } else {
	MAT_COPY(new_xform, xform);
    }

    /* Copy member with current transform matrix */
    dp_new=Copy_object(dbip, dp, new_xform, wdbp);
    if (dp_new == RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, "Failed to copy object ",
			 dp->d_namep, "\n", (char *)NULL);
	return;
    }

    /* replace member name with new copy */
    bu_free(comb_leaf->tr_l.tl_name, "comb_leaf->tr_l.tl_name");
    comb_leaf->tr_l.tl_name = bu_strdup(dp_new->d_namep);

    /* make transform for this arc the identity matrix */
    if (!comb_leaf->tr_l.tl_mat) {
	comb_leaf->tr_l.tl_mat = (matp_t)bu_malloc(sizeof(mat_t), "tl_mat");
    }
    MAT_IDN(comb_leaf->tr_l.tl_mat);
}


static struct directory *
Copy_comb(struct db_i *dbip,
	  struct directory *dp,
	  mat_t xform,
	  struct rt_wdb *wdbp)
{
    struct object_use *use;
    struct directory *found;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    RT_CK_DIR(dp);

    /* Look for a copy that already has this transform matrix */
    for (BU_LIST_FOR (use, object_use, &dp->d_use_hd)) {
	if (bn_mat_is_equal(xform, use->xform, &wdbp->wdb_tol)) {
	    /* found a match, no need to make another copy */
	    use->used = 1;
	    return use->dp;
	}
    }

    /* if we can't get records for this combination, just leave it alone */
    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0)
	return dp;
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    /* copy members */
    if (comb->tree)
	db_tree_funcleaf(dbip, comb, comb->tree, Do_copy_membs,
			 (void *)xform, (void *)wdbp->wdb_interp, (void *)wdbp, (void *)NULL);

    /* Get a use of this object */
    found = RT_DIR_NULL;
    for (BU_LIST_FOR (use, object_use, &dp->d_use_hd)) {
	/* Get a fresh use of this object */
	if (use->used)
	    continue;	/* already used */
	found = use->dp;
	use->used = 1;
	MAT_COPY(use->xform, xform);
	break;
    }

    if (found == RT_DIR_NULL && dp->d_nref == 1 && dp->d_uses == 1) {
	/* only one use, so take original */
	found = dp;
    }

    if (found == RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, "Ran out of uses for combination ",
			 dp->d_namep, "\n", (char *)NULL);
	return RT_DIR_NULL;
    }

    if (rt_db_put_internal(found, dbip, &intern, &rt_uniresource) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "rt_db_put_internal failed for ", dp->d_namep,
			 "\n", (char *)NULL);
	rt_db_free_internal(&intern);

	return RT_DIR_NULL;
    }

    return found;
}


static struct directory *
Copy_object(struct db_i *dbip,
	    struct directory *dp,
	    mat_t xform,
	    struct rt_wdb *wdbp)
{
    RT_CK_DIR(dp);

    if (dp->d_flags & RT_DIR_SOLID)
	return Copy_solid(dbip, dp, xform, wdbp);
    else
	return Copy_comb(dbip, dp, xform, wdbp);
}


HIDDEN void
Do_ref_incr(struct db_i *dbip,
	    struct rt_comb_internal *UNUSED(comb),
	    union tree *comb_leaf,
	    void *UNUSED(user_ptr1),
	    void *UNUSED(user_ptr2),
	    void *UNUSED(user_ptr3),
	    void *UNUSED(user_ptr4))
{
    struct directory *dp;

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    if ((dp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL)
	return;

    dp->d_nref++;
}


int
wdb_xpush_cmd(struct rt_wdb *wdbp,
	      int argc,
	      const char *argv[])
{
    struct directory *old_dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct bu_ptbl tops;
    mat_t xform;
    int i;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_xpush %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* get directory pointer for arg */
    if ((old_dp = db_lookup(wdbp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return TCL_ERROR;

    if (old_dp->d_flags & RT_DIR_SOLID) {
	bu_log("Attempt to xpush a primitive, aborting.\n");
	return TCL_ERROR;
    }

    /* Initialize use and reference counts of all directory entries */
    for (i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;

	for (dp = wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (!(dp->d_flags & (RT_DIR_SOLID | RT_DIR_COMB)))
		continue;

	    dp->d_uses = 0;
	    dp->d_nref = 0;
	}
    }

    /* Count uses in the tree being pushed (updates dp->d_uses) */
    db_functree(wdbp->dbip, old_dp, increment_uses, increment_uses, &rt_uniresource, NULL);

    /* Do a simple reference count to find top level objects */
    for (i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;

	for (dp = wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_flags & RT_DIR_SOLID)
		continue;

	    if (!(dp->d_flags & (RT_DIR_SOLID | RT_DIR_COMB)))
		continue;

	    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0)
		WDB_TCL_READ_ERR_return;
	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    if (comb->tree)
		db_tree_funcleaf(wdbp->dbip, comb, comb->tree, Do_ref_incr,
				 (void *)NULL, (void *)NULL, (void *)NULL, (void *)NULL);
	    rt_db_free_internal(&intern);
	}
    }

    /* anything with zero references is a tree top */
    bu_ptbl_init(&tops, 0, "tops for xpush");
    for (i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;

	for (dp = wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_flags & RT_DIR_SOLID)
		continue;

	    if (!(dp->d_flags & (RT_DIR_SOLID | RT_DIR_COMB)))
		continue;

	    if (dp->d_nref == 0)
		bu_ptbl_ins(&tops, (long *)dp);
	}
    }

    /* now re-zero the reference counts */
    for (i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;

	for (dp = wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (!(dp->d_flags & (RT_DIR_SOLID | RT_DIR_COMB)))
		continue;

	    dp->d_nref = 0;
	}
    }

    /* accurately count references in entire model */
    for (i = 0; i < BU_PTBL_END(&tops); i++) {
	struct directory *dp;

	dp = (struct directory *)BU_PTBL_GET(&tops, i);
	db_functree(wdbp->dbip, dp, increment_nrefs, increment_nrefs, &rt_uniresource, NULL);
    }

    /* Free list of tree-tops */
    bu_ptbl_free(&tops);

    /* Make new names */
    db_functree(wdbp->dbip, old_dp, Make_new_name, Make_new_name, &rt_uniresource, NULL);

    MAT_IDN(xform);

    /* Make new objects */
    if (rt_db_get_internal(&intern, old_dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_log("ERROR: cannot load %s feom the database!!!\n", old_dp->d_namep);
	bu_log("\tNothing has been changed!!\n");
	Free_uses(wdbp->dbip);
	return TCL_ERROR;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    if (!comb->tree) {
	Free_uses(wdbp->dbip);
	return TCL_OK;
    }

    db_tree_funcleaf(wdbp->dbip, comb, comb->tree, Do_copy_membs,
		     (void *)xform, (void *)wdbp->wdb_interp, (void *)wdbp, (void *)NULL);

    if (rt_db_put_internal(old_dp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "rt_db_put_internal failed for ", old_dp->d_namep,
			 "\n", (char *)NULL);
	rt_db_free_internal(&intern);
	Free_uses(wdbp->dbip);
	return TCL_ERROR;
    }

    /* Free use lists and delete unused directory entries */
    Free_uses(wdbp->dbip);
    return TCL_OK;
}


static int
wdb_xpush_tcl(void *clientData,
	      int argc,
	      const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_xpush_cmd(wdbp, argc-1, argv+1);
}


int
wdb_whatid_cmd(struct rt_wdb *wdbp,
	       int argc,
	       const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct bu_vls vls;

    if (argc != 2) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_whatid %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    dp = db_lookup(wdbp->dbip, argv[1], LOOKUP_NOISY);
    if (dp == RT_DIR_NULL)
	return TCL_ERROR;

    if (!(dp->d_flags & RT_DIR_REGION)) {
	Tcl_AppendResult(wdbp->wdb_interp, argv[1], " is not a region", (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0)
	return TCL_ERROR;
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%ld", comb->region_id);
    rt_db_free_internal(&intern);
    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
}


/**
 * Usage:
 * procname whatid object
 */
static int
wdb_whatid_tcl(void *clientData,
	       int argc,
	       const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_whatid_cmd(wdbp, argc-1, argv+1);
}


struct wdb_node_data {
    FILE *fp;
    Tcl_Interp *interp;
};


/**
 *@brief
 * Support for the 'keep' method.
 * Write each node encountered exactly once.
 */
void
wdb_node_write(struct db_i *dbip,
	       struct directory *dp,
	       void *ptr)
{
    struct rt_wdb *keepfp = (struct rt_wdb *)ptr;
    struct rt_db_internal intern;

    RT_CK_WDB(keepfp);

    if (dp->d_nref++ > 0)
	return;		/* already written */

    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	WDB_READ_ERR_return;

    /* if this is an extrusion, keep the referenced sketch */
    if (dp->d_major_type == DB5_MAJORTYPE_BRLCAD && dp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE) {
	struct rt_extrude_internal *extr;
	struct directory *dp2;

	extr = (struct rt_extrude_internal *)intern.idb_ptr;
	RT_EXTRUDE_CK_MAGIC(extr);

	if ((dp2 = db_lookup(dbip, extr->sketch_name, LOOKUP_QUIET)) != RT_DIR_NULL) {
	    wdb_node_write(dbip, dp2, ptr);
	}
    } else if (dp->d_major_type == DB5_MAJORTYPE_BRLCAD && dp->d_minor_type == DB5_MINORTYPE_BRLCAD_DSP) {
	struct rt_dsp_internal *dsp;
	struct directory *dp2;

	/* this is a DSP, if it uses a binary object, keep it also */
	dsp = (struct rt_dsp_internal *)intern.idb_ptr;
	RT_DSP_CK_MAGIC(dsp);

	if (dsp->dsp_datasrc == RT_DSP_SRC_OBJ) {
	    /* need to keep this object */
	    if ((dp2 = db_lookup(dbip, bu_vls_addr(&dsp->dsp_name),  LOOKUP_QUIET)) != RT_DIR_NULL) {
		wdb_node_write(dbip, dp2, ptr);
	    }
	}
    }

    if (wdb_put_internal(keepfp, dp->d_namep, &intern, 1.0) < 0)
	WDB_WRITE_ERR_return;
}


int
wdb_keep_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    struct rt_wdb *keepfp;
    struct directory *dp;
    struct bu_vls title;
    int i;
    struct db_i *new_dbip;

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_keep %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* First, clear any existing counts */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw)
	    dp->d_nref = 0;
    }

    /* Alert user if named file already exists */

    new_dbip = db_open(argv[1], DB_OPEN_READWRITE);


    if (new_dbip != DBI_NULL) {
	if (db_version(new_dbip) != db_version(wdbp->dbip)) {
	    Tcl_AppendResult(wdbp->wdb_interp,
			     "keep: File format mismatch between '",
			     argv[1], "' and '",
			     wdbp->dbip->dbi_filename, "'\n",
			     (char *)NULL);
	    return TCL_ERROR;
	}

	if ((keepfp = wdb_dbopen(new_dbip, RT_WDB_TYPE_DB_DISK)) == NULL) {
	    Tcl_AppendResult(wdbp->wdb_interp, "keep:  Error opening '", argv[1],
			     "'\n", (char *)NULL);
	    return TCL_ERROR;
	} else {
	    Tcl_AppendResult(wdbp->wdb_interp, "keep:  appending to '", argv[1],
			     "'\n", (char *)NULL);

	    /* --- Scan geometry database and build in-memory directory --- */
	    db_dirbuild(new_dbip);
	}
    } else {
	/* Create a new database */
	keepfp = wdb_fopen_v(argv[1], db_version(wdbp->dbip));

	if (keepfp == NULL) {
	    perror(argv[1]);
	    return TCL_ERROR;
	}
    }

    /* ident record */
    bu_vls_init(&title);
    if (bu_strncmp(wdbp->dbip->dbi_title, "Parts of: ", 10) != 0) {
	bu_vls_strcat(&title, "Parts of: ");
    }
    bu_vls_strcat(&title, wdbp->dbip->dbi_title);

    if (db_update_ident(keepfp->dbip, bu_vls_addr(&title), wdbp->dbip->dbi_local2base) < 0) {
	perror("fwrite");
	Tcl_AppendResult(wdbp->wdb_interp, "db_update_ident() failed\n", (char *)NULL);
	wdb_close(keepfp);
	bu_vls_free(&title);
	return TCL_ERROR;
    }
    bu_vls_free(&title);

    for (i = 2; i < argc; i++) {
	if ((dp = db_lookup(wdbp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;
	db_functree(wdbp->dbip, dp, wdb_node_write, wdb_node_write, &rt_uniresource, (void *)keepfp);
    }

    wdb_close(keepfp);
    return TCL_OK;
}


/*
 * Usage:
 * procname keep file object(s)
 */
static int
wdb_keep_tcl(void *clientData,
	     int argc,
	     const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_keep_cmd(wdbp, argc-1, argv+1);
}


int
wdb_cat_cmd(struct rt_wdb *wdbp,
	    int argc,
	    const char *argv[])
{
    struct directory *dp;
    int arg;
    struct bu_vls str;

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_cat %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_init(&str);
    for (arg = 1; arg < argc; arg++) {
	if ((dp = db_lookup(wdbp->dbip, argv[arg], LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;

	bu_vls_trunc(&str, 0);
	wdb_do_list(wdbp->dbip, &str, dp, 0);	/* non-verbose */
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&str), "\n", (char *)NULL);
    }
    bu_vls_free(&str);

    return TCL_OK;
}


/**
 * Usage:
 * procname cat object(s)
 */
static int
wdb_cat_tcl(void *clientData,
	    int argc,
	    const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_cat_cmd(wdbp, argc-1, argv+1);
}


int
wdb_instance_cmd(struct rt_wdb *wdbp,
		 int argc,
		 const char *argv[])
{
    struct directory *dp;
    char oper;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 3 || 4 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_instance %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((dp = db_lookup(wdbp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return TCL_ERROR;

    oper = WMOP_UNION;
    if (argc == 4)
	oper = argv[3][0];

    if (oper != WMOP_UNION &&
	oper != WMOP_SUBTRACT &&
	oper != WMOP_INTERSECT) {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "bad operation: %c\n", oper);
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
	return TCL_ERROR;
    }

    if (wdb_combadd(wdbp->dbip, dp, argv[2], 0, oper, 0, 0, wdbp) == RT_DIR_NULL)
	return TCL_ERROR;

    return TCL_OK;
}


/**
 * @brief
 * Add instance of obj to comb.
 *
 * Usage:
 * procname i obj comb [op]
 */
static int
wdb_instance_tcl(void *clientData,
		 int argc,
		 const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_instance_cmd(wdbp, argc-1, argv+1);
}


int
wdb_observer_cmd(struct rt_wdb *wdbp,
		 int argc,
		 const char *argv[])
{
    if (argc < 2) {
	bu_log("ERROR: expecting two or more arguments\n");
	return TCL_ERROR;
    }

    return bu_observer_cmd((ClientData)&wdbp->wdb_observers, argc-1, (const char **)argv+1);
}


/**
 * @brief
 * Attach/detach observers to/from list.
 *
 * Usage:
 * procname observer cmd [args]
 *
 */
static int
wdb_observer_tcl(void *clientData,
		 int argc,
		 const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_observer_cmd(wdbp, argc-1, argv+1);
}


int
wdb_make_bb_cmd(struct rt_wdb *wdbp,
		int argc,
		const char *argv[])
{
    int i;
    point_t rpp_min, rpp_max;
    struct directory *dp;
    struct rt_arb_internal *arb;
    struct rt_db_internal new_intern;
    const char *new_name;
    int use_air = 0;
    struct ged ged;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_make_bb %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /*XXX Temporary */
    GED_INIT(&ged, wdbp);

    i = 1;

    /* look for a USEAIR option */
    if (BU_STR_EQUAL(argv[i], "-u")) {
	use_air = 1;
	i++;
    }

    /* Since arguments may be paths, make sure first argument isn't */
    if (strchr(argv[i], '/')) {
	Tcl_AppendResult(wdbp->wdb_interp, "Do not use '/' in solid names: ", argv[i], "\n", (char *)NULL);

	/* release any allocated memory */
	ged_free(&ged);

	return TCL_ERROR;
    }

    new_name = argv[i++];
    if (db_lookup(wdbp->dbip, new_name, LOOKUP_QUIET) != RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, new_name, " already exists\n", (char *)NULL);

	/* release any allocated memory */
	ged_free(&ged);

	return TCL_ERROR;
    }

    if (_ged_get_obj_bounds(&ged, argc-2, (const char **)argv+2, use_air, rpp_min, rpp_max) == TCL_ERROR) {
	/* release any allocated memory */
	ged_free(&ged);

	return TCL_ERROR;
    }

    /* release any allocated memory */
    ged_free(&ged);

    /* build bounding RPP */
    BU_ALLOC(arb, struct rt_arb_internal);
    VMOVE(arb->pt[0], rpp_min);
    VSET(arb->pt[1], rpp_min[X], rpp_min[Y], rpp_max[Z]);
    VSET(arb->pt[2], rpp_min[X], rpp_max[Y], rpp_max[Z]);
    VSET(arb->pt[3], rpp_min[X], rpp_max[Y], rpp_min[Z]);
    VSET(arb->pt[4], rpp_max[X], rpp_min[Y], rpp_min[Z]);
    VSET(arb->pt[5], rpp_max[X], rpp_min[Y], rpp_max[Z]);
    VMOVE(arb->pt[6], rpp_max);
    VSET(arb->pt[7], rpp_max[X], rpp_max[Y], rpp_min[Z]);
    arb->magic = RT_ARB_INTERNAL_MAGIC;

    /* set up internal structure */
    RT_DB_INTERNAL_INIT(&new_intern);
    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    new_intern.idb_type = ID_ARB8;
    new_intern.idb_meth = &OBJ[ID_ARB8];
    new_intern.idb_ptr = (void *)arb;

    dp = db_diradd(wdbp->dbip, new_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&new_intern.idb_type);
    if (dp == RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, "Cannot add ", new_name, " to directory\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_db_put_internal(dp, wdbp->dbip, &new_intern, wdbp->wdb_resp) < 0) {
	rt_db_free_internal(&new_intern);
	Tcl_AppendResult(wdbp->wdb_interp, "Database write error, aborting.\n", (char *)NULL);

	return TCL_ERROR;
    }

    return TCL_OK;
}


/**
 * @brief
 * Build an RPP bounding box for the list of objects
 * and/or paths passed to this routine
 *
 * Usage:
 * dbobjname make_bb bbname obj(s)
 */
static int
wdb_make_bb_tcl(void *clientData,
		int argc,
		const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_make_bb_cmd(wdbp, argc-1, argv+1);
}


int
wdb_units_cmd(struct rt_wdb *wdbp,
	      int argc,
	      const char *argv[])
{
    double loc2mm;
    struct bu_vls vls;
    const char *str;
    int sflag = 0;

    bu_vls_init(&vls);
    if (argc < 1 || 2 < argc) {
	bu_vls_printf(&vls, "helplib_alias wdb_units %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (argc == 2 && BU_STR_EQUAL(argv[1], "-s")) {
	--argc;
	++argv;

	sflag = 1;
    }

    if (argc < 2) {
	str = bu_units_string(wdbp->dbip->dbi_local2base);
	if (!str) str = "Unknown_unit";

	if (sflag)
	    bu_vls_printf(&vls, "%s", str);
	else
	    bu_vls_printf(&vls, "You are editing in '%s'.  1 %s = %g mm \n",
			  str, str, wdbp->dbip->dbi_local2base);

	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return TCL_OK;
    }

    /* Allow inputs of the form "25cm" or "3ft" */
    if ((loc2mm = bu_mm_value(argv[1])) <= 0) {
	Tcl_AppendResult(wdbp->wdb_interp, argv[1], ": unrecognized unit\n",
			 "valid units: <um|mm|cm|m|km|in|ft|yd|mi>\n", (char *)NULL);
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (db_update_ident(wdbp->dbip, wdbp->dbip->dbi_title, loc2mm) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp,
			 "Warning: unable to stash working units into database\n",
			 (char *)NULL);
    }

    wdbp->dbip->dbi_local2base = loc2mm;
    wdbp->dbip->dbi_base2local = 1.0 / loc2mm;

    str = bu_units_string(wdbp->dbip->dbi_local2base);
    if (!str) str = "Unknown_unit";
    bu_vls_printf(&vls, "You are now editing in '%s'.  1 %s = %g mm \n",
		  str, str, wdbp->dbip->dbi_local2base);
    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
}


/**
 *@brief
 * Set/get the database units.
 *
 * Usage:
 * dbobjname units [str]
 */
static int
wdb_units_tcl(void *clientData,
	      int argc,
	      const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_units_cmd(wdbp, argc-1, argv+1);
}


int
wdb_hide_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    struct directory *dp;
    struct db_i *dbip;
    struct bu_external ext;
    struct bu_external tmp;
    struct db5_raw_internal raw;
    int i;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_hide %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    RT_CK_WDB(wdbp);

    dbip = wdbp->dbip;

    RT_CK_DBI(dbip);
    if (db_version(dbip) < 5) {
	Tcl_AppendResult(wdbp->wdb_interp,
			 "Database was created with a previous release of BRL-CAD.\nSelect \"Tools->Upgrade Database...\" to enable support for this feature.",
			 (char *)NULL);
	return TCL_ERROR;
    }

    for (i = 1; i < argc; i++) {
	if ((dp = db_lookup(dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    continue;
	}

	RT_CK_DIR(dp);
	BU_EXTERNAL_INIT(&ext);

	if (db_get_external(&ext, dp, dbip) < 0) {
	    Tcl_AppendResult(wdbp->wdb_interp, "db_get_external failed for ",
			     dp->d_namep, " \n", (char *)NULL);
	    continue;
	}

	if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) == NULL) {
	    Tcl_AppendResult(wdbp->wdb_interp, "db5_get_raw_internal_ptr() failed for ",
			     dp->d_namep, " \n", (char *)NULL);
	    bu_free_external(&ext);
	    continue;
	}

	raw.h_name_hidden = (unsigned char)(0x1);

	BU_EXTERNAL_INIT(&tmp);
	db5_export_object3(&tmp, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
			   dp->d_namep,
			   raw.h_name_hidden,
			   &raw.attributes,
			   &raw.body,
			   raw.major_type, raw.minor_type,
			   raw.a_zzz, raw.b_zzz);
	bu_free_external(&ext);

	if (db_put_external(&tmp, dp, dbip)) {
	    Tcl_AppendResult(wdbp->wdb_interp, "db_put_external() failed for ",
			     dp->d_namep, " \n", (char *)NULL);
	    bu_free_external(&tmp);
	    continue;
	}
	bu_free_external(&tmp);
	dp->d_flags |= RT_DIR_HIDDEN;
    }

    return TCL_OK;
}


static int
wdb_hide_tcl(void *clientData,
	     int argc,
	     const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_hide_cmd(wdbp, argc-1, argv+1);
}


int
wdb_unhide_cmd(struct rt_wdb *wdbp,
	       int argc,
	       const char *argv[])
{
    struct directory *dp;
    struct db_i *dbip;
    struct bu_external ext;
    struct bu_external tmp;
    struct db5_raw_internal raw;
    int i;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_unhide %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    RT_CK_WDB(wdbp);

    dbip = wdbp->dbip;

    RT_CK_DBI(dbip);
    if (db_version(dbip) < 5) {
	Tcl_AppendResult(wdbp->wdb_interp,
			 "Database was created with a previous release of BRL-CAD.\nSelect \"Tools->Upgrade Database...\" to enable support for this feature.",
			 (char *)NULL);
	return TCL_ERROR;
    }

    for (i = 1; i < argc; i++) {
	if ((dp = db_lookup(dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    continue;
	}

	RT_CK_DIR(dp);

	BU_EXTERNAL_INIT(&ext);

	if (db_get_external(&ext, dp, dbip) < 0) {
	    Tcl_AppendResult(wdbp->wdb_interp, "db_get_external failed for ",
			     dp->d_namep, " \n", (char *)NULL);
	    continue;
	}

	if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) == NULL) {
	    Tcl_AppendResult(wdbp->wdb_interp, "db5_get_raw_internal_ptr() failed for ",
			     dp->d_namep, " \n", (char *)NULL);
	    bu_free_external(&ext);
	    continue;
	}

	raw.h_name_hidden = (unsigned char)(0x0);

	BU_EXTERNAL_INIT(&tmp);
	db5_export_object3(&tmp, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
			   dp->d_namep,
			   raw.h_name_hidden,
			   &raw.attributes,
			   &raw.body,
			   raw.major_type, raw.minor_type,
			   raw.a_zzz, raw.b_zzz);
	bu_free_external(&ext);

	if (db_put_external(&tmp, dp, dbip)) {
	    Tcl_AppendResult(wdbp->wdb_interp, "db_put_external() failed for ",
			     dp->d_namep, " \n", (char *)NULL);
	    bu_free_external(&tmp);
	    continue;
	}
	bu_free_external(&tmp);
	dp->d_flags &= (~RT_DIR_HIDDEN);
    }

    return TCL_OK;
}


static int
wdb_unhide_tcl(void *clientData,
	       int argc,
	       const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_unhide_cmd(wdbp, argc-1, argv+1);
}


/**
 *@brief
 * implements the "attr" command.
 *
 * argv[1] is a sub-command:
 *		get - get attributes
 *		set - add a new attribute or replace an existing one
 * rm - remove an attribute
 *		append - append to an existing attribute
 *		edit - invoke an editor to edit all attributes
 *
 * argv[2] is the name of the object
 *
 * for "get" or "show", remaining args are attribute names (or none for all)
 *
 * for "set", remaining args are attribute name, attribute value..
 *
 * for "rm", remaining args are all attribute names
 *
 * for "append", remaining args are attribute name, value to append, ...
 *
 * for "edit", remaining args are attribute names
 */
int
wdb_attr_cmd(struct rt_wdb *wdbp,
	     int argc,
	     const char *argv[])
{
    int i;
    struct directory *dp;
    struct bu_attribute_value_set avs;
    struct bu_attribute_value_pair *avpp;

    /* this is only valid for v5 databases */
    if (db_version(wdbp->dbip) < 5) {
	Tcl_AppendResult(wdbp->wdb_interp, "Attributes are not available for this database format.\nPlease upgrade your database format using \"dbupgrade\" to enable attributes.", (char *)NULL);
	return TCL_ERROR;
    }

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_attr %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* Verify that this wdb supports lookup operations
       (non-null dbip) */
    if (wdbp->dbip == 0) {
	Tcl_AppendResult(wdbp->wdb_interp,
			 "db does not support lookup operations",
			 (char *)NULL);
	return TCL_ERROR;
    }

    dp = db_lookup(wdbp->dbip, argv[2], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp,
			 argv[2],
			 " does not exist\n",
			 (char *)NULL);
	return TCL_ERROR;
    }

    bu_avs_init_empty(&avs);
    if (db5_get_attributes(wdbp->dbip, &avs, dp)) {
	Tcl_AppendResult(wdbp->wdb_interp,
			 "Cannot get attributes for object ", dp->d_namep, "\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "get")) {
	if (argc == 3) {
	    /* just list all the attributes */
	    avpp = avs.avp;
	    for (i = 0; i < (int)avs.count; i++, avpp++) {
		Tcl_AppendResult(wdbp->wdb_interp, avpp->name, " {",
				 avpp->value, "} ", (char *)NULL);
	    }
	} else {
	    const char *val;
	    int do_separators=argc-4; /* if more than one attribute */

	    for (i = 3; i < argc; i++) {
		val = bu_avs_get(&avs, argv[i]);
		if (!val) {
		    Tcl_ResetResult(wdbp->wdb_interp);
		    Tcl_AppendResult(wdbp->wdb_interp, "Object ",
				     dp->d_namep, " does not have a ",
				     argv[i], " attribute\n",
				     (char *)NULL);
		    bu_avs_free(&avs);
		    return TCL_ERROR;
		}
		if (do_separators) {
		    Tcl_AppendResult(wdbp->wdb_interp,
				     "{",
				     val,
				     "} ",
				     (char *)NULL);
		} else {
		    Tcl_AppendResult(wdbp->wdb_interp, val,
				     (char *)NULL);
		}
	    }
	}

	bu_avs_free(&avs);
	return TCL_OK;

    } else if (BU_STR_EQUAL(argv[1], "set")) {
	/* setting attribute/value pairs */
	if ((argc - 3) % 2) {
	    Tcl_AppendResult(wdbp->wdb_interp,
			     "Error: attribute names and values must be in pairs!!!\n",
			     (char *)NULL);
	    bu_avs_free(&avs);
	    return TCL_ERROR;
	}

	i = 3;
	while (i < argc) {
	    if (BU_STR_EQUAL(argv[i], "region") && BU_STR_EQUAL(argv[i+1], "R")) {
		dp->d_flags |= RT_DIR_REGION;
	    }
	    (void)bu_avs_add(&avs, argv[i], argv[i+1]);
	    i += 2;
	}
	if (db5_update_attributes(dp, &avs, wdbp->dbip)) {
	    Tcl_AppendResult(wdbp->wdb_interp,
			     "Error: failed to update attributes\n",
			     (char *)NULL);
	    bu_avs_free(&avs);
	    return TCL_ERROR;
	}

	/* avs is freed by db5_update_attributes() */
	return TCL_OK;
    } else if (BU_STR_EQUAL(argv[1], "rm")) {
	i = 3;
	while (i < argc) {
	    if (BU_STR_EQUAL(argv[i], "region")) {
		dp->d_flags = dp->d_flags & ~(RT_DIR_REGION);
	    }
	    (void)bu_avs_remove(&avs, argv[i]);
	    i++;
	}
	if (db5_replace_attributes(dp, &avs, wdbp->dbip)) {
	    Tcl_AppendResult(wdbp->wdb_interp,
			     "Error: failed to update attributes\n",
			     (char *)NULL);
	    bu_avs_free(&avs);
	    return TCL_ERROR;
	}

	/* avs is freed by db5_replace_attributes() */
	return TCL_OK;
    } else if (BU_STR_EQUAL(argv[1], "append")) {
	if ((argc-3)%2) {
	    Tcl_AppendResult(wdbp->wdb_interp,
			     "Error: attribute names and values must be in pairs!!!\n",
			     (char *)NULL);
	    bu_avs_free(&avs);
	    return TCL_ERROR;
	}
	i = 3;
	while (i < argc) {
	    const char *old_val;

	    if (BU_STR_EQUAL(argv[i], "region") && BU_STR_EQUAL(argv[i+1], "R")) {
		dp->d_flags |= RT_DIR_REGION;
	    }
	    old_val = bu_avs_get(&avs, argv[i]);
	    if (!old_val) {
		(void)bu_avs_add(&avs, argv[i], argv[i+1]);
	    } else {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_strcat(&vls, old_val);
		bu_vls_strcat(&vls, argv[i+1]);
		bu_avs_add_vls(&avs, argv[i], &vls);
		bu_vls_free(&vls);
	    }

	    i += 2;
	}
	if (db5_replace_attributes(dp, &avs, wdbp->dbip)) {
	    Tcl_AppendResult(wdbp->wdb_interp,
			     "Error: failed to update attributes\n",
			     (char *)NULL);
	    bu_avs_free(&avs);
	    return TCL_ERROR;
	}

	/* avs is freed by db5_replace_attributes() */
	return TCL_OK;
    } else if (BU_STR_EQUAL(argv[1], "show")) {
	struct bu_vls vls;
	int max_attr_name_len=0;
	int tabs1=0;

	/* pretty print */
	bu_vls_init(&vls);
	if (dp->d_flags & RT_DIR_COMB) {
	    if (dp->d_flags & RT_DIR_REGION) {
		bu_vls_printf(&vls, "%s region:\n", argv[2]);
	    } else {
		bu_vls_printf(&vls, "%s combination:\n", argv[2]);
	    }
	} else if (dp->d_flags & RT_DIR_SOLID) {
	    struct rt_db_internal intern;
	    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) >= 0) {
		bu_vls_printf(&vls, "%s %s:\n", argv[2], intern.idb_meth->ft_label);
		rt_db_free_internal(&intern);
	    }
	} else {
	    switch (dp->d_major_type) {
		case DB5_MAJORTYPE_ATTRIBUTE_ONLY:
		    bu_vls_printf(&vls, "%s global:\n", argv[2]);
		    break;
		case DB5_MAJORTYPE_BINARY_MIME:
		    bu_vls_printf(&vls, "%s binary(mime):\n", argv[2]);
		    break;
		case DB5_MAJORTYPE_BINARY_UNIF:
		    bu_vls_printf(&vls, "%s %s:\n", argv[2],
				  binu_types[dp->d_minor_type]);
		    break;
	    }
	}
	if (argc == 3) {
	    /* just display all attributes */
	    avpp = avs.avp;
	    for (i = 0; i < (int)avs.count; i++, avpp++) {
		int len;

		len = (int)strlen(avpp->name);
		if (len > max_attr_name_len) {
		    max_attr_name_len = len;
		}
	    }
	    tabs1 = 2 + max_attr_name_len/8;
	    avpp = avs.avp;
	    for (i = 0; i < (int)avs.count; i++, avpp++) {
		const char *c;
		int tabs2;
		int k;
		int len;

		bu_vls_printf(&vls, "\t%s", avpp->name);
		len = (int)strlen(avpp->name);
		tabs2 = tabs1 - 1 - len/8;
		for (k = 0; k < tabs2; k++) {
		    bu_vls_putc(&vls, '\t');
		}
		c = avpp->value;
		while (*c) {
		    bu_vls_putc(&vls, *c);
		    if (*c == '\n') {
			for (k = 0; k < tabs1; k++) {
			    bu_vls_putc(&vls, '\t');
			}
		    }
		    c++;
		}
		bu_vls_putc(&vls, '\n');
	    }
	} else {
	    const char *val;
	    int len;

	    /* show just the specified attributes */
	    for (i = 0; i < argc; i++) {
		len = (int)strlen(argv[i]);
		if (len > max_attr_name_len) {
		    max_attr_name_len = len;
		}
	    }
	    tabs1 = 2 + max_attr_name_len/8;
	    for (i = 3; i < argc; i++) {
		int tabs2;
		int k;
		const char *c;

		val = bu_avs_get(&avs, argv[i]);
		if (!val) {
		    Tcl_ResetResult(wdbp->wdb_interp);
		    Tcl_AppendResult(wdbp->wdb_interp, "Object ",
				     dp->d_namep, " does not have a ",
				     argv[i], " attribute\n",
				     (char *)NULL);
		    bu_avs_free(&avs);
		    return TCL_ERROR;
		}
		bu_vls_printf(&vls, "\t%s", argv[i]);
		len = (int)strlen(val);
		tabs2 = tabs1 - 1 - len/8;
		for (k = 0; k < tabs2; k++) {
		    bu_vls_putc(&vls, '\t');
		}
		c = val;
		while (*c) {
		    bu_vls_putc(&vls, *c);
		    if (*c == '\n') {
			for (k = 0; k < tabs1; k++) {
			    bu_vls_putc(&vls, '\t');
			}
		    }
		    c++;
		}
		bu_vls_putc(&vls, '\n');
	    }
	}
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return TCL_OK;
    } else {
	struct bu_vls vls;

	Tcl_AppendResult(wdbp->wdb_interp,
			 "ERROR: unrecognized attr subcommand ",
			 argv[1], "\n",
			 (char *)NULL);
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_attr %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

}


int
wdb_attr_tcl(void *clientData,
	     int argc,
	     const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_attr_cmd(wdbp, argc-1, argv+1);
}


int
wdb_nmg_simplify_cmd(struct rt_wdb *wdbp,
		     int argc,
		     const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal nmg_intern;
    struct rt_db_internal new_intern;
    struct model *m;
    struct nmgregion *r;
    struct shell *s;
    struct bu_ptbl faces;
    struct face *fp;
    int do_all=1;
    int do_arb=0;
    int do_tgc=0;
    int do_poly=0;
    const char *new_name;
    const char *nmg_name;
    int success = 0;
    int shell_count=0;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 3 || 4 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_nmg_simplify %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    RT_DB_INTERNAL_INIT(&new_intern);

    if (argc == 4) {
	do_all = 0;
	if (!bu_strncmp(argv[1], "arb", 3))
	    do_arb = 1;
	else if (!bu_strncmp(argv[1], "tgc", 3))
	    do_tgc = 1;
	else if (!bu_strncmp(argv[1], "poly", 4))
	    do_poly = 1;
	else {
	    struct bu_vls vls;

	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "helplib_alias wdb_nmg_simplify %s", argv[0]);
	    Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	new_name = argv[2];
	nmg_name = argv[3];
    } else {
	new_name = argv[1];
	nmg_name = argv[2];
    }

    if (db_lookup(wdbp->dbip, new_name, LOOKUP_QUIET) != RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, new_name, " already exists\n", (char *)NULL);
	return TCL_ERROR;
    }

    dp = db_lookup(wdbp->dbip, nmg_name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, nmg_name, " does not exist\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_db_get_internal(&nmg_intern, dp, wdbp->dbip, bn_mat_identity, &rt_uniresource) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "rt_db_get_internal() error\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (nmg_intern.idb_type != ID_NMG) {
	Tcl_AppendResult(wdbp->wdb_interp, nmg_name, " is not an NMG solid\n", (char *)NULL);
	rt_db_free_internal(&nmg_intern);
	return TCL_ERROR;
    }

    m = (struct model *)nmg_intern.idb_ptr;
    NMG_CK_MODEL(m);

    /* check that all faces are planar */
    nmg_face_tabulate(&faces, &m->magic);
    for (BU_PTBL_FOR (fp, (struct face *), &faces)) {
	if (fp->g.magic_p != NULL && *(fp->g.magic_p) != NMG_FACE_G_PLANE_MAGIC) {
	    bu_ptbl_free(&faces);
	    Tcl_AppendResult(wdbp->wdb_interp, argv[0], " cannot be applied to \"", argv[1],
			     "\" because it has non-planar faces\n", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    bu_ptbl_free(&faces);


    /* count shells */
    for (BU_LIST_FOR (r, nmgregion, &m->r_hd)) {
	for (BU_LIST_FOR (s, shell, &r->s_hd))
	    shell_count++;
    }

    if ((do_arb || do_all) && shell_count == 1) {
	struct rt_arb_internal *arb_int;

	BU_ALLOC(arb_int, struct rt_arb_internal);

	if (nmg_to_arb(m, arb_int)) {
	    new_intern.idb_ptr = (void *)(arb_int);
	    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	    new_intern.idb_type = ID_ARB8;
	    new_intern.idb_meth = &OBJ[ID_ARB8];
	    success = 1;
	} else if (do_arb) {
	    /* see if we can get an arb by simplifying the NMG */

	    r = BU_LIST_FIRST(nmgregion, &m->r_hd);
	    s = BU_LIST_FIRST(shell, &r->s_hd);
	    nmg_shell_coplanar_face_merge(s, &wdbp->wdb_tol, 1);
	    if (!nmg_kill_cracks(s)) {
		(void) nmg_edge_fuse(&m->magic, &wdbp->wdb_tol);
		(void) nmg_edge_g_fuse(&m->magic, &wdbp->wdb_tol);
		(void) nmg_unbreak_region_edges(&r->l.magic);
		if (nmg_to_arb(m, arb_int)) {
		    new_intern.idb_ptr = (void *)(arb_int);
		    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		    new_intern.idb_type = ID_ARB8;
		    new_intern.idb_meth = &OBJ[ID_ARB8];
		    success = 1;
		}
	    }
	    if (!success) {
		rt_db_free_internal(&nmg_intern);
		Tcl_AppendResult(wdbp->wdb_interp, "Failed to construct an ARB equivalent to ",
				 nmg_name, "\n", (char *)NULL);
		return TCL_OK;
	    }
	}
    }

    if ((do_tgc || do_all) && !success && shell_count == 1) {
	struct rt_tgc_internal *tgc_int;

	BU_ALLOC(tgc_int, struct rt_tgc_internal);

	if (nmg_to_tgc(m, tgc_int, &wdbp->wdb_tol)) {
	    new_intern.idb_ptr = (void *)(tgc_int);
	    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	    new_intern.idb_type = ID_TGC;
	    new_intern.idb_meth = &OBJ[ID_TGC];
	    success = 1;
	} else if (do_tgc) {
	    rt_db_free_internal(&nmg_intern);
	    Tcl_AppendResult(wdbp->wdb_interp, "Failed to construct a TGC equivalent to ",
			     nmg_name, "\n", (char *)NULL);
	    return TCL_OK;
	}
    }

    if ((do_poly || do_all) && !success) {
	struct rt_pg_internal *poly_int;

	BU_ALLOC(poly_int, struct rt_pg_internal);

	if (nmg_to_poly(m, poly_int, &wdbp->wdb_tol)) {
	    new_intern.idb_ptr = (void *)(poly_int);
	    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	    new_intern.idb_type = ID_POLY;
	    new_intern.idb_meth = &OBJ[ID_POLY];
	    success = 1;
	} else if (do_poly) {
	    rt_db_free_internal(&nmg_intern);
	    Tcl_AppendResult(wdbp->wdb_interp, nmg_name, " is not a closed surface, cannot make a polysolid\n", (char *)NULL);
	    return TCL_OK;
	}
    }

    if (success) {
	r = BU_LIST_FIRST(nmgregion, &m->r_hd);
	s = BU_LIST_FIRST(shell, &r->s_hd);

	if (BU_LIST_NON_EMPTY(&s->lu_hd))
	    Tcl_AppendResult(wdbp->wdb_interp, "wire loops in ", nmg_name,
			     " have been ignored in conversion\n", (char *)NULL);

	if (BU_LIST_NON_EMPTY(&s->eu_hd))
	    Tcl_AppendResult(wdbp->wdb_interp, "wire edges in ", nmg_name,
			     " have been ignored in conversion\n", (char *)NULL);

	if (s->vu_p)
	    Tcl_AppendResult(wdbp->wdb_interp, "Single vertexuse in shell of ", nmg_name,
			     " has been ignored in conversion\n", (char *)NULL);

	rt_db_free_internal(&nmg_intern);

	dp = db_diradd(wdbp->dbip, new_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&new_intern.idb_type);
	if (dp == RT_DIR_NULL) {
	    Tcl_AppendResult(wdbp->wdb_interp, "Cannot add ", new_name, " to directory\n", (char *)NULL);
	    return TCL_ERROR;
	}

	if (rt_db_put_internal(dp, wdbp->dbip, &new_intern, &rt_uniresource) < 0) {
	    rt_db_free_internal(&new_intern);
	    WDB_TCL_WRITE_ERR_return;
	}
	return TCL_OK;
    }

    Tcl_AppendResult(wdbp->wdb_interp, "simplification to ", argv[1],
		     " is not yet supported\n", (char *)NULL);
    return TCL_ERROR;
}


/**
 * Usage:
 * procname nmg_simplify [arb|tgc|ell|poly] new_solid nmg_solid
 */
static int
wdb_nmg_simplify_tcl(void *clientData,
		     int argc,
		     const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_nmg_simplify_cmd(wdbp, argc-1, argv+1);
}


int
wdb_nmg_collapse_cmd(struct rt_wdb *wdbp,
		     int argc,
		     const char *argv[])
{
    const char *new_name;
    struct model *m;
    struct rt_db_internal intern;
    struct directory *dp;
    struct bu_ptbl faces;
    struct face *fp;
    long count;
    char count_str[32];
    fastf_t tol_coll;
    fastf_t min_angle;

    WDB_TCL_CHECK_READ_ONLY;

    if (argc < 4) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_nmg_collapse %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (strchr(argv[2], '/')) {
	Tcl_AppendResult(wdbp->wdb_interp, "Do not use '/' in solid names: ", argv[2], "\n", (char *)NULL);
	return TCL_ERROR;
    }

    new_name = argv[2];

    if (db_lookup(wdbp->dbip, new_name, LOOKUP_QUIET) != RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, new_name, " already exists\n", (char *)NULL);
	return TCL_ERROR;
    }

    dp = db_lookup(wdbp->dbip, argv[1], LOOKUP_NOISY);
    if (dp == RT_DIR_NULL)
	return TCL_ERROR;

    if (dp->d_flags & RT_DIR_COMB) {
	Tcl_AppendResult(wdbp->wdb_interp, argv[1], " is a combination, only NMG primitives are allowed here\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (matp_t)NULL, &rt_uniresource) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Failed to get internal form of ", argv[1], "!!!!\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (intern.idb_type != ID_NMG) {
	Tcl_AppendResult(wdbp->wdb_interp, argv[1], " is not an NMG solid!!!!\n", (char *)NULL);
	rt_db_free_internal(&intern);
	return TCL_ERROR;
    }

    tol_coll = atof(argv[3]) * wdbp->dbip->dbi_local2base;
    if (tol_coll <= 0.0) {
	Tcl_AppendResult(wdbp->wdb_interp, "tolerance distance too small\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (argc == 5) {
	min_angle = atof(argv[4]);
	if (min_angle < 0.0) {
	    Tcl_AppendResult(wdbp->wdb_interp, "Minimum angle cannot be less than zero\n", (char *)NULL);
	    return TCL_ERROR;
	}
    } else
	min_angle = 0.0;

    m = (struct model *)intern.idb_ptr;
    NMG_CK_MODEL(m);

    /* check that all faces are planar */
    nmg_face_tabulate(&faces, &m->magic);
    for (BU_PTBL_FOR (fp, (struct face *), &faces)) {
	if (fp->g.magic_p != NULL && *(fp->g.magic_p) != NMG_FACE_G_PLANE_MAGIC) {
	    bu_ptbl_free(&faces);
	    Tcl_AppendResult(wdbp->wdb_interp, argv[0], " cannot be applied to \"", argv[1],
			     "\" because it has non-planar faces\n", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    bu_ptbl_free(&faces);

    /* triangulate model */
    nmg_triangulate_model(m, &wdbp->wdb_tol);

    count = nmg_edge_collapse(m, &wdbp->wdb_tol, tol_coll, min_angle);

    dp = db_diradd(wdbp->dbip, new_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, "Cannot add ", new_name, " to directory\n", (char *)NULL);
	rt_db_free_internal(&intern);
	return TCL_ERROR;
    }

    if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
	rt_db_free_internal(&intern);
	WDB_TCL_WRITE_ERR_return;
    }

    rt_db_free_internal(&intern);

    sprintf(count_str, "%ld", count);
    Tcl_AppendResult(wdbp->wdb_interp, count_str, " edges collapsed\n", (char *)NULL);

    return TCL_OK;
}


/**
 * Usage:
 * procname nmg_collapse nmg_solid new_solid maximum_error_distance [minimum_allowed_angle]
 */
static int
wdb_nmg_collapse_tcl(void *clientData,
		     int argc,
		     const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_nmg_collapse_cmd(wdbp, argc-1, argv+1);
}


int
wdb_summary_cmd(struct rt_wdb *wdbp,
		int argc,
		const char *argv[])
{
    const char *cp;
    int flags = 0;
    int bad = 0;

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_summary %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (argc <= 1) {
	wdb_dir_summary(wdbp->dbip, 0);
	return TCL_OK;
    }

    cp = argv[1];
    while (*cp) switch (*cp++) {
	    case 'p':
		flags |= RT_DIR_SOLID;
		break;
	    case 'r':
		flags |= RT_DIR_REGION;
		break;
	    case 'g':
		flags |= RT_DIR_COMB;
		break;
	    default:
		Tcl_AppendResult(wdbp->wdb_interp, "summary:  P R or G are only valid parameters\n",
				 (char *)NULL);
		bad = 1;
		break;
	}

    wdb_dir_summary(wdbp->dbip, flags);
    return bad ? TCL_ERROR : TCL_OK;
}


/**
 * Usage:
 * procname
 */
static int
wdb_summary_tcl(void *clientData,
		int argc,
		const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_summary_cmd(wdbp, argc-1, argv+1);
}


int
wdb_pathlist_cmd(struct rt_wdb *wdbp,
		 int argc,
		 const char *argv[])
{
    if (argc < 2 || 3 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_pathlist %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    pathListNoLeaf = 0;

    if (argc == 3) {
	if (BU_STR_EQUAL(argv[1], "-noleaf"))
	    pathListNoLeaf = 1;

	++argv;
	--argc;
    }

    if (db_walk_tree(wdbp->dbip, argc-1, (const char **)argv+1, 1,
		     &wdbp->wdb_initial_tree_state,
		     0, 0, wdb_pathlist_leaf_func, (void *)wdbp->wdb_interp) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "wdb_pathlist: db_walk_tree() error", (char *)NULL);
	return TCL_ERROR;
    }

    return TCL_OK;
}


/**
 * Usage:
 * procname
 */
static int
wdb_pathlist_tcl(void *clientData,
		 int argc,
		 const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_pathlist_cmd(wdbp, argc-1, argv+1);
}


int
wdb_bot_smooth_cmd(struct rt_wdb *wdbp,
		   int argc,
		   const char *argv[])
{
    const char *new_bot_name;
    const char *old_bot_name;
    struct directory *dp_old, *dp_new;
    struct rt_bot_internal *old_bot;
    struct rt_db_internal intern;
    fastf_t tolerance_angle=180.0;
    int arg_index=1;
    int id;

    /* check that we are using a version 5 database */
    if (db_version(wdbp->dbip) < 5) {
	Tcl_AppendResult(wdbp->wdb_interp, "This is an older database version.\n",
			 "It does not support BOT surface normals.\n",
			 "Use \"dbupgrade\" to upgrade this database to the current version.\n",
			 (char *)NULL);
	return TCL_ERROR;
    }

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_bot_smooth %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    while (*argv[arg_index] == '-') {
	/* this is an option */
	if (BU_STR_EQUAL(argv[arg_index], "-t")) {
	    arg_index++;
	    tolerance_angle = atof(argv[arg_index]);
	} else {
	    struct bu_vls vls;

	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "helplib_alias wdb_bot_smooth %s", argv[0]);
	    Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}
	arg_index++;
    }

    if (arg_index >= argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_bot_smooth %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    new_bot_name = argv[arg_index++];
    old_bot_name = argv[arg_index];

    if ((dp_old=db_lookup(wdbp->dbip, old_bot_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
	Tcl_AppendResult(wdbp->wdb_interp, old_bot_name, " does not exist!!\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (!BU_STR_EQUAL(old_bot_name, new_bot_name)) {

	if ((dp_new=db_lookup(wdbp->dbip, new_bot_name, LOOKUP_QUIET)) != RT_DIR_NULL) {
	    Tcl_AppendResult(wdbp->wdb_interp, new_bot_name, " already exists!!\n", (char *)NULL);
	    return TCL_ERROR;
	}
    } else {
	dp_new = dp_old;
    }

    if ((id=rt_db_get_internal(&intern, dp_old, wdbp->dbip, NULL, wdbp->wdb_resp)) < 0) {
	Tcl_AppendResult(wdbp->wdb_interp, "Failed to get internal form of ", old_bot_name, "\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (id != ID_BOT) {
	Tcl_AppendResult(wdbp->wdb_interp, old_bot_name, " is not a BOT primitive\n", (char *)NULL);
	rt_db_free_internal(&intern);
	return TCL_ERROR;
    }

    old_bot = (struct rt_bot_internal *)intern.idb_ptr;
    RT_BOT_CK_MAGIC(old_bot);

    if (rt_bot_smooth(old_bot, old_bot_name, wdbp->dbip, tolerance_angle*DEG2RAD)) {
	Tcl_AppendResult(wdbp->wdb_interp, "Failed to smooth ", old_bot_name, "\n", (char *)NULL);
	rt_db_free_internal(&intern);
	return TCL_ERROR;
    }

    if (dp_new == RT_DIR_NULL) {
	dp_new=db_diradd(wdbp->dbip, new_bot_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
	if (dp_new == RT_DIR_NULL) {
	    rt_db_free_internal(&intern);
	    Tcl_AppendResult(wdbp->wdb_interp, "Cannot add ", new_bot_name, " to directory\n", (char *)NULL);
	    return TCL_ERROR;
	}
    }

    if (rt_db_put_internal(dp_new, wdbp->dbip, &intern, wdbp->wdb_resp) < 0) {
	rt_db_free_internal(&intern);
	Tcl_AppendResult(wdbp->wdb_interp, "Database write error, aborting.\n", (char *)NULL);
	return TCL_ERROR;
    }

    rt_db_free_internal(&intern);

    return TCL_OK;
}


static int
wdb_bot_smooth_tcl(void *clientData,
		   int argc,
		   const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_bot_smooth_cmd(wdbp, argc-1, argv+1);
}


int
wdb_bo_cmd(struct rt_wdb *wdbp,
	   int argc,
	   const char *argv[])
{
    int c;
    struct bu_vls vls;
    unsigned int minor_type=0;
    const char *obj_name;
    const char *file_name;
    int input_mode=0;
    int output_mode=0;
    struct rt_binunif_internal *bip;
    struct rt_db_internal intern;
    struct directory *dp;
    const char *cname;

    /* check that we are using a version 5 database */
    if (db_version(wdbp->dbip) < 5) {
	Tcl_AppendResult(wdbp->wdb_interp, "This is an older database version.\n",
			 "It does not support binary objects.\n",
			 "Use \"dbupgrade\" to upgrade this database to the current version.\n",
			 (char *)NULL);
	return TCL_ERROR;
    }

    bu_optind = 1;		/* re-init bu_getopt() */
    bu_opterr = 0;          /* suppress bu_getopt()'s error message */
    while ((c=bu_getopt(argc, (char * const *)argv, "iou:")) != -1) {
	switch (c) {
	    case 'i':
		input_mode = 1;
		break;
	    case 'o':
		output_mode = 1;
		break;
	    default:
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "Unrecognized option - %c", c);
		Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);
		return TCL_ERROR;

	}
    }

    cname = argv[0];

    if (input_mode + output_mode != 1) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias bo %s", cname);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    argc -= bu_optind;
    argv += bu_optind;

    if ((input_mode && argc != 4) || (output_mode && argc != 2)) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias bo %s", cname);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }


    if (input_mode) {
	if (argv[0][0] == 'u') {

	    if (argv[1][1] != '\0') {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "Unrecognized minor type: %s", argv[1]);
		Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);
		return TCL_ERROR;
	    }

	    switch ((int)argv[1][0]) {
		case 'f':
		    minor_type = DB5_MINORTYPE_BINU_FLOAT;
		    break;
		case 'd':
		    minor_type = DB5_MINORTYPE_BINU_DOUBLE;
		    break;
		case 'c':
		    minor_type = DB5_MINORTYPE_BINU_8BITINT;
		    break;
		case 's':
		    minor_type = DB5_MINORTYPE_BINU_16BITINT;
		    break;
		case 'i':
		    minor_type = DB5_MINORTYPE_BINU_32BITINT;
		    break;
		case 'l':
		    minor_type = DB5_MINORTYPE_BINU_64BITINT;
		    break;
		case 'C':
		    minor_type = DB5_MINORTYPE_BINU_8BITINT_U;
		    break;
		case 'S':
		    minor_type = DB5_MINORTYPE_BINU_16BITINT_U;
		    break;
		case 'I':
		    minor_type = DB5_MINORTYPE_BINU_32BITINT_U;
		    break;
		case 'L':
		    minor_type = DB5_MINORTYPE_BINU_64BITINT_U;
		    break;
		default:
		    bu_vls_init(&vls);
		    bu_vls_printf(&vls, "Unrecognized minor type: %s", argv[1]);
		    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
		    bu_vls_free(&vls);
		    return TCL_ERROR;
	    }
	} else {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "Unrecognized major type: %s", argv[0]);
	    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	/* skip past major_type and minor_type */
	argc -= 2;
	argv += 2;

	if (minor_type == 0) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "helplib_alias bo %s", cname);
	    Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	obj_name = *argv;
	if (db_lookup(wdbp->dbip, obj_name, LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "Object %s already exists", obj_name);
	    Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	argc--;
	argv++;

	file_name = *argv;

	/* make a binunif of the entire file */
	if (rt_mk_binunif (wdbp, obj_name, file_name, minor_type, (size_t)-1)) {
	    Tcl_AppendResult(wdbp->wdb_interp, "Error creating ", obj_name,
			     (char *)NULL);
	    return TCL_ERROR;
	}

	return TCL_OK;

    } else if (output_mode) {
	FILE *fp;

	file_name = *argv;

	argc--;
	argv++;

	obj_name = *argv;

	dp = db_lookup(wdbp->dbip, obj_name, LOOKUP_NOISY);
	if (dp == RT_DIR_NULL) {
	    return TCL_ERROR;
	}
	if (!(dp->d_major_type & DB5_MAJORTYPE_BINARY_MASK)) {
	    Tcl_AppendResult(wdbp->wdb_interp, obj_name, " is not a binary object", (char *)NULL);
	    return TCL_ERROR;
	}

	if (dp->d_major_type != DB5_MAJORTYPE_BINARY_UNIF) {
	    Tcl_AppendResult(wdbp->wdb_interp, "source must be a uniform binary object",
			     (char *)NULL);
	    return TCL_ERROR;
	}

	fp = fopen(file_name, "w+b");
	if (fp == NULL) {
	    Tcl_AppendResult(wdbp->wdb_interp, "Error: cannot open file ", file_name,
			     " for writing", (char *)NULL);
	    return TCL_ERROR;
	}

	if (rt_db_get_internal(&intern, dp, wdbp->dbip, NULL,
			       &rt_uniresource) < 0) {
	    Tcl_AppendResult(wdbp->wdb_interp, "Error reading ", dp->d_namep,
			     " from database", (char *)NULL);
	    fclose(fp);
	    return TCL_ERROR;
	}

	RT_CK_DB_INTERNAL(&intern);

	bip = (struct rt_binunif_internal *)intern.idb_ptr;
	if (bip->count < 1) {
	    Tcl_AppendResult(wdbp->wdb_interp, obj_name, " has no contents", (char *)NULL);
	    fclose(fp);
	    rt_db_free_internal(&intern);
	    return TCL_ERROR;
	}

	if (fwrite(bip->u.int8, bip->count * db5_type_sizeof_h_binu(bip->type),
		   1, fp) != 1) {
	    Tcl_AppendResult(wdbp->wdb_interp, "Error writing contents to file",
			     (char *)NULL);
	    fclose(fp);
	    rt_db_free_internal(&intern);
	    return TCL_ERROR;
	}

	fclose(fp);
	rt_db_free_internal(&intern);
	return TCL_OK;
    } else {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias bo %s", cname);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* should never get here */
    /* return TCL_ERROR; */
}


/**
 * Usage:
 * procname bo args
 */
static int
wdb_bo_tcl(void *clientData,
	   int argc,
	   const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_bo_cmd(wdbp, argc-1, argv+1);
}


int wdb_bot_face_sort_cmd(struct rt_wdb *wdbp,
			  int argc,
			  const char *argv[])
{
    int i;
    int tris_per_piece=0;
    struct bu_vls vls;
    int warnings=0;

    if (argc < 3) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_bot_face_sort %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    tris_per_piece = atoi(argv[1]);
    if (tris_per_piece < 1) {
	Tcl_AppendResult(wdbp->wdb_interp, "Illegal value for triangle per piece (",
			 argv[1],
			 ")\n",
			 (char *)NULL);
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_bot_face_sort %s", argv[0]);
	Tcl_Eval(wdbp->wdb_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_init(&vls);
    for (i = 2; i < argc; i++) {
	struct directory *dp;
	struct rt_db_internal intern;
	struct rt_bot_internal *bot;
	int id;

	dp = db_lookup(wdbp->dbip, argv[i], LOOKUP_NOISY);
	if (dp == RT_DIR_NULL) {
	    continue;
	}

	id=rt_db_get_internal(&intern, dp, wdbp->dbip, bn_mat_identity, wdbp->wdb_resp);
	if (id < 0) {
	    bu_vls_printf(&vls,
			  "Failed to get internal form of %s, not sorting this one\n",
			  dp->d_namep);
	    warnings++;
	    continue;
	}

	if (id != ID_BOT) {
	    rt_db_free_internal(&intern);
	    bu_vls_printf(&vls,
			  "%s is not a BOT primitive, skipped\n",
			  dp->d_namep);
	    warnings++;
	    continue;
	}

	bot = (struct rt_bot_internal *)intern.idb_ptr;
	RT_BOT_CK_MAGIC(bot);

	bu_log("processing %s (%zu triangles)\n", dp->d_namep, bot->num_faces);
	while (Tcl_DoOneEvent(TCL_DONT_WAIT | TCL_FILE_EVENTS));
	if (rt_bot_sort_faces(bot, tris_per_piece)) {
	    rt_db_free_internal(&intern);
	    bu_vls_printf(&vls,
			  "Face sort failed for %s, this BOT not sorted\n",
			  dp->d_namep);
	    warnings++;
	    continue;
	}

	if (rt_db_put_internal(dp, wdbp->dbip, &intern, wdbp->wdb_resp)) {
	    if (warnings) {
		Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls),
				 (char *)NULL);
	    }
	    Tcl_AppendResult(wdbp->wdb_interp, "Failed to write sorted BOT (",
			     dp->d_namep,
			     ") to database!!! (This is very bad)\n");
	    rt_db_free_internal(&intern);
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}
    }

    if (warnings) {
	Tcl_AppendResult(wdbp->wdb_interp, bu_vls_addr(&vls), (char *)NULL);
    }
    bu_vls_free(&vls);
    return TCL_OK;
}


/**
 * Usage:
 * procname
 */
static int
wdb_bot_face_sort_tcl(void *clientData,
		      int argc,
		      const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_bot_face_sort_cmd(wdbp, argc-1, argv+1);
}


/**
 * Usage:
 * importFg4Section name sdata
 */
static int
wdb_importFg4Section_tcl(void *clientData,
			 int argc,
			 const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    return wdb_importFg4Section_cmd(wdbp, argc-1, argv+1);
}


static struct bu_cmdtab wdb_newcmds[] = {
    {"arced",		(int (*)(void *, int, const char **))ged_arced},
    {"cc",		(int (*)(void *, int, const char **))ged_cc},
    {"color",		(int (*)(void *, int, const char **))ged_color},
    {"comb_color",	(int (*)(void *, int, const char **))ged_comb_color},
    {"edcomb",		(int (*)(void *, int, const char **))ged_edcomb},
    {"edit",		(int (*)(void *, int, const char **))ged_edit},
    {"edmater",		(int (*)(void *, int, const char **))ged_edmater},
    {"get",		(int (*)(void *, int, const char **))ged_get},
    {"item",		(int (*)(void *, int, const char **))ged_item},
    {"log",		(int (*)(void *, int, const char **))ged_log},
    {"make",		(int (*)(void *, int, const char **))ged_make},
    {"make_name",	(int (*)(void *, int, const char **))ged_make_name},
    {"mater",		(int (*)(void *, int, const char **))ged_mater},
    {"mirror",		(int (*)(void *, int, const char **))ged_mirror},
    {"nirt",		(int (*)(void *, int, const char **))ged_nirt},
    {"ocenter",		(int (*)(void *, int, const char **))ged_ocenter},
    {"orotate",		(int (*)(void *, int, const char **))ged_orotate},
    {"oscale",		(int (*)(void *, int, const char **))ged_oscale},
    {"otranslate",	(int (*)(void *, int, const char **))ged_otranslate},
    {"put",		(int (*)(void *, int, const char **))ged_put},
    {"rmater",		(int (*)(void *, int, const char **))ged_rmater},
    {"shader",		(int (*)(void *, int, const char **))ged_shader},
    {"wmater",		(int (*)(void *, int, const char **))ged_wmater},
    {(const char *)NULL, BU_CMD_NULL}
};


static int
wdb_newcmds_tcl(void *clientData,
		int argc,
		const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
    struct ged ged;
    struct bu_vls vls;
    int ret = GED_ERROR;

    /*XXX Eventually the clientData will be a "struct ged".
     * In the meantime ...
     */
    GED_INIT(&ged, wdbp);

    if (bu_cmd(wdb_newcmds, argc-1, argv+1, 0, &ged, &ret) == BRLCAD_ERROR) {
	/* Command not found. */
	bu_vls_trunc(ged.ged_result_str, 0);
	bu_vls_printf(ged.ged_result_str, "%s not found", argv[1]);

	ret = GED_ERROR;
    }

    bu_vls_init(&vls);

    if (ret == GED_HELP)
	bu_vls_strcat(&vls, "1 ");
    else
	bu_vls_strcat(&vls, "0 ");

    if (strchr(bu_vls_addr(ged.ged_result_str), ' '))
	bu_vls_printf(&vls, "\"%s\"", bu_vls_addr(ged.ged_result_str));
    else
	bu_vls_vlscat(&vls, ged.ged_result_str);

    Tcl_SetResult(wdbp->wdb_interp, bu_vls_addr(&vls), TCL_VOLATILE);
    bu_vls_free(&vls);

    /* release any allocated memory */
    ged_free(&ged);

    if (ret == GED_ERROR)
	return TCL_ERROR;

    return TCL_OK;
}


static struct bu_cmdtab wdb_cmds[] = {
    {"adjust",		wdb_adjust_tcl},
    {"arced",		wdb_newcmds_tcl},
    {"attr",		wdb_attr_tcl},
    {"bo",		wdb_bo_tcl},
    {"bot_face_sort", 	wdb_bot_face_sort_tcl},
    {"bot_decimate", 	wdb_bot_decimate_tcl},
    {"c",		wdb_comb_std_tcl},
    {"cat",		wdb_cat_tcl},
    {"comb",		wdb_comb_tcl},
    {"comb_color",	wdb_newcmds_tcl},
    {"concat",		wdb_concat_tcl},
    {"copyeval",	wdb_copyeval_tcl},
    {"cp",		wdb_copy_tcl},
    {"db",		wdb_stub_tcl},
    {"dbip",		wdb_dbip_tcl},
    {"dump",		wdb_dump_tcl},
    {"dup",		wdb_dup_tcl},
    {"edcomb",		wdb_newcmds_tcl},
    {"edit",		wdb_newcmds_tcl},
    {"edmater",		wdb_newcmds_tcl},
    {"expand",		wdb_expand_tcl},
    {"facetize",	wdb_facetize_tcl},
    {"find",		wdb_find_tcl},
    {"form",		wdb_form_tcl},
    {"g",		wdb_group_tcl},
    {"get",		wdb_get_tcl},
    {"get_type",	wdb_get_type_tcl},
    {"hide",		wdb_hide_tcl},
    {"i",		wdb_instance_tcl},
    {"importFg4Section", wdb_importFg4Section_tcl},
    {"item",		wdb_newcmds_tcl},
    {"keep",		wdb_keep_tcl},
    {"kill",		wdb_kill_tcl},
    {"killall",		wdb_killall_tcl},
    {"killtree",	wdb_killtree_tcl},
    {"l",		wdb_list_tcl},
    {"listeval",	wdb_pathsum_tcl},
    {"log",		wdb_newcmds_tcl},
    {"ls",		wdb_ls_tcl},
    {"lt",		wdb_lt_tcl},
    {"make",		wdb_newcmds_tcl},
    {"make_bb",		wdb_make_bb_tcl},
    {"make_name",	wdb_newcmds_tcl},
    {"match",		wdb_match_tcl},
    {"mater",		wdb_newcmds_tcl},
    {"mirror",		wdb_newcmds_tcl},
    {"move_arb_edge",	wdb_move_arb_edge_tcl},
    {"move_arb_face",	wdb_move_arb_face_tcl},
    {"mv",		wdb_move_tcl},
    {"mvall",		wdb_move_all_tcl},
    {"nirt",		wdb_newcmds_tcl},
    {"nmg_collapse", 	wdb_nmg_collapse_tcl},
    {"nmg_simplify", 	wdb_nmg_simplify_tcl},
    {"observer",	wdb_observer_tcl},
    {"ocenter",		wdb_newcmds_tcl},
    {"orotate",		wdb_newcmds_tcl},
    {"oscale",		wdb_newcmds_tcl},
    {"otranslate",	wdb_newcmds_tcl},
    {"open",		wdb_reopen_tcl},
    {"pathlist",	wdb_pathlist_tcl},
    {"paths",		wdb_pathsum_tcl},
    {"prcolor",		wdb_prcolor_tcl},
    {"push",		wdb_push_tcl},
    {"put",		wdb_newcmds_tcl},
    {"r",		wdb_region_tcl},
    {"rm",		wdb_remove_tcl},
    {"rmap",		wdb_rmap_tcl},
    {"rmater",		wdb_newcmds_tcl},
    {"rotate_arb_face",	wdb_rotate_arb_face_tcl},
    {"rt_gettrees",	wdb_rt_gettrees_tcl},
    {"shader",		wdb_newcmds_tcl},
    {"shells",		wdb_shells_tcl},
    {"showmats",	wdb_showmats_tcl},
    {"bot_smooth",	wdb_bot_smooth_tcl},
    {"summary",		wdb_summary_tcl},
    {"title",		wdb_title_tcl},
    {"tol",		wdb_tol_tcl},
    {"tops",		wdb_tops_tcl},
    {"track",		wdb_track_tcl},
    {"unhide",		wdb_unhide_tcl},
    {"units",		wdb_units_tcl},
    {"version",		wdb_version_tcl},
    {"whatid",		wdb_whatid_tcl},
    {"whichair",	wdb_which_tcl},
    {"whichid",		wdb_which_tcl},
    {"wmater",		wdb_newcmds_tcl},
    {"xpush",		wdb_xpush_tcl},
    {(const char *)NULL, BU_CMD_NULL}
};


/* used to suppress bu_log() output */
static int
do_nothing(void *UNUSED(nada1), void *UNUSED(nada2))
{
    return 0;
}


/**
 *@brief
 * Generic interface for database commands.
 *
 * @par Usage:
 * procname cmd ?args?
 *
 * @return result of wdb command.
 */
static int
wdb_cmd(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
    struct ged ged;
    struct bu_hook_list save_hook_list;
    int ret;

    /* look for the new libged commands before trying one of the old ones */
    GED_INIT(&ged, wdbp);

    bu_hook_list_init(&save_hook_list);
    bu_log_hook_save_all(&save_hook_list);

    /* suppress bu_log output because we don't care if the command
     * exists in wdb_newcmds since it might be in wdb_cmds.  this
     * prevents bu_cmd() from blathering.
     */
    bu_log_add_hook(do_nothing, NULL);

    if (bu_cmd(wdb_newcmds, argc-1, argv+1, 0, (ClientData)&ged, &ret) == BRLCAD_OK) {
	Tcl_SetResult(interp, bu_vls_addr(ged.ged_result_str), TCL_VOLATILE);
	ged_free(&ged);
	/* unsuppress bu_log output */
	bu_log_hook_restore_all(&save_hook_list);
	return ret;
    }

    /* unsuppress bu_log output */
    bu_log_hook_restore_all(&save_hook_list);

    /* release any allocated memory */
    ged_free(&ged);

    /* not a new command -- look for the command in the old command table */
    bu_cmd(wdb_cmds, argc, (const char **)argv, 1, clientData, &ret);
    return ret;
}


/**
 * @brief
 * Called by Tcl when the object is destroyed.
 */
void
wdb_deleteProc(ClientData clientData)
{
    struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

    /* free observers */
    bu_observer_free(&wdbp->wdb_observers);

    /* notify drawable geometry objects of the impending close */
    dgo_impending_wdb_close(wdbp);

    /* close up shop */
    wdb_close(wdbp);
}


/**
 * @brief
 * Create a command named "oname" in "interp" using "wdbp" as its state.
 *
 */
int
wdb_create_cmd(struct rt_wdb *wdbp,	/* pointer to object */
	       const char *oname)	/* object name */
{
    if (wdbp == RT_WDB_NULL) {
	return TCL_ERROR;
    }

    /* Instantiate the newprocname, with clientData of wdbp */
    /* Beware, returns a "token", not TCL_OK. */
    (void)Tcl_CreateCommand(wdbp->wdb_interp, oname, (Tcl_CmdProc *)wdb_cmd,
			    (ClientData)wdbp, wdb_deleteProc);

    /* Return new function name as result */
    Tcl_AppendResult(wdbp->wdb_interp, oname, (char *)NULL);

    return TCL_OK;
}


/**
 * @brief
 * Create an command/object named "oname" in "interp" using "wdbp" as
 * its state.  It is presumed that the wdbp has already been opened.
 */
int
wdb_init_obj(Tcl_Interp *interp,
	     struct rt_wdb *wdbp,	/* pointer to object */
	     const char *oname)	/* object name */
{
    if (wdbp == RT_WDB_NULL) {
	Tcl_AppendResult(interp, "wdb_open ", oname, " failed (wdb_init_obj)", NULL);
	return TCL_ERROR;
    }

    /* initialize rt_wdb */
    bu_vls_init(&wdbp->wdb_name);
    bu_vls_strcpy(&wdbp->wdb_name, oname);

    BU_LIST_INIT(&wdbp->wdb_observers.l);
    wdbp->wdb_interp = interp;

    /* append to list of rt_wdb's */
    BU_LIST_APPEND(&RTG.rtg_headwdb.l, &wdbp->l);

    return TCL_OK;
}


/**
 *@brief
 * A TCL interface to wdb_fopen() and wdb_dbopen().
 *
 * @par Implicit return -
 * Creates a new TCL proc which responds to get/put/etc. arguments
 * when invoked.  clientData of that proc will be rt_wdb pointer
 * for this instance of the database.
 * Easily allows keeping track of multiple databases.
 *
 * @return wdb pointer, for more traditional C-style interfacing.
 *
 * @par Example -
 * set wdbp [wdb_open .inmem inmem $dbip]
 *@n	.inmem get box.s
 *@n	.inmem close
 *
 *@n wdb_open db file "bob.g"
 *@n db get white.r
 *@n db close
 */
static int
wdb_open_tcl(ClientData UNUSED(clientData),
	     Tcl_Interp *interp,
	     int argc,
	     const char *argv[])
{
    struct rt_wdb *wdbp;
    int ret;

    if (argc == 1) {
	/* get list of database objects */
	for (BU_LIST_FOR (wdbp, rt_wdb, &RTG.rtg_headwdb.l))
	    Tcl_AppendResult(interp, bu_vls_addr(&wdbp->wdb_name), " ", (char *)NULL);

	return TCL_OK;
    }

    if (argc < 3 || 4 < argc) {
	Tcl_AppendResult(interp, "\
Usage: wdb_open\n\
       wdb_open newprocname file filename\n\
       wdb_open newprocname disk $dbip\n\
       wdb_open newprocname disk_append $dbip\n\
       wdb_open newprocname inmem $dbip\n\
       wdb_open newprocname inmem_append $dbip\n\
       wdb_open newprocname db filename\n\
       wdb_open newprocname filename\n",
			 NULL);
	return TCL_ERROR;
    }

    /* Delete previous proc (if any) to release all that memory, first */
    (void)Tcl_DeleteCommand(interp, argv[1]);

    if (argc == 3 || BU_STR_EQUAL(argv[2], "db")) {
	struct db_i *dbip;
	int i;

	if (argc == 3)
	    i = 2;
	else
	    i = 3;

	if ((dbip = wdb_prep_dbip(argv[i])) == DBI_NULL)
	    return TCL_ERROR;
	RT_CK_DBI(dbip);

	wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    } else if (BU_STR_EQUAL(argv[2], "file")) {
	wdbp = wdb_fopen(argv[3]);
    } else {
	struct db_i *dbip;

	if (wdb_decode_dbip(argv[3], &dbip) != TCL_OK)
	    return TCL_ERROR;

	if (BU_STR_EQUAL(argv[2], "disk"))
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
	else if (BU_STR_EQUAL(argv[2], "disk_append"))
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK_APPEND_ONLY);
	else if (BU_STR_EQUAL(argv[2], "inmem"))
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
	else if (BU_STR_EQUAL(argv[2], "inmem_append"))
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM_APPEND_ONLY);
	else {
	    Tcl_AppendResult(interp, "wdb_open ", argv[2],
			     " target type not recognized", NULL);
	    return TCL_ERROR;
	}
    }

    if ((ret = wdb_init_obj(interp, wdbp, argv[1])) != TCL_OK)
	return ret;

    return wdb_create_cmd(wdbp, argv[1]);
}


/**
 * @brief create the Tcl command for wdb_open
 *
 */
int
Wdb_Init(Tcl_Interp *interp)
{
    (void)Tcl_CreateCommand(interp, (const char *)"wdb_open", wdb_open_tcl,
			    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    return TCL_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

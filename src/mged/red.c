/*                           R E D . C
 * BRL-CAD
 *
 * Copyright (c) 1992-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mged/red.c
 *
 * These routines allow editing of a combination using the text editor
 * of the users choice.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "db.h"

#include "./mged.h"
#include "./sedit.h"

static char red_tmpfil[MAXPATHLEN] = {0};
static char red_tmpcomb[17];
static char *red_tmpcomb_init = "red_tmp.aXXXXXX";
static char delims[] = " \t/";	/* allowable delimiters */


HIDDEN char *
find_keyword(int i, char *line, char *word)
{
    char *ptr1;
    char *ptr2;
    int j;

    /* find the keyword */
    ptr1 = strstr(&line[i], word);
    if (!ptr1)
	return (char *)NULL;

    /* find the '=' */
    ptr2 = strchr(ptr1, '=');
    if (!ptr2)
	return (char *)NULL;

    /* skip any white space before the value */
    while (isspace(*(++ptr2)));

    /* eliminate trailing white space */
    j = strlen(line);
    while (isspace(line[--j]));
    line[j+1] = '\0';

    /* return pointer to the value */
    return ptr2;
}


struct line_list{
    struct bu_list l;
    char *line;
};


struct line_list HeadLines;

HIDDEN int
count_nodes(char *line)
{
    char *ptr;
    char *name;
    char relation;
    int node_count=0;

    /* sanity */
    if (line == NULL)
	return 0;

    ptr = strtok(line, delims);

    while (ptr) {
	/* First non-white is the relation operator */
	relation = (*ptr);

	if (relation != '+' && relation != 'u' && relation != '-') {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, " %c is not a legal operator\n", relation);
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);
	    return -1;
	}

	/* Next must be the member name */
	name = strtok((char *)NULL, delims);

	if (name == NULL) {
	    Tcl_AppendResult(interp, " operand name missing\n", (char *)NULL);
	    return -1;
	}

	ptr = strtok((char *)NULL, delims);
	/*
	 * If this token is not a boolean operator, then it must be the start
	 * of a matrix which we will skip.
	 */
	if (ptr && !((*ptr == 'u' || *ptr == '-' || *ptr=='+') &&
		     *(ptr+1) == '\0')) {
	    int k;

	    /* skip past matrix, k=1 because we already have the first value */
	    for (k=1; k<16; k++) {
		ptr = strtok((char *)NULL, delims);
		if (!ptr) {
		    Tcl_AppendResult(interp, "expecting a matrix\n", (char *)NULL);
		    return -1;
		}
	    }

	    /* get the next relational operator on the current line */
	    ptr = strtok((char *)NULL, delims);
	}

	node_count++;
    }

    return node_count;
}


HIDDEN int
make_tree(struct rt_comb_internal *comb, struct directory *dp, size_t node_count, char *old_name, char *new_name, struct rt_tree_array *rt_tree_array, int tree_index)
{
    struct rt_db_internal intern;
    union tree *final_tree;

    if (tree_index)
	final_tree = (union tree *)db_mkgift_tree(rt_tree_array, node_count, &rt_uniresource);
    else
	final_tree = (union tree *)NULL;

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_COMBINATION;
    intern.idb_meth = &rt_functab[ID_COMBINATION];
    intern.idb_ptr = (genptr_t)comb;
    comb->tree = final_tree;

    if (!BU_STR_EQUAL(new_name, old_name)) {
	int flags;

	if (comb->region_flag)
	    flags = RT_DIR_COMB | RT_DIR_REGION;
	else
	    flags = RT_DIR_COMB;

	if (dp != RT_DIR_NULL) {
	    if (db_delete(dbip, dp) || db_dirdelete(dbip, dp)) {
		Tcl_AppendResult(interp, "ERROR: Unable to delete directory entry for ",
				 old_name, "\n", (char *)NULL);
		intern.idb_ptr->idb_meth->ft_ifree(&intern, &rt_uniresource);
		return 1;
	    }
	}

	if ((dp=db_diradd(dbip, new_name, -1L, 0, flags, (genptr_t)&intern.idb_type)) == RT_DIR_NULL) {
	    Tcl_AppendResult(interp, "Cannot add ", new_name,
			     " to directory, no changes made\n", (char *)NULL);
	    intern.idb_ptr->idb_meth->ft_ifree(&intern, &rt_uniresource);
	    return 1;
	}
    } else if (dp == RT_DIR_NULL) {
	int flags;

	if (comb->region_flag)
	    flags = RT_DIR_COMB | RT_DIR_REGION;
	else
	    flags = RT_DIR_COMB;

	if ((dp=db_diradd(dbip, new_name, -1L, 0, flags, (genptr_t)&intern.idb_type)) == RT_DIR_NULL) {
	    Tcl_AppendResult(interp, "Cannot add ", new_name,
			     " to directory, no changes made\n", (char *)NULL);
	    intern.idb_ptr->idb_meth->ft_ifree(&intern, &rt_uniresource);
	    return 1;
	}
    } else {
	if (comb->region_flag)
	    dp->d_flags |= RT_DIR_REGION;
	else
	    dp->d_flags &= ~RT_DIR_REGION;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	Tcl_AppendResult(interp, "ERROR: Unable to write new combination into database.\n", (char *)NULL);
	return 1;
    }

    return 0;
}


void
mktemp_comb(char *str)
{
    /* Make a temporary name for a combination
       a template name is expected as in "mk_temp()" with
       5 trailing X's */

    int counter, done;
    char *ptr;


    if (dbip == DBI_NULL)
	return;

    /* Set "ptr" to start of X's */

    ptr = str;
    while (*ptr != '\0')
	ptr++;

    while (*(--ptr) == 'X');
    ptr++;


    counter = 1;
    done = 0;
    while (!done && counter < 99999) {
	sprintf(ptr, "%d", counter);
	if (db_lookup(dbip, str, LOOKUP_QUIET) == RT_DIR_NULL)
	    done = 1;
	else
	    counter++;
    }
}


int save_comb(struct directory *dpold)
{
    /* Save a combination under a temporory name */

    struct directory *dp;
    struct rt_db_internal intern;

    if (dbip == DBI_NULL)
	return 0;

    /* Make a new name */
    mktemp_comb(red_tmpcomb);

    if (rt_db_get_internal(&intern, dpold, dbip, (fastf_t *)NULL, &rt_uniresource) < 0)
	TCL_READ_ERR_return;

    if ((dp=db_diradd(dbip, red_tmpcomb, -1L, 0, dpold->d_flags, (genptr_t)&intern.idb_type)) == RT_DIR_NULL) {
	Tcl_AppendResult(interp, "Cannot save copy of ", dpold->d_namep,
			 ", no changes made\n", (char *)NULL);
	return 1;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	Tcl_AppendResult(interp, "Cannot save copy of ", dpold->d_namep,
			 ", no changes made\n", (char *)NULL);
	return 1;
    }

    return 0;
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

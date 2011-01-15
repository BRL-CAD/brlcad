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
/** @file red.c
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


HIDDEN void
print_matrix(FILE *fp, matp_t matrix)
{
    int k;
    char buf[64];
    fastf_t tmp;

    if (!matrix)
	return;

    for (k=0; k<16; k++) {
	sprintf(buf, "%g", matrix[k]);
	tmp = atof(buf);
	if (tmp == matrix[k])
	    fprintf(fp, " %g", matrix[k]);
	else
	    fprintf(fp, " %.12e", matrix[k]);
	if ((k&3)==3) fputc(' ', fp);
    }
}


HIDDEN void
vls_print_matrix(struct bu_vls *vls, matp_t matrix)
{
    int k;
    char buf[64];
    fastf_t tmp;

    if (!matrix)
	return;

    if (bn_mat_is_identity(matrix))
	return;

    for (k=0; k<16; k++) {
	sprintf(buf, "%g", matrix[k]);
	tmp = atof(buf);
	if (tmp == matrix[k])
	    bu_vls_printf(vls, " %g", matrix[k]);
	else
	    bu_vls_printf(vls, " %.12e", matrix[k]);
	if ((k&3)==3) bu_vls_printf(vls, " ");
    }
}


void
put_rgb_into_comb(struct rt_comb_internal *comb, char *str)
{
    int r, g, b;

    if (sscanf(str, "%d%d%d", &r, &g, &b) != 3) {
	comb->rgb_valid = 0;
	return;
    }

    /* clamp the RGB values to [0, 255] */
    if (r < 0)
	r = 0;
    else if (r > 255)
	r = 255;

    if (g < 0)
	g = 0;
    else if (g > 255)
	g = 255;

    if (b < 0)
	b = 0;
    else if (b > 255)
	b = 255;

    comb->rgb[0] = (unsigned char)r;
    comb->rgb[1] = (unsigned char)g;
    comb->rgb[2] = (unsigned char)b;
    comb->rgb_valid = 1;
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

    RT_INIT_DB_INTERNAL(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_COMBINATION;
    intern.idb_meth = &rt_functab[ID_COMBINATION];
    intern.idb_ptr = (genptr_t)comb;
    comb->tree = final_tree;

    if (!BU_STR_EQUAL(new_name, old_name)) {
	int flags;

	if (comb->region_flag)
	    flags = DIR_COMB | DIR_REGION;
	else
	    flags = DIR_COMB;

	if (dp != DIR_NULL) {
	    if (db_delete(dbip, dp) || db_dirdelete(dbip, dp)) {
		Tcl_AppendResult(interp, "ERROR: Unable to delete directory entry for ",
				 old_name, "\n", (char *)NULL);
		intern.idb_ptr->idb_meth->ft_ifree(&intern, &rt_uniresource);
		return 1;
	    }
	}

	if ((dp=db_diradd(dbip, new_name, -1L, 0, flags, (genptr_t)&intern.idb_type)) == DIR_NULL) {
	    Tcl_AppendResult(interp, "Cannot add ", new_name,
			     " to directory, no changes made\n", (char *)NULL);
	    intern.idb_ptr->idb_meth->ft_ifree(&intern, &rt_uniresource);
	    return 1;
	}
    } else if (dp == DIR_NULL) {
	int flags;

	if (comb->region_flag)
	    flags = DIR_COMB | DIR_REGION;
	else
	    flags = DIR_COMB;

	if ((dp=db_diradd(dbip, new_name, -1L, 0, flags, (genptr_t)&intern.idb_type)) == DIR_NULL) {
	    Tcl_AppendResult(interp, "Cannot add ", new_name,
			     " to directory, no changes made\n", (char *)NULL);
	    intern.idb_ptr->idb_meth->ft_ifree(&intern, &rt_uniresource);
	    return 1;
	}
    } else {
	if (comb->region_flag)
	    dp->d_flags |= DIR_REGION;
	else
	    dp->d_flags &= ~DIR_REGION;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	Tcl_AppendResult(interp, "ERROR: Unable to write new combination into database.\n", (char *)NULL);
	return 1;
    }

    return 0;
}


HIDDEN int
put_tree_into_comb(struct rt_comb_internal *comb, struct directory *dp, char *old_name, char *new_name, char *str)
{
    size_t i;
    int done;
    char *line;
    char *ptr;
    char relation;
    char *name;
    struct rt_tree_array *rt_tree_array;
    struct line_list *llp;
    size_t node_count = 0;
    int tree_index = 0;
    union tree *tp;
    matp_t matrix;
    struct bu_vls vls;
    int result;

    if (str == (char *)NULL)
	return TCL_ERROR;

    BU_LIST_INIT(&HeadLines.l);

    /* break str into lines */
    line = str;
    ptr = strchr(str, '\n');
    if (ptr != NULL)
	*ptr = '\0';
    bu_vls_init(&vls);
    while (line != (char *)NULL) {
	int n;

	bu_vls_strcpy(&vls, line);

	if ((n = count_nodes(bu_vls_addr(&vls))) < 0) {
	    bu_vls_free(&vls);
	    bu_list_free(&HeadLines.l);
	    return TCL_ERROR;
	} else if (n > 0) {
	    BU_GETSTRUCT(llp, line_list);
	    BU_LIST_INSERT(&HeadLines.l, &llp->l);
	    llp->line = line;

	    node_count += n;
	} /* else blank line */

	if (ptr != NULL && *(ptr+1) != '\0') {
	    /* leap frog past EOS */
	    line = ptr + 1;

	    ptr = strchr(line, '\n');
	    if (ptr != NULL)
		*ptr = '\0';
	} else {
	    line = NULL;
	}
    }
    bu_vls_free(&vls);

    /* build tree list */
    if (node_count)
	rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list");
    else
	rt_tree_array = (struct rt_tree_array *)NULL;

    for (BU_LIST_FOR(llp, line_list, &HeadLines.l)) {
	done = 0;
	ptr = strtok(llp->line, delims);
	while (!done) {
	    if (!ptr)
		break;

	    /* First non-white is the relation operator */
	    relation = (*ptr);
	    if (relation == '\0')
		break;

	    /* Next must be the member name */
	    ptr = strtok((char *)NULL, delims);
	    if (ptr == (char *)NULL) {
		bu_list_free(&HeadLines.l);
		if (rt_tree_array)
		    bu_free((char *)rt_tree_array, "red: tree list");
		bu_log("no name specified\n");
		return TCL_ERROR;
	    }
	    name = ptr;

	    /* Eliminate trailing white space from name */
	    i = strlen(ptr);
	    while (isspace(name[--i]))
		name[i] = '\0';

	    /* Check for existence of member */
	    if ((db_lookup(dbip, name, LOOKUP_QUIET)) == DIR_NULL)
		bu_log("\tWARNING: ' %s ' does not exist\n", name);

	    /* get matrix */
	    ptr = strtok((char *)NULL, delims);
	    if (ptr == (char *)NULL) {
		matrix = (matp_t)NULL;
		done = 1;
	    } else if (*ptr == 'u' ||
		       (*ptr == '-' && *(ptr+1) == '\0') ||
		       (*ptr == '+' && *(ptr+1) == '\0')) {
		/* assume another relational operator */
		matrix = (matp_t)NULL;
	    } else {
		int k;

		matrix = (matp_t)bu_calloc(16, sizeof(fastf_t), "red: matrix");
		matrix[0] = atof(ptr);
		for (k=1; k<16; k++) {
		    ptr = strtok((char *)NULL, delims);
		    if (!ptr) {
			bu_log("incomplete matrix for member %s - No changes made\n", name);
			bu_free((char *)matrix, "red: matrix");
			if (rt_tree_array)
			    bu_free((char *)rt_tree_array, "red: tree list");
			bu_list_free(&HeadLines.l);
			return TCL_ERROR;
		    }
		    matrix[k] = atof(ptr);
		}
		if (bn_mat_is_identity(matrix)) {
		    bu_free((char *)matrix, "red: matrix");
		    matrix = (matp_t)NULL;
		}

		ptr = strtok((char *)NULL, delims);
		if (ptr == (char *)NULL)
		    done = 1;
	    }

	    /* Add it to the combination */
	    switch (relation) {
		case '+':
		    rt_tree_array[tree_index].tl_op = OP_INTERSECT;
		    break;
		case '-':
		    rt_tree_array[tree_index].tl_op = OP_SUBTRACT;
		    break;
		default:
		    bu_log("unrecognized relation (assume UNION)\n");
		case 'u':
		    rt_tree_array[tree_index].tl_op = OP_UNION;
		    break;
	    }

	    BU_GETUNION(tp, tree);
	    rt_tree_array[tree_index].tl_tree = tp;
	    tp->tr_l.magic = RT_TREE_MAGIC;
	    tp->tr_l.tl_op = OP_DB_LEAF;
	    tp->tr_l.tl_name = bu_strdup(name);
	    tp->tr_l.tl_mat = matrix;
	    tree_index++;
	}
    }

    bu_list_free(&HeadLines.l);
    result = make_tree(comb, dp, node_count, old_name, new_name, rt_tree_array, tree_index);
    if (result == 0)
	return TCL_OK;
    else
	return TCL_ERROR;
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
	if (db_lookup(dbip, str, LOOKUP_QUIET) == DIR_NULL)
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

    if ((dp=db_diradd(dbip, red_tmpcomb, -1L, 0, dpold->d_flags, (genptr_t)&intern.idb_type)) == DIR_NULL) {
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


/* restore a combination that was saved in "red_tmpcomb" */
void
restore_comb(struct directory *dp)
{
    char *av[4];
    char *name;

    /* Save name of original combo */
    name = bu_strdup(dp->d_namep);

    av[0] = "kill";
    av[1] = name;
    av[2] = NULL;
    av[3] = NULL;
    (void)cmd_kill((ClientData)NULL, interp, 2, av);

    av[0] = "mv";
    av[1] = red_tmpcomb;
    av[2] = name;

    (void)ged_move(gedp, 3, av);

    bu_free(name, "bu_strdup'd name");
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

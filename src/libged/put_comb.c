/*                         P U T _ C O M B . C
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
/** @file libged/put_comb.c
 *
 * The put_comb command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


/* allowable delimiters */
static const char _delims[] = " \t/";

static const char tmpcomb[16] = { 'g', 'e', 'd', '_', 't', 'm', 'p', '.', 'a', 'X', 'X', 'X', 'X', 'X', '\0' };


static int
make_tree(struct ged *gedp, struct rt_comb_internal *comb, struct directory *dp, size_t node_count, const char *old_name, const char *new_name, struct rt_tree_array *rt_tree_array, int tree_index)
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
	    if (db_delete(gedp->ged_wdbp->dbip, dp) || db_dirdelete(gedp->ged_wdbp->dbip, dp)) {
		bu_vls_printf(gedp->ged_result_str, "make_tree: Unable to delete directory entry for %s\n", old_name);
		intern.idb_meth->ft_ifree(&intern);
		return GED_ERROR;
	    }
	}

	if ((dp=db_diradd(gedp->ged_wdbp->dbip, new_name, RT_DIR_PHONY_ADDR, 0, flags, (genptr_t)&intern.idb_type)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "make_tree: Cannot add %s to directory, no changes made\n", new_name);
	    intern.idb_meth->ft_ifree(&intern);
	    return 1;
	}
    } else if (dp == RT_DIR_NULL) {
	int flags;

	if (comb->region_flag)
	    flags = RT_DIR_COMB | RT_DIR_REGION;
	else
	    flags = RT_DIR_COMB;

	if ((dp=db_diradd(gedp->ged_wdbp->dbip, new_name, RT_DIR_PHONY_ADDR, 0, flags, (genptr_t)&intern.idb_type)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "make_tree: Cannot add %s to directory, no changes made\n", new_name);
	    intern.idb_meth->ft_ifree(&intern);
	    return GED_ERROR;
	}
    } else {
	if (comb->region_flag)
	    dp->d_flags |= RT_DIR_REGION;
	else
	    dp->d_flags &= ~RT_DIR_REGION;
    }

    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "make_tree: Unable to write new combination into database.\n");
	return GED_ERROR;
    }

    return GED_OK;
}


static const char *
mktemp_comb(struct ged *gedp, const char *str)
{
    /* Make a temporary name for a combination
       a template name is expected as in "mk_temp()" with
       5 trailing X's */

    int counter, done;
    char *ptr;
    static char name[NAMESIZE] = {0};

    if (gedp->ged_wdbp->dbip == DBI_NULL)
	return NULL;

    /* leave room for 5 digits */
    bu_strlcpy(name, str, NAMESIZE - 5);

    ptr = name;
    while (*ptr != '\0')
	ptr++;

    while (*(--ptr) == 'X')
	;
    ptr++;

    counter = 1;
    done = 0;
    while (!done && counter < 99999) {
	sprintf(ptr, "%d", counter);
	if (db_lookup(gedp->ged_wdbp->dbip, str, LOOKUP_QUIET) == RT_DIR_NULL)
	    done = 1;
	else
	    counter++;
    }

    return name;
}


static const char *
save_comb(struct ged *gedp, struct directory *dpold)
{
    /* Save a combination under a temporory name */

    struct directory *dp;
    struct rt_db_internal intern;

    /* Make a new name */
    const char *name = mktemp_comb(gedp, tmpcomb);

    if (rt_db_get_internal(&intern, dpold, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "save_comb: Database read error, aborting\n");
	return NULL;
    }

    if ((dp = db_diradd(gedp->ged_wdbp->dbip, name, RT_DIR_PHONY_ADDR, 0, dpold->d_flags, (genptr_t)&intern.idb_type)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "save_comb: Cannot save copy of %s, no changed made\n", dpold->d_namep);
	return NULL;
    }

    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "save_comb: Cannot save copy of %s, no changed made\n", dpold->d_namep);
	return NULL;
    }

    return name;
}


/* restore a combination that was created during save_comb() */
static void
restore_comb(struct ged *gedp, struct directory *dp, const char *oldname)
{
    const char *av[4];
    char *name;

    /* get rid of previous comb */
    name = bu_strdup(dp->d_namep);

    av[0] = "kill";
    av[1] = name;
    av[2] = NULL;
    av[3] = NULL;
    (void)ged_kill(gedp, 2, (const char **)av);

    av[0] = "mv";
    av[1] = oldname;
    av[2] = name;

    (void)ged_move(gedp, 3, (const char **)av);

    bu_free(name, "bu_strdup'd name");
}


struct line_list{
    struct bu_list l;
    char *line;
};


static struct line_list HeadLines;


static int
count_nodes(struct ged *gedp, char *line)
{
    char *ptr;
    char *name;
    char relation;
    int node_count=0;

    /* sanity */
    if (line == NULL)
	return 0;

    ptr = strtok(line, _delims);

    while (ptr) {
	/* First non-white is the relation operator */
	relation = (*ptr);

	if (relation != '+' && relation != 'u' && relation != '-') {
	    bu_vls_printf(gedp->ged_result_str, " %c is not a legal operator\n", relation);
	    return -1;
	}

	/* Next must be the member name */
	name = strtok((char *)NULL, _delims);

	if (name == NULL) {
	    bu_vls_printf(gedp->ged_result_str, " operand name missing\n");
	    return -1;
	}

	ptr = strtok((char *)NULL, _delims);
	/*
	 * If this token is not a boolean operator, then it must be the start
	 * of a matrix which we will skip.
	 */
	if (ptr && !((*ptr == 'u' || *ptr == '-' || *ptr=='+') &&
		     *(ptr+1) == '\0')) {
	    int k;

	    /* skip past matrix, k=1 because we already have the first value */
	    for (k=1; k<16; k++) {
		ptr = strtok((char *)NULL, _delims);
		if (!ptr) {
		    bu_vls_printf(gedp->ged_result_str, "expecting a matrix\n");
		    return -1;
		}
	    }

	    /* get the next relational operator on the current line */
	    ptr = strtok((char *)NULL, _delims);
	}

	node_count++;
    }

    return node_count;
}


static int
put_tree_into_comb(struct ged *gedp, struct rt_comb_internal *comb, struct directory *dp, const char *old_name, const char *new_name, const char *imstr)
{
    int i;
    int done;
    char *line;
    char *ptr;
    char relation;
    char *name;
    struct rt_tree_array *rt_tree_array;
    struct line_list *llp;
    int node_count = 0;
    int tree_index = 0;
    union tree *tp;
    matp_t matrix;
    struct bu_vls vls;
    char *str;

    if (imstr == (char *)NULL)
	return GED_ERROR;

    BU_LIST_INIT(&HeadLines.l);

    /* duplicate the immutable str (from argv) for strtok style mutation */
    str = bu_strdup(imstr);

    /* break str into lines */
    line = str;
    ptr = strchr(str, '\n');
    if (ptr != NULL)
	*ptr = '\0';
    bu_vls_init(&vls);
    while (line != (char *)NULL) {
	int n;

	bu_vls_strcpy(&vls, line);

	if ((n = count_nodes(gedp, bu_vls_addr(&vls))) < 0) {
	    bu_vls_free(&vls);
	    bu_list_free(&HeadLines.l);
	    bu_free(str, "dealloc bu_strdup str");
	    return GED_ERROR;
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

    for (BU_LIST_FOR (llp, line_list, &HeadLines.l)) {
	done = 0;
	ptr = strtok(llp->line, _delims);
	while (!done) {
	    if (!ptr)
		break;

	    /* First non-white is the relation operator */
	    relation = (*ptr);
	    if (relation == '\0')
		break;

	    /* Next must be the member name */
	    ptr = strtok((char *)NULL, _delims);
	    if (ptr == (char *)NULL) {
		bu_list_free(&HeadLines.l);
		if (rt_tree_array)
		    bu_free((char *)rt_tree_array, "red: tree list");
		bu_log("no name specified\n");
		bu_free(str, "dealloc bu_strdup str");
		return GED_ERROR;
	    }
	    name = ptr;

	    /* Eliminate trailing white space from name */
	    i = (int)strlen(ptr);
	    while (isspace(name[--i]))
		name[i] = '\0';

	    /* Check for existence of member */
	    if ((db_lookup(gedp->ged_wdbp->dbip, name, LOOKUP_QUIET)) == RT_DIR_NULL)
		bu_log("\tWARNING: ' %s ' does not exist\n", name);

	    /* get matrix */
	    ptr = strtok((char *)NULL, _delims);
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
		    ptr = strtok((char *)NULL, _delims);
		    if (!ptr) {
			bu_log("incomplete matrix for member %s - No changes made\n", name);
			bu_free((char *)matrix, "red: matrix");
			if (rt_tree_array)
			    bu_free((char *)rt_tree_array, "red: tree list");
			bu_list_free(&HeadLines.l);
			bu_free(str, "dealloc bu_strdup str");
			return GED_ERROR;
		    }
		    matrix[k] = atof(ptr);
		}
		if (bn_mat_is_identity(matrix)) {
		    bu_free((char *)matrix, "red: matrix");
		    matrix = (matp_t)NULL;
		}

		ptr = strtok((char *)NULL, _delims);
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
	    RT_TREE_INIT(tp);
	    rt_tree_array[tree_index].tl_tree = tp;
	    tp->tr_l.tl_op = OP_DB_LEAF;
	    tp->tr_l.tl_name = bu_strdup(name);
	    tp->tr_l.tl_mat = matrix;
	    tree_index++;
	}
    }

    bu_list_free(&HeadLines.l);
    i = make_tree(gedp, comb, dp, node_count, old_name, new_name, rt_tree_array, tree_index);

    bu_free(str, "dealloc bu_strdup str");

    return i;
}


void
put_rgb_into_comb(struct rt_comb_internal *comb, const char *str)
{
    int r, g, b;

    if (sscanf(str, "%d%*c%d%*c%d", &r, &g, &b) != 3) {
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


int
ged_put_comb(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    char new_name_v4[NAMESIZE+1];
    char *new_name;
    int offset;
    int save_comb_flag = 0;
    static const char *usage = "comb_name is_Region id air material los color shader inherit boolean_expr";
    static const char *noregionusage = "comb_name n color shader inherit boolean_expr";
    static const char *regionusage = "comb_name y id air material los color shader inherit boolean_expr";
    const char *saved_name = NULL;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 7 || 11 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    comb = (struct rt_comb_internal *)NULL;
    dp = db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_QUIET);
    if (dp != RT_DIR_NULL) {
	if (!(dp->d_flags & RT_DIR_COMB)) {
	    bu_vls_printf(gedp->ged_result_str, "%s: %s is not a combination, so cannot be edited this way\n", argv[0], argv[1]);
	    return GED_ERROR;
	}

	if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "%s: Database read error, aborting\n", argv[0]);
	    return GED_ERROR;
	}

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	saved_name = save_comb(gedp, dp); /* Save combination to a temp name */
	save_comb_flag = 1;
    }

    /* empty the existing combination */
    if (comb) {
	db_free_tree(comb->tree, &rt_uniresource);
	comb->tree = NULL;
    } else {
	/* make an empty combination structure */
	BU_GETSTRUCT(comb, rt_comb_internal);
	RT_COMB_INTERNAL_INIT(comb);
    }

    if (db_version(gedp->ged_wdbp->dbip) < 5) {
	new_name = new_name_v4;
	if (dp == RT_DIR_NULL)
	    NAMEMOVE(argv[1], new_name_v4);
	else
	    NAMEMOVE(dp->d_namep, new_name_v4);
    } else {
	if (dp == RT_DIR_NULL)
	    new_name = (char *)argv[1];
	else
	    new_name = dp->d_namep;
    }

    if (*argv[2] == 'y' || *argv[2] == 'Y')
	comb->region_flag = 1;
    else
	comb->region_flag = 0;

    if (comb->region_flag) {
	if (argc != 11) {
	    bu_vls_printf(gedp->ged_result_str, "region_flag is set, incorrect number of arguments supplied.\n");
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], regionusage);
	    return GED_ERROR;
	}

	comb->region_id = atoi(argv[3]);
	comb->aircode = atoi(argv[4]);
	comb->GIFTmater = atoi(argv[5]);
	comb->los = atoi(argv[6]);

	offset = 6;
    } else {
	if (argc != 7) {
	    bu_vls_printf(gedp->ged_result_str, "region_flag is not set, incorrect number of arguments supplied.\n");
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], noregionusage);
	    return GED_ERROR;
	}
	offset = 2;
    }

    put_rgb_into_comb(comb, argv[offset + 1]);
    bu_vls_strcpy(&comb->shader, argv[offset +2]);

    if (*argv[offset + 3] == 'y' || *argv[offset + 3] == 'Y')
	comb->inherit = 1;
    else
	comb->inherit = 0;

    if (put_tree_into_comb(gedp, comb, dp, argv[1], new_name, argv[offset + 4]) == GED_ERROR) {
	if (comb) {
	    restore_comb(gedp, dp, saved_name);
	    bu_vls_printf(gedp->ged_result_str, "%s: \toriginal restored\n", argv[0]);
	}
	bu_file_delete(_ged_tmpfil);
	return GED_ERROR;
    } else if (save_comb_flag) {
	/* eliminate the temporary combination */
	const char *av[3];

	av[0] = "kill";
	av[1] = saved_name;
	av[2] = NULL;
	(void)ged_kill(gedp, 2, (const char **)av);
    }

    bu_file_delete(_ged_tmpfil);
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

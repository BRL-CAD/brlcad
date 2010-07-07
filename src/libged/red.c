/*                        R E D . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2010 United States Government as represented by
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
/** @file red.c
 *
 * The red command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "db.h"

#include "./ged_private.h"


/* FIXME: Accessing unpublished functions.  this should be hidden
 * behind the scenes, not be an API function. should eliminate direct
 * calls to these functions.
 */
extern size_t db5_is_standard_attribute(const char *attrname);
extern void db5_standardize_avs(struct bu_attribute_value_set *avs);


char _ged_tmpfil[MAXPATHLEN] = {0};
char _delims[] = " \t/";	/* allowable delimiters */

static int
get_attr_val_pair(char *line, struct bu_vls *attr, struct bu_vls *val) 
{
    char *ptr1;

    if (!line) return 0;

    /* find the '=' */
    ptr1 = strchr(&line[0], '=');
    if (!ptr1)
	return 0;

    /* Everything from the beginning to the = is the attribute name*/
    bu_vls_strncpy(attr, line, ptr1 - &line[0]);
    bu_vls_trimspace(attr);
    if (bu_vls_strlen(attr) == 0) return 0;

    ++ptr1;
	
    /* Grab the attribute value */
    bu_vls_strcpy(val, ptr1);
    bu_vls_trimspace(val);
    if (bu_vls_strlen(val) == 0) return 0;

    return 1;
}

void
_ged_print_matrix(FILE *fp, matp_t matrix)
{
    int k;
    char buf[64];
    fastf_t tmp;

    if (!matrix)
	return;

    for (k=0; k<16; k++) {
	sprintf(buf, "%g", matrix[k]);
	tmp = atof(buf);
	if (NEAR_ZERO(tmp - matrix[k], SMALL_FASTF))
	    fprintf(fp, " %g", matrix[k]);
	else
	    fprintf(fp, " %.12e", matrix[k]);
	if ((k&3)==3) fputc(' ', fp);
    }
}

HIDDEN int
build_comb(struct ged *gedp, struct directory *dp)
{
    /* Do some minor checking of the edited file */
    struct rt_comb_internal *comb;
    FILE *fp;
    size_t node_count=0;
    int nonsubs=0;
    int k;
    int scanning_tree, matrix_space, matrix_pos, space_cnt;
    char relation;
    char *tmpstr;
    char *ptr = (char *)NULL;
    union tree *tp;
    int tree_index=0;
    struct rt_db_internal intern, localintern;
    struct rt_tree_array *rt_tree_array;
    matp_t matrix;
    struct bu_attribute_value_set avs;
    struct bu_vls line;
    struct bu_vls matrix_line;
    struct bu_vls attr_vls;
    struct bu_vls val_vls;
    struct bu_vls tmpline;    
    struct bu_vls name_v5;

#if 0
    regex_t attr_regex, combtree_regex, combtree_op_regex, matrix_entry;
    regmatch_t *result_locations, *matrix_locations;
    const char *attr_string = "([:blank:]?=[:blank:]?)"; /* When doing attr hunting, read in next line and check for presence of an attr match - if not present and combtree_string is not present, append the new line to the previous line without the newline - else process the old line as is and begin anew with tne new one.  When a match + terminating case is found, pass the resulting line to get_attr_val_pair - easier than working with the regex results, for such a simple assignment."
    const char *combtree_string = "Combination Tree:";
    const char *combtree_op_string = "[:blank:]?[+-u][:blank:]?"
    const char *float_string = "[+-]?[0-9]*\.?[0-9]?[[eE][+-]?[0-9]+]?";
    struct bu_vls matrix_string;
    bu_vls_sprintf(&matrix_string, "[:blank:][%s[:blank:]+]{15}%s", float_string, float_string);


#endif

    if (gedp->ged_wdbp->dbip == DBI_NULL)
	return -1;

    GED_DB_GET_INTERNAL(gedp, &intern, dp, (fastf_t *)NULL, &rt_uniresource, GED_ERROR);
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb) {
	RT_CK_COMB(comb);
	RT_CK_DIR(dp);
    }

    if ((fp=fopen(_ged_tmpfil, "r")) == NULL) {
	perror("fopen");
	bu_vls_printf(&gedp->ged_result_str, "Cannot open temporary file for reading\n");
	return -1;
    }

    bu_vls_init(&line);
    bu_vls_init(&tmpline);
    bu_vls_init(&matrix_line);
    bu_vls_init(&attr_vls);
    bu_vls_init(&val_vls);
    bu_vls_init(&name_v5);

    bu_avs_init_empty(&avs);

    rt_tree_array = (struct rt_tree_array *)NULL;

    scanning_tree = 0;
    bu_vls_trunc(&line, 0);

    /* We need two passes here - first pass deals with the attribute value pairs and
     * counts tree nodes, the second pass deals with the combination tree itself */

    while (bu_vls_gets(&line, fp) != -1) {
        if (!scanning_tree && strcmp(bu_vls_addr(&line), "Combination Tree:")) {
            /* As long as we're not doing the tree, make sure there's an
	     * equal sign somewhere in the line so avs parsing will work
	     */	    
	    ptr = strchr(&(bu_vls_addr(&line))[0], '=');
    	    if (!ptr) {
		bu_vls_printf(&gedp->ged_result_str, "Not a valid attribute/value pair:  %s\n", bu_vls_addr(&line));
		fclose(fp);
		bu_vls_free(&line);
		bu_vls_free(&tmpline);
		bu_vls_free(&matrix_line);
		bu_vls_free(&attr_vls);
		bu_vls_free(&val_vls);
		bu_vls_free(&name_v5);
		return -1;
	    } else {
		if (get_attr_val_pair(bu_vls_addr(&line), &attr_vls, &val_vls)) {
		    if (strcmp(bu_vls_addr(&val_vls), "") && strcmp(bu_vls_addr(&attr_vls), "name")) 
		        (void)bu_avs_add(&avs, bu_vls_addr(&attr_vls), bu_vls_addr(&val_vls));
		}
	    }
            bu_vls_trunc(&line, 0);
	    continue;
        } else {
 	    if (scanning_tree != 1) {
	        bu_vls_trunc(&line, 0);
	        bu_vls_gets(&line, fp);
		scanning_tree = 1;
	    }
	    bu_vls_trimspace(&line);
	    if (bu_vls_strlen(&line) == 0) {
		/* blank line, don't count, continue */
		continue;
	    }
	    node_count++;
            bu_vls_trunc(&line, 0);
	}
    }

    db5_standardize_avs(&avs);

    /* If we have a non-zero node count, there is a combination tree to handle - do second pass*/
    if (node_count) {
	rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list");
    
        /* Close and open the file again to start reading from the beginning - already opened 
         * successfully once to get here so don't need to check again. */
        fclose(fp);
        fp=fopen(_ged_tmpfil, "r");
    
	scanning_tree = 0;
        bu_vls_trunc(&line, 0);
        
        while (bu_vls_gets(&line, fp) != -1) {
            if (!scanning_tree && strcmp(bu_vls_addr(&line), "Combination Tree:")) {
                bu_vls_trunc(&line, 0);
	        continue;
            } else {
 	        if (scanning_tree != 1) {
	            bu_vls_trunc(&line, 0);
	            bu_vls_gets(&line, fp);
		    scanning_tree = 1;
	        }
	  
  	  	bu_vls_trimspace(&line);
	    	if (bu_vls_strlen(&line) == 0) {
		    /* blank line, continue */
		    continue;
	    	}

	    	/* Check if we have enough spaces for a matrix and if
	     	 * so where it is in the string */
	    	space_cnt=0;
            	tmpstr = bu_vls_strdup(&line);
	   	ptr = strtok(tmpstr, _delims);
	    	while (ptr) {
		    space_cnt++;
		    ptr = strtok((char *)NULL, _delims);
	    	}
	    	bu_free(tmpstr, "free tmpstr");
		/* Handle the matrix portion of the input line, if it exists */
		matrix_pos = 0;
	    	if (space_cnt > 17) {
		    matrix_space = space_cnt - 17;
		    tmpstr = bu_vls_strdup(&line);
	            ptr = strtok(tmpstr, _delims);
		    space_cnt = 0;
		    while (space_cnt < matrix_space) {
			space_cnt++;
			ptr = strtok((char *)NULL, _delims);
		    }
		    matrix_pos = strtok((char *)NULL, _delims) - tmpstr;
		    bu_vls_strncpy(&matrix_line, bu_vls_addr(&line) + matrix_pos, bu_vls_strlen(&line) - matrix_pos);
		    /* If the matrix string has no alphabetical
		     * characters, assume it really is a matrix and
		     * remove that portion of the string.  'eE'
		     * intentionally left out since it may be part of
		     * a floating point number.
		     */
		    if (!strpbrk(bu_vls_addr(&matrix_line), "abcdfghijklmnopqrstuvwxyzABCDFGHIJKLMNOPQRSTUVWXYZ")) {
		        bu_vls_strncpy(&tmpline, bu_vls_addr(&line), matrix_pos);
	                bu_vls_sprintf(&line, "%s", bu_vls_addr(&tmpline));
	    	        bu_vls_trimspace(&line);
		    } else {
		        matrix_pos = 0;
		    }
		    bu_free(tmpstr, "free tmpstr");
	        } else {
		    matrix = (matp_t)NULL;
		}
 
	    	/* First non-white is the relation operator */
	    	relation = bu_vls_addr(&line)[0];
	    	if (relation != '+' && relation != 'u' && relation != '-') {
	            bu_vls_printf(&gedp->ged_result_str, " %c is not a legal combination tree operator\n", relation);
	            fclose(fp);
		    bu_vls_free(&line);
		    bu_vls_free(&tmpline);
		    bu_vls_free(&matrix_line);
		    bu_vls_free(&attr_vls);
		    bu_vls_free(&val_vls);
		    bu_vls_free(&name_v5);
		    return -1;
	    	}
	    	if (relation != '-')
		    nonsubs++;

	    	/* Next must be the member name */
	    	bu_vls_nibble(&line, 2);
	    	if (!ptr && bu_vls_strlen(&line) != 0) ptr = bu_vls_addr(&line) + bu_vls_strlen(&line);
	    	bu_vls_trunc(&name_v5, 0);
	    	if (ptr != NULL && (bu_vls_addr(&line) - ptr != 0)) {
		    bu_vls_strncpy(&name_v5, bu_vls_addr(&line), ptr - bu_vls_addr(&line));
	    	} else {
		    bu_vls_printf(&gedp->ged_result_str, " operand name missing\n%s\n", bu_vls_addr(&line));
		    fclose(fp);
		    bu_vls_free(&line);
		    bu_vls_free(&tmpline);
		    bu_vls_free(&matrix_line);
		    bu_vls_free(&attr_vls);
		    bu_vls_free(&val_vls);
		    bu_vls_free(&name_v5);
		    return -1;
	    	}

		/* Now that we know the name, handle the matrix if any */
		if (matrix_pos) {
		    matrix = (matp_t)bu_calloc(16, sizeof(fastf_t), "red: matrix");
		    ptr = strtok(bu_vls_addr(&matrix_line), _delims);
		    matrix[0] = atof(ptr);
		    for (k=1; k<16; k++) {
			ptr = strtok((char *)NULL, _delims);
			if (!ptr) {
			    bu_vls_printf(&gedp->ged_result_str, "build_comb: incomplete matrix for member %s. No changes made\n", bu_vls_addr(&name_v5));
			    bu_free((char *)matrix, "red: matrix");
			    if (rt_tree_array) bu_free((char *)rt_tree_array, "red: tree list"); 
			    fclose(fp);
			    bu_vls_free(&line);
			    bu_vls_free(&tmpline);
			    bu_vls_free(&matrix_line);
			    bu_vls_free(&attr_vls);
			    bu_vls_free(&val_vls);
			    bu_vls_free(&name_v5);
			    return -1;
			}
			matrix[k] = atof(ptr);
		    }
		    if (bn_mat_is_identity(matrix)) {
			bu_free((char *)matrix, "red: matrix"); 
			matrix = (matp_t)NULL;
		    }
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
			bu_vls_printf(&gedp->ged_result_str, "build_comb: unrecognized relation (assume UNION)\n");
		    case 'u':
			rt_tree_array[tree_index].tl_op = OP_UNION;
			break;
		}
		BU_GETUNION(tp, tree);
		rt_tree_array[tree_index].tl_tree = tp;
		tp->tr_l.magic = RT_TREE_MAGIC;
		tp->tr_l.tl_op = OP_DB_LEAF;
		tp->tr_l.tl_name = bu_strdup(bu_vls_addr(&name_v5));
		tp->tr_l.tl_mat = matrix;
		tree_index++;


                bu_vls_trunc(&line, 0);
	    } 
        } 
    }

    fclose(fp);
    bu_vls_free(&line);
    bu_vls_free(&tmpline);
    bu_vls_free(&matrix_line);
    bu_vls_free(&attr_vls);
    bu_vls_free(&val_vls);
    bu_vls_free(&name_v5);

    if (nonsubs == 0 && node_count) {
        bu_vls_printf(&gedp->ged_result_str, "Cannot create a combination with all subtraction operators\n");
        return -1;
    }

    if (tree_index)
        tp = (union tree *)db_mkgift_tree(rt_tree_array, node_count, &rt_uniresource);
    else
        tp = (union tree *)NULL;

    if (comb && comb->tree) {
	db_free_tree(comb->tree, &rt_uniresource);
	comb->tree = NULL;
    }
    comb->tree = tp;

    if(rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "build_comb: Cannot apply tree\n", dp->d_namep);
	bu_avs_free(&avs);
	return -1;
    }

    /* Now that the tree is handled, get the current data structure pointers and apply
     * the attribute logic - this apparently must come after the rt_db_put_internal */
    GED_DB_GET_INTERNAL(gedp, &localintern, dp, (fastf_t *)NULL, &rt_uniresource, GED_ERROR);

    db5_replace_attributes(dp, &avs, gedp->ged_wdbp->dbip);
    
    comb = (struct rt_comb_internal *)localintern.idb_ptr;
    db5_apply_std_attributes(gedp->ged_wdbp->dbip, dp, comb);

    bu_avs_free(&avs);
    return node_count;
}


HIDDEN int
write_comb(struct ged *gedp, struct rt_comb_internal *comb, const char *name)
{
    /* Writes the file for later editing */
    struct rt_tree_array *rt_tree_array;
    struct bu_attribute_value_set avs;
    struct bu_attribute_value_pair *avpp;
    struct directory *dp;
    FILE *fp;
    size_t i, j, maxlength;
    size_t node_count;
    size_t actual_count;
    struct bu_vls spacer;
    char *standard_attributes[8];
    standard_attributes[0] = "region";
    standard_attributes[1] = "region_id";
    standard_attributes[2] = "material_id";
    standard_attributes[3] = "los";
    standard_attributes[4] = "air";
    standard_attributes[5] = "color";
    standard_attributes[6] = "oshader";
    standard_attributes[7] = "inherit";

    bu_avs_init_empty(&avs);

    bu_vls_init(&spacer);
    bu_vls_sprintf(&spacer, "");

    dp = db_lookup(gedp->ged_wdbp->dbip, name, LOOKUP_QUIET);

    if (comb)
	RT_CK_COMB(comb);

    /* open the file */
    if ((fp=fopen(_ged_tmpfil, "w")) == NULL) {
	perror("fopen");
	bu_vls_printf(&gedp->ged_result_str, "write_comb: Cannot open temporary file for writing\n");
	return GED_ERROR;
    }

    maxlength = 0;
    for (i = 0; i < sizeof(standard_attributes)/sizeof(char *); i++) {
	if (strlen(standard_attributes[i]) > maxlength) 
	    maxlength = strlen(standard_attributes[i]);
    }
	
    if (!comb) {
  	bu_vls_trunc(&spacer, 0);
	for (j = 0; j < maxlength - 4 + 1; j++) {
	    bu_vls_printf(&spacer, " ");
        }
	fprintf(fp, "name%s= %s\n", bu_vls_addr(&spacer), name);
	for (i = 0; i < sizeof(standard_attributes)/sizeof(char *); i++) {
  	    bu_vls_trunc(&spacer, 0);
	    for (j = 0; j < maxlength - strlen(standard_attributes[i]); j++) {
		bu_vls_printf(&spacer, " ");
            }
	    fprintf(fp, "%s%s = \n", standard_attributes[i], bu_vls_addr(&spacer));
        }
	fprintf(fp, "Combination Tree:\n");
	fclose(fp);
	return GED_OK;
    }

    if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	db_non_union_push(comb->tree, &rt_uniresource);
	if (db_ck_v4gift_tree(comb->tree) < 0) {
	    bu_vls_printf(&gedp->ged_result_str, "write_comb: Cannot flatten tree for editing\n");
	    return GED_ERROR;
	}
    }
    node_count = db_tree_nleaves(comb->tree);
    if (node_count > 0) {
	rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list");
	actual_count = (struct rt_tree_array *)db_flatten_tree(rt_tree_array, comb->tree, OP_UNION, 0, &rt_uniresource) - rt_tree_array;
	BU_ASSERT_SIZE_T(actual_count, ==, node_count);
    } else {
	rt_tree_array = (struct rt_tree_array *)NULL;
	actual_count = 0;
    }

    db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp);
    db5_apply_std_attributes(gedp->ged_wdbp->dbip, dp, comb);
    db5_update_std_attributes(gedp->ged_wdbp->dbip, dp, comb);
    if (!db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
        db5_standardize_avs(&avs);
       	avpp = avs.avp;
        for (i=0; i < avs.count; i++, avpp++) {
	    if (strlen(avpp->name) > maxlength) 
		maxlength = strlen(avpp->name);
        }
  	bu_vls_trunc(&spacer, 0);
	for (j = 0; j < maxlength - 4 + 1; j++) {
	    bu_vls_printf(&spacer, " ");
        }
        fprintf(fp, "name%s= %s\n", bu_vls_addr(&spacer), name);
        for (i = 0; i < sizeof(standard_attributes)/sizeof(char *); i++) {
  	    bu_vls_trunc(&spacer, 0);
	    for (j = 0; j < maxlength - strlen(standard_attributes[i]) + 1; j++) {
		bu_vls_printf(&spacer, " ");
            }
	    if (bu_avs_get(&avs, standard_attributes[i])) {
                fprintf(fp, "%s%s= %s\n", standard_attributes[i], bu_vls_addr(&spacer),  bu_avs_get(&avs, standard_attributes[i]));
            } else {
		fprintf(fp, "%s%s= \n", standard_attributes[i], bu_vls_addr(&spacer));
	    }
        }
	avpp = avs.avp;
        for (i=0; i < avs.count; i++, avpp++) {
            if (!db5_is_standard_attribute(avpp->name)) {
		bu_vls_trunc(&spacer, 0);
		for (j = 0; j < maxlength - strlen(avpp->name) + 1; j++) {
		    bu_vls_printf(&spacer, " ");
		}
		fprintf(fp, "%s%s= %s\n", avpp->name, bu_vls_addr(&spacer), avpp->value);
	    }
        }
    }
    bu_vls_free(&spacer);

    fprintf(fp, "Combination Tree:\n");

    for (i=0; i<actual_count; i++) {
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
		bu_vls_printf(&gedp->ged_result_str, "write_comb: Illegal op code in tree\n");
		fclose(fp);
		bu_avs_free(&avs);
		return GED_ERROR;
	}
	if (fprintf(fp, " %c %s", op, rt_tree_array[i].tl_tree->tr_l.tl_name) <= 0) {
	    bu_vls_printf(&gedp->ged_result_str, "write_comb: Cannot write to temporary file (%s). Aborting edit\n",
			  _ged_tmpfil);
	    fclose(fp);
	    bu_avs_free(&avs);
	    return GED_ERROR;
	}
	_ged_print_matrix(fp, rt_tree_array[i].tl_tree->tr_l.tl_mat);
	fprintf(fp, "\n");
    }
    fclose(fp);
    bu_avs_free(&avs);
    return GED_OK;
}



int
ged_red(struct ged *gedp, int argc, const char *argv[])
{
    FILE *fp;
    int c, counter;
    int have_tmp_name = 0;
    struct directory *dp, *tmp_dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    static const char *usage = "comb";
    const char *editstring = NULL;
    const char *av[3];
    struct bu_vls comb_name;
    struct bu_vls temp_name;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    bu_optind = 1;
    /* First, grab the editstring off of the argv list */
    while ((c = bu_getopt(argc, (char * const *)argv, "E:")) != EOF) {
	switch (c) {
	    case 'E' :
	    	editstring = bu_optarg;
		break;
	    default :
		break;
	}
    }

    argc -= bu_optind - 1;
    argv += bu_optind - 1;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc <= 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", "red", usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", "red", usage);
	return GED_ERROR;
    }


    dp = db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_QUIET);

    bu_vls_init(&comb_name);
    bu_vls_init(&temp_name);


    /* Now, sanity check to make sure a comb is in instead of a solid, and either write out existing contents
     * for an existing comb or a blank template for a new comb */
    if (dp != DIR_NULL) {

	/* Stash original primitive name and find appropriate temp name */
	bu_vls_sprintf(&comb_name, "%s", dp->d_namep);

	counter = 0;
	have_tmp_name = 0;
	while (!have_tmp_name) {
	    bu_vls_sprintf(&temp_name, "%s_red%d", dp->d_namep, counter);
	    if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&temp_name), LOOKUP_QUIET) == DIR_NULL)
		have_tmp_name = 1;
	    else
		counter++;
	}
	if (!(dp->d_flags & DIR_COMB)) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: %s must be a combination to use this command\n", argv[0], argv[1]);
	    bu_vls_free(&comb_name);
	    bu_vls_free(&temp_name);
	    return GED_ERROR;
	}

	GED_DB_GET_INTERNAL(gedp, &intern, dp, (fastf_t *)NULL, &rt_uniresource, GED_ERROR);
	comb = (struct rt_comb_internal *)intern.idb_ptr;

	/* Make a file for the text editor */
	fp = bu_temp_file(_ged_tmpfil, MAXPATHLEN);

	if (fp == (FILE *)0) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: unable to edit %s\n", argv[0], argv[1]);
	    bu_vls_printf(&gedp->ged_result_str, "%s: unable to create %s\n", argv[0], _ged_tmpfil);
	    bu_vls_free(&comb_name);
	    bu_vls_free(&temp_name);
	    return GED_ERROR;
	}

	/* Write the combination components to the file */
	if (write_comb(gedp, comb, dp->d_namep)) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: unable to edit %s\n", argv[0], argv[1]);
	    unlink(_ged_tmpfil);
	    bu_vls_free(&comb_name);
	    bu_vls_free(&temp_name);
	    return GED_ERROR;
	}
    } else {
	bu_vls_sprintf(&comb_name, "%s", argv[1]);
	bu_vls_sprintf(&temp_name, "%s", argv[1]);
	comb = (struct rt_comb_internal *)NULL;

	/* Make a file for the text editor */
	fp = bu_temp_file(_ged_tmpfil, MAXPATHLEN);

	if (fp == (FILE *)0) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: unable to edit %s\n", argv[0], argv[1]);
	    bu_vls_printf(&gedp->ged_result_str, "%s: unable to create %s\n", argv[0], _ged_tmpfil);
	    bu_vls_free(&comb_name);
	    bu_vls_free(&temp_name);
	    return GED_ERROR;
	}

	/* Write the combination components to the file */
	if (write_comb(gedp, comb, argv[1])) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: unable to edit %s\n", argv[0], argv[1]);
	    unlink(_ged_tmpfil);
	    bu_vls_free(&comb_name);
	    bu_vls_free(&temp_name);
	    return GED_ERROR;
	}
    }

    (void)fclose(fp);

    /* Edit the file */
    if (_ged_editit(editstring, _ged_tmpfil)) {
	/* specifically avoid CHECK_READ_ONLY; above so that
	 * we can delay checking if the geometry is read-only
	 * until here so that red may be used to view objects.
	 */
	if (!gedp->ged_wdbp->dbip->dbi_read_only) {
	    /* comb is to be changed.  All changes will first be made to
	     * the temporary copy of the comb - if that succeeds, the
	     * result will be copied over the original comb.  If we have an
	     * existing comb copy its contents to the temporary, else create
	     * a new empty comb from scratch. */

	    if (dp) {
	        if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		    bu_vls_printf(&gedp->ged_result_str, "Database read error, aborting\n");
		    bu_vls_free(&comb_name);
		    bu_vls_free(&temp_name);
		    return GED_ERROR;
	        }
	    
 
		if ((tmp_dp = db_diradd(gedp->ged_wdbp->dbip, bu_vls_addr(&temp_name), RT_DIR_PHONY_ADDR, 0, DIR_COMB, (genptr_t)&intern.idb_type)) == DIR_NULL) {
		    bu_vls_printf(&gedp->ged_result_str, "Cannot save copy of %s, no changed made\n", bu_vls_addr(&temp_name));
		    bu_vls_free(&comb_name);
		    bu_vls_free(&temp_name);
		    return GED_ERROR;
		}

		if (rt_db_put_internal(tmp_dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
		    bu_vls_printf(&gedp->ged_result_str, "Cannot save copy of %s, no changed made\n", bu_vls_addr(&temp_name));
		    bu_vls_free(&comb_name);
		    bu_vls_free(&temp_name);
		    return GED_ERROR;
		}
	    } else {
		RT_INIT_DB_INTERNAL(&intern);
		intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		intern.idb_type = ID_COMBINATION;
		intern.idb_meth = &rt_functab[ID_COMBINATION];

		GED_DB_DIRADD(gedp, tmp_dp, bu_vls_addr(&temp_name), -1, 0, DIR_COMB, (genptr_t)&intern.idb_type, 0);

		BU_GETSTRUCT(comb, rt_comb_internal);
		intern.idb_ptr = (genptr_t)comb;
		comb->magic = RT_COMB_MAGIC;
		bu_vls_init(&comb->shader);
		bu_vls_init(&comb->material);
		comb->region_id = 0;  /* This makes a comb/group by default */
		comb->tree = TREE_NULL;
		GED_DB_PUT_INTERNAL(gedp, tmp_dp, &intern, &rt_uniresource, 0);
	    }

 

	    if (build_comb(gedp, tmp_dp) < 0) {
		/* Something went wrong - kill the temporary comb */
		bu_vls_printf(&gedp->ged_result_str, "%s: Error in edited region, no changes made\n", *argv);

		av[0] = "kill";
		av[1] = bu_vls_addr(&temp_name);
		av[2] = NULL;
		(void)ged_kill(gedp, 2, (const char **)av);
		(void)unlink(_ged_tmpfil);
		return GED_ERROR;
	    } else {
		/* it worked - kill the original and put the updated copy in its place if a pre-existing
		 * comb was being edited - otherwise everything is already fine.*/
		if (strcmp(bu_vls_addr(&comb_name), bu_vls_addr(&temp_name))) {
		    av[0] = "kill";
		    av[1] = bu_vls_addr(&comb_name);
		    av[2] = NULL;
		    (void)ged_kill(gedp, 2, (const char **)av);
		    av[0] = "mv";
		    av[1] = bu_vls_addr(&temp_name);
		    av[2] = bu_vls_addr(&comb_name);
		    (void)ged_move(gedp, 3, (const char **)av);
		} 
	    }
	} else {
	    bu_vls_printf(&gedp->ged_result_str, "%s: Because the database is READ-ONLY no changes were made.\n", *argv);
	}
    }

    bu_vls_free(&comb_name);
    bu_vls_free(&temp_name);

    unlink(_ged_tmpfil);
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

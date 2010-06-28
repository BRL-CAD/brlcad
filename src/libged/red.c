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


char _ged_tmpfil[MAXPATHLEN] = {0};
const char _ged_tmpcomb[16] = { 'g', 'e', 'd', '_', 't', 'm', 'p', '.', 'a', 'X', 'X', 'X', 'X', 'X', '\0' };
char _delims[] = " \t/";	/* allowable delimiters */


static char *
find_keyword(int i, char *line, char *word)
{
    char *ptr1;
    char *ptr2;
    size_t j;

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


static int
get_attr_val_pair(char *line, struct bu_vls *attr, struct bu_vls *val) 
{
    char *ptr1;
    size_t j;

    /* find the '=' */
    ptr1 = strchr(&line[0], '=');
    if (!ptr1)
	return 0;

    /* Everything from the beginning to the = is the attribute name*/
    bu_vls_strncpy(attr, line, ptr1 - &line[0]);

    /* skip any white space before the value */
    ++ptr1;
    while (isspace(*(++ptr1)));

    /* eliminate trailing white space */
    j = strlen(line);
    while (isspace(line[--j]));
    line[j+1] = '\0';
	
    /* Grab the attribute value */
    bu_vls_strcpy(val, ptr1);

    return 1;
}


HIDDEN int
check_comb(struct ged *gedp, struct rt_comb_internal *comb, struct directory *dp)
{
    /* Do some minor checking of the edited file */

    FILE *fp;
    size_t node_count=0;
    int nonsubs=0;
    int i, j, k, ch;
    int scanning_tree, matrix_space, matrix_pos, space_cnt;
    char relation;
    char *tmpstr;
    char *ptr = (char *)NULL;
    union tree *tp;
    int tree_index=0;
    struct rt_tree_array *rt_tree_array;
    matp_t matrix;
    struct bu_attribute_value_set avs;
    struct bu_vls line;
    struct bu_vls matrix_line;
    struct bu_vls attr_vls;
    struct bu_vls val_vls;
    struct bu_vls tmpline;    
    struct bu_vls name_v5;

    if (gedp->ged_wdbp->dbip == DBI_NULL)
	return -1;

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
        if (!scanning_tree && strcmp(bu_vls_addr(&line), "combination tree:")) {
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
		get_attr_val_pair(bu_vls_addr(&line), &attr_vls, &val_vls);
		(void)bu_avs_add(&avs, bu_vls_addr(&attr_vls), bu_vls_addr(&val_vls));
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
            if (!scanning_tree && strcmp(bu_vls_addr(&line), "combination tree:")) {
                bu_vls_trunc(&line, 0);
	        continue;
            } else {
 	        if (scanning_tree != 1) {
	            bu_vls_trunc(&line, 0);
	            bu_vls_gets(&line, fp);
		    scanning_tree = 1;
	        }
 	        printf("into tree checking: %s\n", bu_vls_addr(&line)); 

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
	            printf("pointer: %p\n", ptr);
	    	}
	    	bu_free(tmpstr, "free tmpstr");
		/* Handle the matrix portion of the input line, if it exists */
	    	if (space_cnt > 17) {
		    matrix_space = space_cnt - 17;
		    tmpstr = bu_vls_strdup(&line);
	            ptr = strtok(tmpstr, _delims);
		    space_cnt = 0;
		    while (space_cnt < matrix_space) {
			space_cnt++;
			ptr = strtok((char *)NULL, _delims);
			printf("pointer: %p\n", ptr);
		    }
		    matrix_pos = strtok((char *)NULL, _delims) - tmpstr;
	            printf("position: %d\n", matrix_pos);
		    bu_vls_strncpy(&matrix_line, bu_vls_addr(&line) + matrix_pos, bu_vls_addr(&line) + bu_vls_strlen(&line));
		    /* If the matrix string has no alphabetical characters, assume it really is a matrix and remove
		     * that portion of the string */
		    if (!strpbrk(bu_vls_addr(&matrix_line), "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")) {
		        bu_vls_strncpy(&tmpline, bu_vls_addr(&line), bu_vls_addr(&line) + bu_vls_strlen(&line) - matrix_pos);
	                bu_vls_sprintf(&line, "%s", bu_vls_addr(&tmpline));
	    	        bu_vls_trimspace(&line);
		    } else {
		        matrix_pos = 0;
		    }
	            printf("matrix string: %s\n", bu_vls_addr(&matrix_line));
		    bu_free(tmpstr, "free tmpstr");
	        } else {
		    matrix = (matp_t)NULL;
		}
 
	    	/* First non-white is the relation operator */
	    	relation = bu_vls_addr(&line)[0];
 	    	printf("relation: %c\n", relation); 
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
	    	ptr = strpbrk(&(bu_vls_addr(&line))[0], _delims);
	    	if (!ptr && bu_vls_strlen(&line) != 0) ptr = bu_vls_addr(&line) + bu_vls_strlen(&line);
	    	printf("pointers: %p, %p\n", bu_vls_addr(&line), ptr);
	    	bu_vls_trunc(&name_v5, 0);
	    	if (ptr != NULL && (bu_vls_addr(&line) - ptr != 0)) {
		    bu_vls_strncpy(&name_v5, bu_vls_addr(&line), ptr - bu_vls_addr(&line));
 	            printf("name: %s\n", bu_vls_addr(&name_v5)); 
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
/*    return node_count;*/
    return -1;  /* ensure check never passes until I get build_comb working CWY */
}


int
_ged_make_tree(struct ged *gedp, struct rt_comb_internal *comb, struct directory *dp, size_t node_count, const char *old_name, const char *new_name, struct rt_tree_array *rt_tree_array, int tree_index)
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

    if (strcmp(new_name, old_name)) {
	int flags;

	if (comb->region_flag)
	    flags = DIR_COMB | DIR_REGION;
	else
	    flags = DIR_COMB;

	if (dp != DIR_NULL) {
	    if (db_delete(gedp->ged_wdbp->dbip, dp) || db_dirdelete(gedp->ged_wdbp->dbip, dp)) {
		bu_vls_printf(&gedp->ged_result_str, "_ged_make_tree: Unable to delete directory entry for %s\n", old_name);
		intern.idb_meth->ft_ifree(&intern);
		return GED_ERROR;
	    }
	}

	if ((dp=db_diradd(gedp->ged_wdbp->dbip, new_name, RT_DIR_PHONY_ADDR, 0, flags, (genptr_t)&intern.idb_type)) == DIR_NULL) {
	    bu_vls_printf(&gedp->ged_result_str, "_ged_make_tree: Cannot add %s to directory, no changes made\n", new_name);
	    intern.idb_meth->ft_ifree(&intern);
	    return 1;
	}
    } else if (dp == DIR_NULL) {
	int flags;

	if (comb->region_flag)
	    flags = DIR_COMB | DIR_REGION;
	else
	    flags = DIR_COMB;

	if ((dp=db_diradd(gedp->ged_wdbp->dbip, new_name, RT_DIR_PHONY_ADDR, 0, flags, (genptr_t)&intern.idb_type)) == DIR_NULL) {
	    bu_vls_printf(&gedp->ged_result_str, "_ged_make_tree: Cannot add %s to directory, no changes made\n", new_name);
	    intern.idb_meth->ft_ifree(&intern);
	    return GED_ERROR;
	}
    } else {
	if (comb->region_flag)
	    dp->d_flags |= DIR_REGION;
	else
	    dp->d_flags &= ~DIR_REGION;
    }

    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "_ged_make_tree: Unable to write new combination into database.\n");
	return GED_ERROR;
    }

    return GED_OK;
}


HIDDEN int
build_comb(struct ged *gedp, struct rt_comb_internal *comb, struct directory *dp, size_t node_count, char *old_name)
{
    /* Build the new combination by adding to the recently emptied combination
       This keeps combo info associated with this combo intact */

    FILE *fp;
    char relation;
    char *name=NULL, *new_name;
    char name_v4[NAMESIZE+1];
    char new_name_v4[NAMESIZE+1];
    char *ptr;
    int ch;
    int i;
    int done=0;
    int done2;
    struct rt_tree_array *rt_tree_array;
    int tree_index=0;
    union tree *tp;
    matp_t matrix;
    int ret=0;
    struct bu_vls line;
    struct bu_vls attr_vls;
    struct bu_vls val_vls;
    struct bu_attribute_value_set avs;

    if (gedp->ged_wdbp->dbip == DBI_NULL) 
	return GED_OK;

    if (comb) {
	RT_CK_COMB(comb);
	RT_CK_DIR(dp);
    }

    if ((fp=fopen(_ged_tmpfil, "r")) == NULL) {
	bu_vls_printf(&gedp->ged_result_str, "build_comb: Cannot open edited file: %s\n", _ged_tmpfil);
	return GED_ERROR;
    }

    bu_vls_init(&line);
    bu_vls_init(&attr_vls);
    bu_vls_init(&val_vls);
    bu_avs_init_empty(&avs);

    /* empty the existing combination */
    if (comb && comb->tree) {
	db_free_tree(comb->tree, &rt_uniresource);
	comb->tree = NULL;
    } else {
	/* make an empty combination structure */
	BU_GETSTRUCT(comb, rt_comb_internal);
	comb->magic = RT_COMB_MAGIC;
	comb->tree = TREE_NULL;
	bu_vls_init(&comb->shader);
	bu_vls_init(&comb->material);
    }

    /* build tree list */
    if (node_count)
	rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list");
    else
	rt_tree_array = (struct rt_tree_array *)NULL;

    if (gedp->ged_wdbp->dbip->dbi_version < 5) {
	if (dp == DIR_NULL)
	    NAMEMOVE(old_name, new_name_v4);
	else
	    NAMEMOVE(dp->d_namep, new_name_v4);
	new_name = new_name_v4;
    } else {
	if (dp == DIR_NULL)
	    new_name = bu_strdup(old_name);
	else
	    new_name = bu_strdup(dp->d_namep);
    }

    /* Read edited file */
    while (bu_vls_gets(&line, fp) != -1) {
	if (strcmp(bu_vls_addr(&line), "combination tree:")) {
	    get_attr_val_pair(bu_vls_addr(&line), &attr_vls, &val_vls); 
	    (void)bu_avs_add(&avs, bu_vls_addr(&attr_vls), bu_vls_addr(&val_vls));
	    continue;
        }

        /* got all the AV pairs, update and proceed to tree */
	db5_update_attributes(dp, &avs, gedp->ged_wdbp->dbip);
        bu_avs_print(&avs, "After build update\n");
	db5_apply_std_attributes(gedp->ged_wdbp->dbip, dp, comb);

	done2=0;
	ptr = strtok(bu_vls_addr(&line), _delims);
	while (!done2) {
	    if (!ptr)
		break;

	    /* First non-white is the relation operator */
	    relation = (*ptr);
	    if (relation == '\0')
		break;

	    /* Next must be the member name */
	    ptr = strtok((char *)NULL, _delims);
	    if (gedp->ged_wdbp->dbip->dbi_version < 5) {
		bu_strlcpy(name_v4 , ptr, NAMESIZE+1);
		name = name_v4;
	    } else {
		if (name)
		    bu_free(name, "name");
		name = bu_strdup(ptr);
	    }

	    /* Eliminate trailing white space from name */
	    if (gedp->ged_wdbp->dbip->dbi_version < 5)
		i = NAMESIZE;
	    else
		i = (int)strlen(name);
	    while (isspace(name[--i]))
		name[i] = '\0';

	    /* Check for existence of member */
	    if ((db_lookup(gedp->ged_wdbp->dbip, name, LOOKUP_QUIET)) == DIR_NULL)
		bu_vls_printf(&gedp->ged_result_str, "\tWARNING: '%s' does not exist\n", name);
	    /* get matrix */
	    ptr = strtok((char *)NULL, _delims);
	    if (!ptr) {
		matrix = (matp_t)NULL;
		done2 = 1;
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
			bu_vls_printf(&gedp->ged_result_str, "build_comb: incomplete matrix for member %s. No changes made\n", name);
			bu_free((char *)matrix, "red: matrix");
			if (rt_tree_array)
			    bu_free((char *)rt_tree_array, "red: tree list");
			fclose(fp);
			bu_vls_free(&attr_vls);
		        bu_vls_free(&val_vls);
			bu_avs_free(&avs);
			return 1;
		    }
		    matrix[k] = atof(ptr);
		}
		if (bn_mat_is_identity(matrix)) {
		    bu_free((char *)matrix, "red: matrix");
		    matrix = (matp_t)NULL;
		}

		ptr = strtok((char *)NULL, _delims);
		if (ptr == (char *)NULL)
		    done2 = 1;
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
	    tp->tr_l.tl_name = bu_strdup(name);
	    tp->tr_l.tl_mat = matrix;
	    tree_index++;
	}
    }

    fclose(fp);

    ret = _ged_make_tree(gedp, comb, dp, node_count, old_name, new_name, rt_tree_array, tree_index);

    if (gedp->ged_wdbp->dbip->dbi_version >= 5) {
	if (name)
	    bu_free(name, "name ");
	bu_free(new_name, "new_name");
    }

    bu_vls_free(&attr_vls);
    bu_vls_free(&val_vls);
    bu_avs_free(&avs);
    return ret;
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
write_comb(struct ged *gedp, const struct rt_comb_internal *comb, const char *name)
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

    GED_DB_LOOKUP(gedp, dp, name, LOOKUP_QUIET, GED_ERROR);

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
    printf("maxlength: %d\n", maxlength);
	
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
	    fprintf(fp, "%s%s= \n", standard_attributes[i], bu_vls_addr(&spacer));
        }
	fprintf(fp, "combination tree:\n");
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
    printf("node_count: %d\n", node_count);
    if (node_count > 0) {
	rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list");
	actual_count = (struct rt_tree_array *)db_flatten_tree(rt_tree_array, comb->tree, OP_UNION, 0, &rt_uniresource) - rt_tree_array;
	printf("actual_count: %d\n", actual_count);
	BU_ASSERT_SIZE_T(actual_count, ==, node_count);
    } else {
	rt_tree_array = (struct rt_tree_array *)NULL;
	actual_count = 0;
    }

    db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp);
    bu_avs_print(&avs, "Initial");
    db5_apply_std_attributes(gedp->ged_wdbp->dbip, dp, comb);
    db5_update_std_attributes(gedp->ged_wdbp->dbip, dp, comb);
    if (!db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
        bu_avs_print(&avs, "After db5_apply_std_attributes followed by db5_update_std_attributes");
        db5_standardize_avs(&avs);
       	avpp = avs.avp;
        for (i=0; i < avs.count; i++, avpp++) {
	    if (strlen(avpp->name) > maxlength) 
		maxlength = strlen(avpp->name);
        }
	printf("maxlength: %d\n", maxlength);
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

    fprintf(fp, "combination tree:\n");

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
	if (db_lookup(gedp->ged_wdbp->dbip, str, LOOKUP_QUIET) == DIR_NULL)
	    done = 1;
	else
	    counter++;
    }

    return name;
}


const char *
_ged_save_comb(struct ged *gedp, struct directory *dpold)
{
    /* Save a combination under a temporory name */

    struct directory *dp;
    struct rt_db_internal intern;

    /* Make a new name */
    const char *name = mktemp_comb(gedp, _ged_tmpcomb);

    if (rt_db_get_internal(&intern, dpold, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "_ged_save_comb: Database read error, aborting\n");
	return NULL;
    }

    if ((dp = db_diradd(gedp->ged_wdbp->dbip, name, RT_DIR_PHONY_ADDR, 0, dpold->d_flags, (genptr_t)&intern.idb_type)) == DIR_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "_ged_save_comb: Cannot save copy of %s, no changed made\n", dpold->d_namep);
	return NULL;
    }

    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "_ged_save_comb: Cannot save copy of %s, no changed made\n", dpold->d_namep);
	return NULL;
    }

    return name;
}


/* restore a combination that was created during "_ged_save_comb" */
void
_ged_restore_comb(struct ged *gedp, struct directory *dp, const char *oldname)
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


int
ged_red(struct ged *gedp, int argc, const char *argv[])
{
    FILE *fp;
    int c;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    size_t node_count;
    static const char *usage = "comb";
    const char *editstring = NULL;

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

    GED_DB_LOOKUP(gedp, dp, argv[1], LOOKUP_QUIET, GED_ERROR);
    GED_CHECK_COMB(gedp, dp, GED_ERROR);

    if (dp != DIR_NULL) {
	if (!(dp->d_flags & DIR_COMB)) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: %s must be a combination to use this command\n", argv[0], argv[1]);
	    return GED_ERROR;
	}

	GED_DB_GET_INTERNAL(gedp, &intern, dp, (fastf_t *)NULL, &rt_uniresource, GED_ERROR);
	comb = (struct rt_comb_internal *)intern.idb_ptr;

	/* Make a file for the text editor */
	fp = bu_temp_file(_ged_tmpfil, MAXPATHLEN);

	if (fp == (FILE *)0) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: unable to edit %s\n", argv[0], argv[1]);
	    bu_vls_printf(&gedp->ged_result_str, "%s: unable to create %s\n", argv[0], _ged_tmpfil);
	    return GED_ERROR;
	}

	/* Write the combination components to the file */
	if (write_comb(gedp, comb, dp->d_namep)) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: unable to edit %s\n", argv[0], argv[1]);
	    unlink(_ged_tmpfil);
	    return GED_ERROR;
	}
    } else {
	comb = (struct rt_comb_internal *)NULL;

	/* Make a file for the text editor */
	fp = bu_temp_file(_ged_tmpfil, MAXPATHLEN);

	if (fp == (FILE *)0) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: unable to edit %s\n", argv[0], argv[1]);
	    bu_vls_printf(&gedp->ged_result_str, "%s: unable to create %s\n", argv[0], _ged_tmpfil);
	    return GED_ERROR;
	}

	/* Write the combination components to the file */
	if (write_comb(gedp, comb, argv[1])) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: unable to edit %s\n", argv[0], argv[1]);
	    unlink(_ged_tmpfil);
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
	    const char *saved_name = NULL;
	    int checked;

	    checked = check_comb(gedp, comb, dp);
	    if (checked < 0) {
		/* Do some quick checking on the edited file */
		bu_vls_printf(&gedp->ged_result_str, "%s: Error in edited region, no changes made\n", *argv);
		if (comb)
		    intern.idb_meth->ft_ifree(&intern);
		(void)unlink(_ged_tmpfil);
		return GED_ERROR;
	    }
	    node_count = (size_t)checked;

	    if (comb) {
		saved_name = _ged_save_comb(gedp, dp);
		if (saved_name == NULL) {
		    /* Unabled to save combination to a temp name */
		    bu_vls_printf(&gedp->ged_result_str, "%s: No changes made\n", *argv);
		    intern.idb_meth->ft_ifree(&intern);
		    (void)unlink(_ged_tmpfil);
		    return GED_OK;
		}
	    }

	    if (build_comb(gedp, comb, dp, node_count, (char *)argv[1])) {
		bu_vls_printf(&gedp->ged_result_str, "%s: Unable to construct new %s", *argv, dp->d_namep);
		if (comb) {
		    _ged_restore_comb(gedp, dp, saved_name);
		    bu_vls_printf(&gedp->ged_result_str, "%s: \toriginal restored\n", *argv);
		    intern.idb_meth->ft_ifree(&intern);
		}

		(void)unlink(_ged_tmpfil);
		return GED_ERROR;
	    } else if (comb && saved_name != NULL) {
		/* eliminate the temporary combination */
		const char *av[3];

		av[0] = "kill";
		av[1] = saved_name;
		av[2] = NULL;
		(void)ged_kill(gedp, 2, (const char **)av);
	    }
	} else {
	    bu_vls_printf(&gedp->ged_result_str, "%s: Because the database is READ-ONLY no changes were made.\n", *argv);
	}
    }

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

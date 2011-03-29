/*                        R E D . C
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
/** @file red.c
 *
 * The red command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <regex.h>
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
	if (ZERO(tmp - matrix[k]))
	    fprintf(fp, " %g", matrix[k]);
	else
	    fprintf(fp, " %.12e", matrix[k]);
	if ((k&3)==3) fputc(' ', fp);
    }
}


int
_ged_find_matrix(struct ged *gedp, const char *currptr, int strlength, matp_t *matrix, int *name_end)
{
    int ret = 1;
    regex_t matrix_entry, full_matrix, whitespace_regex;
    regmatch_t *float_locations;
    struct bu_vls current_substring, matrix_substring;
    int floatcnt, tail_start;
    const char *floatptr; 
    const char *float_string = "[+-]?[0-9]*[.]?[0-9]+([eE][+-]?[0-9]+)?";

    bu_vls_init(&current_substring);
    bu_vls_init(&matrix_substring);
    bu_vls_sprintf(&current_substring, "(%s[[:space:]]+)", float_string);
    regcomp(&matrix_entry, bu_vls_addr(&current_substring), REG_EXTENDED);
    bu_vls_sprintf(&current_substring, "[[:space:]]+(%s[[:space:]]+){15}(%s)", float_string, float_string);
    regcomp(&full_matrix, bu_vls_addr(&current_substring), REG_EXTENDED);
    regcomp(&whitespace_regex, "([^[:blank:]])", REG_EXTENDED);
    
    float_locations = (regmatch_t *)bu_calloc(full_matrix.re_nsub, sizeof(regmatch_t), "array to hold answers from regex");
 
    floatcnt = 0;
    float_locations[0].rm_so = 0;
    float_locations[0].rm_eo = strlength;
    while (floatcnt < 16 && floatcnt >= 0) {
	if (regexec(&matrix_entry, currptr, matrix_entry.re_nsub, float_locations, REG_STARTEND) == 0) {
	    /* matched */
	    floatcnt++;
	    float_locations[0].rm_so = float_locations[0].rm_eo;
	    float_locations[0].rm_eo = strlength;
	} else {
	    floatcnt = -1 * floatcnt - 1;
	}
    }
    if (floatcnt >= 16) {
	/* Possible matrix - use matrix regex to locate it */
	float_locations[0].rm_so = 0;
	float_locations[0].rm_eo = strlength;
	if (regexec(&full_matrix, currptr, full_matrix.re_nsub, float_locations, REG_STARTEND) == 0) {
	    /* matched */
	    bu_vls_trunc(&matrix_substring, 0);
	    bu_vls_strncpy(&matrix_substring, currptr + float_locations[0].rm_so, float_locations[0].rm_eo - float_locations[0].rm_so);
	    *name_end = float_locations[0].rm_so;
	    tail_start = float_locations[0].rm_eo;
	    (*matrix) = (matp_t)bu_calloc(16, sizeof(fastf_t), "red: matrix");
	    floatptr = bu_vls_addr(&matrix_substring);
	    floatcnt = 0;
	    float_locations[0].rm_so = 0;
	    float_locations[0].rm_eo = bu_vls_strlen(&matrix_substring);
	    while (floatcnt < 16) {
		if (regexec(&matrix_entry, floatptr, matrix_entry.re_nsub, float_locations, REG_STARTEND) == 0) {
		    /* matched */
		    bu_vls_trunc(&current_substring, 0);
		    bu_vls_strncpy(&current_substring, currptr + float_locations[0].rm_so, float_locations[0].rm_eo - float_locations[0].rm_so);
		    (*matrix)[floatcnt] = atof(floatptr); 
		    floatptr = floatptr + float_locations[0].rm_eo;
		    float_locations[0].rm_so = 0;
		    float_locations[0].rm_eo = strlen(floatptr);
		    floatcnt++;
		} else {
		    bu_vls_sprintf(&current_substring, "%s", floatptr);
		    (*matrix)[floatcnt] = atof(bu_vls_addr(&current_substring)); 
		    floatcnt++;
		}
	    }
	    bu_vls_free(&matrix_substring);
	    bu_vls_trunc(&current_substring, 0);
	    bu_vls_strncpy(&current_substring, currptr + tail_start, strlength - tail_start - 1);
	    /* Need to check for non-whitespace in the distance-from-end zone */
	    if (regexec(&whitespace_regex, bu_vls_addr(&current_substring), whitespace_regex.re_nsub, float_locations, 0) == 0) {
		/* matched */
	        bu_vls_printf(&gedp->ged_result_str, "Saw something other than whitespace after matrix - error!\n");
		ret = -1;
	    } else {
		ret = 0;
	    }
	} else {
	    bu_vls_printf(&gedp->ged_result_str, "Yikes!  Found 16 or more float matches in a comb string but no valid matrix!!\n");
	    ret = -1;
	}
    }

    if (floatcnt < -1 && (floatcnt + 1) < -4) {
	bu_vls_printf(&gedp->ged_result_str, "More than 4 floats found without a matrix present - possible invalid matrix?\n");
	ret = -1;
    }

    /* cleanup */
    bu_free(float_locations, "free float_locations");
    bu_vls_free(&current_substring);
    regfree(&matrix_entry);
    regfree(&full_matrix);
    regfree(&whitespace_regex);

    return ret;
}


HIDDEN int
build_comb(struct ged *gedp, struct directory *dp)
{
    struct rt_comb_internal *comb;
    size_t node_count=0;
    int nonsubs=0;
    union tree *tp;
    int tree_index=0;
    struct rt_db_internal intern, localintern;
    struct rt_tree_array *rt_tree_array;
    const char *currptr; 
    regex_t whitespace_regex, attr_regex, combtree_regex, combtree_op_regex;
    regmatch_t *result_locations;
    struct bu_vls current_substring, attr_vls, val_vls, curr_op_vls, next_op_vls;
    struct bu_mapped_file *redtmpfile;
    int attrstart, attrend, attrcumulative, name_start, name_end, treecumulative;
    int ret, gedret, combtagstart, combtagend;
    struct bu_attribute_value_set avs;
    matp_t matrix;
    const char *combtree_header = "-------------Combination Tree------------:\r?\n";
 
    rt_tree_array = (struct rt_tree_array *)NULL;
 
    /* Standard sanity checks */
    if (gedp->ged_wdbp->dbip == DBI_NULL)
	return -1;

    GED_DB_GET_INTERNAL(gedp, &intern, dp, (fastf_t *)NULL, &rt_uniresource, GED_ERROR);
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb) {
	RT_CK_COMB(comb);
	RT_CK_DIR(dp);
    }
    bu_vls_init(&current_substring);

    /* Map the temp file for reading */
    redtmpfile = bu_open_mapped_file(_ged_tmpfil, (char *)NULL);
    if (!redtmpfile) {
	bu_vls_printf(&gedp->ged_result_str, "Cannot open temporary file %s\n", _ged_tmpfil);
	return -1;
    }

    /* Set up the regular expressions */
    regcomp(&whitespace_regex, "([^[:space:]])", REG_EXTENDED);
    regcomp(&attr_regex, "(.+[[:space:]]+=[[:blank:]]+.*)", REG_EXTENDED|REG_NEWLINE);
    bu_vls_sprintf(&current_substring, "(%s)", combtree_header);
    regcomp(&combtree_regex, bu_vls_addr(&current_substring), REG_EXTENDED);
    regcomp(&combtree_op_regex, "([[:space:]]+[[.-.][.+.]u][[:space:]]+)", REG_EXTENDED);
    

    /* Need somewhere to hold the results - initially, size according to attribute regex */
    result_locations = (regmatch_t *)bu_calloc(attr_regex.re_nsub, sizeof(regmatch_t), "array to hold answers from regex");

    /* First thing, find the beginning of the tree definition.  Without that, file is invalid.  Even an empty comb must
     * include this header.
     */
    currptr = (const char *)(redtmpfile->buf);
    ret = regexec(&combtree_regex, currptr, combtree_regex.re_nsub , result_locations, 0);
    if (ret == 0) {
	/* matched */

	combtagstart = result_locations[0].rm_so;
	combtagend = result_locations[0].rm_eo;
	attrcumulative = 0;
	if (regexec(&combtree_regex, currptr + combtagend, combtree_regex.re_nsub, result_locations, 0) == 0) {
	    /* matched */

	    bu_vls_printf(&gedp->ged_result_str, "ERROR - multiple instances of comb tree header \"%s\" in temp file!", combtree_header);
	    bu_vls_printf(&gedp->ged_result_str, "cannot locate comb tree, aborting\n");
	    bu_vls_free(&current_substring);
	    regfree(&whitespace_regex);
	    regfree(&attr_regex);
	    regfree(&combtree_regex);
	    regfree(&combtree_op_regex);
	    bu_free(result_locations, "free regex results array\n");
	    bu_close_mapped_file(redtmpfile);

	    return -1;
	}
    } else {
	bu_vls_printf(&gedp->ged_result_str, "cannot locate comb tree, aborting\n");
	bu_vls_free(&current_substring);
	regfree(&whitespace_regex);
	regfree(&attr_regex);
	regfree(&combtree_regex);
	regfree(&combtree_op_regex);
	bu_free(result_locations, "free regex results array\n");
	bu_close_mapped_file(redtmpfile);

	return -1;
    }

    /* Parsing the file is handled in two stages - attributes and combination tree.  Start with attributes */
    bu_vls_init(&attr_vls);
    bu_vls_init(&val_vls);
    bu_avs_init_empty(&avs);
    while (attrcumulative < combtagstart - 1) {
	/* If attributes are present, the first line must match the attr regex - mult-line attribute names are not supported. */
	if (regexec(&attr_regex, currptr, attr_regex.re_nsub , result_locations, 0) != 0) {
	    /* did NOT match */

	    bu_vls_printf(&gedp->ged_result_str, "invalid attribute line\n");
	    bu_vls_free(&current_substring);
	    bu_vls_free(&attr_vls);
	    bu_vls_free(&val_vls);
	    regfree(&whitespace_regex);
	    regfree(&attr_regex);
	    regfree(&combtree_regex);
	    regfree(&combtree_op_regex);
	    bu_avs_free(&avs);
	    bu_free(result_locations, "free regex results array\n");
	    bu_close_mapped_file(redtmpfile);

	    return -1;
	} else {
	    /* matched */

	    /* If an attribute line is found, set the attr pointers and look for the next attribute, if any.  Multi-line attribute values 
	     * are supported, but only if the line does not itself match the format for an attribute (i.e. no equal sign 
	     * surrounded by spaces or tabs. 
	     */ 
	    attrstart = result_locations[0].rm_so;
	    attrend = result_locations[0].rm_eo;
	    attrcumulative += attrend;
	    if (regexec(&attr_regex, (const char *)(redtmpfile->buf) + attrcumulative, attr_regex.re_nsub , result_locations, 0) == 0) {
		/* matched */

		if (attrcumulative + result_locations[0].rm_eo < combtagstart) {
		    attrend += result_locations[0].rm_so - 1; 
		    attrcumulative += result_locations[0].rm_so - 1;
		} else {
		    attrend = attrend + (combtagstart - attrcumulative);
		    attrcumulative = combtagstart;
		}
	    } else {
		attrend = attrend + (combtagstart - attrcumulative);
		attrcumulative = combtagstart;
	    } 
	    bu_vls_trunc(&current_substring, 0);
	    bu_vls_strncpy(&current_substring, currptr + attrstart, attrend - attrstart);
	    if (get_attr_val_pair(bu_vls_addr(&current_substring), &attr_vls, &val_vls)) {
		if (!BU_STR_EQUAL(bu_vls_addr(&val_vls), "") && !BU_STR_EQUAL(bu_vls_addr(&attr_vls), "name"))
		    (void)bu_avs_add(&avs, bu_vls_addr(&attr_vls), bu_vls_addr(&val_vls)); 
	    }
	    currptr = currptr + attrend;
	}
    }

    db5_standardize_avs(&avs);
 
    bu_vls_free(&attr_vls);
    bu_vls_free(&val_vls);

    /* Now, the comb tree. First, count the number of operators - without at least one, the comb tree is empty
     * and we need to know how many there are before allocating rt_tree_array memory. */
    currptr = (const char *)(redtmpfile->buf) + combtagend;
    node_count = 0;
    ret = regexec(&combtree_op_regex, currptr, combtree_op_regex.re_nsub , result_locations, 0);
    while (ret == 0) {
	currptr = currptr + result_locations[0].rm_eo;
	ret = regexec(&combtree_op_regex, currptr, combtree_op_regex.re_nsub , result_locations, 0);
	node_count++;
    }
    currptr = (const char *)(redtmpfile->buf) + combtagend;
    treecumulative = 0;
    name_start = 0;
    name_end = 0;
    bu_vls_init(&curr_op_vls);
    bu_vls_init(&next_op_vls);

    ret = regexec(&combtree_op_regex, currptr, combtree_op_regex.re_nsub , result_locations, 0);
    if (ret == 0) {
	/* matched */

        /* Check for non-whitespace garbage between first operator and start of comb tree definition */
	result_locations[0].rm_eo = result_locations[0].rm_so;
	result_locations[0].rm_so = 0;
	if (regexec(&whitespace_regex, currptr, whitespace_regex.re_nsub, result_locations, REG_STARTEND) == 0) {
	    /* matched */

	    bu_vls_printf(&gedp->ged_result_str, "Saw something other than comb tree entries after comb tree tag - error!\n");
	    bu_vls_free(&current_substring);
	    bu_vls_free(&curr_op_vls);
	    bu_vls_free(&next_op_vls);
	    regfree(&whitespace_regex);
	    regfree(&attr_regex);
	    regfree(&combtree_regex);
	    regfree(&combtree_op_regex);
	    bu_avs_free(&avs);
	    bu_free(result_locations, "free regex results array\n");
	    bu_close_mapped_file(redtmpfile);

	    return GED_ERROR;
	}
        ret = regexec(&combtree_op_regex, currptr, combtree_op_regex.re_nsub , result_locations, 0);
	bu_vls_trunc(&next_op_vls, 0);
	bu_vls_strncpy(&next_op_vls, currptr + result_locations[0].rm_so, result_locations[0].rm_eo - result_locations[0].rm_so);
	bu_vls_trimspace(&next_op_vls);
	currptr = currptr + result_locations[0].rm_eo;
	rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list");
	/* As long as we have operators ahead of us in the tree, we have comb entries to handle */
	while (ret == 0) {
	    ret = regexec(&combtree_op_regex, currptr, combtree_op_regex.re_nsub , result_locations, 0);
	    bu_vls_sprintf(&curr_op_vls, "%s", bu_vls_addr(&next_op_vls));
	    if (ret == 0) {
		/* matched */
		bu_vls_trunc(&next_op_vls, 0);
		bu_vls_strncpy(&next_op_vls, currptr + result_locations[0].rm_so, result_locations[0].rm_eo - result_locations[0].rm_so);
		bu_vls_trimspace(&next_op_vls);
		name_end = result_locations[0].rm_so;
	    } else {
		name_end = strlen(currptr);
	    }
	    bu_vls_trunc(&current_substring, 0);
	    bu_vls_strncpy(&current_substring, currptr, name_end);
	    if (!bu_vls_strlen(&current_substring)) {
		bu_vls_printf(&gedp->ged_result_str, "Zero length substring\n");
		bu_vls_free(&current_substring);
		bu_vls_free(&curr_op_vls);
		bu_vls_free(&next_op_vls);
		regfree(&whitespace_regex);
		regfree(&attr_regex);
		regfree(&combtree_regex);
		regfree(&combtree_op_regex);
		bu_avs_free(&avs);
		bu_free(result_locations, "free regex results array\n");
		bu_close_mapped_file(redtmpfile);

		return GED_ERROR;

	    } 
	    /* We have a string - now check for a matrix and build it if present 
	     * Otherwise, set matrix to NULL */
	    gedret = _ged_find_matrix(gedp, currptr, name_end, &matrix, &name_end);
	    if (gedret) {
		matrix = (matp_t)NULL;
		if (gedret == -1) {
		    bu_vls_printf(&gedp->ged_result_str, "Problem parsing Matrix\n");
		    bu_vls_free(&current_substring);
		    bu_vls_free(&curr_op_vls);
		    bu_vls_free(&next_op_vls);
		    regfree(&whitespace_regex);
		    regfree(&attr_regex);
		    regfree(&combtree_regex);
		    regfree(&combtree_op_regex);
		    bu_avs_free(&avs);
		    bu_free(result_locations, "free regex results array\n");
		    bu_close_mapped_file(redtmpfile);
		    return GED_ERROR;
		}
	    }
	    bu_vls_trunc(&current_substring, 0);
	    bu_vls_strncpy(&current_substring, currptr, name_end);
	    bu_vls_trimspace(&current_substring);
	    currptr = currptr + result_locations[0].rm_eo;
	    if (bu_vls_addr(&curr_op_vls)[0] != '-')
		nonsubs++;

	    /* Add it to the combination */
	    switch (bu_vls_addr(&curr_op_vls)[0]) {
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
	    tp->tr_l.tl_name = bu_strdup(bu_vls_addr(&current_substring));
	    tp->tr_l.tl_mat = matrix;
	    tree_index++;

	}
    } else {
	/* Empty tree, ok as long as there is no garbage after the comb tree indicator */
	bu_vls_sprintf(&current_substring, "%s", currptr);
	if (regexec(&whitespace_regex, bu_vls_addr(&current_substring), whitespace_regex.re_nsub, result_locations, 0) == 0) {
	    /* matched */

	    bu_vls_printf(&gedp->ged_result_str, "Saw something other than comb tree entries after comb tree tag - error!\n");
	    bu_vls_free(&current_substring);
	    bu_vls_free(&curr_op_vls);
	    bu_vls_free(&next_op_vls);
	    regfree(&whitespace_regex);
	    regfree(&attr_regex);
	    regfree(&combtree_regex);
	    regfree(&combtree_op_regex);
	    bu_avs_free(&avs);
	    bu_free(result_locations, "free regex results array\n");
	    bu_close_mapped_file(redtmpfile);

	    return GED_ERROR;
	}
    }
    bu_vls_free(&current_substring);
    bu_vls_free(&curr_op_vls);
    bu_vls_free(&next_op_vls);
    regfree(&whitespace_regex);
    regfree(&attr_regex);
    regfree(&combtree_regex);
    regfree(&combtree_op_regex);

    bu_free(result_locations, "free regex results array\n");
    bu_close_mapped_file(redtmpfile);

/* Debugging print stuff */
/*
  bu_avs_print(&avs, "Regex based avs build\n");
  printf("\n");
  int i, m;
  fastf_t tmp; 
  for (i=0; i<tree_index; i++) {
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
  printf("write_comb: Illegal op code in tree\n");
  }
  printf(" %c %s\n", op, rt_tree_array[i].tl_tree->tr_l.tl_name);
  bn_mat_print("in rt_tree_array", rt_tree_array[i].tl_tree->tr_l.tl_mat);
  printf("\n");
  }
*/
    if (nonsubs == 0 && node_count) {
	bu_vls_printf(&gedp->ged_result_str, "Cannot create a combination with all subtraction operators\n");
	bu_avs_free(&avs);
	return GED_ERROR;
    }

    if (tree_index)
	tp = (union tree *)db_mkgift_tree(rt_tree_array, node_count, &rt_uniresource);
    else
	tp = (union tree *)NULL;

    if (comb) {
	db_free_tree(comb->tree, &rt_uniresource);
	comb->tree = NULL;
    }
    comb->tree = tp;

    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
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
	fprintf(fp, "-------------Combination Tree------------:\n");
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

    fprintf(fp, "-------------Combination Tree------------:\n");

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
    if (dp != RT_DIR_NULL) {

	/* Stash original primitive name and find appropriate temp name */
	bu_vls_sprintf(&comb_name, "%s", dp->d_namep);

	counter = 0;
	have_tmp_name = 0;
	while (!have_tmp_name) {
	    bu_vls_sprintf(&temp_name, "%s_red%d", dp->d_namep, counter);
	    if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&temp_name), LOOKUP_QUIET) == RT_DIR_NULL)
		have_tmp_name = 1;
	    else
		counter++;
	}
	if (!(dp->d_flags & RT_DIR_COMB)) {
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


		if ((tmp_dp = db_diradd(gedp->ged_wdbp->dbip, bu_vls_addr(&temp_name), RT_DIR_PHONY_ADDR, 0, RT_DIR_COMB, (genptr_t)&intern.idb_type)) == RT_DIR_NULL) {
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

		GED_DB_DIRADD(gedp, tmp_dp, bu_vls_addr(&temp_name), -1, 0, RT_DIR_COMB, (genptr_t)&intern.idb_type, 0);

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
		if (!BU_STR_EQUAL(bu_vls_addr(&comb_name), bu_vls_addr(&temp_name))) {
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

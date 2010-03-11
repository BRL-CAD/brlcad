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
	return((char *)NULL);

    /* find the '=' */
    ptr2 = strchr(ptr1, '=');
    if (!ptr2)
	return((char *)NULL);

    /* skip any white space before the value */
    while (isspace(*(++ptr2)));

    /* eliminate trailing white space */
    j = strlen(line);
    while (isspace(line[--j]));
    line[j+1] = '\0';

    /* return pointer to the value */
    return(ptr2);
}


static int
check_comb(struct ged *gedp)
{
    /* Do some minor checking of the edited file */

    FILE *fp;
    int node_count=0;
    int nonsubs=0;
    int i, j, done, ch;
    int done2, first;
    char relation;
    char name_v4[NAMESIZE+1];
    char *name_v5=NULL;
    char *name=NULL;
    char line[RT_MAXLINE] = {0};
    char lineCopy[RT_MAXLINE] = {0};
    char *ptr = (char *)NULL;
    int region=(-1);
    int id=0, air=0;
    int rgb_valid;

    if ((fp=fopen(_ged_tmpfil, "r")) == NULL) {
	perror("fopen");
	bu_vls_printf(&gedp->ged_result_str, "Cannot open temporary file for reading\n");
	return -1;
    }

    /* Read a line at a time */
    done = 0;
    while (!done) {
	/* Read a line */
	i = (-1);

	while ((ch=getc(fp)) != EOF && ch != '\n' && i < RT_MAXLINE)
	    line[++i] = ch;

	if (ch == EOF) {
	    /* We must be done */
	    done = 1;
	    if (i < 0)
		break;
	}
	if (i == RT_MAXLINE) {
	    line[RT_MAXLINE-1] = '\0';
	    bu_vls_printf(&gedp->ged_result_str, "Line too long in edited file:\n%s\n", line);
	    fclose(fp);
	    return(-1);
	}

	line[++i] = '\0';
	bu_strlcpy(lineCopy, line, RT_MAXLINE);

	/* skip leading white space */
	i = (-1);
	while (isspace(line[++i]));

	if (line[i] == '\0')
	    continue;	/* blank line */

	if ((ptr=find_keyword(i, line, "NAME"))) {
	    if (gedp->ged_wdbp->dbip->dbi_version < 5) {
		size_t len;

		len = strlen(ptr);
		if (len > NAMESIZE) {
		    while (len > 1 && isspace(ptr[len-1]))
			len--;
		}
		if (len > NAMESIZE) {
		    bu_vls_printf(&gedp->ged_result_str, "Name too long for v4 database: %s\n%s\n", ptr, lineCopy);
		    fclose(fp);
		    return(-1);
		}
	    }
	    continue;
	} else if ((ptr=find_keyword(i, line, "REGION_ID"))) {
	    id = atoi(ptr);
	    continue;
	} else if ((ptr=find_keyword(i, line, "REGION"))) {
	    if (*ptr == 'y' || *ptr == 'Y')
		region = 1;
	    else
		region = 0;
	    continue;
	} else if ((ptr=find_keyword(i, line, "AIRCODE"))) {
	    air = atoi(ptr);
	    continue;
	} else if ((ptr=find_keyword(i, line, "GIFT_MATERIAL"))) {
	    continue;
	} else if ((ptr=find_keyword(i, line, "LOS"))) {
	    continue;
	} else if ((ptr=find_keyword(i, line, "COLOR"))) {
	    char *ptr2;

	    rgb_valid = 1;
	    ptr2 = strtok(ptr, _delims);
	    if (!ptr2) {
		continue;
	    } else {
		ptr2 = strtok((char *)NULL, _delims);
		if (!ptr2) {
		    rgb_valid = 0;
		} else {
		    ptr2 = strtok((char *)NULL, _delims);
		    if (!ptr2) {
			rgb_valid = 0;
		    }
		}
	    }
	    if (!rgb_valid) {
		bu_vls_printf(&gedp->ged_result_str, "WARNING: invalid color specification!!! Must be three integers, each 0-255\n");
	    }
	    continue;
	} else if ((ptr=find_keyword(i, line, "SHADER")))
	    continue;
	else if ((ptr=find_keyword(i, line, "INHERIT")))
	    continue;
	else if (!strncmp(&line[i], "COMBINATION:", 12)) {
	    if (region < 0) {
		bu_vls_printf(&gedp->ged_result_str, "Region flag not correctly set\n\tMust be 'Yes' or 'No'\n\tNo changes made\n");
		fclose(fp);
		return(-1);
	    } else if (region) {
		if (id < 0) {
		    bu_vls_printf(&gedp->ged_result_str, "invalid region ID\n\tNo changed made\n");
		    fclose(fp);
		    return(-1);
		}
		if (air < 0) {
		    bu_vls_printf(&gedp->ged_result_str, "invalid air code\n\tNo changed made\n");
		    fclose(fp);
		    return(-1);
		}
		if (air == 0 && id == 0)
		    bu_vls_printf(&gedp->ged_result_str, "Warning: both ID and Air codes are 0!!!\n");
		if (air && id)
		    bu_vls_printf(&gedp->ged_result_str, "Warning: both ID and Air codes are non-zero!!!\n");
	    }
	    continue;
	}

	done2=0;
	first=1;
	ptr = strtok(line, _delims);

	while (!done2) {
	    if (name_v5) {
		bu_free(name_v5, "name_v5");
		name_v5 = NULL;
	    }
	    /* First non-white is the relation operator */
	    if (!ptr) {
		done2 = 1;
		break;
	    }

	    relation = (*ptr);
	    if (relation == '\0') {
		if (first)
		    done = 1;

		done2 = 1;
		break;
	    }
	    first = 0;

	    /* Next must be the member name */
	    ptr = strtok((char *)NULL, _delims);
	    name = NULL;
	    if (ptr != NULL && *ptr != '\0') {
		if (gedp->ged_wdbp->dbip->dbi_version < 5) {
		    bu_strlcpy(name_v4 , ptr , NAMESIZE+1);

		    /* Eliminate trailing white space from name */
		    j = NAMESIZE;
		    while (isspace(name_v4[--j]))
			name_v4[j] = '\0';
		    name = name_v4;
		} else {
		    size_t len;

		    len = strlen(ptr);
		    name_v5 = (char *)bu_malloc(len+1, "name_v5");
		    bu_strlcpy(name_v5, ptr, len+1);
		    while (isspace(name_v5[len-1])) {
			len--;
			name_v5[len] = '\0';
		    }
		    name = name_v5;
		}
	    }

	    if (relation != '+' && relation != 'u' && relation != '-') {
		bu_vls_printf(&gedp->ged_result_str, " %c is not a legal operator\n", relation);
		fclose(fp);
		if (gedp->ged_wdbp->dbip->dbi_version >= 5 && name_v5)
		    bu_free(name_v5, "name_v5");
		return(-1);
	    }

	    if (relation != '-')
		nonsubs++;

	    if (name == NULL || name[0] == '\0') {
		bu_vls_printf(&gedp->ged_result_str, " operand name missing\n%s\n", lineCopy);
		fclose(fp);
		if (gedp->ged_wdbp->dbip->dbi_version >= 5 && name_v5)
		    bu_free(name_v5, "name_v5");
		return(-1);
	    }

	    ptr = strtok((char *)NULL, _delims);
	    if (!ptr)
		done2 = 1;
	    else if (*ptr != 'u' &&
		     (*ptr != '-' || *(ptr+1) != '\0') &&
		     (*ptr != '+' || *(ptr+1) != '\0')) {
		int k;

		/* skip past matrix */
		for (k=1; k<16; k++) {
		    ptr = strtok((char *)NULL, _delims);
		    if (!ptr) {
			bu_vls_printf(&gedp->ged_result_str, "incomplete matrix\n%s\n", lineCopy);
			fclose(fp);
			if (gedp->ged_wdbp->dbip->dbi_version >= 5 && name_v5)
			    bu_free(name_v5, "name_v5");
			return(-1);
		    }
		}

		/* get the next relational operator on the current line */
		ptr = strtok((char *)NULL, _delims);
	    }

	    node_count++;
	}
    }

    if (gedp->ged_wdbp->dbip->dbi_version >= 5 && name_v5)
	bu_free(name_v5, "name_v5");

    fclose(fp);

    if (nonsubs == 0 && node_count) {
	bu_vls_printf(&gedp->ged_result_str, "Cannot create a combination with all subtraction operators\n");
	return(-1);
    }
    return(node_count);
}


int
_ged_make_tree(struct ged *gedp, struct rt_comb_internal *comb, struct directory *dp, int node_count, const char *old_name, const char *new_name, struct rt_tree_array *rt_tree_array, int tree_index)
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

	if ((dp=db_diradd(gedp->ged_wdbp->dbip, new_name, -1L, 0, flags, (genptr_t)&intern.idb_type)) == DIR_NULL) {
	    bu_vls_printf(&gedp->ged_result_str, "_ged_make_tree: Cannot add %s to directory, no changes made\n", new_name);
	    intern.idb_meth->ft_ifree(&intern);
	    return(1);
	}
    } else if (dp == DIR_NULL) {
	int flags;

	if (comb->region_flag)
	    flags = DIR_COMB | DIR_REGION;
	else
	    flags = DIR_COMB;

	if ((dp=db_diradd(gedp->ged_wdbp->dbip, new_name, -1L, 0, flags, (genptr_t)&intern.idb_type)) == DIR_NULL) {
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


static int
build_comb(struct ged *gedp, struct rt_comb_internal *comb, struct directory *dp, int node_count, char *old_name)
{
    /* Build the new combination by adding to the recently emptied combination
       This keeps combo info associated with this combo intact */

    FILE *fp;
    char relation;
    char *name=NULL, *new_name;
    char name_v4[NAMESIZE+1];
    char new_name_v4[NAMESIZE+1];
    char line[RT_MAXLINE] = {0};
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
    while (!done) {
	/* Read a line */
	i = (-1);

	while ((ch=getc(fp)) != EOF && ch != '\n' && i < RT_MAXLINE)
	    line[++i] = ch;

	if (ch == EOF) {
	    /* We must be done */
	    done = 1;
	    if (i < 0)
		break;
	}

	line[++i] = '\0';

	/* skip leading white space */
	i = (-1);
	while (isspace(line[++i]));

	if (line[i] == '\0')
	    continue;	/* blank line */

	if ((ptr=find_keyword(i, line, "NAME"))) {
	    if (gedp->ged_wdbp->dbip->dbi_version < 5)
		NAMEMOVE(ptr, new_name_v4);
	    else {
		bu_free(new_name, "new_name");
		new_name = bu_strdup(ptr);
	    }
	    continue;
	} else if ((ptr=find_keyword(i, line, "REGION_ID"))) {
	    comb->region_id = atoi(ptr);
	    continue;
	} else if ((ptr=find_keyword(i, line, "REGION"))) {
	    if (*ptr == 'y' || *ptr == 'Y')
		comb->region_flag = 1;
	    else
		comb->region_flag = 0;
	    continue;
	} else if ((ptr=find_keyword(i, line, "AIRCODE"))) {
	    comb->aircode = atoi(ptr);
	    continue;
	} else if ((ptr=find_keyword(i, line, "GIFT_MATERIAL"))) {
	    comb->GIFTmater = atoi(ptr);
	    continue;
	} else if ((ptr=find_keyword(i, line, "LOS"))) {
	    comb->los = atoi(ptr);
	    continue;
	} else if ((ptr=find_keyword(i, line, "COLOR"))) {
	    char *ptr2;
	    int value;

	    ptr2 = strtok(ptr, _delims);
	    if (!ptr2) {
		comb->rgb_valid = 0;
		continue;
	    }
	    value = atoi(ptr2);
	    if (value < 0) {
		bu_vls_printf(&gedp->ged_result_str, "build_comb: Red value less than 0, assuming 0\n");
		value = 0;
	    }
	    if (value > 255) {
		bu_vls_printf(&gedp->ged_result_str, "build_comb: Red value greater than 255, assuming 255\n");
		value = 255;
	    }
	    comb->rgb[0] = value;
	    ptr2 = strtok((char *)NULL, _delims);
	    if (!ptr2) {
		bu_vls_printf(&gedp->ged_result_str, "build_comb: Invalid RGB value\n");
		comb->rgb_valid = 0;
		continue;
	    }
	    value = atoi(ptr2);
	    if (value < 0) {
		bu_vls_printf(&gedp->ged_result_str, "build_comb: Green value less than 0, assuming 0\n");
		value = 0;
	    }
	    if (value > 255) {
		bu_vls_printf(&gedp->ged_result_str, "build_comb: Green value greater than 255, assuming 255\n");
		value = 255;
	    }
	    comb->rgb[1] = value;
	    ptr2 = strtok((char *)NULL, _delims);
	    if (!ptr2) {
		bu_vls_printf(&gedp->ged_result_str, "build_comb: Invalid RGB value\n");
		comb->rgb_valid = 0;
		continue;
	    }
	    value = atoi(ptr2);
	    if (value < 0) {
		bu_vls_printf(&gedp->ged_result_str, "build_comb: Blue value less than 0, assuming 0\n");
		value = 0;
	    }
	    if (value > 255) {
		bu_vls_printf(&gedp->ged_result_str, "build_comb: Blue value greater than 255, assuming 255\n");
		value = 255;
	    }
	    comb->rgb[2] = value;
	    comb->rgb_valid = 1;
	    continue;
	} else if ((ptr=find_keyword(i, line, "SHADER"))) {
	    bu_vls_strcpy(&comb->shader,  ptr);
	    continue;
	} else if ((ptr=find_keyword(i, line, "INHERIT"))) {
	    if (*ptr == 'y' || *ptr == 'Y')
		comb->inherit = 1;
	    else
		comb->inherit = 0;
	    continue;
	} else if (!strncmp(&line[i], "COMBINATION:", 12))
	    continue;

	done2=0;
	ptr = strtok(line, _delims);
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
			return(1);
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


static int
write_comb(struct ged *gedp, const struct rt_comb_internal *comb, const char *name)
{
    /* Writes the file for later editing */
    struct rt_tree_array *rt_tree_array;
    FILE *fp;
    int i;
    int node_count;
    int actual_count;

    if (comb)
	RT_CK_COMB(comb);

    /* open the file */
    if ((fp=fopen(_ged_tmpfil, "w")) == NULL) {
	perror("fopen");
	bu_vls_printf(&gedp->ged_result_str, "write_comb: Cannot open temporary file for writing\n");
	return GED_ERROR;
    }

    if (!comb) {
	fprintf(fp, "NAME=%s\n", name);
	fprintf(fp, "REGION=No\n");
	fprintf(fp, "REGION_ID=\n");
	fprintf(fp, "AIRCODE=\n");
	fprintf(fp, "GIFT_MATERIAL=\n");
	fprintf(fp, "LOS=\n");
	fprintf(fp, "COLOR=\n");
	fprintf(fp, "SHADER=\n");
	fprintf(fp, "INHERIT=No\n");
	fprintf(fp, "COMBINATION:\n");
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
	rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count,
							  sizeof(struct rt_tree_array), "tree list");
	actual_count = (struct rt_tree_array *)db_flatten_tree(rt_tree_array,
							       comb->tree,
							       OP_UNION,
							       0,
							       &rt_uniresource) - rt_tree_array;
	BU_ASSERT_LONG(actual_count, ==, node_count);
    } else {
	rt_tree_array = (struct rt_tree_array *)NULL;
	actual_count = 0;
    }

    fprintf(fp, "NAME=%s\n", name);
    if (comb->region_flag) {
	fprintf(fp, "REGION=Yes\n");
	fprintf(fp, "REGION_ID=%ld\n", comb->region_id);
	fprintf(fp, "AIRCODE=%ld\n", comb->aircode);
	fprintf(fp, "GIFT_MATERIAL=%ld\n", comb->GIFTmater);
	fprintf(fp, "LOS=%ld\n", comb->los);
    } else {
	fprintf(fp, "REGION=No\n");
	fprintf(fp, "REGION_ID=\n");
	fprintf(fp, "AIRCODE=\n");
	fprintf(fp, "GIFT_MATERIAL=\n");
	fprintf(fp, "LOS=\n");
    }

    if (comb->rgb_valid)
	fprintf(fp, "COLOR= %d %d %d\n", V3ARGS(comb->rgb));
    else
	fprintf(fp, "COLOR=\n");

    fprintf(fp, "SHADER=%s\n", bu_vls_addr(&comb->shader));

    if (comb->inherit)
	fprintf(fp, "INHERIT=Yes\n");
    else
	fprintf(fp, "INHERIT=No\n");

    fprintf(fp, "COMBINATION:\n");

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
		return GED_ERROR;
	}
	if (fprintf(fp, " %c %s", op, rt_tree_array[i].tl_tree->tr_l.tl_name) <= 0) {
	    bu_vls_printf(&gedp->ged_result_str, "write_comb: Cannot write to temporary file (%s). Aborting edit\n",
			  _ged_tmpfil);
	    fclose(fp);
	    return GED_ERROR;
	}
	_ged_print_matrix(fp, rt_tree_array[i].tl_tree->tr_l.tl_mat);
	fprintf(fp, "\n");
    }
    fclose(fp);
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

    if ((dp = db_diradd(gedp->ged_wdbp->dbip, name, -1L, 0, dpold->d_flags, (genptr_t)&intern.idb_type)) == DIR_NULL) {
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
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int node_count;
    static const char *usage = "comb";
    const char *editstring;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* First, grab the editstring off of the argv list */
    editstring = argv[0];
    argc--;
    argv++;
    
    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
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

	    if ((node_count = check_comb(gedp)) < 0) {
		/* Do some quick checking on the edited file */
		bu_vls_printf(&gedp->ged_result_str, "%s: Error in edited region, no changes made\n", *argv);
		if (comb)
		    intern.idb_meth->ft_ifree(&intern);
		(void)unlink(_ged_tmpfil);
		return GED_ERROR;
	    }

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

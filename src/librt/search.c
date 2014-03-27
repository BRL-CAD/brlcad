/*                        S E A R C H . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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

/* OpenBSD:
 *
 * Copyright (c) 1990, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * NetBSD:
 *
 * Copyright (c) 1990, 1993, 1994
 * The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Cimarron D. Taylor of the University of California, Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <regex.h>
#include "bio.h"

#include "bu/cmd.h"

#include "db.h"
#include "./librt_private.h"
#include "./search.h"


/* NB: the following table must be sorted lexically. */
static OPTION options[] = {
    { "!",          N_NOT,          c_not,          O_ZERO },
    { "(",          N_OPENPAREN,    c_openparen,    O_ZERO },
    { ")",          N_CLOSEPAREN,   c_closeparen,   O_ZERO },
    { "-a",         N_AND,          NULL,           O_NONE },
    { "-ab",        N_ABOVE,        c_above,        O_ZERO },
    { "-above",     N_ABOVE,        c_above,        O_ZERO },
    { "-and",       N_AND,          NULL,           O_NONE },
    { "-attr",	    N_ATTR,	    c_attr,	    O_ARGV },
    { "-below",     N_BELOW,        c_below,        O_ZERO },
    { "-bl",        N_BELOW,        c_below,        O_ZERO },
    { "-bool",      N_BOOL,         c_bool,	    O_ARGV },
    { "-depth",     N_DEPTH,        c_depth,        O_ARGV },
    { "-iname",     N_INAME,        c_iname,        O_ARGV },
    { "-iregex",    N_IREGEX,       c_iregex,       O_ARGV },
    { "-maxdepth",  N_MAXDEPTH,     c_maxdepth,     O_ARGV },
    { "-mindepth",  N_MINDEPTH,     c_mindepth,     O_ARGV },
    { "-name",      N_NAME,         c_name,         O_ARGV },
    { "-nnodes",    N_NNODES,       c_nnodes,       O_ARGV },
    { "-not",       N_NOT,          c_not,          O_ZERO },
    { "-o",         N_OR,           c_or,	    O_ZERO },
    { "-or", 	    N_OR, 	    c_or, 	    O_ZERO },
    { "-path",      N_PATH,         c_path,         O_ARGV },
    { "-print",     N_PRINT,        c_print,        O_ZERO },
    { "-regex",     N_REGEX,        c_regex,        O_ARGV },
    { "-stdattr",   N_STDATTR,      c_stdattr,      O_ZERO },
    { "-type",      N_TYPE,         c_type,	    O_ARGV },
};


/* Search client data container */
struct list_client_data_t {
    struct db_i *dbip;
    struct bu_ptbl *full_paths;
    int flags;
};


HIDDEN void
print_path_with_bools(struct db_full_path *full_path)
{
    struct bu_vls vls1 = BU_VLS_INIT_ZERO;
    struct bu_vls vls2 = BU_VLS_INIT_ZERO;
    struct db_full_path *newpath;
    BU_ALLOC(newpath, struct db_full_path);
    db_full_path_init(newpath);
    db_dup_full_path(newpath, full_path);
    while (newpath->fp_len > 0) {
	struct directory *dp = DB_FULL_PATH_CUR_DIR(newpath);
	int curr_bool = DB_FULL_PATH_CUR_BOOL(newpath);
	bu_vls_sprintf(&vls1, "/%s(%d)%s", dp->d_namep, curr_bool, bu_vls_addr(&vls2));
	bu_vls_sprintf(&vls2, "%s", bu_vls_addr(&vls1));
	DB_FULL_PATH_POP(newpath);
    }
    bu_log("%s", bu_vls_addr(&vls2));
    db_free_full_path(newpath);
}


/**
 * A generic traversal function maintaining awareness of the full path
 * to a given object.
 */
HIDDEN void
db_fullpath_list_subtree(struct db_full_path *path, int curr_bool, union tree *tp,
			     void (*traverse_func) (struct db_full_path *path,
						    struct resource *,
						    genptr_t),
			     struct resource *resp,
			     genptr_t client_data)
{
    struct directory *dp;
    struct list_client_data_t *lcd= (struct list_client_data_t *)client_data;
    int bool_val = curr_bool;

    if (!tp)
	return;

    RT_CK_FULL_PATH(path);
    RT_CHECK_DBI(lcd->dbip);
    RT_CK_TREE(tp);
    RT_CK_RESOURCE(resp);

    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    if (tp->tr_op == OP_UNION) bool_val = 2;
	    if (tp->tr_op == OP_INTERSECT) bool_val = 3;
	    if (tp->tr_op == OP_SUBTRACT) bool_val = 4;
	    db_fullpath_list_subtree(path, bool_val, tp->tr_b.tb_right, traverse_func, resp, client_data);
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    db_fullpath_list_subtree(path, OP_UNION, tp->tr_b.tb_left, traverse_func, resp, client_data);
	    break;
	case OP_DB_LEAF:
	    if ((dp=db_lookup(lcd->dbip, tp->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
		return;
	    } else {
		/* Create the new path, if doing so doesn't create a cyclic
		 * path (for search, that would constitute an infinite loop) */
		if ((lcd->flags & DB_SEARCH_HIDDEN) || !(dp->d_flags & RT_DIR_HIDDEN)) {
		    struct db_full_path *newpath;
		    db_add_node_to_full_path(path, dp);
		    DB_FULL_PATH_SET_CUR_BOOL(path, bool_val);
		    BU_ALLOC(newpath, struct db_full_path);
		    db_full_path_init(newpath);
		    db_dup_full_path(newpath, path);
		    /* Insert the path in the bu_ptbl collecting paths */
		    bu_ptbl_ins(lcd->full_paths, (long *)newpath);
		    if (!cyclic_path(path, NULL)) {
			/* Keep going */
			traverse_func(path, resp, client_data);
		    } else {
			char *path_string = db_path_to_string(path);
			bu_log("WARNING: not traversing cyclic path %s\n", path_string);
			bu_free(path_string, "free path str");
		    }
		}
		DB_FULL_PATH_POP(path);
		break;
	    }

	default:
	    bu_log("db_functree_subtree: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("db_functree_subtree: unrecognized operator\n");
    }
}


/**
 * This walker builds a list of db_full_path entries corresponding to
 * the contents of the tree under *path.  It does so while assigning
 * the boolean operation associated with each path entry to the
 * db_full_path structure.  This list is then used for further
 * processing and filtering by the search routines.
 */
HIDDEN void
db_fullpath_list(struct db_full_path *path,
		     struct resource *resp,
		     genptr_t client_data)
{
    struct directory *dp;
    struct list_client_data_t *lcd= (struct list_client_data_t *)client_data;
    RT_CK_FULL_PATH(path);
    RT_CK_DBI(lcd->dbip);

    dp = DB_FULL_PATH_CUR_DIR(path);
    if (!dp) return;
    if (dp->d_flags & RT_DIR_COMB) {
	struct rt_db_internal in;
	struct rt_comb_internal *comb;

	if (rt_db_get_internal(&in, dp, lcd->dbip, NULL, resp) < 0) return;

	comb = (struct rt_comb_internal *)in.idb_ptr;
	db_fullpath_list_subtree(path, OP_UNION, comb->tree, db_fullpath_list, resp, client_data);
	rt_db_free_internal(&in);
    }
}


HIDDEN struct db_plan_t *
palloc(enum db_search_ntype t, int (*f)(struct db_plan_t *, struct db_node_t *, struct db_i *, struct rt_wdb *, struct bu_ptbl *))
{
    struct db_plan_t *newplan;

    BU_GET(newplan, struct db_plan_t);
    newplan->type = t;
    newplan->eval = f;
    return newplan;
}


/*
 * (expression) functions --
 *
 * True if expression is true.
 */
HIDDEN int
f_expr(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct rt_wdb *wdbp, struct bu_ptbl *results)
{
    struct db_plan_t *p = NULL;
    int state = 0;

    for (p = plan->p_data[0]; p && (state = (p->eval)(p, db_node, dbip, wdbp, results)); p = p->next)
	; /* do nothing */

    return state;
}


/*
 * N_OPENPAREN and N_CLOSEPAREN nodes are temporary place markers.  They are
 * eliminated during phase 2 of find_formplan() --- the '(' node is converted
 * to a N_EXPR node containing the expression and the ')' node is discarded.
 */
HIDDEN int
c_openparen(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    (*resultplan) = (palloc(N_OPENPAREN, (int (*)(struct db_plan_t *, struct db_node_t *, struct db_i *, struct rt_wdb *, struct bu_ptbl *))-1));
    return BRLCAD_OK;
}


HIDDEN int
c_closeparen(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    (*resultplan) = (palloc(N_CLOSEPAREN, (int (*)(struct db_plan_t *, struct db_node_t *, struct db_i *, struct rt_wdb *, struct bu_ptbl *))-1));
    return BRLCAD_OK;
}


/*
 * ! expression functions --
 *
 * Negation of a primary; the unary NOT operator.
 */
HIDDEN int
f_not(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct rt_wdb *wdbp, struct bu_ptbl *results)
{
    struct db_plan_t *p = NULL;
    int state = 0;

    for (p = plan->p_data[0]; p && (state = (p->eval)(p, db_node, dbip, wdbp, results)); p = p->next)
	; /* do nothing */

    return !state;
}


HIDDEN int
c_not(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    (*resultplan) =  (palloc(N_NOT, f_not));
    return BRLCAD_OK;
}


HIDDEN int
find_execute_nested_plans(struct db_i *dbip, struct rt_wdb *wdbp, struct bu_ptbl *results, struct db_node_t *db_node, struct db_plan_t *plan) {
    struct db_plan_t *p = NULL;
    int state = 0;
    for (p = plan; p && (state = (p->eval)(p, db_node, dbip, wdbp, results)); p = p->next)
	; /* do nothing */

    return state;
}


/*
 * -below expression functions --
 *
 * Find objects above objects matching an expression.  In this case,
 * this means following the tree path back to the root.
 */
HIDDEN int
f_below(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct rt_wdb *wdbp, struct bu_ptbl *results)
{
    int state = 0;
    int distance = 0;
    struct db_node_t curr_node;
    struct db_full_path parent_path;

    db_full_path_init(&parent_path);
    db_dup_full_path(&parent_path, db_node->path);
    DB_FULL_PATH_POP(&parent_path);
    curr_node.path = &parent_path;
    distance = db_node->path->fp_len - parent_path.fp_len;

    while ((parent_path.fp_len > 0) && (state == 0) && !(db_node->flags & DB_SEARCH_FLAT)) {
	distance++;
	if ((distance <= plan->max_depth) && (distance >= plan->min_depth)) {
	    state += find_execute_nested_plans(dbip, wdbp, results, &curr_node, plan->ab_data[0]);
	}
	DB_FULL_PATH_POP(&parent_path);
    }

    db_free_full_path(&parent_path);

    return (state > 0) ? 1 : 0;
}


HIDDEN int
c_below(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    (*resultplan) =  (palloc(N_BELOW, f_below));
    return BRLCAD_OK;
}


/*
 * -above expression functions --
 *
 * Find objects below objects matching an expression.  Look at all
 * objects below the current object in the tree.
 */
HIDDEN int
f_above(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct rt_wdb *wdbp, struct bu_ptbl *UNUSED(results))
{
    int i = 0;
    int state = 0;
    struct db_node_t curr_node;
    struct bu_ptbl *full_paths = db_node->full_paths;

    unsigned int f_path_len = db_node->path->fp_len;

    for (i = 0; i < (int)BU_PTBL_LEN(full_paths); i++) {
	struct db_full_path *this_path = (struct db_full_path *)BU_PTBL_GET(full_paths, i);

	/* Check depth criteria by comparing to db_node->path - if OK execute nested plans */
	if (this_path->fp_len > f_path_len && db_full_path_match_top(db_node->path, this_path)) {
	    int relative_depth = this_path->fp_len - f_path_len;

	    if (relative_depth >= plan->min_depth && relative_depth <= plan->max_depth) {
		curr_node.path = this_path;
		curr_node.flags = db_node->flags;
		curr_node.full_paths = full_paths;

		state = find_execute_nested_plans(dbip, wdbp, NULL, &curr_node, plan->bl_data[0]);
		if (state)
		    return 1;
	    }
	}
    }

    return 0;
}


HIDDEN int
c_above(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    (*resultplan) =  (palloc(N_ABOVE, f_above));
    return BRLCAD_OK;
}


/*
 * expression -o expression functions --
 *
 * Alternation of primaries; the OR operator.  The second expression is
 * not evaluated if the first expression is true.
 */
HIDDEN int
f_or(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct rt_wdb *wdbp, struct bu_ptbl *results)
{
    struct db_plan_t *p = NULL;
    int state = 0;

    for (p = plan->p_data[0]; p && (state = (p->eval)(p, db_node, dbip, wdbp, results)); p = p->next)
	; /* do nothing */

    if (state)
	return 1;

    for (p = plan->p_data[1]; p && (state = (p->eval)(p, db_node, dbip, wdbp, results)); p = p->next)
	; /* do nothing */

    return state;
}


HIDDEN int
c_or(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    (*resultplan) = (palloc(N_OR, f_or));
    return BRLCAD_OK;
}


/*
 * -name functions --
 *
 * True if the basename of the filename being examined
 * matches pattern using Pattern Matching Notation S3.14
 */
HIDDEN int
f_name(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct bu_ptbl *UNUSED(results))
{
    struct directory *dp;

    dp = DB_FULL_PATH_CUR_DIR(db_node->path);
    if (!dp)
	return 0;

    return !bu_fnmatch(plan->c_data, dp->d_namep, 0);
}


HIDDEN int
c_name(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_NAME, f_name);
    newplan->c_data = pattern;
    (*resultplan) = newplan;
    return BRLCAD_OK;
}


/*
 * -iname function --
 *
 * True if the basename of the filename being examined
 * matches pattern using case insensitive Pattern Matching Notation S3.14
 */
HIDDEN int
f_iname(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct bu_ptbl *UNUSED(results))
{
    struct directory *dp;

    dp = DB_FULL_PATH_CUR_DIR(db_node->path);

    if (!dp)
	return 0;

    return !bu_fnmatch(plan->c_data, dp->d_namep, BU_FNMATCH_CASEFOLD);
}


HIDDEN int
c_iname(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_INAME, f_iname);
    newplan->ci_data = pattern;
    (*resultplan) = newplan;

    return BRLCAD_OK;
}


/*
 * -regex regexp (and related) functions --
 *
 * True if the complete file path matches the regular expression regexp.
 * For -regex, regexp is a case-sensitive (basic) regular expression.
 * For -iregex, regexp is a case-insensitive (basic) regular expression.
 */
HIDDEN int
f_regex(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct bu_ptbl *UNUSED(results))
{
    return !(regexec(&plan->regexp_data, db_path_to_string(db_node->path), 0, NULL, 0));
}


HIDDEN int
c_regex_common(enum db_search_ntype type, char *regexp, int icase, struct db_plan_t **resultplan)
{
    regex_t reg;
    struct db_plan_t *newplan;
    int rv;

    if (icase == 1) {
	rv = regcomp(&reg, regexp, REG_NOSUB|REG_EXTENDED|REG_ICASE);
    } else {
	rv = regcomp(&reg, regexp, REG_NOSUB|REG_EXTENDED);
    }
    if (rv != 0) {
	bu_log("ERROR: regular expression failed to compile: %s\n", regexp);
	return BRLCAD_ERROR;
    }

    newplan = palloc(type, f_regex);
    newplan->regexp_data = reg;
    (*resultplan) = newplan;
    regfree(&reg);

    return BRLCAD_OK;
}


HIDDEN int
c_regex(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    return c_regex_common(N_REGEX, pattern, 0, resultplan);
}


HIDDEN int
c_iregex(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    return c_regex_common(N_IREGEX, pattern, 1, resultplan);
}


HIDDEN int
string_to_name_and_val(const char *in, struct bu_vls *name, struct bu_vls *value) {
    size_t equalpos = 0;
    int checkval = 0;

    while ((equalpos < strlen(in)) &&
	   (in[equalpos] != '=') &&
	   (in[equalpos] != '>') &&
	   (in[equalpos] != '<')) {
	if ((in[equalpos] == '/') && (in[equalpos + 1] == '=')) {equalpos++;}
	if ((in[equalpos] == '/') && (in[equalpos + 1] == '<')) {equalpos++;}
	if ((in[equalpos] == '/') && (in[equalpos + 1] == '>')) {equalpos++;}
	equalpos++;
    }

    if (equalpos == strlen(in)) {
	/*No logical expression given - just copy attribute name*/
	bu_vls_strcpy(name, in);
    } else {
	checkval = 1; /*Assume simple equality comparison, then check for other cases and change if found.*/
	if ((in[equalpos] == '>') && (in[equalpos + 1] != '=')) {checkval = 2;}
	if ((in[equalpos] == '<') && (in[equalpos + 1] != '=')) {checkval = 3;}
	if ((in[equalpos] == '=') && (in[equalpos + 1] == '>')) {checkval = 4;}
	if ((in[equalpos] == '=') && (in[equalpos + 1] == '<')) {checkval = 5;}
	if ((in[equalpos] == '>') && (in[equalpos + 1] == '=')) {checkval = 4;}
	if ((in[equalpos] == '<') && (in[equalpos + 1] == '=')) {checkval = 5;}

	bu_vls_strncpy(name, in, equalpos);
	if (checkval < 4) {
	    bu_vls_strncpy(value, &(in[equalpos+1]), strlen(in) - equalpos - 1);
	} else {
	    bu_vls_strncpy(value, &(in[equalpos+2]), strlen(in) - equalpos - 1);
	}
    }
    return checkval;
}


/*
 * -attr functions --
 *
 * True if the database object being examined has the attribute
 * supplied to the attr option
 */
HIDDEN int
f_attr(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct rt_wdb *UNUSED(wdbp), struct bu_ptbl *UNUSED(results))
{
    struct bu_vls attribname = BU_VLS_INIT_ZERO;
    struct bu_vls value = BU_VLS_INIT_ZERO;
    struct bu_attribute_value_set avs;
    struct bu_attribute_value_pair *avpp;
    int checkval = 0;
    int strcomparison = 0;
    size_t i;
    struct directory *dp;

    /* Check for unescaped >, < or = characters.  If present, the
     * attribute must not only be present but the value assigned to
     * the attribute must satisfy the logical expression.  In the case
     * where a > or < is used with a string argument the behavior will
     * follow ASCII lexicographical order.  In the case of equality
     * between strings, fnmatch is used to support pattern matching
     */

    checkval = string_to_name_and_val(plan->attr_data, &attribname, &value);

    /* Now that we have the value, check to see if it is all numbers.
     * If so, use numerical comparison logic - otherwise use string
     * logic.
     */

    for (i = 0; i < strlen(bu_vls_addr(&value)); i++) {
	if (!(isdigit((int)(bu_vls_addr(&value)[i])))) strcomparison = 1;
    }

    /* Get attributes for object.
     */

    dp = DB_FULL_PATH_CUR_DIR(db_node->path);
    if (!dp)
	return 0;

    bu_avs_init_empty(&avs);
    if (db5_get_attributes(dbip, &avs, dp) < 0) {
	bu_avs_free(&avs);
	return 0;
    }
    avpp = avs.avp;

    /* Check all attributes for a match to the requested attribute.
     * If an expression was supplied, check the value of any matches
     * to the attribute name in the logical expression before
     * returning success
     */

    for (i = 0; i < (size_t)avs.count; i++, avpp++) {
	if (!bu_fnmatch(bu_vls_addr(&attribname), avpp->name, 0)) {
	    if (checkval >= 1) {

		/* String based comparisons */
		if ((checkval == 1) && (strcomparison == 1)) {
		    if (!bu_fnmatch(bu_vls_addr(&value), avpp->value, 0)) {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 1;
		    } else {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 0;
		    }
		}
		if ((checkval == 2) && (strcomparison == 1)) {
		    if (bu_strcmp(bu_vls_addr(&value), avpp->value) < 0) {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 1;
		    } else {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 0;
		    }
		}
		if ((checkval == 3) && (strcomparison == 1)) {
		    if (bu_strcmp(bu_vls_addr(&value), avpp->value) > 0) {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 1;
		    } else {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 0;
		    }
		}
		if ((checkval == 4) && (strcomparison == 1)) {
		    if ((!bu_fnmatch(bu_vls_addr(&value), avpp->value, 0)) || (bu_strcmp(bu_vls_addr(&value), avpp->value) < 0)) {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 1;
		    } else {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 0;
		    }
		}
		if ((checkval == 5) && (strcomparison == 1)) {
		    if ((!bu_fnmatch(bu_vls_addr(&value), avpp->value, 0)) || (bu_strcmp(bu_vls_addr(&value), avpp->value) > 0)) {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 1;
		    } else {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 0;
		    }
		}


		/* Numerical Comparisons */
		if ((checkval == 1) && (strcomparison == 0)) {
		    if (atol(bu_vls_addr(&value)) == atol(avpp->value)) {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 1;
		    } else {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 0;
		    }
		}
		if ((checkval == 2) && (strcomparison == 0)) {
		    if (atol(bu_vls_addr(&value)) < atol(avpp->value)) {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 1;
		    } else {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 0;
		    }
		}
		if ((checkval == 3) && (strcomparison == 0)) {
		    if (atol(bu_vls_addr(&value)) > atol(avpp->value)) {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 1;
		    } else {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 0;
		    }
		}
		if ((checkval == 4) && (strcomparison == 0)) {
		    if (atol(bu_vls_addr(&value)) <= atol(avpp->value)) {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 1;
		    } else {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 0;
		    }
		}
		if ((checkval == 5) && (strcomparison == 0)) {
		    if (atol(bu_vls_addr(&value)) >= atol(avpp->value)) {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 1;
		    } else {
			bu_avs_free(&avs);
			bu_vls_free(&attribname);
			bu_vls_free(&value);
			return 0;
		    }
		}
		bu_avs_free(&avs);
		bu_vls_free(&attribname);
		bu_vls_free(&value);
		return 0;
	    } else {
		bu_avs_free(&avs);
		bu_vls_free(&attribname);
		bu_vls_free(&value);
		return 1;
	    }
	}
    }
    bu_avs_free(&avs);
    bu_vls_free(&attribname);
    bu_vls_free(&value);
    return 0;
}


HIDDEN int
c_attr(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_ATTR, f_attr);
    newplan->attr_data = pattern;
    (*resultplan) = newplan;
    return BRLCAD_OK;
}


/*
 * -stdattr function --
 *
 * Search based on the presence of the
 * "standard" attributes - matches when there
 * are ONLY "standard" attributes
 * associated with an object.
 */
HIDDEN int
f_stdattr(struct db_plan_t *UNUSED(plan), struct db_node_t *db_node, struct db_i *dbip, struct rt_wdb *UNUSED(wdbp), struct bu_ptbl *UNUSED(results))
{
    struct bu_attribute_value_pair *avpp;
    struct bu_attribute_value_set avs;
    struct directory *dp;
    int found_nonstd_attr = 0;
    int found_attr = 0;
    size_t i;


    /* Get attributes for object and check all of them to see if there
     * is not a match to the standard attributes.  If any is found
     * return failure, otherwise success.
     */

    dp = DB_FULL_PATH_CUR_DIR(db_node->path);
    if (!dp)
	return 0;

    bu_avs_init_empty(&avs);
    if (db5_get_attributes(dbip, &avs, dp) < 0) {
	bu_avs_free(&avs);
	return 0;
    }

    avpp = avs.avp;
    for (i = 0; i < (size_t)avs.count; i++, avpp++) {
	found_attr = 1;
	if (!BU_STR_EQUAL(avpp->name, "GIFTmater") &&
	    !BU_STR_EQUAL(avpp->name, "aircode") &&
	    !BU_STR_EQUAL(avpp->name, "inherit") &&
	    !BU_STR_EQUAL(avpp->name, "los") &&
	    !BU_STR_EQUAL(avpp->name, "material_id") &&
	    !BU_STR_EQUAL(avpp->name, "oshader") &&
	    !BU_STR_EQUAL(avpp->name, "region") &&
	    !BU_STR_EQUAL(avpp->name, "region_id") &&
	    !BU_STR_EQUAL(avpp->name, "rgb")) {

	    found_nonstd_attr = 1;
	}
    }

    bu_avs_free(&avs);

    if (!found_nonstd_attr && found_attr) return 1;
    return 0;
}


HIDDEN int
c_stdattr(char *UNUSED(pattern), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_STDATTR, f_stdattr);
    (*resultplan) = newplan;
    return BRLCAD_OK;
}


/*
 * -type function --
 *
 * Search based on the type of the object - primitives are matched
 * based on their primitive type (tor, tgc, arb4, etc.) and
 * combinations are matched based on whether they are a combination or
 * region.
 */
HIDDEN int
f_type(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct rt_wdb *wdbp, struct bu_ptbl *UNUSED(results))
{
    struct rt_db_internal intern;
    struct directory *dp;
    int type_match = 0;
    int type;

    dp = DB_FULL_PATH_CUR_DIR(db_node->path);
    if (!dp) return 0;
    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) return 0;
    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || !intern.idb_meth->ft_label) {
	rt_db_free_internal(&intern);
	return 0;
    }

    switch (intern.idb_minor_type) {
	case DB5_MINORTYPE_BRLCAD_ARB8:
	    type = rt_arb_std_type(&intern, &wdbp->wdb_tol);
	    switch (type) {
		case 4:
		    type_match = (!bu_fnmatch(plan->type_data, "arb4", 0));
		    break;
		case 5:
		    type_match = (!bu_fnmatch(plan->type_data, "arb5", 0));
		    break;
		case 6:
		    type_match = (!bu_fnmatch(plan->type_data, "arb6", 0));
		    break;
		case 7:
		    type_match = (!bu_fnmatch(plan->type_data, "arb7", 0));
		    break;
		case 8:
		    type_match = (!bu_fnmatch(plan->type_data, "arb8", 0));
		    break;
		default:
		    type_match = (!bu_fnmatch(plan->type_data, "invalid", 0));
		    break;
	    }
	    break;
	case DB5_MINORTYPE_BRLCAD_COMBINATION:
	    if (dp->d_flags & RT_DIR_REGION) {
		if ((!bu_fnmatch(plan->type_data, "r", 0)) || (!bu_fnmatch(plan->type_data, "reg", 0))  || (!bu_fnmatch(plan->type_data, "region", 0))) {
		    type_match = 1;
		}
	    }
	    if ((!bu_fnmatch(plan->type_data, "c", 0)) || (!bu_fnmatch(plan->type_data, "comb", 0)) || (!bu_fnmatch(plan->type_data, "combination", 0))) {
		type_match = 1;
	    }
	    break;
	case DB5_MINORTYPE_BRLCAD_METABALL:
	    /* Because ft_label is only 8 characters, ft_label doesn't work in fnmatch for metaball*/
	    type_match = (!bu_fnmatch(plan->type_data, "metaball", 0));
	    break;
	default:
	    type_match = !bu_fnmatch(plan->type_data, intern.idb_meth->ft_label, 0);
	    break;
    }

    /* Match anything that doesn't define a 2D or 3D shape - unfortunately, this list will have to
     * be updated manually unless/until some functionality is added to generate it */
    if (!bu_fnmatch(plan->type_data, "shape", 0) &&
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_COMBINATION &&
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ANNOTATION &&
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_CONSTRAINT &&
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_GRIP &&
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_JOINT
       ) {
	type_match = 1;
    }

    rt_db_free_internal(&intern);

    return type_match;
}


HIDDEN int
c_type(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_TYPE, f_type);
    newplan->type_data = pattern;
    (*resultplan) = newplan;

    return BRLCAD_OK;
}


/*
 * -bool function --
 *
 * True if the boolean operation combining the object into the tree matches
 * the supplied boolean flag.
 *
 */
HIDDEN int
f_bool(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct bu_ptbl *UNUSED(results))
{
    int bool_match = 0;
    int bool_type = DB_FULL_PATH_CUR_BOOL(db_node->path);
    if (plan->bool_data == bool_type) bool_match = 1;
    return bool_match;
}


HIDDEN int
c_bool(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    int bool_type = 0;
    struct db_plan_t *newplan;

    newplan = palloc(N_BOOL, f_bool);

    if (!bu_fnmatch(pattern, "u", 0) || !bu_fnmatch(pattern, "U", 0)) bool_type = 2;
    if (!bu_fnmatch(pattern, "+", 0)) bool_type = 3;
    if (!bu_fnmatch(pattern, "-", 0)) bool_type = 4;

    newplan->bool_data = bool_type;
    (*resultplan) = newplan;
    return BRLCAD_OK;
}


/*
 * -maxdepth function --
 *
 * True if the object being examined is at depth <= the supplied
 * depth.
 *
 */
HIDDEN int
f_maxdepth(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct bu_ptbl *UNUSED(results))
{
    return ((int)db_node->path->fp_len - 1 <= plan->max_data) ? 1 : 0;
}


HIDDEN int
c_maxdepth(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_MAXDEPTH, f_maxdepth);
    newplan->max_data = atoi(pattern);
    (*resultplan) = newplan;

    return BRLCAD_OK;
}


/*
 * -mindepth function --
 *
 * True if the object being examined is at depth >= the supplied
 * depth.
 *
 */
HIDDEN int
f_mindepth(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct bu_ptbl *UNUSED(results))
{
    return ((int)db_node->path->fp_len - 1 >= plan->min_data) ? 1 : 0;
}


HIDDEN int
c_mindepth(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_MINDEPTH, f_mindepth);
    newplan->min_data = atoi(pattern);
    (*resultplan) = newplan;

    return BRLCAD_OK;
}


/*
 * -depth function --
 *
 * True if the database object being examined satisfies
 * the depth criteria: [><=]depth
 */
HIDDEN int
f_depth(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct bu_ptbl *UNUSED(results))
{
    int ret = 0;
    int checkval = 0;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_vls value = BU_VLS_INIT_ZERO;

    /* Check for unescaped >, < or = characters.  If present, the
     * attribute must not only be present but the value assigned to
     * the attribute must satisfy the logical expression.  In the case
     * where a > or < is used with a string argument the behavior will
     * follow ASCII lexicographical order.  In the case of equality
     * between strings, fnmatch is used to support pattern matching
     */

    checkval = string_to_name_and_val(plan->depth_data, &name, &value);

    if ((bu_vls_strlen(&value) > 0 && isdigit((int)bu_vls_addr(&value)[0]))
	|| (bu_vls_strlen(&value) == 0 && isdigit((int)bu_vls_addr(&name)[0]))) {
	switch (checkval) {
	    case 0:
		ret = ((int)db_node->path->fp_len - 1 == atol(bu_vls_addr(&name))) ? 1 : 0;
		break;
	    case 1:
		ret = ((int)db_node->path->fp_len - 1 == atol(bu_vls_addr(&value))) ? 1 : 0;
		break;
	    case 2:
		ret = ((int)db_node->path->fp_len - 1 > atol(bu_vls_addr(&value))) ? 1 : 0;
		break;
	    case 3:
		ret = ((int)db_node->path->fp_len - 1 < atol(bu_vls_addr(&value))) ? 1 : 0;
		break;
	    case 4:
		ret = ((int)db_node->path->fp_len - 1 >= atol(bu_vls_addr(&value))) ? 1 : 0;
		break;
	    case 5:
		ret = ((int)db_node->path->fp_len - 1 <= atol(bu_vls_addr(&value))) ? 1 : 0;
		break;
	    default:
		ret = 0;
		break;
	}
    }
    bu_vls_free(&name);
    bu_vls_free(&value);

    return ret;
}


HIDDEN int
c_depth(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_DEPTH, f_depth);
    newplan->attr_data = pattern;
    (*resultplan) = newplan;

    return BRLCAD_OK;
}


/*
 * -nnodes function --
 *
 * True if the object being examined is a COMB and has # nodes.  If an
 * expression ># or <# is supplied, true if object has greater than or
 * less than that number of nodes. if >=# or <=# is supplied, true if
 * object has greater than or equal to / less than or equal to # of
 * nodes.
 *
 */
HIDDEN int
f_nnodes(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct rt_wdb *UNUSED(wdbp), struct bu_ptbl *UNUSED(results))
{
    int dogreaterthan = 0;
    int dolessthan = 0;
    int doequal = 0;
    size_t node_count_target = 0;
    size_t node_count = 0;
    struct directory *dp;
    struct rt_db_internal in;
    struct rt_comb_internal *comb;

    /* Check for >, < and = in the first and second character
     * positions.
     */

    if (isdigit((int)plan->node_data[0])) {
	doequal = 1;
	node_count_target = (size_t)atoi(plan->node_data);
    } else {
	if (plan->node_data[0] == '>') dogreaterthan = 1;
	if (plan->node_data[0] == '<') dolessthan = 1;
	if (plan->node_data[0] == '=') doequal = 1;
	if (plan->node_data[0] != '>' && plan->node_data[0] != '<' && plan->node_data[0] != '=') {
	    return 0;
	}
	if (plan->node_data[1] == '=') {
	    doequal = 1;
	    if (isdigit((int)plan->node_data[2])) {
		node_count_target = (size_t)atoi((plan->node_data)+2);
	    } else {
		return 0;
	    }
	} else {
	    if (isdigit((int)plan->node_data[1])) {
		node_count_target = (size_t)atoi((plan->node_data)+1);
	    } else {
		return 0;
	    }
	}
    }

    /* Get the number of nodes for the current object and check if the
     * value satisfied the logical conditions specified in the
     * argument string.
     */

    dp = DB_FULL_PATH_CUR_DIR(db_node->path);
    if (!dp)
	return 0;

    if (dp->d_flags & RT_DIR_COMB) {
	rt_db_get_internal(&in, dp, dbip, (fastf_t *)NULL, &rt_uniresource);
	comb = (struct rt_comb_internal *)in.idb_ptr;
	if (comb->tree == NULL) {
	    node_count = 0;
	} else {
	    node_count = db_tree_nleaves(comb->tree);
	}
	rt_db_free_internal(&in);
    } else {
	return 0;
    }

    if (doequal) {
	if (node_count == node_count_target) {
	    return 1;
	}
    }
    if (dolessthan) {
	if (node_count < node_count_target) {
	    return 1;
	}
    } else if (dogreaterthan) {
	if (node_count > node_count_target) {
	    return 1;
	}
    }

    return 0;
}


HIDDEN int
c_nnodes(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_NNODES, f_nnodes);
    newplan->node_data = pattern;
    (*resultplan) = newplan;
    return BRLCAD_OK;
}


/*
 * -path function --
 *
 * True if the object being examined shares the pattern as part of its
 * path. To exclude results of certain directories use the -not option
 * with this option.
 */
HIDDEN int
f_path(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct bu_ptbl *UNUSED(results))
{
    return !bu_fnmatch(plan->path_data, db_path_to_string(db_node->path), 0);
}


HIDDEN int
c_path(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_PATH, f_path);
    newplan->path_data = pattern;
    (*resultplan) = newplan;
    return BRLCAD_OK;
}


/*
 * -print functions --
 *
 * Always true, causes the current pathname to be added to the results
 * list.
 */
HIDDEN int
f_print(struct db_plan_t *UNUSED(plan), struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct bu_ptbl *results)
{
    if (!results)
	return 1;

    if (db_node->flags & DB_SEARCH_FLAT || db_node->flags & DB_SEARCH_RETURN_UNIQ_DP) {
	bu_ptbl_ins_unique(results, (long *)DB_FULL_PATH_CUR_DIR(db_node->path));
    } else {
	struct db_full_path *new_entry;
	BU_ALLOC(new_entry, struct db_full_path);
	db_full_path_init(new_entry);
	db_dup_full_path(new_entry, (const struct db_full_path *)(db_node->path));
	bu_ptbl_ins(results, (long *)new_entry);
    }
    return 1;
}


HIDDEN int
c_print(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *db_search_isoutput)
{
    *db_search_isoutput = 1;

    (*resultplan) = palloc(N_PRINT, f_print);
    return BRLCAD_OK;
}


HIDDEN int
typecompare(const void *a, const void *b)
{
    return bu_strcmp(((OPTION *)a)->name, ((OPTION *)b)->name);
}


HIDDEN OPTION *
option(char *name)
{
    OPTION tmp;

    tmp.name = name;
    tmp.flags = 0;
    tmp.token = N_ABOVE;
    tmp.create = NULL;

    return ((OPTION *)bsearch(&tmp, options, sizeof(options)/sizeof(OPTION), sizeof(OPTION), typecompare));
}


/*
 * create a node corresponding to a command line argument.
 *
 * TODO: add create/process function pointers to node, so we can skip
 * this switch stuff.
 */
HIDDEN int
find_create(char ***argvp, struct db_plan_t **resultplan, struct bu_ptbl *UNUSED(results), int *db_search_isoutput, int quiet)
{
    OPTION *p;
    struct db_plan_t *newplan = NULL;
    char **argv;
    int checkval;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_vls value = BU_VLS_INIT_ZERO;

    if (!argvp || !resultplan)
	return BRLCAD_ERROR;

    argv = *argvp;

    checkval = string_to_name_and_val(argv[0], &name, &value);

    p = option(bu_vls_addr(&name));
    if (!p) {
	if (!quiet)
	    bu_log("%s: unknown option passed to find_create\n", *argv);
	return BRLCAD_ERROR;
    }

    ++argv;
    if (p->flags & (O_ARGV|O_ARGVP) && !*argv) {
	if (!quiet)
	    bu_log("%s: requires additional arguments\n", *--argv);
	return BRLCAD_ERROR;
    }

    switch (p->flags) {
	case O_NONE:
	    break;
	case O_ZERO:
	    (p->create)(NULL, NULL, 0, &newplan, db_search_isoutput);
	    break;
	case O_ARGV:
	    (p->create)(*argv++, NULL, 0, &newplan, db_search_isoutput);
	    break;
	case O_ARGVP:
	    (p->create)(NULL, &argv, p->token == N_OK, &newplan, db_search_isoutput);
	    break;
	default:
	    return BRLCAD_OK;
    }

    if (newplan) {
	if (bu_vls_strlen(&value) > 0 && isdigit((int)bu_vls_addr(&value)[0])) {
	    switch (checkval) {
		case 1:
		    newplan->min_depth = atol(bu_vls_addr(&value));
		    newplan->max_depth = atol(bu_vls_addr(&value));
		    break;
		case 2:
		    newplan->min_depth = atol(bu_vls_addr(&value)) + 1;
		    newplan->max_depth = INT_MAX;
		    break;
		case 3:
		    newplan->min_depth = 0;
		    newplan->max_depth = atol(bu_vls_addr(&value)) - 1;
		    break;
		case 4:
		    newplan->min_depth = atol(bu_vls_addr(&value));
		    newplan->max_depth = INT_MAX;
		    break;
		case 5:
		    newplan->min_depth = 0;
		    newplan->max_depth = atol(bu_vls_addr(&value));
		    break;
		default:
		    newplan->min_depth = 0;
		    newplan->max_depth = INT_MAX;
		    break;
	    }
	} else {
	    newplan->min_depth = 0;
	    newplan->max_depth = INT_MAX;
	}
    }

    *argvp = argv;
    (*resultplan) = newplan;
    bu_vls_free(&name);
    bu_vls_free(&value);

    return BRLCAD_OK;
}


/*
 * destructively removes the top from the plan
 */
HIDDEN struct db_plan_t *
yanknode(struct db_plan_t **planp)          /* pointer to top of plan (modified) */
{
    struct db_plan_t *node;             /* top node removed from the plan */

    if ((node = (*planp)) == NULL)
	return NULL;

    (*planp) = (*planp)->next;
    node->next = NULL;

    return node;
}


/*
 * Removes one expression from the plan.  This is used mainly by
 * paren_squish.  In comments below, an expression is either a simple
 * node or a N_EXPR node containing a list of simple nodes.
 */
HIDDEN int
yankexpr(struct db_plan_t **planp, struct db_plan_t **resultplan)          /* pointer to top of plan (modified) */
{
    struct db_plan_t *next;     	/* temp node holding subexpression results */
    struct db_plan_t *node;             /* pointer to returned node or expression */
    struct db_plan_t *tail;             /* pointer to tail of subplan */
    struct db_plan_t *subplan;          /* pointer to head of () expression */
    extern int f_expr(struct db_plan_t *, struct db_node_t *, struct db_i *, struct rt_wdb *, struct bu_ptbl *);
    int error_return = BRLCAD_OK;

    /* first pull the top node from the plan */
    if ((node = yanknode(planp)) == NULL) {
	(*resultplan) = NULL;
	return BRLCAD_OK;
    }
    /*
     * If the node is an '(' then we recursively slurp up expressions
     * until we find its associated ')'.  If it's a closing paren we
     * just return it and unwind our recursion; all other nodes are
     * complete expressions, so just return them.
     */
    if (node->type == N_OPENPAREN)
	for (tail = subplan = NULL;;) {
	    if ((error_return = yankexpr(planp, &next)) != BRLCAD_OK) return BRLCAD_ERROR;
	    if (next == NULL) {
		bu_log("(: missing closing ')'\n");
		return BRLCAD_ERROR;
	    }
	    /*
	     * If we find a closing ')' we store the collected subplan
	     * in our '(' node and convert the node to a N_EXPR.  The
	     * ')' we found is ignored.  Otherwise, we just continue
	     * to add whatever we get to our subplan.
	     */
	    if (next->type == N_CLOSEPAREN) {
		if (subplan == NULL) {
		    bu_log("(): empty inner expression");
		    return BRLCAD_ERROR;
		}
		node->p_data[0] = subplan;
		node->type = N_EXPR;
		node->eval = f_expr;
		break;
	    } else {
		if (subplan == NULL)
		    tail = subplan = next;
		else {
		    tail->next = next;
		    tail = next;
		}
		tail->next = NULL;
	    }
	}
    (*resultplan) = node;

    /* If we get here, we're OK */
    return BRLCAD_OK;
}


/*
 * replaces "parenthesized" plans in our search plan with "expr"
 * nodes.
 */
HIDDEN int
paren_squish(struct db_plan_t *plan, struct db_plan_t **resultplan)                /* plan with () nodes */
{
    struct db_plan_t *expr;     /* pointer to next expression */
    struct db_plan_t *tail;     /* pointer to tail of result plan */
    struct db_plan_t *result;   /* pointer to head of result plan */

    result = tail = NULL;

    /*
     * the basic idea is to have yankexpr do all our work and just
     * collect its results together.
     */
    if (yankexpr(&plan, &expr) != BRLCAD_OK) return BRLCAD_ERROR;
    while (expr != NULL) {
	/*
	 * if we find an unclaimed ')' it means there is a missing
	 * '(' someplace.
	 */
	if (expr->type == N_CLOSEPAREN) {
	    bu_log("): no beginning '('");
	    return BRLCAD_ERROR;
	}

	/* add the expression to our result plan */
	if (result == NULL)
	    tail = result = expr;
	else {
	    tail->next = expr;
	    tail = expr;
	}
	tail->next = NULL;
	if (yankexpr(&plan, &expr) != BRLCAD_OK) return BRLCAD_ERROR;
    }
    (*resultplan) = result;
    return BRLCAD_OK;
}


/*
 * compresses "!" expressions in our search plan.
 */
HIDDEN int
not_squish(struct db_plan_t *plan, struct db_plan_t **resultplan)          /* plan to process */
{
    struct db_plan_t *next;     /* next node being processed */
    struct db_plan_t *node;     /* temporary node used in N_NOT processing */
    struct db_plan_t *tail;     /* pointer to tail of result plan */
    struct db_plan_t *result;   /* pointer to head of result plan */

    tail = result = next = NULL;

    while ((next = yanknode(&plan)) != NULL) {
	/*
	 * if we encounter a (expression) then look for nots in the
	 * expr subplan.
	 */
	if (next->type == N_EXPR)
	    if (not_squish(next->p_data[0], &(next->p_data[0])) != BRLCAD_OK) return BRLCAD_ERROR;

	/*
	 * if we encounter a not, then snag the next node and place it
	 * in the not's subplan.  As an optimization we compress
	 * several not's to zero or one not.
	 */
	if (next->type == N_NOT) {
	    int notlevel = 1;

	    node = yanknode(&plan);
	    while (node != NULL && node->type == N_NOT) {
		++notlevel;
		node = yanknode(&plan);
	    }
	    if (node == NULL) {
		bu_log("!: no following expression");
		return BRLCAD_ERROR;
	    }
	    if (node->type == N_OR) {
		bu_log("!: nothing between ! and -o");
		return BRLCAD_ERROR;
	    }
	    if (node->type == N_EXPR)
		if (not_squish(node, &node) != BRLCAD_OK) return BRLCAD_ERROR;
	    if (notlevel % 2 != 1)
		next = node;
	    else
		next->p_data[0] = node;
	}

	/* add the node to our result plan */
	if (result == NULL)
	    tail = result = next;
	else {
	    tail->next = next;
	    tail = next;
	}
	tail->next = NULL;
    }
    (*resultplan) = result;
    return BRLCAD_OK;
}


/*
 * compresses "-above" expressions in our search plan.
 */
HIDDEN int
above_squish(struct db_plan_t *plan, struct db_plan_t **resultplan)          /* plan to process */
{
    struct db_plan_t *next;     /* next node being processed */
    struct db_plan_t *node;     /* temporary node used in N_NOT processing */
    struct db_plan_t *tail;     /* pointer to tail of result plan */
    struct db_plan_t *result;   /* pointer to head of result plan */

    tail = result = next = NULL;

    while ((next = yanknode(&plan)) != NULL) {
	/*
	 * if we encounter a (expression) then look for aboves in the
	 * expr subplan.
	 */
	if (next->type == N_EXPR)
	    if (above_squish(next->ab_data[0], &(next->ab_data[0])) != BRLCAD_OK) return BRLCAD_ERROR;

	/*
	 * if we encounter an above, then snag the next node and place it
	 * in the not's subplan.
	 */
	if (next->type == N_ABOVE) {

	    node = yanknode(&plan);
	    if (node != NULL && node->type == N_ABOVE) {
		bu_log("Error - repeated -above node in plan.\n");
		return BRLCAD_ERROR;
	    }
	    if (node == NULL) {
		bu_log("-above: no following expression");
		return BRLCAD_ERROR;
	    }
	    if (node->type == N_OR) {
		bu_log("-above: nothing between -above and -o");
		return BRLCAD_ERROR;
	    }
	    if (node->type == N_EXPR)
		if (above_squish(node, &node) != BRLCAD_OK) return BRLCAD_ERROR;
	    /*Made it*/
	    next->ab_data[0] = node;
	}

	/* add the node to our result plan */
	if (result == NULL)
	    tail = result = next;
	else {
	    tail->next = next;
	    tail = next;
	}
	tail->next = NULL;
    }
    (*resultplan) = result;
    return BRLCAD_OK;
}


/*
 * compresses "-below" expressions in our search plan.
 */
HIDDEN int
below_squish(struct db_plan_t *plan, struct db_plan_t **resultplan)          /* plan to process */
{
    struct db_plan_t *next;     /* next node being processed */
    struct db_plan_t *node;     /* temporary node used in N_NOT processing */
    struct db_plan_t *tail;     /* pointer to tail of result plan */
    struct db_plan_t *result;   /* pointer to head of result plan */

    tail = result = next = NULL;

    while ((next = yanknode(&plan)) != NULL) {
	/*
	 * if we encounter a (expression) then look for nots in
	 * the expr subplan.
	 */
	if (next->type == N_EXPR)
	    if (below_squish(next->bl_data[0], &(next->bl_data[0])) != BRLCAD_OK) return BRLCAD_ERROR;

	/*
	 * if we encounter a not, then snag the next node and place it
	 * in the not's subplan.
	 */
	if (next->type == N_BELOW) {

	    node = yanknode(&plan);
	    if (node != NULL && node->type == N_BELOW) {
		bu_log("Error - repeated -below node in plan.\n");
		return BRLCAD_ERROR;
	    }
	    if (node == NULL) {
		bu_log("-below: no following expression");
		return BRLCAD_ERROR;
	    }
	    if (node->type == N_OR) {
		bu_log("-below: nothing between -below and -o");
		return BRLCAD_ERROR;
	    }
	    if (node->type == N_EXPR)
		if (below_squish(node, &node) != BRLCAD_OK) return BRLCAD_ERROR;
	    /* Made it */
	    next->bl_data[0] = node;
	}

	/* add the node to our result plan */
	if (result == NULL)
	    tail = result = next;
	else {
	    tail->next = next;
	    tail = next;
	}
	tail->next = NULL;
    }
    (*resultplan) = result;
    return BRLCAD_OK;
}


/*
 * compresses -o expressions in our search plan.
 */
HIDDEN int
or_squish(struct db_plan_t *plan, struct db_plan_t **resultplan)           /* plan with ors to be squished */
{
    struct db_plan_t *next;     /* next node being processed */
    struct db_plan_t *tail;     /* pointer to tail of result plan */
    struct db_plan_t *result;   /* pointer to head of result plan */

    tail = result = next = NULL;

    while ((next = yanknode(&plan)) != NULL) {
	/*
	 * if we encounter a (expression) then look for or's in
	 * the expr subplan.
	 */
	if (next->type == N_EXPR)
	    if (or_squish(next->p_data[0], &(next->p_data[0])) != BRLCAD_OK) return BRLCAD_ERROR;

	/* if we encounter a not then look for not's in the subplan */
	if (next->type == N_NOT)
	    if (or_squish(next->p_data[0], &(next->p_data[0])) != BRLCAD_OK) return BRLCAD_ERROR;

	/*
	 * if we encounter an or, then place our collected plan in the
	 * or's first subplan and then recursively collect the
	 * remaining stuff into the second subplan and return the or.
	 */
	if (next->type == N_OR) {
	    if (result == NULL) {
		bu_log("-o: no expression before -o");
		return BRLCAD_ERROR;
	    }
	    next->p_data[0] = result;
	    if (or_squish(plan, &(next->p_data[1]))  != BRLCAD_OK) return BRLCAD_ERROR;
	    if (next->p_data[1] == NULL) {
		bu_log("-o: no expression after -o");
		return BRLCAD_ERROR;
	    }
	    (*resultplan) = next;
	    return BRLCAD_OK;
	}

	/* add the node to our result plan */
	if (result == NULL)
	    tail = result = next;
	else {
	    tail->next = next;
	    tail = next;
	}
	tail->next = NULL;
    }
    (*resultplan) = result;
    return BRLCAD_OK;
}


HIDDEN struct db_plan_t *
db_search_form_plan(char **argv, int quiet) {
    struct db_plan_t *plan, *tail;
    struct db_plan_t *newplan = NULL;
    struct bu_ptbl *results = NULL;
    int db_search_isoutput = 0;

    /*
     * for each argument in the command line, determine what kind of
     * node it is, create the appropriate node type and add the new
     * plan node to the end of the existing plan.  The resulting plan
     * is a linked list of plan nodes.  For example, the string:
     *
     * % find . -name foo -newer bar -print
     *
     * results in the plan:
     *
     * [-name foo]--> [-newer bar]--> [-print]
     *
     * in this diagram, `[-name foo]' represents the plan node generated
     * by c_name() with an argument of foo and `-->' represents the
     * plan->next pointer.
     */
    for (plan = tail = NULL; *argv;) {
	if (find_create(&argv, &newplan, results, &db_search_isoutput, quiet) != BRLCAD_OK) return NULL;
	if (!newplan)
	    continue;
	if (plan == NULL)
	    tail = plan = newplan;
	else {
	    tail->next = newplan;
	    tail = newplan;
	}
    }

    /*
     * if the user didn't specify one of -print, -ok or -exec, then
     * -print is assumed so we bracket the current expression with
     * parens, if necessary, and add a -print node on the end.
     */
    if (!db_search_isoutput) {
	if (plan == NULL) {
	    c_print(NULL, NULL, 0, &newplan, &db_search_isoutput);
	    tail = plan = newplan;
	} else {
	    c_openparen(NULL, NULL, 0, &newplan, &db_search_isoutput);
	    newplan->next = plan;
	    plan = newplan;
	    c_closeparen(NULL, NULL, 0, &newplan, &db_search_isoutput);
	    tail->next = newplan;
	    tail = newplan;
	    c_print(NULL, NULL, 0, &newplan, &db_search_isoutput);
	    tail->next = newplan;
	    tail = newplan;
	}
    }

    /*
     * the command line has been completely processed into a search
     * plan except for the (,), !, and -o operators.  Rearrange the
     * plan so that the portions of the plan which are affected by the
     * operators are moved into operator nodes themselves.  For
     * example:
     *
     * [!]--> [-name foo]--> [-print]
     *
     * becomes
     *
     * [! [-name foo] ]--> [-print]
     *
     * and
     *
     * [(]--> [-depth]--> [-name foo]--> [)]--> [-print]
     *
     * becomes
     *
     * [expr [-depth]-->[-name foo] ]--> [-print]
     *
     * operators are handled in order of precedence.
     */

    if (paren_squish(plan, &plan) != BRLCAD_OK) return NULL;              /* ()'s */
    if (above_squish(plan, &plan) != BRLCAD_OK) return NULL;              /* above's */
    if (below_squish(plan, &plan) != BRLCAD_OK) return NULL;              /* below's */
    if (not_squish(plan, &plan) != BRLCAD_OK) return NULL;                /* !'s */
    if (or_squish(plan, &plan) != BRLCAD_OK) return NULL;                 /* -o's */
    return plan;
}


HIDDEN void
find_execute_plans(struct db_i *dbip, struct rt_wdb *wdbp, struct bu_ptbl *results, struct db_node_t *db_node, struct db_plan_t *plan) {
    struct db_plan_t *p;
    for (p = plan; p && (p->eval)(p, db_node, dbip, wdbp, results); p = p->next)
	;
}

HIDDEN void
db_search_free_plan(void **vplan) {
    struct db_plan_t *p;
    struct db_plan_t *plan = (struct db_plan_t *)*vplan;
    for (p = plan; p;) {
	plan = p->next;
	BU_PUT(p, struct db_plan_t);
	p = plan;
    }
    /* sanity */
    *vplan = NULL;
}


/*********** DEPRECATED search functionality ******************/
int
db_full_path_list_add(const char *path, int local, struct db_i *dbip, struct db_full_path_list *path_list)
{
    struct db_full_path dfp;
    struct db_full_path_list *new_entry;
    db_full_path_init(&dfp);
    /* Turn string path into a full path structure */
    if (db_string_to_path(&dfp, dbip, path) == -1) {
	db_free_full_path(&dfp);
	return 1;
    }
    /* Make a search list and add the entry from the directory name full path */
    BU_ALLOC(new_entry, struct db_full_path_list);
    BU_ALLOC(new_entry->path, struct db_full_path);
    db_full_path_init(new_entry->path);
    db_dup_full_path(new_entry->path, (const struct db_full_path *)&dfp);
    new_entry->local = local;
    BU_LIST_PUSH(&(path_list->l), &(new_entry->l));
    db_free_full_path(&dfp);
    return 0;
}


/* Internal version of deprecated function */
void
_db_free_full_path_list(struct db_full_path_list *path_list)
{
    struct db_full_path_list *currentpath;

    if (!path_list)
	return;

    if (!BU_LIST_IS_EMPTY(&(path_list->l))) {
	while (BU_LIST_WHILE(currentpath, db_full_path_list, &(path_list->l))) {
	    db_free_full_path(currentpath->path);
	    BU_LIST_DEQUEUE((struct bu_list *)currentpath);
	    bu_free(currentpath->path, "free db_full_path_list path entry");
	    bu_free(currentpath, "free db_full_path_list entry");
	}
    }

    bu_free(path_list, "free path_list");
}


void
db_free_full_path_list(struct db_full_path_list *path_list)
{
    _db_free_full_path_list(path_list);
}


void
db_search_freeplan(void **vplan) {
    db_search_free_plan(vplan);
}


void
db_free_search_tbl(struct bu_ptbl *search_results)
{
    int i;
    if (!search_results || search_results->l.magic != BU_PTBL_MAGIC)
	return;

    for(i = (int)BU_PTBL_LEN(search_results) - 1; i >= 0; i--) {
	struct db_full_path *path = (struct db_full_path *)BU_PTBL_GET(search_results, i);
	if (path->magic && path->magic == DB_FULL_PATH_MAGIC) {
	    db_free_full_path(path);
	    bu_free(path, "free search path container");
	}
    }
    bu_ptbl_free(search_results);
}


/* Internal version of deprecated function */
struct db_full_path_list *
_db_search_full_paths(void *searchplan,        /* search plan */
		      struct db_full_path_list *pathnames,      /* list of pathnames to traverse */
		      struct db_i *dbip,
		      struct rt_wdb *wdbp)
{
    int i;
    struct directory *dp;
    struct db_full_path dfp;
    struct db_full_path *dfptr;
    struct db_full_path_list *new_entry = NULL;
    struct db_full_path_list *currentpath = NULL;
    struct db_full_path_list *searchresults_list = NULL;
    struct bu_ptbl *searchresults = NULL;
    BU_ALLOC(searchresults_list, struct db_full_path_list);
    BU_LIST_INIT(&(searchresults_list->l));
    BU_ALLOC(searchresults, struct bu_ptbl);
    bu_ptbl_init(searchresults, 8, "initialize searchresults table");
    /* If nothing is passed in, try to get the list of toplevel objects */
    if (BU_LIST_IS_EMPTY(&(pathnames->l))) {
	db_full_path_init(&dfp);
	for (i = 0; i < RT_DBNHASH; i++) {
	    for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		if (dp->d_nref == 0 && (dp->d_addr != RT_DIR_PHONY_ADDR)) {
		    if (db_string_to_path(&dfp, dbip, dp->d_namep) < 0)
			continue; /* skip */

		    BU_ALLOC(new_entry, struct db_full_path_list);
		    BU_ALLOC(new_entry->path, struct db_full_path);

		    db_full_path_init(new_entry->path);
		    db_dup_full_path(new_entry->path, (const struct db_full_path *)&dfp);
		    BU_LIST_PUSH(&(pathnames->l), &(new_entry->l));
		}
	    }
	}
	db_free_full_path(&dfp);
    }
    for (BU_LIST_FOR(currentpath, db_full_path_list, &(pathnames->l))) {
	struct bu_ptbl *full_paths = NULL;
	struct db_full_path *start_path = NULL;
	struct db_node_t curr_node;
	struct list_client_data_t lcd;

	BU_ALLOC(full_paths, struct bu_ptbl);
	BU_PTBL_INIT(full_paths);
	BU_ALLOC(start_path, struct db_full_path);
	db_dup_full_path(start_path, currentpath->path);
	curr_node.path = start_path;
	/* by convention, a top level node is "unioned" into the global database */
	DB_FULL_PATH_SET_CUR_BOOL(curr_node.path, 2);
	bu_ptbl_ins(full_paths, (long *)start_path);
	lcd.dbip = wdbp->dbip;
	lcd.full_paths = full_paths;
	db_fullpath_list(start_path, wdbp->wdb_resp, (genptr_t *)&lcd);

	for (i = 0; i < (int)BU_PTBL_LEN(full_paths); i++) {
	    curr_node.path = (struct db_full_path *)BU_PTBL_GET(full_paths, i);
	    curr_node.flags = 0;
	    curr_node.full_paths = full_paths;
	    find_execute_plans(wdbp->dbip, wdbp, searchresults, &curr_node, (struct db_plan_t *)searchplan);
	}
	db_free_search_tbl(full_paths);
	bu_free(full_paths, "free search container");

    }
    for(i = 0; i < (int)BU_PTBL_LEN(searchresults); i++) {
	BU_ALLOC(new_entry, struct db_full_path_list);
	BU_ALLOC(new_entry->path, struct db_full_path);
	dfptr = (struct db_full_path *)BU_PTBL_GET(searchresults, i);
	db_full_path_init(new_entry->path);
	db_dup_full_path(new_entry->path, dfptr);
	BU_LIST_PUSH(&(searchresults_list->l), &(new_entry->l));
    }
    for(i = (int)BU_PTBL_LEN(searchresults) - 1; i >= 0; i--) {
	dfptr = (struct db_full_path *)BU_PTBL_GET(searchresults, i);
	db_free_full_path(dfptr);
    }
    bu_ptbl_free(searchresults);
    return searchresults_list;
}


struct db_full_path_list *
db_search_full_paths(void *searchplan,        /* search plan */
		     struct db_full_path_list *pathnames,      /* list of pathnames to traverse */
		     struct db_i *dbip,
		     struct rt_wdb *wdbp)
{
    return _db_search_full_paths(searchplan, pathnames, dbip, wdbp);
}


struct bu_ptbl *
db_search_unique_objects(void *searchplan,        /* search plan */
			 struct db_full_path_list *pathnames,      /* list of pathnames to traverse */
			 struct db_i *dbip,
			 struct rt_wdb *wdbp)
{
    struct bu_ptbl *uniq_db_objs = NULL;
    struct db_full_path_list *entry = NULL;
    struct db_full_path_list *search_results = NULL;

    BU_ALLOC(uniq_db_objs, struct bu_ptbl);

    search_results = _db_search_full_paths(searchplan, pathnames, dbip, wdbp);
    bu_ptbl_init(uniq_db_objs, 8, "initialize ptr table");
    for (BU_LIST_FOR(entry, db_full_path_list, &(search_results->l))) {
	bu_ptbl_ins_unique(uniq_db_objs, (long *)entry->path->fp_names[entry->path->fp_len - 1]);
    }
    _db_free_full_path_list(search_results);

    return uniq_db_objs;
}


void *
db_search_formplan(char **argv, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp)) {
    return (void *)db_search_form_plan(argv, 0);
}


/*********** New search functionality ******************/

int
db_search(struct bu_ptbl *search_results,
	  int search_flags,
	  const char *plan_str,
	  int path_cnt,
	  struct directory **paths,
	  struct rt_wdb *wdbp)
{
    int result_cnt = 0;
    struct db_plan_t *dbplan = NULL;

    /* Note that dbplan references strings using memory
     * in the following two objects, so they mustn't be
     * freed until the plan is freed.*/
    char **plan_argv = (char **)bu_calloc(strlen(plan_str) + 1, sizeof(char *), "plan argv");

    /* make a copy so we can mess with it */
    char *mutable_plan_str = bu_strdup(plan_str);


    /* get the plan string into an argv array */
    bu_argv_from_string(&plan_argv[0], strlen(plan_str), mutable_plan_str);
    if (!(search_flags & DB_SEARCH_QUIET)) {
	dbplan = db_search_form_plan(plan_argv, 0);
    } else {
	dbplan = db_search_form_plan(plan_argv, 1);
    }
    /* No plan, no search */
    if (!dbplan) {
	bu_free(mutable_plan_str, "free strdup");
	bu_free((char *)plan_argv, "free plan argv");
	return 0;
    }
    /* If the idea was to test the plan string and we *do* have
     * a plan, return success */
    if (!search_results && !paths) {
	db_search_free_plan((void **)&dbplan);
	bu_free(mutable_plan_str, "free strdup");
	bu_free((char *)plan_argv, "free plan argv");
	return 1;
    }

    /* execute the plan */
    {
	int i = 0;
	struct bu_ptbl *full_paths = NULL;
	struct list_client_data_t lcd;

	/* First, check if search_results is initialized - don't trust the caller to do it,
	 * but it's fine if they did */
	if (search_results && search_results != BU_PTBL_NULL) {
	    if (!BU_PTBL_IS_INITIALIZED(search_results))
		BU_PTBL_INIT(search_results);
	}

	/* Build a set of all full paths under the supplied paths, including the starting
	 * paths themselves */
	if (!(search_flags & DB_SEARCH_FLAT)) {
	    BU_ALLOC(full_paths, struct bu_ptbl);
	    BU_PTBL_INIT(full_paths);
	    lcd.dbip = wdbp->dbip;
	    lcd.full_paths = full_paths;
	    lcd.flags = search_flags;
	}

	/* If we're doing a flat search, we can handle everything in this loop.
	 * Otherwise, we're building a list of full paths to be handled in a
	 * second pass. */
	for (i = 0; i < path_cnt; i++) {
	    struct directory *curr_dp = paths[i];
	    struct db_full_path *start_path = NULL;

	    if (curr_dp == RT_DIR_NULL) {
		continue;
	    }

	    if ((search_flags & DB_SEARCH_HIDDEN) || !(curr_dp->d_flags & RT_DIR_HIDDEN)) {

		BU_ALLOC(start_path, struct db_full_path);
		db_full_path_init(start_path);
		db_add_node_to_full_path(start_path, curr_dp);
		DB_FULL_PATH_SET_CUR_BOOL(start_path, 2);

		/* For a flat search, we don't need to build a table of paths -
		 * just run the filters on the path */
		if (search_flags & DB_SEARCH_FLAT) {
		    struct db_node_t curr_node;
		    /* by convention, a top level node is "unioned" into the global database */
		    curr_node.path = start_path;
		    curr_node.flags = search_flags;
		    curr_node.matched_filters = 0;
		    find_execute_plans(wdbp->dbip, wdbp, search_results, &curr_node, dbplan);
		    result_cnt += curr_node.matched_filters;
		    DB_FULL_PATH_POP(start_path);
		} else {
		    /* by convention, a top level node is "unioned" into the global database */
		    bu_ptbl_ins(full_paths, (long *)start_path);
		    /* Use the initial path to tree-walk and build a set of all paths below
		     * start_path */
		    db_fullpath_list(start_path, wdbp->wdb_resp, (genptr_t *)&lcd);
		}
	    }
	}

	if (!(search_flags & DB_SEARCH_FLAT)) {
	    for (i = 0; i < (int)BU_PTBL_LEN(full_paths); i++) {
		struct db_node_t curr_node;
		curr_node.path = (struct db_full_path *)BU_PTBL_GET(full_paths, i);
		curr_node.full_paths = full_paths;
		curr_node.flags = search_flags;
		curr_node.matched_filters = 0;
		find_execute_plans(wdbp->dbip, wdbp, search_results, &curr_node, dbplan);
		result_cnt += curr_node.matched_filters;
	    }

	    /* Done with the paths now - we have our answer */
	    db_free_search_tbl(full_paths);
	    bu_free(full_paths, "free search container");
	}
    }

    db_search_free_plan((void **)&dbplan);
    bu_free(mutable_plan_str, "free strdup");
    bu_free((char *)plan_argv, "free plan argv");

    return result_cnt;
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

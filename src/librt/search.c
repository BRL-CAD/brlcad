/*                        S E A R C H . C
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

#include "cmd.h"

#include "db.h"
#include "./search.h"


/*
 * D B _ F R E E _ F U L L  _ P A T H _ L I S T
 *
 * Free all entries and the list of a db_full_path_list
 */
void
db_free_full_path_list(struct db_full_path_list *path_list)
{
	struct db_full_path_list *currentpath;
	if (path_list) {
		if (!BU_LIST_IS_EMPTY(&(path_list->l))) {
			while (BU_LIST_WHILE(currentpath, db_full_path_list, &(path_list->l))) {
				db_free_full_path(currentpath->path);
				BU_LIST_DEQUEUE((struct bu_list *)currentpath);
				bu_free(currentpath, "free db_full_path_list entry");
			}
		}
		bu_free(path_list, "free path_list");
	}
}


/*
 * D B _ F U L L P A T H _ T R A V E R S E _ S U B T R E E
 *
 * A generic traversal function maintaining awareness of
 * the full path to a given object.
 */
void
db_fullpath_traverse_subtree(union tree *tp,
			     void (*traverse_func) (struct db_i *, struct rt_wdb *, struct db_full_path_list *, struct db_full_path *,
						    void (*) (struct db_i *, struct rt_wdb *, struct db_full_path_list *, struct db_full_path *, genptr_t),
						    void (*) (struct db_i *, struct rt_wdb *, struct db_full_path_list *, struct db_full_path *, genptr_t),
						    struct resource *,
						    genptr_t),
			     struct db_i *dbip,
			     struct rt_wdb *wdbp,
			     struct db_full_path_list *results,
			     struct db_full_path *dfp,
			     void (*comb_func) (struct db_i *, struct rt_wdb *, struct db_full_path_list *, struct db_full_path *, genptr_t),
		             void (*leaf_func) (struct db_i *, struct rt_wdb *, struct db_full_path_list *, struct db_full_path *, genptr_t),
			     struct resource *resp,
			     genptr_t client_data)
{
    struct directory *dp;

    if (!tp)
	return;

    RT_CK_FULL_PATH(dfp);
    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);
    RT_CK_RESOURCE(resp);

    switch (tp->tr_op) {

	case OP_DB_LEAF:
	    if ((dp=db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
		return;
	    } else {
		db_add_node_to_full_path(dfp, dp);
		traverse_func(dbip, wdbp, results, dfp, comb_func, leaf_func, resp, client_data);
		DB_FULL_PATH_POP(dfp);
		break;
	    }
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    db_fullpath_traverse_subtree(tp->tr_b.tb_left, traverse_func, dbip, wdbp, results, dfp, comb_func, leaf_func, resp, client_data);
	    db_fullpath_traverse_subtree(tp->tr_b.tb_right, traverse_func, dbip, wdbp, results, dfp, comb_func, leaf_func, resp, client_data);
	    break;
	default:
	    bu_log("db_functree_subtree: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("db_functree_subtree: unrecognized operator\n");
    }
}


/*
 * D B _ F U L L P A T H _ T R A V E R S E
 *
 * This subroutine is called for a no-frills tree-walk,
 * with the provided subroutines being called when entering and
 * exiting combinations and at leaf (solid) nodes.
 *
 * This routine is recursive, so no variables may be declared static.
 *
 * Unlike db_preorder_traverse, this routine and its subroutines
 * use db_full_path structures instead of directory structures.
 */
void
db_fullpath_traverse(struct db_i *dbip,
		     struct rt_wdb *wdbp,
		     struct db_full_path_list *results,
		     struct db_full_path *dfp,
		     void (*comb_func) (struct db_i *, struct rt_wdb *, struct db_full_path_list *, struct db_full_path *, genptr_t),
		     void (*leaf_func) (struct db_i *, struct rt_wdb *, struct db_full_path_list *, struct db_full_path *, genptr_t),
		     struct resource *resp,
		     genptr_t client_data)
{
    struct directory *dp;
    size_t i;
    RT_CK_FULL_PATH(dfp);
    RT_CK_DBI(dbip);

    dp = DB_FULL_PATH_CUR_DIR(dfp);

    if (dp->d_flags & RT_DIR_COMB) {
	/* entering region */
	if (comb_func)
	    comb_func(dbip, wdbp, results, dfp, client_data);
	if (db_version(dbip) < 5) {
	    union record *rp;
	    struct directory *mdp;
	    /*
	     * Load the combination into local record buffer
	     * This is in external v4 format.
	     */
	    if ((rp = db_getmrec(dbip, dp)) == (union record *)0)
		return;
	    /* recurse */
	    for (i=1; i < dp->d_len; i++) {
		if ((mdp = db_lookup(dbip, rp[i].M.m_instname,
				     LOOKUP_QUIET)) == RT_DIR_NULL) {
		    continue;
		} else {
		    db_add_node_to_full_path(dfp, mdp);
		    db_fullpath_traverse(dbip, wdbp, results, dfp, comb_func, leaf_func, resp, client_data);
		    DB_FULL_PATH_POP(dfp);
		}
	    }
	    bu_free((char *)rp, "db_preorder_traverse[]");
	} else {
	    struct rt_db_internal in;
	    struct rt_comb_internal *comb;

	    if (rt_db_get_internal5(&in, dp, dbip, NULL, resp) < 0)
		return;

	    comb = (struct rt_comb_internal *)in.idb_ptr;

	    db_fullpath_traverse_subtree(comb->tree, db_fullpath_traverse, dbip, wdbp, results, dfp, comb_func, leaf_func, resp, client_data);

	    rt_db_free_internal(&in);
	}
    }
    if (dp->d_flags & RT_DIR_SOLID || dp->d_major_type & DB5_MAJORTYPE_BINARY_MASK) {
	/* at leaf */
	if (leaf_func)
	    leaf_func(dbip, wdbp, results, dfp, client_data);
    }
}


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
    { "-bl",        N_BELOW,        c_below,        O_ZERO },
    { "-below",     N_BELOW,        c_below,        O_ZERO },
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


static struct db_plan_t *
palloc(enum db_search_ntype t, int (*f)(struct db_plan_t *, struct db_full_path *, struct db_i *, struct rt_wdb *, struct db_full_path_list *))
{
    struct db_plan_t *new;

    new = bu_calloc(1, sizeof(struct db_plan_t), "Allocate struct db_plan_t structure");
    new->type = t;
    new->eval = f;
    return new;
}


/*
 * (expression) functions --
 *
 * True if expression is true.
 */
HIDDEN int
f_expr(struct db_plan_t *plan, struct db_full_path *entry, struct db_i *dbip, struct rt_wdb *wdbp, struct db_full_path_list *results)
{
    struct db_plan_t *p = NULL;
    int state = 0;

    for (p = plan->p_data[0]; p && (state = (p->eval)(p, entry, dbip, wdbp, results)); p = p->next)
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
    (*resultplan) = (palloc(N_OPENPAREN, (int (*)(struct db_plan_t *, struct db_full_path *, struct db_i *, struct rt_wdb *, struct db_full_path_list *))-1));
    return BRLCAD_OK;
}


HIDDEN int
c_closeparen(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    (*resultplan) = (palloc(N_CLOSEPAREN, (int (*)(struct db_plan_t *, struct db_full_path *, struct db_i *, struct rt_wdb *, struct db_full_path_list *))-1));
    return BRLCAD_OK;
}


/*
 * ! expression functions --
 *
 * Negation of a primary; the unary NOT operator.
 */
HIDDEN int
f_not(struct db_plan_t *plan, struct db_full_path *entry, struct db_i *dbip, struct rt_wdb *wdbp, struct db_full_path_list *results)
{
    struct db_plan_t *p = NULL;
    int state = 0;

    for (p = plan->p_data[0]; p && (state = (p->eval)(p, entry, dbip, wdbp, results)); p = p->next)
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
find_execute_nested_plans(struct db_i *dbip, struct rt_wdb *wdbp, struct db_full_path_list *results, struct db_full_path *entry, genptr_t inputplan) {
    struct db_plan_t *p = NULL;
    struct db_plan_t *plan = (struct db_plan_t *)inputplan;
    int state = 0;

    for (p = plan; p && (state = (p->eval)(p, entry, dbip, wdbp, results)); p = p->next)
	; /* do nothing */

    return state;
}


/*
 * -above expression functions --
 *
 * Conduct the test described by expression on all levels
 * above the current level in the tree - in this case meaning
 * following the tree path back to the root, NOT testing all
 * objects at any level above the current object depth.
 */
HIDDEN int
f_above(struct db_plan_t *plan, struct db_full_path *entry, struct db_i *dbip, struct rt_wdb *wdbp, struct db_full_path_list *results)
{
    int state = 0;
    struct db_full_path abovepath;
    db_full_path_init(&abovepath);
    db_dup_full_path(&abovepath, entry);
    DB_FULL_PATH_POP(&abovepath);
    while ((abovepath.fp_len > 0) && (state == 0)) {
	state = find_execute_nested_plans(dbip, wdbp, results, &abovepath, plan->ab_data[0]);
	DB_FULL_PATH_POP(&abovepath);
    }
    db_free_full_path(&abovepath);
    return state;
}


HIDDEN int
c_above(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    (*resultplan) =  (palloc(N_ABOVE, f_above));
    return BRLCAD_OK;
}


/*
 * D B _ F U L L P A T H _ S T A T E F U L _ T R A V E R S E _ S U B T R E E
 *
 * A generic traversal function maintaining awareness of
 * the full path to a given object and with function types allowing
 * a return of an integer state.
 */
HIDDEN int
db_fullpath_stateful_traverse_subtree(union tree *tp,
				      int (*traverse_func) (struct db_i *, struct rt_wdb *, struct db_full_path_list *results, struct db_full_path *,
							    int (*) (struct db_i *, struct rt_wdb *, struct db_full_path_list *, struct db_full_path *, genptr_t),
							    int (*) (struct db_i *, struct rt_wdb *, struct db_full_path_list *, struct db_full_path *, genptr_t),
							    struct resource *,
							    genptr_t),
				      struct db_i *dbip, struct rt_wdb *wdbp, struct db_full_path_list *results,
				      struct db_full_path *dfp,
				      int (*comb_func) (struct db_i *, struct rt_wdb *, struct db_full_path_list *, struct db_full_path *, genptr_t),
				      int (*leaf_func) (struct db_i *, struct rt_wdb *, struct db_full_path_list *, struct db_full_path *, genptr_t),
				      struct resource *resp,
				      genptr_t client_data)
{
    struct directory *dp;
    int state = 0;
    if (!tp)
	return 0;

    RT_CK_FULL_PATH(dfp);
    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);
    RT_CK_RESOURCE(resp);

    switch (tp->tr_op) {

	case OP_DB_LEAF:
	    if ((dp=db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
		return 0;
	    } else {
		db_add_node_to_full_path(dfp, dp);
		state = traverse_func(dbip, wdbp, results, dfp, comb_func, leaf_func, resp, client_data);
		DB_FULL_PATH_POP(dfp);
		if (state == 1) {
		    return 1;
		} else {
		    return 0;
		}
	    }
	    break;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    state = db_fullpath_stateful_traverse_subtree(tp->tr_b.tb_left, traverse_func, dbip, wdbp, results, dfp, comb_func, leaf_func, resp, client_data);
	    if (state == 1) return 1;
	    state = db_fullpath_stateful_traverse_subtree(tp->tr_b.tb_right, traverse_func, dbip, wdbp, results, dfp, comb_func, leaf_func, resp, client_data);
	    if (state == 1) {
		return 1;
	    } else {
		return 0;
	    }
	    break;
	default:
	    bu_log("db_functree_subtree: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("db_functree_subtree: unrecognized operator\n");
    }

    /* Silence compiler warnings */
    return 0;
}


/*
 * D B _ F U L L P A T H _ S T A T E F U L _ T R A V E R S E
 *
 * This subroutine is called for a no-frills tree-walk,
 * with the provided subroutines being called when entering and
 * exiting combinations and at leaf (solid) nodes.
 *
 * This routine is recursive, so no variables may be declared static.
 *
 * Unlike db_preorder_traverse, this routine and its subroutines
 * use db_full_path structures instead of directory structures.
 *
 * This walker will hault if either comb_func or leaf_func return
 * a value > 0 and return that value.
 */
HIDDEN int
db_fullpath_stateful_traverse(struct db_i *dbip, struct rt_wdb *wdbp, struct db_full_path_list *results,
			      struct db_full_path *dfp,
			      int (*comb_func) (struct db_i *, struct rt_wdb *, struct db_full_path_list *, struct db_full_path *, genptr_t),
			      int (*leaf_func) (struct db_i *, struct rt_wdb *, struct db_full_path_list *, struct db_full_path *, genptr_t),
			      struct resource *resp,
			      genptr_t client_data)
{
    struct directory *dp;
    size_t i;
    int state = 0;
    RT_CK_FULL_PATH(dfp);
    RT_CK_DBI(dbip);

    dp = DB_FULL_PATH_CUR_DIR(dfp);

    if (dp->d_flags & RT_DIR_COMB) {
	/* entering region */
	if (comb_func)
	    if (comb_func(dbip, wdbp, results, dfp, client_data)) return 1;
	if (db_version(dbip) < 5) {
	    union record *rp;
	    struct directory *mdp;
	    /*
	     * Load the combination into local record buffer
	     * This is in external v4 format.
	     */
	    if ((rp = db_getmrec(dbip, dp)) == (union record *)0)
		return 0;
	    /* recurse */
	    for (i=1; i < dp->d_len; i++) {
		if ((mdp = db_lookup(dbip, rp[i].M.m_instname,
				     LOOKUP_QUIET)) == RT_DIR_NULL) {
		    continue;
		} else {
		    db_add_node_to_full_path(dfp, mdp);
		    state = db_fullpath_stateful_traverse(dbip, wdbp, results, dfp, comb_func, leaf_func, resp, client_data);
		    DB_FULL_PATH_POP(dfp);
		    if (state == 1) {
			return 1;
		    } else {
			return 0;
		    }
		}
	    }
	    bu_free((char *)rp, "db_preorder_traverse[]");
	} else {
	    struct rt_db_internal in;
	    struct rt_comb_internal *comb;

	    if (rt_db_get_internal5(&in, dp, dbip, NULL, resp) < 0)
		return 0;

	    comb = (struct rt_comb_internal *)in.idb_ptr;

	    state = db_fullpath_stateful_traverse_subtree(comb->tree, db_fullpath_stateful_traverse, dbip, wdbp, results, dfp, comb_func, leaf_func, resp, client_data);

	    rt_db_free_internal(&in);
	    if (state == 1) {
		return 1;
	    } else {
		return 0;
	    }
	}
    }
    if (dp->d_flags & RT_DIR_SOLID || dp->d_major_type & DB5_MAJORTYPE_BINARY_MASK) {
	/* at leaf */
	if (leaf_func) {
	    if (leaf_func(dbip, wdbp, results, dfp, client_data)) {
		return 1;
	    } else {
		return 0;
	    }
	}
    }

    return 0;
}


/*
 * -below expression functions --
 *
 * Conduct the test described by expression on all objects
 * below the current object in the tree.
 */
HIDDEN int
f_below(struct db_plan_t *plan, struct db_full_path *entry, struct db_i *dbip, struct rt_wdb *wdbp, struct db_full_path_list *results)
{
    struct db_full_path belowpath;
    struct rt_db_internal in;
    struct rt_comb_internal *comb;
    int state = 0;

    db_full_path_init(&belowpath);
    db_dup_full_path(&belowpath, entry);

    if (DB_FULL_PATH_CUR_DIR(entry)->d_flags & RT_DIR_COMB) {
	if (rt_db_get_internal5(&in, DB_FULL_PATH_CUR_DIR(entry), dbip, NULL, wdbp->wdb_resp) < 0)
	    return 0;

	comb = (struct rt_comb_internal *)in.idb_ptr;

	state = db_fullpath_stateful_traverse_subtree(comb->tree, db_fullpath_stateful_traverse, dbip, wdbp, results, &belowpath, find_execute_nested_plans, find_execute_nested_plans, wdbp->wdb_resp, plan->bl_data[0]);

	rt_db_free_internal(&in);
    }
    db_free_full_path(&belowpath);
    if (state >= 1) {
	return 1;
    } else {
	return 0;
    }

}


HIDDEN int
c_below(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    (*resultplan) =  (palloc(N_BELOW, f_below));
    return BRLCAD_OK;
}


/*
 * expression -o expression functions --
 *
 * Alternation of primaries; the OR operator.  The second expression is
 * not evaluated if the first expression is true.
 */
HIDDEN int
f_or(struct db_plan_t *plan, struct db_full_path *entry, struct db_i *dbip, struct rt_wdb *wdbp, struct db_full_path_list *results)
{
    struct db_plan_t *p = NULL;
    int state = 0;

    for (p = plan->p_data[0]; p && (state = (p->eval)(p, entry, dbip, wdbp, results)); p = p->next)
	; /* do nothing */

    if (state)
	return 1;

    for (p = plan->p_data[1]; p && (state = (p->eval)(p, entry, dbip, wdbp, results)); p = p->next)
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
f_name(struct db_plan_t *plan, struct db_full_path *entry, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct db_full_path_list *UNUSED(results))
{
    return !bu_fnmatch(plan->c_data, DB_FULL_PATH_CUR_DIR(entry)->d_namep, 0);
}


HIDDEN int
c_name(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *new;

    new = palloc(N_NAME, f_name);
    new->c_data = pattern;
    (*resultplan) = new;
    return BRLCAD_OK;
}


/*
 * -iname function --
 *
 * True if the basename of the filename being examined
 * matches pattern using case insensitive Pattern Matching Notation S3.14
 */
HIDDEN int
f_iname(struct db_plan_t *plan, struct db_full_path *entry, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct db_full_path_list *UNUSED(results))
{
    return !bu_fnmatch(plan->c_data, DB_FULL_PATH_CUR_DIR(entry)->d_namep, BU_FNMATCH_CASEFOLD);
}


HIDDEN int
c_iname(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *new;

    new = palloc(N_INAME, f_iname);
    new->ci_data = pattern;
    (*resultplan) = new;
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
f_regex(struct db_plan_t *plan, struct db_full_path *entry, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct db_full_path_list *UNUSED(results))
{
    return !(regexec(&plan->regexp_data, db_path_to_string(entry), 0, NULL, 0));
}


HIDDEN int
c_regex_common(enum db_search_ntype type, char *regexp, int icase, struct db_plan_t **resultplan)
{
    regex_t reg;
    struct db_plan_t *new;
    int rv;

    bu_log("Matching regular expression: %s\n", regexp);
    if (icase == 1) {
	rv = regcomp(&reg, regexp, REG_NOSUB|REG_EXTENDED|REG_ICASE);
    } else {
	rv = regcomp(&reg, regexp, REG_NOSUB|REG_EXTENDED);
    }
    if (rv != 0) {
	bu_log("Error - regex compile did not succeed: %s\n", regexp);
	return BRLCAD_ERROR;
    }
    new = palloc(type, f_regex);
    new->regexp_data = reg;
    (*resultplan) = new;
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


/*
 * -attr functions --
 *
 * True if the database object being examined has the attribute
 * supplied to the attr option
 */
HIDDEN int
f_attr(struct db_plan_t *plan, struct db_full_path *entry, struct db_i *dbip, struct rt_wdb *UNUSED(wdbp), struct db_full_path_list *UNUSED(results))
{
    struct bu_vls attribname;
    struct bu_vls value;
    struct bu_attribute_value_set avs;
    struct bu_attribute_value_pair *avpp;
    size_t equalpos = 0;
    int checkval = 0;
    int strcomparison = 0;
    size_t i;
    bu_vls_init(&attribname);
    bu_vls_init(&value);


    /* Check for unescaped >, < or = characters.  If
     * present, the attribute must not only be present
     * but the value assigned to the attribute must
     * satisfy the logical expression.  In the case
     * where a > or < is used with a string argument
     * the behavior will follow that of the strcmp
     * comparison command.  In the case of equality
     * between strings, fnmatch is used to support
     * pattern matching
     */

    while ((equalpos < strlen(plan->attr_data)) &&
	   (plan->attr_data[equalpos] != '=') &&
	   (plan->attr_data[equalpos] != '>') &&
	   (plan->attr_data[equalpos] != '<')) {
    	if ((plan->attr_data[equalpos] == '/') && (plan->attr_data[equalpos + 1] == '=')) {equalpos++;}
    	if ((plan->attr_data[equalpos] == '/') && (plan->attr_data[equalpos + 1] == '<')) {equalpos++;}
    	if ((plan->attr_data[equalpos] == '/') && (plan->attr_data[equalpos + 1] == '>')) {equalpos++;}
    	equalpos++;
    }


    if (equalpos == strlen(plan->attr_data)) {
	/*No logical expression given - just copy attribute name*/
        bu_vls_strcpy(&attribname, plan->attr_data);
    } else {
	checkval = 1; /*Assume simple equality comparison, then check for other cases and change if found.*/
	if ((plan->attr_data[equalpos] == '>') && (plan->attr_data[equalpos + 1] != '=')) {checkval = 2;}
	if ((plan->attr_data[equalpos] == '<') && (plan->attr_data[equalpos + 1] != '=')) {checkval = 3;}
	if ((plan->attr_data[equalpos] == '=') && (plan->attr_data[equalpos + 1] == '>')) {checkval = 4;}
	if ((plan->attr_data[equalpos] == '=') && (plan->attr_data[equalpos + 1] == '<')) {checkval = 5;}
        if ((plan->attr_data[equalpos] == '>') && (plan->attr_data[equalpos + 1] == '=')) {checkval = 4;}
        if ((plan->attr_data[equalpos] == '<') && (plan->attr_data[equalpos + 1] == '=')) {checkval = 5;}

	bu_vls_strncpy(&attribname, plan->attr_data, equalpos);
	if (checkval < 4) {
	    bu_vls_strncpy(&value, &(plan->attr_data[equalpos+1]), strlen(plan->attr_data) - equalpos - 1);
	} else {
	    bu_vls_strncpy(&value, &(plan->attr_data[equalpos+2]), strlen(plan->attr_data) - equalpos - 1);
	}
    }

    /* Now that we have the value, check to see if it is all numbers.  If so,
     * use numerical comparison logic - otherwise use string logic.
     */

    for (i = 0; i < strlen(bu_vls_addr(&value)); i++) {
	if (!(isdigit(bu_vls_addr(&value)[i]))) strcomparison = 1;
    }

    /* Get attributes for object.
     */

    bu_avs_init_empty(&avs);
    db5_get_attributes(dbip, &avs, DB_FULL_PATH_CUR_DIR(entry));
    avpp = avs.avp;

    /* Check all attributes for a match to the requested
     * attribute.  If an expression was supplied, check the
     * value of any matches to the attribute name in the
     * logical expression before returning success
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
    struct db_plan_t *new;

    new = palloc(N_ATTR, f_attr);
    new->attr_data = pattern;
    (*resultplan) = new;
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
f_stdattr(struct db_plan_t *UNUSED(plan), struct db_full_path *entry, struct db_i *dbip, struct rt_wdb *UNUSED(wdbp), struct db_full_path_list *UNUSED(results))
{
    struct bu_attribute_value_set avs;
    struct bu_attribute_value_pair *avpp;
    size_t i;
    int found_nonstd_attr = 0;
    int found_attr = 0;

    /* Get attributes for object and check all of
     * them to see if there is not a match to the
     * standard attributes.  If any is found return
     * failure, otherwise success.
     */

    bu_avs_init_empty(&avs);
    db5_get_attributes(dbip, &avs, DB_FULL_PATH_CUR_DIR(entry));
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
    struct db_plan_t *new;

    new = palloc(N_STDATTR, f_stdattr);
    (*resultplan) = new;
    return BRLCAD_OK;
}


/*
 * -type function --
 *
 * Search based on the type of the object - primitives are matched
 * based on their primitive type (tor, tgc, arb4, etc.) and combinations
 * are matched based on whether they are a combination or region.
 */
HIDDEN int
f_type(struct db_plan_t *plan, struct db_full_path *entry, struct db_i *dbip, struct rt_wdb *wdbp, struct db_full_path_list *UNUSED(results))
{
    struct rt_db_internal intern;
    int type_match = 0;
    int type;

    rt_db_get_internal(&intern, DB_FULL_PATH_CUR_DIR(entry), dbip, (fastf_t *)NULL, &rt_uniresource);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD) return 0;

    /* Eventually this whole switch statement needs to go away
     * in favor of a function to query the primitive's short name
     * and use that for the comparison - will be MUCH shorter and
     * simpler.
     */
    switch (intern.idb_minor_type) {
	case DB5_MINORTYPE_BRLCAD_TOR:
	    type_match = (!bu_fnmatch(plan->type_data, "tor", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_TGC:
	    type_match = (!bu_fnmatch(plan->type_data, "tgc", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_ELL:
	    type_match = (!bu_fnmatch(plan->type_data, "ell", 0));
	    break;
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
	case DB5_MINORTYPE_BRLCAD_ARS:
	    type_match = (!bu_fnmatch(plan->type_data, "ars", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_HALF:
	    type_match = (!bu_fnmatch(plan->type_data, "half", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_REC:
	    type_match = (!bu_fnmatch(plan->type_data, "rec", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_POLY:
	    type_match = (!bu_fnmatch(plan->type_data, "poly", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_BSPLINE:
	    type_match = (!bu_fnmatch(plan->type_data, "spline", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_SPH:
	    type_match = (!bu_fnmatch(plan->type_data, "sph", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_NMG:
	    type_match = (!bu_fnmatch(plan->type_data, "nmg", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_EBM:
	    type_match = (!bu_fnmatch(plan->type_data, "ebm", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_VOL:
	    type_match = (!bu_fnmatch(plan->type_data, "vol", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_ARBN:
	    type_match = (!bu_fnmatch(plan->type_data, "arbn", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_PIPE:
	    type_match = (!bu_fnmatch(plan->type_data, "pipe", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_PARTICLE:
	    type_match = (!bu_fnmatch(plan->type_data, "part", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_RPC:
	    type_match = (!bu_fnmatch(plan->type_data, "rpc", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_RHC:
	    type_match = (!bu_fnmatch(plan->type_data, "rhc", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_EPA:
	    type_match = (!bu_fnmatch(plan->type_data, "epa", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_EHY:
	    type_match = (!bu_fnmatch(plan->type_data, "ehy", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_ETO:
	    type_match = (!bu_fnmatch(plan->type_data, "eto", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_GRIP:
	    type_match = (!bu_fnmatch(plan->type_data, "grip", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_JOINT:
	    type_match = (!bu_fnmatch(plan->type_data, "joint", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_HF:
	    type_match = (!bu_fnmatch(plan->type_data, "hf", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_DSP:
	    type_match = (!bu_fnmatch(plan->type_data, "dsp", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_SKETCH:
	    type_match = (!bu_fnmatch(plan->type_data, "sketch", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_EXTRUDE:
	    type_match = (!bu_fnmatch(plan->type_data, "extrude", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_SUBMODEL:
	    type_match = (!bu_fnmatch(plan->type_data, "submodel", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_CLINE:
	    type_match = (!bu_fnmatch(plan->type_data, "cline", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_BOT:
	    type_match = (!bu_fnmatch(plan->type_data, "bot", 0));
	    break;
	case DB5_MINORTYPE_BRLCAD_COMBINATION:
	    if (DB_FULL_PATH_CUR_DIR(entry)->d_flags & RT_DIR_REGION) {
		if ((!bu_fnmatch(plan->type_data, "r", 0)) || (!bu_fnmatch(plan->type_data, "reg", 0))  || (!bu_fnmatch(plan->type_data, "region", 0))) {
		    type_match = 1;
		}
	    }
	    if ((!bu_fnmatch(plan->type_data, "c", 0)) || (!bu_fnmatch(plan->type_data, "comb", 0)) || (!bu_fnmatch(plan->type_data, "combination", 0))) {
		type_match = 1;
	    }
	    break;
	case DB5_MINORTYPE_BRLCAD_BREP:
	    type_match = (!bu_fnmatch(plan->type_data, "brep", 0));
	    break;
	default:
	    type_match = (!bu_fnmatch(plan->type_data, "other", 0));
	    break;
    }

    rt_db_free_internal(&intern);
    return type_match;
}


HIDDEN int
c_type(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *new;

    new = palloc(N_TYPE, f_type);
    new->type_data = pattern;
    (*resultplan) = new;
    return BRLCAD_OK;
}


/*
 * -maxdepth function --
 *
 * True if the object being examined is at depth <= the
 * supplied depth.
 *
 */
HIDDEN int
f_maxdepth(struct db_plan_t *plan, struct db_full_path *entry, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct db_full_path_list *UNUSED(results))
{
    struct db_full_path depthtest;
    int depthcount = -1;
    db_full_path_init(&depthtest);
    db_dup_full_path(&depthtest, entry);
    while (depthtest.fp_len > 0) {
	depthcount++;
	DB_FULL_PATH_POP(&depthtest);
    }
    db_free_full_path(&depthtest);
    return depthcount <= plan->max_data;
}


HIDDEN int
c_maxdepth(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *new;

    new = palloc(N_MAXDEPTH, f_maxdepth);
    new->max_data = atoi(pattern);
    (*resultplan) = new;
    return BRLCAD_OK;
}


/*
 * -mindepth function --
 *
 * True if the object being examined is at depth >= the
 * supplied depth.
 *
 */
HIDDEN int
f_mindepth(struct db_plan_t *plan, struct db_full_path *entry, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct db_full_path_list *UNUSED(results))
{
    struct db_full_path depthtest;
    int depthcount = -1;
    db_full_path_init(&depthtest);
    db_dup_full_path(&depthtest, entry);
    while (depthtest.fp_len > 0) {
	depthcount++;
	DB_FULL_PATH_POP(&depthtest);
    }
    db_free_full_path(&depthtest);
    return depthcount >= plan->min_data;
}


HIDDEN int
c_mindepth(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *new;

    new = palloc(N_MINDEPTH, f_mindepth);
    new->min_data = atoi(pattern);
    (*resultplan) = new;
    return BRLCAD_OK;
}


/*
 * -nnodes function --
 *
 * True if the object being examined is a COMB and has # nodes.  
 * If an expression ># or <# is supplied, true if object
 * has greater than or less than that number of nodes. if >=#
 * or <=# is supplied, true if object has greater than or equal
 * to / less than or equal to # of nodes.
 *
 */
HIDDEN int
f_nnodes(struct db_plan_t *plan, struct db_full_path *entry, struct db_i *dbip, struct rt_wdb *UNUSED(wdbp), struct db_full_path_list *UNUSED(results))
{
    int dogreaterthan = 0;
    int dolessthan = 0;
    int doequal = 0;
    size_t node_count_target = 0;
    size_t node_count = 0;
    struct rt_db_internal in;
    struct rt_comb_internal *comb;


    /* Check for >, < and = in the first and second
     * character positions.
     */

    if (isdigit(plan->node_data[0])) {
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
	    if (isdigit(plan->node_data[2])) {
		node_count_target = (size_t)atoi((plan->node_data)+2);
	    } else {
		return 0;
	    }
   	} else {
	    if (isdigit(plan->node_data[1])) {
		node_count_target = (size_t)atoi((plan->node_data)+1);
	    } else {
		return 0;
	    }
   	}
    }

    /* Get the number of nodes for the current object and check
     * if the value satisfied the logical conditions specified
     * in the argument string.
     */

    if (DB_FULL_PATH_CUR_DIR(entry)->d_flags & RT_DIR_COMB) {
	rt_db_get_internal5(&in, DB_FULL_PATH_CUR_DIR(entry), dbip, (fastf_t *)NULL, &rt_uniresource);
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

    if (doequal && dogreaterthan) {
	if (node_count >= node_count_target) {
	    return 1;
	} else {
	    return 0;
	}
    }

    if (doequal && dolessthan) {
	if (node_count <= node_count_target) {
	    return 1;
	} else {
	    return 0;
	}
    }

    if (dogreaterthan) {
	if (node_count > node_count_target) {
	    return 1;
	} else {
	    return 0;
	}
    }

    if (dolessthan) {
	if (node_count < node_count_target) {
	    return 1;
	} else {
	    return 0;
	}
    }


    if (doequal) {
	if (node_count == node_count_target) {
	    return 1;
	} else {
	    return 0;
	}
    }

    return 0;
}


HIDDEN int
c_nnodes(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *new;

    new = palloc(N_NNODES, f_nnodes);
    new->node_data = pattern;
    (*resultplan) = new;
    return BRLCAD_OK;
}


/*
 * -path function --
 *
 * True if the object being examined shares the pattern as
 * part of its path. To exclude results of certain directories
 * use the -not option with this option.
 */
HIDDEN int
f_path(struct db_plan_t *plan, struct db_full_path *entry, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct db_full_path_list *UNUSED(results))
{
    return !bu_fnmatch(plan->path_data, db_path_to_string(entry), 0);
}


HIDDEN int
c_path(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput))
{
    struct db_plan_t *new;

    new = palloc(N_PATH, f_path);
    new->path_data = pattern;
    (*resultplan) = new;
    return BRLCAD_OK;
}


/*
 * -print functions --
 *
 * Always true, causes the current pathame to be added to the results list.
 */
HIDDEN int
f_print(struct db_plan_t *UNUSED(plan), struct db_full_path *entry, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct db_full_path_list *results)
{
    struct db_full_path_list *new_entry;
    BU_GETSTRUCT(new_entry, db_full_path_list);
    new_entry->path = (struct db_full_path *) bu_malloc(sizeof(struct db_full_path), "new full path");
    db_full_path_init(new_entry->path);
    db_dup_full_path(new_entry->path, (const struct db_full_path *)entry);
    BU_LIST_PUSH(&(results->l), &(new_entry->l));
    return 1;
}

HIDDEN int
c_print(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *db_search_isoutput)
{
    *db_search_isoutput = 1;

    (*resultplan) = palloc(N_PRINT, f_print);
    return BRLCAD_OK;
}

/*
 * find_create --
 * create a node corresponding to a command line argument.
 *
 * TODO:
 * add create/process function pointers to node, so we can skip
 * this switch stuff.
 */
HIDDEN int
find_create(char ***argvp, struct db_plan_t **resultplan, struct db_i *UNUSED(dbip), struct rt_wdb *UNUSED(wdbp), struct db_full_path_list *UNUSED(results), int *db_search_isoutput)
{
    OPTION *p;
    struct db_plan_t *new;
    char **argv;

    argv = *argvp;

    if ((p = option(*argv)) == NULL) {
	bu_log("%s: unknown option passed to find_create\n", *argv);
	return BRLCAD_ERROR;
    }
    ++argv;
    if (p->flags & (O_ARGV|O_ARGVP) && !*argv) {
	bu_log("%s: requires additional arguments\n", *--argv);
	return BRLCAD_ERROR;
    }
    switch(p->flags) {
	case O_NONE:
	    new = NULL;
	    break;
	case O_ZERO:
	    (p->create)(NULL, NULL, 0, &new, db_search_isoutput);
	    break;
	case O_ARGV:
	    (p->create)(*argv++, NULL, 0, &new, db_search_isoutput);
	    break;
	case O_ARGVP:
	    (p->create)(NULL, &argv, p->token == N_OK, &new, db_search_isoutput);
	    break;
	default:
	    return BRLCAD_OK;
    }
    *argvp = argv;
    (*resultplan) = new;
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
    return ((OPTION *)bsearch(&tmp, options, sizeof(options)/sizeof(OPTION), sizeof(OPTION), typecompare));
}


/*
 * yanknode --
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
 * yankexpr --
 * Removes one expression from the plan.  This is used mainly by
 * paren_squish.  In comments below, an expression is either a
 * simple node or a N_EXPR node containing a list of simple nodes.
 */
HIDDEN int
yankexpr(struct db_plan_t **planp, struct db_plan_t **resultplan)          /* pointer to top of plan (modified) */
{
    struct db_plan_t *next;     /* temp node holding subexpression results */
    struct db_plan_t *node;             /* pointer to returned node or expression */
    struct db_plan_t *tail;             /* pointer to tail of subplan */
    struct db_plan_t *subplan;          /* pointer to head of () expression */
    extern int f_expr(struct db_plan_t *, struct db_full_path *, struct db_i *, struct rt_wdb *, struct db_full_path_list *);
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
	     * If we find a closing ')' we store the collected
	     * subplan in our '(' node and convert the node to
	     * a N_EXPR.  The ')' we found is ignored.  Otherwise,
	     * we just continue to add whatever we get to our
	     * subplan.
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
    if (!(error_return == BRLCAD_OK)) {
	return BRLCAD_ERROR;
    } else {
	return BRLCAD_OK;
    }
}


/*
 * paren_squish --
 * replaces "parentheisized" plans in our search plan with "expr" nodes.
 */
HIDDEN int
paren_squish(struct db_plan_t *plan, struct db_plan_t **resultplan)                /* plan with () nodes */
{
    struct db_plan_t *expr;     /* pointer to next expression */
    struct db_plan_t *tail;     /* pointer to tail of result plan */
    struct db_plan_t *result;           /* pointer to head of result plan */

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
 * not_squish --
 * compresses "!" expressions in our search plan.
 */
HIDDEN int
not_squish(struct db_plan_t *plan, struct db_plan_t **resultplan)          /* plan to process */
{
    struct db_plan_t *next;     /* next node being processed */
    struct db_plan_t *node;     /* temporary node used in N_NOT processing */
    struct db_plan_t *tail;     /* pointer to tail of result plan */
    struct db_plan_t *result;           /* pointer to head of result plan */

    tail = result = next = NULL;

    while ((next = yanknode(&plan)) != NULL) {
	/*
	 * if we encounter a (expression) then look for nots in
	 * the expr subplan.
	 */
	if (next->type == N_EXPR)
	    if (not_squish(next->p_data[0], &(next->p_data[0])) != BRLCAD_OK) return BRLCAD_ERROR;

	/*
	 * if we encounter a not, then snag the next node and place
	 * it in the not's subplan.  As an optimization we compress
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
 * above_squish --
 * compresses "-above" expressions in our search plan.
 */
HIDDEN int
above_squish(struct db_plan_t *plan, struct db_plan_t **resultplan)          /* plan to process */
{
    struct db_plan_t *next;     /* next node being processed */
    struct db_plan_t *node;     /* temporary node used in N_NOT processing */
    struct db_plan_t *tail;     /* pointer to tail of result plan */
    struct db_plan_t *result;           /* pointer to head of result plan */

    tail = result = next = NULL;

    while ((next = yanknode(&plan)) != NULL) {
	/*
	 * if we encounter a (expression) then look for aboves in
	 * the expr subplan.
	 */
	if (next->type == N_EXPR)
	    if (above_squish(next->ab_data[0], &(next->ab_data[0])) != BRLCAD_OK) return BRLCAD_ERROR;

	/*
	 * if we encounter an above, then snag the next node and place
	 * it in the not's subplan.
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
 * below_squish --
 * compresses "-below" expressions in our search plan.
 */
HIDDEN int
below_squish(struct db_plan_t *plan, struct db_plan_t **resultplan)          /* plan to process */
{
    struct db_plan_t *next;     /* next node being processed */
    struct db_plan_t *node;     /* temporary node used in N_NOT processing */
    struct db_plan_t *tail;     /* pointer to tail of result plan */
    struct db_plan_t *result;           /* pointer to head of result plan */

    tail = result = next = NULL;

    while ((next = yanknode(&plan)) != NULL) {
	/*
	 * if we encounter a (expression) then look for nots in
	 * the expr subplan.
	 */
	if (next->type == N_EXPR)
	    if (below_squish(next->bl_data[0], &(next->bl_data[0])) != BRLCAD_OK) return BRLCAD_ERROR;

	/*
	 * if we encounter a not, then snag the next node and place
	 * it in the not's subplan.
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
 * or_squish --
 * compresses -o expressions in our search plan.
 */
HIDDEN int
or_squish(struct db_plan_t *plan, struct db_plan_t **resultplan)           /* plan with ors to be squished */
{
    struct db_plan_t *next;     /* next node being processed */
    struct db_plan_t *tail;     /* pointer to tail of result plan */
    struct db_plan_t *result;           /* pointer to head of result plan */

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


void *
db_search_formplan(char **argv, struct db_i *dbip, struct rt_wdb *wdbp) {
    struct db_plan_t *plan, *tail;
    struct db_plan_t *new = NULL;
    struct db_full_path_list *results = NULL;
    int db_search_isoutput = 0;

    /*
     * for each argument in the command line, determine what kind of node
     * it is, create the appropriate node type and add the new plan node
     * to the end of the existing plan.  The resulting plan is a linked
     * list of plan nodes.  For example, the string:
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
	if (find_create(&argv, &new, dbip, wdbp, results, &db_search_isoutput) != BRLCAD_OK) return NULL;
	if (!(new))
	    continue;
	if (plan == NULL)
	    tail = plan = new;
	else {
	    tail->next = new;
	    tail = new;
	}
    }

    /*
     * if the user didn't specify one of -print, -ok or -exec, then -print
     * is assumed so we bracket the current expression with parens, if
     * necessary, and add a -print node on the end.
     */
    if (!db_search_isoutput) {
	if (plan == NULL) {
	    c_print(NULL, NULL, 0, &new, &db_search_isoutput);
	    tail = plan = new;
	} else {
	    c_openparen(NULL, NULL, 0, &new, &db_search_isoutput);
	    new->next = plan;
	    plan = new;
	    c_closeparen(NULL, NULL, 0, &new, &db_search_isoutput);
	    tail->next = new;
	    tail = new;
	    c_print(NULL, NULL, 0, &new, &db_search_isoutput);
	    tail->next = new;
	    tail = new;
	}
    }

    /*
     * the command line has been completely processed into a search plan
     * except for the (,), !, and -o operators.  Rearrange the plan so
     * that the portions of the plan which are affected by the operators
     * are moved into operator nodes themselves.  For example:
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
    if (above_squish(plan, &plan) != BRLCAD_OK) return NULL;                /* above's */
    if (below_squish(plan, &plan) != BRLCAD_OK) return NULL;                /* below's */
    if (not_squish(plan, &plan) != BRLCAD_OK) return NULL;                /* !'s */
    if (or_squish(plan, &plan) != BRLCAD_OK) return NULL;                 /* -o's */
    return (void *)plan;
}


HIDDEN void
find_execute_plans(struct db_i *dbip, struct rt_wdb *wdbp, struct db_full_path_list *results, struct db_full_path *dfp, genptr_t inputplan) {
    struct db_plan_t *p;
    struct db_plan_t *plan = (struct db_plan_t *)inputplan;
    for (p = plan; p && (p->eval)(p, dfp, dbip, wdbp, results); p = p->next)
	;

}


void
db_search_freeplan(void **vplan) {
    struct db_plan_t *p;
    struct db_plan_t *plan = (struct db_plan_t *)*vplan;
    for (p = plan; p;) {
	plan = p->next;
	bu_free(p, "Deallocate struct db_plan_t structure");
	p = plan;
    }
    /* sanity */
    *vplan = NULL;
}


struct db_full_path_list *
db_search_full_paths(void *searchplan,        /* search plan */
	     struct db_full_path_list *pathnames,      /* list of pathnames to traverse */
	     struct db_i *dbip,
	     struct rt_wdb *wdbp)
{
    int i;
    struct directory *dp;
    struct db_full_path dfp;
    struct db_full_path_list *new_entry = NULL;
    struct db_full_path_list *currentpath = NULL;
    struct db_full_path_list *searchresults = NULL;
    BU_GETSTRUCT(searchresults, db_full_path_list);
    BU_LIST_INIT(&(searchresults->l));
    /* If nothing is passed in, try to get the list of toplevel objects */
    if (BU_LIST_IS_EMPTY(&(pathnames->l))) {
	    db_full_path_init(&dfp);
	    for (i = 0; i < RT_DBNHASH; i++) {
		    for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
			    if (dp->d_nref == 0 && !(dp->d_flags & RT_DIR_HIDDEN) && (dp->d_addr != RT_DIR_PHONY_ADDR)) {
				    db_string_to_path(&dfp, dbip, dp->d_namep);
				    BU_GETSTRUCT(new_entry, db_full_path_list);
				    new_entry->path = (struct db_full_path *) bu_malloc(sizeof(struct db_full_path), "new full path");
				    db_full_path_init(new_entry->path);
				    db_dup_full_path(new_entry->path, (const struct db_full_path *)&dfp);
				    BU_LIST_PUSH(&(pathnames->l), &(new_entry->l));
			    }
		    }
	    }
	    db_free_full_path(&dfp);
    }
    for(BU_LIST_FOR(currentpath, db_full_path_list, &(pathnames->l))){
	    db_fullpath_traverse(dbip, wdbp, searchresults, currentpath->path, find_execute_plans, find_execute_plans, wdbp->wdb_resp, (struct db_plan_t *)searchplan);
    }
    return searchresults;
}


struct bu_ptbl *
db_search_unique_objects(void *searchplan,        /* search plan */
	     struct db_full_path_list *pathnames,      /* list of pathnames to traverse */
	     struct db_i *dbip,
	     struct rt_wdb *wdbp)
{
	struct bu_ptbl *uniq_db_objs = (struct bu_ptbl *) bu_malloc(sizeof(struct bu_ptbl), "new pointer table");
	struct db_full_path_list *entry = NULL;
	struct db_full_path_list *search_results = NULL;
	search_results = db_search_full_paths(searchplan, pathnames, dbip, wdbp);
	bu_ptbl_init(uniq_db_objs, 8, "initialize ptr table");
	for(BU_LIST_FOR(entry, db_full_path_list, &(search_results->l))) {
		bu_ptbl_ins_unique(uniq_db_objs, (long *)entry->path->fp_names[entry->path->fp_len - 1]);
	}
	db_free_full_path_list(search_results);
	return uniq_db_objs;
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

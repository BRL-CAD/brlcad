/*                      S E A R C H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "common.h"

#include <cstddef>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <vector>
#include <deque>

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <limits.h> /* for INT_MAX */

#ifdef __restrict
#  undef __restrict
#endif
#define __restrict /* quell gcc 4.1.2 system regex.h -pedantic-errors */
#include <sys/types.h> /* for mingw regex.h->stdio.h types */
#include <regex.h>
#include "vmath.h"

#include "bu/cmd.h"
#include "bu/hash.h"
#include "bu/opt.h"
#include "bu/path.h"

#include "rt/db4.h"
#include "./librt_private.h"
#include "./search.h"


/* Forward declarations */
static int c_attr(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_objparam(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_iname(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_maxdepth(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_mindepth(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_depth(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_name(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_nnodes(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_regex(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_iregex(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_path(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_print(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_stdattr(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_matrix(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_type(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_bool(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_openparen(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_closeparen(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_not(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_or(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_above(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_below(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_exec(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static int c_size(char *, char ***, int, struct db_plan_t **, int *, struct bu_ptbl *, struct _db_search_ctx *);
static void find_execute_plans(struct db_i *dbip, struct bu_ptbl *results, struct db_node_t *db_node, struct db_plan_t *plan);
static int find_execute_nested_plans(struct db_i *dbip, struct bu_ptbl *results, struct db_node_t *db_node, struct db_plan_t *plan);


/*
 * NB: these rows must remain sorted lexically.  The parser and the public
 * completion/highlighting vocabulary are deliberately expanded from this one
 * list so an interactive client cannot silently drift from db_search.
 */
#define DB_SEARCH_PLAN_TERMS(X) \
    X("!",          N_NOT,          c_not,          O_ZERO, DB_SEARCH_SYNTAX_NO_ARGUMENT) \
    X("(",          N_OPENPAREN,    c_openparen,    O_ZERO, DB_SEARCH_SYNTAX_NO_ARGUMENT) \
    X(")",          N_CLOSEPAREN,   c_closeparen,   O_ZERO, DB_SEARCH_SYNTAX_NO_ARGUMENT) \
    X("-a",         N_AND,          NULL,           O_NONE, DB_SEARCH_SYNTAX_NO_ARGUMENT) \
    X("-ab",        N_ABOVE,        c_above,        O_ZERO, DB_SEARCH_SYNTAX_NO_ARGUMENT) \
    X("-above",     N_ABOVE,        c_above,        O_ZERO, DB_SEARCH_SYNTAX_NO_ARGUMENT) \
    X("-and",       N_AND,          NULL,           O_NONE, DB_SEARCH_SYNTAX_NO_ARGUMENT) \
    X("-attr",      N_ATTR,         c_attr,         O_ARGV, DB_SEARCH_SYNTAX_STRING_ARGUMENT) \
    X("-below",     N_BELOW,        c_below,        O_ZERO, DB_SEARCH_SYNTAX_NO_ARGUMENT) \
    X("-bl",        N_BELOW,        c_below,        O_ZERO, DB_SEARCH_SYNTAX_NO_ARGUMENT) \
    X("-bool",      N_BOOL,         c_bool,         O_ARGV, DB_SEARCH_SYNTAX_STRING_ARGUMENT) \
    X("-depth",     N_DEPTH,        c_depth,        O_ARGV, DB_SEARCH_SYNTAX_INTEGER_ARGUMENT) \
    X("-exec",      N_EXEC,         c_exec,         O_ARGVP, DB_SEARCH_SYNTAX_EXEC_ARGUMENTS) \
    X("-iname",     N_INAME,        c_iname,        O_ARGV, DB_SEARCH_SYNTAX_STRING_ARGUMENT) \
    X("-iregex",    N_IREGEX,       c_iregex,       O_ARGV, DB_SEARCH_SYNTAX_STRING_ARGUMENT) \
    X("-matrix",    N_MATRIX,       c_matrix,       O_ARGV, DB_SEARCH_SYNTAX_STRING_ARGUMENT) \
    X("-maxdepth",  N_MAXDEPTH,     c_maxdepth,     O_ARGV, DB_SEARCH_SYNTAX_INTEGER_ARGUMENT) \
    X("-mindepth",  N_MINDEPTH,     c_mindepth,     O_ARGV, DB_SEARCH_SYNTAX_INTEGER_ARGUMENT) \
    X("-name",      N_NAME,         c_name,         O_ARGV, DB_SEARCH_SYNTAX_STRING_ARGUMENT) \
    X("-nnodes",    N_NNODES,       c_nnodes,       O_ARGV, DB_SEARCH_SYNTAX_INTEGER_ARGUMENT) \
    X("-not",       N_NOT,          c_not,          O_ZERO, DB_SEARCH_SYNTAX_NO_ARGUMENT) \
    X("-o",         N_OR,           c_or,           O_ZERO, DB_SEARCH_SYNTAX_NO_ARGUMENT) \
    X("-or",        N_OR,           c_or,           O_ZERO, DB_SEARCH_SYNTAX_NO_ARGUMENT) \
    X("-param",     N_PARAM,        c_objparam,     O_ARGV, DB_SEARCH_SYNTAX_STRING_ARGUMENT) \
    X("-path",      N_PATH,         c_path,         O_ARGV, DB_SEARCH_SYNTAX_STRING_ARGUMENT) \
    X("-print",     N_PRINT,        c_print,        O_ZERO, DB_SEARCH_SYNTAX_NO_ARGUMENT) \
    X("-regex",     N_REGEX,        c_regex,        O_ARGV, DB_SEARCH_SYNTAX_STRING_ARGUMENT) \
    X("-size",      N_SIZE,         c_size,         O_ARGV, DB_SEARCH_SYNTAX_STRING_ARGUMENT) \
    X("-stdattr",   N_STDATTR,      c_stdattr,      O_ZERO, DB_SEARCH_SYNTAX_NO_ARGUMENT) \
    X("-type",      N_TYPE,         c_type,         O_ARGV, DB_SEARCH_SYNTAX_TYPE_ARGUMENT)

#define DB_SEARCH_OPTION_ROW(name, node, compile, option_kind, argument) {name, node, compile, option_kind},
static OPTION options[] = {
    DB_SEARCH_PLAN_TERMS(DB_SEARCH_OPTION_ROW)
};
#undef DB_SEARCH_OPTION_ROW

#define DB_SEARCH_SYNTAX_ROW(name, node, compile, option_kind, argument) {name, argument},
static const struct db_search_syntax_term search_syntax_terms[] = {
    DB_SEARCH_PLAN_TERMS(DB_SEARCH_SYNTAX_ROW)
};
#undef DB_SEARCH_SYNTAX_ROW

extern "C" const struct db_search_syntax_term *
db_search_syntax_terms(size_t *count)
{
    if (count)
	*count = sizeof(search_syntax_terms) / sizeof(search_syntax_terms[0]);
    return search_syntax_terms;
}

extern "C" int
db_search_syntax_plan_start(const char *word)
{
    if (!word)
	return 0;
    return (word[0] == '-' || word[0] == '!' || word[0] == '(');
}

extern "C" int
db_search_syntax_exec_substitution(const char *word)
{
    return (word && strstr(word, "{}")) ? 1 : 0;
}

#undef DB_SEARCH_PLAN_TERMS


/* Search client data container */
struct list_client_data_t {
    struct db_i *dbip;
    struct bu_ptbl *full_paths;
    int flags;
};


/* Traversal plan analysis */
struct plan_analysis_t {
    int has_above;
    int has_below;
    int has_maxdepth;
    int maxdepth;
};


struct traversal_ctx_t {
    struct db_i *dbip;
    struct db_plan_t *plan;
    struct bu_ptbl *results;
    std::vector<struct db_plan_t *> *below_nodes;
    void *unique_dps;
    int flags;
    int result_cnt;
    int has_maxdepth;
    int maxdepth;
};


struct path_node_t {
    struct db_full_path *path;
    uint64_t below_passes;
    int depth;
    int cyclic;
};

struct below_path_state_t {
    uint64_t passes;
    int active;
};

typedef std::unordered_map<struct db_full_path *, struct below_path_state_t> BelowStateMap;


struct leaf_info_t {
    const char *name;
    int bool_op;
};


/*
 * leaf_info_t except we must copy the name (rather than a reference) so it
 * stays valid after the comb internal is release and across any database
 * mutations. See traverse_paths() for why this is important.
 */
struct captured_child_t {
    std::string name;
    int bool_op;
    int c_inst;
};


/**
 * Identity equality for bu_h128_t.
 *
 * Compares all 128 bits.  This is the correctness gate used by
 * std::unordered_map / std::unordered_set after a bucket lookup: two keys
 * are the same if and only if both 64-bit words match.  The collision
 * probability at this level is N^2 / 2^129 (see file-level comment).
 */
inline bool operator==(const bu_h128_t &a, const bu_h128_t &b)
{
    return a.w[0] == b.w[0] && a.w[1] == b.w[1];
}

/**
 * Bucket-placement hasher for bu_h128_t (performance only, not identity).
 *
 * This function maps a 128-bit fingerprint to a single std::size_t bucket
 * index.  It is used exclusively for bucket selection; the container
 * resolves actual equality with operator== (above), which inspects all
 * 128 bits.
 *
 * Folding to std::size_t increases *bucket* collision probability back to
 * roughly N^2 / 2^65 on 64-bit platforms – that is intentional and
 * harmless: a bucket collision only lengthens one chain by one node, it
 * never causes a false identity match.  See the file-level comment for
 * a full explanation of the two-level collision model.
 *
 * Implementation: XOR the two 64-bit halves (both have XXH3's excellent
 * avalanche properties), then fold to size_t width for 32-bit platforms.
 */
namespace std {
   template<>
    struct hash<bu_h128_t> {
        size_t operator()(const bu_h128_t &h) const noexcept {
            /* XOR the two 64-bit halves, then fold to size_t width.
             * On 64-bit platforms this is a no-op truncation.
             * On 32-bit platforms the extra shift folds the upper 32
             * bits of the XOR result into the lower 32 before the cast. */
            uint64_t v = h.w[0] ^ h.w[1];
            if (sizeof(size_t) < sizeof(uint64_t))
                v ^= (v >> 32);
            return (size_t)v;
        }
    };
} /* namespace std */


/*
 * Compute the 128-bit path fingerprint for a node reached by appending
 * child_dp to the path whose fingerprint is parent_hash.
 *
 * bu_data_hash128 (XXH3-128) combines the two inputs into a fresh 128-bit
 * fingerprint.  Two different paths produce the same fingerprint only with
 * probability N^2 / 2^129 (birthday bound), where N is the number of distinct
 * paths evaluated.  A fingerprint collision would cause a false-positive
 * cached -above result (a correctness bug), so this collision probability
 * governs the correctness guarantee of the reverse-propagation cache.
 *
 * Note: the std::hash bucket function used by the unordered_set folds this
 * fingerprint to 64 bits for placement.  That increases *bucket* collision
 * probability to ~N^2 / 2^65, but bucket collisions only degrade performance
 * (an extra operator== comparison) – identity is always confirmed by checking
 * all 128 bits.
 *
 * parent_hash == BELOW_PATH_HASH_ROOT is the sentinel for a root-level node.
 */
static inline bu_h128_t
below_path_hash_extend(bu_h128_t parent_hash, struct directory *child_dp,
		       int bool_op, int comb_inst)
{
    uint64_t s[5];
    s[0] = parent_hash.w[0];
    s[1] = parent_hash.w[1];
    s[2] = (uint64_t)(uintptr_t)child_dp;
    s[3] = (uint64_t)(uint32_t)bool_op;
    s[4] = (uint64_t)(uint32_t)comb_inst;
    return bu_data_hash128(s, sizeof(s));
}

/* Zero/sentinel value for 128-bit path hashes (root nodes have no parent). */
static const bu_h128_t BELOW_PATH_HASH_ROOT = {{0, 0}};


static void
plan_analysis_init(struct plan_analysis_t *pa)
{
    if (!pa)
	return;
    pa->has_above = 0;
    pa->has_below = 0;
    pa->has_maxdepth = 0;
    pa->maxdepth = INT_MAX;
}


static void
plan_analysis_update(const struct db_plan_t *plan, struct plan_analysis_t *pa)
{
    const struct db_plan_t *p = NULL;

    if (!plan || !pa)
	return;

    for (p = plan; p; p = p->next) {
	switch (p->type) {
	    case N_ABOVE:
		pa->has_above = 1;
		if (p->p_un._ab_data[0])
		    plan_analysis_update(p->p_un._ab_data[0], pa);
		break;
	    case N_BELOW:
		pa->has_below = 1;
		if (p->p_un._bl_data[0])
		    plan_analysis_update(p->p_un._bl_data[0], pa);
		break;
	    case N_MAXDEPTH:
		/* Traversal pruning is computed separately.  Merely finding a
		 * -maxdepth is not sufficient: it may be under -or, !, or
		 * -below and therefore impose no bound on matching paths. */
		break;
	    case N_EXPR:
	    case N_NOT:
		if (p->p_un._p_data[0])
		    plan_analysis_update(p->p_un._p_data[0], pa);
		break;
	    case N_OR:
		if (p->p_un._p_data[0])
		    plan_analysis_update(p->p_un._p_data[0], pa);
		if (p->p_un._p_data[1])
		    plan_analysis_update(p->p_un._p_data[1], pa);
		break;
	    default:
		break;
	}
    }
}


/*
 * Cached -below evaluation deliberately excludes inner plans with observable
 * actions.  Propagation evaluates an inner expression once per parent,
 * whereas the ancestor walker may evaluate it once per descendant.  Reusing a
 * predicate result is correct, but suppressing -exec callbacks or -print
 * result insertions is not.
 */
static int
plan_has_side_effects(const struct db_plan_t *plan)
{
    const struct db_plan_t *p = NULL;

    for (p = plan; p; p = p->next) {
	switch (p->type) {
	    case N_EXEC:
	    case N_PRINT:
		return 1;
	    case N_ABOVE:
		if (p->p_un._ab_data[0] && plan_has_side_effects(p->p_un._ab_data[0]))
		    return 1;
		break;
	    case N_BELOW:
		if (p->p_un._bl_data[0] && plan_has_side_effects(p->p_un._bl_data[0]))
		    return 1;
		break;
	    case N_EXPR:
	    case N_NOT:
		if (p->p_un._p_data[0] && plan_has_side_effects(p->p_un._p_data[0]))
		    return 1;
		break;
	    case N_OR:
		if (p->p_un._p_data[0] && plan_has_side_effects(p->p_un._p_data[0]))
		    return 1;
		if (p->p_un._p_data[1] && plan_has_side_effects(p->p_un._p_data[1]))
		    return 1;
		break;
	    default:
		break;
	}
    }

    return 0;
}


/*
 * Return true when evaluation of a plan may invoke a caller supplied
 * callback.  Unlike -print, -exec may change the database and invalidate any
 * predicate values computed before normal left-to-right plan evaluation.
 */
static int
plan_has_exec(const struct db_plan_t *plan)
{
    const struct db_plan_t *p = NULL;

    for (p = plan; p; p = p->next) {
	switch (p->type) {
	    case N_EXEC:
		return 1;
	    case N_ABOVE:
		if (p->p_un._ab_data[0] && plan_has_exec(p->p_un._ab_data[0]))
		    return 1;
		break;
	    case N_BELOW:
		if (p->p_un._bl_data[0] && plan_has_exec(p->p_un._bl_data[0]))
		    return 1;
		break;
	    case N_EXPR:
	    case N_NOT:
		if (p->p_un._p_data[0] && plan_has_exec(p->p_un._p_data[0]))
		    return 1;
		break;
	    case N_OR:
		if (p->p_un._p_data[0] && plan_has_exec(p->p_un._p_data[0]))
		    return 1;
		if (p->p_un._p_data[1] && plan_has_exec(p->p_un._p_data[1]))
		    return 1;
		break;
	    default:
		break;
	}
    }

    return 0;
}


/* Return the maximum depth guaranteed by one predicate node, if any. */
static int
plan_node_maxdepth(const struct db_plan_t *p, int *maxdepth);


/*
 * A plan list is an implicit AND.  Any bounded conjunct bounds the complete
 * list.  An OR is bounded only when both alternatives are bounded; its bound
 * is the less restrictive (larger) of the two.
 */
static int
plan_guaranteed_maxdepth(const struct db_plan_t *plan, int *maxdepth)
{
    const struct db_plan_t *p = NULL;
    int found = 0;
    int bound = INT_MAX;

    for (p = plan; p; p = p->next) {
	int node_bound = INT_MAX;
	if (plan_node_maxdepth(p, &node_bound)) {
	    if (!found || node_bound < bound)
		bound = node_bound;
	    found = 1;
	}
    }

    if (found && maxdepth)
	*maxdepth = bound;
    return found;
}


static int
plan_node_maxdepth(const struct db_plan_t *p, int *maxdepth)
{
    int left_bound = INT_MAX;
    int right_bound = INT_MAX;

    if (!p)
	return 0;

    switch (p->type) {
	case N_MAXDEPTH:
	    if (maxdepth)
		*maxdepth = p->p_un._max_data;
	    return 1;
	case N_EXPR:
	    return plan_guaranteed_maxdepth(p->p_un._p_data[0], maxdepth);
	case N_OR:
	    if (!plan_guaranteed_maxdepth(p->p_un._p_data[0], &left_bound) ||
		!plan_guaranteed_maxdepth(p->p_un._p_data[1], &right_bound))
		return 0;
	    if (maxdepth)
		*maxdepth = (left_bound > right_bound) ? left_bound : right_bound;
	    return 1;
	/* A bound inside these operators does not bound the path currently
	 * being evaluated.  NOT may invert it, ABOVE applies it to descendants,
	 * and BELOW applies it to ancestors. */
	case N_NOT:
	case N_ABOVE:
	case N_BELOW:
	default:
	    return 0;
    }
}


/* Return true if evaluating this node (excluding p->next) may have actions. */
static int
plan_node_has_side_effects(const struct db_plan_t *p)
{
    if (!p)
	return 0;

    switch (p->type) {
	case N_EXEC:
	case N_PRINT:
	    return 1;
	case N_ABOVE:
	    return plan_has_side_effects(p->p_un._ab_data[0]);
	case N_BELOW:
	    return plan_has_side_effects(p->p_un._bl_data[0]);
	case N_EXPR:
	case N_NOT:
	    return plan_has_side_effects(p->p_un._p_data[0]);
	case N_OR:
	    return plan_has_side_effects(p->p_un._p_data[0]) ||
		plan_has_side_effects(p->p_un._p_data[1]);
	default:
	    return 0;
    }
}


/*
 * Find a traversal bound that is both logically mandatory and evaluated
 * before any observable action.  Stopping at the first action preserves
 * normal left-to-right -print/-exec semantics while retaining the common
 * implicit "( filters ) -print" maxdepth optimization.
 */
static int
plan_prune_maxdepth(const struct db_plan_t *plan, int *maxdepth)
{
    const struct db_plan_t *p = NULL;
    int found = 0;
    int bound = INT_MAX;

    for (p = plan; p; p = p->next) {
	int node_bound = INT_MAX;
	if (plan_node_has_side_effects(p))
	    break;
	if (plan_node_maxdepth(p, &node_bound)) {
	    if (!found || node_bound < bound)
		bound = node_bound;
	    found = 1;
	}
    }

    if (found && maxdepth)
	*maxdepth = bound;
    return found;
}


/*
 * Assign propagation bits to unconstrained -below nodes.  A uint64_t keeps
 * the state in each traversal work item allocation-free; exceptionally large
 * plans simply use the correct ancestor-walk fallback for nodes beyond bit 63.
 */
static void
collect_below_nodes(struct db_plan_t *plan, std::vector<struct db_plan_t *> &below_nodes)
{
    for (struct db_plan_t *p = plan; p; p = p->next) {
	switch (p->type) {
	    case N_BELOW:
		if (p->min_depth == 0 && p->max_depth == INT_MAX &&
		    !plan_has_side_effects(p->p_un._bl_data[0]) &&
		    below_nodes.size() < 8 * sizeof(uint64_t)) {
		    p->below_cache_index = (int)below_nodes.size();
		    below_nodes.push_back(p);
		}
		/* A nested cache is only usable when its containing -below is
		 * itself evaluated through the propagated-state path. */
		if (p->below_cache_index >= 0 && p->p_un._bl_data[0])
		    collect_below_nodes(p->p_un._bl_data[0], below_nodes);
		break;
	    case N_ABOVE:
		if (p->p_un._ab_data[0])
		    collect_below_nodes(p->p_un._ab_data[0], below_nodes);
		break;
	    case N_EXPR:
	    case N_NOT:
		if (p->p_un._p_data[0])
		    collect_below_nodes(p->p_un._p_data[0], below_nodes);
		break;
	    case N_OR:
		if (p->p_un._p_data[0])
		    collect_below_nodes(p->p_un._p_data[0], below_nodes);
		if (p->p_un._p_data[1])
		    collect_below_nodes(p->p_un._p_data[1], below_nodes);
		break;
	    default:
		break;
	}
    }
}


static void
collect_tree_leaves(union tree *tp, int curr_bool, std::vector<leaf_info_t> &leaves)
{
    int bool_val = curr_bool;
    if (!tp)
	return;

    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    collect_tree_leaves(tp->tr_b.tb_left, OP_UNION, leaves);
	    break;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    collect_tree_leaves(tp->tr_b.tb_left, OP_UNION, leaves);
	    if (tp->tr_op == OP_UNION)
		bool_val = 2;
	    if (tp->tr_op == OP_INTERSECT)
		bool_val = 3;
	    if (tp->tr_op == OP_SUBTRACT)
		bool_val = 4;
	    collect_tree_leaves(tp->tr_b.tb_right, bool_val, leaves);
	    break;
	case OP_DB_LEAF: {
	    leaf_info_t info;
	    info.name = tp->tr_l.tl_name;
	    info.bool_op = curr_bool;
	    leaves.push_back(info);
	    break;
	}
	default:
	    bu_log("collect_tree_leaves: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("collect_tree_leaves: unrecognized operator\n");
    }
}


/*
 * Return the cached -below state inherited by every child of parent_path.
 *
 * A child is below expression E when its parent either already is below E or
 * itself satisfies E.  Computing that recurrence once while walking the tree
 * changes unconstrained -below from O(N*D) ancestor scans to O(N*B), where B
 * is the number of -below nodes in the search plan (normally one).
 */
static uint64_t
below_child_passes(struct db_i *dbip,
		   const std::vector<struct db_plan_t *> *below_nodes,
		   struct db_full_path *parent_path,
		   uint64_t parent_passes,
		   int flags,
		   struct bu_ptbl *full_paths)
{
    struct db_node_t parent_node;
    uint64_t child_passes = parent_passes;

    if (!below_nodes || below_nodes->empty() || !parent_path)
	return child_passes;

    parent_node.path = parent_path;
    parent_node.full_paths = full_paths;
    parent_node.unique_dps = NULL;
    parent_node.above_passes_map = NULL;
    parent_node.above_path_hash = BELOW_PATH_HASH_ROOT;
    parent_node.below_passes = parent_passes;
    parent_node.below_cache_active = 1;
    parent_node.flags = flags;

    for (struct db_plan_t *bp : *below_nodes) {
	uint64_t bit = UINT64_C(1) << bp->below_cache_index;

	if (child_passes & bit)
	    continue;

	parent_node.matched_filters = 1;
	if (find_execute_nested_plans(dbip, NULL, &parent_node, bp->p_un._bl_data[0]))
	    child_passes |= bit;
    }

    return child_passes;
}


/*
 * Full-path collection is required when a plan contains -above.  Preserve the
 * same top-down -below recurrence in that route by following the depth-first,
 * parent-before-child order produced by db_fullpath_list().  If an unexpected
 * ordering is encountered, mark that path inactive so f_below uses its
 * correct ancestor-walk fallback.
 */
static void
build_below_state_map(struct db_i *dbip,
		      const std::vector<struct db_plan_t *> *below_nodes,
		      struct bu_ptbl *full_paths,
		      int flags,
		      BelowStateMap &state_map)
{
    std::vector<struct db_full_path *> path_stack;
    std::vector<struct below_path_state_t> state_stack;

    if (!below_nodes || below_nodes->empty() || !full_paths)
	return;

    state_map.reserve((size_t)BU_PTBL_LEN(full_paths));

    for (size_t i = 0; i < (size_t)BU_PTBL_LEN(full_paths); i++) {
	struct db_full_path *fp =
	    (struct db_full_path *)BU_PTBL_GET(full_paths, (int)i);
	struct below_path_state_t state = {0, 0};
	size_t depth;

	if (!fp || fp->fp_len == 0)
	    continue;

	depth = fp->fp_len;
	if (path_stack.size() >= depth) {
	    path_stack.resize(depth - 1);
	    state_stack.resize(depth - 1);
	}

	if (depth == 1) {
	    state.active = 1;
	} else if (path_stack.size() == depth - 1 &&
		   db_full_path_match_top(path_stack.back(), fp) &&
		   state_stack.back().active) {
	    state.active = 1;
	    state.passes = below_child_passes(dbip, below_nodes,
		    path_stack.back(), state_stack.back().passes, flags, full_paths);
	}

	state_map.emplace(fp, state);
	path_stack.push_back(fp);
	state_stack.push_back(state);
    }
}


static void
evaluate_path(struct traversal_ctx_t *ctx, struct db_full_path *path, uint64_t below_passes)
{
    struct db_node_t curr_node;
    if (!ctx || !path)
	return;

    curr_node.path = path;
    curr_node.flags = ctx->flags;
    curr_node.matched_filters = 1;
    curr_node.full_paths = NULL;
    curr_node.unique_dps = ctx->unique_dps;
    curr_node.above_passes_map = NULL;
    curr_node.above_path_hash = BELOW_PATH_HASH_ROOT;
    curr_node.below_passes = below_passes;
    curr_node.below_cache_active = (ctx->below_nodes && !ctx->below_nodes->empty());

    find_execute_plans(ctx->dbip, ctx->results, &curr_node, ctx->plan);
    ctx->result_cnt += curr_node.matched_filters;
}


static void
traverse_paths(struct traversal_ctx_t *ctx, struct directory **paths, int path_cnt)
{
    int i = 0;
    std::deque<path_node_t> work;

    if (!ctx || !paths || path_cnt <= 0)
	return;

    for (i = 0; i < path_cnt; i++) {
	/* Tree traversal uses a LIFO work list.  Push roots in reverse so
	 * callback and traversal order follows the caller's input order. */
	int path_i = (ctx->flags & DB_SEARCH_FLAT) ? i : path_cnt - i - 1;
	struct directory *curr_dp = paths[path_i];
	struct db_full_path *start_path = NULL;
	struct path_node_t node;

	if (curr_dp == RT_DIR_NULL)
	    continue;

	if ((ctx->flags & DB_SEARCH_HIDDEN) || !(curr_dp->d_flags & RT_DIR_HIDDEN)) {
	    BU_ALLOC(start_path, struct db_full_path);
	    db_full_path_init(start_path);
	    db_add_node_to_full_path(start_path, curr_dp);
	    DB_FULL_PATH_SET_CUR_BOOL(start_path, 2);

	    if (ctx->flags & DB_SEARCH_FLAT) {
		evaluate_path(ctx, start_path, 0);
		db_free_full_path(start_path);
		BU_PUT(start_path, struct db_full_path);
	    } else {
		node.path = start_path;
		node.below_passes = 0;
		node.depth = 0;
		node.cyclic = 0;
		work.push_back(node);
	    }
	}
    }

    while (!work.empty()) {
	struct path_node_t node;
	struct db_full_path *path = NULL;
	struct directory *dp = NULL;

	node = work.back();
	work.pop_back();

	path = node.path;
	if (!path) {
	    continue;
	}

	/*
	 * Snapshot this comb's child list BEFORE running the node's action.
	 *
	 * evaluate_path() may invoke a -exec callback that mutates the
	 * database - for example a Tcl proc that "wraps" the matched comb,
	 * inserting a brand new child into the very comb we are about to
	 * descend into. Reading the child list after the action would follow
	 * those freshly-created objects; recursing without bound and never 
	 * terminating. Capturing the children first gives GNU find semantics:
	 * the action cannot change the set of objects this search traverses.
	 *
	 * Names are resolved to directory pointers after the action (below),
	 * so anything the action deletes is skipped rather than left dangling.
	 */
	std::vector<captured_child_t> children;
	uint64_t child_below_passes = node.below_passes;
	int expand = 0;

	dp = DB_FULL_PATH_CUR_DIR(path);
	if (!node.cyclic &&
	    !(ctx->has_maxdepth && node.depth >= ctx->maxdepth) &&
	    dp && (dp->d_flags & RT_DIR_COMB)) {

	    struct rt_db_internal intern;
	    struct rt_comb_internal *comb = NULL;

	    RT_DB_INTERNAL_INIT(&intern);
	    if (rt_db_get_internal(&intern, dp, ctx->dbip, (fastf_t *)NULL) >= 0) {
		comb = (struct rt_comb_internal *)intern.idb_ptr;
		if (comb && comb->tree) {
		    std::vector<leaf_info_t> leaves;
		    std::unordered_map<std::string, int> c_inst_map;

		    child_below_passes = below_child_passes(ctx->dbip,
			    ctx->below_nodes, path, node.below_passes, ctx->flags, NULL);

		    collect_tree_leaves(comb->tree, OP_UNION, leaves);
		    children.reserve(leaves.size());
		    for (size_t li = 0; li < leaves.size(); li++) {
			captured_child_t cc;
			cc.name = leaves[li].name;
			cc.bool_op = leaves[li].bool_op;
			cc.c_inst = 0;
			if (UNLIKELY(ctx->dbip->i->dbi_use_comb_instance_ids))
			    cc.c_inst = c_inst_map[cc.name]++;
			children.push_back(cc);
		    }
		    expand = 1;
		}
		rt_db_free_internal(&intern);
	    }
	}

	/* Evaluate the node.  A -exec action, if any, runs here and may alter
	 * the database - but never the pre-captured child list above. */
	evaluate_path(ctx, path, node.below_passes);

	/* Push the snapshotted children.  Resolve each name now so that any
	 * object removed by the action is simply skipped. */
	if (expand) {
	    /* Push in reverse tree order so the LIFO work list evaluates the
	     * leftmost child first, matching the recursive walker. */
	    for (size_t li = children.size(); li-- > 0;) {
		struct directory *child_dp = NULL;
		struct db_full_path *child_path = NULL;
		int cyclic = 0;

		child_dp = db_lookup(ctx->dbip, children[li].name.c_str(), LOOKUP_QUIET);
		if (!child_dp)
		    continue;

		if (!(ctx->flags & DB_SEARCH_HIDDEN) && (child_dp->d_flags & RT_DIR_HIDDEN))
		    continue;

		BU_ALLOC(child_path, struct db_full_path);
		db_full_path_init(child_path);
		db_dup_full_path(child_path, path);
		db_add_node_to_full_path(child_path, child_dp);
		DB_FULL_PATH_SET_CUR_BOOL(child_path, children[li].bool_op);

		if (UNLIKELY(ctx->dbip->i->dbi_use_comb_instance_ids)) {
		    DB_FULL_PATH_SET_CUR_COMB_INST(child_path, children[li].c_inst);
		}

		if (db_full_path_cyclic(child_path, NULL, 0)) {
		    char *path_string = db_path_to_string(child_path);
		    bu_log("WARNING: not traversing cyclic path %s\n", path_string);
		    bu_free(path_string, "free path str");
		    cyclic = 1;
		}

		path_node_t child_node;
		child_node.path = child_path;
		child_node.below_passes = child_below_passes;
		child_node.depth = node.depth + 1;
		child_node.cyclic = cyclic;
		work.push_back(child_node);
	    }
	}

	db_free_full_path(path);
	BU_PUT(path, struct db_full_path);
    }
}


/**
 * A generic traversal function maintaining awareness of the full path
 * to a given object.
 */
static void
db_fullpath_list_subtree(struct db_full_path *path, int curr_bool, union tree *tp,
			 void (*traverse_func) (struct db_full_path *path, void *),
			 void *cmap, void *client_data)
{
    struct directory *dp;
    struct list_client_data_t *lcd= (struct list_client_data_t *)client_data;
    std::unordered_map<std::string, int> *c_inst_map = (std::unordered_map<std::string, int> *)cmap;
    int bool_val = curr_bool;

    if (!tp)
	return;

    RT_CK_FULL_PATH(path);
    RT_CHECK_DBI(lcd->dbip);
    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    db_fullpath_list_subtree(path, OP_UNION, tp->tr_b.tb_left, traverse_func, cmap, client_data);
	    break;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    db_fullpath_list_subtree(path, OP_UNION, tp->tr_b.tb_left, traverse_func, cmap, client_data);
	    if (tp->tr_op == OP_UNION)
		bool_val = 2;
	    if (tp->tr_op == OP_INTERSECT)
		bool_val = 3;
	    if (tp->tr_op == OP_SUBTRACT)
		bool_val = 4;
	    db_fullpath_list_subtree(path, bool_val, tp->tr_b.tb_right, traverse_func, cmap, client_data);
	    break;
	case OP_DB_LEAF:
	    if (UNLIKELY(lcd->dbip->i->dbi_use_comb_instance_ids && c_inst_map))
		(*c_inst_map)[std::string(tp->tr_l.tl_name)]++;
	    if ((dp=db_lookup(lcd->dbip, tp->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
		return;
	    } else {
		/* Create the new path, if doing so doesn't create a cyclic
		 * path (for search, that would constitute an infinite loop) */
		if ((lcd->flags & DB_SEARCH_HIDDEN) || !(dp->d_flags & RT_DIR_HIDDEN)) {
		    struct db_full_path *newpath = NULL;
		    db_add_node_to_full_path(path, dp);
		    DB_FULL_PATH_SET_CUR_BOOL(path, bool_val);
		    if (UNLIKELY(lcd->dbip->i->dbi_use_comb_instance_ids && c_inst_map))
			DB_FULL_PATH_SET_CUR_COMB_INST(path, (*c_inst_map)[std::string(tp->tr_l.tl_name)]-1);
		    if (lcd->full_paths) {
			BU_ALLOC(newpath, struct db_full_path);
			db_full_path_init(newpath);
			db_dup_full_path(newpath, path);
			/* Insert the path in the bu_ptbl collecting paths */
			bu_ptbl_ins(lcd->full_paths, (long *)newpath);
		    }
		    if (!db_full_path_cyclic(path, NULL, 0)) {
			/* Keep going */
			traverse_func(path, client_data);
		    } else {
			char *path_string = db_path_to_string(path);
			bu_log("WARNING: not traversing cyclic path %s\n", path_string);
			bu_free(path_string, "free path str");
		    }
		    DB_FULL_PATH_POP(path);
		}
		break;
	    }
	    break;
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
static void
db_fullpath_list(struct db_full_path *path, void *client_data)
{
    struct directory *dp;
    struct list_client_data_t *lcd= (struct list_client_data_t *)client_data;
    RT_CK_FULL_PATH(path);
    RT_CK_DBI(lcd->dbip);

    dp = DB_FULL_PATH_CUR_DIR(path);
    if (!dp)
	return;
    if (dp->d_flags & RT_DIR_COMB) {
	struct rt_db_internal in;
	struct rt_comb_internal *comb;

	if (rt_db_get_internal(&in, dp, lcd->dbip, NULL) < 0)
	    return;

	std::unordered_map<std::string, int> c_inst_map;
	comb = (struct rt_comb_internal *)in.idb_ptr;
	db_fullpath_list_subtree(path, OP_UNION, comb->tree, db_fullpath_list, &c_inst_map, client_data);
	rt_db_free_internal(&in);
    }
}


static struct db_plan_t *
palloc(enum db_search_ntype t, int (*f)(struct db_plan_t *, struct db_node_t *, struct db_i *, struct bu_ptbl *), struct bu_ptbl *p)
{
    struct db_plan_t *newplan;

    BU_GET(newplan, struct db_plan_t);
    newplan->type = t;
    newplan->eval = f;
    newplan->plans = p;
    newplan->below_cache_index = -1;
    newplan->compiled_regex = NULL;
    if (p)
	bu_ptbl_ins_unique(p, (long *)newplan);
    return newplan;
}


/*
 * (expression) functions --
 *
 * True if expression is true.
 */
static int
f_expr(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct bu_ptbl *results)
{
    struct db_plan_t *p = NULL;
    int state = 0;

    for (p = plan->p_un._p_data[0]; p && (state = (p->eval)(p, db_node, dbip, results)); p = p->next)
	; /* do nothing */

    if (!state)
	db_node->matched_filters = 0;
    return state;
}


/*
 * N_OPENPAREN and N_CLOSEPAREN nodes are temporary place markers.  They are
 * eliminated during phase 2 of find_formplan() --- the '(' node is converted
 * to a N_EXPR node containing the expression and the ')' node is discarded.
 */
static int
c_openparen(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    (*resultplan) = (palloc(N_OPENPAREN, (int (*)(struct db_plan_t *, struct db_node_t *, struct db_i *, struct bu_ptbl *))-1, tbl));
    return BRLCAD_OK;
}


static int
c_closeparen(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    (*resultplan) = (palloc(N_CLOSEPAREN, (int (*)(struct db_plan_t *, struct db_node_t *, struct db_i *, struct bu_ptbl *))-1, tbl));
    return BRLCAD_OK;
}


/*
 * ! expression functions --
 *
 * Negation of a primary; the unary NOT operator.
 */
static int
f_not(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct bu_ptbl *results)
{
    struct db_plan_t *p = NULL;
    int state = 0;

    for (p = plan->p_un._p_data[0]; p && (state = (p->eval)(p, db_node, dbip, results)); p = p->next)
	; /* do nothing */

    db_node->matched_filters = state ? 0 : 1;
    return !state;
}


static int
c_not(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    (*resultplan) = (palloc(N_NOT, f_not, tbl));
    return BRLCAD_OK;
}


static int
find_execute_nested_plans(struct db_i *dbip, struct bu_ptbl *results, struct db_node_t *db_node, struct db_plan_t *plan)
{
    struct db_plan_t *p = NULL;
    int state = 0;
    for (p = plan; p && (state = (p->eval)(p, db_node, dbip, results)); p = p->next)
	; /* do nothing */

    return state;
}


/*
 * -below expression functions --
 *
 * A path passes "-below expr" if any ancestor of the path satisfies expr.
 *
 * Fast path: a traversal-propagated bit answers the common unconstrained case
 * in O(1).  The traversal evaluates the inner plan once per parent, making a
 * complete search O(N) for one -below expression.
 *
 * Slow path: depth-constrained expressions, plans with observable -exec or
 * -print actions, and any path that could not be cached walk the ancestor
 * chain.  Cost is O(D) per path where D is the path depth.
 */
static int
f_below(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct bu_ptbl *results)
{
    int state = 0;
    struct db_node_t curr_node;
    struct db_full_path parent_path;
    int distance;

    if (db_node->below_cache_active && plan->below_cache_index >= 0) {
	uint64_t bit = UINT64_C(1) << plan->below_cache_index;
	if (db_node->below_passes & bit)
	    return 1;
	db_node->matched_filters = 0;
	return 0;
    }

    /* A root-level node (no parent) never satisfies -below. */
    if (db_node->path->fp_len <= 1) {
	db_node->matched_filters = 0;
	return 0;
    }

    db_full_path_init(&parent_path);
    db_dup_full_path(&parent_path, db_node->path);
    DB_FULL_PATH_POP(&parent_path);

    curr_node.path = &parent_path;
    curr_node.flags = db_node->flags;
    curr_node.full_paths = NULL;
    curr_node.unique_dps = db_node->unique_dps;
    curr_node.above_passes_map = NULL;
    curr_node.above_path_hash = BELOW_PATH_HASH_ROOT;
    curr_node.below_passes = 0;
    curr_node.below_cache_active = 0;
    curr_node.matched_filters = 1;

    distance = (int)db_node->path->fp_len - (int)parent_path.fp_len;
    while ((parent_path.fp_len > 0) && (state == 0) && !(db_node->flags & DB_SEARCH_FLAT)) {
	if ((distance <= plan->max_depth) && (distance >= plan->min_depth)) {
	    curr_node.matched_filters = 1;
	    state += find_execute_nested_plans(dbip, results, &curr_node,
					     plan->p_un._bl_data[0]);
	}
	DB_FULL_PATH_POP(&parent_path);
	distance++;
    }
    db_free_full_path(&parent_path);

    if (state > 0)
	return 1;

    db_node->matched_filters = 0;
    return 0;
}


static int
c_below(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    (*resultplan) =  (palloc(N_BELOW, f_below, tbl));
    return BRLCAD_OK;
}


/*
 * -above expression functions --
 *
 * A path passes "-above expr" if any object in the subtree below the current
 * path satisfies expr.  The current object is considered "above" the matching
 * descendant.
 *
 * Fast path: when the caller has pre-built an above_passes_map (a per-plan-node
 * hash set of path fingerprints, populated by a bottom-up reverse pass over all
 * collected paths), this reduces to a single O(1) hash-set lookup.
 *
 * Slow path (fallback): used when the map is not available (e.g. when the plan
 * node could not be included in the pre-pass).  Iterates over all collected full
 * paths looking for descendants that satisfy the inner expression - O(N) per
 * node, O(N²) total for the entire search.
 */
static int
f_above(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct bu_ptbl *UNUSED(results))
{
    /* Fast path: use the pre-computed per-node reverse-BFS cache. */
    if (db_node->above_passes_map) {
	typedef std::unordered_map<struct db_plan_t *, std::unordered_set<bu_h128_t>> AboveCacheMap;
	AboveCacheMap *cmap =
	    static_cast<AboveCacheMap *>(db_node->above_passes_map);
	auto it = cmap->find(plan);
	if (it != cmap->end()) {
	    if (it->second.count(db_node->above_path_hash)) {
		return 1;
	    }
	    db_node->matched_filters = 0;
	    return 0;
	}
	/* plan not in map - fall through to slow path */
    }

    /* Slow path: iterate over all collected full paths. */
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
		    curr_node.unique_dps = NULL;
		    curr_node.above_passes_map = NULL;
		    curr_node.above_path_hash = BELOW_PATH_HASH_ROOT;
		    curr_node.below_passes = 0;
		    curr_node.below_cache_active = 0;
		    curr_node.matched_filters = 1;

		    state = find_execute_nested_plans(dbip, NULL, &curr_node, plan->p_un._ab_data[0]);
		    if (state)
			return 1;
		}
	    }
	}

	db_node->matched_filters = 0;
	return 0;
    }
}


static int
c_above(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    (*resultplan) =  (palloc(N_ABOVE, f_above, tbl));
    return BRLCAD_OK;
}


/*
 * expression -o expression functions --
 *
 * Alternation of primaries; the OR operator.  The second expression is
 * not evaluated if the first expression is true.
 */
static int
f_or(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct bu_ptbl *results)
{
    struct db_plan_t *p = NULL;
    int state = 0;

    for (p = plan->p_un._p_data[0]; p && (state = (p->eval)(p, db_node, dbip, results)); p = p->next)
	; /* do nothing */

    if (state)
	return 1;

    for (p = plan->p_un._p_data[1]; p && (state = (p->eval)(p, db_node, dbip, results)); p = p->next)
	; /* do nothing */

    db_node->matched_filters = (state > 0) ? 1 : 0;
    return state;
}


static int
c_or(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    (*resultplan) = (palloc(N_OR, f_or, tbl));
    return BRLCAD_OK;
}


/*
 * -name functions --
 *
 * True if the basename of the filename being examined
 * matches pattern using Pattern Matching Notation S3.14
 */
static int
f_name(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct bu_ptbl *UNUSED(results))
{
    int ret = 0;
    struct directory *dp;

    dp = DB_FULL_PATH_CUR_DIR(db_node->path);
    if (!dp) {
	db_node->matched_filters = 0;
	return 0;
    }

    ret = !bu_path_match(plan->p_un._c_data, dp->d_namep, 0);

    if (!ret)
	db_node->matched_filters = 0;
    return ret;
}


static int
c_name(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_NAME, f_name, tbl);
    newplan->p_un._c_data = pattern;
    (*resultplan) = newplan;
    return BRLCAD_OK;
}


/*
 * -iname function --
 *
 * True if the basename of the filename being examined
 * matches pattern using case insensitive Pattern Matching Notation S3.14
 */
static int
f_iname(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct bu_ptbl *UNUSED(results))
{
    struct directory *dp;
    int ret = 0;

    dp = DB_FULL_PATH_CUR_DIR(db_node->path);
    if (!dp) {
	db_node->matched_filters = 0;
	return 0;
    }

    ret = !bu_path_match(plan->p_un._c_data, dp->d_namep, BU_PATH_MATCH_CASEFOLD);
    if (!ret)
	db_node->matched_filters = 0;
    return ret;
}


static int
c_iname(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_INAME, f_iname, tbl);
    newplan->p_un._c_data = pattern;
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
static int
f_regex(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct bu_ptbl *UNUSED(results))
{
    regex_t *reg = (regex_t *)plan->compiled_regex;
    int ret = 0;
    char *path_string = NULL;

    if (!reg) {
	db_node->matched_filters = 0;
	return 0;
    }
    path_string = db_path_to_string(db_node->path);
    ret = !(regexec(reg, path_string, 0, NULL, 0));
    if (!ret)
	db_node->matched_filters = 0;
    bu_free(path_string, "free path str");
    return ret;
}


static int
c_regex_common(enum db_search_ntype type, char *regexp, int icase, struct db_plan_t **resultplan, struct bu_ptbl *tbl)
{
    regex_t *reg = NULL;
    struct db_plan_t *newplan;
    int rv;

    reg = (regex_t *)bu_malloc(sizeof(regex_t), "compiled search regex");
    if (icase == 1) {
	rv = regcomp(reg, regexp, REG_NOSUB|REG_EXTENDED|REG_ICASE);
    } else {
	rv = regcomp(reg, regexp, REG_NOSUB|REG_EXTENDED);
    }

    if (rv != 0) {
	bu_log("ERROR: regular expression failed to compile: %s\n", regexp);
	bu_free(reg, "compiled search regex");
	return BRLCAD_ERROR;
    }

    newplan = palloc(type, f_regex, tbl);
    newplan->p_un._regex_pattern = regexp;
    newplan->compiled_regex = (void *)reg;
    (*resultplan) = newplan;

    return BRLCAD_OK;
}


static int
c_regex(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    return c_regex_common(N_REGEX, pattern, 0, resultplan, tbl);
}


static int
c_iregex(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    return c_regex_common(N_IREGEX, pattern, 1, resultplan, tbl);
}


static int
string_to_name_and_val(const char *in, struct bu_vls *name, struct bu_vls *value)
{
    size_t equalpos = 0;
    int checkval = 0;

    while ((equalpos < strlen(in)) &&
	   (in[equalpos] != '=') &&
	   (in[equalpos] != '>') &&
	   (in[equalpos] != '<')) {
	if ((in[equalpos] == '/') && (in[equalpos + 1] == '=')) {
	    equalpos++;
	}
	if ((in[equalpos] == '/') && (in[equalpos + 1] == '<')) {
	    equalpos++;
	}
	if ((in[equalpos] == '/') && (in[equalpos + 1] == '>')) {
	    equalpos++;
	}
	equalpos++;
    }

    if (equalpos == strlen(in)) {
	/*No logical expression given - just copy attribute name*/
	bu_vls_strcpy(name, in);
    } else {
	checkval = 1; /*Assume simple equality comparison, then check for other cases and change if found.*/
	if ((in[equalpos] == '>') && (in[equalpos + 1] != '=')) {
	    checkval = 2;
	}
	if ((in[equalpos] == '<') && (in[equalpos + 1] != '=')) {
	    checkval = 3;
	}
	if ((in[equalpos] == '=') && (in[equalpos + 1] == '>')) {
	    checkval = 4;
	}
	if ((in[equalpos] == '=') && (in[equalpos + 1] == '<')) {
	    checkval = 5;
	}
	if ((in[equalpos] == '>') && (in[equalpos + 1] == '=')) {
	    checkval = 4;
	}
	if ((in[equalpos] == '<') && (in[equalpos + 1] == '=')) {
	    checkval = 5;
	}

	bu_vls_strncpy(name, in, equalpos);
	if (checkval < 4) {
	    bu_vls_strncpy(value, &(in[equalpos+1]), strlen(in) - equalpos - 1);
	} else {
	    bu_vls_strncpy(value, &(in[equalpos+2]), strlen(in) - equalpos - 1);
	}
    }
    return checkval;
}


/* Check all attributes for a match to the requested attribute.
 * If an expression was supplied, check the value of any matches
 * to the attribute name in the logical expression before
 * returning success
 */
static int
avs_check(const char *keystr, const char *value, int checkval, int strcomparison, struct bu_attribute_value_set *avs)
{
    struct bu_attribute_value_pair *avpp;

    for (BU_AVS_FOR(avpp, avs)) {
	if (!bu_path_match(keystr, avpp->name, 0)) {
	    if (checkval >= 1) {

		/* String based comparisons */
		if ((checkval == 1) && (strcomparison == 1)) {
		    if (!bu_path_match(value, avpp->value, 0)) {
			return 1;
		    } else {
			return 0;
		    }
		}
		if ((checkval == 2) && (strcomparison == 1)) {
		    if (bu_strcmp(value, avpp->value) < 0) {
			return 1;
		    } else {
			return 0;
		    }
		}
		if ((checkval == 3) && (strcomparison == 1)) {
		    if (bu_strcmp(value, avpp->value) > 0) {
			return 1;
		    } else {
			return 0;
		    }
		}
		if ((checkval == 4) && (strcomparison == 1)) {
		    if ((!bu_path_match(value, avpp->value, 0)) || (bu_strcmp(value, avpp->value) < 0)) {
			return 1;
		    } else {
			return 0;
		    }
		}
		if ((checkval == 5) && (strcomparison == 1)) {
		    if ((!bu_path_match(value, avpp->value, 0)) || (bu_strcmp(value, avpp->value) > 0)) {
			return 1;
		    } else {
			return 0;
		    }
		}


		/* Numerical Comparisons */
		if (strcomparison == 0 && checkval <= 5) {
		    char* val_buf = NULL;
		    char* avpp_val_buf = NULL;

		    const long val_conv = strtol(value, &val_buf, 10);
		    const long avpp_val_conv = strtol(avpp->value, &avpp_val_buf, 10);

		    /* error checking */
		    if (value == val_buf || avpp->value == avpp_val_buf)
			return 0;   /* string did not convert to long */
		    if (val_conv > LONG_MAX || 
			val_conv < LONG_MIN || 
			avpp_val_conv < LONG_MIN || 
			avpp_val_conv > LONG_MAX)
			return 0;   /* conversion is out of range for long */

		    if ((checkval == 1) && (val_conv == avpp_val_conv))
			return 1;
		    if ((checkval == 2) && (val_conv < avpp_val_conv))
			return 1;
		    if ((checkval == 3) && (val_conv > avpp_val_conv))
			return 1;
		    if ((checkval == 4) && (val_conv <= avpp_val_conv))
			return 1;
		    if ((checkval == 5) && (val_conv >= avpp_val_conv))
			return 1;
		}
		return 0;
	    } else {
		return 1;
	    }
	}
    }
    return 0;
}


/*
 * -param functions --
 *
 * True if the database object being examined has the parameter
 * supplied to the param option
 */
static int
f_objparam(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct bu_ptbl *UNUSED(results))
{
    struct bu_vls paramname = BU_VLS_INIT_ZERO;
    struct bu_vls value = BU_VLS_INIT_ZERO;
    struct bu_vls s_tcl = BU_VLS_INIT_ZERO;
    struct rt_db_internal in;
    struct bu_attribute_value_set avs;
    int checkval = 0;
    int strcomparison = 0;
    size_t i;
    struct directory *dp;
    int ret = 0;

    /* Check for unescaped >, < or = characters.  If present, the
     * attribute must not only be present but the value assigned to
     * the attribute must satisfy the logical expression.  In the case
     * where a > or < is used with a string argument the behavior will
     * follow ASCII lexicographical order.  In the case of equality
     * between strings, bu_path_match() is used to support pattern
     * matching.
     */

    checkval = string_to_name_and_val(plan->p_un._attr_data, &paramname, &value);

    /* Now that we have the value, check to see if it is all numbers.
     * If so, use numerical comparison logic - otherwise use string
     * logic.
     */

    for (i = 0; i < strlen(bu_vls_addr(&value)); i++) {
	if (!(isdigit((int)(bu_vls_addr(&value)[i])))) {
	    strcomparison = 1;
	}
    }

    /* Get parameters for object as an avs.
     */

    dp = DB_FULL_PATH_CUR_DIR(db_node->path);
    if (!dp) {
	bu_vls_free(&paramname);
	bu_vls_free(&value);
	db_node->matched_filters = 0;
	return 0;
    }

    RT_DB_INTERNAL_INIT(&in);
    if (rt_db_get_internal(&in, dp, dbip, (fastf_t *)NULL) < 0) {
	rt_db_free_internal(&in);
	bu_vls_free(&paramname);
	bu_vls_free(&value);
	db_node->matched_filters = 0;
	return 0;
    }


    if ((&in)->idb_meth->ft_get(&s_tcl, &in, NULL) == BRLCAD_ERROR) {
	rt_db_free_internal(&in);
	bu_vls_free(&paramname);
	bu_vls_free(&value);
	bu_vls_free(&s_tcl);
	db_node->matched_filters = 0;
	return 0;
    }
    rt_db_free_internal(&in);

    bu_avs_init_empty(&avs);
    if (tcl_list_to_avs(bu_vls_addr(&s_tcl), &avs, 1)) {
	bu_avs_free(&avs);
	bu_vls_free(&s_tcl);
	bu_vls_free(&paramname);
	bu_vls_free(&value);
	db_node->matched_filters = 0;
	return 0;
    }

    ret = avs_check(bu_vls_addr(&paramname), bu_vls_addr(&value), checkval, strcomparison, &avs);
    bu_avs_free(&avs);
    bu_vls_free(&s_tcl);
    bu_vls_free(&paramname);
    bu_vls_free(&value);
    if (!ret)
	db_node->matched_filters = 0;
    return ret;
}


static int
c_objparam(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_PARAM, f_objparam, tbl);
    newplan->p_un._attr_data = pattern;
    (*resultplan) = newplan;

    return BRLCAD_OK;
}


/*
 * -attr functions --
 *
 * True if the database object being examined has the attribute
 * supplied to the attr option
 */
static int
f_attr(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct bu_ptbl *UNUSED(results))
{
    struct bu_vls attribname = BU_VLS_INIT_ZERO;
    struct bu_vls value = BU_VLS_INIT_ZERO;
    struct bu_attribute_value_set avs;
    int checkval = 0;
    int strcomparison = 0;
    size_t i;
    struct directory *dp;
    int ret = 0;

    /* Check for unescaped >, < or = characters.  If present, the
     * attribute must not only be present but the value assigned to
     * the attribute must satisfy the logical expression.  In the case
     * where a > or < is used with a string argument the behavior will
     * follow ASCII lexicographical order.  In the case of equality
     * between strings, bu_path_match() is used to support pattern
     * matching.
     */

    checkval = string_to_name_and_val(plan->p_un._attr_data, &attribname, &value);

    /* Now that we have the value, check to see if it is all numbers.
     * If so, use numerical comparison logic - otherwise use string
     * logic.
     */

    for (i = 0; i < strlen(bu_vls_addr(&value)); i++) {
	if (!(isdigit((int)(bu_vls_addr(&value)[i])))) {
	    strcomparison = 1;
	}
    }

    /* Get attributes for object.
     */

    dp = DB_FULL_PATH_CUR_DIR(db_node->path);
    if (!dp) {
	bu_vls_free(&attribname);
	bu_vls_free(&value);
	db_node->matched_filters = 0;
	return 0;
    }

    bu_avs_init_empty(&avs);
    if (db5_get_attributes(dbip, &avs, dp) < 0) {
	bu_avs_free(&avs);
	bu_vls_free(&attribname);
	bu_vls_free(&value);
	db_node->matched_filters = 0;
	return 0;
    }

    ret = avs_check(bu_vls_addr(&attribname), bu_vls_addr(&value), checkval, strcomparison, &avs);
    bu_avs_free(&avs);
    bu_vls_free(&attribname);
    bu_vls_free(&value);
    if (!ret)
	db_node->matched_filters = 0;
    return ret;
}


static int
c_attr(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_ATTR, f_attr, tbl);
    newplan->p_un._attr_data = pattern;
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
static int
f_stdattr(struct db_plan_t *UNUSED(plan), struct db_node_t *db_node, struct db_i *dbip, struct bu_ptbl *UNUSED(results))
{
    struct bu_attribute_value_pair *avpp;
    struct bu_attribute_value_set avs;
    struct directory *dp;
    int found_nonstd_attr = 0;
    int found_attr = 0;

    /* Get attributes for object and check all of them to see if there
     * is not a match to the standard attributes.  If any is found
     * return failure, otherwise success.
     */

    dp = DB_FULL_PATH_CUR_DIR(db_node->path);
    if (!dp) {
	db_node->matched_filters = 0;
	return 0;
    }

    bu_avs_init_empty(&avs);
    if (db5_get_attributes(dbip, &avs, dp) < 0) {
	bu_avs_free(&avs);
	db_node->matched_filters = 0;
	return 0;
    }

    for (BU_AVS_FOR(avpp, &avs)) {
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

    if (!found_nonstd_attr && found_attr) {
	return 1;
    } else {
	db_node->matched_filters = 0;
	return 0;
    }
}


static int
c_stdattr(char *UNUSED(pattern), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_STDATTR, f_stdattr, tbl);
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
static int
f_type(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct bu_ptbl *UNUSED(results))
{
    struct rt_db_internal intern;
    struct directory *dp;
    struct rt_bot_internal *bot_ip;
    int type_match = 0;
    int type;
    const struct bn_tol arb_tol = BN_TOL_INIT_TOL;

    dp = DB_FULL_PATH_CUR_DIR(db_node->path);
    if (!dp) {
	db_node->matched_filters = 0;
	return 0;
    }
    if (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	db_node->matched_filters = 0;
	return 0;
    }

    /* We can handle combs without needing to perform the rt_db_internal unpacking - do so
     * to help performance. */
    if (dp->d_flags & RT_DIR_COMB) {
	if (dp->d_flags & RT_DIR_REGION) {
	    if ((!bu_path_match(plan->p_un._type_data, "r", 0)) || (!bu_path_match(plan->p_un._type_data, "reg", 0))  || (!bu_path_match(plan->p_un._type_data, "region", 0))) {
		type_match = 1;
	    }
	}
	if ((!bu_path_match(plan->p_un._type_data, "c", 0)) || (!bu_path_match(plan->p_un._type_data, "comb", 0)) || (!bu_path_match(plan->p_un._type_data, "combination", 0))) {
	    type_match = 1;
	}
	goto return_label;
    } else {
	if ((!bu_path_match(plan->p_un._type_data, "r", 0)) || (!bu_path_match(plan->p_un._type_data, "reg", 0))  || (!bu_path_match(plan->p_un._type_data, "region", 0)) || (!bu_path_match(plan->p_un._type_data, "c", 0)) || (!bu_path_match(plan->p_un._type_data, "comb", 0)) || (!bu_path_match(plan->p_un._type_data, "combination", 0))) {
	    goto return_label;
	}

    }

    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL) < 0) {
	db_node->matched_filters = 0;
	return 0;
    }
    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD) {
	rt_db_free_internal(&intern);
	db_node->matched_filters = 0;
	return 0;
    }

    switch (intern.idb_minor_type) {
	case DB5_MINORTYPE_BRLCAD_ARB8:
	    type = rt_arb_std_type(&intern, &arb_tol);
	    switch (type) {
		case ARB4:
		    type_match = (!bu_path_match(plan->p_un._type_data, "arb4", 0));
		    break;
		case ARB5:
		    type_match = (!bu_path_match(plan->p_un._type_data, "arb5", 0));
		    break;
		case ARB6:
		    type_match = (!bu_path_match(plan->p_un._type_data, "arb6", 0));
		    break;
		case ARB7:
		    type_match = (!bu_path_match(plan->p_un._type_data, "arb7", 0));
		    break;
		case ARB8:
		    type_match = (!bu_path_match(plan->p_un._type_data, "arb8", 0));
		    break;
		default:
		    type_match = (!bu_path_match(plan->p_un._type_data, "invalid", 0));
		    break;
	    }
	    break;
	case DB5_MINORTYPE_BRLCAD_ELL:
	    /* are we looking for a sph? */
	    if ((!bu_path_match(plan->p_un._type_data, "sph", 0) ||
		 !bu_path_match(plan->p_un._type_data, "sphere", 0)) && rt_ell_is_sph(&intern)) {
		type_match = 1;
		break;
	    }
	    /* intentional fallthrough - we weren't looking for a sphere */
	default:
	    type_match = !bu_path_match(plan->p_un._type_data, intern.idb_meth->ft_label, 0);
	    break;
    }

    /* Match anything that doesn't define a 2D or 3D shape - unfortunately, this list will have to
     * be updated manually unless/until some functionality is added to generate it */
    if (!bu_path_match(plan->p_un._type_data, "shape", 0) &&
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ANNOT &&
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_COMBINATION &&
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_CONSTRAINT &&
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_DATUM &&
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_GRIP &&
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_JOINT &&
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PNTS &&
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_SCRIPT &&
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_SUBMODEL
	) {
	type_match = 1;
    }

    if (!bu_path_match(plan->p_un._type_data, "plate", 0)) {
	switch (intern.idb_minor_type) {
	    case DB5_MINORTYPE_BRLCAD_BOT:
		bot_ip = (struct rt_bot_internal *)intern.idb_ptr;
		if (bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS) {
		    type_match = 1;
		}
		break;
	    case DB5_MINORTYPE_BRLCAD_BREP:
		if (rt_brep_plate_mode(&intern)) {
		    type_match = 1;
		}
		break;
	    default:
		break;
	}
    }

    if (!bu_path_match(plan->p_un._type_data, "volume", 0) &&
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ANNOT &&
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_COMBINATION &&
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_CONSTRAINT &&
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_DATUM &&
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_GRIP &&
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_JOINT &&
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PNTS &&
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_SCRIPT &&
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_SUBMODEL &&
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_SKETCH) {
	switch (intern.idb_minor_type) {
	    case DB5_MINORTYPE_BRLCAD_BOT:
		bot_ip = (struct rt_bot_internal *)intern.idb_ptr;
		if (bot_ip->mode == RT_BOT_SOLID) {
		    type_match = 1;
		}
		break;
	    case DB5_MINORTYPE_BRLCAD_BREP:
		if (!rt_brep_plate_mode(&intern)) {
		    type_match = 1;
		}
		break;
	    default:
		type_match = 1;
		break;
	}
    }


    rt_db_free_internal(&intern);

return_label:

    if (!type_match)
	db_node->matched_filters = 0;
    return type_match;
}


static int
c_type(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_TYPE, f_type, tbl);
    newplan->p_un._type_data = pattern;
    (*resultplan) = newplan;

    return BRLCAD_OK;
}


/*
 * -size function --
 *
 * True if the database object being examined satisfies
 * the size criteria: [><=]size
 */
static int
f_size(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct bu_ptbl *UNUSED(results))
{
    struct directory *dp;
    int ret = 0;
    int checkval = 0;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_vls value = BU_VLS_INIT_ZERO;

    dp = DB_FULL_PATH_CUR_DIR(db_node->path);
    if (!dp) {
	db_node->matched_filters = 0;
	return 0;
    }

    /* Check for unescaped >, < or = characters.  If present, the
     * attribute must not only be present but the value assigned to
     * the attribute must satisfy the logical expression.  In the case
     * where a > or < is used with a string argument the behavior will
     * follow ASCII lexicographical order.  In the case of equality
     * between strings, fnmatch is used to support pattern matching
     */

    checkval = string_to_name_and_val(plan->p_un._depth_data, &name, &value);

    if ((bu_vls_strlen(&value) > 0 && isdigit((int)bu_vls_addr(&value)[0]))
	|| (bu_vls_strlen(&value) == 0 && isdigit((int)bu_vls_addr(&name)[0]))) {
	switch (checkval) {
	    case 0:
		ret = ((int)dp->d_len == atol(bu_vls_addr(&name))) ? 1 : 0;
		break;
	    case 1:
		ret = ((int)dp->d_len == atol(bu_vls_addr(&value))) ? 1 : 0;
		break;
	    case 2:
		ret = ((int)dp->d_len > atol(bu_vls_addr(&value))) ? 1 : 0;
		break;
	    case 3:
		ret = ((int)dp->d_len < atol(bu_vls_addr(&value))) ? 1 : 0;
		break;
	    case 4:
		ret = ((int)dp->d_len >= atol(bu_vls_addr(&value))) ? 1 : 0;
		break;
	    case 5:
		ret = ((int)dp->d_len <= atol(bu_vls_addr(&value))) ? 1 : 0;
		break;
	    default:
		ret = 0;
		break;
	}
    }
    bu_vls_free(&name);
    bu_vls_free(&value);

    if (!ret) db_node->matched_filters = 0;
    return ret;
}


static int
c_size(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_SIZE, f_size, tbl);
    newplan->p_un._depth_data = pattern;
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
static int
f_bool(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct bu_ptbl *UNUSED(results))
{
    int bool_match = 0;
    int bool_type = DB_FULL_PATH_CUR_BOOL(db_node->path);

    if (plan->p_un._bool_data == bool_type)
	bool_match = 1;
    if (!bool_match)
	db_node->matched_filters = 0;

    return bool_match;
}


static int
c_bool(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    int bool_type = 0;
    struct db_plan_t *newplan;

    newplan = palloc(N_BOOL, f_bool, tbl);

    if (!bu_path_match(pattern, "u", 0) || !bu_path_match(pattern, "U", 0))
	bool_type = 2;
    if (!bu_path_match(pattern, "+", 0))
	bool_type = 3;
    if (!bu_path_match(pattern, "-", 0))
	bool_type = 4;

    newplan->p_un._bool_data = bool_type;
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
static int
f_maxdepth(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct bu_ptbl *UNUSED(results))
{
    int ret = ((int)db_node->path->fp_len - 1 <= plan->p_un._max_data) ? 1 : 0;

    if (!ret)
	db_node->matched_filters = 0;

    return ret;
}


static int
c_maxdepth(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_MAXDEPTH, f_maxdepth, tbl);
    newplan->p_un._max_data = atoi(pattern);
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
static int
f_mindepth(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct bu_ptbl *UNUSED(results))
{
    int ret = ((int)db_node->path->fp_len - 1 >= plan->p_un._min_data) ? 1 : 0;

    if (!ret)
	db_node->matched_filters = 0;

    return ret;
}


static int
c_mindepth(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_MINDEPTH, f_mindepth, tbl);
    newplan->p_un._min_data = atoi(pattern);
    (*resultplan) = newplan;

    return BRLCAD_OK;
}


/*
 * -depth function --
 *
 * True if the database object being examined satisfies
 * the depth criteria: [><=]depth
 */
static int
f_depth(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct bu_ptbl *UNUSED(results))
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
     * between strings, bu_path_match() is used to support pattern
     * matching.
     */

    checkval = string_to_name_and_val(plan->p_un._depth_data, &name, &value);

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

    if (!ret)
	db_node->matched_filters = 0;
    return ret;
}


/*
 * -exec function --
 *
 * True if the expression returns true.
 */
static int
f_exec(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct bu_ptbl *UNUSED(results))
{
    /* TODO make this faster by storing the individual "subholes" so they don't have to be recalculated */
    int ret, hole_i, char_i, plain_begin, plain_len;
    char **filleds = NULL;
    char **originals = NULL;
    char *name = NULL;
    size_t filled_len = 0;
    size_t name_len = 0;
    size_t old_filled_len = 0;

    if (!db_node || !db_node->path || !DB_FULL_PATH_CUR_DIR(db_node->path))
	return 0;

    if (0 < plan->p_un.ex._e_nholes) {
	originals = (char **)bu_calloc(plan->p_un.ex._e_nholes, sizeof(char *), "f_exec originals");
	filleds = (char **)bu_calloc(plan->p_un.ex._e_nholes, sizeof(char *), "f_exec filleds");

	for (hole_i=0; hole_i<plan->p_un.ex._e_nholes; hole_i++)
	    originals[hole_i] = plan->p_un.ex._e_argv[plan->p_un.ex._e_holes[hole_i]];

	if (db_node->flags & DB_SEARCH_RETURN_UNIQ_DP || db_node->flags & DB_SEARCH_FLAT)
	    name = DB_FULL_PATH_CUR_DIR(db_node->path)->d_namep;
	else
	    name = db_path_to_string(db_node->path);
	name_len = strlen(name);
    }

    for (hole_i=0; hole_i<plan->p_un.ex._e_nholes; hole_i++) {
	plain_begin = 0;
	filled_len = 0;
	plain_len = 0;
	for (char_i=0; originals[hole_i][char_i] != '\0'; char_i++) {
	    if (originals[hole_i][char_i] == '{' && originals[hole_i][char_i+1] == '}') {
		old_filled_len = filled_len;
		filled_len += plain_len + name_len;
		filleds[hole_i] = (char *)bu_realloc(filleds[hole_i],
						     filled_len,
						     "f_exec filleds[hole_i]");
		memcpy(filleds[hole_i] + old_filled_len, originals[hole_i] + plain_begin, plain_len);
		if (name) {
		    memcpy(filleds[hole_i] + old_filled_len + plain_len, name, name_len);
		}
		plain_begin = char_i + 2;
		plain_len = 0;
		char_i++; /* skip closing brace */
	    } else {
		plain_len++;
	    }
	}
	old_filled_len = filled_len;
	filled_len += plain_len + 1 /* for the null byte */;
	filleds[hole_i] = (char *)bu_realloc(filleds[hole_i],
					     filled_len,
					     "f_exec filleds[hole_i]");
	memcpy(filleds[hole_i] + old_filled_len, originals[hole_i] + plain_begin, plain_len);
	filleds[hole_i][filled_len-1] = '\0';
    }

    for (hole_i=0; hole_i<plan->p_un.ex._e_nholes; hole_i++) {
	plan->p_un.ex._e_argv[plan->p_un.ex._e_holes[hole_i]] = filleds[hole_i];
    }

    /* Only try to exec if we actually have a callback */
    if (plan->p_un.ex._e_callback) {
	ret = (*plan->p_un.ex._e_callback)(plan->p_un.ex._e_argc, (const char**)plan->p_un.ex._e_argv, plan->p_un.ex._e_userdata1, plan->p_un.ex._e_userdata2);
    } else {
	ret = 1;
    }

    if (name && !(db_node->flags & DB_SEARCH_RETURN_UNIQ_DP || db_node->flags & DB_SEARCH_FLAT)) {
	bu_free(name, "f_exec string");
    }

    for (hole_i=0; hole_i<plan->p_un.ex._e_nholes; hole_i++) {
	plan->p_un.ex._e_argv[plan->p_un.ex._e_holes[hole_i]] = originals[hole_i];
	bu_free(filleds[hole_i], "f_exec filled string");
    }
    if (originals)
	bu_free(originals, "f_exec originals");
    if (filleds)
	bu_free(filleds, "f_exec filleds");

    if (!ret)
	db_node->matched_filters = 0;

    return ret;
}


static int
c_exec(char *UNUSED(ignore), char ***argvp, int UNUSED(is_ok), struct db_plan_t **resultplan, int *db_search_isoutput, struct bu_ptbl *tbl, struct _db_search_ctx *ctx)
{
    struct db_plan_t *newplan;
    char **e_argv = NULL;
    int *holes = NULL;
    int nholes = 0;
    int scfound = 0;
    int i = 0;
    int l = 0; /* should this be unsigned? argc is an int, so this could lead to an overflow in many ways */
    int holefound;

    *db_search_isoutput = 1;

    while (**argvp != NULL && !scfound) {
	scfound = (**argvp)[0] == ';' && (**argvp)[1] == '\0'; /* is this a semicolon? */
	if (!scfound) {
	    e_argv = (char**)bu_realloc(e_argv, sizeof(char**) * (l+1), "e_argv");
	    holefound = 0;
	    for (i=0; !holefound && (**argvp)[i]!='\0'; i++) {
		holefound = (**argvp)[i]=='{' && (**argvp)[i+1]=='}';
	    }
	    if (holefound) { /* is this a {}? (ie.: hole) */
		nholes++;
		holes = (int *)bu_realloc(holes, sizeof(int) * nholes, "e_holes");
		holes[nholes - 1] = l;
	    }
	    e_argv[l] = bu_strdupm(**argvp, "e_argv arg");
	    l++;
	}
	(*argvp)++;
    }

    if (!scfound) {
	/* is this a good idea? */
	for(i = 0; i < l; i++) {
	    bu_free(e_argv[i], "e_argv arg");
	}
	bu_free(e_argv, "e_argv");

	if (holes) {
	    bu_free(holes, "e_holes");
	}
	bu_log("expected ; at the end of -exec's argument list\n");

	return BRLCAD_ERROR;
    }

    newplan = palloc(N_EXEC, f_exec, tbl);

    newplan->p_un.ex._e_argv = e_argv;
    newplan->p_un.ex._e_argc = l;
    newplan->p_un.ex._e_holes = holes;
    newplan->p_un.ex._e_nholes = nholes;
    newplan->p_un.ex._e_callback = ctx->clbk;
    newplan->p_un.ex._e_userdata1 = ctx->u1;
    newplan->p_un.ex._e_userdata2 = ctx->u2;

    (*resultplan) = newplan;

    return BRLCAD_OK;
}


static int
c_depth(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_DEPTH, f_depth, tbl);
    newplan->p_un._depth_data = pattern;
    (*resultplan) = newplan;

    return BRLCAD_OK;
}


/*
 * -idn function --
 *
 * True if the matrix combining the object into its parent tree is an
 * identity matrix.
 */

static int
child_matrix(union tree *tp, const char *n, int target_inst, int *curr_inst, mat_t *m)
{
    if (!tp)
	return 0;
    RT_CK_TREE(tp);
    switch (tp->tr_op) {
	case OP_DB_LEAF:
	    if (BU_STR_EQUAL(n, tp->tr_l.tl_name)) {
		if (*curr_inst == target_inst) {
		    MAT_IDN(*m);
		    if (tp->tr_l.tl_mat)
			MAT_COPY(*m, tp->tr_l.tl_mat);
		    return 1;
		}
		(*curr_inst)++;
	    }
	    return 0;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    return child_matrix(tp->tr_b.tb_left, n, target_inst, curr_inst, m);
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    /* This node is known to be a binary op */
	    if (child_matrix(tp->tr_b.tb_left, n, target_inst, curr_inst, m))
		return 1;
	    return child_matrix(tp->tr_b.tb_right, n, target_inst, curr_inst, m);
	default:
	    bu_log("child_matrix: bad op %d\n", tp->tr_op);
	    return 0;
    }
}


static int
f_matrix(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct bu_ptbl *UNUSED(results))
{
    const struct bn_tol mtol = BN_TOL_INIT_TOL;
    mat_t mat;
    int matrix_found = 1;
    MAT_IDN(mat);

    // Top level objects are always IDN included.  For anything else, see what
    // the comb says.  Note that this filter uses the immediate parent/child
    // matrix, not the accumulated matrix along the path.
    if (DB_FULL_PATH_LEN(db_node->path) > 1) {
	matrix_found = 0;

	struct directory *cdp = DB_FULL_PATH_CUR_DIR(db_node->path);
	struct directory *dp = DB_FULL_PATH_GET(db_node->path, DB_FULL_PATH_LEN(db_node->path) - 2);

	if (dp->d_flags & RT_DIR_COMB) {
	    struct rt_db_internal intern;
	    RT_DB_INTERNAL_INIT(&intern);
	    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL) > 0) {
		struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
		if (comb->tree != NULL) {
		    int curr_inst = 0;
		    int target_inst = DB_FULL_PATH_CUR_COMB_INST(db_node->path);
		    matrix_found = child_matrix(comb->tree, cdp->d_namep,
			    target_inst, &curr_inst, &mat);
		}
		rt_db_free_internal(&intern);
	    }
	}
    }

    if (!matrix_found) {
	db_node->matched_filters = 0;
	return 0;
    }

    if (bn_mat_is_equal(mat, plan->m, &mtol)) {
	return 1;
    }

    db_node->matched_filters = 0;
    return 0;
}


static int
c_matrix(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_MATRIX, f_matrix, tbl);

    MAT_IDN(newplan->m);
    bn_opt_mat(NULL, 1, (const char **)&pattern, (matp_t)newplan->m);
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
static int
f_nnodes(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *dbip, struct bu_ptbl *UNUSED(results))
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

    if (isdigit((int)plan->p_un._node_data[0])) {
	doequal = 1;
	node_count_target = (size_t)atoi(plan->p_un._node_data);
    } else {
	if (plan->p_un._node_data[0] == '>') dogreaterthan = 1;
	if (plan->p_un._node_data[0] == '<') dolessthan = 1;
	if (plan->p_un._node_data[0] == '=') doequal = 1;
	if (plan->p_un._node_data[0] != '>' && plan->p_un._node_data[0] != '<' && plan->p_un._node_data[0] != '=') {
	    db_node->matched_filters = 0;
	    return 0;
	}
	if (plan->p_un._node_data[1] == '=') {
	    doequal = 1;
	    if (isdigit((int)plan->p_un._node_data[2])) {
		node_count_target = (size_t)atoi((plan->p_un._node_data)+2);
	    } else {
		db_node->matched_filters = 0;
		return 0;
	    }
	} else {
	    if (isdigit((int)plan->p_un._node_data[1])) {
		node_count_target = (size_t)atoi((plan->p_un._node_data)+1);
	    } else {
		db_node->matched_filters = 0;
		return 0;
	    }
	}
    }

    /* Get the number of nodes for the current object and check if the
     * value satisfied the logical conditions specified in the
     * argument string.
     */

    dp = DB_FULL_PATH_CUR_DIR(db_node->path);
    if (!dp) {
	db_node->matched_filters = 0;
	return 0;
    }

    if (dp->d_flags & RT_DIR_COMB) {
	RT_DB_INTERNAL_INIT(&in);
	if (rt_db_get_internal(&in, dp, dbip, (fastf_t *)NULL) < 0) {
	    db_node->matched_filters = 0;
	    return 0;
	}
	comb = (struct rt_comb_internal *)in.idb_ptr;
	if (comb->tree == NULL) {
	    node_count = 0;
	} else {
	    node_count = db_tree_nleaves(comb->tree);
	}
	rt_db_free_internal(&in);
    } else {
	db_node->matched_filters = 0;
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

    db_node->matched_filters = 0;
    return 0;
}


static int
c_nnodes(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_NNODES, f_nnodes, tbl);
    newplan->p_un._node_data = pattern;
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
static int
f_path(struct db_plan_t *plan, struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct bu_ptbl *UNUSED(results))
{
    char *path_string = NULL;
    int ret = 0;

    path_string = db_path_to_string(db_node->path);
    ret = !bu_path_match(plan->p_un._path_data, path_string, 0);
    bu_free(path_string, "free path str");

    if (!ret)
	db_node->matched_filters = 0;

    return ret;
}


static int
c_path(char *pattern, char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *UNUSED(db_search_isoutput), struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    struct db_plan_t *newplan;

    newplan = palloc(N_PATH, f_path, tbl);
    newplan->p_un._path_data = pattern;
    (*resultplan) = newplan;

    return BRLCAD_OK;
}


/*
 * -print functions --
 *
 * Always true, causes the current pathname to be added to the results
 * list.
 */
static int
f_print(struct db_plan_t *UNUSED(plan), struct db_node_t *db_node, struct db_i *UNUSED(dbip), struct bu_ptbl *results)
{
    if (!results || !db_node)
	return 1;

    if (db_node->flags & DB_SEARCH_FLAT || db_node->flags & DB_SEARCH_RETURN_UNIQ_DP) {
	long *dbfp = (long *)DB_FULL_PATH_CUR_DIR(db_node->path);
	if (dbfp) {
	    if (db_node->unique_dps) {
		typedef std::unordered_set<struct directory *> UniqueDpSet;
		UniqueDpSet *unique_dps = static_cast<UniqueDpSet *>(db_node->unique_dps);
		if (unique_dps->insert((struct directory *)dbfp).second)
		    bu_ptbl_ins(results, dbfp);
	    } else {
		bu_ptbl_ins_unique(results, dbfp);
	    }
	}
    } else {
	struct db_full_path *new_entry;
	BU_ALLOC(new_entry, struct db_full_path);
	db_full_path_init(new_entry);
	db_dup_full_path(new_entry, (const struct db_full_path *)(db_node->path));
	bu_ptbl_ins(results, (long *)new_entry);
    }
    return 1;
}


static int
c_print(char *UNUSED(ignore), char ***UNUSED(ignored), int UNUSED(unused), struct db_plan_t **resultplan, int *db_search_isoutput, struct bu_ptbl *tbl, struct _db_search_ctx *UNUSED(ctx))
{
    *db_search_isoutput = 1;
    (*resultplan) = palloc(N_PRINT, f_print, tbl);
    return BRLCAD_OK;
}


static int
typecompare(const void *a, const void *b)
{
    return bu_strcmp(((OPTION *)a)->name, ((OPTION *)b)->name);
}


static OPTION *
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
static int
find_create(char ***argvp,
	    struct db_plan_t **resultplan,
	    struct bu_ptbl *UNUSED(results),
	    int *db_search_isoutput,
	    int quiet, struct bu_ptbl *tbl,
	    struct _db_search_ctx *ctx)
{
    OPTION *p;
    struct db_plan_t *newplan = NULL;
    char **argv;
    int checkval;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_vls value = BU_VLS_INIT_ZERO;
    int create_result = BRLCAD_OK;

    if (!argvp || !resultplan)
	return BRLCAD_ERROR;

    argv = *argvp;

    checkval = string_to_name_and_val(argv[0], &name, &value);

    p = option(bu_vls_addr(&name));
    if (!p) {
	if (!quiet) {
	    bu_log("%s: unknown option passed to find_create\n", *argv);
	}
	bu_vls_free(&name);
	bu_vls_free(&value);
	return BRLCAD_ERROR;
    }

    ++argv;
    if (p->flags & (O_ARGV|O_ARGVP) && !*argv) {
	if (!quiet) {
	    bu_log("%s: requires additional arguments\n", *--argv);
	}
	bu_vls_free(&name);
	bu_vls_free(&value);
	return BRLCAD_ERROR;
    }

    switch (p->flags) {
	case O_NONE:
	    break;
	case O_ZERO:
	    create_result = (p->create)(NULL, NULL, 0, &newplan, db_search_isoutput, tbl, ctx);
	    break;
	case O_ARGV:
	    create_result = (p->create)(*argv++, NULL, 0, &newplan, db_search_isoutput, tbl, ctx);
	    break;
	case O_ARGVP:
	    create_result = (p->create)(NULL, &argv, p->token == N_OK, &newplan, db_search_isoutput, tbl, ctx);
	    break;
	default:
	    bu_vls_free(&name);
	    bu_vls_free(&value);
	    return BRLCAD_OK;
    }

    if (create_result != BRLCAD_OK) {
	bu_vls_free(&name);
	bu_vls_free(&value);
	return BRLCAD_ERROR;
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
static struct db_plan_t *
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
static int
yankexpr(struct db_plan_t **planp, struct db_plan_t **resultplan)          /* pointer to top of plan (modified) */
{
    struct db_plan_t *next;     	/* temp node holding subexpression results */
    struct db_plan_t *node;             /* pointer to returned node or expression */
    struct db_plan_t *tail;             /* pointer to tail of subplan */
    struct db_plan_t *subplan;          /* pointer to head of () expression */
    extern int f_expr(struct db_plan_t *, struct db_node_t *, struct db_i *, struct bu_ptbl *);

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
	    if (yankexpr(planp, &next) != BRLCAD_OK)
		return BRLCAD_ERROR;
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
		node->p_un._p_data[0] = subplan;
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
static int
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
    if (yankexpr(&plan, &expr) != BRLCAD_OK)
	return BRLCAD_ERROR;

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
	if (yankexpr(&plan, &expr) != BRLCAD_OK)
	    return BRLCAD_ERROR;
    }
    (*resultplan) = result;
    return BRLCAD_OK;
}


/*
 * compresses "!" expressions in our search plan.
 */
static int
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
	if (next->type == N_EXPR) {
	    if (not_squish(next->p_un._p_data[0], &(next->p_un._p_data[0])) != BRLCAD_OK) {
		return BRLCAD_ERROR;
	    }
	}

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
	    if (node->type == N_EXPR) {
		if (not_squish(node, &node) != BRLCAD_OK) {
		    return BRLCAD_ERROR;
		}
	    }
	    if (notlevel % 2 != 1)
		next = node;
	    else
		next->p_un._p_data[0] = node;
	}

	/* add the node to our result plan */
	if (result == NULL) {
	    tail = result = next;
	} else {
	    tail->next = next;
	    tail = next;
	}
	if (tail)
	    tail->next = NULL;
    }
    (*resultplan) = result;
    return BRLCAD_OK;
}


/*
 * compresses "-above" expressions in our search plan.
 */
static int
above_squish(struct db_plan_t *plan, struct db_plan_t **resultplan)          /* plan to process */
{
    struct db_plan_t *next;     /* next node being processed */
    struct db_plan_t *node;     /* temporary node used in N_NOT processing */
    struct db_plan_t *tail;     /* pointer to tail of result plan */
    struct db_plan_t *result;   /* pointer to head of result plan */

    tail = result = next = NULL;

    while ((next = yanknode(&plan)) != NULL) {
	/*
	 * if we encounter a (expression) then look for above in the
	 * expr subplan.
	 */
	if (next->type == N_EXPR) {
	    if (above_squish(next->p_un._ab_data[0], &(next->p_un._ab_data[0])) != BRLCAD_OK) {
		return BRLCAD_ERROR;
	    }
	}

	/*
	 * if we encounter an above, then snag the next node and place it
	 * in the not's subplan.
	 */
	if (next->type == N_ABOVE) {

	    node = yanknode(&plan);
	    if (node == NULL) {
		bu_log("-above: no following expression");
		return BRLCAD_ERROR;
	    }
	    if (node->type == N_OR) {
		bu_log("-above: nothing between -above and -o");
		return BRLCAD_ERROR;
	    }
	    {
		struct db_plan_t *above_node = node;
		while (above_node->type == N_ABOVE) {
		    struct db_plan_t *operand = yanknode(&plan);
		    if (!operand) {
			bu_log("-above: no following expression");
			return BRLCAD_ERROR;
		    }
		    if (operand->type == N_OR) {
			bu_log("-above: nothing between -above and -o");
			return BRLCAD_ERROR;
		    }
		    above_node->p_un._ab_data[0] = operand;
		    above_node = operand;
		}
		if (above_node->type == N_EXPR &&
		    above_squish(above_node, &above_node) != BRLCAD_OK)
		    return BRLCAD_ERROR;
	    }
	    /*Made it*/
	    next->p_un._ab_data[0] = node;
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
static int
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
	if (next->type == N_EXPR) {
	    if (below_squish(next->p_un._bl_data[0], &(next->p_un._bl_data[0])) != BRLCAD_OK) {
		return BRLCAD_ERROR;
	    }
	}

	/*
	 * if we encounter a not, then snag the next node and place it
	 * in the not's subplan.
	 */
	if (next->type == N_BELOW) {

	    node = yanknode(&plan);
	    if (node == NULL) {
		bu_log("-below: no following expression");
		return BRLCAD_ERROR;
	    }
	    if (node->type == N_OR) {
		bu_log("-below: nothing between -below and -o");
		return BRLCAD_ERROR;
	    }
	    {
		struct db_plan_t *below_node = node;
		while (below_node->type == N_BELOW) {
		    struct db_plan_t *operand = yanknode(&plan);
		    if (!operand) {
			bu_log("-below: no following expression");
			return BRLCAD_ERROR;
		    }
		    if (operand->type == N_OR) {
			bu_log("-below: nothing between -below and -o");
			return BRLCAD_ERROR;
		    }
		    below_node->p_un._bl_data[0] = operand;
		    below_node = operand;
		}
		if (below_node->type == N_EXPR &&
		    below_squish(below_node, &below_node) != BRLCAD_OK)
		    return BRLCAD_ERROR;
	    }

	    /* Made it */
	    next->p_un._bl_data[0] = node;
	}

	/* add the node to our result plan */
	if (result == NULL) {
	    tail = result = next;
	} else {
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
static int
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
	if (next->type == N_EXPR) {
	    if (or_squish(next->p_un._p_data[0], &(next->p_un._p_data[0])) != BRLCAD_OK) {
		return BRLCAD_ERROR;
	    }
	}

	/* if we encounter a not then look for not's in the subplan */
	if (next->type == N_NOT) {
	    if (or_squish(next->p_un._p_data[0], &(next->p_un._p_data[0])) != BRLCAD_OK) {
		return BRLCAD_ERROR;
	    }
	}

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
	    next->p_un._p_data[0] = result;
	    if (or_squish(plan, &(next->p_un._p_data[1]))  != BRLCAD_OK)
		return BRLCAD_ERROR;
	    if (next->p_un._p_data[1] == NULL) {
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


static struct db_plan_t *
db_search_form_plan(char **argv,
		    int quiet,
		    struct bu_ptbl *tbl,
		    struct _db_search_ctx *ctx)
{
    struct db_plan_t *plan = NULL;
    struct db_plan_t *tail = NULL;
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
	if (find_create(&argv, &newplan, results, &db_search_isoutput, quiet, tbl, ctx) != BRLCAD_OK)
	    return NULL;
	if (!newplan)
	    continue;
	if (plan == NULL) {
	    tail = plan = newplan;
	} else {
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
	    c_print(NULL, NULL, 0, &newplan, &db_search_isoutput, tbl, NULL);
	    plan = newplan;
	} else {
	    c_openparen(NULL, NULL, 0, &newplan, &db_search_isoutput, tbl, NULL);
	    newplan->next = plan;
	    plan = newplan;
	    c_closeparen(NULL, NULL, 0, &newplan, &db_search_isoutput, tbl, NULL);
	    tail->next = newplan;
	    tail = newplan;
	    c_print(NULL, NULL, 0, &newplan, &db_search_isoutput, tbl, NULL);
	    tail->next = newplan;
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

    if (paren_squish(plan, &plan) != BRLCAD_OK)
	return NULL;              /* ()'s */
    if (above_squish(plan, &plan) != BRLCAD_OK)
	return NULL;              /* above's */
    if (below_squish(plan, &plan) != BRLCAD_OK)
	return NULL;              /* below's */
    if (not_squish(plan, &plan) != BRLCAD_OK)
	return NULL;              /* !'s */
    if (or_squish(plan, &plan) != BRLCAD_OK)
	return NULL;              /* -o's */
    return plan;
}


static void
find_execute_plans(struct db_i *dbip, struct bu_ptbl *results, struct db_node_t *db_node, struct db_plan_t *plan)
{
    struct db_plan_t *p;
    for (p = plan; p && (p->eval)(p, db_node, dbip, results); p = p->next)
	;
}


static void
free_exec_plan(struct db_plan_t *splan)
{
    int i;

    if (splan->p_un.ex._e_argv) {
	for (i=0; i < (int)splan->p_un.ex._e_argc; i++) {
	    if (splan->p_un.ex._e_argv[i]) {
		bu_free(splan->p_un.ex._e_argv[i], "e_argv[i]");
		splan->p_un.ex._e_argv[i] = NULL;
	    }
	}
	bu_free(splan->p_un.ex._e_argv, "e_argv");
	splan->p_un.ex._e_argv = NULL;
    }
    if (splan->p_un.ex._e_holes) {
	bu_free(splan->p_un.ex._e_holes, "e_holes");
	splan->p_un.ex._e_holes = NULL;
    }
}


static void
db_search_free_plan(struct db_plan_t *splan)
{
    size_t i = 0;
    struct db_plan_t *p;
    if (splan->plans) {
	struct bu_ptbl *plans = splan->plans;
	for (i = 0; i < BU_PTBL_LEN(plans); i++) {
	    p = (struct db_plan_t *)BU_PTBL_GET(plans, i);
	    if (N_EXEC == p->type) {
		free_exec_plan(p);
	    }
	    if ((N_REGEX == p->type || N_IREGEX == p->type) && p->compiled_regex) {
		regfree((regex_t *)p->compiled_regex);
		bu_free(p->compiled_regex, "compiled search regex");
		p->compiled_regex = NULL;
	    }
	    BU_PUT(p, struct db_plan_t);
	}
    } else {
	struct db_plan_t *plan = splan;
	for (p = plan; p;) {
	    plan = p->next;
	    if (N_EXEC == p->type) {
		free_exec_plan(p);
	    }
	    if ((N_REGEX == p->type || N_IREGEX == p->type) && p->compiled_regex) {
		regfree((regex_t *)p->compiled_regex);
		bu_free(p->compiled_regex, "compiled search regex");
		p->compiled_regex = NULL;
	    }
	    BU_PUT(p, struct db_plan_t);
	    p = plan;
	}
    }
}

/*
 * Collect all N_ABOVE plan nodes from a plan tree into a vector.
 * Used by the -above pre-pass to build per-node reverse-BFS caches.
 *
 * Only nodes with an unbounded depth range (min_depth == 0 and
 * max_depth == INT_MAX) are included.  Depth-constrained nodes (e.g.
 * -above=1, -above<3) are excluded: the upward propagation in the
 * pre-pass would incorrectly mark ancestors at the wrong relative
 * distance.  Those nodes are absent from the cache map and f_above
 * falls back to the O(N) slow path automatically.
 *
 * Does NOT recurse into N_ABOVE inner plans.  If an N_ABOVE node
 * appears as the inner plan of another N_ABOVE (e.g. "-above -above
 * -name X"), the inner node is left out of the cache map.  During the
 * pre-pass the outer node's inner-plan evaluation calls f_above on
 * the inner node; since above_passes_map is NULL for that eval_node,
 * the inner node falls back to the O(N) slow path (which requires a
 * valid full_paths pointer - the pre-pass supplies one).  The outer
 * node still benefits from the O(1) cache lookup; only the pre-pass
 * cost for that outer node is O(N) rather than O(1).  Results are
 * always correct.
 */
static void
collect_above_nodes(struct db_plan_t *plan,
		    std::vector<struct db_plan_t *> &above_nodes)
{
    for (struct db_plan_t *p = plan; p; p = p->next) {
	switch (p->type) {
	    case N_ABOVE:
		/* Exclude depth-constrained and observable variants; they need
		 * the semantics-preserving slow path. */
		if (p->min_depth == 0 && p->max_depth == INT_MAX &&
		    !plan_has_side_effects(p->p_un._ab_data[0]))
		    above_nodes.push_back(p);
		break;
	    case N_EXPR:
	    case N_NOT:
		if (p->p_un._p_data[0])
		    collect_above_nodes(p->p_un._p_data[0], above_nodes);
		break;
	    case N_OR:
		if (p->p_un._p_data[0])
		    collect_above_nodes(p->p_un._p_data[0], above_nodes);
		if (p->p_un._p_data[1])
		    collect_above_nodes(p->p_un._p_data[1], above_nodes);
		break;
	    default:
		break;
	}
    }
}


/*
 * Compute the 128-bit path fingerprint for a complete db_full_path by
 * applying below_path_hash_extend incrementally over every path entry.  The
 * local boolean operation and combination instance number are part of the
 * identity: repeated references to one object may have different predicates.
 * Also returns the parent hash (all but the last element) via *parent_hash_out
 * when parent_hash_out is non-NULL.
 */
static bu_h128_t
above_path_hash_compute(struct db_full_path *path, bu_h128_t *parent_hash_out)
{
    bu_h128_t h = BELOW_PATH_HASH_ROOT;
    for (size_t k = 0; k < path->fp_len; k++) {
	if (parent_hash_out && k + 1 == path->fp_len)
	    *parent_hash_out = h;
	h = below_path_hash_extend(h, path->fp_names[k],
		DB_FULL_PATH_GET_BOOL(path, k),
		DB_FULL_PATH_GET_COMB_INST(path, k));
    }
    return h;
}

void
db_search_free(struct bu_ptbl *search_results)
{
    int i;
    if (!search_results || search_results->l.magic != BU_PTBL_MAGIC)
	return;

    for (i = (int)BU_PTBL_LEN(search_results) - 1; i >= 0; i--) {
	struct db_full_path *path = (struct db_full_path *)BU_PTBL_GET(search_results, i);
	if (path->magic && path->magic == DB_FULL_PATH_MAGIC) {
	    db_free_full_path(path);
	    bu_free(path, "free search path container");
	}
    }
    bu_ptbl_free(search_results);
}


int
db_search(struct bu_ptbl *search_results,
	  int search_flags,
	  const char *plan_str,
	  int input_path_cnt,
	  struct directory **input_paths,
	  struct db_i *dbip,
	  bu_clbk_t clbk,
	  void *u1,
	  void *u2
	  )
{
    int i = 0;
    int result_cnt = 0;
    struct bu_ptbl dbplans = BU_PTBL_INIT_ZERO;
    struct db_plan_t *dbplan = NULL;
    struct directory **top_level_objects = NULL;
    struct directory **paths = input_paths;
    int path_cnt = input_path_cnt;
    struct _db_search_ctx ctx;
    struct plan_analysis_t plan_analysis;
    int has_exec = 0;
    ctx.clbk = clbk;
    ctx.u1 = u1;
    ctx.u2 = u2;

    /* Note that dbplan references strings using memory
     * in the following two objects, so they mustn't be
     * freed until the plan is freed.*/
    char **plan_argv = (char **)bu_calloc(strlen(plan_str) + 1, sizeof(char *), "plan argv");

    /* make a copy so we can mess with it */
    char *mutable_plan_str = bu_strdup(plan_str);


    /* get the plan string into an argv array */
    bu_argv_from_string(&plan_argv[0], strlen(plan_str), mutable_plan_str);
    if (!(search_flags & DB_SEARCH_QUIET)) {
	dbplan = db_search_form_plan(plan_argv, 0, &dbplans, &ctx);
    } else {
	dbplan = db_search_form_plan(plan_argv, 1, &dbplans, &ctx);
    }
    /* No plan, no search */
    if (!dbplan) {
	if (BU_PTBL_LEN(&dbplans) > 0) {
	    struct db_plan_t *allocated_plan =
		(struct db_plan_t *)BU_PTBL_GET(&dbplans, 0);
	    db_search_free_plan(allocated_plan);
	}
	bu_ptbl_free(&dbplans);
	bu_free(mutable_plan_str, "free strdup");
	bu_free((char *)plan_argv, "free plan argv");
	return -1;
    }
    /* If the idea was to test the plan string and we *do* have
     * a plan, return success */
    if (!dbip) {
	db_search_free_plan(dbplan);
	bu_ptbl_free(&dbplans);
	bu_free(mutable_plan_str, "free strdup");
	bu_free((char *)plan_argv, "free plan argv");
	return -2;
    }

    if (!paths) {
	if (search_flags & DB_SEARCH_FLAT) {
	    path_cnt = db_ls(dbip,
		    (search_flags & DB_SEARCH_HIDDEN) ? DB_LS_HIDDEN : 0,
		    NULL, &top_level_objects);
	} else if (search_flags & DB_SEARCH_HIDDEN) {
	    path_cnt = db_ls(dbip, DB_LS_TOPS | DB_LS_HIDDEN, NULL, &top_level_objects);
	} else {
	    path_cnt = db_ls(dbip, DB_LS_TOPS, NULL, &top_level_objects);
	}
	paths = top_level_objects;
    }

    plan_analysis_init(&plan_analysis);
    plan_analysis_update(dbplan, &plan_analysis);
    plan_analysis.has_maxdepth = plan_prune_maxdepth(dbplan, &plan_analysis.maxdepth);
    has_exec = plan_has_exec(dbplan);

    /* execute the plan */
    {
	struct bu_ptbl *full_paths = NULL;
	struct list_client_data_t lcd;
	struct traversal_ctx_t tctx;
	std::vector<struct db_plan_t *> below_nodes;
	typedef std::unordered_set<struct directory *> UniqueDpSet;
	UniqueDpSet unique_dps;
	void *unique_dps_ptr = NULL;

	if (search_results &&
	    (search_flags & (DB_SEARCH_FLAT | DB_SEARCH_RETURN_UNIQ_DP)))
	    unique_dps_ptr = (void *)&unique_dps;

	if (plan_analysis.has_below && !has_exec)
	    collect_below_nodes(dbplan, below_nodes);

	/* First, check if search_results is initialized - don't trust the caller to do it,
	 * but it's fine if they did */
	if (search_results && search_results != BU_PTBL_NULL) {
	    if (!BU_PTBL_IS_INITIALIZED(search_results)) {
		BU_PTBL_INIT(search_results);
	    }
	    if (unique_dps_ptr) {
		unique_dps.reserve((size_t)BU_PTBL_LEN(search_results) +
			(path_cnt > 0 ? (size_t)path_cnt : 0));
		for (size_t ri = 0; ri < (size_t)BU_PTBL_LEN(search_results); ri++)
		    unique_dps.insert((struct directory *)BU_PTBL_GET(search_results, (int)ri));
	    }
	}

	/* Use full-path collection when -above is in the plan (required semantics) */
	if (!(search_flags & DB_SEARCH_FLAT) && plan_analysis.has_above) {
	    BU_ALLOC(full_paths, struct bu_ptbl);
	    BU_PTBL_INIT(full_paths);
	    lcd.dbip = dbip;
	    lcd.full_paths = full_paths;
	    lcd.flags = search_flags;

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

		    bu_ptbl_ins(full_paths, (long *)start_path);
		    db_fullpath_list(start_path, (void **)&lcd);
		}
	    }

	    BelowStateMap below_state_map;
	    build_below_state_map(dbip, &below_nodes, full_paths, search_flags,
		    below_state_map);

	    /*
	     * Build a per-node reverse-BFS cache for -above queries.
	     *
	     * Process paths deepest-first (bottom-up).  For each path P:
	     *   1. Evaluate the inner plan of every N_ABOVE node on P.
	     *   2. If P matches any inner plan, mark P's parent hash in that
	     *      node's cache so the parent is known to be "above" a match.
	     *   3. If P's own hash is already in the cache (propagated from a
	     *      deeper descendant), propagate further to P's parent.
	     *
	     * This converts f_above from O(N) per call to O(1) per call for
	     * the common case, reducing the total cost from O(N²) to O(N·D)
	     * where D is the average path depth.
	     */
	    typedef std::unordered_map<struct db_plan_t *, std::unordered_set<bu_h128_t>> AboveCacheMap;
	    AboveCacheMap above_cache_map;
	    {
		std::vector<struct db_plan_t *> above_nodes;
		if (!has_exec)
		    collect_above_nodes(dbplan, above_nodes);

		if (!above_nodes.empty()) {
		    /* db_fullpath_list produces preorder.  Reverse iteration is
		     * therefore already descendants-before-parents and avoids an
		     * unnecessary O(N log N) depth sort. */
		    size_t nfp = (size_t)BU_PTBL_LEN(full_paths);

		    /* Initialise per-node cache entries. */
		    for (auto *an : above_nodes)
			above_cache_map.emplace(an, std::unordered_set<bu_h128_t>{});

		    /* Bottom-up pass. */
		    for (size_t k = nfp; k-- > 0;) {
			struct db_full_path *fp =
			    (struct db_full_path *)BU_PTBL_GET(full_paths, (int)k);
			if (fp->fp_len == 0) continue;

			bu_h128_t parent_hash = BELOW_PATH_HASH_ROOT;
			bu_h128_t path_hash = above_path_hash_compute(fp, &parent_hash);
			/* fp->fp_len == fp_names array size by db_full_path invariant */
			int has_parent = (fp->fp_len > 1);

			struct db_node_t eval_node;
			eval_node.path = fp;
			eval_node.flags = search_flags;
			/*
			 * Provide full_paths so that any N_ABOVE node that
			 * appears inside the inner plan (e.g. -above -above
			 * -name X) can safely use the O(N) slow path without
			 * a NULL-dereference.  Such nested N_ABOVE nodes are
			 * not in above_cache_map (collect_above_nodes does not
			 * recurse into inner plans), so they always fall back
			 * to the slow path, which needs a valid full_paths ptr.
			 */
			eval_node.full_paths = full_paths;
			eval_node.unique_dps = NULL;
			eval_node.above_passes_map = NULL;
			eval_node.above_path_hash = path_hash;
			auto bsit = below_state_map.find(fp);
			if (bsit != below_state_map.end()) {
			    eval_node.below_passes = bsit->second.passes;
			    eval_node.below_cache_active = bsit->second.active;
			} else {
			    eval_node.below_passes = 0;
			    eval_node.below_cache_active = 0;
			}
			eval_node.matched_filters = 1;

			for (auto *an : above_nodes) {
			    auto &ncache = above_cache_map[an];

			    /* Propagate upward if a deeper descendant already matched. */
			    if (ncache.count(path_hash)) {
				if (has_parent)
				    ncache.insert(parent_hash);
				continue;
			    }

			    /* Test whether this path satisfies the inner plan. */
			    eval_node.matched_filters = 1;
			    int state = find_execute_nested_plans(
				dbip, NULL, &eval_node, an->p_un._ab_data[0]);
			    if (state && has_parent)
				ncache.insert(parent_hash);
			}
		    }
		}
	    }

	    for (i = 0; i < (int)BU_PTBL_LEN(full_paths); i++) {
		struct db_node_t curr_node;
		struct db_full_path *curr_path =
		    (struct db_full_path *)BU_PTBL_GET(full_paths, i);
		curr_node.path = curr_path;
		curr_node.full_paths = full_paths;
		curr_node.unique_dps = unique_dps_ptr;
		curr_node.flags = search_flags;
		auto bsit = below_state_map.find(curr_path);
		if (bsit != below_state_map.end()) {
		    curr_node.below_passes = bsit->second.passes;
		    curr_node.below_cache_active = bsit->second.active;
		} else {
		    curr_node.below_passes = 0;
		    curr_node.below_cache_active = 0;
		}
		curr_node.matched_filters = 1;
		/* Supply the pre-computed cache to f_above. */
		if (!above_cache_map.empty()) {
		    curr_node.above_passes_map = (void *)&above_cache_map;
		    curr_node.above_path_hash =
			above_path_hash_compute(curr_path, NULL);
		} else {
		    curr_node.above_passes_map = NULL;
		    curr_node.above_path_hash = BELOW_PATH_HASH_ROOT;
		}
		find_execute_plans(dbip, search_results, &curr_node, dbplan);
		result_cnt += curr_node.matched_filters;
	    }

	    /* Done with the paths now - we have our answer */
	    db_search_free(full_paths);
	    bu_free(full_paths, "free search container");
	} else {
	    tctx.dbip = dbip;
	    tctx.plan = dbplan;
	    tctx.results = search_results;
	    tctx.below_nodes = &below_nodes;
	    tctx.unique_dps = unique_dps_ptr;
	    tctx.flags = search_flags;
	    tctx.result_cnt = 0;
	    tctx.has_maxdepth = plan_analysis.has_maxdepth;
	    tctx.maxdepth = plan_analysis.maxdepth;

	    traverse_paths(&tctx, paths, path_cnt);
	    result_cnt = tctx.result_cnt;
	}
    }

    db_search_free_plan(dbplan);
    bu_ptbl_free(&dbplans);
    bu_free(mutable_plan_str, "free strdup");
    bu_free((char *)plan_argv, "free plan argv");

    if (top_level_objects) {
	bu_free((void *)top_level_objects, "free tops");
    }

    return result_cnt;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

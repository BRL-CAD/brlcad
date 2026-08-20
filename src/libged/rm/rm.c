/*                            R M . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file libged/rm/rm.c
 *
 * The rm command - UNIX-style object/path delete for .g databases.
 *
 * Supports:
 *   rm  <obj|path> ...     safe delete (must be unreferenced / empty)
 *   rm -f <obj|path> ...   force delete (equivalent to kill; skips safety checks)
 *   rm -r <obj|path> ...   recursive safe delete (entire subtree must be safe)
 *   rm -n <obj|path> ...   dry-run (print what would be deleted)
 *   rm -rf / rm -fr        force top-level delete and safe-delete unshared descendants
 *
 * Operands may be bare object names, /path/to/obj paths, or glob patterns
 * expanded against the database via db_path_glob.
 *
 * Path operands of the form "comb/child" remove only that parent-child
 * instance from the combination tree (analogous to "comb obj.c rm").
 * Use "obj.c/child@N" to remove the Nth same-name child instance.
 *
 * Batch removal of direct combination members is available through
 * "comb obj.c rm child...".
 */

#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/glob.h"
#include "bu/hash.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/vls.h"
#include "raytrace.h"

#include "../ged_private.h"


/* -----------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------- */

struct rm_ref_lists {
    struct bu_ptbl parents;
    struct bu_ptbl children;
};


struct rm_ref_graph {
    struct bu_hash_tbl *refs;
};


struct rm_obj_set {
    struct bu_ptbl objs;
    struct bu_hash_tbl *hash;
};


static void *
_rm_hash_get(struct bu_hash_tbl *hash, struct directory *dp)
{
    if (!hash || !dp)
	return NULL;

    return bu_hash_get(hash, (const uint8_t *)&dp, sizeof(dp));
}


static void
_rm_hash_set(struct bu_hash_tbl *hash, struct directory *dp, void *val)
{
    if (!hash || !dp)
	return;

    (void)bu_hash_set(hash, (const uint8_t *)&dp, sizeof(dp), val);
}


static void
_rm_obj_set_init(struct rm_obj_set *set, size_t hint)
{
    if (!set)
	return;

    bu_ptbl_init(&set->objs, 64, "rm object set");
    set->hash = bu_hash_create((unsigned long)(hint > 64 ? hint * 2 : 128));
}


static void
_rm_obj_set_free(struct rm_obj_set *set)
{
    if (!set)
	return;

    bu_ptbl_free(&set->objs);
    if (set->hash)
	bu_hash_destroy(set->hash);
    set->hash = NULL;
}


static int
_rm_obj_set_has(const struct rm_obj_set *set, struct directory *dp)
{
    if (!set || !dp)
	return 0;

    return _rm_hash_get(set->hash, dp) != NULL;
}


static int
_rm_obj_set_add(struct rm_obj_set *set, struct directory *dp)
{
    if (!set || !dp)
	return 0;

    if (_rm_obj_set_has(set, dp))
	return 0;

    _rm_hash_set(set->hash, dp, dp);
    bu_ptbl_ins(&set->objs, (long *)dp);
    return 1;
}


static void
_rm_ref_graph_init(struct rm_ref_graph *graph, struct db_i *dbip)
{
    size_t hint = 1024;

    if (!graph)
	return;

    if (dbip)
	hint = db_directory_size(dbip) * 2;
    if (hint < 1024)
	hint = 1024;

    graph->refs = bu_hash_create((unsigned long)hint);
}


static struct rm_ref_lists *
_rm_ref_graph_lists(struct rm_ref_graph *graph, struct directory *dp, int create)
{
    struct rm_ref_lists *refs;

    if (!graph || !dp)
	return NULL;

    refs = (struct rm_ref_lists *)_rm_hash_get(graph->refs, dp);
    if (refs || !create)
	return refs;

    BU_GET(refs, struct rm_ref_lists);
    bu_ptbl_init(&refs->parents, 8, "rm ref parents");
    bu_ptbl_init(&refs->children, 8, "rm ref children");
    _rm_hash_set(graph->refs, dp, refs);

    return refs;
}


static void
_rm_ref_graph_free(struct rm_ref_graph *graph)
{
    struct bu_hash_entry *entry = NULL;

    if (!graph || !graph->refs)
	return;

    while ((entry = bu_hash_next(graph->refs, entry)) != NULL) {
	struct rm_ref_lists *refs = (struct rm_ref_lists *)bu_hash_value(entry, NULL);
	if (!refs)
	    continue;
	bu_ptbl_free(&refs->parents);
	bu_ptbl_free(&refs->children);
	BU_PUT(refs, struct rm_ref_lists);
    }

    bu_hash_destroy(graph->refs);
    graph->refs = NULL;
}


static void
_rm_ref_graph_cb(struct db_i *UNUSED(dbip), struct directory *parent_dp,
	struct directory *child_dp, const char *UNUSED(child_name),
	db_op_t UNUSED(op), matp_t UNUSED(mat), void *u_data)
{
    struct rm_ref_graph *graph = (struct rm_ref_graph *)u_data;
    struct rm_ref_lists *parent_refs;
    struct rm_ref_lists *child_refs;

    if (!graph || !parent_dp || !child_dp)
	return;

    parent_refs = _rm_ref_graph_lists(graph, parent_dp, 1);
    child_refs = _rm_ref_graph_lists(graph, child_dp, 1);

    bu_ptbl_ins(&parent_refs->children, (long *)child_dp);
    bu_ptbl_ins(&child_refs->parents, (long *)parent_dp);
}


static int
_rm_ref_graph_build(struct rm_ref_graph *graph, struct db_i *dbip)
{
    if (!graph || !dbip)
	return BRLCAD_ERROR;

    _rm_ref_graph_init(graph, dbip);
    if (db_add_update_nref_clbk(dbip, _rm_ref_graph_cb, (void *)graph) < 0)
	return BRLCAD_ERROR;

    db_update_nref(dbip);
    (void)db_rm_update_nref_clbk(dbip, _rm_ref_graph_cb, (void *)graph);

    return BRLCAD_OK;
}


static int
_rm_has_glob_metachar(const char *str)
{
    if (!str)
	return 0;

    return (strchr(str, '*') || strchr(str, '?') || strchr(str, '['));
}


static int
_rm_is_path_operand(const char *str)
{
    if (!str)
	return 0;

    return strchr(str, '/') != NULL;
}


static int
_rm_is_parent_child_path(const char *str)
{
    const char *slash;

    if (!str)
	return 0;

    slash = strrchr(str, '/');
    return slash != NULL && slash != str && slash[1] != '\0';
}


static int
_rm_is_simple_operand(const char *str)
{
    if (!str || !str[0])
	return 0;

    return !_rm_has_glob_metachar(str) && !_rm_is_path_operand(str);
}


static void
_rm_vls_append_argv(struct bu_vls *vls, int argc, const char *argv[])
{
    int i;

    for (i = 0; i < argc; i++) {
	if (i)
	    bu_vls_putc(vls, ' ');
	bu_vls_strcat(vls, argv[i]);
    }
}


static int
_rm_check_legacy_form(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int i;
    int has_bare_member = 0;
    struct bu_vls legacy_args = BU_VLS_INIT_ZERO;

    if (!gedp || argc < 2 || !_rm_is_simple_operand(argv[0]))
	return 0;

    dp = db_lookup(gedp->dbip, argv[0], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || !(dp->d_flags & RT_DIR_COMB))
	return 0;

    for (i = 1; i < argc; i++) {
	if (_rm_is_simple_operand(argv[i])) {
	    has_bare_member = 1;
	    break;
	}
    }

    if (!has_bare_member)
	return 0;

    _rm_vls_append_argv(&legacy_args, argc, argv);
    bu_vls_printf(gedp->ged_result_str,
		  "rm: ambiguous request: '%s' is a combination followed by bare object names.\n"
		  "rm: no objects were deleted.\n"
		  "rm: to remove members from the combination, use: comb %s rm",
		  argv[0], argv[0]);
    for (i = 1; i < argc; i++)
	bu_vls_printf(gedp->ged_result_str, " %s", argv[i]);
    bu_vls_printf(gedp->ged_result_str,
		  "\nrm: or use explicit paths such as: rm %s/%s\n"
		  "rm: to force object deletion for this command, use: rm -f %s\n",
		  argv[0], argv[1], bu_vls_cstr(&legacy_args));
    bu_vls_free(&legacy_args);

    return 1;
}


/**
 * Return non-zero if obj has any external references (i.e. d_nref > 0).
 * db_update_nref must have been called beforehand.
 */
static int
_rm_is_referenced(struct db_i *dbip, struct directory *dp)
{
    if (!dbip || !dp)
	return 0;
    return (dp->d_nref > 0);
}


static int
_rm_has_external_reference(struct rm_ref_graph *graph, struct rm_obj_set *inside, struct directory *dp)
{
    struct rm_ref_lists *refs;
    size_t i;

    if (!graph || !dp)
	return 0;

    refs = _rm_ref_graph_lists(graph, dp, 0);
    if (!refs)
	return 0;

    for (i = 0; i < BU_PTBL_LEN(&refs->parents); i++) {
	struct directory *parent_dp = (struct directory *)BU_PTBL_GET(&refs->parents, i);
	if (!inside || !_rm_obj_set_has(inside, parent_dp))
	    return 1;
    }

    return 0;
}


static void
_rm_collect_subtree(struct rm_ref_graph *graph, struct rm_obj_set *set, struct directory *dp)
{
    struct rm_ref_lists *refs;
    size_t i;

    if (!graph || !set || !dp)
	return;

    if (!_rm_obj_set_add(set, dp))
	return;

    refs = _rm_ref_graph_lists(graph, dp, 0);
    if (!refs)
	return;

    for (i = 0; i < BU_PTBL_LEN(&refs->children); i++) {
	struct directory *child_dp = (struct directory *)BU_PTBL_GET(&refs->children, i);
	_rm_collect_subtree(graph, set, child_dp);
    }
}


/**
 * Return non-zero if the combination dp has any children.
 */
static int
_rm_comb_has_children(struct db_i *dbip, struct directory *dp)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int has_tree;

    if (!(dp->d_flags & RT_DIR_COMB))
	return 0;

    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL) < 0)
	return 0;

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);
    has_tree = (comb->tree != TREE_NULL);
    rt_db_free_internal(&intern);
    return has_tree;
}


static int
_rm_object_has_children(struct db_i *dbip, struct rm_ref_graph *graph, struct directory *dp)
{
    struct rm_ref_lists *refs;

    if (!dp)
	return 0;

    if (_rm_comb_has_children(dbip, dp))
	return 1;

    refs = _rm_ref_graph_lists(graph, dp, 0);
    return refs && BU_PTBL_LEN(&refs->children) > 0;
}


/**
 * Physically delete one object from the database and erase from display.
 * Returns BRLCAD_OK on success, BRLCAD_ERROR on failure.
 */
static int
_rm_delete_object(struct ged *gedp, struct directory *dp)
{
    _dl_eraseAllNamesFromDisplay(gedp, dp->d_namep, 0);

    if (db_delete(gedp->dbip, dp) != 0 || db_dirdelete(gedp->dbip, dp) != 0) {
	bu_vls_printf(gedp->ged_result_str,
		"rm: error deleting '%s'\n", dp->d_namep);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}


static int
_rm_parse_child_instance(struct bu_vls *child_name, long *instance, const char *expr)
{
    const char *atp;
    const char *cp;
    char *endp = NULL;
    long val;

    if (!child_name || !instance || !expr || !expr[0])
	return BRLCAD_ERROR;

    *instance = -1;
    atp = strrchr(expr, '@');
    if (!atp || !atp[1]) {
	bu_vls_strcpy(child_name, expr);
	return BRLCAD_OK;
    }

    for (cp = atp + 1; *cp; cp++) {
	if (!isdigit((unsigned char)*cp)) {
	    bu_vls_strcpy(child_name, expr);
	    return BRLCAD_OK;
	}
    }

    if (atp == expr)
	return BRLCAD_ERROR;

    errno = 0;
    val = strtol(atp + 1, &endp, 10);
    if (errno || !endp || *endp || val <= 0 || val == LONG_MAX)
	return BRLCAD_ERROR;

    bu_vls_strncpy(child_name, expr, (size_t)(atp - expr));
    *instance = val;

    return BRLCAD_OK;
}


static size_t
_rm_tree_count_named_leaves(const union tree *tp, const char *name)
{
    if (!tp || !name)
	return 0;

    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_DB_LEAF:
	    return BU_STR_EQUAL(tp->tr_l.tl_name, name) ? 1 : 0;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    return _rm_tree_count_named_leaves(tp->tr_b.tb_left, name) +
		_rm_tree_count_named_leaves(tp->tr_b.tb_right, name);
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    return _rm_tree_count_named_leaves(tp->tr_b.tb_left, name);
	default:
	    break;
    }

    return 0;
}


static union tree *
_rm_tree_remove_named_leaf_instance(union tree *tp, const char *child_name,
	long instance, long *seen, int *removed)
{
    union tree *replacement;

    if (!tp || !child_name || !seen || !removed || *removed)
	return tp;

    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_DB_LEAF:
	    if (BU_STR_EQUAL(tp->tr_l.tl_name, child_name)) {
		(*seen)++;
		if (*seen == instance) {
		    *removed = 1;
		    db_free_tree(tp);
		    return TREE_NULL;
		}
	    }
	    return tp;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    tp->tr_b.tb_left = _rm_tree_remove_named_leaf_instance(tp->tr_b.tb_left,
		    child_name, instance, seen, removed);
	    tp->tr_b.tb_right = _rm_tree_remove_named_leaf_instance(tp->tr_b.tb_right,
		    child_name, instance, seen, removed);
	    if (!*removed)
		return tp;
	    if (tp->tr_b.tb_left && tp->tr_b.tb_right)
		return tp;
	    replacement = tp->tr_b.tb_left ? tp->tr_b.tb_left : tp->tr_b.tb_right;
	    tp->tr_b.tb_left = TREE_NULL;
	    tp->tr_b.tb_right = TREE_NULL;
	    BU_PUT(tp, union tree);
	    return replacement;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    tp->tr_b.tb_left = _rm_tree_remove_named_leaf_instance(tp->tr_b.tb_left,
		    child_name, instance, seen, removed);
	    if (*removed && !tp->tr_b.tb_left) {
		BU_PUT(tp, union tree);
		return TREE_NULL;
	    }
	    return tp;
	default:
	    break;
    }

    return tp;
}


static int
_rm_comb_remove_child_instance(struct rt_comb_internal *comb, const char *child_name, long instance)
{
    long seen = 0;
    int removed = 0;

    if (!comb || !comb->tree || !child_name)
	return BRLCAD_ERROR;

    comb->tree = _rm_tree_remove_named_leaf_instance(comb->tree, child_name,
	    instance, &seen, &removed);
    return removed ? BRLCAD_OK : BRLCAD_ERROR;
}


static int
_rm_lookup_child_instance(struct ged *gedp, struct directory *parent_dp,
	const char *child_expr, int fflag, struct directory **child_dpp)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct bu_vls child_name = BU_VLS_INIT_ZERO;
    size_t matches;
    long instance;
    int ret = BRLCAD_OK;

    if (child_dpp)
	*child_dpp = RT_DIR_NULL;

    if (!gedp || !parent_dp || !child_expr || !child_expr[0])
	return BRLCAD_ERROR;

    if (_rm_parse_child_instance(&child_name, &instance, child_expr) != BRLCAD_OK) {
	if (!fflag) {
	    bu_vls_printf(gedp->ged_result_str,
		    "rm: invalid instance specifier '%s' (use @N with N >= 1)\n",
		    child_expr);
	}
	bu_vls_free(&child_name);
	return fflag ? BRLCAD_OK : BRLCAD_ERROR;
    }

    if (!(parent_dp->d_flags & RT_DIR_COMB)) {
	if (!fflag) {
	    bu_vls_printf(gedp->ged_result_str,
		    "rm: '%s' is not a combination\n", parent_dp->d_namep);
	    ret = BRLCAD_ERROR;
	}
	bu_vls_free(&child_name);
	return ret;
    }

    if (rt_db_get_internal(&intern, parent_dp, gedp->dbip, (fastf_t *)NULL) < 0) {
	if (!fflag) {
	    bu_vls_printf(gedp->ged_result_str,
		    "rm: database read error for '%s'\n", parent_dp->d_namep);
	}
	bu_vls_free(&child_name);
	return fflag ? BRLCAD_OK : BRLCAD_ERROR;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    matches = _rm_tree_count_named_leaves(comb->tree, bu_vls_cstr(&child_name));
    if (!matches) {
	if (!fflag) {
	    bu_vls_printf(gedp->ged_result_str,
		    "rm: '%s' is not a member of '%s'\n",
		    bu_vls_cstr(&child_name), parent_dp->d_namep);
	    ret = BRLCAD_ERROR;
	}
	goto out;
    }

    if (instance > 0 && (size_t)instance > matches) {
	if (!fflag) {
	    bu_vls_printf(gedp->ged_result_str,
		    "rm: '%s/%s@%ld' does not exist; %zu instances match\n",
		    parent_dp->d_namep, bu_vls_cstr(&child_name), instance, matches);
	    ret = BRLCAD_ERROR;
	}
	goto out;
    }

    if (child_dpp) {
	*child_dpp = db_lookup(gedp->dbip, bu_vls_cstr(&child_name), LOOKUP_QUIET);
	if (*child_dpp == RT_DIR_NULL && !fflag) {
	    bu_vls_printf(gedp->ged_result_str,
		    "rm: '%s': no such object\n", bu_vls_cstr(&child_name));
	    ret = BRLCAD_ERROR;
	}
    }

out:
    rt_db_free_internal(&intern);
    bu_vls_free(&child_name);
    return ret;
}


static int
_rm_lookup_parent_path(struct ged *gedp, const char *parent_expr, int fflag,
	struct directory **parent_dpp)
{
    char *path;
    char *cp;
    char *slashp;
    struct directory *dp = RT_DIR_NULL;
    int ret = BRLCAD_OK;

    if (parent_dpp)
	*parent_dpp = RT_DIR_NULL;

    if (!gedp || !parent_expr || !parent_expr[0])
	return BRLCAD_ERROR;

    path = bu_strdup(parent_expr);
    cp = path;
    while (*cp == '/')
	cp++;
    if (!*cp) {
	bu_free(path, "rm parent path");
	return BRLCAD_ERROR;
    }

    for (slashp = cp + strlen(cp) - 1; slashp > cp && *slashp == '/'; slashp--)
	*slashp = '\0';

    slashp = strchr(cp, '/');
    if (slashp)
	*slashp = '\0';

    {
	struct bu_vls root_name = BU_VLS_INIT_ZERO;
	long root_instance;

	if (_rm_parse_child_instance(&root_name, &root_instance, cp) != BRLCAD_OK || root_instance > 0) {
	    if (!fflag) {
		bu_vls_printf(gedp->ged_result_str,
			"rm: invalid root path component '%s'\n", cp);
		ret = BRLCAD_ERROR;
	    }
	    bu_vls_free(&root_name);
	    goto out;
	}

	dp = db_lookup(gedp->dbip, bu_vls_cstr(&root_name), LOOKUP_QUIET);
	if (dp == RT_DIR_NULL) {
	    if (!fflag) {
		bu_vls_printf(gedp->ged_result_str,
			"rm: '%s': no such object\n", bu_vls_cstr(&root_name));
		ret = BRLCAD_ERROR;
	    }
	    bu_vls_free(&root_name);
	    goto out;
	}
	bu_vls_free(&root_name);
    }

    while (slashp) {
	struct directory *child_dp = RT_DIR_NULL;

	cp = slashp + 1;
	slashp = strchr(cp, '/');
	if (slashp)
	    *slashp = '\0';

	if (!cp[0])
	    continue;

	if (_rm_lookup_child_instance(gedp, dp, cp, fflag, &child_dp) != BRLCAD_OK) {
	    ret = BRLCAD_ERROR;
	    goto out;
	}

	if (child_dp == RT_DIR_NULL) {
	    ret = fflag ? BRLCAD_OK : BRLCAD_ERROR;
	    goto out;
	}

	dp = child_dp;
    }

    if (parent_dpp)
	*parent_dpp = dp;

out:
    bu_free(path, "rm parent path");
    return ret;
}


/**
 * Remove a child instance from a combination.
 * Returns BRLCAD_OK on success, BRLCAD_ERROR on failure.
 */
static int
_rm_remove_from_comb(struct ged *gedp, struct directory *parent_dp,
	const char *child_expr, const char *path_expr, int fflag, int nflag)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct bu_vls child_name = BU_VLS_INIT_ZERO;
    size_t matches;
    long instance;
    int ret = BRLCAD_OK;

    if (rt_db_get_internal(&intern, parent_dp, gedp->dbip, (fastf_t *)NULL) < 0) {
	bu_vls_printf(gedp->ged_result_str,
		"rm: database read error for '%s'\n", parent_dp->d_namep);
	return BRLCAD_ERROR;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (_rm_parse_child_instance(&child_name, &instance, child_expr) != BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str,
		"rm: invalid instance specifier '%s' (use @N with N >= 1)\n",
		child_expr);
	rt_db_free_internal(&intern);
	bu_vls_free(&child_name);
	return BRLCAD_ERROR;
    }

    matches = _rm_tree_count_named_leaves(comb->tree, bu_vls_cstr(&child_name));
    if (!matches) {
	if (!fflag) {
	    bu_vls_printf(gedp->ged_result_str,
		    "rm: '%s' is not a member of '%s'\n",
		    bu_vls_cstr(&child_name), parent_dp->d_namep);
	    ret = BRLCAD_ERROR;
	}
	rt_db_free_internal(&intern);
	bu_vls_free(&child_name);
	return ret;
    }

    if (instance < 0) {
	if (matches > 1) {
	    bu_vls_printf(gedp->ged_result_str,
		    "rm: '%s/%s' is ambiguous: %zu instances match; use %s/%s@N\n",
		    parent_dp->d_namep, bu_vls_cstr(&child_name), matches,
		    parent_dp->d_namep, bu_vls_cstr(&child_name));
	    rt_db_free_internal(&intern);
	    bu_vls_free(&child_name);
	    return BRLCAD_ERROR;
	}
	instance = 1;
    } else if ((size_t)instance > matches) {
	if (!fflag) {
	    bu_vls_printf(gedp->ged_result_str,
		    "rm: '%s/%s@%ld' does not exist; %zu instances match\n",
		    parent_dp->d_namep, bu_vls_cstr(&child_name), instance, matches);
	    ret = BRLCAD_ERROR;
	}
	rt_db_free_internal(&intern);
	bu_vls_free(&child_name);
	return ret;
    }

    if (nflag) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", path_expr ? path_expr : child_expr);
	rt_db_free_internal(&intern);
	bu_vls_free(&child_name);
	return BRLCAD_OK;
    }

    if (_rm_comb_remove_child_instance(comb, bu_vls_cstr(&child_name), instance) != BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str,
		"rm: failed to remove '%s' from '%s'\n",
		child_expr, parent_dp->d_namep);
	rt_db_free_internal(&intern);
	bu_vls_free(&child_name);
	return BRLCAD_ERROR;
    }

    if (path_expr)
	_dl_eraseAllPathsFromDisplay(gedp, path_expr, 0);
    bu_vls_free(&child_name);

    if (rt_db_put_internal(parent_dp, gedp->dbip, &intern) < 0) {
	bu_vls_printf(gedp->ged_result_str,
		"rm: database write error for '%s'\n", parent_dp->d_namep);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


/* -----------------------------------------------------------------------
 * Glob expansion helper
 * --------------------------------------------------------------------- */

/**
 * Expand a single operand against the database using db_path_glob.
 * Appends results to @a out (a bu_ptbl of bu_vls*).
 * If the operand contains no glob metacharacters, appends it as-is.
 */
static void
_rm_expand_operand(const char *operand, struct db_i *dbip, struct bu_ptbl *out)
{
    struct bu_glob_context *gp;
    int i;

    /* If no metacharacters, pass through verbatim */
    if (!strchr(operand, '*') && !strchr(operand, '?') && !strchr(operand, '[')) {
	struct bu_vls *v;
	BU_GET(v, struct bu_vls);
	bu_vls_init(v);
	bu_vls_strcpy(v, operand);
	bu_ptbl_ins(out, (long *)v);
	return;
    }

    gp = bu_glob_ctx_create();
    db_path_glob(gp, operand, BU_GLOB_NOSORT, dbip);

    for (i = 0; i < gp->gl_pathc; i++) {
	struct bu_vls *v;
	BU_GET(v, struct bu_vls);
	bu_vls_init(v);
	bu_vls_strcpy(v, bu_vls_cstr(gp->gl_pathv[i]));
	bu_ptbl_ins(out, (long *)v);
    }

    bu_glob_ctx_destroy(gp);
}


static void
_rm_free_operands(struct bu_ptbl *operands)
{
    size_t i;

    if (!operands)
	return;

    for (i = 0; i < BU_PTBL_LEN(operands); i++) {
	struct bu_vls *v = (struct bu_vls *)BU_PTBL_GET(operands, i);
	bu_vls_free(v);
	BU_PUT(v, struct bu_vls);
    }
    bu_ptbl_free(operands);
}


static int
_rm_process_path_operand(struct ged *gedp, const char *operand, int fflag, int nflag, int *handled)
{
    const char *slash;
    struct bu_vls parent_name = BU_VLS_INIT_ZERO;
    struct directory *parent_dp;
    const char *child_name;
    int ret = BRLCAD_OK;

    if (handled)
	*handled = 0;

    if (!_rm_is_parent_child_path(operand))
	return BRLCAD_OK;

    slash = strrchr(operand, '/');

    if (handled)
	*handled = 1;

    child_name = slash + 1;
    bu_vls_strncpy(&parent_name, operand, (size_t)(slash - operand));

    if (_rm_lookup_parent_path(gedp, bu_vls_cstr(&parent_name), fflag, &parent_dp) != BRLCAD_OK) {
	bu_vls_free(&parent_name);
	return BRLCAD_ERROR;
    }

    if (parent_dp == RT_DIR_NULL) {
	bu_vls_free(&parent_name);
	return fflag ? BRLCAD_OK : BRLCAD_ERROR;
    }

    if (!(parent_dp->d_flags & RT_DIR_COMB)) {
	if (!fflag) {
	    bu_vls_printf(gedp->ged_result_str,
		    "rm: '%s' is not a combination\n",
		    bu_vls_cstr(&parent_name));
	    ret = BRLCAD_ERROR;
	}
	bu_vls_free(&parent_name);
	return ret;
    }

    ret = _rm_remove_from_comb(gedp, parent_dp, child_name, operand, fflag, nflag);
    bu_vls_free(&parent_name);
    return ret;
}


static int
_rm_is_global_object(struct directory *dp)
{
    if (!dp)
	return 0;

    return dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY && dp->d_minor_type == 0;
}


static int
_rm_plan_recursive(struct ged *gedp, struct rm_ref_graph *graph,
	struct rm_obj_set *delete_set, struct directory *top_dp, int fflag,
	int nflag)
{
    struct rm_obj_set subtree;
    int ret = BRLCAD_OK;
    size_t i;

    _rm_obj_set_init(&subtree, 64);
    _rm_collect_subtree(graph, &subtree, top_dp);

    if (!fflag) {
	for (i = 0; i < BU_PTBL_LEN(&subtree.objs); i++) {
	    struct directory *dp = (struct directory *)BU_PTBL_GET(&subtree.objs, i);
	    if (_rm_has_external_reference(graph, &subtree, dp)) {
		bu_vls_printf(gedp->ged_result_str,
			"rm: '%s' cannot be recursively deleted because '%s' is referenced outside the subtree (use -f to force the top object)\n",
			top_dp->d_namep, dp->d_namep);
		ret = BRLCAD_ERROR;
		goto out;
	    }
	}
    }

    for (i = 0; i < BU_PTBL_LEN(&subtree.objs); i++) {
	struct directory *dp = (struct directory *)BU_PTBL_GET(&subtree.objs, i);
	int safe = !_rm_has_external_reference(graph, &subtree, dp);

	if (fflag && dp == top_dp)
	    safe = 1;

	if (!safe)
	    continue;

	if (nflag) {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", dp->d_namep);
	} else {
	    _rm_obj_set_add(delete_set, dp);
	}
    }

out:
    _rm_obj_set_free(&subtree);
    return ret;
}


static int
_rm_plan_object_operand(struct ged *gedp, struct rm_ref_graph *graph,
	struct rm_obj_set *delete_set, const char *operand, int fflag,
	int rflag, int nflag)
{
    struct directory *dp = db_lookup(gedp->dbip, operand, LOOKUP_QUIET);

    if (dp == RT_DIR_NULL) {
	if (!fflag) {
	    bu_vls_printf(gedp->ged_result_str,
		    "rm: '%s': no such object\n", operand);
	    return BRLCAD_ERROR;
	}
	return BRLCAD_OK;
    }

    if (dp->d_addr == RT_DIR_PHONY_ADDR)
	return BRLCAD_OK;

    if (!fflag && _rm_is_global_object(dp)) {
	bu_vls_printf(gedp->ged_result_str,
		"rm: refusing to delete _GLOBAL (use -f to override)\n");
	return BRLCAD_ERROR;
    }

    if (rflag)
	return _rm_plan_recursive(gedp, graph, delete_set, dp, fflag, nflag);

    if (fflag) {
	if (nflag)
	    bu_vls_printf(gedp->ged_result_str, "%s\n", dp->d_namep);
	else
	    _rm_obj_set_add(delete_set, dp);
	return BRLCAD_OK;
    }

    if (_rm_is_referenced(gedp->dbip, dp)) {
	bu_vls_printf(gedp->ged_result_str,
		"rm: '%s' is referenced by other objects (use -f to force)\n",
		dp->d_namep);
	return BRLCAD_ERROR;
    }

    if (_rm_object_has_children(gedp->dbip, graph, dp)) {
	bu_vls_printf(gedp->ged_result_str,
		"rm: '%s' has child references (use -r or -f)\n",
		dp->d_namep);
	return BRLCAD_ERROR;
    }

    if (nflag)
	bu_vls_printf(gedp->ged_result_str, "%s\n", dp->d_namep);
    else
	_rm_obj_set_add(delete_set, dp);

    return BRLCAD_OK;
}


static int
_rm_execute_delete_set(struct ged *gedp, struct rm_obj_set *delete_set)
{
    int ret = BRLCAD_OK;
    size_t i;

    if (!delete_set)
	return BRLCAD_OK;

    for (i = 0; i < BU_PTBL_LEN(&delete_set->objs); i++) {
	struct directory *dp = (struct directory *)BU_PTBL_GET(&delete_set->objs, i);
	if (dp == RT_DIR_NULL || dp->d_addr == RT_DIR_PHONY_ADDR)
	    continue;
	if (_rm_delete_object(gedp, dp) != BRLCAD_OK)
	    ret = BRLCAD_ERROR;
    }

    return ret;
}


/* -----------------------------------------------------------------------
 * Main command
 * --------------------------------------------------------------------- */

int
ged_rm_core(struct ged *gedp, int argc, const char *argv[])
{
    int fflag = 0;   /* force */
    int rflag = 0;   /* recursive */
    int nflag = 0;   /* dry-run */
    int print_help = 0;
    int ret = BRLCAD_OK;
    int opt_ret;
    size_t i;
    int has_object_operands = 0;
    struct bu_opt_desc d[9];
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct bu_ptbl operands = BU_PTBL_INIT_ZERO;
    struct rm_ref_graph graph;
    struct rm_obj_set delete_set;
    static const char *usage = "Usage: rm [-f | --force] [-r | --recursive] [-n | --dry-run] object|path ...";

    BU_OPT(d[0], "f", "force",     "", NULL, &fflag,      "Force deletion");
    BU_OPT(d[1], "r", "recursive", "", NULL, &rflag,      "Recursively delete unshared descendants");
    BU_OPT(d[2], "n", "dry-run",   "", NULL, &nflag,      "Report what would be deleted without modifying the database");
    /* Preserve legacy uppercase short-option aliases. */
    BU_OPT(d[3], "F", "",          "", NULL, &fflag,      "");
    BU_OPT(d[4], "R", "",          "", NULL, &rflag,      "");
    BU_OPT(d[5], "N", "",          "", NULL, &nflag,      "");
    BU_OPT(d[6], "h", "help",      "", NULL, &print_help, "Print help and exit");
    BU_OPT(d[7], "?", "",          "", NULL, &print_help, "");
    BU_OPT_NULL(d[8]);

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	_ged_cmd_help(gedp, usage, d);
	return GED_HELP;
    }

    opt_ret = bu_opt_parse(&optparse_msg, argc, argv, d);
    if (opt_ret < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&optparse_msg));
	bu_vls_free(&optparse_msg);
	return BRLCAD_ERROR;
    }

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	bu_vls_free(&optparse_msg);
	return GED_HELP;
    }

    /* bu_opt_parse leaves unused argv entries at the front of argv,
     * including the command name in argv[0].  Skip that command token
     * so the remaining argc/argv pair describes only rm operands. */
    argc = opt_ret;
    if (argc > 0) {
	argv++;
	argc--;
    }
    bu_vls_free(&optparse_msg);

    if (argc < 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", usage);
	return BRLCAD_ERROR;
    }

    if (!fflag && _rm_check_legacy_form(gedp, argc, argv))
	return BRLCAD_ERROR;

    /* Expand glob patterns; build flat list of resolved operands */
    bu_ptbl_init(&operands, 64, "rm operands");
    for (i = 0; i < (size_t)argc; i++) {
	_rm_expand_operand(argv[i], gedp->dbip, &operands);
    }

    for (i = 0; i < BU_PTBL_LEN(&operands); i++) {
	struct bu_vls *vop = (struct bu_vls *)BU_PTBL_GET(&operands, i);
	int handled = 0;
	int pret = _rm_process_path_operand(gedp, bu_vls_cstr(vop), fflag, nflag, &handled);

	if (handled && pret != BRLCAD_OK)
	    ret = BRLCAD_ERROR;
	if (!handled)
	    has_object_operands = 1;
    }

    if (!has_object_operands) {
	_rm_free_operands(&operands);
	return ret;
    }

    graph.refs = NULL;
    if (_rm_ref_graph_build(&graph, gedp->dbip) != BRLCAD_OK) {
	_rm_free_operands(&operands);
	bu_vls_printf(gedp->ged_result_str, "rm: failed to build reference graph\n");
	return BRLCAD_ERROR;
    }

    _rm_obj_set_init(&delete_set, BU_PTBL_LEN(&operands));

    for (i = 0; i < BU_PTBL_LEN(&operands); i++) {
	struct bu_vls *vop = (struct bu_vls *)BU_PTBL_GET(&operands, i);
	const char *operand = bu_vls_cstr(vop);

	if (_rm_is_parent_child_path(operand))
	    continue;

	if (_rm_plan_object_operand(gedp, &graph, &delete_set, operand,
		    fflag, rflag, nflag) != BRLCAD_OK)
	    ret = BRLCAD_ERROR;
    }

    if (!nflag && _rm_execute_delete_set(gedp, &delete_set) != BRLCAD_OK)
	ret = BRLCAD_ERROR;

    db_update_nref(gedp->dbip);

    _rm_obj_set_free(&delete_set);
    _rm_ref_graph_free(&graph);
    _rm_free_operands(&operands);

    return ret;
}


#include "../include/plugin.h"

#define GED_RM_COMMANDS(X, XID) \
    X(rm, ged_rm_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_RM_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_rm", 1, GED_RM_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

/*                         L I N T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2018 United States Government as represented by
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
/** @file libged/lint.cpp
 *
 * The lint command for finding and reporting problems in .g files.
 */

#include "common.h"

extern "C" {
#include "bu/opt.h"
#include "raytrace.h"
#include "./ged_private.h"
}

struct _ged_lint_opts {
    int verbosity;
    int cyclic_check;
    int missing_check;
    int non_solid_check;
};

struct _ged_lint_opts *
_ged_lint_opts_create()
{
    struct _ged_lint_opts *o = NULL;
    BU_GET(o, struct _ged_lint_opts);
    o->verbosity = 0;
    o->cyclic_check = 0;
    o->missing_check = 0;
    o->non_solid_check = 0;
    return o;
}

void
_ged_lint_opts_destroy(struct _ged_lint_opts *o)
{
    if (!o) return;
    BU_PUT(o, struct _ged_lint_opts);
}

/* After option processing, turn on all checks
 * unless some specific subset have already
 * been enabled */
void
_ged_lint_opts_verify(struct _ged_lint_opts *o)
{
    int checks_set = 0;
    if (!o) return;
    if (o->cyclic_check) checks_set = 1;
    if (o->missing_check) checks_set = 1;
    if (o->non_solid_check) checks_set = 1;
    if (!checks_set) {
	o->cyclic_check = 1;
	o->missing_check = 1;
	o->non_solid_check = 1;
    }
}

struct _ged_cyclic_data {
    struct ged *gedp;
    struct bu_ptbl *paths;
};

/* Because lint is intended to be a deep inspection of the .g looking for problems,
 * we need to do this check using the low level tree walk rather than operating on
 * a search result set (which has checks to filter out cyclic paths.) */
HIDDEN void
_ged_cyclic_search_subtree(struct db_full_path *path, int curr_bool, union tree *tp,
	void (*traverse_func) (struct db_full_path *path, void *), void *client_data)
{
    struct directory *dp;
    struct _ged_cyclic_data *cdata = (struct _ged_cyclic_data *)client_data;
    struct ged *gedp = cdata->gedp;
    int bool_val = curr_bool;

    if (!tp) {
	return;
    }

    RT_CK_FULL_PATH(path);
    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    if (tp->tr_op == OP_UNION) bool_val = 2;
	    if (tp->tr_op == OP_INTERSECT) bool_val = 3;
	    if (tp->tr_op == OP_SUBTRACT) bool_val = 4;
	    _ged_cyclic_search_subtree(path, bool_val, tp->tr_b.tb_right, traverse_func, client_data);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    _ged_cyclic_search_subtree(path, OP_UNION, tp->tr_b.tb_left, traverse_func, client_data);
	    break;
	case OP_DB_LEAF:
	    if ((dp=db_lookup(gedp->ged_wdbp->dbip, tp->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
		return;
	    } else {
		/* See if we've got a cyclic path */
		db_add_node_to_full_path(path, dp);
		DB_FULL_PATH_SET_CUR_BOOL(path, bool_val);
		if (!db_full_path_cyclic(path, NULL, 0)) {
		    /* Not cyclic - keep going */
		    traverse_func(path, client_data);
		} else {
		    /* Cyclic - report it */
		    char *path_string = db_path_to_string(path);
		    bu_ptbl_ins(cdata->paths, (long *)path_string);
		}
		DB_FULL_PATH_POP(path);
		break;
	    }

	default:
	    bu_log("db_functree_subtree: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("db_functree_subtree: unrecognized operator\n");
    }
}

void
_ged_cyclic_search(struct db_full_path *fp, void *client_data)
{
    struct _ged_cyclic_data *cdata = (struct _ged_cyclic_data *)client_data;
    struct ged *gedp = cdata->gedp;
    struct directory *dp;

    RT_CK_FULL_PATH(fp);

    dp = DB_FULL_PATH_CUR_DIR(fp);

    if (!dp) return;

    /* If we're not looking at a comb object we can't have
     * cyclic paths - else, walk the comb */
    if (dp->d_flags & RT_DIR_COMB) {
	struct rt_db_internal in;
	struct rt_comb_internal *comb;

	if (rt_db_get_internal(&in, dp, gedp->ged_wdbp->dbip, NULL, &rt_uniresource) < 0) return;

	comb = (struct rt_comb_internal *)in.idb_ptr;
	_ged_cyclic_search_subtree(fp, OP_UNION, comb->tree, _ged_cyclic_search, client_data);
	rt_db_free_internal(&in);
    }
}

int
_ged_cyclic_check(struct _ged_cyclic_data *cdata, struct ged *gedp, int argc, struct directory **dpa)
{
    int i;
    struct db_full_path *start_path = NULL;
    int ret = GED_OK;
    if (!gedp) return GED_ERROR;
    if (argc) {
	if (!dpa) return GED_ERROR;
	BU_GET(start_path, struct db_full_path);
	db_full_path_init(start_path);
	for (i = 0; i < argc; i++) {
	    db_add_node_to_full_path(start_path, dpa[i]);
	    _ged_cyclic_search(start_path, (void *)cdata);
	    DB_FULL_PATH_POP(start_path);
	}
    } else {
	struct directory *dp;
	BU_GET(start_path, struct db_full_path);
	db_full_path_init(start_path);
	/* We can't do db_ls to get a set of tops objects here, because a cyclic
	 * path can produce a situation where there are no tops objects and/or
	 * hide the paths we need to report. */
	for (i = 0; i < RT_DBNHASH; i++) {
	    for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		db_add_node_to_full_path(start_path, dp);
		_ged_cyclic_search(start_path, (void *)cdata);
		DB_FULL_PATH_POP(start_path);
	    }
	}
    }
    db_free_full_path(start_path);
    BU_PUT(start_path, struct db_full_path);
    return ret;
}

int
_ged_missing_check(struct ged *gedp, int argc, struct directory **dpa)
{
    int ret = GED_OK;
    if (!gedp) return GED_ERROR;
    if (argc) {
	if (!dpa) return GED_ERROR;
    }	
    return ret;
}

int
_ged_non_solid_check(struct ged *gedp, int argc, struct directory **dpa)
{
    int ret = GED_OK;
    if (!gedp) return GED_ERROR;
    if (argc) {
	if (!dpa) return GED_ERROR;
    }	
    return ret;
}

extern "C" int
ged_lint(struct ged *gedp, int argc, const char *argv[])
{
    size_t pc;
    int ret = GED_OK;
    static const char *usage = "Usage: lint [ -CMS ] [obj1] [obj2] [...]\n";
    int print_help = 0;
    struct _ged_lint_opts *opts;
    struct bu_opt_desc d[6];
    struct directory **dpa = NULL;
    int nonexist_obj_cnt = 0;
    struct _ged_cyclic_data *cdata;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    opts = _ged_lint_opts_create();

    BU_OPT(d[0],  "h", "help",          "",  NULL,  &print_help,               "Print help and exit");
    BU_OPT(d[1],  "v", "verbose",       "",  &_ged_vopt,  &(opts->verbosity),  "Verbose output (multiple flags increase verbosity)");
    BU_OPT(d[2],  "C", "cyclic",        "",  NULL,  &(opts->cyclic_check),     "Check for cyclic paths (combs whose children reference their parents - potential for infinite looping)");
    BU_OPT(d[3],  "M", "missing",       "",  NULL,  &(opts->missing_check),    "Check for objects reference by combs that are not in the database");
    BU_OPT(d[4],  "S", "non-solid",     "",  NULL,  &(opts->non_solid_check),  "Check for objects that are intended to be solid but do not satisfy that criteria (for example, non-solid BoTs)");
    BU_OPT_NULL(d[5]);

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* parse standard options */
    argc = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	ret = GED_OK;
	goto ged_lint_memfree;
    }

    if (argc) {
	dpa = (struct directory **)bu_calloc(argc+1, sizeof(struct directory *), "dp array");
	nonexist_obj_cnt = _ged_sort_existing_objs(gedp, argc, argv, dpa);
	if (nonexist_obj_cnt) {
	    int i;
	    bu_vls_printf(gedp->ged_result_str, "Object argument(s) supplied to lint that do not exist in the database:\n");
	    for (i = argc - nonexist_obj_cnt - 1; i < argc; i++) {
		bu_vls_printf(gedp->ged_result_str, " %s\n", argv[i]);
	    }
	    ret = GED_ERROR;
	    goto ged_lint_memfree;
	}
    }

    _ged_lint_opts_verify(opts);

    if (opts->cyclic_check) {
	bu_log("Checking for cyclic paths...\n");
	BU_GET(cdata, struct _ged_cyclic_data);
	cdata->gedp = gedp;
	BU_GET(cdata->paths, struct bu_ptbl);
	bu_ptbl_init(cdata->paths, 0, "path table");
	ret = _ged_cyclic_check(cdata, gedp, argc, dpa);
	if (ret != GED_OK) {
	    goto ged_lint_memfree;
	}
	if (BU_PTBL_LEN(cdata->paths)) {
	    bu_vls_printf(gedp->ged_result_str, "Found cyclic paths:\n");
	    for (pc = 0; pc < BU_PTBL_LEN(cdata->paths); pc++) {
		char *path = (char *)BU_PTBL_GET(cdata->paths, pc);
		bu_vls_printf(gedp->ged_result_str, "  %s\n", path);
	    }
	}
    }

    if (opts->missing_check) {
	ret = _ged_missing_check(gedp, argc, dpa);
	if (ret != GED_OK) {
	    goto ged_lint_memfree;
	}
    }

    if (opts->non_solid_check) {
	ret = _ged_non_solid_check(gedp, argc, dpa);
	if (ret != GED_OK) {
	    goto ged_lint_memfree;
	}
    }

ged_lint_memfree:
    _ged_lint_opts_destroy(opts);
    for (pc = 0; pc < BU_PTBL_LEN(cdata->paths); pc++) {
	char *path = (char *)BU_PTBL_GET(cdata->paths, pc);
	bu_free(path, "free cyclic path");
    }
    bu_ptbl_free(cdata->paths);
    BU_PUT(cdata->paths, struct bu_ptbl);
    BU_PUT(cdata, struct _ged_cyclic_data);
    if (dpa) bu_free(dpa, "dp array");

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

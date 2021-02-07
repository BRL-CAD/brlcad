/*                         L I N T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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

#include <set>
#include <map>
#include <string>

extern "C" {
#include "bu/opt.h"
#include "bg/trimesh.h"
#include "raytrace.h"
#include "../ged_private.h"
}

struct _ged_lint_opts {
    int verbosity;
    int cyclic_check;
    int missing_check;
    int invalid_shape_check;
    struct bu_vls filter;
};

struct _ged_lint_opts *
_ged_lint_opts_create()
{
    struct _ged_lint_opts *o = NULL;
    BU_GET(o, struct _ged_lint_opts);
    o->verbosity = 0;
    o->cyclic_check = 0;
    o->missing_check = 0;
    o->invalid_shape_check = 0;
    o->filter = BU_VLS_INIT_ZERO;
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
    if (o->invalid_shape_check) checks_set = 1;
    if (!checks_set) {
	o->cyclic_check = 1;
	o->missing_check = 1;
	o->invalid_shape_check = 1;
    }
}

struct _ged_cyclic_data {
    struct ged *gedp;
    struct bu_ptbl *paths;
};


struct _ged_missing_data {
    struct ged *gedp;
    std::set<std::string> missing;
};

struct invalid_obj {
    std::string name;
    std::string type;
    std::string error;
};

struct _ged_invalid_data {
    struct ged *gedp;
    struct _ged_lint_opts *o;
    std::set<struct directory *> invalid_dps;
    std::map<struct directory *, struct invalid_obj> invalid_msgs;
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
	    dp = db_lookup(gedp->ged_wdbp->dbip, tp->tr_l.tl_name, LOOKUP_QUIET);
	    if (dp == RT_DIR_NULL) {
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

HIDDEN void
_ged_lint_comb_find_missing(struct _ged_missing_data *mdata, const char *parent, struct db_i *dbip, union tree *tp)
{
    if (!tp) return;

    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	    _ged_lint_comb_find_missing(mdata, parent, dbip, tp->tr_b.tb_right);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    _ged_lint_comb_find_missing(mdata, parent, dbip, tp->tr_b.tb_left);
	    break;
	case OP_DB_LEAF:
	    if (db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_QUIET) == RT_DIR_NULL) {
		std::string inst = std::string(parent) + std::string("/") + std::string(tp->tr_l.tl_name);
		mdata->missing.insert(inst);
	    }
	    break;
	default:
	    bu_log("_ged_lint_comb_find_invalid: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("_ged_lint_comb_find_invalid\n");
    }
}

HIDDEN void
_ged_lint_shape_find_missing(struct _ged_missing_data *mdata, struct db_i *dbip, struct directory *dp)
{
    struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
    struct db5_raw_internal raw;
    unsigned char *cp;
    char datasrc;
    char *sketch_name;
    struct bu_vls dsp_name = BU_VLS_INIT_ZERO;

    if (!mdata || !dbip || !dp) return;

    switch (dp->d_minor_type) {
   	case DB5_MINORTYPE_BRLCAD_DSP:
	    /* For DSP we can't do a full import up front, since the potential invalidity we're looking to detect will cause
	     * the import to fail.  Partially crack it, enough to where we can get what we need */
	    if (db_get_external(&ext, dp, dbip) < 0) return;
	    if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) == NULL) {
		bu_free_external(&ext);
		return;
	    }
	    cp = (unsigned char *)raw.body.ext_buf;
	    cp += 2*SIZEOF_NETWORK_LONG + SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_MAT + SIZEOF_NETWORK_SHORT;
	    datasrc = *cp;
	    cp++; cp++;
	    bu_vls_strncpy(&dsp_name, (char *)cp, raw.body.ext_nbytes - (cp - (unsigned char *)raw.body.ext_buf));
	    if (datasrc == RT_DSP_SRC_OBJ) {
		if (db_lookup(dbip, bu_vls_addr(&dsp_name), LOOKUP_QUIET) == RT_DIR_NULL) {
		    std::string inst = std::string(dp->d_namep) + std::string("/") + std::string(bu_vls_addr(&dsp_name));
		    mdata->missing.insert(inst);
		}
	    }
	    if (datasrc == RT_DSP_SRC_FILE) {
		if (!bu_file_exists(bu_vls_addr(&dsp_name), NULL)) {
		    std::string inst = std::string(dp->d_namep) + std::string("/") + std::string(bu_vls_addr(&dsp_name));
		    mdata->missing.insert(inst);
		}
	    }
	    bu_vls_free(&dsp_name);
	    bu_free_external(&ext);
	    break;
	case DB5_MINORTYPE_BRLCAD_EXTRUDE:
	    /* For EXTRUDE we can't do a full import up front, since the potential invalidity we're looking to detect will cause
	     * the import to fail.  Partially crack it, enough to where we can get what we need */
	    if (db_get_external(&ext, dp, dbip) < 0) return;
            if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) == NULL) {
                bu_free_external(&ext);
                return;
            }
            cp = (unsigned char *)raw.body.ext_buf;
	    sketch_name = (char *)cp + ELEMENTS_PER_VECT*4*SIZEOF_NETWORK_DOUBLE + SIZEOF_NETWORK_LONG;
	    if (db_lookup(dbip, sketch_name, LOOKUP_QUIET) == RT_DIR_NULL) {
		std::string inst = std::string(dp->d_namep) + std::string("/") + std::string(sketch_name);
		mdata->missing.insert(inst);
	    }
	    bu_free_external(&ext);
	    break;
	default:
	    break;
    }
}

int
_ged_missing_check(struct _ged_missing_data *mdata, struct ged *gedp, int argc, struct directory **dpa)
{
    struct directory *dp;
    int ret = GED_OK;
    if (!gedp) return GED_ERROR;
    if (argc) {
	unsigned int i;
	struct bu_ptbl *pc = NULL;
	const char *osearch = "-type comb";
	if (!dpa) return GED_ERROR;
	BU_ALLOC(pc, struct bu_ptbl);
	if (db_search(pc, DB_SEARCH_RETURN_UNIQ_DP, osearch, argc, dpa, gedp->ged_wdbp->dbip, NULL) < 0) {
	    ret = GED_ERROR;
	    bu_ptbl_free(pc);
	    bu_free(pc, "pc table");
	} else {
	    for (i = 0; i < BU_PTBL_LEN(pc); i++) {
		dp = (struct directory *)BU_PTBL_GET(pc, i);
		if (dp->d_flags & RT_DIR_COMB) {
		    struct rt_db_internal in;
		    struct rt_comb_internal *comb;
		    if (rt_db_get_internal(&in, dp, gedp->ged_wdbp->dbip, NULL, &rt_uniresource) < 0) continue;
		    comb = (struct rt_comb_internal *)in.idb_ptr;
		    _ged_lint_comb_find_missing(mdata, dp->d_namep, gedp->ged_wdbp->dbip, comb->tree);
		} else {
		    _ged_lint_shape_find_missing(mdata, gedp->ged_wdbp->dbip, dp);
		}
	    }
	    bu_ptbl_free(pc);
	    bu_free(pc, "pc table");
	}
    } else {
	int i;
	for (i = 0; i < RT_DBNHASH; i++) {
	    for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		if (dp->d_flags & RT_DIR_COMB) {
		    struct rt_db_internal in;
		    struct rt_comb_internal *comb;
		    if (rt_db_get_internal(&in, dp, gedp->ged_wdbp->dbip, NULL, &rt_uniresource) < 0) continue;
		    comb = (struct rt_comb_internal *)in.idb_ptr;
		    _ged_lint_comb_find_missing(mdata, dp->d_namep, gedp->ged_wdbp->dbip, comb->tree);
		} else {
		    _ged_lint_shape_find_missing(mdata, gedp->ged_wdbp->dbip, dp);
		}
	    }
	}
    }
    return ret;
}

/* Someday, when we have parametric constraint evaluation for parameters for primitives, we can hook
 * that into this logic as well... for now, run various special-case routines that are available to
 * spot various categories of problematic primtivies. */
void
_ged_invalid_prim_check(struct _ged_invalid_data *idata, struct ged *gedp, struct directory *dp)
{
    struct invalid_obj obj;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct bu_vls vlog = BU_VLS_INIT_ZERO;
    int not_valid = 0;
    if (!idata || !gedp || !dp) return;

    if (dp->d_flags & RT_DIR_HIDDEN) return;
    if (dp->d_addr == RT_DIR_PHONY_ADDR) return;

    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) return;
    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD) {
	rt_db_free_internal(&intern);
	return;
    }

    switch (dp->d_minor_type) {
	case DB5_MINORTYPE_BRLCAD_BOT:
	    bot = (struct rt_bot_internal *)intern.idb_ptr;
	    RT_BOT_CK_MAGIC(bot);
	    if (bot->mode == RT_BOT_SOLID) {
		not_valid = bg_trimesh_solid2((int)bot->num_vertices, (int)bot->num_faces, bot->vertices, bot->faces, NULL);
		if (not_valid) {
		    obj.name = std::string(dp->d_namep);
		    obj.type= std::string("bot");
		    obj.error = std::string("failed solidity test, but BoT type is RT_BOT_SOLID");
		}
	    }
	    rt_db_free_internal(&intern);
	    break;
	case DB5_MINORTYPE_BRLCAD_BREP:
	    not_valid = !rt_brep_valid(&vlog, &intern, 0);
	    if (not_valid) {
		obj.name = std::string(dp->d_namep);
		obj.type= std::string("brep");
		if (idata->o->verbosity) {
		    obj.error = std::string(bu_vls_addr(&vlog));
		} else {
		    obj.error = std::string("failed OpenNURBS validity test");
		}
	    }
	    break;
	case DB5_MINORTYPE_BRLCAD_ARB8:
	    // TODO - check for twisted arbs.
	    break;
	case DB5_MINORTYPE_BRLCAD_DSP:
	    // TODO - check for empty data object and zero length dimension vectors.
	    break;
	case DB5_MINORTYPE_BRLCAD_EXTRUDE:
	    // TODO - check for empty sketch and zero length dimension vectors.
	    break;
	default:
	    break;
    }

    if (not_valid) {
	idata->invalid_dps.insert(dp);
	idata->invalid_msgs.insert(std::pair<struct directory *, struct invalid_obj>(dp, obj));
    }

    bu_vls_free(&vlog);
}

int
_ged_invalid_shape_check(struct _ged_invalid_data *idata, struct ged *gedp, int argc, struct directory **dpa)
{
    int ret = GED_OK;
    struct directory *dp;
    struct _ged_lint_opts *opts = (struct _ged_lint_opts *)idata->o;
    unsigned int i;
    struct bu_ptbl *pc = NULL;
    struct bu_vls sopts = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sopts, "! -type comb %s", bu_vls_cstr(&opts->filter));
    BU_ALLOC(pc, struct bu_ptbl);
    if (db_search(pc, DB_SEARCH_RETURN_UNIQ_DP, bu_vls_cstr(&sopts), argc, dpa, gedp->ged_wdbp->dbip, NULL) < 0) {
	ret = GED_ERROR;
	bu_free(pc, "pc table");
    } else {
	for (i = 0; i < BU_PTBL_LEN(pc); i++) {
	    dp = (struct directory *)BU_PTBL_GET(pc, i);
	    _ged_invalid_prim_check(idata, gedp, dp);
	}
	bu_ptbl_free(pc);
	bu_free(pc, "pc table");
    }
    bu_vls_free(&sopts);
    return ret;
}

extern "C" int
ged_lint_core(struct ged *gedp, int argc, const char *argv[])
{
    size_t pc;
    int ret = GED_OK;
    static const char *usage = "Usage: lint [ -CMS ] [obj1] [obj2] [...]\n";
    int print_help = 0;
    struct _ged_lint_opts *opts;
    struct directory **dpa = NULL;
    int nonexist_obj_cnt = 0;
    struct _ged_cyclic_data *cdata = NULL;
    struct _ged_missing_data *mdata = new struct _ged_missing_data;
    struct _ged_invalid_data *idata = new struct _ged_invalid_data;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    opts = _ged_lint_opts_create();

    struct bu_opt_desc d[7];
    BU_OPT(d[0],  "h", "help",          "",  NULL,  &print_help,               "Print help and exit");
    BU_OPT(d[1],  "v", "verbose",       "",  &_ged_vopt,  &(opts->verbosity),  "Verbose output (multiple flags increase verbosity)");
    BU_OPT(d[2],  "C", "cyclic",        "",  NULL,  &(opts->cyclic_check),     "Check for cyclic paths (combs whose children reference their parents - potential for infinite looping)");
    BU_OPT(d[3],  "M", "missing",       "",  NULL,  &(opts->missing_check),    "Check for objects reference by combs that are not in the database");
    BU_OPT(d[4],  "I", "invalid-shape", "",  NULL,  &(opts->invalid_shape_check),  "Check for objects that are intended to be valid shapes but do not satisfy that criteria (examples include non-solid BoTs and twisted arbs)");
    BU_OPT(d[5],  "F", "filter",        "",  &bu_opt_vls,  &(opts->filter),  "For checks on existing geometry objects, apply search-style filters to check only the subset of objects that satisfy the filters. In particular these filters do NOT impact cyclic and missing geometry checks.");
    BU_OPT_NULL(d[6]);

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
	bu_log("Checking for references to non-extant objects...\n");
	mdata->gedp = gedp;
	ret = _ged_missing_check(mdata, gedp, argc, dpa);
	if (ret != GED_OK) {
	    goto ged_lint_memfree;
	}
	if (mdata->missing.size()) {
	    std::set<std::string>::iterator s_it;
	    bu_vls_printf(gedp->ged_result_str, "Found references to missing objects:\n");
	    for (s_it = mdata->missing.begin(); s_it != mdata->missing.end(); s_it++) {
		std::string mstr = *s_it;
		bu_vls_printf(gedp->ged_result_str, "  %s\n", mstr.c_str());
	    }
	}
    }

    if (opts->invalid_shape_check) {
	bu_log("Checking for invalid objects...\n");
	idata->o = opts;
	ret = _ged_invalid_shape_check(idata, gedp, argc, dpa);
	if (ret != GED_OK) {
	    goto ged_lint_memfree;
	}
	if (idata->invalid_dps.size()) {
	    std::set<struct directory *>::iterator d_it;
	    bu_vls_printf(gedp->ged_result_str, "Found invalid objects:\n");
	    for (d_it = idata->invalid_dps.begin(); d_it != idata->invalid_dps.end(); d_it++) {
		struct directory *dp = *d_it;
		struct invalid_obj msgdata = idata->invalid_msgs.find(dp)->second;
		std::string msg = msgdata.name + std::string("[") + msgdata.type + std::string("] ") + msgdata.error;
		bu_vls_printf(gedp->ged_result_str, "  %s\n", msg.c_str());
	    }
	}

    }

ged_lint_memfree:
    _ged_lint_opts_destroy(opts);
    if (cdata) {
	for (pc = 0; pc < BU_PTBL_LEN(cdata->paths); pc++) {
	    char *path = (char *)BU_PTBL_GET(cdata->paths, pc);
	    bu_free(path, "free cyclic path");
	}
	bu_ptbl_free(cdata->paths);
	BU_PUT(cdata->paths, struct bu_ptbl);
	BU_PUT(cdata, struct _ged_cyclic_data);
    }

    delete mdata;

    if (dpa) bu_free(dpa, "dp array");

    return ret;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl lint_cmd_impl = { "lint", ged_lint_core, GED_CMD_DEFAULT };
    const struct ged_cmd lint_cmd = { &lint_cmd_impl };
    const struct ged_cmd *lint_cmds[] = { &lint_cmd,  NULL };

    static const struct ged_plugin pinfo = { GED_API,  lint_cmds, 1 };

    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
    {
	return &pinfo;
    }
}
#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

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
 *   rm -r <obj|path> ...   recursive safe delete (subtree, skip externally-referenced)
 *   rm -n <obj|path> ...   dry-run (print what would be deleted)
 *   rm -rf / rm -fr        force + recursive
 *
 * Operands may be bare object names, /path/to/obj paths, or glob patterns
 * expanded against the database via db_path_glob.
 *
 * Path operands of the form "comb/child" remove only that parent-child
 * instance from the combination tree (analogous to the legacy "remove" command).
 *
 * Note: the legacy "remove comb child..." command is still available as the
 * "remove" command in src/libged/remove/remove.c.
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/glob.h"
#include "bu/opt.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/search.h"

#include "../ged_private.h"


/* -----------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------- */

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


/**
 * Remove a child instance from a combination.
 * Returns BRLCAD_OK on success, BRLCAD_ERROR on failure.
 */
static int
_rm_remove_from_comb(struct ged *gedp, struct directory *parent_dp, const char *child_name)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct bu_vls path = BU_VLS_INIT_ZERO;

    if (rt_db_get_internal(&intern, parent_dp, gedp->dbip, (fastf_t *)NULL) < 0) {
	bu_vls_printf(gedp->ged_result_str,
		"rm: database read error for '%s'\n", parent_dp->d_namep);
	return BRLCAD_ERROR;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (db_tree_rm_dbleaf(&(comb->tree), child_name, 0) < 0) {
	bu_vls_printf(gedp->ged_result_str,
		"rm: failed to remove '%s' from '%s'\n",
		child_name, parent_dp->d_namep);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(&path, "%s/%s", parent_dp->d_namep, child_name);
    _dl_eraseAllPathsFromDisplay(gedp, bu_vls_cstr(&path), 0);
    bu_vls_free(&path);

    if (rt_db_put_internal(parent_dp, gedp->dbip, &intern) < 0) {
	bu_vls_printf(gedp->ged_result_str,
		"rm: database write error for '%s'\n", parent_dp->d_namep);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


/* -----------------------------------------------------------------------
 * Recursive delete helpers
 * --------------------------------------------------------------------- */

struct rm_rt_data {
    struct ged *gedp;
    const char *top;        /* top-level object being deleted */
    int nflag;              /* dry-run only */
    int errcount;
};

/**
 * db_treewalk_basic callback that deletes a node if it has no external refs.
 */
static void
_rm_recurse_cb(struct db_i *dbip, struct directory *dp, void *ptr)
{
    struct rm_rt_data *rd = (struct rm_rt_data *)ptr;
    struct bu_vls srch = BU_VLS_INIT_ZERO;
    int ref_outside;

    if (dbip == DBI_NULL)
	return;

    /* Check for references outside the subtree being deleted */
    bu_vls_printf(&srch, "-depth >0 -not -below -name %s -name %s",
	    rd->top, dp->d_namep);
    ref_outside = db_search(NULL, DB_SEARCH_TREE, bu_vls_cstr(&srch),
	    0, NULL, dbip, NULL, NULL, NULL);
    bu_vls_free(&srch);

    if (ref_outside) {
	/* Externally referenced - skip silently (safe delete) */
	return;
    }

    if (rd->nflag) {
	bu_vls_printf(rd->gedp->ged_result_str, "%s\n", dp->d_namep);
    } else {
	if (_rm_delete_object(rd->gedp, dp) != BRLCAD_OK)
	    rd->errcount++;
    }
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
    struct bu_opt_desc d[9];
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct bu_ptbl operands = BU_PTBL_INIT_ZERO;
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

    /* Update references so nref counts are current */
    db_update_nref(gedp->dbip);

    /* Expand glob patterns; build flat list of resolved operands */
    bu_ptbl_init(&operands, 64, "rm operands");
    for (i = 0; i < (size_t)argc; i++) {
	_rm_expand_operand(argv[i], gedp->dbip, &operands);
    }

    /* Process each resolved operand */
    for (i = 0; i < BU_PTBL_LEN(&operands); i++) {
	struct bu_vls *vop = (struct bu_vls *)BU_PTBL_GET(&operands, i);
	const char *operand = bu_vls_cstr(vop);
	const char *slash;

	/* ----------------------------------------------------------------
	 * Path operand: "parent/child" -> remove instance from combination
	 * ---------------------------------------------------------------- */
	slash = strrchr(operand, '/');
	if (slash != NULL && slash != operand) {
	    struct bu_vls parent_name = BU_VLS_INIT_ZERO;
	    struct directory *parent_dp;
	    const char *child_name = slash + 1;

	    bu_vls_strncpy(&parent_name, operand, (size_t)(slash - operand));

	    parent_dp = db_lookup(gedp->dbip, bu_vls_cstr(&parent_name), LOOKUP_QUIET);
	    if (parent_dp == RT_DIR_NULL) {
		if (!fflag) {
		    bu_vls_printf(gedp->ged_result_str,
			    "rm: '%s': no such combination\n",
			    bu_vls_cstr(&parent_name));
		    ret = BRLCAD_ERROR;
		}
		bu_vls_free(&parent_name);
		continue;
	    }

	    if (!(parent_dp->d_flags & RT_DIR_COMB)) {
		bu_vls_printf(gedp->ged_result_str,
			"rm: '%s' is not a combination\n",
			bu_vls_cstr(&parent_name));
		bu_vls_free(&parent_name);
		ret = BRLCAD_ERROR;
		continue;
	    }

	    if (nflag) {
		bu_vls_printf(gedp->ged_result_str, "%s\n", operand);
		bu_vls_free(&parent_name);
		continue;
	    }

	    if (_rm_remove_from_comb(gedp, parent_dp, child_name) != BRLCAD_OK)
		ret = BRLCAD_ERROR;

	    bu_vls_free(&parent_name);
	    continue;
	}

	/* ----------------------------------------------------------------
	 * Object operand
	 * ---------------------------------------------------------------- */
	{
	    struct directory *dp = db_lookup(gedp->dbip, operand, LOOKUP_QUIET);

	    if (dp == RT_DIR_NULL) {
		if (!fflag) {
		    bu_vls_printf(gedp->ged_result_str,
			    "rm: '%s': no such object\n", operand);
		    ret = BRLCAD_ERROR;
		}
		continue;
	    }

	    /* Don't delete phony objects */
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;

	    /* Guard against accidental _GLOBAL deletion */
	    if (!fflag &&
		    dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY &&
		    dp->d_minor_type == 0) {
		bu_vls_printf(gedp->ged_result_str,
			"rm: refusing to delete _GLOBAL (use -f to override)\n");
		ret = BRLCAD_ERROR;
		continue;
	    }

	    /* ----- recursive delete ----- */
	    if (rflag) {
		struct rm_rt_data rd;
		rd.gedp    = gedp;
		rd.top     = dp->d_namep;
		rd.nflag   = nflag;
		rd.errcount = 0;

		if (nflag)
		    bu_vls_printf(gedp->ged_result_str, "%s\n", dp->d_namep);

		db_treewalk_basic(gedp->dbip, dp,
			_rm_recurse_cb, _rm_recurse_cb, (void *)&rd);

		if (rd.errcount)
		    ret = BRLCAD_ERROR;

		/* Delete the top node itself if safe (or forced) */
		if (!nflag) {
		    /* Re-fetch nref after subtree has been removed */
		    db_update_nref(gedp->dbip);
		    dp = db_lookup(gedp->dbip, operand, LOOKUP_QUIET);
		    if (dp != RT_DIR_NULL) {
			if (fflag || !_rm_is_referenced(gedp->dbip, dp)) {
			    if (_rm_delete_object(gedp, dp) != BRLCAD_OK)
				ret = BRLCAD_ERROR;
			}
		    }
		}
		continue;
	    }

	    /* ----- force delete (like kill) ----- */
	    if (fflag) {
		if (nflag) {
		    bu_vls_printf(gedp->ged_result_str, "%s\n", dp->d_namep);
		} else {
		    if (_rm_delete_object(gedp, dp) != BRLCAD_OK)
			ret = BRLCAD_ERROR;
		}
		continue;
	    }

	    /* ----- safe delete ----- */
	    if (_rm_is_referenced(gedp->dbip, dp)) {
		bu_vls_printf(gedp->ged_result_str,
			"rm: '%s' is referenced by other objects (use -f to force)\n",
			dp->d_namep);
		ret = BRLCAD_ERROR;
		continue;
	    }

	    if (_rm_comb_has_children(gedp->dbip, dp)) {
		bu_vls_printf(gedp->ged_result_str,
			"rm: '%s' is a non-empty combination (use -r or -f)\n",
			dp->d_namep);
		ret = BRLCAD_ERROR;
		continue;
	    }

	    if (nflag) {
		bu_vls_printf(gedp->ged_result_str, "%s\n", dp->d_namep);
	    } else {
		if (_rm_delete_object(gedp, dp) != BRLCAD_OK)
		    ret = BRLCAD_ERROR;
	    }
	}
    }

    /* Free expanded operand list */
    for (i = 0; i < BU_PTBL_LEN(&operands); i++) {
	struct bu_vls *v = (struct bu_vls *)BU_PTBL_GET(&operands, i);
	bu_vls_free(v);
	BU_PUT(v, struct bu_vls);
    }
    bu_ptbl_free(&operands);

    db_update_nref(gedp->dbip);

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

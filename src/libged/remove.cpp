/*                         R E M O V E . C
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
/** @file libged/remove.c
 *
 * The remove command.
 *
 */

#include "common.h"

#include <queue>
#include <set>

#include <string.h>

#include "bu/cmd.h"
#include "bu/opt.h"
#include "bu/ptbl.h"
#include "bu/vls.h"

#include "./ged_private.h"

HIDDEN void
_remove_show_help(struct ged *gedp, const char *cmd, struct bu_opt_desc *d)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    const char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(&str, "Usage: %s [options] [comb] [comb/obj] object(s)\n\n", cmd);
    bu_vls_printf(&str, "Any argument of the form comb/obj is interpreted as requesting removal of\n");
    bu_vls_printf(&str, "object \"obj\" from the first level tree of \"comb\" (instances of obj\n");
    bu_vls_printf(&str, "deeper in the tree of \"comb\" are not removed.)  Note that these paths\n");
    bu_vls_printf(&str, "are processed first - any other rm operations will take place after these\n");
    bu_vls_printf(&str, "paths are handled.\n\n");
    bu_vls_printf(&str, "By default, if no options are supplied and the first object is a comb\n");
    bu_vls_printf(&str, "the object(s) are removed from \"comb\" - this is a convenient way to\n");
    bu_vls_printf(&str, "remove multiple objects from a single comb.  Again, instances are.\n");
    bu_vls_printf(&str, "removed only from the immediate child object set of \"comb\".\n\n");
    bu_vls_printf(&str, "If one or more options are supplied or the first object specified is not a comb,\n");
    bu_vls_printf(&str, "the command form becomes:\n\n");
    bu_vls_printf(&str, "       %s [options] object(s)\n\n", cmd);
    bu_vls_printf(&str, "and the scope changes from a single comb's tree to the .g database.\n");
    if (option_help) {
	bu_vls_printf(&str, "\nOptions:\n%s\n", option_help);
	bu_free((char *)option_help, "help str");
    }
    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&str));
    bu_vls_free(&str);
}

HIDDEN void
_rm_ref(struct ged *gedp, struct bu_ptbl *objs, struct bu_vls *rmlog, int dry_run)
{
    size_t i = 0;
    struct directory *dp = RT_DIR_NULL;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    FOR_ALL_DIRECTORY_START(dp, gedp->ged_wdbp->dbip) {
	if (!(dp->d_flags & RT_DIR_COMB))
	    continue;

	if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    if (rmlog) {
		bu_vls_printf(rmlog, "rt_db_get_internal(%s) failure", dp->d_namep);
	    }
	    continue;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	for (i = 0; i < BU_PTBL_LEN(objs); i++) {
	    int code;

	    code = db_tree_del_dbleaf(&(comb->tree), ((struct directory *)BU_PTBL_GET(objs, i))->d_namep, &rt_uniresource, dry_run);
	    if (code == -1)
		continue;       /* not found */
	    if (code == -2)
		continue;       /* empty tree */
	    if (code < 0) {
		if (rmlog) {
		    bu_vls_printf(rmlog, "ERROR: Failure deleting %s/%s\n", dp->d_namep, ((struct directory *)BU_PTBL_GET(objs, i))->d_namep);
		}
	    } else {
		if (rmlog && dry_run) {
		    bu_vls_printf(rmlog, "Remove reference to object %s from %s\n", ((struct directory *)BU_PTBL_GET(objs, i))->d_namep, dp->d_namep);
		}
	    }
	}

	if (!dry_run && rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	    if (rmlog) {
		bu_vls_printf(rmlog, "ERROR: Unable to write new combination into database.\n");
	    }
	    continue;
	}
    } FOR_ALL_DIRECTORY_END;
}


/* this finds references to 'obj' that are not within any of the the 'topobjs'
 * combination hierarchies, elsewhere in the database.  this is so we can make
 * sure we don't kill objects that are referenced elsewhere in the
 * database, in a hierarchy that is not being deleted.
 */
HIDDEN int
_rm_find_reference(struct db_i *dbip, struct bu_ptbl *topobjs, const char *obj, struct bu_vls *rmlog, int verbosity)
{
    int path_cnt = 0;
    int i;
    int ret = 0;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct directory **paths = NULL;

    if (!dbip || !topobjs || !obj)
	return 0;

    bu_vls_sprintf(&str, "-depth >0");
    for (i = 0; i < (int)BU_PTBL_LEN(topobjs); i++) {
	bu_vls_printf(&str, " -not -below -name %s", ((struct directory *)BU_PTBL_GET(topobjs, i))->d_namep);
    }
    bu_vls_printf(&str, " -name %s", obj);

    path_cnt = db_ls(dbip, DB_LS_TOPS, NULL, &(paths));
    ret = db_search(NULL, DB_SEARCH_TREE, bu_vls_cstr(&str), path_cnt, paths, dbip);
    if (paths) bu_free(paths, "free search paths");

    if (ret && rmlog && verbosity > 0) {
	bu_vls_printf(rmlog, "NOTE: %s is referenced by unremoved objects in the database - skipping.\n", obj);
    }

    bu_vls_free(&str);

    return ret;
}


HIDDEN int
_rm_obj(struct ged *gedp, struct directory *dp, struct bu_vls *rmlog, int UNUSED(verbosity), int force, int dry_run)
{
    if (!gedp || !dp) return -1;

    if (dp != RT_DIR_NULL) {
	if (!force && dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY && dp->d_minor_type == 0) {
	    if (rmlog) {
		bu_vls_printf(rmlog, "Warning: you attempted to delete the _GLOBAL object.\n");
		bu_vls_printf(rmlog, "\tIf you delete the \"_GLOBAL\" object you will be losing some important information\n");
		bu_vls_printf(rmlog, "\tsuch as your preferred units and the title of the database.\n");
		bu_vls_printf(rmlog, "\tUse the \"-f\" option, if you really want to do this.\n");
	    }
	    return -1;
	}

	/* don't worry about phony objects */
	if (dp->d_addr == RT_DIR_PHONY_ADDR) return 0;

	/* clear the object from the display */
	if (!dry_run) {
	    _dl_eraseAllNamesFromDisplay(gedp->ged_gdp->gd_headDisplay, gedp->ged_wdbp->dbip, gedp->ged_free_vlist_callback, dp->d_namep, 0, gedp->freesolid);

	    /* actually do the deletion */
	    if (db_delete(gedp->ged_wdbp->dbip, dp) != 0 || db_dirdelete(gedp->ged_wdbp->dbip, dp) != 0) {
		/* Abort kill processing on first error */
		if (rmlog) bu_vls_printf(rmlog, "Error: unable to delete %s", dp->d_namep);
		return -1;
	    }
	} else {
	    bu_vls_printf(rmlog, "Delete %s\n", dp->d_namep);
	}
    } else {
	return -1;
    }
    return 0;
}

HIDDEN int
_removal_queue(struct bu_ptbl *to_remove, struct ged *gedp, struct bu_ptbl *seeds, struct bu_vls *rmlog, int verbosity, int recurse)
{
    size_t i,j;

    std::queue<struct directory *> dpq;
    std::set<struct directory *> checked;

    if (!to_remove || !gedp || !seeds) return 0;

    /* Initialize */
    for (i = 0; i < BU_PTBL_LEN(seeds); i++) {
	struct directory *dp = ((struct directory *)BU_PTBL_GET(seeds, i));
	if (dp && dp != RT_DIR_NULL) {
	    checked.insert(dp);
	    if (_rm_find_reference(gedp->ged_wdbp->dbip, seeds, dp->d_namep, rmlog, verbosity) > 0) continue;
	    if (dp->d_flags & RT_DIR_COMB) {
		/* get immediate children */
		const char *comb_children_search = "-mindepth 1 -maxdepth 1";
		struct bu_ptbl comb_children = BU_PTBL_INIT_ZERO;
		(void)db_search(&comb_children, DB_SEARCH_RETURN_UNIQ_DP, comb_children_search, 1, &dp, gedp->ged_wdbp->dbip);
		if (BU_PTBL_LEN(&comb_children) == 0) {
		    bu_ptbl_ins_unique(to_remove, (long *)dp);
		} else {
		    if (recurse) {
			bu_ptbl_ins_unique(to_remove, (long *)dp);
			for (j = 0; j < BU_PTBL_LEN(&comb_children); j++) {
			    struct directory *dpc = (struct directory *)BU_PTBL_GET(&comb_children, j);
			    if (dpc && checked.find(dpc) == checked.end() && dpc != RT_DIR_NULL) {
				dpq.push(dpc);
			    }
			    if (dpc && dpc != RT_DIR_NULL) {
				checked.insert(dpc);
			    }
			}
		    } else {
			if (rmlog && verbosity > 0) bu_vls_printf(rmlog, "Combination %s is not empty, skipping\n", dp->d_namep);
		    }
		}
		bu_ptbl_free(&comb_children);
	    } else {
		bu_ptbl_ins_unique(to_remove, (long *)dp);
	    }
	}
    }
    /* Walk down the tree structure */
    while (!dpq.empty()) {
	struct directory *dp = dpq.front();
	dpq.pop();
	if (dp && dp != RT_DIR_NULL) {
	    if (_rm_find_reference(gedp->ged_wdbp->dbip, seeds, dp->d_namep, rmlog, verbosity) > 0) continue;
	    bu_ptbl_ins_unique(to_remove, (long *)dp);
	    /* get immediate children */
	    if (dp->d_flags & RT_DIR_COMB) {
		const char *comb_children_search = "-mindepth 1 -maxdepth 1";
		struct bu_ptbl comb_children = BU_PTBL_INIT_ZERO;
		(void)db_search(&comb_children, DB_SEARCH_RETURN_UNIQ_DP, comb_children_search, 1, &dp, gedp->ged_wdbp->dbip);
		for (j = 0; j < BU_PTBL_LEN(&comb_children); j++) {
		    struct directory *dpc = (struct directory *)BU_PTBL_GET(&comb_children, j);
		    if (dpc && checked.find(dpc) == checked.end() && dpc != RT_DIR_NULL) {
			dpq.push(dpc);
		    }
		    if (dpc && dpc != RT_DIR_NULL) {
			checked.insert(dpc);
		    }
		}
		bu_ptbl_free(&comb_children);
	    }
	}
    }

    return BU_PTBL_LEN(to_remove);
}

HIDDEN int
_ged_rm_path(struct ged *gedp, const char *in)
{
    int ret = GED_OK;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct directory *dp;
    struct bu_vls combstr = BU_VLS_INIT_ZERO;
    struct bu_vls obj = BU_VLS_INIT_ZERO;
    const char *slash = strrchr(in, '/');
    bu_vls_sprintf(&obj, "%s", slash+1);
    bu_vls_strncpy(&combstr, in, strlen(in) - strlen(slash));
    bu_log("obj: %s\n", bu_vls_addr(&obj));
    bu_log("comb: %s\n", bu_vls_addr(&combstr));

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&combstr), LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Error: first object %s in path %s does not exist", bu_vls_addr(&combstr), in);
	ret = GED_ERROR;
	goto cleanup;
    }

    if ((dp->d_flags & RT_DIR_COMB) == 0) {
	bu_vls_printf(gedp->ged_result_str, "Error: first object %s in path %s is not a comb", bu_vls_addr(&combstr), in);
	ret = GED_ERROR;
	goto cleanup;
    }

    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	ret = GED_ERROR;
	goto cleanup;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (db_tree_del_dbleaf(&(comb->tree), bu_vls_addr(&obj), &rt_uniresource, 0) < 0) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: Failure deleting %s/%s\n", dp->d_namep, bu_vls_addr(&obj));
	ret = GED_ERROR;
	goto cleanup;
    } else {
	struct bu_vls path = BU_VLS_INIT_ZERO;

	bu_vls_printf(&path, "%s/%s", dp->d_namep, bu_vls_addr(&obj));
	_dl_eraseAllPathsFromDisplay(gedp->ged_gdp->gd_headDisplay, gedp->ged_wdbp->dbip, gedp->ged_free_vlist_callback, bu_vls_addr(&path), 0, gedp->freesolid);
	bu_vls_free(&path);
	bu_vls_printf(gedp->ged_result_str, "deleted %s/%s\n", dp->d_namep, bu_vls_addr(&obj));
    }

    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
	ret = GED_ERROR;
    }

cleanup:
    bu_vls_free(&combstr);
    bu_vls_free(&obj);
    return ret;
}


HIDDEN int
_rm_verbose(struct bu_vls *UNUSED(msg), int UNUSED(argc), const char **UNUSED(argv), void *set_v)
{
    int *int_set = (int *)set_v;
    if (int_set) (*int_set) = (*int_set) + 1;
    return 0;
}

int
ged_remove(struct ged *gedp, int orig_argc, const char *orig_argv[])
{
    int print_help = 0;
    int remove_force = 0;
    int remove_from_comb = 1;
    int remove_recursive = 0;
    int verbose = 0;
    int no_op = 0;
    int legacy = 0;
    size_t argc = (size_t)orig_argc;
    char **argv = bu_dup_argv(argc, orig_argv);
    size_t free_argc = argc;
    char **free_argv = argv;
    int ret_ac;

    struct directory *dp;
    int i;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int ret = GED_OK;
    struct bu_opt_desc d[8];
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls rmlog = BU_VLS_INIT_ZERO;
    struct bu_ptbl objs = BU_PTBL_INIT_ZERO;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    BU_OPT(d[0], "h", "help",  "", NULL, (void *)&print_help, "Print help and exit");
    BU_OPT(d[1], "?", "",      "", NULL, (void *)&print_help, "");
    BU_OPT(d[2], "f", "force",  "", NULL, (void *)&remove_force, "Treat combs like any other objects.  If recursive flag is added, do not verify objects are unused in other trees before deleting.");
    BU_OPT(d[3], "r", "recursive",  "", NULL, (void *)&remove_recursive, "Walks combs and deletes all of their sub-objects (or, when used with just -a, removes references to objects but not the actual objects.) Will not delete objects used elsewhere in the database unless the -f option is also supplied.");
    BU_OPT(d[4], "v", "verbose",  "", &_rm_verbose, (void *)&verbose, "Enable verbose reporting.  Supplying the option multiple times will increase the verbosity of reporting.");
    BU_OPT(d[5], "n", "no-op",  "", NULL, (void *)&no_op, "Perform a \"dry run\" - reports what actions would be taken but does not change the database.");
    BU_OPT(d[6], "L", "legacy",  "", NULL, (void *)&legacy, "[DEPRECATED] Use the old rm behavior, which takes as arguments one comb and a list of objects to remove from that comb.");
    BU_OPT_NULL(d[7]);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Skip first arg */
    argv++; argc--;

    /* parse args */
    ret_ac = bu_opt_parse(&str, argc, (const char **)argv, d);
    if (ret_ac < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_addr(&str));
	_remove_show_help(gedp, orig_argv[0], d);
	bu_vls_free(&str);
	bu_free_argv(free_argc,free_argv);
	return GED_ERROR;
    }

    /* must be wanting help */
    if (print_help || ret_ac == 0) {
	_remove_show_help(gedp, orig_argv[0], d);
	bu_vls_free(&str);
	bu_free_argv(free_argc,free_argv);
	return GED_HELP;
    }


    /* If we've got a comb to start with and none of the flags are active, the
     * interpretation is that the list of objs is removed from the comb child
     * list of the first comb. */
    if (legacy) {
	if ((dp = db_lookup(gedp->ged_wdbp->dbip, argv[0], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    ret = GED_ERROR;
	    goto rcleanup;
	}
	if ((dp->d_flags & RT_DIR_COMB) != 0) {
	    if (BU_PTBL_LEN(&objs) == 1) {
		bu_vls_printf(gedp->ged_result_str, "Error: single comb supplied with no flags - not removing %s.  To remove an individual comb from the database add the -f flag.", argv[1]);
		ret = GED_ERROR;
		goto rcleanup;
	    }
	    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
		ret = GED_ERROR;
		goto rcleanup;
	    }

	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    RT_CK_COMB(comb);

	    /* Process each argument */
	    for (i = 1; i < ret_ac; i++) {
		if (db_tree_del_dbleaf(&(comb->tree), argv[i], &rt_uniresource, 0) < 0) {
		    bu_vls_printf(gedp->ged_result_str, "ERROR: Failure deleting %s/%s\n", dp->d_namep, argv[i]);
		    ret = GED_ERROR;
		    goto rcleanup;
		} else {
		    struct bu_vls path = BU_VLS_INIT_ZERO;

		    bu_vls_printf(&path, "%s/%s", dp->d_namep, argv[i]);
		    _dl_eraseAllPathsFromDisplay(gedp->ged_gdp->gd_headDisplay, gedp->ged_wdbp->dbip, gedp->ged_free_vlist_callback, bu_vls_addr(&path), 0, gedp->freesolid);
		    bu_vls_free(&path);
		    bu_vls_printf(gedp->ged_result_str, "deleted %s/%s\n", dp->d_namep, argv[i]);
		}
	    }

	    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
		ret = GED_ERROR;
		goto rcleanup;
	    }
	    bu_vls_printf(gedp->ged_result_str, "Warning - this behavor of the rm command has been deprecated.  In future versions of BRL-CAD, the correct way to accomplish removing objects from a comb is to specify a path:\n\n");
	    bu_vls_printf(gedp->ged_result_str, "rm ");
	    for (i = 1; i < ret_ac; i++) {
		bu_vls_printf(gedp->ged_result_str, "%s/%s ", dp->d_namep, argv[i]);
	    }
	    bu_vls_printf(gedp->ged_result_str, "\n\n");
	    goto rcleanup;
	} else {
	    ret = GED_ERROR;
	    goto rcleanup;
	}
    }

    /* Collect paths without slashes - those are full object removals, not
     * removal of an object from a comb.  For paths *with* slashes, those
     * are interpreted as removing the last object in the path from its parent
     * comb - handle them immediately. */
    for (i = 0; i < ret_ac; i++) {
	const char *slash = strrchr(argv[i], '/');
	if (slash == NULL) {
	    struct directory *rdp = db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_QUIET);
	    if (rdp != RT_DIR_NULL) {
		bu_ptbl_ins_unique(&objs, (long *)rdp);
	    } else {
		if (verbose >= 2) {
		    bu_vls_printf(&rmlog, "Attempted to delete object %s, but it was not found in the database\n", argv[i]);
		}
	    }
	} else {
	    ret = _ged_rm_path(gedp, argv[i]);
	    /* If the first path arg was a full path,
	     * we're not removing the rest from a comb */
	    if (i == 0) remove_from_comb = 0;
	    if (ret == GED_ERROR) goto rcleanup;
	}
    }

    /* Update references after deletion.
     * IMPORTANT: db_ls and db_search won't work properly without this */
    db_update_nref(gedp->ged_wdbp->dbip, &rt_uniresource);

    if (remove_force || remove_recursive) remove_from_comb = 0;

    /* If we've got nothing else, we're done */
    if (BU_PTBL_LEN(&objs) == 0) {
	goto rcleanup;
    }

    /* Behavior depends on the type of the first object (potentially, based on supplied flags) */
    if ((dp = db_lookup(gedp->ged_wdbp->dbip, ((struct directory *)BU_PTBL_GET(&objs, 0))->d_namep, LOOKUP_NOISY)) == RT_DIR_NULL) {
	ret = GED_ERROR;
	goto rcleanup;
    }


    /* Having handled the removal-from-comb cases, we move on to the more
     * general database removals.  The various combinations of options need
     * different logic */
    if (!remove_force && !remove_recursive) {
	struct bu_ptbl rmobjs = BU_PTBL_INIT_ZERO;
	if (_removal_queue(&rmobjs, gedp, &objs, &rmlog, verbose, 0)) {
	    for (i = 0; i < (int)BU_PTBL_LEN(&rmobjs); i++) {
		(void)_rm_obj(gedp, (struct directory *)BU_PTBL_GET(&rmobjs, i), &rmlog, verbose, 0, no_op);
	    }
	}
	bu_ptbl_free(&rmobjs);
	goto rcleanup;
    }
    if (!remove_force && remove_recursive) {
	struct bu_ptbl rmobjs = BU_PTBL_INIT_ZERO;
	if (_removal_queue(&rmobjs, gedp, &objs, &rmlog, verbose, 1)) {
	    for (i = 0; i < (int)BU_PTBL_LEN(&rmobjs); i++) {
		(void)_rm_obj(gedp, (struct directory *)BU_PTBL_GET(&rmobjs, i), &rmlog, verbose, 0, no_op);
	    }
	}
	bu_ptbl_free(&rmobjs);
	goto rcleanup;
    }
    if (remove_force && !remove_recursive) {
	for (i = 0; i < (int)BU_PTBL_LEN(&objs); i++) {
	    /* If _GLOBAL is specified explicitly with a force flag, remove it */
	    (void)_rm_obj(gedp, (struct directory *)BU_PTBL_GET(&objs, i), &rmlog, verbose, 1, no_op);
	}
	goto rcleanup;
    }
    if (remove_force && remove_recursive) {
	struct bu_ptbl rmobjs = BU_PTBL_INIT_ZERO;
	(void)db_search(&rmobjs, DB_SEARCH_RETURN_UNIQ_DP, "-name *", BU_PTBL_LEN(&objs), (struct directory **)objs.buffer, gedp->ged_wdbp->dbip);
	for (i = 0; i < (int)BU_PTBL_LEN(&rmobjs); i++) {
	    /* If _GLOBAL is specified explicitly with a force flag, remove it */
	    (void)_rm_obj(gedp, (struct directory *)BU_PTBL_GET(&rmobjs, i), &rmlog, verbose, 1, no_op);
	}
	bu_ptbl_free(&rmobjs);
	goto rcleanup;
    }

rcleanup:
    bu_ptbl_free(&objs);
    bu_vls_free(&str);
    bu_free_argv(free_argc,free_argv);
    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&rmlog));
    bu_vls_free(&rmlog);
    return ret;
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

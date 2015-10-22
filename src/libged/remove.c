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
    bu_vls_sprintf(&str, "Usage: %s [options] [comb] object(s)\n", cmd);
    bu_vls_printf(&str, "By default, if no options are supplied and the first object is a comb\n", cmd);
    bu_vls_printf(&str, "the object(s) are removed from the comb definition.\n\n", cmd);
    bu_vls_printf(&str, "If -f is supplied, the command form becomes:\n", cmd);
    bu_vls_printf(&str, "       %s [options] object(s)\n", cmd);
    bu_vls_printf(&str, "and all objects are moved from the .g database.\n", cmd);
    bu_vls_printf(&str, "The -r option implies -f and will recursively walk combs\n", cmd);
    bu_vls_printf(&str, "rather than deleting them.\n", cmd);
    if (option_help) {
	bu_vls_printf(&str, "Options:\n%s\n", option_help);
	bu_free((char *)option_help, "help str");
    }
    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&str));
    bu_vls_free(&str);
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

int
ged_remove(struct ged *gedp, int orig_argc, const char *orig_argv[])
{
    int print_help = 0;
    int remove_force = 0;
    int remove_from_comb = 1;
    int remove_recursive = 0;
    int remove_force_refs = 0;
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
    static const char *usage = "comb object(s)";
    struct bu_opt_desc d[6];
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_ptbl objs = BU_PTBL_INIT_ZERO;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    BU_OPT(d[0], "h", "help",  "", NULL, (void *)&print_help, "Print help and exit");
    BU_OPT(d[1], "?", "",      "", NULL, (void *)&print_help, "");
    BU_OPT(d[2], "f", "force",  "", NULL, (void *)&remove_force, "Treat combs like any other objects.  If recursive, do not verify objects are unused in other trees before deleting.");
    BU_OPT(d[3], "F", "force-refs",  "", NULL, (void *)&remove_force_refs, "Like '-f', but will also remove references to removed objects in other parts of the database rather than leaving invalid references. Overrides '-f' if both are specified");
    BU_OPT(d[4], "r", "recursive",  "", NULL, (void *)&remove_recursive, "Walk combs and delete all of their sub-objects. Will not delete objects used elsewhere in the database.");
    BU_OPT_NULL(d[5]);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Skip first arg */
    argv++; argc--;

    /* parse args */
    ret_ac = bu_opt_parse(&str, argc, (const char **)argv, d);
    if (ret_ac < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_addr(&str));
	bu_vls_free(&str);
	bu_free_argv(free_argc,free_argv);
	return GED_ERROR;
    }

    /* must be wanting help */
    if (print_help || ret_ac == 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	bu_free_argv(free_argc,free_argv);
	return GED_HELP;
    }

    /* Collect paths without slashes - those are full object removals, not
     * removal of an object from a comb.  For paths *with* slashes, those
     * are interpreted as removing the last object in the path from its parent
     * comb. */
    for (i = 0; i < ret_ac; i++) {
	const char *slash = strrchr(argv[i], '/');
	if (slash == NULL) {
	    bu_ptbl_ins(&objs, (long *)argv[i]);
	} else {
	    ret = _ged_rm_path(gedp, argv[i]);
	    /* If the first path arg was a full path,
	     * we're not removing the rest from a comb */
	    if (i == 0) remove_from_comb = 0;
	    if (ret == GED_ERROR) goto rcleanup;
	}
    }

    if (remove_force || remove_force_refs || remove_recursive) remove_from_comb = 0;

    if (BU_PTBL_LEN(&objs) > 0) {
	/* Behavior depends on the type of the first object (potentially, based on supplied flags) */
	if ((dp = db_lookup(gedp->ged_wdbp->dbip, (const char *)BU_PTBL_GET(&objs, 0), LOOKUP_NOISY)) == RT_DIR_NULL) {
	    ret = GED_ERROR;
	    goto rcleanup;
	}

	/* If we've got a comb to start with, we aren't doing a recursive removal, and nothing is
	 * forcing us to remove the comb itself, the interpretation is that the list of objs is
	 * removed from the comb child list of the first comb. */
	if (remove_from_comb && (dp->d_flags & RT_DIR_COMB) != 0) {
	    if (BU_PTBL_LEN(&objs) == 1) {
		bu_vls_printf(gedp->ged_result_str, "Error: single comb supplied without one or more of the '-f' and '-r' flags - not removing %s", argv[1]);
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
	    for (i = 1; i < (int)BU_PTBL_LEN(&objs); i++) {
		const char *obj = (const char *)BU_PTBL_GET(&objs, i);
		if (db_tree_del_dbleaf(&(comb->tree), obj, &rt_uniresource, 0) < 0) {
		    bu_vls_printf(gedp->ged_result_str, "ERROR: Failure deleting %s/%s\n", dp->d_namep, obj);
		    ret = GED_ERROR;
		    goto rcleanup;
		} else {
		    struct bu_vls path = BU_VLS_INIT_ZERO;

		    bu_vls_printf(&path, "%s/%s", dp->d_namep, obj);
		    _dl_eraseAllPathsFromDisplay(gedp->ged_gdp->gd_headDisplay, gedp->ged_wdbp->dbip, gedp->ged_free_vlist_callback, bu_vls_addr(&path), 0, gedp->freesolid);
		    bu_vls_free(&path);
		    bu_vls_printf(gedp->ged_result_str, "deleted %s/%s\n", dp->d_namep, obj);
		}
	    }

	    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
		ret = GED_ERROR;
		goto rcleanup;
	    }
	} else {
	    /* If we don't have the syntax/options that set up removal of a list of objects from a comb, we're
	     * in traditional unix-style rm territory. */
	    if (!remove_recursive) {
		/* the force flag is a no-op at this stage - without recursive removal, it's only role is to override
		 * the remove-from-comb behavior */
		const char **av = (const char **)bu_calloc(BU_PTBL_LEN(&objs) + 1, sizeof(char *), "new argv array");
		av[0] = (const char *)orig_argv[0];
		for (i = 1; i < (int)BU_PTBL_LEN(&objs) + 1; i++) {
		    av[i] = (const char *)BU_PTBL_GET(&objs, i - 1);
		}
		ret = ged_kill(gedp, BU_PTBL_LEN(&objs)+1, av);
		bu_free((void *)av, "free tmp argv");
		return ret;
	    } else {
		/* This is where the force flag can really impact geometry.  By default, killtree checks to
		 * make sure geometry being removed from under one comb isn't used elsewhere in the tree.  With
		 * force on, it will wipe out everything. */
		if (remove_force_refs) {
		    const char **av = (const char **)bu_calloc(BU_PTBL_LEN(&objs) + 2, sizeof(char *), "new argv array");
		    av[0] = "killtree";
		    av[1] = "-a";
		    for (i = 2; i < (int)BU_PTBL_LEN(&objs) + 2; i++) {
			av[i] = (const char *)BU_PTBL_GET(&objs, i - 2);
		    }
		    ret = ged_killtree(gedp, BU_PTBL_LEN(&objs)+2, av);
		    bu_free((void *)av, "free tmp argv");
		    return ret;
		}
		if (remove_force) {
		    const char **av = (const char **)bu_calloc(BU_PTBL_LEN(&objs) + 2, sizeof(char *), "new argv array");
		    av[0] = "killtree";
		    av[1] = "-f";
		    for (i = 2; i < (int)BU_PTBL_LEN(&objs) + 2; i++) {
			av[i] = (const char *)BU_PTBL_GET(&objs, i - 2);
		    }
		    ret = ged_killtree(gedp, BU_PTBL_LEN(&objs)+2, av);
		    bu_free((void *)av, "free tmp argv");
		    return ret;
		}
		{
		    const char **av = (const char **)bu_calloc(BU_PTBL_LEN(&objs) + 1, sizeof(char *), "new argv array");
		    av[0] = "killtree";
		    for (i = 1; i < (int)BU_PTBL_LEN(&objs) + 1; i++) {
			av[i] = (const char *)BU_PTBL_GET(&objs, i - 1);
		    }
		    ret = ged_killtree(gedp, BU_PTBL_LEN(&objs)+1, av);
		    bu_free((void *)av, "free tmp argv");
		    return ret;
		}
	    }
	}
    }

rcleanup:
    bu_ptbl_free(&objs);
    bu_free_argv(free_argc,free_argv);
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

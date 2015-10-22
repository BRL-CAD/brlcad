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
    int remove_comb = 0;
    int remove_recursive = 0;
    size_t argc = (size_t)orig_argc;
    char **argv = bu_dup_argv(argc, orig_argv);
    int ret_ac;

    struct directory *dp;
    int i;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int ret = GED_OK;
    static const char *usage = "comb object(s)";
    struct bu_opt_desc d[5];
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_ptbl objs = BU_PTBL_INIT_ZERO;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    BU_OPT(d[0], "h", "help",  "", NULL, (void *)&print_help, "Print help and exit");
    BU_OPT(d[1], "?", "",      "", NULL, (void *)&print_help, "");
    BU_OPT(d[2], "f", "force",  "", NULL, (void *)&remove_comb, "Treat combs like any other objects.  If recursive, do not verify objects are unused in other trees before deleting.");
    BU_OPT(d[3], "r", "recursive",  "", NULL, (void *)&remove_recursive, "Walk combs and delete all of their sub-objects");
    BU_OPT_NULL(d[4]);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Skip first arg */
    argv++; argc--;

    /* parse args */
    ret_ac = bu_opt_parse(&str, argc, (const char **)argv, d);
    if (ret_ac < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_addr(&str));
	bu_vls_free(&str);
	bu_free_argv(argc,argv);
	return GED_ERROR;
    }

    /* must be wanting help */
    if (print_help || ret_ac == 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	bu_free_argv(argc,argv);
	return GED_HELP;
    }

    /* Collect paths without slashes, process those with */
    for (i = 0; i < ret_ac; i++) {
	const char *slash = strrchr(argv[i], '/');
	if (slash == NULL) {
	    bu_ptbl_ins(&objs, (long *)argv[i]);
	} else {
	    ret = _ged_rm_path(gedp, argv[i]);
	    /* If the first path arg was a full path,
	     * we're not removing the rest from a comb */
	    if (i == 0) remove_comb = 1;
	    if (ret == GED_ERROR) goto rcleanup;
	}
    }

    if (BU_PTBL_LEN(&objs) > 0) {
	if ((dp = db_lookup(gedp->ged_wdbp->dbip, (const char *)BU_PTBL_GET(&objs, 0), LOOKUP_NOISY)) == RT_DIR_NULL) {
	    ret = GED_ERROR;
	    goto rcleanup;
	}
	if (!remove_comb && !remove_recursive && (dp->d_flags & RT_DIR_COMB) != 0) {
	    if (BU_PTBL_LEN(&objs) == 1) {
		bu_vls_printf(gedp->ged_result_str, "Error: single comb supplied without '-f' flag - not removing %s", argv[1]);
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
	    if (!remove_recursive) {
		const char **av = (const char **)bu_calloc(BU_PTBL_LEN(&objs) + 1, sizeof(char *), "new argv array");
		av[0] = (const char *)orig_argv[0];
		for (i = 1; i < (int)BU_PTBL_LEN(&objs) + 1; i++) {
		    av[i] = (const char *)BU_PTBL_GET(&objs, i - 1);
		}
		ret = ged_kill(gedp, BU_PTBL_LEN(&objs)+1, av);
		bu_free((void *)av, "free tmp argv");
		return ret;
	    } else {
		/* TODO - killtree */
	    }
	}
    }

rcleanup:
    bu_ptbl_free(&objs);
    bu_free_argv(argc,argv);
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

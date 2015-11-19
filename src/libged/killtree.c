/*                         K I L L T R E E . C
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
/** @file libged/killtree.c
 *
 * The killtree command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/getopt.h"

#include "./ged_private.h"


#define AV_STEP 4096


struct killtree_data {
    struct ged *gedp;
    int killrefs;
    int print;
    int force;
    const char *top;
    int ac;
    char **av;
    size_t av_capacity;
};


/* this finds references to 'obj' that are not within the 'topobj'
 * combination hierarchy, elsewhere in the database.  this is so we
 * can make sure we don't kill objects that are referenced somewhere
 * else in the database.
 */
HIDDEN int
find_reference(struct db_i *dbip, const char *topobj, const char *obj)
{
    int ret;
    struct bu_vls str = BU_VLS_INIT_ZERO;

    if (!dbip || !topobj || !obj)
	return 0;

    /* FIXME: these should be wrapped in quotes, but need to dewrap
     * after bu_argv_from_string().
     */
    bu_vls_printf(&str, "-depth >0 -not -below -name %s -name %s", topobj, obj);

    ret = db_search(NULL, DB_SEARCH_TREE, bu_vls_cstr(&str), 0, NULL, dbip);

    bu_vls_free(&str);

    return ret;
}


HIDDEN void
killtree_callback(struct db_i *dbip, struct directory *dp, void *ptr)
{
    struct killtree_data *gktdp = (struct killtree_data *)ptr;
    int ref_exists = 0;

    if (dbip == DBI_NULL)
	return;

    /* don't bother checking for references if the -f or -a flags are
     * presented to force a full kill and all references respectively.
     */
    if (!gktdp->force && !gktdp->killrefs)
	ref_exists = find_reference(dbip, gktdp->top, dp->d_namep);

    /* if a reference exists outside of the subtree we're killing, we
     * don't kill this object or it'll create invalid reference
     * elsewhere in the database.  do nothing.
     */
    if (ref_exists)
	return;

    if (gktdp->print) {
	if (!gktdp->killrefs)
	    bu_vls_printf(gktdp->gedp->ged_result_str, "%s ", dp->d_namep);
	else {
	    if ((size_t)(gktdp->ac + 2) >= gktdp->av_capacity) {
		gktdp->av = (char **)bu_realloc(gktdp->av, sizeof(char *) * (gktdp->av_capacity + AV_STEP), "realloc av");
		gktdp->av_capacity += AV_STEP;
	    }
	    gktdp->av[gktdp->ac++] = bu_strdup(dp->d_namep);
	    gktdp->av[gktdp->ac] = (char *)0;

	    bu_vls_printf(gktdp->gedp->ged_result_str, "%s ", dp->d_namep);
	}
    } else {
	_dl_eraseAllNamesFromDisplay(gktdp->gedp->ged_gdp->gd_headDisplay, gktdp->gedp->ged_wdbp->dbip, gktdp->gedp->ged_free_vlist_callback, dp->d_namep, 0, gktdp->gedp->freesolid);

	bu_vls_printf(gktdp->gedp->ged_result_str, "KILL %s:  %s\n",
		      (dp->d_flags & RT_DIR_COMB) ? "COMB" : "Solid",
		      dp->d_namep);

	if (!gktdp->killrefs) {
	    if (db_delete(dbip, dp) != 0 || db_dirdelete(dbip, dp) != 0) {
		bu_vls_printf(gktdp->gedp->ged_result_str, "an error occurred while deleting %s\n", dp->d_namep);
	    }
	} else {
	    if ((size_t)(gktdp->ac + 2) >= gktdp->av_capacity) {
		gktdp->av = (char **)bu_realloc(gktdp->av, sizeof(char *) * (gktdp->av_capacity + AV_STEP), "realloc av");
		gktdp->av_capacity += AV_STEP;
	    }
	    gktdp->av[gktdp->ac++] = bu_strdup(dp->d_namep);
	    gktdp->av[gktdp->ac] = (char *)0;

	    if (db_delete(dbip, dp) != 0 || db_dirdelete(dbip, dp) != 0) {
		bu_vls_printf(gktdp->gedp->ged_result_str, "an error occurred while deleting %s\n", dp->d_namep);

		/* Remove from list */
		bu_free((void *)gktdp->av[--gktdp->ac], "killtree_callback");
		gktdp->av[gktdp->ac] = (char *)0;
	    }
	}
    }
}


int
ged_killtree(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int i;
    int c;
    struct killtree_data gktd;
    static const char *usage = "[-a|-f|-n] object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    gktd.gedp = gedp;
    gktd.killrefs = 0;
    gktd.print = 0;
    gktd.force = 0;
    gktd.ac = 1;
    gktd.top = NULL;

    gktd.av = (char **)bu_calloc(1, sizeof(char *) * AV_STEP, "alloc av");
    gktd.av_capacity = AV_STEP;
    BU_ASSERT(gktd.ac + argc + 2 < AV_STEP); /* potential -n opts */
    gktd.av[0] = "killrefs";
    gktd.av[1] = (char *)0;

    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, "afn")) != -1) {
	switch (c) {
	    case 'a':
		gktd.killrefs = 1;
		break;
	    case 'n':
		gktd.print = 1;
		gktd.av[gktd.ac++] = bu_strdup("-n");
		gktd.av[gktd.ac] = (char *)0;
		break;
	    case 'f':
		gktd.force = 1;
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		bu_free(gktd.av, "free av (error)");
		gktd.av = NULL;
		return GED_ERROR;
	}
    }

    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    /* Objects that would be killed are in the first sublist */
    if (gktd.print)
	bu_vls_printf(gedp->ged_result_str, "{");

    for (i = 1; i < argc; i++) {
	if ((dp = db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;

	/* ignore phony objects */
	if (dp->d_addr == RT_DIR_PHONY_ADDR)
	    continue;

	/* stash the what's killed so we can find refs elsewhere */
	gktd.top = argv[i];

	db_functree(gedp->ged_wdbp->dbip, dp,
		    killtree_callback, killtree_callback,
		    gedp->ged_wdbp->wdb_resp, (void *)&gktd);
    }

    /* Close the sublist of would-be killed objects. Also open the
     * sublist of objects that reference the would-be killed objects.
     */
    if (gktd.print)
	bu_vls_printf(gedp->ged_result_str, "} {");

    if (gktd.killrefs && gktd.ac > 1) {
	gedp->ged_internal_call = 1;
	(void)ged_killrefs(gedp, gktd.ac, (const char **)gktd.av);
	gedp->ged_internal_call = 0;

	for (i = 1; i < gktd.ac; i++) {
	    if (!gktd.print)
		bu_vls_printf(gedp->ged_result_str, "Freeing %s\n", gktd.av[i]);
	    bu_free((void *)gktd.av[i], "killtree_data");
	    gktd.av[i] = NULL;
	}
    }

    if (gktd.print)
	bu_vls_printf(gedp->ged_result_str, "}");

    bu_free(gktd.av, "free av");
    gktd.av = NULL;

    return GED_OK;
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

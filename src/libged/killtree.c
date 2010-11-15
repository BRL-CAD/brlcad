/*                         K I L L T R E E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2010 United States Government as represented by
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
/** @file killtree.c
 *
 * The killtree command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


struct ged_killtree_data {
    struct ged *gedp;
    int killrefs;
    int nflag;
    int ac;
    char *av[MAXARGS];
};

static void
ged_killtree_callback(struct db_i		*dbip,
		      struct directory *dp,
		      genptr_t			ptr);

int
ged_killtree(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int i;
    int c;
    struct ged_killtree_data gktd;
    static const char *usage = "[-a|-n] object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    gktd.gedp = gedp;
    gktd.ac = 1;
    gktd.av[0] = "killrefs";
    gktd.av[1] = (char *)0;
    gktd.killrefs = 0;
    gktd.nflag = 0;

    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, "an")) != EOF) {
	switch( c ) {
	    case 'a':
		gktd.killrefs = 1;
		break;
	    case 'n':
		gktd.nflag = 1;
		gktd.av[gktd.ac++] = bu_strdup("-n");
		gktd.av[gktd.ac] = (char *)0;
		break;
	    default:
		bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return GED_ERROR;
	}
    }

    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    /* Objects that would be killed are in the first sublist */
    if (gktd.nflag)
	bu_vls_printf(&gedp->ged_result_str, "{");

    for (i=1; i<argc; i++) {
	if ((dp = db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_NOISY)) == DIR_NULL)
	    continue;

	/* ignore phony objects */
	if (dp->d_addr == RT_DIR_PHONY_ADDR)
	    continue;

	db_functree(gedp->ged_wdbp->dbip, dp,
		    ged_killtree_callback, ged_killtree_callback,
		    gedp->ged_wdbp->wdb_resp, (genptr_t)&gktd);
    }

    /* Close the sublist of would-be killed objects. Also open the
     * sublist of objects that reference the would-be killed objects.
     */
    if (gktd.nflag)
	bu_vls_printf(&gedp->ged_result_str, "} {");

    if (gktd.killrefs && gktd.ac > 1) {
	gedp->ged_internal_call = 1;
	(void)ged_killrefs(gedp, gktd.ac, (const char **)gktd.av);
	gedp->ged_internal_call = 0;

	for (i=1; i<gktd.ac; i++) {
	    if (!gktd.nflag)
		bu_vls_printf(&gedp->ged_result_str, "Freeing %s\n", gktd.av[i]);
	    bu_free((genptr_t)gktd.av[i], "ged_killtree_data");
	}
    }

    if (gktd.nflag)
	bu_vls_printf(&gedp->ged_result_str, "}");

    return GED_OK;
}

/*
 *			K I L L T R E E
 */
static void
ged_killtree_callback(struct db_i		*dbip,
		      struct directory *dp,
		      genptr_t			ptr)
{
    struct ged_killtree_data *gktdp = (struct ged_killtree_data *)ptr;

    if (dbip == DBI_NULL)
	return;

    if (gktdp->nflag) {
	if (!gktdp->killrefs) 
	    bu_vls_printf(&gktdp->gedp->ged_result_str, "%s ", dp->d_namep);
	else {
	    if (gktdp->ac < MAXARGS-1) {
		gktdp->av[gktdp->ac++] = bu_strdup(dp->d_namep);
		gktdp->av[gktdp->ac] = (char *)0;

		bu_vls_printf(&gktdp->gedp->ged_result_str, "%s ", dp->d_namep);
	    } else {
		bu_vls_printf(&gktdp->gedp->ged_result_str, "MAXARGS exceeded while scheduling %s for a killrefs\n", dp->d_namep);
	    }
	}
    } else {
	_ged_eraseAllNamesFromDisplay(gktdp->gedp, dp->d_namep, 0);

	bu_vls_printf(&gktdp->gedp->ged_result_str, "KILL %s:  %s\n",
		      (dp->d_flags & DIR_COMB) ? "COMB" : "Solid",
		      dp->d_namep);

	if (!gktdp->killrefs) {
	    if (db_delete(dbip, dp) != 0 || db_dirdelete(dbip, dp) != 0) {
		bu_vls_printf(&gktdp->gedp->ged_result_str, "an error occurred while deleting %s\n", dp->d_namep);
	    }
	} else {
	    if (gktdp->ac < MAXARGS-1) {
		gktdp->av[gktdp->ac++] = bu_strdup(dp->d_namep);
		gktdp->av[gktdp->ac] = (char *)0;

		if (db_delete(dbip, dp) != 0 || db_dirdelete(dbip, dp) != 0) {
		    bu_vls_printf(&gktdp->gedp->ged_result_str, "an error occurred while deleting %s\n", dp->d_namep);

		    /* Remove from list */
		    bu_free((genptr_t)gktdp->av[--gktdp->ac], "ged_killtree_callback");
		    gktdp->av[gktdp->ac] = (char *)0;
		}
	    } else {
		bu_vls_printf(&gktdp->gedp->ged_result_str, "MAXARGS exceeded while scheduling %s for a killrefs\n", dp->d_namep);

		if (db_delete(dbip, dp) != 0 || db_dirdelete(dbip, dp) != 0) {
		    bu_vls_printf(&gktdp->gedp->ged_result_str, "an error occurred while deleting %s\n", dp->d_namep);
		}
	    }
	}
    }
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

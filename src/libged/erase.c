/*                         E R A S E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
/** @file erase.c
 *
 * The erase command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "solid.h"

#include "./ged_private.h"


/*
 * Erase objects from the display.
 *
 * Usage:
 *        erase object(s)
 *
 */
int
ged_erase(struct ged *gedp, int argc, const char *argv[])
{
    register int i;
    static const char *usage = "objects(s)";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    for (i = 1; i < argc; ++i)
	ged_erasePathFromDisplay(gedp, argv[i]);

    return GED_OK;
}

/*
 * Erase/remove the display list item from headDisplay if path matches the list item's path.
 *
 */
void
ged_erasePathFromDisplay(struct ged *gedp,
			 const char *path)
{
    register struct ged_display_list *gdlp;
    register struct ged_display_list *next_gdlp;
    register struct solid *sp;
    struct directory *dp;

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	if (!strcmp(path, bu_vls_addr(&gdlp->gdl_path))) {
	    /* Free up the solids list associated with this display list */
	    while (BU_LIST_WHILE(sp, solid, &gdlp->gdl_headSolid)) {
		dp = FIRST_SOLID(sp);
		RT_CK_DIR(dp);
		if (dp->d_addr == RT_DIR_PHONY_ADDR) {
		    if (db_dirdelete(gedp->ged_wdbp->dbip, dp) < 0) {
			bu_vls_printf(&gedp->ged_result_str, "ged_zap: db_dirdelete failed\n");
		    }
		}

		BU_LIST_DEQUEUE(&sp->l);
		FREE_SOLID(sp, &FreeSolid.l);
	    }

	    BU_LIST_DEQUEUE(&gdlp->l);
	    bu_vls_free(&gdlp->gdl_path);
	    free((void *)gdlp);

	    break;
	}

	gdlp = next_gdlp;
    }
}

/*
 * Erase/remove display list item from headDisplay if name is found anywhere along item's path with
 * the exception that the first path element is skipped if skip_first is true.
 *
 * Note - name is not expected to contain path separators.
 * 
 */
void
ged_eraseAllNamesFromDisplay(struct ged *gedp,
			     const char *name,
			     const int skip_first)
{
    register struct ged_display_list *gdlp;
    register struct ged_display_list *next_gdlp;

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	char *dup;
	char *tok;
	int first = 1;
	int found = 0;

	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	dup = strdup(bu_vls_addr(&gdlp->gdl_path));
	tok = strtok(dup, "/");
	while (tok) {
	    if (first) {
		first = 0;

		if (skip_first) {
		    tok = strtok((char *)NULL, "/");
		    continue;
		}
	    }

	    if (!strcmp(tok, name)) {
		ged_freeDisplayListItem(gedp, gdlp);
		found = 1;

		break;
	    }

	    tok = strtok((char *)NULL, "/");
	}

	/* Look for name in solids list */
	if (!found) {
	    struct db_full_path subpath;

	    if (db_string_to_path(&subpath, gedp->ged_wdbp->dbip, name) == 0) {
		ged_eraseAllSubpathsFromSolidList(gedp, gdlp, &subpath, skip_first);
		db_free_full_path(&subpath);
	    }
	}

	free((void *)dup);
	gdlp = next_gdlp;
    }
}

/*
 * Erase/remove display list item from headDisplay if path is a subset of item's path.
 */
void
ged_eraseAllPathsFromDisplay(struct ged *gedp,
			     const char *path,
			     const int skip_first)
{
    register struct ged_display_list *gdlp;
    register struct ged_display_list *next_gdlp;
    struct db_full_path fullpath, subpath;

    if (db_string_to_path(&subpath, gedp->ged_wdbp->dbip, path) == 0) {
	gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
	while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	    next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	    if (db_string_to_path(&fullpath, gedp->ged_wdbp->dbip, bu_vls_addr(&gdlp->gdl_path)) == 0) {
		if (db_full_path_subset(&fullpath, &subpath, skip_first)) {
		    ged_freeDisplayListItem(gedp, gdlp);
		} else {
		    ged_eraseAllSubpathsFromSolidList(gedp, gdlp, &subpath, skip_first);
		}

		db_free_full_path(&fullpath);
	    }

	    gdlp = next_gdlp;
	}

	db_free_full_path(&subpath);
    }
}

void
ged_eraseAllSubpathsFromSolidList(struct ged *gedp,
				  struct ged_display_list *gdlp,
				  struct db_full_path *subpath,
				  const int skip_first)
{
    register struct solid *sp;
    register struct solid *nsp;

    sp = BU_LIST_NEXT(solid, &gdlp->gdl_headSolid);
    while (BU_LIST_NOT_HEAD(sp, &gdlp->gdl_headSolid)) {
	nsp = BU_LIST_PNEXT(solid, sp);
	if (db_full_path_subset(&sp->s_fullpath, subpath, skip_first)) {
	    BU_LIST_DEQUEUE(&sp->l);
	    FREE_SOLID(sp, &FreeSolid.l);
	}
	sp = nsp;
    }
}

void
ged_freeDisplayListItem (struct ged *gedp,
			 struct ged_display_list *gdlp)
{
    register struct solid *sp;
    struct directory *dp;

    /* Free up the solids list associated with this display list */
    while (BU_LIST_WHILE(sp, solid, &gdlp->gdl_headSolid)) {
	dp = FIRST_SOLID(sp);
	RT_CK_DIR(dp);
	if (dp->d_addr == RT_DIR_PHONY_ADDR) {
	    if (db_dirdelete(gedp->ged_wdbp->dbip, dp) < 0) {
		bu_vls_printf(&gedp->ged_result_str, "ged_zap: db_dirdelete failed\n");
	    }
	}

	BU_LIST_DEQUEUE(&sp->l);
	FREE_SOLID(sp, &FreeSolid.l);
    }

    /* Free up the display list */
    BU_LIST_DEQUEUE(&gdlp->l);
    bu_vls_free(&gdlp->gdl_path);
    free((void *)gdlp);
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

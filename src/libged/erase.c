/*                         E R A S E . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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

#include "ged_private.h"
#include "solid.h"


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
    int found = 0;
    int illum = 1;
    static const char *usage = "objects(s)";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* invalid command name */
    if (argc < 1) {
	bu_vls_printf(&gedp->ged_result_str, "Error: command name not provided");
	return BRLCAD_ERROR;
    }

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    ged_eraseobjpath(gedp, argc-1, argv+1, LOOKUP_QUIET, 0);

    return BRLCAD_OK;
}


/*
 * Builds an array of directory pointers from argv and calls
 * either ged_eraseobj or ged_eraseobjall.
 */
void
ged_eraseobjpath(struct ged	*gedp,
		 int		argc,
		 const char	*argv[],
		 int		noisy,
		 int		all)
{
    register struct directory *dp;
    register int i;

    for (i = 0; i < argc; i++) {
	int j;
	int ac, ac_orig;
	char **av, **av_orig;
	struct directory **dpp = (struct directory **)0;

	ac_orig = 1;

	{
	    char *begin;
	    char *end;
	    char *newstr = strdup(argv[i]);
	    int n;

	    /* First count the number of '/' */
	    begin = newstr;
	    while ((end = strchr(begin, '/')) != NULL) {
		begin = end + 1;
		++ac_orig;
	    }
	    av_orig = (char **)bu_calloc(ac_orig+1, sizeof(char *), "ged_eraseobjpath");

	    begin = newstr;
	    n = 0;
	    while ((end = strchr(begin, '/')) != NULL) {
		*end = '\0';
		av_orig[n++] = bu_strdup(begin);
		begin = end + 1;
	    }
	    av_orig[n++] = bu_strdup(begin);
	    av_orig[n] = (char *)0;
	    free((void *)newstr);
	}

	/* make sure we will not dereference null */
	if ((ac_orig == 0) || (av_orig == 0) || (*av_orig == 0)) {
	    bu_vls_printf(&gedp->ged_result_str, "WARNING: Asked to look up a null-named database object\n");
	    goto end;
	}

	/* skip first element if empty */
	ac = ac_orig;
	av = av_orig;

	if (*av[0] == '\0') {
	    --ac;
	    ++av;
	}

	/* ignore last element if empty */
	if (*av[ac-1] == '\0')
	    --ac;

	dpp = bu_calloc(ac+1, sizeof(struct directory *), "ged_eraseobjpath: directory pointers");
	for (j = 0; j < ac; ++j)
	    if ((dp = db_lookup(gedp->ged_wdbp->dbip, av[j], noisy)) != DIR_NULL)
		dpp[j] = dp;
	    else
		goto end;

	dpp[j] = DIR_NULL;

	if (all)
	    ged_eraseobjall(gedp, dpp);
	else
	    ged_eraseobj(gedp, dpp);

    end:
	bu_free((genptr_t)dpp, "ged_eraseobjpath: directory pointers");
	bu_free_argv(ac_orig, (char **)av_orig);
    }
}


/*
 *			E R A S E O B J A L L
 *
 * This routine goes through the solid table and deletes all solids
 * from the solid list which contain the specified object anywhere in their 'path'
 */
void
ged_eraseobjall(struct ged			*gedp,
		register struct directory	**dpp)
{
    register struct directory **tmp_dpp;
    register struct solid *sp;
    register struct solid *nsp;
    struct db_full_path	subpath;

    if (gedp->ged_wdbp->dbip == DBI_NULL)
	return;

    if (*dpp == DIR_NULL)
	return;

    db_full_path_init(&subpath);
    for (tmp_dpp = dpp; *tmp_dpp != DIR_NULL; ++tmp_dpp)  {
	RT_CK_DIR(*tmp_dpp);
	db_add_node_to_full_path(&subpath, *tmp_dpp);
    }

    sp = BU_LIST_NEXT(solid, &gedp->ged_gdp->gd_headSolid);
    while (BU_LIST_NOT_HEAD(sp, &gedp->ged_gdp->gd_headSolid)) {
	nsp = BU_LIST_PNEXT(solid, sp);
	if ( db_full_path_subset( &sp->s_fullpath, &subpath ) )  {
	    BU_LIST_DEQUEUE(&sp->l);
	    FREE_SOLID(sp, &FreeSolid.l);
	}
	sp = nsp;
    }

    if ((*dpp)->d_addr == RT_DIR_PHONY_ADDR) {
	if (db_dirdelete(gedp->ged_wdbp->dbip, *dpp) < 0) {
	    bu_vls_printf(&gedp->ged_result_str, "ged_eraseobjall: db_dirdelete failed\n");
	}
    }
    db_free_full_path(&subpath);
}

/*
 *			E R A S E O B J
 *
 * This routine goes through the solid table and deletes all solids
 * from the solid list which contain the specified object at the
 * beginning of their 'path'
 */
void
ged_eraseobj(struct ged			*gedp,
	     register struct directory	**dpp)
{
#if 1
    /*XXX
     * Temporarily put back the old behavior (as seen in Brlcad5.3),
     * as the behavior after the #else is identical to ged_eraseobjall.
     */
    register struct directory **tmp_dpp;
    register struct solid *sp;
    register struct solid *nsp;
    register int i;

    if (gedp->ged_wdbp->dbip == DBI_NULL)
	return;

    if (*dpp == DIR_NULL)
	return;

    for (tmp_dpp = dpp; *tmp_dpp != DIR_NULL; ++tmp_dpp)
	RT_CK_DIR(*tmp_dpp);

    sp = BU_LIST_FIRST(solid, &gedp->ged_gdp->gd_headSolid);
    while (BU_LIST_NOT_HEAD(sp, &gedp->ged_gdp->gd_headSolid)) {
	nsp = BU_LIST_PNEXT(solid, sp);
	for (i = 0, tmp_dpp = dpp;
	     i < sp->s_fullpath.fp_len && *tmp_dpp != DIR_NULL;
	     ++i, ++tmp_dpp)
	    if (sp->s_fullpath.fp_names[i] != *tmp_dpp)
		goto end;

	if (*tmp_dpp != DIR_NULL)
	    goto end;

	BU_LIST_DEQUEUE(&sp->l);
	FREE_SOLID(sp, &FreeSolid.l);
    end:
	sp = nsp;
    }

    if ((*dpp)->d_addr == RT_DIR_PHONY_ADDR ) {
	if (db_dirdelete(gedp->ged_wdbp->dbip, *dpp) < 0) {
	    bu_vls_printf(&gedp->ged_result_str, "ged_eraseobj: db_dirdelete failed\n");
	}
    }
#else
    register struct directory **tmp_dpp;
    register struct solid *sp;
    register struct solid *nsp;
    struct db_full_path	subpath;

    if (gedp->ged_wdbp->dbip == DBI_NULL)
	return;

    if (*dpp == DIR_NULL)
	return;

    db_full_path_init(&subpath);
    for (tmp_dpp = dpp; *tmp_dpp != DIR_NULL; ++tmp_dpp)  {
	RT_CK_DIR(*tmp_dpp);
	db_add_node_to_full_path(&subpath, *tmp_dpp);
    }

    sp = BU_LIST_FIRST(solid, &gedp->ged_gdp->gd_headSolid);
    while (BU_LIST_NOT_HEAD(sp, &gedp->ged_gdp->gd_headSolid)) {
	nsp = BU_LIST_PNEXT(solid, sp);
	if ( db_full_path_subset( &sp->s_fullpath, &subpath ) )  {
	    BU_LIST_DEQUEUE(&sp->l);
	    FREE_SOLID(sp, &FreeSolid.l);
	}
	sp = nsp;
    }

    if ((*dpp)->d_addr == RT_DIR_PHONY_ADDR ) {
	if (db_dirdelete(gedp->ged_wdbp->dbip, *dpp) < 0) {
	    bu_vls_printf(&gedp->ged_result_str, "ged_eraseobj: db_dirdelete failed\n");
	}
    }
    db_free_full_path(&subpath);
#endif
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

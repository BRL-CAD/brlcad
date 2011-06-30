/*                         T O P S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/tops.c
 *
 * The tops command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_tops(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int i;
    struct directory **dirp;
    struct directory **dirp0 = (struct directory **)NULL;
    int c;

    int no_decorate = 0;
    int aflag = 0;
    int hflag = 0;
    int pflag = 0;

    /* DEPRECATED */
    int gflag = 0;
    /* DEPRECATED */
    int uflag = 0;

    /* static const char *usage = "[-a|-h|-n|-p]"; */

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* process any options */
    bu_optind = 1;	/* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "ahnpgu")) != -1) {
	switch (c) {
	    case 'a':
		aflag = 1;
		break;
	    case 'h':
		hflag = 1;
		break;
	    case 'n':
		no_decorate = 1;
		break;
	    case 'p':
		pflag = 1;
		break;
	    case 'g':
		bu_log("WARNING: The -g option is deprecated.\n");
		gflag = 1;
		break;
	    case 'u':
		bu_log("WARNING: The -u option is deprecated.\n");
		uflag = 1;
		break;
	    default:
		break;
	}
    }

    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    /* Can this be executed only sometimes?
       Perhaps a "dirty bit" on the database? */
    db_update_nref(gedp->ged_wdbp->dbip, &rt_uniresource);

    /*
     * Find number of possible entries and allocate memory
     */
    dirp = _ged_dir_getspace(gedp->ged_wdbp->dbip, 0);
    dirp0 = dirp;

    if (db_version(gedp->ged_wdbp->dbip) < 5) {
	for (i = 0; i < RT_DBNHASH; i++)
	    for (dp = gedp->ged_wdbp->dbip->dbi_Head[i];
		 dp != RT_DIR_NULL;
		 dp = dp->d_forw) {
		if (dp->d_nref == 0)
		    *dirp++ = dp;
	    }
    } else {
	for (i = 0; i < RT_DBNHASH; i++)
	    for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {

		if (dp->d_nref != 0) {
		    continue;
		}

		if ((aflag) ||
		    (hflag && (dp->d_flags & RT_DIR_HIDDEN)) ||
		    (pflag && dp->d_addr == RT_DIR_PHONY_ADDR) ||
		    (gflag && dp->d_major_type == DB5_MAJORTYPE_BRLCAD) ||
		    (uflag && !(dp->d_flags & RT_DIR_HIDDEN))) {

		    /* add object because it matches an option */
		    *dirp++ = dp;

		} else if (!aflag && !hflag && !pflag && !gflag && !uflag &&
			   !(dp->d_flags & RT_DIR_HIDDEN) &&
			   (dp->d_addr != RT_DIR_PHONY_ADDR)) {

		    /* add non-hidden real object */
		    *dirp++ = dp;

		}
	    }
    }

    _ged_vls_col_pr4v(gedp->ged_result_str, dirp0, (int)(dirp - dirp0), no_decorate);
    bu_free((genptr_t)dirp0, "wdb_tops_cmd: wdb_dir_getspace");

    return GED_OK;
}


/*
 * G E D _ D I R _ G E T S P A C E
 *
 * This routine walks through the directory entry list and mallocs enough
 * space for pointers to hold:
 * a) all of the entries if called with an argument of 0, or
 * b) the number of entries specified by the argument if > 0.
 */
struct directory **
_ged_dir_getspace(struct db_i *dbip,
		  int num_entries)
{
    struct directory *dp;
    int i;
    struct directory **dir_basep;

    if (num_entries < 0) {
	bu_log("dir_getspace: was passed %d, used 0\n",
	       num_entries);
	num_entries = 0;
    }
    if (num_entries == 0) {
	/* Set num_entries to the number of entries */
	for (i = 0; i < RT_DBNHASH; i++)
	    for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw)
		num_entries++;
    }

    /* Allocate and cast num_entries worth of pointers */
    dir_basep = (struct directory **) bu_malloc((num_entries+1) * sizeof(struct directory *),
						"dir_getspace *dir[]");
    return dir_basep;
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

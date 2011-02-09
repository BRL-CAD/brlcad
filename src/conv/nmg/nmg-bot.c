/*                       N M G - B O T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file nmg-bot.c
 *
 * Routine to convert all the NMG solids in a BRL-CAD model to BoTs.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "vmath.h"
#include "db.h"
#include "bu.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"


static struct db_i *dbip;
static int verbose=0;
static struct rt_wdb *fdout=NULL;

static void
nmg_conv(struct rt_db_internal *intern, const char *name)
{
    struct model *m;
    struct nmgregion *r;
    struct shell *s;

    RT_CK_DB_INTERNAL(intern);
    m = (struct model *)intern->idb_ptr;
    NMG_CK_MODEL(m);
    r = BU_LIST_FIRST(nmgregion, &m->r_hd);
    if (r && BU_LIST_NEXT(nmgregion, &r->l) !=  (struct nmgregion *)&m->r_hd)
	bu_exit(1, "ERROR: this code works only for NMG models with one region!\n");

    s = BU_LIST_FIRST(shell, &r->s_hd);
    if (s && BU_LIST_NEXT(shell, &s->l) != (struct shell *)&r->s_hd)
	bu_exit(1, "ERROR: this code works only for NMG models with one shell!\n");

    if (!s) {
	bu_log("WARNING: NMG has no shells\n");
	return;
    }

    if (!BU_SETJUMP) {
	/* try */
	mk_bot_from_nmg(fdout, name, s);
    } else {
	/* catch */
	BU_UNSETJUMP;
	bu_log("Failed to convert %s\n", name);
	return;
    } BU_UNSETJUMP;

    if (verbose) bu_log("Converted %s to a Bot solid\n", name);
}


int
main(int argc, char **argv)
{
    struct directory *dp;

    if (argc != 3 && argc != 4) {
	bu_exit(1, "Usage:\n\t%s [-v] input.g output.g\n", argv[0]);
    }

    if (argc == 4) {
	if (BU_STR_EQUAL(argv[1], "-v"))
	    verbose = 1;
	else {
	    bu_log("Illegal option: %s\n", argv[1]);
	    bu_exit(1, "Usage:\n\t%s [-v] input.g output.g\n", argv[0]);
	}
    }

    rt_init_resource(&rt_uniresource, 0, NULL);

    dbip = db_open(argv[argc-2], "r");
    if (dbip == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "Cannot open file (%s)\n", argv[argc-2]);
    }

    if ((fdout=wdb_fopen(argv[argc-1])) == NULL) {
	perror(argv[0]);
	bu_exit(1, "Cannot open file (%s)\n", argv[argc-1]);
    }
    if (db_dirbuild(dbip)) {
	bu_exit(1, "db_dirbuild failed\n");
    }

    /* Visit all records in input database, and spew them out,
     * modifying NMG objects into BoTs.
     */
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	struct rt_db_internal intern;
	int id;
	int ret;
	id = rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);
	if (id < 0) {
	    fprintf(stderr,
		    "%s: rt_db_get_internal(%s) failure, skipping\n",
		    argv[0], dp->d_namep);
	    continue;
	}
	if (id == ID_NMG) {
	    nmg_conv(&intern, dp->d_namep);
	} else {
	    ret = wdb_put_internal(fdout, dp->d_namep, &intern, 1.0);
	    if (ret < 0) {
		fprintf(stderr,
			"%s: wdb_put_internal(%s) failure, skipping\n",
			argv[0], dp->d_namep);
		rt_db_free_internal(&intern);
		continue;
	    }
	}
	rt_db_free_internal(&intern);
    } FOR_ALL_DIRECTORY_END
	  wdb_close(fdout);
    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

/*                    D B U P G R A D E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
/** @file conv/dbupgrade.c
 *
 * This is a program to upgrade database files to the current version.
 *
 * DBUPGRADE takes one input file and one output file name.  It
 * recognizes the BRL-CAD database version provided as input and
 * converts it at specified.  DBUPGRADE also upgrades database
 * structures in current database versions.
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/getopt.h"
#include "bn.h"
#include "rt/geom.h"
#include "raytrace.h"


void
usage(const char *name)
{
    bu_log("Usage: %s [-f] input.g output.g\n", name);
}


int
main(int argc, char **argv)
{
    char name[17] = {0};
    const char *argv0 = argv[0];
    int flag_force = 0;
    int flag_revert = 0;
    int i;
    int in_arg;
    int out_arg;
    int in_version;
    long errors = 0, skipped = 0;
    struct bn_tol tol;
    struct bu_vls colortab = BU_VLS_INIT_ZERO;
    struct db_i *dbip4;
    struct db_i *dbip;
    struct directory *dp;
    struct rt_wdb *fp;

    bu_setprogname(argv0);

    /* this tolerance structure is only used for converting polysolids to BOT's
     * use zero distance to avoid losing any polysolid facets
     */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    while ((i = bu_getopt(argc, argv, "rfh?")) != -1) {
	switch (i) {
	    case 'r':
		/* undocumented option to revert to an old db version
		 * currently, can only revert to db version 4
		 */
		flag_revert = 1;
		break;
	    case 'f':
		flag_force = 1;
		break;
	    default:
		usage(argv0);
		return 1;
	}
    }
    argc -= bu_optind;
    argv += bu_optind;

    if (argc != 2) {
	usage(argv0);
	return 1;
    }

    in_arg = 0;
    out_arg = 1;

    if (!bu_file_exists(argv[in_arg], NULL)) {
	bu_exit(1, "ERROR: Input file (%s) does not exist\n", argv[in_arg]);
    } else if (bu_file_exists(argv[out_arg], NULL)) {
	bu_exit(1, "ERROR: Output file (%s) already exists\n", argv[out_arg]);
    }

    if ((dbip = db_open(argv[in_arg], DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[in_arg]);
	return 2;
    }
    in_version = db_version(dbip);

    if (!flag_revert) {
	if (in_version == 5 && !flag_force) {
	    bu_log("WARNING: This database (%s) is already at the current version.\n",
		   argv[in_arg]);
	    bu_log("         Run with -f to force upgrade (v5->v5) or -r to revert (v5->v4).\n");
	    bu_log("Halting.\n");
	    return 5;
	}
	if (in_version < 4) {
	    bu_log("ERROR: Input database version (%d) not recognized!\n", in_version);
	    return 4;
	}
	if ((fp = wdb_fopen(argv[out_arg])) == NULL) {
	    bu_log("ERROR: Failed to open the output database (%s)\n", argv[out_arg]);
	    perror(argv[out_arg]);
	    return 3;
	}
    } else {
	if (in_version != 5 && !flag_force) {
	    bu_log("WARNING: Can only revert from database version 5\n");
	    bu_log("         Run with -f and -r to force revert (v4->v4).\n");
	    bu_log("Halting.\n");
	    return 6;
	}
	if ((dbip4 = db_create(argv[out_arg], 4)) == DBI_NULL) {
	    bu_log("ERROR: Failed to create the output database (%s)\n", argv[out_arg]);
	    return 3;
	}
	if ((fp = wdb_dbopen(dbip4, RT_WDB_TYPE_DB_DISK)) == RT_WDB_NULL) {
	    bu_log("ERROR: Failed to open the v4 output database (%s)\n", argv[out_arg]);
	    return 4;
	}
    }

    RT_CK_DBI(dbip);
    if (db_dirbuild(dbip))
	bu_exit(1, "db_dirbuild failed\n");

    if ((BU_STR_EQUAL(dbip->dbi_title, "Untitled v4 BRL-CAD Database")) && (in_version == 4)) {
	dbip->dbi_title=bu_strdup("Untitled BRL-CAD Database");
    }
    db_update_ident(fp->dbip, dbip->dbi_title, dbip->dbi_local2base);

    /* set regionid color table */
    if (rt_material_head() != MATER_NULL) {
	rt_vls_color_map(&colortab);

	db5_update_attribute("_GLOBAL", "regionid_colortable", bu_vls_addr(&colortab), fp->dbip);
	bu_vls_free(&colortab);
    }


    /* Retrieve every item in the input database */
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	struct rt_db_internal intern;
	int id;
	int ret;

	if (flag_revert && dp->d_major_type != DB5_MAJORTYPE_BRLCAD) {
	    bu_log("\t%s not supported in version 4 databases, not converted\n",
		   dp->d_namep);
	    skipped++;
	    continue;
	}
	id = rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);
	if (id < 0) {
	    fprintf(stderr,
		    "%s: rt_db_get_internal(%s) failure, skipping\n",
		    argv[0], dp->d_namep);
	    errors++;
	    continue;
	}
	if (id == ID_COMBINATION) {
	    struct rt_comb_internal *comb;
	    char *ptr;

	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    RT_CK_COMB(comb);

	    /* TODO: update d/dir {} shader strings to target {} */
	    /* bu_log("!!!shader is [%s]", bu_vls_addr(&comb->shader)); */

	    /* Convert "plastic" to "phong" in the shader string */
	    while ((ptr=strstr(bu_vls_addr(&comb->shader), "plastic")) != NULL) {
		ptr[0] = 'p'; /* p */
		ptr[1] = 'h'; /* l */
		ptr[2] = 'o'; /* a */
		ptr[3] = 'n'; /* s */
		ptr[4] = 'g'; /* t */
		ptr[5] = ' '; /* i */
		ptr[6] = ' '; /* c */
	    }
	}
	if (id == ID_HF) {
	    if (rt_hf_to_dsp(&intern)) {
		fprintf(stderr,
			"%s: Conversion from HF to DSP failed for solid %s\n",
			argv[0], dp->d_namep);
		errors++;
		continue;
	    }
	}
	if (id == ID_POLY)
	{
	    if (rt_pg_to_bot(&intern, &tol, &rt_uniresource))
	    {
		fprintf(stderr, "%s: Conversion from polysolid to BOT failed for solid %s\n",
			argv[0], dp->d_namep);
		errors++;
		continue;
	    }
	}

	/* to insure null termination */
	bu_strlcpy(name, dp->d_namep, sizeof(name));
	ret = wdb_put_internal(fp, name, &intern, 1.0);
	if (ret < 0) {
	    fprintf(stderr,
		    "%s: wdb_put_internal(%s) failure, skipping\n",
		    argv[0], dp->d_namep);
	    rt_db_free_internal(&intern);
	    errors++;
	    continue;
	}
	rt_db_free_internal(&intern);
    } FOR_ALL_DIRECTORY_END

	  wdb_close(fp);
    db_close(dbip);

    fprintf(stderr, "%ld objects failed to convert\n", errors);
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

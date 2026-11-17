/*                    D B U P G R A D E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
#include "bu/arg.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bn.h"
#include "rt/geom.h"
#include "raytrace.h"


struct dbupgrade_args {
    int help;
    int force;
    int revert;
};

static const struct bu_arg_option dbupgrade_options[] = {
    BU_ARG_FLAG("h", "help", struct dbupgrade_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("f", "force", struct dbupgrade_args, force,
	"Force conversion when the database version normally prevents it"),
    BU_ARG_FLAG("r", "revert", struct dbupgrade_args, revert,
	"Write a version 4 database instead of upgrading to version 5"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand dbupgrade_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("output", BU_ARG_VALUE_FILE, 1, 1,
	"Output BRL-CAD database", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema dbupgrade_schema = BU_ARG_SCHEMA(
    "dbupgrade", "Upgrade or revert a BRL-CAD geometry database",
    dbupgrade_options, dbupgrade_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar dbupgrade_grammar = {
    &dbupgrade_schema, NULL, NULL
};


int
main(int argc, char **argv)
{
    struct dbupgrade_args args = {0, 0, 0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    char name[17] = {0};
    const char *input;
    const char *output;
    int in_version;
    int introspection;
    int operand_index;
    long errors = 0;
    struct bn_tol tol;
    struct bu_vls colortab = BU_VLS_INIT_ZERO;
    struct db_i *dbip4 = NULL;
    struct db_i *dbip = NULL;
    struct directory *dp = NULL;
    struct rt_wdb *fp = NULL;

    bu_setprogname(argv[0]);

    introspection = bu_arg_introspect(&msg, &dbupgrade_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&dbupgrade_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_log("%s\n", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&dbupgrade_schema, "dbupgrade");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "dbupgrade help");
	return 0;
    }
    input = argv[operand_index + 1];
    output = argv[operand_index + 2];

    /* this tolerance structure is only used for converting polysolids to BOT's
     * use zero distance to avoid losing any polysolid facets
     */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    if (!bu_file_exists(input, NULL)) {
	bu_exit(1, "ERROR: Input file (%s) does not exist\n", input);
    } else if (bu_file_exists(output, NULL)) {
	bu_exit(1, "ERROR: Output file (%s) already exists\n", output);
    }

    if ((dbip = db_open(input, DB_OPEN_READONLY)) == DBI_NULL) {
	perror(input);
	return 2;
    }
    in_version = db_version(dbip);

    if (!args.revert) {
	if (in_version == 5 && !args.force) {
	    bu_log("WARNING: This database (%s) is already at the current version.\n",
		   input);
	    bu_log("         Run with -f to force upgrade (v5->v5) or -r to revert (v5->v4).\n");
	    bu_log("Halting.\n");
	    return 5;
	}
	if (in_version < 4) {
	    bu_log("ERROR: Input database version (%d) not recognized!\n", in_version);
	    return 4;
	}
	if ((fp = wdb_fopen(output)) == NULL) {
	    bu_log("ERROR: Failed to open the output database (%s)\n", output);
	    perror(output);
	    return 3;
	}
    } else {
	if (in_version != 5 && !args.force) {
	    bu_log("WARNING: Can only revert from database version 5\n");
	    bu_log("         Run with -f and -r to force revert (v4->v4).\n");
	    bu_log("Halting.\n");
	    return 6;
	}
	if ((dbip4 = db_create(output, 4)) == DBI_NULL) {
	    bu_log("ERROR: Failed to create the output database (%s)\n", output);
	    return 3;
	}
	if ((fp = wdb_dbopen(dbip4, RT_WDB_TYPE_DB_DISK)) == RT_WDB_NULL) {
	    bu_log("ERROR: Failed to open the v4 output database (%s)\n", output);
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
    if (db_mater_head(dbip) != MATER_NULL) {
	db_mater_to_vls(&colortab, dbip);

	db5_update_attribute("_GLOBAL", "regionid_colortable", bu_vls_addr(&colortab), fp->dbip);
	bu_vls_free(&colortab);
    }


    /* Retrieve every item in the input database */
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	struct rt_db_internal intern;
	int id;
	int ret;

	if (args.revert && dp->d_major_type != DB5_MAJORTYPE_BRLCAD) {
	    bu_log("\t%s not supported in version 4 databases, not converted\n",
		   dp->d_namep);
	    continue;
	}
	id = rt_db_get_internal(&intern, dp, dbip, NULL);
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
	    if (rt_pg_to_bot(&intern, &tol))
	    {
		fprintf(stderr, "%s: Conversion from polysolid to BOT failed for solid %s\n",
			argv[0], dp->d_namep);
		errors++;
		continue;
	    }
	}
	if (id == ID_BSPLINE) {
	    if (rt_nurb_to_brep(&intern)) {
		fprintf(stderr,
			"%s: Conversion from BSPLINE to BREP failed for solid %s, keeping original\n",
			argv[0], dp->d_namep);
		/* Non-fatal: fall through and write the original BSPLINE */
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

    db_close(dbip4);
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

/*                         M A K E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/make.c
 *
 * The make command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>


#include "bu/getopt.h"
#include "bu/interrupt.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../ged_private.h"


int
ged_make_core(struct ged *gedp, int argc, const char *argv[])
{
    int k;
    int save_bu_optind;
    struct directory *dp;

    /* intentionally double for sscanf */
    double scale = 1.0;
    double origin[3] = {0.0, 0.0, 0.0};

    struct rt_db_internal internal;

    /* intentionally not included: cline */
    static const char *usage = "-h | -t | -o origin -s sf name <arb8|arb7|arb6|arb5|arb4|arbn|ars|bot|brep|datum|ehy|ell|ell1|epa|eto|extrude|grip|half|hyp|nmg|part|pipe|pnts|rcc|rec|rhc|rpc|rpp|sketch|sph|tec|tgc|tor|trc>";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    bu_optind = 1;

    /* Process arguments */
    while ((k = bu_getopt(argc, (char * const *)argv, "hHo:O:s:S:tT?")) != -1) {
	if (bu_optopt == '?') k='h';
	switch (k) {
	    case 'o':
	    case 'O':
		if (sscanf(bu_optarg, "%lf %lf %lf",
			   &origin[X],
			   &origin[Y],
			   &origin[Z]) != 3) {
		    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		    return BRLCAD_ERROR;
		}
		break;
	    case 's':
	    case 'S':
		if (sscanf(bu_optarg, "%lf", &scale) != 1) {
		    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		    return BRLCAD_ERROR;
		}
		break;
	    case 't':
	    case 'T':
		if (argc == 2) {
		    /* intentionally not included: cline */
		    bu_vls_printf(gedp->ged_result_str, "arb8 arb7 arb6 arb5 arb4 arbn ars bot brep datum ehy ell ell1 epa eto extrude grip half hyp nmg part pipe pnts rcc rec rhc rpc rpp sketch sph tec tgc tor trc superell metaball");
		    return GED_HELP;
		}

		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return BRLCAD_ERROR;
	    case 'h':
	    case 'H':
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return GED_HELP;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return BRLCAD_ERROR;
	}
    }

    argc -= bu_optind;

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    save_bu_optind = bu_optind;

    GED_CHECK_EXISTS(gedp, argv[bu_optind], LOOKUP_QUIET, BRLCAD_ERROR);
    RT_DB_INTERNAL_INIT(&internal);

    if (BU_STR_EQUAL(argv[bu_optind+1], "hf")) {
	bu_vls_printf(gedp->ged_result_str, "make: the height field is deprecated and not supported by this command.\nUse the dsp primitive.\n");
	return BRLCAD_ERROR;
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "pg") ||
	       BU_STR_EQUAL(argv[bu_optind+1], "poly")) {
	bu_vls_printf(gedp->ged_result_str, "make: the polysolid is deprecated and not supported by this command.\nUse the bot primitive.");
	return BRLCAD_ERROR;
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "cline") ||
	       BU_STR_EQUAL(argv[bu_optind+1], "dsp") ||
	       BU_STR_EQUAL(argv[bu_optind+1], "ebm") ||
	       BU_STR_EQUAL(argv[bu_optind+1], "nurb") ||
	       BU_STR_EQUAL(argv[bu_optind+1], "spline") ||
	       BU_STR_EQUAL(argv[bu_optind+1], "submodel") ||
	       BU_STR_EQUAL(argv[bu_optind+1], "vol")) {
	bu_vls_printf(gedp->ged_result_str, "make: the %s primitive is not supported by this command", argv[bu_optind+1]);
	return BRLCAD_ERROR;
    } else if (rt_obj_make(argv[bu_optind+1], origin, scale, &internal) == BRLCAD_OK) {
	bu_log("SUCCESS for type %s\n", argv[bu_optind+1]);
    } else {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* TODO/FIXME: extrude relies on a sketch. Create said sketch as a post-process check.
     * is this how we want to handle this, should extrude's ft_make create it's own sketch,
     * or should an extrude without a sketch be legal?
     */
    if (BU_STR_EQUAL(argv[bu_optind+1], "extrude")) {
	char *av[8];
	char center_str[512];
	char scale_str[128];
	struct rt_extrude_internal* extrude_ip = (struct rt_extrude_internal *)internal.idb_ptr;

	/* sanity */
	if (!extrude_ip)
	    return BRLCAD_ERROR;

	/* attach a sketch name to the extrude */
	av[0] = "make_name";
	av[1] = "skt_";
	ged_exec_make_name(gedp, 2, (const char **)av);
	if (extrude_ip->sketch_name)
	    bu_free(extrude_ip->sketch_name, "empty sketch_name");
	extrude_ip->sketch_name = bu_strdup(bu_vls_addr(gedp->ged_result_str));

	sprintf(center_str, "%f %f %f", V3ARGS(origin));
	sprintf(scale_str, "%f", scale);
	av[0] = "make";
	av[1] = "-o";
	av[2] = center_str;
	av[3] = "-s";
	av[4] = scale_str;
	av[5] = extrude_ip->sketch_name;
	av[6] = "sketch";
	av[7] = (char *)0;
	/* TODO: should probably validate the sketch was made successfully */
	ged_make_core(gedp, 7, (const char **)av);
    }

    /* no interrupts */
    (void)signal(SIGINT, SIG_IGN);

    GED_DB_DIRADD(gedp, dp, argv[save_bu_optind], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERN(gedp, dp, &internal, BRLCAD_ERROR);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_MAKE_COMMANDS(X, XID) \
    X(make, ged_make_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_MAKE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_make", 1, GED_MAKE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

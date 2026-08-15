/*                         B B . C
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
/** @file libged/bb.c
 *
 * The bb command - report the dimensions of the bounding arb8
 * for a given object and optionally create an arb8 with those
 * dimensions.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/opt.h"
#include "bu/units.h"
#include "rt/geom.h"

#include "../ged_private.h"


struct bb_args {
    const char *bb_name;
    int print_dim;
    int print_midpt;
    int print_rpp;
    int quiet;
    int exclude_air;
    int print_vol;
    int oriented;
};

#define BB_OPTIONS(args) \
    BU_OPT_STR(args, "c", NULL, bb_name, "name", "Create a bounding-box object with this name"), \
    BU_OPT_FLAG(args, "d", NULL, print_dim, "Print dimensions"), \
    BU_OPT_FLAG(args, "m", NULL, print_midpt, "Print midpoint"), \
    BU_OPT_FLAG(args, "e", NULL, print_rpp, "Print extents"), \
    BU_OPT_FLAG(args, "q", NULL, quiet, "Suppress the header"), \
    BU_OPT_FLAG(args, "u", NULL, exclude_air, "Exclude air regions"), \
    BU_OPT_FLAG(args, "v", NULL, print_vol, "Print volume"), \
    BU_OPT_FLAG(args, "o", NULL, oriented, "Compute an oriented bounding box"),

BU_OPT_DESC_BUILDER(bb_options, struct bb_args, BB_OPTIONS);

static const ged_opt_rule bb_opt_rules[] = {
    GED_RULE_OPERANDS(BU_CMD_CONDITION_ANY_OPTION_PRESENT, "o", 1, 1,
	"-o requires exactly one BoT object"),
    GED_RULE_NULL
};
static const ged_opt_spec bb_opt_spec =
    GED_OPT_WITH("bb", "Report or create object bounds", bb_options,
	"interspersed objects:object+", bb_opt_rules);

static void
bb_show_help(struct ged *gedp, const char *command)
{
    char *help = ged_cmd_help(command, command);

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "bb standard help");
    }
}


int
ged_bb_core(struct ged *gedp, int argc, const char *argv[])
{
    point_t rpp_min, rpp_max;
    int c;
    int use_air = 1;
    int print_header = 1;
    int print_rpp = 0;
    int print_dim = 0;
    int print_midpt = 0;
    int print_vol = 0;
    int make_bb = 0;
    int oriented_bb = 0;
    int tight_bb = 0;
    int i;
    static const char *usage = "[-c name] [-d] [-m] [-e] [-q] [-u] [-v] [-o] [-t] object1 [object2 object3 ...]";
    const char *str;
    double xlen;
    double ylen;
    double zlen;
    double vol;
    char bbname[64];

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	bb_show_help(gedp, argv[0]);
	return GED_HELP;
    }

    bu_optind = 1;      /* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "c:dmequvot")) != -1) {
	switch (c) {
	    case 'c':
		make_bb = 1;
		memset(bbname, 0, 64);
		bu_strlcpy(bbname, bu_optarg, 64);
		break;
	    case 'd':
		print_dim = 1;
		break;
	    case 'm':
		print_midpt = 1;
		break;
	    case 'e':
		print_rpp = 1;
		break;
	    case 'q':
		print_header = 0;
		break;
	    case 'u':
		use_air = 0;
		break;
	    case 'v':
		print_vol = 1;
		break;
	    case 'o':
		oriented_bb = 1;
		break;
	    case 't':
		tight_bb = 1;
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Unrecognized option - %c", c);
		return BRLCAD_ERROR;
	}
    }
    make_bb = args.bb_name != NULL;
    bbname = args.bb_name;
    print_dim = args.print_dim;
    print_midpt = args.print_midpt;
    print_rpp = args.print_rpp;
    print_header = !args.quiet;
    use_air = !args.exclude_air;
    print_vol = args.print_vol;
    oriented_bb = args.oriented;

    /* Don't want to print NO info, so default to dim and vol printout if nothing specified. */
    if (print_rpp == 0 && print_vol == 0 && print_dim == 0 && print_midpt == 0) {
	print_dim = 1;
	print_vol = 1;
    }

    if (!object_count || (oriented_bb && object_count != 1)) {
	bb_show_help(gedp, argv[0]);
	return object_count ? BRLCAD_ERROR : GED_HELP;
    }
    argc = object_count;
    argv += 1;

    if (!oriented_bb) {
	const int bounds_ret = tight_bb ?
	    _ged_obj_tight_bounds(gedp, argc, (const char **)argv, use_air,
		rpp_min, rpp_max) :
	    rt_obj_bounds(gedp->ged_result_str, gedp->dbip, argc,
		(const char **)argv, use_air, rpp_min, rpp_max);
	if (bounds_ret & BRLCAD_ERROR)
	    return BRLCAD_ERROR;

	/* Report Bounding Box Information */
	str = bu_units_string(gedp->dbip->dbi_local2base);
	if (!str) str = "Unknown_unit";

	/* Print Header */
	if (print_header == 1) {
	    bu_vls_printf(gedp->ged_result_str, "Bounding Box Dimensions, Object(s)");
	    for (i = 0; i < argc - 1 ; i++) {
		bu_vls_printf(gedp->ged_result_str, " %s, ", argv[i]);
	    }
	    bu_vls_printf(gedp->ged_result_str, " %s:\n", argv[argc-1]);
	}

	/* Print rpp */
	if (print_rpp == 1) {
	    bu_vls_printf(gedp->ged_result_str, "min {%f %f %f} max {%f %f %f}\n", rpp_min[0], rpp_min[1], rpp_min[2], rpp_max[0], rpp_max[1], rpp_max[2]);
	}

	/* Print dim info */
	if (print_dim == 1) {
	    xlen = fabs(rpp_max[X] - rpp_min[X])*gedp->dbip->dbi_base2local;
	    ylen = fabs(rpp_max[Y] - rpp_min[Y])*gedp->dbip->dbi_base2local;
	    zlen = fabs(rpp_max[Z] - rpp_min[Z])*gedp->dbip->dbi_base2local;
	    bu_vls_printf(gedp->ged_result_str, "X Length: %g %s\nY Length: %g %s\nZ Length: %g %s\n", xlen, str, ylen, str, zlen, str);
	}

	/* print midpoint */
	if (print_midpt == 1) {
	    point_t midpt;
	    VADD2SCALE(midpt, rpp_min, rpp_max, 0.5);
	    bu_vls_printf(gedp->ged_result_str, "Mid Point: (%f %f %f)\n", midpt[X], midpt[Y], midpt[Z]);
	}

	if (print_vol == 1) {
	    xlen = fabs(rpp_max[X] - rpp_min[X])*gedp->dbip->dbi_base2local;
	    ylen = fabs(rpp_max[Y] - rpp_min[Y])*gedp->dbip->dbi_base2local;
	    zlen = fabs(rpp_max[Z] - rpp_min[Z])*gedp->dbip->dbi_base2local;
	    vol = xlen * ylen * zlen;
	    bu_vls_printf(gedp->ged_result_str, "Bounding Box Volume: %g %s^3\n", vol, str);
	}

	if (make_bb == 1) {
	    struct directory *dp;
	    struct rt_arb_internal *arb;
	    struct rt_db_internal new_intern;

	    BU_ALLOC(arb, struct rt_arb_internal);
	    VMOVE(arb->pt[0], rpp_min);
	    VSET(arb->pt[1], rpp_min[X], rpp_min[Y], rpp_max[Z]);
	    VSET(arb->pt[2], rpp_min[X], rpp_max[Y], rpp_max[Z]);
	    VSET(arb->pt[3], rpp_min[X], rpp_max[Y], rpp_min[Z]);
	    VSET(arb->pt[4], rpp_max[X], rpp_min[Y], rpp_min[Z]);
	    VSET(arb->pt[5], rpp_max[X], rpp_min[Y], rpp_max[Z]);
	    VMOVE(arb->pt[6], rpp_max);
	    VSET(arb->pt[7], rpp_max[X], rpp_max[Y], rpp_min[Z]);
	    arb->magic = RT_ARB_INTERNAL_MAGIC;

	    /* set up internal structure */
	    RT_DB_INTERNAL_INIT(&new_intern);
	    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	    new_intern.idb_type = ID_ARB8;
	    new_intern.idb_meth = &OBJ[ID_ARB8];
	    new_intern.idb_ptr = (void *)arb;

	    dp = db_diradd(gedp->dbip, bbname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&new_intern.idb_type);
	    if (dp == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "Cannot add %s to directory\n", bbname);
		return BRLCAD_ERROR;
	    }

	    if (rt_db_put_internal(dp, gedp->dbip, &new_intern) < 0) {
		rt_db_free_internal(&new_intern);
		bu_vls_printf(gedp->ged_result_str, "Database write error, aborting.\n");
	    }
	}
    } else {
	struct directory *dp;
	struct rt_arb_internal *arb;
	struct rt_db_internal new_intern;

	BU_ALLOC(arb, struct rt_arb_internal);
	arb->magic = RT_ARB_INTERNAL_MAGIC;

	/* set up internal structure */
	RT_DB_INTERNAL_INIT(&new_intern);
	new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	new_intern.idb_type = ID_ARB8;
	new_intern.idb_meth = &OBJ[ID_ARB8];
	new_intern.idb_ptr = (void *)arb;

	if (_ged_obj_oriented_bounds(gedp, argc, (const char **)argv, use_air,
		tight_bb, arb->pt) != BRLCAD_OK) {
	    bu_vls_printf(gedp->ged_result_str,
		"Error: Oriented bounding box calculation failed.\n");
	    rt_db_free_internal(&new_intern);
	    return BRLCAD_ERROR;
	}


	/* Report Bounding Box Information */
	str = bu_units_string(gedp->dbip->dbi_local2base);
	if (!str) str = "Unknown_unit";

	/* Print Header */
	if (print_header == 1) {
	    bu_vls_printf(gedp->ged_result_str, "Bounding Box Dimensions, Object(s)");
	    for (i = 0; i < argc - 1 ; i++) {
		bu_vls_printf(gedp->ged_result_str, " %s, ", argv[i]);
	    }
	    bu_vls_printf(gedp->ged_result_str, " %s:\n", argv[argc-1]);
	}

	/* Print rpp */
	if (print_rpp == 1) {
	    for (i = 0; i < 8; ++i)
		bu_vls_printf(gedp->ged_result_str, "Point %d: %f, %f, %f\n",
		    i + 1, V3ARGS(arb->pt[i]));
	}

	/* Print dim info */
	if (print_dim == 1) {
	    xlen = DIST_PNT_PNT(arb->pt[0], arb->pt[4])*gedp->dbip->dbi_base2local;
	    ylen = DIST_PNT_PNT(arb->pt[0], arb->pt[1])*gedp->dbip->dbi_base2local;
	    zlen = DIST_PNT_PNT(arb->pt[0], arb->pt[3])*gedp->dbip->dbi_base2local;
	    bu_vls_printf(gedp->ged_result_str, "Length: %g %s\nWidth: %g %s\nHeight: %g %s\n", xlen, str, ylen, str, zlen, str);
	}

	if (print_midpt == 1) {
	    point_t midpt;
	    VADD2SCALE(midpt, arb->pt[0], arb->pt[6], 0.5);
	    bu_vls_printf(gedp->ged_result_str, "Mid Point: (%f %f %f)\n",
		V3ARGS(midpt));
	}

	if (print_vol == 1) {
	    new_intern.idb_meth->ft_volume(&vol, &new_intern);
	    /* convert to local units */
	    vol *= pow(gedp->dbip->dbi_base2local,3.0);
	    bu_vls_printf(gedp->ged_result_str, "Bounding Box Volume: %g %s^3\n", vol, str);
	}

	if (!make_bb) {
	    /* free arb - don't need it for actual geometry */
	    rt_db_free_internal(&new_intern);
	} else {

	    dp = db_diradd(gedp->dbip, bbname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&new_intern.idb_type);
	    if (dp == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "Cannot add %s to directory\n", bbname);
		return BRLCAD_ERROR;
	    }

	    if (rt_db_put_internal(dp, gedp->dbip, &new_intern) < 0) {
		rt_db_free_internal(&new_intern);
		bu_vls_printf(gedp->ged_result_str, "Database write error, aborting.\n");
		return BRLCAD_ERROR;
	    }
	    rt_db_free_internal(&new_intern);

	}
    }

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_BB_COMMANDS(X, XID) \
    X(bb, ged_bb_core, GED_CMD_DEFAULT, &bb_opt_spec) \

GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC(GED_BB_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_OPT_SPEC("libged_bb", 1, GED_BB_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

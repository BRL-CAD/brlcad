/*                    C O N S T R A I N T . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#include "common.h"

#include "bu.h"
#include "cmd.h"
#include "ged.h"


HIDDEN int
constraint_set(void *datap, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)datap;
    if (!gedp || argc < 1 || !argv)
	return BRLCAD_ERROR;

    bu_vls_printf(gedp->ged_result_str, "set called\n");

    return BRLCAD_OK;
}


HIDDEN int
constraint_get(void *datap, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)datap;
    if (!gedp || argc < 1 || !argv)
	return BRLCAD_ERROR;

    bu_vls_printf(gedp->ged_result_str, "get called\n");

    return BRLCAD_OK;
}


HIDDEN int
constraint_show(void *datap, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)datap;
    if (!gedp || argc < 1 || !argv)
	return BRLCAD_ERROR;

    bu_vls_printf(gedp->ged_result_str, "show called\n");

    return BRLCAD_OK;
}


HIDDEN int
constraint_rm(void *datap, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)datap;
    if (!gedp || argc < 1 || !argv)
	return BRLCAD_ERROR;

    bu_vls_printf(gedp->ged_result_str, "rm called\n");

    return BRLCAD_OK;
}


HIDDEN int
constraint_eval(void *datap, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)datap;
    if (!gedp || argc < 1 || !argv)
	return BRLCAD_ERROR;

    bu_vls_printf(gedp->ged_result_str, "eval called\n");

    return BRLCAD_OK;
}


HIDDEN int
constraint_help(void *datap, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)datap;
    if (!gedp || argc < 1 || !argv)
	return BRLCAD_ERROR;

    bu_vls_printf(gedp->ged_result_str, "help called\n");

    return BRLCAD_OK;
}


int
ged_constraint(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "{set|get|show|rm|eval|help} constraint_name [expression[=value] ...]";

#if 0
    /* specified points/edges stay equal */
    static const char *coincident = "coincident {{point point}|{edge edge}}";
    /* two straight edges are segments on the same line */
    static const char *colinear = "colinear {line line}";
    /* circle/arc curves maintain equal center point */
    static const char *concentric = "concentric {circle circle}";
    /* specified entity is immutable w.r.t. global coordinate system */
    static const char *fixed = "fixed {point|edge|face|object}";
    /* specified entities are parallel to each other */
    static const char *parallel = "parallel {{edge edge}|{face face}|{object object}}";
    /* specified entities are perpendicular (90 degrees angle to each other */
    static const char *perpendicular = "perpendicular {{edge edge}|{face face}|{object object}}";
    /* specified entities have X/Y values all set to match each other */
    static const char *horizontal = "horizontal {edge|face|object}";
    /* specified entities have Z values all set to match each other */
    static const char *vertical = "vertical {edge|face|object}";
    /* specified entities maintain contact (point sets overlap without intersecting) */
    static const char *tangent = "tangent {point|edge|face|object} {point|edge|face|object}";
    /* specified values are set equal in magnitude/length/area to each other */
    static const char *equal = "equal {point|edge|face|object} {point|edge|face|object}";
#endif

    /* Examples:
     *
     * constraint set c1 coincident ell.V arb8.P[4]
     * constraint set c2 colinear arb8.E[1] rpc.E[0]
     * constraint set c3 tangent car terrain.r
     * constraint set c4 equal ell.R[1] arb8.E[2]
     * constraint set p0 expression ell.R[0]=10.0
     * constriant set p1 expression eto.A=40.2
     * constraint set p2 expression 4
     * constraint set p3 expression ell.R[1] * p2
     * constraint set p4 expression sph.R[0]=p3-10.5*magnitude(ell.V)
     * constraint get c1 c3
     * constraint show c1 c2 c3 c4
     * constraint rm c1 tangent
     *
     * functions:
     *   magnitude(vector)
     *   length(curve)
     *   angle(curve1, curve2)
     *   area(surface)
     *   radius(arccurve)
     *   diameter(arccurve)
     *
     * attr set c1 cad:description "this keeps our car on the ground"
     * attr set c1 cad:plot 0|1
     * attr set c1 cad:reference 0|1
     * attr set c1 cad:format value|name|expression (name=value)
     */

    static struct bu_cmdtab pc_cmds[] = {
	{"set", constraint_set},
	{"get", constraint_get},
	{"show", constraint_show},
	{"rm", constraint_rm},
	{"eval", constraint_eval},
	{"help", constraint_help},
	{(const char *)NULL, BU_CMD_NULL}
    };

    int ret;
    int cmdret;
    struct directory *dp;

    /* intialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    if (argc < 3) {
	/* must be wanting help */
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s\n", argv[0], usage);
	return GED_HELP;
    }

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);

    /* this is only valid for v5 databases */
    if (db_version(gedp->ged_wdbp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str, "Attributes are not available for this database format.\nPlease upgrade your database format using \"dbupgrade\" to enable attributes.");
	return GED_ERROR;
    }

    /* load the constraint object */
    GED_DB_LOOKUP(gedp, dp, argv[2], LOOKUP_QUIET, GED_ERROR);

    /* run our command */
    ret = bu_cmd(pc_cmds, argc-1, argv+1, 0, gedp, &cmdret);
    if (ret != BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s\n", argv[0], usage);
	return GED_ERROR;
    }
    if (cmdret != BRLCAD_OK)
	return GED_ERROR;

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

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
    struct directory *dp;
    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
    struct bu_vls expression = BU_VLS_INIT_ZERO;
    struct ged *gedp = (struct ged *)datap;

    if (!gedp || argc < 3 || !argv)
	return BRLCAD_ERROR;

    GED_CHECK_READ_ONLY(gedp, GED_ERROR);

    dp = db_lookup(gedp->ged_wdbp->dbip, argv[2], LOOKUP_QUIET);
    if (!dp) {
	/* TODO: need to create the object here */
	return BRLCAD_ERROR;
    }

    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
	bu_vls_printf(gedp->ged_result_str, "Cannot get constraints for %s\n", dp->d_namep);
	bu_avs_free(&avs);
	return BRLCAD_ERROR;
    }

    bu_vls_from_argv(&expression, argc-4, &argv[4]);
    (void)bu_avs_add(&avs, argv[3], bu_vls_addr(&expression));
    bu_vls_free(&expression);

    if (db5_update_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
	bu_vls_printf(gedp->ged_result_str, "Failed to set constraints on %s\n", dp->d_namep);
	bu_avs_free(&avs);
	return GED_ERROR;
    }

    bu_avs_free(&avs);

    return BRLCAD_OK;
}


HIDDEN int
constraint_get(void *datap, int argc, const char *argv[])
{
    size_t i, obj;
    struct directory *dp;
    struct bu_attribute_value_set avs;
    struct bu_attribute_value_pair *avpp;
    int ret = BRLCAD_OK;

    struct ged *gedp = (struct ged *)datap;

    if (!gedp || argc < 1 || !argv)
	return BRLCAD_ERROR;

    /* multiple arguments assumed to be multiple objects */
    for (obj = 0; 2+obj < (size_t)argc; obj++) {

	/* load the constraint object */
	dp = db_lookup(gedp->ged_wdbp->dbip, argv[2+obj], LOOKUP_QUIET);
	if (dp == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Unable to find %s in the database.\n", argv[2+obj]);
	    ret = BRLCAD_ERROR;
	    continue;
	}

	bu_avs_init_empty(&avs);
	if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot get constraints for %s\n", dp->d_namep);
	    ret = BRLCAD_ERROR;
	}

	for (i=0, avpp = avs.avp; i < avs.count; i++, avpp++) {
	    bu_vls_printf(gedp->ged_result_str, "%s %s %s\n", argv[2+obj], avpp->name, avpp->value);
	}

	bu_avs_free(&avs);
    }

    return ret;
}


HIDDEN int
constraint_show(void *datap, int argc, const char *argv[])
{
    size_t i, obj;
    struct directory *dp;
    struct bu_attribute_value_set avs;
    struct bu_attribute_value_pair *avpp;
    int ret = BRLCAD_OK;

    struct ged *gedp = (struct ged *)datap;

    if (!gedp || argc < 1 || !argv)
	return BRLCAD_ERROR;

    /* multiple arguments assumed to be multiple objects */
    for (obj = 0; 2+obj < (size_t)argc; obj++) {
	bu_vls_printf(gedp->ged_result_str, "%s:\n", argv[2+obj]);

	/* load the constraint object */
	dp = db_lookup(gedp->ged_wdbp->dbip, argv[2+obj], LOOKUP_QUIET);
	if (dp == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "\tUnable to find %s in the database.\n", argv[2+obj]);
	    ret = BRLCAD_ERROR;
	    continue;
	}

	bu_avs_init_empty(&avs);
	if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
	    bu_vls_printf(gedp->ged_result_str, "\tCannot get constraints for %s\n", dp->d_namep);
	    ret = BRLCAD_ERROR;
	}

	for (i=0, avpp = avs.avp; i < avs.count; i++, avpp++) {
	    bu_vls_printf(gedp->ged_result_str, "\t%s = %s\n", avpp->name, avpp->value);
	}

	bu_avs_free(&avs);
    }

    return ret;
}


HIDDEN int
constraint_eval(void *datap, int argc, const char *argv[])
{
    size_t i, obj;
    struct directory *dp;
    struct bu_attribute_value_set avs;
    struct bu_attribute_value_pair *avpp;
    int ret = BRLCAD_OK;

    struct ged *gedp = (struct ged *)datap;

    if (!gedp || argc < 1 || !argv)
	return BRLCAD_ERROR;

    GED_CHECK_READ_ONLY(gedp, GED_ERROR);

    /* multiple arguments assumed to be multiple objects */
    for (obj = 0; 2+obj < (size_t)argc; obj++) {

	/* load the constraint object */
	dp = db_lookup(gedp->ged_wdbp->dbip, argv[2+obj], LOOKUP_QUIET);
	if (dp == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Unable to find %s in the database.\n", argv[2+obj]);
	    ret = BRLCAD_ERROR;
	    continue;
	}

	bu_avs_init_empty(&avs);
	if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot get constraints from %s\n", dp->d_namep);
	    ret = BRLCAD_ERROR;
	}

	for (i=0, avpp = avs.avp; i < avs.count; i++, avpp++) {
	    bu_vls_printf(gedp->ged_result_str, "Evaluating %s constraint: %s %s\n", argv[2+obj], avpp->name, avpp->value);
	    bu_vls_printf(gedp->ged_result_str, "<<constraint eval here>>\n");
	}

	bu_avs_free(&avs);
    }

    return ret;
}


HIDDEN void
constraint_usage(struct bu_vls *vp, const char *argv0)
{
    static const char *usage1 = "set constraint_name [expression]";
    static const char *usage2 = "{get|show|eval} constraint_name1 [constraint_name2 ...]";

    bu_vls_printf(vp, "Usage: %s %s\n", argv0, usage1);
    bu_vls_printf(vp, "  or   %s %s\n", argv0, usage2);
    bu_vls_printf(vp, "  or   %s help [command]\n", argv0);
}


HIDDEN int
constraint_help(void *datap, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)datap;
    if (!gedp || argc < 1 || !argv)
	return BRLCAD_ERROR;

    bu_vls_printf(gedp->ged_result_str, "Help for the %s command\n\n", argv[0]);

    constraint_usage(gedp->ged_result_str, argv[0]);

    bu_vls_printf(gedp->ged_result_str,
		  "\nThis command lets you specify dimensional and geometric constraints.\n"
		  "These constraints and any specified parameters define explicit\n"
		  "relationships between geometry objects.  Parametric constraints are\n"
		  "enforced during geometry editing when they are included in the boolean\n"
		  "recipe for a combination.\n\n");

    /* face may need to be a planar face */
    /* objects will need to define a principle axis */

    bu_vls_printf(gedp->ged_result_str, "\nConstraint Expressions:\n");
    bu_vls_printf(gedp->ged_result_str, "\tcoincident {{point point}|{point edge}|{edge edge}}\n"); /* mate */
    bu_vls_printf(gedp->ged_result_str, "\t\tspecified entities stay connected\n");
    bu_vls_printf(gedp->ged_result_str, "\tcollinear {line line}\n"); /* align */
    bu_vls_printf(gedp->ged_result_str, "\t\tstraight edges are aligned along the same line\n");
    bu_vls_printf(gedp->ged_result_str, "\tconcentric {circle circle}\n"); /* center */
    bu_vls_printf(gedp->ged_result_str, "\t\tcircle/arc curves maintain same center point\n");
    bu_vls_printf(gedp->ged_result_str, "\tfixed {point|edge|face|object}\n");
    bu_vls_printf(gedp->ged_result_str, "\t\tspecified entity is immutable w.r.t. global coordinate system\n");
    bu_vls_printf(gedp->ged_result_str, "\tparallel {{edge edge}|{edge face}|{face face}|{object object}}\n");
    bu_vls_printf(gedp->ged_result_str, "\t\tspecified entities are parallel to each other\n");
    bu_vls_printf(gedp->ged_result_str, "\tperpendicular {{edge edge}|{edge face}|{face face}|{object object}}\n");
    bu_vls_printf(gedp->ged_result_str, "\t\tspecified entities are perpendicular (90 degrees angle to each other)\n");
    bu_vls_printf(gedp->ged_result_str, "\thorizontal {{point point}|edge|vector}\n");
    bu_vls_printf(gedp->ged_result_str, "\t\tspecified entities have X/Y values all set to match each other\n");
    bu_vls_printf(gedp->ged_result_str, "\tvertical {{point point}|edge|vector}\n");
    bu_vls_printf(gedp->ged_result_str, "\t\tspecified entities have Z values all set to match each other\n");
    bu_vls_printf(gedp->ged_result_str, "\ttangent {point|edge|face|object} {point|edge|face|object}\n");
    bu_vls_printf(gedp->ged_result_str, "\t\tspecified entities maintain contact (point sets overlap without intersecting)\n");
    bu_vls_printf(gedp->ged_result_str, "\tsymmetric {{point point}|{curve curve}} line\n");
    bu_vls_printf(gedp->ged_result_str, "\t\tspecified symmetry about an axis\n");
    bu_vls_printf(gedp->ged_result_str, "\tequal {point|edge|face|object} {point|edge|face|object}\n");
    bu_vls_printf(gedp->ged_result_str, "\t\tspecified values are set equal in magnitude/length/area to each other\n");
    bu_vls_printf(gedp->ged_result_str, "\tformula {relationship}\n");
    bu_vls_printf(gedp->ged_result_str, "\t\tgeneral parametric relationships using functions, variables, and values\n");

    bu_vls_printf(gedp->ged_result_str, "\nEntity Functions:\n");
    bu_vls_printf(gedp->ged_result_str, "\tbisect(curve) => point\n");
    bu_vls_printf(gedp->ged_result_str, "\tcenter(surface) => point\n");
    bu_vls_printf(gedp->ged_result_str, "\tcentroid(object) => point\n");
    bu_vls_printf(gedp->ged_result_str, "\tcurvature(curve1, t) => vector\n");

    bu_vls_printf(gedp->ged_result_str, "\nValue Functions:\n");
    bu_vls_printf(gedp->ged_result_str, "\tangle(curve1, curve2) => value\n"); /* orient */
    bu_vls_printf(gedp->ged_result_str, "\tarea(surface) => value\n");
    bu_vls_printf(gedp->ged_result_str, "\tdiameter(arccurve) => value\n");
    bu_vls_printf(gedp->ged_result_str, "\tdistance(entity, entity) => value\n");
    bu_vls_printf(gedp->ged_result_str, "\thdistance(entity, entity) => value\n");
    bu_vls_printf(gedp->ged_result_str, "\tlength(curve) => value\n");
    bu_vls_printf(gedp->ged_result_str, "\tmagnitude(vector) => value\n");
    bu_vls_printf(gedp->ged_result_str, "\tradius(arccurve) => value\n");
    bu_vls_printf(gedp->ged_result_str, "\tvdistance(entity, entity) => value\n");

    bu_vls_printf(gedp->ged_result_str, "\nExamples:\n");
    bu_vls_printf(gedp->ged_result_str, "\t%s set c1 coincident ell.V arb8.P[4]\n", argv[0]);
    bu_vls_printf(gedp->ged_result_str, "\t%s set c2 collinear arb8.E[1] rpc.E[0]\n", argv[0]);
    bu_vls_printf(gedp->ged_result_str, "\t%s set c3 tangent car terrain.r\n", argv[0]);
    bu_vls_printf(gedp->ged_result_str, "\t%s set c4 equal ell.R[1] arb8.E[2]\n", argv[0]);
    bu_vls_printf(gedp->ged_result_str, "\t%s set p0 formula ell.R[0]=10.0\n", argv[0]);
    bu_vls_printf(gedp->ged_result_str, "\t%s set p1 formula eto.A<40.2\n", argv[0]);
    bu_vls_printf(gedp->ged_result_str, "\t%s set p2 formula 4\n", argv[0]);
    bu_vls_printf(gedp->ged_result_str, "\t%s set p3 formula ell.R[1] * p2\n", argv[0]);
    bu_vls_printf(gedp->ged_result_str, "\t%s set p4 formula sph.R[0]=p3-10.5*magnitude(ell.V, argv[0])\n", argv[0]);
    bu_vls_printf(gedp->ged_result_str, "\t%s get c1 c3\n", argv[0]);
    bu_vls_printf(gedp->ged_result_str, "\t%s show c1 c2 c3 c4\n", argv[0]);
    bu_vls_printf(gedp->ged_result_str, "\t%s eval c3 p4\n", argv[0]);

    return BRLCAD_OK;
}


int
ged_constraint(struct ged *gedp, int argc, const char *argv[])
{
    /* Potential constraint attributes:
     *
     * attr set c1 cad:description "this keeps our car on the ground"
     * attr set c1 cad:plot 0|1
     * attr set c1 cad:reference 0|1
     * attr set c1 cad:format value|name|expression (name=value)
     * attr set c1 cad:disabled 0|1
     */

    static struct bu_cmdtab pc_cmds[] = {
	{"set", constraint_set},
	{"get", constraint_get},
	{"show", constraint_show},
	{"eval", constraint_eval},
	{"help", constraint_help},
	{(const char *)NULL, BU_CMD_NULL}
    };

    int ret;
    int cmdret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    if (argc < 2) {
	/* must be wanting help */
	constraint_usage(gedp->ged_result_str, argv[0]);
	return GED_HELP;
    }
    if (BU_STR_EQUIV(argv[1], "help")) {
        constraint_help(gedp, argc, argv);
	return GED_OK;
    }

    if (argc < 3) {
	/* must be confused */
	constraint_usage(gedp->ged_result_str, argv[0]);
	return GED_HELP;
    }

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);

    /* this is only valid for v5 databases */
    if (db_version(gedp->ged_wdbp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str, "Attributes are not available for this database format.\nPlease upgrade your database format using \"dbupgrade\" to enable attributes.");
	return GED_ERROR;
    }

    /* run our command */
    ret = bu_cmd(pc_cmds, argc, argv, 1, gedp, &cmdret);
    if (ret != BRLCAD_OK) {
	constraint_usage(gedp->ged_result_str, argv[0]);
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

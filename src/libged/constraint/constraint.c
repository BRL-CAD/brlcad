/*                    C O N S T R A I N T . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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


#include "bu/cmdschema.h"
#include "ged.h"


static char *constraint_native_help(void);


static int
constraint_set(void *datap, int argc, const char *argv[])
{
    struct directory *dp;
    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
    struct bu_vls expression = BU_VLS_INIT_ZERO;
    struct ged *gedp = (struct ged *)datap;

    if (!gedp || argc < 3 || !argv)
	return BRLCAD_ERROR;

    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    dp = db_lookup(gedp->dbip, argv[2], LOOKUP_QUIET);
    if (!dp) {
	/* TODO: need to create the object here */
	return BRLCAD_ERROR;
    }

    if (db5_get_attributes(gedp->dbip, &avs, dp)) {
	bu_vls_printf(gedp->ged_result_str, "Cannot get constraints for %s\n", dp->d_namep);
	bu_avs_free(&avs);
	return BRLCAD_ERROR;
    }

    bu_vls_from_argv(&expression, argc-4, &argv[4]);
    (void)bu_avs_add(&avs, argv[3], bu_vls_addr(&expression));
    bu_vls_free(&expression);

    if (db5_update_attributes(dp, &avs, gedp->dbip)) {
	bu_vls_printf(gedp->ged_result_str, "Failed to set constraints on %s\n", dp->d_namep);
	bu_avs_free(&avs);
	return BRLCAD_ERROR;
    }

    bu_avs_free(&avs);

    return BRLCAD_OK;
}


static int
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
	dp = db_lookup(gedp->dbip, argv[2+obj], LOOKUP_QUIET);
	if (dp == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Unable to find %s in the database.\n", argv[2+obj]);
	    ret = BRLCAD_ERROR;
	    continue;
	}

	bu_avs_init_empty(&avs);
	if (db5_get_attributes(gedp->dbip, &avs, dp)) {
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


static int
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
	dp = db_lookup(gedp->dbip, argv[2+obj], LOOKUP_QUIET);
	if (dp == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "\tUnable to find %s in the database.\n", argv[2+obj]);
	    ret = BRLCAD_ERROR;
	    continue;
	}

	bu_avs_init_empty(&avs);
	if (db5_get_attributes(gedp->dbip, &avs, dp)) {
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


static int
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

    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    /* multiple arguments assumed to be multiple objects */
    for (obj = 0; 2+obj < (size_t)argc; obj++) {

	/* load the constraint object */
	dp = db_lookup(gedp->dbip, argv[2+obj], LOOKUP_QUIET);
	if (dp == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Unable to find %s in the database.\n", argv[2+obj]);
	    ret = BRLCAD_ERROR;
	    continue;
	}

	bu_avs_init_empty(&avs);
	if (db5_get_attributes(gedp->dbip, &avs, dp)) {
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


static void
constraint_usage(struct bu_vls *vp, const char *argv0)
{
    char *help = NULL;

    (void)argv0;
    help = constraint_native_help();
    if (help) {
	bu_vls_strcat(vp, help);
	bu_free(help, "constraint native tree help");
    }
}


static int
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


/* The established subcommand callbacks take the root command and selected
 * child word.  Native dispatch owns syntax selection and reconstructs that
 * local ABI only at the execution boundary. */
static int
constraint_tree_call_legacy(void *data, int argc, const char *argv[],
	const struct bu_cmd_schema *schema, bu_cmd_tree_execute_t execute)
{
    const char **full_argv = NULL;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int ret = BRLCAD_ERROR;

    if (!schema || !execute || argc < 1 || !argv)
	return BRLCAD_ERROR;
    if (bu_cmd_schema_parse_complete(schema, NULL, &msg, argc - 1, argv + 1) < 0) {
	if (bu_vls_strlen(&msg))
	    bu_vls_vlscat(((struct ged *)data)->ged_result_str, &msg);
	bu_vls_free(&msg);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&msg);
    full_argv = (const char **)bu_calloc((size_t)argc + 1, sizeof(*full_argv),
	"constraint native tree argv");
    full_argv[0] = "constraint";
    for (int i = 0; i < argc; i++)
	full_argv[i + 1] = argv[i];
    ret = execute(data, argc + 1, full_argv);
    bu_free((void *)full_argv, "constraint native tree argv");
    return ret;
}


static const struct bu_cmd_operand constraint_set_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Object receiving the constraint", "ged.db_object"),
    BU_CMD_OPERAND("constraint_name", BU_CMD_VALUE_STRING, 1, 1,
	"Constraint attribute name", NULL),
    BU_CMD_OPERAND("expression", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Constraint expression", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand constraint_objects_operands[] = {
    BU_CMD_OPERAND("objects", BU_CMD_VALUE_DB_OBJECT, 1, BU_CMD_COUNT_UNLIMITED,
	"Objects whose constraints are processed", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const char * const constraint_help_keywords[] = {"set", "get", "show", "eval", "help", NULL};
static const struct bu_cmd_operand constraint_help_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("command", BU_CMD_VALUE_KEYWORD, 0, 1,
	"Command to describe", NULL, constraint_help_keywords),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema constraint_root_schema = {
    "constraint", "Manage object constraints", NULL, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema constraint_set_schema = {
    "set", "Set a constraint expression", NULL, constraint_set_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema constraint_get_schema = {
    "get", "Get constraint attributes", NULL, constraint_objects_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema constraint_show_schema = {
    "show", "Show constraint attributes", NULL, constraint_objects_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema constraint_eval_schema = {
    "eval", "Evaluate constraints", NULL, constraint_objects_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema constraint_help_schema = {
    "help", "Show constraint help", NULL, constraint_help_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

#define CONSTRAINT_TREE_EXEC(_name, _schema, _func) \
    static int constraint_tree_##_name(void *data, int argc, const char *argv[]) \
    { return constraint_tree_call_legacy(data, argc, argv, _schema, _func); }
CONSTRAINT_TREE_EXEC(set, &constraint_set_schema, constraint_set)
CONSTRAINT_TREE_EXEC(get, &constraint_get_schema, constraint_get)
CONSTRAINT_TREE_EXEC(show, &constraint_show_schema, constraint_show)
CONSTRAINT_TREE_EXEC(eval, &constraint_eval_schema, constraint_eval)
CONSTRAINT_TREE_EXEC(help, &constraint_help_schema, constraint_help)
#undef CONSTRAINT_TREE_EXEC

static const struct bu_cmd_tree_node constraint_subcommands[] = {
    BU_CMD_TREE_NODE(&constraint_set_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, constraint_tree_set),
    BU_CMD_TREE_NODE(&constraint_get_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, constraint_tree_get),
    BU_CMD_TREE_NODE(&constraint_show_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, constraint_tree_show),
    BU_CMD_TREE_NODE(&constraint_eval_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, constraint_tree_eval),
    BU_CMD_TREE_NODE(&constraint_help_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, constraint_tree_help),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree ged_constraint_tree = {
    &constraint_root_schema, constraint_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};


static char *
constraint_native_help(void)
{
    return bu_cmd_tree_describe(&ged_constraint_tree);
}


int
ged_constraint_core(struct ged *gedp, int argc, const char *argv[])
{
    /* Potential constraint attributes:
     *
     * attr set c1 cad:description "this keeps our car on the ground"
     * attr set c1 cad:plot 0|1
     * attr set c1 cad:reference 0|1
     * attr set c1 cad:format value|name|expression (name=value)
     * attr set c1 cad:disabled 0|1
     */

    int cmdret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    if (argc < 2) {
	/* must be wanting help */
	constraint_usage(gedp->ged_result_str, argv[0]);
	return GED_HELP;
    }
    if (BU_STR_EQUIV(argv[1], "help")) {
	if (bu_cmd_tree_dispatch(&ged_constraint_tree, gedp, argc - 1, argv + 1, &cmdret) == 0)
	    return cmdret;
	constraint_usage(gedp->ged_result_str, argv[0]);
	return BRLCAD_ERROR;
    }

    if (argc < 3) {
	/* must be confused */
	constraint_usage(gedp->ged_result_str, argv[0]);
	return GED_HELP;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);

    /* this is only valid for v5 databases */
    if (db_version(gedp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str, "Attributes are not available for this database format.\nPlease upgrade your database format using \"dbupgrade\" to enable attributes.");
	return BRLCAD_ERROR;
    }


    /* Native dispatch selects a canonical child and validates its published
     * operand schema before the historical operation callback runs. */
    if (bu_cmd_tree_dispatch(&ged_constraint_tree, gedp, argc - 1, argv + 1, &cmdret) != 0) {
	constraint_usage(gedp->ged_result_str, argv[0]);
	return BRLCAD_ERROR;
    }
    if (cmdret != BRLCAD_OK)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}


#include "../include/plugin.h"

static int
ged_constraint_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &ged_constraint_tree, input, cursor_pos, result);
}


static int
ged_constraint_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &ged_constraint_tree, input, analysis);
}


static char *
ged_constraint_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&ged_constraint_tree);
}


static int
ged_constraint_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&ged_constraint_tree, msgs);
}


static const struct ged_cmd_grammar ged_constraint_grammar = {
    "constraint", "Manage object constraints", ged_constraint_grammar_validate,
    ged_constraint_grammar_analyze, ged_constraint_grammar_json, ged_constraint_grammar_lint
};

#define GED_CONSTRAINT_COMMANDS(X, XID) \
    X(constraint, ged_constraint_core, GED_CMD_DEFAULT, &ged_constraint_grammar) \

GED_DECLARE_COMMAND_SET_WITH_GRAMMAR(GED_CONSTRAINT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_GRAMMAR("libged_constraint", 1, GED_CONSTRAINT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

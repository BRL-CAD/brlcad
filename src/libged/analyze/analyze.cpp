/*                      A N A L Y Z E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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
/** @file analyze.cpp
 *
 * The analyze command
 *
 */

#include "common.h"

extern "C" {
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
}

#include <limits>

extern "C" {
#include "bu/cmd.h"
}
#include "bu/cmdschema.h"
#include "./ged_analyze.h"
#include "../ged_private.h"

#define DB_SOLID INT_MAX
#define DB_NON_SOLID INT_MAX - 1


#define HELPFLAG "--print-help"
#define PURPOSEFLAG "--print-purpose"

struct _ged_analyze_info {
    struct ged *gedp = NULL;
    const struct bu_cmdtab *cmds = NULL;
    int verbosity = 0;
    std::map<std::pair<int, int>, op_func_ptr> *union_map;
    std::map<std::pair<int, int>, op_func_ptr> *isect_map;
    std::map<std::pair<int, int>, op_func_ptr> *subtr_map;
};


static struct _ged_analyze_info *
_analyze_info_create()
{
    struct _ged_analyze_info *gc = new struct _ged_analyze_info;
    gc->verbosity = 0;
    gc->union_map = new std::map<std::pair<int, int>, op_func_ptr>;
    gc->isect_map = new std::map<std::pair<int, int>, op_func_ptr>;
    gc->subtr_map = new std::map<std::pair<int, int>, op_func_ptr>;


    // Populate the maps with known pair analysis functions
    (*gc->union_map)[std::make_pair(DB5_MINORTYPE_BRLCAD_PNTS, DB_SOLID)] = op_pnts_vol;
    (*gc->isect_map)[std::make_pair(DB5_MINORTYPE_BRLCAD_PNTS, DB_SOLID)] = op_pnts_vol;
    (*gc->subtr_map)[std::make_pair(DB5_MINORTYPE_BRLCAD_PNTS, DB_SOLID)] = op_pnts_vol;

    return gc;
}

static void
_analyze_info_destroy(struct _ged_analyze_info *s)
{
    delete s->union_map;
    delete s->isect_map;
    delete s->subtr_map;
    delete s;
}

static bool
db_solid_type(int type)
{
    switch (type) {
	case DB5_MINORTYPE_BRLCAD_ARB8:
	case DB5_MINORTYPE_BRLCAD_ARBN:
	case DB5_MINORTYPE_BRLCAD_ARS:
	case DB5_MINORTYPE_BRLCAD_BOT:
	case DB5_MINORTYPE_BRLCAD_BREP:
	case DB5_MINORTYPE_BRLCAD_BSPLINE:
	case DB5_MINORTYPE_BRLCAD_CLINE:
	case DB5_MINORTYPE_BRLCAD_COMBINATION:
	case DB5_MINORTYPE_BRLCAD_DSP:
	case DB5_MINORTYPE_BRLCAD_EBM:
	case DB5_MINORTYPE_BRLCAD_EHY:
	case DB5_MINORTYPE_BRLCAD_ELL:
	case DB5_MINORTYPE_BRLCAD_EPA:
	case DB5_MINORTYPE_BRLCAD_ETO:
	case DB5_MINORTYPE_BRLCAD_EXTRUDE:
	case DB5_MINORTYPE_BRLCAD_HALF:
	case DB5_MINORTYPE_BRLCAD_HF:
	case DB5_MINORTYPE_BRLCAD_HRT:
	case DB5_MINORTYPE_BRLCAD_HYP:
	case DB5_MINORTYPE_BRLCAD_METABALL:
	case DB5_MINORTYPE_BRLCAD_NMG:
	case DB5_MINORTYPE_BRLCAD_PARTICLE:
	case DB5_MINORTYPE_BRLCAD_PIPE:
	case DB5_MINORTYPE_BRLCAD_POLY:
	case DB5_MINORTYPE_BRLCAD_REC:
	case DB5_MINORTYPE_BRLCAD_REVOLVE:
	case DB5_MINORTYPE_BRLCAD_RHC:
	case DB5_MINORTYPE_BRLCAD_RPC:
	case DB5_MINORTYPE_BRLCAD_SKETCH:
	case DB5_MINORTYPE_BRLCAD_SPH:
	case DB5_MINORTYPE_BRLCAD_SUBMODEL:
	case DB5_MINORTYPE_BRLCAD_SUPERELL:
	case DB5_MINORTYPE_BRLCAD_TGC:
	case DB5_MINORTYPE_BRLCAD_TOR:
	case DB5_MINORTYPE_BRLCAD_VOL:
	    return true;
	case DB5_MINORTYPE_BRLCAD_ANNOT:
	case DB5_MINORTYPE_BRLCAD_CONSTRAINT:
	case DB5_MINORTYPE_BRLCAD_DATUM:
	case DB5_MINORTYPE_BRLCAD_GRIP:
	case DB5_MINORTYPE_BRLCAD_JOINT:
	case DB5_MINORTYPE_BRLCAD_PNTS:
	case DB5_MINORTYPE_BRLCAD_SCRIPT:
	default:
	    return false;
    };
}

static op_func_ptr
_analyze_find_processor(struct _ged_analyze_info *s, db_op_t op, int t1, int t2)
{
    int type1 = t1;
    int type2 = t2;
    std::map<std::pair<int, int>, op_func_ptr> *omap;
    switch (op) {
	case DB_OP_UNION:
	    omap = s->union_map;
	    break;
	case DB_OP_INTERSECT:
	    omap = s->isect_map;
	    break;
	case DB_OP_SUBTRACT:
	    omap = s->subtr_map;
	    break;
	default:
	    return NULL;
    }

    if (omap->find(std::make_pair(type1, type2)) != omap->end()) {
	return (*omap)[std::make_pair(t1, t2)];
    }

    // If there isn't a specific type, see if there's a generic match for t2
    type2 = (db_solid_type(t2)) ? DB_SOLID : DB_NON_SOLID;
    if (omap->find(std::make_pair(type1, type2)) != omap->end()) {
	return (*omap)[std::make_pair(type1, type2)];
    }

    // If there isn't a specific type, see if there's a generic match for t1
    type1 = (db_solid_type(t1)) ? DB_SOLID : DB_NON_SOLID;
    type2 = t2;
    if (omap->find(std::make_pair(type1, type2)) != omap->end()) {
	return (*omap)[std::make_pair(type1, type2)];
    }
    // If there isn't a match, see if there's a generic match for t1 and t2
    type1 = (db_solid_type(t1)) ? DB_SOLID : DB_NON_SOLID;
    type2 = (db_solid_type(t2)) ? DB_SOLID : DB_NON_SOLID;
    if (omap->find(std::make_pair(type1, type2)) != omap->end()) {
	return (*omap)[std::make_pair(type1, type2)];
    }

    // Nope, nothing
    return NULL;
}

static int
_analyze_cmd_msgs(void *cs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_analyze_info *gc = (struct _ged_analyze_info *)cs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n", ps);
	return 1;
    }
    return 0;
}


struct analyze_root_args {
    int print_help;
    int verbosity;
};

struct analyze_child_args {
    int print_help;
};

struct analyze_boolean_args {
    int print_help;
    struct bu_vls output;
};


static int
analyze_help_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;

    flat.validation.custom_validate = NULL;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help"))
	flat.operands = NULL;
    return bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
}


static void
analyze_vector_validation_result(struct bu_cmd_validate_result *result,
	const struct bu_cmd_validate_result *vector_result, size_t offset)
{
    bu_cmd_validate_result_clear(result);
    *result = *vector_result;
    result->token_start += offset;
    result->token_end += offset;
    result->completion_count = 0;
    result->completion_candidates = NULL;
    result->semantic_provider = "ged.vector_group";
}


static int
analyze_inside_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    struct bu_cmd_validate_result vector_result = BU_CMD_VALIDATE_RESULT_NULL;
    int ret;

    flat.validation.custom_validate = NULL;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help"))
	flat.operands = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID || argc <= 1 ||
	bu_cmd_schema_option_present(schema, argc, argv, "help"))
	return ret;
    if (bu_cmd_vector3_required_validate(argc - 1, argv + 1,
	cursor_arg > 1 ? cursor_arg - 1 : 0, &vector_result) == 0)
	analyze_vector_validation_result(result, &vector_result, 1);
    bu_cmd_validate_result_clear(&vector_result);
    return 0;
}


static const struct bu_cmd_option analyze_root_options[] = {
    BU_CMD_FLAG("h", "help", struct analyze_root_args, print_help,
	"Print command help"),
    BU_CMD_FLAG("v", "verbose", struct analyze_root_args, verbosity,
	"Increase reporting detail"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_option analyze_child_help_options[] = {
    BU_CMD_FLAG("h", "help", struct analyze_child_args, print_help,
	"Print subcommand help"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_option analyze_boolean_options[] = {
    BU_CMD_FLAG("h", "help", struct analyze_boolean_args, print_help,
	"Print subcommand help"),
    BU_CMD_VLS_APPEND("o", "output", struct analyze_boolean_args, output,
	"name", "Specify output object"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand analyze_object_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_OBJECT, 1, BU_CMD_COUNT_UNLIMITED,
	"Objects to analyze", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand analyze_inside_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Object to test", "ged.db_object"),
    BU_CMD_OPERAND("point", BU_CMD_VALUE_VECTOR, 1, 3,
	"Packed XYZ point or three coordinates", "ged.vector_group"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand analyze_boolean_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_OBJECT, 2, BU_CMD_COUNT_UNLIMITED,
	"Objects to combine", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema analyze_root_schema = {
    "analyze", "Analyze primitives and Boolean combinations", analyze_root_options,
    NULL, BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema analyze_implicit_summary_schema = {
    "analyze", "Summarize object properties", analyze_root_options,
    analyze_object_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(analyze_help_schema_validate, NULL)
};
static const struct bu_cmd_schema analyze_summarize_schema = {
    "summarize", "Summarize object properties", analyze_child_help_options,
    analyze_object_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(analyze_help_schema_validate, NULL)
};
static const struct bu_cmd_schema analyze_inside_schema = {
    "inside", "Test whether a point is inside an object", analyze_child_help_options,
    analyze_inside_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(analyze_inside_schema_validate, NULL)
};
static const struct bu_cmd_schema analyze_intersect_schema = {
    "intersect", "Intersect objects", analyze_boolean_options,
    analyze_boolean_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(analyze_help_schema_validate, NULL)
};
static const struct bu_cmd_schema analyze_subtract_schema = {
    "subtract", "Subtract objects", analyze_boolean_options,
    analyze_boolean_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(analyze_help_schema_validate, NULL)
};

/* Analyze solid in internal form.
 * TODO - this switch table probably indicates this should
 * be a functab callback... */
/**
 * TODO: primitives that still need implementing
 * ehy
 * metaball
 * nmg
 */
static void
analyze_do_summary(struct ged *gedp, const struct rt_db_internal *ip)
{
    /* XXX Could give solid name, and current units, here */

    switch (ip->idb_type) {

	case ID_ARB8:
	    analyze_arb8(gedp, ip);
	    break;

	case ID_BOT:
	    analyze_general(gedp, ip);
	    break;

	case ID_ARBN:
	    analyze_arbn(gedp, ip);
	    break;

	case ID_ARS:
	    analyze_ars(gedp, ip);
	    break;

	case ID_TGC:
	    analyze_general(gedp, ip);
	    break;

	case ID_ELL:
	    analyze_general(gedp, ip);
	    break;

	case ID_TOR:
	    analyze_general(gedp, ip);
	    break;

	case ID_RPC:
	    analyze_general(gedp, ip);
	    break;

	case ID_ETO:
	    analyze_general(gedp, ip);
	    break;

	case ID_EPA:
	    analyze_general(gedp, ip);
	    break;

	case ID_PARTICLE:
	    analyze_general(gedp, ip);
	    break;

	case ID_SUPERELL:
	    analyze_superell(gedp, ip);
	    break;

	case ID_SKETCH:
	    analyze_sketch(gedp, ip);
	    break;

	case ID_HYP:
	    analyze_general(gedp, ip);
	    break;

	case ID_PIPE:
	    analyze_general(gedp, ip);
	    break;

	case ID_VOL:
	    analyze_general(gedp, ip);
	    break;

	case ID_EXTRUDE:
	    analyze_general(gedp, ip);
	    break;

	case ID_RHC:
	    analyze_general(gedp, ip);
	    break;

	default:
	    bu_vls_printf(gedp->ged_result_str, "\nanalyze: unable to process %s solid\n",
			  OBJ[ip->idb_type].ft_name);
	    break;
    }
}

extern "C" int
_analyze_cmd_summarize(void *bs, int argc, const char **argv)
{
    const char *usage_string = "analyze [options] summarize obj1 <obj2 ...>";
    const char *purpose_string = "Summary of analytical information about listed objects";
    if (_analyze_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_analyze_info *gc = (struct _ged_analyze_info *)bs;
    struct ged *gedp = gc->gedp;
    struct rt_db_internal intern;
    struct analyze_child_args args = {0};
    int operand_index;
    int operand_count;
    const char **operands;

    argc--; argv++;
    if (!argc) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    operand_index = bu_cmd_schema_parse_complete(&analyze_summarize_schema,
	&args, gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage_string);
	return GED_HELP;
    }
    operand_count = argc - operand_index;
    operands = argv + operand_index;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, operand_count, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* use the names that were input */
    for (int i = 0; i < operand_count; i++) {
	struct directory *ndp = db_lookup(gedp->dbip,  operands[i], LOOKUP_NOISY);
	if (ndp == RT_DIR_NULL)
	    continue;

	GED_DB_GET_INTERN(gedp, &intern, ndp, bn_mat_identity, BRLCAD_ERROR);

	_ged_do_list(gedp, ndp, 1);
	analyze_do_summary(gedp, &intern);
	rt_db_free_internal(&intern);
    }

    return BRLCAD_OK;
}

extern "C" int
_analyze_cmd_inside(void *bs, int argc, const char **argv)
{
    const char *usage_string = "analyze [options] inside obj x y z ";
    const char *purpose_string = "Determine if the point x,y,z is inside the object obj";
    if (_analyze_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_analyze_info *gc = (struct _ged_analyze_info *)bs;
    struct ged *gedp = gc->gedp;
    struct analyze_child_args args = {0};
    int operand_index;
    int operand_count;
    const char **operands;

    argc--; argv++;
    if (!argc) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    operand_index = bu_cmd_schema_parse_complete(&analyze_inside_schema,
	&args, gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage_string);
	return GED_HELP;
    }
    operand_count = argc - operand_index;
    operands = argv + operand_index;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, operand_count, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (operand_count != 2 && operand_count != 4) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n", usage_string);
	return GED_HELP;
    }

    struct directory *dp = db_lookup(gedp->dbip, operands[0], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_sprintf(gedp->ged_result_str, "specified object %s not found.\n", operands[0]);
	return BRLCAD_ERROR;
    }

    point_t p;
	if (!bu_cmd_vector3_from_argv(p, (size_t)(operand_count - 1), operands + 1)) {
	bu_vls_sprintf(gedp->ged_result_str, "invalid point specification\n");
	return BRLCAD_ERROR;
    }

    int ret = pnt_inside_vol(gedp, &p, dp);
    if (ret < 0) {
	bu_vls_sprintf(gedp->ged_result_str, "point test failed\n");
	return BRLCAD_ERROR;
    }

    bu_vls_sprintf(gedp->ged_result_str, "%d\n", ret);

    return BRLCAD_OK;
}
static void
clear_obj(struct ged *gedp, const char *name)
{
    struct bu_vls tmpstr = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&tmpstr, "%s", bu_vls_cstr(gedp->ged_result_str));
    const char *av[4] = {"kill", "-f", "-q", NULL};
    av[3] = name;
    ged_exec_kill(gedp, 4, (const char **)av);
    bu_vls_sprintf(gedp->ged_result_str, "%s", bu_vls_cstr(&tmpstr));
}


static void
mv_obj(struct ged *gedp, const char *n1, const char *n2)
{
    clear_obj(gedp, n2);
    struct bu_vls tmpstr = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&tmpstr, "%s", bu_vls_cstr(gedp->ged_result_str));
    const char *av[3] = {"mv", NULL, NULL};
    av[1] = n1;
    av[2] = n2;
    ged_exec_mv(gedp, 3, (const char **)av);
    bu_vls_sprintf(gedp->ged_result_str, "%s", bu_vls_cstr(&tmpstr));
}

extern "C" int
_analyze_cmd_intersect(void *bs, int argc, const char **argv)
{
    const char *usage_string = "analyze [options] intersect [-o out_obj] obj1 obj2 <...>";
    const char *purpose_string = "Intersect obj1 with obj2 and any subsequent objs";
    if (_analyze_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_analyze_info *gc = (struct _ged_analyze_info *)bs;
    struct ged *gedp = gc->gedp;
    struct analyze_boolean_args args = {0, BU_VLS_INIT_ZERO};
    int operand_index;
    int operand_count;
    const char **operands;

    argc--; argv++;
    if (!argc) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    operand_index = bu_cmd_schema_parse_complete(&analyze_intersect_schema,
	&args, gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_free(&args.output);
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bu_vls_free(&args.output);
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage_string);
	return GED_HELP;
    }
    operand_count = argc - operand_index;
    operands = argv + operand_index;
    struct bu_vls oname = args.output;
    argc = operand_count;
    argv = operands;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (bu_vls_strlen(&oname)) {
	struct directory *dp_out = db_lookup(gedp->dbip, bu_vls_cstr(&oname), LOOKUP_QUIET);
	if (dp_out != RT_DIR_NULL) {
	    bu_vls_sprintf(gedp->ged_result_str, "specified output object %s already exists.\n", bu_vls_cstr(&oname));
	    bu_vls_free(&oname);
	    return BRLCAD_ERROR;
	}
    }

    long ret = 0;
    const char *tmpname = "___analyze_cmd_intersect_tmp_obj__";
    struct directory *dp1 = db_lookup(gedp->dbip, argv[0], LOOKUP_NOISY);
    if (!dp1) {
	bu_vls_sprintf(gedp->ged_result_str, "object %s does not exist.\n", argv[0]);
	bu_vls_free(&oname);
	return BRLCAD_ERROR;
    }
    struct directory *dp2 = db_lookup(gedp->dbip, argv[1], LOOKUP_NOISY);
    if (!dp2) {
	bu_vls_sprintf(gedp->ged_result_str, "object %s does not exist.\n", argv[1]);
	bu_vls_free(&oname);
	return BRLCAD_ERROR;
    }
    op_func_ptr of = _analyze_find_processor(gc, DB_OP_INTERSECT, dp1->d_minor_type, dp2->d_minor_type);
    if (!of) {
	bu_vls_sprintf(gedp->ged_result_str, "Unsupported type pairing\n");
	bu_vls_free(&oname);
	return BRLCAD_ERROR;
    }
    clear_obj(gc->gedp, tmpname);
    ret = (*of)(tmpname, gc->gedp,  DB_OP_INTERSECT, argv[0], argv[1]);
    if (ret == -1) {
	clear_obj(gc->gedp, tmpname);
	bu_vls_free(&oname);
	return BRLCAD_ERROR;
    }

    if (argc > 2) {
	const char *tmpname2 = "___analyze_cmd_intersect_tmp_obj_2__";
	for (int i = 2; i < argc; i++) {
	    const char *n1 = tmpname;
	    const char *n2 = argv[i];
	    dp1 = db_lookup(gedp->dbip, n1, LOOKUP_NOISY);
	    dp2 = db_lookup(gedp->dbip, n2, LOOKUP_NOISY);
	    of = _analyze_find_processor(gc, DB_OP_INTERSECT, dp1->d_minor_type, dp2->d_minor_type);
	    if (!of) {
		bu_vls_sprintf(gedp->ged_result_str, "Unsupported type pairing\n");
		clear_obj(gc->gedp, tmpname);
		clear_obj(gc->gedp, tmpname2);
		bu_vls_free(&oname);
		return BRLCAD_ERROR;
	    }
	    ret = (*of)(tmpname2, gc->gedp,  DB_OP_INTERSECT, n1, n2);
	    mv_obj(gc->gedp, tmpname2, tmpname);
	    if (ret == -1) {
		clear_obj(gc->gedp, tmpname);
		clear_obj(gc->gedp, tmpname2);
		bu_vls_free(&oname);
		return BRLCAD_ERROR;
	    }
	}
    }

    if (bu_vls_strlen(&oname)) {
	mv_obj(gc->gedp, tmpname, bu_vls_cstr(&oname));
    }

    clear_obj(gc->gedp, tmpname);

    bu_vls_sprintf(gedp->ged_result_str, "%ld\n", ret);
    bu_vls_free(&oname);

    return BRLCAD_OK;
}

extern "C" int
_analyze_cmd_subtract(void *bs, int argc, const char **argv)
{
    const char *usage_string = "analyze [options] subtract [-o out_obj] obj1 obj2 <...>";
    const char *purpose_string = "Subtract obj2 (and any subsequent objects) from obj1";
    if (_analyze_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_analyze_info *gc = (struct _ged_analyze_info *)bs;
    struct ged *gedp = gc->gedp;
    struct analyze_boolean_args args = {0, BU_VLS_INIT_ZERO};
    int operand_index;
    int operand_count;
    const char **operands;

    argc--; argv++;
    if (!argc) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    operand_index = bu_cmd_schema_parse_complete(&analyze_subtract_schema,
	&args, gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_free(&args.output);
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bu_vls_free(&args.output);
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage_string);
	return GED_HELP;
    }
    operand_count = argc - operand_index;
    operands = argv + operand_index;
    struct bu_vls oname = args.output;
    argc = operand_count;
    argv = operands;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (bu_vls_strlen(&oname)) {
	struct directory *dp_out = db_lookup(gedp->dbip, bu_vls_cstr(&oname), LOOKUP_QUIET);
	if (dp_out != RT_DIR_NULL) {
	    bu_vls_sprintf(gedp->ged_result_str, "specified output object %s already exists.\n", bu_vls_cstr(&oname));
	    bu_vls_free(&oname);
	    return BRLCAD_ERROR;
	}
    }

    long ret = 0;
    const char *tmpname = "___analyze_cmd_subtract_tmp_obj__";
    struct directory *dp1 = db_lookup(gedp->dbip, argv[0], LOOKUP_NOISY);
    if (!dp1) {
	bu_vls_sprintf(gedp->ged_result_str, "object %s does not exist.\n", argv[0]);
	bu_vls_free(&oname);
	return BRLCAD_ERROR;
    }
    struct directory *dp2 = db_lookup(gedp->dbip, argv[1], LOOKUP_NOISY);
    if (!dp2) {
	bu_vls_sprintf(gedp->ged_result_str, "object %s does not exist.\n", argv[1]);
	bu_vls_free(&oname);
	return BRLCAD_ERROR;
    }
    op_func_ptr of = _analyze_find_processor(gc, DB_OP_SUBTRACT, dp1->d_minor_type, dp2->d_minor_type);
    if (!of) {
	bu_vls_sprintf(gedp->ged_result_str, "Unsupported type pairing\n");
	bu_vls_free(&oname);
	return BRLCAD_ERROR;
    }
    clear_obj(gc->gedp, tmpname);
    ret = (*of)(tmpname, gc->gedp,  DB_OP_SUBTRACT, argv[0], argv[1]);
    if (ret == -1) {
	clear_obj(gc->gedp, tmpname);
	bu_vls_free(&oname);
	return BRLCAD_ERROR;
    }

    if (argc > 2) {
	const char *tmpname2 = "___analyze_cmd_subtract_tmp_obj_2__";
	for (int i = 2; i < argc; i++) {
	    const char *n1 = tmpname;
	    const char *n2 = argv[i];
	    dp1 = db_lookup(gedp->dbip, n1, LOOKUP_NOISY);
	    dp2 = db_lookup(gedp->dbip, n2, LOOKUP_NOISY);
	    of = _analyze_find_processor(gc, DB_OP_SUBTRACT, dp1->d_minor_type, dp2->d_minor_type);
	    if (!of) {
		bu_vls_sprintf(gedp->ged_result_str, "Unsupported type pairing\n");
		clear_obj(gc->gedp, tmpname);
		clear_obj(gc->gedp, tmpname2);
		bu_vls_free(&oname);
		return BRLCAD_ERROR;
	    }
	    ret = (*of)(tmpname2, gc->gedp,  DB_OP_SUBTRACT, n1, n2);
	    mv_obj(gc->gedp, tmpname2, tmpname);
	    if (ret == -1) {
		clear_obj(gc->gedp, tmpname);
		clear_obj(gc->gedp, tmpname2);
		bu_vls_free(&oname);
		return BRLCAD_ERROR;
	    }
	}
    }

    if (bu_vls_strlen(&oname)) {
	mv_obj(gc->gedp, tmpname, bu_vls_cstr(&oname));
    }

    clear_obj(gc->gedp, tmpname);

    bu_vls_sprintf(gedp->ged_result_str, "%ld\n", ret);
    bu_vls_free(&oname);

    return BRLCAD_OK;
}

const struct bu_cmdtab _analyze_cmds[] = {
    { "summarize",           _analyze_cmd_summarize},
    { "inside",              _analyze_cmd_inside},
    { "intersect",           _analyze_cmd_intersect},
    { "subtract",            _analyze_cmd_subtract},
    { (char *)NULL,      NULL}
};


static int
analyze_tree_execute(void *data, int argc, const char *argv[])
{
    struct _ged_analyze_info *gc = (struct _ged_analyze_info *)data;
    int child_result = BRLCAD_ERROR;

    if (!gc || bu_cmd(_analyze_cmds, argc, argv, 0, gc, &child_result) != BRLCAD_OK)
	return BRLCAD_ERROR;
    return child_result;
}


static const struct bu_cmd_tree_node analyze_subcommands[] = {
    BU_CMD_TREE_NODE(&analyze_summarize_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, analyze_tree_execute),
    BU_CMD_TREE_NODE(&analyze_inside_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, analyze_tree_execute),
    BU_CMD_TREE_NODE(&analyze_intersect_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, analyze_tree_execute),
    BU_CMD_TREE_NODE(&analyze_subtract_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, analyze_tree_execute),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree ged_analyze_tree = {
    &analyze_root_schema, analyze_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};
static const struct ged_cmd_native_form analyze_native_forms[] = {
    {"subcommands", NULL, &ged_analyze_tree},
    {"implicit_summary", &analyze_implicit_summary_schema, NULL},
    {NULL, NULL, NULL}
};


static const struct ged_cmd_native_form *
analyze_select_native_form(const struct ged *UNUSED(gedp), size_t argc,
	const char * const *argv)
{
    size_t i = 1;
    const char *word = "";
    size_t word_len;

    while (i < argc) {
	int option_span = bu_cmd_schema_option_span(&analyze_root_schema, argc - i,
	    (const char **)argv + i);
	if (option_span > 0) {
	    i += (size_t)option_span;
	    continue;
	}
	/* Give malformed root options to the explicit tree, which reports the
	 * option diagnostic rather than treating them as object names. */
	if (option_span < 0)
	    return &analyze_native_forms[0];
	word = argv[i] ? argv[i] : "";
	break;
    }
    word_len = strlen(word);
    if (!word_len)
	return &analyze_native_forms[0];
    for (size_t ni = 0; analyze_subcommands[ni].schema; ni++) {
	const char *name = analyze_subcommands[ni].schema->name;
	if (!strncmp(name, word, word_len))
	    return &analyze_native_forms[0];
    }
    return &analyze_native_forms[1];
}


static void
analyze_tree_show_help(struct ged *gedp)
{
    char *help = bu_cmd_tree_describe(&ged_analyze_tree);

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "analyze native tree help");
    }
}


static int
_analyze_cmd_help(void *bs, int argc, const char **argv)
{
    struct _ged_analyze_info *gc = (struct _ged_analyze_info *)bs;
    const struct bu_cmd_tree_node *node;
    const char **help_argv;
    int result = BRLCAD_ERROR;

    if (!gc)
	return BRLCAD_ERROR;
    if (!argc || !argv) {
	analyze_tree_show_help(gc->gedp);
	return BRLCAD_OK;
    }
    node = bu_cmd_tree_find_subcommand(&ged_analyze_tree, argv[0]);
    if (!node)
	return BRLCAD_ERROR;
    help_argv = (const char **)bu_calloc((size_t)argc + 1, sizeof(const char *),
	"analyze subcommand help argv");
    help_argv[0] = node->schema->name;
    help_argv[1] = HELPFLAG;
    for (int i = 1; i < argc; i++)
	help_argv[i + 1] = argv[i];
    (void)bu_cmd(_analyze_cmds, argc + 1, help_argv, 0, gc, &result);
    bu_free((void *)help_argv, "analyze subcommand help argv");
    return result;
}


extern "C" int
ged_analyze_core(struct ged *gedp, int argc, const char *argv[])
{
    struct analyze_root_args args = {0, 0};
    struct _ged_analyze_info *gc;
    int option_index;
    int remaining;
    const char **rest;
    int ret = BRLCAD_ERROR;

    // Sanity
    if (UNLIKELY(!gedp || !argc || !argv)) {
	return BRLCAD_ERROR;
    }

    gc = _analyze_info_create();
    gc->gedp = gedp;
    gc->cmds = _analyze_cmds;

    // Clear results
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	_analyze_cmd_help(gc, 0, NULL);
	_analyze_info_destroy(gc);
	return BRLCAD_OK;
    }

    option_index = bu_cmd_schema_parse(&analyze_root_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (option_index < 0) {
	_analyze_info_destroy(gc);
	return BRLCAD_ERROR;
    }
    gc->verbosity = args.verbosity;
    remaining = argc - 1 - option_index;
    rest = argv + 1 + option_index;

    if (args.print_help) {
	if (remaining && bu_cmd_tree_find_subcommand(&ged_analyze_tree, rest[0]))
	    ret = _analyze_cmd_help(gc, remaining, rest);
	else {
	    _analyze_cmd_help(gc, 0, NULL);
	    ret = BRLCAD_OK;
	}
	_analyze_info_destroy(gc);
	return ret;
    }
    if (!remaining) {
	_analyze_cmd_help(gc, 0, NULL);
	_analyze_info_destroy(gc);
	return BRLCAD_OK;
    }

    // If no explicit subcommand is present, preserve analyze's established
    // shorthand: every remaining word is an object for summarize.
    if (bu_cmd_tree_find_subcommand(&ged_analyze_tree, rest[0])) {
	if (bu_cmd_tree_dispatch(&ged_analyze_tree, gc, remaining, rest, &ret) != 0)
	    ret = BRLCAD_ERROR;
    } else {
	const char **summary_argv = (const char **)bu_calloc((size_t)remaining + 1,
	    sizeof(const char *), "analyze implicit summarize argv");
	summary_argv[0] = "summarize";
	for (int i = 0; i < remaining; i++)
	    summary_argv[i + 1] = rest[i];
	if (bu_cmd(_analyze_cmds, remaining + 1, summary_argv, 0, gc, &ret) != BRLCAD_OK)
	    ret = BRLCAD_ERROR;
	bu_free((void *)summary_argv, "analyze implicit summarize argv");
    }

    _analyze_info_destroy(gc);
    return ret;
}

#include "../include/plugin.h"
static int
ged_analyze_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_native_forms_validate(gedp, analyze_native_forms,
	analyze_select_native_form, input, cursor_pos, result);
}


static int
ged_analyze_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_native_forms_analyze(gedp, analyze_native_forms,
	analyze_select_native_form, input, analysis);
}


static char *
ged_analyze_grammar_json(void)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    char *explicit_form = bu_cmd_tree_describe_json(&ged_analyze_tree);
    char *implicit_form = bu_cmd_schema_describe_json(&analyze_implicit_summary_schema);

    bu_vls_strcat(&out, "{\"name\":\"analyze\",\"kind\":\"native_conditional_grammar\",");
    bu_vls_strcat(&out, "\"help\":\"Analyze primitives and Boolean combinations\",");
    bu_vls_printf(&out, "\"forms\":{\"explicit\":%s,\"implicit_summary\":%s}}",
	explicit_form ? explicit_form : "{}", implicit_form ? implicit_form : "{}");
    if (explicit_form)
	bu_free(explicit_form, "analyze explicit grammar JSON");
    if (implicit_form)
	bu_free(implicit_form, "analyze implicit grammar JSON");
    return bu_vls_strdup(&out);
}


static int
ged_analyze_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&ged_analyze_tree, msgs);
}


static const struct ged_cmd_grammar ged_analyze_grammar = {
    "analyze", "Analyze primitives and Boolean combinations",
    ged_analyze_grammar_validate, ged_analyze_grammar_analyze,
    ged_analyze_grammar_json, ged_analyze_grammar_lint
};

#define GED_ANALYZE_COMMANDS(X, XID) \
    X(analyze, ged_analyze_core, GED_CMD_DEFAULT, &ged_analyze_grammar)

GED_DECLARE_COMMAND_SET_WITH_GRAMMAR(GED_ANALYZE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_GRAMMAR("libged_analyze", 1, GED_ANALYZE_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

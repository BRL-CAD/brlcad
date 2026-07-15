/*                     R E P A I R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "rt/geom.h"
#include "rt/db_internal.h"
#include "wdb.h"

#include "ged_arb.h"
#include "../ged_private.h"


struct arb_repair_args {
    int print_help;
    struct bu_vls out_name;
    fastf_t tolerance;
};


static int
arb_repair_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    size_t operands;
    int ret;

    if (bu_cmd_schema_option_present(schema, argc, argv, "help")) {
	bu_cmd_validate_result_clear(result);
	result->state = BU_CMD_VALIDATE_VALID;
	result->token_start = cursor_arg;
	result->token_end = cursor_arg;
	result->expected = BU_CMD_EXPECT_NONE;
	result->completion_type = BU_CMD_VALUE_FLAG;
	result->hint = "command help";
	return 0;
    }

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;
    operands = bu_cmd_schema_operand_count(schema, argc, argv);
    if (bu_cmd_schema_option_present(schema, argc, argv, "output-name") && operands > 1) {
	bu_cmd_validate_result_clear(result);
	result->state = BU_CMD_VALIDATE_INVALID;
	result->token_start = cursor_arg < argc ? cursor_arg : argc;
	result->token_end = result->token_start;
	result->expected = BU_CMD_EXPECT_OPERAND;
	result->completion_type = BU_CMD_VALUE_DB_OBJECT;
	result->hint = "--output-name permits only one input ARB";
    }
    return 0;
}


static const struct bu_cmd_option arb_repair_options[] = {
    BU_CMD_FLAG("h", "help", struct arb_repair_args, print_help,
	"Print help and exit"),
    BU_CMD_VLS_APPEND("o", "output-name", struct arb_repair_args, out_name, "name",
	"Write the repaired ARB to a new object"),
    BU_CMD_NUMBER("t", "tol", struct arb_repair_args, tolerance, "distance",
	"Distance tolerance for snapping vertices"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand arb_repair_operands[] = {
    BU_CMD_OPERAND("input_arb", BU_CMD_VALUE_DB_OBJECT, 1, BU_CMD_COUNT_UNLIMITED,
	"ARB objects to repair", NULL),
    BU_CMD_OPERAND_NULL
};
extern "C" const struct bu_cmd_schema ged_arb_repair_schema = {
    "repair", "Repair and canonicalize ARB objects", arb_repair_options,
    arb_repair_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(arb_repair_schema_validate, NULL)
};


static void
arb_repair_help(struct ged *gedp)
{
    char *option_help = bu_cmd_schema_describe(&ged_arb_repair_schema);

    bu_vls_printf(gedp->ged_result_str,
	"Usage: arb repair [options] input_arb [input_arb ...]\n\n"
	"Attempts to repair and canonicalize an ARB.\n\n");
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "Options:\n%s\n", option_help);
	bu_free(option_help, "ARB repair native option help");
    }
}

extern "C" int
_arb_cmd_repair(void *bs, int argc, const char **argv)
{
    struct _ged_arb_info *gb = (struct _ged_arb_info *)bs;
    struct ged *gedp = gb->gedp;

    struct arb_repair_args args = {0, BU_VLS_INIT_ZERO, -1.0};
    struct bu_vls parse_msg = BU_VLS_INIT_ZERO;
    struct bu_cmd_validate_result validation = BU_CMD_VALIDATE_RESULT_NULL;
    const char **pargv = NULL;
    int operand_index;
    int input_argc;

    if (argc < 1 || !argv || !argv[0])
	return BRLCAD_ERROR;
    pargv = (const char **)bu_calloc((size_t)argc, sizeof(*pargv), "ARB repair native argv");
    memcpy(pargv, argv, (size_t)argc * sizeof(*pargv));
    operand_index = bu_cmd_schema_parse(&ged_arb_repair_schema, &args, &parse_msg,
	argc - 1, pargv + 1);
    if (operand_index < 0) {
	if (bu_vls_strlen(&parse_msg))
	    bu_vls_vlscat(gedp->ged_result_str, &parse_msg);
	bu_vls_free(&parse_msg);
	bu_vls_free(&args.out_name);
	bu_free((void *)pargv, "ARB repair native argv");
	return BRLCAD_ERROR;
    }
    input_argc = argc - 1 - operand_index;
    if (args.print_help || !input_argc) {
	arb_repair_help(gedp);
	bu_vls_free(&parse_msg);
	bu_vls_free(&args.out_name);
	bu_free((void *)pargv, "ARB repair native argv");
	return GED_HELP;
    }
    if (bu_cmd_schema_validate(&ged_arb_repair_schema, (size_t)(argc - 1), pargv + 1,
	(size_t)(argc - 1), &validation) != 0 || validation.state != BU_CMD_VALIDATE_VALID) {
	if (validation.hint)
	    bu_vls_printf(gedp->ged_result_str, "%s\n", validation.hint);
	bu_cmd_validate_result_clear(&validation);
	bu_vls_free(&parse_msg);
	bu_vls_free(&args.out_name);
	bu_free((void *)pargv, "ARB repair native argv");
	return BRLCAD_ERROR;
    }
    bu_cmd_validate_result_clear(&validation);
    bu_vls_free(&parse_msg);
    argv = pargv + 1 + operand_index;
    argc = input_argc;

    int in_place_repair = 1;
    if (bu_vls_strlen(&args.out_name))
        in_place_repair = 0;

    if (!in_place_repair) {
        if (argc != 1) {
            bu_vls_printf(gedp->ged_result_str, "When specifying an output object name, only one input at a time may be processed.");
            bu_vls_free(&args.out_name);
	    bu_free((void *)pargv, "ARB repair native argv");
            return BRLCAD_ERROR;
        }
        if (db_lookup(gedp->dbip, bu_vls_cstr(&args.out_name), LOOKUP_QUIET) != RT_DIR_NULL) {
            bu_vls_printf(gedp->ged_result_str, "Object %s already exists!\n", bu_vls_cstr(&args.out_name));
            bu_vls_free(&args.out_name);
	    bu_free((void *)pargv, "ARB repair native argv");
            return BRLCAD_ERROR;
        }
    }

    int ret = BRLCAD_OK;
    for (int i = 0; i < argc; i++) {
        struct directory *dp = db_lookup(gedp->dbip, argv[i], LOOKUP_NOISY);
        if (dp == RT_DIR_NULL) continue;

        int real_flag = (dp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
        if (!real_flag) continue;

        if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD) continue;
        if (dp->d_minor_type != DB5_MINORTYPE_BRLCAD_ARB8) {
            bu_vls_printf(gedp->ged_result_str, "Object %s is not an ARB.\n", dp->d_namep);
            ret = BRLCAD_ERROR;
            continue;
        }

        struct rt_db_internal *intern;
        BU_GET(intern, struct rt_db_internal);
        if (rt_db_get_internal(intern, dp, gedp->dbip, bn_mat_identity) < 0) {
            BU_PUT(intern, struct rt_db_internal);
            continue;
        }
        RT_CK_DB_INTERNAL(intern);

        struct rt_arb_internal *arb = (struct rt_arb_internal *)(intern->idb_ptr);

        struct rt_arb_internal *marb = NULL;
        BU_ALLOC(marb, struct rt_arb_internal);

        struct bn_tol tol = BN_TOL_INIT_TOL;
        if (args.tolerance > 0.0) {
            tol.dist = args.tolerance;
            tol.dist_sq = args.tolerance * args.tolerance;
        }
        int rep_ret = rt_arb_repair(marb, arb, &tol, RT_ARB_REPAIR_SNAP_VERTICES);
        
        if (rep_ret < 0) {
            bu_vls_printf(gedp->ged_result_str, "Unable to repair ARB %s\n", dp->d_namep);
            rt_db_free_internal(intern);
            BU_PUT(intern, struct rt_db_internal);
            bu_free(marb, "marb");
            ret = BRLCAD_ERROR;
            continue;
        }

        if (rep_ret == 0 && in_place_repair) {
            bu_vls_printf(gedp->ged_result_str, "ARB %s is already valid\n", dp->d_namep);
            rt_db_free_internal(intern);
            BU_PUT(intern, struct rt_db_internal);
            bu_free(marb, "marb");
            continue;
        }

        struct rt_db_internal out_intern;
        RT_DB_INTERNAL_INIT(&out_intern);
        out_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
        out_intern.idb_type = ID_ARB8;
        out_intern.idb_meth = &OBJ[ID_ARB8];
        out_intern.idb_ptr = (void *)marb;

        // Write out the result
        struct directory *out_dp;
        const char *rname;
        if (in_place_repair) {
            rname = dp->d_namep;
            out_dp = dp;
        } else {
            rname = bu_vls_cstr(&args.out_name);
            out_dp = db_diradd(gedp->dbip, rname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&out_intern.idb_type);
            if (out_dp == RT_DIR_NULL) {
                bu_vls_printf(gedp->ged_result_str, "Failed to write out new ARB %s\n", rname);
                rt_db_free_internal(intern);
                BU_PUT(intern, struct rt_db_internal);
                bu_free(marb, "marb");
                ret = BRLCAD_ERROR;
                continue;
            }
        }

        if (rt_db_put_internal(out_dp, gedp->dbip, &out_intern) < 0) {
            bu_vls_printf(gedp->ged_result_str, "Failed to write out new ARB %s\n", rname);
            rt_db_free_internal(intern);
            BU_PUT(intern, struct rt_db_internal);
            ret = BRLCAD_ERROR;
            continue;
        }

        bu_vls_printf(gedp->ged_result_str, "Repair completed successfully and written to %s\n", rname);
        
        rt_db_free_internal(intern);
        BU_PUT(intern, struct rt_db_internal);
    }

    bu_vls_free(&args.out_name);
    bu_free((void *)pargv, "ARB repair native argv");
    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

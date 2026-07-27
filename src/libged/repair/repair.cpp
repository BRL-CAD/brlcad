/*                        R E P A I R . C P P
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

#include <string.h>
#include <stdlib.h>

#include "bu/cmdschema.h"
#include "bu/vls.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../ged_private.h"
#include "rt/functab.h"

struct repair_args {
    int print_help;
    struct bu_vls out_name;
    fastf_t tolerance;
    fastf_t max_hole_area_percent;
    fastf_t max_hole_area;
};

static int
repair_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;

    flat.validation.custom_validate = NULL;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help"))
	flat.operands = NULL;
    return bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
}

static const struct bu_cmd_option repair_options[] = {
    BU_CMD_FLAG("h", "help", struct repair_args, print_help,
	"Print command help"),
    BU_CMD_VLS_APPEND("o", "output-name", struct repair_args, out_name, "name",
	"Write the repaired object to a new object"),
    BU_CMD_NUMBER("t", "tol", struct repair_args, tolerance, "distance",
	"Distance tolerance for primitives that support vertex snapping"),
    BU_CMD_NUMBER("p", "max-hole-percent", struct repair_args, max_hole_area_percent,
	"percent", "Maximum hole area as a percentage of mesh area"),
    BU_CMD_NONNEGATIVE_NUMBER("a", "max-hole-area", struct repair_args, max_hole_area,
	"area", "Maximum hole area in mm"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand repair_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Object to repair", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema ged_repair_schema = {
    "repair", "Repair an object using its primitive-specific repair routine",
    repair_options, repair_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(repair_schema_validate, NULL)
};

static void
repair_usage(struct bu_vls *log_str, const char *cmd)
{
    char *option_help = bu_cmd_schema_describe(&ged_repair_schema);

    bu_vls_printf(log_str, "Usage: %s [options] object\n\n", cmd);
    if (option_help) {
	bu_vls_printf(log_str, "Options:\n%s\n", option_help);
	bu_free(option_help, "repair option help");
    }
}

extern "C" int
ged_repair(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal intern;
    struct directory *dp = RT_DIR_NULL;
    struct bu_vls log_str = BU_VLS_INIT_ZERO;
    struct bu_vls parse_msg = BU_VLS_INIT_ZERO;
    struct repair_args args = {0, BU_VLS_INIT_ZERO, -1.0, 5.0, 0.0};
    int operand_index;

    int original_argc = argc;
    const char **original_argv = (const char **)bu_calloc(argc + 1, sizeof(char *), "argv copy");
    for (int i = 0; i < argc; i++) original_argv[i] = argv[i];

    operand_index = bu_cmd_schema_parse_complete(&ged_repair_schema, &args,
	&parse_msg, argc - 1, argv + 1);

    if (operand_index < 0) {
	if (bu_vls_strlen(&parse_msg))
	    bu_vls_vlscat(gedp->ged_result_str, &parse_msg);
	bu_vls_free(&parse_msg);
	bu_free(original_argv, "argv copy");
	bu_vls_free(&args.out_name);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&parse_msg);

    if (args.print_help) {
        repair_usage(gedp->ged_result_str, original_argv[0]);
        bu_free(original_argv, "argv copy");
	bu_vls_free(&args.out_name);
        return GED_HELP;
    }

    const char *objname = argv[operand_index + 1];
    dp = db_lookup(gedp->dbip, objname, LOOKUP_QUIET);


    if (dp == RT_DIR_NULL) {
        bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"No valid object specified for repair\"}");
        bu_free(original_argv, "argv copy");
	bu_vls_free(&args.out_name);
        return BRLCAD_ERROR;
    }

    int in_place_repair = !bu_vls_strlen(&args.out_name);

    if (!in_place_repair) {
        if (db_lookup(gedp->dbip, bu_vls_cstr(&args.out_name), LOOKUP_QUIET) != RT_DIR_NULL) {
            bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"Object %s already exists!\"}", bu_vls_cstr(&args.out_name));
            bu_free(original_argv, "argv copy");
	    bu_vls_free(&args.out_name);
            return BRLCAD_ERROR;
        }
    }

    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, gedp->dbip, bn_mat_identity) < 0) {
        bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"Failed to get object\"}");
        bu_free(original_argv, "argv copy");
	bu_vls_free(&args.out_name);
        return BRLCAD_ERROR;
    }

    if (!EDOBJ[intern.idb_type].ft_repair) {
        bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"Repair operation not supported for this object type\"}");
        rt_db_free_internal(&intern);
        bu_free(original_argv, "argv copy");
	bu_vls_free(&args.out_name);
        return BRLCAD_ERROR;
    }

    int ret = EDOBJ[intern.idb_type].ft_repair(&log_str, &intern, NULL, original_argc, original_argv);

    if (ret == 0) {
        struct directory *out_dp = dp;
        const char *rname = objname;
        if (!in_place_repair) {
            rname = bu_vls_cstr(&args.out_name);
            out_dp = db_diradd(gedp->dbip, rname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
            if (out_dp == RT_DIR_NULL) {
                bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"Failed to add new directory entry\"}");
                bu_vls_free(&log_str);
                rt_db_free_internal(&intern);
                bu_free(original_argv, "argv copy");
		bu_vls_free(&args.out_name);
                return BRLCAD_ERROR;
            }
        }

        if (rt_db_put_internal(out_dp, gedp->dbip, &intern) < 0) {
            bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"Failed to write repaired object back to database\"}");
            bu_vls_free(&log_str);
            rt_db_free_internal(&intern);
            bu_free(original_argv, "argv copy");
	    bu_vls_free(&args.out_name);
            return BRLCAD_ERROR;
        }
    }

    if (bu_vls_strlen(&log_str) > 0) {
        bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_cstr(&log_str));
    } else if (ret == 0 || ret == 1) {
        bu_vls_printf(gedp->ged_result_str, "{\"status\":\"success\",\"message\":\"Successfully processed repair command\"}");
    } else {
        bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"Failed to repair object\"}");
    }

    bu_vls_free(&log_str);
    rt_db_free_internal(&intern);
    bu_free(original_argv, "argv copy");
    bu_vls_free(&args.out_name);

    return (ret == 0 || ret == 1) ? BRLCAD_OK : BRLCAD_ERROR;
}

#include "../include/plugin.h"

#define GED_REPAIR_COMMANDS(X, XID, XS, XIDS, XG, XIDG) \
    XS(repair, ged_repair, GED_CMD_DEFAULT, &ged_repair_schema)

GED_DECLARE_COMMAND_SET_WITH_MIXED_SCHEMA(GED_REPAIR_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_MIXED_SCHEMA("libged_repair", 1, GED_REPAIR_COMMANDS)

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

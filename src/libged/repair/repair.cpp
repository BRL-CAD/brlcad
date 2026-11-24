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

#include <vector>

#include "bu/arg.h"
#include "bu/vls.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../ged_private.h"
#include "rt/functab.h"

struct repair_args {
    int print_help;
    struct bu_vls out_name;
    int options_json;
    fastf_t tolerance;
    fastf_t max_hole_area_percent;
    fastf_t max_hole_area;
};

static const struct bu_arg_option repair_options[] = {
    BU_ARG_FLAG("h", "help", struct repair_args, print_help,
	"Print command help"),
    BU_ARG_VALUE("o", "output-name", struct repair_args, out_name, BU_ARG_VALUE_VLS, BU_ARG_REQUIRED, "name",
	"Write the repaired object to a new object"),
    BU_ARG_FLAG(NULL, "options-json", struct repair_args, options_json,
	"Return the primitive-specific repair options as JSON"),
    BU_ARG_VALUE("t", "tol", struct repair_args, tolerance, BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "distance",
	"Distance tolerance for primitives that support vertex snapping"),
    BU_ARG_NUMBER_RANGE("p", "max-hole-percent", struct repair_args,
	max_hole_area_percent, 1, 0.0, 1, 100.0, "percent",
	"Maximum hole area as a percentage of mesh area"),
    BU_ARG_VALUE_VALIDATE("a", "max-hole-area", struct repair_args, max_hole_area, BU_ARG_VALUE_NUMBER, bu_arg_nonnegative_number_validate,
	"area", "Maximum hole area in mm"),
    BU_ARG_OPTION_NULL
};
static const struct bu_arg_operand repair_operands[] = {
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_OBJECT, 1, 1,
	"Object to repair", "ged.db_object"),
    BU_ARG_OPERAND_NULL
};
static const struct bu_arg_schema ged_repair_schema = {
    "repair", "Repair an object using its primitive-specific repair routine",
    repair_options, repair_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL)
};

static void
repair_usage(struct bu_vls *log_str, const char *cmd)
{
    (void)bu_arg_schema_help_append(log_str, &ged_repair_schema, cmd);
}

extern "C" int
ged_repair(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	repair_usage(gedp->ged_result_str, argv[0]);
	return GED_HELP;
    }

    struct rt_db_internal intern;
    struct directory *dp = RT_DIR_NULL;
    struct bu_vls log_str = BU_VLS_INIT_ZERO;
    struct bu_vls parse_msg = BU_VLS_INIT_ZERO;
    struct repair_args args = {0, BU_VLS_INIT_ZERO, 0, -1.0, 5.0, 0.0};
    int operand_index;

    /* Schema parsing reorders argv; callers retain their original argument vector. */
    std::vector<const char *> parsed_argv(argv + 1, argv + argc);
    operand_index = bu_arg_schema_parse_complete(&ged_repair_schema, &args,
	&parse_msg, (int)parsed_argv.size(), parsed_argv.data());

    if (operand_index < 0) {
	bu_vls_strcat(gedp->ged_result_str, "Invalid repair options: ");
	if (bu_vls_strlen(&parse_msg))
	    bu_vls_vlscat(gedp->ged_result_str, &parse_msg);
	bu_vls_free(&parse_msg);
	bu_vls_free(&args.out_name);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&parse_msg);

    if (args.print_help) {
        repair_usage(gedp->ged_result_str, argv[0]);
	bu_vls_free(&args.out_name);
        return GED_HELP;
    }

    /* ft_repair receives only primitive options, without the command name,
     * output name, object operand, or option terminator.  Use the schema's
     * scanner so attached values follow the same rules as execution. */
    std::vector<const char *> primitive_argv;
    for (int i = 0; i < operand_index;) {
	int span = bu_arg_schema_option_span(&ged_repair_schema,
	    (size_t)(operand_index - i), parsed_argv.data() + i);
	if (span <= 0) {
	    bu_vls_strcat(gedp->ged_result_str, "Invalid repair options\n");
	    bu_vls_free(&args.out_name);
	    return BRLCAD_ERROR;
	}
	if (!BU_STR_EQUAL(parsed_argv[i], "--") &&
	    !bu_arg_schema_option_present(&ged_repair_schema, (size_t)span,
		parsed_argv.data() + i, "output-name"))
	    primitive_argv.insert(primitive_argv.end(), parsed_argv.begin() + i,
		parsed_argv.begin() + i + span);
	i += span;
    }

    const char *objname = parsed_argv[operand_index];
    dp = db_lookup(gedp->dbip, objname, LOOKUP_QUIET);

    if (dp == RT_DIR_NULL) {
        bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"No valid object specified for repair\"}");
	bu_vls_free(&args.out_name);
        return BRLCAD_ERROR;
    }

    int in_place_repair = !bu_vls_strlen(&args.out_name);

    if (!in_place_repair) {
        if (db_lookup(gedp->dbip, bu_vls_cstr(&args.out_name), LOOKUP_QUIET) != RT_DIR_NULL) {
            bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"Object %s already exists!\"}", bu_vls_cstr(&args.out_name));
	    bu_vls_free(&args.out_name);
            return BRLCAD_ERROR;
        }
    }

    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, gedp->dbip, bn_mat_identity) < 0) {
        bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"Failed to get object\"}");
	bu_vls_free(&args.out_name);
        return BRLCAD_ERROR;
    }

    if (!EDOBJ[intern.idb_type].ft_repair) {
        bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"Repair operation not supported for this object type\"}");
        rt_db_free_internal(&intern);
	bu_vls_free(&args.out_name);
        return BRLCAD_ERROR;
    }

    int ret = EDOBJ[intern.idb_type].ft_repair(&log_str, &intern, NULL,
	(int)primitive_argv.size(), primitive_argv.data());

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
		bu_vls_free(&args.out_name);
                return BRLCAD_ERROR;
            }
        }

        if (rt_db_put_internal(out_dp, gedp->dbip, &intern) < 0) {
            bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"Failed to write repaired object back to database\"}");
            bu_vls_free(&log_str);
            rt_db_free_internal(&intern);
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
    bu_vls_free(&args.out_name);

    return (ret == 0 || ret == 1) ? BRLCAD_OK : BRLCAD_ERROR;
}

#include "../include/plugin.h"

#define GED_REPAIR_COMMANDS(XS, XIDS, XG, XIDG) \
    XS(repair, ged_repair, GED_CMD_DEFAULT, &ged_repair_schema)

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA_AND_GRAMMAR(GED_REPAIR_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA_AND_GRAMMAR("libged_repair", 1, GED_REPAIR_COMMANDS)

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

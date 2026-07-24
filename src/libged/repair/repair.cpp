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

#include "bu/cmd.h"
#include "bu/vls.h"
#include "bu/opt.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../ged_private.h"
#include "rt/functab.h"

#ifdef BRLCAD_OPENVDB
#  include "bot_openvdb.h"
#endif

static void repair_usage(struct bu_vls *log_str, const char *cmd) {
    bu_vls_printf(log_str, "Usage: %s [-h] [-o output] [--openvdb] [--openvdb-voxel-size #] object\n", cmd);
}

extern "C" int
ged_repair(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal intern;
    struct directory *dp = RT_DIR_NULL;
    const char *objname = NULL;
    struct bu_vls log_str = BU_VLS_INIT_ZERO;
    struct bu_vls out_name = BU_VLS_INIT_ZERO;
    int print_help = 0;
    int use_openvdb = 0;
    fastf_t openvdb_voxel_size = 0.0;

    struct bu_opt_desc d[5];
    BU_OPT(d[0], "h", "help", "", NULL, &print_help, "Print help");
    BU_OPT(d[1], "o", "output-name", "<name>", bu_opt_vls, &out_name, "Output object name");
    BU_OPT(d[2], "", "openvdb", "", NULL, &use_openvdb, "Use OpenVDB level-set rebuild for BoT repair");
    BU_OPT(d[3], "", "openvdb-voxel-size", "#", bu_opt_fastf_t, &openvdb_voxel_size, "OpenVDB voxel size in model units (default: bbox diagonal/100)");
    BU_OPT_NULL(d[4]);

    int original_argc = argc;
    const char **original_argv = (const char **)bu_calloc(argc + 1, sizeof(char *), "argv copy");
    for (int i = 0; i < argc; i++) original_argv[i] = argv[i];

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (print_help || argc < 2) {
        repair_usage(gedp->ged_result_str, original_argv[0]);
        bu_free(original_argv, "argv copy");
        bu_vls_free(&out_name);
        return GED_HELP;
    }

    if (openvdb_voxel_size < 0.0) {
        bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"OpenVDB voxel size must be non-negative\"}");
        bu_free(original_argv, "argv copy");
        bu_vls_free(&out_name);
        return BRLCAD_ERROR;
    }

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        dp = db_lookup(gedp->dbip, argv[i], LOOKUP_QUIET);
        if (dp != RT_DIR_NULL) {
            objname = argv[i];
            break;
        }
    }

    if (!objname || dp == RT_DIR_NULL) {
        bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"No valid object specified for repair\"}");
        bu_free(original_argv, "argv copy");
        bu_vls_free(&out_name);
        return BRLCAD_ERROR;
    }

    int in_place_repair = 1;
    if (bu_vls_strlen(&out_name))
        in_place_repair = 0;

    if (!in_place_repair) {
        if (db_lookup(gedp->dbip, bu_vls_cstr(&out_name), LOOKUP_QUIET) != RT_DIR_NULL) {
            bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"Object %s already exists!\"}", bu_vls_cstr(&out_name));
            bu_free(original_argv, "argv copy");
            bu_vls_free(&out_name);
            return BRLCAD_ERROR;
        }
    }

    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, gedp->dbip, bn_mat_identity) < 0) {
        bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"Failed to get object\"}");
        bu_free(original_argv, "argv copy");
        bu_vls_free(&out_name);
        return BRLCAD_ERROR;
    }

    if (!EDOBJ[intern.idb_type].ft_repair) {
        bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"Repair operation not supported for this object type\"}");
        rt_db_free_internal(&intern);
        bu_free(original_argv, "argv copy");
        bu_vls_free(&out_name);
        return BRLCAD_ERROR;
    }

    int ret = BRLCAD_ERROR;

#ifndef BRLCAD_OPENVDB
    if (use_openvdb) {
        bu_vls_printf(&log_str, "{\"status\":\"error\",\"message\":\"OpenVDB repair was requested, but this BRL-CAD build has no OpenVDB support\"}");
    } else
#endif
    {
        /* The standard primitive repair is preferred.  OpenVDB is a
         * BoT-specific fallback because level-set reconstruction sacrifices
         * some geometric fidelity in exchange for a closed manifold result. */
        if (!use_openvdb)
            ret = EDOBJ[intern.idb_type].ft_repair(&log_str, &intern, NULL, original_argc, original_argv);

#ifdef BRLCAD_OPENVDB
        if (use_openvdb || ret < 0) {
            if (intern.idb_type == ID_BOT) {
                struct rt_bot_internal *bot = (struct rt_bot_internal *)intern.idb_ptr;
                if (bot && bot->mode == RT_BOT_SOLID) {
                    struct rt_bot_internal *vdb_bot = bot_openvdb_repair(bot, openvdb_voxel_size);
                    if (vdb_bot) {
                        intern.idb_meth->ft_ifree(&intern);
                        intern.idb_ptr = (void *)vdb_bot;
                        bu_vls_trunc(&log_str, 0);
                        bu_vls_printf(&log_str, "{\"status\":\"success\",\"message\":\"Successfully repaired BoT using OpenVDB level-set reconstruction\"}");
                        ret = 0;
                    } else {
                        bu_vls_trunc(&log_str, 0);
                        bu_vls_printf(&log_str, "{\"status\":\"error\",\"message\":\"OpenVDB could not reconstruct a valid solid BoT\"}");
                        ret = BRLCAD_ERROR;
                    }
                } else if (use_openvdb) {
                    bu_vls_trunc(&log_str, 0);
                    bu_vls_printf(&log_str, "{\"status\":\"error\",\"message\":\"OpenVDB repair requires a solid BoT\"}");
                    ret = BRLCAD_ERROR;
                }
            } else if (use_openvdb) {
                bu_vls_trunc(&log_str, 0);
                bu_vls_printf(&log_str, "{\"status\":\"error\",\"message\":\"OpenVDB repair is supported only for BoTs\"}");
                ret = BRLCAD_ERROR;
            }
        }
#endif
    }

    if (ret == 0) {
        struct directory *out_dp = dp;
        const char *rname = objname;
        if (!in_place_repair) {
            rname = bu_vls_cstr(&out_name);
            out_dp = db_diradd(gedp->dbip, rname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
            if (out_dp == RT_DIR_NULL) {
                bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"Failed to add new directory entry\"}");
                bu_vls_free(&log_str);
                rt_db_free_internal(&intern);
                bu_free(original_argv, "argv copy");
                bu_vls_free(&out_name);
                return BRLCAD_ERROR;
            }
        }

        if (rt_db_put_internal(out_dp, gedp->dbip, &intern) < 0) {
            bu_vls_printf(gedp->ged_result_str, "{\"status\":\"error\",\"message\":\"Failed to write repaired object back to database\"}");
            bu_vls_free(&log_str);
            rt_db_free_internal(&intern);
            bu_free(original_argv, "argv copy");
            bu_vls_free(&out_name);
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
    bu_vls_free(&out_name);

    return (ret == 0 || ret == 1) ? BRLCAD_OK : BRLCAD_ERROR;
}

#include "../include/plugin.h"

#define GED_REPAIR_COMMANDS(X, XID) \
    X(repair, ged_repair, GED_CMD_DEFAULT)

GED_DECLARE_COMMAND_SET(GED_REPAIR_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_repair", 1, GED_REPAIR_COMMANDS)

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

/*                        M I R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government
 */

#include "bu/getopt.h"
#include "bv/util.h"
#include "ged.h"

const char *ged_mirror_help =
    "Usage: mirror [options] source target\n\n"
    "Creates a mirrored copy of the 'source' object and saves it as 'target'.\n"
    "Useful for symmetrical geometry modeling in BRL-CAD.\n"
    "Options:\n"
    "  -x    Mirror along the X-axis\n"
    "  -y    Mirror along the Y-axis\n"
    "  -z    Mirror along the Z-axis\n";

int
ged_mirror_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[-h] [-p \"point\"] [-d \"dir\"] [-x|-y|-z] [-o offset] old new";

    int k;
    point_t mirror_pt = {0.0, 0.0, 0.0};
    vect_t mirror_dir = {1.0, 0.0, 0.0};
    double scanpt[3];
    double scandir[3];
    double mirror_offset = 0.0;

    int ret;
    struct rt_db_internal *ip;
    struct rt_db_internal internal;
    struct directory *dp;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    // Add help support
    if (argc == 2 && BU_STR_EQUAL(argv[1], "-h")) {
        bu_vls_printf(gedp->ged_result_str, "%s", ged_mirror_help);
        return GED_HELP;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
        bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
        return GED_HELP;
    }

    bu_optind = 1;
    while ((k = bu_getopt(argc, (char * const *)argv, "d:D:hHo:O:p:P:xXyYzZ?")) != -1) {
        if (bu_optopt == '?') k = 'h';
        switch (k) {
            case 'p':
            case 'P':
                if (sscanf(bu_optarg, "%lf %lf %lf", &scanpt[X], &scanpt[Y], &scanpt[Z]) != 3) {
                    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
                    return BRLCAD_ERROR;
                }
                VMOVE(mirror_pt, scanpt);
                break;
            case 'd':
            case 'D':
                if (sscanf(bu_optarg, "%lf %lf %lf", &scandir[X], &scandir[Y], &scandir[Z]) != 3) {
                    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
                    return BRLCAD_ERROR;
                }
                VMOVE(mirror_dir, scandir);
                break;
            case 'o':
            case 'O':
                if (sscanf(bu_optarg, "%lf", &mirror_offset) != 1) {
                    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
                    return BRLCAD_ERROR;
                }
                break;
            case 'x':
            case 'X':
                VSET(mirror_dir, 1.0, 0.0, 0.0);
                break;
            case 'y':
            case 'Y':
                VSET(mirror_dir, 0.0, 1.0, 0.0);
                break;
            case 'z':
            case 'Z':
                VSET(mirror_dir, 0.0, 0.0, 1.0);
                break;
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

    if (argc < 2 || argc > 4) {
        bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
        return BRLCAD_ERROR;
    } else if (argc == 3) {
        switch (argv[bu_optind+2][0]) {
            case 'x':
            case 'X':
                VSET(mirror_dir, 1.0, 0.0, 0.0);
                break;
            case 'y':
            case 'Y':
                VSET(mirror_dir, 0.0, 1.0, 0.0);
                break;
            case 'z':
            case 'Z':
                VSET(mirror_dir, 0.0, 0.0, 1.0);
                break;
            default:
                bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
                return BRLCAD_ERROR;
        }
    }

    if (db_lookup(gedp->dbip, argv[bu_optind+1], LOOKUP_QUIET) != RT_DIR_NULL) {
        bu_vls_printf(gedp->ged_result_str, "%s already exists\n", argv[bu_optind+1]);
        return BRLCAD_ERROR;
    }

    if ((dp = db_lookup(gedp->dbip, argv[bu_optind], LOOKUP_NOISY)) == RT_DIR_NULL) {
        bu_vls_printf(gedp->ged_result_str, "Unable to find solid [%s]\n", argv[bu_optind]);
        return BRLCAD_ERROR;
    }

    ret = rt_db_get_internal(&internal, dp, gedp->dbip, NULL, wdbp->wdb_resp);
    if (ret < 0) {
        bu_vls_printf(gedp->ged_result_str, "Unable to load solid [%s]\n", argv[bu_optind]);
        return BRLCAD_ERROR;
    }

    mirror_offset *= gedp->dbip->dbi_local2base;
    VUNITIZE(mirror_dir);
    VJOIN1(mirror_pt, mirror_pt, mirror_offset, mirror_dir);

    ip = rt_mirror(gedp->dbip, &internal, mirror_pt, mirror_dir, wdbp->wdb_resp);
    if (ip == NULL) {
        bu_vls_printf(gedp->ged_result_str, "Unable to mirror [%s]", argv[bu_optind]);
        return BRLCAD_ERROR;
    }

    dp = db_diradd(gedp->dbip, argv[bu_optind+1], RT_DIR_PHONY_ADDR, 0, dp->d_flags, (void *)&ip->idb_type);
    if (dp == RT_DIR_NULL) {
        bu_vls_printf(gedp->ged_result_str, "Unable to add [%s] to the database directory", argv[bu_optind+1]);
        return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(dp, gedp->dbip, ip, wdbp->wdb_resp) < 0) {
        bu_vls_printf(gedp->ged_result_str, "Unable to store [%s] to the database", argv[bu_optind+1]);
        return BRLCAD_ERROR;
    }

    {
        const char *object = (const char *)argv[bu_optind+1];
        const char *e_argv[2] = {"draw", NULL};
        e_argv[1] = object;
        (void)ged_exec_draw(gedp, 2, e_argv);
        bv_update(gedp->ged_gvp);
    }

    return BRLCAD_OK;
}

#ifdef GED_PLUGIN
#include "../include/plugin.h"

struct ged_cmd_impl mirror_cmd_impl = {
    "mirror",
    ged_mirror_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd mirror_cmd = { &mirror_cmd_impl };
const struct ged_cmd *mirror_cmds[] = { &mirror_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API, mirror_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

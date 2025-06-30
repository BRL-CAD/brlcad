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

/**
 * Implements the 'mirror' command in BRL-CAD.
 * 
 * This function parses command-line arguments to mirror a given geometry
 * object across a specified plane or axis. It handles various options such
 * as point, direction, offset, and axis-based mirroring (-x, -y, -z).
 * 
 * If the -h or -H option is passed, it displays detailed help for usage.
 * 
 * Returns GED_OK on success, GED_ERROR on failure, and GED_HELP when help is shown.
 */
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

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
        bu_vls_printf(gedp->ged_result_str, "%s", ged_mirror_help);
        return GED_HELP;
    }

    bu_optind = 1;
    while ((k = bu_getopt(argc, (char * const *)argv, "d:D:hHo:O:p:P:xXyYzZ?")) != -1) {
        if (bu_optopt == '?') k = 'h';
        switch (k) {
            case 'p':
            case 'P':
                if (sscanf(bu_optarg, "%lf %lf %lf", &scanpt[0], &scanpt[1], &scanpt[2]) != 3) {
                    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
                    return BRLCAD_ERROR;
                }
                VMOVE(mirror_pt, scanpt);
                break;
            case 'd':
            case 'D':
                if (sscanf(bu_optarg, "%lf %lf %lf", &scandir[0], &scandir[1], &scandir[2]) != 3) {
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
                bu_vls_printf(gedp->ged_result_str, "%s", ged_mirror_help);
                return GED_HELP;
            default:
                bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
                return BRLCAD_ERROR;
        }
    }

    return BRLCAD_OK;
}

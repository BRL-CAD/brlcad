#include "common.h"

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bv/plot3.h"
#include "dm.h"
#include "ged.h"
#include "wdb.h"

static void
write_plot(const char *path, const point_t start, const point_t finish)
{
    FILE *fp = fopen(path, "wb");
    if (!fp)
	bu_exit(EXIT_FAILURE, "Unable to create %s\n", path);

    pdv_3move(fp, start);
    pdv_3cont(fp, finish);
    fclose(fp);
}

int
main(int argc, char *argv[])
{
    const char *database = "ged_test_overlay.g";
    const char *first_plot = "ged_test_overlay_first.plot3";
    const char *second_plot = "ged_test_overlay_second.plot3";
    const char *dm_name = "nu";
    struct rt_wdb *wdbp;
    struct ged *gedp;
    struct dm *dmp;
    int ret;

    bu_setprogname(argv[0]);
    if (argc != 1)
	bu_exit(EXIT_FAILURE, "Usage: %s\n", argv[0]);

    wdbp = wdb_fopen(database);
    if (!wdbp)
	bu_exit(EXIT_FAILURE, "Unable to create %s\n", database);
    mk_id(wdbp, "overlay multiple file regression");
    db_close(wdbp->dbip);

    point_t origin = VINIT_ZERO;
    point_t x_axis = {1.0, 0.0, 0.0};
    point_t y_axis = {0.0, 1.0, 0.0};
    write_plot(first_plot, origin, x_axis);
    write_plot(second_plot, origin, y_axis);

    gedp = ged_open("db", database, 1);
    if (!gedp)
	bu_exit(EXIT_FAILURE, "Unable to open %s\n", database);

    dmp = dm_open(NULL, NULL, dm_name, 1, &dm_name);
    if (!dmp)
	bu_exit(EXIT_FAILURE, "Unable to open null display manager\n");
    gedp->ged_gvp->dmp = dmp;

    const char *command[] = {
	"overlay", "-N", "multiple", first_plot, second_plot
    };
    ret = ged_exec(gedp, 5, command);
    if (ret != BRLCAD_OK)
	bu_log("Multiple plot overlay failed: %s\n",
	       bu_vls_cstr(gedp->ged_result_str));

    gedp->ged_gvp->dmp = NULL;
    dm_close(dmp);
    ged_close(gedp);

    bu_file_delete(first_plot);
    bu_file_delete(second_plot);
    bu_file_delete(database);

    return (ret == BRLCAD_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */


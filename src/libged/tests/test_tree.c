#include "common.h"

#include <string.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "ged.h"
#include "wdb.h"

static int
add_combination(struct rt_wdb *wdbp, const char *name, const char *member)
{
    struct wmember members;

    BU_LIST_INIT(&members.l);
    if (!mk_addmember(member, &members.l, NULL, WMOP_UNION))
	return BRLCAD_ERROR;

    return mk_comb(wdbp, name, &members.l, 0, NULL, NULL, NULL,
		   0, 0, 0, 0, 0, 0, 0);
}

static int
cyclic_marker_count(const char *output)
{
    const char *marker = "[cyclic]";
    int count = 0;

    while ((output = strstr(output, marker)) != NULL) {
	count++;
	output += strlen(marker);
    }

    return count;
}

static int
check_tree(struct ged *gedp, const char *root, const char *expected_path)
{
    const char *command[] = {"tree", root};

    if (ged_exec(gedp, 2, command) != BRLCAD_OK) {
	bu_log("tree %s failed: %s\n", root, bu_vls_cstr(gedp->ged_result_str));
	return BRLCAD_ERROR;
    }

    const char *output = bu_vls_cstr(gedp->ged_result_str);
    if (!strstr(output, expected_path) || cyclic_marker_count(output) != 1) {
	bu_log("Unexpected tree output for %s:\n%s\n", root, output);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
main(int argc, char *argv[])
{
    const char *database = "ged_test_tree.g";
    struct rt_wdb *wdbp;
    struct ged *gedp;
    int failures = 0;

    bu_setprogname(argv[0]);
    if (argc != 1)
	bu_exit(EXIT_FAILURE, "Usage: %s\n", argv[0]);

    if (bu_file_exists(database, NULL))
	bu_file_delete(database);

    wdbp = wdb_fopen(database);
    if (!wdbp)
	bu_exit(EXIT_FAILURE, "Unable to create %s\n", database);

    mk_id(wdbp, "tree cyclic path regression");
    failures += add_combination(wdbp, "self.c", "self.c") != 0;
    failures += add_combination(wdbp, "first.c", "second.c") != 0;
    failures += add_combination(wdbp, "second.c", "first.c") != 0;
    db_close(wdbp->dbip);

    if (failures)
	bu_exit(EXIT_FAILURE, "Unable to create cyclic test geometry\n");

    gedp = ged_open("db", database, 1);
    if (!gedp)
	bu_exit(EXIT_FAILURE, "Unable to open %s\n", database);

    failures += check_tree(gedp, "self.c", "self.c/ [cyclic]") != 0;
    failures += check_tree(gedp, "first.c", "first.c/ [cyclic]") != 0;
    ged_close(gedp);

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
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


/*              A R R A N G E _ T E S T _ C O M M A N D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "bu.h"
#include "ged.h"
#include "raytrace.h"
#include "wdb.h"

namespace {

int failures = 0;

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, message); \
        ++failures; \
    } \
} while (0)

std::string database_path;

struct ged *
create_database()
{
    char path[MAXPATHLEN] = {0};
    FILE *temporary = bu_temp_file(path, MAXPATHLEN);
    if (!temporary)
        return nullptr;
    std::fclose(temporary);
    database_path = path;

    struct rt_wdb *wdbp = wdb_fopen(path);
    if (!wdbp)
        return nullptr;
    mk_id_units(wdbp, "arrange command tests", "mm");

    point_t minimum;
    point_t maximum;
    VSET(minimum, -0.75, -1.25, -0.5);
    VSET(maximum, 11.25, 8.75, 5.5);
    mk_rpp(wdbp, "container.s", minimum, maximum);

    VSET(minimum, 20.0, 0.0, 0.0);
    VSET(maximum, 26.0, 4.0, 2.0);
    mk_rpp(wdbp, "long.s", minimum, maximum);
    VSET(minimum, 30.0, 0.0, 0.0);
    VSET(maximum, 34.0, 4.0, 2.0);
    mk_rpp(wdbp, "square.s", minimum, maximum);
    VSET(minimum, 40.0, 0.0, 0.0);
    VSET(maximum, 42.0, 2.0, 2.0);
    mk_rpp(wdbp, "small.s", minimum, maximum);

    VSET(minimum, 0.0, 0.0, 0.0);
    VSET(maximum, 8.0, 3.0, 3.0);
    mk_rpp(wdbp, "upright-container.s", minimum, maximum);
    VSET(minimum, 50.0, 0.0, 0.0);
    VSET(maximum, 56.0, 2.0, 2.0);
    mk_rpp(wdbp, "upright-only.s", minimum, maximum);

    VSET(minimum, 0.0, 0.0, 0.0);
    VSET(maximum, 4.0, 10.0, 1.0);
    mk_rpp(wdbp, "leg-a.s", minimum, maximum);
    VSET(minimum, 4.0, 0.0, 0.0);
    VSET(maximum, 10.0, 4.0, 1.0);
    mk_rpp(wdbp, "leg-b.s", minimum, maximum);
    struct wmember members;
    BU_LIST_INIT(&members.l);
    mk_addmember("leg-a.s", &members.l, NULL, WMOP_UNION);
    mk_addmember("leg-b.s", &members.l, NULL, WMOP_UNION);
    mk_comb(wdbp, "l-container.c", &members.l, 0, NULL, NULL, NULL,
        0, 0, 0, 0, 0, 0, 0);

    db_close(wdbp->dbip);
    return ged_open("db", path, 1);
}

std::size_t
leaf_count(union tree *node)
{
    if (!node)
        return 0;
    if (node->tr_op == OP_DB_LEAF)
        return 1;
    return leaf_count(node->tr_b.tb_left) + leaf_count(node->tr_b.tb_right);
}

std::size_t
combination_leaf_count(struct ged *gedp, const char *name)
{
    struct directory *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
        return 0;
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, gedp->dbip, NULL) < 0)
        return 0;
    std::size_t count = 0;
    if (intern.idb_type == ID_COMBINATION) {
        auto *combination = static_cast<struct rt_comb_internal *>(intern.idb_ptr);
        count = leaf_count(combination->tree);
    }
    rt_db_free_internal(&intern);
    return count;
}

int
run(struct ged *gedp, const std::vector<const char *> &arguments)
{
    int result = ged_exec(gedp, static_cast<int>(arguments.size()),
        const_cast<const char **>(arguments.data()));
    if (result != BRLCAD_OK && result != GED_HELP)
        std::fprintf(stderr, "%s", bu_vls_cstr(gedp->ged_result_str));
    return result;
}

void
test_help_and_errors(struct ged *gedp)
{
    CHECK(run(gedp, {"arrange"}) == GED_HELP, "top-level help");
    CHECK(std::strstr(bu_vls_cstr(gedp->ged_result_str), "nest") != nullptr,
        "top-level help lists nest");
    CHECK(run(gedp, {"arrange", "help", "nest"}) == GED_HELP,
        "style help");
    CHECK(run(gedp, {"arrange", "unknown"}) == BRLCAD_ERROR,
        "unknown style is rejected");
    CHECK(run(gedp, {"arrange", "nest", "-d", "2", "-o", "orthogonal",
        "out.c", "container.s", "small.s"}) == BRLCAD_ERROR,
        "2D rejects spatial orientation changes");
    CHECK(run(gedp, {"arrange", "nest", "out.c", "missing.s", "small.s"}) ==
        BRLCAD_ERROR, "missing container is rejected");
    CHECK(db_lookup(gedp->dbip, "out.c", LOOKUP_QUIET) == RT_DIR_NULL,
        "validation failure writes no output");
}

void
test_spatial_and_dry_run(struct ged *gedp)
{
    CHECK(run(gedp, {"arrange", "nest", "-d", "3", "-s", "1", "-o",
        "orthogonal", "-q", "fast", "packed-3d.c", "container.s",
        "long.s", "square.s", "small.s"}) == BRLCAD_OK,
        "3D orthogonal nesting succeeds");
    CHECK(combination_leaf_count(gedp, "packed-3d.c") == 3,
        "3D output contains all source instances");
    CHECK(std::strstr(bu_vls_cstr(gedp->ged_result_str), "placed 3 of 3") != nullptr,
        "3D report gives placement count");

    CHECK(run(gedp, {"arrange", "nest", "-d", "3", "-s", "1", "-o",
        "upright", "-q", "fast", "upright-3d.c", "upright-container.s",
        "upright-only.s"}) == BRLCAD_OK,
        "3D upright nesting evaluates the identity orientation");
    CHECK(combination_leaf_count(gedp, "upright-3d.c") == 1,
        "3D upright output contains the identity-only part");

    CHECK(run(gedp, {"arrange", "nest", "-n", "-d", "3", "-s", "1",
        "dry.c", "container.s", "small.s"}) == BRLCAD_OK,
        "dry run succeeds");
    CHECK(db_lookup(gedp->dbip, "dry.c", LOOKUP_QUIET) == RT_DIR_NULL,
        "dry run writes no combination");
    CHECK(std::strstr(bu_vls_cstr(gedp->ged_result_str), "dry run") != nullptr,
        "dry-run report is explicit");
}

void
test_planar_concave_and_values(struct ged *gedp)
{
    CHECK(run(gedp, {"arrange", "nest", "-d", "2", "-a", "z", "-s", "1",
        "-q", "balanced", "packed-2d.c", "l-container.c", "small.s",
        "small.s", "small.s", "small.s"}) == BRLCAD_OK,
        "2D concave-container nesting succeeds");
    CHECK(combination_leaf_count(gedp, "packed-2d.c") == 4,
        "2D output contains repeated instances");

    CHECK(run(gedp, {"arrange", "nest", "-C", "-d", "2", "-a", "z",
        "-s", "1", "-q", "fast", "conservative.c", "l-container.c",
        "small.s", "small.s"}) == BRLCAD_OK,
        "conservative bounding-box nesting succeeds");
    CHECK(combination_leaf_count(gedp, "conservative.c") == 2,
        "conservative output contains all instances");
    CHECK(std::strstr(bu_vls_cstr(gedp->ged_result_str),
        "conservative bounds") != nullptr, "conservative report is explicit");

    CHECK(run(gedp, {"arrange", "nest", "-d", "2", "-s", "1", "-q", "fast",
        "--value", "long.s=1", "--value", "small.s=10", "valued.c",
        "l-container.c", "long.s", "small.s", "small.s", "small.s"}) ==
        BRLCAD_OK, "value-weighted nesting succeeds");
    CHECK(combination_leaf_count(gedp, "valued.c") >= 3,
        "higher-density repeated parts are retained");
}

void
test_fractional_origin_and_clearance(struct ged *gedp)
{
    CHECK(run(gedp, {"arrange", "nest", "-d", "3", "-s", "0.5", "-c", "0.5",
        "-g", "0,0,-1", "-q", "fast", "fractional.c", "container.s",
        "small.s", "small.s"}) == BRLCAD_OK,
        "fractional negative origin, clearance, and gravity succeed");
    CHECK(combination_leaf_count(gedp, "fractional.c") == 2,
        "clearance fixture retains both parts");
}

} // namespace

int
main(int UNUSED(argc), char **argv)
{
    bu_setprogname(argv[0]);
    struct ged *gedp = create_database();
    if (!gedp) {
        std::fprintf(stderr, "unable to create arrange test database\n");
        return 1;
    }
    test_help_and_errors(gedp);
    test_spatial_and_dry_run(gedp);
    test_planar_concave_and_values(gedp);
    test_fractional_origin_and_clearance(gedp);
    ged_close(gedp);
    if (!database_path.empty())
        bu_file_delete(database_path.c_str());
    if (failures)
        std::fprintf(stderr, "%d arrange command test(s) failed\n", failures);
    return failures ? 1 : 0;
}

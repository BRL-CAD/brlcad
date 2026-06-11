/*                     T E S T _ T I R E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file tests/ged_test_tire.cpp
 *
 * Regression tests for the tire command.
 */

#include "common.h"

#include <cstdio>
#include <cstring>
#include <optional>
#include <string>

#include "bu.h"
#include "ged.h"
#include "raytrace.h"
#include "rt/db_attr.h"
#include "wdb.h"

static int g_failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
	std::fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (msg)); \
	g_failures++; \
    } \
} while (0)

static void
check_present(struct ged *gedp, const char *name, const char *file, int line)
{
    if (db_lookup(gedp->dbip, name, LOOKUP_QUIET) == RT_DIR_NULL) {
	std::fprintf(stderr, "FAIL [%s:%d]: expected '%s' to exist\n", file, line, name);
	g_failures++;
    }
}

static void
check_absent(struct ged *gedp, const char *name, const char *file, int line)
{
    if (db_lookup(gedp->dbip, name, LOOKUP_QUIET) != RT_DIR_NULL) {
	std::fprintf(stderr, "FAIL [%s:%d]: expected '%s' to be absent\n", file, line, name);
	g_failures++;
    }
}

#define CHECK_PRESENT(gedp, name) check_present((gedp), (name), __FILE__, __LINE__)
#define CHECK_ABSENT(gedp, name) check_absent((gedp), (name), __FILE__, __LINE__)

static bool
comb_has_leaf(struct ged *gedp, const char *comb_name, const char *leaf_name)
{
    struct directory *dp = db_lookup(gedp->dbip, comb_name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	return false;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, gedp->dbip, NULL) < 0)
	return false;

    bool found = false;
    if (intern.idb_type == ID_COMBINATION) {
	auto *comb = static_cast<struct rt_comb_internal *>(intern.idb_ptr);
	RT_CK_COMB(comb);
	found = db_find_named_leaf(comb->tree, leaf_name) != TREE_NULL;
    }

    rt_db_free_internal(&intern);
    return found;
}

static void
check_comb_has_leaf(struct ged *gedp, const char *comb_name, const char *leaf_name, const char *file, int line)
{
    if (!comb_has_leaf(gedp, comb_name, leaf_name)) {
	std::fprintf(stderr, "FAIL [%s:%d]: expected '%s' to reference '%s'\n", file, line, comb_name, leaf_name);
	g_failures++;
    }
}

#define CHECK_COMB_HAS_LEAF(gedp, comb, leaf) check_comb_has_leaf((gedp), (comb), (leaf), __FILE__, __LINE__)

static bool
object_has_attr(struct ged *gedp, const char *name, const char *attr_name, const char *expected_value)
{
    struct directory *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	return false;

    struct bu_attribute_value_set avs;
    bu_avs_init_empty(&avs);
    if (db5_get_attributes(gedp->dbip, &avs, dp) != 0)
	return false;

    const char *value = bu_avs_get(&avs, attr_name);
    bool found = value && (!expected_value || BU_STR_EQUAL(value, expected_value));
    bu_avs_free(&avs);
    return found;
}

static std::optional<std::string>
object_attr_value(struct ged *gedp, const char *name, const char *attr_name)
{
    struct directory *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	return std::nullopt;

    struct bu_attribute_value_set avs;
    bu_avs_init_empty(&avs);
    if (db5_get_attributes(gedp->dbip, &avs, dp) != 0) {
	bu_avs_free(&avs);
	return std::nullopt;
    }

    std::optional<std::string> ret;
    const char *value = bu_avs_get(&avs, attr_name);
    if (value)
	ret = value;
    bu_avs_free(&avs);
    return ret;
}

static void
check_attr(struct ged *gedp, const char *name, const char *attr_name, const char *expected_value, const char *file, int line)
{
    if (!object_has_attr(gedp, name, attr_name, expected_value)) {
	std::fprintf(stderr, "FAIL [%s:%d]: expected '%s' attribute '%s'\n", file, line, name, attr_name);
	g_failures++;
    }
}

#define CHECK_ATTR_PRESENT(gedp, name, attr) check_attr((gedp), (name), (attr), nullptr, __FILE__, __LINE__)
#define CHECK_ATTR_EQ(gedp, name, attr, value) check_attr((gedp), (name), (attr), (value), __FILE__, __LINE__)

static struct ged *
open_test_db()
{
    char tmppath[MAXPATHLEN] = {0};
    FILE *fp = bu_temp_file(tmppath, MAXPATHLEN);
    if (!fp)
	return nullptr;
    std::fclose(fp);

    struct rt_wdb *wdbp = wdb_fopen(tmppath);
    if (!wdbp)
	return nullptr;

    db_close(wdbp->dbip);
    return ged_open("db", tmppath, 1);
}

static void
run_success_case(const char *label, int argc, const char *argv[], const char *top_name, bool expect_wheel, bool expect_tread = false)
{
    struct ged *gedp = open_test_db();
    if (!gedp) {
	std::fprintf(stderr, "SKIP %s: open failed\n", label);
	g_failures++;
	return;
    }

    int ret = ged_exec_tire(gedp, argc, argv);
    if (ret != BRLCAD_OK)
	std::fprintf(stderr, "%s\n", bu_vls_cstr(gedp->ged_result_str));
    CHECK(ret == BRLCAD_OK, label);
    CHECK_PRESENT(gedp, top_name);

    char tire_region[128] = {0};
    char air_region[128] = {0};
    char wheel_region[128] = {0};
    char tread_comb[128] = {0};
    std::snprintf(tire_region, sizeof(tire_region), "%s.tire.r", top_name);
    std::snprintf(air_region, sizeof(air_region), "%s.air.r", top_name);
    std::snprintf(wheel_region, sizeof(wheel_region), "%s.wheel.r", top_name);
    std::snprintf(tread_comb, sizeof(tread_comb), "%s.tread.c", top_name);
    CHECK_PRESENT(gedp, tire_region);
    CHECK_PRESENT(gedp, air_region);
    if (expect_wheel)
	CHECK_PRESENT(gedp, wheel_region);
    else
	CHECK_ABSENT(gedp, wheel_region);
    if (expect_tread)
	CHECK_PRESENT(gedp, tread_comb);
    else
	CHECK_ABSENT(gedp, tread_comb);

    CHECK_COMB_HAS_LEAF(gedp, top_name, tire_region);
    CHECK_COMB_HAS_LEAF(gedp, top_name, air_region);
    if (expect_wheel)
	CHECK_COMB_HAS_LEAF(gedp, top_name, wheel_region);
    CHECK_COMB_HAS_LEAF(gedp, tire_region, (std::string(top_name) + ".tire.outer.c").c_str());
    CHECK_COMB_HAS_LEAF(gedp, tire_region, (std::string(top_name) + ".tire.inner-cut.c").c_str());
    if (expect_tread)
	CHECK_COMB_HAS_LEAF(gedp, tire_region, tread_comb);

    CHECK_ATTR_EQ(gedp, top_name, "tire::generator", "ged tire");
    CHECK_ATTR_EQ(gedp, top_name, "tire::schema_version", "1");
    CHECK_ATTR_EQ(gedp, top_name, "tire::wheel_enabled", expect_wheel ? "1" : "0");
    CHECK_ATTR_EQ(gedp, top_name, "tire::tread_enabled", expect_tread ? "1" : "0");
    CHECK_ATTR_PRESENT(gedp, top_name, "tire::iso");
    CHECK_ATTR_PRESENT(gedp, top_name, "tire::width_mm");
    CHECK_ATTR_PRESENT(gedp, top_name, "tire::rim_radius_mm");
    std::optional<std::string> pattern_json = object_attr_value(gedp, top_name, "tire::tread_pattern_json");
    if (expect_tread) {
	CHECK(pattern_json.has_value(), "tread pattern JSON attribute");
	if (pattern_json) {
	    CHECK(pattern_json->find("\"type\"") != std::string::npos, "tread pattern JSON type field");
	    CHECK(pattern_json->find("\"pattern_count\"") != std::string::npos, "tread pattern JSON pattern_count field");
	    CHECK(pattern_json->find("\"schema_version\"") != std::string::npos, "tread pattern JSON schema_version field");
	}
    } else {
	CHECK(!pattern_json.has_value(), "slick tire should not have tread pattern JSON attribute");
    }

    ged_close(gedp);
}

static std::string
write_text_file(const std::string &contents)
{
    char tmppath[MAXPATHLEN] = {0};
    FILE *fp = bu_temp_file(tmppath, MAXPATHLEN);
    CHECK(fp != nullptr, "create text temp file");
    if (!fp)
	return "";

    std::fputs(contents.c_str(), fp);
    std::fclose(fp);

    return tmppath;
}

static void
run_stored_pattern_roundtrip_case()
{
    const char *label = "stored tread pattern JSON round trip";
    struct ged *gedp = open_test_db();
    if (!gedp) {
	std::fprintf(stderr, "SKIP %s: open failed\n", label);
	g_failures++;
	return;
    }

    const char *source_av[] = {"tire", "--tread-pattern", "mud-terrain", "-n", "roundtrip_source", nullptr};
    int ret = ged_exec_tire(gedp, 5, source_av);
    if (ret != BRLCAD_OK)
	std::fprintf(stderr, "%s\n", bu_vls_cstr(gedp->ged_result_str));
    CHECK(ret == BRLCAD_OK, label);

    std::optional<std::string> pattern_json = object_attr_value(gedp, "roundtrip_source", "tire::tread_pattern_json");
    CHECK(pattern_json.has_value(), "source tire stores tread pattern JSON");
    if (!pattern_json) {
	ged_close(gedp);
	return;
    }

    const std::string pattern_file = write_text_file(*pattern_json);
    const char *reused_av[] = {"tire", "--tread-pattern-file", pattern_file.c_str(), "-n", "roundtrip_reused", nullptr};
    ret = ged_exec_tire(gedp, 5, reused_av);
    if (ret != BRLCAD_OK)
	std::fprintf(stderr, "%s\n", bu_vls_cstr(gedp->ged_result_str));
    CHECK(ret == BRLCAD_OK, "stored tread pattern JSON can be reused");
    CHECK_PRESENT(gedp, "roundtrip_reused");
    CHECK_PRESENT(gedp, "roundtrip_reused.tread.c");
    CHECK_ATTR_EQ(gedp, "roundtrip_reused", "tire::tread_pattern_source", "file");
    CHECK_ATTR_EQ(gedp, "roundtrip_reused", "tire::tread_pattern_id", "mud-terrain");

    ged_close(gedp);
}

static void
run_success_contains_case(const char *label, int argc, const char *argv[], const char *needle)
{
    struct ged *gedp = open_test_db();
    if (!gedp) {
	std::fprintf(stderr, "SKIP %s: open failed\n", label);
	g_failures++;
	return;
    }

    int ret = ged_exec_tire(gedp, argc, argv);
    if (ret != BRLCAD_OK)
	std::fprintf(stderr, "%s\n", bu_vls_cstr(gedp->ged_result_str));
    CHECK(ret == BRLCAD_OK, label);
    CHECK(std::strstr(bu_vls_cstr(gedp->ged_result_str), needle) != nullptr, label);

    ged_close(gedp);
}

static void
run_failure_case(const char *label, int argc, const char *argv[], const char *top_name)
{
    struct ged *gedp = open_test_db();
    if (!gedp) {
	std::fprintf(stderr, "SKIP %s: open failed\n", label);
	g_failures++;
	return;
    }

    int ret = ged_exec_tire(gedp, argc, argv);
    CHECK(ret != BRLCAD_OK, label);
    CHECK_ABSENT(gedp, top_name);

    ged_close(gedp);
}

static void
run_partial_cleanup_case()
{
    const char *label = "partial generation cleanup";
    const char *top_name = "cleanup_tire";
    const char *collision_name = "cleanup_tire.tire.outer.body.c";
    struct ged *gedp = open_test_db();
    if (!gedp) {
	std::fprintf(stderr, "SKIP %s: open failed\n", label);
	g_failures++;
	return;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    point_t base;
    vect_t height;
    VSET(base, 0, 0, 0);
    VSET(height, 0, 1, 0);
    CHECK(mk_rcc(wdbp, collision_name, base, height, 1) == 0, "create collision object");

    const char *argv[] = {"tire", "-n", top_name, nullptr};
    int ret = ged_exec_tire(gedp, 3, argv);
    CHECK(ret != BRLCAD_OK, label);
    CHECK(std::strstr(bu_vls_cstr(gedp->ged_result_str), "already exists") != nullptr, label);

    CHECK_PRESENT(gedp, collision_name);
    CHECK_ABSENT(gedp, top_name);
    CHECK_ABSENT(gedp, "cleanup_tire.tire.outer.surface.tread.s");
    CHECK_ABSENT(gedp, "cleanup_tire.tire.outer.core.s");
    CHECK_ABSENT(gedp, "cleanup_tire.tire.outer.c");

    ged_close(gedp);
}

static std::string
write_custom_pattern_file()
{
    char tmppath[MAXPATHLEN] = {0};
    FILE *fp = bu_temp_file(tmppath, MAXPATHLEN);
    CHECK(fp != nullptr, "create custom pattern temp file");
    if (!fp)
	return "";

    const char *json = "{"
	"\"$schema\":\"https://brlcad.org/schemas/tire-tread-pattern.schema.json\","
	"\"schema_version\":1,"
	"\"id\":\"test-custom\","
	"\"type\":\"hybrid\","
	"\"symmetry\":\"symmetric\","
	"\"terrain\":\"AT\","
	"\"axle\":\"all\","
	"\"void\":0.34,"
	"\"grooves\":4,"
	"\"groove_angle\":12,"
	"\"block\":\"medium\","
	"\"pitch\":{\"type\":\"variable\",\"count\":3},"
	"\"pitch_seq\":[1.0,1.2,0.8],"
	"\"sipes\":\"low\","
	"\"shoulder\":\"semi-open\","
	"\"center\":\"ribbed\","
	"\"tie_bars\":true,"
	"\"shape\":2,"
	"\"pattern_count\":12"
	"}";
    std::fputs(json, fp);
    std::fclose(fp);

    return tmppath;
}

static std::string
write_bad_schema_pattern_file()
{
    const char *json = "{"
	"\"schema_version\":2,"
	"\"type\":\"rib\","
	"\"symmetry\":\"symmetric\""
	"}";
    return write_text_file(json);
}

static std::string
write_unknown_field_pattern_file()
{
    char tmppath[MAXPATHLEN] = {0};
    FILE *fp = bu_temp_file(tmppath, MAXPATHLEN);
    CHECK(fp != nullptr, "create invalid pattern temp file");
    if (!fp)
	return "";

    const char *json = "{"
	"\"type\":\"lug\","
	"\"symmetry\":\"directional\","
	"\"groove_map\":{\"type\":\"V\",\"angle\":25,\"width\":0.12,\"depth\":0.9}"
	"}";
    std::fputs(json, fp);
    std::fclose(fp);

    return tmppath;
}

int
main(int ac, char *av[])
{
    bu_setprogname(av[0]);

    if (ac != 1) {
	std::fprintf(stderr, "Usage: %s\n", av[0]);
	return 1;
    }

    const char *default_av[] = {"tire", "-n", "default_tire", nullptr};
    run_success_case("default tire generation", 3, default_av, "default_tire", true);

    const char *no_wheel_av[] = {"tire", "-w", "0", "-n", "no_wheel_tire", nullptr};
    run_success_case("wheel disabled tire generation", 5, no_wheel_av, "no_wheel_tire", false);

    const char *profile1_av[] = {"tire", "-t", "1", "-p", "1", "-c", "12", "-n", "profile1_tire", nullptr};
    run_success_case("tread profile 1 generation", 9, profile1_av, "profile1_tire", true, true);

    const char *profile2_av[] = {"tire", "-t", "2", "-p", "2", "-c", "12", "-n", "profile2_tire", nullptr};
    run_success_case("tread profile 2 generation", 9, profile2_av, "profile2_tire", true, true);

    const char *named_pattern_av[] = {"tire", "--tread-pattern", "mud-terrain", "-n", "mud_tire", nullptr};
    run_success_case("named tread pattern generation", 5, named_pattern_av, "mud_tire", true, true);

    const char *highway_pattern_av[] = {"tire", "--tread-pattern", "highway-rib", "-n", "highway_tire", nullptr};
    run_success_case("highway tread pattern generation", 5, highway_pattern_av, "highway_tire", true, true);

    const char *all_terrain_pattern_av[] = {"tire", "--tread-pattern", "all-terrain", "-n", "all_terrain_tire", nullptr};
    run_success_case("all-terrain tread pattern generation", 5, all_terrain_pattern_av, "all_terrain_tire", true, true);

    const char *winter_pattern_av[] = {"tire", "--tread-pattern", "winter-siped", "-n", "winter_tire", nullptr};
    run_success_case("winter tread pattern generation", 5, winter_pattern_av, "winter_tire", true, true);

    const char *commercial_pattern_av[] = {"tire", "--tread-pattern", "commercial-rib", "-n", "commercial_tire", nullptr};
    run_success_case("commercial tread pattern generation", 5, commercial_pattern_av, "commercial_tire", true, true);

    std::string custom_pattern_file = write_custom_pattern_file();
    const char *custom_pattern_av[] = {"tire", "--tread-pattern-file", custom_pattern_file.c_str(), "-n", "custom_tire", nullptr};
    run_success_case("custom tread pattern file generation", 5, custom_pattern_av, "custom_tire", true, true);

    const char *list_patterns_av[] = {"tire", "--list-tread-patterns", nullptr};
    run_success_contains_case("list tread patterns", 2, list_patterns_av, "mud-terrain");

    const char *bad_profile_av[] = {"tire", "-t", "3", "-n", "bad_profile", nullptr};
    run_failure_case("invalid tread profile", 5, bad_profile_av, "bad_profile");

    const char *bad_pattern_av[] = {"tire", "-p", "not-a-pattern", "-n", "bad_pattern", nullptr};
    run_failure_case("invalid tread pattern", 5, bad_pattern_av, "bad_pattern");

    const char *bad_iso_av[] = {"tire", "-d", "215x55Q17", "-n", "bad_iso", nullptr};
    run_failure_case("invalid ISO string", 5, bad_iso_av, "bad_iso");

    const char *bad_iso_suffix_av[] = {"tire", "-d", "215/55R17junk", "-n", "bad_iso_suffix", nullptr};
    run_failure_case("invalid ISO string suffix", 5, bad_iso_suffix_av, "bad_iso_suffix");

    const char *bad_count_av[] = {"tire", "-t", "1", "-c", "2", "-n", "bad_count", nullptr};
    run_failure_case("invalid tread count", 7, bad_count_av, "bad_count");

    const char *high_count_av[] = {"tire", "-t", "1", "-c", "513", "-n", "high_count", nullptr};
    run_failure_case("excessive tread count", 7, high_count_av, "high_count");

    const char *conflicting_pattern_av[] = {"tire", "-p", "highway-rib", "--tread-pattern-file", custom_pattern_file.c_str(), "-n", "conflicting_pattern", nullptr};
    run_failure_case("conflicting tread pattern options", 7, conflicting_pattern_av, "conflicting_pattern");

    std::string unknown_field_pattern_file = write_unknown_field_pattern_file();
    const char *unknown_field_pattern_av[] = {"tire", "--tread-pattern-file", unknown_field_pattern_file.c_str(), "-n", "unknown_field_pattern", nullptr};
    run_failure_case("unknown JSON tread pattern field", 5, unknown_field_pattern_av, "unknown_field_pattern");

    std::string bad_schema_pattern_file = write_bad_schema_pattern_file();
    const char *bad_schema_pattern_av[] = {"tire", "--tread-pattern-file", bad_schema_pattern_file.c_str(), "-n", "bad_schema_pattern", nullptr};
    run_failure_case("unsupported JSON tread pattern schema", 5, bad_schema_pattern_av, "bad_schema_pattern");

    run_partial_cleanup_case();
    run_stored_pattern_roundtrip_case();

    return g_failures ? 1 : 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

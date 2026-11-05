/*                 F A C E T I Z E _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file facetize/tests/brep_output.cpp
 *
 * End-to-end coverage for region-preserving B-Rep output.
 */

#include "common.h"

#include <filesystem>
#include <set>
#include <string>

#include "bu/app.h"
#include "bu/env.h"
#include "bu/file.h"
#include "brep/boolean.h"
#include "ged.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

static const char TEST_REGION[] = "simple.r";
static const char TEST_REGION_TWO[] = "second.r";
static const char TEST_EMPTY_REGION[] = "empty.r";
static const char TEST_UNSUPPORTED_REGION[] = "unsupported.r";
static const char TEST_MODEL[] = "model.c";
static const char TEST_OUTPUT[] = "model.brep";

static int
create_input(const char *path, bool include_unsupported = false)
{
    (void)bu_file_delete(path);
    struct db_i *dbip = db_create(path, BRLCAD_DB_FORMAT_LATEST);
    if (!dbip)
	return BRLCAD_ERROR;
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (!wdbp) {
	db_close(dbip);
	return BRLCAD_ERROR;
    }

    point_t center = {0.0, 0.0, 0.0};
    point_t second_center = {30.0, 0.0, 0.0};
    int ret = mk_sph(wdbp, "sphere.s", center, 10.0) < 0 ||
	mk_sph(wdbp, "second.s", second_center, 5.0) < 0 ?
	BRLCAD_ERROR : BRLCAD_OK;
    if (ret == BRLCAD_OK &&
	    mk_comb1(wdbp, TEST_REGION, "sphere.s", 1) < 0)
	ret = BRLCAD_ERROR;
    if (ret == BRLCAD_OK &&
	    mk_comb1(wdbp, TEST_REGION_TWO, "second.s", 1) < 0)
	ret = BRLCAD_ERROR;
    if (ret == BRLCAD_OK && include_unsupported) {
	const vect_t normal = {0.0, 0.0, 1.0};
	if (mk_half(wdbp, "standalone.half", normal, 0.0) < 0 ||
		mk_comb1(wdbp, TEST_UNSUPPORTED_REGION,
		    "standalone.half", 1) < 0)
	    ret = BRLCAD_ERROR;
    }
    if (ret == BRLCAD_OK) {
	struct bu_list empty_members;
	BU_LIST_INIT(&empty_members);
	(void)mk_addmember("sphere.s", &empty_members, NULL, WMOP_UNION);
	(void)mk_addmember("sphere.s", &empty_members, NULL, WMOP_SUBTRACT);
	if (mk_comb(wdbp, TEST_EMPTY_REGION, &empty_members, 1, NULL, NULL,
		NULL, 0, 0, 0, 0, 0, 0, 0) < 0)
	    ret = BRLCAD_ERROR;
    }
    if (ret == BRLCAD_OK) {
	struct bu_list members;
	BU_LIST_INIT(&members);
	(void)mk_addmember(TEST_REGION, &members, NULL, WMOP_UNION);
	(void)mk_addmember(TEST_REGION_TWO, &members, NULL, WMOP_UNION);
	(void)mk_addmember(TEST_EMPTY_REGION, &members, NULL, WMOP_UNION);
	if (include_unsupported)
	    (void)mk_addmember(TEST_UNSUPPORTED_REGION, &members, NULL,
		    WMOP_UNION);
	if (mk_comb(wdbp, TEST_MODEL, &members, 0, NULL, NULL, NULL,
		0, 0, 0, 0, 0, 0, 0) < 0)
	    ret = BRLCAD_ERROR;
    }

    wdb_close(wdbp);
    return ret;
}

static int
run_facetize(const char *path, int jobs)
{
    struct ged *gedp = ged_open("db", path, 1);
    if (!gedp)
	return BRLCAD_ERROR;
    const std::string job_count = std::to_string(jobs);
    const char *argv[] = {
	"facetize", "--regions", "--brep-output", "--jobs",
	job_count.c_str(),
	TEST_MODEL, TEST_OUTPUT, NULL
    };
    const int ret = ged_exec(gedp, 7, argv);
    if (ret != BRLCAD_OK)
	bu_log("[facetize_brep_output] command failed: %s\n",
		bu_vls_cstr(gedp->ged_result_str));
    ged_close(gedp);
    return ret;
}

static int
run_facetize_tolerating_failures(const char *path, int jobs)
{
    struct ged *gedp = ged_open("db", path, 1);
    if (!gedp)
	return BRLCAD_ERROR;
    const std::string job_count = std::to_string(jobs);
    const char *argv[] = {
	"facetize", "--regions", "--brep-output", "--tolerate-failures",
	"--jobs", job_count.c_str(), TEST_MODEL, TEST_OUTPUT, NULL
    };
    const int ret = ged_exec(gedp, 8, argv);
    if (ret != BRLCAD_OK)
	bu_log("[facetize_brep_output] tolerated command failed: %s\n",
		bu_vls_cstr(gedp->ged_result_str));
    ged_close(gedp);
    return ret;
}

static bool
object_exists(const char *path, const char *name)
{
    struct db_i *dbip = db_open(path, DB_OPEN_READONLY);
    if (!dbip)
	return false;
    const bool exists = db_dirbuild(dbip) >= 0 &&
	db_lookup(dbip, name, LOOKUP_QUIET) != RT_DIR_NULL;
    db_close(dbip);
    return exists;
}

static std::set<std::filesystem::path>
worker_crash_logs()
{
    std::set<std::filesystem::path> logs;
    std::error_code error;
    std::filesystem::directory_iterator entry(
	std::filesystem::current_path(), error);
    const std::filesystem::directory_iterator end;
    while (!error && entry != end) {
	const std::string name = entry->path().filename().string();
	if (name.find("brep_boolean_worker") == 0 &&
		name.size() >= 9 &&
		name.compare(name.size() - 9, 9, "-bomb.log") == 0)
	    logs.insert(entry->path());
	entry.increment(error);
    }
    return logs;
}

static bool
verify_output(const char *path)
{
    struct db_i *dbip = db_open(path, DB_OPEN_READONLY);
    if (!dbip || db_dirbuild(dbip) < 0) {
	if (dbip)
	    db_close(dbip);
	return false;
    }

    struct directory *top = db_lookup(dbip, TEST_OUTPUT, LOOKUP_QUIET);
    bool valid = top && (top->d_flags & RT_DIR_COMB);
    size_t brep_count = 0;
    size_t solid_count = 0;
    size_t empty_count = 0;
    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if (dp->d_minor_type != ID_BREP)
	    continue;
	brep_count++;
	struct rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	if (rt_db_get_internal(&internal, dp, dbip, NULL) < 0) {
	    valid = false;
	    continue;
	}
	struct rt_brep_internal *brep_internal =
	    (struct rt_brep_internal *)internal.idb_ptr;
	const char *output_kind = bu_avs_get(&internal.idb_avs,
	    "facetize_output");
	const char *fidelity = bu_avs_get(&internal.idb_avs,
	    "brep_conversion_fidelity");
	const char *source_region = bu_avs_get(&internal.idb_avs,
	    "brep_source_region");
	const char *conversion_status = bu_avs_get(&internal.idb_avs,
	    "brep_conversion_status");
	valid = valid && brep_internal && brep_internal->brep && output_kind &&
	    BU_STR_EQUAL(output_kind, "brep") && source_region &&
	    source_region[0] && conversion_status;
	if (conversion_status && BU_STR_EQUAL(conversion_status, "1")) {
	    empty_count++;
	    valid = valid && brep_internal->brep->m_F.Count() == 0;
	} else {
	    solid_count++;
	    valid = valid && brep_internal->brep->IsValid() &&
		brep_internal->brep->IsSolid() && fidelity &&
		BU_STR_EQUAL(fidelity, "exact");
	}
	rt_db_free_internal(&internal);
    } FOR_ALL_DIRECTORY_END;
    db_close(dbip);
    return valid && brep_count == 3 && solid_count == 2 && empty_count == 1;
}

static bool
verify_hybrid_output(const char *path)
{
    if (!verify_output(path))
	return false;

    struct db_i *dbip = db_open(path, DB_OPEN_READONLY);
    if (!dbip || db_dirbuild(dbip) < 0) {
	if (dbip)
	    db_close(dbip);
	return false;
    }

    struct directory *fallback = db_lookup(dbip,
	"facetize_unsupported.r", LOOKUP_QUIET);
    bool valid = fallback && (fallback->d_flags & RT_DIR_COMB);
    if (valid) {
	struct rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	if (rt_db_get_internal(&internal, fallback, dbip, NULL) < 0) {
	    valid = false;
	} else {
	    const char *output_kind = bu_avs_get(&internal.idb_avs,
		    "facetize_output");
	    const char *fidelity = bu_avs_get(&internal.idb_avs,
		    "brep_conversion_fidelity");
	    const char *status = bu_avs_get(&internal.idb_avs,
		    "brep_conversion_status");
	    const char *transport = bu_avs_get(&internal.idb_avs,
		    "brep_worker_transport_status");
	    const std::string expected_status =
		std::to_string(BREP_BOOLEAN_UNSUPPORTED);
	    valid = output_kind && BU_STR_EQUAL(output_kind, "original_csg") &&
		fidelity && BU_STR_EQUAL(fidelity, "unconverted") && status &&
		BU_STR_EQUAL(status, expected_status.c_str()) && transport &&
		BU_STR_EQUAL(transport, "ok");
	    rt_db_free_internal(&internal);
	}
    }
    db_close(dbip);
    return valid;
}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 2) {
	bu_log("Usage: %s test-directory\n", argv[0]);
	return 1;
    }

    std::filesystem::path test_dir(argv[1]);
    std::error_code error;
    std::filesystem::remove_all(test_dir, error);
    if (error || !std::filesystem::create_directories(test_dir, error) ||
	error)
	return 1;
    const std::filesystem::path database = test_dir / "brep_output.g";

    bool passed =
	create_input(database.string().c_str()) == BRLCAD_OK &&
	run_facetize(database.string().c_str(), 2) == BRLCAD_OK &&
	verify_output(database.string().c_str());

    const std::filesystem::path hybrid_database =
	test_dir / "brep_output_hybrid.g";
    passed = passed &&
	create_input(hybrid_database.string().c_str(), true) == BRLCAD_OK &&
	run_facetize_tolerating_failures(
	    hybrid_database.string().c_str(), 2) == BRLCAD_OK &&
	verify_hybrid_output(hybrid_database.string().c_str());

    const std::filesystem::path crash_database = test_dir / "worker_crash.g";
    const std::filesystem::path crash_claim = test_dir / "worker_crash.claim";
    const std::set<std::filesystem::path> prior_crash_logs =
	worker_crash_logs();
    (void)bu_file_delete(crash_claim.string().c_str());
    if (create_input(crash_database.string().c_str()) != BRLCAD_OK) {
	passed = false;
    } else {
	(void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT",
	    "crash:simple.r", 1);
	(void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT_FILE",
	    crash_claim.string().c_str(), 1);
	const int crash_result = run_facetize(crash_database.string().c_str(),
	    1);
	(void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT", "", 1);
	(void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT_FILE", "", 1);
	passed = passed && crash_result == BRLCAD_ERROR &&
	    bu_file_exists(crash_claim.string().c_str(), NULL) &&
	    !object_exists(crash_database.string().c_str(), TEST_OUTPUT);
    }
    for (const std::filesystem::path &log : worker_crash_logs()) {
	if (prior_crash_logs.find(log) == prior_crash_logs.end())
	    (void)bu_file_delete(log.string().c_str());
    }
    if (!passed)
	bu_log("[facetize_brep_output] FAIL\n");
    else
	std::filesystem::remove_all(test_dir, error);
    return passed ? 0 : 1;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

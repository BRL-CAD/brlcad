/*                  B R E P _ B O O L E A N _ W O R K E R . C P P
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES ARE DISCLAIMED.
 */
/** @file brep_boolean_worker.cpp
 *
 * Exercise persistent B-Rep workers, asynchronous pool dispatch, job-count
 * and resident-memory retirement, and recovery from malformed responses,
 * timeouts, and child termination.
 */

#include "common.h"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/log.h"
#include "brep.h"
#include "raytrace.h"
#include "rt/comb.h"
#include "wdb.h"


namespace {

int failures = 0;


static void
expect(bool condition, const char *description)
{
    if (!condition) {
	bu_log("FAIL: %s\n", description);
	failures++;
    }
}


static ON_Brep *
make_box(double minimum_x, double maximum_x)
{
    ON_3dPoint corners[8] = {
	ON_3dPoint(minimum_x, 0.0, 0.0),
	ON_3dPoint(maximum_x, 0.0, 0.0),
	ON_3dPoint(maximum_x, 1.0, 0.0),
	ON_3dPoint(minimum_x, 1.0, 0.0),
	ON_3dPoint(minimum_x, 0.0, 1.0),
	ON_3dPoint(maximum_x, 0.0, 1.0),
	ON_3dPoint(maximum_x, 1.0, 1.0),
	ON_3dPoint(minimum_x, 1.0, 1.0)
    };
    return ON_BrepBox(corners);
}


static bool
add_union(struct rt_wdb *writer, const char *combination,
    const char *left, const char *right)
{
    struct bu_list members;
    BU_LIST_INIT(&members);
    if (!mk_addmember(left, &members, NULL, WMOP_UNION) ||
	!mk_addmember(right, &members, NULL, WMOP_UNION))
    {
	return false;
    }
    return mk_comb(writer, combination, &members, 1, NULL, NULL, NULL,
	0, 0, 0, 0, 0, 0, 0) >= 0;
}


static bool
create_database(const std::string &path)
{
    (void)bu_file_delete(path.c_str());
    struct db_i *dbip = db_create(path.c_str(), BRLCAD_DB_FORMAT_LATEST);
    if (!dbip)
	return false;
    struct rt_wdb *writer = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (!writer) {
	db_close(dbip);
	return false;
    }

    const char *primitive_names[] = {
	"a.s", "b.s", "c.s", "d.s", "e.s", "f.s"
    };
    const vect_t halfspace_normal = {0.0, 0.0, 1.0};
    bool success = true;
    for (size_t i = 0; i < 6; ++i) {
	point_t minimum = {static_cast<double>(i) * 3.0, 0.0, 0.0};
	point_t maximum = {static_cast<double>(i) * 3.0 + 1.0, 1.0, 1.0};
	if (mk_rpp(writer, primitive_names[i], minimum, maximum) < 0) {
	    success = false;
	    break;
	}
    }
    if (success)
	success = add_union(writer, "one.c", "a.s", "b.s") &&
	    add_union(writer, "two.c", "c.s", "d.s") &&
	    add_union(writer, "three.c", "e.s", "f.s");
    if (success && mk_half(writer, "standalone.half", halfspace_normal,
	    0.5) < 0)
	success = false;

    ON_Brep *left = make_box(0.0, 1.0);
    ON_Brep *right = make_box(2.0, 3.0);
    ON_Brep *invalid = make_box(4.0, 5.0);
    if (invalid != NULL)
	invalid->m_F[0].m_si = -1;
    if (!left || !right || mk_brep(writer, "left.brep", left) < 0 ||
	mk_brep(writer, "right.brep", right) < 0 || !invalid ||
	mk_brep(writer, "invalid.brep", invalid) < 0)
    {
	success = false;
    }
    delete left;
    delete right;
    delete invalid;
    wdb_close(writer);
    return success;
}


static std::map<uint64_t, struct rt_brep_worker_result>
collect(struct rt_brep_worker_pool *pool)
{
    std::map<uint64_t, struct rt_brep_worker_result> results;
    while (rt_brep_worker_pool_pending(pool)) {
	struct rt_brep_worker_result result = RT_BREP_WORKER_RESULT_INIT;
	const int poll_result = rt_brep_worker_pool_poll(pool, &result, 1000);
	if (poll_result < 0) {
	    expect(false, "worker pool polling succeeds");
	    break;
	}
	if (!poll_result)
	    continue;
	results[result.request_id] = result;
    }
    return results;
}


static bool
successful(const struct rt_brep_worker_result &result)
{
    return result.transport_status == RT_BREP_WORKER_TRANSPORT_OK &&
	result.boolean_status == BREP_BOOLEAN_SUCCESS && result.brep &&
	result.brep->IsValid() && result.brep->IsSolid();
}


static void
destroy_results(std::map<uint64_t, struct rt_brep_worker_result> &results)
{
    for (auto &entry : results)
	rt_brep_worker_result_destroy(&entry.second);
    results.clear();
}


static void
test_reuse_and_retirement(const std::string &database)
{
    struct rt_brep_worker_options options = RT_BREP_WORKER_OPTIONS_INIT;
    options.max_jobs_per_worker = 2;
    options.max_worker_resident_bytes = std::numeric_limits<size_t>::max();
    struct rt_brep_worker_pool *pool = rt_brep_worker_pool_create(
	database.c_str(), &options);
    expect(pool != NULL, "create persistent worker pool");
    if (!pool)
	return;

    const uint64_t first = rt_brep_worker_pool_submit_conversion(pool,
	"one.c", NULL);
    const uint64_t second = rt_brep_worker_pool_submit_conversion(pool,
	"two.c", NULL);
    const uint64_t third = rt_brep_worker_pool_submit_conversion(pool,
	"three.c", NULL);
    expect(first && second && third, "queue three conversion requests");
    std::map<uint64_t, struct rt_brep_worker_result> results = collect(pool);
    expect(results.size() == 3, "collect all conversion results");
    if (results.size() == 3) {
	expect(successful(results[first]) && successful(results[second]) &&
	    successful(results[third]), "persistent conversions succeed");
	expect(results[first].worker_pid == results[second].worker_pid,
	    "one worker process handles consecutive requests");
	expect(results[first].worker_job_count == 1 &&
	    results[second].worker_job_count == 2,
	    "worker job sequence is reported");
	expect(results[third].worker_job_count == 1,
	    "worker restarts after its configured job limit");
    }
    destroy_results(results);
    rt_brep_worker_pool_destroy(pool);
}


static void
test_parallel_and_direct_boolean(const std::string &database)
{
    struct rt_brep_worker_options options = RT_BREP_WORKER_OPTIONS_INIT;
    options.worker_count = 2;
    options.max_jobs_per_worker = 100;
    options.max_worker_resident_bytes = std::numeric_limits<size_t>::max();
    struct rt_brep_worker_pool *pool = rt_brep_worker_pool_create(
	database.c_str(), &options);
    expect(pool != NULL, "create two-worker pool");
    if (!pool)
	return;

    const uint64_t conversion = rt_brep_worker_pool_submit_conversion(pool,
	"one.c", NULL);
    const uint64_t boolean = rt_brep_worker_pool_submit_boolean(pool,
	"left.brep", "right.brep", BOOLEAN_UNION);
    const uint64_t invalid = rt_brep_worker_pool_submit_boolean(pool,
	"invalid.brep", "right.brep", BOOLEAN_UNION);
    std::map<uint64_t, struct rt_brep_worker_result> results = collect(pool);
    expect(results.size() == 3 && successful(results[conversion]) &&
	successful(results[boolean]),
	"conversion and direct Boolean requests complete");
    if (results.size() == 3) {
	expect(results[conversion].worker_pid != results[boolean].worker_pid,
	    "independent workers execute requests concurrently");
	const struct rt_brep_worker_result &failure = results[invalid];
	expect(failure.transport_status == RT_BREP_WORKER_TRANSPORT_OK &&
	    failure.boolean_status == BREP_BOOLEAN_INVALID_INPUT &&
	    failure.conversion_info.failure.stage ==
		RT_BREP_FAILURE_STAGE_BOOLEAN_EVALUATION &&
	    failure.conversion_info.failure.reason ==
		RT_BREP_FAILURE_REASON_INVALID_OBJECT &&
	    failure.conversion_info.failure.boolean_failure.stage ==
		BREP_BOOLEAN_FAILURE_STAGE_INPUT_VALIDATION &&
	    failure.conversion_info.failure.boolean_failure.reason ==
		BREP_BOOLEAN_FAILURE_REASON_INVALID_INPUT,
	    "worker protocol preserves nested Boolean diagnostics");
    }
    destroy_results(results);
    rt_brep_worker_pool_destroy(pool);
}


static void
test_unsupported_conversion_status(const std::string &database)
{
    struct rt_brep_worker_pool *pool = rt_brep_worker_pool_create(
	database.c_str(), NULL);
    expect(pool != NULL, "create worker pool for unsupported conversion");
    if (!pool)
	return;

    const uint64_t request = rt_brep_worker_pool_submit_conversion(pool,
	"standalone.half", NULL);
    const uint64_t missing = rt_brep_worker_pool_submit_conversion(pool,
	"missing.s", NULL);
    std::map<uint64_t, struct rt_brep_worker_result> results = collect(pool);
    expect(request && missing && results.size() == 2,
	"collect failed conversion results");
    if (request && missing && results.size() == 2) {
	const struct rt_brep_worker_result &result = results[request];
	expect(result.transport_status == RT_BREP_WORKER_TRANSPORT_OK &&
	    result.boolean_status == BREP_BOOLEAN_UNSUPPORTED &&
	    result.conversion_info.initial_status ==
		BREP_BOOLEAN_UNSUPPORTED && result.brep == NULL,
	    "standalone halfspace reports unsupported without a protocol error");
	const struct rt_brep_worker_result &missing_result = results[missing];
	expect(missing_result.transport_status == RT_BREP_WORKER_TRANSPORT_OK &&
	    missing_result.boolean_status == BREP_BOOLEAN_INVALID_INPUT &&
	    missing_result.conversion_info.failure.stage ==
		RT_BREP_FAILURE_STAGE_LEAF_LOOKUP &&
	    missing_result.conversion_info.failure.reason ==
		RT_BREP_FAILURE_REASON_MISSING_OBJECT &&
	    std::string(missing_result.conversion_info.failure.tree_path) == "r" &&
	    std::string(missing_result.conversion_info.failure.object_name) ==
		"missing.s",
	    "worker protocol preserves structured failure diagnostics");
    }
    destroy_results(results);
    rt_brep_worker_pool_destroy(pool);
}


static void
test_resident_retirement(const std::string &database)
{
    struct rt_brep_worker_options options = RT_BREP_WORKER_OPTIONS_INIT;
    options.max_jobs_per_worker = 100;
    options.max_worker_resident_bytes = 1;
    struct rt_brep_worker_pool *pool = rt_brep_worker_pool_create(
	database.c_str(), &options);
    expect(pool != NULL, "create resident-limit worker pool");
    if (!pool)
	return;

    const uint64_t first = rt_brep_worker_pool_submit_conversion(pool,
	"one.c", NULL);
    const uint64_t second = rt_brep_worker_pool_submit_conversion(pool,
	"two.c", NULL);
    std::map<uint64_t, struct rt_brep_worker_result> results = collect(pool);
    expect(results.size() == 2 && successful(results[first]) &&
	successful(results[second]),
	"resident-limit conversions succeed");
    if (results.size() == 2 && results[first].worker_resident_bytes > 0) {
	expect(results[second].worker_job_count == 1,
	    "worker restarts after its resident-memory limit");
    } else if (results.size() == 2) {
	bu_log("SKIP: resident-memory accounting is unavailable\n");
    }
    destroy_results(results);
    rt_brep_worker_pool_destroy(pool);
}


static void
test_crash_recovery(const std::string &database, const std::string &claim)
{
    (void)bu_file_delete(claim.c_str());
    (void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT", "crash:two.c", 1);
    (void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT_FILE", claim.c_str(), 1);

    struct rt_brep_worker_options options = RT_BREP_WORKER_OPTIONS_INIT;
    options.request_timeout_ms = 5000;
    options.max_jobs_per_worker = 100;
    options.max_worker_resident_bytes = std::numeric_limits<size_t>::max();
    struct rt_brep_worker_pool *pool = rt_brep_worker_pool_create(
	database.c_str(), &options);
    expect(pool != NULL, "create crash-recovery worker pool");
	if (!pool) {
	(void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT", "", 1);
	(void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT_FILE", "", 1);
	return;
    }

    const uint64_t before = rt_brep_worker_pool_submit_conversion(pool,
	"one.c", NULL);
    const uint64_t crash = rt_brep_worker_pool_submit_conversion(pool,
	"two.c", NULL);
    const uint64_t after = rt_brep_worker_pool_submit_conversion(pool,
	"three.c", NULL);
    std::map<uint64_t, struct rt_brep_worker_result> results = collect(pool);
    expect(bu_file_exists(claim.c_str(), NULL), "fault injection was claimed");
    expect(results.size() == 3, "crashed request does not lose queued work");
    if (results.size() == 3) {
	expect(successful(results[before]), "request before worker crash succeeds");
	expect(results[crash].transport_status == RT_BREP_WORKER_CRASHED &&
	    results[crash].boolean_status == BREP_BOOLEAN_INTERNAL_ERROR &&
	    !results[crash].brep,
	    "crashed request reports an isolated transport failure");
	expect(successful(results[after]), "replacement worker handles next request");
	expect(results[after].worker_job_count == 1,
	    "replacement worker begins a new job sequence");
    }
    destroy_results(results);
    rt_brep_worker_pool_destroy(pool);
    (void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT", "", 1);
    (void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT_FILE", "", 1);
    (void)bu_file_delete(claim.c_str());
}


static void
test_protocol_recovery(const std::string &database, const std::string &claim,
    const char *fault_kind)
{
    (void)bu_file_delete(claim.c_str());
    const std::string selector = std::string(fault_kind) + ":two.c";
    (void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT", selector.c_str(), 1);
    (void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT_FILE", claim.c_str(), 1);

    struct rt_brep_worker_options options = RT_BREP_WORKER_OPTIONS_INIT;
    options.request_timeout_ms = 5000;
    options.max_jobs_per_worker = 100;
    options.max_worker_resident_bytes = std::numeric_limits<size_t>::max();
    struct rt_brep_worker_pool *pool = rt_brep_worker_pool_create(
	database.c_str(), &options);
    expect(pool != NULL, "create protocol-recovery worker pool");
    if (!pool) {
	(void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT", "", 1);
	(void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT_FILE", "", 1);
	return;
    }

    const uint64_t malformed = rt_brep_worker_pool_submit_conversion(pool,
	"two.c", NULL);
    const uint64_t after = rt_brep_worker_pool_submit_conversion(pool,
	"three.c", NULL);
    std::map<uint64_t, struct rt_brep_worker_result> results = collect(pool);
    expect(bu_file_exists(claim.c_str(), NULL),
	"protocol fault injection was claimed");
    expect(results.size() == 2,
	"malformed response does not lose queued work");
    if (results.size() == 2) {
	expect(results[malformed].transport_status ==
	    RT_BREP_WORKER_PROTOCOL_ERROR &&
	    results[malformed].boolean_status == BREP_BOOLEAN_INTERNAL_ERROR &&
	    !results[malformed].brep,
	    "malformed response reports an isolated protocol failure");
	expect(successful(results[after]),
	    "replacement worker handles work after a protocol failure");
	expect(results[after].worker_job_count == 1,
	    "protocol replacement starts a new job sequence");
    }
    destroy_results(results);
    rt_brep_worker_pool_destroy(pool);
    (void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT", "", 1);
    (void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT_FILE", "", 1);
    (void)bu_file_delete(claim.c_str());
}


static void
test_timeout_recovery(const std::string &database, const std::string &claim)
{
    (void)bu_file_delete(claim.c_str());
    (void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT", "hang:two.c", 1);
    (void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT_FILE", claim.c_str(), 1);

    struct rt_brep_worker_options options = RT_BREP_WORKER_OPTIONS_INIT;
    options.request_timeout_ms = 2000;
    options.max_jobs_per_worker = 100;
    options.max_worker_resident_bytes = std::numeric_limits<size_t>::max();
    struct rt_brep_worker_pool *pool = rt_brep_worker_pool_create(
	database.c_str(), &options);
    expect(pool != NULL, "create timeout-recovery worker pool");
    if (!pool) {
	(void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT", "", 1);
	(void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT_FILE", "", 1);
	return;
    }

    const uint64_t timeout = rt_brep_worker_pool_submit_conversion(pool,
	"two.c", NULL);
    const uint64_t after = rt_brep_worker_pool_submit_conversion(pool,
	"three.c", NULL);
    std::map<uint64_t, struct rt_brep_worker_result> results = collect(pool);
    expect(bu_file_exists(claim.c_str(), NULL),
	"timeout fault injection was claimed");
    expect(results.size() == 2, "timed-out request does not lose queued work");
    if (results.size() == 2) {
	expect(results[timeout].transport_status ==
	    RT_BREP_WORKER_TIMED_OUT &&
	    results[timeout].boolean_status == BREP_BOOLEAN_INTERNAL_ERROR &&
	    !results[timeout].brep,
	    "timed-out request reports an isolated transport failure");
	expect(successful(results[after]),
	    "replacement worker handles work after a timeout");
	expect(results[after].worker_job_count == 1,
	    "timeout replacement starts a new job sequence");
    }
    destroy_results(results);
    rt_brep_worker_pool_destroy(pool);
    (void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT", "", 1);
    (void)bu_setenv("LIBRT_BREP_BOOLEAN_TEST_FAULT_FILE", "", 1);
    (void)bu_file_delete(claim.c_str());
}

} // namespace


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1) {
	bu_log("Usage: %s\n", argv[0]);
	return 1;
    }

    const char *temporary = bu_temp_file_name("brep_worker", 0);
    const std::string database = std::string(temporary) + ".g";
    const std::string claim = std::string(temporary) + ".fault";
    if (!create_database(database)) {
	bu_log("Unable to create B-Rep worker test database\n");
	return 1;
    }

    test_reuse_and_retirement(database);
    test_parallel_and_direct_boolean(database);
    test_unsupported_conversion_status(database);
    test_resident_retirement(database);
    test_crash_recovery(database, claim);
    test_protocol_recovery(database, claim, "protocol");
    test_protocol_recovery(database, claim, "protocol-detail");
    test_timeout_recovery(database, claim);

    (void)bu_file_delete(claim.c_str());
    (void)bu_file_delete(database.c_str());
    return failures ? 1 : 0;
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

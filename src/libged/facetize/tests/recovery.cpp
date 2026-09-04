/*              F A C E T I Z E _ R E C O V E R Y . C P P
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
/** @file facetize/tests/recovery.cpp
 *
 * End-to-end fault injection tests for facetize worker write recovery.
 */

#include "common.h"

#include <filesystem>
#include <string>
#include <vector>

#include <string.h>

#include "bu/app.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "wdb.h"
#include "ged.h"

static const char FAULT_ENV[] = "LIBGED_FACETIZE_TEST_FAULT";
static const char FAULT_FILE_ENV[] = "LIBGED_FACETIZE_TEST_FAULT_FILE";
static const char TEST_OBJECT[] = "one.s";
static const char TEST_COMBINATION[] = "model.c";
static const char TEST_OUTPUT[] = "model.bot";

struct recovery_case {
    const char *name;
    const char *stage;
    bool expect_success;
    bool expect_replay;
    bool expect_resubmit;
};

static int
capture_log(void *data, void *message)
{
    if (data && message)
	static_cast<std::string *>(data)->append(static_cast<const char *>(message));
    return 0;
}

static int
create_input(const char *gfile)
{
    (void)bu_file_delete(gfile);
    struct db_i *dbip = db_create(gfile, BRLCAD_DB_FORMAT_LATEST);
    if (!dbip)
	return BRLCAD_ERROR;
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (!wdbp) {
	db_close(dbip);
	return BRLCAD_ERROR;
    }

    point_t centers[3] = {
	{0.0, 0.0, 0.0},
	{30.0, 0.0, 0.0},
	{60.0, 0.0, 0.0}
    };
    const char *names[3] = {TEST_OBJECT, "two.s", "three.s"};
    int ret = BRLCAD_OK;
    for (size_t i = 0; i < 3; i++) {
	if (mk_sph(wdbp, names[i], centers[i], 10.0) < 0) {
	    ret = BRLCAD_ERROR;
	    break;
	}
    }

    if (ret == BRLCAD_OK) {
	struct bu_list members;
	BU_LIST_INIT(&members);
	for (const char *name : names)
	    (void)mk_addmember(name, &members, NULL, WMOP_UNION);
	if (mk_comb(wdbp, TEST_COMBINATION, &members, 0, NULL, NULL, NULL,
		0, 0, 0, 0, 0, 0, 0) < 0)
	    ret = BRLCAD_ERROR;
    }

    wdb_close(wdbp);
    return ret;
}

static int
run_facetize(const char *gfile, int jobs, std::string &output)
{
    struct ged *gedp = ged_open("db", gfile, 1);
    if (!gedp)
	return BRLCAD_ERROR;

    std::string log_output;
    struct bu_hook_list saved_hooks = BU_HOOK_LIST_INIT_ZERO;
    bu_log_hook_save_all(&saved_hooks);
    bu_log_hook_delete_all();
    bu_log_add_hook(capture_log, &log_output);

    std::string job_count = std::to_string(jobs);
    const char *argv[] = {
	"facetize", "-v", "--jobs", job_count.c_str(), "--methods", "NMG",
	TEST_COMBINATION, TEST_OUTPUT, NULL
    };
    int ret = ged_exec(gedp, 8, argv);
    output = bu_vls_cstr(gedp->ged_result_str);
    ged_close(gedp);

    bu_log_hook_delete_all();
    bu_log_hook_restore_all(&saved_hooks);
    bu_hook_delete_all(&saved_hooks);
    output.append(log_output);
    return ret;
}

static bool
object_exists(const char *gfile, const char *object_name)
{
    struct db_i *dbip = db_open(gfile, DB_OPEN_READONLY);
    if (!dbip)
	return false;
    bool exists = db_dirbuild(dbip) >= 0 &&
	db_lookup(dbip, object_name, LOOKUP_QUIET) != RT_DIR_NULL;
    db_close(dbip);
    return exists;
}

static bool
object_external(const char *gfile, const char *object_name,
	std::vector<unsigned char> &bytes)
{
    bytes.clear();
    struct db_i *dbip = db_open(gfile, DB_OPEN_READONLY);
    if (!dbip)
	return false;
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	return false;
    }

    struct directory *dp = db_lookup(dbip, object_name, LOOKUP_QUIET);
    struct bu_external external;
    if (!dp || db_get_external(&external, dp, dbip) < 0) {
	db_close(dbip);
	return false;
    }
    const unsigned char *data =
	static_cast<const unsigned char *>(external.ext_buf);
    bytes.assign(data, data + external.ext_nbytes);
    bu_free_external(&external);
    db_close(dbip);
    return true;
}

static bool
outputs_match(const char *control_file, const char *test_file)
{
    std::vector<unsigned char> control;
    std::vector<unsigned char> test;
    return object_external(control_file, TEST_OUTPUT, control) &&
	object_external(test_file, TEST_OUTPUT, test) && control == test;
}

static bool
has_temporary_write_file(const std::filesystem::path &cache_dir)
{
    std::error_code error;
    std::filesystem::recursive_directory_iterator entry(cache_dir, error);
    std::filesystem::recursive_directory_iterator end;
    while (!error && entry != end) {
	if (entry->is_regular_file(error)) {
	    std::string name = entry->path().filename().string();
	    if (name.find(".facetize_worker_") != std::string::npos ||
		    entry->path().extension() == ".bak" ||
		    name.find(".io_probe") != std::string::npos)
		return true;
	}
	entry.increment(error);
    }
    return false;
}

static bool
reset_cache(const std::filesystem::path &cache_dir)
{
    std::error_code error;
    std::filesystem::remove_all(cache_dir, error);
    if (error)
	return false;
    return std::filesystem::create_directories(cache_dir, error) && !error;
}

static int
run_recovery_case(const recovery_case &test_case,
	const std::filesystem::path &test_dir,
	const std::filesystem::path &cache_dir, const char *control_file)
{
    std::filesystem::path gfile = test_dir /
	(std::string(test_case.name) + ".g");
    std::filesystem::path claim_file = test_dir /
	(std::string(test_case.name) + ".fault");
    (void)bu_file_delete(claim_file.string().c_str());
    if (create_input(gfile.string().c_str()) != BRLCAD_OK) {
	bu_log("[facetize_recovery] %s: unable to create input\n",
		test_case.name);
	return BRLCAD_ERROR;
    }

    std::string selector = std::string(test_case.stage) + ":" + TEST_OBJECT;
    (void)bu_setenv(FAULT_ENV, selector.c_str(), 1);
    (void)bu_setenv(FAULT_FILE_ENV, claim_file.string().c_str(), 1);
    std::string output;
    int ret = run_facetize(gfile.string().c_str(), 2, output);
    (void)bu_setenv(FAULT_ENV, "", 1);
    (void)bu_setenv(FAULT_FILE_ENV, "", 1);

    bool passed = true;
    if (!bu_file_exists(claim_file.string().c_str(), NULL)) {
	bu_log("[facetize_recovery] %s: fault did not trigger\n",
		test_case.name);
	passed = false;
    }
    if ((ret == BRLCAD_OK) != test_case.expect_success) {
	bu_log("[facetize_recovery] %s: unexpected command result %d\n%s\n",
		test_case.name, ret, output.c_str());
	passed = false;
    }
    if (test_case.expect_success) {
	if (!outputs_match(control_file, gfile.string().c_str())) {
	    bu_log("[facetize_recovery] %s: result differs from serial control\n",
		    test_case.name);
	    passed = false;
	}
    } else if (object_exists(gfile.string().c_str(), TEST_OUTPUT)) {
	bu_log("[facetize_recovery] %s: failed command left an output object\n",
		test_case.name);
	passed = false;
    }
    bool replay_reported = output.find("retrying tessellation") !=
	std::string::npos;
    if (replay_reported != test_case.expect_replay) {
	bu_log("[facetize_recovery] %s: unexpected replay reporting\n%s\n",
		test_case.name, output.c_str());
	passed = false;
    }
    bool resubmit_reported = output.find("resubmitted staged tessellation") !=
	std::string::npos;
    if (resubmit_reported != test_case.expect_resubmit) {
	bu_log("[facetize_recovery] %s: unexpected resubmission reporting\n%s\n",
		test_case.name, output.c_str());
	passed = false;
    }
    if (has_temporary_write_file(cache_dir)) {
	bu_log("[facetize_recovery] %s: temporary write file was not cleaned\n",
		test_case.name);
	passed = false;
    }

    bu_log("[facetize_recovery] %s: %s\n", test_case.name,
	    passed ? "PASS" : "FAIL");
    return passed ? BRLCAD_OK : BRLCAD_ERROR;
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
    std::filesystem::path cache_dir = test_dir / "cache";
    std::error_code error;
    std::filesystem::remove_all(test_dir, error);
    if (error) {
	bu_log("[facetize_recovery] unable to clear test directory\n");
	return 1;
    }
    if (!std::filesystem::create_directories(cache_dir, error) || error) {
	bu_log("[facetize_recovery] unable to create test directory\n");
	return 1;
    }
    std::filesystem::path control_file = test_dir / "control.g";
    std::string control_output;
    if (create_input(control_file.string().c_str()) != BRLCAD_OK ||
	    run_facetize(control_file.string().c_str(), 1, control_output) !=
	    BRLCAD_OK) {
	bu_log("[facetize_recovery] unable to create serial control\n%s\n",
		control_output.c_str());
	return 1;
    }

    const recovery_case cases[] = {
	{"tess_before_ready", "tess-before-ready", false, false, false},
	{"stage_before_done", "stage-before-done", false, false, false},
	{"writer_before_proceed", "writer-before-proceed", true, false, true},
	{"writer_before_done", "writer-before-done", true, true, false}
    };

    int ret = 0;
    for (const recovery_case &test_case : cases) {
	if (!reset_cache(cache_dir)) {
	    bu_log("[facetize_recovery] %s: unable to reset cache directory\n",
		    test_case.name);
	    ret = 1;
	    continue;
	}
	if (run_recovery_case(test_case, test_dir, cache_dir,
		control_file.string().c_str()) != BRLCAD_OK)
	    ret = 1;
    }

    if (!ret)
	std::filesystem::remove_all(test_dir, error);
    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

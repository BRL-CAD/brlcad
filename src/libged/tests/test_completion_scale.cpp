/*              T E S T _ C O M P L E T I O N _ S C A L E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */

#include "common.h"

#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

#include "bu.h"
#include "ged.h"
#include "raytrace.h"

#include "../ged_private.h"
#include "../completion_index.h"


#define CHECK(_condition, _message) do { \
    if (!(_condition)) { \
	std::fprintf(stderr, "ERROR: %s\n", _message); \
	status = 1; \
	goto done; \
    } \
} while (0)


int
main(int argc, char **argv)
{
    const size_t object_count = 100000;
    const size_t indexed_count = object_count + 6;
    const std::string prefix = "zz_completion_scale_";
    const std::string numeric_prefix = prefix + "numeric_";
    const std::string huge_nines = numeric_prefix + "999999999999999999999";
    const std::string huge_power = numeric_prefix + "1000000000000000000000";
    struct ged *gedp = GED_NULL;
    struct ged_cmd_completion_request request = GED_CMD_COMPLETION_REQUEST_NULL;
    struct ged_cmd_completion_result result = GED_CMD_COMPLETION_RESULT_NULL;
    struct directory *late = RT_DIR_NULL;
    std::string draw;
    std::string late_name;
    int64_t started = 0;
    int object_type = ID_SPH;
    int status = 0;

    bu_setprogname(argv[0]);
    if (argc != 2) {
	std::fprintf(stderr, "Usage: %s test.g\n", argv[0]);
	return 1;
    }
    if (bu_file_exists(argv[1], NULL))
	(void)bu_file_delete(argv[1]);
    gedp = ged_open("db", argv[1], 0);
    CHECK(gedp != GED_NULL && gedp->dbip != DBI_NULL,
	"unable to create scaling database");

    for (size_t i = 0; i < object_count; i++) {
	std::string name = prefix + std::to_string(i);
	CHECK(db_diradd(gedp->dbip, name.c_str(), RT_DIR_PHONY_ADDR, 0,
		RT_DIR_SOLID, (void *)&object_type) != RT_DIR_NULL,
	    "unable to populate scaling database");
    }
    CHECK(db_diradd(gedp->dbip, (prefix + "0001").c_str(),
	    RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&object_type) != RT_DIR_NULL,
	"unable to add a naturally equivalent spelling");
    CHECK(db_diradd(gedp->dbip,
	    (prefix + std::to_string(std::numeric_limits<unsigned long>::max())).c_str(),
	    RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&object_type) != RT_DIR_NULL,
	"unable to add a maximum-width natural-sort spelling");
    CHECK(db_diradd(gedp->dbip, (numeric_prefix + "01").c_str(),
	    RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&object_type) != RT_DIR_NULL &&
	db_diradd(gedp->dbip, (numeric_prefix + "1").c_str(),
	    RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&object_type) != RT_DIR_NULL &&
	db_diradd(gedp->dbip, huge_nines.c_str(), RT_DIR_PHONY_ADDR, 0,
	    RT_DIR_SOLID, (void *)&object_type) != RT_DIR_NULL &&
	db_diradd(gedp->dbip, huge_power.c_str(), RT_DIR_PHONY_ADDR, 0,
	    RT_DIR_SOLID, (void *)&object_type) != RT_DIR_NULL,
	"unable to add arbitrary-width natural-sort spellings");

    request.cursor_pos = std::strlen("ls /");
    request.max_candidates = 32;
    started = bu_gettime();
    CHECK(ged_cmd_complete_query(gedp, "ls /", &request, &result) == 0 &&
	    result.completion_count == 32 && result.total_count == indexed_count &&
	    result.truncated && result.replacement_start == std::strlen("ls /"),
	"cold empty root query should be bounded and exact");
    CHECK(bu_gettime() - started < 5000000,
	"cold 100k-object root query exceeded the interactive budget");
    ged_cmd_completion_result_clear(&result);

    draw = "draw " + prefix;
    request.cursor_pos = draw.size();
    started = bu_gettime();
    CHECK(ged_cmd_complete_query(gedp, draw.c_str(), &request, &result) == 0 &&
	    result.completion_count == 32 && result.total_count == indexed_count &&
	    result.truncated,
	"typed 100k-object query should remain bounded and exact");
    CHECK(BU_STR_EQUAL(result.completion_candidates[0],
	    (prefix + "0").c_str()) &&
	BU_STR_EQUAL(result.completion_candidates[1], (prefix + "0001").c_str()) &&
	BU_STR_EQUAL(result.completion_candidates[2], (prefix + "1").c_str()) &&
	BU_STR_EQUAL(result.completion_candidates[31], (prefix + "30").c_str()),
	"bounded broad-prefix candidates should retain natural ordering");
    CHECK(bu_gettime() - started < 250000,
	"warm broad-prefix query should not scan its full matching range");
    ged_cmd_completion_result_clear(&result);

    draw = "draw " + numeric_prefix;
    request.cursor_pos = draw.size();
    request.max_candidates = 4;
    CHECK(ged_cmd_complete_query(gedp, draw.c_str(), &request, &result) == 0 &&
	    result.completion_count == 4 && result.total_count == 4 &&
	    BU_STR_EQUAL(result.completion_candidates[0],
		(numeric_prefix + "01").c_str()) &&
	    BU_STR_EQUAL(result.completion_candidates[1],
		(numeric_prefix + "1").c_str()) &&
	    BU_STR_EQUAL(result.completion_candidates[2], huge_nines.c_str()) &&
	    BU_STR_EQUAL(result.completion_candidates[3], huge_power.c_str()),
	"natural ordering should compare decimal runs beyond machine integer width");
    ged_cmd_completion_result_clear(&result);

    CHECK(ged_cmd_complete_result(gedp, draw.c_str(), draw.size(), &result) ==
	    4 && result.completion_count == 4 && result.total_count == 4 &&
	    !result.truncated,
	"an explicitly unbounded narrow query should return its complete set");
    ged_cmd_completion_result_clear(&result);

    draw = "draw " + prefix;
    CHECK(ged_cmd_complete_result(gedp, draw.c_str(), draw.size(), &result) ==
	    (int)indexed_count && result.completion_count == indexed_count &&
	    result.total_count == indexed_count && !result.truncated &&
	    BU_STR_EQUAL(result.completion_candidates[1],
		(prefix + "0001").c_str()) &&
	    BU_STR_EQUAL(result.completion_candidates[2], (prefix + "1").c_str()),
	"an explicitly unbounded query should not impose an interactive candidate ceiling");
    ged_cmd_completion_result_clear(&result);

    {
	const unsigned int unknown_flag = 0x80000000u;
	const char **policy_values = NULL;
	struct bu_vls policy_prefix = BU_VLS_INIT_ZERO;
	CHECK(ged_geom_completions_filtered(&policy_values, &policy_prefix,
		gedp->dbip, "__no_policy_match__",
		GED_DB_COMPLETION_GEOMETRY) == 0,
	    "unable to seed the policy-view cache");
	size_t known_count = _ged_cmd_completion_cache_policy_count(gedp);
	CHECK(ged_geom_completions_filtered(&policy_values, &policy_prefix,
		gedp->dbip, "__no_policy_match__",
		GED_DB_COMPLETION_GEOMETRY | unknown_flag) == 0 &&
		_ged_cmd_completion_cache_policy_count(gedp) == known_count,
	    "ignored completion-policy bits should share a cache entry");
	for (unsigned int flags = 0; flags < 64; flags++)
	    CHECK(ged_geom_completions_filtered(&policy_values, &policy_prefix,
		    gedp->dbip, "__no_policy_match__", flags) == 0,
		"unable to exercise policy-view eviction");
	CHECK(_ged_cmd_completion_cache_policy_count(gedp) <=
		GED_DB_COMPLETION_POLICY_CACHE_MAX,
	    "database completion policy views should have a fixed cache bound");
	bu_vls_free(&policy_prefix);
    }

    late_name = prefix + "late_mutation";
    late = db_diradd(gedp->dbip, late_name.c_str(),
	RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&object_type);
    CHECK(late != RT_DIR_NULL, "unable to add mutation probe");
    draw = "draw " + late_name;
    request.cursor_pos = draw.size();
    request.max_candidates = 4;
    CHECK(ged_cmd_complete_query(gedp, draw.c_str(), &request, &result) == 0 &&
	    result.completion_count == 1 && result.total_count == 1,
	"a rebuilt index should expose an added object");
    ged_cmd_completion_result_clear(&result);
    CHECK(db_dirdelete(gedp->dbip, late) == 0,
	"unable to delete mutation probe");
    CHECK(ged_cmd_complete_query(gedp, draw.c_str(), &request, &result) == 0 &&
	    result.completion_count == 0 && result.total_count == 0,
	"a rebuilt index should remove a deleted object");

done:
    ged_cmd_completion_result_clear(&result);
    if (gedp)
	ged_close(gedp);
    if (bu_file_exists(argv[1], NULL))
	(void)bu_file_delete(argv[1]);
    return status;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

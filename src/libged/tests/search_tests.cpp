/*                  S E A R C H _ T E S T S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2024 United States Government as represented by
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
/** @file search_test.cpp
 *
 * Brief description
 *
 */

#include "common.h"

#include <iostream>
#include <vector>
#include <string>
#include <functional>   // UnitTests function creation

#include "bu.h"
#include "ged.h"
#include "rt/search.h"

class UnitTests {
public:
    using TestFunc = std::function<bool()>;

    template<typename Func, typename... Args>
    void addTest(const std::string& name, Func func, Args... args) {
        tests.emplace_back(name, [func, args...]() { return func(args...); });
    }

    bool runTest(size_t index) const {
        if (index < tests.size()) {
            return tests[index].second();
        }

        std::cerr << "[ERROR] invalid index" << index << std::endl;
        return false;
    }

    bool runAllTests() const {
        bool all_pass = true;
        for (size_t i = 0; i < tests.size(); i++) {
            bool pass = tests[i].second();
            all_pass &= pass;

            if (!pass)
                std::cout << tests[i].first << " " << (pass ? "[PASS]" : "[FAIL]") << std::endl;
        }

        return all_pass;
    }

private:
    std::vector<std::pair<std::string, TestFunc>> tests;
};


/* pass through to db_search, excluding bu_ptbl param
 * de-duplicates code which only defines and checks ptbl length */
template <typename... Args>
bool search_count_helper(const int expected_count, 
                         int flags,
			 const char *filter,
			 int path_c,
			 struct directory **path_v,
			 struct db_i *dbip,
			 bu_clbk_t clbk,
			 void *u1,
			 void *u2)
{
    // ensure good return, and count inside ptbl match expectations
    struct bu_ptbl search_results = BU_PTBL_INIT_ZERO;
    int ret = db_search(&search_results, flags, filter, path_c, path_v, dbip, clbk, u1, u2);

    bool count_matches = (ret >= 0 && BU_PTBL_LEN(&search_results) == (unsigned long)expected_count);
    db_search_free(&search_results);

    return count_matches;
}

bool GenericSearches(struct ged* gedp) {
    const int EXPECTED_TREE = 10;
    const int EXPECTED_FLAT = 2;
    const int EXPECTED_HIDD = 11;
    const int EXPECTED_UNIQ = 8;
    
    bool tree = search_count_helper(EXPECTED_TREE, DB_SEARCH_TREE, "", 0, NULL, gedp->dbip, NULL, NULL, NULL);
    bool flat = search_count_helper(EXPECTED_FLAT, DB_SEARCH_FLAT, "", 0, NULL, gedp->dbip, NULL, NULL, NULL);
    bool hidd = search_count_helper(EXPECTED_HIDD, DB_SEARCH_HIDDEN, "", 0, NULL, gedp->dbip, NULL, NULL, NULL);
    bool uniq = search_count_helper(EXPECTED_UNIQ, DB_SEARCH_RETURN_UNIQ_DP, "", 0, NULL, gedp->dbip, NULL, NULL, NULL);

    return (tree && flat && hidd && uniq);
}

bool PathSearches(struct ged* gedp) {
    const int EXPECTED_PATH = 5;
    const char* COMB_NAME = "ball_inside";

    struct directory* path_dp = db_lookup(gedp->dbip, COMB_NAME, LOOKUP_QUIET);
    bool path = search_count_helper(EXPECTED_PATH, DB_SEARCH_TREE, "", 1, &path_dp, gedp->dbip, NULL, NULL, NULL);

    return path;
}

bool TypeSearches(struct ged* gedp) {
    const int EXPECTED_SHAPE = 4;
    const int EXPECTED_REGION = 4;
    const int EXPECTED_COMB = 6;

    bool shape = search_count_helper(EXPECTED_SHAPE, DB_SEARCH_TREE, "-type shape", 0, NULL, gedp->dbip, NULL, NULL, NULL);
    bool region = search_count_helper(EXPECTED_REGION, DB_SEARCH_TREE, "-type region", 0, NULL, gedp->dbip, NULL, NULL, NULL);
    bool comb = search_count_helper(EXPECTED_COMB, DB_SEARCH_TREE, "-type comb", 0, NULL, gedp->dbip, NULL, NULL, NULL);

    return (shape && region && comb);
}

bool AttrSearches(struct ged* gedp) {
    const int EXPECTED_ID = 2;

    bool id = search_count_helper(EXPECTED_ID, DB_SEARCH_TREE, "-attr region_id=1001", 0, NULL, gedp->dbip, NULL, NULL, NULL);

    return id;
}

bool OrSearches(struct ged* gedp) {
    const int EXPECTED_REG_ARB = 6;
    
    const char* sFilter = "-type region -or -type arb8";

    bool reg_arb = search_count_helper(EXPECTED_REG_ARB, DB_SEARCH_TREE, sFilter, 0, NULL, gedp->dbip, NULL, NULL, NULL);
    
    return reg_arb;
}

bool AndSearches(struct ged* gedp) {
    const int EXPECTED_REG_COMB = 4;

    const char* sFilter = "-type region -and -type comb";

    bool reg_comb = search_count_helper(EXPECTED_REG_COMB, DB_SEARCH_TREE, sFilter, 0, NULL, gedp->dbip, NULL, NULL, NULL);

    return reg_comb;
}

void CheckUsage(int ac, char* av[]) {
    if (ac != 2) {
        bu_exit(BRLCAD_ERROR, "Usage: %s file.g", av[0]);
    }

    if (!bu_file_exists(av[1], NULL)) {
        bu_exit(BRLCAD_ERROR, "ERROR: [%s] does not exist, expecting .g file", av[1]);
    }
}

int main(int ac, char *av[]) {
    bu_setprogname(av[0]);

    CheckUsage(ac, av);

    // good usage, open db
    struct ged* gedp = ged_open("db", av[1], 1);

    // add in all tests to suite
    UnitTests uTests;
    uTests.addTest("Generic Searches", GenericSearches, gedp);
    uTests.addTest("Path Searches", PathSearches, gedp);
    uTests.addTest("Type Searches", TypeSearches, gedp);
    uTests.addTest("Attr Searches", AttrSearches, gedp);
    uTests.addTest("Or Searches", OrSearches, gedp);
    uTests.addTest("And Searches", AndSearches, gedp);

    // run all tests
    bool passed = uTests.runAllTests();

    // cleanup
    ged_close(gedp);

    // true = all pass = good return
    return (passed ? 0 : 1);
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

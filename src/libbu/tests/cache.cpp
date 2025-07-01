/*                       C A C H E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2025 United States Government as represented by
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

#include "common.h"

#include <map>
#include <string>

#include <limits.h>
#include <string.h>
#include "bu.h"

void
print_keys(struct bu_cache *c, int kcnt_expected)
{
    // Test key retrieval - this is useful if we don't know what's in a cache
    // and need to validate/clear out stale entries.
    int64_t start, elapsed;
    fastf_t seconds;

    start = bu_gettime();
    char **keys = NULL;
    int kcnt = bu_cache_keys(&keys, c);
    if (kcnt != kcnt_expected)
	bu_exit(1, "Failed to extract all keys: expected %d, got %d\n", kcnt_expected, kcnt);
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Retrieved %d keys in %g seconds.\n", kcnt, seconds);
    //for (int i = 0; i < kcnt; i++)
	//bu_log("%s\n", keys[i]);


    // Make sure the keys can read their values
    start = bu_gettime();
    for (int i = 0; i < kcnt; i++) {
	void *rdata = NULL;
	size_t rsize = bu_cache_get(&rdata, keys[i], c);
	if (rsize != sizeof(double) && rsize != sizeof(int)) {
	    bu_log("Failed to read %s value\n", keys[i]);
	}
	if (rsize == sizeof(int)) {
	    // Get the number from the key
	    struct bu_vls kstr = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&kstr, "%s", keys[i]);
	    bu_vls_nibble(&kstr, 4);
	    int v = std::atoi(bu_vls_cstr(&kstr));
	    int rval;
	    memcpy(&rval, rdata, sizeof(rval));
	    if (v != rval)
		bu_log("Failed to read correct %s value - got %d\n", keys[i], rval);
	    bu_vls_free(&kstr);
	}
    }
    bu_cache_get_done(c);


    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Read %d values using bu_cache_keys list in %g seconds.\n", kcnt, seconds);

    // Done with keys array
    bu_argv_free(kcnt, keys);
}


static void
basic_test()
{
    int64_t start, elapsed;
    fastf_t seconds;

    int kcnt_expected = 0;
    int item_cnt = 100000;
    const char *cfile = "50M_cache";
    struct bu_vls keystr = BU_VLS_INIT_ZERO;

    char cache_file[MAXPATHLEN] = {0};
    bu_dir(cache_file, MAXPATHLEN, BU_DIR_CACHE, cfile, NULL);

    bu_log("\n*******************************************************************************\n");
    bu_log("********** Basic test - timed basic operations with INT and DBL types *********\n");
    bu_log("*******************************************************************************\n");

    start = bu_gettime();
    struct bu_cache *c = bu_cache_open(cfile, 1, 5e7);
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Cache opening and setup completed in %g seconds.\n", seconds);

    // Test writing INT
    start = bu_gettime();
    for (int i = 0; i < item_cnt; i++) {
	bu_vls_sprintf(&keystr, "int:%d", i);
	int ival = i;
	size_t written = bu_cache_write((char *)&ival, sizeof(int), bu_vls_cstr(&keystr), c);
	if (written != sizeof(int)) {
	    bu_vls_free(&keystr);
	    bu_exit(1, "Failed to write int %d\n", i);
	}
    }
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("INT write test completed - wrote %d INTs in %g seconds.\n", item_cnt, seconds);

    // Test reading INT
    start = bu_gettime();
    for (int i = 0; i < item_cnt; i++) {
	bu_vls_sprintf(&keystr, "int:%d", i);
	void *rdata = NULL;
	size_t rsize = bu_cache_get(&rdata, bu_vls_cstr(&keystr), c);
	if (rsize != sizeof(int)) {
	    bu_vls_free(&keystr);
	    bu_exit(1, "Failed to read int:%d - expected to read %zd bytes but read %zd\n", i, sizeof(int), rsize);
	}
	int rval = 0;
	memcpy(&rval, rdata, sizeof(rval));
	if (rval != i) {
	    bu_vls_free(&keystr);
	    bu_exit(1, "Failed read from int:%d key was %d, expected %d\n", i, rval, i);
	}
    }
    bu_cache_get_done(c);
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("INT read test completed - read %d INTs in %g seconds.\n", item_cnt, seconds);

    kcnt_expected = item_cnt;
    print_keys(c, kcnt_expected);

   // Test writing DBL
    double dblseed = M_PI;
    start = bu_gettime();
    for (int i = 0; i < item_cnt; i++) {
	bu_vls_sprintf(&keystr, "dbl:%d", i);
	double dval = i*dblseed;
	size_t written = bu_cache_write((char *)&dval, sizeof(double), bu_vls_cstr(&keystr), c);
	if (written != sizeof(double)) {
	    bu_vls_free(&keystr);
	    bu_exit(1, "Failed to write double %d:%g\n", i, dval);
	}
    }
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("DBL write test completed - wrote %d DBLs in %g seconds.\n", item_cnt, seconds);

    // Test reading DBL
    start = bu_gettime();
    for (int i = 0; i < item_cnt; i++) {
	bu_vls_sprintf(&keystr, "dbl:%d", i);
	void *rdata = NULL;
	size_t rsize = bu_cache_get(&rdata, bu_vls_cstr(&keystr), c);
	if (rsize != sizeof(double)) {
	    bu_vls_free(&keystr);
	    bu_exit(1, "Failed to read dbl:%d - expected to read %zd bytes but read %zd\n", i, sizeof(double), rsize);
	}
	double rval = 0;
	memcpy(&rval, rdata, sizeof(double));
	if (!NEAR_EQUAL(rval, dblseed*i, SMALL_FASTF)) {
	    bu_vls_free(&keystr);
	    bu_exit(1, "Failed read from dbl:%d expected %g, got %g\n", i, dblseed*i, rval);
	}
    }
    bu_cache_get_done(c);
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("DBL read test completed - read %d DBLs in %g seconds.\n", item_cnt, seconds);

    kcnt_expected = 2*item_cnt;
    print_keys(c, kcnt_expected);

    // Test removal of data
    int rr_s = 2*item_cnt/10;
    int rr_e = 5*item_cnt/10;
    start = bu_gettime();
    for (int i = rr_s; i < rr_e; i++) {
	bu_vls_sprintf(&keystr, "dbl:%d", i);
	bu_cache_clear(bu_vls_cstr(&keystr), c);
    }
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Removal of dbl: keys in %d-%d range completed in %g seconds.\n", rr_s, rr_e, seconds);


    // Test reading post remove - we should be able to get every value that wasn't
    // removed, and shouldn't be able to get anything that WAS removed
    start = bu_gettime();
    int rcnt = 0;
    for (int i = 0; i < item_cnt; i++) {
	bu_vls_sprintf(&keystr, "dbl:%d", i);
	void *rdata = NULL;
	size_t rsize = bu_cache_get(&rdata, bu_vls_cstr(&keystr), c);
	if (rsize != sizeof(double)) {
	    if (i >= rr_s && i < rr_e)
		continue;
	    bu_vls_free(&keystr);
	    bu_exit(1, "Failed to read non-removed value dbl:%d\n", i);
	}
	rcnt++;
	double rval = 0;
	memcpy(&rval, rdata, sizeof(double));
	if (!NEAR_EQUAL(rval, dblseed*i, SMALL_FASTF)) {
	    bu_vls_free(&keystr);
	    bu_exit(1, "Failed read from dbl:%d expected %g, got %g\n", i, dblseed*i, rval);
	}
    }
    bu_cache_get_done(c);
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("DBL post-remove read test completed - read %d DBLs in %g seconds.\n", rcnt, seconds);

    kcnt_expected = 2*item_cnt - rr_e + rr_s;
    print_keys(c, kcnt_expected);

    // Done processing - close cache before checking file size info to make sure
    // we are synced
    bu_cache_close(c);

    if (!bu_file_exists(cache_file, NULL))
	bu_exit(1, "Cache file %s not found\n", cache_file);

    bu_cache_erase(cfile);

    if (bu_file_exists(cache_file, NULL))
	bu_exit(1, "Cache file %s not erased\n", cache_file);
}

static void
limit_test()
{
    int item_cnt;
    unsigned long pgsize = 4096;
    unsigned long fsize = 5e6/pgsize;
    fsize = fsize * pgsize;
    const char *cfile = "5M_cache";
    struct bu_vls keystr = BU_VLS_INIT_ZERO;
    struct bu_cache *c = bu_cache_open(cfile, 1, fsize);

    bu_log("\n*******************************************************************************\n");
    bu_log("************ Limit test - try to put too much data into a small cache *********\n");
    bu_log("*******************************************************************************\n");

    // Write some integers to help fill things up
    item_cnt = 100000;
    for (int i = 0; i < item_cnt; i++) {
	bu_vls_sprintf(&keystr, "int:%d", i);
	int ival = i;
	size_t written = bu_cache_write((char *)&ival, sizeof(int), bu_vls_cstr(&keystr), c);
	if (written != sizeof(int)) {
	    bu_vls_free(&keystr);
	    bu_exit(1, "FAIL: Unexpected - failed to write int %d\n", i);
	}
    }

    // Write doubles - this is where we expect to hit failure
    double dblseed = M_PI;
    int failed = -1;
    int expected_failed = 4557;
    for (int i = 0; i < item_cnt; i++) {
	bu_vls_sprintf(&keystr, "dbl:%d", i);
	double dval = i*dblseed;
	size_t written = bu_cache_write((char *)&dval, sizeof(double), bu_vls_cstr(&keystr), c);
	if (written != sizeof(double)) {
	    bu_log("Failed to assign %s value %g\n", bu_vls_cstr(&keystr), dval);
	    bu_vls_free(&keystr);
	    failed = i;
	    break;
	}
    }

    if (failed == -1)
	bu_exit(1, "FAIL: Expected failure not observed\n");

    if (failed != expected_failed) {
	bu_log("Warning - expected fail at %d, but observed failure was at %d\n", expected_failed, failed);
    } else {
	bu_log("Expected failure at %d observed\n", expected_failed);
    }

    // Test reading DBL
    for (int i = 0; i < failed; i++) {
	bu_vls_sprintf(&keystr, "dbl:%d", i);
	void *rdata = NULL;
	size_t rsize = bu_cache_get(&rdata, bu_vls_cstr(&keystr), c);
	if (rsize != sizeof(double)) {
	    bu_vls_free(&keystr);
	    bu_exit(1, "Failed to read dbl:%d - expected to read %zd bytes but read %zd\n", i, sizeof(double), rsize);
	}
	double rval = 0;
	memcpy(&rval, rdata, sizeof(double));
	if (!NEAR_EQUAL(rval, dblseed*i, SMALL_FASTF)) {
	    bu_vls_free(&keystr);
	    bu_exit(1, "Failed read from dbl:%d expected %g, got %g\n", i, dblseed*i, rval);
	}
    }
    bu_cache_get_done(c);
    bu_log("All successfully written values read\n");

    // Close the cache to make sure we're synced to disk before the next checks
    bu_cache_close(c);

    // Before we remove anything, see if the file size jibes with the limit
    char cache_file[MAXPATHLEN] = {0};
    bu_dir(cache_file, MAXPATHLEN, BU_DIR_CACHE, cfile, "data.mdb", NULL);
    if (!bu_file_exists(cache_file, NULL))
	bu_exit(1, "Cache data file %s not found\n", cache_file);
    int dfsize = bu_file_size(cache_file);
    unsigned long fdelta = std::abs((long)((long)dfsize - (long)fsize));
    bu_log("Cache data file %s:\nApproximate expected size (bytes): %d\nObserved size(bytes):%lu\nDelta (bytes):%lu\n", cache_file, dfsize, fsize, fdelta);

    bu_dir(cache_file, MAXPATHLEN, BU_DIR_CACHE, cfile, NULL);
    if (!bu_file_exists(cache_file, NULL))
	bu_exit(1, "Cache file %s not found\n", cache_file);

    bu_cache_erase(cfile);

    if (bu_file_exists(cache_file, NULL))
	bu_exit(1, "Cache file %s not erased\n", cache_file);
}

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);

    // Set up to use a local cache
    char cache_dir[MAXPATHLEN] = {0};
    bu_dir(cache_dir, MAXPATHLEN, BU_DIR_CURR, "libbu_test_cache", NULL);
    bu_setenv("BU_DIR_CACHE", cache_dir, 1);
    bu_dirclear(cache_dir);
    bu_mkdir(cache_dir);

    // Basic test writing INT and DBL values
    basic_test();

    // Test behavior when data exceeds max cache database size
    limit_test();

    // Final cleanup
    bu_dirclear(cache_dir);

    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

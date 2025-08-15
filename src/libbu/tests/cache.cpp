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

#include <chrono>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <climits>
#include <cstring>

#include <limits.h>
#include <string.h>
#include "bu.h"

#define PRINT_LIMIT 10

// We have multiple tests where we write out and then read back in a floating
// point number, looking to see if we read what was expected.  Different
// platforms have different reproducibility behaviors when it comes to the
// i*dblseed calculation we use to generate numbers for writing and testing.
// We want the tightest tolerance the floating point math will reliably allow
// us, since we want to check the values were read in correctly, but too tight
// and we're into calculation fuzz.  For current test inputs this is the
// tightest value that works on OSX.
#define FLOAT_CMP_TOL 1e-14

//---------------------- Section: Key Printing Utility ----------------------

void
print_keys(struct bu_cache *c, int kcnt_expected, int item_cnt)
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

    // Make sure the keys can read their values
    start = bu_gettime();
    struct bu_cache_txn *txn = NULL;
    for (int i = 0; i < kcnt; i++) {
	void *rdata = NULL;
	size_t rsize = bu_cache_get(&rdata, keys[i], c, &txn);
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

	    // If we don't have too many items, go ahead and print the key/val
	    if (item_cnt <= PRINT_LIMIT)
		bu_log("%s -> %d\n", keys[i], rval);
	}

	if (rsize == sizeof(double)) {
	    if (item_cnt <= PRINT_LIMIT) {
		double rval;
		memcpy(&rval, rdata, sizeof(rval));
		bu_log("%s -> %g\n", keys[i], rval);
	    }
	}
    }
    bu_cache_get_done(&txn);

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Read %d values using bu_cache_keys list in %g seconds.\n", kcnt, seconds);

    // Done with keys array
    bu_argv_free(kcnt, keys);
}

//------------------------ Section: Basic/Common Tests ----------------------

int
basic_test(int item_cnt)
{
    int ret = 0;
    int64_t start, elapsed;
    fastf_t seconds;

    int kcnt_expected = 0;
    const char *cfile = "std_cache";
    char keystr[BU_CACHE_KEY_MAXLEN];

    char cache_file[MAXPATHLEN] = {0};
    bu_dir(cache_file, MAXPATHLEN, BU_DIR_CACHE, cfile, NULL);

    bu_log("\n*******************************************************************************\n");
    bu_log("********** Basic test - timed basic operations with INT and DBL types *********\n");
    bu_log("*******************************************************************************\n");

    start = bu_gettime();
    struct bu_cache *c = bu_cache_open(cfile, 1, 0);
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Cache opening and setup completed in %g seconds.\n", seconds);

    // Test writing INT
    start = bu_gettime();
    for (int i = 0; i < item_cnt; i++) {
	snprintf(keystr, BU_CACHE_KEY_MAXLEN, "int:%d", i);
	int ival = i;
	size_t written = bu_cache_write((char *)&ival, sizeof(int), keystr, c, NULL);
	if (written != sizeof(int)) {
	    bu_log("Failed to write int %d\n", i);
	    ret = 1;
	}
    }
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("INT write test completed - wrote %d INTs in %g seconds.\n", item_cnt, seconds);

    // Test reading INT
    start = bu_gettime();
    struct bu_cache_txn *txn = NULL;
    for (int i = 0; i < item_cnt; i++) {
	snprintf(keystr, BU_CACHE_KEY_MAXLEN, "int:%d", i);
	void *rdata = NULL;
	size_t rsize = bu_cache_get(&rdata, keystr, c, &txn);
	if (rsize != sizeof(int)) {
	    bu_log("Failed to read int:%d - expected to read %zd bytes but read %zd\n", i, sizeof(int), rsize);
	    ret = 1;
	    continue;
	}
	int rval = 0;
	memcpy(&rval, rdata, sizeof(rval));
	if (rval != i) {
	    bu_log("Failed read from int:%d key was %d, expected %d\n", i, rval, i);
	    ret = 1;
	}
    }
    bu_cache_get_done(&txn);
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("INT read test completed - read %d INTs in %g seconds.\n", item_cnt, seconds);

    kcnt_expected = item_cnt;
    print_keys(c, kcnt_expected, item_cnt);

    // Test writing DBL
    double dblseed = M_PI;
    start = bu_gettime();
    for (int i = 0; i < item_cnt; i++) {
	snprintf(keystr, BU_CACHE_KEY_MAXLEN, "dbl:%d", i);
	double dval = i*dblseed;
	if (item_cnt <= PRINT_LIMIT)
	    bu_log("Assigning %g to key %s\n", dval, keystr);
	size_t written = bu_cache_write((char *)&dval, sizeof(double), keystr, c, NULL);
	if (written != sizeof(double)) {
	    bu_log("Failed to write double %d:%g\n", i, dval);
	    ret = 1;
	}
    }
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("DBL write test completed - wrote %d DBLs in %g seconds.\n", item_cnt, seconds);

    // Test reading DBL
    start = bu_gettime();
    for (int i = 0; i < item_cnt; i++) {
	snprintf(keystr, BU_CACHE_KEY_MAXLEN, "dbl:%d", i);
	void *rdata = NULL;
	size_t rsize = bu_cache_get(&rdata, keystr, c, &txn);
	if (rsize != sizeof(double)) {
	    bu_log("Failed to read dbl:%d - expected to read %zd bytes but read %zd\n", i, sizeof(double), rsize);
	    ret = 1;
	    continue;
	}
	double rval = 0;
	memcpy(&rval, rdata, sizeof(double));
	double ctrl = i*dblseed;
	if (!NEAR_EQUAL(rval, ctrl, FLOAT_CMP_TOL)) {
	    bu_log("Failed read from dbl:%d expected %0.15f, got %0.15f (delta: %0.17f)\n", i, ctrl, rval, ctrl - rval);
	    ret = 1;
	}
    }
    bu_cache_get_done(&txn);
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("DBL read test completed - read %d DBLs in %g seconds.\n", item_cnt, seconds);

    kcnt_expected = 2*item_cnt;
    print_keys(c, kcnt_expected, item_cnt);

    // Test removal of data
    int rr_s = 2*item_cnt/10;
    int rr_e = 5*item_cnt/10;
    start = bu_gettime();
    for (int i = rr_s; i < rr_e; i++) {
	snprintf(keystr, BU_CACHE_KEY_MAXLEN, "dbl:%d", i);
	bu_cache_clear(keystr, c, NULL);
    }
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Removal of dbl: keys in %d-%d range completed in %g seconds.\n", rr_s, rr_e, seconds);

    // Test reading post remove - we should be able to get every value that wasn't
    // removed, and shouldn't be able to get anything that WAS removed
    start = bu_gettime();
    int rcnt = 0;
    for (int i = 0; i < item_cnt; i++) {
	snprintf(keystr, BU_CACHE_KEY_MAXLEN, "dbl:%d", i);
	void *rdata = NULL;
	size_t rsize = bu_cache_get(&rdata, keystr, c, &txn);
	if (rsize != sizeof(double)) {
	    if (i >= rr_s && i < rr_e)
		continue;
	    bu_log("Failed to read non-removed value dbl:%d\n", i);
	    ret = 1;
	    continue;
	}
	rcnt++;
	double rval = 0;
	memcpy(&rval, rdata, sizeof(double));
	double ctrl = i*dblseed;
	if (!NEAR_EQUAL(rval, ctrl, FLOAT_CMP_TOL)) {
	    bu_log("Failed read from dbl:%d expected %0.15f, got %0.15f (delta: %0.17f)\n", i, ctrl, rval, ctrl - rval);
	    ret = 1;
	}
    }
    bu_cache_get_done(&txn);
    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("DBL post-remove read test completed - read %d DBLs in %g seconds.\n", rcnt, seconds);

    kcnt_expected = 2*item_cnt - rr_e + rr_s;
    print_keys(c, kcnt_expected, item_cnt);

    // Done processing - close cache before checking file size info to make sure
    // we are synced
    if (bu_cache_close(c) != BRLCAD_OK) {
	bu_log("Failed to close cache!\n");
	ret = 1;
    }

    // Reopen the cache and repeat the key test
    bu_log("Reopen and repeat key read...\n");
    c = bu_cache_open(cfile, 0, 0);
    print_keys(c, kcnt_expected, item_cnt);
    if (bu_cache_close(c) != BRLCAD_OK) {
	bu_log("Failed to close cache!\n");
	ret = 1;
    }

    if (!bu_file_exists(cache_file, NULL)) {
	bu_log("Cache file %s not found\n", cache_file);
	ret = 1;
    }

    bu_cache_erase(cfile);

    if (bu_file_exists(cache_file, NULL)) {
	bu_log("Cache file %s not erased\n", cache_file);
	ret = 1;
    }

    bu_log("Basic functionality test %s\n", ret ? "[FAIL]" : "[PASS]");
    return ret;
}

//------------------------ Section: Limit/Out-of-space Tests ----------------

int
limit_test()
{
    int ret = 0;
    int item_cnt;
    unsigned long pgsize = 4096;
    unsigned long fsize = 5e6/pgsize;
    fsize = fsize * pgsize;
    const char *cfile = "5M_cache";
    // Don't need a VLS for this really, but using it just to show how it
    // works.  See the basic test for a more typical key generation pattern.
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
	size_t written = bu_cache_write((char *)&ival, sizeof(int), bu_vls_cstr(&keystr), c, NULL);
	if (written != sizeof(int)) {
	    bu_log("FAIL: Unexpected - failed to write int %d\n", i);
	    ret = 1;
	}
    }

    // Write doubles - this is where we expect to hit failure
    double dblseed = M_PI;
    int failed = -1;
    int expected_failed_max = 4557;
    for (int i = 0; i < item_cnt; i++) {
	bu_vls_sprintf(&keystr, "dbl:%d", i);
	double dval = i*dblseed;
	size_t written = bu_cache_write((char *)&dval, sizeof(double), bu_vls_cstr(&keystr), c, NULL);
	if (written != sizeof(double)) {
	    bu_log("Failed to assign %s value %g\n", bu_vls_cstr(&keystr), dval);
	    failed = i;
	    break;
	}
    }

    if (failed == -1) {
	bu_log("FAIL: Expected failure not observed\n");
	ret = 1;
    } else if (failed > expected_failed_max) {
	bu_log("Warning - expected fail at <=%d, but observed failure was at %d\n", expected_failed_max, failed);
    } else {
	bu_log("Expected failure at %d observed\n", failed);
    }

    // Test reading DBL
    struct bu_cache_txn *txn = NULL;
    for (int i = 0; i < failed; i++) {
	bu_vls_sprintf(&keystr, "dbl:%d", i);
	void *rdata = NULL;
	size_t rsize = bu_cache_get(&rdata, bu_vls_cstr(&keystr), c, &txn);
	if (rsize != sizeof(double)) {
	    bu_log("Failed to read dbl:%d - expected to read %zd bytes but read %zd\n", i, sizeof(double), rsize);
	    ret = 1;
	}
	double rval = 0;
	memcpy(&rval, rdata, sizeof(double));
	double ctrl = i*dblseed;
	if (!NEAR_EQUAL(rval, ctrl, FLOAT_CMP_TOL)) {
	    bu_log("Failed read from dbl:%d expected %0.15f, got %0.15f (delta: %0.17f)\n", i, ctrl, rval, ctrl - rval);
	    ret = 1;
	}
    }
    bu_cache_get_done(&txn);
    bu_log("All successfully written values read\n");

    // Close the cache to make sure we're synced to disk before the next checks
    if (bu_cache_close(c) != BRLCAD_OK) {
	bu_log("Failed to close cache!\n");
	ret = 1;
    }

    // Before proceeding, see if the file size jibes with the limit
    char cache_file[MAXPATHLEN] = {0};
    bu_dir(cache_file, MAXPATHLEN, BU_DIR_CACHE, cfile, "data.mdb", NULL);
    if (!bu_file_exists(cache_file, NULL)) {
	bu_log("Cache data file %s not found\n", cache_file);
	ret = 1;
    }
    int dfsize = bu_file_size(cache_file);
    unsigned long fdelta = std::abs((long)((long)dfsize - (long)fsize));
    bu_log("Cache data file %s:\nApproximate expected size (bytes): %d\nObserved size(bytes):%lu\nDelta (bytes):%lu\n", cache_file, dfsize, fsize, fdelta);

    // Reopen the cache with a larger size and confirm we can add the
    // remainder of the values.
    fsize = 5e7/pgsize;
    fsize = fsize * pgsize;
    c = bu_cache_open(cfile, 1, fsize);
    bu_log("Reopened with larger max size\n");

    // Write remaining doubles - this time there shouldn't be a failure
    for (int i = failed; i < item_cnt; i++) {
	bu_vls_sprintf(&keystr, "dbl:%d", i);
	double dval = i*dblseed;
	size_t written = bu_cache_write((char *)&dval, sizeof(double), bu_vls_cstr(&keystr), c, NULL);
	if (written != sizeof(double)) {
	    bu_log("Failed to assign %s value %g\n", bu_vls_cstr(&keystr), dval);
	    ret = 1;
	}
    }
    bu_log("Remaining DBL values added\n");

    // Test reading all DBLs, both those added with the old max size and those
    // just added.
    for (int i = 0; i < item_cnt; i++) {
	bu_vls_sprintf(&keystr, "dbl:%d", i);
	void *rdata = NULL;
	size_t rsize = bu_cache_get(&rdata, bu_vls_cstr(&keystr), c, &txn);
	if (rsize != sizeof(double)) {
	    bu_log("Failed to read dbl:%d - expected to read %zd bytes but read %zd\n", i, sizeof(double), rsize);
	    ret = 1;
	    continue;
	}
	double rval = 0;
	memcpy(&rval, rdata, sizeof(double));
	double ctrl = i*dblseed;
	if (!NEAR_EQUAL(rval, ctrl, FLOAT_CMP_TOL)) {
	    bu_log("Failed read from dbl:%d expected %0.15f, got %0.15f (delta: %0.17f)\n", i, ctrl, rval, ctrl - rval);
	    ret = 1;
	}
    }
    bu_cache_get_done(&txn);
    bu_log("All DBL values (old and newly added) successfully read\n");
    if (bu_cache_close(c) != BRLCAD_OK) {
	bu_log("Failed to close cache!\n");
	ret = 1;
    }

    bu_dir(cache_file, MAXPATHLEN, BU_DIR_CACHE, cfile, NULL);
    if (!bu_file_exists(cache_file, NULL)) {
	bu_log("Cache file %s not found\n", cache_file);
	ret = 1;
    }

    bu_cache_erase(cfile);

    if (bu_file_exists(cache_file, NULL)) {
	bu_log("Cache file %s not erased\n", cache_file);
	ret = 1;
    }

    bu_vls_free(&keystr);
    bu_log("Limit/size test %s\n", ret ? "[FAIL]" : "[PASS]");
    return ret;
}

//------------------------ Section: Threading/Concurrency Tests -------------

int
val_incr(struct bu_cache *c, int item_cnt)
{
    int ret = 0;

    struct bu_cache_txn *t = NULL;
    char keystr[BU_CACHE_KEY_MAXLEN];
    for (int i = 0; i < item_cnt; i++) {
	int ival = i;
	snprintf(keystr, BU_CACHE_KEY_MAXLEN, "int:%d", i);
	size_t written = bu_cache_write((char *)&ival, sizeof(int), keystr, c, &t);
	if (written != sizeof(int)) {
	    bu_log("Failed to write int %d\n", i);
	    ret = 1;
	}
    }

    if (bu_cache_write_commit(c, &t) != 0) {
	bu_log("val_incr: batch write failed\n");
	ret = 1;
    }
    return ret;
}

void
val_read(struct bu_cache *c, int item_cnt, std::map<int, int> &ctrl)
{
    // Once we do read do it as fast as possible, but stagger the start of
    // reading a little to give all the reader threads time to get setup and
    // ready.  Mix up the kickoff times a little to add some entropy to the
    // process.
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_real_distribution<> rdist(0.0, 0.1);

    // We don't want to have threads finishing before others are launched, so
    // baseline the wait time to at least half a second.
    double wt = 0.5 + rdist(mt);
    std::chrono::duration<double> wtime(wt);
    std::this_thread::sleep_for(wtime);

    // Wait over - commence reading
    char keystr[BU_CACHE_KEY_MAXLEN];
    for (int i = 0; i < item_cnt; i++) {

	snprintf(keystr, BU_CACHE_KEY_MAXLEN, "int:%d", i);

	// If another thread or threads have already incremented
	// read that value.
	void *rdata = NULL;
	struct bu_cache_txn *txn = NULL;
	size_t rsize = bu_cache_get(&rdata, keystr, c, &txn);
	int rval = 0;
	if (rsize != sizeof(int))
	    bu_exit(1, "Failed to read int:%d - expected to read %zd bytes but read %zd\n", i, sizeof(int), rsize);
	memcpy(&rval, rdata, sizeof(rval));
	bu_cache_get_done(&txn);

	if (ctrl[i] != rval)
	    bu_exit(1, "%d - failed to read correct int value - expected to read %d but read %d\n", i, ctrl[i], rval);

	//std::cout << "thread " << std::this_thread::get_id() << " :" << i << "->" << rval << "\n";
    }

    std::cout << "thread " << std::this_thread::get_id() << " complete\n";
}

int
threading_test()
{
    int ret = 0;
    int64_t start, elapsed;
    fastf_t seconds;

    const char *cfile = "thread_cache";
    char cache_file[MAXPATHLEN] = {0};
    bu_dir(cache_file, MAXPATHLEN, BU_DIR_CACHE, cfile, NULL);

    bu_log("\n********************************************************************************\n");
    bu_log("*** Threading test - reading from cache with multiple threads ***\n");
    bu_log("********************************************************************************\n");

    struct bu_cache *c = bu_cache_open(cfile, 1, 0);

    start = bu_gettime();

    int tcnt = 20;
    int item_cnt = 300;

    if (val_incr(c, item_cnt)) {
	bu_log("Threading/concurrency test [FAIL]\n");
	bu_cache_close(c);
	bu_cache_erase(cfile);
	return 1;
    }

    // Read the values from a single thread as a control
    start = bu_gettime();
    std::map<int, int> ivals;
    struct bu_cache_txn *txn = NULL;
    char keystr[BU_CACHE_KEY_MAXLEN];
    for (int i = 0; i < item_cnt; i++) {
	snprintf(keystr, BU_CACHE_KEY_MAXLEN, "int:%d", i);
	void *rdata = NULL;
	size_t rsize = bu_cache_get(&rdata, keystr, c, &txn);
	if (rsize != sizeof(int)) {
	    bu_log("Failed to read int:%d - expected to read %zd bytes but read %zd\n", i, sizeof(int), rsize);
	    ret = 1;
	    continue;
	}
	int rval = 0;
	memcpy(&rval, rdata, sizeof(rval));
	ivals[i] = rval;
    }

    // Test reading from multiple threads
    std::vector<std::thread> rthreads;
    start = bu_gettime();

    // Kick off all the threads
    for (int i = 0; i < tcnt; ++i)
	rthreads.emplace_back(val_read, c, item_cnt, std::ref(ivals));

    // Wait for all the threads to finish
    for (std::thread& t : rthreads)
        t.join();

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Multi-threaded read test completed - %g seconds (NOTE: most of this is random wait time).\n", seconds);

    if (bu_cache_close(c) != BRLCAD_OK) {
	bu_log("Failed to close cache!\n");
	ret = 1;
    }

    if (!bu_file_exists(cache_file, NULL)) {
	bu_log("Cache file %s not found\n", cache_file);
	ret = 1;
    }

    bu_cache_erase(cfile);

    if (bu_file_exists(cache_file, NULL)) {
	bu_log("Cache file %s not erased\n", cache_file);
	ret = 1;
    }

    bu_log("Threading/concurrency test %s\n", ret ? "[FAIL]" : "[PASS]");
    return ret;
}

//------------------------ Section: Stress Tests ----------------------------

int stress_long_key_test(struct bu_cache *c) {
    int ret = 0;
    char longkey[BU_CACHE_KEY_MAXLEN + 2];
    char val[] = "testval";

    // 1. Key at max supported length (including null terminator)
    memset(longkey, 'A', BU_CACHE_KEY_MAXLEN - 1);
    longkey[BU_CACHE_KEY_MAXLEN - 1] = '\0';
    size_t written = bu_cache_write(val, sizeof(val), longkey, c, NULL);
    if (written != sizeof(val)) {
	bu_log("Failed to write max-length key\n");
	ret = 1;
    }

    // 2. Key just below max
    memset(longkey, 'B', BU_CACHE_KEY_MAXLEN - 2);
    longkey[BU_CACHE_KEY_MAXLEN - 2] = '\0';
    written = bu_cache_write(val, sizeof(val), longkey, c, NULL);
    if (written != sizeof(val)) {
	bu_log("Failed to write (max-1)-length key\n");
	ret = 1;
    }

    // 3. Key just above max (should fail)
    memset(longkey, 'C', BU_CACHE_KEY_MAXLEN);
    longkey[BU_CACHE_KEY_MAXLEN] = '\0';
    written = bu_cache_write(val, sizeof(val), longkey, c, NULL);
    if (written != 0) {
	bu_log("Should have failed to write (max+1)-length key, but did not\n");
	ret = 1;
    }

    bu_log("Long key test (BU_CACHE_KEY_MAXLEN) %s\n", ret ? "[FAIL]" : "[PASS]");
    return ret;
}

int stress_large_value_test(struct bu_cache *c) {
    int ret = 0;
    size_t bigsize = 1024 * 1024; // 1MB
    std::vector<char> bigval(bigsize, '@');
    const char *key = "bigvalue";
    size_t written = bu_cache_write(bigval.data(), bigsize, key, c, NULL);
    if (written != bigsize) {
	bu_log("Failed to write large value\n");
	ret = 1;
    }

    void *rdata = nullptr;
    size_t rsize = bu_cache_get(&rdata, key, c, NULL);
    if (rsize != bigsize) {
	bu_log("Failed to read back large value\n");
	ret = 1;
    }
    bu_free(rdata, "free big value");
    bu_log("Large value test %s\n", ret ? "[FAIL]" : "[PASS]");
    return ret;
}

int stress_unicode_key_value_test(struct bu_cache *c) {
    int ret = 0;
    // Use Japanese UTF-8 string literals for key and value
    const char *key = u8"鍵"; // "key" in Japanese
    const char *val = u8"値"; // "value" in Japanese
    size_t written = bu_cache_write((void *)val, strlen(val) + 1, key, c, NULL);
    if (written != strlen(val) + 1) {
	bu_log("Failed to write unicode key/value\n");
	ret = 1;
    }
    void *rdata = nullptr;
    size_t rsize = bu_cache_get(&rdata, key, c, NULL);
    if (rsize != strlen(val) + 1) {
	bu_log("Failed to read back unicode value\n");
	ret = 1;
    }
    bu_log("Unicode key/value test %s\n", ret ? "[FAIL]" : "[PASS]");
    bu_free(rdata, "free unicode value");
    return ret;
}

int mass_small_values_test(struct bu_cache *c) {
    int ret = 0;
    const int N = 10000;
    char key[32];
    int val = 12345;
    for (int i = 0; i < N; ++i) {
        snprintf(key, sizeof(key), "k%d", i);
        size_t written = bu_cache_write(&val, sizeof(val), key, c, NULL);
        if (written != sizeof(val)) {
            bu_log("Failed to write small value %d\n", i);
            ret = 1;
        }
    }
    bu_log("Mass small value test %s\n", ret ? "[FAIL]" : "[PASS]");
    return ret;
}

int mass_large_values_test(struct bu_cache *c) {
    int ret = 0;
    const int N = 100;
    size_t bigsize = 256 * 1024; // 256 KB
    std::vector<char> bigval(bigsize, '#');
    char key[32];
    for (int i = 0; i < N; ++i) {
        snprintf(key, sizeof(key), "bigk%d", i);
        size_t written = bu_cache_write(bigval.data(), bigsize, key, c, NULL);
        if (written != bigsize) {
            bu_log("Failed to write large value %d\n", i);
            ret = 1;
        }
    }
    bu_log("Mass large value test %s\n", ret ? "[FAIL]" : "[PASS]");
    return ret;
}

int stress_tests()
{
    int ret = 0;
    struct bu_cache *c = bu_cache_open("stress_cache", 1, 0);
    ret |= stress_long_key_test(c);
    ret |= stress_large_value_test(c);
    ret |= stress_unicode_key_value_test(c);
    ret |= mass_small_values_test(c);
    ret |= mass_large_values_test(c);
    if (bu_cache_close(c) != BRLCAD_OK) {
	bu_log("Failed to close cache!\n");
	ret = 1;
    }
    bu_cache_erase("stress_cache");
    bu_log("Overall stress tests %s\n", ret ? "[FAIL]" : "[PASS]");
    return ret;
}

//------------------------ Section: Boundary/Error Handling Tests -----------

int boundary_and_error_tests()
{
    int ret = 0;
    bu_log("\n================ Boundary & Error Handling Tests ================\n");
    struct bu_cache *c = bu_cache_open("boundary_cache", 1, 0);

    // ---- Empty string key ----
    const char *empty_key = "";
    int val = 42;
    size_t written = bu_cache_write(&val, sizeof(val), empty_key, c, NULL);
    if (written != 0) {
        bu_log("Should not have written with empty string key\n");
        ret = 1;
    }

    void *rdata = NULL;
    size_t rsize = bu_cache_get(&rdata, empty_key, c, NULL);
    if (rsize != 0) {
        bu_log("Should not have read value for empty string key\n");
        ret = 1;
    }

    // ---- Key with whitespace ----
    const char *ws_key = "   key with spaces   ";
    written = bu_cache_write(&val, sizeof(val), ws_key, c, NULL);
    if (written != sizeof(val)) {
        bu_log("Failed to write key with whitespace\n");
        ret = 1;
    }
    rdata = NULL;
    rsize = bu_cache_get(&rdata, ws_key, c, NULL);
    if (rsize != sizeof(val)) {
        bu_log("Failed to read key with whitespace\n");
        ret = 1;
    }
    bu_free(rdata, "free ws read");

    // ---- Very large value ----
    size_t lval_size = 2 * 1024 * 1024; // 2MB
    std::vector<char> lval(lval_size, '!');
    const char *lkey = "large_val";
    written = bu_cache_write(lval.data(), lval_size, lkey, c, NULL);
    if (written != lval_size) {
        bu_log("Failed to write very large value\n");
        ret = 1;
    }
    rdata = NULL;
    rsize = bu_cache_get(&rdata, lkey, c, NULL);
    if (rsize != lval_size) {
        bu_log("Failed to read very large value\n");
        ret = 1;
    }
    bu_free(rdata, "free large val");

    // ---- Very small value (1 byte) ----
    char sval = 'x';
    const char *skey = "singlebyte";
    written = bu_cache_write(&sval, 1, skey, c, NULL);
    if (written != 1) {
        bu_log("Failed to write single byte value\n");
        ret = 1;
    }
    rdata = NULL;
    rsize = bu_cache_get(&rdata, skey, c, NULL);
    if (rsize != 1 || ((char*)rdata)[0] != 'x') {
        bu_log("Failed to read single byte value\n");
        ret = 1;
    }
    bu_free(rdata, "free small val");

    // ---- Zero size value ----
    const char *zkey = "zero_size";
    written = bu_cache_write(&val, 0, zkey, c, NULL);
    if (written != 0) {
        bu_log("Should not have written zero-size value\n");
        ret = 1;
    }
    rdata = NULL;
    rsize = bu_cache_get(&rdata, zkey, c, NULL);
    if (rsize != 0) {
        bu_log("Should not have read zero-size value\n");
        ret = 1;
    }

    // ---- NULL key ----
    written = bu_cache_write(&val, sizeof(val), NULL, c, NULL);
    if (written != 0) {
        bu_log("Should not have written with NULL key\n");
        ret = 1;
    }
    rdata = NULL;
    rsize = bu_cache_get(&rdata, NULL, c, NULL);
    if (rsize != 0) {
        bu_log("Should not have read with NULL key\n");
        ret = 1;
    }

    // ---- NULL data ----
    written = bu_cache_write(NULL, sizeof(val), "null_data", c, NULL);
    if (written != 0) {
        bu_log("Should not have written NULL data\n");
        ret = 1;
    }
    rdata = NULL;
    rsize = bu_cache_get(NULL, "null_data", c, NULL);
    if (rsize != 0) {
        bu_log("Should not have read value for NULL data key\n");
        ret = 1;
    }

    // ---- NULL cache handle ----
    written = bu_cache_write(&val, sizeof(val), "badcache", NULL, NULL);
    if (written != 0) {
        bu_log("Should not have written with NULL cache\n");
        ret = 1;
    }
    rsize = bu_cache_get(&rdata, "badcache", NULL, NULL);
    if (rsize != 0) {
        bu_log("Should not have read with NULL cache\n");
        ret = 1;
    }

    // ---- Key with only whitespace ----
    const char *only_ws = "    ";
    written = bu_cache_write(&val, sizeof(val), only_ws, c, NULL);
    if (written != sizeof(val)) {
        bu_log("Failed to write key with only whitespace\n");
        ret = 1;
    }
    rdata = NULL;
    rsize = bu_cache_get(&rdata, only_ws, c, NULL);
    if (rsize != sizeof(val)) {
        bu_log("Failed to read key with only whitespace\n");
        ret = 1;
    }
    bu_free(rdata, "free ws-only val");

    if (bu_cache_close(c) != BRLCAD_OK) {
        bu_log("Failed to close cache!\n");
        ret = 1;
    }
    bu_cache_erase("boundary_cache");
    bu_log("Boundary & error handling tests %s\n", ret ? "[FAIL]" : "[PASS]");
    return ret;
}

/* Validate multiple writes in single commit behavior (should be MUCH faster) */
int
txn_reuse_performance_test(int item_cnt)
{
    int ret = 0;
    const char *cfile = "txn_perf_cache";
    char keystr[BU_CACHE_KEY_MAXLEN];

    bu_log("\n===============================================================================\n");
    bu_log("Write Transaction Reuse Performance Test\n");
    bu_log("===============================================================================\n");

    // ======= Non-reuse: One write txn per call =======
    bu_dirclear(NULL); // Clear cache dir if needed
    struct bu_cache *c = bu_cache_open(cfile, 1, 0);

    int64_t start = bu_gettime();
    for (int i = 0; i < item_cnt; i++) {
        snprintf(keystr, BU_CACHE_KEY_MAXLEN, "nrtxn:%d", i);
        int ival = i;
        size_t written = bu_cache_write((void *)&ival, sizeof(int), keystr, c, NULL);
        if (written != sizeof(int)) {
            bu_log("One-shot write failed for key %s\n", keystr);
            ret = 1;
        }
    }
    int64_t elapsed = bu_gettime() - start;
    fastf_t single_seconds = elapsed / 1000000.0;
    bu_log("Non-reuse: Wrote %d INT entries in %g seconds.\n", item_cnt, single_seconds);


    // === Validation pass: read back all keys and verify contents ===
    int bad_reads = 0;
    struct bu_cache_txn *rtxn = NULL;
    for (int i = 0; i < item_cnt; i++) {
        snprintf(keystr, BU_CACHE_KEY_MAXLEN, "nrtxn:%d", i);
        void *rdata = NULL;
        size_t rsize = bu_cache_get(&rdata, keystr, c, &rtxn);
        if (rsize != sizeof(int)) {
            bu_log("One-shot validation: failed to read key %s (size %zd)\n", keystr, rsize);
            ret = 1;
            bad_reads++;
            continue;
        }
        int rval = 0;
        memcpy(&rval, rdata, sizeof(rval));
        if (rval != i) {
            bu_log("One-shot validation: bad value for key %s: got %d expected %d\n", keystr, rval, i);
            ret = 1;
            bad_reads++;
        }
    }
    bu_cache_get_done(&rtxn);
    bu_log("One-shot write validation: %d bad reads out of %d\n", bad_reads, item_cnt);

    if (bu_cache_close(c) != BRLCAD_OK) {
	bu_log("Failed to close cache!\n");
        ret = 1;
    }
    bu_cache_erase(cfile);

    // ======= Reuse: Single write txn for all writes =======
    c = bu_cache_open(cfile, 1, 0);

    start = bu_gettime();
    struct bu_cache_txn *txn = NULL;
    for (int i = 0; i < item_cnt; i++) {
	snprintf(keystr, BU_CACHE_KEY_MAXLEN, "reuse:%d", i);
	int ival = i;
	size_t written = bu_cache_write((void *)&ival, sizeof(int), keystr, c, &txn);
	if (written != sizeof(int)) {
	    bu_log("Batched write failed for key %s\n", keystr);
	    ret = 1;
	}
    }
    if (bu_cache_write_commit(c, &txn) != 0) {
	bu_log("Failed to commit batched write transaction\n");
	ret = 1;
    }

    elapsed = bu_gettime() - start;
    fastf_t multi_seconds = elapsed / 1000000.0;
    bu_log("Reuse: Wrote %d INT entries in %g seconds.\n", item_cnt, multi_seconds);

    // === Validation pass: read back all keys and verify contents ===
    bad_reads = 0;
    rtxn = NULL;
    for (int i = 0; i < item_cnt; i++) {
        snprintf(keystr, BU_CACHE_KEY_MAXLEN, "reuse:%d", i);
        void *rdata = NULL;
        size_t rsize = bu_cache_get(&rdata, keystr, c, &rtxn);
        if (rsize != sizeof(int)) {
            bu_log("Reuse validation: failed to read key %s (size %zd)\n", keystr, rsize);
            ret = 1;
            bad_reads++;
            continue;
        }
        int rval = 0;
        memcpy(&rval, rdata, sizeof(rval));
        if (rval != i) {
            bu_log("Reuse validation: bad value for key %s: got %d expected %d\n", keystr, rval, i);
            ret = 1;
            bad_reads++;
        }
    }
    bu_cache_get_done(&rtxn);
    bu_log("Reuse write validation: %d bad reads out of %d\n", bad_reads, item_cnt);

    if (bu_cache_close(c) != BRLCAD_OK) {
	bu_log("Failed to close cache!\n");
        ret = 1;
    }
    bu_cache_erase(cfile);

    if (single_seconds < multi_seconds) {
	bu_log("Per-data write was faster than batched write - that indicates something is wrong!.\n");
	ret = 1;
    }

    bu_log("Transaction reuse performance test %s\n", ret ? "[FAIL]" : "[PASS]");
    return ret;
}

//------------------------ Section: Test Summary Printing -------------------

void print_test_summary(int ret_basic, int ret_limit, int ret_threading, int ret_stress, int ret_boundary, int ret_multiwrite)
{
    bu_log("\n========================\n");
    bu_log("   Test Summary Report  \n");
    bu_log("========================\n");

    auto show = [](int ok) { return ok ? "[FAIL]" : "[PASS]"; };
    bu_log("Basic Functionality:      %s\n", show(ret_basic));
    bu_log("Limit/Size Test:          %s\n", show(ret_limit));
    bu_log("Threading/Concurrency:    %s\n", show(ret_threading));
    bu_log("Stress Tests:             %s\n", show(ret_stress));
    bu_log("Boundary/Error Handling:  %s\n", show(ret_boundary));
    bu_log("Write Transaction Reuse Performance Test:  %s\n", show(ret_multiwrite));
    bu_log("========================\n");
    int overall = ret_basic | ret_limit | ret_threading | ret_stress | ret_boundary |ret_multiwrite;
    if (!overall)
        bu_log("ALL TESTS PASSED\n");
    else
        bu_log("SOME TESTS FAILED\n");
    bu_log("========================\n");
}

//------------------------ Section: Main Entry ------------------------------

int
main(int argc, char **argv)
{
    // Number of each type to write in the basic test - can be overridden by
    // explicit specification on command line.
    int item_cnt = 100000;

    bu_setprogname(argv[0]);

    // Set up to use a local cache
    char cache_dir[MAXPATHLEN] = {0};
    bu_dir(cache_dir, MAXPATHLEN, BU_DIR_CURR, "libbu_test_cache", NULL);
    bu_setenv("BU_DIR_CACHE", cache_dir, 1);
    bu_dirclear(cache_dir);
    bu_mkdir(cache_dir);

    if (argc > 1) {
	const char *av[2] = {NULL};
	av[0] = argv[1];
	int iret = bu_opt_int(NULL, 1, av, &item_cnt);
	if (iret != 1)
	    bu_exit(-1, "Invalid item count specifier\n");
    }

    int ret = 0;
    int ret_basic = basic_test(item_cnt);           ret |= ret_basic;
    int ret_limit = limit_test();                   ret |= ret_limit;
    int ret_threading = threading_test();           ret |= ret_threading;
    int ret_stress = stress_tests();                ret |= ret_stress;
    int ret_boundary = boundary_and_error_tests();  ret |= ret_boundary;
    int ret_multiwrite = txn_reuse_performance_test(item_cnt);  ret |= ret_multiwrite;

    bu_dirclear(cache_dir);

    print_test_summary(ret_basic, ret_limit, ret_threading, ret_stress, ret_boundary, ret_multiwrite);

    return (ret ? 1 : 0);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

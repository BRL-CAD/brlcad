/*                      D E N S I T Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2011-2022 United States Government as represented by
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

#include <set>

#include "bu/app.h"
#include "analyze.h"
#include "limits.h"


const char *simple_buff = "1 7.8295        steel";
const char *test_buff =
    "#  Test density file with comments and bad input\n"
    "2    7.82      Carbon Tool Steel\n"
    "3    2.7       Aluminum, 6061-T6\n"
    "4    2.74      Aluminum, 7079-T6\n"
    "#  Incomplete line following\n"
    "               Copper, pure\n"
    "               6    19.32     Gold, pure\n"
    "               7    8.03      Stainless, 18Cr-8Ni\n"
    "#  Comment\n"
    "               8    7.47      Stainless 27Cr\n"
    "\n"
    "               9    7.715     Steel, tool\n"
    "\n"
    "#  Blank line above\n"
    "#  Comment following valid data on the line below\n"
    "               10   7.84      Carbon Steel # used for widgets\n"
    "               12   3.00      Gunner\n"
    "               14   10.00     Fuel\n"
    "#  Material ID too high \n"
    CPP_XSTR(LONG_MAX) " 70.84    Kryptonite\n";

const char *tbuff_out_ctrl =
    "2	7.82	Carbon Tool Steel\n"
    "3	2.7	Aluminum, 6061-T6\n"
    "4	2.74	Aluminum, 7079-T6\n"
    "6	19.32	Gold, pure\n"
    "7	8.03	Stainless, 18Cr-8Ni\n"
    "8	7.47	Stainless 27Cr\n"
    "9	7.715	Steel, tool\n"
    "10	7.84	Carbon Steel\n"
    "12	3	Gunner\n"
    "14	10	Fuel\n";


static int
dtest_validate(struct analyze_densities *a, long int id, fastf_t density, const char *name)
{
    char *db_name = analyze_densities_name(a, id);
    if (!db_name) {
	bu_log("Error - could not find name for id %ld\n", id);
	return -1;
    }
    if (!BU_STR_EQUAL(name, db_name)) {
	bu_log("Error - found incorrect name for %ld (%s instead of %s)\n", id, db_name, name);
	bu_free(db_name, "free name");
	return -1;
    }
    bu_free(db_name, "free name");

    fastf_t db_density = analyze_densities_density(a, id);
    if (db_density < 0) {
	bu_log("Error - could not find density for id %ld\n", id);
	return -1;
    }
    if (!NEAR_EQUAL(density, db_density, SMALL_FASTF)) {
	bu_log("Error - found incorrect density for %ld (%g instead of %g)\n", id, db_density, density);
	return -1;
    }

    long int max_ids = 3;
    long int ids[3] = {-1, -1, -1};
    long int idcnt = analyze_densities_id((long int *)ids, max_ids, a, name);
    if (idcnt == 0) {
	bu_log("Error - could not find any ids associated with name %s\n", name);
	return -1;
    }

    int have_correct_id = 0;
    for (long int i = 0; i < max_ids; i++) {
	if (ids[i] >= 0 && ids[i] == id) {
	    have_correct_id = 1;
	    break;
	}
    }
    if (!have_correct_id) {
	bu_log("Error - could not find correct id (%ld) associated with name %s\n", id, name);
	return -1;
    }

    return 0;
}


static int
dtest_insert(struct analyze_densities *a, long int id, fastf_t density, const char *name, struct bu_vls *msgs)
{
    if (analyze_densities_set(a, id, density, name, msgs)) {
	bu_log("Error inserting density: %s\n", bu_vls_cstr(msgs));
	return -1;
    }
    if (dtest_validate(a, id, density, name)) {
	return -1;
    }
    return 0;
}


int
main(int argc, char **argv)
{
    struct bu_vls msgs = BU_VLS_INIT_ZERO;
    struct analyze_densities *a = NULL;

    bu_setprogname(argv[0]);

    if (argc < 1 || !argv) {
	return -1;
    }

    /* Test behaviors with an empty database */
    if (argc == 1) {
	long int max_ids = 2;
	long int ids[2];

	analyze_densities_create(&a);

	if (analyze_densities_name(a, 0)) {
	    bu_log("Error - empty database returned a non-NULL name on id lookup??\n");
	    goto analyze_density_fail;
	}
	if (analyze_densities_id((long int *)ids, max_ids, a, "Aluminum") != 0) {
	    bu_log("Error - empty database returned a non-NULL id set on name lookup??\n");
	    goto analyze_density_fail;
	}
	if (analyze_densities_density(a, 0) >= 0) {
	    bu_log("Error - empty database returned a density on id lookup??\n");
	    goto analyze_density_fail;
	}
	if (analyze_densities_load(a, NULL, &msgs, NULL) != 0) {
	    bu_log("Error - loading NULL buffer returned a non-zero density count??\nMsg: %s", bu_vls_cstr(&msgs));
	    goto analyze_density_fail;
	}
	if (analyze_densities_load(a, "", &msgs, NULL) != 0) {
	    bu_log("Error - loading empty buffer returned a non-zero density count??Msg: %s\n", bu_vls_cstr(&msgs));
	    goto analyze_density_fail;
	}
	if (analyze_densities_next(a, -1) != -1) {
	    bu_log("Error - found initial \"next\" iterator in empty database??\n");
	    goto analyze_density_fail;
	}
	if (analyze_densities_next(a, 0) != -1) {
	    bu_log("Error - found \"next\" iterator in empty database with invalid id??\n");
	    goto analyze_density_fail;
	}

	analyze_densities_destroy(a);
	bu_vls_free(&msgs);
	return 0;
    }

    if (BU_STR_EQUAL(argv[1], "std")) {

	int ecnt = 0;

	std::set<long int> valid_ids = {0, 2, 15, 4, 31};

	analyze_densities_create(&a);

	/* Initialize with several values */
	if (dtest_insert(a, 0, 0.007715, "Steel", &msgs)) {
	    goto analyze_density_fail;
	}
	if (dtest_insert(a, 2, 0.00782, "Carbon Tool Steel", &msgs)) {
	    goto analyze_density_fail;
	}
	if (dtest_insert(a, 15, 0.0027, "Aluminum, 6061-T6", &msgs)) {
	    goto analyze_density_fail;
	}
	if (dtest_insert(a, 4,  0.00274, "Aluminum, 7079-T6", &msgs)) {
	    goto analyze_density_fail;
	}

	/* Check assignment and duplicate id behaviors given existing entries */

	/* Redefine id 0 */
	if (dtest_insert(a, 0, 0.001, "Light Steel", &msgs)) {
	    goto analyze_density_fail;
	}

	/* Rename id 15 */
	if (dtest_insert(a, 15, 0.0027, "Aluminum", &msgs)) {
	    goto analyze_density_fail;
	}

	/* Add another id for Carbon Tool Steel */
	if (dtest_insert(a, 31, analyze_densities_density(a, 2), "Carbon Tool Steel", &msgs)) {
	    goto analyze_density_fail;
	}

	/* Check that we can retrieve both IDs for Carbon Tool Steel */
	long int max_ids = 2;
	long int ids[2] = {-1, -1};
	long int idcnt = analyze_densities_id((long int *)ids, max_ids, a, "Carbon Tool Steel");
	if (idcnt != 2) {
	    bu_log("Error - could not find both ids for Carbon Tool Steel\n");
	    goto analyze_density_fail;
	}
	std::set<long int> exp_cts = {2, 31};
	std::set<long int> found_cts;
	for (long int i = 0; i < 2; i++) {
	    found_cts.insert(ids[i]);
	}
	if (found_cts != exp_cts) {
	    bu_log("Error - could not find both ids for Carbon Tool Steel\n");
	    goto analyze_density_fail;
	}

	/* Check that we can iterate over the database */
	std::set<long int> found_ids;
	long int curr_id = -1;
	while ((curr_id = analyze_densities_next(a, curr_id)) != -1) {
	    if (valid_ids.find(curr_id) == valid_ids.end()) {
		bu_log("Error - found invalid id %ld while iterating\n", curr_id);
		goto analyze_density_fail;
	    } else {
		found_ids.insert(curr_id);
	    }
	}
	if (found_ids != valid_ids) {
	    std::set<long int>::iterator f_it;
	    bu_log("Error - did not find the exact set of all valid ids while iterating\nFound: ");
	    for (f_it = found_ids.begin(); f_it != found_ids.end(); f_it++) {
		bu_log("%ld ", *f_it);
	    }
	    bu_log("\nExpecting: ");
	    for (f_it = valid_ids.begin(); f_it != valid_ids.end(); f_it++) {
		bu_log("%ld ", *f_it);
	    }
	    bu_log("\n");
	    goto analyze_density_fail;
	}

	/* Clear the database */
	analyze_densities_clear(a);
	analyze_densities_init(a);
	curr_id = -1;
	long int id_cnt = 0;
	while ((curr_id = analyze_densities_next(a, curr_id)) != -1) id_cnt++;
	if (id_cnt) {
	    bu_log("Error - analyze_densities_clear + analyze_densities_init failed to rest the database: found %ld entities\n", id_cnt);
	    goto analyze_density_fail;
	}

	/* Test loading from buffers */
	ecnt = 0;
	long int sb_cnt = analyze_densities_load(a, simple_buff, &msgs, &ecnt);
	if (!sb_cnt) {
	    bu_log("Error - could not find id in simple test buffer: %s\n", bu_vls_cstr(&msgs));
	    goto analyze_density_fail;
	}
	if (ecnt) {
	    bu_log("Error - invalid line found parsing simple test buffer: %s\n", bu_vls_cstr(&msgs));
	    goto analyze_density_fail;
	}
	if (sb_cnt != 1) {
	    bu_log("Error - found %ld ids in test buffer, but expected 1\n", sb_cnt);
	    goto analyze_density_fail;
	}

	analyze_densities_clear(a);
	analyze_densities_init(a);

	ecnt = 0;
	long int tb_cnt = analyze_densities_load(a, test_buff, &msgs, &ecnt);
	if (!tb_cnt) {
	    bu_log("Error - could not find ids in test buffer: %s\n", bu_vls_cstr(&msgs));
	    goto analyze_density_fail;
	}
	if (tb_cnt != 10) {
	    bu_log("Error - found %ld ids in test buffer, but expected 10\n", tb_cnt);
	    goto analyze_density_fail;
	}
	if (ecnt != 2) {
	    bu_log("Error - found %d lines with parsing errors in test buffer, but expected 2:\n%s\n", ecnt, bu_vls_cstr(&msgs));
	    goto analyze_density_fail;
	}

	/* Test writing out a buffer */
	char *tbuff_out;
	long int tbuff_len = analyze_densities_write(&tbuff_out, a);
	if (!tbuff_len) {
	    bu_log("Error - unable to write out buffer!\n");
	    goto analyze_density_fail;
	}

	if (!BU_STR_EQUAL(tbuff_out, tbuff_out_ctrl)) {
	    bu_log("Error - buffer does not match!\n");
	    bu_log("Expected:\n%s", tbuff_out_ctrl);
	    bu_log("Got:\n%s", tbuff_out);
	    goto analyze_density_fail;
	}

	analyze_densities_destroy(a);
	bu_vls_free(&msgs);
	return 0;
    }

analyze_density_fail:
    analyze_densities_destroy(a);
    bu_vls_free(&msgs);
    return -1;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8



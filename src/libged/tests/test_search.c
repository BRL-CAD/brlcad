/*                   T E S T _ S E A R C H . C
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
/** @file test_search.c
 *
 * Brief description
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <bu.h>
#include <ged.h>

#define MAX_PARTS 12 // note: known max number of \n separated results for test file 'search_tests.g'

typedef struct {
    char** arr;
    int count;
} split_data;

split_data split_newline(char* input) {
    char* parts[MAX_PARTS];
    int count = 0;

    parts[count++] = input;
    char* ptr = input;
    while (*ptr && count < MAX_PARTS) {
	if (*ptr == '\n') {
	    *ptr = 0;
	    parts[count++] = ptr + 1;
	}
	ptr++;
    }

    // make sure we're null terminated
    if (!*parts[count - 1]) {
	parts[--count] = 0;
    } else {
	parts[count] = 0;
    }

    split_data ret = {parts, count};
    return ret;
}

int run_search_check_count(struct ged *dbp, int expected_count, int num_args, ...) {
    const char *cmd[6] = {"search", NULL, NULL, NULL, NULL, NULL};
    va_list args;
    va_start(args, num_args);

    for (int i = 1; i < num_args + 1; i++) {	// offset start by 1 so 'search' is first in cmd array
	cmd[i] = va_arg(args, char*);
    }

    int ged_ret = ged_exec(dbp, num_args + 1, cmd);
    if (ged_ret) {
	printf("ERROR: bad return for %s\n", *cmd);
	return ged_ret;
    }
    split_data split_ret = split_newline(bu_vls_addr(dbp->ged_result_str));
    if (split_ret.count != expected_count) {
	printf("ERROR: incorrect number of results for %s\nExpected: [%d] but got [%d]\n", *cmd, expected_count, split_ret.count);
	return 1;
    }

    return 0;
}

int
main(int ac, char *av[]) {
    struct ged *dbp;

    split_data split_ret;

    bu_setprogname(av[0]);

    // check usage
    if (ac != 2) {
	printf("Usage: %s file.g\n", av[0]);
	return 1;
    }
    if (!bu_file_exists(av[1], NULL)) {
	printf("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
	return 2;
    }

    dbp = ged_open("db", av[1], 1);

    int curr_test = 0;
    int ret_code = 0;
    while (++curr_test) {
	switch (curr_test)
	{
	    case 1:
		// Generic searches
		// TODO: check something more concrete than just number of newline separated results
		ret_code += run_search_check_count(dbp, 8, 1, ".");
		ret_code += run_search_check_count(dbp, 10, 1, "/");
		ret_code += run_search_check_count(dbp, 8, 1, "|");
		break;
	    case 2:
		// -type region
		ret_code += run_search_check_count(dbp, 3, 3, ".", "-type", "region");
		ret_code += run_search_check_count(dbp, 3, 2, "-type", "region");
		break;
	    case 3:
		{
		    // verbosity
		    const char *cmd[6] = {"search", "-v", ".", "-type", "region", NULL};
		    ged_exec(dbp, 5, cmd);
		    split_ret = split_newline(bu_vls_addr(dbp->ged_result_str));

		    if (!split_ret.count) {
			printf("ERROR: split_ret.count == 0");
			ret_code = 1;
			break;
		    }
		    if (split_ret.arr[split_ret.count - 1][1] != '3') {
			printf("ERROR: bad -v result. Got \"%s\", expected [%s]\n", split_ret.arr[split_ret.count - 1], "3");
			ret_code = 1;
			break;
		    }

		    // max verbosity
		    cmd[1] = "-vvv";
		    cmd[2] = "/";
		    ged_exec(dbp, 5, cmd);
		    split_ret = split_newline(bu_vls_addr(dbp->ged_result_str));
		    if (split_ret.count) {
			// should see union 'u' as second char of first search result
			char chk_u = split_ret.arr[0][1];
			// should see region 'r' as second to last char of first search result
			char chk_r = split_ret.arr[0][strnlen(split_ret.arr[0], 30) - 2];
			if (chk_u != 'u') {
			    printf("ERROR: bad -vv result. Got \"%s\", expected [%s]\n", split_ret.arr[0], "/u");
			    ret_code = 1;
			    break;
			}
			if (chk_r != 'r') {
			    printf("ERROR: bad -vvv result. Got \"%s\", expected [%s]\n", split_ret.arr[0], "(r)");
			    ret_code = 1;
			    break;
			}
		    } else {
			printf("ERROR: split_ret.count == 0");
			ret_code = 1;
			break;
		    }
		    break;
		}
	    default:
		goto finish_tests;
		break;
	}
	if (ret_code) {
	    printf("[test_search]: test [%d] failed\n", curr_test);
	    break;
	}
    }

finish_tests:
    ged_close(dbp);

    return 0;
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

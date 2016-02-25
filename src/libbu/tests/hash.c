/*                       H A S H . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2016 United States Government as represented by
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
#include <limits.h>
#include <string.h>
#include "bu.h"

const char *array1[] = {
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7"
};

const char *array2[] = {
    "r1",
    "r2",
    "r3",
    "r4",
    "r5",
    "r6",
    "r7"
};

int indices[] = {7,6,4,3,5,1,2};

int hash_noop_test() {
    bu_nhash_tbl *t = bu_nhash_tbl_create(0);
    bu_nhash_tbl_destroy(t);
    return 0;
}

int hash_add_del_one() {
    int *val = NULL;
    bu_nhash_tbl *t = bu_nhash_tbl_create(0);
    if (bu_nhash_set(t, (const uint8_t *)array2[0], strlen(array2[0]), (void *)&indices[0]) == -1) return 1;
    val = (int *)bu_nhash_get(t, (const uint8_t *)"r1", strlen("r1"));
    bu_nhash_tbl_destroy(t);
    return (*val == 7) ? 0 : 1;
}

int
main(int argc, const char **argv)
{
    int ret = 0;
    long test_num;
    char *endptr = NULL;

    /* Sanity checks */
    if (argc < 2) {
	bu_exit(1, "ERROR: wrong number of parameters - need test num");
    }
    test_num = strtol(argv[1], &endptr, 0);
    if (endptr && strlen(endptr) != 0) {
	bu_exit(1, "Invalid test number: %s\n", argv[1]);
    }

    switch (test_num) {
	case 0:
	    ret = hash_noop_test();
	case 1:
	    ret = hash_add_del_one();
	    break;
	case 2:
	    /*ret = ;*/
	    break;
    }

    return ret;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

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

#ifdef TEST_TCL_HASH
#include <tcl.h>
#endif

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

int
hash_basic_test() {
    int i = 0;
    struct bu_hash_tbl *tbl = bu_hash_tbl_create(1);
    struct bu_hash_tbl *ntbl = bu_hash_tbl_create(1);

    for (i = 0; i < 7; i++) {
	int n = 0;
	struct bu_hash_entry *b, *ne;
	b = bu_hash_tbl_add(tbl, (const unsigned char *)array1[i], strlen(array1[i]), &n);
	if (n) {
	    bu_set_hash_value(b, (unsigned char *)array2[i]);
	}
	n = 0;
	ne = bu_hash_tbl_add(ntbl, (unsigned char *)&i, 1, &n);
	if (n) {
	    bu_set_hash_value(ne, (unsigned char *)array2[i]);
	}

    }

    for (i = 0; i < 7; i++) {
	struct bu_hash_entry *t, *p;
	unsigned long idx;
	t = bu_hash_tbl_find(tbl, (const unsigned char *)array1[i], strlen(array1[i]), &p, &idx);
	bu_log("Hash lookup %d: %s -> %s,%s\n", i, array1[i], t->key, t->value);
	t = bu_hash_tbl_find(ntbl, (unsigned char *)&i, 1, &p, &idx);
	bu_log("Hash lookup %d: %s -> %s,%s\n", i, array1[i], t->key, t->value);
    }

    return 1;
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
	    ret = hash_basic_test();
	case 1:
	    /*ret = ;*/
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

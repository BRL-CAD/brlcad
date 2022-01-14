/*                       S T R . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"

#include "vmath.h"

#define SIGNUM(x) ((x > 0) - (x < 0))

static int
test_bu_strlcatm(int argc, char *argv[])
{
    char *dst;
    char *expected_result = argv[6];
    int ret, expected_ret;
    int scanned_size;
    size_t size;
    size_t len;
    char *empty_result = bu_strdup("");

    /* CMake won't pass empty strings as test parameters properly;
     * assume expected_result is supposed to be empty.
     */
    if (argc == 6) {
	expected_result = empty_result;
	argc++;
    }

    if (argc != 7) {
	bu_exit(1, "ERROR: input format is string1 string2 result_size expected_ret expected_result [%s]\n", argv[0]);
    }

    sscanf(argv[4], "%d", &scanned_size);
    if (scanned_size < 1)
	size = 1;
    else if (scanned_size > 1024*1024 /* arbitrary upper limit */)
	size = 1024*1024;
    else
	size = (size_t)scanned_size;

    sscanf(argv[5], "%d", &expected_ret);

    dst = (char *)bu_malloc(size, "test_bu_strlcatm");
    bu_strlcpym(dst, argv[2], size, "test_bu_strlcatm");
    ret = bu_strlcatm(dst, argv[3], size, "test_bu_strlcatm");
    len = strlen(dst);

    printf("Result: \"%s\" (return value %d)", dst, ret);

    int eqtest1 = BU_STR_EQUAL(expected_result, dst);
    int eqtest2 = (expected_ret == ret);
    bu_free(empty_result, "empty_result");

    return !(eqtest1 && eqtest2
	     && len <= size
	     && dst[len] == '\0');
}

static int
test_bu_strlcpym(int argc, char *argv[])
{
    char *dst;
    char *expected_result = argv[5];
    int ret, expected_ret;
    int scanned_size;
    size_t size;
    size_t len;
    char *empty_result = bu_strdup("");

    /* CMake won't pass empty strings as test parameters properly;
     * assume expected_result is supposed to be empty.
     */
    if (argc == 5) {
	expected_result = empty_result;
	argc++;
    }

    if (argc != 6) {
	bu_exit(1, "ERROR: input format is string result_size expected_ret expected_result [%s]\n", argv[0]);
    }

    sscanf(argv[3], "%d", &scanned_size);
    if (scanned_size < 1)
	size = 1;
    else if (scanned_size > 1024*1024 /* arbitrary upper limit */)
	size = 1024*1024;
    else
	size = (size_t)scanned_size;

    sscanf(argv[4], "%d", &expected_ret);

    dst = (char *)bu_malloc(size, "test_bu_strlcpym");
    ret = bu_strlcpym(dst, argv[2], size, "test_bu_strlcpym");
    len = strlen(dst);

    printf("Result: \"%s\" (return value %d)", dst, ret);

    int eqtest1 = BU_STR_EQUAL(expected_result, dst);
    int eqtest2 = (expected_ret == ret);
    bu_free(empty_result, "empty_result");

    return !(eqtest1 && eqtest2
	     && len <= size
	     && dst[len] == '\0');
}

static int
test_bu_strdupm(int argc, char *argv[])
{
    char *dst;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is string [%s]\n", argv[0]);
    }

    dst = bu_strdupm(argv[2], "test_bu_strlcpym");

    printf("Result: \"%s\"", dst);

    return !(BU_STR_EQUAL(argv[2], dst));
}

static int
test_bu_strcmp_like_functions(int argc, char *argv[], int (*fun)(const char *, const char *))
{
    int ret, expected_ret;

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is string1 string2 expected_ret [%s]\n", argv[0]);
    }
    sscanf(argv[4], "%d", &expected_ret);

    ret = (fun)(argv[2], argv[3]);

    printf("Result: %d", ret);

    return !(SIGNUM(expected_ret) == SIGNUM(ret));
}

static int
test_bu_strncmp_like_functions(int argc, char *argv[], int (*fun)(const char *, const char *, size_t))
{
    int n;
    int ret, expected_ret;

    if (argc != 6) {
	bu_exit(1, "ERROR: input format is string1 string2 n expected_ret [%s]\n", argv[0]);
    }
    sscanf(argv[4], "%d", &n);
    sscanf(argv[5], "%d", &expected_ret);

    ret = (*fun)(argv[2], argv[3], n);

    printf("Result: %d", ret);

    return !(SIGNUM(expected_ret) == SIGNUM(ret));
}

int
main(int argc, char *argv[])
{
    int function_num = 0;

    bu_setprogname(argv[0]);

    if (argc < 2) {
	bu_exit(1, "Usage: %s {function_num} {args...}\n", argv[0]);
    }

    sscanf(argv[1], "%d", &function_num);

    switch (function_num) {
	case 1:
	    return test_bu_strlcatm(argc, argv);
	case 2:
	    return test_bu_strlcpym(argc, argv);
	case 3:
	    return test_bu_strdupm(argc, argv);
	case 4:
	    return test_bu_strcmp_like_functions(argc, argv, &bu_strcmp);
	case 5:
	    return test_bu_strncmp_like_functions(argc, argv, &bu_strncmp);
	case 6:
	    return test_bu_strcmp_like_functions(argc, argv, &bu_strcasecmp);
	case 7:
	    return test_bu_strncmp_like_functions(argc, argv, &bu_strncasecmp);
    }

    bu_log("ERROR: function_num %d is not valid [%s]\n", function_num, argv[0]);

    return 1;
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

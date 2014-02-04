/*                       B U _ V L S . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2014 United States Government as represented by
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

#include "./vls_internals.h"
#include "./test_internals.h"


/* This prints out the values of the expected vls and compares it with
 * the actual vls; it uses the C-style return values of 0 for false and 1
 * for true
 */
static int
compare_vls(struct bu_vls *actual_vls, struct bu_vls *expected_vls)
{
    bu_log("magic: %lu", (unsigned long)expected_vls->vls_magic);
    bu_log("str: %s", expected_vls->vls_str);
    bu_log("offset: %lu", (unsigned long)expected_vls->vls_offset);
    bu_log("len: %lu", (unsigned long)expected_vls->vls_len);
    bu_log("max: %lu", (unsigned long)expected_vls->vls_max);

    return (expected_vls->vls_magic == actual_vls->vls_magic
	    && bu_vls_strcmp(expected_vls, actual_vls) == 0);
}


static int
test_bu_vls_init(void)
{
    struct bu_vls expected_vls = { BU_VLS_MAGIC, (char *)0, 0, 0, 0 };
    struct bu_vls actual_vls;

    bu_vls_init(&actual_vls);

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true
     */
    return !compare_vls(&actual_vls, &expected_vls);
}


static int
test_bu_vls_vlsinit(void)
{
    struct bu_vls expected_vls = { BU_VLS_MAGIC, (char *)0, 0, 0, 0 };
    struct bu_vls *actual_vls;
    int retval;

    actual_vls = bu_vls_vlsinit();

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true
     */
    retval = !compare_vls(actual_vls, &expected_vls);

    bu_vls_free(actual_vls);

    return retval;
}


static int
test_bu_vls_access(int argc, char *argv[])
{
    char *null_expected_string = '\0';
    char *null_actual_string;

    char *set_expected_string;
    char *set_actual_addr_string;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int retval;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is string_to_test [%s]\n", argv[0]);
    }

    set_expected_string = argv[2];

    null_actual_string = bu_vls_addr(&vls);
    bu_vls_strcpy(&vls, set_expected_string);
    set_actual_addr_string = bu_vls_addr(&vls);

    printf("Actual null: %s\n", null_actual_string);
    printf("Actual addr_string (set): %s\n", set_actual_addr_string);

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true
     */
    retval = !(bu_strcmp(null_actual_string, null_expected_string) == 0
	       && bu_strcmp(set_actual_addr_string, set_expected_string) == 0);

    bu_vls_free(&vls);

    return retval;
}


static int
test_bu_vls_strncpy(int argc, char *argv[])
{
    char *expected_result_string;
    char *actual_result_string;
    int actual_result_len;
    char *string_orig;
    char *string_new;
    int n;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 6) {
	bu_exit(1, "ERROR: input format is string_orig string_new n expected_result [%s]\n", argv[0]);
    }

    string_orig = argv[2];
    string_new = argv[3];
    sscanf(argv[4], "%d", &n);
    expected_result_string = argv[5];

    bu_vls_strcpy(&vls, string_orig);
    bu_vls_strncpy(&vls, string_new, n);

    actual_result_string = bu_vls_strdup(&vls);
    actual_result_len = bu_vls_strlen(&vls);

    printf("Result: %s\n", actual_result_string);
    printf("Result len: %d\n", actual_result_len);

    bu_vls_free(&vls);

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true
     */
    return !(bu_strcmp(actual_result_string, expected_result_string) == 0
	     && actual_result_len == n);
}


static int
test_bu_vls_strdup(int argc, char *argv[])
{
    char *set_expected_string;
    char *set_actual_strdup_string;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int retval;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is string_to_test [%s]\n", argv[0]);
    }

    set_expected_string = argv[2];

    bu_vls_strcpy(&vls, set_expected_string);
    set_actual_strdup_string = bu_vls_strdup(&vls);

    printf("Actual strdup_string (before free): %s\n", set_actual_strdup_string);
    retval = bu_strcmp(set_actual_strdup_string, set_expected_string) == 0;

    bu_vls_free(&vls);
    printf("Actual strdup_string (after free): %s\n", set_actual_strdup_string);
    retval = retval && bu_strcmp(set_actual_strdup_string, set_expected_string) == 0;

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true
     */
    return !retval;
}


static int
test_bu_vls_strlen(int argc, char *argv[])
{
    char *string;
    int expected_length;
    int actual_length;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is string_to_test [%s]\n", argv[0]);
    }

    string = argv[2];
    expected_length = strlen(string);

    bu_vls_strcpy(&vls, string);
    actual_length = bu_vls_strlen(&vls);

    bu_vls_free(&vls);

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true
     */
    return !(expected_length == actual_length);
}


static int
test_bu_vls_trunc(int argc, char *argv[])
{
    char *in_string;
    int trunc_len;
    char *expected_out_string;
    char *actual_out_string;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is string_to_test trunc_len expected_result[%s]\n", argv[0]);
    }

    in_string = argv[2];
    sscanf(argv[3], "%d", &trunc_len);
    expected_out_string = argv[4];

    bu_vls_strcpy(&vls, in_string);
    bu_vls_trunc(&vls, trunc_len);
    actual_out_string = bu_vls_strdup(&vls);

    printf("trunc_len: %d\n", trunc_len);
    printf("Result: %s\n", actual_out_string);

    bu_vls_free(&vls);

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true
     */
    return !(bu_strcmp(actual_out_string, expected_out_string) == 0);
}


static int
test_bu_vls_nibble(int argc, char *argv[])
{
    char *in_string;
    int nibble_len;
    char *expected_out_string;
    char *actual_out_string;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is string_to_test nibble_len expected_result[%s]\n", argv[0]);
    }

    in_string = argv[2];
    sscanf(argv[3], "%d", &nibble_len);
    expected_out_string = argv[4];

    bu_vls_strcpy(&vls, in_string);
    bu_vls_nibble(&vls, nibble_len);
    actual_out_string = bu_vls_strdup(&vls);

    printf("Result: %s\n", actual_out_string);

    bu_vls_free(&vls);

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true
     */
    return !(bu_strcmp(actual_out_string, expected_out_string) == 0);
}


static int
test_bu_vls_strcat(int argc, char *argv[])
{
    char *in_string_1;
    char *in_string_2;
    char *expected_out_string;
    char *actual_out_string;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc!= 5) {
	bu_exit(1, "ERROR: input format is string1 string2 expected_result [%s]\n", argv[0]);
    }

    in_string_1 = argv[2];
    in_string_2 = argv[3];
    expected_out_string = argv[4];

    bu_vls_strcpy(&vls, in_string_1);
    bu_vls_strcat(&vls, in_string_2);
    actual_out_string = bu_vls_strdup(&vls);

    printf("Result: %s\n", actual_out_string);

    bu_vls_free(&vls);

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true
     */
    return !(bu_strcmp(actual_out_string, expected_out_string) == 0);
}


static int
test_bu_vls_strncat(int argc, char *argv[])
{
    /* argv[1] is the function number, ignored */
    char *in_string_1;         /* argv[2] */
    char *in_string_2;         /* argv[3] */
    int n;                     /* argv[4] */
    char *expected_out_string; /* argv[5] */

    int test_results = CTEST_FAIL;
    struct bu_vls *actual_vls;
    size_t narg;

    if (argc != 6) {
	bu_exit(1, "ERROR: input format is string1 string2 n expected_result [%s]\n", argv[0]);
    }

    in_string_1 = argv[2];
    in_string_2 = argv[3];
    bu_sscanf(argv[4], "%d", &n);
    expected_out_string = argv[5];

    narg = (size_t)n;
    actual_vls = bu_vls_vlsinit();
    bu_vls_strcpy(actual_vls, in_string_1);
    bu_vls_strncat(actual_vls, in_string_2, n);

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true
     */
    if (!bu_strcmp(bu_vls_cstr(actual_vls), expected_out_string)) {
	printf("PASSED Input1: '%s' Input2: '%s' n: %d narg: %u Output: '%s' Expected: '%s'",
	       in_string_1, in_string_2, n, (unsigned int)narg, bu_vls_cstr(actual_vls), expected_out_string);
	test_results = CTEST_PASS;
    }
    else {
	printf("FAILED Input1: '%s' Input2: '%s' n: %d narg: %u Output: '%s' Expected: '%s'",
	       in_string_1, in_string_2, n, (unsigned int)narg, bu_vls_cstr(actual_vls), expected_out_string);
	test_results = CTEST_FAIL;
    }

    bu_vls_free(actual_vls);

    return test_results;
}


static int
test_bu_vls_vlscat(int argc, char *argv[])
{
    char *in_string_1;
    char *in_string_2;
    char *expected_out_string;
    char *actual_out_string;
    struct bu_vls vls_1 = BU_VLS_INIT_ZERO;
    struct bu_vls vls_2 = BU_VLS_INIT_ZERO;

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is string1 string2 expected_result [%s]\n", argv[0]);
    }

    in_string_1 = argv[2];
    in_string_2 = argv[3];
    expected_out_string = argv[4];

    bu_vls_strcpy(&vls_1, in_string_1);
    bu_vls_strcpy(&vls_2, in_string_2);
    bu_vls_vlscat(&vls_1, &vls_2);
    actual_out_string = bu_vls_strdup(&vls_1);

    printf("Result: %s\n", actual_out_string);

    bu_vls_free(&vls_1);
    bu_vls_free(&vls_2);

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true
     */
    return !(bu_strcmp(actual_out_string, expected_out_string) == 0);
}


static int
test_bu_vls_strcmp(int argc, char *argv[])
{
    char *in_string_1;
    char *in_string_2;
    int expected_result;
    int actual_result;
    struct bu_vls vls_1 = BU_VLS_INIT_ZERO;
    struct bu_vls vls_2 = BU_VLS_INIT_ZERO;

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is string1 string2 expected_result [%s]\n", argv[0]);
    }

    in_string_1 = argv[2];
    in_string_2 = argv[3];
    sscanf(argv[4], "%d", &expected_result);

    bu_vls_strcpy(&vls_1, in_string_1);
    bu_vls_strcpy(&vls_2, in_string_2);
    actual_result = bu_vls_strcmp(&vls_1, &vls_2);

    printf("Result: %d\n", actual_result);

    bu_vls_free(&vls_1);
    bu_vls_free(&vls_2);

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true; the condition just checks
     * that the expected and actual results are on the same side of 0;
     * it currently expects expected_result to be on of -1, 0, or
     * 1.
     */
    return !(abs(actual_result) == expected_result*actual_result);
}


static int
test_bu_vls_strncmp(int argc, char *argv[])
{
    char *in_string_1;
    char *in_string_2;
    int n;
    int expected_result;
    int actual_result;
    struct bu_vls vls_1 = BU_VLS_INIT_ZERO;
    struct bu_vls vls_2 = BU_VLS_INIT_ZERO;

    if (argc != 6) {
	bu_exit(1, "ERROR: input format is string1 string2 n expected_result [%s]\n", argv[0]);
    }

    in_string_1 = argv[2];
    in_string_2 = argv[3];
    sscanf(argv[4], "%d", &n);
    sscanf(argv[5], "%d", &expected_result);

    bu_vls_strcpy(&vls_1, in_string_1);
    bu_vls_strcpy(&vls_2, in_string_2);
    actual_result = bu_vls_strncmp(&vls_1, &vls_2, n);

    printf("Result: %d\n", actual_result);

    bu_vls_free(&vls_1);
    bu_vls_free(&vls_2);

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true; the condition just checks
     * that the expected and actual results are on the same side of 0;
     * it currently expects expected_result to be on of -1, 0, or
     * 1.
     */
    return !(abs(actual_result) == expected_result*actual_result);
}


static int
test_bu_vls_from_argv(int argc, char *argv[])
{
    char *expected_result;
    char *actual_result;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc < 4) {
	bu_exit(1, "ERROR: input format is strings expected_result [%s]\n", argv[0]);
    }

    expected_result = argv[argc-1];

    bu_vls_from_argv(&vls, argc-3, (const char **)argv+2);
    actual_result = bu_vls_strdup(&vls);

    printf("Result: %s\n", actual_result);

    bu_vls_free(&vls);

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true.
     */
    return !(bu_strcmp(actual_result, expected_result) == 0);
}


static int
test_bu_vls_trimspace(int argc, char *argv[])
{
    char *in_string;
    char *expected_result;
    char *actual_result;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is string expected_result [%s]\n", argv[0]);
    }

    in_string = argv[2];
    expected_result = argv[3];

    bu_vls_strcpy(&vls, in_string);
    bu_vls_trimspace(&vls);

    actual_result = bu_vls_strdup(&vls);

    printf("Result: %s\n", actual_result);

    bu_vls_free(&vls);

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true.
     */
    return !(bu_strcmp(actual_result, expected_result) == 0);
}


static int
test_bu_vls_spaces(int argc, char *argv[])
{
    char *in_string;
    int num_spaces;
    char *expected_result;
    char *actual_result;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is string num_spaces expected_result [%s]\n", argv[0]);
    }

    in_string = argv[2];
    sscanf(argv[3], "%d", &num_spaces);
    expected_result = argv[4];

    bu_vls_strcpy(&vls, in_string);
    bu_vls_spaces(&vls, num_spaces);

    actual_result = bu_vls_strdup(&vls);

    printf("Result: %s\n", actual_result);

    bu_vls_free(&vls);

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true.
     */
    return !(bu_strcmp(actual_result, expected_result) == 0);
}


static int
test_bu_vls_detab(int argc, char *argv[])
{
    char *in_string;
    char *expected_result;
    char *actual_result;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is string expected_result [%s]\n", argv[0]);
    }

    in_string = argv[2];
    expected_result = argv[3];

    bu_vls_strcpy(&vls, in_string);
    bu_vls_detab(&vls);

    actual_result = bu_vls_strdup(&vls);

    printf("Result: %s\n", actual_result);

    bu_vls_free(&vls);

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true.
     */
    return !(bu_strcmp(actual_result, expected_result) == 0);
}


static int
test_bu_vls_prepend(int argc, char *argv[])
{
    char *in_string;
    char *prepend_string;
    char *expected_result;
    char *actual_result;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is string string_to_prepend expected_result[%s]\n", argv[0]);
    }

    in_string = argv[2];
    prepend_string=  argv[3];
    expected_result = argv[4];

    bu_vls_strcpy(&vls, in_string);
    bu_vls_prepend(&vls, prepend_string);

    actual_result = bu_vls_strdup(&vls);

    printf("Result: %s\n", actual_result);

    bu_vls_free(&vls);

    /* These functions need to return sh-style return values where
     * non-zero is false and zero is true.
     */
    return !(bu_strcmp(actual_result, expected_result) == 0);
}


int
main(int argc, char *argv[])
{
    int function_num = 0;

    if (argc < 2) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    sscanf(argv[1], "%d", &function_num);

    switch (function_num) {
	case 1:
	    /* We don't need any arguments here, as there is only one
	     * thing that bu_vls_init can do
	     */
	    return test_bu_vls_init();
	case 2:
	    /* We don't need any arguments here, as there is only one
	     * thing that bu_vls_init can do
	     */
	    return test_bu_vls_vlsinit();
	case 3:
	    return test_bu_vls_access(argc, argv);
	case 4:
	    return test_bu_vls_strncpy(argc, argv);
	case 5:
	    return test_bu_vls_strdup(argc, argv);
	case 6:
	    return test_bu_vls_strlen(argc, argv);
	case 7:
	    return test_bu_vls_trunc(argc, argv);
	case 8:
	    return 0; /* deprecation removal */
	case 9:
	    return test_bu_vls_nibble(argc, argv);
	case 10:
	    return test_bu_vls_strcat(argc, argv);
	case 11:
	    return test_bu_vls_strncat(argc, argv);
	case 12:
	    return test_bu_vls_vlscat(argc, argv);
	case 13:
	    return test_bu_vls_strcmp(argc, argv);
	case 14:
	    return test_bu_vls_strncmp(argc, argv);
	case 15:
	    return test_bu_vls_from_argv(argc, argv);
	case 16:
	    return test_bu_vls_trimspace(argc, argv);
	case 17:
	    return test_bu_vls_spaces(argc, argv);
	case 18:
	    return test_bu_vls_detab(argc, argv);
	case 19:
	    return test_bu_vls_prepend(argc, argv);
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

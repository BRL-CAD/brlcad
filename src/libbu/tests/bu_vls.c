/*                       B U _ V L S . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2013 United States Government as represented by
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

/* This prints out the values of the expected vls and compares it with
the actual vls; it uses the C-style return values of 0 for false and 1
for true */
static int compare_vls(struct bu_vls *actual_vls, struct bu_vls *expected_vls)
{
    bu_log("magic: %lu", (unsigned long)expected_vls->vls_magic);
    bu_log("str: %s", expected_vls->vls_str);
    bu_log("offset: %lu", (unsigned long)expected_vls->vls_offset);
    bu_log("len: %lu", (unsigned long)expected_vls->vls_len);
    bu_log("max: %lu", (unsigned long)expected_vls->vls_max);

    return (expected_vls->vls_magic == actual_vls->vls_magic
	    && bu_vls_strcmp(expected_vls, actual_vls) == 0);
}

static int test_bu_vls_init()
{
    struct bu_vls expected_vls = { BU_VLS_MAGIC, (char *)0, 0, 0, 0 };
    struct bu_vls actual_vls;

    bu_vls_init(&actual_vls);

    /* These functions need to return sh-style return values where
       non-zero is false and zero is true */
    return !compare_vls(&actual_vls, &expected_vls);
}

static int test_bu_vls_vlsinit()
{
    struct bu_vls expected_vls = { BU_VLS_MAGIC, (char *)0, 0, 0, 0 };
    struct bu_vls *actual_vls;
    int retval;

    actual_vls = bu_vls_vlsinit();

    /* These functions need to return sh-style return values where
       non-zero is false and zero is true */
    retval = !compare_vls(actual_vls, &expected_vls);

    bu_vls_vlsfree(actual_vls);

    return retval;
}

static int test_bu_vls_access(int argc, char *argv[])
{
    char *null_expected_string = '\0';
    char *null_actual_string;

    char *set_expected_string;
    char *set_actual_addr_string;
    struct bu_vls *vls;
    int retval;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is string_to_test [%s]\n", argv[0]);
    }

    set_expected_string = argv[2];

    vls = bu_vls_vlsinit();

    null_actual_string = bu_vls_addr(vls);
    bu_vls_strcpy(vls, set_expected_string);
    set_actual_addr_string = bu_vls_addr(vls);

    printf("Actual null: %s\n", null_actual_string);
    printf("Actual addr_string (set): %s\n", set_actual_addr_string);

    /* These functions need to return sh-style return values where
       non-zero is false and zero is true */
    retval = !(bu_strcmp(null_actual_string, null_expected_string) == 0
	       && bu_strcmp(set_actual_addr_string, set_expected_string) == 0);

    bu_vls_vlsfree(vls);

    return retval;
}

static int test_bu_vls_strdup(int argc, char *argv[])
{
    char *set_expected_string;
    char *set_actual_strdup_string;
    struct bu_vls *vls;
    int retval;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is string_to_test [%s]\n", argv[0]);
    }

    set_expected_string = argv[2];

    vls = bu_vls_vlsinit();

    bu_vls_strcpy(vls, set_expected_string);
    set_actual_strdup_string = bu_vls_strdup(vls);

    printf("Actual strdup_string (before free): %s\n", set_actual_strdup_string);
    retval = bu_strcmp(set_actual_strdup_string, set_expected_string) == 0;

    bu_vls_vlsfree(vls);
    printf("Actual strdup_string (after free): %s\n", set_actual_strdup_string);
    retval = retval && bu_strcmp(set_actual_strdup_string, set_expected_string) == 0;

    /* These functions need to return sh-style return values where
       non-zero is false and zero is true */
    return !retval;
}

static int test_bu_vls_strlen(int argc, char *argv[])
{
    char *string;
    int expected_length;
    int actual_length;
    struct bu_vls *vls;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is string_to_test [%s]\n", argv[0]);
    }

    string = argv[2];
    expected_length = strlen(string);

    vls = bu_vls_vlsinit();

    bu_vls_strcpy(vls, string);
    actual_length = bu_vls_strlen(vls);

    bu_vls_vlsfree(vls);

    /* These functions need to return sh-style return values where
       non-zero is false and zero is true */
    return !(expected_length == actual_length);
}

static int test_bu_vls_trunc(int argc, char *argv[])
{
    char *in_string;
    int trunc_len;
    char *expected_out_string;
    char *actual_out_string;
    struct bu_vls *vls;

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is string_to_test trunc_len expected_result[%s]\n", argv[0]);
    }

    in_string = argv[2];
    sscanf(argv[3], "%d", &trunc_len);
    expected_out_string = argv[4];

    vls = bu_vls_vlsinit();

    bu_vls_strcpy(vls, in_string);
    bu_vls_trunc(vls, trunc_len);
    actual_out_string = bu_vls_strdup(vls);

    printf("trunc_len: %d\n", trunc_len);
    printf("Result: %s\n", actual_out_string);

    bu_vls_vlsfree(vls);

    /* These functions need to return sh-style return values where
       non-zero is false and zero is true */
    return !(bu_strcmp(actual_out_string, expected_out_string) == 0);
}

static int test_bu_vls_trunc2(int argc, char *argv[])
{
    char *in_string;
    int trunc_len;
    char *expected_out_string;
    char *actual_out_string;
    struct bu_vls *vls;

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is string_to_test trunc_len expected_result[%s]\n", argv[0]);
    }

    in_string = argv[2];
    sscanf(argv[3], "%d", &trunc_len);
    expected_out_string = argv[4];

    vls = bu_vls_vlsinit();

    bu_vls_strcpy(vls, in_string);
    bu_vls_trunc2(vls, trunc_len);
    actual_out_string = bu_vls_strdup(vls);

    printf("Result: %s\n", actual_out_string);

    bu_vls_vlsfree(vls);

    /* These functions need to return sh-style return values where
       non-zero is false and zero is true */
    return !(bu_strcmp(actual_out_string, expected_out_string) == 0);
}

static int test_bu_vls_nibble(int argc, char *argv[])
{
    char *in_string;
    int nibble_len;
    char *expected_out_string;
    char *actual_out_string;
    struct bu_vls *vls;

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is string_to_test nibble_len expected_result[%s]\n", argv[0]);
    }

    in_string = argv[2];
    sscanf(argv[3], "%d", &nibble_len);
    expected_out_string = argv[4];

    vls = bu_vls_vlsinit();

    bu_vls_strcpy(vls, in_string);
    bu_vls_nibble(vls, nibble_len);
    actual_out_string = bu_vls_strdup(vls);

    printf("Result: %s\n", actual_out_string);

    bu_vls_vlsfree(vls);

    /* These functions need to return sh-style return values where
       non-zero is false and zero is true */
    return !(bu_strcmp(actual_out_string, expected_out_string) == 0);
}

static int test_bu_vls_strcat(int argc, char *argv[])
{
    char *in_string_1;
    char *in_string_2;
    char *expected_out_string;
    char *actual_out_string;
    struct bu_vls *vls;

    if (argc!= 5) {
	bu_exit(1, "ERROR: input format is string1 string2 expected_result [%s]\n", argv[0]);
    }

    in_string_1 = argv[2];
    in_string_2 = argv[3];
    expected_out_string = argv[4];

    vls = bu_vls_vlsinit();

    bu_vls_strcpy(vls, in_string_1);
    bu_vls_strcat(vls, in_string_2);
    actual_out_string = bu_vls_strdup(vls);

    printf("Result: %s\n", actual_out_string);

    bu_vls_vlsfree(vls);

    /* These functions need to return sh-style return values where
       non-zero is false and zero is true */
    return !(bu_strcmp(actual_out_string, expected_out_string) == 0);
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
	       thing that bu_vls_init can do */
	    return test_bu_vls_init();
	case 2:
	    /* We don't need any arguments here, as there is only one
	       thing that bu_vls_init can do */
	    return test_bu_vls_vlsinit();
	case 3:
	    return test_bu_vls_access(argc, argv);
	case 4:
	    return test_bu_vls_strdup(argc, argv);
	case 5:
	    return test_bu_vls_strlen(argc, argv);
	case 6:
	    return test_bu_vls_trunc(argc, argv);
	case 7:
	    return test_bu_vls_trunc2(argc, argv);
	case 8:
	    return test_bu_vls_nibble(argc, argv);
	case 9:
	    return test_bu_vls_strcat(argc, argv);
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

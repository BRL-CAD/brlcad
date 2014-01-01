/*                      B N _ T A B D A T A . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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

#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"

static int
tab_equal(const struct bn_table *a, const struct bn_table *b)
{
    unsigned int i;

    if (a->nx != b->nx) {
	return 1;
    }

    for (i = 0; i < a->nx; i++) {
	if (!NEAR_EQUAL(a->x[i], b->x[i], BN_TOL_DIST)) {
	    return 0;
	}
    }

    return 1;
}


static int
tabdata_equal(const struct bn_tabdata *a, const struct bn_tabdata *b)
{
    unsigned int i;

    if (a->ny != b->ny) {
	return 1;
    }

    for (i = 0; i < a->ny; i++) {
	if (!NEAR_EQUAL(a->y[i], b->y[i], BN_TOL_DIST)) {
	    return 0;
	}
    }

    return 1;
}

/* This function expects the string that is passed in to hold a table
 * of the form nx,x0,x1,x2,...,x(nx) (note: there are nx+1 items after nx).
 */
static void
scan_tab_args(char *in, struct bn_table **tab_in)
{
    int i;
    int nx;
    int offset;
    int new_offset;
    struct bn_table *tab;

    sscanf(in, "%d%n", &nx, &offset);
    new_offset = offset;

    BN_GET_TABLE(tab, nx);

    for (i = 0; i <= nx; i++) {
	sscanf(in+offset, ",%lg%n", &((tab->x)[i]), &new_offset);
	offset += new_offset;
    }
    *tab_in = tab;
}

/* This function expects the string that is passed in to hold a tabdata
 * of the form y0,y1,y2,...,y(nx-1). The number of ys is determined by
 * the table argument's nx.
 */
static void
scan_tabdata_args(char *in, struct bn_tabdata **tabdata_in, struct bn_table *table)
{
    int i;
    struct bn_tabdata *tabdata;
    int new_offset;
    int offset;

    BN_GET_TABDATA(tabdata, table);

    sscanf(in, "%lg%n", &((tabdata->y)[0]), &offset);
    for (i = 1; i <= (int)tabdata->ny; i++) {
	sscanf(in+offset, ",%lg%n", &((tabdata->y)[i]), &new_offset);
	offset += new_offset;
    }
    *tabdata_in = tabdata;
}

static int
test_bn_table_make_uniform(int argc, char *argv[])
{
    struct bn_table *expected;
    struct bn_table *actual;
    double first, last;
    unsigned int num;

    if (argc != 6) {
	bu_exit(1, "<args> format: first last num expected [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf", &first);
    sscanf(argv[3], "%lf", &last);
    sscanf(argv[4], "%u", &num);
    scan_tab_args(argv[5], &expected);

    actual = bn_table_make_uniform(num, first, last);

    bn_pr_table("Result", actual);

    return !tab_equal(expected, actual);
}

static int
test_bn_tabdata_add(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td_in1;
    struct bn_tabdata *td_in2;
    struct bn_tabdata *expected;
    struct bn_tabdata *actual;

    if (argc != 6) {
	bu_exit(1, "<args> format: table tabdata1 tabdata2 expected_tabdata [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &td_in1, tab_in);
    scan_tabdata_args(argv[4], &td_in2, tab_in);
    scan_tabdata_args(argv[5], &expected, tab_in);
    BN_GET_TABDATA(actual, tab_in);

    bn_tabdata_add(actual, td_in1, td_in2);

    bn_pr_tabdata("Result", actual);

    return !tabdata_equal(expected, actual);
}

static int
test_bn_tabdata_mul(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td_in1;
    struct bn_tabdata *td_in2;
    struct bn_tabdata *expected;
    struct bn_tabdata *actual;

    if (argc != 6) {
	bu_exit(1, "<args> format: table tabdata1 tabdata2 expected_tabdata [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &td_in1, tab_in);
    scan_tabdata_args(argv[4], &td_in2, tab_in);
    scan_tabdata_args(argv[5], &expected, tab_in);
    BN_GET_TABDATA(actual, tab_in);

    bn_tabdata_mul(actual, td_in1, td_in2);

    bn_pr_tabdata("Result", actual);

    return !tabdata_equal(expected, actual);
}

static int
test_bn_tabdata_mul3(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td_in1;
    struct bn_tabdata *td_in2;
    struct bn_tabdata *td_in3;
    struct bn_tabdata *expected;
    struct bn_tabdata *actual;

    if (argc != 7) {
	bu_exit(1, "<args> format: table tabdata1 tabdata2 tabdata3 expected_tabdata [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &td_in1, tab_in);
    scan_tabdata_args(argv[4], &td_in2, tab_in);
    scan_tabdata_args(argv[5], &td_in3, tab_in);
    scan_tabdata_args(argv[6], &expected, tab_in);
    BN_GET_TABDATA(actual, tab_in);

    bn_tabdata_mul3(actual, td_in1, td_in2, td_in3);

    bn_pr_tabdata("Result", actual);

    return !tabdata_equal(expected, actual);
}

static int
test_bn_tabdata_incr_mul3_scale(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td_in1;
    struct bn_tabdata *td_in2;
    struct bn_tabdata *td_in3;
    struct bn_tabdata *expected;
    struct bn_tabdata *actual;
    fastf_t scale;

    if (argc != 9) {
	bu_exit(1, "<args> format: table tabdataout_orig tabdata1 tabdata2 tabdata3 scale expected_tabdata [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &actual, tab_in);
    scan_tabdata_args(argv[4], &td_in1, tab_in);
    scan_tabdata_args(argv[5], &td_in2, tab_in);
    scan_tabdata_args(argv[6], &td_in3, tab_in);
    sscanf(argv[7], "%lg", &scale);
    scan_tabdata_args(argv[8], &expected, tab_in);

    bn_tabdata_incr_mul3_scale(actual, td_in1, td_in2, td_in3, scale);

    bn_pr_tabdata("Result", actual);

    return !tabdata_equal(expected, actual);
}

static int
test_bn_tabdata_incr_mul2_scale(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td_in1;
    struct bn_tabdata *td_in2;
    struct bn_tabdata *expected;
    struct bn_tabdata *actual;
    fastf_t scale;

    if (argc != 8) {
	bu_exit(1, "<args> format: table tabdataout_orig tabdata1 tabdata2 scale expected_tabdata [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &actual, tab_in);
    scan_tabdata_args(argv[4], &td_in1, tab_in);
    scan_tabdata_args(argv[5], &td_in2, tab_in);
    sscanf(argv[6], "%lg", &scale);
    scan_tabdata_args(argv[7], &expected, tab_in);

    bn_tabdata_incr_mul2_scale(actual, td_in1, td_in2, scale);

    bn_pr_tabdata("Result", actual);

    return !tabdata_equal(expected, actual);
}

static int
test_bn_tabdata_scale(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td;
    struct bn_tabdata *expected;
    struct bn_tabdata *actual;
    fastf_t scale;

    if (argc != 6) {
	bu_exit(1, "<args> format: table tabdata scale expected_tabdata [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &td, tab_in);
    sscanf(argv[4], "%lg", &scale);
    scan_tabdata_args(argv[5], &expected, tab_in);
    BN_GET_TABDATA(actual, tab_in);

    bn_tabdata_scale(actual, td, scale);

    bn_pr_tabdata("Result", actual);

    return !tabdata_equal(expected, actual);
}

static int
test_bn_table_scale(int argc, char *argv[])
{
    struct bn_table *expected;
    struct bn_table *actual;
    fastf_t scale;

    if (argc != 5) {
	bu_exit(1, "<args> format: table scale expected_tabdata [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &actual);
    sscanf(argv[3], "%lg", &scale);
    scan_tab_args(argv[4], &expected);

    bn_table_scale(actual, scale);

    bn_pr_table("Result", actual);

    return !tab_equal(expected, actual);
}

static int
test_bn_tabdata_join1(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td_in1;
    struct bn_tabdata *td_in2;
    struct bn_tabdata *expected;
    struct bn_tabdata *actual;
    fastf_t scale;

    if (argc != 7) {
	bu_exit(1, "<args> format: table tabdata1 tabdata2 scale expected_tabdata [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &td_in1, tab_in);
    scan_tabdata_args(argv[4], &td_in2, tab_in);
    sscanf(argv[5], "%lg", &scale);
    scan_tabdata_args(argv[6], &expected, tab_in);
    BN_GET_TABDATA(actual, tab_in);

    bn_tabdata_join1(actual, td_in1, scale, td_in2);

    bn_pr_tabdata("Result", actual);

    return !tabdata_equal(expected, actual);
}

static int
test_bn_tabdata_join2(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td_in1;
    struct bn_tabdata *td_in2;
    struct bn_tabdata *td_in3;
    struct bn_tabdata *expected;
    struct bn_tabdata *actual;
    fastf_t scale_1, scale_2;

    if (argc != 9) {
	bu_exit(1, "<args> format: table tabdata1 tabdata2 tabdata3 scale1 scale2 scale3 expected_tabdata [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &td_in1, tab_in);
    scan_tabdata_args(argv[4], &td_in2, tab_in);
    scan_tabdata_args(argv[5], &td_in3, tab_in);
    sscanf(argv[6], "%lg", &scale_1);
    sscanf(argv[7], "%lg", &scale_2);
    scan_tabdata_args(argv[8], &expected, tab_in);
    BN_GET_TABDATA(actual, tab_in);

    bn_tabdata_join2(actual, td_in1, scale_1, td_in2, scale_2, td_in3);

    bn_pr_tabdata("Result", actual);

    return !tabdata_equal(expected, actual);
}

static int
test_bn_tabdata_blend2(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td_in1;
    struct bn_tabdata *td_in2;
    struct bn_tabdata *expected;
    struct bn_tabdata *actual;
    fastf_t scale_1, scale_2;

    if (argc != 8) {
	bu_exit(1, "<args> format: table tabdata1 tabdata2 scale1 scale2 expected_result [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &td_in1, tab_in);
    scan_tabdata_args(argv[4], &td_in2, tab_in);
    sscanf(argv[5], "%lg", &scale_1);
    sscanf(argv[6], "%lg", &scale_2);
    scan_tabdata_args(argv[7], &expected, tab_in);
    BN_GET_TABDATA(actual, tab_in);

    bn_tabdata_blend2(actual, scale_1, td_in1, scale_2, td_in2);

    bn_pr_tabdata("Result", actual);

    return !tabdata_equal(expected, actual);
}

static int
test_bn_tabdata_blend3(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td_in1;
    struct bn_tabdata *td_in2;
    struct bn_tabdata *td_in3;
    struct bn_tabdata *expected;
    struct bn_tabdata *actual;
    fastf_t scale_1, scale_2, scale_3;

    if (argc != 10) {
	bu_exit(1, "<args> format: table tabdata1 tabdata2 tabdata3 scale1 scale2 scale3 expected_result [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &td_in1, tab_in);
    scan_tabdata_args(argv[4], &td_in2, tab_in);
    scan_tabdata_args(argv[5], &td_in3, tab_in);
    sscanf(argv[6], "%lg", &scale_1);
    sscanf(argv[7], "%lg", &scale_2);
    sscanf(argv[8], "%lg", &scale_3);
    scan_tabdata_args(argv[9], &expected, tab_in);
    BN_GET_TABDATA(actual, tab_in);

    bn_tabdata_blend3(actual, scale_1, td_in1, scale_2, td_in2, scale_3, td_in3);

    bn_pr_tabdata("Result", actual);

    return !tabdata_equal(expected, actual);
}

static int
test_bn_tabdata_area1(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td_in;
    fastf_t expected, actual;

    if (argc != 5) {
	bu_exit(1, "<args> format: table tabdata expected_result [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &td_in, tab_in);
    sscanf(argv[4], "%lg", &expected);

    actual = bn_tabdata_area1(td_in);

    bu_log("Result: %g\n", actual);

    return !NEAR_EQUAL(expected, actual, BN_TOL_DIST);
}

static int
test_bn_tabdata_area2(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td_in;
    fastf_t expected, actual;

    if (argc != 5) {
	bu_exit(1, "<args> format: table tabdata expected_result [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &td_in, tab_in);
    sscanf(argv[4], "%lg", &expected);

    actual = bn_tabdata_area2(td_in);

    bu_log("Result: %g\n", actual);

    return !NEAR_EQUAL(expected, actual, BN_TOL_DIST);
}

static int
test_bn_tabdata_mul_area1(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td_in1;
    struct bn_tabdata *td_in2;
    fastf_t expected, actual;

    if (argc != 6) {
	bu_exit(1, "<args> format: table tabdata1 tabdata2 expected_result [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &td_in1, tab_in);
    scan_tabdata_args(argv[4], &td_in2, tab_in);
    sscanf(argv[5], "%lg", &expected);

    actual = bn_tabdata_mul_area1(td_in1, td_in2);

    bu_log("Result: %g\n", actual);

    return !NEAR_EQUAL(expected, actual, BN_TOL_DIST);
}

static int
test_bn_tabdata_mul_area2(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td_in1;
    struct bn_tabdata *td_in2;
    fastf_t expected, actual;

    if (argc != 6) {
	bu_exit(1, "<args> format: table tabdata1 tabdata2 expected_result [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &td_in1, tab_in);
    scan_tabdata_args(argv[4], &td_in2, tab_in);
    sscanf(argv[5], "%lg", &expected);

    actual = bn_tabdata_mul_area2(td_in1, td_in2);

    bu_log("Result: %g\n", actual);

    return !NEAR_EQUAL(expected, actual, BN_TOL_DIST);
}

static int
test_bn_table_lin_interp(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td_in;
    fastf_t wl;
    fastf_t expected, actual;

    if (argc != 6) {
	bu_exit(1, "<args> format: table tabdata wl expected_result [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &td_in, tab_in);
    sscanf(argv[4], "%lg", &wl);
    sscanf(argv[5], "%lg", &expected);

    actual = bn_table_lin_interp(td_in, wl);

    bu_log("Result: %g\n", actual);

    return !NEAR_EQUAL(expected, actual, BN_TOL_DIST);
}

static int
test_bn_tabdata_copy(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td_in;
    struct bn_tabdata *actual;

    if (argc != 4) {
	bu_exit(1, "<args> format: table tabdata [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &td_in, tab_in);
    BN_GET_TABDATA(actual, tab_in);

    bn_tabdata_copy(actual, td_in);

    bn_pr_tabdata("Result", actual);

    return !tabdata_equal(td_in, actual);
}

static int
test_bn_tabdata_dup(int argc, char *argv[])
{
    struct bn_table *tab_in;
    struct bn_tabdata *td_in;
    struct bn_tabdata *actual;

    if (argc != 4) {
	bu_exit(1, "<args> format: table tabdata [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    scan_tabdata_args(argv[3], &td_in, tab_in);

    actual = bn_tabdata_dup(td_in);

    bn_pr_tabdata("Result", actual);

    return !tabdata_equal(td_in, actual);
}

static int
test_bn_tabdata_get_constval(int argc, char *argv[])
{
    struct bn_table *tab_in;
    fastf_t val;
    struct bn_tabdata *expected;    struct bn_tabdata *actual;

    if (argc != 5) {
	bu_exit(1, "<args> format: table val expected_tabdata [%s]\n", argv[0]);
    }

    scan_tab_args(argv[2], &tab_in);
    sscanf(argv[3], "%lg", &val);
    scan_tabdata_args(argv[4], &expected, tab_in);

    actual = bn_tabdata_get_constval(val, tab_in);

    bn_pr_tabdata("Result", actual);

    return !tabdata_equal(expected, actual);
}


int
main(int argc, char *argv[])
{
    int function_num = 0;

    if (argc < 3) {
	bu_exit(1, "Argument format: <function_number> <args> [%s]\n", argv[0]);
    }

    sscanf(argv[1], "%d", &function_num);

    switch (function_num) {
    case 1:
	return test_bn_table_make_uniform(argc, argv);
    case 2:
	return test_bn_tabdata_add(argc, argv);
    case 3:
	return test_bn_tabdata_mul(argc, argv);
    case 4:
	return test_bn_tabdata_mul3(argc, argv);
    case 5:
	return test_bn_tabdata_incr_mul3_scale(argc, argv);
    case 6:
	return test_bn_tabdata_incr_mul2_scale(argc, argv);
    case 7:
	return test_bn_tabdata_scale(argc, argv);
    case 8:
	return test_bn_table_scale(argc, argv);
    case 9:
	return test_bn_tabdata_join1(argc, argv);
    case 10:
	return test_bn_tabdata_join2(argc, argv);
    case 11:
	return test_bn_tabdata_blend2(argc, argv);
    case 12:
	return test_bn_tabdata_blend3(argc, argv);
    case 13:
	return test_bn_tabdata_area1(argc, argv);
    case 14:
	return test_bn_tabdata_area2(argc, argv);
    case 15:
	return test_bn_tabdata_mul_area1(argc, argv);
    case 16:
	return test_bn_tabdata_mul_area2(argc, argv);
    case 17:
	return test_bn_table_lin_interp(argc, argv);
    case 18:
	return test_bn_tabdata_copy(argc, argv);
    case 19:
	return test_bn_tabdata_dup(argc, argv);
    case 20:
	return test_bn_tabdata_get_constval(argc, argv);
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

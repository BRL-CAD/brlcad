/*                  L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2026 United States Government as represented by
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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "bn.h"
#include "bv/plot3.h"

#define BUFFER_SIZE 2000
#define MAX_POINTS 30

/*
 * Helper functions
 */

/* Initialises the buffers and opens a memory file for the plot output */
FILE *
initialise_buffers(char *expected_buf)
{
    FILE *temp;

    if (expected_buf)
	memset(expected_buf, 0, BUFFER_SIZE);

    temp = bu_temp_file(NULL, 0);

    if (temp == NULL)
	return 0;

    rewind(temp);

    return temp;
}

/* Checks the result against the expected and closes the result fd */
int
compare_result(char *expected_buf, FILE *result_fd)
{
    size_t ret;
    char result_buf[BUFFER_SIZE];
    memset(result_buf, 0, BUFFER_SIZE);

    rewind(result_fd);
    errno = 0;
    ret = fread(result_buf, sizeof(char), BUFFER_SIZE, result_fd);
    if (ret < BUFFER_SIZE)
	perror("fread");

    fclose(result_fd);

    return bu_strncmp(expected_buf, result_buf, BUFFER_SIZE) == 0;
}

/* Checks the result length and closes the result fd */
static size_t
check_result_len(FILE *result_fd)
{
    size_t ret;
    char result_buf[BUFFER_SIZE+1];

    rewind(result_fd);
    ret = fread(result_buf, sizeof(char), BUFFER_SIZE, result_fd);
    if (ret < BUFFER_SIZE)
	perror("fread");
    fclose(result_fd);

    return ret;
}

/* Converts an array of doubles to integers */
void
convert_points(double *d_values, int *i_values, int count)
{
    while (--count > 0) {
	*i_values++ = (int) *d_values++;
    }
}


/*
 * Tests
 */

/* Produces the expected output of pl_list based on the input */
void
make_tp_i2list_expected(char *buf, int buflen, int *x, int *y, int npoints)
{
    int chars_written;

    if (npoints <= 0)
	return;

    chars_written = snprintf(buf, buflen, "m %d %d\n", *x++, *y++);
    buf += chars_written;
    buflen -= chars_written;

    while (--npoints > 0) {
	chars_written = snprintf(buf, buflen, "n %d %d\n", *x++, *y++);
	buf += chars_written;
	buflen -= chars_written;
    }
}

/* This test ensures the pl_list function produces the output it's supposed to */
int
test_tp_i2list(int *x, int *y, int npoints)
{
    char expected_buf[BUFFER_SIZE];
    FILE *buf_out = initialise_buffers(expected_buf);

    if (!buf_out) {
	printf("File Invalid\n");
	return 0;
    }

    make_tp_i2list_expected(expected_buf, BUFFER_SIZE, x, y, npoints);

    /* Set plot output to human readable */
    pl_setOutputMode(PL_OUTPUT_MODE_TEXT);

    /* Plot the points */
    pl_list(buf_out, x, y, npoints);

    return compare_result(expected_buf, buf_out);
}


/* Produces the expected output of pd_list based on the input */
void
make_tp_2list_expected(char *buf, int buflen, double *x, double *y, int npoints)
{
    int chars_written;

    if (npoints <= 0)
	return;

    chars_written = snprintf(buf, buflen, "o %g %g\n", *x++, *y++);
    buf += chars_written;
    buflen -= chars_written;

    while (--npoints > 0) {
	chars_written = snprintf(buf, buflen, "q %g %g\n", *x++, *y++);
	buf += chars_written;
	buflen -= chars_written;
    }
}

/* This test ensures the pd_list function produces the output it's supposed to */
int
test_tp_2list(double *x, double *y, int npoints)
{
    char expected_buf[BUFFER_SIZE];
    FILE *buf_out = initialise_buffers(expected_buf);

    if (!buf_out) {
	printf("File Invalid\n");
	return 0;
    }

    make_tp_2list_expected(expected_buf, BUFFER_SIZE, x, y, npoints);

    pl_setOutputMode(PL_OUTPUT_MODE_TEXT);
    pd_list(buf_out, x, y, npoints);

    return compare_result(expected_buf, buf_out);
}


/*
 * This test tests the pd_marked_list marker function to ensure it
 *  doesn't have any SEGFAULTs when invalid values are put in, and
 *  produces output when required and not when none is required
 */
int
test_tp_2mlist(double *x, double *y, int npoints)
{
    FILE *buf_out = initialise_buffers(NULL);
    int flag, mark, interval, size;

    if (!buf_out) {
	printf("File Invalid\n");
	return 0;
    }

    pl_setOutputMode(PL_OUTPUT_MODE_TEXT);

    /* Test some invalid values (if it gets past this section, no SEGFAULTS have occurred) */
    flag = -5;
    mark = -5;
    interval = -5;
    size = -5;
    for (; flag < 5; flag++)
	for (; mark < 5; mark++)
	    for (; interval < 5; interval++)
		for (; size < 5; size++)
		    pd_marked_list(buf_out, x, y, npoints, flag, mark, interval, size);
    fclose(buf_out);

    /* Check it doesn't produce output when there are no points */
    flag = 1;
    mark = 1;
    interval = 1;
    size = 1;

    buf_out = initialise_buffers(NULL);
    pd_marked_list(buf_out, x, y, 0, flag, mark, interval, size);

    if (check_result_len(buf_out) > 0) {
	printf("Produced output when no points were given.\n");
	return 0;
    }

    /* If we don't actually have any points then nothing else is correct */
    if (npoints <= 0)
	return 1;


    /* Check it doesn't produce output if all output is turned off */
    flag = 0;
    mark = 1;
    interval = 1;
    size = 1;

    buf_out = initialise_buffers(NULL);
    pd_marked_list(buf_out, x, y, 0, flag, mark, interval, size);

    if (check_result_len(buf_out) > 0) {
	printf("Produced output when output was turned off.\n");
	return 0;
    }


    /* Check it doesn't produce output if mark is null */
    flag = 1;
    mark = 0;
    interval = 1;
    size = 1;

    buf_out = initialise_buffers(NULL);
    pd_marked_list(buf_out, x, y, 0, flag, mark, interval, size);

    if (check_result_len(buf_out) > 0) {
	printf("Produced output when mark was null.\n");
	return 0;
    }


    /* Check it does produce output all else is good */
    flag = 1;
    mark = 1;
    interval = 1;
    size = 1;

    buf_out = initialise_buffers(NULL);
    pd_marked_list(buf_out, x, y, npoints, flag, mark, interval, size);

    if (check_result_len(buf_out) <= 0) {
	printf("Didn't produce output when expected.\n");
	return 0;
    }

    return 1;
}


int
test_plot3_font_getchar(void)
{
    unsigned char space = ' ';
    unsigned char marker = 1;
    unsigned char fallback = 9;
    unsigned char question = '?';

    if (!plot3_font_getchar(&space) || !plot3_font_getchar(&marker))
	return 0;

    if (*plot3_font_getchar(&space) != PLOT3_FONT_LAST)
	return 0;

    if (*plot3_font_getchar(&marker) == PLOT3_FONT_LAST)
	return 0;

    return plot3_font_getchar(&fallback) == plot3_font_getchar(&question);
}


int
test_pd_symbol(void)
{
    FILE *buf_out = initialise_buffers(NULL);

    if (!buf_out) {
	printf("File Invalid\n");
	return 0;
    }

    pl_setOutputMode(PL_OUTPUT_MODE_TEXT);
    pd_symbol(buf_out, "A", 0.0, 0.0, 10.0, 0.0);

    return check_result_len(buf_out) > 0;
}


/* Produces the expected output of pd_3list based on the input */
void
make_tp_3list_expected(char *buf, int buflen, double *x, double *y, double *z, int npoints)
{
    int chars_written;

    if (npoints <= 0)
	return;

    chars_written = snprintf(buf, buflen, "O %g %g %g\n", *x++, *y++, *z++);
    buf += chars_written;
    buflen -= chars_written;

    while (--npoints > 0) {
	chars_written = snprintf(buf, buflen, "Q %g %g %g\n", *x++, *y++, *z++);
	buf += chars_written;
	buflen -= chars_written;
    }
}

/* This test ensures the pd_3list function produces the output it's supposed to */
int
test_tp_3list(double *x, double *y, double *z, int npoints)
{
    char expected_buf[BUFFER_SIZE];
    FILE *buf_out = initialise_buffers(expected_buf);

    if (!buf_out) {
	printf("File Invalid\n");
	return 0;
    }

    make_tp_3list_expected(expected_buf, BUFFER_SIZE, x, y, z, npoints);

    pl_setOutputMode(PL_OUTPUT_MODE_TEXT);
    pd_3list(buf_out, x, y, z, npoints);

    return compare_result(expected_buf, buf_out);
}


int
test_pdv_3symbol(void)
{
    FILE *buf_out = initialise_buffers(NULL);
    point_t p = VINIT_ZERO;
    mat_t mat;

    if (!buf_out) {
	printf("File Invalid\n");
	return 0;
    }

    MAT_IDN(mat);
    pl_setOutputMode(PL_OUTPUT_MODE_TEXT);
    pdv_3symbol(buf_out, "A", p, mat, 10.0);

    return check_result_len(buf_out) > 0;
}


int
test_pdv_3vector(void)
{
    char expected_buf[BUFFER_SIZE];
    FILE *buf_out = initialise_buffers(expected_buf);
    point_t from = VINIT_ZERO;
    point_t to = VINIT_ZERO;

    if (!buf_out) {
	printf("File Invalid\n");
	return 0;
    }

    VSET(to, 1.0, 0.0, 0.0);
    snprintf(expected_buf, BUFFER_SIZE, "V 0 0 0 1 0 0\n");

    pl_setOutputMode(PL_OUTPUT_MODE_TEXT);
    pdv_3vector(buf_out, from, to, 0.0, 0.0);

    return compare_result(expected_buf, buf_out);
}


/*
 * Test Running
 */


/* Testing list.c 2D functions */
int
automatic_2d_test(double *double_x, double *double_y, int npoints)
{
    int int_x[MAX_POINTS] = {0};
    int int_y[MAX_POINTS] = {0};

    convert_points(double_x, int_x, npoints);
    convert_points(double_y, int_y, npoints);

    if (!test_tp_i2list(int_x, int_y, npoints)) {
	printf("pl_list test failed\n");
	return 0;
    }
    if (!test_tp_2list(double_x, double_y, npoints)) {
	printf("pd_list test failed\n");
	return 0;
    }
    if (!test_tp_2mlist(double_x, double_y, npoints)) {
	printf("pd_marked_list test failed\n");
	return 0;
    }
    if (!test_plot3_font_getchar()) {
	printf("plot3_font_getchar test failed\n");
	return 0;
    }
    if (!test_pd_symbol()) {
	printf("pd_symbol test failed\n");
	return 0;
    }

    return 1;
}

/* 3D functions */
int
automatic_3d_test(double *double_x, double *double_y, double *double_z, int npoints)
{
    if (!test_tp_3list(double_x, double_y, double_z, npoints)) {
	printf("pd_3list test failed\n");
	return 0;
    }
    if (!test_pdv_3symbol()) {
	printf("pdv_3symbol test failed\n");
	return 0;
    }
    if (!test_pdv_3vector()) {
	printf("pdv_3vector test failed\n");
	return 0;
    }

    return 1;
}


int
list_main(int argc, char *argv[])
{
    double x_data[MAX_POINTS] = {0.0};
    double y_data[MAX_POINTS] = {0.0};
    double z_data[MAX_POINTS] = {0.0};

    int i = 0;
    double x, y, z;

    if (argc < 2) {
	printf("Must supply at least the dimension.\n");
	return -1;
    }

    /* If it's a 2D test */
    if (*argv[1] == '2') {
	while (i < (argc-2) && i < MAX_POINTS) {
	    sscanf(argv[i+2], "%lg,%lg", &x, &y);

	    x_data[i] = x;
	    y_data[i] = y;

	    i++;
	}

	return !automatic_2d_test(x_data, y_data, i);
    } else if (*argv[1] == '3') {
	/* or a 3D test */

	while (i < (argc-2) && i < MAX_POINTS) {
	    sscanf(argv[i+2], "%lg,%lg,%lg", &x, &y, &z);

	    x_data[i] = x;
	    y_data[i] = y;
	    z_data[i] = z;

	    i++;
	}

	return !automatic_3d_test(x_data, y_data, z_data, i);
    } else {
	printf("Wrong dimension specified.\n");
	return -1;
    }
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

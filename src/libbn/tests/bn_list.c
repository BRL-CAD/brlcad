/*                  T E S T _ L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2014 United States Government as represented by
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

#include "bn.h"
#include "bu.h"
#include "plot3.h"

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
    ret = fread(result_buf, sizeof(char), BUFFER_SIZE, result_fd);
    if (ret < BUFFER_SIZE)
	perror("fread");

    fclose(result_fd);

    return bu_strncmp(expected_buf, result_buf, BUFFER_SIZE) == 0;
}

/* Checks the result length and closes the result fd */
int
check_result_len(FILE *result_fd)
{
    size_t ret;
    char result_buf[BUFFER_SIZE+1];
    memset(result_buf, 0, BUFFER_SIZE+1);

    rewind(result_fd);
    ret = fread(result_buf, sizeof(char), BUFFER_SIZE, result_fd);
    if (ret < BUFFER_SIZE)
	perror("fread");
    fclose(result_fd);

    return strlen(result_buf);
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

/* Produces the expected output of tp_i2list based on the input */
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

/* This test ensures the tp_i2list function produces the output it's supposed to */
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
    tp_i2list(buf_out, x, y, npoints);

    return compare_result(expected_buf, buf_out);
}


/* Produces the expected output of tp_2list based on the input */
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

/* This test ensures the tp_2list function produces the output it's supposed to */
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
    tp_2list(buf_out, x, y, npoints);

    return compare_result(expected_buf, buf_out);
}


/*
 * This test tests the tp_2mlist marker function to ensure it
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
		    tp_2mlist(buf_out, x, y, npoints, flag, mark, interval, size);
    fclose(buf_out);

    /* Check it doesn't produce output when there are no points */
    flag = 1;
    mark = 1;
    interval = 1;
    size = 1;

    buf_out = initialise_buffers(NULL);
    tp_2mlist(buf_out, x, y, 0, flag, mark, interval, size);

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
    tp_2mlist(buf_out, x, y, 0, flag, mark, interval, size);

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
    tp_2mlist(buf_out, x, y, 0, flag, mark, interval, size);

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
    tp_2mlist(buf_out, x, y, npoints, flag, mark, interval, size);

    if (check_result_len(buf_out) <= 0) {
	printf("Didn't produce output when expected.\n");
	return 0;
    }

    return 1;
}


/* Produces the expected output of tp_3list based on the input */
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

/* This test ensures the tp_3list function produces the output it's supposed to */
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
    tp_3list(buf_out, x, y, z, npoints);

    return compare_result(expected_buf, buf_out);
}


/*
 * Test Running
 */


/* Testing list.c 2D functions */
int
automatic_2d_test(double *double_x, double *double_y, int npoints)
{
    int int_x[MAX_POINTS], int_y[MAX_POINTS];

    convert_points(double_x, int_x, npoints);
    convert_points(double_y, int_y, npoints);

    if (!test_tp_i2list(int_x, int_y, npoints)) {
	printf("tp_i2list test failed\n");
	return 0;
    }
    if (!test_tp_2list(double_x, double_y, npoints)) {
	printf("tp_2list test failed\n");
	return 0;
    }
    if (!test_tp_2mlist(double_x, double_y, npoints)) {
	printf("tp_2mlist test failed\n");
	return 0;
    }

    return 1;
}

/* 3D functions */
int
automatic_3d_test(double *double_x, double *double_y, double *double_z, int npoints)
{
    if (!test_tp_3list(double_x, double_y, double_z, npoints)) {
	printf("tp_3list test failed\n");
	return 0;
    }

    return 1;
}


int
main(int argc, char *argv[])
{
    double x_data[MAX_POINTS];
    double y_data[MAX_POINTS];
    double z_data[MAX_POINTS];

    int i = 0;
    double x, y, z;

    if (argc < 1) {
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

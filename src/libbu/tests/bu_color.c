/*                       B U _ C O L O R . C
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

#include "vmath.h"

static int
test_bu_rgb_to_hsv(int argc, char *argv[])
{
    fastf_t expected_hsv_color[3];
    fastf_t actual_hsv_color[3];
    unsigned int rgb_color[3];

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is rgb_values expected_hsv_values [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%u,%u,%u", &rgb_color[RED], &rgb_color[GRN], &rgb_color[BLU]);
    sscanf(argv[3], "%lf,%lf,%lf", &expected_hsv_color[HUE], &expected_hsv_color[SAT], &expected_hsv_color[VAL]);

    bu_rgb_to_hsv((unsigned char *)rgb_color, actual_hsv_color);

    printf("Result: %f,%f,%f", actual_hsv_color[HUE], actual_hsv_color[SAT], actual_hsv_color[VAL]);

    return !(EQUAL(expected_hsv_color[HUE], actual_hsv_color[HUE])
	     && EQUAL(expected_hsv_color[SAT], actual_hsv_color[SAT])
	     && EQUAL(expected_hsv_color[VAL], actual_hsv_color[VAL]));
}

static int
test_bu_hsv_to_rgb(int argc, char *argv[])
{
    unsigned int expected_rgb_color[3];
    unsigned char actual_rgb_color[3];
    fastf_t hsv_color[3];

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is hsv_values expected_rgb_values [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &hsv_color[HUE], &hsv_color[SAT], &hsv_color[VAL]);
    sscanf(argv[3], "%u,%u,%u", &expected_rgb_color[RED], &expected_rgb_color[GRN], &expected_rgb_color[BLU]);

    bu_hsv_to_rgb(hsv_color, actual_rgb_color);

    printf("Result: %u,%u,%u", actual_rgb_color[RED], actual_rgb_color[GRN], actual_rgb_color[BLU]);

    return !(expected_rgb_color[RED] == actual_rgb_color[RED]
	     && expected_rgb_color[GRN] == actual_rgb_color[GRN]
	     && expected_rgb_color[BLU] == actual_rgb_color[BLU]);
}

static int
test_bu_str_to_rgb(int argc, char *argv[])
{
    unsigned int expected_rgb_color[3];
    unsigned char actual_rgb_color[3];
    char *rgb_string;

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is rgb_string expected_rgb_values [%s]\n", argv[0]);
    }

    rgb_string = argv[2];
    sscanf(argv[3], "%u,%u,%u", &expected_rgb_color[RED], &expected_rgb_color[GRN], &expected_rgb_color[BLU]);

    bu_str_to_rgb(rgb_string, actual_rgb_color);

    printf("Result: %u,%u,%u", actual_rgb_color[RED], actual_rgb_color[GRN], actual_rgb_color[BLU]);

    return !(expected_rgb_color[RED] == actual_rgb_color[RED]
	     && expected_rgb_color[GRN] == actual_rgb_color[GRN]
	     && expected_rgb_color[BLU] == actual_rgb_color[BLU]);
}

static int
test_bu_color_to_rgb_floats(int argc, char *argv[])
{
    fastf_t expected_rgb_color[3];
    fastf_t actual_rgb_color[3];
    struct bu_color color = BU_COLOR_INIT_ZERO;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is rgb_color [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &expected_rgb_color[RED], &expected_rgb_color[GRN], &expected_rgb_color[BLU]);

    color.buc_rgb[RED] = expected_rgb_color[RED];
    color.buc_rgb[GRN] = expected_rgb_color[GRN];
    color.buc_rgb[BLU] = expected_rgb_color[BLU];

    bu_color_to_rgb_floats(&color, actual_rgb_color);

    printf("Result: %f,%f,%f", actual_rgb_color[RED], actual_rgb_color[GRN], actual_rgb_color[BLU]);

    return !(EQUAL(expected_rgb_color[RED], actual_rgb_color[RED])
	     && EQUAL(expected_rgb_color[GRN], actual_rgb_color[GRN])
	     && EQUAL(expected_rgb_color[BLU], actual_rgb_color[BLU]));
}

static int
test_bu_color_from_rgb_floats(int argc, char *argv[])
{
    fastf_t expected_rgb_color[3];
    fastf_t actual_rgb_color[3];
    struct bu_color color = BU_COLOR_INIT_ZERO;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is rgb_color [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &expected_rgb_color[RED], &expected_rgb_color[GRN], &expected_rgb_color[BLU]);

    bu_color_from_rgb_floats(&color, expected_rgb_color);

    actual_rgb_color[RED] = color.buc_rgb[RED];
    actual_rgb_color[GRN] = color.buc_rgb[GRN];
    actual_rgb_color[BLU] = color.buc_rgb[BLU];

    printf("Result: %f,%f,%f", actual_rgb_color[RED], actual_rgb_color[GRN], actual_rgb_color[BLU]);

    return !(EQUAL(expected_rgb_color[RED], actual_rgb_color[RED])
	     && EQUAL(expected_rgb_color[GRN], actual_rgb_color[GRN])
	     && EQUAL(expected_rgb_color[BLU], actual_rgb_color[BLU]));
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
	    return test_bu_rgb_to_hsv(argc, argv);
	case 2:
	    return test_bu_hsv_to_rgb(argc, argv);
	case 3:
	    return test_bu_str_to_rgb(argc, argv);
	case 4:
	    return test_bu_color_to_rgb_floats(argc, argv);
	case 5:
	    return test_bu_color_from_rgb_floats(argc, argv);
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

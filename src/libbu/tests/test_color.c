/*                    T E S T _ C O L O R . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "bu.h"

#include "vmath.h"


static int
test_bu_rgb_to_hsv(int argc, char *argv[])
{
    fastf_t expected_hsv_color[3] = {11.0, 22.0, 33.0};
    fastf_t actual_hsv_color[3] = {11.0, 22.0, 33.0};
    unsigned int scanned_rgb_color[3] = {11, 22, 33};
    unsigned char rgb_color[3] = {11, 22, 33};

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is rgb_values expected_hsv_values [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%u, %u, %u", &scanned_rgb_color[RED], &scanned_rgb_color[GRN], &scanned_rgb_color[BLU]);
    VMOVE(rgb_color, scanned_rgb_color);
    sscanf(argv[3], "%lf, %lf, %lf", &expected_hsv_color[HUE], &expected_hsv_color[SAT], &expected_hsv_color[VAL]);

    bu_rgb_to_hsv(rgb_color, actual_hsv_color);

    VPRINT("Result:", actual_hsv_color);

    /* Use 0.01 as tolerance to allow the numbers in CMakeLists.txt to
     * be a reasonable length.
     */
    return !(VNEAR_EQUAL(expected_hsv_color, actual_hsv_color, 0.01));
}


static int
test_bu_hsv_to_rgb(int argc, char *argv[])
{
    unsigned int expected_rgb_color[3] = {11, 22, 33};
    unsigned char actual_rgb_color[3] = {11, 22, 33};
    fastf_t hsv_color[3] = {11.0, 22.0, 33.0};

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is hsv_values expected_rgb_values [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf, %lf, %lf", &hsv_color[HUE], &hsv_color[SAT], &hsv_color[VAL]);
    sscanf(argv[3], "%u, %u, %u", &expected_rgb_color[RED], &expected_rgb_color[GRN], &expected_rgb_color[BLU]);

    bu_hsv_to_rgb(hsv_color, actual_rgb_color);

    bu_log("Result: %u, %u, %u", actual_rgb_color[RED], actual_rgb_color[GRN], actual_rgb_color[BLU]);

    return !(VEQUAL(expected_rgb_color, actual_rgb_color));
}


static int
test_bu_str_to_rgb(int argc, char *argv[])
{
    unsigned int expected_rgb_color[3] = {11, 22, 33};
    unsigned char actual_rgb_color[3] = {11, 22, 33};
    char *rgb_string;

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is rgb_string expected_rgb_values [%s]\n", argv[0]);
    }

    rgb_string = argv[2];
    sscanf(argv[3], "%u, %u, %u", &expected_rgb_color[RED], &expected_rgb_color[GRN], &expected_rgb_color[BLU]);

    bu_str_to_rgb(rgb_string, actual_rgb_color);

    bu_log("Result: %u, %u, %u", actual_rgb_color[RED], actual_rgb_color[GRN], actual_rgb_color[BLU]);

    return !(VEQUAL(expected_rgb_color, actual_rgb_color));
}


static int
test_bu_color_to_rgb_floats(int argc, char *argv[])
{
    fastf_t expected_rgb_color[3] = {11.0, 22.0, 33.0};
    fastf_t actual_rgb_color[3] = {11.0, 22.0, 33.0};
    struct bu_color color = BU_COLOR_INIT_ZERO;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is rgb_color [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf, %lf, %lf", &expected_rgb_color[RED], &expected_rgb_color[GRN], &expected_rgb_color[BLU]);

    VSCALE(color.buc_rgb, expected_rgb_color, 1.0 / 255.0);

    /* this is a simple pass-through test of bu_color_to_rgb_floats()
     * that shouldn't result in change so long as our naive
     * normalization math behaves within typical floating point fuzz..
     */

    bu_color_to_rgb_floats(&color, actual_rgb_color);

    VSCALE(actual_rgb_color, actual_rgb_color, 255.0);

    return !(VEQUAL(expected_rgb_color, actual_rgb_color));
}


static int
test_bu_color_from_rgb_floats(int argc, char *argv[])
{
    fastf_t expected_rgb_color[3] = {11.0, 22.0, 33.0};
    fastf_t actual_rgb_color[3] = {11.0, 22.0, 33.0};
    struct bu_color color = BU_COLOR_INIT_ZERO;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is rgb_color [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf, %lf, %lf", &expected_rgb_color[RED], &expected_rgb_color[GRN], &expected_rgb_color[BLU]);

    bu_color_from_rgb_floats(&color, expected_rgb_color);

    VSCALE(actual_rgb_color, color.buc_rgb, 255.0);

    VPRINT("Result:", actual_rgb_color);

    return !(VEQUAL(expected_rgb_color, actual_rgb_color));
}


static int
test_bu_color_to_rgb_chars(int argc, char *argv[])
{
    int expected_rgb_color[3] = {11, 22, 33};
    unsigned char actual_rgb_color[3] = {11, 22, 33};

    struct bu_color color = BU_COLOR_INIT_ZERO;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is rgb_color [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%d, %d, %d", &expected_rgb_color[RED], &expected_rgb_color[GRN], &expected_rgb_color[BLU]);

    VSCALE(color.buc_rgb, expected_rgb_color, 1.0 / 255.0);

    bu_color_to_rgb_chars(&color, actual_rgb_color);

    bu_log("Result: %d, %d, %d", actual_rgb_color[RED], actual_rgb_color[GRN], actual_rgb_color[BLU]);

    return !(VEQUAL(expected_rgb_color, actual_rgb_color));
}


static int
test_bu_color_from_rgb_chars(int argc, char *argv[])
{
    int scanned_rgb_color[3] = {11, 22, 33};
    unsigned char expected_rgb_color[3] = {11, 22, 33};
    unsigned char actual_rgb_color[3] = {11, 22, 33};
    struct bu_color color = BU_COLOR_INIT_ZERO;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is rgb_color [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%d, %d, %d", &scanned_rgb_color[RED], &scanned_rgb_color[GRN], &scanned_rgb_color[BLU]);

    VMOVE(expected_rgb_color, scanned_rgb_color);

    bu_color_from_rgb_chars(&color, expected_rgb_color);
    bu_color_to_rgb_chars(&color, actual_rgb_color);

    bu_log("Result: %d, %d, %d", actual_rgb_color[RED], actual_rgb_color[GRN], actual_rgb_color[BLU]);

    return !(VEQUAL(expected_rgb_color, actual_rgb_color));
}


/* Exercise bu_color_from_str() across all of the supported color
 * specification notations.  argv[2] is the specification, argv[3] is
 * either the expected "r, g, b" (0-255) result or the literal
 * "invalid" to assert that the specification is rejected.
 */
static int
test_bu_color_from_str(int argc, char *argv[])
{
    struct bu_color color = BU_COLOR_INIT_ZERO;
    unsigned char actual[3] = {0, 0, 0};
    unsigned int expected[3] = {0, 0, 0};
    int ret;

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is color_spec expected_rgb [%s]\n", argv[0]);
    }

    ret = bu_color_from_str(&color, argv[2]);

    if (BU_STR_EQUAL(argv[3], "invalid")) {
	/* the specification is expected to be rejected */
	return (ret != 0);
    }

    if (!ret) {
	bu_log("Failed to parse color specification: %s\n", argv[2]);
	return 1;
    }

    sscanf(argv[3], "%u, %u, %u", &expected[RED], &expected[GRN], &expected[BLU]);
    bu_color_to_rgb_chars(&color, actual);
    bu_log("Result: %u, %u, %u\n", actual[RED], actual[GRN], actual[BLU]);

    return !(actual[RED] == expected[RED]
	     && actual[GRN] == expected[GRN]
	     && actual[BLU] == expected[BLU]);
}


/* Exercise the alpha-bearing notations of bu_color_from_str().
 * argv[2] is the specification, argv[3] is the expected
 * "r, g, b, a" (0-255) result.
 */
static int
test_bu_color_from_str_alpha(int argc, char *argv[])
{
    struct bu_color color = BU_COLOR_INIT_ZERO;
    unsigned int expected[4] = {0, 0, 0, 0};
    int actual[4] = {0, 0, 0, 0};
    int ret;

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is color_spec expected_rgba [%s]\n", argv[0]);
    }

    ret = bu_color_from_str(&color, argv[2]);
    if (!ret) {
	bu_log("Failed to parse color specification: %s\n", argv[2]);
	return 1;
    }

    sscanf(argv[3], "%u, %u, %u, %u",
	   &expected[RED], &expected[GRN], &expected[BLU], &expected[ALP]);

    actual[RED] = (int)lrint(color.buc_rgb[RED] * 255.0);
    actual[GRN] = (int)lrint(color.buc_rgb[GRN] * 255.0);
    actual[BLU] = (int)lrint(color.buc_rgb[BLU] * 255.0);
    actual[ALP] = (int)lrint(color.buc_rgb[ALP] * 255.0);
    bu_log("Result: %d, %d, %d, %d\n",
	   actual[RED], actual[GRN], actual[BLU], actual[ALP]);

    return !((unsigned int)actual[RED] == expected[RED]
	     && (unsigned int)actual[GRN] == expected[GRN]
	     && (unsigned int)actual[BLU] == expected[BLU]
	     && (unsigned int)actual[ALP] == expected[ALP]);
}


/* Exercise bu_color_to_str().  argv[2] is an "r, g, b" (0-255) input
 * color, argv[3] is the requested format, and argv[4] is the expected
 * serialized string.
 */
static int
test_bu_color_to_str(int argc, char *argv[])
{
    struct bu_color color = BU_COLOR_INIT_ZERO;
    unsigned int in[3] = {0, 0, 0};
    struct bu_vls out = BU_VLS_INIT_ZERO;
    const char *format;
    int ret;
    int result;

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is rgb format expected [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%u, %u, %u", &in[RED], &in[GRN], &in[BLU]);
    color.buc_rgb[RED] = (fastf_t)in[RED] / 255.0;
    color.buc_rgb[GRN] = (fastf_t)in[GRN] / 255.0;
    color.buc_rgb[BLU] = (fastf_t)in[BLU] / 255.0;

    /* an empty format string selects the canonical default */
    format = BU_STR_EQUAL(argv[3], "default") ? "" : argv[3];

    ret = bu_color_to_str(&out, &color, format);
    if (!ret) {
	bu_log("bu_color_to_str failed for format: %s\n", argv[3]);
	bu_vls_free(&out);
	return 1;
    }

    bu_log("Result: %s\n", bu_vls_cstr(&out));
    result = !BU_STR_EQUAL(bu_vls_cstr(&out), argv[4]);
    bu_vls_free(&out);
    return result;
}


int
main(int argc, char *argv[])
{
    int function_num = 0;

    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    if (argc < 2) {
	bu_log("Usage: %s {function_num} {function_test_arg0} {...}", argv[0]);
	bu_exit(1, "ERROR: missing function number\n");
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
	case 6:
	    return test_bu_color_to_rgb_chars(argc, argv);
	case 7:
	    return test_bu_color_from_rgb_chars(argc, argv);
	case 8:
	    return test_bu_color_from_str(argc, argv);
	case 9:
	    return test_bu_color_from_str_alpha(argc, argv);
	case 10:
	    return test_bu_color_to_str(argc, argv);
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
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */

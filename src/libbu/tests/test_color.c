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


static int
color_near_equal(const double a[4], const double b[4], double tolerance)
{
    size_t i;

    for (i = 0; i < 4; i++) {
	if (fabs(a[i] - b[i]) > tolerance)
	    return 0;
    }
    return 1;
}


static int
test_bu_color_parse(int argc, char *argv[])
{
    static const struct {
	const char *spec;
	double expected[4];
    } valid[] = {
	{"255/0/0", {1.0, 0.0, 0.0, 1.0}},
	{"1.0 0 0", {1.0, 0.0, 0.0, 1.0}},
	{"#f008", {1.0, 0.0, 0.0, 136.0 / 255.0}},
	{"rebeccapurple", {0x66 / 255.0, 0x33 / 255.0, 0x99 / 255.0, 1.0}},
	{"green", {0.0, 128.0 / 255.0, 0.0, 1.0}},
	{"purple", {128.0 / 255.0, 0.0, 128.0 / 255.0, 1.0}},
	{"rgb(100%, 0%, 0%)", {1.0, 0.0, 0.0, 1.0}},
	{"rgba(255, 0, 0, 0.5)", {1.0, 0.0, 0.0, 0.5}},
	{"hsl(120, 100%, 50%)", {0.0, 1.0, 0.0, 1.0}},
	{"hsl(0%, 100%, 50%)", {1.0, 0.0, 0.0, 1.0}},
	{"hsva(240, 100%, 100%, 25%)", {0.0, 0.0, 1.0, 0.25}},
	{"transparent", {0.0, 0.0, 0.0, 0.0}}
    };
    static const char *invalid[] = {
	"not-a-color",
	"rgb(256, 0, 0)",
	"rgba(255, 0, 0, 2)",
	"hsl(0, 101%, 50%)",
	"rgb(1, 2, 3) trailing"
    };
    size_t i;

    if (argc != 2)
	bu_exit(1, "ERROR: unexpected arguments [%s]\n", argv[0]);

    for (i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
	struct bu_color color = BU_COLOR_INIT_ZERO;
	double actual[4];
	size_t j;
	if (!bu_color_parse(valid[i].spec, &color)) {
	    bu_log("bu_color_parse rejected valid input: %s\n", valid[i].spec);
	    return 1;
	}
	for (j = 0; j < 4; j++)
	    actual[j] = color.buc_rgb[j];
	if (!color_near_equal(actual, valid[i].expected, 1.0e-12)) {
	    bu_log("bu_color_parse returned the wrong value for: %s\n", valid[i].spec);
	    return 1;
	}
    }

    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
	struct bu_color color = {{0.125, 0.25, 0.375, 0.5}};
	double unchanged[4] = {0.125, 0.25, 0.375, 0.5};
	double actual[4];
	size_t j;
	if (bu_color_parse(invalid[i], &color)) {
	    bu_log("bu_color_parse accepted invalid input: %s\n", invalid[i]);
	    return 1;
	}
	for (j = 0; j < 4; j++)
	    actual[j] = color.buc_rgb[j];
	if (!color_near_equal(actual, unchanged, 0.0)) {
	    bu_log("bu_color_parse changed output after rejecting: %s\n", invalid[i]);
	    return 1;
	}
    }

    return 0;
}


static int
check_color_format(const char *spec, bu_color_format_t format, const char *expected)
{
    struct bu_color color = BU_COLOR_INIT_ZERO;
    struct bu_vls output = BU_VLS_INIT_ZERO;
    int failed = 0;

    bu_vls_strcpy(&output, "prefix:");
    if (!bu_color_parse(spec, &color)
	|| !bu_color_format(&color, format, &output)
	|| !BU_STR_EQUAL(bu_vls_cstr(&output), expected)) {
	bu_log("bu_color_format failed for %s: expected [%s], got [%s]\n",
	       spec, expected, bu_vls_cstr(&output));
	failed = 1;
    }
    bu_vls_free(&output);
    return failed;
}


static int
test_bu_color_format(int argc, char *argv[])
{
    struct bu_color color = BU_COLOR_INIT_ZERO;
    struct bu_vls output = BU_VLS_INIT_ZERO;

    if (argc != 2)
	bu_exit(1, "ERROR: unexpected arguments [%s]\n", argv[0]);

    if (check_color_format("rgba(255, 0, 0, 0.5)", BU_COLOR_FORMAT_RGB,
			   "prefix:rgb(255, 0, 0)"))
	return 1;
    if (check_color_format("rgba(255, 0, 0, 0.5)", BU_COLOR_FORMAT_RGBA,
			   "prefix:rgba(255, 0, 0, 0.5)"))
	return 1;
    if (check_color_format("rgba(255, 0, 0, 0.5)", BU_COLOR_FORMAT_HEXA,
			   "prefix:#ff000080"))
	return 1;
    if (check_color_format("red", BU_COLOR_FORMAT_HEX,
			   "prefix:#ff0000"))
	return 1;
    if (check_color_format("red", BU_COLOR_FORMAT_HSL,
			   "prefix:hsl(0, 100%, 50%)"))
	return 1;
    if (check_color_format("rgba(255, 0, 0, 0.5)", BU_COLOR_FORMAT_HSLA,
			   "prefix:hsla(0, 100%, 50%, 0.5)"))
	return 1;
    if (check_color_format("red", BU_COLOR_FORMAT_HSV,
			   "prefix:hsv(0, 100%, 100%)"))
	return 1;
    if (check_color_format("rgba(255, 0, 0, 0.5)", BU_COLOR_FORMAT_HSVA,
			   "prefix:hsva(0, 100%, 100%, 0.5)"))
	return 1;
    if (check_color_format("rebeccapurple", BU_COLOR_FORMAT_NAME,
			   "prefix:rebeccapurple"))
	return 1;

    if (!bu_color_parse("#010203", &color))
	return 1;
    bu_vls_strcpy(&output, "unchanged");
    if (bu_color_format(&color, BU_COLOR_FORMAT_NAME, &output)
	|| !BU_STR_EQUAL(bu_vls_cstr(&output), "unchanged")) {
	bu_log("bu_color_format did not fail atomically for an unnamed color\n");
	bu_vls_free(&output);
	return 1;
    }
    bu_vls_free(&output);
    return 0;
}


static int
test_bu_color_convert(int argc, char *argv[])
{
    double rgb[4] = {1.0, 0.0, 0.0, 0.25};
    double expected_hsl[4] = {0.0, 1.0, 0.5, 0.25};
    double hsl[4];
    double green_hsl[4] = {120.0, 1.0, 0.5, 0.75};
    double expected_green[4] = {0.0, 1.0, 0.0, 0.75};
    double green[4];
    double wrapped_hsv[4] = {-120.0, 1.0, 1.0, 1.0};
    double expected_blue[4] = {0.0, 0.0, 1.0, 1.0};
    double invalid[4] = {0.0, 2.0, 1.0, 1.0};
    double unchanged[4] = {9.0, 8.0, 7.0, 6.0};
    double expected_unchanged[4] = {9.0, 8.0, 7.0, 6.0};

    if (argc != 2)
	bu_exit(1, "ERROR: unexpected arguments [%s]\n", argv[0]);

    if (!bu_color_convert(rgb, BU_COLOR_SPACE_RGB, BU_COLOR_SPACE_HSL, hsl)
	|| !color_near_equal(hsl, expected_hsl, 1.0e-12))
	return 1;
    if (!bu_color_convert(green_hsl, BU_COLOR_SPACE_HSL, BU_COLOR_SPACE_RGB, green)
	|| !color_near_equal(green, expected_green, 1.0e-12))
	return 1;
    if (!bu_color_convert(wrapped_hsv, BU_COLOR_SPACE_HSV, BU_COLOR_SPACE_RGB,
			  wrapped_hsv)
	|| !color_near_equal(wrapped_hsv, expected_blue, 1.0e-12))
	return 1;
    if (bu_color_convert(invalid, BU_COLOR_SPACE_HSL, BU_COLOR_SPACE_RGB, unchanged)
	|| !color_near_equal(unchanged, expected_unchanged, 0.0))
	return 1;

    return 0;
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
	    return test_bu_color_parse(argc, argv);
	case 9:
	    return test_bu_color_format(argc, argv);
	case 10:
	    return test_bu_color_convert(argc, argv);
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

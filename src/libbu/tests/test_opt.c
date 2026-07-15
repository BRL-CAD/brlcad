/*                      T E S T _ O P T . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2026 United States Government as represented by
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
#include <limits.h>
#include <ctype.h>
#include "bu.h"
#include "bn.h"
#include "string.h"


#define help_str "Print help string and exit."


static int
d1_verb(struct bu_vls *msg, size_t argc, const char **argv, void *set_v)
{
    int val = INT_MAX;
    int *int_set = (int *)set_v;
    int int_ret;
    if (!argv || !argv[0] || strlen(argv[0]) == 0 || argc == 0) {
	/* Have verbosity flag but no valid arg - go with just the flag */
	if (int_set) (*int_set) = 1;
	return 0;
    }

    int_ret = bu_opt_int(msg, argc, argv, (void *)&val);
    if (int_ret == -1) return -1;

    if (val < 0 || val > 3) return -1;

    if (int_set) (*int_set) = val;

    return 1;
}


static void
set_msg_str(struct bu_vls *msg, int ac, const char **av)
{
    int i = 0;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    if (!msg || !av) return;
    bu_vls_sprintf(&vls, "Parsing arg string \"");
    for (i = 0; i < ac - 1; i++) {
	bu_vls_printf(&vls, "%s ", av[i]);
    }
    bu_vls_printf(&vls, "%s\":", av[ac - 1]);
    bu_vls_printf(msg, "%s", bu_vls_addr(&vls));
    bu_vls_free(&vls);
}


#define EXPECT_SUCCESS_FLAG(_name, _var) { \
	set_msg_str(&parse_msgs, ac, av); \
	ret = bu_opt_parse(&parse_msgs, ac, av, d); \
	if (ret || _var != 1) { \
	    bu_vls_printf(&parse_msgs, "\nError - expected value \"1\" and got value %d\n", _var); \
	    val_ok = 0; \
	} else { \
	    bu_vls_printf(&parse_msgs, "  \nGot expected value: %s = %d\n", _name, _var); \
	} \
    }


#define EXPECT_SUCCESS_INT(_name, _var, _exp) { \
	set_msg_str(&parse_msgs, ac, av); \
	ret = bu_opt_parse(&parse_msgs, ac, av, d); \
	if (ret || _var != _exp) { \
	    bu_vls_printf(&parse_msgs, "\nError - expected value \"%ld\" and got value %ld\n", (long int)_exp, (long int)_var); \
	    val_ok = 0; \
	} else { \
	    bu_vls_printf(&parse_msgs, "  \nGot expected value: %s = %ld\n", _name, (long int)_var); \
	} \
    }

#define EXPECT_SUCCESS_INT_UNKNOWN(_name, _var, _exp) { \
	set_msg_str(&parse_msgs, ac, av); \
	ret = bu_opt_parse(&parse_msgs, ac, av, d); \
	if (ret <= 0) { \
	    bu_vls_printf(&parse_msgs, "\nError - extra args but none found.\n"); \
	    val_ok = 0; \
	} else { \
	    if (_var != _exp) { \
		bu_vls_printf(&parse_msgs, "\nError - expected value \"%d\" and got value %d\n", _exp, _var); \
		val_ok = 0; \
	    } else { \
		bu_vls_printf(&parse_msgs, "  \nGot expected value: %s = %d\n", _name, _var); \
	    } \
	} \
    }

#define EXPECT_FAILURE_INT_UNKNOWN(_name, _var, _exp) { \
	set_msg_str(&parse_msgs, ac, av); \
	ret = bu_opt_parse(&parse_msgs, ac, av, d); \
	if (ret <= 0 || _var == _exp) { \
	    bu_vls_printf(&parse_msgs, "\nError - expected failure (%s) but no error returned\n", _name); \
	    val_ok = 0; \
	} else { \
	    bu_vls_printf(&parse_msgs, "  \nOK (expected failure) %s\n", _name); \
	} \
    }

#define EXPECT_SUCCESS_FLOAT(_name, _var, _exp) { \
	set_msg_str(&parse_msgs, ac, av); \
	ret = bu_opt_parse(&parse_msgs, ac, av, d); \
	if (ret || !NEAR_EQUAL(_var, _exp, SMALL_FASTF)) { \
	    bu_vls_printf(&parse_msgs, "\nError - expected value \"%f\" and got value %f\n", _exp, _var); \
	    val_ok = 0; \
	} else { \
	    bu_vls_printf(&parse_msgs, "  \nGot expected value: %s = %f\n", _name, _var); \
	} \
    }

#define EXPECT_SUCCESS_STRING(_name, _var, _exp) { \
	set_msg_str(&parse_msgs, ac, av); \
	ret = bu_opt_parse(&parse_msgs, ac, av, d); \
	if (ret || !BU_STR_EQUAL(_var, _exp)) { \
	    bu_vls_printf(&parse_msgs, "\nError - expected value \"%s\" and got value %s\n", _exp, _var); \
	    val_ok = 0; \
	} else { \
	    bu_vls_printf(&parse_msgs, "  \nGot expected value: %s = %s\n", _name, _var); \
	} \
    }

#define EXPECT_SUCCESS_COLOR(_name, _color, _r, _g, _b) { \
	unsigned char rgb[3] = {0, 0, 0}; \
	set_msg_str(&parse_msgs, ac, av); \
	ret = bu_opt_parse(&parse_msgs, ac, av, d); \
	bu_color_to_rgb_chars(&_color, rgb); \
	if (ret || (!NEAR_EQUAL(rgb[RED], _r, SMALL_FASTF) || !NEAR_EQUAL(rgb[GRN], _g, SMALL_FASTF) || !NEAR_EQUAL(rgb[BLU], _b, SMALL_FASTF))) { \
	    bu_vls_printf(&parse_msgs, "\nError - expected value \"%d/%d/%d\" and got value %d/%d/%d\n", _r, _g, _b, rgb[RED], rgb[GRN], rgb[BLU]); \
	    val_ok = 0; \
	} else { \
	    bu_vls_printf(&parse_msgs, "  \nGot expected value: %s == %d/%d/%d\n", _name, rgb[RED], rgb[GRN], rgb[BLU]); \
	} \
    }

#define EXPECT_SUCCESS_COLOR_UNKNOWN(_name, _color, _r, _g, _b) { \
	unsigned char rgb[3] = {0, 0, 0}; \
	set_msg_str(&parse_msgs, ac, av); \
	ret = bu_opt_parse(&parse_msgs, ac, av, d); \
	bu_color_to_rgb_chars(&_color, rgb); \
	if (ret <= 0) { \
	    bu_vls_printf(&parse_msgs, "\nError - extra args expected but not found\n"); \
	    val_ok = 0; \
	} else { \
	    if ((!NEAR_EQUAL(rgb[RED], _r, SMALL_FASTF) || !NEAR_EQUAL(rgb[GRN], _g, SMALL_FASTF) || !NEAR_EQUAL(rgb[BLU], _b, SMALL_FASTF))) { \
		bu_vls_printf(&parse_msgs, "\nError - expected value \"%d/%d/%d\" and got value %d/%d/%d\n", _r, _g, _b, rgb[RED], rgb[GRN], rgb[BLU]); \
		val_ok = 0; \
	    } else { \
		bu_vls_printf(&parse_msgs, "  \nGot expected value: %s == %d/%d/%d\n", _name, rgb[RED], rgb[GRN], rgb[BLU]); \
	    } \
	} \
    }

#define EXPECT_SUCCESS_VECT(_name, _v, _v1, _v2, _v3) { \
	set_msg_str(&parse_msgs, ac, av); \
	ret = bu_opt_parse(&parse_msgs, ac, av, d); \
	if (ret || (!NEAR_EQUAL(_v[0], _v1, SMALL_FASTF) || !NEAR_EQUAL(_v[1], _v2, SMALL_FASTF) || !NEAR_EQUAL(_v[2], _v3, SMALL_FASTF))) { \
	    bu_vls_printf(&parse_msgs, "\nError - expected value \"%f/%f/%f\" and got value %f/%f/%f\n", _v1, _v2, _v3, _v[0], _v[1], _v[2]); \
	    val_ok = 0; \
	} else { \
	    bu_vls_printf(&parse_msgs, "  \nGot expected value: %s == %f/%f/%f\n", _name,  _v[0], _v[1], _v[2]); \
	} \
    }


#define EXPECT_FAILURE(_name, _reason) { \
	set_msg_str(&parse_msgs, ac, av); \
	ret = bu_opt_parse(&parse_msgs, ac, av, d); \
	if (ret != -1) { \
	    bu_vls_printf(&parse_msgs, "\nError - expected parser to fail with error and it didn't\n"); \
	    val_ok = 0; \
	} else { \
	    bu_vls_printf(&parse_msgs, "  \nOK (expected failure) - %s failed (%s)\n", _name, _reason); \
	    ret = 0; \
	} \
    }


static int
desc_1(const char *cgy, int test_num)
{
    static int print_help = 0;
    static int verbosity = 0;
    static int b = -1;
    static int m = 0;
    static int F = 0;
    static const char *str = NULL;
    static struct bu_vls vls = BU_VLS_INIT_ZERO;
    static struct bu_vls vls2 = BU_VLS_INIT_ZERO;
    static int i = 0;
    static long l = 0;
    static fastf_t f = 0;

    /* Option descriptions */
    struct bu_opt_desc d[] = {
	{"h", "help",    "",       NULL,            (void *)&print_help, help_str},
	{"?", "",        "",       NULL,            (void *)&print_help, help_str},
	{"v", "verb",    "[#]",    &d1_verb,        (void *)&verbosity,  "Set verbosity (range is 0 to 3)"},
	{"b", "bool",    "bool",   &bu_opt_bool,    (void *)&b,          "Set boolean flag"},
	{"s", "str",     "string", &bu_opt_str,     (void *)&str,        "Set string"},
	{"i", "int",     "#",      &bu_opt_int,     (void *)&i,          "Set int"},
	{"l", "long",    "#",      &bu_opt_long,    (void *)&l,          "Set long"},
	{"f", "fastf_t", "#",      &bu_opt_fastf_t, (void *)&f,          "Read float"},
	{"m", "mflag",   "flag",   NULL,            (void *)&m,          "Set boolean flag"},
	{"F", "Fflag",   "flag",   NULL,            (void *)&F,          "Set boolean flag"},
	{"",  "vls1", "variable-length string", &bu_opt_vls, (void *)&vls, "Set variable length string"},
	{"a", "vls2", "variable-length string", &bu_opt_vls, (void *)&vls2, "Set variable length string with flag"},
	BU_OPT_DESC_NULL
    };

    int ac = 0;
    int val_ok = 1;
    int ret = -1;
    int containers = 6;
    const char **av;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;

    av = (const char **)bu_calloc(containers, sizeof(char *), "Input array");

    if (cgy[0] == 'v') {
	switch (test_num) {
	    /* Verbosity option tests */
	    case 0:
		ret = bu_opt_parse(&parse_msgs, 0, NULL, d);
		ret = (ret == -1) ? 0 : -1;
		break;
	    case 1:
		ac = 1;
		av[0] = "-v";
		EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
		break;
	    case 2:
		ac = 1;
		av[0] = "-v1";
		EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
		break;
	    case 3:
		ac = 2;
		av[0] = "-v";
		av[1] = "1";
		EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
		break;
	    case 4:
		ac = 1;
		av[0] = "-v=1";
		EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
		break;
	    case 5:
		ac = 2;
		av[0] = "--v";
		av[1] = "1";
		EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
		break;
	    case 6:
		ac = 1;
		av[0] = "--v=1";
		EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
		break;
	    case 7:
		ac = 2;
		av[0] = "-v";
		av[1] = "2";
		EXPECT_SUCCESS_INT("verbosity", verbosity, 2);
		break;
	    case 8:
		ac = 2;
		av[0] = "-v";
		av[1] = "4";
		EXPECT_FAILURE("verbosity", "4 > 3");
		break;
	    case 9:
		ac = 2;
		av[0] = "--verb";
		av[1] = "2";
		EXPECT_SUCCESS_INT("verbosity", verbosity, 2);
		break;
	    case 10:
		ac = 2;
		av[0] = "--verb";
		av[1] = "4";
		EXPECT_FAILURE("verbosity", "4 > 3");
		break;
	    case 11:
		ac = 1;
		av[0] = "--verb=2";
		EXPECT_SUCCESS_INT("verbosity", verbosity, 2);
		break;
	    case 12:
		ac = 1;
		av[0] = "--verb=4";
		EXPECT_FAILURE("verbosity", "4 > 3");
		break;
	    case 13:
		ac = 1;
		av[0] = "--v";
		EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
		break;
	    case 14:
		ac = 1;
		av[0] = "--verb";
		EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
		break;
	    default:
		bu_vls_printf(&parse_msgs, "unknown test: %d\n", test_num);
		return -1;
	}
    }
    if (cgy[0] == 'h') {
	switch (test_num) {
	    /* Help option tests */
	    case 1:
		ac = 1;
		av[0] = "-h";
		EXPECT_SUCCESS_INT("print_help", print_help, 1);
		break;
	    case 2:
		ac = 1;
		av[0] = "-?";
		EXPECT_SUCCESS_INT("print_help", print_help, 1);
		break;
	    case 3:
		ac = 1;
		av[0] = "--help";
		EXPECT_SUCCESS_INT("print_help", print_help, 1);
		break;
	    case 4:
		ac = 1;
		av[0] = "--help=4";
		EXPECT_FAILURE("print_help", "extra arg");
		break;
	    case 5:
		ac = 1;
		av[0] = "-?4";
		/* "-?4": first char '?' is a known flag, '4' is not.
		 * bu_opt_parse now treats such partially-known grouped
		 * flag strings as unknown positional args (rather than
		 * a fatal parse error) so that GED sub-command tokens
		 * like "-print" pass through unmolested.  The flag
		 * variable is NOT set because the whole token is
		 * discarded without processing any flags from it. */
		EXPECT_SUCCESS_INT_UNKNOWN("print_help", print_help, 0);
		break;
	    case 6:
		ac = 2;
		av[0] = "-?";
		av[1] = "4";
		EXPECT_SUCCESS_INT_UNKNOWN("print_help", print_help, 1);
		break;
	    case 7:
		ac = 1;
		av[0] = "-?=4";
		/* "-?=4": '?' is a known flag, '=' is not, so opt_process
		 * returns -1 (grouped-flag with unrecognized char).  Same
		 * as case 5, the whole token becomes an unknown positional
		 * arg and the flag variable is left unset. */
		EXPECT_SUCCESS_INT_UNKNOWN("print_help", print_help, 0);
		break;
	    case 8:
		ac = 1;
		av[0] = "--?4";
		EXPECT_FAILURE_INT_UNKNOWN("print_help", print_help, 1);
		break;
	    case 9:
		ac = 2;
		av[0] = "--?";
		av[1] = "4";
		EXPECT_SUCCESS_INT_UNKNOWN("print_help", print_help, 1);
		break;
	    case 10:
		ac = 1;
		av[0] = "--?=4";
		EXPECT_FAILURE("print_help", "extra arg");
		break;
	    default:
		bu_vls_printf(&parse_msgs, "unknown test: %d\n", test_num);
		return -1;
	}
    }
    if (cgy[0] == 'b') {
	switch (test_num) {
	    /* boolean option tests */
	    case 1:
		ac = 2;
		av[0] = "-b";
		av[1] = "true";
		EXPECT_SUCCESS_INT("bool", b, 1);
		break;
	    case 2:
		ac = 2;
		av[0] = "-b";
		av[1] = "false";
		EXPECT_SUCCESS_INT("bool", b, 0);
		break;
	    case 3:
		ac = 2;
		av[0] = "--bool";
		av[1] = "1";
		EXPECT_SUCCESS_INT("bool", b, 1);
		break;
	    case 4:
		ac = 2;
		av[0] = "--bool";
		av[1] = "0";
		EXPECT_SUCCESS_INT("bool", b, 0);
		break;
	    default:
		bu_vls_printf(&parse_msgs, "unknown test: %d\n", test_num);
		return -1;
	}
    }
    if (cgy[0] == 's') {
	switch (test_num) {
	    /* string option tests */
	    case 1:
		ac = 1;
		av[0] = "-s";
		EXPECT_FAILURE("string", "missing argument");
		break;
	    case 2:
		ac = 2;
		av[0] = "-s";
		av[1] = "test_str";
		EXPECT_SUCCESS_STRING("string", str, "test_str");
		break;
	    case 3:
		ac = 2;
		av[0] = "--vls1";
		av[1] = "vls_str";
		EXPECT_SUCCESS_STRING("vls", bu_vls_cstr(&vls), "vls_str");
		break;
	    case 4:
		ac = 2;
		av[0] = "-a";
		av[1] = "vls_str2";
		EXPECT_SUCCESS_STRING("vls", bu_vls_cstr(&vls2), "vls_str2");
		break;
	    case 5:
		ac = 2;
		av[0] = "--vls2";
		av[1] = "vls_str2";
		EXPECT_SUCCESS_STRING("vls", bu_vls_cstr(&vls2), "vls_str2");
		break;
	    default:
		bu_vls_printf(&parse_msgs, "unknown test: %d\n", test_num);
		return -1;
	}
    }
    if (cgy[0] == 'i') {
	/* int option tests */
	switch(test_num) {
	    case 1:
		ac = 1;
		av[0] = "-i";
		EXPECT_FAILURE("int_num", "missing arg");
		break;
	    case 2:
		ac = 2;
		av[0] = "-i";
		av[1] = "-f";
		EXPECT_FAILURE("int_num", "invalid arg");
		break;
	    case 3:
		ac = 2;
		av[0] = "-i";
		av[1] = "1";
		EXPECT_SUCCESS_INT("int_num", i, 1);
		break;
	    case 4:
		ac = 2;
		av[0] = "-i";
		av[1] = "-1";
		EXPECT_SUCCESS_INT("int_num", i, -1);
		break;
	    case 5:
		ac = 2;
		av[0] = "-i";
		av[1] = "214748364700";
		EXPECT_FAILURE("int_num", "number too large for int container");
		break;

	    default:
		bu_vls_printf(&parse_msgs, "unknown test: %d\n", test_num);
		return -1;

	}
    }
    if (cgy[0] == 'l') {
	/* long option tests */
	if (INT_MAX != LONG_MAX) {
	    switch(test_num) {
		case 1:
		    //ac = 2;
		    //av[0] = "-l";
		    //av[1] = "214748364800";
		    /* TODO - how to prevent out-of-bounds errors on 32 bit platforms... */
		    /*EXPECT_SUCCESS_INT("long_num", l, 214748364800);*/
		    break;
		case 2:
		    //ac = 2;
		    //av[0] = "-l";
		    //av[1] = "-214748364800";
		    /* TODO - how to prevent out-of-bounds errors on 32 bit platforms... */
		    /*EXPECT_SUCCESS_INT("long_num", l, -214748364800);*/
		    break;
		default:
		    bu_vls_printf(&parse_msgs, "unknown test: %d\n", test_num);
		    return -1;

	    }
	} else {
	    switch(test_num) {
		case 1:
		    ac = 2;
		    av[0] = "-l";
		    av[1] = "21474836";
		    EXPECT_SUCCESS_INT("long_num", l, 21474836);
		    break;
		case 2:
		    ac = 2;
		    av[0] = "-l";
		    av[1] = "-21474836";
		    EXPECT_SUCCESS_INT("long_num", l, -21474836);
		    break;
		default:
		    bu_vls_printf(&parse_msgs, "unknown test: %d\n", test_num);
		    return -1;

	    }
	}
    }
    if (cgy[0] == 'f') {
	/* fastf_t option tests */
	switch (test_num) {
	    case 1:
		ac = 2;
		av[0] = "-f";
		av[1] = "1.234";
		EXPECT_SUCCESS_FLOAT("float_num", f, 1.234);
		break;
	    case 2:
		ac = 2;
		av[0] = "-f";
		av[1] = "-1.234";
		EXPECT_SUCCESS_FLOAT("float_num", f, -1.234);
		break;
	    case 3:
		ac = 2;
		av[0] = "-f";
		av[1] = "-3.0e-3";
		EXPECT_SUCCESS_FLOAT("float_num", f, -0.003);
		break;
	    default:
		bu_vls_printf(&parse_msgs, "unknown test: %d\n", test_num);
		return -1;
	}
    }

    if (cgy[0] == 'm') {
	switch (test_num) {
	    /* boolean option tests */
	    case 1:
		ac = 1;
		av[0] = "-Fm";
		EXPECT_SUCCESS_FLAG("bool", m);
		break;
	    case 2:
		ac = 1;
		av[0] = "-mF";
		EXPECT_SUCCESS_FLAG("bool", m);
		break;
	    default:
		bu_vls_printf(&parse_msgs, "unknown test: %d\n", test_num);
		return -1;
	}
    }


    if (ret > 0) {
	int u = 0;
	bu_vls_printf(&parse_msgs, "\nUnknown args: ");
	for (u = 0; u < ret - 1; u++) {
	    bu_vls_printf(&parse_msgs, "%s, ", av[u]);
	}
	bu_vls_printf(&parse_msgs, "%s\n", av[ret - 1]);
    }

    ret = (!val_ok) ? -1 : 0;

    if (bu_vls_strlen(&parse_msgs) > 0) {
	bu_log("%s\n", bu_vls_addr(&parse_msgs));
    }
    bu_vls_free(&parse_msgs);
    bu_free((void *)av, "free av");
    return ret;
}


static int
dc_color(struct bu_vls *msg, size_t argc, const char **argv, void *set_c)
{
    struct bu_color *set_color = (struct bu_color *)set_c;
    unsigned int rgb[3] = {0, 0, 0};

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "color");

    /* First, see if the first string converts to rgb */
    if (!bu_str_to_rgb((char *)argv[0], (unsigned char *)&rgb)) {
	/* nope - maybe we have 3 args? */
	if (argc >= 3) {
	    struct bu_vls tmp_color = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&tmp_color, "%s/%s/%s", argv[0], argv[1], argv[2]);
	    if (!bu_str_to_rgb(bu_vls_addr(&tmp_color), (unsigned char *)&rgb)) {
		/* Not valid with 3 */
		bu_vls_free(&tmp_color);
		if (msg)
		    bu_vls_sprintf(msg, "No valid color found.\n");
		return -1;
	    } else {
		/* 3 did the job */
		bu_vls_free(&tmp_color);
		if (set_color)
		    (void)bu_color_from_rgb_chars(set_color, (unsigned char *)&rgb);
		return 3;
	    }
	} else {
	    /* Not valid with 1 and don't have 3 - we require at least one, so
	     * claim one argv as belonging to this option regardless. */
	    if (msg)
		bu_vls_sprintf(msg, "No valid color found: %s\n", argv[0]);
	    return -1;
	}
    } else {
	/* yep, 1 did the job */
	if (set_color)
	    (void)bu_color_from_rgb_chars(set_color, (unsigned char *)&rgb);
	return 1;
    }

    return -1;
}


static int
desc_2(int test_num)
{
    int ret = 0;
    int val_ok = 1;
    int print_help = 0;
    struct bu_color color = BU_COLOR_INIT_ZERO;
    int containers = 7;
    int ac = 0;
    const char **av;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",  "",      NULL,      &print_help, help_str);
    BU_OPT(d[1], "C", "color", "r/g/b", &dc_color, &color,      "Set color");
    BU_OPT_NULL(d[2]);

    av = (const char **)bu_calloc(containers, sizeof(char *), "Input array");

    switch (test_num) {
	case 0:
	    ret = bu_opt_parse(&parse_msgs, 0, NULL, d);
	    ret = (ret == -1) ? 0 : -1;
	    break;
	case 1:
	    ac = 2;
	    av[0] = "-C";
	    av[1] = "200/10/30";
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 2:
	    ac = 4;
	    av[0] = "-C";
	    av[1] = "200";
	    av[2] = "10";
	    av[3] = "30";
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 3:
	    ac = 4;
	    av[0] = "-C";
	    av[1] = "200/10/30";
	    av[2] = "50";
	    av[3] = "100";
	    EXPECT_SUCCESS_COLOR_UNKNOWN("color", color, 200, 10, 30);
	    break;
	case 4:
	    ac = 6;
	    av[0] = "-C";
	    av[1] = "200";
	    av[2] = "10";
	    av[3] = "30";
	    av[4] = "50";
	    av[5] = "100";
	    EXPECT_SUCCESS_COLOR_UNKNOWN("color", color, 200, 10, 30);
	    break;
	case 5:
	    ac = 2;
	    av[0] = "--color";
	    av[1] = "200/10/30";
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 6:
	    ac = 4;
	    av[0] = "--color";
	    av[1] = "200";
	    av[2] = "10";
	    av[3] = "30";
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 7:
	    ac = 4;
	    av[0] = "--color";
	    av[1] = "200/10/30";
	    av[2] = "50";
	    av[3] = "100";
	    EXPECT_SUCCESS_COLOR_UNKNOWN("color", color, 200, 10, 30);
	    break;
	case 8:
	    ac = 6;
	    av[0] = "--color";
	    av[1] = "200";
	    av[2] = "10";
	    av[3] = "30";
	    av[4] = "50";
	    av[5] = "100";
	    EXPECT_SUCCESS_COLOR_UNKNOWN("color", color, 200, 10, 30);
	    break;
	case 9:
	    ac = 1;
	    av[0] = "-C200/10/30";
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 10:
	    ac = 1;
	    av[0] = "-C=200/10/30";
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 11:
	    ac = 3;
	    av[0] = "-C200";
	    av[1] = "10";
	    av[2] = "30";
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 12:
	    ac = 3;
	    av[0] = "-C=200";
	    av[1] = "10";
	    av[2] = "30";
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 13:
	    ac = 1;
	    av[0] = "-C";
	    EXPECT_FAILURE("color", "missing argument");
	    break;
	case 14:
	    ac = 1;
	    av[0] = "--color";
	    EXPECT_FAILURE("color", "missing argument");
	    break;
	case 15:
	    ac = 5;
	    av[0] = "--color";
	    av[1] = "file";
	    av[2] = "10";
	    av[3] = "30";
	    av[4] = "50";
	    EXPECT_FAILURE("color", "invalid argument");
	    break;
	case 16:
	    ac = 1;
	    av[0] = "-C0/0/50";
	    EXPECT_SUCCESS_COLOR("color", color, 0, 0, 50);
	    break;
	case 17:
	    ac = 2;
	    av[0] = "-C";
	    av[1] = "200;10;30";
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 18:
	    ac = 2;
	    av[0] = "-C";
	    av[1] = "200,10,30";
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
    }

    if (ret > 0) {
	int u = 0;
	bu_vls_printf(&parse_msgs, "\nUnknown args: ");
	for (u = 0; u < ret - 1; u++) {
	    bu_vls_printf(&parse_msgs, "%s, ", av[u]);
	}
	bu_vls_printf(&parse_msgs, "%s\n", av[ret - 1]);
    }

    ret = (!val_ok) ? -1 : 0;

    if (bu_vls_strlen(&parse_msgs) > 0) {
	bu_log("%s\n", bu_vls_addr(&parse_msgs));
    }
    bu_vls_free(&parse_msgs);
    bu_free((void *)av, "free av");
    return ret;
}


static int
desc_3(int test_num)
{
    int ret = 0;
    int val_ok = 1;
    int print_help = 0;
    vect_t v = {0, 0, 0};
    int ac = 0;
    const char **av;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",   "",      NULL,           &print_help, help_str);
    BU_OPT(d[1], "V", "vector", "x, y, z", &bu_opt_vect_t, &v,          "Set vector");
    BU_OPT_NULL(d[2]);

    av = (const char **)bu_calloc(5, sizeof(char *), "Input array");

    switch (test_num) {
	case 0:
	    ac = 2;
	    av[0] = "-V";
	    av[1] = "2, 10, 30";
	    EXPECT_SUCCESS_VECT("vect_t", v, 2.0, 10.0, 30.0);
	    break;
	case 1:
	    ac = 2;
	    av[0] = "-V";
	    av[1] = "2/10/30";
	    EXPECT_SUCCESS_VECT("vect_t", v, 2.0, 10.0, 30.0);
	    break;
	case 2:
	    ac = 2;
	    av[0] = "-V";
	    av[1] = "30.3, 2, -10.1";
	    EXPECT_SUCCESS_VECT("vect_t", v, 30.3, 2.0, -10.1);
	    break;
	case 3:
	    ac = 2;
	    av[0] = "-V";
	    av[1] = "30.3, 2, -10.1";
	    EXPECT_SUCCESS_VECT("vect_t", v, 30.3, 2.0, -10.1);
	    break;
	case 4:
	    ac = 4;
	    av[0] = "-V";
	    av[1] = "30.3";
	    av[2] = "2";
	    av[3] = "-10.1";
	    EXPECT_SUCCESS_VECT("vect_t", v, 30.3, 2.0, -10.1);
	    break;

    }

    if (ret > 0) {
	int u = 0;
	bu_vls_printf(&parse_msgs, "\nUnknown args: ");
	for (u = 0; u < ret - 1; u++) {
	    bu_vls_printf(&parse_msgs, "%s, ", av[u]);
	}
	bu_vls_printf(&parse_msgs, "%s\n", av[ret - 1]);
    }

    ret = (!val_ok) ? -1 : 0;

    if (bu_vls_strlen(&parse_msgs) > 0) {
	bu_log("%s\n", bu_vls_addr(&parse_msgs));
    }
    bu_vls_free(&parse_msgs);
    bu_free((void *)av, "free av");
    return ret;
}


static int
desc_4(int test_num)
{
    static const char *pattern_keywords[] = {"alpha", "beta", NULL};
    static const char *mode_keywords[] = {"fast", "slow", NULL};
    int help = 0;
    int verbose = 0;
    const char *output = NULL;
    const char *point = NULL;
    const char *mode = NULL;
    struct bu_color color = BU_COLOR_INIT_ZERO;
    struct bu_opt_validate_result vr = BU_OPT_VALIDATE_RESULT_NULL;
    int ret = 0;
    char *json = NULL;
    static const struct bu_opt_arg_shape point_shape = {BU_OPT_SHAPE_TOKEN_SEQUENCE, 3, 3, "x y z"};

    struct bu_opt_desc root_opts[] = {
	{"h", "help", "", NULL, (void *)&help, help_str},
	{"?", "help-alt", "", NULL, (void *)&help, help_str},
	{"v", "verbose", "[#]", &d1_verb, (void *)&verbose, "Set verbosity"},
	{"o", "output", "file", &bu_opt_str, (void *)&output, "Output file"},
	{"C", "color", "r/g/b", &dc_color, (void *)&color, "Set color"},
	{"p", "point", "x y z", &bu_opt_str, (void *)&point, "Set point"},
	BU_OPT_DESC_NULL
    };
    struct bu_opt_desc_meta root_meta[] = {
	{"help", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0},
	{"verbose", BU_OPT_ARG_OPTIONAL, BU_OPT_VAL_INTEGER, 1, NULL, NULL, NULL, NULL, NULL, NULL, 0},
	{"output", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_FILE_PATH, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0},
	{"color", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_COLOR, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0},
	{"point", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_VECTOR, 0, NULL, NULL, &point_shape, "point", "geometry", NULL, 0},
	BU_OPT_DESC_META_NULL
    };
    struct bu_opt_operand_desc root_operands[] = {
	{"object", BU_OPT_VAL_DB_OBJECT, 1, 2, "Database object", NULL, NULL, NULL},
	BU_OPT_OPERAND_DESC_NULL
    };
    struct bu_opt_desc list_opts[] = {
	{"l", "long", "", NULL, (void *)&help, "Long listing"},
	BU_OPT_DESC_NULL
    };
    struct bu_opt_operand_desc list_operands[] = {
	{"pattern", BU_OPT_VAL_KEYWORD, 0, 1, "Optional pattern", pattern_keywords, NULL, NULL},
	BU_OPT_OPERAND_DESC_NULL
    };
    struct bu_opt_desc deep_opts[] = {
	{"m", "mode", "mode", &bu_opt_str, (void *)&mode, "Mode"},
	BU_OPT_DESC_NULL
    };
    struct bu_opt_desc_meta deep_meta[] = {
	{"mode", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_KEYWORD, 0, mode_keywords, NULL, NULL, NULL, NULL, NULL, 0},
	BU_OPT_DESC_META_NULL
    };
    struct bu_opt_cmd_desc deep_cmds[] = {
	{"deep", "Nested command", deep_opts, deep_meta, NULL, NULL, NULL, BU_OPT_PARSE_OPTIONS_INTERSPERSED, 0, NULL},
	BU_OPT_CMD_DESC_NULL
    };
    struct bu_opt_cmd_desc subcmds[] = {
	{"list", "List things", list_opts, NULL, list_operands, deep_cmds, NULL, BU_OPT_PARSE_OPTIONS_INTERSPERSED, 0, NULL},
	BU_OPT_CMD_DESC_NULL
    };
    struct bu_opt_cmd_desc cmd = {"testcmd", "Test command", root_opts, root_meta, root_operands, subcmds, NULL, BU_OPT_PARSE_OPTIONS_INTERSPERSED, 0, NULL};

    switch (test_num) {
	case 0:
	    json = bu_opt_describe_json(&cmd);
	    ret = (!json ||
		!strstr(json, "\"parse_policy\":\"options_interspersed\"") ||
		!strstr(json, "\"schema_version\":0") ||
		!strstr(json, "\"short\":\"p\"") ||
		!strstr(json, "\"argument_type\":\"vector\"") ||
		!strstr(json, "\"kind\":\"token_sequence\"") ||
		!strstr(json, "\"min_tokens\":3") ||
		!strstr(json, "\"max_tokens\":3") ||
		!strstr(json, "\"canonical\":\"point\"") ||
		!strstr(json, "\"conflict_group\":\"geometry\""));
	    if (ret) {
		bu_log("JSON missing expected Wave 0 metadata:\n%s\n", json ? json : "(null)");
	    }
	    bu_free(json, "json");
	    break;
	case 1:
	{
	    const char *av[] = {"-v", "2", "--output", "out.g", "obj"};
	    (void)bu_opt_validate_argv(&cmd, 5, av, 5, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_VALID);
	    break;
	}
	case 2:
	{
	    const char *av[] = {"--bogus", "obj"};
	    (void)bu_opt_validate_argv(&cmd, 2, av, 0, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_INVALID && vr.token_start == 0);
	    break;
	}
	case 3:
	{
	    const char *av[] = {"--output"};
	    (void)bu_opt_validate_argv(&cmd, 1, av, 1, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_INCOMPLETE && (vr.expected & BU_OPT_EXPECT_OPTION_ARG));
	    break;
	}
	case 4:
	{
	    const char *av[] = {"--output", "out.g"};
	    (void)bu_opt_validate_argv(&cmd, 2, av, 2, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_INCOMPLETE && (vr.expected & BU_OPT_EXPECT_OPERAND));
	    break;
	}
	case 5:
	{
	    const char *av[] = {"obj1", "obj2", "obj3"};
	    (void)bu_opt_validate_argv(&cmd, 3, av, 3, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_INVALID);
	    break;
	}
	case 6:
	{
	    const char *av[] = {"--output", "a", "--output", "b", "obj"};
	    (void)bu_opt_validate_argv(&cmd, 5, av, 2, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_INVALID);
	    break;
	}
	case 7:
	{
	    const char *av[] = {"-v", "1", "-v", "2", "obj"};
	    (void)bu_opt_validate_argv(&cmd, 5, av, 5, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_VALID);
	    break;
	}
	case 8:
	{
	    const char *av[] = {"list", "--long"};
	    (void)bu_opt_validate_argv(&cmd, 2, av, 2, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_VALID);
	    break;
	}
	case 9:
	{
	    const char *av[] = {"list", "deep", "--mode", "fast"};
	    (void)bu_opt_validate_argv(&cmd, 4, av, 4, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_VALID);
	    break;
	}
	case 10:
	    (void)bu_opt_validate_string(&cmd, "--output", 8, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_INCOMPLETE && (vr.expected & BU_OPT_EXPECT_OPTION_ARG));
	    break;
	case 11:
	    (void)bu_opt_validate_string(&cmd, "", 0, &vr);
	    ret = !(vr.completion_count >= 2 && vr.completion_candidates
		&& vr.state == BU_OPT_VALIDATE_INCOMPLETE);
	    if (!ret) {
		int have_output = 0;
		int have_list = 0;
		size_t i = 0;
		for (i = 0; i < vr.completion_count; i++) {
		    if (BU_STR_EQUAL(vr.completion_candidates[i], "--output"))
			have_output = 1;
		    if (BU_STR_EQUAL(vr.completion_candidates[i], "list"))
			have_list = 1;
		}
		ret = !(have_output && have_list);
	    }
	    break;
	case 12:
	    (void)bu_opt_validate_string(&cmd, "--out", 5, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_INVALID && vr.completion_count == 1
		&& vr.completion_candidates
		&& BU_STR_EQUAL(vr.completion_candidates[0], "--output"));
	    break;
	case 13:
	    (void)bu_opt_validate_string(&cmd, "li", 2, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_VALID && vr.completion_count >= 1
		&& vr.completion_candidates
		&& BU_STR_EQUAL(vr.completion_candidates[0], "list"));
	    break;
	case 14:
	    (void)bu_opt_validate_string(&cmd, "list deep --mode fa", 19, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_VALID && vr.expected == BU_OPT_EXPECT_OPTION_ARG
		&& vr.completion_count == 1 && vr.completion_candidates
		&& BU_STR_EQUAL(vr.completion_candidates[0], "fast"));
	    break;
	case 15:
	{
	    const char *av[] = {"-hh", "obj"};
	    (void)bu_opt_validate_argv(&cmd, 2, av, 0, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_INVALID);
	    break;
	}
	case 16:
	    /* completion_type: option needing a file path arg */
	    (void)bu_opt_validate_string(&cmd, "--output", 8, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_INCOMPLETE
		&& vr.completion_type == BU_OPT_VAL_FILE_PATH);
	    break;
	case 17:
	    /* completion_type: db_object operand position */
	    (void)bu_opt_validate_string(&cmd, "--help ", 7, &vr);
	    ret = !(vr.completion_type == BU_OPT_VAL_DB_OBJECT);
	    break;
	case 18:
	    /* completion_type: keyword option arg */
	    (void)bu_opt_validate_string(&cmd, "list deep --mode ", 17, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_INCOMPLETE
		&& vr.completion_type == BU_OPT_VAL_KEYWORD);
	    break;
	case 19:
	{
	    /* char_start/char_end: bad option highlighted at correct offset */
	    /* "--out" starts at char 0, ends at char 5 */
	    (void)bu_opt_validate_string(&cmd, "--out", 5, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_INVALID
		&& vr.char_start == 0 && vr.char_end == 5);
	    break;
	}
	case 20:
	{
	    /* char_start/char_end: bad option in the middle of a string */
	    /* "obj --out" → bad option "--out" starts at byte 4, ends at 9 */
	    (void)bu_opt_validate_string(&cmd, "obj --out", 9, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_INVALID
		&& vr.char_start == 4 && vr.char_end == 9);
	    break;
	}
	case 21:
	{
	    struct bu_opt_cmd_desc policy_cmd = cmd;
	    const char *av[] = {"obj", "--output"};
	    policy_cmd.parse_policy = BU_OPT_PARSE_OPTIONS_BEFORE_OPERANDS;
	    (void)bu_opt_validate_argv(&policy_cmd, 2, av, 2, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_VALID);
	    break;
	}
	case 22:
	{
	    const char *av[] = {"--point", "1", "2", "3", "obj"};
	    (void)bu_opt_validate_argv(&cmd, 5, av, 5, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_VALID);
	    break;
	}
	case 23:
	{
	    const char *av[] = {"--point", "1", "2"};
	    (void)bu_opt_validate_argv(&cmd, 3, av, 3, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_INCOMPLETE && (vr.expected & BU_OPT_EXPECT_OPTION_ARG));
	    break;
	}
	case 24:
	{
	    int shared_schema_storage = 0;
	    struct bu_opt_desc opts[] = {
		{"a", "alpha", "", NULL, &shared_schema_storage, "Alpha"},
		{"b", "beta", "", NULL, &shared_schema_storage, "Beta"},
		BU_OPT_DESC_NULL
	    };
	    struct bu_opt_desc_meta metas[] = {
		{"alpha", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL, NULL, NULL, "--alpha", NULL, NULL, 0},
		{"beta", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL, NULL, NULL, "--beta", NULL, NULL, 0},
		BU_OPT_DESC_META_NULL
	    };
	    struct bu_opt_cmd_desc scmd = {"shared", "", opts, metas, root_operands, NULL, NULL, BU_OPT_PARSE_OPTIONS_INTERSPERSED, 1, NULL};
	    const char *av[] = {"-a", "-b", "obj"};
	    (void)bu_opt_validate_argv(&scmd, 3, av, 3, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_VALID);
	    if (!ret) {
		struct bu_opt_desc legacy_opts[] = {
		    {"noleaf", "noleaf", "", NULL, &shared_schema_storage, "No leaf"},
		    BU_OPT_DESC_NULL
		};
		struct bu_opt_desc_meta legacy_meta[] = {
		    {"noleaf", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL, NULL, NULL, "--noleaf", NULL, NULL, 0},
		    BU_OPT_DESC_META_NULL
		};
		struct bu_opt_cmd_desc legacy_cmd = {"legacy", "", legacy_opts, legacy_meta, NULL, NULL, NULL, BU_OPT_PARSE_OPTIONS_INTERSPERSED, 1, NULL};
		const char *legacy_av[] = {"-noleaf"};
		bu_opt_validate_result_clear(&vr);
		(void)bu_opt_validate_argv(&legacy_cmd, 1, legacy_av, 1, &vr);
		ret = !(vr.state == BU_OPT_VALIDATE_VALID);
	    }
	    break;
	}
	case 25:
	{
	    int shared_schema_storage = 0;
	    struct bu_opt_desc opts[] = {
		{"a", "alpha", "", NULL, &shared_schema_storage, "Alpha"},
		{"A", "", "", NULL, &shared_schema_storage, "Alpha alias"},
		BU_OPT_DESC_NULL
	    };
	    struct bu_opt_desc_meta metas[] = {
		{"alpha", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL, NULL, NULL, "--alpha", NULL, NULL, 0},
		{"A", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL, NULL, NULL, "--alpha", NULL, NULL, 0},
		BU_OPT_DESC_META_NULL
	    };
	    struct bu_opt_cmd_desc scmd = {"aliases", "", opts, metas, root_operands, NULL, NULL, BU_OPT_PARSE_OPTIONS_INTERSPERSED, 1, NULL};
	    const char *av[] = {"-a", "-A", "obj"};
	    (void)bu_opt_validate_argv(&scmd, 3, av, 3, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_INVALID);
	    break;
	}
	case 26:
	{
	    int storage = 0;
	    struct bu_opt_desc opts[] = {
		{"x", "", "", NULL, &storage, "Actual spelling"},
		BU_OPT_DESC_NULL
	    };
	    struct bu_opt_desc_meta metas[] = {
		{"x", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL, NULL, NULL, "--missing", NULL, NULL, 0},
		BU_OPT_DESC_META_NULL
	    };
	    struct bu_opt_cmd_desc scmd = {"canonical", "", opts, metas, NULL, NULL, NULL, BU_OPT_PARSE_OPTIONS_INTERSPERSED, 1, NULL};
	    (void)bu_opt_validate_string(&scmd, "-", 1, &vr);
	    ret = !(vr.completion_count == 1 && vr.completion_candidates &&
		    BU_STR_EQUAL(vr.completion_candidates[0], "-x"));
	    break;
	}
	case 27:
	{
	    int storage = 0;
	    struct bu_opt_desc opts[] = {
		{"", "format=json", "", NULL, &storage, "Literal equals spelling"},
		BU_OPT_DESC_NULL
	    };
	    struct bu_opt_cmd_desc scmd = {"equals", "", opts, NULL, NULL, NULL, NULL, BU_OPT_PARSE_OPTIONS_INTERSPERSED, 1, NULL};
	    const char *av[] = {"--format=json"};
	    (void)bu_opt_validate_argv(&scmd, 1, av, 1, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_VALID);
	    break;
	}
	case 28:
	{
	    static const char *keywords[] = {"--id", "--name", NULL};
	    struct bu_opt_operand_desc operands[] = {
		{"mode", BU_OPT_VAL_KEYWORD, 1, 1, "Dash-prefixed operand", keywords, NULL, NULL},
		BU_OPT_OPERAND_DESC_NULL
	    };
	    struct bu_opt_cmd_desc scmd = {"operand", "", NULL, NULL, operands, NULL, NULL, BU_OPT_PARSE_STOP_AT_FIRST_OPERAND, 1, NULL};
	    const char *av[] = {"--id"};
	    (void)bu_opt_validate_argv(&scmd, 1, av, 1, &vr);
	    ret = !(vr.state == BU_OPT_VALIDATE_VALID);
	    if (!ret) {
		bu_opt_validate_result_clear(&vr);
		(void)bu_opt_validate_string(&scmd, "--i", 3, &vr);
		ret = !(vr.completion_count == 1 && vr.completion_candidates &&
			BU_STR_EQUAL(vr.completion_candidates[0], "--id"));
	    }
	    break;
	}
	default:
	    ret = -1;
	    break;
    }

    if (ret) {
	bu_log("bu_opt metadata test %d failed: state=%d start=%lu end=%lu "
	    "expected=%u completions=%lu comp_type=%d char=[%lu,%lu) hint=%s\n",
	    test_num, vr.state,
	    (unsigned long)vr.token_start, (unsigned long)vr.token_end,
	    vr.expected, (unsigned long)vr.completion_count,
	    (int)vr.completion_type,
	    (unsigned long)vr.char_start, (unsigned long)vr.char_end,
	    vr.hint ? vr.hint : "(null)");
    }

    bu_opt_validate_result_clear(&vr);

    return ret;
}


int
main(int argc, char *argv[])
{
    int ret = -1;
    long desc_num;
    long test_num;
    const char *cgy = NULL;
    char *endptr = NULL;

    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    /* Sanity check */
    if (argc < 4) {
	bu_log("Usage: %s {desc_number} {category_num} {test_num}\n", argv[0]);
	bu_exit(1, "ERROR: wrong number of parameters - need desc num, category and test num\n");
    }

    /* Set the option description to used based on the input number */
    desc_num = strtol(argv[1], &endptr, 0);
    if (endptr && strlen(endptr) != 0) {
	bu_exit(1, "Invalid desc number: %s\n", argv[1]);
    }

    cgy = argv[2];

    test_num = strtol(argv[3], &endptr, 0);
    if (endptr && strlen(endptr) != 0) {
	bu_exit(2, "Invalid test number: %s\n", argv[2]);
    }

    switch (desc_num) {
	case 0:
	    return !(bu_opt_parse(NULL, 0, NULL, NULL) == -1);
	    break;
	case 1:
	    return desc_1(cgy, test_num);
	    break;
	case 2:
	    return desc_2(test_num);
	    break;
	case 3:
	    return desc_3(test_num);
	    break;
	case 4:
	    return desc_4(test_num);
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
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */

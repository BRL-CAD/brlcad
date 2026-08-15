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
completion_has(const struct bu_opt_validate_result *result, const char *value)
{
    if (!result || !value)
	return 0;
    for (size_t i = 0; i < result->completion_count; i++)
	if (result->completion_candidates[i] &&
	    BU_STR_EQUAL(result->completion_candidates[i], value))
	    return 1;
    return 0;
}


static int
opt_sidecar_validate(const struct bu_opt_desc *option, size_t argc,
	const char **argv, size_t cursor_arg, void *context, void *data,
	struct bu_opt_validate_result *result)
{
    int *calls = (int *)data;

    if (!option || !argc || !argv || cursor_arg >= argc || context != data ||
	!result)
	return -1;
    (*calls)++;
    return 0;
}


static int
opt_attached_sidecar_validate(const struct bu_opt_desc *option, size_t argc,
	const char **argv, size_t cursor_arg, void *context, void *data,
	struct bu_opt_validate_result *result)
{
    int *calls = (int *)data;

    if (!option || !argv || cursor_arg >= argc || context != data || !result ||
	    !BU_STR_EQUAL(argv[cursor_arg], "fast"))
	return -1;
    (*calls)++;
    result->state = BU_OPT_VALIDATE_VALID;
    return 0;
}


static int
opt_failing_sidecar_validate(const struct bu_opt_desc *UNUSED(option),
	size_t UNUSED(argc), const char **UNUSED(argv),
	size_t UNUSED(cursor_arg), void *UNUSED(context), void *UNUSED(data),
	struct bu_opt_validate_result *result)
{
    result->completion_candidates = (const char **)bu_calloc(2,
	sizeof(char *), "failing option validation candidates");
    result->completion_candidates[0] = bu_strdup("discard-me");
    result->completion_count = 1;
    result->hint = "discard this partial result";
    return -1;
}


static int
opt_pair(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(data))
{
    if (argc < 2 || !argv || !argv[0] || !argv[1]) {
	if (msg)
	    bu_vls_printf(msg, "two values required\n");
	return -1;
    }
    return 2;
}


static int
opt_custom_flag(struct bu_vls *UNUSED(msg), size_t UNUSED(argc),
	const char **UNUSED(argv), void *data)
{
    int *value = (int *)data;

    if (value)
	*value = 1;
    return 0;
}


struct opt_callback_state {
    int calls;
    const char *value;
};


static int
opt_attached_once(struct bu_vls *UNUSED(msg), size_t argc, const char **argv,
	void *data)
{
    struct opt_callback_state *state = (struct opt_callback_state *)data;

    /* Legacy callbacks are allowed to require their descriptor storage. */
    state->calls++;
    if (!argc || !argv || !argv[0])
	return -1;
    state->value = argv[0];
    return 1;
}


static int
opt_flag_once(struct bu_vls *UNUSED(msg), size_t UNUSED(argc),
	const char **UNUSED(argv), void *data)
{
    struct opt_callback_state *state = (struct opt_callback_state *)data;

    state->calls++;
    return 0;
}


struct opt_builder_args {
    int help;
    int number;
};


#define OPT_VALIDATION_OPTIONS(args) \
    {"h", "help", "", NULL, args ? &args->help : NULL, "Print help"}, \
    {"n", "number", "integer", bu_opt_int, args ? &args->number : NULL, \
	"Set an integer"},

BU_OPT_DESC_BUILDER(opt_validation_builder, struct opt_builder_args,
    OPT_VALIDATION_OPTIONS);


static size_t
opt_overflow_builder(struct bu_opt_desc *UNUSED(descs),
	size_t UNUSED(capacity), void *UNUSED(storage))
{
    return (size_t)-1;
}


static int
desc_validation(int test_num)
{
    static int opt_custom_calls = 0;
    int flag = 0;
    int boolean = 0;
    int integer = 0;
    long long_value = 0;
    fastf_t number = 0.0;
    char character = '\0';
    const char *string = NULL;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    struct bu_color color = BU_COLOR_INIT_ZERO;
    vect_t vector = VINIT_ZERO;
    const char *opaque = NULL;
    struct bu_opt_desc descs[] = {
	{"h", "help", "", NULL, &flag, "Print help"},
	{"n", "number", "integer", bu_opt_int, &integer, "Set an integer"},
	{"C", "color", "color", bu_opt_color, &color, "Set a color"},
	{NULL, "custom", "value", d1_verb, &opaque, "Opaque custom value"},
	{"b", "bool", "boolean", bu_opt_bool, &boolean, "Set a boolean"},
	{"l", "long", "integer", bu_opt_long, &long_value, "Set a long"},
	{"X", "hex", "hexadecimal", bu_opt_long_hex, &long_value, "Set a hexadecimal long"},
	{"f", "fastf", "number", bu_opt_fastf_t, &number, "Set a number"},
	{"c", "char", "character", bu_opt_char, &character, "Set a character"},
	{"s", "string", "text", bu_opt_str, &string, "Set a string"},
	{"v", "vls", "text", bu_opt_vls, &vls, "Append text"},
	{"V", "vector", "x/y/z", bu_opt_vect_t, &vector, "Set a vector"},
	{"I", "increment", "", bu_opt_incr_long, &long_value, "Increment a counter"},
	{"L", "language", "code", bu_opt_lang, &vls, "Set a language"},
	{"M", "man-section", "section", bu_opt_man_section, &character, "Set a manual section"},
	{"P", "pair", "first second", opt_pair, &opaque, "Set a custom pair"},
	BU_OPT_DESC_NULL
    };
    const char *mode_candidates[] = {"fast", "thorough", NULL};
    struct bu_opt_value_spec specs[] = {
	{"custom", NULL, BU_OPT_VALUE_STRING, 1, 1, NULL, "mode", mode_candidates,
	    opt_sidecar_validate,
	    &opt_custom_calls},
	{"pair", NULL, BU_OPT_VALUE_STRING, 2, 2, NULL, "two values", NULL, NULL, NULL},
	BU_OPT_VALUE_SPEC_NULL
    };
    struct bu_opt_validate_result result = BU_OPT_VALIDATE_RESULT_NULL;
    const char *dash[] = {"--"};
    const char *number_bad[] = {"--number", "abc"};
    const char *number_good[] = {"--number", "42"};
    const char *color_partial[] = {"--color", "10", "20"};
    const char *color_good[] = {"--color", "10", "20", "30"};
    const char *custom[] = {"--custom", "f"};
    const char *custom_missing[] = {"--custom"};
    const char *custom_option_seed[] = {"--cu"};
    const char *custom_done[] = {"--custom", "fast", "operand"};
    const char *built_parse[] = {"--number", "7"};
    const char *built_options_first[] = {"--number", "7", "operand", "--help"};
    const char *built_options_unknown[] = {"--unknown", "operand"};
    const char *built_options_marker[] = {"--number", "7", "--", "--help"};
    const char *built_complete[] = {"--n"};
    const char *bool_good[] = {"--bool", "true"};
    const char *bool_seed[] = {"--bool", "t"};
    const char *bool_bad[] = {"--bool", "perhaps"};
    const char *bool_empty[] = {"--bool", ""};
    const char *long_good[] = {"--long", "123"};
    const char *long_bad[] = {"--long", "12x"};
    const char *hex_good[] = {"--hex", "ff"};
    const char *hex_bad[] = {"--hex", "not-hex"};
    const char *fast_good[] = {"--fastf", "1.5"};
    const char *fast_bad[] = {"--fastf", "1.5x"};
    const char *char_good[] = {"--char", "word"};
    const char *string_good[] = {"--string", "text"};
    const char *vls_good[] = {"--vls", "text"};
    const char *vector_good[] = {"--vector", "1/2/3"};
    const char *vector_bad[] = {"--vector", "1/two/3"};
    const char *increment_good[] = {"--increment"};
    const char *language_good[] = {"--language", "en"};
    const char *language_bad[] = {"--language", "zz"};
    const char *man_good[] = {"--man-section", "3"};
    const char *man_seed[] = {"--man-section", ""};
    const char *man_bad[] = {"--man-section", "2"};
    const char *pair_partial[] = {"--pair", "one"};
    const char *pair_good[] = {"--pair", "one", "two"};
    struct opt_builder_args built_args = {0, 0};
    int ret = 1;

    switch (test_num) {
	case 0:
	    ret = bu_opt_desc_value_type(&descs[0]) != BU_OPT_VALUE_FLAG ||
		bu_opt_desc_value_type(&descs[1]) != BU_OPT_VALUE_INTEGER ||
		bu_opt_desc_value_type(&descs[2]) != BU_OPT_VALUE_COLOR ||
		bu_opt_desc_value_type(&descs[3]) != BU_OPT_VALUE_UNKNOWN ||
		bu_opt_desc_value_type(&descs[4]) != BU_OPT_VALUE_BOOL ||
		bu_opt_desc_value_type(&descs[5]) != BU_OPT_VALUE_LONG ||
		bu_opt_desc_value_type(&descs[6]) != BU_OPT_VALUE_HEX_LONG ||
		bu_opt_desc_value_type(&descs[7]) != BU_OPT_VALUE_NUMBER ||
		bu_opt_desc_value_type(&descs[8]) != BU_OPT_VALUE_CHAR ||
		bu_opt_desc_value_type(&descs[9]) != BU_OPT_VALUE_STRING ||
		bu_opt_desc_value_type(&descs[10]) != BU_OPT_VALUE_VLS ||
		bu_opt_desc_value_type(&descs[11]) != BU_OPT_VALUE_VECTOR ||
		bu_opt_desc_value_type(&descs[12]) != BU_OPT_VALUE_INCREMENT ||
		bu_opt_desc_value_type(&descs[13]) != BU_OPT_VALUE_LANGUAGE ||
		bu_opt_desc_value_type(&descs[14]) != BU_OPT_VALUE_MAN_SECTION ||
		bu_opt_desc_value_type(&descs[15]) != BU_OPT_VALUE_UNKNOWN;
	    break;
	case 1:
	    ret = bu_opt_desc_validate(descs, NULL, 0, NULL, 0, NULL, &result) ||
		!completion_has(&result, "--help") ||
		!completion_has(&result, "--number");
	    break;
	case 2:
	    ret = bu_opt_desc_validate(descs, NULL, 1, dash, 0, NULL, &result) ||
		!completion_has(&result, "--color");
	    break;
	case 3:
	    ret = bu_opt_desc_validate(descs, NULL, 2, number_bad, 1, NULL, &result) ||
		result.state != BU_OPT_VALIDATE_INVALID ||
		result.value_type != BU_OPT_VALUE_INTEGER;
	    break;
	case 4:
	    ret = bu_opt_desc_validate(descs, NULL, 2, number_good, 1, NULL, &result) ||
		result.state != BU_OPT_VALIDATE_VALID;
	    break;
	case 5:
	    ret = bu_opt_desc_validate(descs, NULL, 3, color_partial, 2, NULL, &result) ||
		result.state != BU_OPT_VALIDATE_INCOMPLETE;
	    break;
	case 6:
	    ret = bu_opt_desc_validate(descs, NULL, 4, color_good, 3, NULL, &result) ||
		result.state != BU_OPT_VALIDATE_VALID;
	    break;
	case 7:
	    ret = bu_opt_desc_validate(descs, specs, 2, custom, 1,
		&opt_custom_calls, &result) ||
		result.value_type != BU_OPT_VALUE_STRING ||
		!completion_has(&result, "fast") || opt_custom_calls != 1;
	    break;
	case 8:
	    ret = bu_opt_parse_build(NULL, 2, built_parse, opt_validation_builder,
		&built_args) != 0 || built_args.number != 7;
	    break;
	case 9:
	    ret = bu_opt_validate_build(opt_validation_builder, NULL, 1,
		built_complete, 0, NULL, &result) ||
		!completion_has(&result, "--number") || result.option != NULL;
	    break;
	case 10:
#define CHECK_STANDARD(_argv, _argc, _cursor, _state) do { \
	    if (bu_opt_desc_validate(descs, NULL, _argc, _argv, _cursor, NULL, &result) || \
		result.state != _state) \
		ret = 1; \
	    bu_opt_validate_result_clear(&result); \
	} while (0)
	    ret = 0;
	    CHECK_STANDARD(bool_good, 2, 1, BU_OPT_VALIDATE_VALID);
	    CHECK_STANDARD(bool_bad, 2, 1, BU_OPT_VALIDATE_INVALID);
	    CHECK_STANDARD(bool_empty, 2, 1, BU_OPT_VALIDATE_INVALID);
	    CHECK_STANDARD(long_good, 2, 1, BU_OPT_VALIDATE_VALID);
	    CHECK_STANDARD(long_bad, 2, 1, BU_OPT_VALIDATE_INVALID);
	    CHECK_STANDARD(hex_good, 2, 1, BU_OPT_VALIDATE_VALID);
	    CHECK_STANDARD(hex_bad, 2, 1, BU_OPT_VALIDATE_INVALID);
	    CHECK_STANDARD(fast_good, 2, 1, BU_OPT_VALIDATE_VALID);
	    CHECK_STANDARD(fast_bad, 2, 1, BU_OPT_VALIDATE_INVALID);
	    CHECK_STANDARD(char_good, 2, 1, BU_OPT_VALIDATE_VALID);
	    CHECK_STANDARD(string_good, 2, 1, BU_OPT_VALIDATE_VALID);
	    CHECK_STANDARD(vls_good, 2, 1, BU_OPT_VALIDATE_VALID);
	    CHECK_STANDARD(vector_good, 2, 1, BU_OPT_VALIDATE_VALID);
	    CHECK_STANDARD(vector_bad, 2, 1, BU_OPT_VALIDATE_INVALID);
	    CHECK_STANDARD(increment_good, 1, 0, BU_OPT_VALIDATE_VALID);
	    CHECK_STANDARD(language_good, 2, 1, BU_OPT_VALIDATE_VALID);
	    CHECK_STANDARD(language_bad, 2, 1, BU_OPT_VALIDATE_INVALID);
	    CHECK_STANDARD(man_good, 2, 1, BU_OPT_VALIDATE_VALID);
	    CHECK_STANDARD(man_bad, 2, 1, BU_OPT_VALIDATE_INVALID);
#undef CHECK_STANDARD
	    break;
	case 11:
#define CHECK_MISSING(_index) do { \
	    const char *missing[] = {descs[_index].longopt}; \
	    struct bu_vls spelling = BU_VLS_INIT_ZERO; \
	    bu_vls_sprintf(&spelling, "--%s", missing[0]); \
	    missing[0] = bu_vls_addr(&spelling); \
	    if (bu_opt_desc_validate(descs, NULL, 1, missing, 1, NULL, &result) || \
		result.state != BU_OPT_VALIDATE_INCOMPLETE || \
		!(result.expected & BU_OPT_EXPECT_OPTION_ARG)) \
		ret = 1; \
	    bu_opt_validate_result_clear(&result); \
	    bu_vls_free(&spelling); \
	} while (0)
	    ret = 0;
	    CHECK_MISSING(1);
	    CHECK_MISSING(2);
	    CHECK_MISSING(4);
	    CHECK_MISSING(5);
	    CHECK_MISSING(6);
	    CHECK_MISSING(7);
	    CHECK_MISSING(8);
	    CHECK_MISSING(9);
	    CHECK_MISSING(10);
	    CHECK_MISSING(11);
	    CHECK_MISSING(13);
	    CHECK_MISSING(14);
#undef CHECK_MISSING
	    break;
	case 12:
	    ret = bu_opt_desc_validate(descs, specs, 3, custom_done, 2,
		&opt_custom_calls, &result) || opt_custom_calls != 0 ||
		result.option_name != NULL;
	    break;
	case 13:
	    ret = bu_opt_desc_validate(descs, specs, 2, pair_partial, 2, NULL,
		&result) || result.state != BU_OPT_VALIDATE_INCOMPLETE ||
		!(result.expected & BU_OPT_EXPECT_OPTION_ARG);
	    bu_opt_validate_result_clear(&result);
	    ret |= bu_opt_desc_validate(descs, specs, 3, pair_good, 2, NULL,
		&result) || result.state != BU_OPT_VALIDATE_VALID ||
		result.value_type != BU_OPT_VALUE_STRING;
	    break;
	case 14:
	    ret = bu_opt_desc_validate(descs, NULL, 2, bool_seed, 1, NULL,
		&result) || !completion_has(&result, "true") ||
		completion_has(&result, "false");
	    bu_opt_validate_result_clear(&result);
	    ret |= bu_opt_desc_validate(descs, NULL, 2, man_seed, 1, NULL,
		&result) || !completion_has(&result, "1") ||
		!completion_has(&result, "n");
	    break;
	case 15:
	{
	    const struct bu_opt_value_spec invalid[] = {
		{"missing", NULL, BU_OPT_VALUE_STRING, 1, 1, NULL, NULL,
		    NULL, NULL, NULL},
		BU_OPT_VALUE_SPEC_NULL
	    };
	    ret = bu_opt_desc_validate(descs, invalid, 0, NULL, 0, NULL,
		&result) != -1;
	    break;
	}
	case 16:
	    opt_custom_calls = 0;
	    ret = bu_opt_desc_validate(descs, specs, 1, custom_option_seed, 0,
		&opt_custom_calls, &result) || opt_custom_calls != 0 ||
		!completion_has(&result, "--custom");
	    break;
	case 17:
	    opt_custom_calls = 0;
	    ret = bu_opt_desc_validate(descs, specs, 1, custom_missing, 0,
		&opt_custom_calls, &result) || opt_custom_calls != 0 ||
		result.state != BU_OPT_VALIDATE_INCOMPLETE ||
		!(result.expected & BU_OPT_EXPECT_OPTION_ARG);
	    break;
	case 18:
	    ret = bu_opt_parse_build_with_policy(NULL, 4, built_options_first,
		opt_validation_builder, &built_args,
		BU_OPT_PARSE_OPTIONS_FIRST) != 2 || built_args.number != 7 ||
		built_args.help != 0 ||
		!BU_STR_EQUAL(built_options_first[0], "operand") ||
		!BU_STR_EQUAL(built_options_first[1], "--help");
	    break;
	case 19:
	{
	    int custom_flag = 0;
	    struct bu_opt_desc flag_descs[] = {
		{"q", "custom-flag", "", opt_custom_flag, &custom_flag,
		    "Set a custom flag"},
		BU_OPT_DESC_NULL
	    };
	    const struct bu_opt_value_spec flag_specs[] = {
		{"custom-flag", NULL, BU_OPT_VALUE_FLAG, 0, 0, NULL,
		    "custom flag", NULL, NULL, NULL},
		BU_OPT_VALUE_SPEC_NULL
	    };
	    const char *flag_argv[] = {"--custom-flag"};
	    ret = bu_opt_desc_validate(flag_descs, flag_specs, 1, flag_argv, 0,
		NULL, &result) || result.state != BU_OPT_VALIDATE_VALID ||
		(result.expected & BU_OPT_EXPECT_OPTION_ARG);
	    bu_opt_validate_result_clear(&result);
	    ret |= bu_opt_parse(NULL, 1, flag_argv, flag_descs) != 0 ||
		custom_flag != 1;
	    break;
	}
	case 20:
	{
	    long increments = 0;
	    struct bu_opt_desc increment_descs[] = {
		{"v", NULL, "", bu_opt_incr_long, &increments, "Be verbose"},
		BU_OPT_DESC_NULL
	    };
	    const char *increment_argv[] = {"-vv"};
	    ret = bu_opt_parse(NULL, 1, increment_argv, increment_descs) != 0 ||
		increments != 2;
	    break;
	}
	case 21:
	    ret = bu_opt_parse_build_with_policy(NULL, 2, built_options_unknown,
		opt_validation_builder, &built_args,
		BU_OPT_PARSE_OPTIONS_FIRST) != -1;
	    break;
	case 22:
	    ret = bu_opt_parse_build_with_policy(NULL, 4, built_options_marker,
		opt_validation_builder, &built_args,
		BU_OPT_PARSE_OPTIONS_FIRST) != 1 || built_args.number != 7 ||
		built_args.help != 0 ||
		!BU_STR_EQUAL(built_options_marker[0], "--help");
	    break;
	case 23:
	{
	    size_t count = 1;
	    const char *no_args[] = {NULL};
	    struct bu_opt_desc *empty = bu_opt_desc_build(
		bu_opt_desc_empty_builder, NULL, &count);
	    ret = !empty || count != 0 || empty[0].shortopt || empty[0].longopt ||
		bu_opt_parse_build(NULL, 0, no_args, bu_opt_desc_empty_builder, NULL) != 0;
	    bu_free(empty, "empty bu_opt descriptor table");
	    break;
	}
	case 24:
	{
	    fastf_t plus = 0.0;
	    fastf_t star = 0.0;
	    int bang = 0;
	    struct bu_opt_desc punctuation_descs[] = {
		{"+", NULL, "number", bu_opt_fastf_t, &plus,
		    "Set a plus value"},
		{"*", NULL, "number", bu_opt_fastf_t, &star,
		    "Set a star value"},
		{"!", NULL, "", NULL, &bang, "Set a flag"},
		BU_OPT_DESC_NULL
	    };
	    const char *punctuation_argv[] = {"-+2.5", "-*", "3.5", "-!"};
	    ret = bu_opt_parse(NULL, 4, punctuation_argv,
		punctuation_descs) != 0 || !NEAR_EQUAL(plus, 2.5, SMALL_FASTF) ||
		!NEAR_EQUAL(star, 3.5, SMALL_FASTF) || bang != 1;
	    break;
	}
	case 25:
	{
	    int calls = 0;
	    struct bu_opt_desc wide_descs[] = {
		{"z", "six-values", "a b c d e f", opt_pair, NULL,
		    "Set six custom values"},
		BU_OPT_DESC_NULL
	    };
	    const struct bu_opt_value_spec wide_specs[] = {
		{"six-values", NULL, BU_OPT_VALUE_STRING, 6, 6, NULL,
		    "six values", NULL, opt_sidecar_validate, &calls},
		BU_OPT_VALUE_SPEC_NULL
	    };
	    const char *wide_argv[] = {
		"--six-values", "one", "two", "three", "four", "five", "six"
	    };
	    ret = bu_opt_desc_validate(wide_descs, wide_specs, 7, wide_argv, 6,
		&calls, &result) || calls != 1 ||
		result.state != BU_OPT_VALIDATE_VALID ||
		result.option != &wide_descs[0];
	    break;
	}
	case 26:
	{
	    ret = bu_opt_desc_build(opt_overflow_builder, NULL, NULL) != NULL;
	    break;
	}
	case 27:
	{
	    struct opt_callback_state attached = {0, NULL};
	    struct opt_callback_state clustered = {0, NULL};
	    int plain_flag = 0;
	    struct bu_opt_desc callback_descs[] = {
		{"x", "value", "text", opt_attached_once, &attached,
		    "Set an attached value"},
		{"f", "callback-flag", "", opt_flag_once, &clustered,
		    "Set a callback flag"},
		{"q", "plain-flag", "", NULL, &plain_flag, "Set a plain flag"},
		BU_OPT_DESC_NULL
	    };
	    const char *attached_argv[] = {"-xvalue"};
	    const char *cluster_argv[] = {"-fq"};

	    ret = bu_opt_parse(NULL, 1, attached_argv, callback_descs) != 0 ||
		attached.calls != 1 || !attached.value ||
		!BU_STR_EQUAL(attached.value, "value");
	    ret |= bu_opt_parse(NULL, 1, cluster_argv, callback_descs) != 0 ||
		clustered.calls != 1 || plain_flag != 1;
	    break;
	}
	case 28:
	{
	    int calls = 0;
	    const char *attached_argv[] = {"--custom=fast"};
	    struct bu_opt_value_spec attached_specs[] = {
		{"custom", NULL, BU_OPT_VALUE_STRING, 1, 1, NULL, "mode", NULL,
		    opt_attached_sidecar_validate, &calls},
		BU_OPT_VALUE_SPEC_NULL
	    };
	    ret = bu_opt_desc_validate(descs, attached_specs, 1, attached_argv, 0,
		&calls, &result) || calls != 1 ||
		result.state != BU_OPT_VALIDATE_VALID;
	    break;
	}
	case 29:
	{
	    const char *modes[] = {"fast", "safe", NULL};
	    const char *value = NULL;
	    struct bu_opt_desc alias_descs[] = {
		{"m", "mode", "mode", d1_verb, &value, "Select a mode"},
		{"M", NULL, "mode", d1_verb, &value, "Mode compatibility alias"},
		BU_OPT_DESC_NULL
	    };
	    const struct bu_opt_value_spec alias_specs[] = {
		BU_OPT_VALUE_CANDIDATES("mode", BU_OPT_VALUE_STRING, "mode", modes),
		BU_OPT_VALUE_ALIAS("M", "mode"),
		BU_OPT_VALUE_SPEC_NULL
	    };
	    const struct bu_opt_value_spec bad_target[] = {
		BU_OPT_VALUE_ALIAS("M", "missing"),
		BU_OPT_VALUE_SPEC_NULL
	    };
	    const struct bu_opt_value_spec alias_cycle[] = {
		BU_OPT_VALUE_ALIAS("mode", "M"),
		BU_OPT_VALUE_ALIAS("M", "mode"),
		BU_OPT_VALUE_SPEC_NULL
	    };
	    struct bu_opt_cmd cmd = BU_OPT_CMD_INIT_ZERO;
	    ret = bu_opt_cmd_create(&cmd, alias_descs, alias_specs) ||
		cmd.option_count != 2 ||
		cmd.options[1].value_type != cmd.options[0].value_type ||
		cmd.options[1].arg_requirement != cmd.options[0].arg_requirement ||
		cmd.options[1].value_keywords != cmd.options[0].value_keywords ||
		!BU_STR_EQUAL(bu_cmd_option_canonical(&cmd.options[1]), "mode");
	    if (!ret) {
		cmd.options[0].semantic_provider = "test.mode";
		cmd.options[0].argument = "canonical mode";
		ret = bu_opt_cmd_aliases(&cmd) ||
		    !BU_STR_EQUAL(cmd.options[1].semantic_provider, "test.mode") ||
		    !BU_STR_EQUAL(cmd.options[1].argument, "canonical mode");
	    }
	    bu_opt_cmd_clear(&cmd);
	    ret |= bu_opt_cmd_create(&cmd, alias_descs, bad_target) != -1;
	    bu_opt_cmd_clear(&cmd);
	    ret |= bu_opt_cmd_create(&cmd, alias_descs, alias_cycle) != -1;
	    bu_opt_cmd_clear(&cmd);
	    break;
	}
	case 30:
	{
	    const char *custom_value[] = {"--custom", "value"};
	    struct bu_opt_value_spec failing_specs[] = {
		{"custom", NULL, BU_OPT_VALUE_STRING, 1, 1, NULL, "mode", NULL,
		    opt_failing_sidecar_validate, NULL},
		BU_OPT_VALUE_SPEC_NULL
	    };

	    ret = bu_opt_desc_validate(descs, NULL, 0, NULL, 0, NULL, &result) ||
		!result.completion_count ||
		bu_opt_desc_validate(descs, failing_specs, 2, custom_value, 1,
		    NULL, &result) != -1 || result.state != BU_OPT_VALIDATE_UNKNOWN ||
		result.completion_candidates || result.completion_count ||
		result.hint || result.option || result.option_name;
	    if (!ret) {
		ret = bu_opt_desc_validate(descs, NULL, 0, NULL, 0, NULL,
		    &result) || !result.completion_count ||
		    bu_opt_desc_validate(NULL, NULL, 0, NULL, 0, NULL,
			&result) != -1 || result.completion_candidates ||
		    result.completion_count || result.hint || result.option;
	    }
	    if (!ret) {
		ret = bu_opt_desc_validate(descs, NULL, 0, NULL, 0, NULL,
		    &result) || !result.completion_count ||
		    bu_opt_validate_build(opt_overflow_builder, NULL, 0, NULL, 0,
			NULL, &result) != -1 || result.completion_candidates ||
		    result.completion_count || result.hint || result.option;
	    }
	    if (!ret)
		ret = bu_opt_validate_build(opt_validation_builder, NULL, 0,
		    NULL, 0, NULL, NULL) != -1;
	    break;
	}
	default:
	    break;
    }
    bu_opt_validate_result_clear(&result);
    bu_vls_free(&vls);
    return ret;
}


static int
desc_help(void)
{
    int help_flag = 0;
    int count = 0;
    const struct bu_opt_desc descs[] = {
	{"h", "help", "", NULL, &help_flag, "Print help and exit"},
	{"n", "count", "count", bu_opt_int, &count, "Set the item count"},
	BU_OPT_DESC_NULL
    };
    char *usage = bu_opt_usage(descs, "sample", "input [output]");
    char *help = bu_opt_help(descs, "sample", "input [output]",
	"Process a sample input");
    struct bu_opt_desc_opts filter = BU_OPT_DESC_OPTS_INIT_ZERO;
    filter.accept = "help";
    char *filtered = bu_opt_describe(descs, &filter);
    int ret = !usage || !help ||
	!BU_STR_EQUAL(usage, "Usage: sample [options] input [output]\n") ||
	!strstr(help, "Usage: sample [options] input [output]\n") ||
	!strstr(help, "\nProcess a sample input\n") ||
	!strstr(help, "\nOptions:\n") ||
	!strstr(help, "-h, --help") || !strstr(help, "-n count, --count count") ||
	!filtered || !strstr(filtered, "-h, --help") || strstr(filtered, "--count");

    if (usage)
	bu_free(usage, "bu_opt usage test");
    if (help)
	bu_free(help, "bu_opt help test");
    if (filtered)
	bu_free(filtered, "filtered bu_opt help test");
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
	    return desc_validation(test_num);
	    break;
	case 5:
	    return desc_help();
	    break;
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

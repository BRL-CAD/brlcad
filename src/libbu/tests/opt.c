/*                       O P T . C
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
#include <limits.h>
#include <ctype.h>
#include "bu.h"
#include "bn.h"
#include "string.h"


#define help_str "Print help string and exit."


int
d1_verb(struct bu_vls *msg, int argc, const char **argv, void *set_v)
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

void
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

#define EXPECT_SUCCESS_INT(_name, _var, _exp) { \
    if (ret || _var != _exp) { \
	bu_vls_printf(&parse_msgs, "\nError - expected value \"%d\" and got value %d\n", _exp, _var); \
	val_ok = 0; \
    } else { \
	bu_vls_printf(&parse_msgs, "  \nGot expected value: %s = %d\n", _name, _var); \
    } \
}

#define EXPECT_SUCCESS_INT_UNKNOWN(_name, _var, _exp) { \
    if (ret <= 0) { \
	bu_vls_printf(&parse_msgs, "\nError - extra args but none found.\n"); \
	val_ok = 0; \
    } else { \
    if ( _var != _exp) { \
	bu_vls_printf(&parse_msgs, "\nError - expected value \"%d\" and got value %d\n", _exp, _var); \
	val_ok = 0; \
    } else { \
	bu_vls_printf(&parse_msgs, "  \nGot expected value: %s = %d\n", _name, _var); \
    }} \
}

#define EXPECT_FAILURE_INT_UNKNOWN(_name, _var, _exp) { \
    if (ret <= 0 || _var == _exp) { \
	bu_vls_printf(&parse_msgs, "\nError - expected failure (%s) but no error returned\n", _name); \
	val_ok = 0; \
    } else { \
	bu_vls_printf(&parse_msgs, "  \nOK (expected failure) %s\n", _name); \
    } \
}

#define EXPECT_SUCCESS_FLOAT(_name, _var, _exp) { \
    if (ret || !NEAR_EQUAL(_var, _exp, SMALL_FASTF)) { \
	bu_vls_printf(&parse_msgs, "\nError - expected value \"%f\" and got value %f\n", _exp, _var); \
	val_ok = 0; \
    } else { \
	bu_vls_printf(&parse_msgs, "  \nGot expected value: %s = %f\n", _name, _var); \
    } \
}

#define EXPECT_SUCCESS_STRING(_name, _var, _exp) { \
    if (ret || !BU_STR_EQUAL(_var, _exp)) { \
	bu_vls_printf(&parse_msgs, "\nError - expected value \"%s\" and got value %s\n", _exp, _var); \
	val_ok = 0; \
    } else { \
	bu_vls_printf(&parse_msgs, "  \nGot expected value: %s = %s\n", _name, _var); \
    } \
}

#define EXPECT_SUCCESS_COLOR(_name, _color, _r, _g, _b) { \
    if (ret || (!NEAR_EQUAL(_color.buc_rgb[0], _r, SMALL_FASTF) || !NEAR_EQUAL(_color.buc_rgb[1], _g, SMALL_FASTF) || !NEAR_EQUAL(_color.buc_rgb[2], _b, SMALL_FASTF))) { \
	bu_vls_printf(&parse_msgs, "\nError - expected value \"%d/%d/%d\" and got value %.0f/%.0f/%.0f\n", _r, _g, _b, _color.buc_rgb[0], _color.buc_rgb[1], _color.buc_rgb[2]); \
	val_ok = 0; \
    } else { \
	bu_vls_printf(&parse_msgs, "  \nGot expected value: %s == %.0f/%.0f/%.0f)\n", _name,  _color.buc_rgb[0], _color.buc_rgb[1], _color.buc_rgb[2]); \
    } \
}

#define EXPECT_SUCCESS_COLOR_UNKNOWN(_name, _color, _r, _g, _b) { \
    if (ret <= 0) { \
	bu_vls_printf(&parse_msgs, "\nError - extra args expected but not found\n"); \
	val_ok = 0; \
    } else { \
    if ((!NEAR_EQUAL(_color.buc_rgb[0], _r, SMALL_FASTF) || !NEAR_EQUAL(_color.buc_rgb[1], _g, SMALL_FASTF) || !NEAR_EQUAL(_color.buc_rgb[2], _b, SMALL_FASTF))) { \
	bu_vls_printf(&parse_msgs, "\nError - expected value \"%d/%d/%d\" and got value %.0f/%.0f/%.0f\n", _r, _g, _b, _color.buc_rgb[0], _color.buc_rgb[1], _color.buc_rgb[2]); \
	val_ok = 0; \
    } else { \
	bu_vls_printf(&parse_msgs, "  \nGot expected value: %s == %.0f/%.0f/%.0f)\n", _name,  _color.buc_rgb[0], _color.buc_rgb[1], _color.buc_rgb[2]); \
    }} \
}


#define EXPECT_FAILURE(_name, _reason) { \
    if (!ret==-1) { \
	bu_vls_printf(&parse_msgs, "\nError - expected parser to fail with error and it didn't\n"); \
	val_ok = 0; \
    } else { \
	bu_vls_printf(&parse_msgs, "  \nOK (expected failure) - %s failed (%s)\n", _name, _reason); \
	ret = 0; \
    } \
}


int desc_1(const char *cgy, int test_num)
{
    static int print_help = 0;
    static int verbosity = 0;
    static int b = -1;
    static const char *str = NULL;
    static int i = 0;
    static long l = 0;
    static fastf_t f = 0;

    /* Option descriptions */
    struct bu_opt_desc d[9] = {
	{"h", "help",    0, 0, NULL,     (void *)&print_help, "",       help_str},
	{"?", "",        0, 0, NULL,     (void *)&print_help, "",       help_str},
	{"v", "verb",    0, 1, &d1_verb, (void *)&verbosity,  "#",      "Set verbosity (range is 0 to 3)"},
	{"b", "bool",    1, 1, &bu_opt_bool, (void *)&b,      "bool",   "Set boolean flag"},
	{"s", "str",     1, 1, &bu_opt_str,  (void *)&str,    "string", "Set string"},
	{"i", "int",     1, 1, &bu_opt_int,  (void *)&i,      "#",      "Set int"},
	{"l", "long",    1, 1, &bu_opt_long, (void *)&l,      "#",      "Set long"},
	{"f", "fastf_t", 1, 1, &bu_opt_fastf_t, (void *)&f,   "#",      "Read float"},
	BU_OPT_DESC_NULL
    };

    int ac = 0;
    int val_ok = 1;
    int ret = -1;
    int containers = 6;
    const char **av;
    const char **unknown;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;

    av = (const char **)bu_calloc(containers, sizeof(char *), "Input array");
    unknown = (const char **)bu_calloc(containers, sizeof(char *), "unknown results");

    if (cgy[0] == 'v') {
	switch (test_num) {
	    /* Verbosity option tests */
	    case 0:
		ret = bu_opt_parse(&unknown, 0, &parse_msgs, 0, NULL, d);
		ret = (ret == -1) ? 0 : -1;
		break;
	    case 1:
		ac = 1;
		av[0] = "-v";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
		break;
	    case 2:
		ac = 1;
		av[0] = "-v1";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
		break;
	    case 3:
		ac = 2;
		av[0] = "-v";
		av[1] = "1";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
		break;
	    case 4:
		ac = 1;
		av[0] = "-v=1";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
		break;
	    case 5:
		ac = 2;
		av[0] = "--v";
		av[1] = "1";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
		break;
	    case 6:
		ac = 1;
		av[0] = "--v=1";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
		break;
	    case 7:
		ac = 2;
		av[0] = "-v";
		av[1] = "2";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("verbosity", verbosity, 2);
		break;
	    case 8:
		ac = 2;
		av[0] = "-v";
		av[1] = "4";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_FAILURE("verbosity", "4 > 3");
		break;
	    case 9:
		ac = 2;
		av[0] = "--verb";
		av[1] = "2";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("verbosity", verbosity, 2);
		break;
	    case 10:
		ac = 2;
		av[0] = "--verb";
		av[1] = "4";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_FAILURE("verbosity", "4 > 3");
		break;
	    case 11:
		ac = 1;
		av[0] = "--verb=2";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("verbosity", verbosity, 2);
		break;
	    case 12:
		ac = 1;
		av[0] = "--verb=4";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_FAILURE("verbosity", "4 > 3");
		break;
	    case 13:
		ac = 1;
		av[0] = "--v";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
		break;
	    case 14:
		ac = 1;
		av[0] = "--verb";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
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
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("print_help", print_help, 1);
		break;
	    case 2:
		ac = 1;
		av[0] = "-?";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("print_help", print_help, 1);
		break;
	    case 3:
		ac = 1;
		av[0] = "--help";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("print_help", print_help, 1);
		break;
	    case 4:
		ac = 1;
		av[0] = "--help=4";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_FAILURE("print_help", "extra arg");
		break;
	    case 5:
		ac = 1;
		av[0] = "-?4";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_FAILURE("print_help", "extra arg");
		break;
	    case 6:
		ac = 2;
		av[0] = "-?";
		av[1] = "4";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT_UNKNOWN("print_help", print_help, 1);
		break;
	    case 7:
		ac = 1;
		av[0] = "-?=4";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_FAILURE("print_help", "extra arg");
		break;
	    case 8:
		ac = 1;
		av[0] = "--?4";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_FAILURE_INT_UNKNOWN("print_help", print_help, 1);
		break;
	    case 9:
		ac = 2;
		av[0] = "--?";
		av[1] = "4";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT_UNKNOWN("print_help", print_help, 1);
		break;
	    case 10:
		ac = 1;
		av[0] = "--?=4";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
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
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("bool", b, 1);
		break;
	    case 2:
		ac = 2;
		av[0] = "-b";
		av[1] = "false";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("bool", b, 0);
		break;
	    case 3:
		ac = 2;
		av[0] = "--bool";
		av[1] = "1";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("bool", b, 1);
		break;
	    case 4:
		ac = 2;
		av[0] = "--bool";
		av[1] = "0";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
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
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_FAILURE("string", "missing argument");
		break;
	    case 2:
		ac = 2;
		av[0] = "-s";
		av[1] = "test_str";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_STRING("string", str, "test_str");
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
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_FAILURE("int_num", "missing arg");
		break;
	    case 2:
		ac = 2;
		av[0] = "-i";
		av[1] = "-f";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_FAILURE("int_num", "invalid arg");
		break;
	    case 3:
		ac = 2;
		av[0] = "-i";
		av[1] = "1";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("int_num", i, 1);
		break;
	    case 4:
		ac = 2;
		av[0] = "-i";
		av[1] = "-1";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_INT("int_num", i, -1);
		break;
	    case 5:
		ac = 2;
		av[0] = "-i";
		av[1] = "214748364700";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
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
		    ac = 2;
		    av[0] = "-l";
		    av[1] = "214748364800";
		    set_msg_str(&parse_msgs, ac, av);
		    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		    EXPECT_SUCCESS_INT("long_num", l, 214748364800);
		    break;
		case 2:
		    ac = 2;
		    av[0] = "-l";
		    av[1] = "-214748364800";
		    set_msg_str(&parse_msgs, ac, av);
		    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		    EXPECT_SUCCESS_INT("long_num", l, -214748364800);
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
		    set_msg_str(&parse_msgs, ac, av);
		    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		    EXPECT_SUCCESS_INT("long_num", l, 21474836);
		    break;
		case 2:
		    ac = 2;
		    av[0] = "-l";
		    av[1] = "-21474836";
		    set_msg_str(&parse_msgs, ac, av);
		    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
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
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_FLOAT("float_num", f, 1.234);
		break;
	    case 2:
		ac = 2;
		av[0] = "-f";
		av[1] = "-1.234";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_FLOAT("float_num", f, -1.234);
		break;
	    case 3:
		ac = 2;
		av[0] = "-f";
		av[1] = "-3.0e-3";
		set_msg_str(&parse_msgs, ac, av);
		ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
		EXPECT_SUCCESS_FLOAT("float_num", f, -0.003);
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
	    bu_vls_printf(&parse_msgs, "%s, ", unknown[u]);
	}
	bu_vls_printf(&parse_msgs, "%s\n", unknown[ret - 1]);
    }

    ret = (!val_ok) ? -1 : 0;

    if (bu_vls_strlen(&parse_msgs) > 0) {
	bu_log("%s\n", bu_vls_addr(&parse_msgs));
    }
    bu_vls_free(&parse_msgs);
    bu_free(av, "free av");
    bu_free(unknown, "free av");
    return ret;
}



int
isnum(const char *str) {
    int i, sl;
    if (!str) return 0;
    sl = strlen(str);
    for (i = 0; i < sl; i++) if (!isdigit(str[i])) return 0;
    return 1;
}

int
dc_color(struct bu_vls *msg, int argc, const char **argv, void *set_c)
{
    struct bu_color *set_color = (struct bu_color *)set_c;
    unsigned int rgb[3];
    if (!argv || !argv[0] || strlen(argv[0]) == 0 || argc == 0) {
	return 0;
    }

    /* First, see if the first string converts to rgb */
    if (!bu_str_to_rgb((char *)argv[0], (unsigned char *)&rgb)) {
	/* nope - maybe we have 3 args? */
	if (argc == 3) {
	    struct bu_vls tmp_color = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&tmp_color, "%s/%s/%s", argv[0], argv[1], argv[2]);
	    if (!bu_str_to_rgb(bu_vls_addr(&tmp_color), (unsigned char *)&rgb)) {
		/* Not valid with 3 */
		bu_vls_free(&tmp_color);
		if (msg) bu_vls_sprintf(msg, "No valid color found.\n");
		return -1;
	    } else {
		/* 3 did the job */
		bu_vls_free(&tmp_color);
		if (set_color) (void)bu_color_from_rgb_chars(set_color, (unsigned char *)&rgb);
		return 3;
	    }
	} else {
	    /* Not valid with 1 and don't have 3 - we require at least one, so
	     * claim one argv as belonging to this option regardless. */
	    if (msg) bu_vls_sprintf(msg, "No valid color found: %s\n", argv[0]);
	    return -1;
	}
    } else {
	/* yep, 1 did the job */
	if (set_color) (void)bu_color_from_rgb_chars(set_color, (unsigned char *)&rgb);
	return 1;
    }

    return 0;
}

int desc_2(int test_num)
{
    int ret = 0;
    int val_ok = 1;
    int print_help = 0;
    struct bu_color color = BU_COLOR_INIT_ZERO;
    int containers = 7;
    int ac = 0;
    const char **av;
    const char **unknown;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",  0, 0, NULL,      (void *)&print_help, "",      help_str);
    BU_OPT(d[1], "C", "color", 1, 3, &dc_color, (void *)&color,      "r/g/b", "Set color");
    BU_OPT_NULL(d[2]);

    av = (const char **)bu_calloc(containers, sizeof(char *), "Input array");
    unknown = (const char **)bu_calloc(containers, sizeof(char *), "unknown results");

    switch (test_num) {
	case 0:
	    ret = bu_opt_parse(&unknown, 0, &parse_msgs, 0, NULL, d);
	    ret = (ret == -1) ? 0 : -1;
	    break;
	case 1:
	    ac = 2;
	    av[0] = "-C";
	    av[1] = "200/10/30";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 2:
	    ac = 4;
	    av[0] = "-C";
	    av[1] = "200";
	    av[2] = "10";
	    av[3] = "30";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 3:
	    ac = 4;
	    av[0] = "-C";
	    av[1] = "200/10/30";
	    av[2] = "50";
	    av[3] = "100";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
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
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_COLOR_UNKNOWN("color", color, 200, 10, 30);
	    break;
	case 5:
	    ac = 2;
	    av[0] = "--color";
	    av[1] = "200/10/30";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 6:
	    ac = 4;
	    av[0] = "--color";
	    av[1] = "200";
	    av[2] = "10";
	    av[3] = "30";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 7:
	    ac = 4;
	    av[0] = "--color";
	    av[1] = "200/10/30";
	    av[2] = "50";
	    av[3] = "100";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
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
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_COLOR_UNKNOWN("color", color, 200, 10, 30);
	    break;
	case 9:
	    ac = 1;
	    av[0] = "-C200/10/30";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 10:
	    ac = 1;
	    av[0] = "-C=200/10/30";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 11:
	    ac = 3;
	    av[0] = "-C200";
	    av[1] = "10";
	    av[2] = "30";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 12:
	    ac = 3;
	    av[0] = "-C=200";
	    av[1] = "10";
	    av[2] = "30";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 13:
	    ac = 1;
	    av[0] = "-C";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_FAILURE("color", "missing argument");
	    break;
	case 14:
	    ac = 1;
	    av[0] = "--color";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_FAILURE("color", "missing argument");
	    break;
	case 15:
	    ac = 5;
	    av[0] = "--color";
	    av[1] = "file";
	    av[2] = "10";
	    av[3] = "30";
	    av[4] = "50";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_FAILURE("color", "invalid argument");
	    break;

    }

    if (ret > 0) {
	int u = 0;
	bu_vls_printf(&parse_msgs, "\nUnknown args: ");
	for (u = 0; u < ret - 1; u++) {
	    bu_vls_printf(&parse_msgs, "%s, ", unknown[u]);
	}
	bu_vls_printf(&parse_msgs, "%s\n", unknown[ret - 1]);
    }

    ret = (!val_ok) ? -1 : 0;

    if (bu_vls_strlen(&parse_msgs) > 0) {
	bu_log("%s\n", bu_vls_addr(&parse_msgs));
    }
    bu_vls_free(&parse_msgs);
    bu_free(av, "free av");
    bu_free(unknown, "free av");
    return ret;
}


int
main(int argc, const char **argv)
{
    int ret = -1;
    long desc_num;
    long test_num;
    const char *cgy = NULL;
    char *endptr = NULL;

    /* Sanity check */
    if (argc < 4)
	bu_exit(1, "ERROR: wrong number of parameters - need option desc num, category and test num");

    /* Set the option description to used based on the input number */
    desc_num = strtol(argv[1], &endptr, 0);
    if (endptr && strlen(endptr) != 0) {
	bu_exit(1, "Invalid desc number: %s\n", argv[1]);
    }

    cgy = argv[2];

    test_num = strtol(argv[3], &endptr, 0);
    if (endptr && strlen(endptr) != 0) {
	bu_exit(1, "Invalid test number: %s\n", argv[2]);
    }

    switch (desc_num) {
	case 0:
	    ret = bu_opt_parse(NULL, 0, NULL, 0, NULL, NULL);
	case 1:
	    ret = desc_1(cgy, test_num);
	    break;
	case 2:
	    ret = desc_2(test_num);
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
 * ex: shiftwidth=4 tabstop=8
 */

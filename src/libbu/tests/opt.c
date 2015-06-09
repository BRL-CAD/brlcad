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

#define voff 0
#define hoff 15

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
    if (ret <= 0 || _var != _exp) { \
	bu_vls_printf(&parse_msgs, "\nError - expected value \"%d\" and got value %d\n", _exp, _var); \
	val_ok = 0; \
    } else { \
	bu_vls_printf(&parse_msgs, "  \nGot expected value: %s = %d\n", _name, _var); \
    } \
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

#define EXPECT_SUCCESS_COLOR(_name, _color, _r, _g, _b) { \
    if (ret || (!NEAR_EQUAL(_color.buc_rgb[0], _r, SMALL_FASTF) || !NEAR_EQUAL(_color.buc_rgb[1], _g, SMALL_FASTF) || !NEAR_EQUAL(_color.buc_rgb[2], _b, SMALL_FASTF))) { \
	bu_vls_printf(&parse_msgs, "\nError - expected value \"%d/%d/%d\" and got value %.0f/%.0f/%.0f\n", _r, _g, _b, _color.buc_rgb[0], _color.buc_rgb[1], _color.buc_rgb[2]); \
	val_ok = 0; \
    } else { \
	bu_vls_printf(&parse_msgs, "  \nGot expected value: %s == %.0f/%.0f/%.0f)\n", _name,  _color.buc_rgb[0], _color.buc_rgb[1], _color.buc_rgb[2]); \
    } \
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


int desc_1(int test_num)
{
    static int print_help = 0;
    static int verbosity = 0;

    /* Option descriptions */
    struct bu_opt_desc d[4] = {
	{"h", "help",      0, 0, NULL,     (void *)&print_help, "",  help_str},
	{"?", "",          0, 0, NULL,     (void *)&print_help, "",  help_str},
	{"v", "verbosity", 0, 1, &d1_verb, (void *)&verbosity,  "#", "Set verbosity (range is 0 to 3)"},
	BU_OPT_DESC_NULL
    };

    int ac = 0;
    int val_ok = 1;
    int ret = 0;
    int containers = 6;
    const char **av;
    const char **unknown;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;

    av = (const char **)bu_calloc(containers, sizeof(char *), "Input array");
    unknown = (const char **)bu_calloc(containers, sizeof(char *), "unknown results");

    switch (test_num) {
	/* Verbosity option tests */
	case voff:
	    ret = bu_opt_parse(&unknown, 0, &parse_msgs, 0, NULL, d);
	    ret = (ret == -1) ? 0 : -1;
	    break;
	case voff+1:
	    ac = 1;
	    av[0] = "-v";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
	    break;
	case voff+2:
	    ac = 1;
	    av[0] = "-v1";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
	    break;
	case voff+3:
	    ac = 2;
	    av[0] = "-v";
	    av[1] = "1";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
	    break;
	case voff+4:
	    ac = 1;
	    av[0] = "-v=1";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
	    break;
	case voff+5:
	    ac = 2;
	    av[0] = "--v";
	    av[1] = "1";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
	    break;
	case voff+6:
	    ac = 1;
	    av[0] = "--v=1";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
	    break;
	case voff+7:
	    ac = 2;
	    av[0] = "-v";
	    av[1] = "2";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT("verbosity", verbosity, 2);
	    break;
	case voff+8:
	    ac = 2;
	    av[0] = "-v";
	    av[1] = "4";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_FAILURE("verbosity", "4 > 3");
	    break;
	case voff+9:
	    ac = 2;
	    av[0] = "--verbosity";
	    av[1] = "2";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT("verbosity", verbosity, 2);
	    break;
	case voff+10:
	    ac = 2;
	    av[0] = "--verbosity";
	    av[1] = "4";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_FAILURE("verbosity", "4 > 3");
	    break;
	case voff+11:
	    ac = 1;
	    av[0] = "--verbosity=2";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT("verbosity", verbosity, 2);
	    break;
	case voff+12:
	    ac = 1;
	    av[0] = "--verbosity=4";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_FAILURE("verbosity", "4 > 3");
	    break;
	case voff+13:
	    ac = 1;
	    av[0] = "--v";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
	    if (ret || verbosity != 1) {
		bu_vls_printf(&parse_msgs, "\nError - expected value \"1\" and got value %d\n", verbosity);
		val_ok = 0;
	    } else {
		bu_vls_printf(&parse_msgs, "  OK\nverbosity = %d\n", verbosity);
	    }
	    break;
	case voff+14:
	    ac = 1;
	    av[0] = "--verbosity";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT("verbosity", verbosity, 1);
	    break;
	/* Help option tests */
	case hoff:
	    ac = 1;
	    av[0] = "-h";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT("print_help", print_help, 1);
	    break;
	case hoff + 1:
	    ac = 1;
	    av[0] = "-?";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT("print_help", print_help, 1);
	    break;
	case hoff + 2:
	    ac = 1;
	    av[0] = "--help";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT("print_help", print_help, 1);
	    break;
	case hoff + 3:
	    ac = 1;
	    av[0] = "--help=4";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_FAILURE("print_help", "extra arg");
	    break;
	case hoff + 4:
	    ac = 1;
	    av[0] = "-?4";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_FAILURE("print_help", "extra arg");
	    break;
	case hoff + 5:
	    ac = 2;
	    av[0] = "-?";
	    av[1] = "4";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT_UNKNOWN("print_help", print_help, 1);
	    break;
	case hoff + 6:
	    ac = 1;
	    av[0] = "-?=4";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_FAILURE("print_help", "extra arg");
	    break;
	case hoff + 7:
	    ac = 1;
	    av[0] = "--?4";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_FAILURE_INT_UNKNOWN("print_help", print_help, 1);
	    break;
	case hoff + 8:
	    ac = 2;
	    av[0] = "--?";
	    av[1] = "4";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT_UNKNOWN("print_help", print_help, 1);
	    break;
	case hoff + 9:
	    ac = 1;
	    av[0] = "--?=4";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_FAILURE("print_help", "extra arg");
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
    int containers = 6;
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
	    av[0] = "--color";
	    av[1] = "200/10/30";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
	    break;
	case 2:
	    ac = 4;
	    av[0] = "--color";
	    av[1] = "200";
	    av[2] = "10";
	    av[3] = "30";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_COLOR("color", color, 200, 10, 30);
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

    if (!val_ok) ret = -1;

    if (bu_vls_strlen(&parse_msgs) > 0) {
	bu_log("%s\n", bu_vls_addr(&parse_msgs));
    }
    bu_vls_free(&parse_msgs);
    bu_free(av, "free av");
    bu_free(unknown, "free av");
    return ret;
}


int desc_3(int test_num)
{
    static int print_help = 0;
    static int int_num = 0;
    static fastf_t float_num = 0;

    struct bu_opt_desc d[4] = {
	{"h", "help",    0, 0, NULL,            (void *)&print_help, "", help_str},
	{"n", "num",     1, 1, &bu_opt_int,     (void *)&int_num, "#", "Read int"},
	{"f", "fastf_t", 1, 1, &bu_opt_fastf_t, (void *)&float_num, "#", "Read float"},
	BU_OPT_DESC_NULL
    };

    int containers = 6;
    int val_ok = 1;
    int ac = 0;
    const char **av;
    const char **unknown;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    int ret = 0;

    av = (const char **)bu_calloc(containers, sizeof(char *), "Input array");
    unknown = (const char **)bu_calloc(containers, sizeof(char *), "unknown results");

    switch (test_num) {
	case 0:
	    ret = bu_opt_parse(&unknown, 0, &parse_msgs, 0, NULL, d);
	    ret = (ret == -1) ? 0 : -1;
	    break;
	case 1:
	    ac = 1;
	    av[0] = "-n";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_FAILURE("int_num", "missing arg");
	    break;
	case 2:
	    ac = 2;
	    av[0] = "-n";
	    av[1] = "-f";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_FAILURE("int_num", "invalid arg");
	    break;
	case 3:
	    ac = 2;
	    av[0] = "-n";
	    av[1] = "1";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT("int_num", int_num, 1);
	    break;
	case 4:
	    ac = 2;
	    av[0] = "-n";
	    av[1] = "-1";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_INT("int_num", int_num, -1);
	    break;
	case 5:
	    ac = 2;
	    av[0] = "-f";
	    av[1] = "-3.0e-3";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_FLOAT("float_num", float_num, -0.003);
	    break;
	case 6:
	    ac = 4;
	    av[0] = "-n";
	    av[1] = "2";
	    av[2] = "-f";
	    av[3] = "0.01";
	    set_msg_str(&parse_msgs, ac, av);
	    ret = bu_opt_parse(&unknown, containers, &parse_msgs, ac, av, d);
	    EXPECT_SUCCESS_FLOAT("int_num", int_num, 2);
	    EXPECT_SUCCESS_FLOAT("float_num", float_num, 0.01);
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

    if (!val_ok) ret = -1;

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
    int ret = 0;
    long desc_num;
    long test_num;
    char *endptr = NULL;

    /* Sanity check */
    if (argc < 3)
	bu_exit(1, "ERROR: wrong number of parameters - need option num and test num");

    /* Set the option description to used based on the input number */
    desc_num = strtol(argv[1], &endptr, 0);
    if (endptr && strlen(endptr) != 0) {
	bu_exit(1, "Invalid desc number: %s\n", argv[1]);
    }

    test_num = strtol(argv[2], &endptr, 0);
    if (endptr && strlen(endptr) != 0) {
	bu_exit(1, "Invalid test number: %s\n", argv[2]);
    }

    switch (desc_num) {
	case 0:
	    ret = bu_opt_parse(NULL, 0, NULL, 0, NULL, NULL);
	case 1:
	    ret = desc_1(test_num);
	    break;
	case 2:
	    ret = desc_2(test_num);
	    break;
	case 3:
	    ret = desc_3(test_num);
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

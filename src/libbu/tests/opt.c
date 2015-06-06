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

int
d1_verb(struct bu_vls *msg, int argc, const char **argv, void *set_v)
{
    int val = INT_MAX;
    int *int_set = (int *)set_v;
    int int_ret;
    if (!argv || !argv[0] || strlen(argv[0]) == 0 || argc == 1) {
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

int
isnum(const char *str) {
    int i, sl;
    if (!str) return 0;
    sl = strlen(str);
    for (i = 0; i < sl; i++) if (!isdigit(str[i])) return 0;
    return 1;
}

int
d2_color(struct bu_vls *msg, int argc, const char **argv, void *set_c)
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

#define help_str "Print help string and exit."

int
main(int argc, const char **argv)
{
    int u = 0;
    int ret = 0;
    char *endptr = NULL;
    long test_num;
    struct bu_ptbl *results = NULL;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    static int i = 0;
    static int print_help = 0;
    static fastf_t f = 0;
    static struct bu_color color = BU_COLOR_INIT_ZERO;
    const char **unknown = (const char **)bu_calloc(argc, sizeof(char *), "unknown results");

    struct bu_opt_desc d1[4] = {
	{"h", "help",      0, 0, NULL,       (void *)&print_help, "", help_str},
	{"?", "",          0, 0, NULL,       (void *)&print_help, "", help_str},
	{"v", "verbosity", 0, 1, &d1_verb, (void *)&i, "#", "Set verbosity (range is 0 to 3)"},
	BU_OPT_DESC_NULL
    };

    struct bu_opt_desc d2[4] = {
	{"h", "help",  0, 0, NULL,        (void *)&print_help, "", help_str},
	{"C", "color", 1, 3, &d2_color, (void *)&color, "r/g/b", "Set color"},
	BU_OPT_DESC_NULL
    };

    struct bu_opt_desc d3[4] = {
	{"h", "help",    0, 0, NULL,            (void *)&print_help, "", help_str},
	{"n", "num",     1, 1, &bu_opt_int,     (void *)&i, "#", "Read int"},
	{"f", "fastf_t", 1, 1, &bu_opt_fastf_t, (void *)&f, "#", "Read float"},
	BU_OPT_DESC_NULL
    };

    if (argc < 2)
	bu_exit(1, "ERROR: wrong number of parameters");

    test_num = strtol(argv[1], &endptr, 0);
    if (endptr && strlen(endptr) != 0) {
	bu_exit(1, "Invalid test number: %s\n", argv[1]);
    }

    switch (test_num) {
	case 0:
	    ret = bu_opt_parse(&unknown, argc, &parse_msgs, 0, NULL, NULL);
	    return (results == NULL) ? 0 : 1;
	    break;
	case 1:
	    ret = bu_opt_parse(&unknown, argc, &parse_msgs, argc-2, argv+2, d1);
	    break;
	case 2:
	    ret = bu_opt_parse(&unknown, argc, &parse_msgs, argc-2, argv+2, d2);
	    break;
	case 3:
	    ret = bu_opt_parse(&unknown, argc, &parse_msgs, argc-2, argv+2, d3);
	    break;
    }

    if (ret == -1) {
	bu_log("%s", bu_vls_addr(&parse_msgs));
	return 0;
    }

    if (test_num == 1 || test_num == 3) {
	bu_log("Int var: %d\n", i);
	bu_log("Fastf_t var: %f\n", f);
    }
    if (test_num == 2)
	bu_log("Color var: %0.2f, %0.2f, %0.2f\n", color.buc_rgb[0], color.buc_rgb[1], color.buc_rgb[2]);

    if (ret > 0) {
	bu_log("Unknown args: ");
	for (u = 0; u < ret - 1; u++) {
	    bu_log("%s, ", unknown[u]);
	}
	bu_log("%s\n", unknown[ret - 1]);
    }
    bu_vls_free(&parse_msgs);
    bu_free(unknown, "unknown argv");

    return 0;
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

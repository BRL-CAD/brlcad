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
d1_verbosity(struct bu_vls *msg, struct bu_opt_data *data, void *set_v)
{
    int val = INT_MAX;
    int *int_set = (int *)set_v;
    int int_ret = bu_opt_arg_int(msg, data, (void *)&val);
    if (int_ret == -1 || int_ret == 0) return int_ret;

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
d2_color(struct bu_vls *msg, struct bu_opt_data *data, void *set_c)
{
    struct bu_color *set_color = (struct bu_color *)set_c;
    unsigned int rgb[3];
    if (!data || !data->argv || !data->argv[0] || strlen(data->argv[0]) == 0 || data->argc == 0) {
	return 0;
    }

    /* First, see if the first string converts to rgb */
    if (!bu_str_to_rgb((char *)data->argv[0], (unsigned char *)&rgb)) {
	/* nope - maybe we have 3 argv? */
	if (data->argc == 3) {
	    struct bu_vls tmp_color = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&tmp_color, "%s/%s/%s", data->argv[0], data->argv[1], data->argv[2]);
	    if (!bu_str_to_rgb(bu_vls_addr(&tmp_color), (unsigned char *)&rgb)) {
		/* Not valid with 3 */
		bu_vls_free(&tmp_color);
		if (msg) bu_vls_sprintf(msg, "No valid color found.");
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
	    if (msg) bu_vls_sprintf(msg, "No valid color found: %s", data->argv[0]);
	    return -1;
	}
    } else {
	/* yep, 1 did the job */
	if (set_color) (void)bu_color_from_rgb_chars(set_color, (unsigned char *)&rgb);
	return 1;
    }

    return 0;
}

void
print_results(struct bu_ptbl *results)
{
    size_t i = 0;
    struct bu_opt_data *data;
    for (i = 0; i < BU_PTBL_LEN(results); i++) {
	data = (struct bu_opt_data *)BU_PTBL_GET(results, i);
	bu_log("option name: %s\n", data->name);
	if (data->argv && data->argc != 0) {
	    int j = 0;
	    bu_log("option args: ");
	    for (j = 0; j < data->argc; j++) {
		bu_log("%s ", data->argv[j]);
	    }
	    bu_log("\n");
	}
	bu_log("option is valid: %d\n\n", data->valid);
    }
}

#define help_str "Print help string and exit."

int
main(int argc, const char **argv)
{
    int ret = 0;
    int function_num;
    struct bu_ptbl *results = NULL;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    static int i = 0;
    static struct bu_color color = BU_COLOR_INIT_ZERO;

    enum d1_opt_ind {D1_HELP, D1_VERBOSITY};
    struct bu_opt_desc d1[4] = {
	{D1_HELP, 0, 0, "h", "help", NULL, "", help_str, NULL},
	{D1_HELP, 0, 0, "?", "",     NULL, "", help_str, NULL},
	{D1_VERBOSITY, 0, 1, "v", "verbosity", &(d1_verbosity), "#", "Set verbosity (range is 0 to 3)", (void *)&i},
	BU_OPT_DESC_NULL
    };

    enum d2_opt_ind {D2_HELP, D2_COLOR};
    struct bu_opt_desc d2[4] = {
	{D2_HELP, 0, 0, "h", "help", NULL, "", help_str, NULL},
	{D2_COLOR, 1, 3, "C", "color", &(d2_color), "r/g/b", "Set color", (void *)&color},
	BU_OPT_DESC_NULL
    };

    enum d3_opt_ind {D3_HELP, D3_NUM};
    struct bu_opt_desc d3[4] = {
	{D3_HELP, 0, 0, "h", "help", NULL, "", help_str, NULL},
	{D3_NUM, 1, 1, "n", "num", &bu_opt_arg_int, "#", "Read number", (void *)&i},
	BU_OPT_DESC_NULL
    };



    if (argc < 2)
	bu_exit(1, "ERROR: wrong number of parameters");

    sscanf(argv[1], "%d", &function_num);

    switch (function_num) {
	case 0:
	    ret = bu_opt_parse(&results, &parse_msgs, 0, NULL, NULL);
	    return (results == NULL) ? 0 : 1;
	    break;
	case 1:
	    ret = bu_opt_parse(&results, &parse_msgs, argc-2, argv+2, d1);
	    break;
	case 2:
	    ret = bu_opt_parse(&results, &parse_msgs, argc-2, argv+2, d2);
	    break;
	case 3:
	    ret = bu_opt_parse(&results, &parse_msgs, argc-2, argv+2, d3);
	    break;
    }

    if (ret == -1) {
	bu_log("%s", bu_vls_addr(&parse_msgs));
    }

    if (results) {
	bu_opt_compact(results);
	print_results(results);
	if (function_num == 1 || function_num == 3)
	    bu_log("Int var: %d\n", i);
	if (function_num == 2)
	    bu_log("Color var: %0.2f, %0.2f, %0.2f\n", color.buc_rgb[0], color.buc_rgb[1], color.buc_rgb[2]);
    }

    bu_vls_free(&parse_msgs);

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

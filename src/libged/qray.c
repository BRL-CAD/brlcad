/*                          Q R A Y . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/qray.c
 *
 * Routines to set and get "Query Ray" variables.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "dg.h"
#include "ged.h"

#include "./qray.h"


static void qray_print_fmts(struct ged *gedp);
static void qray_print_vars(struct ged *gedp);
static int qray_get_fmt_index(struct ged *gedp, char c);

static struct ged_qray_color def_qray_odd_color = { 0, 255, 255 };
static struct ged_qray_color def_qray_even_color = { 255, 255, 0 };
static struct ged_qray_color def_qray_void_color = { 255, 0, 255 };
static struct ged_qray_color def_qray_overlap_color = { 255, 255, 255 };

static struct qray_fmt_data def_qray_fmt_data[] = {
    {'r', "\"Origin (x y z) = (%.2f %.2f %.2f)  (h v d) = (%.2f %.2f %.2f)\\nDirection (x y z) = (%.4f %.4f %.4f)  (az el) = (%.2f %.2f)\\n\" x_orig y_orig z_orig h v d_orig x_dir y_dir z_dir a e"},
    {'h', "\"    Region Name               Entry (x y z) LOS  Obliq_in\\n\""},
    {'p', "\"%-20s (%9.3f %9.3f %9.3f) %8.2f %8.3f\\n\" reg_name x_in y_in z_in los obliq_in"},
    {'f', "\"\""},
    {'m', "\"You missed the target\\n\""},
    {'o', "\"OVERLAP: '%s' and '%s' xyz_in=(%g %g %g) los=%g\\n\" ov_reg1_name ov_reg2_name ov_x_in ov_y_in ov_z_in ov_los"},
    {'g', "\"\""},
    {(char)0, (char *)NULL}
};


static void
usage(struct ged *gedp, const char *argv0)
{
    bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", argv0);
    bu_vls_printf(gedp->ged_result_str, " qray vars			print a list of all variables (i.e. var = val)\n");
    bu_vls_printf(gedp->ged_result_str, " qray basename [str]		set or get basename for query ray primitives\n");
    bu_vls_printf(gedp->ged_result_str, " qray effects [t|g|b]		set or get effects (i.e. text, graphical or both)\n");
    bu_vls_printf(gedp->ged_result_str, " qray echo [0|1]		set or get command echo\n");
    bu_vls_printf(gedp->ged_result_str, " qray oddcolor [r g b]		set or get color of odd partitions\n");
    bu_vls_printf(gedp->ged_result_str, " qray evencolor [r g b]		set or get color of even partitions\n");
    bu_vls_printf(gedp->ged_result_str, " qray voidcolor [r g b]		set or get color of void areas\n");
    bu_vls_printf(gedp->ged_result_str, " qray overlapcolor [r g b]	set or get color of overlap areas\n");
    bu_vls_printf(gedp->ged_result_str, " qray fmt [r|h|p|f|m|o|g [str]]	set or get format string(s)\n");
    bu_vls_printf(gedp->ged_result_str, " qray script [str]		set or get the nirt script string\n");
    bu_vls_printf(gedp->ged_result_str, " qray [help]			print this help message\n");
}


static void
qray_print_fmts(struct ged *gedp)
{
    int i;

    for (i = 0; gedp->ged_gdp->gd_qray_fmts[i].type != (char)0; ++i)
	bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_addr(&gedp->ged_gdp->gd_qray_fmts[i].fmt));
}


static void
qray_print_vars(struct ged *gedp)
{
    bu_vls_printf(gedp->ged_result_str, "basename = %s\n", bu_vls_addr(&gedp->ged_gdp->gd_qray_basename));
    bu_vls_printf(gedp->ged_result_str, "script = %s\n", bu_vls_addr(&gedp->ged_gdp->gd_qray_script));
    bu_vls_printf(gedp->ged_result_str, "effects = %c\n", gedp->ged_gdp->gd_qray_effects);
    bu_vls_printf(gedp->ged_result_str, "echo = %d\n", gedp->ged_gdp->gd_qray_cmd_echo);
    bu_vls_printf(gedp->ged_result_str, "oddcolor = %d %d %d\n",
		  gedp->ged_gdp->gd_qray_odd_color.r, gedp->ged_gdp->gd_qray_odd_color.g, gedp->ged_gdp->gd_qray_odd_color.b);
    bu_vls_printf(gedp->ged_result_str, "evencolor = %d %d %d\n",
		  gedp->ged_gdp->gd_qray_even_color.r, gedp->ged_gdp->gd_qray_even_color.g, gedp->ged_gdp->gd_qray_even_color.b);
    bu_vls_printf(gedp->ged_result_str, "voidcolor = %d %d %d\n",
		  gedp->ged_gdp->gd_qray_void_color.r, gedp->ged_gdp->gd_qray_void_color.g, gedp->ged_gdp->gd_qray_void_color.b);
    bu_vls_printf(gedp->ged_result_str, "overlapcolor = %d %d %d\n",
		  gedp->ged_gdp->gd_qray_overlap_color.r, gedp->ged_gdp->gd_qray_overlap_color.g, gedp->ged_gdp->gd_qray_overlap_color.b);

    qray_print_fmts(gedp);
}


static int
qray_get_fmt_index(struct ged *gedp,
		   char c)
{
    int i;

    for (i = 0; gedp->ged_gdp->gd_qray_fmts[i].type != (char)0; ++i)
	if (c == gedp->ged_gdp->gd_qray_fmts[i].type)
	    return i;

    return -1;
}


int
ged_qray(struct ged *gedp,
	 int argc,
	 const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_INITIALIZED(gedp) ? GED_ERROR : GED_OK);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	usage(gedp, argv[0]);
	return GED_HELP;
    }

    if (argc > 6) {
	usage(gedp, argv[0]);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "fmt")) {
	int i;

	if (argc == 2) {
	    /* get all format strings */
	    qray_print_fmts(gedp);
	    return GED_OK;
	} else if (argc == 3) {
	    /* get particular format string */
	    if ((i = qray_get_fmt_index(gedp, *argv[2])) < 0) {
		bu_vls_printf(gedp->ged_result_str, "qray: unrecognized format type: '%s'\n", argv[2]);
		usage(gedp, argv[0]);

		return GED_ERROR;
	    }

	    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&gedp->ged_gdp->gd_qray_fmts[i].fmt));
	    return GED_OK;
	} else if (argc == 4) {
	    /* set value */
	    if ((i = qray_get_fmt_index(gedp, *argv[2])) < 0) {
		bu_vls_printf(gedp->ged_result_str, "qray: unrecognized format type: '%s'\n", argv[2]);
		usage(gedp, argv[0]);

		return GED_ERROR;
	    }

	    bu_vls_trunc(&gedp->ged_gdp->gd_qray_fmts[i].fmt, 0);
	    bu_vls_printf(&gedp->ged_gdp->gd_qray_fmts[i].fmt, "%s", argv[3]);
	    return GED_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'qray fmt' command accepts 0, 1 or 2 argumentS\n");
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "basename")) {
	if (argc == 2) {
	    /* get value */
	    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&gedp->ged_gdp->gd_qray_basename));

	    return GED_OK;
	} else if (argc == 3) {
	    /* set value */
	    bu_vls_strcpy(&gedp->ged_gdp->gd_qray_basename, argv[2]);
	    return GED_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'qray basename' command accepts 0 or 1 argument\n");
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "script")) {
	if (argc == 2) {
	    /* get value */
	    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&gedp->ged_gdp->gd_qray_script));

	    return GED_OK;
	} else if (argc == 3) {
	    /* set value */
	    bu_vls_strcpy(&gedp->ged_gdp->gd_qray_script, argv[2]);
	    return GED_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'qray scripts' command accepts 0 or 1 argument\n");
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "effects")) {
	if (argc == 2) {
	    /* get value */
	    bu_vls_printf(gedp->ged_result_str, "%c", gedp->ged_gdp->gd_qray_effects);

	    return TCL_OK;
	} else if (argc == 3) {
	    /* set value */
	    if (*argv[2] != 't' && *argv[2] != 'g' && *argv[2] != 'b') {
		bu_vls_printf(gedp->ged_result_str, "qray effects: bad value - %s", argv[2]);

		return GED_ERROR;
	    }

	    gedp->ged_gdp->gd_qray_effects = *argv[2];

	    return GED_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'qray effects' command accepts 0 or 1 argument\n");
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "echo")) {
	if (argc == 2) {
	    /* get value */
	    if (gedp->ged_gdp->gd_qray_cmd_echo)
		bu_vls_printf(gedp->ged_result_str, "1");
	    else
		bu_vls_printf(gedp->ged_result_str, "0");

	    return GED_OK;
	} else if (argc == 3) {
	    /* set value */
	    int ival;

	    if (sscanf(argv[2], "%d", &ival) < 1) {
		bu_vls_printf(gedp->ged_result_str, "qray echo: bad value - %s", argv[2]);

		return GED_ERROR;
	    }

	    if (ival)
		gedp->ged_gdp->gd_qray_cmd_echo = 1;
	    else
		gedp->ged_gdp->gd_qray_cmd_echo = 0;

	    return GED_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'qray echo' command accepts 0 or 1 argument\n");
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "oddcolor")) {
	if (argc == 2) {
	    /* get value */
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  gedp->ged_gdp->gd_qray_odd_color.r,
			  gedp->ged_gdp->gd_qray_odd_color.g,
			  gedp->ged_gdp->gd_qray_odd_color.b);

	    return GED_OK;
	} else if (argc == 5) {
	    /* set value */
	    int r, g, b;

	    if (sscanf(argv[2], "%d", &r) != 1 ||
		sscanf(argv[3], "%d", &g) != 1 ||
		sscanf(argv[4], "%d", &b) != 1 ||
		r < 0 || g < 0 || b < 0 ||
		255 < r || 255 < g || 255 < b) {
		bu_vls_printf(gedp->ged_result_str, "qray oddcolor %s %s %s - bad value",
			      argv[2], argv[3], argv[4]);

		return GED_ERROR;
	    }

	    gedp->ged_gdp->gd_qray_odd_color.r = r;
	    gedp->ged_gdp->gd_qray_odd_color.g = g;
	    gedp->ged_gdp->gd_qray_odd_color.b = b;

	    return GED_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'qray oddcolor' command accepts 0 or 3 arguments\n");
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "evencolor")) {
	if (argc == 2) {
	    /* get value */
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  gedp->ged_gdp->gd_qray_even_color.r,
			  gedp->ged_gdp->gd_qray_even_color.g,
			  gedp->ged_gdp->gd_qray_even_color.b);

	    return GED_OK;
	} else if (argc == 5) {
	    /* set value */
	    int r, g, b;

	    if (sscanf(argv[2], "%d", &r) != 1 ||
		sscanf(argv[3], "%d", &g) != 1 ||
		sscanf(argv[4], "%d", &b) != 1 ||
		r < 0 || g < 0 || b < 0 ||
		255 < r || 255 < g || 255 < b) {
		bu_vls_printf(gedp->ged_result_str, "qray evencolor %s %s %s - bad value",
			      argv[2], argv[3], argv[4]);

		return GED_ERROR;
	    }

	    gedp->ged_gdp->gd_qray_even_color.r = r;
	    gedp->ged_gdp->gd_qray_even_color.g = g;
	    gedp->ged_gdp->gd_qray_even_color.b = b;

	    return GED_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'qray evencolor' command accepts 0 or 3 arguments\n");
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "voidcolor")) {
	if (argc == 2) {
	    /* get value */
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  gedp->ged_gdp->gd_qray_void_color.r,
			  gedp->ged_gdp->gd_qray_void_color.g,
			  gedp->ged_gdp->gd_qray_void_color.b);

	    return GED_OK;
	} else if (argc == 5) {
	    /* set value */
	    int r, g, b;

	    if (sscanf(argv[2], "%d", &r) != 1 ||
		sscanf(argv[3], "%d", &g) != 1 ||
		sscanf(argv[4], "%d", &b) != 1 ||
		r < 0 || g < 0 || b < 0 ||
		255 < r || 255 < g || 255 < b) {
		bu_vls_printf(gedp->ged_result_str, "qray voidcolor %s %s %s - bad value",
			      argv[2], argv[3], argv[4]);

		return GED_ERROR;
	    }

	    gedp->ged_gdp->gd_qray_void_color.r = r;
	    gedp->ged_gdp->gd_qray_void_color.g = g;
	    gedp->ged_gdp->gd_qray_void_color.b = b;

	    return GED_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'qray voidpcolor' command accepts 0 or 3 arguments\n");
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "overlapcolor")) {
	if (argc == 2) {
	    /* get value */
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  gedp->ged_gdp->gd_qray_overlap_color.r,
			  gedp->ged_gdp->gd_qray_overlap_color.g,
			  gedp->ged_gdp->gd_qray_overlap_color.b);

	    return GED_OK;
	} else if (argc == 5) {
	    /* set value */
	    int r, g, b;

	    if (sscanf(argv[2], "%d", &r) != 1 ||
		sscanf(argv[3], "%d", &g) != 1 ||
		sscanf(argv[4], "%d", &b) != 1 ||
		r < 0 || g < 0 || b < 0 ||
		255 < r || 255 < g || 255 < b) {
		bu_vls_printf(gedp->ged_result_str,
			      "qray overlapcolor %s %s %s - bad value",
			      argv[2], argv[3], argv[4]);
		return GED_ERROR;
	    }

	    gedp->ged_gdp->gd_qray_overlap_color.r = r;
	    gedp->ged_gdp->gd_qray_overlap_color.g = g;
	    gedp->ged_gdp->gd_qray_overlap_color.b = b;

	    return GED_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The 'qray overlapcolor' command accepts 0 or 3 arguments\n");
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "vars")) {
	qray_print_vars(gedp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[1], "help")) {
	usage(gedp, argv[0]);
	return GED_HELP;
    }

    bu_vls_printf(gedp->ged_result_str, "qray: unrecognized command: '%s'\n", argv[1]);
    usage(gedp, argv[0]);

    return GED_ERROR;
}


void
qray_init(struct ged_drawable *gdp)
{
    int i;
    int n = 0;
    struct qray_fmt_data *qfdp;

    bu_vls_init(&gdp->gd_qray_basename);
    bu_vls_strcpy(&gdp->gd_qray_basename, DG_QRAY_BASENAME);
    bu_vls_init(&gdp->gd_qray_script);

    gdp->gd_qray_effects = 'b';
    gdp->gd_qray_cmd_echo = 0;
    gdp->gd_qray_odd_color = def_qray_odd_color;
    gdp->gd_qray_even_color = def_qray_even_color;
    gdp->gd_qray_void_color = def_qray_void_color;
    gdp->gd_qray_overlap_color = def_qray_overlap_color;

    /* count the number of default format types */
    for (qfdp = def_qray_fmt_data; qfdp->fmt != (char *)NULL; ++qfdp)
	++n;

    gdp->gd_qray_fmts = (struct ged_qray_fmt *)bu_calloc(n+1, sizeof(struct ged_qray_fmt), "qray_fmts");

    for (i = 0; i < n; ++i) {
	gdp->gd_qray_fmts[i].type = def_qray_fmt_data[i].type;
	bu_vls_init(&gdp->gd_qray_fmts[i].fmt);
	bu_vls_strcpy(&gdp->gd_qray_fmts[i].fmt, def_qray_fmt_data[i].fmt);
    }

    gdp->gd_qray_fmts[i].type = (char)0;
}


void
qray_free(struct ged_drawable *gdp)
{
    int i;

    bu_vls_free(&gdp->gd_qray_basename);
    bu_vls_free(&gdp->gd_qray_script);
    for (i = 0; gdp->gd_qray_fmts[i].type != (char)0; ++i)
	bu_vls_free(&gdp->gd_qray_fmts[i].fmt);
    bu_free(gdp->gd_qray_fmts, "dgo_free_qray");
}


void
qray_data_to_vlist(struct ged *gedp,
		   struct bn_vlblock *vbp,
		   struct qray_dataList *headp,
		   vect_t dir,
		   int do_overlaps)
{
    int i = 1;			/* start out odd */
    struct bu_list *vhead;
    struct qray_dataList *ndlp;
    vect_t in_pt, out_pt;
    vect_t last_out_pt = { 0, 0, 0 };

    for (BU_LIST_FOR(ndlp, qray_dataList, &headp->l)) {
	if (do_overlaps)
	    vhead = rt_vlblock_find(vbp,
				    gedp->ged_gdp->gd_qray_overlap_color.r,
				    gedp->ged_gdp->gd_qray_overlap_color.g,
				    gedp->ged_gdp->gd_qray_overlap_color.b);
	else if (i % 2)
	    vhead = rt_vlblock_find(vbp,
				    gedp->ged_gdp->gd_qray_odd_color.r,
				    gedp->ged_gdp->gd_qray_odd_color.g,
				    gedp->ged_gdp->gd_qray_odd_color.b);
	else
	    vhead = rt_vlblock_find(vbp,
				    gedp->ged_gdp->gd_qray_even_color.r,
				    gedp->ged_gdp->gd_qray_even_color.g,
				    gedp->ged_gdp->gd_qray_even_color.b);

	VSET(in_pt, ndlp->x_in, ndlp->y_in, ndlp->z_in);
	VJOIN1(out_pt, in_pt, ndlp->los, dir);
	VSCALE(in_pt, in_pt, gedp->ged_wdbp->dbip->dbi_local2base);
	VSCALE(out_pt, out_pt, gedp->ged_wdbp->dbip->dbi_local2base);
	RT_ADD_VLIST(vhead, in_pt, BN_VLIST_LINE_MOVE);
	RT_ADD_VLIST(vhead, out_pt, BN_VLIST_LINE_DRAW);

	if (!do_overlaps && i > 1 && !VNEAR_EQUAL(last_out_pt, in_pt, SQRT_SMALL_FASTF)) {
	    vhead = rt_vlblock_find(vbp,
				    gedp->ged_gdp->gd_qray_void_color.r,
				    gedp->ged_gdp->gd_qray_void_color.g,
				    gedp->ged_gdp->gd_qray_void_color.b);
	    RT_ADD_VLIST(vhead, last_out_pt, BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(vhead, in_pt, BN_VLIST_LINE_DRAW);
	}

	VMOVE(last_out_pt, out_pt);
	++i;
    }
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

/*                        P N G - F B . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file png-fb.c
 *
 * Program to take PNG (Portable Network Graphics) files and send them to a framebuffer.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bio.h"
#include "bu/cmdschema.h"
#include "dm.h"
#include "ged.h"

struct png2fb_state {
    fastf_t def_screen_gamma;	/* Don't add more gamma, default = 1.0*/
    /* Particularly because on SGI, the system provides gamma correction,
     * so programs like this one don't have to.
     */
    const char *file_name;
    int multiple_lines;	/* Streamlined operation */
    int file_xoff;
    int file_yoff;
    int scr_xoff;
    int scr_yoff;
    int clear;
    int zoom;
    int inverse;	/* Draw upside-down */
    int one_line_only;	/* insist on 1-line writes */
    int verbose;
    int header_only;
    int help;
};

static const struct bu_cmd_option png2fb_schema_options[] = {
    BU_CMD_FLAG("1", NULL, struct png2fb_state, one_line_only, "Use one-line writes"),
    BU_CMD_INTEGER("m", NULL, struct png2fb_state, multiple_lines, "lines",
	"Draw multiple lines per write"),
    BU_CMD_POSITIVE_NUMBER("g", NULL, struct png2fb_state, def_screen_gamma, "gamma",
	"Set screen gamma"),
    BU_CMD_FLAG("H", NULL, struct png2fb_state, header_only, "Read the PNG header only"),
    BU_CMD_FLAG("i", NULL, struct png2fb_state, inverse, "Draw upside-down"),
    BU_CMD_FLAG("c", NULL, struct png2fb_state, clear, "Clear the framebuffer"),
    BU_CMD_FLAG("v", NULL, struct png2fb_state, verbose, "Print verbose reporting"),
    BU_CMD_FLAG("z", NULL, struct png2fb_state, zoom, "Zoom to fill the screen"),
    BU_CMD_INTEGER("x", NULL, struct png2fb_state, file_xoff, "pixels", "Set input X offset"),
    BU_CMD_INTEGER("y", NULL, struct png2fb_state, file_yoff, "pixels", "Set input Y offset"),
    BU_CMD_INTEGER("X", NULL, struct png2fb_state, scr_xoff, "pixels", "Set framebuffer X offset"),
    BU_CMD_INTEGER("Y", NULL, struct png2fb_state, scr_yoff, "pixels", "Set framebuffer Y offset"),
    BU_CMD_FLAG("h", "help", struct png2fb_state, help, "Print command help"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand png2fb_schema_operands[] = {
    BU_CMD_OPERAND("input_file", BU_CMD_VALUE_FILE, 0, 1,
	"Optional PNG input file", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema png2fb_cmd_schema = {
    "png2fb", "Display a PNG image in the framebuffer", png2fb_schema_options,
    png2fb_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};

static void
png2fb_show_help(struct ged *gedp, const char *command)
{
    char *option_help = bu_cmd_schema_describe(&png2fb_cmd_schema);

    bu_vls_printf(gedp->ged_result_str,
	"Usage: %s [-H] [-i] [-c] [-v] [-z] [-1] [-m lines] [-g gamma] "
	"[-x pixels] [-y pixels] [-X pixels] [-Y pixels] [file.png]", command);
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "\nOptions:\n%s", option_help);
	bu_free(option_help, "png2fb native option help");
    }
}


int
ged_png2fb_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int operand_index;
    FILE *fp_in = NULL;
    struct png2fb_state p2fbs = {1.0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	png2fb_show_help(gedp, argv[0]);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&png2fb_cmd_schema, &p2fbs,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	png2fb_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
    }
    if (p2fbs.help) {
	png2fb_show_help(gedp, argv[0]);
	return GED_HELP;
    }
    if (operand_index < argc - 1)
	p2fbs.file_name = argv[operand_index + 1];

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, "no current view set\n");
	return BRLCAD_ERROR;
    }

    struct dm *dmp = (struct dm *)gedp->ged_gvp->dmp;
    if (!dmp) {
	bu_vls_printf(gedp->ged_result_str, "no display manager currently active");
	return BRLCAD_ERROR;
    }

    struct fb *fbp = dm_get_fb(dmp);

    if (!fbp) {
	bu_vls_printf(gedp->ged_result_str, "display manager does not have a framebuffer");
	return BRLCAD_ERROR;
    }
    if (!p2fbs.file_name) {
	if (isatty(fileno(stdin))) {
	png2fb_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
	}
	p2fbs.file_name = "-";
	fp_in = stdin;
	setmode(fileno(fp_in), O_BINARY);
    } else {
	fp_in = fopen(p2fbs.file_name, "rb");
	if (!fp_in) {
	    bu_vls_printf(gedp->ged_result_str, "cannot open \"%s\" for reading\n",
		p2fbs.file_name);
	    return BRLCAD_ERROR;
	}
    }

    ret = fb_read_png(fbp, fp_in,
		      p2fbs.file_xoff, p2fbs.file_yoff,
		      p2fbs.scr_xoff, p2fbs.scr_yoff,
		      p2fbs.clear, p2fbs.zoom, p2fbs.inverse,
		      p2fbs.one_line_only, p2fbs.multiple_lines,
		      p2fbs.verbose, p2fbs.header_only,
		      p2fbs.def_screen_gamma,
		      gedp->ged_result_str);

    if (fp_in != stdin)
	fclose(fp_in);

    (void)dm_draw_begin(dmp);
    fb_refresh(fbp, 0, 0, fb_getwidth(fbp), fb_getheight(fbp));
    (void)dm_draw_end(dmp);

    if (ret == BRLCAD_OK)
	return BRLCAD_OK;

    return BRLCAD_ERROR;
}


#include "../include/plugin.h"

#define GED_PNG2FB_COMMANDS(X, XID) \
    X(png2fb, ged_png2fb_core, GED_CMD_DEFAULT, &png2fb_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_PNG2FB_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_png2fb", 1, GED_PNG2FB_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

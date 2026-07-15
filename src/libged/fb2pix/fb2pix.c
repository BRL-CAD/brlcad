/*                        F B 2 P I X . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
/** @file libged/fb2pix.c
 *
 * fb2pix writes a framebuffer image to a .pix file.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <sys/stat.h>

#include "bio.h"

#include "bu/app.h"
#include "bu/cmdschema.h"
#include "dm.h"

#include "pkg.h"
#include "ged.h"


struct fb2pix_state {
    int crunch;		/* Color map crunch? */
    int inverse;	/* Draw upside-down */
    int square_size;
    int requested_width;
    int requested_height;
    int screen_height;	/* input height */
    int screen_width;	/* input width */
    int help;
    const char *file_name;
};

static const struct bu_cmd_option fb2pix_schema_options[] = {
    BU_CMD_FLAG("c", NULL, struct fb2pix_state, crunch, "Crunch the color map"),
    BU_CMD_FLAG("i", NULL, struct fb2pix_state, inverse, "Write upside-down"),
	BU_CMD_POSITIVE_INTEGER("s", NULL, struct fb2pix_state, square_size, "pixels",
	"Set both image dimensions"),
	BU_CMD_POSITIVE_INTEGER("w", NULL, struct fb2pix_state, requested_width, "pixels",
	"Set image width"),
	BU_CMD_POSITIVE_INTEGER("n", NULL, struct fb2pix_state, requested_height, "pixels",
	"Set image height"),
    BU_CMD_FLAG("h", "help", struct fb2pix_state, help, "Print command help"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand fb2pix_schema_operands[] = {
    BU_CMD_OPERAND("output_file", BU_CMD_VALUE_FILE, 0, 1,
	"Optional PIX output file", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema fb2pix_cmd_schema = {
    "fb2pix", "Write the framebuffer to a PIX file", fb2pix_schema_options,
    fb2pix_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};

static void
fb2pix_show_help(struct ged *gedp, const char *command)
{
    char *option_help = bu_cmd_schema_describe(&fb2pix_cmd_schema);

    bu_vls_printf(gedp->ged_result_str,
	"Usage: %s [-c] [-i] [-s pixels] [-w pixels] [-n pixels] [file.pix]",
	command);
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "\nOptions:\n%s", option_help);
	bu_free(option_help, "fb2pix native option help");
    }
}


int
ged_fb2pix_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int operand_index;
    FILE *outfp = NULL;
    struct fb2pix_state f2ps = {0, 0, 0, 0, 0, 0, 0, 0, NULL};

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	fb2pix_show_help(gedp, argv[0]);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&fb2pix_cmd_schema, &f2ps,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	fb2pix_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
    }
    if (f2ps.help) {
	fb2pix_show_help(gedp, argv[0]);
	return GED_HELP;
    }
    if (operand_index < argc - 1)
	f2ps.file_name = argv[operand_index + 1];
    f2ps.screen_width = f2ps.screen_height = f2ps.square_size > 0 ? f2ps.square_size : 512;
    if (f2ps.requested_width > 0)
	f2ps.screen_width = f2ps.requested_width;
    if (f2ps.requested_height > 0)
	f2ps.screen_height = f2ps.requested_height;

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no current view set\n");
	return BRLCAD_ERROR;
    }

    struct dm *dmp = (struct dm *)gedp->ged_gvp->dmp;
    if (!dmp) {
	bu_vls_printf(gedp->ged_result_str, ": no current display manager set\n");
	return BRLCAD_ERROR;
    }

    struct fb *fbp = dm_get_fb(dmp);

    if (!fbp) {
	bu_vls_printf(gedp->ged_result_str, "display manager does not have a framebuffer");
	return BRLCAD_ERROR;
    }
    if (!f2ps.file_name) {
	if (isatty(fileno(stdout))) {
	fb2pix_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
	}
	f2ps.file_name = "-";
	outfp = stdout;
    } else {
	outfp = fopen(f2ps.file_name, "wb");
	if (!outfp) {
	    bu_vls_printf(gedp->ged_result_str, "cannot open \"%s\" for writing\n",
		f2ps.file_name);
	    return BRLCAD_ERROR;
	}
	(void)bu_fchmod(fileno(outfp), 0444);
    }

    setmode(fileno(stdout), O_BINARY);

    ret = fb_write_fp(fbp, outfp,
		      f2ps.screen_width, f2ps.screen_height,
		      f2ps.crunch, f2ps.inverse, gedp->ged_result_str);

    if (outfp != stdout)
	fclose(outfp);

    if (ret == BRLCAD_OK) {
	(void)dm_draw_begin(dmp);
	fb_refresh(fbp, 0, 0, fb_getwidth(fbp), fb_getheight(fbp));
	(void)dm_draw_end(dmp);
	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
}

#include "../include/plugin.h"

#define GED_FB2PIX_COMMANDS(X, XID) \
    X(fb2pix, ged_fb2pix_core, GED_CMD_DEFAULT, &fb2pix_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_FB2PIX_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_fb2pix", 1, GED_FB2PIX_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

/*                        P I X 2 F B . C
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
/** @file libged/pix2fb.c
 *
 * Program to take bottom-up pixel files and send them to a framebuffer.
 *
 */

#include "common.h"

#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#ifdef HAVE_WINSOCK_H
#  include <winsock.h>
#endif

#include "bio.h"

#include "bu/cmdschema.h"
#include "bu/snooze.h"
#include "dm.h"

#include "pkg.h"
#include "ged.h"

struct pix2fb_state {
    size_t file_width;	/* default input width */
    size_t file_height;	/* default input height */
    char *file_name;
    int multiple_lines;	/* Streamlined operation */
    int infd;
    int fileinput;		/* file or pipe on input? */
    int autosize;		/* !0 to autosize input */
    int scr_width;		/* screen tracks file if not given */
    int scr_height;
    int file_xoff;
    int file_yoff;
    int scr_xoff;
    int scr_yoff;
    int clear;
    int zoom;
    int inverse;		/* Draw upside-down */
    int one_line_only;	/* insist on 1-line writes */
    int pause_sec; 	/* Pause that many seconds before
			   closing the FB and exiting */
};

#define PIX2FB_STATE_INIT_ZERO {512, 512, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

struct pix2fb_args {
    int multiple_lines;
    int autosize;
    int file_square_size;
    int file_width;
    int file_height;
    int file_xoff;
    int file_yoff;
    int screen_square_size;
    int screen_width;
    int screen_height;
    int screen_xoff;
    int screen_yoff;
    int clear;
    int zoom;
    int inverse;
    int one_line_only;
    int pause_sec;
    int help;
    const char *file_name;
};

static const struct bu_cmd_option pix2fb_schema_options[] = {
    BU_CMD_FLAG("1", NULL, struct pix2fb_args, one_line_only, "Use one-line writes"),
    BU_CMD_INTEGER("m", NULL, struct pix2fb_args, multiple_lines, "lines",
	"Set multiple-line write count"),
    BU_CMD_FLAG("a", NULL, struct pix2fb_args, autosize, "Autosize the input"),
    BU_CMD_FLAG("i", NULL, struct pix2fb_args, inverse, "Draw upside-down"),
    BU_CMD_FLAG("c", NULL, struct pix2fb_args, clear, "Clear the framebuffer"),
    BU_CMD_FLAG("z", NULL, struct pix2fb_args, zoom, "Zoom to fill the screen"),
    BU_CMD_POSITIVE_INTEGER("s", NULL, struct pix2fb_args, file_square_size, "pixels",
	"Set both input dimensions"),
    BU_CMD_POSITIVE_INTEGER("w", NULL, struct pix2fb_args, file_width, "pixels",
	"Set input width"),
    BU_CMD_POSITIVE_INTEGER("n", NULL, struct pix2fb_args, file_height, "pixels",
	"Set input height"),
    BU_CMD_INTEGER("x", NULL, struct pix2fb_args, file_xoff, "pixels", "Set input X offset"),
    BU_CMD_INTEGER("y", NULL, struct pix2fb_args, file_yoff, "pixels", "Set input Y offset"),
    BU_CMD_INTEGER("X", NULL, struct pix2fb_args, screen_xoff, "pixels", "Set framebuffer X offset"),
    BU_CMD_INTEGER("Y", NULL, struct pix2fb_args, screen_yoff, "pixels", "Set framebuffer Y offset"),
    BU_CMD_POSITIVE_INTEGER("S", NULL, struct pix2fb_args, screen_square_size, "pixels",
	"Set both framebuffer dimensions"),
    BU_CMD_POSITIVE_INTEGER("W", NULL, struct pix2fb_args, screen_width, "pixels",
	"Set framebuffer width"),
    BU_CMD_POSITIVE_INTEGER("N", NULL, struct pix2fb_args, screen_height, "pixels",
	"Set framebuffer height"),
    BU_CMD_NONNEGATIVE_INTEGER("p", NULL, struct pix2fb_args, pause_sec, "seconds",
	"Pause before returning"),
    BU_CMD_FLAG("h", "help", struct pix2fb_args, help, "Print command help"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand pix2fb_schema_operands[] = {
    BU_CMD_OPERAND("input_file", BU_CMD_VALUE_FILE, 0, 1,
	"Optional PIX input file", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema pix2fb_cmd_schema = {
    "pix2fb", "Display a PIX image in the framebuffer", pix2fb_schema_options,
    pix2fb_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};

static void
pix2fb_show_help(struct ged *gedp, const char *command)
{
    char *option_help = bu_cmd_schema_describe(&pix2fb_cmd_schema);

    bu_vls_printf(gedp->ged_result_str,
	"Usage: %s [-a] [-i] [-c] [-z] [-1] [-m lines] [-s pixels] [-w pixels] "
	"[-n pixels] [-x pixels] [-y pixels] [-X pixels] [-Y pixels] [-S pixels] "
	"[-W pixels] [-N pixels] [-p seconds] [file.pix]", command);
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "\nOptions:\n%s", option_help);
	bu_free(option_help, "pix2fb native option help");
    }
}


int
ged_pix2fb_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    int operand_index;
    struct pix2fb_args args = {0};

    struct pix2fb_state p2fbs = PIX2FB_STATE_INIT_ZERO;

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	pix2fb_show_help(gedp, argv[0]);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&pix2fb_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	pix2fb_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
    }
    if (args.help) {
	pix2fb_show_help(gedp, argv[0]);
	return GED_HELP;
    }
    if (operand_index < argc - 1)
	args.file_name = argv[operand_index + 1];

    p2fbs.multiple_lines = args.multiple_lines;
    p2fbs.autosize = args.autosize;
    p2fbs.file_xoff = args.file_xoff;
    p2fbs.file_yoff = args.file_yoff;
    p2fbs.scr_xoff = args.screen_xoff;
    p2fbs.scr_yoff = args.screen_yoff;
    p2fbs.clear = args.clear;
    p2fbs.zoom = args.zoom;
    p2fbs.inverse = args.inverse;
    p2fbs.one_line_only = args.one_line_only;
    p2fbs.pause_sec = args.pause_sec;
    if (args.file_square_size > 0)
	p2fbs.file_width = p2fbs.file_height = (size_t)args.file_square_size;
    if (args.file_width > 0)
	p2fbs.file_width = (size_t)args.file_width;
    if (args.file_height > 0)
	p2fbs.file_height = (size_t)args.file_height;
    if (args.file_square_size > 0 || args.file_width > 0 || args.file_height > 0)
	p2fbs.autosize = 0;
    if (args.screen_square_size > 0)
	p2fbs.scr_width = p2fbs.scr_height = args.screen_square_size;
    if (args.screen_width > 0)
	p2fbs.scr_width = args.screen_width;
    if (args.screen_height > 0)
	p2fbs.scr_height = args.screen_height;

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
    if (!args.file_name) {
	if (isatty(fileno(stdin))) {
	pix2fb_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
	}
	p2fbs.file_name = (char *)"-";
	p2fbs.infd = 0;
	setmode(p2fbs.infd, O_BINARY);
    } else {
	p2fbs.file_name = (char *)args.file_name;
	p2fbs.infd = open(p2fbs.file_name, 0);
	if (p2fbs.infd < 0) {
	    bu_vls_printf(gedp->ged_result_str, "cannot open \"%s\" for reading\n",
		p2fbs.file_name);
	    return BRLCAD_ERROR;
	}
	setmode(p2fbs.infd, O_BINARY);
	p2fbs.fileinput = 1;
    }

    ret = fb_read_fd(fbp, p2fbs.infd,
		     p2fbs.file_width, p2fbs.file_height,
		     p2fbs.file_xoff, p2fbs.file_yoff,
		     p2fbs.scr_width, p2fbs.scr_height,
		     p2fbs.scr_xoff, p2fbs.scr_yoff,
		     p2fbs.fileinput, p2fbs.file_name, p2fbs.one_line_only, p2fbs.multiple_lines,
		     p2fbs.autosize, p2fbs.inverse, p2fbs.clear, p2fbs.zoom,
		     gedp->ged_result_str);

    if (p2fbs.infd != 0)
	close(p2fbs.infd);

    bu_snooze(BU_SEC2USEC(p2fbs.pause_sec));


    if (ret == BRLCAD_OK) {
	(void)dm_draw_begin(dmp);
	fb_refresh(fbp, 0, 0, fb_getwidth(fbp), fb_getheight(fbp));
	(void)dm_draw_end(dmp);
	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
}


#include "../include/plugin.h"

#define GED_PIX2FB_COMMANDS(X, XID) \
    X(pix2fb, ged_pix2fb_core, GED_CMD_DEFAULT, &pix2fb_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_PIX2FB_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_pix2fb", 1, GED_PIX2FB_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

/*                        P N G - F B . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2022 United States Government as represented by
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
#include "bu/getopt.h"
#include "dm.h"
#include "ged.h"

static int multiple_lines = 0;	/* Streamlined operation */

static char *file_name;
static FILE *fp_in;

static int file_xoff=0, file_yoff=0;
static int scr_xoff=0, scr_yoff=0;
static int clear = 0;
static int zoom = 0;
static int inverse = 0;		/* Draw upside-down */
static int one_line_only = 0;	/* insist on 1-line writes */
static int verbose = 0;
static int header_only = 0;

static double def_screen_gamma=1.0;	/* Don't add more gamma, by default */
/* Particularly because on SGI, the system provides gamma correction,
 * so programs like this one don't have to.
 */

static char usage[] = "\
Usage: png2fb [-H -i -c -v -z -1] [-m #lines]\n\
	[-g screen_gamma]\n\
	[-x file_xoff] [-y file_yoff] [-X scr_xoff] [-Y scr_yoff]\n\
	[-S squarescrsize] [file.png]\n";

static int
get_args(int argc, char **argv)
{
    int c;

    bu_optind = 1;
    while ((c = bu_getopt(argc, argv, "1m:g:HicvzF:x:y:X:Y:S:W:N:h?")) != -1) {
	switch (c) {
	    case '1':
		one_line_only = 1;
		break;
	    case 'm':
		multiple_lines = atoi(bu_optarg);
		break;
	    case 'g':
		def_screen_gamma = atof(bu_optarg);
		break;
	    case 'H':
		header_only = 1;
		break;
	    case 'i':
		inverse = 1;
		break;
	    case 'c':
		clear = 1;
		break;
	    case 'v':
		verbose = 1;
		break;
	    case 'z':
		zoom = 1;
		break;
	    case 'x':
		file_xoff = atoi(bu_optarg);
		break;
	    case 'y':
		file_yoff = atoi(bu_optarg);
		break;
	    case 'X':
		scr_xoff = atoi(bu_optarg);
		break;
	    case 'Y':
		scr_yoff = atoi(bu_optarg);
		break;
	    default:		/* '?''h' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return 0;
	file_name = "-";
	fp_in = stdin;
	setmode(fileno(fp_in), O_BINARY);
    } else {
	file_name = argv[bu_optind];
	fp_in = fopen(file_name, "rb");
	if (fp_in == NULL) {
	    perror(file_name);
	    fprintf(stderr,
		    "png-fb: cannot open \"%s\" for reading\n",
		    file_name);
	    return 0;
	}
    }

    if (argc > ++bu_optind)
	fprintf(stderr, "png-fb: excess argument(s) ignored\n");

    return 1;		/* OK */
}


int
ged_png2fb_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    if (!gedp->ged_dmp) {
	bu_vls_printf(gedp->ged_result_str, "no display manager currently active");
	return BRLCAD_ERROR;
    }

    struct dm *dmp = (struct dm *)gedp->ged_dmp;
    struct fb *fbp = dm_get_fb(dmp);

    if (!fbp) {
	bu_vls_printf(gedp->ged_result_str, "display manager does not have a framebuffer");
	return BRLCAD_ERROR;
    }

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (!get_args(argc, (char **)argv)) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    ret = fb_read_png(fbp, fp_in,
		      file_xoff, file_yoff,
		      scr_xoff, scr_yoff,
		      clear, zoom, inverse,
		      one_line_only, multiple_lines,
		      verbose, header_only,
		      def_screen_gamma,
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


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl png2fb_cmd_impl = {
    "png2fb",
    ged_png2fb_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd png2fb_cmd = { &png2fb_cmd_impl };
const struct ged_cmd *png2fb_cmds[] = { &png2fb_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  png2fb_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

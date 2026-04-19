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
#include "bu/getopt.h"
#include "dm.h"
#include "ged.h"

struct png2fb_state {
    double def_screen_gamma;	/* Don't add more gamma, default = 1.0*/
    /* Particularly because on SGI, the system provides gamma correction,
     * so programs like this one don't have to.
     */
    char *file_name;
    FILE *fp_in;
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
};

#define PNG2FB_STATE_INIT_ZERO {1.0, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

static char png2fb_usage[] = "\
Usage: png2fb [-H -i -c -v -z -1] [-m #lines]\n\
	[-g screen_gamma]\n\
	[-x file_xoff] [-y file_yoff] [-X scr_xoff] [-Y scr_yoff]\n\
	[-S squarescrsize] [file.png]\n";

static int
png2fb_get_args(struct png2fb_state *s, int argc, char **argv)
{
    int c;

    if (!s)
	return 0;

    bu_optind = 1;
    while ((c = bu_getopt(argc, argv, "1m:g:HicvzF:x:y:X:Y:S:W:N:h?")) != -1) {
	switch (c) {
	    case '1':
		s->one_line_only = 1;
		break;
	    case 'm':
		s->multiple_lines = atoi(bu_optarg);
		break;
	    case 'g':
		s->def_screen_gamma = atof(bu_optarg);
		break;
	    case 'H':
		s->header_only = 1;
		break;
	    case 'i':
		s->inverse = 1;
		break;
	    case 'c':
		s->clear = 1;
		break;
	    case 'v':
		s->verbose = 1;
		break;
	    case 'z':
		s->zoom = 1;
		break;
	    case 'x':
		s->file_xoff = atoi(bu_optarg);
		break;
	    case 'y':
		s->file_yoff = atoi(bu_optarg);
		break;
	    case 'X':
		s->scr_xoff = atoi(bu_optarg);
		break;
	    case 'Y':
		s->scr_yoff = atoi(bu_optarg);
		break;
	    default:		/* '?''h' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return 0;
	s->file_name = "-";
	s->fp_in = stdin;
	setmode(fileno(s->fp_in), O_BINARY);
    } else {
	s->file_name = argv[bu_optind];
	s->fp_in = fopen(s->file_name, "rb");
	if (s->fp_in == NULL) {
	    perror(s->file_name);
	    fprintf(stderr,
		    "png-fb: cannot open \"%s\" for reading\n",
		    s->file_name);
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

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    struct png2fb_state p2fbs = PNG2FB_STATE_INIT_ZERO;

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

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], png2fb_usage);
	return GED_HELP;
    }

    if (!png2fb_get_args(&p2fbs, argc, (char **)argv)) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], png2fb_usage);
	return GED_HELP;
    }

    ret = fb_read_png(fbp, p2fbs.fp_in,
		      p2fbs.file_xoff, p2fbs.file_yoff,
		      p2fbs.scr_xoff, p2fbs.scr_yoff,
		      p2fbs.clear, p2fbs.zoom, p2fbs.inverse,
		      p2fbs.one_line_only, p2fbs.multiple_lines,
		      p2fbs.verbose, p2fbs.header_only,
		      p2fbs.def_screen_gamma,
		      gedp->ged_result_str);

    if (p2fbs.fp_in != stdin)
	fclose(p2fbs.fp_in);

    (void)dm_draw_begin(dmp);
    fb_refresh(fbp, 0, 0, fb_getwidth(fbp), fb_getheight(fbp));
    (void)dm_draw_end(dmp);

    if (ret == BRLCAD_OK)
	return BRLCAD_OK;

    return BRLCAD_ERROR;
}


#include "../include/plugin.h"

#define GED_PNG2FB_COMMANDS(X, XID) \
    X(png2fb, ged_png2fb_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_PNG2FB_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_png2fb", 1, GED_PNG2FB_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

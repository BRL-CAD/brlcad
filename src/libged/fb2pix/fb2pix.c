/*                        F B 2 P I X . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2022 United States Government as represented by
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
#include "bu/getopt.h"
#include "dm.h"

#include "pkg.h"
#include "ged.h"


char *file_name;
FILE *outfp;

static int crunch = 0;		/* Color map crunch? */
static int inverse = 0;		/* Draw upside-down */
int screen_height;			/* input height */
int screen_width;			/* input width */


static int
get_args(int argc, char **argv)
{
    int c;

    bu_optind = 1;
    while ((c = bu_getopt(argc, argv, "ciF:s:w:n:h?")) != -1) {
	switch (c) {
	    case 'c':
		crunch = 1;
		break;
	    case 'i':
		inverse = 1;
		break;
	    case 's':
		/* square size */
		screen_height = screen_width = atoi(bu_optarg);
		break;
	    case 'w':
		screen_width = atoi(bu_optarg);
		break;
	    case 'n':
		screen_height = atoi(bu_optarg);
		break;

	    default:		/* 'h' '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdout)))
	    return 0;
	file_name = "-";
	outfp = stdout;
    } else {
	file_name = argv[bu_optind];
	outfp = fopen(file_name, "wb");
	if (outfp == NULL) {
	    fprintf(stderr,
			  "fb-pix: cannot open \"%s\" for writing\n",
			  file_name);
	    return 0;
	}
	(void)bu_fchmod(fileno(outfp), 0444);
    }

    if (argc > ++bu_optind)
	fprintf(stderr, "fb-pix: excess argument(s) ignored\n");

    return 1;		/* OK */
}


int
ged_fb2pix_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    char usage[] = "Usage: fb-pix [-h -i -c] \n\
	[-s squaresize] [-w width] [-n height] [file.pix]\n";

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

    screen_height = screen_width = 512;		/* Defaults */

    if (!get_args(argc, (char **)argv)) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    setmode(fileno(stdout), O_BINARY);

    ret = fb_write_fp(fbp, outfp,
		      screen_width, screen_height,
		      crunch, inverse, gedp->ged_result_str);

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


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl fb2pix_cmd_impl = {
    "fb2pix",
    ged_fb2pix_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd fb2pix_cmd = { &fb2pix_cmd_impl };
const struct ged_cmd *fb2pix_cmds[] = { &fb2pix_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  fb2pix_cmds, 1 };

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

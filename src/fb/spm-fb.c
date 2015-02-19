/*                        S P M - F B . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2014 United States Government as represented by
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
/** @file spm-fb.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>

#include "bu/getopt.h"
#include "bu/log.h"
#include "fb.h"
#include "spm.h"


static fb *fbp;

static char *framebuffer = NULL;
static int scr_width = 0;
static int scr_height = 0;

static char *file_name;
static int square = 0;
static int vsize;

static char usage[] = "\
Usage: spm-fb [-s] [-F framebuffer]\n\
	[-S squarescrsize] [-W scr_width] [-N scr_height]\n\
	vsize [filename]\n";


int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "F:sS:W:N:h?")) != -1) {
	switch (c) {
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 's':
		square = 1;
		break;
	    case 'S':
		scr_height = scr_width = atoi(bu_optarg);
		break;
	    case 'W':
		scr_width = atoi(bu_optarg);
		break;
	    case 'N':
		scr_height = atoi(bu_optarg);
		break;

	    default:		/* '?' 'h' */
		return 0;
	}
    }

    if (bu_optind >= argc)
	return 0;		/* missing positional arg */
    vsize = atoi(argv[bu_optind++]);

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return 0;
	file_name = "-";
    } else {
	file_name = argv[bu_optind];
    }

    if (argc > ++bu_optind)
	fprintf(stderr, "spm-fb: excess argument(s) ignored\n");

    return 1;		/* OK */
}


/*
 * Displays a sphere map on a framebuffer.
 */
void
spm_fb(bn_spm_map_t *mapp)
{
    int j;

    for (j = 0; j < mapp->ny; j++) {
	fb_write(fbp, 0, j, mapp->xbin[j], mapp->nx[j]);
#ifdef NEVER
	for (i = 0; i < mapp->nx[j]; i++) {
	    rgb[RED] = mapp->xbin[j][i*3];
	    rgb[GRN] = mapp->xbin[j][i*3+1];
	    rgb[BLU] = mapp->xbin[j][i*3+2];
	    fb_write(fbp, i, j, (unsigned char *)rgb, 1);
	}
#endif
    }
}


/*
 * Display a square sphere map on a framebuffer.
 */
void
spm_square(bn_spm_map_t *mapp)
{
    int x, y;
    unsigned char *scanline;

    scanline = (unsigned char *)malloc(scr_width * sizeof(RGBpixel));

    for (y = 0; y < scr_height; y++) {
	for (x = 0; x < scr_width; x++) {
	    bn_spm_read(mapp, &scanline[x],
			(double)x/(double)scr_width,
			(double)y/(double)scr_height);
	}
	if (fb_write(fbp, 0, y, scanline, scr_width) != scr_width) break;
    }
    free(scanline);
}


int
main(int argc, char **argv)
{
    bn_spm_map_t *mp;

    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit(1, NULL);
    }

    if ((fbp = fb_open(framebuffer, scr_width, scr_height)) == FB_NULL)
	bu_exit(12, NULL);
    scr_width = fb_getwidth(fbp);
    scr_height = fb_getheight(fbp);

    mp = bn_spm_init(vsize, sizeof(RGBpixel));
    if (mp == BN_SPM_MAP_NULL || fbp == FB_NULL)
	bu_exit(1, NULL);

    bn_spm_load(mp, file_name);

    if (square)
	spm_square(mp);
    else
	spm_fb(mp);

    bn_spm_free(mp);
    fb_close(fbp);
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

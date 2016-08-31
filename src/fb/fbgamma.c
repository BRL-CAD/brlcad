/*                       F B G A M M A . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2016 United States Government as represented by
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
/** @file fbgamma.c
 *
 * Load a gamma correcting colormap into a framebuffer.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bu/malloc.h"
#include "bu/getopt.h"
#include "bu/log.h"
#include "fb.h"
#include "pkg.h"

char *options = "iHoF:h?";

void checkgamma(double g);

unsigned char rampval[10] = { 255, 128, 64, 32, 16, 8, 4, 2, 1, 0 };
int x, y, scr_width, scr_height, patch_width, patch_height;
unsigned char *altline;
unsigned char *line;
char *framebuffer = (char *)NULL;
int image = 0;

static char usage[] = "\
Usage: fbgamma [-H -o -i] [-F framebuffer] val [gval bval]\n";

void mk_ramp(fb *fb_i, int r, int g, int b, int n)
{

    /* grey ramp */
    for (x=0; x < scr_width; ++x) {
	if (r) line[x*3] = rampval[x / patch_width + 1];
	else line[x*3] = 0;
	if (g) line[x*3+1] = rampval[x / patch_width + 1];
	else line[x*3+1] = 0;
	if (b) line[x*3+2] = rampval[x / patch_width + 1];
	else line[x*3+2] = 0;
    }
    for (y=patch_height*n; y < patch_height*(n+1) && y < scr_height; ++y) {
	fb_write(fb_i, 0, y, line, scr_width);
    }

    for (x=0; x < scr_width; ++x) {
	if (r) line[x*3] = rampval[x / patch_width];
	else line[x*3] = 0;
	if (g) line[x*3+1] = rampval[x / patch_width];
	else line[x*3+1] = 0;
	if (b) line[x*3+2] = rampval[x / patch_width];
	else line[x*3+2] = 0;
    }
    for (y=patch_height*(n+1); y < patch_height*(n+2) && y < scr_height; y += 2) {
	fb_write(fb_i, 0, y, altline, scr_width);
	fb_write(fb_i, 0, y+1, line, scr_width);
    }
}


void disp_image(fb *fb_i)
{

    scr_width = fb_getwidth(fb_i);
    scr_height = fb_getheight(fb_i);

    patch_width = scr_width / 8;
    patch_height = scr_height / 14;

    line = (unsigned char *) bu_malloc(scr_width*3, "line");
    altline = (unsigned char *) bu_calloc(scr_width*3, sizeof(unsigned char), "altline");

    mk_ramp(fb_i, 1, 1, 1, 0);
    mk_ramp(fb_i, 1, 0, 0, 2);
    mk_ramp(fb_i, 1, 1, 0, 4);
    mk_ramp(fb_i, 0, 1, 0, 6);
    mk_ramp(fb_i, 0, 1, 1, 8);
    mk_ramp(fb_i, 0, 0, 1, 10);
    mk_ramp(fb_i, 1, 0, 1, 12);

    (void)bu_free(line, "line");
    (void)bu_free(altline, "altline");
}


int
main(int argc, char **argv)
{
    int i;
    int onegamma = 0;
    int fbsize = 512;
    int overlay = 0;
    double gamr = 0, gamg = 0, gamb = 0;	/* gamma's */
    double f;
    ColorMap cm;
    fb *fbp;

    onegamma = 0;

    /* check for flags */
    bu_opterr = 0;
    while ((i=bu_getopt(argc, argv, options)) != -1) {
	switch (i) {
	    case 'H'	: fbsize = 1024; break;
	    case 'o'	: overlay++; break;
	    case 'i'	: image = !image; break;
	    case 'F'	: framebuffer = bu_optarg; break;
	    default	: bu_exit(1, "%s", usage);
	}
    }

    if (bu_optind == argc - 1) {
	/* single value for all channels */
	f = atof(argv[bu_optind]);
	checkgamma(f);
	gamr = gamg = gamb = 1.0 / f;
	onegamma++;
    } else if (bu_optind == argc - 4) {
	/* different RGB values */
	f = atof(argv[bu_optind]);
	checkgamma(f);
	gamr = 1.0 / f;
	f = atof(argv[bu_optind+1]);
	checkgamma(f);
	gamg = 1.0 / f;
	f = atof(argv[bu_optind+2]);
	checkgamma(f);
	gamb = 1.0 / f;
    } else {
	bu_exit(1, "%s", usage);
    }

    if ((fbp = fb_open(framebuffer, fbsize, fbsize)) == FB_NULL) {
	bu_exit(2, "Unable to open framebuffer\n");
    }

    /* draw the gamma image if requested */
    if (image) disp_image(fbp);

    /* get the starting map */
    if (overlay) {
	fb_rmap(fbp, &cm);
    } else {
	/* start with a linear map */
	for (i = 0; i < 256; i++) {
	    cm.cm_red[i] = cm.cm_green[i]
		= cm.cm_blue[i] = i << 8;
	}
    }

    /* apply the gamma(s) */
    for (i = 0; i < 256; i++) {
	if (gamr < 0)
	    cm.cm_red[i] = 65535 * pow((double)cm.cm_red[i] / 65535.0, -1.0/gamr);
	else
	    cm.cm_red[i] = 65535 * pow((double)cm.cm_red[i] / 65535.0, gamr);
	if (onegamma && (overlay == 0)) {
	    cm.cm_green[i] = cm.cm_red[i];
	    cm.cm_blue[i]  = cm.cm_red[i];
	} else {
	    if (gamg < 0)
		cm.cm_green[i] = 65535 * pow((double)cm.cm_green[i] / 65535.0, -1.0/gamg);
	    else
		cm.cm_green[i] = 65535 * pow((double)cm.cm_green[i] / 65535.0, gamg);
	    if (gamb < 0)
		cm.cm_blue[i]  = 65535 * pow((double)cm.cm_blue[i] / 65535.0, -1.0/gamb);
	    else
		cm.cm_blue[i]  = 65535 * pow((double)cm.cm_blue[i] / 65535.0, gamb);
	}
    }

    fb_wmap(fbp, &cm);
    fb_close(fbp);
    return 0;
}


void
checkgamma(double g)
{
    if (fabs(g) < 1.0e-10) {
	fprintf(stderr, "fbgamma: gamma too close to zero\n");
	bu_exit(3, "%s", usage);
    }
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

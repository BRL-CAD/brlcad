/*                       F B C M R O T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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
/** @file fbcmrot.c
 *
 * Dynamicly rotate the color map
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>		/* For struct timeval */
#endif
#include "bselect.h"
#include "bio.h"

#include "bu.h"
#include "fb.h"
#include "vmath.h"

ColorMap cm1, cm2;
ColorMap *local_inp, *local_outp;

int size = 512;
double fps = 0.0;	/* frames per second */
int increment = 1;
int onestep = 0;

FBIO *fbp;

static char usage[] = "\
Usage: fbcmrot [-h] [-i increment] steps_per_second\n";

int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "hi:")) != -1) {
	switch (c) {
	    case 'h':
		/* high-res */
		size = 1024;
		break;
	    case 'i':
		/* increment */
		increment = atoi(bu_optarg);
		break;

	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	/* no fps specified */
	fps = 0;
    } else {
	fps = atof(argv[bu_optind]);
	if (NEAR_ZERO(fps, VDIVIDE_TOL))
	    onestep++;
    }

    if (argc > ++bu_optind)
	(void)fprintf(stderr, "fbcmrot: excess argument(s) ignored\n");

    return 1;		/* OK */
}

int
main(int argc, char **argv)
{
    int i;
    struct timeval tv;

    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit(1, NULL);
    }

    if (fps > 0.0) {
	tv.tv_sec = (long) (1.0 / fps);
	tv.tv_usec = (long) (((1.0 / fps) - tv.tv_sec) * 1000000);
    }

    if ((fbp = fb_open(NULL, size, size)) == FBIO_NULL) {
	fprintf(stderr, "fbcmrot:  fb_open failed\n");
	return	1;
    }

    local_inp = &cm1;
    local_outp = &cm2;
    fb_rmap(fbp, local_inp);

    while (1) {
	int from;
	ColorMap *tp;

	/* Build color map for current value */
	for (i=0, from = increment; i < 256; i++, from++) {
	    if (from < 0)
		from += 256;
	    else if (from > 255)
		from -= 256;
	    local_outp->cm_red[i]   = local_inp->cm_red[from];
	    local_outp->cm_green[i] = local_inp->cm_green[from];
	    local_outp->cm_blue[i]  = local_inp->cm_blue[from];
	}

	fb_wmap(fbp, local_outp);
	tp = local_outp;
	local_outp = local_inp;
	local_inp = tp;

	if (fps > 0.0) {
	    fd_set readfds;

	    FD_ZERO(&readfds);
	    FD_SET(fileno(stdin), &readfds);
	    select(fileno(stdin)+1, &readfds, (fd_set *)0, (fd_set *)0, &tv);
	}
	if (onestep)
	    break;
    }
    fb_close(fbp);
    return	0;
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

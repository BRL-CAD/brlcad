/*                        F B F A D E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file fbfade.c
 *
 * fade in or out a frame buffer image
 *
 * This program displays a frame buffer image gradually, randomly
 * selecting the pixel display sequence.  (Suggested by Gary Moss.)
 * It requires fast single-pixel write support for best effect.
 *
 * Options:
 *
 * "-h" assumes 1024x1024 default input size instead of 512x512
 *
 * "-f in_fb_file" reads from the specified frame buffer file instead
 * of assuming constant black ("fade out") value
 *
 * "-s size" is the input size (width & height)
 *
 * "-w width" is the input width
 *
 * "-n height" is the input height
 *
 * "-F out_fb_file" writes to the specified frame buffer file instead of
 * the one specified by the FB_FILE environment variable (the default
 * frame buffer, if no FB_FILE)
 *
 * "-S size" is the output size (width & height)
 *
 * "-W width" is the output width
 *
 * "-N height" is the output height
 *
 * "out_fb_file" is the same as -F out_fb_file, for convenience
 */

#include "common.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "bio.h"

#include "bu.h"
#include "fb.h"			/* BRL-CAD package libfb.a interface */
#include "pkg.h"


#define USAGE1 "fbfade [ -s size ] [ -w width ] [ -n height ] [ -f in_fb_file ]"
#define USAGE2	\
	"\t[ -h ] [ -S size ] [ -W width ] [ -N height ] [ [ -F ] out_fb_file ]"
#define OPTSTR "f:F:hn:N:s:S:w:W:"


typedef int bool_t;

static bool_t hires = 0;		/* set for 1Kx1K; clear for 512x512 */
static char *in_fb_file = NULL;		/* input image name */
static char *out_fb_file = NULL;	/* output frame buffer name */
static FBIO *fbp = FBIO_NULL;		/* libfb input/output handle */
static int src_width = 0;		/* input image width */
static int src_height = 0;		/* input image height */
static int dst_width = 0;		/* output frame buffer size */
static int dst_height = 0;		/* output frame buffer size */
static RGBpixel *pix;			/* input image */
static RGBpixel bg = { 0, 0, 0 };	/* background */

/* in ioutil.c */
extern void Message(const char *format, ...);
extern void Fatal(FBIO *fbiop, const char *format, ...);


static void
Sig_Catcher(int sig)
{
    (void)signal(sig, SIG_DFL);

    /* The following is not guaranteed to work, but it's worth a try. */
    Fatal(fbp, "Interrupted by signal %d", sig);
}


int
main(int argc, char **argv)
{
    /* Plant signal catcher. */
    {
	/* signals to catch */
	static int getsigs[] = {
#ifdef SIGHUP
	    SIGHUP,			/* hangup */
#endif
#ifdef SIGINT
	    SIGINT,			/* interrupt */
#endif
#ifdef SIGQUIT
	    SIGQUIT,		/* quit */
#endif
#ifdef SIGPIPE
	    SIGPIPE,		/* write on a broken pipe */
#endif
#ifdef SIGTERM
	    SIGTERM,		/* software termination signal */
#endif
	    0
	};
	int i;

	for (i = 0; getsigs[i] != 0; ++i)
	    if (signal(getsigs[i], SIG_IGN) != SIG_IGN)
		(void)signal(getsigs[i], Sig_Catcher);
    }

    /* Process arguments. */
    {
	int c;
	bool_t errors = 0;

	while ((c = bu_getopt(argc, argv, OPTSTR)) != -1)
	    switch (c) {
		default:	/* '?': invalid option */
		    errors = 1;
		    break;

		case 'f':	/* -f in_fb_file */
		    in_fb_file = bu_optarg;
		    break;

		case 'F':	/* -F out_fb_file */
		    out_fb_file = bu_optarg;
		    break;

		case 'h':	/* -h */
		    hires = 1;
		    break;

		case 'n':	/* -n height */
		    if ((src_height = atoi(bu_optarg)) <= 0)
			errors = 1;

		    break;

		case 'N':	/* -N height */
		    if ((dst_height = atoi(bu_optarg)) <= 0)
			errors = 1;

		    break;

		case 's':	/* -s size */
		    if ((src_height = src_width = atoi(bu_optarg)) <= 0)
			errors = 1;

		    break;

		case 'S':	/* -S size */
		    if ((dst_height = dst_width = atoi(bu_optarg)) <= 0)
			errors = 1;

		    break;

		case 'w':	/* -w width */
		    if ((src_width = atoi(bu_optarg)) <= 0)
			errors = 1;

		    break;

		case 'W':	/* -W width */
		    if ((dst_width = atoi(bu_optarg)) <= 0)
			errors = 1;

		    break;
	    }

	if (errors)
	    Fatal(fbp, "Usage: %s\n%s", USAGE1, USAGE2);
    }

    if (bu_optind < argc) {
	/* out_fb_file */
	if (bu_optind < argc - 1 || out_fb_file != NULL) {
	    Message("Usage: %s\n%s", USAGE1, USAGE2);
	    Fatal(fbp, "Can't handle multiple output frame buffers!");
	}

	out_fb_file = argv[bu_optind];
    }

    /* Open frame buffer for unbuffered input. */

    if (src_width == 0)
	src_width = hires ? 1024 : 512;		/* starting default */

    if (src_height == 0)
	src_height = hires ? 1024 : 512;	/* starting default */

    if (in_fb_file != NULL) {

	if ((fbp = fb_open(in_fb_file, src_width, src_height)) == FBIO_NULL)
	    Fatal(fbp, "Couldn't open input frame buffer");
	else {
	    int y;
	    int wt = fb_getwidth(fbp);
	    int ht = fb_getheight(fbp);

	    /* Use smaller actual input size instead of request. */

	    if (wt < src_width)
		src_width = wt;

	    if (ht < src_height)
		src_height = ht;

	    if ((pix = (RGBpixel *)malloc((size_t)src_width * (size_t)src_height * sizeof(RGBpixel))) == NULL)
		Fatal(fbp, "Not enough memory for pixel array");

	    for (y = 0; y < src_height; ++y) {
		if (fb_read(fbp, 0, y, pix[y * src_width], src_width) == -1)
		    Fatal(fbp, "Error reading raster");
	    }

	    if (fb_close(fbp) == -1) {
		fbp = FBIO_NULL;	/* avoid second try */
		Fatal(fbp, "Error closing input frame buffer");
	    }
	}
    }

    /* Open frame buffer for unbuffered output. */

    if (dst_width == 0)
	dst_width = src_width;		/* default */

    if (dst_height == 0)
	dst_height = src_height;	/* default */

    if ((fbp = fb_open(out_fb_file, dst_width, dst_height)) == FBIO_NULL)
	Fatal(fbp, "Couldn't open output frame buffer");
    else {
	int wt = fb_getwidth(fbp);
	int ht = fb_getheight(fbp);

	/* Use smaller actual frame buffer size for output. */

	if (wt < dst_width)
	    dst_width = wt;

	if (ht < dst_height)
	    dst_height = ht;

	/* Avoid selecting pixels outside the input image. */

	if (dst_width > src_width)
	    dst_width = src_width;

	if (dst_height > src_height)
	    dst_height = src_height;
    }

    /* The following is probably an optimally fast shuffling
     * algorithm; unfortunately, it requires a huge auxiliary array.
     * The way it works is to start with an array of all pixel
     * indices, then repeat: select an entry at random from the array,
     * output that index, replace that entry with the last array
     * entry, then reduce the array size.
     */
    {
	long *loc;		/* keeps track of pixel shuffling */
	long wxh = (long)dst_width * (long)dst_height;
	/* down-counter */

	if ((loc = (long *)malloc((size_t)wxh * sizeof(long))) == NULL)
	    Fatal(fbp, "Not enough memory for location array");

	/* Initialize pixel location array to sequential order. */

	while (--wxh >= 0L)
	    loc[wxh] = wxh;

	/* Select a pixel at random, paint it, and adjust the location
	 * array.
	 */

	for (wxh = (long)dst_width * (long)dst_height; --wxh >= 0L;) {
	    long r = (long)((double)wxh * drand48());
	    long x = loc[r] % dst_width;
	    long y = loc[r] / dst_width;

	    if (fb_write(fbp, (int)x, (int)y, in_fb_file == NULL ? bg : pix[x + y * src_width], 1) == -1) {
		Fatal(fbp, "Error writing pixel");
	    }

	    loc[r] = loc[wxh];	/* track the shuffle */
	}
    }

    /* Close the frame buffer. */

    if (fb_close(fbp) == -1) {
	fbp = FBIO_NULL;	/* avoid second try */
	Fatal(fbp, "Error closing output frame buffer");
    }

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

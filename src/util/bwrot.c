/*                         B W R O T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2010 United States Government as represented by
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
 */
/** @file bwrot.c
 *
 * Rotate, Invert, and/or Reverse the pixels in a Black
 * and White (.bw) file.
 *
 * The rotation logic was worked out for data ordered with
 * "upper left" first.  It is being used on files in first
 * quadrant order (lower left first).  Thus the "forward",
 * "backward" flags are reversed.
 *
 * The code was designed to never need to seek on the input,
 * while it *may* need to seek on output (if the max buffer
 * is too small).  It would be nice if we could handle the
 * reverse case also (e.g. pipe on stdout).
 *
 * Note that this program can be applied to any collection
 * of single byte entities.
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu.h"
#include "bn.h"


size_t buflines, scanbytes;
int firsty = -1;	/* first "y" scanline in buffer */
int lasty = -1;	/* last "y" scanline in buffer */
unsigned char *bp;
unsigned char *obp;

size_t nxin = 512;
size_t nyin = 512;
size_t yin, xout, yout;
int plus90, minus90, reverse, invert;


int
get_args(int argc, char **argv, FILE **ifp, FILE **ofp, double *angle)
{
    int c;
    char *in_file_name = NULL;
    char *out_file_name = NULL;

    if (!ifp || !ofp || !angle)
	bu_exit(1, "bwrot: internal error processing arguments\n");

    while ((c = bu_getopt(argc, argv, "fbrihs:w:n:S:W:N:a:o:")) != EOF) {
	switch (c) {
	    case 'f':
		minus90++;
		break;
	    case 'b':
		plus90++;
		break;
	    case 'r':
		reverse++;
		break;
	    case 'i':
		invert++;
		break;
	    case 'h':
		/* high-res */
		nxin = nyin = 1024;
		break;
	    case 'S':
	    case 's':
		/* square size */
		nxin = nyin = atoi(bu_optarg);
		break;
	    case 'W':
	    case 'w':
		nxin = atoi(bu_optarg);
		break;
	    case 'N':
	    case 'n':
		nyin = atoi(bu_optarg);
		break;
	    case 'a':
		*angle = atof(bu_optarg);
		break;
	    case 'o':
		out_file_name = bu_optarg;
		*ofp = fopen(out_file_name, "wb+");
		if (*ofp == NULL) {
		    bu_log("ERROR: %s cannot open \"%s\" for writing\n", bu_getprogname(), out_file_name);
		    return 0;
		}
		break;

	    default:		/* '?' */
		bu_log("ERROR: %s encountered unrecognized '-%c' option\n", bu_getprogname(), c);
		return 0;
	}
    }

    /* XXX - backward compatability hack */
    if (bu_optind+2 == argc) {
	nxin = atoi(argv[bu_optind++]);
	nyin = atoi(argv[bu_optind++]);
    }

    if (bu_optind >= argc) {
	in_file_name = "-";
    } else {
	in_file_name = argv[bu_optind];
    }

    if (strcmp(in_file_name, "-") == 0) {
	*ifp = stdout;
    } else {
	*ifp = fopen(in_file_name, "rb");
	if (*ifp == NULL) {
	    bu_log("ERROR: %s cannot open \"%s\" for reading\n", bu_getprogname(), in_file_name);
	    return 0;
	}
    }

    /* sanity */
    if (isatty(fileno(*ifp))) {
	bu_log("ERROR: %s will not read bw data from a tty\nSpecify an input file.\n", bu_getprogname());
	return 0;
    }
    if (isatty(fileno(*ofp))) {
	bu_log("ERROR: %s will not write bw data to a tty\nRedirect output or use the -o output option.\n", bu_getprogname());
	return 0;
    }

    if (argc > ++bu_optind) {
	bu_log("bwrot: excess argument(s) ignored\n");
    }

    return 1;		/* OK */
}


static void
fill_buffer(FILE *ifp, unsigned char *buf)
{
    buflines = fread(buf, scanbytes, buflines, ifp);

    firsty = lasty + 1;
    lasty = firsty + (buflines - 1);
}


static void
reverse_buffer(unsigned char *buf)
{
    int i;
    unsigned char *p1, *p2, temp;

    for (i = 0; i < buflines; i++) {
	p1 = &buf[ i * scanbytes ];
	p2 = p1 + (scanbytes - 1);
	while (p1 < p2) {
	    temp = *p1;
	    *p1++ = *p2;
	    *p2-- = temp;
	}
    }
}


/*
 * Arbitrary angle rotation.
 *
 * 'a' is rotation angle
 *
 * Currently this needs to be able to buffer the entire image
 * in memory at one time.
 *
 * To rotate a point (x, y) CCW about the origin:
 * x' = x cos(a) - y sin(a)
 * y' = x sin(a) + y cos(a)
 * To rotate it about a point (xc, yc):
 * x' = (x-xc) cos(a) - (y-yc) sin(a) + xc
 *       = x cos(a) - y sin(a) + [xc - xc cos(a) + yc sin(a)]
 * y' = (x-xc) sin(a) + (y-yc) cos(a) + yc
 *	 = x sin(a) + y cos(a) + [yc - yc cos(a) - xc sin(a)]
 * So, to take one step in x:
 * dx' = cos(a)
 * dy' = sin(a)
 * or one step in y:
 * dx' = -sin(a)
 * dy' = cos(a)
 */
static void
arbrot(double a, FILE *ifp, unsigned char *buf)
{
#define DtoR(x)	((x)*M_PI/180.0)
    int x, y;				/* working coord */
    double x2, y2;				/* its rotated position */
    double xc, yc;				/* rotation origin */
    int x_min, y_min, x_max, y_max;	/* area to rotate */
    double x_goop, y_goop;
    double sina, cosa;

    if (buflines != nyin) {
	/* I won't all fit in the buffer */
	fprintf(stderr, "bwrot: Sorry but I can't do an arbitrary rotate of an image this large\n");
	bu_exit (1, NULL);
    }
    if (buflines > nyin) buflines = nyin;
    fill_buffer(ifp, buf);

    /*
     * Convert rotation angle to radians.
     * Because we "pull down" the pixel from their rotated positions
     * to their standard ones, the sign of the rotation is reversed.
     */
    a = -DtoR(a);
    sina = sin(a);
    cosa = cos(a);

    /* XXX - Let the user pick the rotation origin? */
    xc = nxin / 2.0;
    yc = nyin / 2.0;

    x_goop = xc - xc * cosa + yc * sina;
    y_goop = yc - yc * cosa - xc * sina;

    x_min = 0;
    y_min = 0;
    x_max = nxin;
    y_max = nyin;

    for (y = y_min; y < y_max; y++) {
	x2 = x_min * cosa - y * sina + x_goop;
	y2 = x_min * sina + y * cosa + y_goop;
	for (x = x_min; x < x_max; x++) {
	    /* check for in bounds */
	    if (x2 >= 0 && x2 < nxin && y2 >= 0 && y2 < nyin)
		putchar(buf[(int)y2*nyin + (int)x2]);
	    else
		putchar(0);	/* XXX - setable color? */
	    /* "forward difference" our coordinates */
	    x2 += cosa;
	    y2 += sina;
	}
    }
}


int
main(int argc, char **argv)
{
    const size_t MAXBUFBYTES = 1280*1024;

    char usage[] = "\
Usage: bwrot [-f -b -r -i [-s squaresize] [-w width] [-n height] [-o output.bw] input.bw [> output.bw]\n\
   or  bwrot -a angle [-s squaresize] [-w width] [-n height] [-o output.bw] input.bw [> output.bw]\n";

    int x, y;
    int ret = 0;
    long outbyte, outplace;
    FILE *ifp, *ofp;
    unsigned char *obuf;
    unsigned char *buffer;
    double angle = 0.0;

    ifp = stdin;
    ofp = stdout;
    bu_setprogname(argv[0]);

    if (!get_args(argc, argv, &ifp, &ofp, &angle)) {
	bu_exit(1, "%s", usage);
    }

    scanbytes = nxin;
    buflines = MAXBUFBYTES / scanbytes;
    if (buflines <= 0) {
	bu_exit(1, "%s", "bwrot: I'm not compiled to do a scanline that long!\n");
    }
    if (buflines > nyin) buflines = nyin;
    buffer = (unsigned char *)bu_malloc(buflines * scanbytes, "buffer");
    obuf = (unsigned char *)bu_malloc((nyin > nxin) ? nyin : nxin, "obuf");

    /*
     * Break out to added arbitrary angle routine
     */
    if (angle > 0.0) {
	arbrot(angle, ifp, buffer);
	goto done;
    }

    /*
     * Clear our "file pointer."  We need to maintain this
     * In order to tell if seeking is required.  ftell() always
     * fails on pipes, so we can't use it.
     */
    outplace = 0;

    yin = 0;
    while (yin < nyin) {
	/* Fill buffer */
	fill_buffer(ifp, buffer);
	if (!buflines)
	    break;
	if (reverse)
	    reverse_buffer(buffer);
	if (plus90) {
	    for (x = 0; x < nxin; x++) {
		obp = obuf;
		bp = &buffer[ (lasty-firsty)*scanbytes + x ];
		for (y = lasty; y >= yin; y--) {
		    /* firsty? */
		    *obp++ = *bp;
		    bp -= scanbytes;
		}
		yout = x;
		xout = (nyin - 1) - lasty;
		outbyte = ((yout * nyin) + xout);
		if (outplace != outbyte) {
		    if (fseek(ofp, outbyte, 0) < 0) {
			ret = 3;
			bu_log("%s: Can't seek on output, yet I need to!\n", bu_getprogname());
			goto done;
		    }
		    outplace = outbyte;
		}
		fwrite(obuf, 1, buflines, ofp);
		outplace += buflines;
	    }
	} else if (minus90) {
	    for (x = nxin-1; x >= 0; x--) {
		obp = obuf;
		bp = &buffer[ x ];
		for (y = firsty; y <= lasty; y++) {
		    *obp++ = *bp;
		    bp += scanbytes;
		}
		yout = (nxin - 1) - x;
		xout = yin;
		outbyte = ((yout * nyin) + xout);
		if (outplace != outbyte) {
		    if (fseek(ofp, outbyte, 0) < 0) {
			ret = 3;
			bu_log("%s: Can't seek on output, yet I need to!\n", bu_getprogname());
			goto done;
		    }
		    outplace = outbyte;
		}
		fwrite(obuf, 1, buflines, ofp);
		outplace += buflines;
	    }
	} else if (invert) {
	    for (y = lasty; y >= firsty; y--) {
		yout = (nyin - 1) - y;
		outbyte = yout * scanbytes;
		if (outplace != outbyte) {
		    if (fseek(ofp, outbyte, 0) < 0) {
			ret = 3;
			bu_log("%s: Can't seek on output, yet I need to!\n", bu_getprogname());
			goto done;
		    }
		    outplace = outbyte;
		}
		fwrite(&buffer[(y-firsty)*scanbytes], 1, scanbytes, ofp);
		outplace += scanbytes;
	    }
	} else {
	    /* Reverse only */
	    for (y = 0; y < buflines; y++) {
		fwrite(&buffer[y*scanbytes], 1, scanbytes, ofp);
	    }
	}

	yin += buflines;
    }

done:
    bu_free(buffer, "buffer");
    bu_free(obuf, "obuf");

    return ret;
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

/*                           R O T . C
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
 */
/** @file libicv/rot.c
 *
 * Rotate, Invert, and/or Reverse the pixels in an image file.
 *
 * The rotation logic was worked out for data ordered with "upper
 * left" first.  It is being used on files in first quadrant order
 * (lower left first).  Thus the "forward", "backward" flags are
 * reversed.
 *
 * The code was designed to never need to seek on the input, while it
 * *may* need to seek on output (if the max buffer is too small).  It
 * would be nice if we could handle the reverse case also (e.g. pipe
 * on stdout).
 *
 * Note that this program can be applied to any collection of
 * interleaved channel byte entities.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "bn.h"


size_t buflines, scanbytes;
ssize_t firsty = -1;	/* first "y" scanline in buffer */
ssize_t lasty = -1;	/* last "y" scanline in buffer */
unsigned char *bp;
unsigned char *obp;

size_t nxin = 512;
size_t nyin = 512;
size_t yin, xout, yout;
int plus90, minus90, reverse, invert;
size_t pixbytes = 1;


int
get_args(int argc, char **argv, FILE **ifp, FILE **ofp, double *angle)
{
    int c;
    char *in_file_name = NULL;
    char *out_file_name = NULL;

    if (!ifp || !ofp || !angle)
	bu_exit(1, "internal error processing arguments\n");

    while ((c = bu_getopt(argc, argv, "fbrih#:a:s:o:w:n:S:W:N:")) != -1) {
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
	    case '#':
		pixbytes = atoi(bu_optarg);
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

    if (BU_STR_EQUAL(in_file_name, "-")) {
	*ifp = stdin;
    } else {
	*ifp = fopen(in_file_name, "rb");
	if (*ifp == NULL) {
	    bu_log("ERROR: %s cannot open \"%s\" for reading\n", bu_getprogname(), in_file_name);
	    return 0;
	}
    }

    /* sanity */
    if (isatty(fileno(*ifp))) {
	bu_log("ERROR: %s will not read data from a tty\nRedirect input or specify an input file.\n", bu_getprogname());
	return 0;
    }
    if (isatty(fileno(*ofp))) {
	bu_log("ERROR: %s will not write data to a tty\nRedirect output or use the -o output option.\n", bu_getprogname());
	return 0;
    }

    if (argc > ++bu_optind) {
	bu_log("WARNING: excess argument(s) ignored\n");
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
    size_t i, j;
    unsigned char *p1, *p2, temp;

    for (i = 0; i < buflines; i++) {
	p1 = &buf[ i * scanbytes ];
	p2 = p1 + (scanbytes - pixbytes);
	while (p1 < p2) {
	    for (j = 0; j < pixbytes; j++) {
		temp = *p1;
		*p1++ = *p2;
		*p2++ = temp;
	    }
	    p2 -= 2*pixbytes;
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
#define DtoR(x)	((x)*DEG2RAD)
    size_t x, y;				/* working coord */
    double x2, y2;				/* its rotated position */
    double xc, yc;				/* rotation origin */
    size_t x_min, y_min, x_max, y_max;		/* area to rotate */
    double x_goop, y_goop;
    double sina, cosa;

    if (buflines != nyin) {
	/* I won't all fit in the buffer */
	fprintf(stderr, "Sorry but I can't do an arbitrary rotate of an image this large\n");
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
	    if (x2 > 0.0
		&& ZERO(x2)
		&& x2 < (double)nxin
		&& y2 > 0.0
		&& ZERO(y2)
		&& y2 < (double)nyin)
	    {
		putchar(buf[(int)y2*nyin + (int)x2]);
	    } else {
		putchar(0);	/* XXX - setable color? */
	    }
	    /* "forward difference" our coordinates */
	    x2 += cosa;
	    y2 += sina;
	}
    }
}


int
icv_rot(int argc, char **argv)
{
    const size_t MAXPIXELS = 16768 * 16768; /* boo hiss */

    size_t x, y, j;
    int ret = 0;
    long outbyte, outplace;
    FILE *ifp, *ofp;
    unsigned char *obuf;
    unsigned char *buffer;
    double angle = 0.0;
    size_t wrote = 0;

    ifp = stdin;
    ofp = stdout;
    bu_setprogname(argv[0]);

    if (!get_args(argc, argv, &ifp, &ofp, &angle)) {
	bu_exit(1, "Usage: %s [-rifb | -a angle] [-# bytes] [-s squaresize] [-w width] [-n height] [-o outputfile] inputfile [> outputfile]\n", bu_getprogname());
    }

    scanbytes = nxin * pixbytes;
    buflines = MAXPIXELS / nxin;
    if (buflines <= 0) {
	bu_exit(1, "ERROR: %s is not compiled to handle a scanline that long!\n");
    }
    if (buflines > nyin) buflines = nyin;
    buffer = (unsigned char *)bu_malloc(buflines * scanbytes, "buffer");
    obuf = (unsigned char *)bu_malloc((nyin > nxin) ? nyin*pixbytes : nxin*pixbytes, "obuf");

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
		bp = &buffer[ (lasty-firsty)*scanbytes + x*pixbytes ];
		for (y = lasty+1; y > yin; y--) {
		    /* firsty? */
		    for (j = 0; j < pixbytes; j++)
			*obp++ = *bp++;
		    bp -= scanbytes + pixbytes;
		}
		yout = x;
		xout = (nyin - 1) - lasty;
		outbyte = ((yout * nyin) + xout) * pixbytes;
		if (outplace != outbyte) {
		    if (fseek(ofp, outbyte, SEEK_SET) < 0) {
			ret = 3;
			perror("fseek");
			bu_log("ERROR: %s can't seek on output (ofp=%p, outbute=%ld)\n", bu_getprogname(), ofp, outbyte);
			goto done;
		    }
		    outplace = outbyte;
		}
		wrote = fwrite(obuf, pixbytes, buflines, ofp);
		if (wrote != buflines) {
		    ret = 4;
		    perror("fwrite");
		    bu_log("ERROR: %s can't out write image data (wrote %zu of %d)\n", wrote, buflines);
		    goto done;
		}
		outplace += buflines*pixbytes;
	    }
	} else if (minus90) {
	    for (x = nxin; x > 0; x--) {
		obp = obuf;
		bp = &buffer[ (x-1)*pixbytes ];
		for (y = firsty+1; (ssize_t)y < lasty; y++) {
		    for (j = 0; j < pixbytes; j++)
			*obp++ = *bp++;
		    bp += scanbytes - pixbytes;
		}
		yout = (nxin - 1) - x + 1;
		xout = yin;
		outbyte = ((yout * nyin) + xout) * pixbytes;
		if (outplace != outbyte) {
		    if (fseek(ofp, outbyte, SEEK_SET) < 0) {
			ret = 3;
			perror("fseek");
			bu_log("ERROR: %s can't seek on output (ofp=%p, outbute=%ld)\n", bu_getprogname(), ofp, outbyte);
			goto done;
		    }
		    outplace = outbyte;
		}
		wrote = fwrite(obuf, pixbytes, buflines, ofp);
		if (wrote != buflines) {
		    ret = 4;
		    perror("fwrite");
		    bu_log("ERROR: %s can't out write image data (wrote %zu of %d)\n", wrote, buflines);
		    goto done;
		}
		outplace += buflines*pixbytes;
	    }
	} else if (invert) {
	    for (y = lasty+1; (ssize_t)y > firsty; y--) {
		yout = (nyin - 1) - y + 1;
		outbyte = yout * scanbytes;
		if (outplace != outbyte) {
		    if (fseek(ofp, outbyte, SEEK_SET) < 0) {
			ret = 3;
			perror("fseek");
			bu_log("ERROR: %s can't seek on output (ofp=%p, outbute=%ld)\n", bu_getprogname(), ofp, outbyte);
			goto done;
		    }
		    outplace = outbyte;
		}
		wrote = fwrite(&buffer[(y-firsty-1)*scanbytes], 1, scanbytes, ofp);
		if (wrote != scanbytes) {
		    ret = 4;
		    perror("fwrite");
		    bu_log("ERROR: %s can't out write image data (wrote %zu of %d)\n", wrote, scanbytes);
		    goto done;
		}
		outplace += scanbytes;
	    }
	} else {
	    /* Reverse only */
	    for (y = 0; y < buflines; y++) {
		wrote = fwrite(&buffer[y*scanbytes], 1, scanbytes, ofp);
		if (wrote != scanbytes) {
		    ret = 4;
		    perror("fwrite");
		    bu_log("ERROR: %s can't out write image data (wrote %zu of %d)\n", wrote, scanbytes);
		    goto done;
		}
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

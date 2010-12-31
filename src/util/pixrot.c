/*                        P I X R O T . C
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
/** @file pixrot.c
 *
 * Rotate, Invert, and/or Reverse the pixels in a RGB (.pix) file.
 *
 * The rotation logic was worked out for data ordered with "upper
 * left" first.  It is being used on files in first quadrant order
 * (lower left first).  Thus the "forward", "backward" flags are
 * reversed.
 *
 * This is a generalization of bwrot and can in fact handle "pixels"
 * of any size.  Thus this routine could be used to say, rotate a
 * matix of floating point values, etc.
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu.h"


static const char usage[] = "\
Usage: pixrot [-f -b -r -i -#bytes] [-s squaresize]\n\
	[-w width] [-n height] [file.pix] > file.pix\n";


/* 4 times bigger than typ. screen */
/*#define MAXBUFBYTES (1280*1024*3*4) */
#define MAXBUFBYTES (12288*16384*2)
int buflines, scanbytes;
int firsty = -1;	/* first "y" scanline in buffer */
int lasty = -1;	/* last "y" scanline in buffer */
unsigned char *buffer;
unsigned char *bp;
unsigned char *obuf;
unsigned char *obp;

int nxin = 512;
int nyin = 512;
int yin, xout, yout;
int plus90, minus90, reverse, invert;
int pixbytes = 3;

void fill_buffer(void);
void reverse_buffer(void);

static char *file_name;
FILE *ifp, *ofp;

int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "fbrih#:s:w:n:S:W:N:")) != EOF) {
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
	    case '#':
		pixbytes = atoi(bu_optarg);
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

	    default:		/* '?' */
		return 0;
	}
    }

    /* XXX - backward compatability hack */
    if (bu_optind+2 == argc) {
	nxin = atoi(argv[bu_optind++]);
	nyin = atoi(argv[bu_optind++]);
    }
    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return 0;
	file_name = "-";
	ifp = stdin;
    } else {
	file_name = argv[bu_optind];
	if ((ifp = fopen(file_name, "r")) == NULL) {
	    bu_log("pixrot: cannot open \"%s\" for reading\n", file_name);
	    return 0;
	}
    }

    if (argc > ++bu_optind)
	bu_log("pixrot: excess argument(s) ignored\n");

    return 1;		/* OK */
}


int
main(int argc, char **argv)
{
    int x, y, j;
    long outbyte, outplace;
    size_t ret;

    if (!get_args(argc, argv) || isatty(fileno(stdout))) {
	bu_exit(1, "%s", usage);
    }

    ofp = stdout;

    scanbytes = nxin * pixbytes;
    buflines = MAXBUFBYTES / scanbytes;
    if (buflines <= 0) {
	bu_exit(2, "pixrot: I'm not compiled to do a scanline that long!\n");
    }
    if (buflines > nyin) buflines = nyin;
    buffer = malloc(buflines * scanbytes);
    obuf = (nyin > nxin) ? malloc(nyin * pixbytes) : malloc(nxin * pixbytes);
    if (buffer == (unsigned char *)0 || obuf == (unsigned char *)0) {
	bu_exit(3, "pixrot: malloc failed\n");
    }

    /*
     * Clear our "file pointer." We need to maintain this
     * In order to tell if seeking is required.  ftell() always
     * fails on pipes, so we can't use it.
     */
    outplace = 0;

    yin = 0;
    while (yin < nyin) {
	/* Fill buffer */
	fill_buffer();
	if (reverse)
	    reverse_buffer();
	if (plus90) {
	    for (x = 0; x < nxin; x++) {
		obp = obuf;
		bp = &buffer[ (lasty-firsty)*scanbytes + x*pixbytes ];
		for (y = lasty; y >= yin; y--) {
		    /* firsty? */
		    for (j = 0; j < pixbytes; j++)
			*obp++ = *bp++;
		    bp = bp - scanbytes - pixbytes;
		}
		yout = x;
		xout = (nyin - 1) - lasty;
		outbyte = ((yout * nyin) + xout) * pixbytes;
		if (outplace != outbyte) {
		    if (fseek(ofp, outbyte, 0) < 0) {
			bu_exit(3, "pixrot: Can't seek on output, yet I need to!\n");
		    }
		    outplace = outbyte;
		}
		ret = fwrite(obuf, pixbytes, buflines, ofp);
		if (ret == 0) {
		    perror("fwrite");
		    break;
		}
		outplace += (pixbytes * buflines);
	    }
	} else if (minus90) {
	    for (x = nxin-1; x >= 0; x--) {
		obp = obuf;
		bp = &buffer[ x*pixbytes ];
		for (y = firsty; y <= lasty; y++) {
		    for (j = 0; j < pixbytes; j++)
			*obp++ = *bp++;
		    bp = bp + scanbytes - pixbytes;
		}
		yout = (nxin - 1) - x;
		xout = yin;
		outbyte = ((yout * nyin) + xout) * pixbytes;
		if (outplace != outbyte) {
		    if (fseek(ofp, outbyte, 0) < 0) {
			bu_exit(3, "pixrot: Can't seek on output, yet I need to!\n");
		    }
		    outplace = outbyte;
		}
		ret = fwrite(obuf, pixbytes, buflines, ofp);
		if (ret == 0) {
		    perror("fwrite");
		    break;
		}
		outplace += (pixbytes * buflines);
	    }
	} else if (invert) {
	    for (y = lasty; y >= firsty; y--) {
		yout = (nyin - 1) - y;
		outbyte = yout * scanbytes;
		if (outplace != outbyte) {
		    if (fseek(ofp, outbyte, 0) < 0) {
			bu_exit(3, "pixrot: Can't seek on output, yet I need to!\n");
		    }
		    outplace = outbyte;
		}
		ret = fwrite(&buffer[(y-firsty)*scanbytes], 1, scanbytes, ofp);
		if (ret == 0) {
		    perror("fwrite");
		    break;
		}
		outplace += scanbytes;
	    }
	} else {
	    /* Reverse only */
	    for (y = 0; y < buflines; y++) {
		ret = fwrite(&buffer[y*scanbytes], 1, scanbytes, ofp);
		if (ret == 0) {
		    perror("fwrite");
		    break;
		}
	    }
	}

	yin += buflines;
    }
    return 0;
}


void
fill_buffer(void)
{
    buflines = fread(buffer, scanbytes, buflines, ifp);

    firsty = lasty + 1;
    lasty = firsty + (buflines - 1);
}


void
reverse_buffer(void)
{
    int i, j;
    unsigned char *p1, *p2, temp;

    for (i = 0; i < buflines; i++) {
	p1 = &buffer[ i * scanbytes ];
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
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

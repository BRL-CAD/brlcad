/*                       P I X - S U N . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
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
/** @file pix-sun.c
 *	Program to take a BRL-CAD PIX format image file and convert the
 *	image to a Sun Microsystems 8-bit deep color "rasterfile" format
 *	image.
 *
 *  Author -
 *	Lee A. Butler
 *
 *  Source -
 *      SECAD/VLD Computing Consortium, Bldg 394
 *      The U. S. Army Ballistic Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5066
 */
#include "common.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"


/* declarations to support use of bu_getopt() system call */
char *options = "hs:w:n:d";
char optflags[sizeof(options)];
char *progname = "(noname)";
int dither = 0;

#define MAPSIZE 256   /* Number of unique color values in Sun Colormap */
/* Description of header for files containing raster images */
struct rasterfile {
    long ras_magic;	/* magic number */
    int	 ras_width;	/* width (pixels) of image */
    int	 ras_height;	/* height (pixels) of image */
    int	 ras_depth;	/* depth (1, 8, or 24 bits) of pixel */
    int	 ras_length;	/* length (bytes) of image */
    int	 ras_type;	/* type of file; see RT_* below */
    int	 ras_maptype;	/* type of colormap; see RMT_* below */
    int	 ras_maplength;	/* length (bytes) of following map */
    /* color map follows for ras_maplength bytes, followed by image */
} ras = {
    0x59a66a95,	/* Magic Number */
    512,	/* default width */
    512,	/* default height */
    8,		/* bits per pixel */
    0,		/* length of image */
    1,		/* standard rasterfile format */
    1,		/* equal RGB color map */
    MAPSIZE*3	/* length (bytes) of RGB colormap */
};

/* The Sun Rasterfile Colormap
 * This colormap has a 6x6x6 color cube, plus 10 extra values for each of
 * the primary colors and the grey levels
 */
unsigned char redmap[MAPSIZE] =
{ 0, 51,102,153,204,255,  0, 51,102,153,204,255,  0, 51,102,153,
204,255,  0, 51,102,153,204,255,  0, 51,102,153,204,255,  0, 51,
102,153,204,255,  0, 51,102,153,204,255,  0, 51,102,153,204,255,
  0, 51,102,153,204,255,  0, 51,102,153,204,255,  0, 51,102,153,
204,255,  0, 51,102,153,204,255,  0, 51,102,153,204,255,  0, 51,
102,153,204,255,  0, 51,102,153,204,255,  0, 51,102,153,204,255,
  0, 51,102,153,204,255,  0, 51,102,153,204,255,  0, 51,102,153,
204,255,  0, 51,102,153,204,255,  0, 51,102,153,204,255,  0, 51,
102,153,204,255,  0, 51,102,153,204,255,  0, 51,102,153,204,255,
  0, 51,102,153,204,255,  0, 51,102,153,204,255,  0, 51,102,153,
204,255,  0, 51,102,153,204,255,  0, 51,102,153,204,255,  0, 51,
102,153,204,255,  0, 51,102,153,204,255,  0, 51,102,153,204,255,
  0, 51,102,153,204,255,  0, 51,102,153,204,255,  0, 51,102,153,
204,255,  0, 51,102,153,204,255, 17, 34, 68, 85,119,136,170,187,
221,238,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0, 17, 34, 68, 85,119,136,170,187,221,238 };

unsigned char grnmap[MAPSIZE] =
{ 0,  0,  0,  0,  0,  0, 51, 51, 51, 51, 51, 51,102,102,102,102,
102,102,153,153,153,153,153,153,204,204,204,204,204,204,255,255,
255,255,255,255,  0,  0,  0,  0,  0,  0, 51, 51, 51, 51, 51, 51,
102,102,102,102,102,102,153,153,153,153,153,153,204,204,204,204,
204,204,255,255,255,255,255,255,  0,  0,  0,  0,  0,  0, 51, 51,
 51, 51, 51, 51,102,102,102,102,102,102,153,153,153,153,153,153,
204,204,204,204,204,204,255,255,255,255,255,255,  0,  0,  0,  0,
  0,  0, 51, 51, 51, 51, 51, 51,102,102,102,102,102,102,153,153,
153,153,153,153,204,204,204,204,204,204,255,255,255,255,255,255,
  0,  0,  0,  0,  0,  0, 51, 51, 51, 51, 51, 51,102,102,102,102,
102,102,153,153,153,153,153,153,204,204,204,204,204,204,255,255,
255,255,255,255,  0,  0,  0,  0,  0,  0, 51, 51, 51, 51, 51, 51,
102,102,102,102,102,102,153,153,153,153,153,153,204,204,204,204,
204,204,255,255,255,255,255,255,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0, 17, 34, 68, 85,119,136,170,187,221,238,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0, 17, 34, 68, 85,119,136,170,187,221,238 };

unsigned char blumap[MAPSIZE] =
{ 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
 51, 51, 51, 51, 51, 51, 51, 51,102,102,102,102,102,102,102,102,
102,102,102,102,102,102,102,102,102,102,102,102,102,102,102,102,
102,102,102,102,102,102,102,102,102,102,102,102,153,153,153,153,
153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,
153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,
204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,
204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,
204,204,204,204,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 17, 34, 68, 85,
119,136,170,187,221,238, 17, 34, 68, 85,119,136,170,187,221,238 };


/* indicies of the primary colors and grey values in the color map */
static unsigned char rvec[16] = { 0, 216, 217, 1, 218, 219, 2, 220, 221,
				3, 222, 223, 4, 224, 225, 5};
static unsigned char gvec[16] = {  0, 226, 227,  6, 228, 229, 12, 230,
				231,  18, 232, 233, 24, 234, 235, 30};
static unsigned char bvec[16] = {  0, 236, 237,  36, 238, 239,  72, 240,
				241, 108, 242, 243, 144, 244, 245, 180 };
static unsigned char nvec[16] = {  0, 246, 247,  43, 248, 249,  86, 250,
				251, 129, 252, 253, 172, 254, 255, 215};

#define MAG1	51.0	/* magnitude of dither noise in color cube */
#define MAG2	8.0	/* magnitude of dither noise along primaries*/

#define DITHER(d, v, noise, mag) { \
	if ( (d = v + noise * mag) > 255) d=255; \
	else if (d < 0) d = 0; }

static double table[10] = {0.0, 0.1, -0.4, 0.2, -0.3, 0.3, -0.2, 0.4, -0.1, 0.5};
double *noise_ptr = table;
double *end_table = &table[10];
#define NOISE() (noise_ptr < end_table ? *noise_ptr++ : *(noise_ptr=table) )

/* convert 24 bit pixel into appropriate 8 bit pixel */
#define DITHERMAP(red, green, blue, i) {\
	register int _r, _g, _b; double dr, dg, db; \
	dr = NOISE(); DITHER(_r, red, dr, MAG1); _r = (_r+26) / 51; \
	dg = NOISE(); DITHER(_g, green, dg, MAG1); _g = (_g+26) / 51; \
	db = NOISE(); DITHER(_b, blue, db, MAG1); _b = (_b+26) / 51; \
	if (_r == _g) { \
		if (_r == _b) {		/* grey */ \
			DITHER(_r, red, dr, MAG2); \
			DITHER(_g, green, dg, MAG2); \
			DITHER(_b, blue, db, MAG2); \
			i = nvec[ ( (_r+_g+_b)/3) >> 4]; \
		} else if (_r == 0) {	/* all blue */ \
			DITHER(_r, red, dr, MAG2); \
			DITHER(_g, green, dg, MAG2); \
			DITHER(_b, blue, db, MAG2); \
			i = bvec[ _b >> 4]; \
		} else	/* color cube # */ \
			i = (unsigned char)(_r + _g * 6 + _b * 36); \
	} \
	else if (_g == _b && _g == 0) {	/* all red */ \
		DITHER(_r, red, dr, MAG2); \
		DITHER(_g, green, dg, MAG2); \
		DITHER(_b, blue, db, MAG2); \
		i = rvec[ _r >> 4]; \
	} else if (_r == _b && _r == 0) {	/* all green */ \
		DITHER(_r, red, dr, MAG2); \
		DITHER(_g, green, dg, MAG2); \
		DITHER(_b, blue, db, MAG2); \
		i = gvec[_g >> 4]; \
	} else	/* color cube # */ \
		i = (unsigned char)(_r + _g * 6 + _b * 36); }

#define REMAPIXEL(red, green, blue, i) {\
	register unsigned char _r, _g, _b; \
	_r = ((int)red+26)/51; _g = ((int)green+26)/51; _b = ((int)blue+26)/51; \
	if (_r == _g) { \
	    if (_r == _b) i = nvec[ ( ((int)red+(int)green+(int)blue) /3) >> 4]; /* grey */ \
	    else if (_r == 0)  i = bvec[blue/16];	   /* all blue */ \
	    else i = (unsigned char)(_r + _g * 6 + _b * 36);  /* cube # */ \
	} \
	else if (_g == _b && _g == 0) i = rvec[red/16];   /* all red */ \
	else if (_r == _b && _r == 0) i = gvec[green/16]; /* all green */ \
	else i = (unsigned char)(_r + _g * 6 + _b * 36);  /* color cube # */ \
}
/*
 *   D O I T --- convert stdin pix file to stdout rasterfile
 */
void doit(void)
{
    int i, cx, cy;
    unsigned char *pix, *rast;
    register unsigned char red, green, blue;

    if ( ((ras.ras_width/2)*2) != ras.ras_width ) {
	(void)fprintf(stderr, "%s: Cannot handle odd x dimension\n",progname);
	exit(1);
    }

    i = ras.ras_width * ras.ras_height;
    /* allocate buffer for the pix file */
    if ((pix=(unsigned char *)malloc(i*3)) == (unsigned char *)NULL) {
	(void)fprintf(stderr, "%s: cannot get memory for a %d x %d pix file\n",
		progname, ras.ras_width, ras.ras_height );
	exit(1);
    }

    if ((rast=(unsigned char *)malloc(i)) == (unsigned char *)NULL) {
	(void)fprintf(stderr, "%s: cannot get memory for a %d x %d pixrect\n",
		progname, ras.ras_width, ras.ras_height );
	exit(1);
    }

    /* load the pix file into memory (What's Virtual Memory for anyway?)
     * we reverse the order of the scan lines to compensate
     * for differences of origin location in rasterfiles vs. PIX files
     */
    for (i=ras.ras_height-1 ; i >= 0 ; i--)
	if (fread(&pix[i*ras.ras_width*3], ras.ras_width*3, 1, stdin) != 1) {
	 (void)fprintf(stderr, "%s: error reading %d x %d pix file scanline %d\n",
		progname, ras.ras_width, ras.ras_height, i);
	 exit(1);
	}

    /* convert 24 bit pixels to 8 bits,
     * switching top to bottom to compensate for the different origin
     * representations of PIX files and Sun pixrects
     */
    if (dither) {
	for(cy=0 ; cy < ras.ras_height ; cy++)
	    for(cx=0 ; cx < ras.ras_width ; cx++) {
		red = pix[(cx + cy * ras.ras_width)*3];
		green = pix[1 + (cx + cy * ras.ras_width)*3];
		blue = pix[2 + (cx + cy * ras.ras_width)*3];
		DITHERMAP(red, green, blue, rast[cx + cy * ras.ras_width]);
	    }
    }
    else {
	for(cy=0 ; cy < ras.ras_height ; cy++)
	    for(cx=0 ; cx < ras.ras_width ; cx++) {
		red = pix[(cx + cy * ras.ras_width)*3];
		green = pix[1 + (cx + cy * ras.ras_width)*3];
		blue = pix[2 + (cx + cy * ras.ras_width)*3];
		REMAPIXEL(red, green, blue, rast[cx + cy * ras.ras_width]);
	    }
    }

    /* now that we have the 8 bit pixels,
     *  we don't need the 24 bit pixels
     */
    free(pix);

    /* fill in miscelaneous rasterfile header fields */
    ras.ras_length = ras.ras_width * ras.ras_height;

    /* write the rasterfile header */
    if (fwrite(&ras, sizeof(ras), 1, stdout) != 1) {
	(void)fprintf(stderr, "%s: error writing rasterfile header to stdout\n", progname);
	exit(1);
    }

    /* write the colormap */
    if (fwrite(redmap, MAPSIZE, 1, stdout) != 1) {
	(void)fprintf(stderr, "%s: error writing colormap\n", progname);
	exit(1);
    }

    if (fwrite(grnmap, MAPSIZE, 1, stdout) != 1) {
	(void)fprintf(stderr, "%s: error writing colormap\n", progname);
	exit(1);
    }

    if (fwrite(blumap, MAPSIZE, 1, stdout) != 1) {
	(void)fprintf(stderr, "%s: error writing colormap\n", progname);
	exit(1);
    }

    /* write out the actual pixels */
    if (fwrite(rast, ras.ras_width, ras.ras_height, stdout) != ras.ras_height){
	(void)fprintf(stderr, "%s: error writing image\n", progname);
	exit(1);
    }
    free(rast);
}

void usage(void)
{

    (void)fprintf(stderr, "Usage: %s [-s squaresize] [-w width] [-n height] [ -d ]\n", progname);
    (void)fprintf(stderr, "\t< BRLpixfile > rasterfile\n");
    exit(1);
}

/*
 *   M A I N
 *
 *   Perform miscelaneous tasks such as argument parsing and
 *   I/O setup and then call "doit" to perform the task at hand
 */
int main(int ac, char **av)
{
    int	c, optlen;

    progname = *av;
    if (isatty(fileno(stdin))) usage();

    /* Get # of options & turn all the option flags off
    */
    optlen = strlen(options);

    for (c=0 ; c < optlen ; optflags[c++] = '\0');

    /* Turn off bu_getopt's error messages */
    bu_opterr = 0;

    /* get all the option flags from the command line
    */
    while ((c=bu_getopt(ac,av,options)) != EOF)
	switch (c) {
	case 'd'    : dither = !dither; break;
	case 'w'    : ras.ras_width = atoi(bu_optarg); break;
	case 'n'    : ras.ras_height = atoi(bu_optarg); break;
	case 's'    : ras.ras_width = ras.ras_height = atoi(bu_optarg); break;
	default     : usage(); break;
	}


    if (bu_optind < ac) usage();

    doit();
    return (0);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

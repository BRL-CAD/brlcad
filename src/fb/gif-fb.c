/*                        G I F - F B . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file gif-fb.c
 *
 * gif-fb -- convert a GIF file to (overlaid) frame buffer images
 *
 * Typical compilation:	cc -O -I/usr/brlcad/include -o gif-fb \
 *
 * gif-fb.c /usr/brlcad/lib/libfb.a
 * Add -DNO_VFPRINTF, -DNO_MEMCPY, or -DNO_STRRCHR if vfprintf(),
 * memcpy(), or strrchr() are not present in your C library
 * (e.g. on 4BSD-based systems).
 *
 * This is a full implementation of the (noninteractive) GIF format
 * conversion as specified in "A Standard Defining a Mechanism for the
 * Storage and Transmission of Raster-Based Graphics Information",
 * June 15, 1987 by CompuServe Incorporated.  This spec is far from
 * ideal, but it is a standard that has had wide influence in the PC
 * arena, and there are a lot of images available in GIF format.  Most
 * small computer systems have GIF translators available for them, so
 * this program provides a means of getting most PC images into the
 * BRL-CAD domain.
 *
 * Options:
 *
 * -F fb_file	outputs to the specified frame buffer file instead
 * of the one specified by the FB_FILE environment
 * variable (the default frame buffer, if no FB_FILE)
 *
 * -c		overrides the GIF file's specified color resolution,
 * using all 8 bits for each R, G, and B channel.
 *
 * -i image#	outputs just the specified image number (starting
 * at 1) to the frame buffer, instead of all images
 *
 * -o		"overlay": skips the initial clearing of the frame
 * buffer to the background color
 *
 * -v		"verbose": prints information about the images on
 * the standard error output
 *
 * -z		zoom up the framebuffer, keeping image aspect square
 *
 * gif_file	GIF input file to be translated (standard input if
 * no explicit GIF file name is specified)
 */

#include "common.h"

#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "bio.h"

#include "bu.h"
#include "fb.h"


#define USAGE "Usage: gif-fb [-F fb_file] [-c] [-i image#] [-o] [-v] [-z] [gif_file]\n       (stdin used with '<' construct if gif_file not supplied)"
#define OPTSTR "F:ci:ovzh?"

typedef int bool_t;

static bool_t clear = 1;		/* set if clear to background wanted */
static bool_t ign_cr = 0;		/* set if 8-bit color resoln. forced */
static bool_t verbose = 0;		/* set for GIF-file info printout */
static bool_t do_zoom = 0;		/* set to zoom framebuffer */
static int image = 0;		/* # of image to display (0 => all) */
static char *gif_file = NULL;	/* GIF file name */
static FILE *gfp = NULL;		/* GIF input stream handle */
static char *fb_file = NULL;	/* frame buffer name */
static FBIO *fbp = FBIO_NULL;	/* frame buffer handle */
static int ht;			/* virtual frame buffer height */
static int width, height;		/* overall "screen" size */
static int write_width;		/* used width of screen, <= width */
static int left, top, right, bottom;	/* image boundary */
static bool_t M_bit;			/* set if color map provided */
static bool_t I_bit;			/* set if image interlaced */
static int cr;			/* # bits of color resolution */
static int cr_mask;		/* mask to strip all but high cr bits */
static int g_pixel;		/* global # bits/pixel in image */
static int pixel;			/* local # bits/pixel in image */
static int background;		/* color index of screen background */
static size_t entries;		/* # of global color map entries */
static RGBpixel *g_cmap;		/* bu_malloc()ed global color map */
static RGBpixel *cmap;			/* bu_malloc()ed local color map */
/* NOTE:  Relies on R, G, B order and also on RGBpixel being 3 unsigned chars. */

#define GIF_EXTENSION '!'
#define GIF_IMAGE ','
#define GIF_TERMINATOR ';'

/* in ioutil.c */
void Message(const char *format, ...);
void Fatal(FBIO *fbiop, const char *format, ...);


static void
Sig_Catcher(int sig)
{
    (void)signal(sig, SIG_DFL);

    /* The following is not guaranteed to work, but it's worth a try. */
    Fatal(fbp, "Interrupted by signal %d", sig);
}


static void
Skip(void)					/* skip over raster data */
{
    int c;

    if ((c = getc(gfp)) == EOF)
	Fatal(fbp, "Error reading code size");

    while ((c = getc(gfp)) != 0) {
	if (c == EOF) {
	    Fatal(fbp, "Error reading block byte count");
	}
	do {
	    if (getc(gfp) == EOF)
		Fatal(fbp, "Error reading data byte");
	} while (--c > 0);
    }
}


/*
  The raster output loop is inverted, because it is simpler to
  let the LZW decompression drive pixel writing than vice versa.
*/

static int start_row[5] = { 0, 4, 2, 1, 0 };
static int step[5] = { 8, 8, 4, 2, 1 };
static int row, col;			/* current pixel coordinates */
static int pass;			/* current pass */
static int stop;			/* final pass + 1 */
static unsigned char *pixbuf = NULL;	/* bu_malloc()ed scan line buffer */


static void
PutPixel(int value)
{
    if (pass == stop)
	Fatal(fbp, "Too much raster data for image size");

    if (value > (int)entries)
	Fatal(fbp, "Decoded color index %d exceeds color map size", value);

    pixbuf[col*3+RED] = cmap[value][RED];	/* stuff pixel */
    pixbuf[col*3+GRN] = cmap[value][GRN];
    pixbuf[col*3+BLU] = cmap[value][BLU];

    if (++col == right) {
	/*
	  Note that BRL-CAD frame buffers are upside-down.
	  The following produces a right-side-up image at
	  the bottom of the available frame buffer.
	*/

	if (fb_write(fbp, 0, ht - row, pixbuf, write_width) < 0)
	    Message("Error writing scan line to frame buffer");

	col = left;

	if ((row += step[pass]) >= bottom && ++pass < stop)
	    row = start_row[pass];
    }
}


/*
  Limpel-Ziv-Welch decompression, based on "A Technique for
  High-Performance Data Compression" by Terry A. Welch in IEEE
  Computer, June 1984, pp. 8-19.

  GIF format usurps the first two "compression codes" for use
  as "reset" and "end of information".  The initial "code size"
  varies from 2 through (incremented) pixel, a maximum of 8.
  The LZW code data starts out 1 bit wider than the GIF "code
  size", and grows occasionally, up to 12 bits per code "chunk".
  LZW codes from 0 through (clear_code-1) are raw colormap
  index values, while those from clear_code on up are indices
  into the string-chaining table.

  This is my own implementation, using recursion instead of an
  explicit stack to expand the strings.
*/

static int code_size;		/* initial LZW chunk size, in bits */
static int chunk_size;		/* current LZW chunk size, in bits */
static int chunk_mask;		/* bit mask for extracting chunks */
static int compress_code;		/* first compression code value */
static int k;			/* extension character */
static struct
{
    short pfx;		/* prefix string's table index */
    short ext;		/* extension value */
}	table[1 << 12];		/* big enough for 12-bit codes */
/* Unlike the example in Welch's paper, our table contains no atomic values. */

static int bytecnt;		/* # of bytes remaining in block */
static int rem_bits;		/* data bits left over from last call */
static int bits_on_hand;		/* # of bits left over from last call */


static int
GetCode(void)
{
    int next_val;

    while (bits_on_hand < chunk_size) {
	/* Read 8 more bits from the GIF file. */

	while (bytecnt == 0) {
	    /* Start new data block. */

	    if ((bytecnt = getc(gfp)) == EOF)
		Fatal(fbp, "%s at start of new LZW data block", feof(gfp) ? "EOF" : "Error");

	    if (bytecnt == 0)
		Message("Warning: 0-byte data block");
	    /* Should this abort the image?  GIF spec is unclear. */
	}

	if ((next_val = getc(gfp)) == EOF)
	    Fatal(fbp, "%s while reading LZW data block", feof(gfp) ? "EOF" : "Error");

	--bytecnt;		/* used up another byte of input */
	rem_bits |= next_val << bits_on_hand;
	bits_on_hand += 8;
    }

    /* Now have enough bits to extract the next LZW code. */

    next_val = rem_bits & chunk_mask;

    /* Prepare for next entry. */

    rem_bits >>= chunk_size;
    bits_on_hand -= chunk_size;

    return next_val;
}


#ifdef USE_RECURSION

/* WARNING:  This recursion could get pretty deep (2047 nested calls)! */
static void
Expand(c)
    int c;		/* LZW code */
{
    if (c < compress_code)	/* "atomic", i.e. raw color index */
	PutPixel(k = c);	/* first atom in string */
    else {
	/* "molecular"; follow chain */
	Expand(table[c].pfx);
	PutPixel(table[c].ext);
    }
}


#else	/* The non-recursive version is usually faster. */

static short exp_buffer[(1 << 11) - 2];	/* reverse-order atomic codes */

/* Non-recursive version, for wimpy systems: */
static void
Expand(int c)
/* LZW code */
{
    short *bp = exp_buffer;

    while (c >= compress_code) {
	/* "molecular"; follow chain */
	assert(bp < &exp_buffer[2046]);
	*bp++ = table[c].ext;
	c = table[c].pfx;
    }

    /* current `c' is "atomic", i.e. raw color index */

    PutPixel(k = c);		/* first atom in string */

    while (bp > exp_buffer) {
	PutPixel((int)*--bp);
    }
}


#endif	/* USE_RECURSION */


static void
LZW(void)
{
    int c;		/* input LZW code, also input byte */
    int w;		/* prefix code */
    int next_code;	/* next available table index */
    int max_code;	/* limit at which LZW chunk must grow */
    int eoi_code;	/* end of LZW stream */
    int clear_code;	/* table reset code */

    if ((code_size = getc(gfp)) == EOF)
	Fatal(fbp, "Error reading code size");

    if (code_size < pixel)
	Message("Warning: initial code size smaller than colormap");

    if (code_size > pixel && !(pixel == 1 && code_size == 2))
	Message("Warning: initial code size greater than colormap");
    /* This case will probably eventually trigger Fatal() in PutPixel(). */

    /* Initialize GetCode() parameters. */

    bytecnt = 0;			/* need a new data block */
    bits_on_hand = 0;		/* there was no "last call" */

    /* Initialize LZW algorithm parameters. */

    clear_code = 1 << code_size;
    eoi_code = clear_code + 1;
    compress_code = clear_code + 2;

    if ((chunk_size = code_size + 1) > 12)	/* LZW chunk size */
	Fatal(fbp, "GIF spec's LZW code size limit (12) violated");

    max_code = 1 << chunk_size;	/* LZW chunk will grow at this point */
    chunk_mask = max_code - 1;
    next_code = compress_code;	/* empty chain-code table */
    w = -1;				/* we use -1 for "nil" */

    while ((c = GetCode()) != eoi_code) {
	if (c == clear_code) {
	    /* Reinitialize LZW parameters. */

	    chunk_size = code_size + 1;
	    max_code = 1 << chunk_size;	/* growth trigger */
	    chunk_mask = max_code - 1;
	    next_code = compress_code;	/* empty code table */
	    w = -1;		/* we use -1 for "nil" */
	} else {
	    if (c > next_code)
		Fatal(fbp, "LZW code impossibly large (%x > %x, diff: %d)", c, next_code, c-next_code);

	    if (c == next_code) {
		/* KwKwK special case */
		if (w < 0)	/* w supposedly previous code */
		    Fatal(fbp, "initial LZW KwKwK code??");

		Expand(w);	/* sets `k' */
		PutPixel(k);
	    } else {
		/* normal case */
		Expand(c);	/* sets `k' */
	    }

	    if (w >= 0 && next_code < 1 << 12) {
		table[next_code].pfx = w;
		table[next_code].ext = k;

		if (++next_code == max_code && chunk_size < 12) {
		    ++chunk_size;
		    max_code <<= 1;
		    chunk_mask = max_code - 1;
		}
	    }

	    w = c;
	}
    }

    /* EOI code encountered. */

    if (bytecnt > 0) {
	Message("Warning: unused raster data present");

	do {
	    if ((c = getc(gfp)) == EOF) {
		Fatal(fbp, "Error reading extra raster data");
	    }
	} while (--bytecnt > 0);
    }

    /* Strange data format in the GIF spec! */

    if ((c = getc(gfp)) != 0)
	Fatal(fbp, "Zero byte count missing");
}


static void
Rasters(void)				/* process (convert) raster data */
{
    /* Initialize inverted-loop parameters. */

    pass = I_bit ? 0 : 4;		/* current pass */
    stop = I_bit ? 4 : 5;		/* final pass + 1 */

    row = top + start_row[pass];	/* next pixel row */
    col = left;			/* next pixel column */

    /* Process rasters in possibly interlaced order. */

    LZW();				/* uncompress LZW data & write pixels */
}


int
main(int argc, char **argv)
{
    {
	/* Plant signal catcher. */
	static int getsigs[] = {
	    /* signals to catch */
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
	bool_t errors;

    	errors = argc == 1 && isatty(fileno(stdin));

	while ((c = bu_getopt(argc, argv, OPTSTR)) != -1) {
	    switch (c) {
		default:	/* '?': invalid option */
		    errors = 1;
		    break;

		case 'F':	/* -F fb_file */
		    fb_file = bu_optarg;
		    break;

		case 'c':	/* -c */
		    ign_cr = 1;
		    break;

		case 'i':	/* -i image# */
		    image = atoi(bu_optarg);
		    break;

		case 'o':	/* -o */
		    clear = 0;
		    break;

		case 'v':	/* -v */
		    verbose = 1;
		    break;
		case 'z':
		    do_zoom = 1;
		    break;
	    }
	}

	if (errors)
	    Fatal(fbp, USAGE);
    }

    if (bu_optind < argc) {
	/* gif_file */
	if (bu_optind < argc - 1) {
	    Message(USAGE);
	    Fatal(fbp, "Can't handle multiple GIF files");
	}

	if ((gfp = fopen(gif_file = argv[bu_optind], "rb")) == NULL)
	    Fatal(fbp, "Couldn't open GIF file \"%s\"", gif_file);
    } else
	gfp = stdin;

    /* Process GIF signature. */

    {
	/* Scan until "GIF" seen, to skip over additional headers
	   (e.g., from Macintosh BBSes). */

	int state;	/* FSA state */
#define ST_INITIAL 0 /* initial state of FSA */
#define ST_G_SEEN 1 /* just after 'G' */
#define ST_I_SEEN 2 /* just after 'I' */
#define ST_F_SEEN 3 /* just after 'F' */

	for (state = ST_INITIAL; state != ST_F_SEEN;)
	    /* In theory, the signature should be mapped to ASCII
	       as it is read.  However, non-ASCII systems are
	       expected to have other problems anyway, so we don't
	       worry about that in this implementation. */

	    switch (getc(gfp)) {
		case EOF:
		    Fatal(fbp, "File does not contain \"GIF\" header");

		case 'G':
		    state = ST_G_SEEN;
		    break;

		case 'I':
		    if (state != ST_G_SEEN)
			state = ST_INITIAL;
		    else
			state = ST_I_SEEN;
		    break;

		case 'F':
		    if (state != ST_I_SEEN)
			state = ST_INITIAL;
		    else {
			char ver[3];
			/* GIF version (assume 8-bit bytes) */

			state = ST_F_SEEN;	/* ends loop */

			if (fread(ver, 1, 3, gfp) != 3)
			    Fatal(fbp, "Error reading GIF signature");

			if (bu_strncmp(ver, "87a", 3) != 0)
			    Message("GIF version \"%3.3s\" not known, \"87a\" assumed", ver);
		    }
		    break;
	    }
    }

    /* Process screen descriptor. */

    {
	unsigned char desc[7];	/* packed screen descriptor */

	if (fread(desc, 1, 7, gfp) != 7)
	    Fatal(fbp, "Error reading screen descriptor");

	width = desc[1] << 8 | desc[0];
	height = desc[3] << 8 | desc[2];
	if (width < 0)
	    width = 0;
	else if (width > INT_MAX-1)
	    width = INT_MAX-1;
	if (height < 0)
	    height = 0;
	else if (height > INT_MAX-1)
	    height = INT_MAX-1;

	M_bit = (desc[4] & 0x80) != 0;
	cr = (desc[4] >> 4 & 0x07) + 1;
	g_pixel = (desc[4] & 0x07) + 1;
	background = desc[5];
	if (background < 0)
	    background = 0;
	else if (background > CHAR_MAX)
	    background = CHAR_MAX;

	if (verbose) {
	    Message("screen %dx%d", width, height);

	    if (M_bit)
		Message("global color map provided");

	    Message("%d bits of color resolution", cr);
	    Message("%d default bits per pixel", g_pixel);
	    Message("background color index %d", background);
	}

	if ((desc[5] & 0x08) != 0x00)
	    Message("Screen descriptor byte 6 bit 3 unknown");

	if (desc[6] != 0x00)
	    Message("Screen descriptor byte 7 = %2.2x unknown", desc[6]);

	if (ign_cr)
	    cr = 8;		/* override value from GIF file */
    }

    /* Process global color map. */

    g_cmap = (RGBpixel *)bu_malloc(256 * sizeof(RGBpixel), "g_cmap");
    cmap = (RGBpixel *)bu_malloc(256 * sizeof(RGBpixel), "cmap");

    entries = (size_t)(1 << g_pixel);

    if (M_bit) {
	int i;
	double expand;	/* dynamic range expansion factor ~ 1 */

	/* Read in global color map. */

	if (verbose)
	    Message("global color map has %d entries", entries);

	if (fread(g_cmap, 3, entries, gfp) != entries)
	    Fatal(fbp, "Error reading global color map");

	/* Mask off low-order "noise" bits found in some GIF files,
	   and expand dynamic range to support pure white and black. */

	if (cr == 8)
	    cr_mask = ~0;	/* shift by 0 can tickle compiler bug */
	else
	    cr_mask = ~0 << (8 - cr);

	expand = 255.0 / (double)(0xFF & cr_mask);

	for (i = 0; i < (int)entries; ++i) {
	    g_cmap[i][RED] = (((int)g_cmap[i][RED]) & cr_mask) * expand
		+ 0.5;
	    g_cmap[i][GRN] = (((int)g_cmap[i][GRN]) & cr_mask) * expand
		+ 0.5;
	    g_cmap[i][BLU] = (((int)g_cmap[i][BLU]) & cr_mask) * expand
		+ 0.5;
	}
    } else {
	int i;

	/* Set up default linear grey scale global color map.
	   GIF specs for this case are utterly nonsensical. */

	if (verbose)
	    Message("default global color map has %d grey values", entries);

	for (i = 0; i < (int)entries; ++i)
	    g_cmap[i][RED] =
		g_cmap[i][GRN] =
		g_cmap[i][BLU] = i * 255.0 / (entries - 1) + 0.5;
    }

    /* Open frame buffer for unbuffered output. */

    pixbuf = (unsigned char *)bu_malloc(width * sizeof(RGBpixel), "pixbuf");

    if ((fbp = fb_open(fb_file, width, height)) == FBIO_NULL) {
	Fatal(fbp, "Couldn't open frame buffer");
    }

    {
	int wt = fb_getwidth(fbp);
	int zoom;

	ht = fb_getheight(fbp);

	if (wt < width || ht < height)
	    Message("Frame buffer too small (%dx%d); %dx%d needed", wt, ht, width, height);

	if (verbose && (wt > width || ht > height))
	    Message("Frame buffer (%dx%d) larger than GIF screen", wt, ht);

	write_width = width;
	if (write_width > wt) write_width = wt;

	zoom = fb_getwidth(fbp)/width;
	if (fb_getheight(fbp)/height < zoom)
	    zoom = fb_getheight(fbp)/height;
	if (do_zoom && zoom > 1) {
	    (void)fb_view(fbp, width/2, height/2,
			  zoom, zoom);
	} else {
	    /* Unzoomed, full screen */
	    (void)fb_view(fbp,
			  fb_getwidth(fbp)/2, fb_getheight(fbp)/2,
			  1, 1);
	}

	ht = height - 1;	/* for later use as (ht - row) */
    }

    /* Fill frame buffer with background color. */

    if (clear && fb_clear(fbp, g_cmap[background]) == -1) {
	Fatal(fbp, "Error clearing frame buffer to background");
    }

    /* Fill scanline buffer with background color too */
    {
	int i;
	for (i=0; i < width; i++) {
	    COPYRGB(&pixbuf[i*3], g_cmap[background]);
	}
    }

    /* Convert images.  GIF spec says no pauses between them. */

    for (;;) {
	int c;

	if ((c = getc(gfp)) == EOF)
	    Fatal(fbp, "Missing GIF terminator");

	switch (c) {
	    default:
		Message("Warning: unknown separator 0x%2.2x", c);
		continue;	/* so says the GIF spec */

	    case GIF_TERMINATOR:	/* GIF terminator */
	    terminate:
		/* GIF spec suggests pause and wait for go-ahead here,
		   also "screen clear", but they're impractical. */

		if (fb_close(fbp) == -1) {
		    fbp = FBIO_NULL;	/* avoid second try */
		    Fatal(fbp, "Error closing frame buffer");
		}

		fbp = FBIO_NULL;

		if (image > 0)
		    Fatal(fbp, "Specified image not found");

		/* release allocated memory */
		bu_free(pixbuf, "pixbuf");
		bu_free(cmap, "cmap");
		bu_free(g_cmap, "g_cmap");

		bu_exit(EXIT_SUCCESS, NULL);

	    case GIF_EXTENSION: {
		/* GIF extension block introducer */
		int i;

		if ((i = getc(gfp)) == EOF)
		    Fatal(fbp, "Error reading extension function code");

		Message("Extension function code %d unknown", i);

		while ((i = getc(gfp)) != 0) {
		    if (i == EOF) {
			Fatal(fbp, "Error reading extension block byte count");
		    }

		    do {
			if (getc(gfp) == EOF) {
			    Fatal(fbp, "Error reading extension block");
			}
		    } while (--i > 0);
		}
	    }
		break;

	    case GIF_IMAGE: {
		/* image separator */
		unsigned char desc[9];  /* image descriptor */

		if (fread(desc, 1, 9, gfp) != 9)
		    Fatal(fbp, "Error reading image descriptor");

		left = desc[1] << 8 | desc[0];
		top = desc[3] << 8 | desc[2];
		right = desc[5] << 8 | desc[4];
		bottom = desc[7] << 8 | desc[6];
		M_bit = (desc[8] & 0x80) != 0;
		I_bit = (desc[8] & 0x40) != 0;

		pixel = M_bit ? (desc[8] & 0x07) + 1 : g_pixel;

		if (verbose) {
		    Message("image (%d, %d, %d, %d)", left, top, right, bottom);

		    if (M_bit) {
			Message("local color map provided");
			Message("%d bits per pixel", pixel);
		    }

		    Message(I_bit ? "interlaced" : "sequential");
		}

		if (left < 0 || right > width || left >= right
		    || top < 0 || bottom > height || top >= bottom)
		{
		    Fatal(fbp, "Absurd image (%d, %d, %d, %d)", left, top, right, bottom);
		}
	    }

		/* Process local color map. */

		entries = (size_t)(1 << pixel);

		if (M_bit) {
		    int i;
		    double expand;	/* range expansion */

		    /* Read in local color map. */

		    if (verbose)
			Message("local color map has %d entries", entries);

		    if (fread(cmap, 3, entries, gfp) != entries)
			Fatal(fbp, "Error reading local color map");

		    /* Mask off low-order "noise" bits,
		       and expand dynamic range. */

		    expand = 255.0 / (double)(0xFF & cr_mask);

		    for (i = 0; i < (int)entries; ++i) {
			cmap[i][RED] = (((int)cmap[i][RED]) & cr_mask)
			    * expand + 0.5;
			cmap[i][GRN] = (((int)cmap[i][GRN]) & cr_mask)
			    * expand + 0.5;
			cmap[i][BLU] = (((int)cmap[i][BLU]) & cr_mask)
			    * expand + 0.5;
		    }
		} else {
		    /* Use default global color map. */

		    if (verbose)
			Message("global color map used");

		    (void)memcpy((char *)cmap, (char *)g_cmap, 3 * entries);
		}

		/* `image' is 0 if all images are to be displayed;
		   otherwise it is a down-counter to the image wanted */
		if (image <= 1) {
		    Rasters();	/* process the raster data */

		    if (image != 0)
			goto terminate;	/* that's all, folks */
		} else {
		    --image;	/* desperately seeking Susan? */
		    Skip();	/* skip over the raster data */
		}

		break;
	}
    }

    /* [not reached] */
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

/*                       S U N - P I X . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2025 United States Government as represented by
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
/** @file util/sun-pix.c
 *
 * Program to take Sun bitmap files created with Sun's ``screendump''
 * command, and convert them to pix(5) format files.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <limits.h>
#include "bio.h"

#include "vmath.h"

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/getopt.h"
#include "bu/log.h"

/*
 * Description of Sun header for files containing raster images
 */
struct rasterfile {
    int ras_magic;		/* magic number */
    int ras_width;		/* width (pixels) of image */
    int ras_height;		/* height (pixels) of image */
    int ras_depth;		/* depth (1, 8, or 24 bits) of pixel */
    int ras_length;		/* length (bytes) of image */
    int ras_type;		/* type of file; see RT_* below */
    int ras_maptype;		/* type of colormap; see RMT_* below */
    int ras_maplength;		/* length (bytes) of following map */
    /* color map follows for ras_maplength bytes, followed by image */
} header;

char inbuf[sizeof(struct rasterfile)];

#define RAS_MAGIC 0x59a66a95

/* Sun supported ras_type's */
#define RT_OLD 0	/* Raw pixrect image in 68000 byte order */
#define RT_STANDARD 1	/* Raw pixrect image in 68000 byte order */
#define RT_BYTE_ENCODED 2	/* Run-length compression of bytes */
#define RT_EXPERIMENTAL 0xffff	/* Reserved for testing */

/* Sun registered ras_maptype's */
#define RMT_RAW 2
/* Sun supported ras_maptype's */
#define RMT_NONE 0	/* ras_maplength is expected to be 0 */
#define RMT_EQUAL_RGB 1	/* red[ras_maplength/3], green[], blue[] */

#define GETUC_CHECKED(uc, fp, err_msg) \
    { \
	int _c = getc(fp); \
	if (_c == EOF) { \
	    bu_exit(1, err_msg); \
	} \
	uc = (unsigned char)_c; \
    }

/*
 * NOTES:
 * Each line of the image is rounded out to a multiple of 16 bits.
 * This corresponds to the rounding convention used by the memory pixrect
 * package (/usr/include/pixrect/memvar.h) of the SunWindows system.
 * The ras_encoding field (always set to 0 by Sun's supported software)
 * was renamed to ras_length in release 2.0.  As a result, rasterfiles
 * of type 0 generated by the old software claim to have 0 length; for
 * compatibility, code reading rasterfiles must be prepared to compute the
 * true length from the width, height, and depth fields.
 */

int pixout = 1;		/* 0 = bw(5) output, 1 = pix(5) output */
int colorout = 0;
int hflag = 0;
int inverted = 0;
int pure = 0;			/* No Sun header if pure is 1 */
int verbose = 0;
struct colors {
    unsigned char CL_red;
    unsigned char CL_green;
    unsigned char CL_blue;
};


struct colors Cmap[256];
static size_t CMAP_MAX_INDEX = sizeof(Cmap) - 1;

static char hyphen[] = "-";
static char *file_name;
static FILE *fp;

char usage[] = "\
Usage: sun-pix [-b -H -i -P -v -C] [sun.bitmap]\n\
       (standard output must be redirected)\n";


#define NET_LONG_LEN 4 /* # bytes to network long */

static uint32_t
getlong(char *msgp)
{
    unsigned char *p = (unsigned char *) msgp;
    uint32_t u;

    u = *p++; u <<= 8;
    u |= *p++; u <<= 8;
    u |= *p++; u <<= 8;
    return u | *p;
}


static int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "bHiPvCh?")) != -1) {
	switch (c) {
	    case 'b':
		pixout = 0;	/* bw(5) */
		break;
	    case 'C':
		colorout = 1;	/* output the color map */
		break;
	    case 'H':
		hflag = 1;	/* print header */
		break;
	    case 'i':
		inverted = 1;
		break;
	    case 'P':
		pure = 1;
		break;
	    case 'v':
		verbose = 1;
		break;

	    default:		/* '?' 'h' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return 0;
	file_name = hyphen;
	fp = stdin;
    } else {
	file_name = argv[bu_optind];
	if ((fp = fopen(file_name, "rb")) == NULL) {
	    fprintf(stderr,
		    "sun-pix: cannot open \"%s\" for reading\n",
		    file_name);
	    return 0;
	}
    }

    if (argc > ++bu_optind)
	fprintf(stderr, "sun-pix: excess argument(s) ignored\n");

    return 1;		/* OK */
}


/*
 * Encode/decode functions for RT_BYTE_ENCODED images:
 *
 * The "run-length encoding" is of the form
 *
 * <byte><byte>...<ESC><0>...<byte><ESC><count><byte>...
 *
 * where the counts are in the range 0..255 and the actual number of
 * instances of <byte> is <count>+1 (i.e. actual is 1..256). One- or
 * two-character sequences are left unencoded; three-or-more character
 * sequences are encoded as <ESC><count><byte>.  <ESC> is the character
 * code 128.  Each single <ESC> in the input data stream is encoded as
 * <ESC><0>, because the <count> in this scheme can never be 0 (actual
 * count can never be 1).  <ESC><ESC> is encoded as <ESC><1><ESC>.
 *
 * This algorithm will fail (make the "compressed" data bigger than the
 * original data) only if the input stream contains an excessive number of
 * one- and two-character sequences of the <ESC> character.
 */

#define ESCAPE 128

static size_t
decoderead(unsigned char *buf, int size, int length, FILE *readfp)

/* should be one! */
/* number of items to read */
/* input file pointer */
{
    static int repeat = -1;
    static int lastchar = 0;
    int number_read = 0;

    if (size != 1) {
	bu_exit(1, "decoderead: unable to process size = %d.\n", size);
    }

    while (length) {
	if (repeat >= 0) {
	    *buf = lastchar;
	    --length;
	    ++buf;
	    number_read++;
	    --repeat;
	} else {
	    lastchar = getc(readfp);
	    if (lastchar < 0) return number_read;
	    if (lastchar == ESCAPE) {
		repeat = getc(readfp);
		if (repeat <0) return number_read;
		if (repeat == 0) {
		    *buf = ESCAPE;
		    ++buf;
		    number_read++;
		    --length;
		    --repeat;
		} else {
		    lastchar = getc(readfp);
		    if (lastchar < 0) return number_read;
		}
	    } else {
		*buf = lastchar;
		--length;
		++buf;
		++number_read;
	    }
	}
    }
    return number_read;
}


unsigned char bits[8] = { 128, 64, 32, 16, 8, 4, 2, 1 };

int
main(int argc, char **argv)
{
    int x;
    int off = 0;
    int on = 255;
    int width;			/* line width in bits */
    int scanbytes;		/* bytes/line (padded to 16 bits) */
    unsigned char c, cmap_idx, buf[BU_PAGE_SIZE];

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    fp = stdin;
    if (!get_args(argc, argv) || (isatty(fileno(stdout)) && (hflag == 0))) {
	(void)fputs(usage, stderr);
	bu_exit (1, NULL);
    }
    if (inverted) {
	off = 255;
	on = 0;
    }

    if (!pure) {
	long nbits;
	size_t ret;

	ret = fread(inbuf, sizeof(struct rasterfile), 1, fp);
	if (ret < 1)
	    perror("fread");

	header.ras_magic = getlong(&inbuf[NET_LONG_LEN*0]);
	header.ras_width = getlong(&inbuf[NET_LONG_LEN*1]);
	header.ras_height = getlong(&inbuf[NET_LONG_LEN*2]);
	header.ras_depth = getlong(&inbuf[NET_LONG_LEN*3]);
	header.ras_length = getlong(&inbuf[NET_LONG_LEN*4]);
	header.ras_type = getlong(&inbuf[NET_LONG_LEN*5]);
	header.ras_maptype = getlong(&inbuf[NET_LONG_LEN*6]);
	header.ras_maplength = getlong(&inbuf[NET_LONG_LEN*7]);

	if (header.ras_magic != RAS_MAGIC) {
	    bu_log("sun-pix: bad magic number, was x%x, s/b x%x\n",
		   header.ras_magic, RAS_MAGIC);
	    return 1;
	}

	/* Width is rounded up to next multiple of 16 bits */
	if (header.ras_width*15 > (LONG_MAX-1) / header.ras_depth) {
	    bu_log("Image width too big\n");
	    return -1;
	}
	nbits = header.ras_width * header.ras_depth;
	nbits = (nbits + 15) & ~15;
	header.ras_width = nbits / header.ras_depth;

	/* sanitize header, basic bounds checks */
	if (header.ras_width < 0)
	    header.ras_width = 0;
	if (header.ras_width > INT_MAX-1)
	    header.ras_width = INT_MAX-1;
	if (header.ras_height < 0)
	    header.ras_height = 0;
	if (header.ras_height > INT_MAX-1)
	    header.ras_height = INT_MAX-1;
	if (header.ras_depth < 0)
	    header.ras_depth = 0;
	if (header.ras_depth > 256)
	    header.ras_depth = 256;
	if (header.ras_length < 0)
	    header.ras_length = 0;
	if (header.ras_length > INT_MAX-1)
	    header.ras_length = INT_MAX-1;
	if (header.ras_type < 0)
	    header.ras_type = 0;
	if (header.ras_type > RT_EXPERIMENTAL-1)
	    header.ras_type = 0;
	if (header.ras_maptype < 0)
	    header.ras_maptype = 0;
	if (header.ras_maptype > 10)
	    header.ras_maptype = 10;
	if (header.ras_maplength < 0)
	    header.ras_maplength = 0;
	if (header.ras_maplength > INT_MAX-1)
	    header.ras_maplength = INT_MAX-1;

	if (verbose) {
	    fprintf(stderr,
		    "ras_width = %d, ras_height = %d\nras_depth = %d, ras_length = %d\n",
		    header.ras_width, header.ras_height,
		    header.ras_depth, header.ras_length);
	    fprintf(stderr,
		    "ras_type = %d, ras_maptype = %d, ras_maplength = %d\n",
		    header.ras_type,
		    header.ras_maptype,
		    header.ras_maplength);
	}
	if (hflag) {
	    printf("-w%d -n%d\n", header.ras_width, header.ras_height);
	    return 0;
	}
    } else {
	/* "pure" bitmap */
	header.ras_type = RT_STANDARD;
	header.ras_depth = 1;
    }

    switch (header.ras_type) {
	case RT_OLD:		/* ??? */
	case RT_BYTE_ENCODED:
	case RT_STANDARD:
	    break;
	default:
	    bu_log("sun-pix:  Unable to process type %d images\n",
		   header.ras_type);
	    return 1;
    }

    width = header.ras_width;

    switch (header.ras_depth) {
	case 1:
	    /* 1-bit image */
	    /* Gobble colormap -- ought to know what to do with it */
	    for (x=0; x<header.ras_maplength; x++) {
		(void)getc(fp);
	    }
	    if (colorout) {
		fprintf(stdout, "%d\t%04x %04x %04x\n", off, off<<8,
			off<<8, off<<8);
		fprintf(stdout, "%d\t%04x %04x %04x\n", on, on<<8,
			on<<8, on<<8);
		break;
	    }

	    scanbytes = ((width + 15) & ~15L) / 8;
	    while ((header.ras_type == RT_BYTE_ENCODED) ?
		   decoderead(buf, sizeof(*buf), scanbytes, fp) :
		   fread(buf, sizeof(*buf), scanbytes, fp)) {
		for (x = 0; x < width; x++) {
		    if (buf[x>>3] & bits[x&7]) {
			putchar(on);
			if (pixout) {putchar(on);putchar(on);}
		    } else {
			putchar(off);
			if (pixout) {putchar(off);putchar(off);}
		    }
		}
	    }
	    break;
	case 8:
	    /* 8-bit image */
	    if (header.ras_maptype != RMT_EQUAL_RGB) {
		bu_log("sun-pix:  unable to handle depth=8, maptype = %d.\n",
		       header.ras_maptype);
		return 1;
	    }
	    scanbytes = width;
	    for (x = 0; x < header.ras_maplength/3; x++) {
		GETUC_CHECKED(c, fp, "sun-pix: expected red color value, but end-of-file reached.\n");
		if (inverted) {
		    Cmap[x].CL_red = 255-c;
		} else {
		    Cmap[x].CL_red = c;
		}
	    }
	    for (x = 0; x < header.ras_maplength/3; x++) {
		GETUC_CHECKED(c, fp, "sun-pix: expected green color value, but end-of-file reached.\n");
		if (inverted) {
		    Cmap[x].CL_green = 255-c;
		} else {
		    Cmap[x].CL_green = c;
		}
	    }
	    for (x = 0; x < header.ras_maplength/3; x++) {
		GETUC_CHECKED(c, fp, "sun-pix: expected blue color value, but end-of-file reached.\n");
		if (inverted) {
		    Cmap[x].CL_blue = 255-c;
		} else {
		    Cmap[x].CL_blue = c;
		}
	    }
	    if (colorout) {
		for (x = 0; x <header.ras_maplength/3; x++) {
		    fprintf(stdout, "%d\t%04x %04x %04x\n",
			    x, Cmap[x].CL_red<<8, Cmap[x].CL_green<<8,
			    Cmap[x].CL_blue<<8);
		}
		break;
	    }

	    while ((header.ras_type == RT_BYTE_ENCODED) ?
		   decoderead(buf, sizeof(*buf), scanbytes, fp) :
		   fread(buf, sizeof(*buf), scanbytes, fp)) {
		for (x = 0; x < width && x < (int)sizeof(*buf); x++) {
		    cmap_idx = buf[x];
		    if (cmap_idx >= CMAP_MAX_INDEX) {
			bu_log("Warning: Read invalid index %u.\n",
			       (unsigned int)buf[x]);
			return 1;
		    }
		    if (pixout) {
			putchar(Cmap[cmap_idx].CL_red);
			putchar(Cmap[cmap_idx].CL_green);
			putchar(Cmap[cmap_idx].CL_blue);
		    } else {
			putchar(cmap_idx);
		    }
		}
	    }
	    break;
	default:
	    bu_log("sun-pix:  unable to handle depth=%d\n",
		   header.ras_depth);
	    return 1;
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

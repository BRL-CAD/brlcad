/*                       F B L A B E L . C
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
/** @file fblabel.c
 *
 * Draw a Label on a Frame buffer image.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "bu/color.h"
#include "bu/getopt.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/vfont.h"
#include "fb.h"
#include "pkg.h"


#define FONTBUFSZ 200

static char *framebuffer = NULL;
static char *font1 = NULL;

fb *fbp;

static char usage[] = "\
Usage: fblabel [-c -a] [-F framebuffer] [-C r/g/b]\n\
	[-S scr_squaresize] [-W scr_width] [-N scr_height]\n\
	[-f fontstring] xpos ypos textstring\n";

/* Variables controlling the font itself */

int width = 0;	/* Size of current character.		*/
int height = 0;

static int filterbuf[FONTBUFSZ][FONTBUFSZ];

static int scr_width = 0;	/* default input width */
static int scr_height = 0;	/* default input height */
static int clear = 0;

static RGBpixel pixcolor;
static int xpos;
static int ypos;
static char *textstring;
static int debug = 0;
static int alias_off = 0;

/*
 * squash - Filter super-sampled image for one scan line
 */

/* Cone filtering weights.
 * #define CNTR_WT 0.23971778
 * #define MID_WT 0.11985889
 * #define CRNR_WT 0.07021166
 */

/* Gaussian filtering weights. */
#define CNTR_WT 0.3011592441
#define MID_WT 0.1238102667
#define CRNR_WT 0.0508999223

/* Squash takes three super-sampled "bit arrays", and returns an array
   of intensities at half the resolution.  N is the size of the bit
   arrays.  The "bit arrays" are actually int arrays whose values are
   assumed to be only 0 or 1.
*/
void
squash(int *buf0, int *buf1, int *buf2, float *ret_buf, int n)
{
    int j;

    for (j = 1; j < n - 1; j++) {
	if (alias_off) {
	    ret_buf[j] = buf1[j];
	} else {
	    ret_buf[j] =
		(
		    buf2[j - 1] * CRNR_WT +
		    buf2[j] * MID_WT +
		    buf2[j + 1] * CRNR_WT +
		    buf1[j - 1] * MID_WT +
		    buf1[j] * CNTR_WT +
		    buf1[j + 1] * MID_WT +
		    buf0[j - 1] * CRNR_WT +
		    buf0[j] * MID_WT +
		    buf0[j + 1] * CRNR_WT
		    );
	}
    }
}


/*
   Extract a bit field from a bit string.
*/
int
bitx(char *bitstring, int posn)
{
    for (; posn >= 8; posn -= 8, bitstring++);
#if defined(CANT_DO_ZERO_SHIFT)
    if (posn == 0)
	return (int) (*bitstring) & 1;
    else
#endif
	return (int) (*bitstring) & (1 << posn);
}


/*
 Fills in the buffer by reading a row of a bitmap from the character
 font file.  The file pointer is assumed to be in the correct
 position.
*/
void
fill_buf(int wid, int *buf, char *bitrow)
{
    int j;

    /*
     * For each bit in the row, set the array value to 1 if it's
     * on. The bitx routine extracts the bit value.  Can't just use
     * the j-th bit because the bytes are backwards.
     */
    for (j = 0; j < wid; j++)
	if (bitx(bitrow, (j & ~7) + (7 - (j & 7))))
	    buf[j + 2] = 1;
	else
	    buf[j + 2] = 0;

    /*
     * Need two samples worth of background on either end to make the
     * filtering come out right without special casing the filtering.
     */
    buf[0] = buf[1] = buf[wid + 2] = buf[wid + 3] = 0;
}


void
do_char(struct vfont *vfp, struct vfont_dispatch *vdp, int x, int y)
{
    int i, j;
    int base;
    int totwid = width;
    int ln;
    static float resbuf[FONTBUFSZ];
    static RGBpixel fbline[FONTBUFSZ];
    int bytes_wide;	/* # bytes/row in bitmap */

    bytes_wide = (width+7)>>3;

    /* Read in the character bit map, with two blank lines on each end. */
    for (i = 0; i < 2; i++)
	memset((char *)&filterbuf[i][0], 0, (totwid+4)*sizeof(int));

    for (ln=0, i = height + 1; i >= 2; i--, ln++)
	fill_buf (width, &filterbuf[i][0],
		  &vfp->vf_bits[vdp->vd_addr + bytes_wide*ln]);

    for (i = height + 2; i < height + 4; i++)
	memset((char *)&filterbuf[i][0], 0, (totwid+4)*sizeof(int));

    /* Initial base line for filtering depends on odd flag. */
    if (vdp->vd_down % 2)
	base = 1;
    else
	base = 2;

    /* Produce a RGBpixel buffer from a description of the character
     * and the read back data from the frame buffer for anti-aliasing.
     */
    for (i = height + base; i >= base; i--) {
	squash(filterbuf[i - 1],	/* filter info */
	       filterbuf[i],
	       filterbuf[i + 1],
	       resbuf,
	       totwid + 4
	    );
	fb_read(fbp, x, y - vdp->vd_down + i, (unsigned char *)fbline, totwid+3);
	for (j = 0; j < (totwid + 3) - 1; j++) {
	    int tmp;
	    /* EDITOR'S NOTE : do not rearrange this code, the SUN
	     * compiler can't handle more complex expressions.
	     */

	    tmp = fbline[j][RED] & 0377;
	    fbline[j][RED] =
		(int)(pixcolor[RED]*resbuf[j]+(1-resbuf[j])*tmp);
	    fbline[j][RED] &= 0377;
	    tmp = fbline[j][GRN] & 0377;
	    fbline[j][GRN] =
		(int)(pixcolor[GRN]*resbuf[j]+(1-resbuf[j])*tmp);
	    fbline[j][GRN] &= 0377;
	    tmp = fbline[j][BLU] & 0377;
	    fbline[j][BLU] =
		(int)(pixcolor[BLU]*resbuf[j]+(1-resbuf[j])*tmp);
	    fbline[j][BLU] &= 0377;
	}
	if (fb_write(fbp, x, y-vdp->vd_down+i, (unsigned char *)fbline, totwid+3) < totwid+3) {
	    fprintf(stderr, "fblabel: pixel write error\n");
	    bu_exit(1, NULL);
	}
    }
}


void
do_line(struct vfont *vfp, char *line)
{
    int currx;
    int char_id;
    size_t char_count;
    size_t len = strlen(line);

    if (vfp == VFONT_NULL) return;

    currx = xpos;

    for (char_count = 0; char_count < len; char_count++) {
	struct vfont_dispatch *vdp;

	char_id = (int) line[char_count] & 0377;

	/* Obtain the dimensions for the character */
	vdp = &vfp->vf_dispatch[char_id];
	width = vdp->vd_left + vdp->vd_right;
	height = vdp->vd_up + vdp->vd_down;
	if (debug) fprintf(stderr, "%c w=%2d h=%2d, currx=%d\n", char_id, width, height, currx);

	/*
	 * pace characters are frequently not represented in the font
	 * set, so leave white space here.
	 */
	if (width <= 1) {
	    char_id = 'n';	/* 1-en space */
	    vdp = &vfp->vf_dispatch[char_id];
	    width = vdp->vd_left + vdp->vd_right;
	    if (width <= 1) {
		char_id = 'N';	/* 1-en space */
		vdp = &vfp->vf_dispatch[char_id];
		width = vdp->vd_left + vdp->vd_right;
		if (width <= 1)
		    width = 16;	/* punt */
	    }
	    currx += width;
	    continue;
	}

	if (currx + width > fb_getwidth(fbp) - 1) {
	    fprintf(stderr, "fblabel:  Ran off screen\n");
	    break;
	}

	do_char(vfp, vdp, currx, ypos);
	currx += vdp->vd_width + 2;
    }
}


int
get_args(int argc, char **argv)
{

    int c;

    pixcolor[RED]  = 255;
    pixcolor[GRN]  = 255;
    pixcolor[BLU]  = 255;

    while ((c = bu_getopt(argc, argv, "adcF:f:r:g:b:C:s:S:w:W:n:N:h?")) != -1) {
	switch (c) {
	    case 'a':
		alias_off = 1;
		break;
	    case 'd':
		debug = 1;
		break;
	    case 's':
	    case 'S':
		/* square size */
		scr_height = scr_width = atoi(bu_optarg);
		break;
	    case 'w':
	    case 'W':
		scr_width = atoi(bu_optarg);
		break;
	    case 'n':
	    case 'N':
		scr_height = atoi(bu_optarg);
		break;
	    case 'c':
		clear = 1;
		break;
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 'f':
		font1 = bu_optarg;
		break;
	    case 'C': {
		char *cp = bu_optarg;
		unsigned char *conp
		    = (unsigned char *)pixcolor;

		/* premature null => atoi gives zeros */
		for (c=0; c < 3; c++) {
		    *conp++ = atoi(cp);
		    while (*cp && *cp++ != '/')
			;
		}
	    }
		break;
		/* backward compatibility */
	    case 'r':
		pixcolor[RED] = atoi(bu_optarg);
		break;
	    case 'g':
		pixcolor[GRN] = atoi(bu_optarg);
		break;
	    case 'b':
		pixcolor[BLU] = atoi(bu_optarg);
		break;
	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind+3 > argc)
	return 0;
    xpos = atoi(argv[bu_optind++]);
    ypos = atoi(argv[bu_optind++]);
    textstring = argv[bu_optind++];
    if (debug) fprintf(stderr, "fblabel %d %d %s\n", xpos, ypos, textstring);

    if (argc > bu_optind)
	fprintf(stderr, "fblabel: excess argument(s) ignored\n");

    return 1;		/* OK */
}


int
main(int argc, char **argv)
{
    struct vfont *vfp;

    bu_setprogname(argv[0]);

    if (!get_args(argc, argv)) {
	fputs(usage, stderr);
	bu_exit(1, NULL);
    }

    if ((fbp = fb_open(framebuffer, scr_width, scr_height)) == NULL) {
	fprintf(stderr, "fblabel:  Unable to open framebuffer %s\n", framebuffer);
	bu_exit(12, NULL);
    }

    if (clear) {
	fb_clear(fbp, PIXEL_NULL);
    }

    if ((vfp = vfont_get(font1)) == VFONT_NULL) {
	fprintf(stderr, "fblabel:  Can't get font \"%s\"\n",
		font1 == NULL ? "(null)" : font1);
	bu_exit(1, NULL);
    }

    do_line(vfp, textstring);

    fb_close(fbp);
    vfont_free(vfp);

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

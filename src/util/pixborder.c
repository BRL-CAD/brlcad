/*                     P I X B O R D E R . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2014 United States Government as represented by
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
/** @file util/pixborder.c
 *
 * Add a 1-pixel-wide border to regions of a specified color.
 * (Do not confuse this with drawing border around the image.)
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>

#include "vmath.h"
#include "bu/color.h"
#include "bu/getopt.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bn.h"
#include "fb.h"


#define ACHROMATIC -1.0
#define HUE 0
#define SAT 1
#define VAL 2

static char *file_name;
static FILE *infp;

static int fileinput = 0;	/* Is input a file (not stdin)? */
static int autosize = 0;	/* Try to guess input dimensions? */
static int tol_using_rgb = 1; /* Compare via RGB, not HSV? */

static size_t file_width = 512;
static size_t file_height = 512;

static ssize_t left_edge = -1;
static ssize_t right_edge = -1;
static ssize_t bottom_edge = -1;
static ssize_t top_edge = -1;

#define COLORS_NEITHER 0
#define COLORS_INTERIOR 1
#define COLORS_EXTERIOR 2
#define COLORS_BOTH (COLORS_INTERIOR | COLORS_EXTERIOR)
static int colors_specified = COLORS_NEITHER;

static unsigned char border_rgb[3];
static unsigned char exterior_rgb[3];
static unsigned char interior_rgb[3];
static unsigned char rgb_tol[3];

fastf_t border_hsv[3];
fastf_t exterior_hsv[3];
fastf_t interior_hsv[3];
fastf_t hsv_tol[3];

#define OPT_STRING "ab:e:i:n:s:t:w:x:y:B:E:I:T:X:Y:h?"

static char usage[] = "\
Usage: pixborder [-b 'R G B'] [-e 'R G B'] [-i 'R G B'] [-t 'R G B']\n\
		 [-B 'H S V'] [-E 'H S V'] [-I 'H S V'] [-T 'H S V']\n\
		 [-x left_edge]  [-y bottom_edge]\n\
		 [-X right_edge] [-Y top_edge]\n\
		 [-a] [-s squaresize] [-w file_width] [-n file_height]\n\
		 [file.pix]\n";

/*
 * Read in an HSV triple.
 */
static int read_hsv (fastf_t *hsvp, char *buf)
{
    double tmp[3];

    if (sscanf(buf, "%lf %lf %lf", tmp, tmp + 1, tmp + 2) != 3)
	return 0;
    if ((tmp[HUE] < 0.0) || (tmp[HUE] > 360.0)
	|| (tmp[SAT] < 0.0) || (tmp[SAT] > 1.0)
	|| (tmp[VAL] < 0.0) || (tmp[VAL] > 1.0))
	return 0;
    if (ZERO(tmp[SAT]))
	tmp[HUE] = ACHROMATIC;
    VMOVE(hsvp, tmp);
    return 1;
}


static int
read_row(unsigned char *rp, size_t width, FILE *fp)
{
    size_t ret = fread(rp + 3, 3, width, fp);
    if (ret != width)
	return 0;
    *(rp + RED) = *(rp + GRN) = *(rp + BLU) = 0;
    *(rp + 3 * (width + 1) + RED) =
	*(rp + 3 * (width + 1) + GRN) =
	*(rp + 3 * (width + 1) + BLU) = 0;
    return 1;
}


/*
 * Convert between RGB and HSV color models
 *
 * R, G, and B are in {0, 1, ..., 255},
 * H is in [0.0, 360.0), and S and V are in [0.0, 1.0],
 * unless S = 0.0, in which case H = ACHROMATIC.
 *
 * These two routines are adapted from
 * pp. 592-3 of J.D. Foley, A. van Dam, S.K. Feiner, and J.F. Hughes,
 * _Computer graphics: principles and practice_, 2nd ed., Addison-Wesley,
 * Reading, MA, 1990.
 */

static void rgb_to_hsv (unsigned char *rgb, fastf_t *hsv)
{
    fastf_t red, grn, blu;
    fastf_t *hue = &hsv[HUE];
    fastf_t *sat = &hsv[SAT];
    fastf_t *val = &hsv[VAL];
    fastf_t max, min;
    fastf_t delta;

    /*
     * Compute value
     */
    max = min = red = (fastf_t)rgb[RED] / 255.0;

    grn = (fastf_t)rgb[GRN] / 255.0;
    if (grn < min)
	min = grn;
    else if (grn > max)
	max = grn;

    blu = (fastf_t)rgb[BLU] / 255.0;
    if (blu < min)
	min = blu;
    else if (blu > max)
	max = blu;

    *val = max;

    /*
     * Compute saturation
     */
    delta = max - min;
    if (max > 0.0)
	*sat = delta / max;
    else
	*sat = 0.0;

    /*
     * Compute hue
     */
    if (ZERO(*sat))
	*hue = ACHROMATIC;
    else {
	if (ZERO(red - max)) /* red == max */
	    *hue = (grn - blu) / delta;
	else if (ZERO(grn - max)) /* grn == max */
	    *hue = 2.0 + (blu - red) / delta;
	else if (ZERO(blu - max)) /* blu == max */
	    *hue = 4.0 + (red - grn) / delta;

	/*
	 * Convert hue to degrees
	 */
	*hue *= 60.0;
	if (*hue < 0.0)
	    *hue += 360.0;
    }
}


int hsv_to_rgb (fastf_t *hsv, unsigned char *rgb)
{
    fastf_t float_rgb[3];
    fastf_t hue, sat, val;
    fastf_t hue_frac;
    fastf_t p, q, t;
    int hue_int;

    VSETALL(float_rgb, 0.0);

    hue = hsv[HUE];
    sat = hsv[SAT];
    val = hsv[VAL];

    if (ZERO(sat)) {
	if (ZERO(hue - ACHROMATIC)) { /* hue == ACHROMATIC */
	    VSETALL(float_rgb, val);
	} else {
	    (void) fprintf(stderr, "Illegal HSV (%g, %g, %g)\n",
			   V3ARGS(hsv));
	    return 0;
	}
    } else {
	if (ZERO(hue - 360.0)) /* hue == 360.0 */
	    hue = 0.0;
	hue /= 60.0;
	hue_int = floor((double) hue);
	hue_frac = hue - hue_int;
	p = val * (1.0 - sat);
	q = val * (1.0 - (sat * hue_frac));
	t = val * (1.0 - (sat * (1.0 - hue_frac)));
	switch (hue_int) {
	    case 0: VSET(float_rgb, val, t, p); break;
	    case 1: VSET(float_rgb, q, val, p); break;
	    case 2: VSET(float_rgb, p, val, t); break;
	    case 3: VSET(float_rgb, p, q, val); break;
	    case 4: VSET(float_rgb, t, p, val); break;
	    case 5: VSET(float_rgb, val, p, q); break;
	    default:
		(void) fprintf(stderr, "%s:%d: This shouldn't happen\n",
			       __FILE__, __LINE__);
		bu_exit (1, NULL);
	}
    }

    rgb[RED] = float_rgb[RED] * 255;
    rgb[GRN] = float_rgb[GRN] * 255;
    rgb[BLU] = float_rgb[BLU] * 255;

    return 1;
}


static int same_rgb (unsigned char *color1, unsigned char *color2)
{
    return ((abs(color1[RED] - color2[RED]) <= (int) rgb_tol[RED]) &&
	    (abs(color1[GRN] - color2[GRN]) <= (int) rgb_tol[GRN]) &&
	    (abs(color1[BLU] - color2[BLU]) <= (int) rgb_tol[BLU]));
}


static int same_hsv (fastf_t *color1, fastf_t *color2)
{
    return ((fabs(color1[HUE] - color2[HUE]) <= hsv_tol[HUE]) &&
	    (fabs(color1[SAT] - color2[SAT]) <= hsv_tol[SAT]) &&
	    (fabs(color1[VAL] - color2[VAL]) <= hsv_tol[VAL]));
}


static int is_interior (unsigned char *pix_rgb)
{
    if (tol_using_rgb)
	return ((colors_specified == COLORS_EXTERIOR)	?
		(! same_rgb(pix_rgb, exterior_rgb))	:
		same_rgb(pix_rgb, interior_rgb));
    else {
	vect_t pix_hsv = VINIT_ZERO;

	rgb_to_hsv(pix_rgb, pix_hsv);
	return ((colors_specified == COLORS_EXTERIOR)	?
		(! same_hsv(pix_hsv, exterior_hsv))	:
		same_hsv(pix_hsv, interior_hsv));
    }
}


static int is_exterior (unsigned char *pix_rgb)
{
    if (tol_using_rgb)
	return ((colors_specified == COLORS_INTERIOR)	?
		(! same_rgb(pix_rgb, interior_rgb))	:
		same_rgb(pix_rgb, exterior_rgb));
    else {
	vect_t pix_hsv = VINIT_ZERO;

	rgb_to_hsv(pix_rgb, pix_hsv);
	return ((colors_specified == COLORS_INTERIOR)	?
		(! same_hsv(pix_hsv, interior_hsv))	:
		same_hsv(pix_hsv, exterior_hsv));
    }
}


static int is_border (unsigned char *prp, unsigned char *trp, unsigned char *nrp, int col_nm)

/* Previous row */
/* Current (this) row */
/* Next row */
/* Current column */

{
    unsigned char pix_rgb[3];

    VMOVE(pix_rgb, trp + (col_nm + 1) * 3);

    /*
     * Ensure that this pixel is in a region of interest
     */
    if (! is_interior(pix_rgb))
	return 0;

    /*
     * Check its left and right neighbors
     */
    VMOVE(pix_rgb, trp + (col_nm + 0) * 3);
    if (is_exterior(pix_rgb))
	return 1;
    VMOVE(pix_rgb, trp + (col_nm + 2) * 3);
    if (is_exterior(pix_rgb))
	return 1;

    /*
     * Check its upper and lower neighbors
     */
    VMOVE(pix_rgb, nrp + (col_nm + 1) * 3);
    if (is_exterior(pix_rgb))
	return 1;
    VMOVE(pix_rgb, prp + (col_nm + 1) * 3);
    if (is_exterior(pix_rgb))
	return 1;

    /*
     * All four of its neighbors are also in the region
     */
    return 0;
}


static int
get_args (int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, OPT_STRING)) != -1) {
	switch (c) {
	    case 'a':
		autosize = 1;
		break;
	    case 'b':
		if (! bu_str_to_rgb(bu_optarg, border_rgb)) {
		    (void) fprintf(stderr, "Illegal color: '%s'\n", bu_optarg);
		    return 0;
		}
		break;
	    case 'e':
		if (! bu_str_to_rgb(bu_optarg, exterior_rgb)) {
		    (void) fprintf(stderr, "Illegal color: '%s'\n", bu_optarg);
		    return 0;
		}
		rgb_to_hsv(exterior_rgb, exterior_hsv);
		colors_specified |= COLORS_EXTERIOR;
		break;
	    case 'i':
		if (! bu_str_to_rgb(bu_optarg, interior_rgb)) {
		    (void) fprintf(stderr, "Illegal color: '%s'\n", bu_optarg);
		    return 0;
		}
		rgb_to_hsv(interior_rgb, interior_hsv);
		colors_specified |= COLORS_INTERIOR;
		break;
	    case 'n':
		file_height = atoi(bu_optarg);
		autosize = 0;
		break;
	    case 's':
		file_height = file_width = atol(bu_optarg);
		autosize = 0;
		break;
	    case 't':
		if (! bu_str_to_rgb(bu_optarg, rgb_tol)) {
		    (void) fprintf(stderr, "Illegal color: '%s'\n", bu_optarg);
		    return 0;
		}
		tol_using_rgb = 1;
		break;
	    case 'w':
		file_width = atol(bu_optarg);
		autosize = 0;
		break;
	    case 'x':
		left_edge = atol(bu_optarg);
		break;
	    case 'y':
		bottom_edge = atol(bu_optarg);
		break;
	    case 'B':
		if (! read_hsv(border_hsv, bu_optarg)) {
		    (void) fprintf(stderr, "Illegal color: '%s'\n", bu_optarg);
		    return 0;
		}
		hsv_to_rgb(border_hsv, border_rgb);
		break;
	    case 'E':
		if (! read_hsv(exterior_hsv, bu_optarg)) {
		    (void) fprintf(stderr, "Illegal color: '%s'\n", bu_optarg);
		    return 0;
		}
		hsv_to_rgb(exterior_hsv, exterior_rgb);
		colors_specified |= COLORS_EXTERIOR;
		break;
	    case 'I':
		if (! read_hsv(interior_hsv, bu_optarg)) {
		    (void) fprintf(stderr, "Illegal color: '%s'\n", bu_optarg);
		    return 0;
		}
		hsv_to_rgb(interior_hsv, interior_rgb);
		colors_specified |= COLORS_INTERIOR;
		break;
	    case 'T':
		if (! read_hsv(hsv_tol, bu_optarg)) {
		    (void) fprintf(stderr, "Illegal color: '%s'\n", bu_optarg);
		    return 0;
		}
		tol_using_rgb = 0;
		break;
	    case 'X':
		right_edge = atoi(bu_optarg);
		break;
	    case 'Y':
		top_edge = atoi(bu_optarg);
		break;
	    default:  /* 'h' '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return 0;
	file_name = "stdin";
	infp = stdin;
    } else {
	file_name = argv[bu_optind];
	infp = fopen(file_name, "r");
	if (infp == NULL) {
	    perror(file_name);
	    (void) fprintf(stderr, "Cannot open file '%s'\n", file_name);
	    return 0;
	}
	++fileinput;
    }

    if (argc > ++bu_optind)
	(void) fprintf(stderr, "pixborder: excess argument(s) ignored\n");

    if (left_edge == -1)
	left_edge = 0;
    if (right_edge == -1)
	right_edge = file_width - 1;
    if (bottom_edge == -1)
	bottom_edge = 0;
    if (top_edge == -1)
	top_edge = file_height - 1;
    return 1;
}


int
main (int argc, char **argv)
{
    char *outbuf;
    unsigned char *inrow[3];
    size_t i;
    ssize_t next_row;
    ssize_t prev_row;
    ssize_t this_row;
    size_t col_nm;
    size_t row_nm;

    VSETALL(border_rgb,     1);
    rgb_to_hsv(border_rgb, border_hsv);
    VSETALL(exterior_rgb,   1);
    rgb_to_hsv(exterior_rgb, exterior_hsv);
    VSETALL(interior_rgb, 255);
    rgb_to_hsv(interior_rgb, interior_hsv);
    VSETALL(rgb_tol,        0);

    if (!get_args(argc, argv)) {
	(void) fputs(usage, stderr);
	bu_exit (1, NULL);
    }

    /*
     * Autosize the input if appropriate
     */
    if (fileinput && autosize) {
	size_t w, h;

	if (fb_common_file_size(&w, &h, file_name, 3)) {
	    file_width = (long)w;
	    file_height = (long)h;
	} else
	    (void) fprintf(stderr, "pixborder: unable to autosize\n");
    }

    /*
     * Allocate a 1-scanline output buffer
     * and a circular input buffer of 3 scanlines
     */
    outbuf = (char *)bu_malloc(3*file_width, "outbuf");
    for (i = 0; i < 3; ++i)
	inrow[i] = (unsigned char *)bu_malloc(3*(file_width + 2), "inrow[i]");
    prev_row = 0;
    this_row = 1;
    next_row = 2;

    /*
     * Initialize previous-row buffer
     */
    for (i = 0; i < 3 * (file_width + 2); ++i)
	*(inrow[prev_row] + i) = 0;

    /*
     * Initialize current- and next-row buffers
     */
    if ((! read_row(inrow[this_row], file_width, infp))
	|| (! read_row(inrow[next_row], file_width, infp)))
    {
	perror(file_name);
	fprintf(stderr, "pixborder:  fread() error\n");
	bu_exit (1, NULL);
    }

    /*
     * Do the filtering
     */
    for (row_nm = 0; row_nm < file_height; ++row_nm) {
	size_t ret;

	/*
	 * Fill the output-scanline buffer
	 */
	if (((ssize_t)row_nm < bottom_edge) || ((ssize_t)row_nm > top_edge)) {
	    ret = fwrite(inrow[this_row] + 3, 3, file_width, stdout);
	    if (ret != file_width) {
		perror("stdout");
		bu_exit (2, NULL);
	    }
	} else {
	    for (col_nm = 0; col_nm < file_width; ++col_nm) {
		unsigned char *color_ptr;

		if (((ssize_t)col_nm >= left_edge) && ((ssize_t)col_nm <= right_edge)
		    && is_border(inrow[prev_row], inrow[this_row],
				 inrow[next_row], col_nm))
		    color_ptr = border_rgb;
		else
		    color_ptr = inrow[this_row] + (col_nm + 1) * 3;

		VMOVE(outbuf + col_nm * 3, color_ptr);
	    }

	    /*
	     * Write the output scanline
	     */
	    ret = fwrite(outbuf, 3, file_width, stdout);
	    if (ret != file_width) {
		perror("stdout");
		bu_exit (2, NULL);
	    }
	}

	/*
	 * Advance the circular input buffer
	 */
	prev_row = this_row;
	this_row = next_row;
	i = next_row;
	next_row = ++i % 3;

	/*
	 * Grab the next input scanline
	 */
	if (row_nm < file_height - 2) {
	    if (! read_row(inrow[next_row], file_width, infp)) {
		perror(file_name);
		(void) fprintf(stderr, "pixborder:  fread() error\n");
		bu_exit (1, NULL);
	    }
	} else
	    for (i = 0; i < 3 * (file_width + 2); ++i)
		*(inrow[next_row] + i) = 0;
    }

    return 1;
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

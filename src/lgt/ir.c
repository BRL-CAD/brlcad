/*                            I R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "vmath.h"
#include "raytrace.h"
#include "fb.h"

#include "../libbu/vfont.h"

#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"
#include "./tree.h"

#define IR_DATA_WID 512
#define Avg_Fah(sum)	((sum)/(sample_sz))
#define Kelvin2Fah(f)	(9.0/5.0)*((f)-273.15) + 32.0
#define S_BINS 10
#define HUE_TOL 0.5

extern fastf_t epsilon;

static RGBpixel black = { 0, 0, 0 };
static int ir_max_index = -1;
RGBpixel *ir_table = (RGBpixel *)RGBPIXEL_NULL;
struct vfont_file font;

static void temp_To_RGB(unsigned char *rgb, int temp);

int
ir_Chk_Table(void)
{
    if (ir_table == (RGBpixel *)PIXEL_NULL) {
	get_Input(input_ln, MAX_LN, "Enter minimum temperature : ");
	if (sscanf(input_ln, "%d", &ir_min) != 1) {
	    prnt_Scroll("Could not read minimum temperature.");
	    return 0;
	}
	get_Input(input_ln, MAX_LN, "Enter maximum temperature : ");
	if (sscanf(input_ln, "%d", &ir_max) != 1) {
	    prnt_Scroll("Could not read maximum temperature.");
	    return 0;
	}
	if (! init_Temp_To_RGB())
	    return 0;
    }
    return 1;
}


static int
adjust_Page(int y)
{
    int scans_per_page = fb_get_pagebuffer_pixel_size(fbiop)/fb_getwidth(fbiop);
    int newy = y - (y % scans_per_page);
    return newy;
}


#define D_XPOS (x-xmin)
void
display_Temps(int xmin, int ymin)
{
    int x, y;
    int interval = ((grid_sz*3+2)/4)/(S_BINS+2);
    int xmax = xmin+(interval*S_BINS);
    int ymax;
    fastf_t xrange = xmax - xmin;

    /* Avoid page thrashing of frame buffer.			*/
    ymin = adjust_Page(ymin);
    ymax = ymin + interval;

    /* Initialize ir_table if necessary.				*/
    if (! ir_Chk_Table())
	return;

    for (y = ymin; y <= ymax; y++) {
	x = xmin;
	if (fb_seek(fbiop, x, y) == -1) {
	    bu_log("\"%s\"(%d) fb_seek to pixel <%d, %d> failed.\n",
		   __FILE__, __LINE__, x, y
		);
	    return;
	}
	for (; x <= xmax + interval; x++) {
	    fastf_t percent;
	    static RGBpixel *pixel;
	    percent = D_XPOS / xrange;
	    if (D_XPOS % interval == 0) {
		int temp = AMBIENT+percent*RANGE;
		int lgtindex = temp - ir_min;
		pixel = (RGBpixel *) ir_table[(lgtindex < ir_max_index ?
					       lgtindex : ir_max_index)];
		/* this should be an &ir_table...,
		   allowed by ANSI C, but not K&R
		   compilers. */
		(void) fb_wpixel(fbiop, (unsigned char *) black);
	    } else {
		(void) fb_wpixel(fbiop, (unsigned char *) pixel);
	    }
	}
    }
    font = get_font((char *) NULL, bu_log);
    if (font.ffdes == NULL) {
	bu_log("Could not load font.\n");
	fb_flush(fbiop);
	return;
    }
    y = ymin;
    for (x = xmin; x <= xmax; x += interval) {
	char tempstr[4];
	fastf_t percent = D_XPOS / xrange;
	int temp = AMBIENT+percent*RANGE;
	int shrinkfactor = fb_getwidth(fbiop)/grid_sz;
	(void) sprintf(tempstr, "%3d", temp);
	do_line(x+2,
		y+(interval-(12/shrinkfactor))/2,
		tempstr
/*, shrinkfactor*/
	    );
    }
    fb_flush(fbiop);
    return;
}


static int
get_IR(int x, int y, int *fahp, FILE *fp)
{
    if (bu_fseek(fp, (y*IR_DATA_WID + x)*sizeof(int), 0) != 0)
	return 0;
    else
	if (fread((char *) fahp, (int) sizeof(int), 1, fp) != 1)
	    return 0;
	else
	    return 1;
}
int
read_IR(FILE *fp)
{
    int fy;
    int rx, ry;
    int min, max;
    if (fread((char *) &min, (int) sizeof(int), 1, fp) != 1
	||	fread((char *) &max, (int) sizeof(int), 1, fp) != 1
	)
    {
	bu_log("Can't read minimum and maximum temperatures.\n");
	return 0;
    } else {
	bu_log("IR data temperature range is %d to %d\n",
	       min, max
	    );
	if (ir_min == ABSOLUTE_ZERO) {
	    /* Temperature range not set.			*/
	    ir_min = min;
	    ir_max = max;
	} else {
	    /* Merge with existing range.			*/
	    V_MIN(ir_min, min);
	    V_MAX(ir_max, max);
	    bu_log("Global temperature range is %d to %d\n",
		   ir_min, ir_max
		);
	}
	(void) fflush(stdout);
    }
    if (! init_Temp_To_RGB()) {
	return 0;
    }
    for (ry = 0, fy = grid_sz-1; ; ry += ir_aperture, fy--) {
	if (fb_seek(fbiop, 0, fy) == -1) {
	    bu_log("\"%s\"(%d) fb_seek to pixel <%d, %d> failed.\n",
		   __FILE__, __LINE__, 0, fy
		);
	    return 0;
	}
	for (rx = 0; rx < IR_DATA_WID; rx += ir_aperture) {
	    int fah;
	    int sum = 0;
	    int i;
	    int lgtindex;
	    RGBpixel *pixel;
	    for (i = 0; i < ir_aperture; i++) {
		int j;
		for (j = 0; j < ir_aperture; j++) {
		    if (get_IR(rx+j, ry+i, &fah, fp))
			sum += fah < ir_min ? ir_min : fah;
		    else {
			/* EOF */
			if (ir_octree.o_temp == ABSOLUTE_ZERO)
			    ir_octree.o_temp = AMBIENT - 1;
			display_Temps(grid_sz/8, 0);
			return 1;
		    }
		}
	    }
	    fah = Avg_Fah(sum);
	    if ((lgtindex = fah-ir_min) > ir_max_index || lgtindex < 0) {
		bu_log("temperature out of range (%d)\n",
		       fah
		    );
		return 0;
	    }
	    pixel = (RGBpixel *) ir_table[lgtindex];
	    (void) fb_wpixel(fbiop, (unsigned char *)pixel);
	}
    }
}


/*
  Map temperatures to spectrum of colors.
  This routine is extracted from the "mandel" program written by
  Douglas A. Gwyn here at BRL, and has been modified slightly
  to suit the input data.
*/
static void
temp_To_RGB(unsigned char *rgb, int temp)
{
    fastf_t scale = 4.0 / RANGE;
    fastf_t t = temp;
    fastf_t hue = 4.0 - ((t < AMBIENT ? AMBIENT :
			  t > HOTTEST ? HOTTEST :
			  t) - AMBIENT) * scale;
    int h = (int) hue;	/* integral part */
    int f = (int)(256.0 * (hue - (fastf_t)h));
    /* fractional part * 256 */
    if (ZERO(t - ABSOLUTE_ZERO))
	rgb[RED] = rgb[GRN] = rgb[BLU] = 0;
    else
	switch (h) {
	    default:	/* 0 */
		rgb[RED] = 255;
		rgb[GRN] = f;
		rgb[BLU] = 0;
		break;
	    case 1:
		rgb[RED] = 255 - f;
		rgb[GRN] = 255;
		rgb[BLU] = 0;
		break;
	    case 2:
		rgb[RED] = 0;
		rgb[GRN] = 255;
		rgb[BLU] = f;
		break;
	    case 3:
		rgb[RED] = 0;
		rgb[GRN] = 255 - f;
		rgb[BLU] = 255;
		break;
	    case 4:
		rgb[RED] = f;
		rgb[GRN] = 0;
		rgb[BLU] = 255;
		break;
/* case 5:
   rgb[RED] = 255;
   rgb[GRN] = 0;
   rgb[BLU] = 255 - f;
   break;
*/
	}
/* bu_log("temp=%d rgb=(%d %d %d)\n", temp, rgb[RED], rgb[GRN], rgb[BLU]);
 */
    return;
}


/*
  Initialize pseudo-color mapping table for the current view.  This
  color assignment will vary with each set of IR data read so as to
  map the full range of data to the full spectrum of colors.  This
  means that a given color will not necessarily have the same
  temperature mapping for different views of the vehicle, but is only
  valid for display of the current view.
*/
int
init_Temp_To_RGB(void)
{
    int temp, i;
    RGBpixel rgb;
    if ((ir_aperture = fb_getwidth(fbiop)/grid_sz) < 1) {
	bu_log("Grid too large for IR application, max. is %d.\n",
	       IR_DATA_WID
	    );
	return 0;
    }
    sample_sz = pow(ir_aperture, 2);
    if (ir_table != (RGBpixel *)RGBPIXEL_NULL)
	/* Table already initialized presumably from another view,
	   since range may differ we must create a different
	   table of color assignment, so free storage and re-
	   initialize.
	*/
	free((char *) ir_table);
    ir_table = (RGBpixel *) malloc((unsigned)(sizeof(RGBpixel)*((ir_max-ir_min)+1)));
    if (ir_table == (RGBpixel *)RGBPIXEL_NULL) {
	Malloc_Bomb(sizeof(RGBpixel)*((ir_max-ir_min)+1));
	fatal_error = TRUE;
	return 0;
    }
    for (temp = ir_min, i = 0; temp <= ir_max; temp++, i++) {
	temp_To_RGB(rgb, temp);
	COPYRGB(ir_table[i], rgb);
    }
    ir_max_index = i - 1;
    return 1;
}


int
same_Hue(RGBpixel (*pixel1p), RGBpixel (*pixel2p))
{
    fastf_t rval1, gval1, bval1;
    fastf_t rval2, gval2, bval2;
    fastf_t rratio, gratio, bratio;
    if ((*pixel1p)[RED] == (*pixel2p)[RED]
	&&	(*pixel1p)[GRN] == (*pixel2p)[GRN]
	&&	(*pixel1p)[BLU] == (*pixel2p)[BLU]
	)
	return 1;
    rval1 = (*pixel1p)[RED];
    gval1 = (*pixel1p)[GRN];
    bval1 = (*pixel1p)[BLU];
    rval2 = (*pixel2p)[RED];
    gval2 = (*pixel2p)[GRN];
    bval2 = (*pixel2p)[BLU];
    if (ZERO(rval1)) {
	if (!ZERO(rval2))
	    return 0;
	else /* Both red values are zero. */
	    rratio = 0.0;
    } else
	if (ZERO(rval2))
	    return 0;
	else /* Neither red value is zero. */
	    rratio = rval1/rval2;
    if (ZERO(gval1)) {
	if (!ZERO(gval2))
	    return 0;
	else /* Both green values are zero. */
	    gratio = 0.0;
    } else
	if (ZERO(gval2))
	    return 0;
	else /* Neither green value is zero. */
	    gratio = gval1/gval2;
    if (ZERO(bval1)) {
	if (!ZERO(bval2))
	    return 0;
	else /* Both blue values are zero. */
	    bratio = 0.0;
    } else
	if (ZERO(bval2))
	    return 0;
	else /* Neither blue value is zero. */
	    bratio = bval1/bval2;
    if (ZERO(rratio)) {
	if (ZERO(gratio))
	    return 1;
	else
	    if (ZERO(bratio))
		return 1;
	    else
		if (NEAR_EQUAL(gratio, bratio, HUE_TOL))
		    return 1;
		else
		    return 0;
    } else
	if (ZERO(gratio)) {
	    if (ZERO(bratio))
		return 1;
	    else
		if (NEAR_EQUAL(bratio, rratio, HUE_TOL))
		    return 1;
		else
		    return 0;
	} else
	    if (ZERO(bratio)) {
		if (NEAR_EQUAL(rratio, gratio, HUE_TOL))
		    return 1;
		else
		    return 0;
	    } else {
		if (NEAR_EQUAL(rratio, gratio, HUE_TOL)
		    &&	NEAR_EQUAL(gratio, bratio, HUE_TOL)
		    )
		    return 1;
		else
		    return 0;
	    }
}


int
pixel_To_Temp(RGBpixel (*pixelp))
{
    RGBpixel *p;
    RGBpixel *q = (RGBpixel *) ir_table[ir_max-ir_min];
    int temp = ir_min;
    for (p = (RGBpixel *) ir_table[0]; p <= q; p++, temp++) {
	if (same_Hue(p, pixelp))
	    return temp;
    }
    /* prnt_Scroll("Pixel=(%d, %d, %d): not assigned a temperature.\n",
       (int)(*pixelp)[RED],
       (int)(*pixelp)[GRN],
       (int)(*pixelp)[BLU]
       );*/
    return ABSOLUTE_ZERO;
}


int
f_IR_Model(struct application *ap, Octree *op)
{
    fastf_t octnt_min[3], octnt_max[3];
    fastf_t delta = modl_radius / pow_Of_2(ap->a_level);
    fastf_t point[3] = VINIT_ZERO; /* Intersection point.	*/
    fastf_t norml[3] = VINIT_ZERO; /* Unit normal at point.	*/
    /* Push ray origin along ray direction to intersection point.	*/
    VJOIN1(point, ap->a_ray.r_pt, ap->a_uvec[0], ap->a_ray.r_dir);

    /* Compute octant RPP.						*/
    octnt_min[X] = op->o_points->c_point[X] - delta;
    octnt_min[Y] = op->o_points->c_point[Y] - delta;
    octnt_min[Z] = op->o_points->c_point[Z] - delta;
    octnt_max[X] = op->o_points->c_point[X] + delta;
    octnt_max[Y] = op->o_points->c_point[Y] + delta;
    octnt_max[Z] = op->o_points->c_point[Z] + delta;

    if (NEAR_EQUAL(point[X], octnt_min[X], epsilon))
	/* Intersection point lies on plane whose normal is the
	   negative X-axis.
	*/
    {
	norml[X] = -1.0;
	norml[Y] =  0.0;
	norml[Z] =  0.0;
    } else
	if (NEAR_EQUAL(point[X], octnt_max[X], epsilon))
	    /* Intersection point lies on plane whose normal is the
	       positive X-axis.
	    */
	{
	    norml[X] = 1.0;
	    norml[Y] = 0.0;
	    norml[Z] = 0.0;
	} else
	    if (NEAR_EQUAL(point[Y], octnt_min[Y], epsilon))
		/* Intersection point lies on plane whose normal is the
		   negative Y-axis.
		*/
	    {
		norml[X] =  0.0;
		norml[Y] = -1.0;
		norml[Z] =  0.0;
	    } else
		if (NEAR_EQUAL(point[Y], octnt_max[Y], epsilon))
		    /* Intersection point lies on plane whose normal is the
		       positive Y-axis.
		    */
		{
		    norml[X] = 0.0;
		    norml[Y] = 1.0;
		    norml[Z] = 0.0;
		} else
		    if (NEAR_EQUAL(point[Z], octnt_min[Z], epsilon))
			/* Intersection point lies on plane whose normal is the
			   negative Z-axis.
			*/
		    {
			norml[X] =  0.0;
			norml[Y] =  0.0;
			norml[Z] = -1.0;
		    } else
			if (NEAR_EQUAL(point[Z], octnt_max[Z], epsilon))
			    /* Intersection point lies on plane whose normal is the
			       positive Z-axis.
			    */
			{
			    norml[X] = 0.0;
			    norml[Y] = 0.0;
			    norml[Z] = 1.0;
			}

    {
	/* Factor in reflectance from "ambient" light source.	*/
	fastf_t intensity = VDOT(norml, lgts[0].dir);
	/* Calculate index into false-color table.		*/
	int lgtindex = op->o_temp - ir_min;
	if (lgtindex > ir_max_index) {
	    bu_log("Temperature (%d) above range of data.\n", op->o_temp);
	    return -1;
	}
	if (lgtindex < 0)
	    /* Un-assigned octants get colored grey.		*/
	    ap->a_color[0] = ap->a_color[1] = ap->a_color[2] = intensity;
	else {
	    /* Lookup false-coloring for octant's temperature.	*/
	    intensity *= RGB_INVERSE;
	    ap->a_color[0] = (fastf_t) (ir_table[lgtindex][RED]) * intensity;
	    ap->a_color[1] = (fastf_t) (ir_table[lgtindex][GRN]) * intensity;
	    ap->a_color[2] = (fastf_t) (ir_table[lgtindex][BLU]) * intensity;
	}
    }
    return 1;
}
int
f_IR_Backgr(struct application *ap)
{
    VMOVE(ap->a_color, bg_coefs);
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

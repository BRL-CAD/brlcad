/*                          S I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file size.c
 *
 * This file contains routines relating to image size:
 *
 * * A "guessing" function to make an educated guess at an unknown image's size.
 * * Functions to scale down an image to a lower resolution.
 *
 */

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <sys/stat.h>

#include "icv.h"
#include "icv_private.h"
#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/mime.h"
#include "bu/str.h"
#include "bu/units.h"


struct xy_size {
    const char *label;
    size_t width;  /* image width in pixels */
    size_t height; /* image height in pixels */
};

/* See https://en.wikipedia.org/wiki/Graphics_display_resolution
 * for more information - list assembled cira 2016/02/26 */
static const struct xy_size icv_display_sizes[] = {
    { "nHD *"     ,   640,  360 }, /*    230400 */
    { "qHD *"     ,   960,  540 }, /*    518400 */
    { "HD *"      ,  1280,  720 }, /*    921600 */
    { "FHD *"     ,  1920, 1080 }, /*   2073600 */
    { "WQHD *"    ,  2560, 1440 }, /*   3686400 */
    { "WQXGA+ *"  ,  3200, 1800 }, /*   5760000 */
    { "4K_UHD *"  ,  3840, 2160 }, /*   8294400 */
    { "SUHD *"    ,  5120, 2880 }, /*  14745600 */
    { "8K_FUHD *" ,  7680, 4320 }, /*  33177600 */
    { "16K_QUHD *", 15360, 8640 }, /* 132710400 */

    { "QQVGA *"   ,   160,  120 }, /*     19200 */
    { "HQVGA *"   ,   240,  160 }, /*     38400 */
    { "QVGA *"    ,   320,  240 }, /*     76800 */
    { "WQVGA *"   ,   400,  240 }, /*     96000 */
    { "HVGA *"    ,   480,  320 }, /*    153600 */
    { "VGA *"     ,   640,  480 }, /*    307200 */
    { "WVGA *"    ,   768,  480 }, /*    368640 */
    { "FWVGA *"   ,   854,  480 }, /*    409920 */
    { "SVGA *"    ,   800,  600 }, /*    480000 */
    { "WSVGA *"   ,  1024,  600 }, /*    614400 */
    { "WSVGA1 *"  ,  1024,  576 }, /*    589824 */

    { "XGA *"     ,  1024,  768 }, /*    786432 */
    { "WXGA *"    ,  1366,  768 }, /*   1049088 */
    { "WXGA1 *"   ,  1360,  768 }, /*   1044480 */
    { "WXGA2 *"   ,  1280,  800 }, /*   1024000 */
    { "XGA+ *"    ,  1152,  864 }, /*    995328 */
    { "WXGA+ *"   ,  1440,  900 }, /*   1296000 */
    { "SXGA+ *"   ,  1280, 1024 }, /*   1310720 */
    { "WSXGA+ *"  ,  1680, 1050 }, /*   1764000 */
    { "UXGA *"    ,  1600, 1200 }, /*   1920000 */
    { "WUXGA *"   ,  1920, 1200 }, /*   2304000 */

    { "QWXGA *"   ,  2048, 1152 }, /*   2359296 */
    { "QXGA *"    ,  2048, 1536 }, /*   3145728 */
    { "WQXGA *"   ,  2560, 1600 }, /*   4096000 */
    { "QSXGA *"   ,  2560, 2048 }, /*   5242880 */
    { "WQSXGA *"  ,  3200, 2048 }, /*   6553600 */
    { "QUXGA *"   ,  3200, 2400 }, /*   7680000 */
    { "WQUXGA *"  ,  3840, 2400 }, /*   9216000 */

    { "HXGA *"    ,  4096, 3072 }, /*  12582912 */
    { "WHXGA *"   ,  5120, 3200 }, /*  16384000 */
    { "HSXGA *"   ,  5120, 4096 }, /*  20971520 */
    { "WHSXGA *"  ,  6400, 4096 }, /*  26214400 */
    { "HXGA *"    ,  6400, 4800 }, /*  30720000 */
    { "WHUXGA *"  ,  7680, 4800 }, /*  36864000 */

    { NULL,   0,    0 }
};


struct paper_size {
    const char *label;
    double width_mm;  /* width in mm */
    double height_mm; /* height in mm */
    double width_in;  /* width in inches */
    double height_in; /* height in inches */
};

/* Specify the size in whatever unit the "standard"
 * definition uses, and zero the other unit. */
static const struct paper_size paper_sizes[] = {
    { "Letter",   0,    0, 8.5,  11 },
    { "Legal",    0,    0, 8.5,  14 },
    { "Ledger",   0,    0,  17,  11 },

    { "A0",     841, 1189,   0,   0 },
    { "A1",     594,  841,   0,   0 },
    { "A2",     420,  594,   0,   0 },
    { "A3",     297,  420,   0,   0 },
    { "A4",     210,  297,   0,   0 },
    { "A5",     148,  210,   0,   0 },
    { "A6",     105,  148,   0,   0 },
    { "A7",      74,  105,   0,   0 },
    { "A8",      52,   74,   0,   0 },
    { "A9",      37,   52,   0,   0 },
    { "A10",     26,   37,   0,   0 },

    { "B0",    1000, 1414,   0,   0 },
    { "B1",     707, 1000,   0,   0 },
    { "B2",     500,  707,   0,   0 },
    { "B3",     353,  500,   0,   0 },
    { "B4",     250,  353,   0,   0 },
    { "B5",     176,  250,   0,   0 },
    { "B6",     125,  176,   0,   0 },
    { "B7",      88,  125,   0,   0 },
    { "B8",      62,   88,   0,   0 },
    { "B9",      44,   62,   0,   0 },
    { "B10",     31,   44,   0,   0 },

    { "C0",     917, 1297,   0,   0 },
    { "C1",     648,  917,   0,   0 },
    { "C2",     458,  648,   0,   0 },
    { "C3",     324,  458,   0,   0 },
    { "C4",     229,  324,   0,   0 },
    { "C5",     162,  229,   0,   0 },
    { "C6",     114,  162,   0,   0 },
    { "C7",      81,  114,   0,   0 },
    { "C8",      57,   81,   0,   0 },
    { "C9",      40,   57,   0,   0 },
    { "C10",     28,   40,   0,   0 },

    { "wallet",   0,    0,   2,   3 },
    { "135mm",    0,    0,   4,   6 },
    { "4D",       0,    0,   4.5, 6 },
    { "6R",       0,    0,   6,   8 },
    { "8R",       0,    0,   8,  10 },
    { "S8R",      0,    0,   8,  12 },
    { "10R",      0,    0,  10,  12 },
    { "11R",      0,    0,  11,  14 },
    { "S11R",     0,    0,  17,  11 },

    { NULL,   0,    0,   0,   0 }
};

/* DPI settings */
static const size_t dpi_array[] = {72, 96, 150, 300, 2540, 4000, 0};

int
icv_image_size(const char *name, size_t dpi, size_t file_size, bu_mime_image_t type, size_t *width, size_t *height)
{
    /* Legacy inference discards partial pixels and defaults non-BW types
     * to three bytes per pixel. New layout queries require exact sizes. */
    size_t pixel_size = type == BU_MIME_IMAGE_BW ? 1 : 3;
    if (file_size && file_size < pixel_size)
	return 0;
    return icv_raw_size(name, dpi, file_size - file_size % pixel_size, pixel_size, width, height);
}

int
icv_named_size(const char *name, size_t dpi, size_t *width, size_t *height)
{
    return name && icv_raw_size(name, dpi, 0, 1, width, height);
}

static int
icv_size_matches(size_t pixels, size_t width, size_t height)
{
    return width && height && pixels / width == height && pixels % width == 0;
}

static int
icv_paper_dimensions(const struct paper_size *paper, size_t dpi, size_t *width, size_t *height)
{
    double inches = bu_units_conversion("inch");
    double w = (paper->width_mm > 0 ? paper->width_mm / inches : paper->width_in) * dpi;
    double h = (paper->height_mm > 0 ? paper->height_mm / inches : paper->height_in) * dpi;
    if (!isfinite(w) || !isfinite(h) || w < 1 || h < 1 ||
	w >= (double)SIZE_MAX || h >= (double)SIZE_MAX)
	return 0;
    *width = (size_t)w;
    *height = (size_t)h;
    return *width <= SIZE_MAX / *height;
}

int
icv_raw_size(const char *name, size_t dpi, size_t file_size, size_t pixel_size, size_t *width, size_t *height)
{
    const size_t default_dpi = 300;
    size_t pixels;
    if (!width || !height || !pixel_size || file_size % pixel_size)
	return 0;
    pixels = file_size / pixel_size;
    if (name) {
	for (const struct xy_size *display = icv_display_sizes; display->label; ++display) {
	    if (!BU_STR_EQUAL(display->label, name))
		continue;
	    if (file_size && !icv_size_matches(pixels, display->width, display->height))
		return 0;
	    *width = display->width;
	    *height = display->height;
	    return 1;
	}
	for (const struct paper_size *paper = paper_sizes; paper->label; ++paper) {
	    if (!BU_STR_EQUAL(paper->label, name))
		continue;
	    if (dpi || !file_size) {
		size_t w, h;
		if (!icv_paper_dimensions(paper, dpi ? dpi : default_dpi, &w, &h) ||
		    (file_size && !icv_size_matches(pixels, w, h)))
		    return 0;
		*width = w;
		*height = h;
		return 1;
	    }
	    for (size_t i = 0; dpi_array[i]; ++i) {
		size_t w, h;
		if (icv_paper_dimensions(paper, dpi_array[i], &w, &h) && icv_size_matches(pixels, w, h)) {
		    *width = w;
		    *height = h;
		    return 1;
		}
	    }
	    return 0;
	}
    }
    if (!pixels)
	return 0;
    double approximation = sqrt((double)pixels);
    size_t root = (size_t)approximation;
    /* Correct floating rounding without overflowing root*root. */
    while (root > pixels / root)
	--root;
    while (root + 1 <= pixels / (root + 1))
	++root;
    if (icv_size_matches(pixels, root, root)) {
	*width = *height = root;
	return 1;
    }
    for (const struct xy_size *display = icv_display_sizes; display->label; ++display) {
	if (icv_size_matches(pixels, display->width, display->height)) {
	    *width = display->width;
	    *height = display->height;
	    return 1;
	}
    }
    for (const struct paper_size *paper = paper_sizes; paper->label; ++paper) {
	for (size_t i = 0; dpi_array[i]; ++i) {
	    size_t w, h;
	    if (icv_paper_dimensions(paper, dpi_array[i], &w, &h) && icv_size_matches(pixels, w, h)) {
		*width = w;
		*height = h;
		return 1;
	    }
	}
    }
    return 0;
}

static int
shrink_image(icv_image_t* bif, size_t factor)
{
    double *data_p, *res_p; /**< input and output pointers */
    double *p;
    size_t facsq, py, px;
    size_t x, y, c;
    size_t widthstep =  bif->width*bif->channels;
    if (UNLIKELY(factor < 1)) {
	bu_log("Cannot shrink image to 0 factor, factor should be a positive value.");
	return -1;
    }
    if (factor > bif->width || factor > bif->height) {
	bu_log("Cannot shrink image: factor is larger than image dimensions.");
	return -1;
    }
    if (factor > (size_t)-1 / factor) {
	bu_log("Cannot shrink image: factor is too large.");
	return -1;
    }

    facsq = factor*factor;
    res_p = bif->data;
    p = (double *)bu_malloc(bif->channels*sizeof(double), "shrink_image : Pixel Values Temp Buffer");

    size_t out_w = bif->width / factor;
    size_t out_h = bif->height / factor;
    if (out_w == 0 || out_h == 0) {
	bu_free(p, "shrink_image : Pixel Values Temp Buffer");
	return -1;
    }

    for (y = 0; y < out_h * factor; y += factor)
	for (x = 0; x < out_w * factor; x += factor) {

	    for (c = 0; c < bif->channels; c++) {
		p[c]= 0;
	    }

	    for (py = 0; py < factor; py++) {
		data_p = bif->data + (y+py)*widthstep + x*bif->channels;
		for (px = 0; px < factor; px++) {
		    for (c = 0; c < bif->channels; c++) {
			p[c] += *data_p++;
		    }
		}
	    }

	    for (c = 0; c < bif->channels; c++)
		*res_p++ = p[c]/facsq;
	}

    bif->width = (int)bif->width/factor;
    bif->height = (int)bif->height/factor;
    if (icv_image_data_realloc(bif, (size_t)(bif->width*bif->height*bif->channels)*sizeof(double), "shrink_image : Reallocation") != 0) {
	bu_free(p, "shrink_image : Pixel Values Temp Buffer");
	return -1;
    }
    bu_free(p, "shrink_image : Pixel Values Temp Buffer");

    return 0;

}


static int
under_sample(icv_image_t* bif, size_t factor)
{
    double *data_p, *res_p;
    size_t x, y, widthstep;

    if (UNLIKELY(factor < 1)) {
	bu_log("Cannot shrink image to 0 factor, factor should be a positive value.");
	return -1;
    }
    if (factor > bif->width || factor > bif->height) {
	bu_log("Cannot under-sample image: factor is larger than image dimensions.");
	return -1;
    }

    widthstep = bif->width*bif->channels;
    res_p = bif->data;

    size_t out_w = bif->width / factor;
    size_t out_h = bif->height / factor;
    if (out_w == 0 || out_h == 0)
	return -1;

    for (y = 0; y < out_h * factor; y += factor) {
	data_p = bif->data + widthstep*y;
	for (x = 0; x < out_w * factor;
	     x += factor, res_p += bif->channels, data_p += factor * bif->channels)
	    VMOVEN(res_p, data_p, bif->channels);
    }

    bif->width = (int)bif->width/factor;
    bif->height = (int)bif->height/factor;
    if (icv_image_data_realloc(bif, (size_t)(bif->width*bif->height*bif->channels)*sizeof(double), "under_sample : Reallocation") != 0)
	return -1;

    return 0;
}

static int
resize_output_valid(const icv_image_t *bif, size_t out_width, size_t out_height)
{
    if (out_width == 0 || out_height == 0 || bif->width == 0 || bif->height == 0) {
	bu_log("icv_resize : input and output dimensions must be non-zero.\n");
	return 0;
    }
    if (out_width > (size_t)-1 / out_height / bif->channels / sizeof(double)) {
	bu_log("icv_resize : output dimensions excessively large, causing integer overflow.\n");
	return 0;
    }
    return 1;
}

static double
mapped_coord(size_t out_index, size_t out_size, size_t in_size)
{
    if (out_size <= 1 || in_size <= 1)
	return 0.0;
    return ((double)out_index * (double)(in_size - 1)) / (double)(out_size - 1);
}

static int
ninterp(icv_image_t* bif, size_t out_width, size_t out_height)
{
    size_t i, j;
    size_t x, y;
    size_t widthstep;
    double *in_r, *in_c; /* Pointer to row and col of input buffers */
    double *out_data, *out_p;

    if (!resize_output_valid(bif, out_width, out_height))
	return -1;

    out_p = out_data = (double *)bu_malloc(out_width*out_height*bif->channels*sizeof(double), "ninterp : out_data");

    widthstep= bif->width*bif->channels;

    for (j = 0; j < out_height; j++) {
	double yc = mapped_coord(j, out_height, bif->height);
	y = (size_t)(yc + 0.5);
	if (y >= bif->height) y = bif->height - 1;
	in_r = bif->data + y*widthstep;

	for (i = 0; i < out_width; i++) {
	    double xc = mapped_coord(i, out_width, bif->width);
	    x = (size_t)(xc + 0.5);
	    if (x >= bif->width) x = bif->width - 1;

	    in_c = in_r + x*bif->channels;

	    VMOVEN(out_p, in_c, bif->channels);
	    out_p += bif->channels;
	}
    }

    icv_image_data_free(bif, "ninterp : in_data");
    icv_image_data_set_bu(bif, out_data);

    bif->width = out_width;
    bif->height = out_height;

    return 0;

}


static int
binterp(icv_image_t *bif, size_t out_width, size_t out_height)
{
    size_t i, j;
    size_t c;
    double x, y, dx, dy, mid1, mid2;
    double *out_data, *out_p;
    double *upp_r, *low_r; /* upper and lower row */
    double *upp_c, *low_c;
    size_t widthstep;

    if (!resize_output_valid(bif, out_width, out_height))
	return -1;

    out_p = out_data = (double *)bu_malloc(out_width*out_height*bif->channels*sizeof(double), "binterp : out data");

    widthstep = bif->width*bif->channels;

    for (j = 0; j < out_height; j++) {
	double y_floor;
	y = mapped_coord(j, out_height, bif->height);
	y_floor = floor(y);
	dy = y - y_floor;

	size_t y_low = (size_t)y_floor;
	size_t y_upp = y_low + 1;
	if (y_upp >= bif->height) y_upp = bif->height - 1;

	low_r = bif->data + widthstep * y_low;
	upp_r = bif->data + widthstep * y_upp;

	for (i = 0; i < out_width; i++) {
	    double x_floor;
	    x = mapped_coord(i, out_width, bif->width);
	    x_floor = floor(x);
	    dx = x - x_floor;

	    size_t x_low = (size_t)x_floor;
	    size_t x_upp = x_low + 1;
	    if (x_upp >= bif->width) x_upp = bif->width - 1;

	    upp_c = upp_r + x_low * bif->channels;
	    low_c = low_r + x_low * bif->channels;
	    double *upp_c_next = upp_r + x_upp * bif->channels;
	    double *low_c_next = low_r + x_upp * bif->channels;

	    for (c = 0; c < bif->channels; c++) {
		mid1 = low_c[0] + dx * (low_c_next[0] - low_c[0]);
		mid2 = upp_c[0] + dx * (upp_c_next[0] - upp_c[0]);
		*out_p++ = mid1 + dy * (mid2 - mid1);
		upp_c++;
		low_c++;
		upp_c_next++;
		low_c_next++;
	    }
	}
    }
    icv_image_data_free(bif, "binterp : Input Data");
    icv_image_data_set_bu(bif, out_data);
    bif->width = out_width;
    bif->height = out_height;
    return 0;

}


int
icv_resize(icv_image_t *bif, ICV_RESIZE_METHOD method, size_t out_width, size_t out_height, size_t factor)
{
    ICV_IMAGE_VAL_INT(bif);

    switch (method) {
	case ICV_RESIZE_UNDERSAMPLE :
	    return under_sample(bif, factor);
	case ICV_RESIZE_SHRINK :
	    return shrink_image(bif, factor);
	case ICV_RESIZE_NINTERP :
	    return ninterp(bif, out_width, out_height);
	case ICV_RESIZE_BINTERP :
	    return binterp(bif, out_width, out_height);
	default :
	    bu_log("icv_resize : Invalid Option to resize");
	    return -1;
    }

}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

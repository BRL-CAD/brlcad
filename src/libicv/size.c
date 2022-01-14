/*                          S I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2022 United States Government as represented by
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

#include <stdio.h>
#include <sys/stat.h>

#include "icv.h"
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
struct xy_size icv_display_sizes[] = {
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
struct paper_size paper_sizes[] = {
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
size_t dpi_array[] = {72, 96, 150, 300, 2540, 4000, 0};

int
icv_image_size(const char *name, size_t udpi, size_t file_size, bu_mime_image_t img_type, size_t *widthp, size_t *heightp)
{
    struct xy_size *sp;
    struct paper_size *ps;
    size_t root;
    size_t npixels;
    int dpi_ind = 0;
    unsigned long i;
    int pixel_size = 3; /* Start with an assumption of 3 doubles per pixel */
    switch (img_type) {
	case BU_MIME_IMAGE_PIX:
	    pixel_size = 3;
	    break;
	case BU_MIME_IMAGE_BW:
	    pixel_size = 1;
	    break;
	default:
	    break;
    }

    npixels = (size_t)(file_size / pixel_size);

    /* If we have a name, try that first. */
    if (name) {
	/* First try the display sizes - they're not DPI dependent. */
	sp = icv_display_sizes;
	while (sp->label) {
	    if (BU_STR_EQUAL(sp->label, name)) {
		*widthp = sp->width;
		*heightp = sp->height;
		/* If we don't have a file size, just return success*/
		if (!file_size) return 1;
		/* If we *do* have a size, validate that the image dimensions match the
		 * display size identified. */
		return (npixels == sp->width * sp->height) ? 1 : 0;
	    }
	    sp++;
	}
	/* Next, try the paper sizes. */
	ps = paper_sizes;
	while (ps->label) {
	    if (BU_STR_EQUAL(ps->label, name)) {
		/* For paper sizes, dimensions aren't enough by themselves.  We need a dpi.
		 * If one was supplied, use it. */
		if (udpi) {
		    *widthp = (ps->width_mm > 0) ? (size_t)(ps->width_mm * udpi * 1/bu_units_conversion("inch")) : (size_t)(ps->width_in * udpi);
		    *heightp = (ps->height_mm > 0) ? (size_t)(ps->height_mm * udpi * 1/bu_units_conversion("inch")) : (size_t)(ps->height_in * udpi);
		    /* If we don't have a file size, just return success*/
		    if (!file_size) return 1;
		    /* If we *do* have a size, validate that the image dimensions match the
		     * display size identified. */
		    return (npixels == *widthp * *heightp) ? 1 : 0;
		}
		/* If not, and if we have a file size, try to find a matching fit in the DPI array */
		if (file_size) {
		    i = 0;
		    while (dpi_array[i]) {
			size_t tw = (ps->width_mm > 0) ? (size_t)(ps->width_mm * dpi_array[i] * 1/bu_units_conversion("inch")) : (size_t)(ps->width_in * dpi_array[i]);
			size_t th = (ps->height_mm > 0) ? (size_t)(ps->height_mm * dpi_array[i] * 1/bu_units_conversion("inch")) : (size_t)(ps->height_in * dpi_array[i]);
			if (npixels == tw * th) {
			    *widthp = tw;
			    *heightp = th;
			    return 1;
			}
			i++;
		    }
		    return 0;
		}
		/* If we don't have a file size, go with 300 dpi */
		*widthp = (ps->width_mm > 0) ? (size_t)(ps->width_mm * 300 * 1/bu_units_conversion("inch")) : (size_t)(ps->width_in * 300);
		*heightp = (ps->height_mm > 0) ? (size_t)(ps->height_mm * 300 * 1/bu_units_conversion("inch")) : (size_t)(ps->height_in * 300);
		return 1;
	    }
	    ps++;
	}
    }

    /* If we don't have a name or none of the names matched, try the image dimensions if we have them. */
    if (npixels <= 0) return  0;

    /* If the size is a perfect square, then use that. */
    root = (size_t)(sqrt((double)npixels)+0.999);
    if (root*root == npixels) {
	*widthp = root;
	*heightp = root;
	return  1;
    }


    /* Check for "standard" display sized images */
    sp = icv_display_sizes;
    while (sp->width != 0) {
	if (npixels == sp->width * sp->height) {
	    *widthp = sp->width;
	    *heightp = sp->height;
	    return 1;
	}
	sp++;
    }

    /* Try paper sizes in various DPI settings */
    ps = paper_sizes;
    while (ps->label) {
	dpi_ind = 0;
	while (dpi_array[dpi_ind]) {
	    size_t tw = (ps->width_mm > 0) ? (size_t)(ps->width_mm * dpi_array[dpi_ind] * 1/bu_units_conversion("inch")) : (size_t)(ps->width_in * dpi_array[dpi_ind]);
	    size_t th = (ps->height_mm > 0) ? (size_t)(ps->height_mm * dpi_array[dpi_ind] * 1/bu_units_conversion("inch")) : (size_t)(ps->height_in * dpi_array[dpi_ind]);
	    if (npixels == tw * th) {
		*widthp = tw;
		*heightp = th;
		return 1;
	    }
	    dpi_ind++;
	}
	ps++;
    }

    /* Nope, we are clueless. */
    return      0;
}

HIDDEN int
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

    facsq = factor*factor;
    res_p = bif->data;
    p = (double *)bu_malloc(bif->channels*sizeof(double), "shrink_image : Pixel Values Temp Buffer");

    for (y = 0; y < bif->height; y += factor)
	for (x = 0; x < bif->width; x += factor) {

	    for (c = 0; c < bif->channels; c++) {
		p[c]= 0;
	    }

	    for (py = 0; py < factor; py++) {
		data_p = bif->data + (y+py)*widthstep;
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
    bif->data = (double *)bu_realloc(bif->data, (size_t)(bif->width*bif->height*bif->channels)*sizeof(double), "shrink_image : Reallocation");

    return 0;

}


HIDDEN int
under_sample(icv_image_t* bif, size_t factor)
{
    double *data_p, *res_p;
    size_t x, y, widthstep;

    if (UNLIKELY(factor < 1)) {
	bu_log("Cannot shrink image to 0 factor, factor should be a positive value.");
	return -1;
    }

    widthstep = bif->width*bif->channels;
    res_p = bif->data;

    for (y = 0; y < bif->height; y += factor) {
	data_p = bif->data + widthstep*y;
	for (x = 0; x < bif->width;
	     x += factor, res_p += bif->channels, data_p += factor * bif->channels)
	    VMOVEN(res_p, data_p, bif->channels);
    }

    bif->width = (int)bif->width/factor;
    bif->height = (int)bif->height/factor;
    bif->data = (double *)bu_realloc(bif->data, (size_t)(bif->width*bif->height*bif->channels)*sizeof(double), "under_sample : Reallocation");

    return 0;
}


HIDDEN int
ninterp(icv_image_t* bif, size_t out_width, size_t out_height)
{
    double xstep, ystep;
    size_t i, j;
    size_t x, y;
    size_t widthstep;
    double *in_r, *in_c; /* Pointer to row and col of input buffers */
    double *out_data, *out_p;
    xstep = (double)(bif->width-1) / (double)(out_width) - 1.0e-06;
    ystep = (double)(bif->height-1) / (double)(out_height) - 1.0e-06;

    if ((xstep < 1.0 && ystep > 1.0) || (xstep > 1.0 && ystep < 1.0)) {
	bu_log("Operation unsupported.  Cannot stretch one dimension while compressing the other.\n");
	return -1;
    }

    out_p = out_data = (double *)bu_malloc(out_width*out_height*bif->channels*sizeof(double), "ninterp : out_data");

    widthstep= bif->width*bif->channels;

    for (j = 0; j < out_height; j++) {
	y = (int)(j*ystep);
	in_r = bif->data + y*widthstep;

	for (i = 0; i < out_width; i++) {
	    x = (int)(i*xstep);

	    in_c = in_r + x*bif->channels;

	    VMOVEN(out_p, in_c, bif->channels);
	    out_p += bif->channels;
	}
    }

    bu_free(bif->data, "ninterp : in_data");

    bif->data = out_data;

    bif->width = out_width;
    bif->height = out_height;

    return 0;

}


HIDDEN int
binterp(icv_image_t *bif, size_t out_width, size_t out_height)
{
    size_t i, j;
    size_t c;
    double x, y, dx, dy, mid1, mid2;
    double xstep, ystep;
    double *out_data, *out_p;
    double *upp_r, *low_r; /* upper and lower row */
    double *upp_c, *low_c;
    size_t widthstep;

    xstep = (double)(bif->width - 1) / (double)out_width - 1.0e-6;
    ystep = (double)(bif->height -1) / (double)out_height - 1.0e-6;

    if ((xstep < 1.0 && ystep > 1.0) || (xstep > 1.0 && ystep < 1.0)) {
	bu_log("Operation unsupported.  Cannot stretch one dimension while compressing the other.\n");
	return -1;
    }

    out_p = out_data = (double *)bu_malloc(out_width*out_height*bif->channels*sizeof(double), "binterp : out data");

    widthstep = bif->width*bif->channels;

    for (j = 0; j < out_height; j++) {
	y = j*ystep;
	dy = y - (int)y;

	low_r = bif->data + widthstep* (int)y;
	upp_r = bif->data + widthstep* (int)(y+1);

	for (i = 0; i < out_width; i++) {
	    x = i*xstep;
	    dx = x - (int)x;

	    upp_c = upp_r + (int)x*bif->channels;
	    low_c = low_r + (int)x*bif->channels;

	    for (c = 0; c < bif->channels; c++) {
		mid1 = low_c[0] + dx * ((double)low_c[bif->channels] - (double)low_c[0]);
		mid2 = upp_c[0] + dx * ((double)upp_c[bif->channels] - (double)upp_c[0]);
		*out_p = mid1 + dy * (mid2 - mid1);

		out_p++;
		upp_c++;
		low_c++;
	    }
	}
    }
    bu_free(bif->data, "binterp : Input Data");
    bif->data = out_data;
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

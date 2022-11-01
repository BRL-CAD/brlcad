/*                           P P M . C
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
/** @file ppm.c
 *
 * This file contains reading and writing routines for ppm format.
 *
 */

#include "common.h"
#include <string.h>
#include "ppm.h"

#include "bu/log.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "icv_private.h"

int
ppm_write(icv_image_t *bif, const char *filename)
{
    FILE *fp;
    pixel *pixelrow;
    int p, q = 0;
    int rows = (int)bif->height;
    int cols = (int)bif->width;

    if (bif->color_space == ICV_COLOR_SPACE_GRAY) {
	icv_gray2rgb(bif);
    } else if (bif->color_space != ICV_COLOR_SPACE_RGB) {
	bu_log("ppm_write : Color Space conflict");
	return -1;
    }

    if (filename==NULL) {
	fp = stdout;
    } else if ((fp = fopen(filename, "wb")) == NULL) {
	bu_log("ERROR : Cannot open file for saving\n");
	return -1;
    }

    ppm_writeppminit(fp, cols, rows, (pixval)255, 0 );

    pixelrow = ppm_allocrow((int)bif->width);
    for (p = 0; p < rows; p++) {
	for (q = 0; q < cols; q++) {
	    int offset = ((rows - 1) * cols * 3) - (p * cols * 3);
	    pixelrow[q].r = lrint(bif->data[offset + q*3+0]*255.0);
	    pixelrow[q].g = lrint(bif->data[offset + q*3+1]*255.0);
	    pixelrow[q].b = lrint(bif->data[offset + q*3+2]*255.0);
	}
	ppm_writeppmrow(fp, pixelrow, cols, (pixval) 255, 0 );
    }

    ppm_freerow(pixelrow);

    pm_close(fp);

    return 0;
}

icv_image_t*
ppm_read(const char *filename)
{
    FILE *fp;
    icv_image_t *bif;
    int rows, cols, format;
    int row;
    pixval maxval;
    pixel **pixels;
    int p,q;

    double *data;

    if (filename == NULL) {
	fp = stdin;
	setmode(fileno(fp), O_BINARY);
    } else if ((fp = pm_openr(filename)) == NULL) {
	bu_log("ERROR: Cannot open file for reading\n");
	return NULL;
    }

    ppm_readppminit(fp, &cols, &rows, &maxval, &format);

    /* For now, only handle PPM - should handle all variations */
    if (PPM_FORMAT_TYPE(format) != PPM_TYPE) return NULL;

    pixels = ppm_allocarray(cols, rows);

    for (row = 0; row < rows; row++) {
	ppm_readppmrow(fp, pixels[row], cols, maxval, format);
    }

    pm_close(fp);



    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);

    bif->width = (size_t)cols;
    bif->height = (size_t)rows;

    data = (double *)bu_malloc(bif->width * bif->height * 3 * sizeof(double), "image data");
    for (p = 0; p < rows ; p++) {
	pixel *r = pixels[p];
	for (q = 0; q < cols; q++) {
	    int offset = ((rows - 1) * cols * 3) - (p * cols * 3);
	    data[offset + q*3+0] = (double)r[q].r/(double)255.0;
	    data[offset + q*3+1] = (double)r[q].g/(double)255.0;
	    data[offset + q*3+2] = (double)r[q].b/(double)255.0;
	}
    }

    bif->data = data;
    ppm_freearray(pixels, rows);
    bif->magic = ICV_IMAGE_MAGIC;
    bif->channels = 3;
    bif->color_space = ICV_COLOR_SPACE_RGB;
    return bif;
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

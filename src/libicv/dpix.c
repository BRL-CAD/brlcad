/*                          D P I X . C
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
/** @file dpix.c
 *
 * this contains read/write routines for dpix format.
 *
 */

#include "common.h"

#include <sys/stat.h>  /* for file mode info in WRMODE */

#include "bio.h"
#include "vmath.h"
#include "bu/log.h"
#include "icv_private.h"

#define WRMODE S_IRUSR|S_IRGRP|S_IROTH

/*
 * This function normalizes the data array of the input image.
 * This performs the normalization when the input image has data
 * entries less than 0.0 or greater than 1.0 .
 */
static icv_image_t *
icv_normalize(icv_image_t *bif)
{
    double *data;
    double max, min;
    double m, b;
    size_t size;
    size_t i;

    if (bif == NULL) {
	bu_log("icv_normalize : trying to normalize a NULL bif\n");
	return bif;
    }

    data = bif->data;

    /* Number of data elements. */
    size = bif->height*bif->width*bif->channels;

    min = INFINITY;
    max = -INFINITY;

    for (i = 0; i<size; i++) {
	V_MIN(min, *data);
	V_MAX(max, *data);
	data++;
    }
    /* strict Condition for avoiding normalization */
    if (max <= 1.0 && min >= 0.0)
	return bif;

    if (max - min < 1e-12) {
	icv_sanitize(bif);
	return bif;
    }

    data = bif->data;
    m = 1/(max-min);
    b = -min/(max-min);

    for (i =0; i<size; i++) {
	*data = m*(*data) + b;
	data++;
    }

    return bif;
}


icv_image_t *
dpix_read(FILE *fp, size_t width, size_t height)
{
    if (UNLIKELY(!fp))
	return NULL;

    icv_image_t *bif;
    size_t size;
    ssize_t ret;

    if (width == 0 || height == 0) {
	bu_log("dpix_read : Using default size.\n");
	height = 512;
	width = 512;
    }

    if (width > 0 && height > (size_t)-1 / width / 3 / sizeof(double)) {
	bu_log("dpix_read: dimensions excessively large, causing integer overflow\n");
	return NULL;
    }

    bif = icv_create(width, height, ICV_COLOR_SPACE_RGB);
    if (!bif)
	return NULL;

    /* Size in Bytes for reading. */
    size = width*height*3*sizeof(bif->data[0]);

    /* read dpix data
     * TODO - why are we using the lower level read API here? */
    int fd = fileno(fp);
    ret = read(fd, bif->data, size);

    if (ret != (ssize_t)size) {
	bu_log("dpix_read : Error while reading\n");
	icv_destroy(bif);
	return NULL;
    }

    icv_normalize(bif);

    return bif;
}


int
dpix_write(icv_image_t *bif, FILE *fp)
{
    icv_image_t *wimg;

    if (UNLIKELY(!bif))
	return BRLCAD_ERROR;
    if (UNLIKELY(!fp))
	return BRLCAD_ERROR;

    wimg = icv_image_for_write(bif, ICV_COLOR_SPACE_RGB, 3);
    if (!wimg) {
	bu_log("dpix_write : Color Space conflict");
	return BRLCAD_ERROR;
    }

    if (wimg->channels != 3) {
	bu_log("dpix_write : Channel count conflict (expected 3, got %d)", (int)wimg->channels);
	icv_destroy(wimg);
	return BRLCAD_ERROR;
    }

    size_t size = wimg->width*wimg->height*3*sizeof(wimg->data[0]);

    // TODO - why does dpix use write instead of fwrite?
    int fd = fileno(fp);
    ssize_t ret = write(fd, wimg->data, size);
    icv_destroy(wimg);

    if (ret < 0 || (size_t)ret != size) {
	bu_log("dpix_write : Short Write");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
dpix_write_mem(icv_image_t *bif, unsigned char **outbuffer, size_t *outsize)
{
    icv_image_t *wimg;

    if (UNLIKELY(!bif))
	return BRLCAD_ERROR;
    if (UNLIKELY(!outbuffer || !outsize))
	return BRLCAD_ERROR;

    wimg = icv_image_for_write(bif, ICV_COLOR_SPACE_RGB, 3);
    if (!wimg) {
	bu_log("dpix_write_mem : Color Space conflict");
	return BRLCAD_ERROR;
    }

    if (wimg->channels != 3) {
	bu_log("dpix_write_mem : Channel count conflict (expected 3, got %d)", (int)wimg->channels);
	icv_destroy(wimg);
	return BRLCAD_ERROR;
    }

    /* DPIX uses the raw double data directly */
    *outsize = (size_t)wimg->width * wimg->height * 3 * sizeof(wimg->data[0]);
    *outbuffer = (unsigned char *)bu_malloc(*outsize, "dpix_write_mem buffer");

    memcpy(*outbuffer, wimg->data, *outsize);
    icv_destroy(wimg);

    return BRLCAD_OK;
}

icv_image_t *
dpix_read_mem(const unsigned char *buffer, size_t size, size_t width, size_t height)
{
    if (UNLIKELY(!buffer || size == 0))
	return NULL;

    icv_image_t *bif;

    if (width == 0 || height == 0) {
	bu_log("dpix_read_mem: Using default size.\n");
	height = 512;
	width = 512;
    }

    if (width > 0 && height > (size_t)-1 / width / 3 / sizeof(double)) {
	bu_log("dpix_read_mem: dimensions excessively large, causing integer overflow\n");
	return NULL;
    }

    size_t expected_size = width * height * 3 * sizeof(double);
    if (size < expected_size) {
	bu_log("dpix_read_mem: Buffer size too small for dimensions\n");
	return NULL;
    }

    bif = icv_create(width, height, ICV_COLOR_SPACE_RGB);
    if (!bif)
	return NULL;

    memcpy(bif->data, buffer, expected_size);

    icv_normalize(bif);

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

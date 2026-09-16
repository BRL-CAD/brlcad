/*                           P P M . C
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
ppm_write(icv_image_t *bif, FILE *fp)
{
    icv_image_t *wimg;

    if (UNLIKELY(!bif))
	return BRLCAD_ERROR;
    if (UNLIKELY(!fp))
	return BRLCAD_ERROR;

    wimg = icv_image_for_write(bif, ICV_COLOR_SPACE_RGB, 3);
    if (!wimg) {
	bu_log("ppm_write : Color Space conflict");
	return BRLCAD_ERROR;
    }

    if (wimg->channels != 3) {
	bu_log("ppm_write : Channel count conflict (expected 3, got %d)", (int)wimg->channels);
	icv_destroy(wimg);
	return BRLCAD_ERROR;
    }

    int rows = (int)wimg->height;
    int cols = (int)wimg->width;

    ppm_writeppminit(fp, cols, rows, (pixval)255, 0);

    pixel *pixelrow = ppm_allocrow(cols);

    for (size_t p = 0; p < (size_t)rows ; p++) {
	for (size_t q = 0; q < (size_t)cols; q++) {
	    size_t offset = (((size_t)rows - 1) * (size_t)cols * 3) - (p * (size_t)cols * 3);
	    pixelrow[q].r = lrint(wimg->data[offset + q*3+0]*255.0);
	    pixelrow[q].g = lrint(wimg->data[offset + q*3+1]*255.0);
	    pixelrow[q].b = lrint(wimg->data[offset + q*3+2]*255.0);
	}
	ppm_writeppmrow(fp, pixelrow, cols, (pixval) 255, 0);
    }

    ppm_freerow((void *)pixelrow);
    icv_destroy(wimg);

    return 0;
}

int
ppm_write_mem(icv_image_t *bif, unsigned char **outbuffer, size_t *outsize)
{
    unsigned char *data;
    char header[128];
    size_t header_len;
    size_t data_len;
    size_t row;
    icv_image_t *wimg;

    if (UNLIKELY(!bif))
	return BRLCAD_ERROR;
    if (UNLIKELY(!outbuffer || !outsize))
	return BRLCAD_ERROR;

    wimg = icv_image_for_write(bif, ICV_COLOR_SPACE_RGB, 3);
    if (!wimg) {
	bu_log("ppm_write_mem : Color Space conflict");
	return BRLCAD_ERROR;
    }

    if (wimg->channels != 3) {
	bu_log("ppm_write_mem : Channel count conflict (expected 3, got %d)", (int)wimg->channels);
	icv_destroy(wimg);
	return BRLCAD_ERROR;
    }

    *outbuffer = NULL;
    *outsize = 0;

    /* Create the PPM header and calculate exact memory requirements */
    sprintf(header, "P6\n%zu %zu\n255\n", wimg->width, wimg->height);
    header_len = strlen(header);
    data_len = (size_t)wimg->width * (size_t)wimg->height * 3;

    *outsize = header_len + data_len;
    *outbuffer = (unsigned char *)bu_malloc(*outsize, "ppm_write_mem buffer");

    /* Write the header */
    memcpy(*outbuffer, header, header_len);

    data = icv_data2uchar(wimg);
    if (!data) {
	bu_free(*outbuffer, "ppm_write_mem buffer");
	*outbuffer = NULL;
	*outsize = 0;
	icv_destroy(wimg);
	return BRLCAD_ERROR;
    }

    /* Write rows top-to-bottom (PPM convention).
     * BRL-CAD images are stored bottom-up, so we flip the rows. */
    for (row = 0; row < (size_t)wimg->height; row++) {
	size_t src_row = wimg->height - 1 - row;
	unsigned char *src = data + src_row * wimg->width * 3;
	unsigned char *dst = *outbuffer + header_len + row * wimg->width * 3;

	memcpy(dst, src, wimg->width * 3);
    }

    bu_free(data, "ppm_write_mem data");
    icv_destroy(wimg);

    return BRLCAD_OK;
}

icv_image_t*
ppm_read(FILE *fp)
{
    if (UNLIKELY(!fp))
	return NULL;

    int rows, cols, format;
    pixval maxval;
    ppm_readppminit(fp, &cols, &rows, &maxval, &format);

    /* For now, only handle PPM - should handle all variations */
    if (PPM_FORMAT_TYPE(format) != PPM_TYPE)
	return NULL;

    pixel **pixels = ppm_allocarray(cols, rows);

    for (int row = 0; row < rows; row++)
	ppm_readppmrow(fp, pixels[row], cols, maxval, format);

    icv_image_t *bif;
    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);

    bif->width = (size_t)cols;
    bif->height = (size_t)rows;

    if (bif->width > 0 && bif->height > (size_t)-1 / bif->width / 3 / sizeof(double)) {
	bu_log("ppm_read: dimensions excessively large, causing integer overflow\n");
	bu_free(bif, "icv container");
	ppm_freearray(pixels, rows);
	return NULL;
    }

    double *data = (double *)bu_malloc(bif->width * bif->height * 3 * sizeof(double), "image data");
    for (size_t p = 0; p < (size_t)rows ; p++) {
	pixel *r = pixels[p];
	for (size_t q = 0; q < (size_t)cols; q++) {
	    size_t offset = (((size_t)rows - 1) * (size_t)cols * 3) - (p * (size_t)cols * 3);
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

#include <ctype.h>

static const unsigned char *
skip_ppm_comments(const unsigned char *p, const unsigned char *end)
{
    while (p < end) {
	while (p < end && isspace(*p)) p++;
	if (p < end && *p == '#') {
	    while (p < end && *p != '\n' && *p != '\r') p++;
	} else {
	    break;
	}
    }
    return p;
}

static int
parse_ppm_int(const unsigned char **p, const unsigned char *end)
{
    *p = skip_ppm_comments(*p, end);
    if (*p >= end || !isdigit(**p)) return -1;
    int val = 0;
    while (*p < end && isdigit(**p)) {
	val = val * 10 + (**p - '0');
	(*p)++;
    }
    return val;
}

icv_image_t*
ppm_read_mem(const unsigned char *buffer, size_t size)
{
    if (UNLIKELY(!buffer || size == 0))
	return NULL;

    const unsigned char *p = buffer;
    const unsigned char *end = buffer + size;

    if (p + 2 > end || p[0] != 'P' || p[1] != '6') {
	bu_log("ppm_read_mem: Invalid magic (expected P6)\n");
	return NULL;
    }
    p += 2;

    int cols = parse_ppm_int(&p, end);
    int rows = parse_ppm_int(&p, end);
    int maxval = parse_ppm_int(&p, end);

    if (cols <= 0 || rows <= 0 || maxval <= 0 || maxval > 255) {
	bu_log("ppm_read_mem: Invalid header parameters\n");
	return NULL;
    }

    if (p < end && isspace(*p)) {
	p++; // exactly one whitespace character separates maxval and data
    }

    if (cols > 0 && rows > 0 && (size_t)rows > (size_t)-1 / cols / 3 / sizeof(double)) {
	bu_log("ppm_read_mem: dimensions excessively large, causing integer overflow\n");
	return NULL;
    }

    size_t expected_size = (size_t)cols * rows * 3;
    if ((size_t)(end - p) < expected_size) {
	bu_log("ppm_read_mem: Incomplete image data\n");
	return NULL;
    }

    icv_image_t *bif;
    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);

    bif->width = (size_t)cols;
    bif->height = (size_t)rows;

    double *data = (double *)bu_malloc(bif->width * bif->height * 3 * sizeof(double), "image data");
    double norm = 1.0 / maxval;

    for (size_t row = 0; row < (size_t)rows; row++) {
	for (size_t col = 0; col < (size_t)cols; col++) {
	    size_t offset = (((size_t)rows - 1 - row) * (size_t)cols + col) * 3; // store bottom-up
	    data[offset + 0] = p[0] * norm;
	    data[offset + 1] = p[1] * norm;
	    data[offset + 2] = p[2] * norm;
	    p += 3;
	}
    }

    bif->data = data;
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

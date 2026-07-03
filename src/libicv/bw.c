/*                            B W . C
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
/** @file libicv/bw.c
 *
 * This file contains routines related to bw format.
 *
 */

#include "common.h"
#include <string.h>
#include "bu/log.h"
#include "bu/malloc.h"
#include "icv_private.h"

int
bw_write(icv_image_t *bif, FILE *fp)
{
    icv_image_t *wimg;
    int retc = BRLCAD_OK;

    if (UNLIKELY(!bif))
	return BRLCAD_ERROR;
    if (UNLIKELY(!fp))
	return BRLCAD_ERROR;

    wimg = icv_image_for_write(bif, ICV_COLOR_SPACE_GRAY, 1);
    if (!wimg) {
	bu_log("bw_write : Color Space conflict");
	return BRLCAD_ERROR;
    }

    if (wimg->channels != 1) {
	bu_log("bw_write : Channel count conflict (expected 1, got %d)", (int)wimg->channels);
	icv_destroy(wimg);
	return BRLCAD_ERROR;
    }

    unsigned char *data = icv_data2uchar(wimg);
    size_t size = wimg->height*wimg->width;
    size_t ret = fwrite(data, 1, size, fp);
    bu_free(data, "bw_write : Unsigned Char data");
    icv_destroy(wimg);

    if (ret != size) {
	bu_log("bw_write : Short Write\n");
	retc = BRLCAD_ERROR;
    }

    return retc;
}

int
bw_write_mem(icv_image_t *bif, unsigned char **outbuffer, size_t *outsize)
{
    icv_image_t *wimg;

    if (UNLIKELY(!bif))
	return BRLCAD_ERROR;
    if (UNLIKELY(!outbuffer || !outsize))
	return BRLCAD_ERROR;

    wimg = icv_image_for_write(bif, ICV_COLOR_SPACE_GRAY, 1);
    if (!wimg) {
	bu_log("bw_write_mem : Color Space conflict");
	return BRLCAD_ERROR;
    }

    if (wimg->channels != 1) {
	bu_log("bw_write_mem : Channel count conflict (expected 1, got %d)", (int)wimg->channels);
	icv_destroy(wimg);
	return BRLCAD_ERROR;
    }

    *outsize = (size_t)wimg->height * wimg->width;
    *outbuffer = (unsigned char *)bu_malloc(*outsize, "bw_write_mem buffer");

    unsigned char *data = icv_data2uchar(wimg);
    memcpy(*outbuffer, data, *outsize);
    bu_free(data, "bw_write_mem : Unsigned Char data");
    icv_destroy(wimg);

    return BRLCAD_OK;
}


icv_image_t *
bw_read(FILE *fp, size_t width, size_t height)
{
    if (UNLIKELY(!fp))
	return NULL;

    unsigned char *data = NULL;
    size_t size;

    icv_image_t *bif;
    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);

    /* buffer pixel wise */
    if (width == 0 || height == 0) {
	size_t buffsize = 1024;
	size = 0;
	data = (unsigned char *)bu_malloc(buffsize, "bw_read : unsigned char data");

	/* FIXME: this is a simple but VERY slow way to read data.
	 * Better to read in big chunks, but then one has to handle
	 * partial-reads better.  Below seems to ignore a read error.
	 */
	while (fread(&data[size], 1, 1, fp)==1) {
	    size++;
	    if (size==buffsize) {
		buffsize+=1024;
		data = (unsigned char *)bu_realloc(data, buffsize, "bw_read : increase size to accommodate data");
	    }
	}
	if (size > 0 && size < buffsize) {
	    data = (unsigned char *)bu_realloc(data, size, "bw_read : decrease size in overbuffered");
	}

	bif->height = 1;
	bif->width = (int) size;
    } else { /* buffer frame wise */
	if (width > 0 && height > (size_t)-1 / width) {
	    bu_log("bw_read: dimensions excessively large, causing integer overflow\n");
	    bu_free(bif, "icv_structure");
	    return NULL;
	}
	size = height*width;
	data = (unsigned char *)bu_malloc(size, "bw_read : unsigned char data");

	size_t ret = fread(data, 1, size, fp);
	if (ret != size) {
	    bu_log("bw_read: Error Occurred while Reading\n");
	    bu_free(data, "icv_image data");
	    bu_free(bif, "icv_structure");
	    return NULL;
	}

	bif->height = height;
	bif->width = width;
    }

    if (size)
	bif->data = icv_uchar2double(data, size);
    else {
	/* zero sized image */
	bu_free(bif, "icv container");
	bu_free(data, "unsigned char data");
	return NULL;
    }

    bu_free(data, "bw_read : unsigned char data");

    bif->magic = ICV_IMAGE_MAGIC;
    bif->channels = 1;
    bif->color_space = ICV_COLOR_SPACE_GRAY;
    bif->gamma_corr = 0.0;

    return bif;
}

icv_image_t *
bw_read_mem(const unsigned char *buffer, size_t size, size_t width, size_t height)
{
    if (UNLIKELY(!buffer || size == 0))
	return NULL;

    icv_image_t *bif;
    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);

    if (width == 0 || height == 0) {
	bif->height = 1;
	bif->width = (int)size;
    } else {
	if (width > 0 && height > (size_t)-1 / width) {
	    bu_log("bw_read_mem: dimensions excessively large, causing integer overflow\n");
	    bu_free(bif, "icv_structure");
	    return NULL;
	}
	if (size < width * height) {
	    bu_log("bw_read_mem: provided size is smaller than width*height\n");
	    bu_free(bif, "icv_structure");
	    return NULL;
	}
	bif->height = height;
	bif->width = width;
	size = width * height;
    }

    bif->data = icv_uchar2double((unsigned char *)buffer, size);
    bif->magic = ICV_IMAGE_MAGIC;
    bif->channels = 1;
    bif->color_space = ICV_COLOR_SPACE_GRAY;
    bif->gamma_corr = 0.0;

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

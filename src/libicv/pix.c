/*                           P I X . C
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
/** @file pix.c
 *
 * Contains routines related to pix format.
 *
 */
#include "common.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "icv_private.h"

int
pix_write(icv_image_t *bif, FILE *fp)
{
    icv_image_t *wimg;

    if (UNLIKELY(!bif))
	return BRLCAD_ERROR;
    if (UNLIKELY(!fp))
	return BRLCAD_ERROR;

    wimg = icv_image_for_write(bif, ICV_COLOR_SPACE_RGB, 3);
    if (!wimg) {
	bu_log("pix_write : Color Space conflict");
	return BRLCAD_ERROR;
    }

    if (wimg->channels != 3) {
	bu_log("pix_write : Channel count conflict (expected 3, got %d)", (int)wimg->channels);
	icv_destroy(wimg);
	return BRLCAD_ERROR;
    }

    unsigned char *data = icv_data2uchar(wimg);
    size_t size = (size_t) wimg->width*wimg->height*3;
    size_t ret = fwrite(data, 1, size, fp);
    bu_free(data, "pix_write : Unsigned Char data");
    icv_destroy(wimg);

    if (ret != size) {
	bu_log("pix_write : Short Write");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
pix_write_mem(icv_image_t *bif, unsigned char **outbuffer, size_t *outsize)
{
    icv_image_t *wimg;

    if (UNLIKELY(!bif))
	return BRLCAD_ERROR;
    if (UNLIKELY(!outbuffer || !outsize))
	return BRLCAD_ERROR;

    wimg = icv_image_for_write(bif, ICV_COLOR_SPACE_RGB, 3);
    if (!wimg) {
	bu_log("pix_write_mem : Color Space conflict");
	return BRLCAD_ERROR;
    }

    if (wimg->channels != 3) {
	bu_log("pix_write_mem : Channel count conflict (expected 3, got %d)", (int)wimg->channels);
	icv_destroy(wimg);
	return BRLCAD_ERROR;
    }

    *outsize = (size_t)wimg->width * wimg->height * 3;
    *outbuffer = (unsigned char *)bu_malloc(*outsize, "pix_write_mem buffer");

    unsigned char *data = icv_data2uchar(wimg);
    memcpy(*outbuffer, data, *outsize);
    bu_free(data, "pix_write_mem : Unsigned Char data");
    icv_destroy(wimg);

    return BRLCAD_OK;
}

icv_image_t *
pix_read(FILE *fp, size_t width, size_t height)
{
    if (UNLIKELY(!fp))
	return NULL;

    unsigned char *data = 0;
    size_t size;
    size_t buffsize=1024*3;

    icv_image_t *bif;
    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);
    /* buffer pixel wise */
    if (width == 0 || height == 0) {
	size = 0;
	data = (unsigned char *)bu_malloc(buffsize, "pix_read : unsigned char data");

	/* FIXME: this is a simple but VERY slow way to read data.
	 * Better to read in big chunks, but then one has to handle
	 * partial-reads better.  Below seems to ignore a read error.
	 */
	while (fread(&data[size], 1, 3, fp)==3) {
	    size+=3;
	    if (size==buffsize) {
		buffsize+=1024*3;
		data = (unsigned char *)bu_realloc(data, buffsize, "pix_read : increase size to accommodate data");
	    }
	}
	if (size > 0 && size < buffsize) {
	    data = (unsigned char *)bu_realloc(data, size, "pix_read : decrease size in overbuffered");
	}
	bif->height = 1;
	bif->width = (int) size/3;
    } else { /* buffer frame wise */
	if (width > 0 && height > (size_t)-1 / width / 3) {
	    bu_log("pix_read: dimensions excessively large, causing integer overflow\n");
	    bu_free(bif, "icv_structure");
	    return NULL;
	}
	size = (size_t) height*width*3;
	data = (unsigned char *)bu_malloc(size, "pix_read : unsigned char data");
	size_t ret = fread(data, 1, size, fp);
	if (ret != size) {
	    bu_log("pix_read: Error Occurred while Reading\n");
	    bu_free(data, "icv_image data");
	    bu_free(bif, "icv_structure");
	    return NULL;
	}
	bif->height = height;
	bif->width = width;
    }
    if (size) {
	bif->data = icv_uchar2double(data, size);
    } else {
	/* zero sized image */
	bu_free(bif, "icv container");
	bu_free(data, "unsigned char data");
	return NULL;
    }
    bu_free(data, "pix_read : unsigned char data");
    bif->magic = ICV_IMAGE_MAGIC;
    bif->channels = 3;
    bif->color_space = ICV_COLOR_SPACE_RGB;
    return bif;
}

icv_image_t *
pix_read_mem(const unsigned char *buffer, size_t size, size_t width, size_t height)
{
    if (UNLIKELY(!buffer || size == 0))
	return NULL;

    icv_image_t *bif;
    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);

    if (width == 0 || height == 0) {
	bif->height = 1;
	bif->width = (int)(size / 3);
	size = bif->width * 3; // Align to 3 bytes
    } else {
	if (width > 0 && height > (size_t)-1 / width / 3) {
	    bu_log("pix_read_mem: dimensions excessively large, causing integer overflow\n");
	    bu_free(bif, "icv_structure");
	    return NULL;
	}
	if (size < width * height * 3) {
	    bu_log("pix_read_mem: provided size is smaller than width*height*3\n");
	    bu_free(bif, "icv_structure");
	    return NULL;
	}
	bif->height = height;
	bif->width = width;
	size = width * height * 3;
    }

    if (size == 0) {
	bu_free(bif, "icv container");
	return NULL;
    }

    bif->data = icv_uchar2double((unsigned char *)buffer, size);
    bif->magic = ICV_IMAGE_MAGIC;
    bif->channels = 3;
    bif->color_space = ICV_COLOR_SPACE_RGB;
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

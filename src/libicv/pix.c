/*                           P I X . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2025 United States Government as represented by
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
    if (UNLIKELY(!bif))
	return BRLCAD_ERROR;
    if (UNLIKELY(!fp))
	return BRLCAD_ERROR;

    if (bif->color_space == ICV_COLOR_SPACE_GRAY) {
	icv_gray2rgb(bif);
    } else if (bif->color_space != ICV_COLOR_SPACE_RGB) {
	bu_log("pix_write : Color Space conflict");
	return BRLCAD_ERROR;
    }

    unsigned char *data = icv_data2uchar(bif);
    size_t size = (size_t) bif->width*bif->height*3;
    size_t ret = fwrite(data, 1, size, fp);
    bu_free(data, "pix_write : Unsigned Char data");

    if (ret != size) {
	bu_log("pix_write : Short Write");
	return BRLCAD_ERROR;
    }

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
	if (size<buffsize) {
	    data = (unsigned char *)bu_realloc(data, size, "pix_read : decrease size in overbuffered");
	}
	bif->height = 1;
	bif->width = (int) size/3;
    } else { /* buffer frame wise */
	size = (size_t) height*width*3;
	data = (unsigned char *)bu_malloc(size, "pix_read : unsigned char data");
	size_t ret = fread(data, 1, size, fp);
	if (ret!=0 && ferror(fp)) {
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
    bif->data = icv_uchar2double(data, size);
    bu_free(data, "pix_read : unsigned char data");
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

/*                        J P E G . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2026 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdio.h>
#include <jpeglib.h>

#include "bio.h"

#include "bu/log.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "icv_private.h"

struct icv_jpeg_error_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

static void
icv_jpeg_error_exit(j_common_ptr cinfo)
{
    struct icv_jpeg_error_mgr *myerr = (struct icv_jpeg_error_mgr *)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(myerr->setjmp_buffer, 1);
}

int
jpeg_write(icv_image_t *bif, FILE *fp, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct icv_jpeg_error_mgr jerr;
    unsigned char *data;
    JSAMPROW row_pointer[1];
    size_t row;

    if (UNLIKELY(!bif))
	return BRLCAD_ERROR;
    if (UNLIKELY(!fp))
	return BRLCAD_ERROR;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = icv_jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
	jpeg_destroy_compress(&cinfo);
	return BRLCAD_ERROR;
    }

    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width = (JDIMENSION)bif->width;
    cinfo.image_height = (JDIMENSION)bif->height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    if (quality < 1) quality = 1;
    if (quality > 100) quality = 100;
    jpeg_set_quality(&cinfo, quality, TRUE);

    // Force 4:4:4 (disable chroma subsampling) - for
    // raytraced images, which usually have sharp edges,
    // we want to preserve per-pixel color.
    cinfo.comp_info[0].h_samp_factor = 1; // Y
    cinfo.comp_info[0].v_samp_factor = 1;
    cinfo.comp_info[1].h_samp_factor = 1; // Cb
    cinfo.comp_info[1].v_samp_factor = 1;
    cinfo.comp_info[2].h_samp_factor = 1; // Cr
    cinfo.comp_info[2].v_samp_factor = 1;

    // better compression, same quality
    cinfo.optimize_coding = TRUE;

    // highest DCT precision
    cinfo.dct_method = JDCT_ISLOW;

    jpeg_start_compress(&cinfo, TRUE);

    data = icv_data2uchar(bif);

    /* Write rows top-to-bottom (JPEG convention).
     * BRL-CAD images are stored bottom-up, so row 0 in data is the
     * bottom of the image.  We flip here so viewers display correctly. */
    while (cinfo.next_scanline < cinfo.image_height) {
	row = bif->height - 1 - cinfo.next_scanline;
	row_pointer[0] = data + row * bif->width * 3;
	jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    bu_free(data, "jpeg_write data");
    return BRLCAD_OK;
}


icv_image_t *
jpeg_read(FILE *fp)
{
    struct jpeg_decompress_struct cinfo;
    struct icv_jpeg_error_mgr jerr;
    unsigned char *image;
    JSAMPROW row_pointer[1];
    size_t width, height;
    icv_image_t *bif;

    if (UNLIKELY(!fp))
	return NULL;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = icv_jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
	jpeg_destroy_decompress(&cinfo);
	return NULL;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);
    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    width = cinfo.output_width;
    height = cinfo.output_height;

    if (cinfo.output_components != 3) {
	bu_log("jpeg_read: unexpected component count: %d\n",
	       cinfo.output_components);
	jpeg_destroy_decompress(&cinfo);
	return NULL;
    }

    image = (unsigned char *)bu_malloc(width * height * 3, "jpeg_read image");

    /* Read top-to-bottom (JPEG), store bottom-up (BRL-CAD convention) */
    while (cinfo.output_scanline < cinfo.output_height) {
	size_t row = height - 1 - cinfo.output_scanline;
	row_pointer[0] = image + row * width * 3;
	jpeg_read_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);
    bif->width = width;
    bif->height = height;
    bif->channels = 3;
    bif->color_space = ICV_COLOR_SPACE_RGB;
    bif->magic = ICV_IMAGE_MAGIC;
    bif->data = icv_uchar2double(image, 3 * width * height);
    bu_free(image, "jpeg_read uchar data");

    return bif;
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

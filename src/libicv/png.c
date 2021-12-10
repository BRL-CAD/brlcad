/*                      P N G . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2021 United States Government as represented by
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
#include "png.h"

#include "bio.h"

#include "bu/str.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "icv_private.h"

int
png_write(icv_image_t *bif, const char *filename)
{
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    size_t i = 0;
    static int png_color_type;
    unsigned char *data;
    FILE *fh;

    switch (bif->color_space) {
	case ICV_COLOR_SPACE_GRAY:
	    png_color_type = PNG_COLOR_TYPE_GRAY;
	    break;
	default:
	    png_color_type = PNG_COLOR_TYPE_RGB;
    }

    fh = fopen(filename, "wb");
    if (UNLIKELY(fh==NULL)) {
	perror("fopen");
	bu_log("ERROR: png_write failed to get a FILE pointer\n");
	return 0;
    }

    data = icv_data2uchar(bif);

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (UNLIKELY(png_ptr == NULL)) {
	fclose(fh);
	return 0;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL || setjmp(png_jmpbuf(png_ptr))) {
	png_destroy_read_struct(&png_ptr, info_ptr ? &info_ptr : NULL, NULL);
	bu_log("ERROR: Unable to create png header\n");
	fclose(fh);
	return 0;
    }

    png_init_io(png_ptr, fh);
    png_set_IHDR(png_ptr, info_ptr, (unsigned)bif->width, (unsigned)bif->height, 8, png_color_type,
		 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
		 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);
    for (i = bif->height-1; i > 0; --i) {
	png_write_row(png_ptr, (png_bytep) (data + bif->width*bif->channels*i));
    }
    png_write_row(png_ptr, (png_bytep) (data + 0));
    png_write_end(png_ptr, info_ptr);

    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fh);
    return 1;
}

icv_image_t *
png_read(const char* filename)
{
    size_t i;
    FILE *fp = NULL;
    png_structp png_p;
    png_infop info_p;
    char header[8];
    int bit_depth;
    int color_type;
    png_color_16p input_backgrd;
    double gammaval;
    unsigned char *image;
    unsigned char **rows;
    png_color_16 def_backgrd={ 0, 0, 0, 0, 0 };
    icv_image_t *bif;

    if (filename == NULL) {
	fp = stdin;
	setmode(fileno(fp), O_BINARY);
    } else if ((fp = fopen(filename, "rb")) == NULL) {
	bu_log("ERROR: Cannot open file for reading\n");
	return NULL;
    }

    if (fread(header, 8, 1, fp) != 1) {
	bu_log("png-pix: ERROR: Failed while reading file header!!!\n");
	fclose(fp);
	return NULL;
    }

    if (png_sig_cmp((png_bytep)header, 0, 8)) {
	bu_log("png-pix: This is not a PNG file!!!\n");
	fclose(fp);
	return NULL;
    }

    png_p = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p) {
	bu_log("png-pix: png_create_read_struct() failed!!\n");
	fclose(fp);
	return NULL;
    }

    info_p = png_create_info_struct(png_p);
    if (!info_p) {
	bu_log("png-pix: png_create_info_struct() failed!!\n");
	fclose(fp);
	return NULL;
    }

    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);

    png_init_io(png_p, fp);
    png_set_sig_bytes(png_p, 8);
    png_read_info(png_p, info_p);
    color_type = png_get_color_type(png_p, info_p);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
	png_set_gray_to_rgb(png_p);
    }
    png_set_expand(png_p);
    bit_depth = png_get_bit_depth(png_p, info_p);
    if (bit_depth == 16) png_set_strip_16(png_p);

    bif->width = png_get_image_width(png_p, info_p);
    bif->height = png_get_image_height(png_p, info_p);

    if (png_get_bKGD(png_p, info_p, &input_backgrd)) {
	png_set_background(png_p, input_backgrd, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
    } else {
	png_set_background(png_p, &def_backgrd, PNG_BACKGROUND_GAMMA_FILE, 0, 1.0);
    }

    if (png_get_gAMA(png_p, info_p, &gammaval)) {
	png_set_gAMA(png_p, info_p, gammaval);
    }

    png_read_update_info(png_p, info_p);


    /* allocate memory for image */
    image = (unsigned char *)bu_calloc(1, bif->width*bif->height*3, "image");

    /* create rows array */
    rows = (unsigned char **)bu_calloc(bif->height, sizeof(unsigned char *), "rows");
    for (i = 0; i < bif->height; i++)
	rows[bif->height - 1 - i] = image+(i * bif->width * 3);

    png_read_image(png_p, rows);


    bif->data = icv_uchar2double(image, 3 * bif->width * bif->height);
    bu_free(image, "png_read : unsigned char data");
    bif->magic = ICV_IMAGE_MAGIC;
    bif->channels = 3;
    bif->color_space = ICV_COLOR_SPACE_RGB;

    fclose(fp);

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

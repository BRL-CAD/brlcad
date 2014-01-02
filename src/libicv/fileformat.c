/*                      F I L E F O R M A T . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2014 United States Government as represented by
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
#include <fcntl.h>
#include <sys/stat.h>	/* for file mode info in WRMODE */
#include <png.h>

#include "bio.h"

#include "bn.h"
#include "bu.h"
#include "vmath.h"
#include "icv.h"


/* c99 doesn't declare these, but C++ does */
#if (!defined(_WIN32) || defined(__CYGWIN__)) && !defined(__cplusplus)
extern FILE *fdopen(int, const char *);
#endif


/* this might be a little better than saying 0444 */
#define WRMODE S_IRUSR|S_IRGRP|S_IROTH

/* defined in encoding.c */
extern double *uchar2double(unsigned char *data, long int size);
extern unsigned char *data2uchar(const icv_image_t *bif);

/* defined in bw.c */
extern int bw_write(icv_image_t *bif, const char *filename);
extern icv_image_t *bw_read(const char *filename, int width, int height);

/* defined in pix.c */
extern int pix_write(icv_image_t *bif, const char *filename);
extern icv_image_t *pix_read(const char* filename, int width, int height);

/* defined in dpix.c */
extern icv_image_t *dpix_read(const char* filename, int width, int height);
extern int dpix_write(icv_image_t *bif, const char *filename);

/* defined in ppm.c */
extern int ppm_write(icv_image_t *bif, const char *filename);
extern icv_image_t* ppm_read(const char *filename);

/* private functions */

/*
 * Attempt to guess the file type. Understands ImageMagick style
 * FMT:filename as being preferred, but will attempt to guess based on
 * extension as well.
 *
 * I suck. I'll fix this later. Honest.
 *
 * FIXME: assuming trimmedname is BUFSIZ is a crash waiting to bite
 * someone down the road.  should pass a size or use a vls or have it
 * return the string as as return type (making the int type be an int*
 * argument instead that gets set).
 */
int
icv_guess_file_format(const char *filename, char *trimmedname)
{
    /* look for the FMT: header */
#define CMP(name) if (!bu_strncmp(filename, #name":", strlen(#name))) {bu_strlcpy(trimmedname, filename+strlen(#name)+1, BUFSIZ);return ICV_IMAGE_##name; }
    CMP(PIX);
    CMP(PNG);
    CMP(PPM);
    CMP(BMP);
    CMP(BW);
    CMP(DPIX)
#undef CMP

    /* no format header found, copy the name as it is */
    bu_strlcpy(trimmedname, filename, BUFSIZ);

    /* and guess based on extension */
#define CMP(name, ext) if (!bu_strncmp(filename+strlen(filename)-strlen(#name)-1, "."#ext, strlen(#name)+1)) return ICV_IMAGE_##name;
    CMP(PIX, pix);
    CMP(PNG, png);
    CMP(PPM, ppm);
    CMP(BMP, bmp);
    CMP(BW, bw);
    CMP(DPIX, dpix);
#undef CMP
    /* defaulting to PIX */
    return ICV_IMAGE_UNKNOWN;
}

HIDDEN int
png_write(icv_image_t *bif, const char *filename)
{
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    int i = 0;
    int png_color_type = PNG_COLOR_TYPE_RGB;
    unsigned char *data;
    FILE *fh;

    fh = fopen(filename, "wb");
    if (UNLIKELY(fh==NULL)) {
	perror("fdopen");
	bu_log("ERROR: png_write failed to get a FILE pointer\n");
	return 0;
    }

    data = data2uchar(bif);

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
    for (i = bif->height-1; i >= 0; --i)
	png_write_row(png_ptr, (png_bytep) (data + bif->width*bif->channels*i));
    png_write_end(png_ptr, info_ptr);

    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fh);
    return 1;
}

/* end of private functions */

/* begin public functions */

icv_image_t *
icv_read(const char *filename, int format, int width, int height)
{
    if (format == ICV_IMAGE_AUTO) {
	/* do some voodoo with the file magic or something... */
	format = ICV_IMAGE_PIX;
    }

    switch (format) {
	case ICV_IMAGE_PIX:
	    return pix_read(filename, width, height);
	case ICV_IMAGE_BW :
	    return bw_read(filename, width, height);
	case ICV_IMAGE_DPIX :
	    return dpix_read(filename, width, height);
	case ICV_IMAGE_PPM :
	    return ppm_read(filename);
	default:
	    bu_log("icv_read not implemented for this format\n");
	    return NULL;
    }
}


int
icv_write(icv_image_t *bif, const char *filename, ICV_IMAGE_FORMAT format)
{
    /* FIXME: should not be introducing fixed size buffers */
    char buf[BUFSIZ] = {0};

    if (format == ICV_IMAGE_AUTO) {
	format = (ICV_IMAGE_FORMAT)icv_guess_file_format(filename, buf);
    }

    ICV_IMAGE_VAL_INT(bif);

    switch (format) {
	/* case ICV_IMAGE_BMP:
	   return bmp_write(bif, filename); */
	case ICV_IMAGE_PPM:
	    return ppm_write(bif, filename);
	case ICV_IMAGE_PNG:
	    return png_write(bif, filename);
	case ICV_IMAGE_PIX:
	    return pix_write(bif, filename);
	case ICV_IMAGE_BW:
	    return bw_write(bif, filename);
	case ICV_IMAGE_DPIX :
	    return dpix_write(bif, filename);
	default:
	    bu_log("Unrecognized format.  Outputting in PIX format.\n");
    }

    return pix_write(bif, filename);
}


int
icv_writeline(icv_image_t *bif, int y, void *data, ICV_DATA type)
{
    double *dst;
    size_t width_size;
    unsigned char *p=NULL;
    if (bif == NULL) {
	bu_log("ERROR: trying to write a line to null bif\n");
	return -1;
    }

    ICV_IMAGE_VAL_INT(bif);

    if (y > bif->height || y < 0)
        return -1;

    if (data == NULL)
        return -1;

    width_size = (size_t) bif->width*bif->channels;
    dst = bif->data + width_size*y;

    if (type == ICV_DATA_UCHAR) {
	p = (unsigned char *)data;
	for (; width_size > 0; width_size--) {
		*dst = ICV_CONV_8BIT(*p);
		p++;
		dst++;
	}
    } else
	memcpy(dst, data, width_size*sizeof(double));


    return 0;
}


int
icv_writepixel(icv_image_t *bif, int x, int y, double *data)
{
    double *dst;

    ICV_IMAGE_VAL_INT(bif);

    if (x > bif->width || x < 0)
        return -1;

    if (y > bif->height || y < 0)
        return -1;

    if (data == NULL)
        return -1;

    dst = bif->data + (y*bif->width + x)*bif->channels;

    /* can copy float to double also double to double */
    VMOVEN(dst, data, bif->channels);
    return 0;
}


icv_image_t *
icv_create(int width, int height, ICV_COLOR_SPACE color_space)
{
    icv_image_t *bif;
    BU_ALLOC(bif, struct icv_image);
    bif->width = width;
    bif->height = height;
    bif->color_space = color_space;
    bif->alpha_channel = 0;
    bif->magic = ICV_IMAGE_MAGIC;
    switch (color_space) {
	case ICV_COLOR_SPACE_RGB :
	    /* Add all the other three channel images here (eg. HSV, YCbCr etc.) */
	    bif->data = (double *) bu_malloc(bif->height*bif->width*3*sizeof(double), "Image Data");
	    bif->channels = 3;
	    break;
	case ICV_COLOR_SPACE_GRAY :
	    bif->data = (double *) bu_malloc(bif->height*bif->width*1*sizeof(double), "Image Data");
	    bif->channels = 1;
	    break;
	default :
	    bu_exit(1, "icv_create_image : Color Space Not Defined");
	    break;
    }
    return icv_zero(bif);
}


icv_image_t *
icv_zero(icv_image_t *bif)
{
    double *data;
    long size, i;

    ICV_IMAGE_VAL_PTR(bif);

    data = bif->data;
    size = bif->width * bif->height * bif->channels;
    for (i = 0; i < size; i++)
	*data++ = 0;

    return bif;
}


int
icv_destroy(icv_image_t *bif)
{
    ICV_IMAGE_VAL_INT(bif);

    bu_free(bif->data, "Image Data");
    bu_free(bif, "ICV IMAGE Structure");
    return 0;
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

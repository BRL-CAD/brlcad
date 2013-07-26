/*                      F I L E F O R M A T . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2013 United States Government as represented by
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


/* c99 doesn't declare these */
#if !defined(_WIN32) || defined(__CYGWIN__)
extern FILE *fdopen(int, const char *);
#endif


/*** bif flags ***/
/* streaming output (like pix and bw) vs buffer output (like png) */
#define BIF_STREAM 0x0
#define BIF_BUFFER 0x1
/*** end bif flags ***/

/* this might be a little better than saying 0444 */
#define WRMODE S_IRUSR|S_IRGRP|S_IROTH

/* private functions */

/* flip an image vertically */
HIDDEN int
image_flip(unsigned char *buf, int width, int height)
{
    unsigned char *buf2;
    int i;
    size_t pitch = width * 3 * sizeof(char);

    buf2 = (unsigned char *)bu_malloc((size_t)(height * pitch), "image flip");
    for (i=0 ; i<height ; i++)
	memcpy(buf2+i*pitch, buf+(height-i)*pitch, pitch);
    memcpy(buf, buf2, height * pitch);
    bu_free(buf2, "image flip");
    return 0;
}

/* Save functions use the return value not only for success/failure,
 * but also to note if further action is needed.
 *
 *   0 - failure.
 *   1 - success, no further action needed.
 *   2 - success, close() required on fd.
 *
 * This might be better just using the f* functions instead of
 * mixing...
 */

/*
 * Attempt to guess the file type. Understands ImageMagick style
 * FMT:filename as being preferred, but will attempt to guess based on
 * extension as well.
 *
 * I suck. I'll fix this later. Honest.
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
#undef CMP
    /* defaulting to PIX */
    return ICV_IMAGE_UNKNOWN;
}

/**
 * converts unsigned char array to double array.
 * This function returns array of double data.
 *
 * Used to convert data from pix,bw,ppm type images for icv_image
 * struct.
 *
 * This doesnot free the char data.
 *
 * @param data pointer to the array to be converted.
 * @param size Size of the array.
 * @return double array.
 *
 */

HIDDEN double*
uchar2double(unsigned char* data, long int size)
{
    double *double_data, *double_p;
    unsigned char *char_p;
    long int i;

    char_p = data;
    double_p = double_data = (double*) bu_malloc(size*sizeof(double), "uchar2data : double data");
    for (i=0; i<size; i++) {
	 *double_p = ((double)(*char_p))/255.0;
	 double_p++;
	 char_p++;
    }

    return double_data;
}

unsigned char*
data2uchar(const icv_image_t* bif)
{
    long int size;
    long int i;
    unsigned char *uchar_data, *char_p;
    double *double_p;

    size = bif->height*bif->width*bif->channels;
    printf("data2uchar: Size = %ld\n",size);
    char_p = uchar_data = (unsigned char*) bu_malloc((size_t)size, "data2uchar : unsigned char data");

    double_p = bif->data;

    if (ZERO(bif->gamma_corr)) {
	for (i=0; i<size; i++) {
	    *char_p = (unsigned char)floor((*double_p)*255.0);
	    char_p++;
	    double_p++;
	}

    } else {
	float *rand_p;
	double ex = 1.0/bif->gamma_corr;
	bn_rand_init(rand_p, 0);

	for (i=0; i<size; i++) {
	    *char_p = floor(pow(*double_p, ex)*255.0 + (double) bn_rand0to1(rand_p) + 0.5);
	    char_p++;
	    double_p++;
	}
    }

    return uchar_data;
}

HIDDEN int
pix_save(icv_image_t* bif, const char* filename)
{
    unsigned char *data;
    int fd;
    long int ret, size;

    /* TODO :: ADD functions which converts to GRAY Color Space */
    if (bif->color_space != ICV_COLOR_SPACE_RGB) {
	bu_log("bw_save : Color Space conflict");
	return -1;
    }
    data =  data2uchar(bif);
    size = bif->width*bif->height*3;
    fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, WRMODE);
    ret = write(fd, data, (unsigned)size);
    close(fd);
    if (ret != size) {
	bu_log("pix_save : Short Write");
	return -1;
    }
    return 0;
}
HIDDEN int
bw_save(icv_image_t* bif, const char* filename)
{

    unsigned char *data;
    int fd;
    long int ret;
    long int size;

    /* TODO :: ADD functions which converts RGB to GRAY Color Space */
    if (bif->color_space != ICV_COLOR_SPACE_GRAY) {
	bu_log("bw_save : Color Space conflict");
	return -1;
    }
    data =  data2uchar(bif);
    size = bif->height*bif->width;
    fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, WRMODE);
    if (fd < 0) {
	bu_log("Unable to open the file\n");
	return -1;
    }

    ret = write(fd, data, size );
    close(fd);
    bu_free(data, "bw_save : Unsigned Char data");
    if (ret != size) {
	bu_log("bw_save : Short Write\n");
	return -1;
    }

    return 0;
}

HIDDEN icv_image_t *
pix_load(const char* filename, int width, int height)
{
    int fd;
    unsigned char* data = 0;
    icv_image_t* bif;

    size_t size;

    if(width == 0 || height == 0 ) {
	height = 512;
	width = 512;
    }

    size = height*width*3;

    if ((fd = open( filename, O_RDONLY, WRMODE))<0) {
	    bu_log("pix_load: Cannot open file for reading\n");
	    return NULL;
    }
    data = (unsigned char *)bu_malloc(size, "pix_load : unsigned char data");
    if (read(fd, data, size) < 0) {
	bu_log("pix_load: Error Occured while Reading\n");
	bu_free(data,"icv_image data");
	return NULL;
    }
    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);
    bif->data = uchar2double(data,size);
    bu_free(data, "pix_load : unsigned char data");
    bif->magic = ICV_IMAGE_FILE_MAGIC;
    bif->height = height;
    bif->width = width;
    bif->channels = 3;
    bif->color_space = ICV_COLOR_SPACE_RGB;
    close(fd);
    return bif;
}

HIDDEN icv_image_t *
bw_load(const char* filename, int width, int height)
{
    int fd;
    unsigned char* data = 0;
    icv_image_t* bif;

    int size;

    if(width == 0 || height == 0 ) {
	height = 512;
	width = 512;
    }

    size = height*width;

    if ((fd = open( filename, O_RDONLY, WRMODE))<0) {
	    bu_log("bw_load: Cannot open file for reading\n");
	    return NULL;
    }
    data = (unsigned char *)bu_malloc(size, "bw_load : unsigned char data");
    if (read(fd, data, size) != size) {
	bu_log("bw_load: Error Occured while Reading\n");
	bu_free(data,"icv_image data");
	return NULL;
    }
    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);
    bif->data = uchar2double(data,size);
    bu_free(data, "bw_load : unsigned char data");
    bif->magic = ICV_IMAGE_FILE_MAGIC;
    bif->height = height;
    bif->width = width;
    bif->channels = 1;
    bif->color_space = ICV_COLOR_SPACE_GRAY;
    bif->gamma_corr = 0.0;
    close(fd);
    return bif;
}

/* end of private functions */

/* begin public functions */

icv_image_t *
icv_image_load(const char *filename, int format, int width, int height)
{
    if(format == ICV_IMAGE_AUTO) {
	/* do some voodoo with the file magic or something... */
	format = ICV_IMAGE_PIX;
    }

    switch(format) {
	case ICV_IMAGE_PIX:
	    return pix_load(filename, width, height);
	case ICV_IMAGE_BW :
	    return bw_load(filename, width, height);
	default:
	    bu_log("icv_image_load not implemented for this format\n");
	    return NULL;
    }
}

int
icv_image_save(icv_image_t* bif, const char* filename, ICV_IMAGE_FORMAT format)
{
    char buf[BUFSIZ];
    ICV_IMAGE_FORMAT guess_format; /**< Needed to guess the image format */

    if (format == ICV_IMAGE_AUTO || ICV_IMAGE_AUTO_NO_PIX) {
	    guess_format = icv_guess_file_format(filename, buf);
	    if(guess_format == ICV_IMAGE_UNKNOWN) {
		if (format == ICV_IMAGE_AUTO_NO_PIX) {
		bu_log("icv_image_save : Error Recognizing ICV format\n");
		return 0;
		} else
		    format = ICV_IMAGE_PIX;
	    } else
	    format =  guess_format;
	}

    switch(format) {
	/* case ICV_IMAGE_BMP:
	    return bmp_save(bif, filename);
	case ICV_IMAGE_PNG:
	    return png_save(bif, filename);
	case ICV_IMAGE_PPM:
	    return ppm_save(bif, filename);*/
	case ICV_IMAGE_PIX :
	    return pix_save(bif, filename);
	case ICV_IMAGE_BW :
	    return bw_save(bif, filename);
	default :
	    bu_log("icv_image_save : Format not implemented\n");
	    return 0;
    }

}
int
icv_image_writeline(icv_image_t *bif, int y, void *data, ICV_DATA type)
{
    double *dst, *p;
    size_t width_size;
    if (bif == NULL) {
	bu_log("ERROR: trying to write a line to null bif\n");
	return -1;
    }

    if (type == ICV_DATA_UCHAR)
	p = uchar2double(data, bif->width*bif->channels);
    else
	p = data;

    width_size = bif->width*bif->channels;
    dst = bif->data + width_size*y;

    memcpy(dst, p, width_size*sizeof(double));

    return 0;
}

int
icv_image_writepixel(icv_image_t *bif, int x, int y, double *data)
{
    double *dst;
    if (bif == NULL) {
	bu_log("ERROR: trying to write the pixel to a null bif\n");
	return -1;
    }
    dst = bif->data + (y*bif->width + x)*bif->channels;

    /* can copy float to double also double to double*/
    VMOVEN(dst, data, bif->channels);
    return 0;
}

icv_image_t*
icv_image_create(int width, int height, ICV_COLOR_SPACE color_space)
{
    icv_image_t* bif;
    BU_ALLOC(bif, struct icv_image);
    bif->width = width;
    bif->height = height;
    bif->color_space = color_space;
    bif->alpha_channel = 0;
    switch(color_space) {
	case ICV_COLOR_SPACE_RGB :
	/* Add all the other three channel images here (eg. HSV, YCbCr etc.) */
	    bif->data = (double*) bu_malloc(bif->height*bif->width*3*sizeof(double), "Image Data");
	    bif->channels = 3;
	break;
	case ICV_COLOR_SPACE_GRAY :
	    bif->data = (double*) bu_malloc(bif->height*bif->width*1*sizeof(double), "Image Data");
	    bif->channels = 1;
	break;
	default :
	    bu_exit(1,"icv_create_image : Color Space Not Defined");
	break;
    }
    return icv_image_zero(bif);
}

icv_image_t*
icv_image_zero(icv_image_t* bif)
{
    double* data;
    long size,i;

    data = bif->data;
    size = bif->width * bif->height * bif->channels;
    for (i=0; i< size; i++ )
	*data++ = 0;

    return bif;
}

void
icv_image_free(icv_image_t* bif)
{
    bu_free(bif->data, "Image Data");
    bu_free(bif, "ICV IMAGE Structure");
    return;
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

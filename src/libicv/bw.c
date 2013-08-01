/*                            B W . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#include <stdlib.h>
#include <fcntl.h>

#include "bio.h"
#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "icv.h"


#define WRMODE S_IRUSR|S_IRGRP|S_IROTH

/**
 * converts unsigned char array to double array.
 * This function returns array of double data.
 *
 * Used to convert data from pix, bw, ppm type images for icv_image
 * struct.
 *
 * This does not free the char data.
 *
 * @param data pointer to the array to be converted.
 * @param size Size of the array.
 * @return double array.
 *
 */
HIDDEN double *
uchar2double(unsigned char *data, long int size)
{
    double *double_data, *double_p;
    unsigned char *char_p;
    long int i;

    char_p = data;
    double_p = double_data = (double *) bu_malloc(size*sizeof(double), "uchar2data : double data");
    for (i=0; i<size; i++) {
	    *double_p = ((double)(*char_p))/255.0;
	    double_p++;
	    char_p++;
    }

    return double_data;
}


/**
 * Converts double data of icv_image to unsigned char data.
 * This function also does gamma correction using the gamma_corr
 * parameter of the image structure.
 *
 * This is mainly used for saving pix, bw and ppm type images.
 * Gamma correction prevents bad color aliasing.
 *
 */
HIDDEN unsigned char *
data2uchar(const icv_image_t *bif)
{
    long int size;
    long int i;
    unsigned char *uchar_data, *char_p;
    double *double_p;

    size = bif->height*bif->width*bif->channels;
    char_p = uchar_data = (unsigned char *) bu_malloc((size_t)size, "data2uchar : unsigned char data");

    double_p = bif->data;

    if (ZERO(bif->gamma_corr)) {
	    for (i=0; i<size; i++) {
		*char_p = (unsigned char)((*double_p)*255.0 +0.5) ;
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
bw_save(icv_image_t *bif, const char *filename)
{

    unsigned char *data;
    int fd;
    size_t ret, size;

    if (bif->color_space == ICV_COLOR_SPACE_RGB) {
	    icv_rgb2gray(bif, 0, 0, 0, 0, 0);
    } else if (bif->color_space != ICV_COLOR_SPACE_GRAY) {
	    bu_log("bw_save : Color Space conflict");
	    return -1;
    }
    data =  data2uchar(bif);
    size = (size_t) bif->height*bif->width;
    fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, WRMODE);
    if (fd < 0) {
	    bu_log("Unable to open the file\n");
	    return -1;
    }

    ret = write(fd, data, size);
    close(fd);
    bu_free(data, "bw_save : Unsigned Char data");
    if (ret != size) {
	    bu_log("bw_save : Short Write\n");
	    return -1;
    }

    return 0;
}

HIDDEN icv_image_t *
bw_load(const char *filename, int width, int height)
{
    int fd;
    unsigned char *data = 0;
    icv_image_t *bif;

    size_t size;

    if (width == 0 || height == 0) {
	    height = 512;
	    width = 512;
    }

    size = (size_t) height*width;

    if ((fd = open(filename, O_RDONLY, WRMODE))<0) {
	    bu_log("bw_load: Cannot open file for reading\n");
	    return NULL;
    }
    data = (unsigned char *)bu_malloc(size, "bw_load : unsigned char data");
    if (read(fd, data, size) !=0) {
	    bu_log("bw_load: Error Occurred while Reading\n");
	    bu_free(data, "icv_image data");
	    return NULL;
    }
    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);
    bif->data = uchar2double(data, size);
    bu_free(data, "bw_load : unsigned char data");
    bif->magic = ICV_IMAGE_MAGIC;
    bif->height = height;
    bif->width = width;
    bif->channels = 1;
    bif->color_space = ICV_COLOR_SPACE_GRAY;
    bif->gamma_corr = 0.0;
    close(fd);
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

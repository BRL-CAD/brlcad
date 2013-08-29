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
#include <sys/stat.h>	/* for file mode info in WRMODE */
#include <fcntl.h>

#include "bio.h"
#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "icv.h"


/* defined in encoding.c */
extern HIDDEN double *uchar2double(unsigned char *data, long int size);
extern HIDDEN unsigned char *data2uchar(const icv_image_t *bif);

#define WRMODE S_IRUSR|S_IRGRP|S_IROTH

HIDDEN int
bw_write(icv_image_t *bif, const char *filename)
{

    unsigned char *data;
    int fd;
    size_t ret, size;

    if (bif->color_space == ICV_COLOR_SPACE_RGB) {
	icv_rgb2gray_ntsc(bif);
    } else if (bif->color_space != ICV_COLOR_SPACE_GRAY) {
	bu_log("bw_write : Color Space conflict");
	return -1;
    }
    data =  data2uchar(bif);
    size = (size_t) bif->height*bif->width;

    if(filename==NULL)
	fd = fileno(stdout);
    else if ((fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, WRMODE)) < 0) {
	bu_log("bw_write: Cannot open file for saving\n");
	return -1;
    }

    ret = write(fd, data, size);
    close(fd);
    bu_free(data, "bw_write : Unsigned Char data");
    if (ret != size) {
	bu_log("bw_write : Short Write\n");
	return -1;
    }

    return 0;
}

HIDDEN icv_image_t *
bw_read(const char *filename, int width, int height)
{
    int fd;
    unsigned char *data = NULL;
    icv_image_t *bif;
    size_t size;
    size_t buffsize=1024;

    if(filename==NULL)
	fd = fileno(stdin);
    else if ((fd = open(filename, O_RDONLY, WRMODE)) < 0) {
	bu_log("bw_read: Cannot open %s for reading\n", filename);
	return NULL;
    }
    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);
    /* buffer pixel wise */
    if (width == 0 || height == 0) {
	int status = 0;
	size = 0;
	data = (unsigned char *)bu_malloc(buffsize, "bw_read : unsigned char data");
	while((status = read(fd, &data[size], 1))==1) {
	    size++;
	    if(size==buffsize) {
		buffsize+=1024;
		data = (unsigned char *)bu_realloc(data, buffsize, "bw_read : increase size to acomodate data");
	    }
	}
	if(size<buffsize) {
	    data = (unsigned char *)bu_realloc(data, size, "bw_read : decrease size in overbuffered");
	}
	bif->height = 1;
	bif->width = (int) size;
    } else { /* buffer frame wise */
	size = (size_t) height*width;
	data = (unsigned char *)bu_malloc(size, "bw_read : unsigned char data");
	if (read(fd, data, size) < 0) {
	    bu_log("bw_read: Error Occurred while Reading\n");
	    bu_free(data, "icv_image data");
	    bu_free(bif, "icv_structure");
	    return NULL;
	}
	bif->height = height;
	bif->width = width;
    }
    bif->data = uchar2double(data, size);
    bu_free(data, "bw_read : unsigned char data");
    bif->magic = ICV_IMAGE_MAGIC;
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

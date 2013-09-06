/*                           P I X . C
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
/** @file pix.c
 *
 * Contains routines related to pix format.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <sys/stat.h>	/* for file mode info in WRMODE */
#include <fcntl.h>

#include "bio.h"
#include "bu.h"
#include "bn.h"
#include "icv.h"

/* this might be a little better than saying 0444 */
#define WRMODE S_IRUSR|S_IRGRP|S_IROTH

/* defined in encoding.c */
extern HIDDEN double *uchar2double(unsigned char *data, long int size);
extern HIDDEN unsigned char *data2uchar(const icv_image_t *bif);

HIDDEN int
pix_write(icv_image_t *bif, const char *filename)
{
    unsigned char *data;
    int fd;
    size_t ret, size;

    if (bif->color_space == ICV_COLOR_SPACE_GRAY) {
	icv_gray2rgb(bif);
    } else if (bif->color_space != ICV_COLOR_SPACE_RGB) {
	bu_log("pix_write : Color Space conflict");
	return -1;
    }

    if(filename==NULL) {
	fd = fileno(stdout);
#if defined(_WIN32) && !defined(__CYGWIN__)
    setmode(fd, O_BINARY);
#endif
    } else if ((fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, WRMODE)) < 0) {
	bu_log("pix_write: Cannot open file for saving\n");
	return -1;
    }

    data =  data2uchar(bif);
    size = (size_t) bif->width*bif->height*3;
    ret = write(fd, data, size);
    close(fd);
    if (ret != size) {
	bu_log("pix_write : Short Write");
	return -1;
    }
    return 0;
}


HIDDEN icv_image_t *
pix_read(const char* filename, int width, int height)
{
    int fd;
    unsigned char *data = 0;
    icv_image_t *bif;
    size_t size;
    size_t buffsize=1024*3;

    if(filename == NULL) {
	fd = fileno(stdin);
#if defined(_WIN32) && !defined(__CYGWIN__)
    setmode(fd, O_BINARY);
#endif
    } else if ((fd = open(filename, O_RDONLY, WRMODE))<0) {
	bu_log("pix_read: Cannot open file for reading\n");
	return NULL;
    }
    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);
    /* buffer pixel wise */
    if (width == 0 || height == 0) {
	int status = 0;
	size = 0;
	data = (unsigned char *)bu_malloc(buffsize, "pix_read : unsigned char data");
	while((status = read(fd, &data[size], 3))==3) {
	    size+=3;
	    if(size==buffsize) {
		buffsize+=1024*3;
		data = (unsigned char *)bu_realloc(data, buffsize, "pix_read : increase size to acomodate data");
	    }
	}
	if(size<buffsize) {
	    data = (unsigned char *)bu_realloc(data, size, "pix_read : decrease size in overbuffered");
	}
	bif->height = 1;
	bif->width = (int) size/3;
    } else { /* buffer frame wise */
	size = (size_t) height*width*3;
	data = (unsigned char *)bu_malloc(size, "pix_read : unsigned char data");
	if (read(fd, data, size) < 0) {
	    bu_log("pix_read: Error Occurred while Reading\n");
	    bu_free(data, "icv_image data");
	    bu_free(bif, "icv_structure");
	    return NULL;
	}
	bif->height = height;
	bif->width = width;
    }
    if(size)
	bif->data = uchar2double(data, size);
    else {
	/* zero sized image */
	bu_free(bif, "icv container");
	bu_free(data, "unisigned char data");
	return NULL;
    }
    bif->data = uchar2double(data, size);
    bu_free(data, "pix_read : unsigned char data");
    bif->magic = ICV_IMAGE_MAGIC;
    bif->channels = 3;
    bif->color_space = ICV_COLOR_SPACE_RGB;
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

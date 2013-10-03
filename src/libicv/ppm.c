/*                           P P M . C
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
/** @file ppm.c
 *
 * This file contains reading and writing routines for ppm format.
 *
 */

#include "common.h"
#include <string.h>

#include "bu.h"
#include "icv.h"

/* defined in encoding.c */
extern unsigned char *data2uchar(const icv_image_t *bif);
extern double *uchar2double(unsigned char *data, long int size);

/* flip an image vertically */
int
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

int
ppm_write(icv_image_t *bif, const char *filename)
{
    unsigned char *data;
    FILE *fp;
    size_t ret, size;

    if (bif->color_space == ICV_COLOR_SPACE_GRAY) {
	icv_gray2rgb(bif);
    } else if (bif->color_space != ICV_COLOR_SPACE_RGB) {
	bu_log("ppm_write : Color Space conflict");
	return -1;
    }

    if (filename==NULL) {
	fp = stdout;
    } else if ((fp = fopen(filename, "w")) == NULL) {
	bu_log("ERROR : Cannot open file for saving\n");
	return -1;
    }
    data =  data2uchar(bif);
    size = (size_t) bif->width*bif->height*3;
    image_flip(data, bif->width, bif->height);
    ret = fprintf(fp, "P6 %d %d 255\n", bif->width, bif->height);

    ret = fwrite(data, 1, size, fp);

    fclose(fp);
    if (ret != size) {
	bu_log("ERROR : Short Write");
	return -1;
    }
    return 0;
}


HIDDEN void
ppm_nextline(FILE *fp)
{
    int c;

    /* skip to the binary data section, starting on next line.
     * supports mac, unix, windows line endings.
     */
    do {
	c = fgetc(fp);
	if (c == '\r') {
	    c = fgetc(fp);
	    if (c != '\n') {
		ungetc(c, fp);
		c = '\n'; /* pretend we're not an old mac file */
	    }
	}
    } while (c != '\n');
}


icv_image_t*
ppm_read(const char *filename)
{
    char buff[3]; /* To ensure that the format. And thus read "P6" */
    FILE *fp;
    char c;
    icv_image_t *bif;
    int rgb_comp_color;
    unsigned char *data;
    size_t size,ret;

    if (filename == NULL) {
	fp = stdin;
    } else if ((fp = fopen(filename, "r")) == NULL) {
	bu_log("ERROR: Cannot open file for reading\n");
	return NULL;
    }

    if (!bu_fgets(buff, sizeof(buff), fp)) {
	bu_log("ERROR : Invalid Image");
	return NULL;
    }

    /* check the image format */
    if (buff[0] != 'P' || buff[1] != '6') {
	bu_log("ERROR: Invalid image format (must be 'P6')\n");
	return NULL;
    }

    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);

    /* check for comments lines in PPM image header */
    c = getc(fp);
    while (c == '#') {
	/* encountered comment, skip to next line */
	ppm_nextline(fp);
	c = getc(fp);
    }

    ungetc(c, fp);

    /* read image size information */
    if (fscanf(fp, "%d %d", &bif->width, &bif->height) != 2) {
	fprintf(stderr, "Invalid image size (error loading '%s')\n", filename);
	bu_free(bif, "icv_image structure");
	return NULL;
    }

    /* read rgb component */
    if (fscanf(fp, "%d", &rgb_comp_color) != 1) {
	bu_log("ERROR: Invalid rgb component\n");
	bu_free(bif, "icv_image structure");
	return NULL;
    }

    /* check rgb component depth */
    if (rgb_comp_color!= 255) {
	bu_log("ERROR: Image does not have 8-bits components\n");
	bu_free(bif, "icv_image structure");
	return NULL;
    }

    /* skip to the binary data section, starting on next line.
     * supports mac, unix, windows line endings.
     */
    ppm_nextline(fp);

    /* memory allocation for pixel data */
    data = (unsigned char*) bu_malloc(bif->width * bif->height * 3, "image data");

    size = 3 * bif->width * bif->height;
    /* read pixel data from file */
    ret = fread(data, 1, size, fp);
    if (ret!=0 && ferror(fp)) {
	bu_log("Error Occurred while Reading\n");
	bu_free(data, "icv_image data");
	bu_free(bif, "icv_structure");
	fclose(fp);
	return NULL;
    }

    fclose(fp);

    image_flip(data, bif->width, bif->height);

    bif->data = uchar2double(data, size);
    bu_free(data, "ppm_read : unsigned char data");
    bif->magic = ICV_IMAGE_MAGIC;
    bif->channels = 3;
    bif->color_space = ICV_COLOR_SPACE_RGB;
    fclose(fp);
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

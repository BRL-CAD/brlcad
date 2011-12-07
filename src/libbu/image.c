/*                           I M A G E . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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

#include "bu.h"
#include "vmath.h"


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
HIDDEN int
guess_file_format(const char *filename, char *trimmedname)
{
    /* look for the FMT: header */
#define CMP(name) if (!strncmp(filename, #name":", strlen(#name))) {bu_strlcpy(trimmedname, filename+strlen(#name)+1, BUFSIZ);return BU_IMAGE_##name; }
    CMP(PIX);
    CMP(PNG);
    CMP(PPM);
    CMP(BMP);
    CMP(BW);
#undef CMP

    /* no format header found, copy the name as it is */
    bu_strlcpy(trimmedname, filename, BUFSIZ);

    /* and guess based on extension */
#define CMP(name, ext) if (!strncmp(filename+strlen(filename)-strlen(#name)-1, "."#ext, strlen(#name)+1)) return BU_IMAGE_##name;
    CMP(PNG, png);
    CMP(PPM, ppm);
    CMP(BMP, bmp);
    CMP(BW, bw);
#undef CMP
    /* defaulting to PIX */
    return BU_IMAGE_PIX;
}

HIDDEN int
png_save(int fd, unsigned char *rgb, int width, int height, int depth)
{
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    int i = 0;
	int png_color_type = PNG_COLOR_TYPE_RGB;
    FILE *fh;

    fh = fdopen(fd, "wb");
    if (UNLIKELY(fh==NULL)) {
	perror("fdopen");
	bu_log("ERROR: png_save failed to get a FILE pointer\n");
	return 0;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (UNLIKELY(png_ptr == NULL))
	return 0;

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL || setjmp(png_jmpbuf(png_ptr))) {
	png_destroy_read_struct(&png_ptr, info_ptr ? &info_ptr : NULL, NULL);
	bu_log("ERROR: Unable to create png header\n");
	return 0;
    }

    if (depth == 4) {
	png_color_type = PNG_COLOR_TYPE_RGBA;
    }

    png_init_io(png_ptr, fh);
    png_set_IHDR(png_ptr, info_ptr, (unsigned)width, (unsigned)height, 8, png_color_type,
		  PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
		  PNG_FILTER_TYPE_BASE);
    png_write_info(png_ptr, info_ptr);
    for (i = height-1; i >= 0; --i)
	png_write_row(png_ptr, (png_bytep) (rgb + width*depth*i));
    png_write_end(png_ptr, info_ptr);

    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fh);
    return 1;
}

HIDDEN int
bmp_save(int fd, unsigned char *rgb, int width, int height)
{
    FILE *fh;

    fh = fdopen(fd, "wb");
    if (UNLIKELY(fh==NULL)) {
	perror("fdopen");
	bu_log("ERROR: bmp_save failed to get a FILE pointer\n");
	return 0;
    }

    if (UNLIKELY(!rgb || width<0 || height<0)) {
	bu_log("ERROR: invalid image specification\n");
	return 0;
    }

    bu_log("ERROR: Unimplemented\n");

    return 0;
}

HIDDEN int
pix_save(int fd, unsigned char *rgb, int size)
{
    int ret;
    ret = write(fd, rgb, (unsigned)size);
    if (ret != size)
	return 0;
    return 2;
}

/* size is bytes of PIX data, bw output file will be 1/3 this size.
 * Also happens to munge up the contents of rgb.
 */
HIDDEN int
bw_save(int fd, unsigned char *rgb, int size)
{
    int ret;
    int bwsize = size/3, i;

    if (bwsize*3 != size) {
	bu_log("ERROR: image size=%d is not a multiple of 3.\n", size);
	return 0;
    }

    /* an ugly naive pixel grey-scale hack. Does not take human color
     * curves.
     */
    for (i=0;i<bwsize;++i)
	rgb[i] = (int)((float)rgb[i*3]+(float)rgb[i*3+1]+(float)rgb[i*3+2]/3.0);

    ret = write(fd, rgb, (unsigned)bwsize);
    if (ret != bwsize)
	return 0;

    return 2;
}

HIDDEN int
ppm_save(int fd, unsigned char *rgb, int width, int height)
{
    int ret;
    char buf[BUFSIZ] = {0};

    image_flip(rgb, width, height);
    snprintf(buf, BUFSIZ, "P6 %d %d 255\n", width, height);
    ret = write(fd, buf, strlen(buf));
    ret = write(fd, rgb, (size_t)(3*width*height));
    if (ret  != 3*width*height)
	return 0;
    return 2;
}

/* end if private functions */

/* begin public functions */

int
bu_image_load()
{
    bu_log("%s is deprecated\n", __FUNCTION__);
    return 0;
}

int
bu_image_save(unsigned char *data, int width, int height, int depth, char *filename, int filetype)
{
    int i;
    struct bu_image_file *bif = NULL;

    bu_log("%s is deprecated\n", __FUNCTION__);
    bif = bu_image_save_open(filename, filetype, width, height, depth);
    if (bif==NULL)
	return -1;

    for (i=0;i<height;++i) {
	int ret = bu_image_save_writeline(bif, i, (unsigned char*)(data+i*width*depth));
	if (UNLIKELY(ret == -1)) {
	    bu_log("Unexpected error saving image scanline\n");
	}
    }

    return bu_image_save_close(bif);
}

struct bu_image_file *
bu_image_save_open(const char *filename, int format, int width, int height, int depth)
{
    struct bu_image_file *bif = (struct bu_image_file *)bu_malloc(sizeof(struct bu_image_file), "bu_image_save_open");
    bu_log("%s is deprecated\n", __FUNCTION__);
    bif->magic = BU_IMAGE_FILE_MAGIC;
    if (format == BU_IMAGE_AUTO || BU_IMAGE_AUTO_NO_PIX) {
	char buf[BUFSIZ];
	bif->format = guess_file_format(filename, buf);
	if(format == BU_IMAGE_AUTO_NO_PIX && bif->format == BU_IMAGE_PIX)
	    return NULL;
	bif->filename = bu_strdup(buf);
    } else {
	bif->format = format;
	bif->filename = bu_strdup(filename);
    }

    /* if we want the ability to "continue" a stopped output, this would be
     * where to check for an existing "partial" file. */
    bif->fd = open(bif->filename, O_WRONLY|O_CREAT|O_TRUNC, WRMODE);
    if (UNLIKELY(bif->fd < 0)) {
	perror("open");
	free(bif);
	bu_log("ERROR: opening output file \"%s\" for writing\n", bif->filename);
	return NULL;
    }
    bif->width = width;
    bif->height = height;
    bif->depth = depth;
    bif->data = (unsigned char *)bu_malloc((size_t)(width*height*depth), "bu_image_file data");
    return bif;
}

int
bu_image_save_writeline(struct bu_image_file *bif, int y, unsigned char *data)
{
    bu_log("%s is deprecated\n", __FUNCTION__);
    if (UNLIKELY(bif==NULL)) {
	bu_log("ERROR: trying to write a line with a null bif\n");
	return -1;
    }
    memcpy(bif->data + bif->width*bif->depth*y, data, (size_t)bif->width*bif->depth);
    return 0;
}

int
bu_image_save_writepixel(struct bu_image_file *bif, int x, int y, unsigned char *data)
{
    unsigned char *dst;

    bu_log("%s is deprecated\n", __FUNCTION__);
    if( bif == NULL ) {
	bu_log("ERROR: trying to write a line with a null bif\n");
	return -1;
    }
    dst = bif->data + (bif->width*y+x)*bif->depth;
    VMOVE(dst, data);
    return 0;
}

int
bu_image_save_close(struct bu_image_file *bif)
{
    int r = 0;
    bu_log("%s is deprecated\n", __FUNCTION__);
    switch (bif->format) {
	case BU_IMAGE_BMP:
	    r = bmp_save(bif->fd, bif->data, bif->width, bif->height);
	    break;
	case BU_IMAGE_PNG:
	    r = png_save(bif->fd, bif->data, bif->width, bif->height, bif->depth);
	    break;
	case BU_IMAGE_PPM:
	    r = ppm_save(bif->fd, bif->data, bif->width, bif->height);
	    break;
	case BU_IMAGE_PIX:
	    r = pix_save(bif->fd, bif->data, bif->width*bif->height*bif->depth);
	    break;
	case BU_IMAGE_BW:
	    r = bw_save(bif->fd, bif->data, bif->width*bif->height*bif->depth);
	    break;
    }
    switch (r) {
	case 0:
	    bu_log("Failed to write image\n");
	    break;
	    /* 1 signals success with no further action needed */
	case 2:
	    close(bif->fd);
	    break;
    }

    bu_free(bif->filename, "bu_image_file filename");
    bu_free(bif->data, "bu_image_file data");
    bu_free(bif, "bu_image_file");

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

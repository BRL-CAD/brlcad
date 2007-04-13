/*                           I M A G E . C
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
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
/** @addtogroup bu */
/** @{ */
/** @file image.c
 *
 *  @brief image save/load routines
 *
 *  save or load images in a variety of formats.
 *
 *  @author
 *	Erik Greenwald
 *
 *  @par Source -
 *	The U. S. Army Research Laboratory
 * @n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/** @} */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>	/* for file mode info in WRMODE */

#include <png.h>

#include "machine.h"
#include "bu.h"

/*** bif flags ***/
/* streaming output (like pix and bw) vs buffer output (like png) */
#define BIF_STREAM 0x0
#define BIF_BUFFER 0x1
/*** end bif flags ***/

/* this might be a little better than saying 0444 */
#define WRMODE S_IRUSR|S_IRGRP|S_IROTH

/* private functions */
/* 
 * Attempt to guess the file type. Understands ImageMagick style 
 * FMT:filename as being preferred, but will attempt to guess
 * based on extension as well.
 *
 * I suck. I'll fix this later. Honest.
 */
static int 
guess_file_format(char *filename, char *trimmedname)
{
    /* look for the FMT: header */
#define CMP(name) if(!strncmp(filename,#name":",strlen(#name))){strncpy(trimmedname,filename+strlen(#name)+1,BUFSIZ);return BU_IMAGE_##name; }
    CMP(PIX);
    CMP(PNG);
    CMP(BMP);
    CMP(BW);
#undef CMP

    /* no format header found, copy the name as it is */
    strncpy(trimmedname, filename, BUFSIZ);

    /* and guess based on extension */
#define CMP(name,ext) if(!strncmp(filename+strlen(filename)-strlen(#name)-1,"."#ext,strlen(#name)+1)) return BU_IMAGE_##name;
    CMP(PNG,png);
    CMP(BMP,bmp);
    CMP(BW,bw);
#undef CMP
    /* defaulting to PIX */
    return BU_IMAGE_PIX;
}

static int 
png_save(int fd, char *rgb, int width, int height)
{
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    int i = 0;
    FILE *fh;

    fh = fdopen(fd,"wb");
    if(fh==NULL) {
	perror("png_save trying to get FILE pointer for descriptor");
	exit(-1);
	return 0;
    }
    png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(png_ptr == NULL) return 0;
    info_ptr = png_create_info_struct (png_ptr);
    if(info_ptr == NULL || setjmp (png_jmpbuf (png_ptr))) {
	printf("Ohs Noes!\n"); fflush(stdout);
	png_destroy_read_struct (&png_ptr, info_ptr ? &info_ptr : NULL, NULL);
	return 0;
    }

    png_init_io (png_ptr, fh);
    png_set_compression_level (png_ptr, Z_BEST_COMPRESSION);
    png_set_IHDR (png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
                  PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                  PNG_FILTER_TYPE_DEFAULT);
    png_write_info (png_ptr, info_ptr);	/* causes badness, NULL write func... */
    for (i = height; i > 0; --i)
        png_write_row (png_ptr, (png_bytep) (rgb + width*3*i));
    png_write_end (png_ptr, info_ptr);
    png_destroy_read_struct(png_ptr, info_ptr, NULL);
    return 1;
}

static int 
bmp_save(int fd, char *rgb, int width, int height)
{
    return 0;
}

static int 
pix_save(int fd, char *rgb, int size) { return write(fd, rgb, size); }

/* size is bytes of PIX data, bw output file will be 1/3 this size. 
 * Also happens to munge up the contents of rgb. */
static int 
bw_save(int fd, char *rgb, int size) 
{ 
    int bwsize = size/3, i;
    if(bwsize*3 != size) {
	printf("Huh, size=%d is not a multiple of 3.\n", size);
	exit(-1);	/* flaming death */
    }
    /* an ugly naïve pixel grey-scale hack. Does not take human color curves. */
    for(i=0;i<bwsize;++i) rgb[i] = (int)((float)rgb[i*3]+(float)rgb[i*3+1]+(float)rgb[i*3+2]/3.0);
    return write(fd, rgb, bwsize); 
}

/* end if private functions */

/* begin public functions */

int
bu_image_load()
{
    return 0;
}

int
bu_image_save(char *data, int width, int height, int depth, char *filename, int filetype)
{
    int i;
    struct bu_image_file *bif = bu_image_save_open(filename,filetype,width,height,depth);
    if(bif==NULL) return -1;
    for(i=0;i<height;++i) {
	if(bu_image_save_writeline(bif,i,data+i*width*depth)==-1) {
	    bu_log("Uh?");
	}
    }
    return bu_image_save_close(bif);
}

struct bu_image_file *
bu_image_save_open(char *filename, int format, int width, int height, int depth)
{
    struct bu_image_file *bif = (struct bu_image_file *)bu_malloc(sizeof(struct bu_image_file),"bu_image_save_open");
    bif->magic = BU_IMAGE_FILE_MAGIC;
    if(format == BU_IMAGE_AUTO) {
	char buf[BUFSIZ];
	bif->format = guess_file_format(filename,buf);
	bif->filename = strdup(buf);
    } else {
	bif->format = format;
	bif->filename = strdup(filename);
    }

    /* if we want the ability to "continue" a stopped output, this would be
     * where to check for an existing "partial" file. */
    bif->fd = open(bif->filename,O_WRONLY|O_CREAT|O_TRUNC, WRMODE);
    if(bif->fd < 0) {
	char buf[BUFSIZ];
	snprintf(buf,BUFSIZ,"Opening output file \"%s\" for writing",bif->filename);
	perror(buf);
	free(bif);
	exit(-1);
	return NULL;
    }
    bif->width = width;
    bif->height = height;
    bif->depth = depth;
    bif->data = (char *)bu_malloc(width*height*depth, "bu_image_file data");
    return bif;
}

int 
bu_image_save_writeline(struct bu_image_file *bif, int y, char *data)
{
    if(bif==NULL) { printf("trying to write a line with a null bif\n"); return -1; }
    memcpy(bif->data + bif->width*bif->depth*y, data, bif->width*bif->depth);
    return 0;
}

int 
bu_image_save_close(struct bu_image_file *bif)
{
    switch(bif->format) {
	case BU_IMAGE_BMP: bmp_save(bif->fd,bif->data,bif->width,bif->height); break;
	case BU_IMAGE_PNG: png_save(bif->fd,bif->data,bif->width,bif->height); break;
	case BU_IMAGE_PIX: pix_save(bif->fd,bif->data,bif->width*bif->height*bif->depth); break;
	case BU_IMAGE_BW: bw_save(bif->fd, bif->data, bif->width*bif->height*bif->depth); break;
    }
    close(bif->fd);
    bu_free(bif->filename,"bu_image_file filename");
    bu_free(bif->data,"bu_image_file data");
    bu_free(bif,"bu_image_file");
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

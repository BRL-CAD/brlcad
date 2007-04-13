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
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>	/* for file mode info in WRMODE */

#include "machine.h"
#include "bu.h"

/* this might be a little better than saying 0444 */
#define WRMODE S_IRUSR|S_IRGRP|S_IROTH

/* private functions */
/* 
 * Attempt to guess the file type. Understands ImageMagick style 
 * FMT:filename as being preferred, but will attempt to guess
 * based on extension as well.
 */
int 
devine_file_format(char *filename, char *trimmedname)
{
    strncpy(trimmedname, filename, BUFSIZ);
    return BU_IMAGE_PIX;
}

int 
bw_save(char *data, int width, int height, int depth, char *filename)
{
    int fd;
    if(depth != 1) {
	bu_log("BW must be fed 24 bit RGB data! Aborting.");
	return -1;
    }
    fd = open(filename, O_CREAT|O_TRUNC|O_WRONLY, WRMODE);
    if(fd<=0) {
	perror("Unable to open file for writing");
	return fd;
    }
    if(write(fd,data,width*height*depth) != width*height*depth) {
	perror("Unable to write to file");
	close(fd);
	return -1;
    }
    close(fd);
    return 0;
}

int 
png_save(char *data, int width, int height, int depth, char *filename)
{
    return 0;
}

int 
bmp_save(char *data, int width, int height, int depth, char *filename)
{
    return 0;
}

int 
pix_save(char *data, int width, int height, int depth, char *filename)
{
    int fd;
    if(depth != 3) {
	bu_log("PIX must be fed 24 bit RGB data! Aborting.");
	return -1;
    }
    fd = open(filename, O_CREAT|O_TRUNC|O_WRONLY, WRMODE);
    if(fd<=0) {
	perror("Unable to open file for writing");
	return fd;
    }
    if(write(fd,data,width*height*depth) != width*height*depth) {
	perror("Unable to write to file");
	close(fd);
	return -1;
    }
    close(fd);
    return 0;
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
	bif->format = devine_file_format(filename,buf);
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
	return NULL;
    }
    bif->width = width;
    bif->height = height;
    bif->depth = depth;
    return bif;
}

int 
bu_image_save_writeline(struct bu_image_file *bif, int y, char *data)
{
    if(bif==NULL) { printf("trying to write a line with a null bif\n"); return -1; }
    lseek(bif->fd, bif->width*bif->depth*y, SEEK_SET);
    write(bif->fd, data, bif->width*bif->depth);
    return 0;
}

int 
bu_image_save_close(struct bu_image_file *bif)
{
    close(bif->fd);
    bu_free(bif->filename,"bu_image_file filename");
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

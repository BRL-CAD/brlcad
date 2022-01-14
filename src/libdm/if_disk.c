/*                       I F _ D I S K . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2022 United States Government as represented by
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
/** @addtogroup libstruct fb */
/** @{ */
/** @file if_disk.c
 *
 */
/** @} */

#include "common.h"

#include "bio.h"

#include "bu/color.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bu/log.h"
#include "./include/private.h"
#include "dm.h"


#define FILE_CMAP_SIZE ((size_t)ifp->i->if_width * ifp->i->if_height * sizeof(RGBpixel))

/* Ensure integer number of pixels per DMA */
#define DISK_DMA_BYTES ((size_t)16*(size_t)1024/sizeof(RGBpixel)*sizeof(RGBpixel))
#define DISK_DMA_PIXELS (DISK_DMA_BYTES/sizeof(RGBpixel))

#define if_seekpos u5.l	/* stored seek position */


HIDDEN int
dsk_open(struct fb *ifp, const char *file, int width, int height)
{
    static char zero = 0;

    FB_CK_FB(ifp->i);

    /* check for default size */
    if (width == 0)
	width = ifp->i->if_width;
    if (height == 0)
	height = ifp->i->if_height;

    if (BU_STR_EQUAL(file, "-")) {
	/*
	 * It is the applications job to write ascending scanlines.
	 * If it does not, then this can be stacked with /dev/mem,
	 * i.e.	/dev/mem -
	 */
	ifp->i->if_fd = 1;		/* fileno(stdout) */
	ifp->i->if_width = width;
	ifp->i->if_height = height;
	ifp->i->if_seekpos = 0;
	return 0;
    }

    if ((ifp->i->if_fd = open(file, O_RDWR | O_BINARY, 0)) == -1
	&& (ifp->i->if_fd = open(file, O_RDONLY | O_BINARY, 0)) == -1) {
	if ((ifp->i->if_fd = open(file, O_RDWR | O_CREAT | O_BINARY, 0664)) > 0) {
	    /* New file, write byte at end */
	    if (bu_lseek(ifp->i->if_fd, (height*width*sizeof(RGBpixel)-1), 0) == -1) {
		fb_log("disk_device_open : can not seek to end of new file.\n");
		return -1;
	    }
	    if (write(ifp->i->if_fd, &zero, 1) < 0) {
		fb_log("disk_device_open : initial write failed.\n");
		return -1;
	    }
	} else
	    return -1;
    }

    setmode(ifp->i->if_fd, O_BINARY);

    ifp->i->if_width = width;
    ifp->i->if_height = height;
    if (bu_lseek(ifp->i->if_fd, 0, 0) == -1) {
	fb_log("disk_device_open : can not seek to beginning.\n");
	return -1;
    }
    ifp->i->if_seekpos = 0;
    return 0;
}

HIDDEN struct fb_platform_specific *
dsk_get_fbps(uint32_t UNUSED(magic))
{
        return NULL;
}


HIDDEN void
dsk_put_fbps(struct fb_platform_specific *UNUSED(fbps))
{
        return;
}

HIDDEN int
dsk_open_existing(struct fb *UNUSED(ifp), int UNUSED(width), int UNUSED(height), struct fb_platform_specific *UNUSED(fb_p))
{
        return 0;
}

HIDDEN int
dsk_close_existing(struct fb *UNUSED(ifp))
{
        return 0;
}

HIDDEN int
dsk_configure_window(struct fb *UNUSED(ifp), int UNUSED(width), int UNUSED(height))
{
        return 0;
}

HIDDEN int
dsk_refresh(struct fb *UNUSED(ifp), int UNUSED(x), int UNUSED(y), int UNUSED(w), int UNUSED(h))
{
        return 0;
}

HIDDEN int
dsk_close(struct fb *ifp)
{
    return close(ifp->i->if_fd);
}


HIDDEN int
dsk_free(struct fb *ifp)
{
    close(ifp->i->if_fd);
    if (bu_file_delete(ifp->i->if_name)) {
	return 0;
    } else {
	return 1;
    }
}


/*
 * Clear the disk file to the given color.
 */
HIDDEN int
disk_color_clear(struct fb *ifp, register unsigned char *bpp)
{
    static unsigned char pix_buf[DISK_DMA_BYTES] = {0};
    register unsigned char *pix_to;
    size_t i;
    int fd;
    size_t pixelstodo;

    /* Fill buffer with background color. */
    for (i = DISK_DMA_PIXELS, pix_to = pix_buf; i > 0; i--) {
	COPYRGB(pix_to, bpp);
	pix_to += sizeof(RGBpixel);
    }

    /* Set start of framebuffer */
    fd = ifp->i->if_fd;
    if (ifp->i->if_seekpos != 0 && bu_lseek(fd, 0, 0) == -1) {
	fb_log("disk_color_clear : seek failed.\n");
	return -1;
    }

    /* Send until frame buffer is full. */
    pixelstodo = ifp->i->if_height * ifp->i->if_width;
    while (pixelstodo > 0) {
	i = pixelstodo > DISK_DMA_PIXELS ? DISK_DMA_PIXELS : pixelstodo;
	if (write(fd, pix_buf, i * sizeof(RGBpixel)) == -1)
	    return -1;
	pixelstodo -= i;
	ifp->i->if_seekpos += i * sizeof(RGBpixel);
    }

    return 0;
}


HIDDEN int
dsk_clear(struct fb *ifp, unsigned char *bgpp)
{
    static RGBpixel black = { 0, 0, 0 };

    if (bgpp == (unsigned char *)NULL)
	return disk_color_clear(ifp, black);

    return disk_color_clear(ifp, bgpp);
}


HIDDEN ssize_t
dsk_read(struct fb *ifp, int x, int y, unsigned char *pixelp, size_t count)
{
    size_t bytes = count * sizeof(RGBpixel);
    size_t todo;
    ssize_t got;
    size_t dest;
    size_t bytes_read = 0;
    int fd = ifp->i->if_fd;

    /* Reads on stdout make no sense.  Take reads from stdin. */
    if (fd == 1) fd = 0;

    dest = ((y * ifp->i->if_width) + x) * sizeof(RGBpixel);
    if (ifp->i->if_seekpos != dest && bu_lseek(fd, dest, 0) == -1) {
	fb_log("disk_buffer_read : seek to %ld failed.\n", dest);
	return -1;
    }
    ifp->i->if_seekpos = dest;
    while (bytes > 0) {
	todo = bytes;
	if ((got = read(fd, (char *) pixelp, todo)) != (ssize_t)todo) {
	    if (got <= 0) {
		if (got < 0) {
		    perror("READ ERROR");
		}

		if (bytes_read <= 0)
		    return -1;	/* error */

		/* early EOF -- indicate what we got */
		return bytes_read/sizeof(RGBpixel);
	    }
	    if (fd != 0) {
		/* This happens all the time reading from pipes */
		fb_log("disk_buffer_read(fd=%d): y=%d read of %lu got %ld bytes\n",
		       fd, y, todo, got);
	    }
	}
	bytes -= got;
	pixelp += got;
	ifp->i->if_seekpos += got;
	bytes_read += got;
    }
    return bytes_read/sizeof(RGBpixel);
}


HIDDEN ssize_t
dsk_write(struct fb *ifp, int x, int y, const unsigned char *pixelp, size_t count)
{
    register ssize_t bytes = count * sizeof(RGBpixel);
    ssize_t todo;
    size_t dest;

    dest = (y * ifp->i->if_width + x) * sizeof(RGBpixel);
    if (dest != ifp->i->if_seekpos) {
	if (bu_lseek(ifp->i->if_fd, (b_off_t)dest, 0) == -1) {
	    fb_log("disk_buffer_write : seek to %zd failed.\n", dest);
	    return -1;
	}
	ifp->i->if_seekpos = dest;
    }
    while (bytes > 0) {
	ssize_t ret;
	todo = bytes;
	ret = write(ifp->i->if_fd, (char *) pixelp, todo);
	if (ret != todo) {
	    fb_log("disk_buffer_write: write failed\n");
	    return -1;
	}
	bytes -= todo;
	pixelp += todo / sizeof(RGBpixel);
	ifp->i->if_seekpos += todo;
    }
    return count;
}


HIDDEN int
dsk_rmap(struct fb *ifp, ColorMap *cmap)
{
    int fd = ifp->i->if_fd;

    /* Reads on stdout make no sense.  Take reads from stdin. */
    if (fd == 1) fd = 0;

    if (ifp->i->if_seekpos != FILE_CMAP_SIZE &&
	bu_lseek(fd, (b_off_t)FILE_CMAP_SIZE, 0) == -1) {
	fb_log("disk_colormap_read : seek to %zd failed.\n", FILE_CMAP_SIZE);
	return -1;
    }
    if (read(fd, (char *) cmap, sizeof(ColorMap)) != sizeof(ColorMap)) {
	/* Not necessarily an error.  It is not required that a
	 * color map be saved and the standard map is not
	 * generally saved.
	 */
	return -1;
    }
    return 0;
}


HIDDEN int
dsk_wmap(struct fb *ifp, const ColorMap *cmap)
{
    if (cmap == (ColorMap *) NULL)
	/* Do not write default map to file. */
	return 0;
    if (fb_is_linear_cmap(cmap))
	return 0;
    if (bu_lseek(ifp->i->if_fd, (b_off_t)FILE_CMAP_SIZE, 0) == -1) {
	fb_log("disk_colormap_write : seek to %zd failed.\n", FILE_CMAP_SIZE);
	return -1;
    }
    if (write(ifp->i->if_fd, (char *)cmap, sizeof(ColorMap))
	!= sizeof(ColorMap)) {
	fb_log("disk_colormap_write : write failed.\n");
	return -1;
    }
    return 0;
}


HIDDEN int
dsk_help(struct fb *ifp)
{
    fb_log("Description: %s\n", disk_interface.i->if_type);
    fb_log("Device: %s\n", ifp->i->if_name);
    fb_log("Max width/height: %d %d\n",
	   disk_interface.i->if_max_width,
	   disk_interface.i->if_max_height);
    fb_log("Default width/height: %d %d\n",
	   disk_interface.i->if_width,
	   disk_interface.i->if_height);
    if (ifp->i->if_fd == 1) {
	fb_log("File \"-\" reads from stdin, writes to stdout\n");
    } else {
	fb_log("Note: you may have just created a disk file\n");
	fb_log("called \"%s\" by running this.\n", ifp->i->if_name);
    }

    return 0;
}


struct fb_impl disk_interface_impl = {
    0,
    FB_DISK_MAGIC,
    dsk_open,
    dsk_open_existing,
    dsk_close_existing,
    dsk_get_fbps,
    dsk_put_fbps,
    dsk_close,
    dsk_clear,
    dsk_read,
    dsk_write,
    dsk_rmap,
    dsk_wmap,
    fb_sim_view,	/* set view */
    fb_sim_getview,	/* get view */
    fb_null_setcursor,	/* define cursor */
    fb_sim_cursor,	/* set cursor */
    fb_sim_getcursor,	/* get cursor */
    fb_sim_readrect,
    fb_sim_writerect,
    fb_sim_bwreadrect,
    fb_sim_bwwriterect,
    dsk_configure_window,
    dsk_refresh,
    fb_null,		/* poll */
    fb_null,		/* flush */
    dsk_free,
    dsk_help,
    "Disk File Interface",
    FB_XMAXSCREEN,	/* the sky's really the limit here */
    FB_YMAXSCREEN,
    "filename",		/* not in list so name is safe */
    512,
    512,
    -1,			/* select fd */
    -1,
    1, 1,		/* zoom */
    256, 256,		/* window center */
    0, 0, 0,		/* cursor */
    PIXEL_NULL,
    PIXEL_NULL,
    PIXEL_NULL,
    -1,
    0,
    0L,
    0L,
    0,			/* debug */
    0,			/* refresh rate */
    NULL,
    NULL,
    0,
    NULL,
    {0}, /* u1 */
    {0}, /* u2 */
    {0}, /* u3 */
    {0}, /* u4 */
    {0}, /* u5 */
    {0}  /* u6 */
};

struct fb disk_interface = { &disk_interface_impl };

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

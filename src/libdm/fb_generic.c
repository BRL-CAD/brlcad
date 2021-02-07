/*                    F B _ G E N E R I C . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2021 United States Government as represented by
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

/** \defgroup fb Framebuffer
 * \ingroup libstruct fb */
/** @{ */
/** @file fb_generic.c
 *
 * The main table where framebuffers are initialized and prioritized
 * for run-time access.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include "png.h"

#include "bsocket.h"
#include "bio.h"

#include "bu/color.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/log.h"
#include "bu/exit.h"

#include "vmath.h"
#include "./include/private.h"
#include "icv.h"
#include "dm.h"

struct fb *fb_get()
{
    struct fb *new_fb = FB_NULL;
    BU_GET(new_fb, struct fb);
    BU_GET(new_fb->i, struct fb_impl);
    new_fb->i->if_name = NULL;
    return new_fb;
}

void fb_put(struct fb *ifp)
{
    if (ifp != FB_NULL) {
	BU_PUT(ifp->i, struct fb_impl);
	BU_PUT(ifp, struct fb);
    }
}

struct fb *
fb_open_existing(const char *file, int width, int height, struct fb_platform_specific *fb_p)
{
    struct fb *ifp;
    ifp = (struct fb *)calloc(sizeof(struct fb), 1);
    ifp->i = (struct fb_impl *) calloc(sizeof(struct fb_impl), 1);
    fb_set_interface(ifp, file);
    fb_set_magic(ifp, FB_MAGIC);
    if (ifp->i->if_open_existing) ifp->i->if_open_existing(ifp, width, height, fb_p);
    return ifp;
}

int
fb_refresh(struct fb *ifp, int x, int y, int w, int h)
{
    return ifp->i->if_refresh(ifp, x, y, w, h);
}

int
fb_configure_window(struct fb *ifp, int width, int height)
{
    /* unknown/unset framebuffer */
    if (!ifp || !ifp->i->if_configure_window || width < 0 || height < 0) {
	return 0;
    }
    return ifp->i->if_configure_window(ifp, width, height);
}

void fb_set_name(struct fb *ifp, const char *name)
{
    if (!ifp) return;
    ifp->i->if_name = (char *)bu_malloc((unsigned)strlen(name)+1, "if_name");
    bu_strlcpy(ifp->i->if_name, name, strlen(name)+1);
}

const char *fb_get_name(const struct fb *ifp)
{
    if (!ifp) return NULL;
    return ifp->i->if_name;
}

long fb_get_pagebuffer_pixel_size(struct fb *ifp)
{
    if (!ifp) return 0;
    return ifp->i->if_ppixels;
}

int fb_is_set_fd(struct fb *ifp, fd_set *infds)
{
    if (!ifp) return 0;
    if (!infds) return 0;
    if (!ifp->i->if_selfd) return 0;
    if (ifp->i->if_selfd <= 0) return 0;
    return FD_ISSET(ifp->i->if_selfd, infds);
}

int fb_set_fd(struct fb *ifp, fd_set *select_list)
{
    if (!ifp) return 0;
    if (!select_list) return 0;
    if (!ifp->i->if_selfd) return 0;
    if (ifp->i->if_selfd <= 0) return 0;
    FD_SET(ifp->i->if_selfd, select_list);
    return ifp->i->if_selfd;
}

int fb_clear_fd(struct fb *ifp, fd_set *list)
{
    if (!ifp) return 0;
    if (!list) return 0;
    if (!ifp->i->if_selfd) return 0;
    if (ifp->i->if_selfd <= 0) return 0;
    FD_CLR(ifp->i->if_selfd, list);
    return ifp->i->if_selfd;
}

void fb_set_magic(struct fb *ifp, uint32_t magic)
{
    if (!ifp) return;
    ifp->i->if_magic = magic;
}


char *fb_gettype(struct fb *ifp)
{
    return ifp->i->if_type;
}

int fb_getwidth(struct fb *ifp)
{
    return ifp->i->if_width;
}
int fb_getheight(struct fb *ifp)
{
    return ifp->i->if_height;
}

int fb_get_max_width(struct fb *ifp)
{
    return ifp->i->if_max_width;
}
int fb_get_max_height(struct fb *ifp)
{
    return ifp->i->if_max_height;
}


int fb_poll(struct fb *ifp)
{
    return (*ifp->i->if_poll)(ifp);
}

long fb_poll_rate(struct fb *ifp)
{
    return ifp->i->if_poll_refresh_rate;
}

int fb_help(struct fb *ifp)
{
    return (*ifp->i->if_help)(ifp);
}
int fb_free(struct fb *ifp)
{
    return (*ifp->i->if_free)(ifp);
}
int fb_clear(struct fb *ifp, unsigned char *pp)
{
    return (*ifp->i->if_clear)(ifp, pp);
}
ssize_t fb_read(struct fb *ifp, int x, int y, unsigned char *pp, size_t count)
{
    return (*ifp->i->if_read)(ifp, x, y, pp, count);
}
ssize_t fb_write(struct fb *ifp, int x, int y, const unsigned char *pp, size_t count)
{
    return (*ifp->i->if_write)(ifp, x, y, pp, count);
}
int fb_rmap(struct fb *ifp, ColorMap *cmap)
{
    return (*ifp->i->if_rmap)(ifp, cmap);
}
int fb_wmap(struct fb *ifp, const ColorMap *cmap)
{
    return (*ifp->i->if_wmap)(ifp, cmap);
}
int fb_view(struct fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    return (*ifp->i->if_view)(ifp, xcenter, ycenter, xzoom, yzoom);
}
int fb_getview(struct fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    return (*ifp->i->if_getview)(ifp, xcenter, ycenter, xzoom, yzoom);
}
int fb_setcursor(struct fb *ifp, const unsigned char *bits, int xb, int yb, int xo, int yo)
{
    return (*ifp->i->if_setcursor)(ifp, bits, xb, yb, xo, yo);
}
int fb_cursor(struct fb *ifp, int mode, int x, int y)
{
    return (*ifp->i->if_cursor)(ifp, mode, x, y);
}
int fb_getcursor(struct fb *ifp, int *mode, int *x, int *y)
{
    return (*ifp->i->if_getcursor)(ifp, mode, x, y);
}
int fb_readrect(struct fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
    return (*ifp->i->if_readrect)(ifp, xmin, ymin, width, height, pp);
}
int fb_writerect(struct fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    return (*ifp->i->if_writerect)(ifp, xmin, ymin, width, height, pp);
}
int fb_bwreadrect(struct fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
    return (*ifp->i->if_bwreadrect)(ifp, xmin, ymin, width, height, pp);
}
int fb_bwwriterect(struct fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    return (*ifp->i->if_bwwriterect)(ifp, xmin, ymin, width, height, pp);
}


#define Malloc_Bomb(_bytes_)					\
    fb_log("\"%s\"(%d) : allocation of %lu bytes failed.\n",	\
	   __FILE__, __LINE__, _bytes_)

/**
 * Disk interface enable flag.  Used so the remote daemon
 * can turn off the disk interface.
 */
int _fb_disk_enable = 1;


/**
 * Filler for fb function slots not used by a particular device
 */
int fb_null(struct fb *ifp)
{
    if (ifp) {
	FB_CK_FB(ifp->i);
    }

    return 0;
}


/**
 * Used by if_*.c routines that don't have programmable cursor patterns.
 */
int fb_null_setcursor(struct fb *ifp, const unsigned char *UNUSED(bits), int UNUSED(xbits), int UNUSED(ybits), int UNUSED(xorig), int UNUSED(yorig))
{
    if (ifp) {
	FB_CK_FB(ifp->i);
    }

    return 0;
}


int
fb_close(struct fb *ifp)
{
    int i;

    FB_CK_FB(ifp->i);
    fb_flush(ifp);
    if ((i=(*ifp->i->if_close)(ifp)) <= -1) {
	fb_log("fb_close: can not close device \"%s\", ret=%d.\n",
	       ifp->i->if_name, i);
	return -1;
    }
    if (ifp->i->if_pbase != PIXEL_NULL)
	free((void *) ifp->i->if_pbase);
    free((void *) ifp->i->if_name);
    free((void *) ifp->i);
    free((void *) ifp);
    return 0;
}


int
fb_close_existing(struct fb *ifp)
{
    int status = 0;
    if (!ifp)
	return 0;

    FB_CK_FB(ifp->i);

    fb_flush(ifp);

    /* FIXME: these should be callbacks, not listed directly */

    status = ifp->i->if_close_existing(ifp);

    if (status  <= -1) {
	fb_log("fb_close_existing: cannot close device \"%s\", ret=%d.\n", ifp->i->if_name, status);
	return BRLCAD_ERROR;
    }
    fb_put(ifp);
    return BRLCAD_OK;
}


/**
 * Check for a color map being linear in the upper 8 bits of R, G, and
 * B.  Returns 1 for linear map, 0 for non-linear map (i.e.,
 * non-identity map).
 */
int
fb_is_linear_cmap(register const ColorMap *cmap)
{
    register int i;

    for (i=0; i<256; i++) {
	if (cmap->cm_red[i]>>8 != i) return 0;
	if (cmap->cm_green[i]>>8 != i) return 0;
	if (cmap->cm_blue[i]>>8 != i) return 0;
    }
    return 1;
}


void
fb_make_linear_cmap(register ColorMap *cmap)
{
    register int i;

    for (i=0; i<256; i++) {
	cmap->cm_red[i] = i<<8;
	cmap->cm_green[i] = i<<8;
	cmap->cm_blue[i] = i<<8;
    }
}

static void
fb_cmap_crunch(RGBpixel (*scan_buf), int pixel_ct, ColorMap *cmap)
{
    unsigned short	*rp = cmap->cm_red;
    unsigned short	*gp = cmap->cm_green;
    unsigned short	*bp = cmap->cm_blue;

    /* noalias ? */
    for (; pixel_ct > 0; pixel_ct--, scan_buf++ )  {
	(*scan_buf)[RED] = rp[(*scan_buf)[RED]] >> 8;
	(*scan_buf)[GRN] = gp[(*scan_buf)[GRN]] >> 8;
	(*scan_buf)[BLU] = bp[(*scan_buf)[BLU]] >> 8;
    }
}

int
fb_write_fp(struct fb *ifp, FILE *fp, int req_width, int req_height, int crunch, int inverse, struct bu_vls *result)
{
    unsigned char *scanline;	/* 1 scanline pixel buffer */
    int scanbytes;		/* # of bytes of scanline */
    int scanpix;		/* # of pixels of scanline */
    ColorMap cmap;		/* libfb color map */

    scanpix = req_width;
    scanbytes = scanpix * sizeof(RGBpixel);
    if ((scanline = (unsigned char *)malloc(scanbytes)) == RGBPIXEL_NULL) {
	bu_vls_printf(result, "fb_write_to_pix_fp: malloc(%d) failure", scanbytes);
	return BRLCAD_ERROR;
    }

    if (req_height > fb_getheight(ifp))
	req_height = fb_getheight(ifp);
    if (req_width > fb_getwidth(ifp))
	req_width = fb_getwidth(ifp);

    if (crunch) {
	if (fb_rmap(ifp, &cmap) == -1) {
	    crunch = 0;
	} else if (fb_is_linear_cmap(&cmap)) {
	    crunch = 0;
	}
    }

    if (!inverse) {
	int y;

	/* Regular -- read bottom to top */
	for (y=0; y < req_height; y++) {
	    fb_read(ifp, 0, y, scanline, req_width);
	    if (crunch)
		fb_cmap_crunch((RGBpixel *)scanline, scanpix, &cmap);
	    if (fwrite((char *)scanline, scanbytes, 1, fp) != 1) {
		perror("fwrite");
		break;
	    }
	}
    } else {
	int y;

	/* Inverse -- read top to bottom */
	for (y = req_height-1; y >= 0; y--) {
	    fb_read(ifp, 0, y, scanline, req_width);
	    if (crunch)
		fb_cmap_crunch((RGBpixel *)scanline, scanpix, &cmap);
	    if (fwrite((char *)scanline, scanbytes, 1, fp) != 1) {
		perror("fwrite");
		break;
	    }
	}
    }

    bu_free((void *)scanline, "fb_write_to_pix_fp(): scanline");
    return BRLCAD_OK;
}

/*
 * Throw bytes away.  Use reads into scanline buffer if a pipe, else seek.
 */
static int
fb_skip_bytes(int fd, b_off_t num, int fileinput, int scanbytes, unsigned char *scanline)
{
    int n, tries;

    if (fileinput) {
	(void)bu_lseek(fd, num, 1);
	return 0;
    }

    while (num > 0) {
	tries = num > scanbytes ? scanbytes : num;
	n = read(fd, scanline, tries);
	if (n <= 0) {
	    return -1;
	}
	num -= n;
    }
    return 0;
}


int
fb_read_fd(struct fb *ifp, int fd, int file_width, int file_height, int file_xoff, int file_yoff, int scr_width, int scr_height, int scr_xoff, int scr_yoff, int fileinput, char *file_name, int one_line_only, int multiple_lines, int autosize, int inverse, int clear, int zoom, struct bu_vls *UNUSED(result))
{
    int y;
    int xout, yout, n, m, xstart, xskip;
    unsigned char *scanline;	/* 1 scanline pixel buffer */
    int scanbytes;		/* # of bytes of scanline */
    int scanpix;		/* # of pixels of scanline */

    /* autosize input? */
    if (fileinput && autosize) {
	size_t w, h;
	if (fb_common_file_size(&w, &h, file_name, 3)) {
	    file_width = w;
	    file_height = h;
	} else {
	    fprintf(stderr, "pix-fb: unable to autosize\n");
	}
    }

    /* If screen size was not set, track the file size */
    if (scr_width == 0)
	scr_width = file_width;
    if (scr_height == 0)
	scr_height = file_height;

    /* Get the screen size we were given */
    scr_width = fb_getwidth(ifp);
    scr_height = fb_getheight(ifp);

    /* compute number of pixels to be output to screen */
    if (scr_xoff < 0) {
	xout = scr_width + scr_xoff;
	xskip = (-scr_xoff);
	xstart = 0;
    } else {
	xout = scr_width - scr_xoff;
	xskip = 0;
	xstart = scr_xoff;
    }

    if (xout < 0)
	bu_exit(0, NULL);			/* off screen */
    if ((size_t)xout > (size_t)(file_width-file_xoff))
	xout = (file_width-file_xoff);
    scanpix = xout;				/* # pixels on scanline */

    if (inverse)
	scr_yoff = (-scr_yoff);

    yout = scr_height - scr_yoff;
    if (yout < 0)
	bu_exit(0, NULL);			/* off screen */
    if ((size_t)yout > (size_t)(file_height-file_yoff))
	yout = (file_height-file_yoff);

    /* Only in the simplest case use multi-line writes */
    if (!one_line_only
	&& multiple_lines > 0
	&& !inverse
	&& !zoom
	&& (size_t)xout == (size_t)file_width
	&& (size_t)file_width <= (size_t)scr_width)
    {
	scanpix *= multiple_lines;
    }

    scanbytes = scanpix * sizeof(RGBpixel);
    if ((scanline = (unsigned char *)malloc(scanbytes)) == RGBPIXEL_NULL) {
	fprintf(stderr,
		"pix-fb:  malloc(%d) failure for scanline buffer\n",
		scanbytes);
	bu_exit(2, NULL);
    }

    if (clear) {
	fb_clear(ifp, PIXEL_NULL);
    }
    if (zoom) {
	/* Zoom in, and center the display.  Use square zoom. */
	int zoomit;
	zoomit = scr_width/xout;
	if (scr_height/yout < zoomit) zoomit = scr_height/yout;
	if (inverse) {
	    fb_view(ifp,
		    scr_xoff+xout/2, scr_height-1-(scr_yoff+yout/2),
		    zoomit, zoomit);
	} else {
	    fb_view(ifp,
		    scr_xoff+xout/2, scr_yoff+yout/2,
		    zoomit, zoomit);
	}
    }

    if (file_yoff != 0) fb_skip_bytes(fd, (b_off_t)file_yoff*(b_off_t)file_width*sizeof(RGBpixel), fileinput, scanbytes, scanline);

    if (multiple_lines) {
	/* Bottom to top with multi-line reads & writes */
	unsigned long height;
	for (y = scr_yoff; y < scr_yoff + yout; y += multiple_lines) {
	    n = bu_mread(fd, (char *)scanline, scanbytes);
	    if (n <= 0) break;
	    height = multiple_lines;
	    if (n != scanbytes) {
		height = (n/sizeof(RGBpixel)+xout-1)/xout;
		if (height <= 0) break;
	    }
	    /* Don't over-write */
	    if ((size_t)(y + height) > (size_t)(scr_yoff + yout))
		height = scr_yoff + yout - y;
	    if (height <= 0) break;
	    m = fb_writerect(ifp, scr_xoff, y,
			     file_width, height,
			     scanline);
	    if ((size_t)m != file_width*height) {
		fprintf(stderr,
			"pix-fb: fb_writerect(x=%d, y=%d, w=%lu, h=%lu) failure, ret=%d, s/b=%d\n",
			scr_xoff, y,
			(unsigned long)file_width, height, m, scanbytes);
	    }
	}
    } else if (!inverse) {
	/* Normal way -- bottom to top */
	for (y = scr_yoff; y < scr_yoff + yout; y++) {
	    if (y < 0 || y > scr_height) {
		fb_skip_bytes(fd, (b_off_t)file_width*sizeof(RGBpixel), fileinput, scanbytes, scanline);
		continue;
	    }
	    if (file_xoff+xskip != 0)
		fb_skip_bytes(fd, (b_off_t)(file_xoff+xskip)*sizeof(RGBpixel), fileinput, scanbytes, scanline);
	    n = bu_mread(fd, (char *)scanline, scanbytes);
	    if (n <= 0) break;
	    m = fb_write(ifp, xstart, y, scanline, xout);
	    if (m != xout) {
		fprintf(stderr,
			"pix-fb: fb_write(x=%d, y=%d, npix=%d) ret=%d, s/b=%d\n",
			scr_xoff, y, xout,
			m, xout);
	    }
	    /* slop at the end of the line? */
	    if ((size_t)file_xoff+xskip+scanpix < (size_t)file_width)
		fb_skip_bytes(fd, (b_off_t)(file_width-file_xoff-xskip-scanpix)*sizeof(RGBpixel), fileinput, scanbytes, scanline);
	}
    } else {
	/* Inverse -- top to bottom */
	for (y = scr_height-1-scr_yoff; y >= scr_height-scr_yoff-yout; y--) {
	    if (y < 0 || y >= scr_height) {
		fb_skip_bytes(fd, (b_off_t)file_width*sizeof(RGBpixel), fileinput, scanbytes, scanline);
		continue;
	    }
	    if (file_xoff+xskip != 0)
		fb_skip_bytes(fd, (b_off_t)(file_xoff+xskip)*sizeof(RGBpixel), fileinput, scanbytes, scanline);
	    n = bu_mread(fd, (char *)scanline, scanbytes);
	    if (n <= 0) break;
	    m = fb_write(ifp, xstart, y, scanline, xout);
	    if (m != xout) {
		fprintf(stderr,
			"pix-fb: fb_write(x=%d, y=%d, npix=%d) ret=%d, s/b=%d\n",
			scr_xoff, y, xout,
			m, xout);
	    }
	    /* slop at the end of the line? */
	    if ((size_t)file_xoff+xskip+scanpix < (size_t)file_width)
		fb_skip_bytes(fd, (b_off_t)(file_width-file_xoff-xskip-scanpix)*sizeof(RGBpixel), fileinput, scanbytes, scanline);
	}
    }
    free(scanline);
    return BRLCAD_OK;
}


int
fb_read_icv(struct fb *ifp, icv_image_t *img_in, int file_xoff_in, int file_yoff_in, int file_maxwidth_in, int file_maxheight_in, int scr_xoff, int scr_yoff, int clear, int zoom, int inverse, int one_line_only, int multiple_lines, struct bu_vls *result)
{
    /* Sanity */
    if (!ifp || !img_in) {
	return BRLCAD_ERROR;
    }

    int y;
    int xout, yout, m, xstart;
    unsigned char **scanline;	/* 1 scanline pixel buffer */
    int scanbytes;		/* # of bytes of scanline */
    int scanpix;		/* # of pixels of scanline */
    int scr_width;
    int scr_height;

    /* Make a copy so we can edit if the options require */
    icv_image_t *img = icv_create(img_in->width, img_in->height, img_in->color_space);
    memcpy(img->data, img_in->data, img_in->width * img_in->height * img_in->channels * sizeof(double));

    int file_xoff = file_xoff_in;
    int file_yoff = file_yoff_in;
    int file_maxwidth = file_maxwidth_in;
    int file_maxheight = file_maxheight_in;

    /* Crop the image, if the file_ variables indicate we need to */
    if (file_xoff || file_yoff || file_maxwidth || file_maxheight) {
	file_maxwidth = (file_maxwidth) ? file_maxwidth : (int)img->width - file_xoff;
	file_maxheight = (file_maxheight) ? file_maxheight : (int)img->height - file_yoff;
	icv_rect(img, file_xoff, file_yoff, file_maxwidth, file_maxheight);
	// After resize, file offsets are zero. TODO - simplify below logic
	// to eliminate references to these variables, they should no longer
	// be needed...
	file_xoff = 0;
	file_yoff = 0;
    }

    unsigned char *data = icv_data2uchar(img);
     /* create rows array */
    scanline = (unsigned char **)bu_calloc(img->height, sizeof(unsigned char *), "scanline");
    for (unsigned int i=0; i<img->height; i++) {
	scanline[img->height - i - 1] = data+(i*img->width*3);
    }

    /* Get the screen size we were given */
    scr_width = fb_getwidth(ifp);
    scr_height = fb_getheight(ifp);

    /* compute number of pixels to be output to screen */
    if (scr_xoff < 0) {
	xout = scr_width + scr_xoff;
	xstart = 0;
    } else {
	xout = scr_width - scr_xoff;
	xstart = scr_xoff;
    }

    if (xout < 0) {
	bu_free(data, "unsigned char image data");
	bu_free(scanline, "scanline");
	icv_destroy(img);
	return BRLCAD_OK;
    }
    V_MIN(xout, (int)img->width);
    scanpix = xout;				/* # pixels on scanline */

    if (inverse)
	scr_yoff = (-scr_yoff);

    yout = scr_height - scr_yoff;
    if (yout < 0) {
	bu_free(data, "unsigned char image data");
	bu_free(scanline, "scanline");
	icv_destroy(img);
	return BRLCAD_OK;
    }
    V_MIN(yout, (int)img->height);

    /* Only in the simplest case use multi-line writes */
    if (!one_line_only && multiple_lines > 0 && !inverse && !zoom &&
	xout == (int)img->width && file_xoff == 0 &&
	(int)img->width <= scr_width) {
	scanpix *= multiple_lines;
    }

    scanbytes = scanpix * sizeof(RGBpixel);

    if (clear) {
	fb_clear(ifp, PIXEL_NULL);
    }
    if (zoom) {
	/* Zoom in, and center the display.  Use square zoom. */
	int newzoom;
	newzoom = scr_width/xout;
	V_MIN(newzoom, scr_height/yout);

	if (inverse) {
	    fb_view(ifp, scr_xoff+xout/2, scr_height-1-(scr_yoff+yout/2), newzoom, newzoom);
	} else {
	    fb_view(ifp, scr_xoff+xout/2, scr_yoff+yout/2, newzoom, newzoom);
	}
    } else {
	/* We may need to reset the view if we have previously zoomed but now
	 * have a command that didn't pass the flag */
	fb_view(ifp, scr_width/2, scr_height/2, 1, 1);
    }

    if (multiple_lines) {
	/* Bottom to top with multi-line reads & writes */
	int height=img->height;
	for (y = scr_yoff; y < scr_yoff + yout; y += multiple_lines) {
	    /* Don't over-write */
	    if (y + height > scr_yoff + yout)
		height = scr_yoff + yout - y;
	    if (height <= 0) break;
	    m = fb_writerect(ifp, scr_xoff, y,
			     img->width, height,
			     scanline[file_yoff++]);
	    if (m != (int)img->width*height) {
		bu_vls_printf(result,
			      "icv-fb: fb_writerect(x=%d, y=%d, w=%d, h=%d) failure, ret=%d, s/b=%d\n",
			      scr_xoff, y,
			      (int)img->width, height, m, scanbytes);
	    }
	}
    } else if (!inverse) {
	/* Normal way -- bottom to top */
	int line=img->height-file_yoff-1;
	for (y = scr_yoff; y < scr_yoff + yout; y++) {
	    m = fb_write(ifp, xstart, y, scanline[line--]+(3*file_xoff), xout);
	    if (m != xout) {
		bu_vls_printf(result,
			      "icv-fb: fb_write(x=%d, y=%d, npix=%d) ret=%d, s/b=%d\n",
			      scr_xoff, y, xout,
			      m, xout);
	    }
	}
    } else {
	/* Inverse -- top to bottom */
	int line=img->height-file_yoff-1;
	for (y = scr_height-1-scr_yoff; y >= scr_height-scr_yoff-yout; y--) {
	    m = fb_write(ifp, xstart, y, scanline[line--]+(3*file_xoff), xout);
	    if (m != xout) {
		bu_vls_printf(result,
			      "icv-fb: fb_write(x=%d, y=%d, npix=%d) ret=%d, s/b=%d\n",
			      scr_xoff, y, xout,
			      m, xout);
	    }
	}
    }

    bu_free(data, "unsigned char image data");
    bu_free(scanline, "scanline");
    icv_destroy(img);
    return BRLCAD_OK;
}

icv_image_t *
fb_write_icv(struct fb *ifp, int UNUSED(scr_xoff), int UNUSED(scr_yoff), int UNUSED(width), int UNUSED(height))
{
    icv_image_t *fbimg = icv_create(fb_getwidth(ifp), fb_getheight(ifp), ICV_COLOR_SPACE_RGB);

    unsigned char *scanline = (unsigned char *)bu_calloc(3 * fb_getwidth(ifp), sizeof(char), "raw image");
    for (int y=0; y < fb_getheight(ifp); y++) {
	fb_read(ifp, 0, y, scanline, fb_getwidth(ifp));
	icv_writeline(fbimg, y, scanline, ICV_DATA_UCHAR);
    }

    return fbimg;
}

static png_color_16 def_backgrd={ 0, 0, 0, 0, 0 };

int
fb_read_png(struct fb *ifp, FILE *fp_in, int file_xoff, int file_yoff, int scr_xoff, int scr_yoff, int clear, int zoom, int inverse, int one_line_only, int multiple_lines, int verbose, int header_only, double def_screen_gamma, struct bu_vls *result)
{
    int y;
    int i;
    int xout, yout, m, xstart;
    png_structp png_p;
    png_infop info_p;
    char header[8];
    int bit_depth;
    int color_type;
    png_color_16p input_backgrd;
    double gammaval=1.0;
    int file_width, file_height;
    unsigned char *image;
    unsigned char **scanline;	/* 1 scanline pixel buffer */
    int scanbytes;		/* # of bytes of scanline */
    int scanpix;		/* # of pixels of scanline */
    int scr_width;
    int scr_height;

    if (fread(header, 8, 1, fp_in) != 1) {
	bu_vls_printf(result, "png-fb: ERROR: Failed while reading file header!!!\n");
	return BRLCAD_ERROR;
    }

    if (png_sig_cmp((png_bytep)header, 0, 8)) {
	bu_vls_printf(result,  "png-fb: This is not a PNG file!!!\n");
	return BRLCAD_ERROR;
    }

    png_p = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p) {
	bu_vls_printf(result,  "png-fb: png_create_read_struct() failed!!\n");
	return BRLCAD_ERROR;
    }

    info_p = png_create_info_struct(png_p);
    if (!info_p) {
	bu_vls_printf(result,  "png-fb: png_create_info_struct() failed!!\n");
	return BRLCAD_ERROR;
    }

    png_init_io(png_p, fp_in);

    png_set_sig_bytes(png_p, 8);

    png_read_info(png_p, info_p);

    color_type = png_get_color_type(png_p, info_p);

    png_set_expand(png_p);
    bit_depth = png_get_bit_depth(png_p, info_p);
    if (bit_depth == 16)
	png_set_strip_16(png_p);

    if (color_type == PNG_COLOR_TYPE_GRAY ||
	color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
	png_set_gray_to_rgb(png_p);

    file_width = png_get_image_width(png_p, info_p);
    file_height = png_get_image_height(png_p, info_p);

    if (verbose) {
	switch (color_type) {
	    case PNG_COLOR_TYPE_GRAY:
		bu_vls_printf(result, "color type: b/w (bit depth=%d)\n", bit_depth);
		break;
	    case PNG_COLOR_TYPE_GRAY_ALPHA:
		bu_vls_printf(result, "color type: b/w with alpha channel (bit depth=%d)\n", bit_depth);
		break;
	    case PNG_COLOR_TYPE_PALETTE:
		bu_vls_printf(result, "color type: color palette (bit depth=%d)\n", bit_depth);
		break;
	    case PNG_COLOR_TYPE_RGB:
		bu_vls_printf(result, "color type: RGB (bit depth=%d)\n", bit_depth);
		break;
	    case PNG_COLOR_TYPE_RGB_ALPHA:
		bu_vls_printf(result, "color type: RGB with alpha channel (bit depth=%d)\n", bit_depth);
		break;
	    default:
		bu_vls_printf(result, "Unrecognized color type (bit depth=%d)\n", bit_depth);
		break;
	}
	bu_vls_printf(result, "Image size: %d X %d\n", file_width, file_height);
    }

    if (header_only) {
	bu_vls_printf(result, "WIDTH=%d HEIGHT=%d\n", file_width, file_height);
	return BRLCAD_OK;
    }

    if (png_get_bKGD(png_p, info_p, &input_backgrd)) {
	if (verbose && (color_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
			color_type == PNG_COLOR_TYPE_RGB_ALPHA))
	    bu_vls_printf(result, "background color: %d %d %d\n", input_backgrd->red, input_backgrd->green, input_backgrd->blue);
	png_set_background(png_p, input_backgrd, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
    } else
	png_set_background(png_p, &def_backgrd, PNG_BACKGROUND_GAMMA_FILE, 0, 1.0);

    if (!png_get_gAMA(png_p, info_p, &gammaval))
	gammaval = 0.5;
    png_set_gamma(png_p, def_screen_gamma, gammaval);
    if (verbose)
	bu_vls_printf(result, "file gamma: %f, additional screen gamma: %f\n",
		      gammaval, def_screen_gamma);

    if (verbose) {
	if (png_get_interlace_type(png_p, info_p) == PNG_INTERLACE_NONE)
	    bu_vls_printf(result, "not interlaced\n");
	else
	    bu_vls_printf(result, "interlaced\n");
    }

    png_read_update_info(png_p, info_p);

    /* allocate memory for image */
    image = (unsigned char *)bu_calloc(1, file_width*file_height*3, "image");

    /* create rows array */
    scanline = (unsigned char **)bu_calloc(file_height, sizeof(unsigned char *), "scanline");
    for (i=0; i<file_height; i++)
	scanline[i] = image+(i*file_width*3);

    png_read_image(png_p, scanline);

    if (verbose) {
	png_timep mod_time;
	png_textp text;
	int num_text;

	png_read_end(png_p, info_p);
	if (png_get_text(png_p, info_p, &text, &num_text)) {
	    for (i=0; i<num_text; i++)
		bu_vls_printf(result, "%s: %s\n", text[i].key, text[i].text);
	}
	if (png_get_tIME(png_p, info_p, &mod_time))
	    bu_vls_printf(result, "Last modified: %d/%d/%d %d:%d:%d\n", mod_time->month, mod_time->day,
			  mod_time->year, mod_time->hour, mod_time->minute, mod_time->second);
    }

    /* Get the screen size we were given */
    scr_width = fb_getwidth(ifp);
    scr_height = fb_getheight(ifp);

    /* compute number of pixels to be output to screen */
    if (scr_xoff < 0) {
	xout = scr_width + scr_xoff;
	xstart = 0;
    } else {
	xout = scr_width - scr_xoff;
	xstart = scr_xoff;
    }

    if (xout < 0) {
	bu_free(image, "image");
	bu_free(scanline, "scanline");
	return BRLCAD_OK;
    }
    V_MIN(xout, (file_width-file_xoff));
    scanpix = xout;				/* # pixels on scanline */

    if (inverse)
	scr_yoff = (-scr_yoff);

    yout = scr_height - scr_yoff;
    if (yout < 0) {
	bu_free(image, "image");
	bu_free(scanline, "scanline");
	return BRLCAD_OK;
    }
    V_MIN(yout, (file_height-file_yoff));

    /* Only in the simplest case use multi-line writes */
    if (!one_line_only && multiple_lines > 0 && !inverse && !zoom &&
	xout == file_width && file_xoff == 0 &&
	file_width <= scr_width) {
	scanpix *= multiple_lines;
    }

    scanbytes = scanpix * sizeof(RGBpixel);

    if (clear) {
	fb_clear(ifp, PIXEL_NULL);
    }
    if (zoom) {
	/* Zoom in, and center the display.  Use square zoom. */
	int newzoom;
	newzoom = scr_width/xout;
	V_MIN(newzoom, scr_height/yout);

	if (inverse) {
	    fb_view(ifp,
		    scr_xoff+xout/2, scr_height-1-(scr_yoff+yout/2),
		    newzoom, newzoom);
	} else {
	    fb_view(ifp,
		    scr_xoff+xout/2, scr_yoff+yout/2,
		    newzoom, newzoom);
	}
    }

    if (multiple_lines) {
	/* Bottom to top with multi-line reads & writes */
	int height=file_height;
	for (y = scr_yoff; y < scr_yoff + yout; y += multiple_lines) {
	    /* Don't over-write */
	    if (y + height > scr_yoff + yout)
		height = scr_yoff + yout - y;
	    if (height <= 0) break;
	    m = fb_writerect(ifp, scr_xoff, y,
			     file_width, height,
			     scanline[file_yoff++]);
	    if (m != file_width*height) {
		bu_vls_printf(result,
			      "png-fb: fb_writerect(x=%d, y=%d, w=%d, h=%d) failure, ret=%d, s/b=%d\n",
			      scr_xoff, y,
			      file_width, height, m, scanbytes);
	    }
	}
    } else if (!inverse) {
	/* Normal way -- bottom to top */
	int line=file_height-file_yoff-1;
	for (y = scr_yoff; y < scr_yoff + yout; y++) {
	    m = fb_write(ifp, xstart, y, scanline[line--]+(3*file_xoff), xout);
	    if (m != xout) {
		bu_vls_printf(result,
			      "png-fb: fb_write(x=%d, y=%d, npix=%d) ret=%d, s/b=%d\n",
			      scr_xoff, y, xout,
			      m, xout);
	    }
	}
    } else {
	/* Inverse -- top to bottom */
	int line=file_height-file_yoff-1;
	for (y = scr_height-1-scr_yoff; y >= scr_height-scr_yoff-yout; y--) {
	    m = fb_write(ifp, xstart, y, scanline[line--]+(3*file_xoff), xout);
	    if (m != xout) {
		bu_vls_printf(result,
			      "png-fb: fb_write(x=%d, y=%d, npix=%d) ret=%d, s/b=%d\n",
			      scr_xoff, y, xout,
			      m, xout);
	    }
	}
    }

    bu_free(image, "image");
    bu_free(scanline, "scanline");
    return BRLCAD_OK;
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

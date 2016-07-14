/*                    F B _ G E N E R I C . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2014 United States Government as represented by
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
 * \ingroup libfb */
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

#include "bio.h"

#include "bu/color.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"

#include "fb_private.h"
#include "fb.h"

/**
 * First element of list is default device when no name given
 */
static
fb *_if_list[] = {
#ifdef IF_WGL
    &wgl_interface,
#endif
#ifdef IF_OGL
    &ogl_interface,
#endif
#ifdef IF_X
    &X24_interface,
#endif
#ifdef IF_TK
    &tk_interface,
#endif
#ifdef IF_QT
    &qt_interface,
#endif
#ifdef IF_OSGL
    &osgl_interface,
#endif

    &debug_interface,
/* never get any of the following by default */
    &stk_interface,
    &memory_interface,
    &null_interface,
    (fb *) 0
};

fb *fb_get()
{
    struct fb_internal *new_fb = FB_NULL;
    BU_GET(new_fb, struct fb_internal);
    new_fb->if_name = NULL;
    return new_fb;
}

void fb_put(fb *ifp)
{
    if (ifp != FB_NULL)
	BU_PUT(ifp, struct fb_internal);
}

void fb_set_interface(fb *ifp, const char *interface)
{
    int i = 0;
    if (!ifp) return;
    while (_if_list[i] != FB_NULL) {
	if (bu_strncmp(interface, _if_list[i]->if_name+5,
		    strlen(_if_list[i]->if_name-5)) == 0) {
	    /* found it, copy its struct in */
	    *ifp = *(_if_list[i]);
	    return;
	} else {
	    i++;
	}
    }
}

struct fb_platform_specific *
fb_get_platform_specific(uint32_t magic)
{
    int i = 0;
    if (!magic) return NULL;
    while (_if_list[i] != FB_NULL) {
	if (magic == _if_list[i]->type_magic) {
	    /* found it, get its specific struct */
	    return (*(_if_list[i])).if_existing_get(magic);
	} else {
	    i++;
	}
    }
    return NULL;
}

void
fb_put_platform_specific(struct fb_platform_specific *fb_p)
{
    int i = 0;
    if (!fb_p) return;
    while (_if_list[i] != FB_NULL) {
	if (fb_p->magic == _if_list[i]->type_magic) {
	    /* found it, clear its specific struct */
	    (*(_if_list[i])).if_existing_put(fb_p);
	    return;
	} else {
	    i++;
	}
    }
    return;
}

fb *
fb_open_existing(const char *file, int width, int height, struct fb_platform_specific *fb_p)
{
    fb *ifp = (fb *) calloc(sizeof(fb), 1);
    fb_set_interface(ifp, file);
    fb_set_magic(ifp, FB_MAGIC);
    ifp->if_open_existing(ifp, width, height, fb_p);
    return ifp;
}

int
fb_refresh(fb *ifp, int x, int y, int w, int h)
{
    return ifp->if_refresh(ifp, x, y, w, h);
}

int
fb_configure_window(fb *ifp, int width, int height)
{
    /* unknown/unset framebuffer */
    if (!ifp || !ifp->if_configure_window || width < 0 || height < 0) {
	return 0;
    }
    return ifp->if_configure_window(ifp, width, height);
}

void fb_set_name(fb *ifp, const char *name)
{
    if (!ifp) return;
    /*if (ifp->if_name) bu_free(ifp->if_name, "free pre-existing fb name");*/
    ifp->if_name = (char *)bu_malloc((unsigned)strlen(name)+1, "if_name");
    bu_strlcpy(ifp->if_name, name, strlen(name)+1);
}

char *fb_get_name(fb *ifp)
{
    if (!ifp) return NULL;
    return ifp->if_name;
}

long fb_get_pagebuffer_pixel_size(fb *ifp)
{
    if (!ifp) return 0;
    return ifp->if_ppixels;
}


int fb_is_set_fd(fb *ifp, fd_set *infds)
{
    if (!ifp) return 0;
    if (!infds) return 0;
    if (!ifp->if_selfd) return 0;
    if (ifp->if_selfd <= 0) return 0;
    return FD_ISSET(ifp->if_selfd, infds);
}

int fb_set_fd(fb *ifp, fd_set *select_list)
{
    if (!ifp) return 0;
    if (!select_list) return 0;
    if (!ifp->if_selfd) return 0;
    if (ifp->if_selfd <= 0) return 0;
    FD_SET(ifp->if_selfd, select_list);
    return ifp->if_selfd;
}

int fb_clear_fd(fb *ifp, fd_set *list)
{
    if (!ifp) return 0;
    if (!list) return 0;
    if (!ifp->if_selfd) return 0;
    if (ifp->if_selfd <= 0) return 0;
    FD_CLR(ifp->if_selfd, list);
    return ifp->if_selfd;
}

void fb_set_magic(fb *ifp, uint32_t magic)
{
    ifp->if_magic = magic;
}


char *fb_gettype(fb *ifp)
{
    return ifp->if_type;
}

int fb_getwidth(fb *ifp)
{
    return ifp->if_width;
}
int fb_getheight(fb *ifp)
{
    return ifp->if_height;
}

int fb_get_max_width(fb *ifp)
{
    return ifp->if_max_width;
}
int fb_get_max_height(fb *ifp)
{
    return ifp->if_max_height;
}


int fb_poll(fb *ifp)
{
    return (*ifp->if_poll)(ifp);
}

long fb_poll_rate(fb *ifp)
{
    return ifp->if_poll_refresh_rate;
}

int fb_help(fb *ifp)
{
    return (*ifp->if_help)(ifp);
}
int fb_free(fb *ifp)
{
    return (*ifp->if_free)(ifp);
}
int fb_clear(fb *ifp, unsigned char *pp)
{
    return (*ifp->if_clear)(ifp, pp);
}
ssize_t fb_read(fb *ifp, int x, int y, unsigned char *pp, size_t count)
{
    return (*ifp->if_read)(ifp, x, y, pp, count);
}
ssize_t fb_write(fb *ifp, int x, int y, const unsigned char *pp, size_t count)
{
    return (*ifp->if_write)(ifp, x, y, pp, count);
}
int fb_rmap(fb *ifp, ColorMap *cmap)
{
    return (*ifp->if_rmap)(ifp, cmap);
}
int fb_wmap(fb *ifp, const ColorMap *cmap)
{
    return (*ifp->if_wmap)(ifp, cmap);
}
int fb_view(fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    return (*ifp->if_view)(ifp, xcenter, ycenter, xzoom, yzoom);
}
int fb_getview(fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    return (*ifp->if_getview)(ifp, xcenter, ycenter, xzoom, yzoom);
}
int fb_setcursor(fb *ifp, const unsigned char *bits, int xb, int yb, int xo, int yo)
{
    return (*ifp->if_setcursor)(ifp, bits, xb, yb, xo, yo);
}
int fb_cursor(fb *ifp, int mode, int x, int y)
{
    return (*ifp->if_cursor)(ifp, mode, x, y);
}
int fb_getcursor(fb *ifp, int *mode, int *x, int *y)
{
    return (*ifp->if_getcursor)(ifp, mode, x, y);
}
int fb_readrect(fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
    return (*ifp->if_readrect)(ifp, xmin, ymin, width, height, pp);
}
int fb_writerect(fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    return (*ifp->if_writerect)(ifp, xmin, ymin, width, height, pp);
}
int fb_bwreadrect(fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
    return (*ifp->if_bwreadrect)(ifp, xmin, ymin, width, height, pp);
}
int fb_bwwriterect(fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    return (*ifp->if_bwwriterect)(ifp, xmin, ymin, width, height, pp);
}


#define Malloc_Bomb(_bytes_)					\
    fb_log("\"%s\"(%d) : allocation of %lu bytes failed.\n",	\
	   __FILE__, __LINE__, _bytes_)

/**
 * True if the non-null string s is all digits
 */
static int
fb_totally_numeric(const char *s)
{
    if (s == (char *)0 || *s == 0)
	return 0;

    while (*s) {
	if (*s < '0' || *s > '9')
	    return 0;
	s++;
    }

    return 1;
}


/**
 * Disk interface enable flag.  Used so the remote daemon
 * can turn off the disk interface.
 */
int _fb_disk_enable = 1;


/**
 * Filler for fb function slots not used by a particular device
 */
int fb_null(fb *ifp)
{
    if (ifp) {
	FB_CK_FB(ifp);
    }

    return 0;
}


/**
 * Used by if_*.c routines that don't have programmable cursor patterns.
 */
int fb_null_setcursor(fb *ifp, const unsigned char *UNUSED(bits), int UNUSED(xbits), int UNUSED(ybits), int UNUSED(xorig), int UNUSED(yorig))
{
    if (ifp) {
	FB_CK_FB(ifp);
    }

    return 0;
}



fb *
fb_open(const char *file, int width, int height)
{
    register fb *ifp;
    int i;

    if (width < 0 || height < 0)
	return FB_NULL;

    ifp = (fb *) calloc(sizeof(fb), 1);
    if (ifp == FB_NULL) {
	Malloc_Bomb(sizeof(fb));
	return FB_NULL;
    }
    if (file == NULL || *file == '\0') {
	/* No name given, check environment variable first.	*/
	if ((file = (const char *)getenv("FB_FILE")) == NULL || *file == '\0') {
	    /* None set, use first device as default */
	    *ifp = *(_if_list[0]);	/* struct copy */
	    file = ifp->if_name;
	    goto found_interface;
	}
    }
    /*
     * Determine what type of hardware the device name refers to.
     *
     * "file" can in general look like: hostname:/pathname/devname#
     *
     * If we have a ':' assume the remote interface
     * (We don't check to see if it's us. Good for debugging.)
     * else strip out "/path/devname" and try to look it up in the
     * device array.  If we don't find it assume it's a file.
     */
    i = 0;
    while (_if_list[i] != (fb *)NULL) {
	if (bu_strncmp(file, _if_list[i]->if_name,
		    strlen(_if_list[i]->if_name)) == 0) {
	    /* found it, copy its struct in */
	    *ifp = *(_if_list[i]);
	    goto found_interface;
	}
	i++;
    }

    /* Not in list, check special interfaces or disk files */
    /* "/dev/" protection! */
    if (bu_strncmp(file, "/dev/", 5) == 0) {
	fb_log("fb_open: no such device \"%s\".\n", file);
	free((void *) ifp);
	return FB_NULL;
    }

#ifdef IF_REMOTE
    if (fb_totally_numeric(file) || strchr(file, ':') != NULL) {
	/* We have a remote file name of the form <host>:<file>
	 * or a port number (which assumes localhost) */
	*ifp = remote_interface;
	goto found_interface;
    }
#endif /* IF_REMOTE */
    /* Assume it's a disk file */
    if (_fb_disk_enable) {
	*ifp = disk_interface;
    } else {
	fb_log("fb_open: no such device \"%s\".\n", file);
	free((void *) ifp);
	return FB_NULL;
    }

found_interface:
    /* Copy over the name it was opened by. */
    ifp->if_name = (char*)malloc((unsigned) strlen(file) + 1);
    if (ifp->if_name == (char *)NULL) {
	Malloc_Bomb(strlen(file) + 1);
	free((void *) ifp);
	return FB_NULL;
    }
    bu_strlcpy(ifp->if_name, file, strlen(file)+1);

    /* Mark OK by filling in magic number */
    ifp->if_magic = FB_MAGIC;

    if ((i=(*ifp->if_open)(ifp, file, width, height)) <= -1) {
	fb_log("fb_open: can't open device \"%s\", ret=%d.\n",
	       file, i);
	ifp->if_magic = 0;		/* sanity */
	free((void *) ifp->if_name);
	free((void *) ifp);
	return FB_NULL;
    }
    return ifp;
}


int
fb_close(fb *ifp)
{
    int i;

    FB_CK_FB(ifp);
    fb_flush(ifp);
    if ((i=(*ifp->if_close)(ifp)) <= -1) {
	fb_log("fb_close: can not close device \"%s\", ret=%d.\n",
	       ifp->if_name, i);
	return -1;
    }
    if (ifp->if_pbase != PIXEL_NULL)
	free((void *) ifp->if_pbase);
    free((void *) ifp->if_name);
    free((void *) ifp);
    return 0;
}


int
fb_close_existing(fb *ifp)
{
    int status = 0;
    if (!ifp)
	return 0;

    FB_CK_FB(ifp);

    fb_flush(ifp);

    /* FIXME: these should be callbacks, not listed directly */

    status = ifp->if_close_existing(ifp);

    if (status  <= -1) {
	fb_log("fb_close_existing: cannot close device \"%s\", ret=%d.\n", ifp->if_name, status);
	return BRLCAD_ERROR;
    }
    fb_put(ifp);
    return BRLCAD_OK;
}


/**
 * Generic Help.
 * Print out the list of available frame buffers.
 */
int
fb_genhelp(void)
{
    int i;

    i = 0;
    while (_if_list[i] != (fb *)NULL) {
	fb_log("%-12s  %s\n",
	       _if_list[i]->if_name,
	       _if_list[i]->if_type);
	i++;
    }

    /* Print the ones not in the device list */
#ifdef IF_REMOTE
    fb_log("%-12s  %s\n",
	   remote_interface.if_name,
	   remote_interface.if_type);
#endif
    if (_fb_disk_enable) {
	fb_log("%-12s  %s\n",
	       disk_interface.if_name,
	       disk_interface.if_type);
    }

    return 0;
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
fb_write_fp(fb *ifp, FILE *fp, int req_width, int req_height, int crunch, int inverse, struct bu_vls *result)
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
fb_skip_bytes(int fd, off_t num, int fileinput, int scanbytes, unsigned char *scanline)
{
    int n, tries;

    if (fileinput) {
	(void)lseek(fd, num, 1);
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
fb_read_fd(fb *ifp, int fd, int file_width, int file_height, int file_xoff, int file_yoff, int scr_width, int scr_height, int scr_xoff, int scr_yoff, int fileinput, char *file_name, int one_line_only, int multiple_lines, int autosize, int inverse, int clear, int zoom, struct bu_vls *UNUSED(result))
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

    if (file_yoff != 0) fb_skip_bytes(fd, (off_t)file_yoff*(off_t)file_width*sizeof(RGBpixel), fileinput, scanbytes, scanline);

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
		fb_skip_bytes(fd, (off_t)file_width*sizeof(RGBpixel), fileinput, scanbytes, scanline);
		continue;
	    }
	    if (file_xoff+xskip != 0)
		fb_skip_bytes(fd, (off_t)(file_xoff+xskip)*sizeof(RGBpixel), fileinput, scanbytes, scanline);
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
		fb_skip_bytes(fd, (off_t)(file_width-file_xoff-xskip-scanpix)*sizeof(RGBpixel), fileinput, scanbytes, scanline);
	}
    } else {
	/* Inverse -- top to bottom */
	for (y = scr_height-1-scr_yoff; y >= scr_height-scr_yoff-yout; y--) {
	    if (y < 0 || y >= scr_height) {
		fb_skip_bytes(fd, (off_t)file_width*sizeof(RGBpixel), fileinput, scanbytes, scanline);
		continue;
	    }
	    if (file_xoff+xskip != 0)
		fb_skip_bytes(fd, (off_t)(file_xoff+xskip)*sizeof(RGBpixel), fileinput, scanbytes, scanline);
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
		fb_skip_bytes(fd, (off_t)(file_width-file_xoff-xskip-scanpix)*sizeof(RGBpixel), fileinput, scanbytes, scanline);
	}
    }
    free(scanline);
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

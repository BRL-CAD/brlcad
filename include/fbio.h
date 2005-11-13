/*                         F B I O . H
 * BRL-CAD
 *
 * Copyright (C) 2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file fbio.h
 *
 *  BRL-CAD Framebuffer Library I/O Interfaces
 *
 *  Source -
 *	BRL-CAD Open Source
 *
 *  $Header$
 */

#ifndef __FBIO_H__
#define __FBIO_H__

#ifdef USE_PROTOTYPES
#	define	FB_ARGS(args)			args
#else
#	define	FB_ARGS(args)			()
#endif

/*
 *			R G B p i x e l
 *
 *  Format of disk pixels is .pix raw image files.
 *  Formerly used as arguments to many of the library routines,
 *  but has fallen into disuse due to the difficulties with
 *  ANSI function prototypes, and the fact that arrays are not
 *  real types in C.  The most notable problem is that of passing
 *  a pointer to an array of RGBpixel.  It looks doubly dimensioned,
 *  but isn't.
 */
typedef unsigned char RGBpixel[3];

#define	RED	0
#define	GRN	1
#define	BLU	2

/*
 *			C o l o r M a p
 *
 *  These generic color maps have up to 16 bits of significance,
 *  left-justified in a short.  Think of this as fixed-point values
 *  from 0 to 1.
 */
typedef struct  {
	unsigned short cm_red[256];
	unsigned short cm_green[256];
	unsigned short cm_blue[256];
} ColorMap;


#define PIXEL_NULL	(unsigned char *) 0
#define RGBPIXEL_NULL	(unsigned char *) 0
#define COLORMAP_NULL	(ColorMap *) 0
#define FBIO_NULL	(FBIO *) 0

#define FB_MAGIC	0xfbfb00fb
#define FB_CK_FBIO(_p)	FB_CKMAG(_p, FB_MAGIC, "FBIO" )


/*
 *			F B I O
 *
 *  A frame-buffer IO structure.
 *  One of these is allocated for each active framebuffer.
 *  A pointer to this structure is the first argument to all
 *  the library routines.
 */
typedef struct FBIO_ {
	long	if_magic;
	/* Static information: per device TYPE.	*/
	int	(*if_open)FB_ARGS((struct FBIO_ *ifp, char *file, int width, int height));		/* open device		*/
	int	(*if_close)FB_ARGS((struct FBIO_ *ifp));				/* close device		*/
	int	(*if_clear)FB_ARGS((struct FBIO_ *ifp, unsigned char *pp));		/* clear device 	*/
	int	(*if_read)FB_ARGS((struct FBIO_ *ifp, int x, int y, unsigned char *pp, int count));		/* read pixels		*/
	int	(*if_write)FB_ARGS((struct FBIO_ *ifp, int x, int y, const unsigned char *pp, int count));	/* write pixels		*/
	int	(*if_rmap)FB_ARGS((struct FBIO_ *ifp, ColorMap *cmap));		/* read colormap 	*/
	int	(*if_wmap)FB_ARGS((struct FBIO_ *ifp, const ColorMap *cmap));		/* write colormap 	*/
	int	(*if_view)FB_ARGS((struct FBIO_ *ifp, int xcent, int ycent, int xzoom, int yzoom));		/* set view		*/
	int	(*if_getview)FB_ARGS((struct FBIO_ *ifp, int *xcent, int *ycent, int *xzoom, int *yzoom));	/* get view		*/
	int	(*if_setcursor)FB_ARGS((struct FBIO_ *ifp, const unsigned char *bits, int xb, int yb, int xo, int yo));	/* define cursor 	*/
	int	(*if_cursor)FB_ARGS((struct FBIO_ *ifp, int mode, int x, int y));		/* set cursor		*/
	int	(*if_getcursor)FB_ARGS((struct FBIO_ *ifp, int *mode, int *x, int *y));	/* get cursor		*/
	int	(*if_readrect)FB_ARGS((struct FBIO_ *ifp, int xmin, int ymin, int width, int height, unsigned char *pp));	/* read rectangle 	*/
	int	(*if_writerect)FB_ARGS((struct FBIO_ *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp));	/* write rectangle 	*/
	int	(*if_bwreadrect)FB_ARGS((struct FBIO_ *ifp, int xmin, int ymin, int width, int height, unsigned char *pp));	/* read monochrome rectangle 	*/
	int	(*if_bwwriterect)FB_ARGS((struct FBIO_ *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp));	/* write rectangle 	*/
	int	(*if_poll)FB_ARGS((struct FBIO_ *ifp));		/* handle events 	*/
	int	(*if_flush)FB_ARGS((struct FBIO_ *ifp));	/* flush output 	*/
	int	(*if_free)FB_ARGS((struct FBIO_ *ifp));		/* free resources 	*/
	int	(*if_help)FB_ARGS((struct FBIO_ *ifp));		/* print useful info	*/
	char	*if_type;		/* what "open" calls it	*/
	int	if_max_width;		/* max device width	*/
	int	if_max_height;		/* max device height	*/
	/* Dynamic information: per device INSTANCE. */
	char	*if_name;	/* what the user called it */
	int	if_width;	/* current values */
	int	if_height;
	int	if_selfd;	/* select(fd) for input events if >= 0 */
	/* Internal information: needed by the library.	*/
	int	if_fd;		/* internal file descriptor */
	int	if_xzoom;	/* zoom factors */
	int	if_yzoom;
	int	if_xcenter;	/* pan position */
	int	if_ycenter;
	int	if_cursmode;	/* cursor on/off */
	int	if_xcurs;	/* cursor position */
	int	if_ycurs;
	unsigned char *if_pbase;/* Address of malloc()ed page buffer.	*/
	unsigned char *if_pcurp;/* Current pointer into page buffer.	*/
	unsigned char *if_pendp;/* End of page buffer.			*/
	int	if_pno;		/* Current "page" in memory.		*/
	int	if_pdirty;	/* Page modified flag.			*/
	long	if_pixcur;	/* Current pixel number in framebuffer. */
	long	if_ppixels;	/* Sizeof page buffer (pixels).		*/
	int	if_debug;	/* Buffered IO debug flag.		*/
	/* State variables for individual interface modules */
	union	{
		char	*p;
		long	l;
	} u1, u2, u3, u4, u5, u6;
} FBIO;

/* declare all the possible interfaces */
extern FBIO remote_interface;	/* not in list[] */
extern FBIO adage_interface;
extern FBIO sun_interface;
extern FBIO sgi_interface;
extern FBIO ogl_interface;
extern FBIO rat_interface;
extern FBIO ug_interface;
extern FBIO X24_interface;
extern FBIO X_interface;
extern FBIO ptty_interface;
extern FBIO abekas_interface;
extern FBIO ts_interface;

/* Always included */
extern FBIO debug_interface, disk_interface, stk_interface;
extern FBIO memory_interface, null_interface;

#endif  /* __FBIO_H__ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

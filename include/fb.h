/*                            F B . H
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
/** @file fb.h
 *
 *  BRL "Generic" Framebuffer Library Interface Defines.
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 *  Include Sequencing -
 *	#include <stdio.h>
 *	#include "machine.h"
 *	#include "fb.h"
 *
 *  Libraries Used -
 *	LIBFB LIBFB_LIBES -lc
 *
 *  $Header$
 */

#ifndef FB_H
#define FB_H seen

/* Needed for fd_set */
#if defined (HAVE_SYS_SELECT_H)
#  include <sys/select.h>
#else
#  if defined(HAVE_SYS_TYPES_H)
#    include <sys/types.h>
#  endif
#  if defined(HAVE_SYS_TIME_H)
#    include <sys/time.h>
#  endif
#  if defined(HAVE_UNISTD_H)
#    include <unistd.h>
#  else
#    if defined(HAVE_SYS_UNISTD_H)
#      include <sys/unistd.h>
#    endif
#  endif
#endif

#ifndef FB_EXPORT
#   if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#      ifdef FB_EXPORT_DLL
#         define FB_EXPORT __declspec(dllexport)
#      else
#         define FB_EXPORT __declspec(dllimport)
#      endif
#   else
#      define FB_EXPORT
#   endif
#endif

/*
 *			R G B p i x e l
 *
 *  Format of disk pixels in .pix files.
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

#ifdef USE_PROTOTYPES
#	define	FB_ARGS(args)			args
#else
#	define	FB_ARGS(args)			()
#endif

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

#define FB_MAGIC	0xfbfb00fb

#define PIXEL_NULL	(unsigned char *) 0
#define RGBPIXEL_NULL	(unsigned char *) 0
#define COLORMAP_NULL	(ColorMap *) 0
#define FBIO_NULL	(FBIO *) 0

/* Library entry points which are macros. */
#define fb_gettype(_ifp)		(_ifp->if_type)
#define fb_getwidth(_ifp)		(_ifp->if_width)
#define fb_getheight(_ifp)		(_ifp->if_height)
#define fb_poll(_ifp)			(*_ifp->if_poll)(_ifp)
#define fb_help(_ifp)			(*_ifp->if_help)(_ifp)
#define fb_free(_ifp)			(*_ifp->if_free)(_ifp)
#define fb_clear(_ifp,_pp)		(*_ifp->if_clear)(_ifp,_pp)
#define fb_read(_ifp,_x,_y,_pp,_ct)	(*_ifp->if_read)(_ifp,_x,_y,_pp,_ct)
#define fb_write(_ifp,_x,_y,_pp,_ct)	(*_ifp->if_write)(_ifp,_x,_y,_pp,_ct)
#define fb_rmap(_ifp,_cmap)		(*_ifp->if_rmap)(_ifp,_cmap)
#define fb_wmap(_ifp,_cmap)		(*_ifp->if_wmap)(_ifp,_cmap)
#define fb_view(_ifp,_xc,_yc,_xz,_yz)	(*_ifp->if_view)(_ifp,_xc,_yc,_xz,_yz)
#define fb_getview(_ifp,_xcp,_ycp,_xzp,_yzp) \
		(*_ifp->if_getview)(_ifp,_xcp,_ycp,_xzp,_yzp)
#define fb_setcursor(_ifp,_bits,_xb,_yb,_xo,_yo) \
		(*_ifp->if_setcursor)(_ifp,_bits,_xb,_yb,_xo,_yo)
#define fb_cursor(_ifp,_mode,_x,_y)	(*_ifp->if_cursor)(_ifp,_mode,_x,_y)
#define fb_getcursor(_ifp,_modep,_xp,_yp) \
		(*_ifp->if_getcursor)(_ifp,_modep,_xp,_yp)
#define fb_readrect(_ifp,_xmin,_ymin,_width,_height,_pp) \
		(*_ifp->if_readrect)(_ifp,_xmin,_ymin,_width,_height,_pp)
#define fb_writerect(_ifp,_xmin,_ymin,_width,_height,_pp) \
		(*_ifp->if_writerect)(_ifp,_xmin,_ymin,_width,_height,_pp)
#define fb_bwreadrect(_ifp,_xmin,_ymin,_width,_height,_pp) \
		(*_ifp->if_bwreadrect)(_ifp,_xmin,_ymin,_width,_height,_pp)
#define fb_bwwriterect(_ifp,_xmin,_ymin,_width,_height,_pp) \
		(*_ifp->if_bwwriterect)(_ifp,_xmin,_ymin,_width,_height,_pp)

/* Library entry points which are true functions. */
#ifdef USE_PROTOTYPES
FB_EXPORT extern void 	fb_configureWindow(FBIO *, int, int);
FB_EXPORT extern FBIO	*fb_open(char *file, int width, int height);
FB_EXPORT extern int	fb_close(FBIO *ifp);
FB_EXPORT extern int	fb_genhelp(void);
FB_EXPORT extern int	fb_ioinit(FBIO *ifp);
FB_EXPORT extern int	fb_seek(FBIO *ifp, int x, int y);
FB_EXPORT extern int	fb_tell(FBIO *ifp, int *xp, int *yp);
FB_EXPORT extern int	fb_rpixel(FBIO *ifp, unsigned char *pp);
FB_EXPORT extern int	fb_wpixel(FBIO *ifp, unsigned char *pp);
FB_EXPORT extern int	fb_flush(FBIO *ifp);
FB_EXPORT extern void	fb_log(char *fmt, ...);
FB_EXPORT extern int	fb_null(FBIO *ifp);
FB_EXPORT extern int	fb_null_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig);

/* utility functions */
FB_EXPORT extern int	fb_common_file_size(int *w, int *h, char *file, int psize);
FB_EXPORT extern int	fb_common_image_size(int *w, int *h, int npixels);
FB_EXPORT extern int	fb_is_linear_cmap(const ColorMap *cmap);
FB_EXPORT extern void	fb_make_linear_cmap(ColorMap *cmap);
/* backward compatibility hacks */
FB_EXPORT extern int	fb_reset(FBIO *ifp);
FB_EXPORT extern int	fb_viewport(FBIO *ifp, int left, int top, int right, int bottom);
FB_EXPORT extern int	fb_window(FBIO *ifp, int xcenter, int ycenter);
FB_EXPORT extern int	fb_zoom(FBIO *ifp, int xzoom, int yzoom);
FB_EXPORT extern int	fb_scursor(FBIO *ifp, int mode, int x, int y);

/*
 * Some functions and variables we couldn't hide.
 * Not for general consumption.
 */
FB_EXPORT extern int	_fb_pgin();
FB_EXPORT extern int	_fb_pgout();
FB_EXPORT extern int	_fb_pgflush();
FB_EXPORT extern int	_fb_disk_enable;
FB_EXPORT extern int	fb_sim_readrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp);
FB_EXPORT extern int	fb_sim_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp);
FB_EXPORT extern int	fb_sim_bwreadrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp);
FB_EXPORT extern int	fb_sim_bwwriterect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp);
FB_EXPORT extern int	fb_sim_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom);
FB_EXPORT extern int	fb_sim_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom);
FB_EXPORT extern int	fb_sim_cursor(FBIO *ifp, int mode, int x, int y);
FB_EXPORT extern int	fb_sim_getcursor(FBIO *ifp, int *mode, int *x, int *y);
#else
FB_EXPORT extern FBIO	*fb_open();
FB_EXPORT extern int	fb_close();
FB_EXPORT extern int	fb_genhelp();
FB_EXPORT extern int	fb_ioinit();
FB_EXPORT extern int	fb_seek();
FB_EXPORT extern int	fb_tell();
FB_EXPORT extern int	fb_rpixel();
FB_EXPORT extern int	fb_wpixel();
FB_EXPORT extern int	fb_flush();
FB_EXPORT extern void	fb_log();
FB_EXPORT extern int	fb_null();
FB_EXPORT extern int	fb_null_setcursor();
/* utility functions */
FB_EXPORT extern int	fb_common_file_size();
FB_EXPORT extern int	fb_common_image_size();
FB_EXPORT extern int	fb_is_linear_cmap();
FB_EXPORT extern void	fb_make_linear_cmap();
/* backward compatibility hacks */
FB_EXPORT extern int	fb_reset();
FB_EXPORT extern int	fb_viewport();
FB_EXPORT extern int	fb_window();
FB_EXPORT extern int	fb_zoom();
FB_EXPORT extern int	fb_scursor();

/*
 * Some functions and variables we couldn't hide.
 * Not for general consumption.
 */
FB_EXPORT extern int	_fb_pgin();
FB_EXPORT extern int	_fb_pgout();
FB_EXPORT extern int	_fb_pgflush();
FB_EXPORT extern int	_fb_disk_enable;
FB_EXPORT extern int	fb_sim_readrect();
FB_EXPORT extern int	fb_sim_writerect();
FB_EXPORT extern int	fb_sim_bwreadrect();
FB_EXPORT extern int	fb_sim_bwwriterect();
FB_EXPORT extern int	fb_sim_view();
FB_EXPORT extern int	fb_sim_getview();
FB_EXPORT extern int	fb_sim_cursor();
FB_EXPORT extern int	fb_sim_getcursor();
#endif

FB_EXPORT extern int _ogl_open_existing();
FB_EXPORT extern int ogl_close_existing();

/*
 * Copy one RGB pixel to another.
 */
#define	COPYRGB(to,from) { (to)[RED]=(from)[RED];\
			   (to)[GRN]=(from)[GRN];\
			   (to)[BLU]=(from)[BLU]; }

/*
 * A fast inline version of fb_wpixel.  This one does NOT check for errors,
 *  nor "return" a value.  For reasons of C syntax it needs the basename
 *  of an RGBpixel rather than a pointer to one.
 */
#define	FB_WPIXEL(ifp,pp) {if((ifp)->if_pno==-1)_fb_pgin((ifp),(ifp)->if_pixcur/(ifp)->if_ppixels);\
	(*((ifp)->if_pcurp+0))=(pp)[0];(*((ifp)->if_pcurp+1))=(pp)[1];(*((ifp)->if_pcurp+2))=(pp)[2];\
	(ifp)->if_pcurp+=3;(ifp)->if_pixcur++;(ifp)->if_pdirty=1;\
	if((ifp)->if_pcurp>=(ifp)->if_pendp){_fb_pgout((ifp));(ifp)->if_pno= -1;}}

/* Debug Bitvector Definition */
#define	FB_DEBUG_BIO	1	/* Buffered io calls (less r/wpixel) */
#define	FB_DEBUG_CMAP	2	/* Contents of colormaps */
#define	FB_DEBUG_RW	4	/* Contents of reads and writes */
#define	FB_DEBUG_BRW	8	/* Buffered IO rpixel and wpixel */

#define FB_CKMAG(_ptr, _magic, _str)	\
	if( !(_ptr) )  { \
		fb_log("ERROR: null %s ptr, file %s, line %d\n", \
			_str, __FILE__, __LINE__ ); \
		abort(); \
	} else if( *((long *)(_ptr)) != (_magic) )  { \
		fb_log("ERROR: bad %s ptr x%x, s/b x%x, was x%x, file %s, line %d\n", \
			_str, _ptr, _magic, \
			*((long *)(_ptr)), __FILE__, __LINE__ ); \
		abort(); \
	}
#define FB_CK_FBIO(_p)	FB_CKMAG(_p, FB_MAGIC, "FBIO" )

/* tcl.c */
/* The presence of Tcl_Interp as an arg prevents giving arg list */
FB_EXPORT extern void fb_tcl_setup();
#ifdef BRLCAD_DEBUG
FB_EXPORT extern int Fb_d_Init();
#else
FB_EXPORT extern int Fb_Init();
#endif
FB_EXPORT extern int fb_refresh(FBIO *ifp, int x, int y, int w, int h);

/* vers.c (created by libfb/Cakefile) */
FB_EXPORT extern char fb_version[];

FB_EXPORT extern FBIO X24_interface;
#ifdef IF_OGL
FB_EXPORT extern FBIO ogl_interface;
#endif

#ifdef IF_REMOTE
FB_EXPORT extern FBIO remote_interface;
#endif

/* server.c */
FB_EXPORT extern FBIO	*fb_server_fbp;
FB_EXPORT extern fd_set	*fb_server_select_list;
FB_EXPORT extern int	*fb_server_max_fd;
FB_EXPORT extern int	fb_server_got_fb_free;       /* !0 => we have received an fb_free */
FB_EXPORT extern int	fb_server_refuse_fb_free;    /* !0 => don't accept fb_free() */
FB_EXPORT extern int	fb_server_retain_on_close;   /* !0 => we are holding a reusable FB open */
FB_EXPORT extern const struct pkg_switch fb_server_pkg_switch[];


#endif /* FB_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

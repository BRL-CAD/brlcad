/*
 *			F B . H
 *
 *  BRL "Generic" Framebuffer Library Interface Defines.
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Status -
 *	Public Domain, Distribution Unlimited
 *
 *  $Header$
 */

#ifndef INCL_FB
#define INCL_FB

/*
 *			R G B p i x e l
 *
 *  Format of disk pixels in .pix files.
 *  Also used as arguments to many of the library routines.
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

/*
 *			F B I O
 *
 *  A frame-buffer IO structure.
 *  One of these is allocated for each active framebuffer.
 *  A pointer to this structure is the first argument to all
 *  the library routines.
 */
typedef struct  {
	/* Static information: per device TYPE.	*/
	int	(*if_open)();		/* open device		*/
	int	(*if_close)();		/* close device		*/
	int	(*if_clear)();		/* clear device 	*/
	int	(*if_read)();		/* read pixels		*/
	int	(*if_write)();		/* write pixels		*/
	int	(*if_rmap)();		/* read colormap 	*/
	int	(*if_wmap)();		/* write colormap 	*/
	int	(*if_view)();		/* set view		*/
	int	(*if_getview)();	/* get view		*/
	int	(*if_setcursor)();	/* define cursor 	*/
	int	(*if_cursor)();		/* set cursor		*/
	int	(*if_getcursor)();	/* get cursor		*/
	int	(*if_readrect)();	/* read rectangle 	*/
	int	(*if_writerect)();	/* write rectangle 	*/
	int	(*if_poll)();		/* handle events 	*/
	int	(*if_flush)();		/* flush output 	*/
	int	(*if_free)();		/* free resources 	*/
	int	(*if_help)();		/* print useful info	*/
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
	RGBpixel *if_pbase;	/* Address of malloc()ed page buffer.	*/
	RGBpixel *if_pcurp;	/* Current pointer into page buffer.	*/
	RGBpixel *if_pendp;	/* End of page buffer.			*/
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

#ifdef NULL
#define PIXEL_NULL	(RGBpixel *) NULL
#define RGBPIXEL_NULL	(RGBpixel *) NULL
#define COLORMAP_NULL	(ColorMap *) NULL
#define FBIO_NULL	(FBIO *) NULL
#else
#define PIXEL_NULL	(RGBpixel *) 0
#define RGBPIXEL_NULL	(RGBpixel *) 0
#define COLORMAP_NULL	(ColorMap *) 0
#define FBIO_NULL	(FBIO *) 0
#endif

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

/* Library entry points which are true functions. */
#if __STDC__
extern FBIO	*fb_open(char *file, int width, int height);
extern int	fb_close(FBIO *ifp);
extern int	fb_genhelp(void);
extern int	fb_ioinit(FBIO *ifp);
extern int	fb_seek(FBIO *ifp, int x, int y);
extern int	fb_tell(FBIO *ifp, int *xp, int *yp);
extern int	fb_rpixel(FBIO *ifp, RGBpixel *pp);
extern int	fb_wpixel(FBIO *ifp, RGBpixel *pp);
extern int	fb_flush(FBIO *ifp);
extern void	fb_log(char *fmt, ...);
extern int	fb_null(void);
/* utility functions */
extern int	fb_common_file_size(int *w, int *h, char *file, int psize);
extern int	fb_common_image_size(int *w, int *h, int npixels);
extern int	fb_is_linear_cmap(ColorMap *cmap);
extern void	fb_make_linear_cmap(ColorMap *cmap);
/* backward compatibility hacks */
extern int	fb_reset(FBIO *ifp);
extern int	fb_viewport(FBIO *ifp, int left, int top, int right, int bottom);
extern int	fb_window(FBIO *ifp, int xcenter, int ycenter);
extern int	fb_zoom(FBIO *ifp, int xzoom, int yzoom);
extern int	fb_scursor(FBIO *ifp, int mode, int x, int y);
#else
extern FBIO	*fb_open();
extern int	fb_close();
extern int	fb_genhelp();
extern int	fb_ioinit();
extern int	fb_seek();
extern int	fb_tell();
extern int	fb_rpixel();
extern int	fb_wpixel();
extern int	fb_flush();
extern void	fb_log();
extern int	fb_null();
/* utility functions */
extern int	fb_common_file_size();
extern int	fb_common_image_size();
extern int	fb_is_linear_cmap();
extern void	fb_make_linear_cmap();
/* backward compatibility hacks */
extern int	fb_reset();
extern int	fb_viewport();
extern int	fb_window();
extern int	fb_zoom();
extern int	fb_scursor();
#endif

/*
 * Some functions and variables we couldn't hide.
 * Not for general consumption.
 */
extern int	_fb_pgin();
extern int	_fb_pgout();
extern int	_fb_pgflush();
extern int	_fb_disk_enable;
extern int	fb_sim_readrect();
extern int	fb_sim_writerect();
extern int	fb_sim_view();
extern int	fb_sim_getview();
extern int	fb_sim_cursor();
extern int	fb_sim_getcursor();

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
	(*((ifp)->if_pcurp))[0]=(pp)[0];(*((ifp)->if_pcurp))[1]=(pp)[1];(*((ifp)->if_pcurp))[2]=(pp)[2];\
	(ifp)->if_pcurp++;(ifp)->if_pixcur++;(ifp)->if_pdirty=1;\
	if((ifp)->if_pcurp>=(ifp)->if_pendp){_fb_pgout((ifp));(ifp)->if_pno= -1;}}

/* Debug Bitvector Definition */
#define	FB_DEBUG_BIO	1	/* Buffered io calls (less r/wpixel) */
#define	FB_DEBUG_CMAP	2	/* Contents of colormaps */
#define	FB_DEBUG_RW	4	/* Contents of reads and writes */
#define	FB_DEBUG_BRW	8	/* Buffered IO rpixel and wpixel */

#endif /* INCL_FB */

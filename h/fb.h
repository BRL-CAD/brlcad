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

#ifdef OLD
/*
 *			P i x e l
 *
 *  Format of disk pixels in .fb files.
 *  Also used as arguments to many of the library routines.
 *  The similarity to Ikonas pixels is historical.
 *  The spare byte is unfortunate, but the struct would pad anyways.
 */
typedef struct  {
	unsigned char	red;
	unsigned char	green;
	unsigned char	blue;
	unsigned char	spare;
} Pixel;
#endif OLD

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
	int	(*if_dopen)();
	int	(*if_dclose)();
	int	(*if_dreset)();
	int	(*if_dclear)();
	int	(*if_bread)();
	int	(*if_bwrite)();
	int	(*if_cmread)();
	int	(*if_cmwrite)();
	int	(*if_viewport_set)();
	int	(*if_window_set)();
	int	(*if_zoom_set)();
	int	(*if_curs_set)();
	int	(*if_cmemory_addr)();
	int	(*if_cscreen_addr)();
	int	(*if_help)();	/* prints useful information */
	char	*if_type;	/* what "open" claims it is. */
	int	if_max_width;
	int	if_max_height;
	/* Dynamic information: per device INSTANCE. */
	char	*if_name;	/* what the user called it */
	int	if_width;	/* current values */
	int	if_height;
	/* Internal information: needed by the library.	*/
	int	if_fd;
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

/* Library entry points which are macros.				*/
#define fb_gettype(_ifp)		(_ifp->if_type)
#define fb_getwidth(_ifp)		(_ifp->if_width)
#define fb_getheight(_ifp)		(_ifp->if_height)
#define fb_help(_ifp)			(*_ifp->if_help)(_ifp)
#define fb_reset(_ifp)			(*_ifp->if_dreset)(_ifp)
#define fb_clear(_ifp,_pp)		(*_ifp->if_dclear)(_ifp,_pp)
#define fb_read(_ifp,_x,_y,_pp,_ct)	(*_ifp->if_bread)(_ifp,_x,_y,_pp,_ct)
#define fb_write(_ifp,_x,_y,_pp,_ct)	(*_ifp->if_bwrite)(_ifp,_x,_y,_pp,_ct)
#define fb_rmap(_ifp,_cmap)		(*_ifp->if_cmread)(_ifp,_cmap)
#define fb_wmap(_ifp,_cmap)		(*_ifp->if_cmwrite)(_ifp,_cmap)
#define	fb_viewport(_ifp,_l,_t,_r,_b)	(*_ifp->if_viewport_set)(_ifp,_l,_t,_r,_b)
#define fb_window(_ifp,_x,_y)		(*_ifp->if_window_set)(_ifp,_x,_y)
#define fb_zoom(_ifp,_x,_y)		(*_ifp->if_zoom_set)(_ifp,_x,_y)
#define fb_setcursor(_ifp,_bits,_xb,_yb,_xo,_yo) (*_ifp->if_curs_set)(_ifp,_bits,_xb,_yb,_xo,_yo)
#define fb_cursor(_ifp,_mode,_x,_y)	(*_ifp->if_cmemory_addr)(_ifp,_mode,_x,_y)
#define fb_scursor(_ifp,_mode,_x,_y)	(*_ifp->if_cscreen_addr)(_ifp,_mode,_x,_y)

/* Library entry points which are true functions.			*/
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

/*
 * Some functions and variables we couldn't hide.
 * Not for general consumption.
 */
extern int	_fb_pgin();
extern int	_fb_pgout();
extern int	_disk_enable;

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

#endif INCL_FB

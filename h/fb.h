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
	int	(*if_cinit_bitmap)();
	int	(*if_cmemory_addr)();
	int	(*if_cscreen_addr)();
	char	*if_type;	/* what "open" claims it is. */
	int	if_max_width;
	int	if_max_height;
	/* Dynamic information: per device INSTANCE. */
	char	*if_name;	/* what the user called it */
	int	if_width;	/* current values */
	int	if_height;
	/* Internal information: needed by the library.	*/
	int	if_fd;
	Pixel	*if_pbase;	/* Address of malloc()ed page buffer.	*/
	Pixel	*if_pcurp;	/* Current pointer into page buffer.	*/
	Pixel	*if_pendp;	/* End of page buffer.			*/
	int	if_pno;		/* Current "page" in memory.		*/
	int	if_pref;	/* Page "referenced" flag.		*/
	long	if_pixcur;	/* Current pixel number in framebuffer. */
	long	if_pbytes;	/* Sizeof page buffer (bytes).		*/
	long	if_ppixels;	/* Sizeof page buffer (pixels).		*/
	union	{		/* State variables for interface modules */
		char	*p;
		long	l;
	} u1, u2;
} FBIO;

#ifdef NULL
#define PIXEL_NULL	(Pixel *) NULL
#define COLORMAP_NULL	(ColorMap *) NULL
#define FBIO_NULL	(FBIO *) NULL
#else
#define PIXEL_NULL	(Pixel *) 0
#define COLORMAP_NULL	(ColorMap *) 0
#define FBIO_NULL	(FBIO *) 0
#endif

/* Library entry points which are macros.				*/
#define fb_gettype(_ifp)		(_ifp->if_type)
#define fb_getwidth(_ifp)		(_ifp->if_width)
#define fb_getheight(_ifp)		(_ifp->if_height)
#define fb_reset(_ifp)			(*_ifp->if_dreset)(_ifp)
#define fb_clear(_ifp,_pp)		(*_ifp->if_dclear)(_ifp,_pp)
#define fb_read(_ifp,_x,_y,_pp,_ct)	(*_ifp->if_bread)(_ifp,_x,_y,_pp,_ct)
#define fb_write(_ifp,_x,_y,_pp,_ct)	(*_ifp->if_bwrite)(_ifp,_x,_y,_pp,_ct)
#define fb_rmap(_ifp,_cmap)		(*_ifp->if_cmread)(_ifp,_cmap)
#define fb_wmap(_ifp,_cmap)		(*_ifp->if_cmwrite)(_ifp,_cmap)
#define	fb_viewport(_ifp,_l,_t,_r,_b)	(*_ifp->if_viewport_set)(_ifp,_l,_t,_r,_b)
#define fb_window(_ifp,_x,_y)		(*_ifp->if_window_set)(_ifp,_x,_y)
#define fb_zoom(_ifp,_x,_y)		(*_ifp->if_zoom_set)(_ifp,_x,_y)
#define fb_setcursor(_ifp,_bitmap)	(*_ifp->if_cinit_bitmap)(_ifp,_bitmap)
#define fb_cursor(_ifp,_mode,_x,_y)	(*_ifp->if_cmemory_addr)(_ifp,_mode,_x,_y)
#define fb_scursor(_ifp,_mode,_x,_y)	(*_ifp->if_cscreen_addr)(_ifp,_mode,_x,_y)

/* Library entry points which are true functions.			*/
extern FBIO	*fb_open();
extern int	fb_close();
extern void	fb_log();
extern int	fb_null();

#endif INCL_FB

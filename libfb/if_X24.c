/*
 *			I F _ X 2 4 . C
 *
 *  24 bit X Window System (X11) libfb interface.
 *
 *  Authors -
 *	Christopher Jackson
 *	Timothy G. Smith
 *
 *  Source -
 *	Sun Microsystems, Inc.
 *	Southern Area Special Projects Group
 *	6716 Alexander Bell Drive, Suite 200
 *	Columbia, MD 21046
 *  
 *  Copyright Notice -
 *	Copyright (c) 1994 Sun Microsystems, Inc. - All Rights Reserved.
 *
 *	Permission is hereby granted, without written agreement and without
 *	license or royalty fees, to use, copy, modify, and distribute this
 *	software and its documentation for any purpose, provided that the
 *	above copyright notice and the following two paragraphs appear in
 *	all copies of this software.
 *
 *	IN NO EVENT SHALL SUN MICROSYSTEMS INC. BE LIABLE TO ANY PARTY FOR
 *	DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING
 *	OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF SUN
 *	MICROSYSTEMS INC. HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	SUN MICROSYSTEMS INC. SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 *	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 *	AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER
 *	IS ON AN "AS IS" BASIS, AND SUN MICROSYSTEMS INC. HAS NO OBLIGATION TO
 *	PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 *  Acknowledgements -
 *	This software is loosely based on the original if_X.c by
 *	Phillip Dykstra of BRL, which was Copyright 1988, US Army.
 */

/*
 To do:

- Support little-endian X servers and hosts properly
- Provide real cursor support
- Arrange for HAVE_MMAP to be defined by build

*/

#define X_DBG	0
#define UPD_DBG 0
#define BLIT_DBG 0
#define EVENT_DBG 0

#define HAVE_MMAP 1

#ifndef lint
static char sccsid[] = "@(#)if_X24.c version 1.19 (03 Nov 1994)";
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include <stdio.h>
#include <sys/types.h>

#include "conf.h"
#include "machine.h"
#include "fb.h"
#include "externs.h"
#include "./fblocal.h"

#ifdef HAVE_MMAP
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#else
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>		/* for XA_RGB_BEST_MAP */


#define SHMEM_KEY	42

static int	X24_open(),
		X24_close(),
		X24_clear(),
		X24_read(),
		X24_write(),
		X24_rmap(),
		X24_wmap(),
		X24_view(),
		X24_getview(),
		X24_setcursor(),
		X24_cursor(),
		X24_getcursor(),
		X24_readrect(),
		X24_writerect(),
		X24_poll(),
		X24_flush(),
		X24_free(),
		X24_help();

static int	X24_getmem();
static void	X24_zapmem();
static void	X24_destroy();
static void	X24_blit();
static void	X24_updstate();

static	int	linger();
static	int	xsetup();
static	void	print_display_info();	/* debug */
static	void	slowrect();

static unsigned long
		X24_pixel();

static void	handle_event(FBIO *ifp, XEvent *event);


/* This is the ONLY thing that we normally "export" */
FBIO X24_interface =  {
	0,			/* magic number slot	*/
	X24_open,		/* open device		*/
	X24_close,		/* close device		*/
	X24_clear,		/* clear device		*/
	X24_read,		/* read	pixels		*/
	X24_write,		/* write pixels		*/
	X24_rmap,		/* read colormap	*/
	X24_wmap,		/* write colormap	*/
	X24_view,		/* set view		*/
	X24_getview,		/* get view		*/
	X24_setcursor,		/* define cursor	*/
	X24_cursor,		/* set cursor		*/
	X24_getcursor,		/* get cursor		*/
	X24_readrect,		/* read rectangle	*/
	X24_writerect,		/* write rectangle	*/
	X24_poll,		/* process events	*/
	X24_flush,		/* flush output		*/
	X24_free,		/* free resources	*/
	X24_help,		/* help message		*/
	"24 bit X Window System (X11)",	/* device description	*/
	2048,			/* max width		*/
	2048,			/* max height		*/
	"/dev/X",		/* short device name	*/
	512,			/* default/current width  */
	512,			/* default/current height */
	-1,			/* select file desc	*/
	-1,			/* file descriptor	*/
	1, 1,			/* zoom			*/
	256, 256,		/* window center	*/
	0, 0, 0,		/* cursor		*/
	PIXEL_NULL,		/* page_base		*/
	PIXEL_NULL,		/* page_curp		*/
	PIXEL_NULL,		/* page_endp		*/
	-1,			/* page_no		*/
	0,			/* page_dirty		*/
	0L,			/* page_curpos		*/
	0L,			/* page_pixels		*/
	0			/* debug		*/
};


/*
 * Per window state information.
 */
struct	xinfo {
	Display		*xi_dpy;	/* Display and Screen(s) info */
	Window		xi_win;		/* Window ID */
	int		xi_screen;	/* Our screen selection */
	Visual		*xi_visual;	/* Our visual selection */
	GC		xi_gc;		/* current graphics context */
	GC		xi_cgc;		/* graphics context for clipping */
	Region		xi_reg;		/* Valid displayed region */
	Colormap	xi_cmap;	/* Colormap */
	XImage		*xi_image;	/* XImage (size of screen) */
	Window		xi_curswin;	/* Cursor Window ID */

	unsigned char	*xi_xpixbuf;	/* 32 bit per pixel X Image buffer */
	unsigned char	*xi_mem;	/* 24bit backing store */

	int		xi_shmid;	/* Sys V shared mem id */

	int		xi_mode;	/* 0,1,2 */
	int		xi_flags;
	ColorMap 	*xi_rgb_cmap;	/* User's libfb colormap */
	unsigned char	*xi_redmap;	/* Fake colormap for TrueColor */
	unsigned char	*xi_blumap;	/* Fake colormap for TrueColor */
	unsigned char	*xi_grnmap;	/* Fake colormap for TrueColor */

	/* The following values are in Image Pixels */

	int		xi_iwidth;	/* Width of user's whole image */
	int		xi_iheight;	/* Height of user's whole image */

	int		xi_ilf;		/* Image coordinate of LLHC image */
	int		xi_ibt;		/*  pixel */
	int		xi_irt;		/* Image coordinate of URHC image */
	int		xi_itp;		/*  pixel */

	/* The following values are in X Pixels */

	int		xi_ilf_w;	/* Width of leftmost image pixels */
	int		xi_irt_w;	/* Width of rightmost image pixels */
	int		xi_ibt_h;	/* Height of bottommost image pixels */
	int		xi_itp_h;	/* Height of topmost image pixels */

	int		xi_xwidth;	/* Width of X window */
	int		xi_xheight;	/* Height of X window */

	int		xi_xlf;		/* X-coord of leftmost pixels */
	int		xi_xrt;		/* X-coord of rightmost pixels */
	int		xi_xtp;		/* Y-coord of topmost pixels */
	int		xi_xbt;		/* Y-coord of bottomost pixels */
};
#define	XI(ptr) ((struct xinfo *)((ptr)->u1.p))
#define	XI_SET(ptr, val) ((ptr)->u1.p) = (char *) val;


#define MODE1_MASK	(1<<1)
#define MODE1_TRANSIENT	(0<<1)
#define MODE1_LINGERING (1<<1)

#define MODE10_MASK	(1<<10)
#define MODE10_MALLOC	(0<<10)
#define MODE10_SHARED	(1<<10)

#define MODE11_MASK	(1<<11)
#define MODE11_NORMAL	(0<<11)
#define MODE11_ZAP	(1<<11)

static struct modeflags {
	char	c;
	long	mask;
	long	value;
	char	*help;
} modeflags[] = {
	{ 'l',	MODE1_MASK, MODE1_LINGERING,
		"Lingering window - else transient" },
	{ 's',  MODE10_MASK, MODE10_SHARED,
		"Use shared memory backing store" },
	{ 'z',	MODE11_MASK, MODE11_ZAP,
		"Zap (free) shared memory" },
	{ '\0', 0, 0, "" }
};

#define FLG_LINCMAP	0x1	/* We're using a linear colormap */
#define FLG_XCMAP	0x2	/* The X server can do colormapping for us */
#define FLG_INIT	0x4	/* Display is fully initialized */

#define	BLIT_DISP	0x1	/* Write bits to screen */
#define BLIT_PZ		0x2	/* This is a pan or zoom */
#define BLIT_RESIZE	0x4	/* We just resized (screen empty) */

#define BS_NAME	"/tmp/X24_fb"

static double dtime()
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return ((double) tv.tv_sec + (double) tv.tv_usec / 1000000.0);
}

static int
X24_open(ifp, file, width, height)
FBIO	*ifp;
char	*file;
int	width, height;
{
	struct xinfo *xi;
	int ret;
	int mode;			/* local copy */

#if X_DBG
printf("X24_open(ifp:0x%x, file:%s width:%d, height:%d): entered.\n",
       ifp, file, width, height);
#endif
	FB_CK_FBIO(ifp);

	mode = 0;

	/*
	 *  First, attempt to determine operating mode for this open,
	 *  based upon the "unit number" or flags.
	 *  file = "/dev/X###"
	 *  The default mode is zero.
	 */
	if (file != NULL)  {
		register char *cp;
		char	modebuf[80];
		char	*mp;
		int	alpha;
		struct	modeflags *mfp;

		if (strncmp(file, ifp->if_name, strlen(ifp->if_name))) {
			/* How did this happen?? */
			mode = 0;
#if X_DBG
printf("X24_open(ifp:0x%x, file:%s): if_name:%s, mismatch.\n", file, ifp->if_name);
#endif
		}
		else {
			/* Parse the options */
			alpha = 0;
			mp = &modebuf[0];
			cp = &file[sizeof("/dev/X")-1];
			while(*cp != '\0' && !isspace(*cp)) {
				*mp++ = *cp;	/* copy it to buffer */
				if (isdigit(*cp)) {
					cp++;
					continue;
				}
				alpha++;
				for(mfp = modeflags; mfp->c != '\0'; mfp++) {
					if (mfp->c == *cp) {
						mode = (mode&~mfp->mask)|mfp->value;
						break;
					}
				}
				if (mfp->c == '\0' && *cp != '-') {
					fb_log("if_X24: unknown option '%c' ignored\n", *cp);
				}
				cp++;
			}
			*mp = '\0';
			if (!alpha)
				mode = atoi(modebuf);
		}
	}

#if X_DBG
printf("X24_open() mode:0x%x\n", mode);
#endif

	/* Just zap the shared memory and exit */
	if ((mode & MODE11_MASK) == MODE11_ZAP) {
		/* Only task: Attempt to release shared memory segment */
		X24_zapmem();
		return(-1);
	}

	if (width <= 0)
		width = ifp->if_width;
	if (height <= 0)
		height = ifp->if_height;
	if (width > ifp->if_max_width) 
		width = ifp->if_max_width;
	if (height > ifp->if_max_height) 
		height = ifp->if_max_height;

	ifp->if_width = width;
	ifp->if_height = height;

	ifp->if_xzoom = 1;
	ifp->if_yzoom = 1;
	ifp->if_xcenter = width/2;
	ifp->if_ycenter = height/2;

	/* create a struct of state information */
	if ((xi = (struct xinfo *) calloc(1, sizeof(struct xinfo))) == NULL) {
		fb_log("X24_open: xinfo malloc failed\n");
		return(-1);
	}
	XI_SET(ifp, xi);

	xi->xi_mode = mode;
	xi->xi_iwidth = width;
	xi->xi_iheight = height;

	/* Allocate backing store (shared memory or local) */

	switch (X24_getmem(ifp))
	{
	case 0:		/* Allocated, shared memory was there already */
		X24_blit(ifp, 0, 0, xi->xi_iwidth, xi->xi_iheight, 0);
		break;

	case 1:		/* Allocated, shared memory is new */
		break;

	case -1:	/* Failed */
		X24_destroy(xi);
		return (-1);
		break;
	}

	/* Set up an X window, graphics context, etc. */

	if ((ret = xsetup(ifp, width, height)) < 0) {
#if X_DBG
		fb_log("if_X24: Can't get 24 bit Visual on X display \"%s\"\n",
		       XDisplayName(NULL));
#endif
		X24_destroy(xi);
		return(-1);
	}
	if (ret == 0 )  {
		/* xsetup unable to get 24-bit visual. */
		extern FBIO X_interface;	/* from if_X.c */
		char	*nametemp;

#if X_DBG
		fb_log("if_X24: Can't get 24 bit Visual on X display \"%s\", trying 8/1-bit code\n",
		       XDisplayName(NULL));
#endif
		X24_destroy(xi);

		/* Let if_X take a crack at it */
		nametemp = ifp->if_name;
		*ifp = X_interface;	/* struct copy */
		ifp->if_name = nametemp;
		ifp->if_magic = FB_MAGIC;

		return X_interface.if_open( ifp, nametemp, width, height );
		/*
		 * Because function ptrs in 'ifp' were changed,
		 * no further calls will be made on if_X24.c.
		 */
	}

	/* Allocate the image buffer */

	if ((xi->xi_xpixbuf = (unsigned char *) calloc(4, width*height)) ==
	    NULL) {
		fb_log("X24_open: xpixbuf malloc failed\n");
		X24_destroy(xi);
		return(-1);
	}

	/*
	 *  Create an Image structure.
	 *  The image is our client resident copy which we
	 *  can get/put from/to a server resident Pixmap or
	 *  Window (i.e. a "Drawable").
	 */
	xi->xi_image = XCreateImage(xi->xi_dpy,
		xi->xi_visual, 24, ZPixmap, 0,
		(char *) xi->xi_xpixbuf, width, height, 32, 0);

	/* Update state for blits */

	X24_updstate(ifp);

	/* Make the Display connection available for selecting on */
	ifp->if_selfd = ConnectionNumber(xi->xi_dpy);

	/* Mark display ready */

	xi->xi_flags |= FLG_INIT;

	return(0);
}

static int
X24_close(ifp)
FBIO	*ifp;
{
	struct xinfo *xi = XI(ifp);

#if X_DBG
printf("X24_close(ifp:0x%x): entered.\n", ifp);
#endif

	XFlush(xi->xi_dpy);
	if ((xi->xi_mode & MODE1_MASK) == MODE1_LINGERING) {
		if (linger(ifp))
			return(0);	/* parent leaves the display */
	}

	X24_destroy(xi);

	return (0);
}

static void
X24_destroy(xi)
struct xinfo *xi;
{
	if (xi) {
		if (xi->xi_xpixbuf)
			free(xi->xi_xpixbuf);

		if (xi->xi_rgb_cmap &&
		    (xi->xi_mode & MODE10_MASK) == MODE10_MALLOC)
			free(xi->xi_rgb_cmap);

		if (xi->xi_redmap)
			free(xi->xi_redmap);
		if (xi->xi_grnmap)
			free(xi->xi_grnmap);
		if (xi->xi_blumap)
			free(xi->xi_blumap);

		if (xi->xi_dpy) {
			if (xi->xi_cgc)
				XFreeGC(xi->xi_dpy, xi->xi_cgc);

			if (xi->xi_gc)
				XFreeGC(xi->xi_dpy, xi->xi_gc);

			if (xi->xi_image) {
				xi->xi_image->data = NULL;
				XDestroyImage(xi->xi_image);
			}

			if (xi->xi_reg)
				XDestroyRegion(xi->xi_reg);

			XCloseDisplay(xi->xi_dpy);
		}

		free((char *) xi);
	}
}

static int
X24_clear(ifp, pp)
FBIO	*ifp;
unsigned char	*pp;
{
	struct xinfo *xi = XI(ifp);

	int red, grn, blu;
	int npix;
	int n;
	unsigned char *cp;

	XSetWindowAttributes attr;
	XGCValues gcv;

#if X_DBG
printf("X24_clear(ifp:0x%x, pp:0x%x) pixel = (%d, %d, %d): entered.\n",
	ifp, pp, pp[RED], pp[GRN], pp[BLU]);
#endif

	red = pp[RED];
	grn = pp[GRN];
	blu = pp[BLU];

	/* Clear the backing store */

	npix = xi->xi_iwidth * xi->xi_xheight;

	if (pp == (unsigned char *) NULL) {
		memset(xi->xi_mem, 0, npix*3);
	} else if (red == grn && red == blu) {
		memset(xi->xi_mem, red, npix*3);
	} else {
		cp = xi->xi_mem;
		n = npix;
		while (n--) {
			*cp++ = red;
			*cp++ = grn;
			*cp++ = blu;
		}
	}

	/* Clear the X image */

	npix = xi->xi_xwidth * xi->xi_xheight;

	if (pp == (unsigned char *) NULL ||
		(!red && !grn && !blu)) {
		memset(xi->xi_xpixbuf, 0, npix*4);
	} else {
		unsigned long *lp = (unsigned long *) xi->xi_xpixbuf;
		unsigned long pix;

		/*
		 * XXX non-portable hack.  We know the byte order here.
		 * We should probably use the masks and such from the
		 * Visual.
		 */

		pix = X24_pixel(ifp, red, grn, blu);

		n = npix;
		while (n--)
			*lp++ = pix;
	}

	/* Clear the actual window by setting the background and clearing it */

	attr.background_pixel = X24_pixel(ifp, red, grn, blu);

	XChangeWindowAttributes(xi->xi_dpy, xi->xi_win, CWBackPixel,
		&attr);

	XClearWindow(xi->xi_dpy, xi->xi_win);

	/* Set background color in the clear GC too */

	gcv.foreground = attr.background_pixel;
	XChangeGC(xi->xi_dpy, xi->xi_cgc, GCForeground, &gcv);

	return(0);
}


static int
X24_read(ifp, x, y, pixelp, count)
FBIO	*ifp;
int	x, y;
unsigned char	*pixelp;
int	count;
{
	struct xinfo *xi = XI(ifp);
	int maxcount;

	/* check origin bounds */
	if (x < 0 || x >= xi->xi_iwidth || y < 0 || y >= xi->xi_iheight)
		return	-1;

	/* clip read length */
	maxcount = xi->xi_iwidth * (xi->xi_iheight - y) - x;
	if (count > maxcount)
		count = maxcount;

	memcpy(pixelp, &(xi->xi_mem[(y*xi->xi_iwidth+x)*sizeof(RGBpixel)]),
		count*sizeof(RGBpixel));
	return (count);
}


static int
X24_write(ifp, x, y, pixelp, count)
FBIO	*ifp;
int	x, y;
CONST unsigned char	*pixelp;
int	count;
{
	struct xinfo *xi = XI(ifp);

	int	maxcount;

#if X_DBG
printf("X24_write(ifp:0x%x, x:%d, y:%d, pixelp:0x%x, count:%d) entered.\n",
	ifp, x, y, pixelp, count);
#endif

	/* Check origin bounds */
	if (x < 0 || x >= xi->xi_iwidth || y < 0 || y >= xi->xi_iheight)
		return	-1;

	/* Clip write length */
	maxcount = xi->xi_iwidth * (xi->xi_iheight - y) - x;
	if (count > maxcount)
		count = maxcount;

	/* Save it in 24bit backing store */

	memcpy(&(xi->xi_mem[(y*xi->xi_iwidth+x)*sizeof(RGBpixel)]),
	      pixelp, count*sizeof(RGBpixel));

	/* Get the bits to the screen */

	if (x + count <= xi->xi_iwidth)
		X24_blit(ifp, x, y, count, 1, BLIT_DISP);
	else {
		int ylines;
		int tcount;

		tcount = count - (xi->xi_iwidth - x);
		ylines = 1 + (tcount + xi->xi_iwidth - 1) / xi->xi_iwidth;

		X24_blit(ifp, 0, y, xi->xi_iwidth, ylines, BLIT_DISP);
	}

	return(count);
}

static int
X24_rmap(ifp, cmp)
FBIO	*ifp;
ColorMap	*cmp;
{
	struct xinfo *xi = XI(ifp);

#if X_DBG
printf("X24_rmap(ifp:0x%x, cmp:0x%x) entered.\n",
	ifp, cmp);
#endif

	memcpy(cmp, xi->xi_rgb_cmap, sizeof (ColorMap));

	return(0);
}

static int
X24_wmap(ifp, cmp)
FBIO	*ifp;
CONST ColorMap	*cmp;
{
	struct xinfo *xi = XI(ifp);
	ColorMap *map = xi->xi_rgb_cmap;
	int waslincmap;

#if X_DBG
printf("X24_wmap(ifp:0x%x, cmp:0x%x) entered.\n",
	ifp, cmp);
#endif

	/* Did we have a linear colormap before this call? */

	waslincmap = xi->xi_flags & FLG_LINCMAP;

	/* Clear linear colormap flag, since it may be changing */

	xi->xi_flags &= ~FLG_LINCMAP;

	/* Copy in or generate colormap */

	if (cmp)
		memcpy(map, cmp, sizeof (ColorMap));
	else {
		fb_make_linear_cmap(map);
		xi->xi_flags |= FLG_LINCMAP;
	}

	/* Decide if this colormap is linear */

	if (!(xi->xi_flags & FLG_LINCMAP)) {
		int i;
		int nonlin = 0;

		for (i = 0; i < 256; i++)
			if (map->cm_red[i] >> 8 != i ||
			    map->cm_green[i] >> 8 != i ||
			    map->cm_blue[i] >> 8 != i) {
				nonlin = 1;
				break;
			}

		if (!nonlin)
			xi->xi_flags |= FLG_LINCMAP;
	}
	
	/*
	 * If it was linear before, and they're making it linear again,
	 * there's nothing to do.
	 */
	if (waslincmap && xi->xi_flags & FLG_LINCMAP)
		return (0);

	if (xi->xi_flags & FLG_XCMAP) {
		XColor cells[256];
		int i;
		
		/* Copy into the server's colormap. */

		for (i = 0; i < 256; i++) {
			cells[i].pixel = (i << 16) | (i << 8) | i;
			cells[i].red = map->cm_red[i];
			cells[i].green = map->cm_green[i];
			cells[i].blue = map->cm_blue[i];
			cells[i].flags = DoRed | DoGreen | DoBlue;
		}

		XStoreColors(xi->xi_dpy, xi->xi_cmap, cells, 256);
	} else {
		int i;
		unsigned char *red = xi->xi_redmap;
		unsigned char *grn = xi->xi_grnmap;
		unsigned char *blu = xi->xi_blumap;

		/* Copy into our fake colormap arrays. */

		for (i = 0; i < 256; i++) {
			red[i] = map->cm_red[i] >> 8;
			grn[i] = map->cm_green[i] >> 8;
			blu[i] = map->cm_blue[i] >> 8;
		}

		/*
		 * If we're initialized, redraw the screen to make changes
		 * take effect.
		 */

		if (xi->xi_flags & FLG_INIT)
			X24_blit(ifp, 0, 0, xi->xi_iwidth, xi->xi_iheight,
				BLIT_DISP);
	}

	return(0);
}

static int
X24_view(ifp, xcenter, ycenter, xzoom, yzoom)
FBIO	*ifp;
int	xcenter, ycenter;
int	xzoom, yzoom;
{
	struct xinfo *xi = XI(ifp);


#if X_DBG
printf("X24_view(ifp:0x%x, xcenter:%d, ycenter:%d, xzoom:%d, yzoom:%d) entered.\n",
	ifp, xcenter, ycenter, xzoom, yzoom);
#endif

	/* bypass if no change */
	if (ifp->if_xcenter == xcenter && ifp->if_ycenter == ycenter
	 && ifp->if_xzoom == xcenter && ifp->if_yzoom == ycenter)
		return	0;
	/* check bounds */
	if (xcenter < 0 || xcenter >= xi->xi_iwidth
	 || ycenter < 0 || ycenter >= xi->xi_iheight)
		return	-1;
	if (xzoom <= 0 || xzoom >= xi->xi_iwidth/2
	 || yzoom <= 0 || yzoom >= xi->xi_iheight/2)
		return	-1;

	ifp->if_xcenter = xcenter;
	ifp->if_ycenter = ycenter;
	ifp->if_xzoom = xzoom;
	ifp->if_yzoom = yzoom;

	X24_updstate(ifp);
	X24_blit(ifp, 0, 0, xi->xi_iwidth, xi->xi_iheight,
		BLIT_DISP | BLIT_PZ);

	return	0;
}

static int
X24_getview(ifp, xcenter, ycenter, xzoom, yzoom)
FBIO	*ifp;
int	*xcenter, *ycenter;
int	*xzoom, *yzoom;
{
	struct xinfo *xi = XI(ifp);


#if X_DBG
printf("X24_getview(ifp:0x%x, xcenter:0x%x, ycenter:0x%x, xzoom:0x%x, yzoom:0x%x) entered.\n",
	ifp, xcenter, ycenter, xzoom, yzoom);
#endif

	*xcenter = ifp->if_xcenter;
	*ycenter = ifp->if_ycenter;
	*xzoom = ifp->if_xzoom;
	*yzoom = ifp->if_yzoom;

	return(0);
}

static int
X24_setcursor(ifp, bits, xbits, ybits, xorig, yorig)
FBIO	*ifp;
CONST unsigned char *bits;
int	xbits, ybits;
int	xorig, yorig;
{
	struct xinfo *xi = XI(ifp);

#if X_DBG
printf("X24_setcursor(ifp:0x%x, bits:%u, xbits:%d, ybits:%d, xorig:%d, yorig:%d) entered.\n",
	ifp, bits, xbits, ybits, xorig, yorig);
#endif

	return(0);
}

static int
X24_cursor(ifp, mode, x, y)
FBIO	*ifp;
int	mode;
int	x, y;
{
	struct xinfo *xi = XI(ifp);

#if X_DBG
printf("X24_cursor(ifp:0x%x, mode:%d, x:%d, y:%d) entered.\n",
	ifp, mode, x, y);
#endif

	fb_sim_cursor(ifp, mode, x, y);

	return(0);
}

static int
X24_getcursor(ifp, mode, x, y)
FBIO	*ifp;
int	*mode;
int	*x, *y;
{
	struct xinfo *xi = XI(ifp);

#if X_DBG
printf("X24_getcursor(ifp:0x%x, mode:%d, x:0x%x, y:0x%x) entered.\n",
	ifp, mode, x, y);
#endif

	fb_sim_getcursor(ifp, mode, x, y);

	return(0);
}

static int
X24_readrect(ifp, xmin, ymin, width, height, pp)
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
unsigned char	*pp;
{
	struct xinfo *xi = XI(ifp);


#if X_DBG
printf("X24_readrect(ifp:0x%x, xmin:%d, ymin:%d, width:%d, height:%d, pp:0x%x) entered.\n",
	ifp, xmin, ymin, width, height, pp);
#endif
	/* Clip arguments */

	if (xmin < 0)
		xmin = 0;
	if (ymin < 0)
		ymin = 0;
	if (xmin + width > xi->xi_iwidth)
		width = xi->xi_iwidth - xmin;
	if (ymin + height > xi->xi_iheight)
		height = xi->xi_iheight - ymin;

	/* Do copy to backing store */

	if (xmin == 0 && width == xi->xi_iwidth) {
		/* We can do it all in one copy */

		memcpy(pp, &(xi->xi_mem[ymin * xi->xi_iwidth *
			sizeof (RGBpixel)]),
			width * height * sizeof (RGBpixel));
	} else {
		/* Need to do individual lines */

		int ht = height;
		unsigned char *p = &(xi->xi_mem[(ymin * xi->xi_iwidth + xmin) *
			sizeof (RGBpixel)]);

		while (ht--) {
			memcpy(pp, p, width * sizeof (RGBpixel));
			p += xi->xi_iwidth * sizeof (RGBpixel);
			pp += width * sizeof (RGBpixel);
		}
	}

	return (width * height);
}

static int
X24_writerect(ifp, xmin, ymin, width, height, pp)
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
CONST unsigned char	*pp;
{
	struct xinfo *xi = XI(ifp);

#if X_DBG
printf("X24_writerect(ifp:0x%x, xmin:%d, ymin:%d, width:%d, height:%d, pp:0x%x) entered.\n",
	ifp, xmin, ymin, width, height, pp);
#endif

	/* Clip arguments */

	if (xmin < 0)
		xmin = 0;
	if (ymin < 0)
		ymin = 0;
	if (xmin + width > xi->xi_iwidth)
		width = xi->xi_iwidth - xmin;
	if (ymin + height > xi->xi_iheight)
		height = xi->xi_iheight - ymin;

	/* Do copy to backing store */

	if (xmin == 0 && width == xi->xi_iwidth) {
		/* We can do it all in one copy */

		memcpy(&(xi->xi_mem[ymin * xi->xi_iwidth * sizeof (RGBpixel)]),
			pp, width * height * sizeof (RGBpixel));
	} else {
		/* Need to do individual lines */

		int ht = height;
		unsigned char *p = &(xi->xi_mem[(ymin * xi->xi_iwidth + xmin) *
			sizeof (RGBpixel)]);

		while (ht--) {
			memcpy(p, pp, width * sizeof (RGBpixel));
			p += xi->xi_iwidth * sizeof (RGBpixel);
			pp += width * sizeof (RGBpixel);
		}
	}

	/* Flush to screen */

	X24_blit(ifp, xmin, ymin, width, height, BLIT_DISP);

	return (width * height);
}

static int
X24_poll(ifp)
FBIO	*ifp;
{
	struct xinfo *xi = XI(ifp);

	XEvent	event;

#if 0
printf("X24_poll(ifp:0x%x) entered\n", ifp);
#endif

	/* Check for and dispatch event */
	while (XCheckMaskEvent(xi->xi_dpy, ~NoEventMask, &event))
	    handle_event(ifp, &event);

	return(0);
}

static int
X24_flush(ifp)
FBIO	*ifp;
{
	struct xinfo *xi = XI(ifp);


#if X_DBG
printf("X24_flush(ifp:0x%x) entered\n", ifp);
#endif

	XFlush(xi->xi_dpy);
	return(0);
}

static int
X24_free(ifp)
FBIO	*ifp;
{
	struct xinfo *xi = XI(ifp);


#if X_DBG
printf("X24_free(ifp:0x%x) entered\n", ifp);
#endif

	return(0);
}

static int
X24_help(ifp)
FBIO	*ifp;
{
	struct xinfo *xi = XI(ifp);

	struct	modeflags *mfp;

#if X_DBG
printf("X24_help(ifp:0x%x) entered\n", ifp);
#endif

	fb_log("Description: %s\n", X24_interface.if_type);
	fb_log("Device: %s\n", ifp->if_name);
	fb_log("Max width/height: %d %d\n",
		X24_interface.if_max_width,
		X24_interface.if_max_height);
	fb_log("Default width/height: %d %d\n",
		X24_interface.if_width,
		X24_interface.if_height);
	fb_log("Usage: /dev/X24[options]\n");
	for(mfp = modeflags; mfp->c != '\0'; mfp++) {
		fb_log("   %c   %s\n", mfp->c, mfp->help);
	}
	return(0);
}


/*
 *			X S E T U P
 *
 *  Returns -
 *	1	OK, using 24-bit X support in this file
 *	0	OK, except couldn't get 24-bit visual.
 *	-1	fatal error
 */
static
xsetup(ifp, width, height)
FBIO	*ifp;
int	width, height;
{
	struct xinfo *xi = XI(ifp);

	XGCValues	gcv;
	XSizeHints	xsh;		/* part of the "standard" props */
	XWMHints	xwmh;		/* size guidelines for window mngr */
	XSetWindowAttributes xswa;
	XRectangle rect;

	XVisualInfo	visinfo;	/* Visual we'll use */

#if X_DBG
printf("xsetup(ifp:0x%x, width:%d, height:%d) entered\n", ifp, width, height);
#endif

	/* Save these in state structure */

	xi->xi_xwidth = width;
	xi->xi_xheight = height;

	/* Open the display - use the env variable DISPLAY */
	if ((xi->xi_dpy = XOpenDisplay(NULL)) == NULL) {
		fb_log("if_X: Can't open X display \"%s\"\n",
			XDisplayName(NULL));
		return	-1;
	}

#if 0
	print_display_info(xi->xi_dpy);
#endif

	/* Use the screen we connected to */
	xi->xi_screen = DefaultScreen(xi->xi_dpy);

	/*
	 * We need a 24-bit Visual.  First we try for DirectColor, since then
	 * the X server can do colormapping for us; if not, we try for
	 * TrueColor.  If we can't get either we bomb.
	 */

	if (XMatchVisualInfo(xi->xi_dpy, xi->xi_screen, 24, DirectColor,
	    &visinfo)) {
		xi->xi_flags |= FLG_XCMAP;
	} else if (XMatchVisualInfo(xi->xi_dpy, xi->xi_screen, 24, TrueColor,
	    &visinfo)) {
		/* Nothing to do */
	} else {
		return 0;	/* unable to get 24-bit visual */
	}
	xi->xi_visual = visinfo.visual;

	/* Set up colormap, if possible */

	if (xi->xi_flags & FLG_XCMAP) {
		xi->xi_cmap = XCreateColormap(xi->xi_dpy, RootWindow(xi->xi_dpy,
			xi->xi_screen), xi->xi_visual, AllocAll);
	} else {
		/*
		 * We need this, even though we're not going to use it,
		 * because if we don't specify a colormap when we create the
		 * window (thus getting the default), and the default visual
		 * is not 24-bit, the window create will fail.
		 */

		xi->xi_cmap = XCreateColormap(xi->xi_dpy, RootWindow(xi->xi_dpy,
			xi->xi_screen), xi->xi_visual, AllocNone);

		xi->xi_redmap = malloc(256);
		xi->xi_grnmap = malloc(256);
		xi->xi_blumap = malloc(256);

		if (!xi->xi_redmap || !xi->xi_grnmap || !xi->xi_blumap) {
			fb_log("if_X24: Can't allocate colormap memory\n");
			XCloseDisplay(xi->xi_dpy);
			return (-1);
		}
	}

	/* Set up default linear colormap */

	X24_wmap(ifp, NULL);

	/*
	 * Fill in XSetWindowAttributes struct for XCreateWindow.
	 */
	xswa.event_mask = ExposureMask | ButtonPressMask | StructureNotifyMask;
	xswa.background_pixel = X24_pixel(ifp, 0, 0, 0);
	xswa.border_pixel = X24_pixel(ifp, 255, 255, 255);
	xswa.bit_gravity = ForgetGravity;
#ifdef X_DBG
	xswa.backing_store = NotUseful;
#else
	xswa.backing_store = Always;
#endif
	xswa.colormap = xi->xi_cmap;

#if X_DBG
printf("Creating window\n");
#endif

	xi->xi_win = XCreateWindow(xi->xi_dpy, RootWindow(xi->xi_dpy,
		xi->xi_screen), 0, 0, width, height, 3, 24, InputOutput,
		xi->xi_visual, CWEventMask | CWBackPixel | CWBorderPixel |
		    CWBitGravity | CWBackingStore | CWColormap,
		&xswa);

	if (xi->xi_win == 0) {
		fb_log("if_X: Can't create window\n");
		return	-1;
	}

	/* Tell window manager about colormap */

	XSetWindowColormap(xi->xi_dpy, xi->xi_win, xi->xi_cmap);

	/*
	 * Fill in XSizeHints struct to inform window
	 * manager about initial size and location.
	 */
	xsh.flags = PPosition | PSize | PMinSize | PMaxSize;
	xsh.width = width;
	xsh.max_width = ifp->if_max_width;
	xsh.min_width = 0;
	xsh.height = height;
	xsh.max_height = ifp->if_max_height;
	xsh.min_height = 0;
	xsh.x = xsh.y = 0;

	/* Set standard properties for Window Managers */

	XSetStandardProperties(xi->xi_dpy, xi->xi_win,
		"Frame buffer",		/* window name */
		"Frame buffer",		/* icon name */
		None,			/* icon pixmap */
		NULL, 0,		/* command (argv, argc) */
		&xsh);			/* size hints */

	xwmh.input = False;		/* no terminal input? */
	xwmh.initial_state = NormalState;
	xwmh.flags = InputHint | StateHint;
	XSetWMHints(xi->xi_dpy, xi->xi_win, &xwmh);

	/* Create a Graphics Context for drawing */

	gcv.foreground = X24_pixel(ifp, 255, 255, 255);
	gcv.background = X24_pixel(ifp, 0, 0, 0);
	xi->xi_gc = XCreateGC(xi->xi_dpy, xi->xi_win,
		GCForeground | GCBackground, &gcv);

	/* Create a Graphics Context for clipping */

	gcv.foreground = X24_pixel(ifp, 0, 0, 0);
	gcv.background = X24_pixel(ifp, 0, 0, 0);
	xi->xi_cgc = XCreateGC(xi->xi_dpy, xi->xi_win,
		GCForeground | GCBackground, &gcv);

	/* Initialize the valid region */

	xi->xi_reg = XCreateRegion();
	rect.x = 0;
	rect.y = 0;
	rect.width = xi->xi_xwidth;
	rect.height = xi->xi_xheight;
	XUnionRectWithRegion(&rect, xi->xi_reg, xi->xi_reg);

	/* Map window to screen */

	XMapWindow(xi->xi_dpy, xi->xi_win);
	XFlush(xi->xi_dpy);


	return 1;	/* OK, using if_X24 */
}

static int alive = 1;

static int
linger(ifp)
FBIO	*ifp;
{
	struct xinfo *xi = XI(ifp);
	XEvent	event;

	if (fork() != 0)
		return 1;	/* release the parent */

#if X_DBG
printf("X24 linger(ifp:0x%x): entered.\n", ifp);
#endif

	while(alive) {
		XNextEvent(xi->xi_dpy, &event);
		handle_event(ifp, &event);
	}
}


static void
handle_event(ifp, event)
FBIO	*ifp;
XEvent *event;
{
	struct xinfo *xi = XI(ifp);


	switch((int) event->type) {
	case Expose: {
		XExposeEvent	*expose = (XExposeEvent *) event;

		int ex1, ey1, ex2, ey2;

#if EVENT_DBG
printf("expose event x= %d y= %d width= %d height= %d\n",
	expose->x, expose->y, expose->width, expose->height);
#endif

		ex1 = expose->x;
		ey1 = expose->y;
		ex2 = ex1 + expose->width - 1;
		ey2 = ey1 + expose->height - 1;

		/* Clip to outline of valid bits in window */

		if (ex1 < xi->xi_xlf)
			ex1 = xi->xi_xlf;
		if (ex2 > xi->xi_xrt)
			ex2 = xi->xi_xrt;
		if (ey1 < xi->xi_xtp)
			ey1 = xi->xi_xtp;
		if (ey2 > xi->xi_xbt)
			ey2 = xi->xi_xbt;

#if EVENT_DBG
printf("expose limits (%d, %d) to (%d, %d)\n",
	xi->xi_xlf, xi->xi_xtp, xi->xi_xrt, xi->xi_xbt);
printf("clipped expose (%d, %d) to (%d, %d)\n",
	ex1, ey1, ex2, ey2);
#endif

		if (ex2 >= ex1 && ey2 >= ey1)
			XPutImage(xi->xi_dpy, xi->xi_win, xi->xi_gc,
				xi->xi_image, ex1, ey1, ex1,
				ey1, ex2 - ex1 + 1, ey2 - ey1 + 1);
		break;
	}

	case ButtonPress: {
		int button = (int) event->xbutton.button;
		if (button == Button1) {
			/* Check for single button mouse remap.
			 * ctrl-1 => 2
			 * meta-1 => 3
			 */
			if (event->xbutton.state & ControlMask)
				button = Button2;
			else if (event->xbutton.state & Mod1Mask)
				button = Button3;
		}

		switch(button) {
		case Button1:
			break;

		case Button2: {
			int	x, sy;
			int	ix, isy;
			unsigned char	*cp;

			x = event->xbutton.x;
			sy = xi->xi_xheight - event->xbutton.y - 1;

			x -= xi->xi_xlf;
			sy -= xi->xi_xheight - xi->xi_xbt - 1;
			if (x < 0 || sy < 0)
			{
				fb_log("No RGB (outside image) 1\n");
				break;
			}

			if (x < xi->xi_ilf_w)
				ix = xi->xi_ilf;
			else
				ix = xi->xi_ilf + 
					(x - xi->xi_ilf_w + ifp->if_xzoom - 1) /
					ifp->if_xzoom;

			if (sy < xi->xi_ibt_h)
				isy = xi->xi_ibt;
			else
				isy = xi->xi_ibt + 
					(sy - xi->xi_ibt_h + ifp->if_yzoom -
						1) / ifp->if_yzoom;

			if (ix >= xi->xi_iwidth || isy >= xi->xi_iheight)
			{
				fb_log("No RGB (outside image) 2\n");
				break;
			}

			cp = &(xi->xi_mem[(isy*xi->xi_iwidth + ix)*3]);
			fb_log("At image (%d, %d), real RGB=(%3d %3d %3d)\n",
				ix, isy, cp[RED], cp[GRN], cp[BLU]);

			break;
		}

		case Button3:
			alive = 0;
			break;
		}
		break;
	}

	case ConfigureNotify: {
		XConfigureEvent *conf = (XConfigureEvent *) event;


		if (conf->height == xi->xi_xheight &&
		    conf->width == xi->xi_xwidth)
			return;

#if EVENT_DBG
printf("configure, oldht %d oldwid %d newht %d newwid %d\n",
	xi->xi_xheight, xi->xi_xwidth, conf->height,
	conf->width);
#endif

		xi->xi_xheight = conf->height;
		xi->xi_xwidth = conf->width;

		X24_updstate(ifp);

		/* Destroy old image struct and image buffer */

		XDestroyImage(xi->xi_image);

		/* Make new buffer and new image */

		if ((xi->xi_xpixbuf = (unsigned char *)
		    calloc(4, xi->xi_xwidth*xi->xi_xheight)) == NULL) {
			fb_log("X24_open: xpixbuf malloc failed in resize!\n");
			return;
		}

		xi->xi_image = XCreateImage(xi->xi_dpy, xi->xi_visual, 24,
			ZPixmap, 0, (char *) xi->xi_xpixbuf, xi->xi_xwidth,
			xi->xi_xheight, 32, 0);

		/*
		 * Blit backing store to image buffer (we'll blit to screen
		 * when we get the expose event)
		 */

		X24_blit(ifp, 0, 0, xi->xi_iwidth, xi->xi_iheight, BLIT_RESIZE);
	}

	default:
		break;
	}

	return;
}



/*
 *  A given Display (i.e. Server) can have any number of Screens.
 *  Each Screen can support one or more Visual types.
 *  unix:0.1.2 => host:display.screen.visual
 *  Typically the screen and visual default to 0 by being omitted.
 */
static void
print_display_info(dpy)
Display *dpy;
{
	int	i;
	int	screen;
	Visual	*visual, *DefaultVisual;
	XVisualInfo *vp;
	int	num;
	Window	win = DefaultRootWindow(dpy);
	XStandardColormap cmap;

	printf("Server \"%s\", release %d\n",
		ServerVendor(dpy), VendorRelease(dpy));

	/* How many screens? */
	screen = DefaultScreen(dpy);
	printf("%d Screen(s), we connected to screen %d\n",
		ScreenCount(dpy), screen);

	/* How many visuals? */
	vp = XGetVisualInfo(dpy, VisualNoMask, NULL, &num);
	printf("%d Visual(s)\n", num);

	printf("ImageByteOrder: %s\n",
		ImageByteOrder(dpy) == MSBFirst ? "MSBFirst" : "LSBFirst");
	printf("BitmapBitOrder: %s\n",
		BitmapBitOrder(dpy) == MSBFirst ? "MSBFirst" : "LSBFirst");
	printf("BitmapUnit: %d\n", BitmapUnit(dpy));
	printf("BitmapPad: %d\n", BitmapPad(dpy));

	printf("==== Screen %d ====\n", screen);
	printf("%d x %d pixels, %d x %d mm, (%.2f x %.2f dpi)\n",
		DisplayWidth(dpy,screen), DisplayHeight(dpy,screen),
		DisplayWidthMM(dpy,screen), DisplayHeightMM(dpy,screen),
		DisplayWidth(dpy,screen)*25.4/DisplayWidthMM(dpy,screen),
		DisplayHeight(dpy,screen)*25.4/DisplayHeightMM(dpy,screen));
	printf("%d DisplayPlanes (other Visuals, if any, may vary)\n",
		DisplayPlanes(dpy,screen));
	printf("%d DisplayCells\n", DisplayCells(dpy,screen));
	printf("BlackPixel = %d\n", BlackPixel(dpy,screen));
	printf("WhitePixel = %d\n", WhitePixel(dpy,screen));
	printf("Save Unders: %s\n",
		DoesSaveUnders(ScreenOfDisplay(dpy,screen)) ? "True" : "False");
	i = DoesBackingStore(ScreenOfDisplay(dpy,screen));
	printf("Backing Store: %s\n", i == WhenMapped ? "WhenMapped" :
		(i == Always ? "Always" : "NotUseful"));
	printf("Installed Colormaps: min %d, max %d\n",
		MinCmapsOfScreen(ScreenOfDisplay(dpy,screen)),
		MaxCmapsOfScreen(ScreenOfDisplay(dpy,screen)));
	printf("DefaultColormap: 0x%x\n", DefaultColormap(dpy,screen));


	DefaultVisual = DefaultVisual(dpy,screen);

	for (i = 0; i < num; i++) {

		visual = vp[i].visual;

		printf("---- Visual 0x%x (%d)----\n", visual, i);

		printf("screen: %d\n", vp[i].screen);
		printf("depth : %d\n", vp[i].depth);

		switch(visual->class) {
		case DirectColor:
			printf("DirectColor: Alterable RGB maps, pixel RGB subfield indicies\n");
			printf("RGB Masks: 0x%x 0x%x 0x%x\n", visual->red_mask,
			       visual->green_mask, visual->blue_mask);
			break;
		case TrueColor:
			printf("TrueColor: Fixed RGB maps, pixel RGB subfield indicies\n");
			printf("RGB Masks: 0x%x 0x%x 0x%x\n", visual->red_mask,
			       visual->green_mask, visual->blue_mask);
			break;
		case PseudoColor:
			printf("PseudoColor: Alterable RGB maps, single index\n");
			break;
		case StaticColor:
			printf("StaticColor: Fixed RGB maps, single index\n");
			break;
		case GrayScale:
			printf("GrayScale: Alterable map (R=G=B), single index\n");
			break;
		case StaticGray:
			printf("StaticGray: Fixed map (R=G=B), single index\n");
			break;
			default:
			printf("Unknown visual class %d\n",
			       visual->class);
			break;
		}
		printf("Map Entries: %d\n", visual->map_entries);
		printf("Bits per RGB: %d\n", visual->bits_per_rgb);
	}
	XFree((char *) vp);



	printf("==== Standard Colormaps ====\n");
	if (XGetStandardColormap(dpy, win, &cmap, XA_RGB_BEST_MAP)) {
		printf("XA_RGB_BEST_MAP    - Yes (0x%x)\n", cmap.colormap);
		printf("R[0..%d] * %d + G[0..%d] * %d  + B[0..%d] * %d + %d\n",
			cmap.red_max, cmap.red_mult, cmap.green_max, cmap.green_mult,
			cmap.blue_max, cmap.blue_mult, cmap.base_pixel);
	} else
		printf("XA_RGB_BEST_MAP    - No\n");
	if (XGetStandardColormap(dpy, win, &cmap, XA_RGB_DEFAULT_MAP)) {
		printf("XA_RGB_DEFAULT_MAP - Yes (0x%x)\n", cmap.colormap);
		printf("R[0..%d] * %d + G[0..%d] * %d  + B[0..%d] * %d + %d\n",
			cmap.red_max, cmap.red_mult, cmap.green_max, cmap.green_mult,
			cmap.blue_max, cmap.blue_mult, cmap.base_pixel);
	} else
		printf("XA_RGB_DEFAULT_MAP - No\n");
	if (XGetStandardColormap(dpy, win, &cmap, XA_RGB_GRAY_MAP)) {
		printf("XA_RGB_GRAY_MAP    - Yes (0x%x)\n", cmap.colormap);
		printf("R[0..%d] * %d + %d\n",
			cmap.red_max, cmap.red_mult, cmap.base_pixel);
	} else
		printf("XA_RGB_GRAY_MAP    - No\n");
}


/*
 * Allocate backing store for two reaons.  First, if we are running on a
 * truecolor display then the colormaps are not modifiable and colormap
 * ops have to be simulated by manipulating the pixel values.  Second, X
 * does not provide a means to zoom or pan so zooming and panning must
 * also be simulated by manipulating the pixel values.  In order to
 * preserve the semantics of libfb which say that reads will read back
 * the original image, it is necessary to allocate backing store.  This
 * code tries to allocate a System V shared memory segment for backing
 * store.  System V shared memory persists until explicitly killed, so
 * this also means that under X, the previous contents of the frame
 * buffer still exist, and can be accessed again, even though the
 * windows are transient, per-process.
 */
static int
X24_getmem(ifp)
FBIO	*ifp;
{
	struct xinfo *xi = XI(ifp);

	char	*mem = NULL;
	int	pixsize;
	int	size;
	int	new = 0;

	pixsize = ifp->if_max_height * ifp->if_max_width * sizeof(RGBpixel);
	size = pixsize + sizeof (*xi->xi_rgb_cmap);

	/*
	 * get shared mem segment, creating it if it does not exist
	 */
	switch (xi->xi_mode & MODE10_MASK)
	{
	case MODE10_SHARED:
	{
		/* First try to attach to an existing shared memory */

#ifdef HAVE_MMAP
		int fd;

		if ((fd = open(BS_NAME, O_RDWR | O_CREAT, 0666)) < 0)
			fb_log("X24_getmem: can't create fb file, using \
private memory instead, errno %d\n", errno);
		else if (ftruncate(fd, size) < 0)
			fb_log("X24_getmem: can't ftruncate fb file, using \
private memory instead, errno %d\n", errno);
		else if ((mem = mmap(NULL, size, PROT_READ |
			PROT_WRITE, MAP_SHARED, fd, 0)) == (char *) -1)
				fb_log("X24_getmem: can't mmap fb file, \
using private memory instead, errno %d\n", errno);
		else {
			close(fd);
			break;
		}

		/* Change it to local */
		xi->xi_mode = (xi->xi_mode & ~MODE10_MASK) | MODE10_MALLOC;
#else
		if ((xi->xi_shmid = shmget(SHMEM_KEY, size, 0)) < 0) {
			/* No existing one, create a new one */
			xi->xi_shmid = shmget(SHMEM_KEY, size, IPC_CREAT|0666);
			new = 1;
		}

		if (xi->xi_shmid < 0) {
			fb_log("X24_getmem: can't shmget shared memory, using \
private memory instead, errno %d\n", errno);
		}
		else
		{
			/* Open the segment Read/Write */
			if ((mem = shmat(xi->xi_shmid, 0, 0)) != (void *)-1)
				break;
			else
				fb_log("X24_getmem: can't shmat shared memory, \
using private memory instead, errno %d\n", errno);
		}

		/* Change it to local */
		xi->xi_mode = (xi->xi_mode & ~MODE10_MASK) | MODE10_MALLOC;
#endif

		/*FALLTHROUGH*/

	}
	case MODE10_MALLOC:

		if ((mem = malloc(size)) == 0) {
			fb_log("if_X24: Unable to allocate %d bytes of backing \
store\n", size);
			return (-1);
		}
		new = 1;
		break;
	}

	xi->xi_rgb_cmap = (ColorMap *) mem;
	xi->xi_mem = (unsigned char *) mem + sizeof (*xi->xi_rgb_cmap);

	/* Clear memory frame buffer to black */
	if (new) {
		memset(mem, 0, size);
	}

	return (new);
}

/*
 *			X 2 4 _ Z A P M E M
 */
static void
X24_zapmem()
{
 	int	shmid;
	int	i;


#ifdef HAVE_MMAP
	unlink(BS_NAME);
#else
	if ((shmid = shmget(SHMEM_KEY, 0, 0)) < 0) {
		fb_log("X24_zapmem shmget failed, errno=%d\n", errno);
		return;
	}

	i = shmctl(shmid, IPC_RMID, 0);
	if (i < 0) {
		fb_log("X24_zapmem shmctl failed, errno=%d\n", errno);
		return;
	}
#endif
	fb_log("if_X24: shared memory released\n");
	return;
}

static unsigned long
X24_pixel(ifp, r, g, b)
FBIO	*ifp;
int	r, g, b;
{
	struct xinfo *xi = XI(ifp);

	if (xi->xi_flags & FLG_XCMAP)
		return ((b << 16) | (g << 8) | r);
	else
		return ((xi->xi_blumap[b] << 16) |
			(xi->xi_grnmap[g] << 8) |
			xi->xi_redmap[r]);
}

static void
X24_updstate(ifp)
FBIO	*ifp;
{
	struct xinfo *xi = XI(ifp);

	int xwp, ywp;		/* Size of X window in image pixels */
	int xrp, yrp;		/* Leftover X pixels */

	int tp_h, bt_h;		/* Height of top/bottom image pixel slots */
	int lf_w, rt_w;		/* Width of left/right image pixel slots */

	int want, avail;	/* Wanted/available image pixels */

#if UPD_DBG
printf("upd: x %dx%d i %dx%d z %d,%d ctr (%d, %d)\n",
	xi->xi_xwidth, xi->xi_xheight,
	xi->xi_iwidth, xi->xi_iheight,
	ifp->if_xzoom, ifp->if_yzoom,
	ifp->if_xcenter, ifp->if_ycenter);
#endif

	/*
	 * Set ?wp to the number of whole zoomed image pixels we could display
	 * in the X window.
	 */
	xwp = xi->xi_xwidth / ifp->if_xzoom;
	ywp = xi->xi_xheight / ifp->if_yzoom;

	/*
	 * Set ?rp to the number of leftover X pixels we have, after displaying
	 * wp whole zoomed image pixels.
	 */
	xrp = xi->xi_xwidth % ifp->if_xzoom;
	yrp = xi->xi_xheight % ifp->if_yzoom;

	/*
	 * Force ?wp to be the same as the window width (mod 2).  This
	 * keeps the image from jumping around when using large zoom factors.
	 */

	if (xwp && (xwp ^ xi->xi_xwidth) & 1) {
		xwp--;
		xrp += ifp->if_xzoom;
	}

	if (ywp && (ywp ^ xi->xi_xheight) & 1) {
		ywp--;
		yrp += ifp->if_yzoom;
	}

	/*
	 * Now we calculate the height/width of the outermost image pixel slots.
	 * If we've got any leftover X pixels, we'll make truncated slots
	 * out of them; if not, the outermost ones end up full size.  We'll
	 * adjust ?wp to be the number of full and truncated slots available.
	 */
	switch (xrp)
	{
	case 0:
		lf_w = ifp->if_xzoom;
		rt_w = ifp->if_xzoom;
		break;
		
	case 1:
		lf_w = 1;
		rt_w = ifp->if_xzoom;
		xwp += 1;
		break;

	default:
		lf_w = xrp / 2;
		rt_w = xrp - lf_w;
		xwp += 2;
		break;
	}

	switch (yrp)
	{
	case 0:
		tp_h = ifp->if_yzoom;
		bt_h = ifp->if_yzoom;
		break;
		
	case 1:
		tp_h = 1;
		bt_h = ifp->if_yzoom;
		ywp += 1;
		break;

	default:
		tp_h = yrp / 2;
		bt_h = yrp - tp_h;
		ywp += 2;
		break;
	}

	/*
	 * We've now divided our X window up into image pixel slots as follows:
	 * - All slots are xzoom by yzoom X pixels in size, except:
	 *     slots in the top row are tp_h X pixels high
	 *     slots in the bottom row are bt_h X pixels high
	 *     slots in the left column are lf_w X pixels wide
	 *     slots in the right column are rt_w X pixels wide
	 * - The window is xwp by ywp slots in size.
	 */

	/*
	 * We can think of xcenter as being "number of pixels we'd like
	 * displayed on the left half of the screen".  We have xwp/2 pixels
	 * available on the left half.  We use this information to calculate
	 * the remaining parameters as noted.
	 */

	want = ifp->if_xcenter;
	avail = xwp/2;
	if (want >= avail)
	{
		/*
		 * Just enough or too many pixels to display.  We'll be
		 * butted up against the left edge, so
		 *  - the leftmost X pixels will have an x coordinate of 0;
		 *  - the leftmost column of image pixels will be as wide as the
		 *    leftmost column of image pixel slots; and
		 *  - the leftmost image pixel displayed will have an x
		 *    coordinate equal to the number of pixels that didn't fit.
		 */

		xi->xi_xlf = 0;
		xi->xi_ilf_w = lf_w;
		xi->xi_ilf = want - avail;
	} else {
		/*
		 * Not enough image pixels to fill the area.  We'll be
		 * offset from the left edge, so
		 *  - the leftmost X pixels will have an x coordinate equal
		 *    to the number of pixels taken up by the unused image
		 *    pixel slots;
		 *  - the leftmost column of image pixels will be as wide as the
		 *    xzoom width; and
		 *  - the leftmost image pixel displayed will have a zero
		 *    x coordinate.
		 */

		xi->xi_xlf = lf_w + (avail - want - 1) * ifp->if_xzoom;
		xi->xi_ilf_w = ifp->if_xzoom;
		xi->xi_ilf = 0;
	}

	/* Calculation for bottom edge. */

	want = ifp->if_ycenter;
	avail = ywp/2;
	if (want >= avail)
	{
		/*
		 * Just enough or too many pixels to display.  We'll be
		 * butted up against the bottom edge, so
		 *  - the bottommost X pixels will have a y coordinate
		 *    equal to the window height minus 1;
		 *  - the bottommost row of image pixels will be as tall as the
		 *    bottommost row of image pixel slots; and
		 *  - the bottommost image pixel displayed will have a y
		 *    coordinate equal to the number of pixels that didn't fit.
		 */

		xi->xi_xbt = xi->xi_xheight - 1;
		xi->xi_ibt_h = bt_h;
		xi->xi_ibt = want - avail;
	} else {
		/*
		 * Not enough image pixels to fill the area.  We'll be
		 * offset from the bottom edge, so
		 *  - the bottommost X pixels will have a y coordinate equal
		 *    to the window height, less the space taken up by the
		 *    unused image pixel slots, minus 1;
		 *  - the bottom row of image pixels will be as tall as the
		 *    yzoom width; and
		 *  - the bottommost image pixel displayed will have a zero
		 *    y coordinate.
		 */

		xi->xi_xbt = xi->xi_xheight - (bt_h + (avail - want - 1) *
			ifp->if_yzoom) - 1;
		xi->xi_ibt_h = ifp->if_yzoom;
		xi->xi_ibt = 0;
	}

	/* Calculation for right edge. */

	want = xi->xi_iwidth - ifp->if_xcenter;
	avail =  xwp - xwp/2;
	if (want >= avail)
	{
		/*
		 * Just enough or too many pixels to display.  We'll be
		 * butted up against the right edge, so
		 *  - the rightmost X pixels will have an x coordinate equal
		 *    to the window width minus 1;
		 *  - the rightmost column of image pixels will be as wide as
		 *    the rightmost column of image pixel slots; and
		 *  - the rightmost image pixel displayed will have an x
		 *    coordinate equal to the center plus the number of pixels
		 *    that fit, minus 1.
		 */

		xi->xi_xrt = xi->xi_xwidth - 1;
		xi->xi_irt_w = rt_w;
		xi->xi_irt = ifp->if_xcenter + avail - 1;
	} else {
		/*
		 * Not enough image pixels to fill the area.  We'll be
		 * offset from the right edge, so
		 *  - the rightmost X pixels will have an x coordinate equal
		 *    to the window width, less the space taken up by the
		 *    unused image pixel slots, minus 1;
		 *  - the rightmost column of image pixels will be as wide as
		 *    the xzoom width; and
		 *  - the rightmost image pixel displayed will have an x
		 *    coordinate equal to the width of the image minus 1.
		 */

		xi->xi_xrt = xi->xi_xwidth - (rt_w + (avail - want - 1) *
			ifp->if_xzoom) - 1;
		xi->xi_irt_w = ifp->if_xzoom;
		xi->xi_irt = xi->xi_iwidth - 1;
	}

	/* Calculation for top edge. */

	want = xi->xi_iheight - ifp->if_ycenter;
	avail = ywp - ywp/2;
	if (want >= avail)
	{
		/*
		 * Just enough or too many pixels to display.  We'll be
		 * butted up against the top edge, so
		 *  - the topmost X pixels will have a y coordinate of 0;
		 *  - the topmost row of image pixels will be as tall as
		 *    the topmost row of image pixel slots; and
		 *  - the topmost image pixel displayed will have a y
		 *    coordinate equal to the center plus the number of pixels
		 *    that fit, minus 1.
		 */

		xi->xi_xtp = 0;
		xi->xi_itp_h = tp_h;
		xi->xi_itp = ifp->if_ycenter + avail - 1;
	} else {
		/*
		 * Not enough image pixels to fill the area.  We'll be
		 * offset from the top edge, so
		 *  - the topmost X pixels will have a y coordinate equal
		 *    to the space taken up by the unused image pixel slots;
		 *  - the topmost row of image pixels will be as tall as
		 *    the yzoom height; and
		 *  - the topmost image pixel displayed will have a y
		 *    coordinate equal to the height of the image minus 1.
		 */

		xi->xi_xtp = tp_h + (avail - want - 1) * ifp->if_yzoom;
		xi->xi_itp_h = ifp->if_yzoom;
		xi->xi_itp = xi->xi_iheight - 1;
	}

#if UPD_DBG
printf("upd: off (%d, %d) 1 (%d, %d) 2 (%d, %d) wd (%d, %d) ht (%d, %d)\n",
	xi->xi_xlf, xi->xi_xtp, xi->xi_ilf, xi->xi_ibt,
	xi->xi_irt, xi->xi_itp, xi->xi_ilf_w, xi->xi_irt_w,
	xi->xi_ibt_h, xi->xi_itp_h);
#endif
}

static void
X24_blit(ifp, x1, y1, w, h, flags)
FBIO	*ifp;
int x1, y1, w, h;	/* Rectangle of changed bits (image space coord.) */
int flags;		/* BLIT_xxx flags */
{
	struct xinfo *xi = XI(ifp);

	int x2 = x1 + w - 1;	/* Convert to rectangle corners */
	int y2 = y1 + h - 1;

	int x1wd, x2wd, y1ht, y2ht;
	int x, y;
	int ox, oy;
	int xdel, ydel;
	int xwd, xht;

	unsigned char *irgb;
	unsigned long *opix;

#if BLIT_DBG
printf("blit: enter %dx%d at (%d, %d), disp (%d, %d) to (%d, %d)  flags %d\n",
	w, h, x1, y1, xi->xi_ilf, xi->xi_ibt, xi->xi_irt, xi->xi_itp, flags);
#endif

	/*
	 * If the changed rectangle is outside the displayed one, there's
	 * nothing to do
	 */

	if (x1 > xi->xi_irt ||
	    x2 < xi->xi_ilf ||
	    y1 > xi->xi_itp ||
	    y2 < xi->xi_ibt)
		return;

	/*
	 * Clamp to actual displayed portion of image
	 */
	if (x1 < xi->xi_ilf) x1 = xi->xi_ilf;
	if (x2 > xi->xi_irt) x2 = xi->xi_irt;
	if (y1 < xi->xi_ibt) y1 = xi->xi_ibt;
	if (y2 > xi->xi_itp) y2 = xi->xi_itp;

	/*
	 * Figure out sizes of outermost image pixels
	 */
	x1wd = (x1 == xi->xi_ilf) ? xi->xi_ilf_w : ifp->if_xzoom;
	x2wd = (x2 == xi->xi_irt) ? xi->xi_irt_w : ifp->if_xzoom;
	y1ht = (y1 == xi->xi_ibt) ? xi->xi_ibt_h : ifp->if_yzoom;
	y2ht = (y2 == xi->xi_itp) ? xi->xi_itp_h : ifp->if_yzoom;

#if BLIT_DBG
printf("blit: postclip (%d, %d) to (%d, %d) wds (%d, %d) hts (%d, %d)\n",
	x1, y1, x2, y2, x1wd, x2wd, y1ht, y2ht);
#endif

	/* Compute ox: offset from left edge of window to left pixel */

	xdel = x1 - xi->xi_ilf;
	if (xdel)
		ox = x1wd + (xdel - 1 * ifp->if_xzoom) + xi->xi_xlf;
	else
		ox = xi->xi_xlf;


	/* Compute oy: offset from top edge of window to bottom pixel */

	ydel = y1 - xi->xi_ibt;
	if (ydel)
		oy = xi->xi_xbt - (y1ht + (ydel - 1 * ifp->if_yzoom));
	else
		oy = xi->xi_xbt;

#if BLIT_DBG
printf("blit: output to (%d, %d)\n", ox, oy);
#endif

	/*
	 * Set pointers to start of source and destination areas; note that
	 * we're going from lower to higher image coordinates, so irgb
	 * increases, but since images are in quadrant I and X uses quadrant
	 * IV, opix _decreases_.
	 */

	opix = (unsigned long *) &(xi->xi_xpixbuf[(oy * xi->xi_xwidth + ox) *
		sizeof (unsigned long)]);
	irgb = &(xi->xi_mem[(y1 * xi->xi_iwidth + x1) * sizeof (RGBpixel)]);

	if (ifp->if_xzoom == 1 && ifp->if_yzoom == 1) {
		/* Special case if no zooming */

		int j, k;

		for (j = y2 - y1 + 1; j; j--) {
			unsigned char *line_irgb;
			unsigned long *line_opix;
			unsigned char *red = xi->xi_redmap;
			unsigned char *grn = xi->xi_grnmap;
			unsigned char *blu = xi->xi_blumap;

			line_irgb = irgb;
			line_opix = opix;

			/* For each line, convert/copy pixels */

			if (xi->xi_flags & (FLG_XCMAP | FLG_LINCMAP))
				for (k = x2 - x1 + 1; k; k--) {
					*line_opix++ =
						(line_irgb[BLU] << 16) |
						(line_irgb[GRN] << 8) |
						 line_irgb[RED];
					line_irgb += 3;
				}
			else
				for (k = x2 - x1 + 1; k; k--) {
					*line_opix++ =
						(blu[line_irgb[BLU]] << 16) |
						(grn[line_irgb[GRN]] << 8) |
						 red[line_irgb[RED]];
					line_irgb += 3;
				}

			irgb += xi->xi_iwidth * 3;
			opix -= xi->xi_xwidth;
		}
	} else {
		/* General case */

		for (y = y1; y <= y2; y++) {
			int pyht;
			int copied;
			unsigned char *line_irgb;
			unsigned long pix, *line_opix, *prev_line;
			unsigned char *red = xi->xi_redmap;
			unsigned char *grn = xi->xi_grnmap;
			unsigned char *blu = xi->xi_blumap;

			/* Calculate # lines needed */

			if (y == y1)
				pyht = y1ht;
			else if (y == y2)
				pyht = y2ht;
			else
				pyht = ifp->if_yzoom;


			/* Save pointer to start of line */

			line_irgb = irgb;
			prev_line = line_opix = opix;

			/* For the first line, convert/copy pixels */

			if (xi->xi_flags & (FLG_XCMAP | FLG_LINCMAP))
				for (x = x1; x <= x2; x++) {
					int pxwd;

					/* Calculate # pixels needed */

					if (x == x1)
						pxwd = x1wd;
					else if (x == x2)
						pxwd = x2wd;
					else
						pxwd = ifp->if_xzoom;

					/* Get/convert pixel */

					pix = (line_irgb[BLU] << 16) |
						(line_irgb[GRN] << 8) |
						line_irgb[RED];

					line_irgb += 3;

					/* Make as many copies as needed */

					while (pxwd--)
						*line_opix++ = pix;
				}
			else
				for (x = x1; x <= x2; x++) {
					int pxwd;

					/* Calculate # pixels needed */

					if (x == x1)
						pxwd = x1wd;
					else if (x == x2)
						pxwd = x2wd;
					else
						pxwd = ifp->if_xzoom;

					/* Get/convert pixel */

					pix = (blu[line_irgb[BLU]] << 16) |
						(grn[line_irgb[GRN]] << 8) |
						red[line_irgb[RED]];

					line_irgb += 3;

					/* Make as many copies as needed */

					while (pxwd--)
						*line_opix++ = pix;
				}

			copied = line_opix - opix;

			irgb += xi->xi_iwidth * 3;
			opix -= xi->xi_xwidth;

			/* Copy remaining output lines from 1st output line */

			pyht--;
			while (pyht--) {
				memcpy(opix, prev_line, 4 * copied);
				opix -= xi->xi_xwidth;
			}
		}
	}


	/* Figure out size of changed area on screen in X pixels */

	if (x2 == x1)
		xwd = x1wd;
	else
		xwd = x1wd + x2wd + ifp->if_xzoom * (x2 - x1 - 1);

	if (y2 == y1)
		xht = y1ht;
	else
		xht = y1ht + y2ht + ifp->if_yzoom * (y2 - y1 - 1);

	/* Blit out changed image */

	if (flags & BLIT_DISP) {
		XPutImage(xi->xi_dpy, xi->xi_win, xi->xi_gc, xi->xi_image,
			ox, oy - xht + 1, ox, oy - xht + 1, xwd, xht);
#if BLIT_DBG
printf("blit: PutImage of %dx%d to (%d, %d)\n", xwd, xht, ox, oy - xht + 1);
#endif
	}

	/* If we changed the valid region, make a new one. */

	if (flags & (BLIT_PZ | BLIT_RESIZE))
	{
		XRectangle rect;
		Region Nreg = XCreateRegion();


		rect.x = ox;
		rect.y = oy - xht + 1;
		rect.width = xwd;
		rect.height = xht;
		XUnionRectWithRegion(&rect, Nreg, Nreg);

		if (flags & BLIT_PZ) {
			/* If we're panning/zooming, clear out old image. */

			Region Creg = XCreateRegion();

			XSubtractRegion(xi->xi_reg, Nreg, Creg);

			if (!XEmptyRegion(Creg))
			{
				XSetRegion(xi->xi_dpy, xi->xi_cgc, Creg);

				XFillRectangle(xi->xi_dpy, xi->xi_win,
					xi->xi_cgc, 0, 0, xi->xi_xwidth,
					xi->xi_xheight);
			}
			XDestroyRegion(Creg);

		}
		XDestroyRegion(xi->xi_reg);
		xi->xi_reg = Nreg;
	}

	if (flags & BLIT_DISP)
		XFlush(xi->xi_dpy);
}

/** \addtogroup if */

/*@{*/

/** \file if_X24.c
 *  X Window System (X11) libfb interface, supporting 24-, 8-, and 1-bit
 *  displays.
 *
 *  Authors -
 *	Christopher J. Jackson, Sun
 *	Timothy G. Smith, Sun
 *	Michael John Muuss, ARL
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
/*@}*/

#include "common.h"

#ifdef IF_X


#define X_DBG	0
#define UPD_DBG 0
#define BLIT_DBG 0
#define EVENT_DBG 0
#define BLIT_DBG_PIX 0

/* Print a debug message on first time into a piece of code */
#if 0
# define DEBUG1(str)	{static int before=1; if(before) {write(2,str, strlen(str)); before=0;} }
#else
# define DEBUG1(str)	/*NIL*/
#endif

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#if defined(USE_SYS_TIME_H)
#  include <sys/time.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "fb.h"
#include "./fblocal.h"

#ifdef HAVE_SYS_MMAN_H
#  include <unistd.h>
#  include <sys/mman.h>
#  include <fcntl.h>
#  define CAN_LINGER 1
#  undef HAVE_SYS_SHM_H		/* Don't use both ways, mmap is preferred. */
# else
#  ifdef HAVE_SYS_SHM_H
#	include <sys/ipc.h>
#	include <sys/shm.h>
#	define	CAN_LINGER 1
#  endif
#endif

#include <X11/X.h>
#ifdef HAVE_XOSDEFS_H
#include <X11/Xfuncproto.h>
#include <X11/Xosdefs.h>
#endif
#if defined(linux)
#	undef	X_NOT_STDC_ENV
#	undef	X_NOT_POSIX
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <ctype.h>

#define SHMEM_KEY	42


int X24_refresh();
int X24_close_existing();
int X24_open_existing();
int _X24_open_existing();

HIDDEN int	X24_open(),
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

HIDDEN int	X24_getmem();
HIDDEN void	X24_zapmem();
HIDDEN void	X24_destroy();
HIDDEN void	X24_blit();
HIDDEN void	X24_updstate();

HIDDEN	int	x24_linger();
HIDDEN	int	x24_setup();
HIDDEN	void	print_display_info();	/* debug */
HIDDEN	void	X24_createColorCube();
HIDDEN	void	X24_createColorTables();


HIDDEN void X24_handle_event FB_ARGS((FBIO *ifp, XEvent *event));
void X24_configureWindow FB_ARGS((FBIO *ifp, int width, int height));

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
	fb_sim_bwreadrect,
	fb_sim_bwwriterect,
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
	XVisualInfo	xi_visinfo;	/* Visual Info */
	int		xi_depth;	/* Depth of our window */
	GC		xi_gc;		/* current graphics context */
	GC		xi_cgc;		/* graphics context for clipping */
	Region		xi_reg;		/* Valid displayed region */
	int		xi_usereg;	/* Flag determining whether or not to use regions */
	Colormap	xi_cmap;	/* Colormap */
	XImage		*xi_image;	/* XImage (size of screen) */
	Window		xi_cwinp;	/* Cursor's Parent Window ID */
	Window		xi_cwin;	/* Cursor Window ID */
	unsigned long	xi_wp;		/* White pixel */
	unsigned long	xi_bp;		/* Black pixel */

	/*
	 * Pixel buffer usage:
	 *
	 * xi_mem is the actual data received from the user.  It's stored
	 * in 24-bit RGB format, in image-space coordinates.
	 *
	 * xi_pix is the panned, zoomed, and possibly colormapped output
	 * image.  It's stored in whatever X11 format makes sense for our
	 * Visual, in X11-space coordinates.  This is the buffer backing
	 * xi_image.
	 *
	 */

	unsigned char	*xi_mem;	/* 24-bit backing store */
	unsigned char	*xi_pix;	/* X Image buffer */

#ifdef HAVE_SYS_SHM_H
	int		xi_shmid;	/* Sys V shared mem id */
#endif

	unsigned long	xi_mode;	/* 0,1,2 */
	unsigned long	xi_flags;

	ColorMap 	*xi_rgb_cmap;	/* User's libfb colormap */
	unsigned char	*xi_redmap;	/* Fake colormap for non-DirectColor */
	unsigned char	*xi_blumap;	/* Fake colormap for non-DirectColor */
	unsigned char	*xi_grnmap;	/* Fake colormap for non-DirectColor */

	unsigned char	*xi_ccredtbl;	/* Lookup table for red component */
	unsigned char	*xi_ccgrntbl;	/* Lookup table for green component */
	unsigned char	*xi_ccblutbl;	/* Lookup table for blue component */

	unsigned char	*xi_andtbl;	/* Lookup table for 1-bit dithering */
	unsigned char	*xi_ortbl;	/* Lookup table for 1-bit dithering */

	int		xi_ncolors;	/* Number of colors in colorcube */
	int		xi_base;	/* Base color in colorcube */

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


/* Flags in xi_flags */

#define FLG_VMASK       0x07	/* Visual mask */
				/* Note: values below are in preference order*/
#define FLG_VD24        0x01	/* 24-bit DirectColor */
#define FLG_VT24        0x02	/* 24-bit TrueColor */
#define FLG_VD16        0x03	/* 16-bit DirectColor */
#define FLG_VT16        0x04	/* 16-bit TrueColor */
#define FLG_VP8         0x05	/* 8-bit PseudoColor */
#define FLG_VS8         0x06	/* 8-bit StaticGray */
#define FLG_VG8		0x07	/* 8-bit GrayScale */
#define FLG_VS1         0x08	/* 1-bit StaticGray */

#define FLG_LINCMAP	0x10	/* We're using a linear colormap */
#define FLG_XCMAP	0x20	/* The X server can do colormapping for us */
#define FLG_INIT	0x40	/* Display is fully initialized */

/* Mode flags for open */

#define MODE1_MASK	(1<<1)
#define MODE1_TRANSIENT	(0<<1)
#define MODE1_LINGERING (1<<1)

#define MODEV_MASK	(7<<1)

#define MODE10_MASK	(1<<10)
#define MODE10_MALLOC	(0<<10)
#define MODE10_SHARED	(1<<10)

#define MODE11_MASK	(1<<11)
#define MODE11_NORMAL	(0<<11)
#define MODE11_ZAP	(1<<11)

static struct modeflags {
	char		c;
	unsigned long	mask;
	unsigned long	value;
	char		*help;
} modeflags[] = {
	{ 'l',	MODE1_MASK, MODE1_LINGERING,
		"Lingering window - else transient" },
	{ 's',  MODE10_MASK, MODE10_SHARED,
		"Use shared memory backing store" },
	{ 'z',	MODE11_MASK, MODE11_ZAP,
		"Zap (free) shared memory" },
	{ 'D',	MODEV_MASK, FLG_VD24 << 1,
		"Select 24-bit DirectColor display if available" },
	{ 'T',	MODEV_MASK, FLG_VT24 << 1,
		"Select 24-bit TrueColor display if available" },
	{ 'P',	MODEV_MASK, FLG_VP8 << 1,
		"Select 8-bit PseudoColor display if available" },
	{ 'S',	MODEV_MASK, FLG_VS8 << 1,
		"Select 8-bit StaticGray display if available" },
	{ 'G',	MODEV_MASK, FLG_VG8 << 1,
		"Select 8-bit GrayScale display if available" },
	{ 'M',	MODEV_MASK, FLG_VS1 << 1,
		"Select 1-bit StaticGray display if available" },
	{ '\0', 0, 0, "" }
};

/* Flags for X24_blit's flags argument */

#define	BLIT_DISP	0x1	/* Write bits to screen */
#define BLIT_PZ		0x2	/* This is a pan or zoom */
#define BLIT_RESIZE	0x4	/* We just resized (screen empty) */

#define BS_NAME	"/tmp/X24_fb"

/* Elements of 6x9x4 colorcube */

static unsigned char reds[] = { 0, 51, 102, 153, 204, 255 };
static unsigned char grns[] = { 0, 32, 64, 96, 128, 159, 191, 223, 255 };
static unsigned char blus[] = { 0, 85, 170, 255 };

/* Dither masks */

static float dmsk881[] = {
0.705882, 0.956863, 0.235294, 0.486275, 0.737255, 0.988235, 0.203922, 0.454902,
0.172549, 0.423529, 0.674510, 0.925490, 0.141176, 0.392157, 0.643137, 0.894118,
0.580392, 0.831373, 0.109804, 0.360784, 0.611765, 0.862745, 0.078431, 0.329412,
0.047059, 0.298039, 0.549020, 0.800000, 0.015686, 0.266667, 0.517647, 0.768628,
0.721569, 0.972549, 0.188235, 0.439216, 0.690196, 0.941177, 0.219608, 0.470588,
0.125490, 0.376471, 0.627451, 0.878431, 0.156863, 0.407843, 0.658824, 0.909804,
0.596078, 0.847059, 0.062745, 0.313726, 0.564706, 0.815686, 0.094118, 0.345098,
0.000000, 0.250980, 0.501961, 0.752941, 0.031373, 0.282353, 0.533333, 0.784314
};

static float dmsk883[] = {
0.784314, 0.533333, 0.282353, 0.031373, 0.752941, 0.501961, 0.250980, 0.000000,
0.345098, 0.094118, 0.815686, 0.564706, 0.313726, 0.062745, 0.847059, 0.596078,
0.909804, 0.658824, 0.407843, 0.156863, 0.878431, 0.627451, 0.376471, 0.125490,
0.470588, 0.219608, 0.941177, 0.690196, 0.439216, 0.188235, 0.972549, 0.721569,
0.768628, 0.517647, 0.266667, 0.015686, 0.800000, 0.549020, 0.298039, 0.047059,
0.329412, 0.078431, 0.862745, 0.611765, 0.360784, 0.109804, 0.831373, 0.580392,
0.894118, 0.643137, 0.392157, 0.141176, 0.925490, 0.674510, 0.423529, 0.172549,
0.454902, 0.203922, 0.988235, 0.737255, 0.486275, 0.235294, 0.956863, 0.705882,

0.988235, 0.737255, 0.486275, 0.235294, 0.956863, 0.705882, 0.454902, 0.203922,
0.392157, 0.141176, 0.925490, 0.674510, 0.423529, 0.172549, 0.894118, 0.643137,
0.862745, 0.611765, 0.360784, 0.109804, 0.831373, 0.580392, 0.329412, 0.078431,
0.266667, 0.015686, 0.800000, 0.549020, 0.298039, 0.047059, 0.768628, 0.517647,
0.941177, 0.690196, 0.439216, 0.188235, 0.972549, 0.721569, 0.470588, 0.219608,
0.407843, 0.156863, 0.878431, 0.627451, 0.376471, 0.125490, 0.909804, 0.658824,
0.815686, 0.564706, 0.313726, 0.062745, 0.847059, 0.596078, 0.345098, 0.094118,
0.282353, 0.031373, 0.752941, 0.501961, 0.250980, 0.000000, 0.784314, 0.533333,

0.000000, 0.250980, 0.501961, 0.752941, 0.031373, 0.282353, 0.533333, 0.784314,
0.596078, 0.847059, 0.062745, 0.313726, 0.564706, 0.815686, 0.094118, 0.345098,
0.125490, 0.376471, 0.627451, 0.878431, 0.156863, 0.407843, 0.658824, 0.909804,
0.721569, 0.972549, 0.188235, 0.439216, 0.690196, 0.941177, 0.219608, 0.470588,
0.047059, 0.298039, 0.549020, 0.800000, 0.015686, 0.266667, 0.517647, 0.768628,
0.580392, 0.831373, 0.109804, 0.360784, 0.611765, 0.862745, 0.078431, 0.329412,
0.172549, 0.423529, 0.674510, 0.925490, 0.141176, 0.392157, 0.643137, 0.894118,
0.705882, 0.956863, 0.235294, 0.486275, 0.737255, 0.988235, 0.203922, 0.454902
};

/* Luminance factor tables (filled in in x24_setup()) */

static int lumdone = 0;		/* Nonzero if tables valid */
static unsigned long rlumtbl[256];
static unsigned long glumtbl[256];
static unsigned long blumtbl[256];


/*
 *			X 2 4 _ O P E N
 */
HIDDEN int
X24_open(ifp, file, width, height)
FBIO	*ifp;
char	*file;
int	width, height;
{
	struct xinfo *xi;

	unsigned long mode;			/* local copy */
	int getmem_stat;

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

	if ((getmem_stat = X24_getmem(ifp)) == -1) {
		X24_destroy(xi);
		return (-1);
	}

	/* Set up an X window, graphics context, etc. */

	if (x24_setup(ifp, width, height) < 0) {
		X24_destroy(xi);
		return(-1);
	}

	/* Update state for blits */

	X24_updstate(ifp);

	/* Make the Display connection available for selecting on */
	ifp->if_selfd = ConnectionNumber(xi->xi_dpy);

	/* If we already have data, display it */

	if (getmem_stat == 0) {
		X24_wmap(ifp, xi->xi_rgb_cmap);
		X24_blit(ifp, 0, 0, xi->xi_iwidth, xi->xi_iheight, BLIT_DISP);
	} else {
		/* Set up default linear colormap */
		X24_wmap(ifp, NULL);
	}

	/* Mark display ready */

	xi->xi_flags |= FLG_INIT;


	return(0);
}

int
X24_open_existing(ifp, argc, argv)
FBIO *ifp;
int argc;
char **argv;
{
  Display *dpy;
  Window win;
  Window cwinp;
  Colormap cmap;
  XVisualInfo *vip;
  int width;
  int height;
  GC gc;

  if(argc != 9)
    return -1;

  if(sscanf(argv[1], "%lu", (unsigned long *)&dpy) != 1)
     return -1;

  if(sscanf(argv[2], "%lu", (unsigned long *)&win) != 1)
    return -1;

  if(sscanf(argv[3], "%lu", (unsigned long *)&cwinp) != 1)
    return -1;

  if(sscanf(argv[4], "%lu", (unsigned long *)&cmap) != 1)
    return -1;

  if(sscanf(argv[5], "%lu", (unsigned long *)&vip) != 1)
    return -1;

  if(sscanf(argv[6], "%d", &width) != 1)
    return -1;

  if(sscanf(argv[7], "%d", &height) != 1)
    return -1;

  if(sscanf(argv[8], "%lu", (unsigned long *)&gc) != 1)
    return -1;

  return _X24_open_existing(ifp, dpy, win, cwinp, cmap, vip, width, height, gc);
}

int
_X24_open_existing(ifp, dpy, win, cwinp, cmap, vip, width, height, gc)
FBIO *ifp;
Display *dpy;
Window win;
Window cwinp;
Colormap cmap;
XVisualInfo *vip;
int width;
int height;
GC gc;
{
  struct xinfo *xi;
#if 0
  XRectangle rect;
#endif
  int getmem_stat;

  ifp->if_width = width;
  ifp->if_height = height;

  ifp->if_xzoom = 1;
  ifp->if_yzoom = 1;

  ifp->if_xcenter = width/2;
  ifp->if_ycenter = height/2;

  /* create a struct of state information */
  if ((xi = (struct xinfo *)calloc(1, sizeof(struct xinfo))) == NULL) {
    fb_log("X24_open: xinfo malloc failed\n");
    return -1;
  }
  XI_SET(ifp, xi);

  /* X setup */
  xi->xi_xwidth = width;
  xi->xi_xheight = height;
  xi->xi_dpy = dpy;
  xi->xi_screen = DefaultScreen(xi->xi_dpy);

  xi->xi_visinfo = *vip;		/* struct copy */
  xi->xi_visual = vip->visual;
  xi->xi_depth = vip->depth;
  xi->xi_cmap = cmap;
  xi->xi_win = win;
  xi->xi_cwinp = cwinp;

  /*XXX For now use same GC for both */
  xi->xi_gc = gc;
  xi->xi_cgc = gc;


  switch (vip->class) {
  case TrueColor:
    if (vip->depth >= 24) {
      xi->xi_mode = FLG_VT24 << 1;
      xi->xi_flags = FLG_VT24;
      xi->xi_wp = 0xFFFFFF;
      xi->xi_bp = 0x000000;
    } else if (vip->depth >= 16) {
      xi->xi_mode = FLG_VT16 << 1;
      xi->xi_flags = FLG_VT16;
      xi->xi_wp = 0xFFFFFF;
      xi->xi_bp = 0x000000;
    } else {
      xi->xi_mode = FLG_VS1 << 1;
      xi->xi_flags = FLG_VS1;
      xi->xi_wp = 0x0;
      xi->xi_bp = 0x1;
    }

    break;
  case DirectColor:
    if (vip->depth >= 24) {
      xi->xi_mode = FLG_VD24 << 1;
      xi->xi_flags = FLG_VD24 | FLG_XCMAP;
      xi->xi_wp = 0xFFFFFF;
      xi->xi_bp = 0x000000;
    } else if (vip->depth >= 16) {
      xi->xi_mode = FLG_VD16 << 1;
      xi->xi_flags = FLG_VD16 | FLG_XCMAP;
      xi->xi_wp = 0xFFFFFF;
      xi->xi_bp = 0x000000;
    } else {
      xi->xi_mode = FLG_VS1 << 1;
      xi->xi_flags = FLG_VS1 | FLG_XCMAP;
      xi->xi_wp = 0x0;
      xi->xi_bp = 0x1;
    }

    break;
  case PseudoColor:
    if (vip->depth >= 8) {
      xi->xi_mode = FLG_VP8 << 1;
      xi->xi_flags = FLG_VP8 | FLG_XCMAP;

      xi->xi_ncolors = sizeof (reds) * sizeof (blus) * sizeof (grns);
      xi->xi_base = 255 - xi->xi_ncolors;
      xi->xi_bp = xi->xi_base;
      xi->xi_wp = xi->xi_base + xi->xi_ncolors - 1;

      X24_createColorTables(xi);
    } else {
      xi->xi_mode = FLG_VS1 << 1;
      xi->xi_flags = FLG_VS1 | FLG_XCMAP;
      xi->xi_wp = 0x0;
      xi->xi_bp = 0x1;
    }

    break;
  default:
    xi->xi_mode = FLG_VS1 << 1;
    xi->xi_flags = FLG_VS1 | FLG_XCMAP;
    xi->xi_wp = 0x0;
    xi->xi_bp = 0x1;

    break;
  }

  if (!(xi->xi_flags & FLG_XCMAP)) {
	  xi->xi_redmap = (unsigned char *)malloc(256);
	  xi->xi_grnmap = (unsigned char *)malloc(256);
	  xi->xi_blumap = (unsigned char *)malloc(256);

	  if (!xi->xi_redmap || !xi->xi_grnmap || !xi->xi_blumap) {
		  fb_log("if_X24: Can't allocate colormap memory\n");
		  return (-1);
	  }
  }

  xi->xi_iwidth = width;
  xi->xi_iheight = height;

  /* Allocate backing store (shared memory or local) */
  if ((getmem_stat = X24_getmem(ifp)) == -1) {
	  free((char *)xi);
	  return -1;
  }

#if 0
  /* Initialize the valid region */
  xi->xi_reg = XCreateRegion();
  rect.x = 0;
  rect.y = 0;
  rect.width = xi->xi_xwidth;
  rect.height = xi->xi_xheight;
  XUnionRectWithRegion(&rect, xi->xi_reg, xi->xi_reg);
#else
  /* We're not using Region's */
  xi->xi_reg = (Region)0;
  xi->xi_usereg = 0;
#endif

  /* this will be reallocated in the call to X24_configureWindow */
  if ((xi->xi_pix = (unsigned char *) calloc(1, 1)) == NULL) {
    fb_log("X24_open: calloc failed\n");
    return -1;
  }

  /* this will be destroyed in the call to X24_configureWindow */
  xi->xi_image = XCreateImage(xi->xi_dpy,
			      xi->xi_visual, xi->xi_depth, ZPixmap, 0,
			      (char *) xi->xi_pix, 1, 1, 8, 0);

  /* Update state for blits */
  X24_updstate(ifp);

  /* Make the Display connection available for selecting on */
  ifp->if_selfd = ConnectionNumber(xi->xi_dpy);

  if (getmem_stat == 0) {
	  X24_wmap(ifp, xi->xi_rgb_cmap);
	  X24_blit(ifp, 0, 0, xi->xi_iwidth, xi->xi_iheight, BLIT_DISP);
  } else {
	  /* Set up default linear colormap */
	  X24_wmap(ifp, NULL);
  }

  /* Mark display ready */
  xi->xi_flags |= FLG_INIT;

  /* force reconfiguration */
  xi->xi_xwidth = 0;
  xi->xi_xheight = 0;
  X24_configureWindow(ifp, width, height);

  return 0;
}

HIDDEN int
X24_close(ifp)
FBIO	*ifp;
{
	struct xinfo *xi = XI(ifp);

	XFlush(xi->xi_dpy);
	if ((xi->xi_mode & MODE1_MASK) == MODE1_LINGERING) {
		if (x24_linger(ifp))
			return(0);	/* parent leaves the display */
	}

	X24_destroy(xi);

	return (0);
}

int
X24_close_existing(ifp)
FBIO    *ifp;
{
  struct xinfo *xi = XI(ifp);

  if (xi->xi_image)
    XDestroyImage(xi->xi_image);

  if (xi->xi_reg)
    XDestroyRegion(xi->xi_reg);

  if (xi->xi_ccredtbl)
    free(xi->xi_ccredtbl);
  if (xi->xi_ccgrntbl)
    free(xi->xi_ccgrntbl);
  if (xi->xi_ccblutbl)
    free(xi->xi_ccblutbl);

  free((char *)xi);

  return (0);
}

HIDDEN void
X24_destroy(xi)
struct xinfo *xi;
{
	if (xi) {
		if (xi->xi_rgb_cmap &&
		    (xi->xi_mode & MODE10_MASK) == MODE10_MALLOC)
			free(xi->xi_rgb_cmap);

		if (xi->xi_redmap)
			free(xi->xi_redmap);
		if (xi->xi_grnmap)
			free(xi->xi_grnmap);
		if (xi->xi_blumap)
			free(xi->xi_blumap);
		if (xi->xi_ccredtbl)
			free(xi->xi_ccredtbl);
		if (xi->xi_ccgrntbl)
			free(xi->xi_ccgrntbl);
		if (xi->xi_ccblutbl)
			free(xi->xi_ccblutbl);
		if (xi->xi_andtbl)
			free(xi->xi_andtbl);
		if (xi->xi_ortbl)
			free(xi->xi_ortbl);

		if (xi->xi_dpy) {
			if (xi->xi_cgc)
				XFreeGC(xi->xi_dpy, xi->xi_cgc);

			if (xi->xi_gc)
				XFreeGC(xi->xi_dpy, xi->xi_gc);

			if (xi->xi_image)
				XDestroyImage(xi->xi_image);

			if (xi->xi_reg)
				XDestroyRegion(xi->xi_reg);

			XCloseDisplay(xi->xi_dpy);
		}

		free((char *) xi);
	}
}

HIDDEN int
X24_clear(ifp, pp)
FBIO	*ifp;
unsigned char	*pp;
{
  struct xinfo *xi = XI(ifp);

  int red, grn, blu;
  int npix;
  int n;
  unsigned char *cp;

#if X_DBG
  printf("X24_clear(ifp:0x%x, pp:0x%x) pixel = (%d, %d, %d): entered.\n",
	 ifp, pp, pp[RED], pp[GRN], pp[BLU]);
#endif

  if(pp == (unsigned char *)NULL){
    red = grn = blu = 0;
  }else{
    red = pp[RED];
    grn = pp[GRN];
    blu = pp[BLU];
  }

  /* Clear the backing store */
  npix = xi->xi_iwidth * xi->xi_xheight;

  if(red == grn && red == blu){
    memset(xi->xi_mem, red, npix*3);
  }else{
    cp = xi->xi_mem;
    n = npix;
    while(n--){
      *cp++ = red;
      *cp++ = grn;
      *cp++ = blu;
    }
  }

  X24_blit(ifp, 0, 0, xi->xi_iwidth, xi->xi_iheight,
	   BLIT_DISP | BLIT_PZ);

  return(0);
}


HIDDEN int
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


HIDDEN int
X24_write(ifp, x, y, pixelp, count)
FBIO	*ifp;
int	x, y;
const unsigned char	*pixelp;
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

HIDDEN int
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

HIDDEN int
X24_wmap(ifp, cmp)
FBIO	*ifp;
const ColorMap	*cmp;
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

	if (cmp) {
		if (cmp != map)
			memcpy(map, cmp, sizeof (ColorMap));
	} else {
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

HIDDEN int
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

HIDDEN int
X24_getview(ifp, xcenter, ycenter, xzoom, yzoom)
FBIO	*ifp;
int	*xcenter, *ycenter;
int	*xzoom, *yzoom;
{

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

/*ARGSUSED*/
HIDDEN int
X24_setcursor(ifp, bits, xbits, ybits, xorig, yorig)
FBIO	*ifp;
const unsigned char *bits;
int	xbits, ybits;
int	xorig, yorig;
{

#if X_DBG
printf("X24_setcursor(ifp:0x%x, bits:%u, xbits:%d, ybits:%d, xorig:%d, yorig:%d) entered.\n",
	ifp, bits, xbits, ybits, xorig, yorig);
#endif

	return(0);
}

HIDDEN int
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

  if (mode) {
    register int xx, xy;
    register int delta;

    /* If we don't have a cursor, create it */
    if (!xi->xi_cwin) {
      XSetWindowAttributes xswa;

      xswa.background_pixel = xi->xi_bp;
      xswa.border_pixel = xi->xi_wp;
      xswa.colormap = xi->xi_cmap;
      xswa.save_under = True;

      xi->xi_cwin = XCreateWindow(xi->xi_dpy, xi->xi_cwinp,
				     0, 0, 4, 4, 2, xi->xi_depth, InputOutput,
				     xi->xi_visual, CWBackPixel | CWBorderPixel |
				     CWSaveUnder | CWColormap, &xswa);
    }

    delta = ifp->if_width/ifp->if_xzoom/2;
    xx = x - (ifp->if_xcenter - delta);
    xx *= ifp->if_xzoom;
    xx += ifp->if_xzoom/2;  /* center cursor */

    delta = ifp->if_height/ifp->if_yzoom/2;
    xy = y - (ifp->if_ycenter - delta);
    xy *= ifp->if_yzoom;
    xy += ifp->if_yzoom/2;  /* center cursor */
    xy = xi->xi_xheight - xy;

    /* Move cursor into place; make it visible if it isn't */
    XMoveWindow(xi->xi_dpy, xi->xi_cwin, xx - 4, xy - 4);

    if (!ifp->if_cursmode)
      XMapRaised(xi->xi_dpy, xi->xi_cwin);
  } else {
    /* If we have a cursor and it's visible, hide it */
    if (xi->xi_cwin && ifp->if_cursmode)
      XUnmapWindow(xi->xi_dpy, xi->xi_cwin);
  }

  /* Without this flush, cursor movement is sluggish */
  XFlush(xi->xi_dpy);

  /* Update position of cursor */
  ifp->if_cursmode = mode;
  ifp->if_xcurs = x;
  ifp->if_ycurs = y;

  return(0);
}

HIDDEN int
X24_getcursor(ifp, mode, x, y)
FBIO	*ifp;
int	*mode;
int	*x, *y;
{

#if X_DBG
printf("X24_getcursor(ifp:0x%x, mode:%d, x:0x%x, y:0x%x) entered.\n",
	ifp, mode, x, y);
#endif

	fb_sim_getcursor(ifp, mode, x, y);

	return(0);
}

HIDDEN int
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

HIDDEN int
X24_writerect(ifp, xmin, ymin, width, height, pp)
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
const unsigned char	*pp;
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

HIDDEN int
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
	  X24_handle_event(ifp, &event);

	return(0);
}

HIDDEN int
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

HIDDEN int
X24_free(ifp)
FBIO	*ifp;
{

#if X_DBG
printf("X24_free(ifp:0x%x) entered\n", ifp);
#endif

	return(0);
}

HIDDEN int
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
	fb_log("Usage: /dev/X[options]\n");
	for(mfp = modeflags; mfp->c != '\0'; mfp++) {
		fb_log("   %c   %s\n", mfp->c, mfp->help);
	}

	fb_log( "\nCurrent internal state:\n");
	fb_log( "	xi_depth=%d\n", xi->xi_depth );
	fb_log( "	xi_mode=%d\n", xi->xi_mode );
	fb_log( "	xi_flags=%d\n", xi->xi_flags );
	fb_log( "	xi_xwidth=%d\n", xi->xi_xwidth );
	fb_log( "	xi_xheight=%d\n", xi->xi_xheight );

	fb_log("X11 Visual:\n");
	fb_log( "	class=%d\n", xi->xi_visinfo.class );

	switch(xi->xi_visinfo.class) {
	case DirectColor:
		fb_log("\tDirectColor: Alterable RGB maps, pixel RGB subfield indicies\n");
		fb_log("\tRGB Masks: 0x%x 0x%x 0x%x\n", xi->xi_visinfo.red_mask,
		       xi->xi_visinfo.green_mask, xi->xi_visinfo.blue_mask);
		break;
	case TrueColor:
		fb_log("\tTrueColor: Fixed RGB maps, pixel RGB subfield indicies\n");
		fb_log("\tRGB Masks: 0x%x 0x%x 0x%x\n", xi->xi_visinfo.red_mask,
		       xi->xi_visinfo.green_mask, xi->xi_visinfo.blue_mask);
		break;
	case PseudoColor:
		fb_log("\tPseudoColor: Alterable RGB maps, single index\n");
		break;
	case StaticColor:
		fb_log("\tStaticColor: Fixed RGB maps, single index\n");
		break;
	case GrayScale:
		fb_log("\tGrayScale: Alterable map (R=G=B), single index\n");
		break;
	case StaticGray:
		fb_log("\tStaticGray: Fixed map (R=G=B), single index\n");
		break;
	default:
		fb_log("\tUnknown visual class %d\n",
		       xi->xi_visinfo.class);
		break;
	}
	fb_log("\tColormap Size: %d\n", xi->xi_visinfo.colormap_size);
	fb_log("\tBits per RGB: %d\n", xi->xi_visinfo.bits_per_rgb);
	fb_log("\tscreen: %d\n", xi->xi_visinfo.screen);
	fb_log("\tdepth (total bits per pixel): %d\n", xi->xi_visinfo.depth);
	if( xi->xi_visinfo.depth < 24 )
		fb_log("\tWARNING: unable to obtain full 24-bits of color, image will be quantized.\n");

	return(0);
}

/*
  Create 6x9x4 color cube.
*/
HIDDEN void
X24_createColorCube(xi)
struct xinfo *xi;
{
  int i;
  int redmul, grnmul;
  unsigned long pixels[256], pmask[1], pixel[1];
  XColor colors[256];

  /*
   * Color cube is in RGB order
   */
  grnmul = sizeof (blus);
  redmul = sizeof (blus) * sizeof (grns);

  XAllocColorCells(xi->xi_dpy, xi->xi_cmap, 1, pmask, 0, pixels,
		   xi->xi_base + xi->xi_ncolors);

  for (pixel[0] = 0; pixel[0] < xi->xi_base; pixel[0]++) {
    XFreeColors(xi->xi_dpy, xi->xi_cmap, pixel, 1, 0);
  }

  /* Fill the colormap and the colorcube */
  for (i = 0; i < xi->xi_ncolors; i++) {
    colors[i].red = reds[i / redmul] << 8;
    colors[i].green = grns[(i % redmul) / grnmul] << 8;
    colors[i].blue = blus[i % grnmul] << 8;
    colors[i].flags = DoRed | DoGreen | DoBlue;
    colors[i].pixel = xi->xi_base + i;
  }

  XStoreColors(xi->xi_dpy, xi->xi_cmap, colors, xi->xi_ncolors);
}

/*
  Create fast lookup tables for dithering
*/
HIDDEN void
X24_createColorTables(xi)
struct xinfo *xi;
{
  int i, j, idx;
  int redmul, grnmul;

  grnmul = sizeof (blus);
  redmul = sizeof (blus) * sizeof (grns);

  xi->xi_ccredtbl = (unsigned char *)malloc(64 * 256);
  xi->xi_ccgrntbl = (unsigned char *)malloc(64 * 256);
  xi->xi_ccblutbl = (unsigned char *)malloc(64 * 256);

  for (i = 0; i < 256; i++) {
    int redval, grnval, bluval;
    int redtbl, grntbl, blutbl;
    int reditbl, grnitbl, bluitbl;

    idx = i / (256 / (sizeof (reds) - 1));
    reditbl = redtbl = idx * redmul;
    if (idx < (sizeof (reds) - 1))
      reditbl += redmul;
    redval = reds[idx];

    idx = i / (256 / (sizeof (grns) - 1));
    grnitbl = grntbl = idx * grnmul;
    if (idx < (sizeof (grns) - 1))
      grnitbl += grnmul;
    grnval = grns[idx];

    idx = i / (256 / (sizeof (blus) - 1));
    bluitbl = blutbl = idx;
    if (idx < (sizeof (blus) - 1))
      bluitbl++;
    bluval = blus[idx];

    for (j = 0; j < 64; j++) {
      if (i - redval > (256 / (sizeof (reds) - 1)) *
	  dmsk883[128+j])
	xi->xi_ccredtbl[(i << 6) + j] = reditbl;
      else
	xi->xi_ccredtbl[(i << 6) + j] = redtbl;

      if (i - grnval > (256 / (sizeof (grns) - 1)) *
	  dmsk883[64+j])
	xi->xi_ccgrntbl[(i << 6) + j] = grnitbl;
      else
	xi->xi_ccgrntbl[(i << 6) + j] = grntbl;

      if (i - bluval > (256 / (sizeof (blus) - 1)) *
	  dmsk883[j])
	xi->xi_ccblutbl[(i << 6) + j] = bluitbl;
      else
	xi->xi_ccblutbl[(i << 6) + j] = blutbl;
    }
  }
}

HIDDEN
int
x24_setup(ifp, width, height)
FBIO	*ifp;
int	width, height;
{
	struct xinfo *xi = XI(ifp);

	XGCValues	gcv;
	XSizeHints	xsh;		/* part of the "standard" props */
	XWMHints	xwmh;		/* size guidelines for window mngr */
	XSetWindowAttributes xswa;
	XRectangle rect;
	char		*xname;

#if X_DBG
printf("x24_setup(ifp:0x%x, width:%d, height:%d) entered\n", ifp, width, height);
#endif

	/* Save these in state structure */

	xi->xi_xwidth = width;
	xi->xi_xheight = height;

	/* Open the display - use the env variable DISPLAY */
	xname = XDisplayName(NULL);
	/* Attempt one level of fallback, esp. for fbserv daemon */
	if( !xname || *xname == '\0' )  xname = ":0";

	if ((xi->xi_dpy = XOpenDisplay(xname)) == NULL) {
		fb_log("if_X: Can't open X display \"%s\"\n", xname);
		return	-1;
	}

#if 0
	print_display_info(xi->xi_dpy);
#endif

	/* Use the screen we connected to */
	xi->xi_screen = DefaultScreen(xi->xi_dpy);

	/*
	 * Here we try to get the best possible visual that's no better than the
	 * one that the user asked for.  Note that each case falls through to
	 * the next.
	 */

	switch ((xi->xi_mode & MODEV_MASK) >> 1)
	{
	default:
	case FLG_VD24:
		if (XMatchVisualInfo(xi->xi_dpy, xi->xi_screen, 24, DirectColor,
		    &xi->xi_visinfo)) {
			xi->xi_flags |= FLG_XCMAP | FLG_VD24;
			break;
		}
		/*FALLTHROUGH*/
	case FLG_VT24:
		if (XMatchVisualInfo(xi->xi_dpy, xi->xi_screen, 24, TrueColor,
		    &xi->xi_visinfo)) {
			xi->xi_flags |= FLG_VT24;
			break;
		}
		/*FALLTHROUGH*/
	case FLG_VD16:
		if (XMatchVisualInfo(xi->xi_dpy, xi->xi_screen, 16, DirectColor,
		    &xi->xi_visinfo)) {
			xi->xi_flags |= FLG_XCMAP | FLG_VD16;
			break;
		}
		/*FALLTHROUGH*/
	case FLG_VT16:
		if (XMatchVisualInfo(xi->xi_dpy, xi->xi_screen, 16, TrueColor,
		    &xi->xi_visinfo)) {
			xi->xi_flags |= FLG_VT16;
			break;
		}
		/*FALLTHROUGH*/
	case FLG_VP8:
		if (XMatchVisualInfo(xi->xi_dpy, xi->xi_screen, 8, PseudoColor,
		    &xi->xi_visinfo)) {
			xi->xi_flags |= FLG_VP8;
			break;
		}
		/*FALLTHROUGH*/
	case FLG_VS8:
		if (XMatchVisualInfo(xi->xi_dpy, xi->xi_screen, 8, StaticGray,
		    &xi->xi_visinfo)) {
			xi->xi_flags |= FLG_VS8;
			break;
		}
		/*FALLTHROUGH*/
	case FLG_VG8:
		if (XMatchVisualInfo(xi->xi_dpy, xi->xi_screen, 8, GrayScale,
		    &xi->xi_visinfo)) {
			xi->xi_flags |= FLG_VG8;
			break;
		}
		/*FALLTHROUGH*/
	case FLG_VS1:
		if (XMatchVisualInfo(xi->xi_dpy, xi->xi_screen, 1, StaticGray,
		    &xi->xi_visinfo)) {
			xi->xi_flags |= FLG_VS1;
			break;
		}
		/*FALLTHROUGH*/
		fb_log("if_X24: Can't get supported Visual on X display \"%s\"\n",
		       XDisplayName(NULL));
		print_display_info(xi->xi_dpy);
		return (-1);
	}

	xi->xi_visual = xi->xi_visinfo.visual;
	xi->xi_depth = xi->xi_visinfo.depth;

	/* Set up colormaps, white/black pixels */

	switch (xi->xi_flags & FLG_VMASK)
	{
	case FLG_VD24:
		xi->xi_cmap = XCreateColormap(xi->xi_dpy, RootWindow(xi->xi_dpy,
			xi->xi_screen), xi->xi_visual, AllocAll);
		xi->xi_wp = 0xFFFFFF;
		xi->xi_bp = 0x000000;
		break;

	case FLG_VT24:
		/*
		 * We need this colormap, even though we're not really going to
		 * use it, because if we don't specify a colormap when we
		 * create the window (thus getting the default), and the
		 * default visual is not 24-bit, the window create will fail.
		 */

		xi->xi_cmap = XCreateColormap(xi->xi_dpy, RootWindow(xi->xi_dpy,
			xi->xi_screen), xi->xi_visual, AllocNone);
		xi->xi_wp = 0xFFFFFF;
		xi->xi_bp = 0x000000;
		break;

	case FLG_VD16:
		xi->xi_cmap = XCreateColormap(xi->xi_dpy, RootWindow(xi->xi_dpy,
			xi->xi_screen), xi->xi_visual, AllocAll);
		xi->xi_wp = 0xFFFFFF;
		xi->xi_bp = 0x000000;
		break;

	case FLG_VT16:
		/*
		 * We need this colormap, even though we're not really going to
		 * use it, because if we don't specify a colormap when we
		 * create the window (thus getting the default), and the
		 * default visual is not 24-bit, the window create will fail.
		 */

		xi->xi_cmap = XCreateColormap(xi->xi_dpy, RootWindow(xi->xi_dpy,
			xi->xi_screen), xi->xi_visual, AllocNone);
		xi->xi_wp = 0xFFFFFF;
		xi->xi_bp = 0x000000;
		break;

	case FLG_VP8:
	{
		xi->xi_cmap = XCreateColormap(xi->xi_dpy, RootWindow(xi->xi_dpy,
			xi->xi_screen), xi->xi_visual, AllocNone);

		xi->xi_ncolors = sizeof (reds) * sizeof (blus) * sizeof (grns);
		xi->xi_base = 255 - xi->xi_ncolors;

		/* Create color cube */
		X24_createColorCube(xi);

		/* Create fast lookup tables for dithering */
		X24_createColorTables(xi);

		/* Do white/black pixels */
		xi->xi_bp = xi->xi_base;
		xi->xi_wp = xi->xi_base + xi->xi_ncolors - 1;

		break;
	}

	case FLG_VS8:
		/*
		 * We need this colormap, even though we're not really going to
		 * use it, because if we don't specify a colormap when we
		 * create the window (thus getting the default), and the
		 * default visual is not 8-bit, the window create will fail.
		 */

		xi->xi_cmap = XCreateColormap(xi->xi_dpy, RootWindow(xi->xi_dpy,
			xi->xi_screen), xi->xi_visual, AllocNone);
		xi->xi_wp = 0xFF;
		xi->xi_bp = 0x00;
		break;

	case FLG_VG8:
	{
		/*
		 * We're being a little lazy here by just taking over the
		 * entire colormap and writing a linear ramp to it.  If we
		 * didn't take the whole thing we might be able to avoid a
		 * little colormap flashing, but then we'd need separate
		 * display code for GrayScale and StaticGray and I'm just not
		 * sure it's worth it.
		 */

		int	i;
		XColor	colors[256];

		xi->xi_cmap = XCreateColormap(xi->xi_dpy, RootWindow(xi->xi_dpy,
			xi->xi_screen), xi->xi_visual, AllocAll);

		/* Fill the colormap and the colorcube */

		for (i = 0; i < 255; i++) {
			colors[i].red = i << 8;
			colors[i].green = i << 8;
			colors[i].blue = i << 8;
			colors[i].flags = DoRed | DoGreen | DoBlue;
			colors[i].pixel = i;
		}

		XStoreColors(xi->xi_dpy, xi->xi_cmap, colors, 256);

		/* Do white/black pixels */

		xi->xi_bp = 0x00;
		xi->xi_wp = 0xFF;
		break;
	}
	case FLG_VS1:
	{
		int i, j, x, didx;

		/*
		 * We need this colormap, even though we're not really going to
		 * use it, because if we don't specify a colormap when we
		 * create the window (thus getting the default), and the
		 * default visual is not 1-bit, the window create will fail.
		 */

		xi->xi_cmap = XCreateColormap(xi->xi_dpy, RootWindow(xi->xi_dpy,
			xi->xi_screen), xi->xi_visual, AllocNone);

		/* Create fast lookup tables for dithering */

		xi->xi_andtbl = (unsigned char *)malloc(64 * 256);
		xi->xi_ortbl = (unsigned char *)malloc(64 * 256);

		for (i = 0; i < 256; i++)
			for (j = 0; j < 64; j++) {
				didx = j;
				x = 7 - (j & 0x7);

				if (i > (256.0 * dmsk881[didx])) {
					xi->xi_andtbl[(i << 6) + j] = 0xFF;
					xi->xi_ortbl[(i << 6) + j] = 1 << x;
				}
				else {
					xi->xi_andtbl[(i << 6) + j] = ~(1 << x);
					xi->xi_ortbl[(i << 6) + j] = 0;
				}
			}

		xi->xi_wp = 0x0;
		xi->xi_bp = 0x1;
		break;
	}
	}

	/* Create fake colormaps if the X server won't do it for us */

	if (!(xi->xi_flags & FLG_XCMAP)) {
		xi->xi_redmap = (unsigned char *)malloc(256);
		xi->xi_grnmap = (unsigned char *)malloc(256);
		xi->xi_blumap = (unsigned char *)malloc(256);

		if (!xi->xi_redmap || !xi->xi_grnmap || !xi->xi_blumap) {
			fb_log("if_X24: Can't allocate colormap memory\n");
			return (-1);
		}
	}

	/*
	 * Fill in XSetWindowAttributes struct for XCreateWindow.
	 */
	xswa.event_mask = ExposureMask | ButtonPressMask | StructureNotifyMask;
	xswa.background_pixel = xi->xi_bp;
	xswa.border_pixel = xi->xi_wp;
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
		xi->xi_screen), 0, 0, width, height, 3, xi->xi_depth,
		InputOutput, xi->xi_visual, CWEventMask | CWBackPixel |
		    CWBorderPixel | CWBitGravity | CWBackingStore | CWColormap,
		&xswa);
	xi->xi_cwinp = xi->xi_win;

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

	gcv.foreground = xi->xi_wp;
	gcv.background = xi->xi_bp;
	xi->xi_gc = XCreateGC(xi->xi_dpy, xi->xi_win,
		GCForeground | GCBackground, &gcv);

	/* Create a Graphics Context for clipping */

	gcv.foreground = xi->xi_bp;
	gcv.background = xi->xi_bp;
	xi->xi_cgc = XCreateGC(xi->xi_dpy, xi->xi_win,
		GCForeground | GCBackground, &gcv);

	/* Initialize the valid region */
	xi->xi_usereg = 1;
	xi->xi_reg = XCreateRegion();
	rect.x = 0;
	rect.y = 0;
	rect.width = xi->xi_xwidth;
	rect.height = xi->xi_xheight;
	XUnionRectWithRegion(&rect, xi->xi_reg, xi->xi_reg);

	/* Map window to screen */

	XMapWindow(xi->xi_dpy, xi->xi_win);
	XFlush(xi->xi_dpy);

	/* Allocate image buffer, and make our X11 Image */

	switch (xi->xi_flags & FLG_VMASK)
	{
	case FLG_VD24:
	case FLG_VT24:
	  if ((xi->xi_pix = (unsigned char *) calloc(sizeof(unsigned int),
						     width*height)) == NULL) {
			fb_log("X24_open: pix32 malloc failed\n");
			return(-1);
		}

		xi->xi_image = XCreateImage(xi->xi_dpy,
			xi->xi_visual, xi->xi_depth, ZPixmap, 0,
			(char *) xi->xi_pix, width, height,
			 sizeof(unsigned int) * 8, 0);
		break;

	case FLG_VD16:
	case FLG_VT16:
	  if ((xi->xi_pix = (unsigned char *) calloc(2, width*height)) == NULL) {
			fb_log("X24_open: pix32 malloc failed\n");
			return(-1);
		}

		xi->xi_image = XCreateImage(xi->xi_dpy,
			xi->xi_visual, xi->xi_depth, ZPixmap, 0,
			(char *) xi->xi_pix, width, height,
			16, 0);
		break;

	case FLG_VP8:
	case FLG_VS8:
	case FLG_VG8:
		if ((xi->xi_pix = (unsigned char *) calloc(sizeof(char),
							   width*height)) == NULL) {
			fb_log("X24_open: pix8 malloc failed\n");
			return(-1);
		}
		memset(xi->xi_pix, xi->xi_bp, width*height);

		xi->xi_image = XCreateImage(xi->xi_dpy,
			xi->xi_visual, xi->xi_depth, ZPixmap, 0,
			(char *) xi->xi_pix, width, height, 8, 0);
		break;

	case FLG_VS1:
		xi->xi_image = XCreateImage(xi->xi_dpy,
			xi->xi_visual, xi->xi_depth, XYBitmap, 0,
			NULL, width, height, 32, 0);

		if ((xi->xi_pix = (unsigned char *) calloc(sizeof(char),
							   xi->xi_image->bytes_per_line * height)) == NULL) {
			fb_log("X24_open: pix1 malloc failed\n");
			return(-1);
		}
		xi->xi_image->data = (char *) xi->xi_pix;
		xi->xi_image->byte_order = MSBFirst;
		xi->xi_image->bitmap_bit_order = MSBFirst;

		memset(xi->xi_pix, 0, xi->xi_image->bytes_per_line *
		    height);
		break;
	}

	/* Calculate luminance tables if we need them */

	switch (xi->xi_flags & FLG_VMASK)
	{
	case FLG_VG8:
	case FLG_VS1:
		if (!lumdone) {
			int i;
			for (i = 0; i < 256; i++) {
				rlumtbl[i] = i * 5016388;
				glumtbl[i] = i * 9848226;
				blumtbl[i] = i * 1912603;
			}
			lumdone = 1;
		}
	}

	return (0);
}

static int alive = 1;

HIDDEN int
x24_linger(ifp)
FBIO	*ifp;
{
	struct xinfo *xi = XI(ifp);
	XEvent	event;

	if (fork() != 0)
		return (1);	/* release the parent */

	while(alive) {
		XNextEvent(xi->xi_dpy, &event);
		X24_handle_event(ifp, &event);
	}
	return (0);
}


HIDDEN void
X24_handle_event(ifp, event)
FBIO *ifp;
XEvent *event;
{
  struct xinfo *xi = XI(ifp);

  switch((int)event->type){
  case Expose:
    {
      XExposeEvent *expose = (XExposeEvent *)event;
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
  case ButtonPress:
    {
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

      switch(button){
      case Button1:
	break;
      case Button2:
	{
	  int	x, sy;
	  int	ix, isy;
	  unsigned char	*cp;

	  x = event->xbutton.x;
	  sy = xi->xi_xheight - event->xbutton.y - 1;

	  x -= xi->xi_xlf;
	  sy -= xi->xi_xheight - xi->xi_xbt - 1;
	  if(x < 0 || sy < 0){
	    fb_log("No RGB (outside image) 1\n");
	    break;
	  }

	  if (x < xi->xi_ilf_w)
	    ix = xi->xi_ilf;
	  else
	    ix = xi->xi_ilf + (x - xi->xi_ilf_w + ifp->if_xzoom - 1) / ifp->if_xzoom;

	  if (sy < xi->xi_ibt_h)
	    isy = xi->xi_ibt;
	  else
	    isy = xi->xi_ibt + (sy - xi->xi_ibt_h + ifp->if_yzoom - 1) / ifp->if_yzoom;

	  if(ix >= xi->xi_iwidth || isy >= xi->xi_iheight){
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
  case ConfigureNotify:
    {
      XConfigureEvent *conf = (XConfigureEvent *)event;

      if(conf->width == xi->xi_xwidth &&
	 conf->height == xi->xi_xheight)
	return;

#if EVENT_DBG
      printf("configure, oldht %d oldwid %d newht %d newwid %d\n",
	     xi->xi_xheight, xi->xi_xwidth, conf->height,
	     conf->width);
#endif

      X24_configureWindow(ifp, conf->width, conf->height);

      /*
       * Blit backing store to image buffer (we'll blit to screen
       * when we get the expose event)
       */
      X24_blit(ifp, 0, 0, xi->xi_iwidth, xi->xi_iheight, BLIT_RESIZE);
      break;
    }
  default:
    break;
  }

  return;
}

void
X24_configureWindow(ifp, width, height)
FBIO *ifp;
int width, height;
{
  struct xinfo *xi = XI(ifp);
  XRectangle rect;
  
  if (!xi) {
      return;
  }

  if(width == xi->xi_xwidth && height == xi->xi_xheight) {
    return;
  }

  ifp->if_width = ifp->if_max_width = width;
  ifp->if_height = ifp->if_max_height = height;

  xi->xi_xwidth = xi->xi_iwidth = width;
  xi->xi_xheight = xi->xi_iheight = height;

  ifp->if_xcenter = width/2;
  ifp->if_ycenter = height/2;

  /* redo region */
  if (xi->xi_usereg) {
	  XDestroyRegion(xi->xi_reg);
	  xi->xi_reg = XCreateRegion();
	  rect.x = 0;
	  rect.y = 0;
	  rect.width = xi->xi_xwidth;
	  rect.height = xi->xi_xheight;
	  XUnionRectWithRegion(&rect, xi->xi_reg, xi->xi_reg);
  }

  X24_updstate(ifp);

  switch(xi->xi_flags & FLG_VMASK){
  case FLG_VD24:
  case FLG_VT24:
    /* Destroy old image struct and image buffer */
    XDestroyImage(xi->xi_image);

    /* Make new buffer and new image */
    if ((xi->xi_pix = (unsigned char *)calloc(sizeof(unsigned int),
					      xi->xi_xwidth*xi->xi_xheight)) == NULL) {
      fb_log("X24: pix32 malloc failed in resize!\n");
      return;
    }

    xi->xi_image = XCreateImage(xi->xi_dpy, xi->xi_visual,
				xi->xi_depth, ZPixmap, 0, (char *) xi->xi_pix,
				xi->xi_xwidth, xi->xi_xheight,
				sizeof (unsigned int) * 8, 0);

    break;
  case FLG_VD16:
  case FLG_VT16:
    /* Destroy old image struct and image buffer */
    XDestroyImage(xi->xi_image);

    /* Make new buffer and new image */
    if ((xi->xi_pix = (unsigned char *)calloc( 2, xi->xi_xwidth*xi->xi_xheight)) == NULL) {
      fb_log("X24: pix32 malloc failed in resize!\n");
      return;
    }

    xi->xi_image = XCreateImage(xi->xi_dpy, xi->xi_visual,
				xi->xi_depth, ZPixmap, 0, (char *) xi->xi_pix,
				xi->xi_xwidth, xi->xi_xheight,
				16, 0);

    break;
  case FLG_VP8:
  case FLG_VS8:
  case FLG_VG8:
    /* Destroy old image struct and image buffers */
    XDestroyImage(xi->xi_image);

    /* Make new buffers and new image */
    if ((xi->xi_pix = (unsigned char *)calloc(sizeof(char),
					      xi->xi_xwidth * xi->xi_xheight)) == NULL) {
      fb_log("X24: pix8 malloc failed in resize!\n");
      return;
    }

    xi->xi_image = XCreateImage(xi->xi_dpy, xi->xi_visual,
				xi->xi_depth, ZPixmap, 0, (char *) xi->xi_pix,
				xi->xi_xwidth, xi->xi_xheight, 8, 0);
    break;
  case FLG_VS1:
    /* Destroy old image struct and image buffers */
    XDestroyImage(xi->xi_image);

    /* Make new buffers and new image */
    xi->xi_image = XCreateImage(xi->xi_dpy,
				xi->xi_visual, xi->xi_depth, XYBitmap, 0,
				NULL, xi->xi_xwidth, xi->xi_xheight, 32, 0);

    if ((xi->xi_pix = (unsigned char *)calloc(sizeof(char),
					      xi->xi_image->bytes_per_line * xi->xi_xheight)) == NULL) {
      fb_log("X24: pix1 malloc failed in resize!\n");
      return;
    }

    xi->xi_image->data = (char *) xi->xi_pix;
    xi->xi_image->byte_order = MSBFirst;
    xi->xi_image->bitmap_bit_order = MSBFirst;

    break;
  }

}

/*
 *  A given Display (i.e. Server) can have any number of Screens.
 *  Each Screen can support one or more Visual types.
 *  unix:0.1.2 => host:display.screen.visual
 *  Typically the screen and visual default to 0 by being omitted.
 */
HIDDEN void
print_display_info(dpy)
Display *dpy;
{
	int	i;
	int	screen;
	Visual	*visual;
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
	printf("BlackPixel = %lu\n", BlackPixel(dpy,screen));
	printf("WhitePixel = %lu\n", WhitePixel(dpy,screen));
	printf("Save Unders: %s\n",
		DoesSaveUnders(ScreenOfDisplay(dpy,screen)) ? "True" : "False");
	i = DoesBackingStore(ScreenOfDisplay(dpy,screen));
	printf("Backing Store: %s\n", i == WhenMapped ? "WhenMapped" :
		(i == Always ? "Always" : "NotUseful"));
	printf("Installed Colormaps: min %d, max %d\n",
		MinCmapsOfScreen(ScreenOfDisplay(dpy,screen)),
		MaxCmapsOfScreen(ScreenOfDisplay(dpy,screen)));
	printf("DefaultColormap: 0x%lx\n", DefaultColormap(dpy,screen));


	for (i = 0; i < num; i++) {

		visual = vp[i].visual;

		printf("---- Visual 0x%lx (%d)----\n", (unsigned long int)visual, i);

		printf("screen: %d\n", vp[i].screen);
		printf("depth : %d\n", vp[i].depth);

		switch(visual->class) {
		case DirectColor:
			printf("DirectColor: Alterable RGB maps, pixel RGB subfield indicies\n");
			printf("RGB Masks: 0x%lx 0x%lx 0x%lx\n", visual->red_mask,
			       visual->green_mask, visual->blue_mask);
			break;
		case TrueColor:
			printf("TrueColor: Fixed RGB maps, pixel RGB subfield indicies\n");
			printf("RGB Masks: 0x%lx 0x%lx 0x%lx\n", visual->red_mask,
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
		printf("XA_RGB_BEST_MAP    - Yes (0x%lx)\n", cmap.colormap);
		printf("R[0..%lu] * %lu + G[0..%lu] * %lu  + B[0..%lu] * %lu + %lu\n",
			cmap.red_max, cmap.red_mult, cmap.green_max, cmap.green_mult,
			cmap.blue_max, cmap.blue_mult, cmap.base_pixel);
	} else
		printf("XA_RGB_BEST_MAP    - No\n");
	if (XGetStandardColormap(dpy, win, &cmap, XA_RGB_DEFAULT_MAP)) {
		printf("XA_RGB_DEFAULT_MAP - Yes (0x%lx)\n", cmap.colormap);
		printf("R[0..%lu] * %lu + G[0..%lu] * %lu  + B[0..%lu] * %lu + %lu\n",
			cmap.red_max, cmap.red_mult, cmap.green_max, cmap.green_mult,
			cmap.blue_max, cmap.blue_mult, cmap.base_pixel);
	} else
		printf("XA_RGB_DEFAULT_MAP - No\n");
	if (XGetStandardColormap(dpy, win, &cmap, XA_RGB_GRAY_MAP)) {
		printf("XA_RGB_GRAY_MAP    - Yes (0x%lx)\n", cmap.colormap);
		printf("R[0..%lu] * %lu + %lu\n",
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
HIDDEN int
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

#ifdef HAVE_SYS_MMAN_H
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
#endif
#ifdef HAVE_SYS_SHM_H
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
			if ((mem = (char *)shmat(xi->xi_shmid, 0, 0)) != (char *)-1L)
				break;
			else
				fb_log("X24_getmem: can't shmat shared memory, \
using private memory instead, errno %d\n", errno);
		}

		/* Change it to local */
		xi->xi_mode = (xi->xi_mode & ~MODE10_MASK) | MODE10_MALLOC;
#endif

#ifndef CAN_LINGER
		/* Change it to local */
		xi->xi_mode = (xi->xi_mode & ~MODE10_MASK) | MODE10_MALLOC;
#endif

		/*FALLTHROUGH*/

	}
	case MODE10_MALLOC:

		if ((mem = (char *)malloc(size)) == 0) {
			fb_log("if_X24: Unable to allocate %d bytes of backing \
store\n  Run shell command 'limit datasize unlmited' and try again.\n", size);
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
HIDDEN void
X24_zapmem()
{
#ifndef HAVE_SYS_MMAN_H
 	int	shmid;
	int	i;
#endif

#ifdef HAVE_SYS_MMAN_H
	unlink(BS_NAME);
#endif
#ifdef HAVE_SYS_SHM_H
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
#ifdef CAN_LINGER
	fb_log("if_X24: shared memory released\n");
#endif
	return;
}

HIDDEN void
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

/*	X 2 4 _ b l i t
 *
 * This routine is called when ever the framebuffer is updated OR when
 * there is an expose event generated by the X server.
 *
 * The X server world is confusing because XDR is NOT done for client
 * or server, thus leaving each client responsable for getting a
 * X pixel map prepared in the correct lay out.
 *
 * There are something like 18 different visuals and 2 endians that
 * we need to deal with 1-bit, 2-bit, 4-bit and 8-bit monochrome
 * 8-bit CLUT, 8-bit True Color, 8-bit Direct Color
 * 15-bit Pseudo Color, 15-bit True Color, 15-bit Direct color
 * 16-bit Pseudo Color, 16-bit True Color, 16-bit Direct Color
 * 24-bit Pseudo, True and Direct
 * and finally 32-bit Pseudo, True and Direct colors.
 *
 * For the 1-8 bit case we do some dithering and get an image up as best
 * we can.
 * for the >8 bit cases we need to do a bit more.
 *
 * Our input is always in RGB (24 bit) order in memory so there is nothing
 * fancy we need to do there.
 *
 * For output, the X server tells us, in an opaque object that we are
 * not suppose to peek into, where the RGB components go in a BIG endian
 * bit vector and how many bits per component.  Using this information we
 * construct masks and shift counts for each component.  This information
 * is later used to construct a bit vector in a register.  This register
 * is then clocked out as bytes in the correct ordering.
 */

HIDDEN void
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

	/*
	 * Newish code, discover masks and shifts for each of RGB
	 *
	 * The Masks are right justified, we just shift them up 6 bits
	 * in the long to give us some room on the low end.  We'll correct
	 * this out later.
	 */
	unsigned int a_pixel;

	unsigned long test_mask;
	unsigned long a_mask;

	int red_shift, green_shift, blue_shift;
	unsigned int mask_red = xi->xi_image->red_mask << 6;
	unsigned int mask_green = xi->xi_image->green_mask << 6;
	unsigned int mask_blue = xi->xi_image->blue_mask << 6;
	int i;

	/*
	 * Now that we know the mask, we shift a bit left, one bit at a time
	 * until it overlaps the mask.  This tells us how far we have to
	 * shift our pixel to get it under the bit mask.
	 */
	a_mask = mask_red;
	test_mask = 1;
	for (i=0; i<sizeof(unsigned long)*8;i++) {
		if (test_mask & a_mask) break;
		test_mask = test_mask << 1;
	}
	for (;i<sizeof(unsigned long)*8;i++) {
		if (!(test_mask & a_mask)) break;
		test_mask = test_mask << 1;
	}
	red_shift = i-8;

	a_mask = mask_green;
	test_mask = 1;
	for (i=0; i<sizeof(unsigned long)*8;i++) {
		if (test_mask & a_mask) break;
		test_mask = test_mask << 1;
	}
	for (;i<sizeof(unsigned long)*8;i++) {
		if (!(test_mask & a_mask)) break;
		test_mask = test_mask << 1;
	}
	green_shift = i-8;

	a_mask = mask_blue;
	test_mask = 1;
	for (i=0; i<sizeof(unsigned long)*8;i++) {
		if (test_mask & a_mask) break;
		test_mask = test_mask << 1;
	}
	for (;i<sizeof(unsigned long)*8;i++) {
		if (!(test_mask & a_mask)) break;
		test_mask =test_mask << 1;
	}
	blue_shift = i-8;

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
		ox = x1wd + ((xdel - 1) * ifp->if_xzoom) + xi->xi_xlf;
	else
		ox = xi->xi_xlf;


	/* Compute oy: offset from top edge of window to bottom pixel */

	ydel = y1 - xi->xi_ibt;
	if (ydel)
		oy = xi->xi_xbt - (y1ht + ((ydel - 1) * ifp->if_yzoom));
	else
		oy = xi->xi_xbt;


	/* Figure out size of changed area on screen in X pixels */

	if (x2 == x1)
		xwd = x1wd;
	else
		xwd = x1wd + x2wd + ifp->if_xzoom * (x2 - x1 - 1);

	if (y2 == y1)
		xht = y1ht;
	else
		xht = y1ht + y2ht + ifp->if_yzoom * (y2 - y1 - 1);

#if BLIT_DBG
printf("blit: output to (%d, %d)\n", ox, oy);
printf("blit: xi_flags & FLG_VMASK = 0x%x\n", xi->xi_flags & FLG_VMASK );
#endif

	/*
	 * Set pointers to start of source and destination areas; note that
	 * we're going from lower to higher image coordinates, so irgb
	 * increases, but since images are in quadrant I and X uses quadrant
	 * IV, opix _decreases_.
	 */

	switch (xi->xi_flags & FLG_VMASK)
	{
	case FLG_VD24:
	case FLG_VT24:
	case FLG_VD16:
	case FLG_VT16:
	{
		unsigned char *irgb;
		unsigned char *opix;
		unsigned char *red = xi->xi_redmap;
		unsigned char *grn = xi->xi_grnmap;
		unsigned char *blu = xi->xi_blumap;

		/*
		 * Calculate the beginning of the line where we are going
		 * to be outputing pixels.
		 */
		opix = &(xi->xi_pix[oy * xi->xi_image->bytes_per_line + ox *
			(xi->xi_image->bits_per_pixel/8)]);

#if BLIT_DBG_PIX
		if (opix < xi->xi_pix) {
			bu_log("X24_blit: about to clobber memory 1\n");
			bu_log("\topix - Ox%lx\txi->xi_pix - 0x%lx\n", opix, xi->xi_pix);
			break;
		}
#endif
		/*
		 * Our source of pixels in packed RGB order
		 */
		irgb = &(xi->xi_mem[(y1 * xi->xi_iwidth + x1) * sizeof
			(RGBpixel)]);

		/* General case, zooming in effect */

		for (y = y1; y <= y2; y++) {
			unsigned char *line_irgb;
			unsigned char *p;
			unsigned char *holdit;
			int pyht;

#if BLIT_DBG_PIX
			if (opix < xi->xi_pix) {
				bu_log("X24_blit: about to clobber memory 2\n");
				bu_log("\topix - Ox%lx\txi->xi_pix - 0x%lx\n", opix, xi->xi_pix);
				break;
			}
#endif

			/* Calculate the number of lines needed */
			/*
			 * If we are zoomed, then it is possable that not
			 * all of that zoomed pixel will be showned.
			 * y1ht is the number of lines allocated for the
			 * bottom most line.  y2ht is the number of lines
			 * for the top most line.  if_yzoom is for everything
			 * else.
			 */
			if (y == y1) {
				pyht = y1ht;
			} else if (y==y2) {
				pyht = y2ht;
			} else {
				pyht = ifp->if_yzoom;
			}

			/* Save pointer to start of line */

			line_irgb = irgb;
			p = (unsigned char *)opix;

			/* For the first line, convert/copy pixels */

			for (x = x1; x <= x2; x++) {
				int pxwd;

				/* Calculate # pixels needed */
				/* See comment above for more info */

				if (x == x1)
					pxwd = x1wd;
				else if (x == x2)
					pxwd = x2wd;
				else
					pxwd = ifp->if_xzoom;

				/*
				 * Construct a pixel with the color components
				 * in the right places as described by the
				 * X servers red, green and blue masks.
				 */
				if (xi->xi_flags & (FLG_XCMAP | FLG_LINCMAP))  {
					a_pixel  = (line_irgb[RED] << red_shift) & mask_red;
					a_pixel |= (line_irgb[GRN] << green_shift) & mask_green;
					a_pixel |= (line_irgb[BLU] << blue_shift) & mask_blue;
				} else {
					a_pixel  = (red[line_irgb[RED]] << red_shift) & mask_red;
					a_pixel |= (grn[line_irgb[GRN]] << green_shift) & mask_green;
					a_pixel |= (blu[line_irgb[BLU]] << blue_shift) & mask_blue;
				}
				/* take out the safety put in above. */
				a_pixel = a_pixel >> 6;

				/*
				 * Now we clock out that pixel according to
				 * the number of bytes AND the byte ordering
				 *
				 * A slightly faster version would unroll
				 * these loops into a number of different
				 * loops.
				 *
				 * The while loop on the inside causes pixel
				 * replication for when we are zoomed.
				 */
				if( ImageByteOrder(xi->xi_dpy) == MSBFirst )  {
					if (xi->xi_image->bits_per_pixel == 16) {
						while (pxwd--)   {
							*p++ = (a_pixel >> 8) & 0xff;
							*p++ = a_pixel & 0xff;
						}
					} else if (xi->xi_image->bits_per_pixel == 24) {
						while (pxwd--)   {
							*p++ = (a_pixel >> 16) & 0xff;
							*p++ = (a_pixel >> 8) & 0xff;
							*p++ = a_pixel & 0xff;
						}
					} else if (xi->xi_image->bits_per_pixel == 32) {
						while (pxwd--) {
							*p++ = (a_pixel >> 24) & 0xff;
							*p++ = (a_pixel >> 16) & 0xff;
							*p++ = (a_pixel >> 8) & 0xff;
							*p++ = a_pixel & 0xff;
						}
					}

				} else { /* LSB order */
					if (xi->xi_image->bits_per_pixel == 16) {
						while (pxwd--)   {
							*p++ = a_pixel & 0xff;
							*p++ = (a_pixel >> 8) & 0xff;
						}
					} else if (xi->xi_image->bits_per_pixel == 24) {
						while (pxwd--)   {
							*p++ = a_pixel & 0xff;
							*p++ = (a_pixel >> 8) & 0xff;
							*p++ = (a_pixel >> 16) & 0xff;
						}
					} else if (xi->xi_image->bits_per_pixel == 32) {
						while (pxwd--) {
							*p++ = a_pixel & 0xff;
							*p++ = (a_pixel >> 8) & 0xff;
							*p++ = (a_pixel >> 16) & 0xff;
							*p++ = (a_pixel >> 24) & 0xff;
						}
					}

				}
				/*
				 * Move to the next input line.
				 */
				line_irgb += sizeof (RGBpixel);
			}
			/*
			 * Remember where we put all those bytes.
			 */
			holdit = (unsigned char *)opix;

			/*
			 * If we are zoomed, we need to duplicate all
			 * that work from above, or we can just copy
			 * that one line to N others.
			 */
			while (pyht-- > 1) {
				unsigned char *src;

				/*
				 * move to the beginning of the next
				 * line up in the X server.
				 */
				opix -= xi->xi_image->bytes_per_line;
				p = (unsigned char *)opix;

#if BLIT_DBG_PIX
				if (opix < xi->xi_pix) {
					bu_log("X24_blit: about to clobber memory 3\n");
					bu_log("\topix - Ox%lx\txi->xi_pix - 0x%lx\n", opix, xi->xi_pix);
					break;
				}
#endif
				/*
				 * Go to the place where we stored pixels
				 * on the first line output.
				 */
				src = (unsigned char *)holdit;
				/*
				 * and copy that line to where we are now.
				 */
				memcpy(p, src, xi->xi_image->bytes_per_line);
			}
			/*
			 * And again, move to the beginning of the next
			 * line up in the X server.
			 */
			opix -= xi->xi_image->bytes_per_line;

			irgb += xi->xi_iwidth * sizeof(RGBpixel);
		}
		break;
	}

	case FLG_VP8:
	{
		int dmx = ox & 0x7;
		int dmy = (oy & 0x7) << 3;

		unsigned int r, g, b;
		unsigned char *red = xi->xi_redmap;
		unsigned char *grn = xi->xi_grnmap;
		unsigned char *blu = xi->xi_blumap;

		unsigned char *ip = &(xi->xi_mem[(y1 * xi->xi_iwidth + x1) *
			sizeof (RGBpixel)]);
		unsigned char *op = (unsigned char *) &xi->xi_pix[oy *
			xi->xi_xwidth + ox];


		if (ifp->if_xzoom == 1 && ifp->if_yzoom == 1) {
			/* Special case if no zooming */

			int j, k;

			for (j = y2 - y1 + 1; j; j--) {
				unsigned char *lip;
				unsigned char *lop;

				lip = ip;
				lop = op;

				/* For each line, convert/copy pixels */

				if (xi->xi_flags & (FLG_XCMAP | FLG_LINCMAP))
					for (k = x2 - x1 + 1; k; k--) {
						r = lip[RED];
						g = lip[GRN];
						b = lip[BLU];

						*lop++ = xi->xi_base +
							xi->xi_ccredtbl[(r << 6) + dmx + dmy] +
							xi->xi_ccgrntbl[(g << 6) + dmx + dmy] +
							xi->xi_ccblutbl[(b << 6) + dmx + dmy];

						dmx = (dmx + 1) & 0x7;
						lip += sizeof (RGBpixel);
					}
				else
					for (k = x2 - x1 + 1; k; k--) {
						r = red[lip[RED]];
						g = grn[lip[GRN]];
						b = blu[lip[BLU]];

						*lop++ = xi->xi_base +
							xi->xi_ccredtbl[(r << 6) + dmx + dmy] +
							xi->xi_ccgrntbl[(g << 6) + dmx + dmy] +
							xi->xi_ccblutbl[(b << 6) + dmx + dmy];

						dmx = (dmx + 1) & 0x7;
						lip += sizeof (RGBpixel);
					}

				ip += xi->xi_iwidth * sizeof (RGBpixel);
				op -= xi->xi_image->bytes_per_line;
				dmx = ox & 0x7;
				dmy = (dmy + 0x38) & 0x38;
			}
		} else {
			/* General case */

			for (y = y1; y <= y2; y++) {
				int pyht;
				unsigned char *lip;
				unsigned char *lop;

				/* Calculate # lines needed */

				if (y == y1)
					pyht = y1ht;
				else if (y == y2)
					pyht = y2ht;
				else
					pyht = ifp->if_yzoom;

				/* For each line, convert/copy pixels */

				while (pyht--)
				{
					lip = ip;
					lop = op;

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

							r = lip[RED];
							g = lip[GRN];
							b = lip[BLU];

							while (pxwd--)
							{
								*lop++ = xi->xi_base +
									xi->xi_ccredtbl[(r << 6) + dmx + dmy] +
									xi->xi_ccgrntbl[(g << 6) + dmx + dmy] +
									xi->xi_ccblutbl[(b << 6) + dmx + dmy];

								dmx = (dmx + 1) & 0x7;
							}

							lip += sizeof (RGBpixel);
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

							r = red[lip[RED]];
							g = grn[lip[GRN]];
							b = blu[lip[BLU]];

							while (pxwd--)
							{
								*lop++ = xi->xi_base +
									xi->xi_ccredtbl[(r << 6) + dmx + dmy] +
									xi->xi_ccgrntbl[(g << 6) + dmx + dmy] +
									xi->xi_ccblutbl[(b << 6) + dmx + dmy];

								dmx = (dmx + 1) & 0x7;
							}

							lip += sizeof (RGBpixel);
						}

					op -= xi->xi_image->bytes_per_line;
					dmx = ox & 0x7;
					dmy = (dmy + 0x38) & 0x38;
				}
				ip += xi->xi_iwidth * sizeof (RGBpixel);
			}
		}
		break;
	}

	case FLG_VS8:
	case FLG_VG8:
	{
		unsigned int r, g, b;
		unsigned char *red = xi->xi_redmap;
		unsigned char *grn = xi->xi_grnmap;
		unsigned char *blu = xi->xi_blumap;

		unsigned char *ip = &(xi->xi_mem[(y1 * xi->xi_iwidth + x1) *
			sizeof (RGBpixel)]);
		unsigned char *op = (unsigned char *) &xi->xi_pix[oy *
			xi->xi_xwidth + ox];

		if (ifp->if_xzoom == 1 && ifp->if_yzoom == 1) {
			/* Special case if no zooming */

			int j, k;

			for (j = y2 - y1 + 1; j; j--) {
				unsigned char *lip;
				unsigned char *lop;

				lip = ip;
				lop = op;

				/* For each line, convert/copy pixels */

				if (xi->xi_flags & (FLG_XCMAP | FLG_LINCMAP))
					for (k = x2 - x1 + 1; k; k--) {
						r = lip[RED];
						g = lip[GRN];
						b = lip[BLU];

						*lop++ = (rlumtbl[r] +
							glumtbl[g] +
							blumtbl[b] +
							8388608) >> 24;
						lip += sizeof (RGBpixel);
					}
				else
					for (k = x2 - x1 + 1; k; k--) {
						r = red[lip[RED]];
						g = grn[lip[GRN]];
						b = blu[lip[BLU]];

						*lop++ = (rlumtbl[r] +
							glumtbl[g] +
							blumtbl[b] +
							8388608) >> 24;
						lip += sizeof (RGBpixel);
					}

				ip += xi->xi_iwidth * sizeof (RGBpixel);
				op -= xi->xi_xwidth;
			}
		} else {
			/* General case */

			for (y = y1; y <= y2; y++) {
				int pyht;
				int copied;
				unsigned char *lip;
				unsigned char pix, *lop, *prev_line;

				/* Calculate # lines needed */

				if (y == y1)
					pyht = y1ht;
				else if (y == y2)
					pyht = y2ht;
				else
					pyht = ifp->if_yzoom;


				/* Save pointer to start of line */

				lip = ip;
				prev_line = lop = op;

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

						r = lip[RED];
						g = lip[GRN];
						b = lip[BLU];

						pix = (rlumtbl[r] +
							glumtbl[g] +
							blumtbl[b] +
							8388608) >> 24;

						lip += sizeof (RGBpixel);

						/* Make as many copies as needed */

						while (pxwd--)
							*lop++ = pix;
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

						r = red[lip[RED]];
						g = grn[lip[GRN]];
						b = blu[lip[BLU]];

						pix = (rlumtbl[r] +
							glumtbl[g] +
							blumtbl[b] +
							8388608) >> 24;

						lip += sizeof (RGBpixel);

						/* Make as many copies as needed */

						while (pxwd--)
							*lop++ = pix;
					}

				copied = lop - op;

				ip += xi->xi_iwidth * sizeof (RGBpixel);
				op -= xi->xi_xwidth;

				/* Copy remaining output lines from 1st output line */

				pyht--;
				while (pyht--) {
					memcpy(op, prev_line, copied);
					op -= xi->xi_xwidth;
				}
			}
		}
		break;
	}

	case FLG_VS1:
	{
		int dmx = ox & 0x7;
		int dmy = (oy & 0x7) << 3;

		unsigned int r, g, b;
		unsigned char *red = xi->xi_redmap;
		unsigned char *grn = xi->xi_grnmap;
		unsigned char *blu = xi->xi_blumap;

		unsigned char *ip = &(xi->xi_mem[(y1 * xi->xi_iwidth + x1) *
			sizeof (RGBpixel)]);
		unsigned char *op = (unsigned char *) &xi->xi_pix[oy *
			xi->xi_image->bytes_per_line + ox / 8];


		if (ifp->if_xzoom == 1 && ifp->if_yzoom == 1) {
			/* Special case if no zooming */

			int j, k;

			for (j = y2 - y1 + 1; j; j--) {
				unsigned char *lip;
				unsigned char *lop;
				unsigned char loppix;
				unsigned int lum;

				lip = ip;
				lop = op;
				loppix = *lop;

				/* For each line, convert/copy pixels */

				if (xi->xi_flags & (FLG_XCMAP | FLG_LINCMAP))
					for (k = x2 - x1 + 1; k; k--) {
						r = lip[RED];
						g = lip[GRN];
						b = lip[BLU];

						lum = (rlumtbl[r] +
							glumtbl[g] +
							blumtbl[b] +
							8388608) >> 24;

						loppix = (loppix &
							xi->xi_andtbl[(lum << 6)
								+ dmx + dmy]) |
							xi->xi_ortbl[(lum << 6)
								+ dmx + dmy];

						dmx = (dmx + 1) & 0x7;
						if (!dmx) {
							*lop = loppix;
							lop++;
						}

						lip += sizeof (RGBpixel);
					}
				else
					for (k = x2 - x1 + 1; k; k--) {
						r = lip[RED];
						g = lip[GRN];
						b = lip[BLU];

						lum = (rlumtbl[red[r]] +
							glumtbl[grn[g]] +
							blumtbl[blu[b]] +
							8388608) >> 24;

						loppix = (loppix &
							xi->xi_andtbl[(lum << 6)
								+ dmx + dmy]) |
							xi->xi_ortbl[(lum << 6)
								+ dmx + dmy];

						dmx = (dmx + 1) & 0x7;
						if (!dmx) {
							*lop = loppix;
							lop++;
						}

						lip += sizeof (RGBpixel);
					}

				if (dmx)
					*lop = loppix;
				ip += xi->xi_iwidth * sizeof (RGBpixel);
				op -= xi->xi_image->bytes_per_line;
				dmx = ox & 0x7;
				dmy = (dmy + 0x38) & 0x38;
			}
		} else {
			/* General case */

			for (y = y1; y <= y2; y++) {
				int pyht;
				unsigned char *lip;
				unsigned char *lop;
				unsigned char loppix;
				unsigned int lum;

				/* Calculate # lines needed */

				if (y == y1)
					pyht = y1ht;
				else if (y == y2)
					pyht = y2ht;
				else
					pyht = ifp->if_yzoom;

				/* For each line, convert/copy pixels */

				while (pyht--)
				{
					lip = ip;
					lop = op;
					loppix = *lop;

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

							r = lip[RED];
							g = lip[GRN];
							b = lip[BLU];

							lum = (rlumtbl[r] +
								glumtbl[g] +
								blumtbl[b] +
								8388608) >> 24;

							while (pxwd--)
							{
								loppix = (loppix &
								xi->xi_andtbl[(lum << 6)
									+ dmx + dmy]) |
								xi->xi_ortbl[(lum << 6)
									+ dmx + dmy];

								dmx = (dmx + 1) & 0x7;
								if (!dmx) {
									*lop = loppix;
									lop++;
								}
							}

							lip += sizeof (RGBpixel);
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

							r = lip[RED];
							g = lip[GRN];
							b = lip[BLU];

							lum = (rlumtbl[red[r]] +
								glumtbl[grn[g]] +
								blumtbl[blu[b]] +
								8388608) >> 24;

							while (pxwd--)
							{
								loppix = (loppix &
								xi->xi_andtbl[(lum << 6)
									+ dmx + dmy]) |
								xi->xi_ortbl[(lum << 6)
									+ dmx + dmy];

								dmx = (dmx + 1) & 0x7;
								if (!dmx) {
									*lop = loppix;
									lop++;
								}
							}

							lip += sizeof (RGBpixel);
						}

					if (dmx)
						*lop = loppix;
					op -= xi->xi_image->bytes_per_line;
					dmx = ox & 0x7;
					dmy = (dmy + 0x38) & 0x38;
				}
				ip += xi->xi_iwidth * sizeof (RGBpixel);
			}
		}
		break;
	}
	}

	/* Blit out changed image */

	if (flags & BLIT_DISP) {
		XPutImage(xi->xi_dpy, xi->xi_win, xi->xi_gc, xi->xi_image,
			ox, oy - xht + 1, ox, oy - xht + 1, xwd, xht);
#if BLIT_DBG
printf("blit: PutImage of %dx%d to (%d, %d)\n", xwd, xht, ox, oy - xht + 1);
#endif
	}

	/* If we changed the valid region, make a new one. */

	if (xi->xi_usereg && (flags & (BLIT_PZ | BLIT_RESIZE)))
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

int
X24_refresh(ifp, x, y, w, h)
FBIO *ifp;
int x, y, w, h;
{
  if(w < 0){
    w = -w;
    x -= w;
  }

  if(h < 0){
    h = -h;
    y -= h;
  }

  X24_blit(ifp, x, y, w, h, BLIT_DISP);

  return 0;
}

#endif /* IF_X */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

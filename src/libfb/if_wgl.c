/*                       I F _ W G L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @addtogroup if */
/** @{ */
/** @file if_wgl.c
 *
 * Frame Buffer Library interface for Windows OpenGL.
 *
 * There are several different Frame Buffer modes supported.  Set your
 * environment FB_FILE to the appropriate type.  Note that some of the
 * /dev/sgi modes are not supported, and there are some new
 * modes. (see the modeflag definitions below).
 *
 * /dev/wgl[options]
 *
 */
/** @} */

#include "common.h"

#ifdef IF_WGL

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <windowsx.h>

#include "bio.h"
#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif
#undef RED

#include "tk.h"
#include "tkPlatDecls.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "fb.h"


#define CJDEBUG 0

/* XXX - arbitrary upper bound */
#define XMAXSCREEN 16383
#define YMAXSCREEN 16383

/* Internal callbacks etc.*/
HIDDEN void wgl_do_event(FBIO *ifp);
HIDDEN void expose_callback(FBIO *ifp, int eventPtr);
void wgl_configureWindow(FBIO *ifp, int width, int height);

/* Other Internal routines */
HIDDEN void wgl_clipper(FBIO *ifp);
HIDDEN int wgl_getmem(FBIO *ifp);
HIDDEN void backbuffer_to_screen(FBIO *ifp, int one_y);
HIDDEN void wgl_cminit(FBIO *ifp);
HIDDEN PIXELFORMATDESCRIPTOR * wgl_choose_visual(FBIO *ifp);
HIDDEN int is_linear_cmap(FBIO *ifp);

HIDDEN int wgl_nwindows = 0; 	/* number of open windows */


FBIO *saveifp;
int titleBarHeight = 0;
int borderWidth = 0;

/*
 * Structure of color map in shared memory region.  Has exactly the
 * same format as the SGI hardware "gammaramp" map Note that only the
 * lower 8 bits are significant.
 */
struct wgl_cmap {
    short cmr[256];
    short cmg[256];
    short cmb[256];
};


/*
 * This defines the format of the in-memory framebuffer copy.  The
 * alpha component and reverse order are maintained for compatibility
 * with /dev/sgi
 */
struct wgl_pixel {
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char alpha;
};


/* Clipping structure for zoom/pan operations */
struct wgl_clip {
    int xpixmin;	/* view clipping planes clipped to pixel memory space*/
    int xpixmax;
    int ypixmin;
    int ypixmax;
    int xscrmin;	/* view clipping planes */
    int xscrmax;
    int yscrmin;
    int yscrmax;
    double oleft;	/* glOrtho parameters */
    double oright;
    double otop;
    double obottom;
};


/*
 * Per window state information, overflow area.
 */
struct sgiinfo {
    short mi_curs_on;
    short mi_cmap_flag;		/* enabled when there is a non-linear map in memory */
    int mi_shmid;
    int mi_memwidth;		/* width of scanline in if_mem */
    short mi_xoff;		/* X viewport offset, rel. window*/
    short mi_yoff;		/* Y viewport offset, rel. window*/
    int mi_pid;			/* for multi-cpu check */
    int mi_parent;		/* PID of linger-mode process */
    int mi_doublebuffer;	/* 0=singlebuffer 1=doublebuffer */
    struct wgl_pixel mi_scanline[XMAXSCREEN+1];	/* one scanline */
};


/*
 * Per window state information particular to the OpenGL interface
 */
struct wglinfo {
    HGLRC glxc;
    Display *dispp;		/* pointer to X display connection */
    Window wind;		/* Window identifier */
    int firstTime;
    int alive;
    long event_mask;		/* event types to be received */
    short front_flag;		/* front buffer being used (b-mode) */
    short copy_flag;		/* pan and zoom copied from backbuffer */
    short soft_cmap_flag;	/* use software colormapping */
    int cmap_size;		/* hardware colormap size */
    int win_width;		/* actual window width */
    int win_height;		/* actual window height */
    int vp_width;		/* actual viewport width */
    int vp_height;		/* actual viewport height */
    struct wgl_clip clip;	/* current view clipping */
    Window cursor;
    PIXELFORMATDESCRIPTOR *vip;	/* pointer to info on current visual */
    Colormap xcmap;		/* xstyle color map */
    int use_ext_ctrl;		/* for controlling the Wgl graphics engine externally */
    HDC hdc;
    HWND hwnd;

};


#define SGI(ptr)	((struct sgiinfo *)((ptr)->u1.p))
#define SGIL(ptr)	((ptr)->u1.p)		/* left hand side version */
#define WGL(ptr)	((struct wglinfo *)((ptr)->u6.p))
#define WGLL(ptr)	((ptr)->u6.p)		/* left hand side version */
#define if_mem		u2.p			/* shared memory pointer */
#define if_cmap		u3.p			/* color map in shared memory */
#define CMR(x)		((struct wgl_cmap *)((x)->if_cmap))->cmr
#define CMG(x)		((struct wgl_cmap *)((x)->if_cmap))->cmg
#define CMB(x)		((struct wgl_cmap *)((x)->if_cmap))->cmb
#define if_zoomflag	u4.l			/* zoom > 1 */
#define if_mode		u5.l			/* see MODE_* defines */

#define MARGIN 4 /* # pixels margin to screen edge */

#define CLIP_XTRA 1

#define WIN_L (ifp->if_max_width - ifp->if_width - MARGIN)
#define WIN_T (ifp->if_max_height - ifp->if_height - MARGIN)

/*
 * The mode has several independent bits:
 * SHARED -vs- MALLOC'ed memory for the image
 * TRANSIENT -vs- LINGERING windows
 * Windowed -vs- Centered Full screen
 * Suppress dither -vs- dither
 * Double -vs- Single buffered
 * DrawPixels -vs- CopyPixels
 */
#define MODE_1MASK	(1<<0)
#define MODE_1SHARED	(0<<0)	/* Use Shared memory */
#define MODE_1MALLOC	(1<<0)	/* Use malloc memory */

#define MODE_2MASK	(1<<1)
#define MODE_2TRANSIENT	(0<<1)
#define MODE_2LINGERING (1<<1)	/* leave window up after closing*/

#define MODE_4MASK	(1<<3)
#define MODE_4NORMAL	(0<<3)	/* dither if it seems necessary */
#define MODE_4NODITH	(1<<3)	/* suppress any dithering */

#define MODE_7MASK	(1<<6)
#define MODE_7NORMAL	(0<<6)	/* install colormap in hardware if possible*/
#define MODE_7SWCMAP	(1<<6)	/* use software colormapping */

#define MODE_9MASK	(1<<8)
#define MODE_9NORMAL	(0<<8)	/* doublebuffer if possible */
#define MODE_9SINGLEBUF	(1<<8)	/* singlebuffer only */

#define MODE_11MASK	(1<<10)
#define MODE_11NORMAL	(0<<10)	/* always draw from mem. to window */
#define MODE_11COPY	(1<<10)	/* keep full image on back buffer */

#define MODE_12MASK	(1<<11)
#define MODE_12NORMAL	(0<<11)
#define MODE_12DELAY_WRITES_TILL_FLUSH	(1<<11)
/* and copy current view to front */
#define MODE_15MASK	(1<<14)
#define MODE_15NORMAL	(0<<14)
#define MODE_15ZAP	(1<<14)	/* zap the shared memory segment */

HIDDEN struct modeflags {
    char c;
    long mask;
    long value;
    char *help;
} modeflags[] = {
    { 'p',	MODE_1MASK, MODE_1MALLOC,
      "Private memory - else shared" },
    { 'l',	MODE_2MASK, MODE_2LINGERING,
      "Lingering window" },
    { 't',	MODE_2MASK, MODE_2TRANSIENT,
      "Transient window" },
    { 'd',  MODE_4MASK, MODE_4NODITH,
      "Suppress dithering - else dither if not 24-bit buffer" },
    { 'c',	MODE_7MASK, MODE_7SWCMAP,
      "Perform software colormap - else use hardware colormap if possible" },
    { 's',	MODE_9MASK, MODE_9SINGLEBUF,
      "Single buffer - else double buffer if possible" },
    { 'b',	MODE_11MASK, MODE_11COPY,
      "Fast pan and zoom using backbuffer copy - else normal " },
    { 'D',	MODE_12DELAY_WRITES_TILL_FLUSH, MODE_12DELAY_WRITES_TILL_FLUSH,
      "Don't update screen until fb_flush() is called.  (Double buffer sim)" },
    { 'z',	MODE_15MASK, MODE_15ZAP,
      "Zap (free) shared memory.  Can also be done with fbfree command" },
    { '\0', 0, 0, "" }
};


/************************************************************************/
/******************* Shared Memory Support ******************************/
/************************************************************************/

/*
 * W G L _ G E T M E M
 *
 * Because there is no hardware zoom or pan, we need to repaint the
 * screen (with big pixels) to implement these operations.  This means
 * that the actual "contents" of the frame buffer need to be stored
 * somewhere else.  If possible, we allocate a shared memory segment
 * to contain that image.  This has several advantages, the most
 * important being that when operating the display in 12-bit output
 * mode, pixel-readbacks still give the full 24-bits of color.  System
 * V shared memory persists until explicitly killed, so this also
 * means that in MEX mode, the previous contents of the frame buffer
 * still exist, and can be again accessed, even though the MEX windows
 * are transient, per-process.
 *
 * There are a few oddities, however.  The worst is that System V will
 * not allow the break (see sbrk(2)) to be set above a shared memory
 * segment, and shmat(2) does not seem to allow the selection of any
 * reasonable memory address (like 6 Mbytes up) for the shared memory.
 * In the initial version of this routine, that prevented subsequent
 * calls to malloc() from succeeding, quite a drawback.  The
 * work-around used here is to increase the current break to a large
 * value, attach to the shared memory, and then return the break to
 * its original value.  This should allow most reasonable requests for
 * memory to be satisfied.  In special cases, the values used here
 * might need to be increased.
 */
HIDDEN int
wgl_getmem(FBIO *ifp)
{
    int pixsize;
    int size;
    int i;
    char *sp;
    int new = 0;

    errno = 0;

    {
	/* In this mode, only malloc as much memory as is needed. */
	SGI(ifp)->mi_memwidth = ifp->if_width;
	pixsize = ifp->if_height * ifp->if_width * sizeof(struct wgl_pixel);
	size = pixsize + sizeof(struct wgl_cmap);

	sp = calloc(1, size);
	if (sp == 0) {
	    fb_log("wgl_getmem: frame buffer memory malloc failed\n");
	    goto fail;
	}
	new = 1;
	goto success;
    }

success:
    ifp->if_mem = sp;
    ifp->if_cmap = sp + pixsize;	/* cmap at end of area */
    i = CMB(ifp)[255];			/* try to deref last word */
    CMB(ifp)[255] = i;

    /* Provide non-black colormap on creation of new shared mem */
    if (new)
	wgl_cminit(ifp);
    return 0;
fail:
    fb_log("wgl_getmem:  Unable to attach to shared memory.\n");
    if ((sp = calloc(1, size)) == NULL) {
	fb_log("wgl_getmem:  malloc failure\n");
	return -1;
    }
    new = 1;
    goto success;
}


/*
 * W G L _ Z A P M E M
 */
void
wgl_zapmem(void)
{
}


/*
 * S I G K I D
 */
HIDDEN void
sigkid(int UNUSED(pid))
{
    exit(0);
}


/* W G L _ X M I T _ S C A N L I N E S
 *
 * Note: unlike sgi_xmit_scanlines, this function updates an arbitrary
 * rectangle of the frame buffer
 */
HIDDEN void
wgl_xmit_scanlines(FBIO *ifp, int ybase, int nlines, int xbase, int npix)
{
    int y;
    int n;
    int sw_cmap;	/* !0 => needs software color map */
    struct wgl_clip *clp;

    /* Caller is expected to handle attaching context, etc. */

    clp = &(WGL(ifp)->clip);

    if (WGL(ifp)->soft_cmap_flag  && SGI(ifp)->mi_cmap_flag) {
	sw_cmap = 1;
    } else {
	sw_cmap = 0;
    }

    if (xbase > clp->xpixmax || ybase > clp->ypixmax)
	return;
    if (xbase < clp->xpixmin)
	xbase = clp->xpixmin;
    if (ybase < clp->ypixmin)
	ybase = clp->ypixmin;

    if ((xbase + npix -1) > clp->xpixmax)
	npix = clp->xpixmax - xbase + 1;
    if ((ybase + nlines - 1) > clp->ypixmax)
	nlines = clp->ypixmax - ybase + 1;

    if (!WGL(ifp)->use_ext_ctrl) {
	if (!WGL(ifp)->copy_flag) {
	    /*
	     * Blank out areas of the screen around the image, if
	     * exposed.  In COPY mode, this is done in
	     * backbuffer_to_screen().
	     */

	    /* Blank out area left of image */
	    glColor3b(0, 0, 0);
	    if (clp->xscrmin < 0) glRecti(
		clp->xscrmin - CLIP_XTRA,
		clp->yscrmin - CLIP_XTRA,
		CLIP_XTRA,
		clp->yscrmax + CLIP_XTRA);

	    /* Blank out area below image */
	    if (clp->yscrmin < 0) glRecti(
		clp->xscrmin - CLIP_XTRA,
		clp->yscrmin - CLIP_XTRA,
		clp->xscrmax + CLIP_XTRA,
		CLIP_XTRA);

	    /* Blank out area right of image */
	    if (clp->xscrmax >= ifp->if_width) glRecti(
		ifp->if_width - CLIP_XTRA,
		clp->yscrmin - CLIP_XTRA,
		clp->xscrmax + CLIP_XTRA,
		clp->yscrmax + CLIP_XTRA);

	    /* Blank out area above image */
	    if (clp->yscrmax >= ifp->if_height) glRecti(
		clp->xscrmin - CLIP_XTRA,
		ifp->if_height- CLIP_XTRA,
		clp->xscrmax + CLIP_XTRA,
		clp->yscrmax + CLIP_XTRA);

	} else if (WGL(ifp)->front_flag) {
	    /* in COPY mode, always draw full sized image into
	     * backbuffer.  backbuffer_to_screen() is used to update
	     * the front buffer
	     */
	    glDrawBuffer(GL_BACK);
	    WGL(ifp)->front_flag = 0;
	    glMatrixMode(GL_PROJECTION);
	    glPushMatrix();	/* store current view clipping matrix*/
	    glLoadIdentity();
	    glOrtho(-0.25, ((GLdouble) WGL(ifp)->vp_width)-0.25,
		    -0.25, ((GLdouble) WGL(ifp)->vp_height)-0.25,
		    -1.0, 1.0);
	    glPixelZoom(1.0, 1.0);
	}
    }

    if (sw_cmap) {
	/* Software colormap each line as it's transmitted */
	int x;
	struct wgl_pixel *wglp;
	struct wgl_pixel *op;

	y = ybase;
	if (CJDEBUG) printf("Doing sw colormap xmit\n");
	/* Perform software color mapping into temp scanline */
	op = SGI(ifp)->mi_scanline;
	for (n=nlines; n>0; n--, y++) {
	    wglp = (struct wgl_pixel *)&ifp->if_mem[
		(y*SGI(ifp)->mi_memwidth)*
		sizeof(struct wgl_pixel) ];
	    for (x=xbase+npix-1; x>=xbase; x--) {
		op[x].red   = CMR(ifp)[wglp[x].red];
		op[x].green = CMG(ifp)[wglp[x].green];
		op[x].blue  = CMB(ifp)[wglp[x].blue];
	    }

	    glPixelStorei(GL_UNPACK_SKIP_PIXELS, xbase);
	    glRasterPos2i(xbase, y);
	    glDrawPixels(npix, 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
			 (const GLvoid *) op);

	}

    } else {
	/* No need for software colormapping */

	glPixelStorei(GL_UNPACK_ROW_LENGTH, SGI(ifp)->mi_memwidth);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, xbase);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, ybase);

	glRasterPos2i(xbase, ybase);
	glDrawPixels(npix, nlines, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
		     (const GLvoid *) ifp->if_mem);

	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    }
}


LONG WINAPI
MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
	case WM_PAINT:
	    if (!WGL(saveifp)->use_ext_ctrl)
		expose_callback(saveifp, 0);
	    break;
	case WM_LBUTTONDOWN:
	    break;
	case WM_RBUTTONDOWN:
	    break;
	case WM_MBUTTONDOWN:
	    break;
	case WM_CLOSE:
	    WGL(saveifp)->alive = -1;
	    break;
	case WM_LBUTTONUP:
	    WGL(saveifp)->alive = 0;
	    break;
	case WM_RBUTTONUP:
	    WGL(saveifp)->alive = 0;
	    break;
	case WM_MBUTTONUP:
	    {
		int x, y;
		struct wgl_pixel *wglp;

		x = GET_X_LPARAM(lParam);
		y = saveifp->if_height - GET_Y_LPARAM(lParam) - 1;

		if (x < 0 || y < 0) {
		    fb_log("No RGB (outside image viewport)\n");
		    break;
		}

		wglp = (struct wgl_pixel *)&saveifp->if_mem[
		    (y*SGI(saveifp)->mi_memwidth)*
		    sizeof(struct wgl_pixel)];

		fb_log("At image (%d, %d), real RGB=(%3d %3d %3d)\n",
		       x, y, (int)wglp[x].red, (int)wglp[x].green, (int)wglp[x].blue);
	    }
	    break;
	case WM_KEYDOWN:
	    break;
	case WM_KEYUP:
	    WGL(saveifp)->alive = 0;
	    break;
	case WM_SIZE:
	    {
		/* WIP */
#if 0
		if (conf->width == WGL(ifp)->win_width &&
		    conf->height == WGL(ifp)->win_height)
		    return;

		wgl_configureWindow(ifp, conf->width, conf->height);
#endif
	    }
	default:
	    return DefWindowProc (hWnd, uMsg, wParam, lParam);
    }

    return 1;
}


HIDDEN int
wgl_open(FBIO *ifp, const char *file, int width, int height)
{
    static char title[128];
    int mode,  ret;
    HWND hwnd;
    HDC hdc;
    HGLRC glxc;
    HINSTANCE hinstance;
    DWORD Dword;
    WNDCLASS wndclass;

    FB_CK_FBIO(ifp);

    saveifp = ifp;


    /*
     * First, attempt to determine operating mode for this open, based
     * upon the "unit number" or flags.  file = "/dev/wgl###"
     */
    mode = MODE_2LINGERING;

    if (file != NULL) {
	const char *cp;
	char modebuf[80];
	char *mp;
	int alpha;
	struct modeflags *mfp;

	if (strncmp(file, ifp->if_name, strlen(ifp->if_name))) {
	    /* How did this happen? */
	    mode = 0;
	} else {
	    /* Parse the options */
	    alpha = 0;
	    mp = &modebuf[0];
	    cp = &file[8];
	    while (*cp != '\0' && !isspace(*cp)) {
		*mp++ = *cp;	/* copy it to buffer */
		if (isdigit(*cp)) {
		    cp++;
		    continue;
		}
		alpha++;
		for (mfp = modeflags; mfp->c != '\0'; mfp++) {
		    if (mfp->c == *cp) {
			mode = (mode&~mfp->mask)|mfp->value;
			break;
		    }
		}
		if (mfp->c == '\0' && *cp != '-') {
		    fb_log("if_wgl: unknown option '%c' ignored\n", *cp);
		}
		cp++;
	    }
	    *mp = '\0';
	    if (!alpha)
		mode |= atoi(modebuf);
	}

	if ((mode & MODE_15MASK) == MODE_15ZAP) {
	    /* Only task: Attempt to release shared memory segment */
	    wgl_zapmem();
	    return -1;
	}
    }
    ifp->if_mode = mode;

    /*
     * Allocate extension memory sections, addressed by
     * SGI(ifp)->mi_xxx and WGL(ifp)->xxx
     */

    if ((SGIL(ifp) = (char *)calloc(1, sizeof(struct sgiinfo))) == NULL) {
	fb_log("wgl_open:  sgiinfo malloc failed\n");
	return -1;
    }
    if ((WGLL(ifp) = (char *)calloc(1, sizeof(struct wglinfo))) == NULL) {
	fb_log("wgl_open:  wglinfo malloc failed\n");
	return -1;
    }

    SGI(ifp)->mi_shmid = -1;	/* indicate no shared memory */

    /* the Silicon Graphics Library Window management routines use
     * shared memory. This causes lots of problems when you want to
     * pass a window structure to a child process.  One hack to get
     * around this is to immediately fork and create a child process
     * and sleep until the child sends a kill signal to the parent
     * process. (in FBCLOSE) This allows us to use the traditional fb
     * utility programs as well as allow the frame buffer window to
     * remain around until killed by the menu subsystem.
     */

    /* use defaults if invalid width and height specified */
    if (width <= 0)
	width = ifp->if_width;
    if (height <= 0)
	height = ifp->if_height;
    /* use max values if width and height are greater */
    if (width > ifp->if_max_width)
	width = ifp->if_max_width;
    if (height > ifp->if_max_height)
	height = ifp->if_max_height;

    ifp->if_width = width;
    ifp->if_height = height;

    SGI(ifp)->mi_curs_on = 1;

    /* Build a descriptive window title bar */
    (void)snprintf(title, 128, "BRL-CAD /dev/wgl %s, %s",
		   ((ifp->if_mode & MODE_2MASK) == MODE_2TRANSIENT) ?
		   "Transient Win":
		   "Lingering Win",
		   ((ifp->if_mode & MODE_1MASK) == MODE_1MALLOC) ?
		   "Private Mem" :
		   "Shared Mem");

    /* initialize window state variables before calling wgl_getmem */
    ifp->if_zoomflag = 0;
    ifp->if_xzoom = 1;	/* for zoom fakeout */
    ifp->if_yzoom = 1;	/* for zoom fakeout */
    ifp->if_xcenter = width/2;
    ifp->if_ycenter = height/2;
    SGI(ifp)->mi_pid = bu_process_id();

    /* Attach to shared memory, potentially with a screen repaint */
    if (wgl_getmem(ifp) < 0)
	return -1;

    /* Register the frame class */
    wndclass.style         = 0;
    wndclass.lpfnWndProc   = (WNDPROC)MainWndProc;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.hInstance     = Tk_GetHINSTANCE();
    wndclass.hIcon         = LoadIcon (Tk_GetHINSTANCE(), "Win OpenGL");
    wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wndclass.lpszMenuName  = "Win OpenGL";
    wndclass.lpszClassName = "Win OpenGL";

    ret = RegisterClass (&wndclass);

    titleBarHeight = GetSystemMetrics(SM_CYCAPTION);
    borderWidth = GetSystemMetrics(SM_CYFRAME);

    WGL(ifp)->hwnd =  CreateWindow(
	"Win OpenGL",  /* pointer to registered class name */
	title, /* pointer to window name */
	WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, /* window style */
	CW_USEDEFAULT, /* horizontal position of window */
	CW_USEDEFAULT, /* vertical position of window */
	ifp->if_width + 2 * borderWidth, /* window width */
	ifp->if_height + titleBarHeight + 2 * borderWidth, /* window height */
	NULL, /* handle to parent or owner window */
	NULL, /* handle to menu or child-window identifier */
	Tk_GetHINSTANCE(), /* handle to application instance */
	NULL /* pointer to window-creation data */
	);

    Dword = GetLastError();

    hinstance=Tk_GetHINSTANCE();
    hwnd = WGL(ifp)->hwnd;
    WGL(ifp)->hdc = GetDC(WGL(ifp)->hwnd);
    hdc = WGL(ifp)->hdc;


    /* Choose an appropriate visual. */
    if ((WGL(ifp)->vip = wgl_choose_visual(ifp)) == NULL) {
	fb_log("wgl_open: Couldn't find an appropriate visual.  Exiting.\n");
	return -1;
    }

    WGL(ifp)->glxc = wglCreateContext(WGL(ifp)->hdc);
    glxc = WGL(ifp)->glxc;

    /* count windows */
    wgl_nwindows++;

    WGL(ifp)->alive = 1;
    WGL(ifp)->firstTime = 1;

    ShowWindow(WGL(ifp)->hwnd, SW_SHOW);
    UpdateWindow(WGL(ifp)->hwnd);

    /* Loop through events until first exposure event is processed */
    while (WGL(ifp)->firstTime == 1)
	wgl_do_event(ifp);

    return 0;
}


int
_wgl_open_existing(FBIO *ifp,
		   Display *dpy,
		   Window win,
		   Colormap cmap,
		   PIXELFORMATDESCRIPTOR *vip,
		   HDC hdc,
		   int width,
		   int height,
		   HGLRC glxc,
		   int double_buffer,
		   int soft_cmap)
{

    /* XXX for now use private memory */
    ifp->if_mode = MODE_1MALLOC;

    /*
     * Allocate extension memory sections, addressed by
     * SGI(ifp)->mi_xxx and WGL(ifp)->xxx
     */

    if ((SGIL(ifp) = (char *)calloc(1, sizeof(struct sgiinfo))) == NULL) {
	fb_log("wgl_open:  sgiinfo malloc failed\n");
	return -1;
    }
    if ((WGLL(ifp) = (char *)calloc(1, sizeof(struct wglinfo))) == NULL) {
	fb_log("wgl_open:  wglinfo malloc failed\n");
	return -1;
    }

    WGL(ifp)->use_ext_ctrl = 1;

    SGI(ifp)->mi_shmid = -1;	/* indicate no shared memory */
    ifp->if_width = ifp->if_max_width = width;
    ifp->if_height = ifp->if_max_height = height;

    WGL(ifp)->win_width = WGL(ifp)->vp_width = width;
    WGL(ifp)->win_height = WGL(ifp)->vp_height = height;

    SGI(ifp)->mi_curs_on = 1;

    /* initialize window state variables before calling wgl_getmem */
    ifp->if_zoomflag = 0;
    ifp->if_xzoom = 1;	/* for zoom fakeout */
    ifp->if_yzoom = 1;	/* for zoom fakeout */
    ifp->if_xcenter = width/2;
    ifp->if_ycenter = height/2;
    SGI(ifp)->mi_pid = bu_process_id();

    /* Attach to shared memory, potentially with a screen repaint */
    if (wgl_getmem(ifp) < 0)
	return -1;

    WGL(ifp)->dispp = dpy;
    /* ifp->if_selfd = ConnectionNumber(WGL(ifp)->dispp); */

    WGL(ifp)->vip = vip;
    WGL(ifp)->glxc = glxc;
    SGI(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);
    WGL(ifp)->soft_cmap_flag = soft_cmap;
    SGI(ifp)->mi_doublebuffer = double_buffer;
    WGL(ifp)->xcmap = cmap;

    WGL(ifp)->wind = win;
    WGL(ifp)->hdc = hdc;
    ++wgl_nwindows;

    WGL(ifp)->alive = 1;
    WGL(ifp)->firstTime = 1;

    wgl_clipper(ifp);

    return 0;
}


int
wgl_open_existing(FBIO *ifp, int argc, char **argv)
{
    Display *dpy;
    Window win;
    Colormap cmap;
    PIXELFORMATDESCRIPTOR *vip;
    HDC hdc;
    int width;
    int height;
    HGLRC glxc;
    int double_buffer;
    int soft_cmap;

    if (argc != 11)
	return -1;

    if (sscanf(argv[1], "%p", (void *)&dpy) != 1)
	return -1;

    if (sscanf(argv[2], "%p", (void *)&win) != 1)
	return -1;

    if (sscanf(argv[3], "%p", (void *)&cmap) != 1)
	return -1;

    if (sscanf(argv[4], "%p", (void *)&vip) != 1)
	return -1;

    if (sscanf(argv[5], "%p", (void *)&hdc) != 1)
	return -1;

    if (sscanf(argv[8], "%p", (void *)&glxc) != 1)
	return -1;

    if (sscanf(argv[6], "%d", &width) != 1)
	return -1;

    if (sscanf(argv[7], "%d", &height) != 1)
	return -1;

    if (sscanf(argv[9], "%d", &double_buffer) != 1)
	return -1;

    if (sscanf(argv[10], "%d", &soft_cmap) != 1)
	return -1;

    return _wgl_open_existing(ifp, dpy, win, cmap, vip, hdc, width, height,
			      glxc, double_buffer, soft_cmap);
}


HIDDEN int
wgl_final_close(FBIO *ifp)
{

    if (CJDEBUG) {
	printf("wgl_final_close: All done...goodbye!\n");
    }

    if (WGL(ifp)->glxc) {
	wglDeleteContext(WGL(ifp)->glxc);
    }
    if (WGL(ifp)->hdc) {
	ReleaseDC(WGL(ifp)->hwnd, WGL(ifp)->hdc);
    }
    DestroyWindow(WGL(ifp)->hwnd);

    if (SGIL(ifp) != NULL) {
	/* free up memory associated with image */
      
	/* free private memory */
	(void)free(ifp->if_mem);
	/* free state information */
	(void)free((char *)SGIL(ifp));
	SGIL(ifp) = NULL;
    }

    if (WGLL(ifp) != NULL) {
	(void) free((char *)WGLL(ifp));
	WGLL(ifp) = NULL;
    }

    wgl_nwindows--;
    return 0;
}


HIDDEN int
wgl_flush(FBIO *ifp)
{
    if ((ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH) {

	if (wglMakeCurrent(WGL(ifp)->hdc, WGL(ifp)->glxc)==False) {
	    fb_log("Warning, wgl_flush: wglMakeCurrent unsuccessful.\n");
	}

	/* Send entire in-memory buffer to the screen, all at once */
	wgl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	if (SGI(ifp)->mi_doublebuffer) {
	    SwapBuffers(WGL(ifp)->hdc);
	} else {
	    if (WGL(ifp)->copy_flag) {
		backbuffer_to_screen(ifp, -1);
	    }
	}

	/* unattach context for other threads to use, also flushes */
	wglMakeCurrent(NULL, NULL);
    }
    /* XFlush(WGL(ifp)->dispp); */
    glFlush();
    return 0;
}


HIDDEN int
wgl_close(FBIO *ifp)
{

    wgl_flush(ifp);

    /* only the last open window can linger -
     * call final_close if not lingering
     */
    if (wgl_nwindows > 1 ||
	(ifp->if_mode & MODE_2MASK) == MODE_2TRANSIENT)
	return wgl_final_close(ifp);

    if (CJDEBUG)
	printf("wgl_close: remaining open to linger awhile.\n");

    /*
     * else:
     *
     * LINGER mode.  Don't return to caller until user mouses "close"
     * menu item.  This may delay final processing in the calling
     * function for some time, but the assumption is that the user
     * wishes to compare this image with others.
     *
     * Since we plan to linger here, long after our invoker expected
     * us to be gone, be certain that no file descriptors remain open
     * to associate us with pipelines, network connections, etc., that
     * were ALREADY ESTABLISHED before the point that fb_open() was
     * called.
     *
     * The simple for i=0..20 loop will not work, because that smashes
     * some window-manager files.  Therefore, we content ourselves
     * with eliminating stdin.
     */
    fclose(stdin);

    while (0 < WGL(ifp)->alive)
	wgl_do_event(ifp);

    return 0;
}


int
wgl_close_existing(FBIO *ifp)
{
    /*
      if (WGL(ifp)->cursor)
      XDestroyWindow(WGL(ifp)->dispp, WGL(ifp)->cursor);
    */

    if (SGIL(ifp) != NULL) {
	/* free up memory associated with image */
	/* free private memory */
	(void)free(ifp->if_mem);
	/* free state information */
	(void)free((char *)SGIL(ifp));
	SGIL(ifp) = NULL;
    }

    if (WGLL(ifp) != NULL) {
	(void) free((char *)WGLL(ifp));
	WGLL(ifp) = NULL;
    }

    return 0;
}


/*
 * W G L _ P O L L
 *
 * Handle any pending input events
 */
HIDDEN int
wgl_poll(FBIO *ifp)
{
    wgl_do_event(ifp);

    if (WGL(ifp)->alive < 0)
	return 1;
    else
	return 0;
}


/*
 * W G L _ F R E E
 *
 * Free shared memory resources, and close.
 */
HIDDEN int
wgl_free(FBIO *ifp)
{
    int ret;

    if (CJDEBUG) printf("entering wgl_free\n");
    /* Close the framebuffer */
    ret = wgl_final_close(ifp);

    if ((ifp->if_mode & MODE_1MASK) == MODE_1SHARED) {
	/* If shared mem, release the shared memory segment */
	wgl_zapmem();
    }
    return ret;
}


/**
 * pp is a pointer to beginning of memory segment
 */
HIDDEN int
wgl_clear(FBIO *ifp, unsigned char *pp)
{
    struct wgl_pixel bg;
    struct wgl_pixel *wglp;
    int cnt;
    int y;

    if (CJDEBUG) printf("entering wgl_clear\n");

    if (wglMakeCurrent(WGL(ifp)->hdc, WGL(ifp)->glxc)==False) {
	fb_log("Warning, wgl_clear: wglMakeCurrent unsuccessful.\n");
    }

    /* Set clear colors */
    if (pp != RGBPIXEL_NULL) {
	bg.alpha = 0;
	bg.red   = (pp)[RED];
	bg.green = (pp)[GRN];
	bg.blue  = (pp)[BLU];
	glClearColor(pp[RED]/255.0, pp[GRN]/255.0, pp[BLU]/255.0, 0.0);
    } else {
	bg.alpha = 0;
	bg.red   = 0;
	bg.green = 0;
	bg.blue  = 0;
	glClearColor(0, 0, 0, 0);
    }

    /* Flood rectangle in shared memory */
    for (y=0; y < ifp->if_height; y++) {
	wglp = (struct wgl_pixel *)&ifp->if_mem[
	    (y*SGI(ifp)->mi_memwidth+0)*sizeof(struct wgl_pixel) ];
	for (cnt=ifp->if_width-1; cnt >= 0; cnt--) {
	    *wglp++ = bg;	/* struct copy */
	}
    }


    /* Update screen */
    if (WGL(ifp)->use_ext_ctrl) {
	glClear(GL_COLOR_BUFFER_BIT);
    } else {
	if (WGL(ifp)->copy_flag) {
	    /* COPY mode: clear both buffers */
	    if (WGL(ifp)->front_flag) {
		glDrawBuffer(GL_BACK);
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawBuffer(GL_FRONT);
		glClear(GL_COLOR_BUFFER_BIT);
	    } else {
		glDrawBuffer(GL_FRONT);
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawBuffer(GL_BACK);
		glClear(GL_COLOR_BUFFER_BIT);
	    }
	} else {
	    glClear(GL_COLOR_BUFFER_BIT);
	    if (SGI(ifp)->mi_doublebuffer) {
		SwapBuffers(WGL(ifp)->hdc);
	    }
	}

    }

    /* unattach context for other threads to use */
    wglMakeCurrent(NULL, NULL);

    return 0;
}


/*
 * W G L _ V I E W
 */
HIDDEN int
wgl_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    struct wgl_clip *clp;

    if (CJDEBUG) printf("entering wgl_view\n");

    if (xzoom < 1) xzoom = 1;
    if (yzoom < 1) yzoom = 1;
    if (ifp->if_xcenter == xcenter && ifp->if_ycenter == ycenter
	&& ifp->if_xzoom == xzoom && ifp->if_yzoom == yzoom)
	return 0;

    if (xcenter < 0 || xcenter >= ifp->if_width)
	return -1;
    if (ycenter < 0 || ycenter >= ifp->if_height)
	return -1;
    if (xzoom >= ifp->if_width || yzoom >= ifp->if_height)
	return -1;

    ifp->if_xcenter = xcenter;
    ifp->if_ycenter = ycenter;
    ifp->if_xzoom = xzoom;
    ifp->if_yzoom = yzoom;

    if (ifp->if_xzoom > 1 || ifp->if_yzoom > 1)
	ifp->if_zoomflag = 1;
    else ifp->if_zoomflag = 0;


    if (WGL(ifp)->use_ext_ctrl) {
	wgl_clipper(ifp);
    } else {
	if (wglMakeCurrent(WGL(ifp)->hdc, WGL(ifp)->glxc)==False) {
	    fb_log("Warning, wgl_view: wglMakeCurrent unsuccessful.\n");
	}

	/* Set clipping matrix and zoom level */
	glMatrixMode(GL_PROJECTION);
	if (WGL(ifp)->copy_flag && !WGL(ifp)->front_flag) {
	    /* COPY mode - no changes to backbuffer copy - just
	     * need to update front buffer
	     */
	    glPopMatrix();
	    glDrawBuffer(GL_FRONT);
	    WGL(ifp)->front_flag = 1;
	}
	glLoadIdentity();

	wgl_clipper(ifp);
	clp = &(WGL(ifp)->clip);
	glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
	glPixelZoom((float) ifp->if_xzoom, (float) ifp->if_yzoom);

	if (WGL(ifp)->copy_flag) {
	    backbuffer_to_screen(ifp, -1);
	} else {
	    wgl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    if (SGI(ifp)->mi_doublebuffer) {
		SwapBuffers(WGL(ifp)->hdc);
	    }
	}

	/* unattach context for other threads to use */
	wglMakeCurrent(NULL, NULL);
    }

    return 0;
}


/*
 * W G L _ G E T V I E W
 */
HIDDEN int
wgl_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    if (CJDEBUG) printf("entering wgl_getview\n");

    *xcenter = ifp->if_xcenter;
    *ycenter = ifp->if_ycenter;
    *xzoom = ifp->if_xzoom;
    *yzoom = ifp->if_yzoom;

    return 0;
}


/*
 * read count pixels into pixelp starting at x, y
 */
HIDDEN int
wgl_read(FBIO *ifp, int x, int y, unsigned char *pixelp, size_t count)
{
    size_t n;
    size_t scan_count;	/* # pix on this scanline */
    unsigned char *cp;
    int ret;
    struct wgl_pixel *wglp;

    if (CJDEBUG) printf("entering wgl_read\n");

    if (x < 0 || x >= ifp->if_width ||
	y < 0 || y >= ifp->if_height)
	return -1;

    ret = 0;
    cp = (unsigned char *)(pixelp);

    while (count) {
	if (y >= ifp->if_height)
	    break;

	if (count >= ifp->if_width-x)
	    scan_count = ifp->if_width-x;
	else
	    scan_count = count;

	wglp = (struct wgl_pixel *)&ifp->if_mem[
	    (y*SGI(ifp)->mi_memwidth+x)*sizeof(struct wgl_pixel) ];

	n = scan_count;
	while (n) {
	    cp[RED] = wglp->red;
	    cp[GRN] = wglp->green;
	    cp[BLU] = wglp->blue;
	    wglp++;
	    cp += 3;
	    n--;
	}
	ret += scan_count;
	count -= scan_count;
	x = 0;
	/* Advance upwards */
	if (++y >= ifp->if_height)
	    break;
    }
    return ret;
}


/*
 * write count pixels from pixelp starting at xstart, ystart
 */
HIDDEN int
wgl_write(FBIO *ifp, int xstart, int ystart, const unsigned char *pixelp, size_t count)
{
    size_t scan_count;	/* # pix on this scanline */
    size_t pix_count;	/* # pixels to send */
    unsigned char *cp;
    int ret;
    int ybase;
    int x;
    int y;

    if (CJDEBUG) printf("entering wgl_write\n");

    /* fast exit cases */
    pix_count = count;
    if (pix_count == 0)
	return 0;	/* OK, no pixels transferred */

    x = xstart;
    ybase = y = ystart;

    if (x < 0 || x >= ifp->if_width ||
	y < 0 || y >= ifp->if_height)
	return -1;

    ret = 0;
    cp = (unsigned char *)(pixelp);

    while (pix_count) {
	unsigned int n;
	struct wgl_pixel *wglp;

	if (y >= ifp->if_height)
	    break;

	if (pix_count >= ifp->if_width-x)
	    scan_count = ifp->if_width-x;
	else
	    scan_count = pix_count;

	wglp = (struct wgl_pixel *)&ifp->if_mem[
	    (y*SGI(ifp)->mi_memwidth+x)*sizeof(struct wgl_pixel) ];

	n = scan_count;
	if ((n & 3) != 0) {
	    /* This code uses 60% of all CPU time */
	    while (n) {
		/* alpha channel is always zero */
		wglp->red   = cp[RED];
		wglp->green = cp[GRN];
		wglp->blue  = cp[BLU];
		wglp++;
		cp += 3;
		n--;
	    }
	} else {
	    while (n) {
		/* alpha channel is always zero */
		wglp[0].red   = cp[RED+0*3];
		wglp[0].green = cp[GRN+0*3];
		wglp[0].blue  = cp[BLU+0*3];
		wglp[1].red   = cp[RED+1*3];
		wglp[1].green = cp[GRN+1*3];
		wglp[1].blue  = cp[BLU+1*3];
		wglp[2].red   = cp[RED+2*3];
		wglp[2].green = cp[GRN+2*3];
		wglp[2].blue  = cp[BLU+2*3];
		wglp[3].red   = cp[RED+3*3];
		wglp[3].green = cp[GRN+3*3];
		wglp[3].blue  = cp[BLU+3*3];
		wglp += 4;
		cp += 3*4;
		n -= 4;
	    }
	}
	ret += scan_count;
	pix_count -= scan_count;
	x = 0;
	if (++y >= ifp->if_height)
	    break;
    }

    if ((ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH)
	return ret;

    if (!WGL(ifp)->use_ext_ctrl) {

	if (wglMakeCurrent(WGL(ifp)->hdc, WGL(ifp)->glxc)==False) {
	    fb_log("Warning, wgl_write: wglMakeCurrent unsuccessful.\n");
	}

	if (xstart + count <= ifp->if_width) {
	    /* "Fast path" case for writes of less than one scanline.
	     * The assumption is that there will be a lot of short
	     * writes, and it's best just to ignore the backbuffer
	     */
	    wgl_xmit_scanlines(ifp, ybase, 1, xstart, count);
	    if (WGL(ifp)->copy_flag) {
		/* repaint one scanline from backbuffer */
		backbuffer_to_screen(ifp, ybase);
	    }
	} else {
	    /* Normal case -- multi-pixel write */
	    if (SGI(ifp)->mi_doublebuffer) {
		/* refresh whole screen */
		wgl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
		SwapBuffers(WGL(ifp)->hdc);
	    } else {
		/* just write rectangle */
		wgl_xmit_scanlines(ifp, ybase, y-ybase, 0, ifp->if_width);
		if (WGL(ifp)->copy_flag) {
		    backbuffer_to_screen(ifp, -1);
		}
	    }
	}

	/* unattach context for other threads to use */
	wglMakeCurrent(NULL, NULL);
    }

    return ret;

}


/*
 * W G L _ W R I T E R E C T
 *
 * The task of this routine is to reformat the pixels into SGI
 * internal form, and then arrange to have them sent to the screen
 * separately.
 */
HIDDEN int
wgl_writerect(FBIO *ifp,
	      int xmin,
	      int ymin,
	      int width,
	      int height,
	      const unsigned char *pp)
{
    int x;
    int y;
    unsigned char *cp;
    struct wgl_pixel *wglp;

    if (CJDEBUG) printf("entering wgl_writerect\n");


    if (width <= 0 || height <= 0)
	return 0;  /* do nothing */
    if (xmin < 0 || xmin+width > ifp->if_width ||
	ymin < 0 || ymin+height > ifp->if_height)
	return -1; /* no can do */

    cp = (unsigned char *)(pp);
    for (y = ymin; y < ymin+height; y++) {
	wglp = (struct wgl_pixel *)&ifp->if_mem[
	    (y*SGI(ifp)->mi_memwidth+xmin)*sizeof(struct wgl_pixel) ];
	for (x = xmin; x < xmin+width; x++) {
	    /* alpha channel is always zero */
	    wglp->red   = cp[RED];
	    wglp->green = cp[GRN];
	    wglp->blue  = cp[BLU];
	    wglp++;
	    cp += 3;
	}
    }

    if ((ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH)
	return width*height;

    if (!WGL(ifp)->use_ext_ctrl) {
	if (wglMakeCurrent(WGL(ifp)->hdc, WGL(ifp)->glxc)==False) {
	    fb_log("Warning, wgl_writerect: wglMakeCurrent unsuccessful.\n");
	}

	if (SGI(ifp)->mi_doublebuffer) {
	    /* refresh whole screen */
	    wgl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    SwapBuffers(WGL(ifp)->hdc);
	} else {
	    /* just write rectangle*/
	    wgl_xmit_scanlines(ifp, ymin, height, xmin, width);
	    if (WGL(ifp)->copy_flag) {
		backbuffer_to_screen(ifp, -1);
	    }
	}

	/* unattach context for other threads to use */
	wglMakeCurrent(NULL, NULL);
    }

    return width*height;
}


/*
 * W G L _ B W W R I T E R E C T
 *
 * The task of this routine is to reformat the pixels into SGI
 * internal form, and then arrange to have them sent to the screen
 * separately.
 */
HIDDEN int
wgl_bwwriterect(FBIO *ifp,
		int xmin,
		int ymin,
		int width,
		int height,
		const unsigned char *pp)
{
    int x;
    int y;
    unsigned char *cp;
    struct wgl_pixel *wglp;

    if (CJDEBUG) printf("entering wgl_bwwriterect\n");


    if (width <= 0 || height <= 0)
	return 0;  /* do nothing */
    if (xmin < 0 || xmin+width > ifp->if_width ||
	ymin < 0 || ymin+height > ifp->if_height)
	return -1; /* no can do */

    cp = (unsigned char *)(pp);
    for (y = ymin; y < ymin+height; y++) {
	wglp = (struct wgl_pixel *)&ifp->if_mem[
	    (y*SGI(ifp)->mi_memwidth+xmin)*sizeof(struct wgl_pixel) ];
	for (x = xmin; x < xmin+width; x++) {
	    int val;
	    /* alpha channel is always zero */
	    wglp->red   = (val = *cp++);
	    wglp->green = val;
	    wglp->blue  = val;
	    wglp++;
	}
    }

    if ((ifp->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH)
	return width*height;

    if (!WGL(ifp)->use_ext_ctrl) {
	if (wglMakeCurrent(WGL(ifp)->hdc, WGL(ifp)->glxc)==False) {
	    fb_log("Warning, wgl_writerect: wglMakeCurrent unsuccessful.\n");
	}

	if (SGI(ifp)->mi_doublebuffer) {
	    /* refresh whole screen */
	    wgl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    SwapBuffers(WGL(ifp)->hdc);
	} else {
	    /* just write rectangle*/
	    wgl_xmit_scanlines(ifp, ymin, height, xmin, width);
	    if (WGL(ifp)->copy_flag) {
		backbuffer_to_screen(ifp, -1);
	    }
	}

	/* unattach context for other threads to use */
	wglMakeCurrent(NULL, NULL);
    }

    return width*height;
}


HIDDEN int
wgl_rmap(FBIO *ifp, ColorMap *cmp)
{
    int i;

    if (CJDEBUG) printf("entering wgl_rmap\n");

    /* Just parrot back the stored colormap */
    for (i = 0; i < 256; i++) {
	cmp->cm_red[i]   = CMR(ifp)[i]<<8;
	cmp->cm_green[i] = CMG(ifp)[i]<<8;
	cmp->cm_blue[i]  = CMB(ifp)[i]<<8;
    }
    return 0;
}


/*
 * I S _ L I N E A R _ C M A P
 *
 * Check for a color map being linear in R, G, and B.  Returns 1 for
 * linear map, 0 for non-linear map (ie, non-identity map).
 */
HIDDEN int
is_linear_cmap(FBIO *ifp)
{
    int i;

    for (i=0; i<256; i++) {
	if (CMR(ifp)[i] != i) return 0;
	if (CMG(ifp)[i] != i) return 0;
	if (CMB(ifp)[i] != i) return 0;
    }
    return 1;
}


/*
 * W G L _ C M I N I T
 */
HIDDEN void
wgl_cminit(FBIO *ifp)
{
    int i;

    for (i = 0; i < 256; i++) {
	CMR(ifp)[i] = i;
	CMG(ifp)[i] = i;
	CMB(ifp)[i] = i;
    }
}


/*
 * W G L _ W M A P
 */
HIDDEN int
wgl_wmap(FBIO *ifp, const ColorMap *cmp)
{
    int i;
    int prev;	/* !0 = previous cmap was non-linear */

    if (CJDEBUG) printf("entering wgl_wmap\n");

    prev = SGI(ifp)->mi_cmap_flag;
    if (cmp == COLORMAP_NULL) {
	wgl_cminit(ifp);
    } else {
	for (i = 0; i < 256; i++) {
	    CMR(ifp)[i] = cmp-> cm_red[i]>>8;
	    CMG(ifp)[i] = cmp-> cm_green[i]>>8;
	    CMB(ifp)[i] = cmp-> cm_blue[i]>>8;
	}
    }
    SGI(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);


    if (!WGL(ifp)->use_ext_ctrl) {
	if (WGL(ifp)->soft_cmap_flag) {
	    /* if current and previous maps are linear, return */
	    if (SGI(ifp)->mi_cmap_flag == 0 && prev == 0) return 0;

	    /* Software color mapping, trigger a repaint */

	    if (wglMakeCurrent(WGL(ifp)->hdc, WGL(ifp)->glxc)==False) {
		fb_log("Warning, wgl_wmap: wglMakeCurrent unsuccessful.\n");
	    }

	    wgl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    if (SGI(ifp)->mi_doublebuffer) {
		SwapBuffers(WGL(ifp)->hdc);
	    } else if (WGL(ifp)->copy_flag) {
		backbuffer_to_screen(ifp, -1);
	    }

	    /* unattach context for other threads to use, also flushes */
	    wglMakeCurrent(NULL, NULL);
	} else {
	    /* Send color map to hardware */
	}
    }

    return 0;
}


/*
 * W G L _ H E L P
 */
HIDDEN int
wgl_help(FBIO *ifp)
{
    struct modeflags *mfp;

    fb_log("Description: %s\n", ifp->if_type);
    fb_log("Device: %s\n", ifp->if_name);
    fb_log("Max width height: %d %d\n",
	   ifp->if_max_width,
	   ifp->if_max_height);
    fb_log("Default width height: %d %d\n",
	   ifp->if_width,
	   ifp->if_height);
    fb_log("Usage: /dev/wgl[option letters]\n");
    for (mfp = modeflags; mfp->c != '\0'; mfp++) {
	fb_log("   %c   %s\n", mfp->c, mfp->help);
    }

    fb_log("\nCurrent internal state:\n");
    fb_log("	mi_doublebuffer=%d\n", SGI(ifp)->mi_doublebuffer);
    fb_log("	mi_cmap_flag=%d\n", SGI(ifp)->mi_cmap_flag);
    fb_log("	wgl_nwindows=%d\n", wgl_nwindows);

    return 0;
}


HIDDEN int
wgl_setcursor(FBIO *ifp,
	      const unsigned char *bits,
	      int xbits,
	      int ybits,
	      int xorig,
	      int yorig)
{
    return 0;
}


HIDDEN int
wgl_cursor(FBIO *ifp, int mode, int x, int y)
{
    return 0;
}


/*
 * W G L _ C L I P P E R ()
 *
 * Given:
 * - the size of the viewport in pixels (vp_width, vp_height)
 * - the size of the framebuffer image (if_width, if_height)
 * - the current view center (if_xcenter, if_ycenter)
 * - the current zoom (if_xzoom, if_yzoom)
 *
 * Calculate:
 * - the position of the viewport in image space
 * (xscrmin, xscrmax, yscrmin, yscrmax)
 * - the portion of the image which is visible in the viewport
 * (xpixmin, xpixmax, ypixmin, ypixmax)
 */
HIDDEN void
wgl_clipper(FBIO *ifp)
{
    struct wgl_clip *clp;
    int i;
    double pixels;

    clp = &(WGL(ifp)->clip);

    i = WGL(ifp)->vp_width/(2*ifp->if_xzoom);
    clp->xscrmin = ifp->if_xcenter - i;
    i = WGL(ifp)->vp_width/ifp->if_xzoom;
    clp->xscrmax = clp->xscrmin + i;
    pixels = (double) i;
    clp->oleft = ((double) clp->xscrmin) - 0.25*pixels/((double) WGL(ifp)->vp_width);
    clp->oright = clp->oleft + pixels;

    i = WGL(ifp)->vp_height/(2*ifp->if_yzoom);
    clp->yscrmin = ifp->if_ycenter - i;
    i = WGL(ifp)->vp_height/ifp->if_yzoom;
    clp->yscrmax = clp->yscrmin + i;
    pixels = (double) i;
    clp->obottom = ((double) clp->yscrmin) - 0.25*pixels/((double) WGL(ifp)->vp_height);
    clp->otop = clp->obottom + pixels;

    clp->xpixmin = clp->xscrmin;
    clp->xpixmax = clp->xscrmax;
    clp->ypixmin = clp->yscrmin;
    clp->ypixmax = clp->yscrmax;

    if (clp->xpixmin < 0) {
	clp->xpixmin = 0;
    }

    if (clp->ypixmin < 0) {
	clp->ypixmin = 0;
    }

    /* In copy mode, the backbuffer copy image is limited to the
     * viewport size; use that for clipping.  Otherwise, use size of
     * framebuffer memory segment
     */
    if (WGL(ifp)->copy_flag) {
	if (clp->xpixmax > WGL(ifp)->vp_width-1) {
	    clp->xpixmax = WGL(ifp)->vp_width-1;
	}
	if (clp->ypixmax > WGL(ifp)->vp_height-1) {
	    clp->ypixmax = WGL(ifp)->vp_height-1;
	}
    } else {
	if (clp->xpixmax > ifp->if_width-1) {
	    clp->xpixmax = ifp->if_width-1;
	}
	if (clp->ypixmax > ifp->if_height-1) {
	    clp->ypixmax = ifp->if_height-1;
	}
    }

}


/********************************/
/* Call back routines and so on */
/********************************/

HIDDEN void
wgl_do_event(FBIO *ifp)
{
    MSG msg;
    BOOL bRet;
    /* Check and Dispatch any messages. */

    if ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
	if (bRet == -1) {
	    /* handle the error and possibly exit */
	} else {
	    TranslateMessage(&msg);
	    DispatchMessage(&msg);
	}
    }
}


HIDDEN void
expose_callback(FBIO *ifp, int eventPtr)
{
    /* XWindowAttributes xwa; */
    struct wgl_clip *clp;

    if (CJDEBUG) fb_log("entering expose_callback()\n");

    if (wglMakeCurrent(WGL(ifp)->hdc, WGL(ifp)->glxc) == False) {
	fb_log("Warning, libfb/expose_callback: wglMakeCurrent unsuccessful.\n");
    }

    if (WGL(ifp)->firstTime) {
	WGL(ifp)->firstTime = 0;

	/* just in case the configuration is double buffered but
	 * we want to pretend it's not
	 */

	if (!SGI(ifp)->mi_doublebuffer) {
	    glDrawBuffer(GL_FRONT);
	}

	if ((ifp->if_mode & MODE_4MASK) == MODE_4NODITH) {
	    glDisable(GL_DITHER);
	}

	/* set copy mode if possible and requested */
	if (SGI(ifp)->mi_doublebuffer &&
	    ((ifp->if_mode & MODE_11MASK)==MODE_11COPY)) {
	    /* Copy mode only works if there are two buffers to
	     * use. It conflicts with double buffering
	     */
	    WGL(ifp)->copy_flag = 1;
	    SGI(ifp)->mi_doublebuffer = 0;
	    WGL(ifp)->front_flag = 1;
	    glDrawBuffer(GL_FRONT);
	} else {
	    WGL(ifp)->copy_flag = 0;
	}

	WGL(ifp)->win_width=ifp->if_width;
	WGL(ifp)->win_height=ifp->if_height;

	/* clear entire window */
	glViewport(0, 0, WGL(ifp)->win_width, WGL(ifp)->win_height);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	/* Set normal viewport size to minimum of actual window size
	 * and requested framebuffer size
	 */
	WGL(ifp)->vp_width = (WGL(ifp)->win_width < ifp->if_width) ?
	    WGL(ifp)->win_width : ifp->if_width;
	WGL(ifp)->vp_height = (WGL(ifp)->win_height < ifp->if_height) ?
	    WGL(ifp)->win_height : ifp->if_height;
	ifp->if_xcenter = WGL(ifp)->vp_width/2;
	ifp->if_ycenter = WGL(ifp)->vp_height/2;

	/* center viewport in window */
	SGI(ifp)->mi_xoff=(WGL(ifp)->win_width-WGL(ifp)->vp_width)/2;
	SGI(ifp)->mi_yoff=(WGL(ifp)->win_height-WGL(ifp)->vp_height)/2;
	glViewport(SGI(ifp)->mi_xoff,
		   SGI(ifp)->mi_yoff,
		   WGL(ifp)->vp_width,
		   WGL(ifp)->vp_height);
	/* initialize clipping planes and zoom */
	wgl_clipper(ifp);
	clp = &(WGL(ifp)->clip);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop,
		-1.0, 1.0);
	glPixelZoom((float) ifp->if_xzoom, (float) ifp->if_yzoom);
    } else if ((WGL(ifp)->win_width > ifp->if_width) ||
	       (WGL(ifp)->win_height > ifp->if_height)) {
	/* clear whole buffer if window larger than framebuffer */
	if (WGL(ifp)->copy_flag && !WGL(ifp)->front_flag) {
	    glDrawBuffer(GL_FRONT);
	    glViewport(0, 0, WGL(ifp)->win_width,
		       WGL(ifp)->win_height);
	    glClearColor(0, 0, 0, 0);
	    glClear(GL_COLOR_BUFFER_BIT);
	    glDrawBuffer(GL_BACK);
	} else {
	    glViewport(0, 0, WGL(ifp)->win_width,
		       WGL(ifp)->win_height);
	    glClearColor(0, 0, 0, 0);
	    glClear(GL_COLOR_BUFFER_BIT);
	}
	/* center viewport */
	glViewport(SGI(ifp)->mi_xoff,
		   SGI(ifp)->mi_yoff,
		   WGL(ifp)->vp_width,
		   WGL(ifp)->vp_height);
    }

    /* repaint entire image */
    wgl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
    if (SGI(ifp)->mi_doublebuffer) {
	SwapBuffers(WGL(ifp)->hdc);
    } else if (WGL(ifp)->copy_flag) {
	backbuffer_to_screen(ifp, -1);
    }

    if (CJDEBUG) {
	int dbb, db, view[4], getster, getaux;
	glGetIntegerv(GL_VIEWPORT, view);
	glGetIntegerv(GL_DOUBLEBUFFER, &dbb);
	glGetIntegerv(GL_DRAW_BUFFER, &db);
	fb_log("Viewport: x %d y %d width %d height %d\n", view[0],
	       view[1], view[2], view[3]);
	fb_log("expose: double buffered: %d, draw buffer %d\n", dbb, db);
	fb_log("front %d\tback%d\n", GL_FRONT, GL_BACK);
	glGetIntegerv(GL_STEREO, &getster);
	glGetIntegerv(GL_AUX_BUFFERS, &getaux);
	fb_log("double %d, stereo %d, aux %d\n", dbb, getster, getaux);
    }

    /* unattach context for other threads to use */
    wglMakeCurrent(NULL, NULL);
}


void
wgl_configureWindow(FBIO *ifp, int width, int height)
{
    if (width == WGL(ifp)->win_width &&
	height == WGL(ifp)->win_height)
	return;

    ifp->if_width = ifp->if_max_width = width;
    ifp->if_height = ifp->if_max_height = height;

    WGL(ifp)->win_width = WGL(ifp)->vp_width = width;
    WGL(ifp)->win_height = WGL(ifp)->vp_height = height;

    ifp->if_zoomflag = 0;
    ifp->if_xzoom = 1;
    ifp->if_yzoom = 1;
    ifp->if_xcenter = width/2;
    ifp->if_ycenter = height/2;

    wgl_getmem(ifp);
    wgl_clipper(ifp);
}


/* BACKBUFFER_TO_SCREEN - copy pixels from copy on the backbuffer to
 * the front buffer. Do one scanline specified by one_y, or whole
 * screen if one_y equals -1.
 */
HIDDEN void
backbuffer_to_screen(FBIO *ifp, int one_y)
{
    struct wgl_clip *clp;

    if (!(WGL(ifp)->front_flag)) {
	WGL(ifp)->front_flag = 1;
	glDrawBuffer(GL_FRONT);
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPixelZoom((float) ifp->if_xzoom, (float) ifp->if_yzoom);
    }

    clp = &(WGL(ifp)->clip);

    if (one_y > clp->ypixmax) {
	return;
    } else if (one_y < 0) {
	/* do whole visible screen */

	/* Blank out area left of image */
	glColor3b(0, 0, 0);
	if (clp->xscrmin < 0) glRecti(
	    clp->xscrmin - CLIP_XTRA,
	    clp->yscrmin - CLIP_XTRA,
	    CLIP_XTRA,
	    clp->yscrmax + CLIP_XTRA);

	/* Blank out area below image */
	if (clp->yscrmin < 0) glRecti(
	    clp->xscrmin - CLIP_XTRA,
	    clp->yscrmin - CLIP_XTRA,
	    clp->xscrmax + CLIP_XTRA,
	    CLIP_XTRA);

	/* We are in copy mode, so we use vp_width rather than
	 * if_width
	 */
	/* Blank out area right of image */
	if (clp->xscrmax >= WGL(ifp)->vp_width) glRecti(
	    ifp->if_width - CLIP_XTRA,
	    clp->yscrmin - CLIP_XTRA,
	    clp->xscrmax + CLIP_XTRA,
	    clp->yscrmax + CLIP_XTRA);

	/* Blank out area above image */
	if (clp->yscrmax >= WGL(ifp)->vp_height) glRecti(
	    clp->xscrmin - CLIP_XTRA,
	    WGL(ifp)->vp_height - CLIP_XTRA,
	    clp->xscrmax + CLIP_XTRA,
	    clp->yscrmax + CLIP_XTRA);

	/* copy image from backbuffer */
	glRasterPos2i(clp->xpixmin, clp->ypixmin);
	glCopyPixels(SGI(ifp)->mi_xoff + clp->xpixmin,
		     SGI(ifp)->mi_yoff + clp->ypixmin,
		     clp->xpixmax - clp->xpixmin +1,
		     clp->ypixmax - clp->ypixmin +1,
		     GL_COLOR);


    } else if (one_y < clp->ypixmin) {
	return;
    } else {
	/* draw one scanline */
	glRasterPos2i(clp->xpixmin, one_y);
	glCopyPixels(SGI(ifp)->mi_xoff + clp->xpixmin,
		     SGI(ifp)->mi_yoff + one_y,
		     clp->xpixmax - clp->xpixmin +1,
		     1,
		     GL_COLOR);
    }
}


/* W G L _ C H O O S E _ V I S U A L
 *
 * Select an appropriate visual, and set flags.
 *
 * The user requires support for:
 * -OpenGL rendering in RGBA mode
 *
 * The user may desire support for:
 * -a single-buffered OpenGL context
 * -a double-buffered OpenGL context
 * -hardware colormapping (DirectColor)
 *
 * We first try to satisfy all requirements and desires. If that
 * fails, we remove the desires one at a time until we succeed or
 * until only requirements are left. If at any stage more than one
 * visual meets the current criteria, the visual with the greatest
 * depth is chosen.
 *
 * The following flags are set:
 * SGI(ifp)->mi_doublebuffer
 * WGL(ifp)->soft_cmap_flag
 *
 * Return NULL on failure.
 */
HIDDEN PIXELFORMATDESCRIPTOR *
wgl_choose_visual(FBIO *ifp)
{
    int iPixelFormat;
    PIXELFORMATDESCRIPTOR pfd, *ppfd;
    BOOL good;

    ppfd = &pfd;

    iPixelFormat  = GetPixelFormat(WGL(ifp)->hdc);
    ppfd->nSize = sizeof(PIXELFORMATDESCRIPTOR);
    ppfd->nVersion = 1;
    ppfd->dwFlags =  PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_TYPE_RGBA | PFD_STEREO;
    ppfd->iPixelType = PFD_TYPE_RGBA;
    ppfd->cColorBits = 24;
    ppfd->cRedBits = 0;
    ppfd->cRedShift = 0;
    ppfd->cGreenBits = 0;
    ppfd->cGreenShift = 0;
    ppfd->cBlueBits = 0;
    ppfd->cBlueShift = 0;
    ppfd->cAlphaBits = 0;
    ppfd->cAlphaShift = 0;
    ppfd->cAccumBits = 0;
    ppfd->cAccumRedBits = 0;
    ppfd->cAccumGreenBits = 0;
    ppfd->cAccumBlueBits = 0;
    ppfd->cAccumAlphaBits = 0;
    ppfd->cDepthBits = 32;
    ppfd->cStencilBits = 0;
    ppfd->cAuxBuffers = 0;
    ppfd->iLayerType = PFD_MAIN_PLANE;
    ppfd->bReserved = 0;
    ppfd->dwLayerMask = 0;
    ppfd->dwVisibleMask = 0;
    ppfd->dwDamageMask = 0;

    iPixelFormat = ChoosePixelFormat(WGL(ifp)->hdc, ppfd);
    good = SetPixelFormat(WGL(ifp)->hdc, iPixelFormat, ppfd);


    SGI(ifp)->mi_doublebuffer = 1;
    WGL(ifp)->soft_cmap_flag = 1;

    if (good) return ppfd;
    else return (PIXELFORMATDESCRIPTOR *)NULL;
}


int
wgl_refresh(FBIO *ifp,
	    int x,
	    int y,
	    int w,
	    int h)
{
    int mm;
    struct wgl_clip *clp;

    if (w < 0) {
	w = -w;
	x -= w;
    }

    if (h < 0) {
	h = -h;
	y -= h;
    }

    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    wgl_clipper(ifp);
    clp = &(WGL(ifp)->clip);
    glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
    glPixelZoom((float) ifp->if_xzoom, (float) ifp->if_yzoom);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glViewport(0, 0, WGL(ifp)->win_width, WGL(ifp)->win_height);
    wgl_xmit_scanlines(ifp, y, h, x, w);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(mm);

    glFlush();
    return 0;
}


/* This is the ONLY thing that we normally "export" */
FBIO wgl_interface =
{
    0,			/* magic number slot */
    wgl_open,		/* open device */
    wgl_close,		/* close device */
    wgl_clear,		/* clear device */
    wgl_read,		/* read pixels */
    wgl_write,		/* write pixels */
    wgl_rmap,		/* read colormap */
    wgl_wmap,		/* write colormap */
    wgl_view,		/* set view */
    wgl_getview,	/* get view */
    wgl_setcursor,	/* define cursor */
    wgl_cursor,		/* set cursor */
    fb_sim_getcursor,	/* get cursor */
    fb_sim_readrect,	/* read rectangle */
    wgl_writerect,	/* write rectangle */
    fb_sim_bwreadrect,
    wgl_bwwriterect,	/* write rectangle */
    wgl_poll,		/* process events */
    wgl_flush,		/* flush output */
    wgl_free,		/* free resources */
    wgl_help,		/* help message */
    "Microsoft Windows OpenGL",	/* device description */
    XMAXSCREEN+1,	/* max width */
    YMAXSCREEN+1,	/* max height */
    "/dev/wgl",		/* short device name */
    512,		/* default/current width */
    512,		/* default/current height */
    -1,			/* select file desc */
    -1,			/* file descriptor */
    1, 1,		/* zoom */
    256, 256,		/* window center */
    0, 0, 0,		/* cursor */
    PIXEL_NULL,		/* page_base */
    PIXEL_NULL,		/* page_curp */
    PIXEL_NULL,		/* page_endp */
    -1,			/* page_no */
    0,			/* page_dirty */
    0L,			/* page_curpos */
    0L,			/* page_pixels */
    0			/* debug */
};


#else

/* quell empty-compilation unit warnings */
static const int unused = 0;

#endif /* IF_WGL */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

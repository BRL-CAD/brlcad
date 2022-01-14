/*                       I F _ W G L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/** @addtogroup libstruct fb */
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

#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

/* winsock (bsocket.h) first, ordering matters */
#include "bsocket.h"
#include "bio.h"
#include <windowsx.h>

#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif
#undef RED

#include "tk.h"
#include "tkPlatDecls.h"
#include "bu/color.h"
#include "bu/str.h"
#include "bu/snooze.h"
#include "bu/parallel.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "dm.h"
#include "../include/private.h"
#include "./fb_wgl.h"


/* Internal callbacks etc.*/
HIDDEN void wgl_do_event(struct fb *ifp);
HIDDEN void expose_callback(struct fb *ifp, int eventPtr);

/* Other Internal routines */
HIDDEN void fb_clipper(struct fb *ifp);
HIDDEN int wgl_getmem(struct fb *ifp);
HIDDEN void backbuffer_to_screen(struct fb *ifp, int one_y);
HIDDEN void wgl_cminit(struct fb *ifp);
HIDDEN int is_linear_cmap(struct fb *ifp);

HIDDEN int wgl_nwindows = 0; 	/* number of open windows */

extern struct fb wgl_interface;

struct fb *saveifp;
int titleBarHeight = 0;
int borderWidth = 0;


/*
 * Per window state information, overflow area.
 */
struct wininfo {
    short mi_curs_on;
    short mi_cmap_flag;		/* enabled when there is a non-linear map in memory */
    int mi_shmid;
    int mi_memwidth;		/* width of scanline in if_mem */
    short mi_xoff;		/* X viewport offset, rel. window*/
    short mi_yoff;		/* Y viewport offset, rel. window*/
    int mi_doublebuffer;	/* 0=singlebuffer 1=doublebuffer */
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
    struct fb_clip clip;	/* current view clipping */
    Window cursor;
    Colormap xcmap;		/* xstyle color map */
    int use_ext_ctrl;		/* for controlling the Wgl graphics engine externally */
    HDC hdc;
    HWND hwnd;

};


#define WIN(ptr)	((struct wininfo *)((ptr)->i->u1.p))
#define WINL(ptr)	((ptr)->i->u1.p)		/* left hand side version */
#define WGL(ptr)	((struct wglinfo *)((ptr)->i->u6.p))
#define WGLL(ptr)	((ptr)->i->u6.p)		/* left hand side version */
#define if_mem		u2.p			/* shared memory pointer */
#define if_cmap		u3.p			/* color map in shared memory */
#define CMR(x)		((struct fb_cmap *)((x)->i->if_cmap))->cmr
#define CMG(x)		((struct fb_cmap *)((x)->i->if_cmap))->cmg
#define CMB(x)		((struct fb_cmap *)((x)->i->if_cmap))->cmb
#define if_zoomflag	u4.l			/* zoom > 1 */
#define if_mode		u5.l			/* see MODE_* defines */

#define MARGIN 4 /* # pixels margin to screen edge */

#define CLIP_XTRA 1

#define WIN_L (ifp->i->if_max_width - ifp->i->if_width - MARGIN)
#define WIN_T (ifp->i->if_max_height - ifp->i->if_height - MARGIN)

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
#define MODE_15MASK	(1<<14)

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
    { 'D',	MODE_12MASK, MODE_12DELAY_WRITES_TILL_FLUSH,
      "Don't update screen until fb_flush() is called.  (Double buffer sim)" },
    { '\0', 0, 0, "" }
};


HIDDEN int
wgl_getmem(struct fb *ifp)
{
    int pixsize;
    int size;
    int i;
    char *sp;
    int new = 0;

    errno = 0;

    {
	/* In this mode, only malloc as much memory as is needed. */
	WIN(ifp)->mi_memwidth = ifp->i->if_width;
	pixsize = ifp->i->if_height * ifp->i->if_width * sizeof(struct fb_pixel);
	size = pixsize + sizeof(struct fb_cmap);

	sp = calloc(1, size);
	if (sp == 0) {
	    fb_log("wgl_getmem: frame buffer memory malloc failed\n");
	    goto fail;
	}
	new = 1;
	goto success;
    }

success:
    ifp->i->if_mem = sp;
    ifp->i->if_cmap = sp + pixsize;	/* cmap at end of area */
    i = CMB(ifp)[255];			/* try to deref last word */
    CMB(ifp)[255] = i;

    /* Provide non-black colormap on creation of new mem */
    if (new)
	wgl_cminit(ifp);
    return 0;
fail:
    fb_log("wgl_getmem:  Unable to allocate memory.\n");
    if ((sp = calloc(1, size)) == NULL) {
	fb_log("wgl_getmem:  malloc failure\n");
	return -1;
    }
    new = 1;
    goto success;
}


HIDDEN void
sigkid(int UNUSED(pid))
{
    exit(0);
}


/*
 * Note: unlike sgi_xmit_scanlines, this function updates an arbitrary
 * rectangle of the frame buffer
 */
HIDDEN void
wgl_xmit_scanlines(struct fb *ifp, int ybase, int nlines, int xbase, int npix)
{
    int y;
    int n;
    int sw_cmap;	/* !0 => needs software color map */
    struct fb_clip *clp;

    /* Caller is expected to handle attaching context, etc. */

    clp = &(WGL(ifp)->clip);

    if (WGL(ifp)->soft_cmap_flag  && WIN(ifp)->mi_cmap_flag) {
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
	    if (clp->xscrmax >= ifp->i->if_width) glRecti(
		ifp->i->if_width - CLIP_XTRA,
		clp->yscrmin - CLIP_XTRA,
		clp->xscrmax + CLIP_XTRA,
		clp->yscrmax + CLIP_XTRA);

	    /* Blank out area above image */
	    if (clp->yscrmax >= ifp->i->if_height) glRecti(
		clp->xscrmin - CLIP_XTRA,
		ifp->i->if_height- CLIP_XTRA,
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
	struct fb_pixel *wglp;
	struct fb_pixel *scanline;

	y = ybase;

	if (FB_DEBUG)
	    printf("Doing sw colormap xmit\n");

	/* Perform software color mapping into temp scanline */
	scanline = (struct fb_pixel *)calloc(ifp->i->if_width, sizeof(struct fb_pixel));
	if (scanline == NULL) {
	    fb_log("wgl_getmem: scanline memory malloc failed\n");
	    return;
	}

	for (n=nlines; n>0; n--, y++) {
	    wglp = (struct fb_pixel *)&ifp->i->if_mem[(y*WIN(ifp)->mi_memwidth) * sizeof(struct fb_pixel)];
	    for (x=xbase+npix-1; x>=xbase; x--) {
		scanline[x].red   = CMR(ifp)[wglp[x].red];
		scanline[x].green = CMG(ifp)[wglp[x].green];
		scanline[x].blue  = CMB(ifp)[wglp[x].blue];
	    }

	    glPixelStorei(GL_UNPACK_SKIP_PIXELS, xbase);
	    glRasterPos2i(xbase, y);
	    glDrawPixels(npix, 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (const GLvoid *)scanline);
	}

	(void)free((void *)scanline);

    } else {
	/* No need for software colormapping */

	glPixelStorei(GL_UNPACK_ROW_LENGTH, WIN(ifp)->mi_memwidth);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, xbase);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, ybase);

	glRasterPos2i(xbase, ybase);
	glDrawPixels(npix, nlines, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
		     (const GLvoid *) ifp->i->if_mem);

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
	    WGL(saveifp)->alive = 0;
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
		struct fb_pixel *wglp;

		x = GET_X_LPARAM(lParam);
		y = saveifp->i->if_height - GET_Y_LPARAM(lParam) - 1;

		if (x < 0 || y < 0) {
		    fb_log("No RGB (outside image viewport)\n");
		    break;
		}

		wglp = (struct fb_pixel *)&saveifp->i->if_mem[
		    (y*WIN(saveifp)->mi_memwidth)*
		    sizeof(struct fb_pixel)];

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
	    /* WIP: unimplemented, intentional fall through */
	default:
	    return DefWindowProc (hWnd, uMsg, wParam, lParam);
    }

    return 1;
}

/*
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
 * WIN(ifp)->mi_doublebuffer
 * WGL(ifp)->soft_cmap_flag
 *
 * Return 0 on failure.
 */
static int
wgl_choose_visual(struct fb *ifp)
{
	int iPixelFormat;
	PIXELFORMATDESCRIPTOR pfd;
	BOOL good;

	iPixelFormat = GetPixelFormat(WGL(ifp)->hdc);
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_TYPE_RGBA | PFD_STEREO;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cRedBits = 0;
	pfd.cRedShift = 0;
	pfd.cGreenBits = 0;
	pfd.cGreenShift = 0;
	pfd.cBlueBits = 0;
	pfd.cBlueShift = 0;
	pfd.cAlphaBits = 0;
	pfd.cAlphaShift = 0;
	pfd.cAccumBits = 0;
	pfd.cAccumRedBits = 0;
	pfd.cAccumGreenBits = 0;
	pfd.cAccumBlueBits = 0;
	pfd.cAccumAlphaBits = 0;
	pfd.cDepthBits = 32;
	pfd.cStencilBits = 0;
	pfd.cAuxBuffers = 0;
	pfd.iLayerType = PFD_MAIN_PLANE;
	pfd.bReserved = 0;
	pfd.dwLayerMask = 0;
	pfd.dwVisibleMask = 0;
	pfd.dwDamageMask = 0;

	iPixelFormat = ChoosePixelFormat(WGL(ifp)->hdc, &pfd);
	good = SetPixelFormat(WGL(ifp)->hdc, iPixelFormat, &pfd);

	WIN(ifp)->mi_doublebuffer = 1;
	WGL(ifp)->soft_cmap_flag = 1;

	if (good)
		return 1;
	return 0;
}


static int
wgl_open(struct fb *ifp, const char *file, int width, int height)
{
    static char title[128];
    int mode,  ret;
    HWND hwnd;
    HDC hdc;
    HGLRC glxc;
    HINSTANCE hinstance;
    DWORD Dword;
    WNDCLASS wndclass;
    int gotvisual;

    FB_CK_FB(ifp->i);

    saveifp = ifp;

    /*
     * First, attempt to determine operating mode for this open, based
     * upon the "unit number" or flags.  file = "/dev/wgl###"
     */
    mode = MODE_2LINGERING | MODE_12DELAY_WRITES_TILL_FLUSH;

    if (file != NULL) {
	const char *cp;
	char modebuf[80];
	char *mp;
	int alpha;
	struct modeflags *mfp;

	if (bu_strncmp(file, ifp->i->if_name, strlen(ifp->i->if_name))) {
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
    }
    ifp->i->if_mode = mode;

    /*
     * Allocate extension memory sections, addressed by
     * WIN(ifp)->mi_xxx and WGL(ifp)->xxx
     */

    if ((WINL(ifp) = (char *)calloc(1, sizeof(struct wininfo))) == NULL) {
	fb_log("wgl_open:  wininfo malloc failed\n");
	return -1;
    }
    if ((WGLL(ifp) = (char *)calloc(1, sizeof(struct wglinfo))) == NULL) {
	fb_log("wgl_open:  wglinfo malloc failed\n");
	return -1;
    }

    WIN(ifp)->mi_shmid = -1;	/* indicate no shared memory */

    /* use defaults if invalid width and height specified */
    if (width <= 0)
	width = ifp->i->if_width;
    if (height <= 0)
	height = ifp->i->if_height;
    /* use max values if width and height are greater */
    if (width > ifp->i->if_max_width)
	width = ifp->i->if_max_width;
    if (height > ifp->i->if_max_height)
	height = ifp->i->if_max_height;

    ifp->i->if_width = width;
    ifp->i->if_height = height;

    WIN(ifp)->mi_curs_on = 1;

    /* Build a descriptive window title bar */
    (void)snprintf(title, 128, "BRL-CAD /dev/wgl %s, %s",
		   ((ifp->i->if_mode & MODE_2MASK) == MODE_2TRANSIENT) ?
		   "Transient Win":
		   "Lingering Win",
		   ((ifp->i->if_mode & MODE_1MASK) == MODE_1MALLOC) ?
		   "Private Mem" :
		   "Shared Mem");

    /* initialize window state variables before calling wgl_getmem */
    ifp->i->if_zoomflag = 0;
    ifp->i->if_xzoom = 1;	/* for zoom fakeout */
    ifp->i->if_yzoom = 1;	/* for zoom fakeout */
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    /* Allocate memory, potentially with a screen repaint */
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
	ifp->i->if_width + 2 * borderWidth, /* window width */
	ifp->i->if_height + titleBarHeight + 2 * borderWidth, /* window height */
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
    gotvisual = wgl_choose_visual(ifp);
    if (!gotvisual) {
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


HIDDEN struct fb_platform_specific *
wgl_get_fbps(uint32_t magic)
{
    struct fb_platform_specific *fb_ps = NULL;
    struct wgl_fb_info *data = NULL;
    BU_GET(fb_ps, struct fb_platform_specific);
    BU_GET(data, struct wgl_fb_info);
    fb_ps->magic = magic;
    fb_ps->data = data;
    return fb_ps;
}


HIDDEN void
wgl_put_fbps(struct fb_platform_specific *fbps)
{
    BU_CKMAG(fbps, FB_WGL_MAGIC, "wgl framebuffer");
    BU_PUT(fbps->data, struct wgl_fb_info);
    BU_PUT(fbps, struct fb_platform_specific);
    return;
}


HIDDEN int
open_existing(struct fb *ifp,
	      Display *dpy,
	      Window win,
	      Colormap cmap,
	      HDC hdc,
	      int width,
	      int height,
	      HGLRC glxc,
	      int double_buffer,
	      int soft_cmap)
{

    /* XXX for now use private memory */
    ifp->i->if_mode = MODE_1MALLOC;

    /*
     * Allocate extension memory sections, addressed by
     * WIN(ifp)->mi_xxx and WGL(ifp)->xxx
     */

    if ((WINL(ifp) = (char *)calloc(1, sizeof(struct wininfo))) == NULL) {
	fb_log("wgl_open:  wininfo malloc failed\n");
	return -1;
    }
    if ((WGLL(ifp) = (char *)calloc(1, sizeof(struct wglinfo))) == NULL) {
	fb_log("wgl_open:  wglinfo malloc failed\n");
	return -1;
    }

    WGL(ifp)->use_ext_ctrl = 1;

    WIN(ifp)->mi_shmid = -1;	/* indicate no shared memory */
    ifp->i->if_width = ifp->i->if_max_width = width;
    ifp->i->if_height = ifp->i->if_max_height = height;

    WGL(ifp)->win_width = WGL(ifp)->vp_width = width;
    WGL(ifp)->win_height = WGL(ifp)->vp_height = height;

    WIN(ifp)->mi_curs_on = 1;

    /* initialize window state variables before calling wgl_getmem */
    ifp->i->if_zoomflag = 0;
    ifp->i->if_xzoom = 1;	/* for zoom fakeout */
    ifp->i->if_yzoom = 1;	/* for zoom fakeout */
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    /* Allocate memory, potentially with a screen repaint */
    if (wgl_getmem(ifp) < 0)
	return -1;

    WGL(ifp)->dispp = dpy;

    WGL(ifp)->glxc = glxc;
    WIN(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);
    WGL(ifp)->soft_cmap_flag = soft_cmap;
    WIN(ifp)->mi_doublebuffer = double_buffer;
    WGL(ifp)->xcmap = cmap;

    WGL(ifp)->wind = win;
    WGL(ifp)->hdc = hdc;
    ++wgl_nwindows;

    WGL(ifp)->alive = 1;
    WGL(ifp)->firstTime = 1;

    fb_clipper(ifp);

    return 0;
}


int
wgl_open_existing(struct fb *ifp, int width, int height, struct fb_platform_specific *fb_p)
{
    struct wgl_fb_info *wgl_internal = (struct wgl_fb_info *)fb_p->data;
    BU_CKMAG(fb_p, FB_WGL_MAGIC, "wgl framebuffer");
    return open_existing(ifp, wgl_internal->dpy, wgl_internal->win, wgl_internal->cmap,
			 wgl_internal->hdc, width, height,
			 wgl_internal->glxc, wgl_internal->double_buffer, wgl_internal->soft_cmap);
    return 0;
}


HIDDEN int
wgl_final_close(struct fb *ifp)
{
    if (FB_DEBUG)
	printf("wgl_final_close: All done...goodbye!\n");

    if (WGL(ifp)->glxc) {
	wglDeleteContext(WGL(ifp)->glxc);
    }
    if (WGL(ifp)->hdc) {
	ReleaseDC(WGL(ifp)->hwnd, WGL(ifp)->hdc);
    }
    DestroyWindow(WGL(ifp)->hwnd);

    if (WINL(ifp) != NULL) {
	/* free up memory associated with image */

	/* free private memory */
	(void)free(ifp->i->if_mem);
	/* free state information */
	(void)free((char *)WINL(ifp));
	WINL(ifp) = NULL;
    }

    if (WGLL(ifp) != NULL) {
	(void) free((char *)WGLL(ifp));
	WGLL(ifp) = NULL;
    }

    wgl_nwindows--;
    return 0;
}


HIDDEN int
wgl_flush(struct fb *ifp)
{
    if ((ifp->i->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH) {

	if (wglMakeCurrent(WGL(ifp)->hdc, WGL(ifp)->glxc)==False) {
	    fb_log("Warning, wgl_flush: wglMakeCurrent unsuccessful.\n");
	}

	/* Send entire in-memory buffer to the screen, all at once */
	wgl_xmit_scanlines(ifp, 0, ifp->i->if_height, 0, ifp->i->if_width);
	if (WIN(ifp)->mi_doublebuffer) {
	    SwapBuffers(WGL(ifp)->hdc);
	} else {
	    if (WGL(ifp)->copy_flag) {
		backbuffer_to_screen(ifp, -1);
	    }
	}

	/* unattach context for other threads to use, also flushes */
	wglMakeCurrent(NULL, NULL);
    }
    glFlush();
    return 0;
}


/*
 * Handle any pending input events
 */
HIDDEN int
wgl_poll(struct fb *ifp)
{
    wgl_do_event(ifp);

    if (WGL(ifp)->alive)
	return 0;
    return 1;
}


static int
wgl_close(struct fb *ifp)
{

    wgl_flush(ifp);

    /* only the last open window can linger -
     * call final_close if not lingering
     */
    if (wgl_nwindows > 1 ||
	(ifp->i->if_mode & MODE_2MASK) == MODE_2TRANSIENT)
	return wgl_final_close(ifp);

    if (FB_DEBUG)
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

    while (!wgl_poll(ifp)) {
	bu_snooze(fb_poll_rate(ifp));
    }

    return 0;
}


int
wgl_close_existing(struct fb *ifp)
{
    /*
      if (WGL(ifp)->cursor)
      XDestroyWindow(WGL(ifp)->dispp, WGL(ifp)->cursor);
    */

    if (WINL(ifp) != NULL) {
	/* free up memory associated with image */
	/* free private memory */
	(void)free(ifp->i->if_mem);
	/* free state information */
	(void)free((char *)WINL(ifp));
	WINL(ifp) = NULL;
    }

    if (WGLL(ifp) != NULL) {
	(void) free((char *)WGLL(ifp));
	WGLL(ifp) = NULL;
    }

    return 0;
}


/*
 * Free memory resources, and close.
 */
HIDDEN int
wgl_free(struct fb *ifp)
{
    int ret;

    if (FB_DEBUG)
	printf("entering wgl_free\n");

    /* Close the framebuffer */
    ret = wgl_final_close(ifp);

    return ret;
}


/**
 * pp is a pointer to beginning of memory segment
 */
HIDDEN int
wgl_clear(struct fb *ifp, unsigned char *pp)
{
    struct fb_pixel bg;
    struct fb_pixel *wglp;
    int cnt;
    int y;

    if (FB_DEBUG)
	printf("entering wgl_clear\n");

    /* Set clear colors */
    if (pp != RGBPIXEL_NULL) {
	bg.alpha = 0;
	bg.red   = (pp)[RED];
	bg.green = (pp)[GRN];
	bg.blue  = (pp)[BLU];
    } else {
	bg.alpha = 0;
	bg.red   = 0;
	bg.green = 0;
	bg.blue  = 0;
    }

    /* Flood rectangle in memory */
    for (y=0; y < ifp->i->if_height; y++) {
	wglp = (struct fb_pixel *)&ifp->i->if_mem[
	    (y*WIN(ifp)->mi_memwidth+0)*sizeof(struct fb_pixel) ];
	for (cnt=ifp->i->if_width-1; cnt >= 0; cnt--) {
	    *wglp++ = bg;	/* struct copy */
	}
    }

    if (WGL(ifp)->use_ext_ctrl) {
	return 0;
    }

    if (wglMakeCurrent(WGL(ifp)->hdc, WGL(ifp)->glxc)==False) {
	fb_log("Warning, wgl_clear: wglMakeCurrent unsuccessful.\n");
    }

    if (pp != RGBPIXEL_NULL) {
	glClearColor(pp[RED]/255.0, pp[GRN]/255.0, pp[BLU]/255.0, 0.0);
    } else {
	glClearColor(0, 0, 0, 0);
    }

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
	if (WIN(ifp)->mi_doublebuffer) {
	    SwapBuffers(WGL(ifp)->hdc);
	}
    }

    /* unattach context for other threads to use */
    wglMakeCurrent(NULL, NULL);

    return 0;
}


HIDDEN int
wgl_view(struct fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    struct fb_clip *clp;

    if (FB_DEBUG)
	printf("entering wgl_view\n");

    if (xzoom < 1) xzoom = 1;
    if (yzoom < 1) yzoom = 1;
    if (ifp->i->if_xcenter == xcenter && ifp->i->if_ycenter == ycenter
	&& ifp->i->if_xzoom == xzoom && ifp->i->if_yzoom == yzoom)
	return 0;

    if (xcenter < 0 || xcenter >= ifp->i->if_width)
	return -1;
    if (ycenter < 0 || ycenter >= ifp->i->if_height)
	return -1;
    if (xzoom >= ifp->i->if_width || yzoom >= ifp->i->if_height)
	return -1;

    ifp->i->if_xcenter = xcenter;
    ifp->i->if_ycenter = ycenter;
    ifp->i->if_xzoom = xzoom;
    ifp->i->if_yzoom = yzoom;

    if (ifp->i->if_xzoom > 1 || ifp->i->if_yzoom > 1)
	ifp->i->if_zoomflag = 1;
    else ifp->i->if_zoomflag = 0;


    if (WGL(ifp)->use_ext_ctrl) {
	fb_clipper(ifp);
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

	fb_clipper(ifp);
	clp = &(WGL(ifp)->clip);
	glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
	glPixelZoom((float) ifp->i->if_xzoom, (float) ifp->i->if_yzoom);

	if (WGL(ifp)->copy_flag) {
	    backbuffer_to_screen(ifp, -1);
	} else {
	    wgl_xmit_scanlines(ifp, 0, ifp->i->if_height, 0, ifp->i->if_width);
	    if (WIN(ifp)->mi_doublebuffer) {
		SwapBuffers(WGL(ifp)->hdc);
	    }
	}

	/* unattach context for other threads to use */
	wglMakeCurrent(NULL, NULL);
    }

    return 0;
}


HIDDEN int
wgl_getview(struct fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    if (FB_DEBUG)
	printf("entering wgl_getview\n");

    *xcenter = ifp->i->if_xcenter;
    *ycenter = ifp->i->if_ycenter;
    *xzoom = ifp->i->if_xzoom;
    *yzoom = ifp->i->if_yzoom;

    return 0;
}


/*
 * read count pixels into pixelp starting at x, y
 */
HIDDEN int
wgl_read(struct fb *ifp, int x, int y, unsigned char *pixelp, size_t count)
{
    size_t n;
    size_t scan_count;	/* # pix on this scanline */
    unsigned char *cp;
    int ret;
    struct fb_pixel *wglp;

    if (FB_DEBUG)
	printf("entering wgl_read\n");

    if (x < 0 || x >= ifp->i->if_width ||
	y < 0 || y >= ifp->i->if_height)
	return -1;

    ret = 0;
    cp = (unsigned char *)(pixelp);

    while (count) {
	if (y >= ifp->i->if_height)
	    break;

	if (count >= (size_t)(ifp->i->if_width-x))
	    scan_count = ifp->i->if_width-x;
	else
	    scan_count = count;

	wglp = (struct fb_pixel *)&ifp->i->if_mem[
	    (y*WIN(ifp)->mi_memwidth+x)*sizeof(struct fb_pixel) ];

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
	if (++y >= ifp->i->if_height)
	    break;
    }
    return ret;
}


/*
 * write count pixels from pixelp starting at xstart, ystart
 */
HIDDEN int
wgl_write(struct fb *ifp, int xstart, int ystart, const unsigned char *pixelp, size_t count)
{
    size_t scan_count;	/* # pix on this scanline */
    size_t pix_count;	/* # pixels to send */
    unsigned char *cp;
    int ret;
    int ybase;
    int x;
    int y;

    if (FB_DEBUG)
	printf("entering wgl_write\n");

    /* fast exit cases */
    pix_count = count;
    if (pix_count == 0)
	return 0;	/* OK, no pixels transferred */

    x = xstart;
    ybase = y = ystart;

    if (x < 0 || x >= ifp->i->if_width ||
	y < 0 || y >= ifp->i->if_height)
	return -1;

    ret = 0;
    cp = (unsigned char *)(pixelp);

    while (pix_count) {
	unsigned int n;
	struct fb_pixel *wglp;

	if (y >= ifp->i->if_height)
	    break;

	if (pix_count >= (size_t)(ifp->i->if_width-x))
	    scan_count = ifp->i->if_width-x;
	else
	    scan_count = pix_count;

	wglp = (struct fb_pixel *)&ifp->i->if_mem[
	    (y*WIN(ifp)->mi_memwidth+x)*sizeof(struct fb_pixel) ];

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
	if (++y >= ifp->i->if_height)
	    break;
    }

    if ((ifp->i->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH)
	return ret;

    if (!WGL(ifp)->use_ext_ctrl) {

	if (wglMakeCurrent(WGL(ifp)->hdc, WGL(ifp)->glxc)==False) {
	    fb_log("Warning, wgl_write: wglMakeCurrent unsuccessful.\n");
	}

	if (xstart + count <= (size_t)ifp->i->if_width) {
	    wgl_xmit_scanlines(ifp, ybase, 1, xstart, count);
	    if (WIN(ifp)->mi_doublebuffer) {
		SwapBuffers(WGL(ifp)->hdc);
	    } else if (WGL(ifp)->copy_flag) {
		/* repaint one scanline from backbuffer */
		backbuffer_to_screen(ifp, ybase);
	    }
	} else {
	    /* Normal case -- multi-pixel write */
	    if (WIN(ifp)->mi_doublebuffer) {
		/* refresh whole screen */
		wgl_xmit_scanlines(ifp, 0, ifp->i->if_height, 0, ifp->i->if_width);
		SwapBuffers(WGL(ifp)->hdc);
	    } else {
		/* just write rectangle */
		wgl_xmit_scanlines(ifp, ybase, y-ybase, 0, ifp->i->if_width);
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
 * The task of this routine is to reformat the pixels into internal
 * form, and then arrange to have them sent to the screen separately.
 */
HIDDEN int
wgl_writerect(struct fb *ifp,
	      int xmin,
	      int ymin,
	      int width,
	      int height,
	      const unsigned char *pp)
{
    int x;
    int y;
    unsigned char *cp;
    struct fb_pixel *wglp;

    if (FB_DEBUG)
	printf("entering wgl_writerect\n");


    if (width <= 0 || height <= 0)
	return 0;  /* do nothing */
    if (xmin < 0 || xmin+width > ifp->i->if_width ||
	ymin < 0 || ymin+height > ifp->i->if_height)
	return -1; /* no can do */

    cp = (unsigned char *)(pp);
    for (y = ymin; y < ymin+height; y++) {
	wglp = (struct fb_pixel *)&ifp->i->if_mem[
	    (y*WIN(ifp)->mi_memwidth+xmin)*sizeof(struct fb_pixel) ];
	for (x = xmin; x < xmin+width; x++) {
	    /* alpha channel is always zero */
	    wglp->red   = cp[RED];
	    wglp->green = cp[GRN];
	    wglp->blue  = cp[BLU];
	    wglp++;
	    cp += 3;
	}
    }

    if ((ifp->i->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH)
	return width*height;

    if (!WGL(ifp)->use_ext_ctrl) {
	if (wglMakeCurrent(WGL(ifp)->hdc, WGL(ifp)->glxc)==False) {
	    fb_log("Warning, wgl_writerect: wglMakeCurrent unsuccessful.\n");
	}

	if (WIN(ifp)->mi_doublebuffer) {
	    /* refresh whole screen */
	    wgl_xmit_scanlines(ifp, 0, ifp->i->if_height, 0, ifp->i->if_width);
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
 * The task of this routine is to reformat the pixels into internal
 * form, and then arrange to have them sent to the screen separately.
 */
HIDDEN int
wgl_bwwriterect(struct fb *ifp,
		int xmin,
		int ymin,
		int width,
		int height,
		const unsigned char *pp)
{
    int x;
    int y;
    unsigned char *cp;
    struct fb_pixel *wglp;

    if (FB_DEBUG)
	printf("entering wgl_bwwriterect\n");


    if (width <= 0 || height <= 0)
	return 0;  /* do nothing */
    if (xmin < 0 || xmin+width > ifp->i->if_width ||
	ymin < 0 || ymin+height > ifp->i->if_height)
	return -1; /* no can do */

    cp = (unsigned char *)(pp);
    for (y = ymin; y < ymin+height; y++) {
	wglp = (struct fb_pixel *)&ifp->i->if_mem[
	    (y*WIN(ifp)->mi_memwidth+xmin)*sizeof(struct fb_pixel) ];
	for (x = xmin; x < xmin+width; x++) {
	    int val;
	    /* alpha channel is always zero */
	    wglp->red   = (val = *cp++);
	    wglp->green = val;
	    wglp->blue  = val;
	    wglp++;
	}
    }

    if ((ifp->i->if_mode & MODE_12MASK) == MODE_12DELAY_WRITES_TILL_FLUSH)
	return width*height;

    if (!WGL(ifp)->use_ext_ctrl) {
	if (wglMakeCurrent(WGL(ifp)->hdc, WGL(ifp)->glxc)==False) {
	    fb_log("Warning, wgl_writerect: wglMakeCurrent unsuccessful.\n");
	}

	if (WIN(ifp)->mi_doublebuffer) {
	    /* refresh whole screen */
	    wgl_xmit_scanlines(ifp, 0, ifp->i->if_height, 0, ifp->i->if_width);
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
wgl_rmap(struct fb *ifp, ColorMap *cmp)
{
    int i;

    if (FB_DEBUG)
	printf("entering wgl_rmap\n");

    /* Just parrot back the stored colormap */
    for (i = 0; i < 256; i++) {
	cmp->cm_red[i]   = CMR(ifp)[i]<<8;
	cmp->cm_green[i] = CMG(ifp)[i]<<8;
	cmp->cm_blue[i]  = CMB(ifp)[i]<<8;
    }
    return 0;
}


/*
 * Check for a color map being linear in R, G, and B.  Returns 1 for
 * linear map, 0 for non-linear map (i.e., non-identity map).
 */
HIDDEN int
is_linear_cmap(struct fb *ifp)
{
    int i;

    for (i=0; i<256; i++) {
	if (CMR(ifp)[i] != i) return 0;
	if (CMG(ifp)[i] != i) return 0;
	if (CMB(ifp)[i] != i) return 0;
    }
    return 1;
}


HIDDEN void
wgl_cminit(struct fb *ifp)
{
    int i;

    for (i = 0; i < 256; i++) {
	CMR(ifp)[i] = i;
	CMG(ifp)[i] = i;
	CMB(ifp)[i] = i;
    }
}


HIDDEN int
wgl_wmap(struct fb *ifp, const ColorMap *cmp)
{
    int i;
    int prev;	/* !0 = previous cmap was non-linear */

    if (FB_DEBUG)
	printf("entering wgl_wmap\n");

    prev = WIN(ifp)->mi_cmap_flag;
    if (cmp == COLORMAP_NULL) {
	wgl_cminit(ifp);
    } else {
	for (i = 0; i < 256; i++) {
	    CMR(ifp)[i] = cmp-> cm_red[i]>>8;
	    CMG(ifp)[i] = cmp-> cm_green[i]>>8;
	    CMB(ifp)[i] = cmp-> cm_blue[i]>>8;
	}
    }
    WIN(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);


    if (!WGL(ifp)->use_ext_ctrl) {
	if (WGL(ifp)->soft_cmap_flag) {
	    /* if current and previous maps are linear, return */
	    if (WIN(ifp)->mi_cmap_flag == 0 && prev == 0) return 0;

	    /* Software color mapping, trigger a repaint */

	    if (wglMakeCurrent(WGL(ifp)->hdc, WGL(ifp)->glxc)==False) {
		fb_log("Warning, wgl_wmap: wglMakeCurrent unsuccessful.\n");
	    }

	    wgl_xmit_scanlines(ifp, 0, ifp->i->if_height, 0, ifp->i->if_width);
	    if (WIN(ifp)->mi_doublebuffer) {
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


HIDDEN int
wgl_help(struct fb *ifp)
{
    struct modeflags *mfp;

    fb_log("Description: %s\n", ifp->i->if_type);
    fb_log("Device: %s\n", ifp->i->if_name);
    fb_log("Max width height: %d %d\n",
	   ifp->i->if_max_width,
	   ifp->i->if_max_height);
    fb_log("Default width height: %d %d\n",
	   ifp->i->if_width,
	   ifp->i->if_height);
    fb_log("Usage: /dev/wgl[option letters]\n");
    for (mfp = modeflags; mfp->c != '\0'; mfp++) {
	fb_log("   %c   %s\n", mfp->c, mfp->help);
    }

    fb_log("\nCurrent internal state:\n");
    fb_log("	mi_doublebuffer=%d\n", WIN(ifp)->mi_doublebuffer);
    fb_log("	mi_cmap_flag=%d\n", WIN(ifp)->mi_cmap_flag);
    fb_log("	wgl_nwindows=%d\n", wgl_nwindows);

    return 0;
}


HIDDEN int
wgl_setcursor(struct fb *ifp,
	      const unsigned char *bits,
	      int xbits,
	      int ybits,
	      int xorig,
	      int yorig)
{
    return 0;
}


HIDDEN int
wgl_cursor(struct fb *ifp, int mode, int x, int y)
{
    return 0;
}


/*
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
fb_clipper(struct fb *ifp)
{
    struct fb_clip *clp;
    int i;
    double pixels;

    clp = &(WGL(ifp)->clip);

    i = WGL(ifp)->vp_width/(2*ifp->i->if_xzoom);
    clp->xscrmin = ifp->i->if_xcenter - i;
    i = WGL(ifp)->vp_width/ifp->i->if_xzoom;
    clp->xscrmax = clp->xscrmin + i;
    pixels = (double) i;
    clp->oleft = ((double) clp->xscrmin) - 0.25*pixels/((double) WGL(ifp)->vp_width);
    clp->oright = clp->oleft + pixels;

    i = WGL(ifp)->vp_height/(2*ifp->i->if_yzoom);
    clp->yscrmin = ifp->i->if_ycenter - i;
    i = WGL(ifp)->vp_height/ifp->i->if_yzoom;
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
	if (clp->xpixmax > ifp->i->if_width-1) {
	    clp->xpixmax = ifp->i->if_width-1;
	}
	if (clp->ypixmax > ifp->i->if_height-1) {
	    clp->ypixmax = ifp->i->if_height-1;
	}
    }

}


/********************************/
/* Call back routines and so on */
/********************************/

HIDDEN void
wgl_do_event(struct fb *ifp)
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
expose_callback(struct fb *ifp, int eventPtr)
{
    struct fb_clip *clp;

    if (FB_DEBUG)
	fb_log("entering expose_callback()\n");

    if (wglMakeCurrent(WGL(ifp)->hdc, WGL(ifp)->glxc) == False) {
	fb_log("Warning, libfb/expose_callback: wglMakeCurrent unsuccessful.\n");
    }

    if (WGL(ifp)->firstTime) {
	WGL(ifp)->firstTime = 0;

	/* just in case the configuration is double buffered but
	 * we want to pretend it's not
	 */

	if (!WIN(ifp)->mi_doublebuffer) {
	    glDrawBuffer(GL_FRONT);
	}

	if ((ifp->i->if_mode & MODE_4MASK) == MODE_4NODITH) {
	    glDisable(GL_DITHER);
	}

	/* set copy mode if possible and requested */
	if (WIN(ifp)->mi_doublebuffer &&
	    ((ifp->i->if_mode & MODE_11MASK)==MODE_11COPY)) {
	    /* Copy mode only works if there are two buffers to
	     * use. It conflicts with double buffering
	     */
	    WGL(ifp)->copy_flag = 1;
	    WIN(ifp)->mi_doublebuffer = 0;
	    WGL(ifp)->front_flag = 1;
	    glDrawBuffer(GL_FRONT);
	} else {
	    WGL(ifp)->copy_flag = 0;
	}

	WGL(ifp)->win_width=ifp->i->if_width;
	WGL(ifp)->win_height=ifp->i->if_height;

	/* clear entire window */
	glViewport(0, 0, WGL(ifp)->win_width, WGL(ifp)->win_height);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	/* Set normal viewport size to minimum of actual window size
	 * and requested framebuffer size
	 */
	WGL(ifp)->vp_width = (WGL(ifp)->win_width < ifp->i->if_width) ?
	    WGL(ifp)->win_width : ifp->i->if_width;
	WGL(ifp)->vp_height = (WGL(ifp)->win_height < ifp->i->if_height) ?
	    WGL(ifp)->win_height : ifp->i->if_height;
	ifp->i->if_xcenter = WGL(ifp)->vp_width/2;
	ifp->i->if_ycenter = WGL(ifp)->vp_height/2;

	/* center viewport in window */
	WIN(ifp)->mi_xoff=(WGL(ifp)->win_width-WGL(ifp)->vp_width)/2;
	WIN(ifp)->mi_yoff=(WGL(ifp)->win_height-WGL(ifp)->vp_height)/2;
	glViewport(WIN(ifp)->mi_xoff,
		   WIN(ifp)->mi_yoff,
		   WGL(ifp)->vp_width,
		   WGL(ifp)->vp_height);
	/* initialize clipping planes and zoom */
	fb_clipper(ifp);
	clp = &(WGL(ifp)->clip);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop,
		-1.0, 1.0);
	glPixelZoom((float) ifp->i->if_xzoom, (float) ifp->i->if_yzoom);
    } else if ((WGL(ifp)->win_width > ifp->i->if_width) ||
	       (WGL(ifp)->win_height > ifp->i->if_height)) {
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
	glViewport(WIN(ifp)->mi_xoff,
		   WIN(ifp)->mi_yoff,
		   WGL(ifp)->vp_width,
		   WGL(ifp)->vp_height);
    }

    /* repaint entire image */
    wgl_xmit_scanlines(ifp, 0, ifp->i->if_height, 0, ifp->i->if_width);
    if (WIN(ifp)->mi_doublebuffer) {
	SwapBuffers(WGL(ifp)->hdc);
    } else if (WGL(ifp)->copy_flag) {
	backbuffer_to_screen(ifp, -1);
    }

    if (FB_DEBUG) {
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


int
wgl_configureWindow(struct fb *ifp, int width, int height)
{
    if (width == WGL(ifp)->win_width &&
	height == WGL(ifp)->win_height)
	return 1;

    ifp->i->if_width = ifp->i->if_max_width = width;
    ifp->i->if_height = ifp->i->if_max_height = height;

    WGL(ifp)->win_width = WGL(ifp)->vp_width = width;
    WGL(ifp)->win_height = WGL(ifp)->vp_height = height;

    ifp->i->if_zoomflag = 0;
    ifp->i->if_xzoom = 1;
    ifp->i->if_yzoom = 1;
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    wgl_getmem(ifp);
    fb_clipper(ifp);

    return 0;
}


/* BACKBUFFER_TO_SCREEN - copy pixels from copy on the backbuffer to
 * the front buffer. Do one scanline specified by one_y, or whole
 * screen if one_y equals -1.
 */
HIDDEN void
backbuffer_to_screen(struct fb *ifp, int one_y)
{
    struct fb_clip *clp;

    if (!(WGL(ifp)->front_flag)) {
	WGL(ifp)->front_flag = 1;
	glDrawBuffer(GL_FRONT);
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPixelZoom((float) ifp->i->if_xzoom, (float) ifp->i->if_yzoom);
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
	    ifp->i->if_width - CLIP_XTRA,
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
	glCopyPixels(WIN(ifp)->mi_xoff + clp->xpixmin,
		     WIN(ifp)->mi_yoff + clp->ypixmin,
		     clp->xpixmax - clp->xpixmin +1,
		     clp->ypixmax - clp->ypixmin +1,
		     GL_COLOR);


    } else if (one_y < clp->ypixmin) {
	return;
    } else {
	/* draw one scanline */
	glRasterPos2i(clp->xpixmin, one_y);
	glCopyPixels(WIN(ifp)->mi_xoff + clp->xpixmin,
		     WIN(ifp)->mi_yoff + one_y,
		     clp->xpixmax - clp->xpixmin +1,
		     1,
		     GL_COLOR);
    }
}




int
wgl_refresh(struct fb *ifp,
	    int x,
	    int y,
	    int w,
	    int h)
{
    int mm;
    struct fb_clip *clp;

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

    fb_clipper(ifp);
    clp = &(WGL(ifp)->clip);
    glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
    glPixelZoom((float) ifp->i->if_xzoom, (float) ifp->i->if_yzoom);

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

    if (!WGL(ifp)->use_ext_ctrl) {
	glFlush();
    }

    return 0;
}


/* This is the ONLY thing that we normally "export" */
struct fb_impl wgl_interface_impl = {
    0,			/* magic number slot */
    FB_WGL_MAGIC,
    wgl_open,		/* open device */
    wgl_open_existing,
    wgl_close_existing,
    wgl_get_fbps,
    wgl_put_fbps,
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
    wgl_configureWindow,
    wgl_refresh,
    wgl_poll,		/* process events */
    wgl_flush,		/* flush output */
    wgl_free,		/* free resources */
    wgl_help,		/* help message */
    "Microsoft Windows OpenGL",	/* device description */
    FB_XMAXSCREEN,	/* max width */
    FB_YMAXSCREEN,	/* max height */
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
    0,			/* debug */
    50000,		/* refresh rate */
    NULL,
    NULL,
    0,
    NULL,
    {0}, /* u1 */
    {0}, /* u2 */
    {0}, /* u3 */
    {0}, /* u4 */
    {0}, /* u5 */
    {0}  /* u6 */
};

struct fb wgl_interface = { &wgl_interface_impl };

#ifdef DM_PLUGIN
static const struct fb_plugin finfo = { &wgl_interface };

COMPILER_DLLEXPORT const struct fb_plugin *fb_plugin_info()
{
    return &finfo;
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

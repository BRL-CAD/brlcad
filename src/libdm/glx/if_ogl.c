/*                        I F _ O G L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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
/** @file if_ogl.c
 *
 * Frame Buffer Library interface for OpenGL.
 *
 * Modernization note (2025):
 * The legacy glDrawPixels / glCopyPixels based blitting pipeline (including
 * the historic “copy” backbuffer pan/zoom mode) has been removed and
 * replaced with a persistent texture + screen‑aligned quad presentation
 * strategy:
 *
 *   1. All pixel modifications update the CPU backing store (shared or
 *      private memory as before).
 *   2. Modified pixel regions accumulate into a single dirty rectangle.
 *   3. On flush (or immediate mode) the dirty region is uploaded with
 *      glTexSubImage2D and the view is presented by drawing a textured quad
 *      covering the viewport (accounting for zoom/pan).
 *
 * Benefits:
 *   - Pan/zoom no longer needs backbuffer copies or glPixelZoom – it is purely
 *     coordinate math when drawing the textured quad.
 *   - Avoids poorly accelerated legacy raster operations on modern drivers.
 *   - Provides a foundation for future shader/palette GPU paths if desired
 *     (still staying within fixed‑function for current compatibility).
 *
 * MODE_11COPY is retained (and still displayed) for compatibility with
 * scripts, but it is now a deprecated no‑op. Historically it enabled a
 * “fast” pan/zoom by copying from an off‑screen backbuffer; the texture
 * approach makes this optimization unnecessary.
 */
/** @} */

#include "common.h"

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include <sys/ipc.h>
#include <sys/shm.h>

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>

#define j1 J1
#define y1 Y1
#define read rd
#define index idx
#define access acs
#define remainder rem
#ifdef HAVE_GL_GLX_H
#  define class REDEFINE_CLASS_STRING_TO_AVOID_CXX_CONFLICT
#  include <GL/glx.h>
#  ifdef HAVE_XRENDER
#    include <X11/extensions/Xrender.h>
#  endif
#endif
#undef remainder
#undef access
#undef index
#undef read
#undef y1
#undef j1
#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

#include "bio.h"
#include "bresource.h"

#include "bu/color.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/snooze.h"
#include "bu/str.h"
#include "../include/private.h"
#include "dm.h"
#include "./fb_ogl.h"

extern struct fb ogl_interface;

#define DIRECT_COLOR_VISUAL_ALLOWED 0

static int ogl_nwindows = 0;     /* number of open windows */
static XColor color_cell[256];   /* used to set colormap */

#if 0
static void gl_printglmat(struct bu_vls *tmp_vls, GLfloat *m)
{
    bu_vls_printf(tmp_vls, "   %g %g %g %g\n", m[0], m[4], m[8], m[12]);
    bu_vls_printf(tmp_vls, "   %g %g %g %g\n", m[1], m[5], m[9], m[13]);
    bu_vls_printf(tmp_vls, "   %g %g %g %g\n", m[2], m[6], m[10], m[14]);
    bu_vls_printf(tmp_vls, "   %g %g %g %g\n", m[3], m[7], m[11], m[15]);
}
#endif

/* Per window state information */
struct wininfo {
    short mi_curs_on;
    short mi_cmap_flag;          /* non-linear map present */
    int   mi_shmid;
    int   mi_pixwidth;
    short mi_xoff;
    short mi_yoff;
    int   mi_doublebuffer;
};

/* OpenGL-specific info */
struct oglinfo {
    GLXContext glxc;
    Display *dispp;
    Window wind;
    int firstTime;
    int alive;
    long event_mask;
    short soft_cmap_flag;
    int cmap_size;
    int win_width;
    int win_height;
    int vp_width;
    int vp_height;
    struct fb_clip clip;
    Window cursor;
    XVisualInfo *vip;
    Colormap xcmap;
    int use_ext_ctrl;

    /* Texture presentation state */
    GLuint texid;
    int tex_initialized;
    int dirty;
    int dirty_xmin, dirty_ymin, dirty_xmax, dirty_ymax;
    int view_changed;
};

#define WIN(ptr) ((struct wininfo *)((ptr)->i->u1.p))
#define WINL(ptr) ((ptr)->i->u1.p)
#define OGL(ptr) ((struct oglinfo *)((ptr)->i->u6.p))
#define OGLL(ptr) ((ptr)->i->u6.p)
#define if_mem u2.p
#define if_cmap u3.p
#define CMR(x) ((struct fb_cmap *)((x)->i->if_cmap))->cmr
#define CMG(x) ((struct fb_cmap *)((x)->i->if_cmap))->cmg
#define CMB(x) ((struct fb_cmap *)((x)->i->if_cmap))->cmb
#define if_zoomflag u4.l
#define if_mode u5.l

#define CLIP_XTRA 1

/* Mode bits (legacy preserved) */
#define MODE_1MASK   (1<<0)
#define MODE_1SHARED (0<<0)
#define MODE_1MALLOC (1<<0)

#define MODE_2MASK   (1<<1)
#define MODE_2TRANSIENT (0<<1)
#define MODE_2LINGERING (1<<1)

#define MODE_4MASK   (1<<3)
#define MODE_4NORMAL (0<<3)
#define MODE_4NODITH (1<<3)

#define MODE_7MASK   (1<<6)
#define MODE_7NORMAL (0<<6)
#define MODE_7SWCMAP (1<<6)

#define MODE_9MASK   (1<<8)
#define MODE_9NORMAL (0<<8)
#define MODE_9SINGLEBUF (1<<8)

#define MODE_11MASK  (1<<10)
#define MODE_11NORMAL (0<<10)
#define MODE_11COPY  (1<<10) /* deprecated no-op */

#define MODE_12MASK  (1<<11)
#define MODE_12NORMAL (0<<11)
#define MODE_12DELAY_WRITES_TILL_FLUSH (1<<11)

#define MODE_15MASK  (1<<14)
#define MODE_15NORMAL (0<<14)
#define MODE_15ZAP (1<<14)

static struct modeflags {
    char c;
    long mask;
    long value;
    char *help;
} modeflags[] = {
    { 'p', MODE_1MASK, MODE_1MALLOC, "Private memory - else shared" },
    { 'l', MODE_2MASK, MODE_2LINGERING, "Lingering window" },
    { 't', MODE_2MASK, MODE_2TRANSIENT, "Transient window" },
    { 'd', MODE_4MASK, MODE_4NODITH, "Suppress dithering - else dither if not 24-bit buffer" },
    { 'c', MODE_7MASK, MODE_7SWCMAP, "Perform software colormap - else use hardware colormap if possible" },
    { 's', MODE_9MASK, MODE_9SINGLEBUF, "Single buffer - else double buffer if possible" },
    { 'b', MODE_11MASK, MODE_11COPY, "Deprecated: legacy backbuffer copy pan/zoom (texture path supersedes)" },
    { 'D', MODE_12MASK, MODE_12DELAY_WRITES_TILL_FLUSH, "Don't update screen until fb_flush() is called." },
    { 'z', MODE_15MASK, MODE_15ZAP, "Zap (free) shared memory." },
    { '\0', 0, 0, "" }
};

/* Forward declarations */
static void fb_clipper(struct fb *ifp);

/******************** Texture Path Helpers ************************/

static void
ogl_dirty_clear(struct fb *ifp)
{
    OGL(ifp)->dirty = 0;
    OGL(ifp)->dirty_xmin = OGL(ifp)->dirty_ymin = 0;
    OGL(ifp)->dirty_xmax = OGL(ifp)->dirty_ymax = -1;
}

static void
ogl_dirty_add(struct fb *ifp, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    if (!OGL(ifp)->dirty) {
	OGL(ifp)->dirty = 1;
	OGL(ifp)->dirty_xmin = x;
	OGL(ifp)->dirty_ymin = y;
	OGL(ifp)->dirty_xmax = x + w - 1;
	OGL(ifp)->dirty_ymax = y + h - 1;
    } else {
	if (x < OGL(ifp)->dirty_xmin) OGL(ifp)->dirty_xmin = x;
	if (y < OGL(ifp)->dirty_ymin) OGL(ifp)->dirty_ymin = y;
	if (x + w - 1 > OGL(ifp)->dirty_xmax) OGL(ifp)->dirty_xmax = x + w - 1;
	if (y + h - 1 > OGL(ifp)->dirty_ymax) OGL(ifp)->dirty_ymax = y + h - 1;
    }
}

static int
ogl_make_current(struct fb *ifp)
{
    if (glXMakeCurrent(OGL(ifp)->dispp, OGL(ifp)->wind, OGL(ifp)->glxc)==False) {
	fb_log("if_ogl: glXMakeCurrent failed\n");
	return 0;
    }
    return 1;
}

static void
ogl_release_current(struct fb *ifp)
{
    glXMakeCurrent(OGL(ifp)->dispp, None, NULL);
}

static void
ogl_init_texture(struct fb *ifp)
{
    if (OGL(ifp)->tex_initialized) return;
    if (!ogl_make_current(ifp)) return;

    glGenTextures(1, &OGL(ifp)->texid);
    glBindTexture(GL_TEXTURE_2D, OGL(ifp)->texid);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		 ifp->i->if_width, ifp->i->if_height,
		 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, ifp->i->if_mem);

    OGL(ifp)->tex_initialized = 1;
    ogl_dirty_clear(ifp);
    ogl_release_current(ifp);
}

static void
ogl_upload_region(struct fb *ifp, int x, int y, int w, int h)
{
    if (!OGL(ifp)->tex_initialized) return;
    if (w <= 0 || h <= 0) return;
    if (x < 0) {
	w += x;
	x = 0;
    }
    if (y < 0) {
	h += y;
	y = 0;
    }
    if (x + w > ifp->i->if_width)  w = ifp->i->if_width - x;
    if (y + h > ifp->i->if_height) h = ifp->i->if_height - y;
    if (w <= 0 || h <= 0) return;

    if (!ogl_make_current(ifp)) return;

    int sw_cmap = (OGL(ifp)->soft_cmap_flag && WIN(ifp)->mi_cmap_flag);
    glBindTexture(GL_TEXTURE_2D, OGL(ifp)->texid);

    if (sw_cmap) {
	struct fb_pixel *tmp = (struct fb_pixel *)bu_malloc(sizeof(struct fb_pixel)*w*h, "ogl sw cmap tmp");
	for (int row=0; row<h; row++) {
	    struct fb_pixel *src = (struct fb_pixel *)ifp->i->if_mem + (size_t)(row + y)*WIN(ifp)->mi_pixwidth + x;
	    struct fb_pixel *dst = tmp + (size_t)row*w;
	    for (int col=0; col<w; col++) {
		dst[col].red   = CMR(ifp)[src[col].red];
		dst[col].green = CMG(ifp)[src[col].green];
		dst[col].blue  = CMB(ifp)[src[col].blue];
		dst[col].alpha = 0;
	    }
	}
	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (const GLvoid *)tmp);
	bu_free(tmp, "ogl sw cmap tmp");
    } else {
	glPixelStorei(GL_UNPACK_ROW_LENGTH, WIN(ifp)->mi_pixwidth);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, x);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, y);
	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (const GLvoid *)ifp->i->if_mem);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    }

    ogl_release_current(ifp);
}

static void
ogl_upload_dirty(struct fb *ifp)
{
    if (!OGL(ifp)->dirty) return;
    int x = OGL(ifp)->dirty_xmin;
    int y = OGL(ifp)->dirty_ymin;
    int w = OGL(ifp)->dirty_xmax - OGL(ifp)->dirty_xmin + 1;
    int h = OGL(ifp)->dirty_ymax - OGL(ifp)->dirty_ymin + 1;
    ogl_upload_region(ifp, x, y, w, h);
    ogl_dirty_clear(ifp);
}

static void
ogl_present(struct fb *ifp)
{
    if (!OGL(ifp)->tex_initialized) return;
    if (!ogl_make_current(ifp)) return;

    if ((ifp->i->if_mode & MODE_4MASK) == MODE_4NODITH)
	glDisable(GL_DITHER);

    struct fb_clip *clp = &(OGL(ifp)->clip);
    glViewport(0, 0, OGL(ifp)->win_width, OGL(ifp)->win_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, OGL(ifp)->win_width, 0, OGL(ifp)->win_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);

    int xpixmin = clp->xpixmin;
    int xpixmax = clp->xpixmax;
    int ypixmin = clp->ypixmin;
    int ypixmax = clp->ypixmax;

    if (xpixmin > xpixmax || ypixmin > ypixmax) {
	ogl_release_current(ifp);
	return;
    }

    int vis_w = xpixmax - xpixmin + 1;
    int vis_h = ypixmax - ypixmin + 1;
    double zx = ifp->i->if_xzoom;
    if (zx < 1) zx = 1;
    double zy = ifp->i->if_yzoom;
    if (zy < 1) zy = 1;
    double draw_w = vis_w * zx;
    double draw_h = vis_h * zy;
    double sx = (OGL(ifp)->win_width  - draw_w)/2.0;
    double sy = (OGL(ifp)->win_height - draw_h)/2.0;

    float tx0 = (float)xpixmin / (float)ifp->i->if_width;
    float ty0 = (float)ypixmin / (float)ifp->i->if_height;
    float tx1 = (float)(xpixmax + 1) / (float)ifp->i->if_width;
    float ty1 = (float)(ypixmax + 1) / (float)ifp->i->if_height;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, OGL(ifp)->texid);
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(tx0, ty0);
    glVertex2f(sx,       sy);
    glTexCoord2f(tx0, ty1);
    glVertex2f(sx,       sy + draw_h);
    glTexCoord2f(tx1, ty0);
    glVertex2f(sx+draw_w, sy);
    glTexCoord2f(tx1, ty1);
    glVertex2f(sx+draw_w, sy + draw_h);
    glEnd();

    if (WIN(ifp)->mi_doublebuffer)
	glXSwapBuffers(OGL(ifp)->dispp, OGL(ifp)->wind);
    else
	glFlush();

    ogl_release_current(ifp);
    OGL(ifp)->view_changed = 0;
}

/******************* Colormap / Memory Helpers ************************/

static void
ogl_cminit(struct fb *ifp)
{
    for (int i=0; i<256; i++) {
	CMR(ifp)[i] = i;
	CMB(ifp)[i] = i;
	CMG(ifp)[i] = i;
    }
}

/******************* Shared Memory Support ****************************/

/* (Unchanged logic apart from formatting / warnings) */
static int
ogl_getmem(struct fb *ifp)
{
    size_t pixsize = ifp->i->if_height * ifp->i->if_width * sizeof(struct fb_pixel);
    size_t size;
    int i;
    char *sp;
    int new_mem = 0;
    int shm_result = 0;

    errno = 0;

    if (pixsize > sizeof(struct fb_pixel)*1000*1000) {
	ifp->i->if_mode |= MODE_1MALLOC;
    }

    if ((ifp->i->if_mode & MODE_1MASK) == MODE_1MALLOC) {
	WIN(ifp)->mi_pixwidth = ifp->i->if_width;
	size = pixsize + sizeof(struct fb_cmap);
	sp = (char *)calloc(1, size);
	if (!sp) {
	    fb_log("ogl_getmem: frame buffer memory malloc failed\n");
	    return -1;
	}
	new_mem = 1;
    } else {
	WIN(ifp)->mi_pixwidth = ifp->i->if_max_width;
	pixsize = ifp->i->if_max_height * ifp->i->if_max_width * sizeof(struct fb_pixel);
	size = pixsize + sizeof(struct fb_cmap);
	shm_result = bu_shmget(&(WIN(ifp)->mi_shmid), &sp, SHMEM_KEY, size);
	if (shm_result == -1) {
	    memset(sp, 0, size);
	    new_mem = 1;
	} else if (shm_result == 1) {
	    ifp->i->if_mode |= MODE_1MALLOC;
	    fb_log("ogl_getmem: Unable to attach to shared memory, using private\n");
	    sp = (char *)calloc(1, size);
	    if (!sp) {
		fb_log("ogl_getmem: malloc failure\n");
		return -1;
	    }
	    new_mem = 1;
	}
    }

    ifp->i->if_mem = sp;
    ifp->i->if_cmap = sp + (size - sizeof(struct fb_cmap));
    i = CMB(ifp)[255];
    CMB(ifp)[255] = i;

    if (new_mem) ogl_cminit(ifp);
    return 0;
}

void
ogl_zapmem(void)
{
    int shmid = shmget(SHMEM_KEY, 0, 0);
    if (shmid < 0) {
	if (errno != ENOENT) {
	    fb_log("ogl_zapmem shmget failed, errno=%d\n", errno);
	    perror("shmget");
	}
	return;
    }
    if (shmctl(shmid, IPC_RMID, 0) < 0) {
	fb_log("ogl_zapmem shmctl failed, errno=%d\n", errno);
	perror("shmctl");
	return;
    }
    fb_log("if_ogl: shared memory released\n");
}

/******************* Clipping ****************************************/

void
fb_clipper(struct fb *ifp)
{
    struct fb_clip *clp;
    int i;
    double pixels;

    clp = &(OGL(ifp)->clip);

    i = OGL(ifp)->vp_width/(2*ifp->i->if_xzoom);
    clp->xscrmin = ifp->i->if_xcenter - i;
    i = OGL(ifp)->vp_width/ifp->i->if_xzoom;
    clp->xscrmax = clp->xscrmin + i;
    pixels = (double) i;
    clp->oleft = ((double) clp->xscrmin) - 0.25*pixels/((double) OGL(ifp)->vp_width);
    clp->oright = clp->oleft + pixels;

    i = OGL(ifp)->vp_height/(2*ifp->i->if_yzoom);
    clp->yscrmin = ifp->i->if_ycenter - i;
    i = OGL(ifp)->vp_height/ifp->i->if_yzoom;
    clp->yscrmax = clp->yscrmin + i;
    pixels = (double) i;
    clp->obottom = ((double) clp->yscrmin) - 0.25*pixels/((double) OGL(ifp)->vp_height);
    clp->otop = clp->obottom + pixels;

    clp->xpixmin = clp->xscrmin;
    if (clp->xpixmin < 0) clp->xpixmin = 0;
    clp->xpixmax = clp->xscrmax;
    if (clp->xpixmax > ifp->i->if_width-1) clp->xpixmax = ifp->i->if_width-1;
    clp->ypixmin = clp->yscrmin;
    if (clp->ypixmin < 0) clp->ypixmin = 0;
    clp->ypixmax = clp->yscrmax;
    if (clp->ypixmax > ifp->i->if_height-1) clp->ypixmax = ifp->i->if_height-1;
}

/******************* Expose / Events ********************************/

static void
expose_callback(struct fb *ifp)
{
    XWindowAttributes xwa;

    if (OGL(ifp)->firstTime) {
	if (!ogl_make_current(ifp)) return;
	OGL(ifp)->firstTime = 0;
	XGetWindowAttributes(OGL(ifp)->dispp, OGL(ifp)->wind, &xwa);
	OGL(ifp)->win_width = xwa.width;
	OGL(ifp)->win_height = xwa.height;

	OGL(ifp)->vp_width = (OGL(ifp)->win_width < ifp->i->if_width) ?
			     OGL(ifp)->win_width : ifp->i->if_width;
	OGL(ifp)->vp_height = (OGL(ifp)->win_height < ifp->i->if_height) ?
			      OGL(ifp)->win_height : ifp->i->if_height;

	ifp->i->if_xcenter = OGL(ifp)->vp_width/2;
	ifp->i->if_ycenter = OGL(ifp)->vp_height/2;

	WIN(ifp)->mi_xoff = (OGL(ifp)->win_width - OGL(ifp)->vp_width)/2;
	WIN(ifp)->mi_yoff = (OGL(ifp)->win_height - OGL(ifp)->vp_height)/2;

	glViewport(0, 0, OGL(ifp)->win_width, OGL(ifp)->win_height);
	glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT);
	ogl_release_current(ifp);

	fb_clipper(ifp);
    }

    if (!OGL(ifp)->tex_initialized) {
	ogl_init_texture(ifp);
	/* upload full initial image */
	ogl_dirty_add(ifp, 0, 0, ifp->i->if_width, ifp->i->if_height);
    }

    /* Ensure any outstanding dirty region is uploaded before present */
    if (OGL(ifp)->dirty) ogl_upload_dirty(ifp);
    ogl_present(ifp);
}

int
ogl_configureWindow(struct fb *ifp, int width, int height)
{
    if (width == OGL(ifp)->win_width && height == OGL(ifp)->win_height)
	return 1;

    ifp->i->if_width = ifp->i->if_max_width = width;
    ifp->i->if_height = ifp->i->if_max_height = height;

    OGL(ifp)->win_width = OGL(ifp)->vp_width = width;
    OGL(ifp)->win_height = OGL(ifp)->vp_height = height;

    ifp->i->if_zoomflag = 0;
    ifp->i->if_xzoom = ifp->i->if_yzoom = 1;
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    ogl_getmem(ifp);
    fb_clipper(ifp);

    /* Recreate texture to new size */
    if (OGL(ifp)->tex_initialized) {
	if (ogl_make_current(ifp)) {
	    glDeleteTextures(1, &OGL(ifp)->texid);
	    ogl_release_current(ifp);
	}
	OGL(ifp)->tex_initialized = 0;
    }
    ogl_init_texture(ifp);
    ogl_dirty_add(ifp, 0, 0, ifp->i->if_width, ifp->i->if_height);
    ogl_upload_dirty(ifp);
    ogl_present(ifp);
    return 0;
}

static void
ogl_do_event(struct fb *ifp)
{
    XEvent event;
    while (XCheckWindowEvent(OGL(ifp)->dispp, OGL(ifp)->wind, OGL(ifp)->event_mask, &event)) {
	switch (event.type) {
	    case Expose:
		if (!OGL(ifp)->use_ext_ctrl)
		    expose_callback(ifp);
		break;
	    case ButtonPress: {
		int button = (int)event.xbutton.button;
		if (button == Button1) {
		    if (event.xbutton.state & ControlMask) button = Button2;
		    else if (event.xbutton.state & Mod1Mask) button = Button3;
		    else if (event.xbutton.state & Mod2Mask) button = Button3;
		}
		switch (button) {
		    case Button2: {
			int x = event.xbutton.x;
			int y = ifp->i->if_height - event.xbutton.y - 1;
			if (x < 0 || y < 0) {
			    fb_log("No RGB (outside image viewport)\n");
			    break;
			}
			size_t memidx = (size_t)y * WIN(ifp)->mi_pixwidth;
			struct fb_pixel *oglp = (struct fb_pixel *)&ifp->i->if_mem[memidx * sizeof(struct fb_pixel)];
			fb_log("At image (%d, %d), RGB=(%3d %3d %3d)\n", x, y, (int)oglp[x].red, (int)oglp[x].green, (int)oglp[x].blue);
			break;
		    }
		    case Button3:
			OGL(ifp)->alive = 0;
			break;
		    default:
			break;
		}
		break;
	    }
	    case ConfigureNotify: {
		XConfigureEvent *conf = (XConfigureEvent *)&event;
		if (!(conf->width == OGL(ifp)->win_width && conf->height == OGL(ifp)->win_height)) {
		    ogl_configureWindow(ifp, conf->width, conf->height);
		}
		break;
	    }
	    default:
		break;
	}
    }
}

/******************* Visual Selection / Open ************************/

static int is_linear_cmap(struct fb *ifp)
{
    for (int i=0; i<256; i++) {
	if (CMR(ifp)[i] != i) return 0;
	if (CMG(ifp)[i] != i) return 0;
	if (CMB(ifp)[i] != i) return 0;
    }
    return 1;
}

static XVisualInfo *
fb_ogl_choose_visual(struct fb *ifp)
{
    XVisualInfo *vip, *vibase, *maxvip, _template;
#define NGOOD 200
    int good[NGOOD];
    int num, i, j;
    int m_hard_cmap, m_sing_buf, m_doub_buf;
    int use, rgba, dbfr;

    m_hard_cmap = ((ifp->i->if_mode & MODE_7MASK)==MODE_7NORMAL);
    m_sing_buf  = ((ifp->i->if_mode & MODE_9MASK)==MODE_9SINGLEBUF);
    m_doub_buf = !m_sing_buf;

    memset((void *)&_template, 0, sizeof(XVisualInfo));
    vibase = XGetVisualInfo(OGL(ifp)->dispp, 0, &_template, &num);

    while (1) {
	for (i=0, j=0, vip=vibase; i<num; i++, vip++) {
	    glXGetConfig(OGL(ifp)->dispp, vip, GLX_USE_GL, &use);
	    if (!use) continue;
	    glXGetConfig(OGL(ifp)->dispp, vip, GLX_RGBA, &rgba);
	    if (!rgba) continue;

#ifdef HAVE_XRENDER
	    XRenderPictFormat *pict_format = XRenderFindVisualFormat(OGL(ifp)->dispp, vip->visual);
	    if (pict_format && pict_format->direct.alphaMask > 0) continue;
#endif
	    if (m_hard_cmap && (vip->class != DirectColor)) continue;
	    if (m_hard_cmap && (vip->colormap_size < 256)) continue;
	    glXGetConfig(OGL(ifp)->dispp, vip, GLX_DOUBLEBUFFER, &dbfr);
	    if (m_doub_buf && (!dbfr)) continue;
	    if (m_sing_buf && dbfr) continue;
	    if (j >= NGOOD-1) {
		fb_log("fb_ogl_open: More than %d candidate visuals!\n", NGOOD);
		break;
	    }
	    good[j++] = i;
	}

	if (j >= 1) {
	    maxvip = vibase + good[0];
	    for (i=1; i<j; i++) {
		vip = vibase + good[i];
		if (vip->depth > maxvip->depth)
		    maxvip = vip;
	    }
	    OGL(ifp)->soft_cmap_flag = !m_hard_cmap;
	    WIN(ifp)->mi_doublebuffer = m_doub_buf;
	    return maxvip;
	}

	if (m_hard_cmap) {
	    m_hard_cmap = 0;
	    fb_log("fb_ogl_open: hardware colormapping not available. Using software colormap.\n");
	} else if (m_sing_buf) {
	    m_sing_buf = 0;
	} else if (m_doub_buf) {
	    m_doub_buf = 0;
	    fb_log("fb_ogl_open: double buffering not available. Using single buffer.\n");
	} else {
	    return NULL;
	}
    }
}

/* Open a new window */
static int
fb_ogl_open(struct fb *ifp, const char *file, int width, int height)
{
    static char title[128] = {0};
    int mode;

    FB_CK_FB(ifp->i);

    /* Default: no delayed writes. User must add 'D' flag explicitly. */
    mode = MODE_2LINGERING;

    if (file) {
	const char *cp;
	char modebuf[80] = {0};
	char *mp = modebuf;
	int alpha = 0;
	struct modeflags *mfp;
	if (!bu_strncmp(file, ifp->i->if_name, strlen(ifp->i->if_name))) {
	    cp = &file[8];
	    while (*cp && !isspace((int)(*cp))) {
		*mp++ = *cp;
		if (isdigit((int)(*cp))) {
		    cp++;
		    continue;
		}
		alpha++;
		for (mfp=modeflags; mfp->c != '\0'; mfp++) {
		    if (mfp->c == *cp) {
			mode = (mode & ~mfp->mask) | mfp->value;
			break;
		    }
		}
		if (mfp->c == '\0' && *cp != '-') {
		    fb_log("if_ogl: unknown option '%c' ignored\n", *cp);
		}
		cp++;
	    }
	    *mp = '\0';
	    if (!alpha) mode |= atoi(modebuf);
	}
	if ((mode & MODE_15MASK) == MODE_15ZAP) {
	    ogl_zapmem();
	    return -1;
	}
    }

#if DIRECT_COLOR_VISUAL_ALLOWED
    ifp->i->if_mode = mode;
#else
    ifp->i->if_mode = mode | MODE_7SWCMAP;
#endif

    struct wininfo *winfo = (struct wininfo *)calloc(1, sizeof(struct wininfo));
    WINL(ifp) = (char *)winfo;
    if (!WINL(ifp)) {
	fb_log("fb_ogl_open: wininfo malloc failed\n");
	return -1;
    }

    struct oglinfo *oinfo = (struct oglinfo *)calloc(1, sizeof(struct oglinfo));
    OGLL(ifp) = (char *)oinfo;
    if (!OGL(ifp)) {
	fb_log("fb_ogl_open: oglinfo malloc failed\n");
	return -1;
    }

    WIN(ifp)->mi_shmid = -1;

    if (width <= 0) width = ifp->i->if_width;
    if (height <= 0) height = ifp->i->if_height;
    if (width > ifp->i->if_max_width) width = ifp->i->if_max_width;
    if (height > ifp->i->if_max_height) height = ifp->i->if_max_height;

    ifp->i->if_width = width;
    ifp->i->if_height = height;

    if (ogl_getmem(ifp) < 0)
	return -1;

    WIN(ifp)->mi_curs_on = 1;

    snprintf(title, 128, "BRL-CAD /dev/ogl %s, %s",
	     ((ifp->i->if_mode & MODE_2MASK) == MODE_2TRANSIENT) ? "Transient Win":"Lingering Win",
	     ((ifp->i->if_mode & MODE_1MASK) == MODE_1MALLOC) ? "Private Mem":"Shared Mem");

    ifp->i->if_zoomflag = 0;
    ifp->i->if_xzoom = ifp->i->if_yzoom = 1;
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    OGL(ifp)->dispp = XOpenDisplay(NULL);
    if (!OGL(ifp)->dispp) {
	fb_log("fb_ogl_open: Failed to open display. Check DISPLAY.\n");
	return -1;
    }
    ifp->i->if_selfd = ConnectionNumber(OGL(ifp)->dispp);

    OGL(ifp)->vip = fb_ogl_choose_visual(ifp);
    if (!OGL(ifp)->vip) {
	fb_log("fb_ogl_open: Couldn't find an appropriate visual.\n");
	return -1;
    }

    OGL(ifp)->glxc = glXCreateContext(OGL(ifp)->dispp, OGL(ifp)->vip, 0, GL_TRUE);
    if (!OGL(ifp)->glxc) {
	fb_log("ERROR: Couldn't create an OpenGL context!\n");
	return -1;
    }

    WIN(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);
    if (!OGL(ifp)->soft_cmap_flag) {
	OGL(ifp)->xcmap = XCreateColormap(OGL(ifp)->dispp,
					  RootWindow(OGL(ifp)->dispp, OGL(ifp)->vip->screen),
					  OGL(ifp)->vip->visual,
					  AllocAll);
	for (int i=0; i<256; i++) {
	    color_cell[i].pixel = i;
	    color_cell[i].red = CMR(ifp)[i];
	    color_cell[i].green = CMG(ifp)[i];
	    color_cell[i].blue = CMB(ifp)[i];
	    color_cell[i].flags = DoRed | DoGreen | DoBlue;
	}
	XStoreColors(OGL(ifp)->dispp, OGL(ifp)->xcmap, color_cell, 256);
    } else {
	OGL(ifp)->xcmap = XCreateColormap(OGL(ifp)->dispp,
					  RootWindow(OGL(ifp)->dispp, OGL(ifp)->vip->screen),
					  OGL(ifp)->vip->visual,
					  AllocNone);
    }

    XSync(OGL(ifp)->dispp, 0);

    XSetWindowAttributes swa;
    memset(&swa, 0, sizeof(swa));
    long valuemask = CWBackPixel | CWBorderPixel | CWEventMask | CWColormap;
    swa.background_pixel = BlackPixel(OGL(ifp)->dispp, OGL(ifp)->vip->screen);
    swa.border_pixel = BlackPixel(OGL(ifp)->dispp, OGL(ifp)->vip->screen);
    swa.event_mask = OGL(ifp)->event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask;
    swa.colormap = OGL(ifp)->xcmap;

    OGL(ifp)->wind = XCreateWindow(OGL(ifp)->dispp,
				   RootWindow(OGL(ifp)->dispp, OGL(ifp)->vip->screen),
				   0, 0, ifp->i->if_width, ifp->i->if_height, 0,
				   OGL(ifp)->vip->depth, InputOutput,
				   OGL(ifp)->vip->visual,
				   valuemask, &swa);

    XStoreName(OGL(ifp)->dispp, OGL(ifp)->wind, title);
    ogl_nwindows++;
    XMapRaised(OGL(ifp)->dispp, OGL(ifp)->wind);

    OGL(ifp)->alive = 1;
    OGL(ifp)->firstTime = 1;

    while (OGL(ifp)->firstTime == 1)
	ogl_do_event(ifp);

    return 0;
}

/* Open existing controlled externally */
static int
open_existing(struct fb *ifp, Display *dpy, Window win, Colormap cmap, XVisualInfo *vip,
	      int width, int height, GLXContext glxc, int double_buffer, int soft_cmap)
{
    ifp->i->if_mode = MODE_1MALLOC; /* force private memory */

    struct wininfo *winfo = (struct wininfo *)calloc(1, sizeof(struct wininfo));
    WINL(ifp) = (char *)winfo;
    if (!WINL(ifp)) {
	fb_log("fb_ogl_open: wininfo malloc failed\n");
	return -1;
    }
    struct oglinfo *oinfo = (struct oglinfo *)calloc(1, sizeof(struct oglinfo));
    OGLL(ifp) = (char *)oinfo;
    if (!OGL(ifp)) {
	fb_log("fb_ogl_open: oglinfo malloc failed\n");
	return -1;
    }

    OGL(ifp)->use_ext_ctrl = 1;
    WIN(ifp)->mi_shmid = -1;

    ifp->i->if_width = ifp->i->if_max_width = width;
    ifp->i->if_height = ifp->i->if_max_height = height;

    OGL(ifp)->win_width = OGL(ifp)->vp_width = width;
    OGL(ifp)->win_height = OGL(ifp)->vp_height = height;

    WIN(ifp)->mi_curs_on = 1;

    ifp->i->if_zoomflag = 0;
    ifp->i->if_xzoom = ifp->i->if_yzoom = 1;
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    if (ogl_getmem(ifp) < 0)
	return -1;

    OGL(ifp)->dispp = dpy;
    ifp->i->if_selfd = ConnectionNumber(OGL(ifp)->dispp);
    OGL(ifp)->vip = vip;
    OGL(ifp)->glxc = glxc;
    WIN(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);
    OGL(ifp)->soft_cmap_flag = soft_cmap;
    WIN(ifp)->mi_doublebuffer = double_buffer;
    OGL(ifp)->xcmap = cmap;

    OGL(ifp)->wind = win;
    ++ogl_nwindows;

    OGL(ifp)->alive = 1;
    OGL(ifp)->firstTime = 1;

    fb_clipper(ifp);

    return 0;
}

static struct fb_platform_specific *
ogl_get_fbps(uint32_t magic)
{
    struct fb_platform_specific *fb_ps = NULL;
    struct ogl_fb_info *data = NULL;
    BU_GET(fb_ps, struct fb_platform_specific);
    BU_GET(data, struct ogl_fb_info);
    fb_ps->magic = magic;
    fb_ps->data = data;
    return fb_ps;
}

static void
ogl_put_fbps(struct fb_platform_specific *fbps)
{
    BU_CKMAG(fbps, FB_OGL_MAGIC, "ogl framebuffer");
    BU_PUT(fbps->data, struct ogl_fb_info);
    BU_PUT(fbps, struct fb_platform_specific);
}

static int
ogl_open_existing(struct fb *ifp, int width, int height, struct fb_platform_specific *fb_p)
{
    struct ogl_fb_info *ogl_internal = (struct ogl_fb_info *)fb_p->data;
    BU_CKMAG(fb_p, FB_OGL_MAGIC, "ogl framebuffer");
    return open_existing(ifp, ogl_internal->dpy, ogl_internal->win,
			 ogl_internal->cmap, ogl_internal->vip, width, height, ogl_internal->glxc,
			 ogl_internal->double_buffer, ogl_internal->soft_cmap);
}

int
ogl_close_existing(struct fb *ifp)
{
    if (OGL(ifp)->cursor)
	XDestroyWindow(OGL(ifp)->dispp, OGL(ifp)->cursor);

    if (WINL(ifp)) {
	if (WIN(ifp)->mi_shmid != -1) {
	    errno = 0;
	    if (shmdt(ifp->i->if_mem) == -1) {
		fb_log("fb_ogl_close: shmdt failed, errno=%d\n", errno);
		perror("shmdt");
		return -1;
	    }
	} else {
	    free(ifp->i->if_mem);
	}
	free((char *)WINL(ifp));
	WINL(ifp) = NULL;
    }

    if (OGL(ifp)) {
	if (OGL(ifp)->tex_initialized && ogl_make_current(ifp)) {
	    glDeleteTextures(1, &OGL(ifp)->texid);
	    ogl_release_current(ifp);
	}
	free((char *)OGLL(ifp));
	OGLL(ifp) = NULL;
    }

    return 0;
}

static int
ogl_final_close(struct fb *ifp)
{
    Display *display = OGL(ifp)->dispp;
    Window window = OGL(ifp)->wind;
    Colormap colormap = OGL(ifp)->xcmap;

    ogl_close_existing(ifp);

    XDestroyWindow(display, window);
    XFreeColormap(display, colormap);

    ogl_nwindows--;
    return 0;
}

/******************* Flush / Poll / Close ***************************/

static int
ogl_flush(struct fb *ifp)
{
    /* Always commit any outstanding changes */
    if (OGL(ifp)->dirty)
	ogl_upload_dirty(ifp);
    if (OGL(ifp)->view_changed || OGL(ifp)->dirty)
	ogl_present(ifp);
    if (OGL(ifp)->dispp) {
	XFlush(OGL(ifp)->dispp);
    }
    return 0;
}

static int
ogl_poll(struct fb *ifp)
{
    ogl_do_event(ifp);
    return OGL(ifp)->alive ? 0 : 1;
}

static int
fb_ogl_close(struct fb *ifp)
{
    ogl_flush(ifp);

    if (ogl_nwindows > 1 ||
	(ifp->i->if_mode & MODE_2MASK) == MODE_2TRANSIENT)
	return ogl_final_close(ifp);

    fclose(stdin);
    while (!ogl_poll(ifp)) {
	bu_snooze(fb_poll_rate(ifp));
    }
    return ogl_final_close(ifp);
}

static int
ogl_free(struct fb *ifp)
{
    int ret = ogl_final_close(ifp);
    if ((ifp->i->if_mode & MODE_1MASK) == MODE_1SHARED) {
	ogl_zapmem();
    }
    return ret;
}

/******************* Clear / View ***********************************/

static int
ogl_clear(struct fb *ifp, unsigned char *pp)
{
    struct fb_pixel bg;
    bg.alpha = 0;
    if (pp != RGBPIXEL_NULL) {
	bg.red = pp[RED];
	bg.green = pp[GRN];
	bg.blue = pp[BLU];
    } else {
	bg.red = bg.green = bg.blue = 0;
    }

    for (int y=0; y < ifp->i->if_height; y++) {
	struct fb_pixel *oglp = (struct fb_pixel *)&ifp->i->if_mem[(size_t)(y * WIN(ifp)->mi_pixwidth) * sizeof(struct fb_pixel)];
	for (int cnt=ifp->i->if_width; cnt>0; cnt--) {
	    *oglp++ = bg;
	}
    }

    ogl_dirty_add(ifp, 0, 0, ifp->i->if_width, ifp->i->if_height);
    if ((ifp->i->if_mode & MODE_12MASK) != MODE_12DELAY_WRITES_TILL_FLUSH) {
	ogl_upload_dirty(ifp);
	ogl_present(ifp);
    }

    return 0;
}

static int
ogl_view(struct fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    if (xzoom < 1) xzoom = 1;
    if (yzoom < 1) yzoom = 1;

    if (xcenter < 0 || xcenter >= ifp->i->if_width) return -1;
    if (ycenter < 0 || ycenter >= ifp->i->if_height) return -1;
    if (xzoom >= ifp->i->if_width || yzoom >= ifp->i->if_height) return -1;

    if (ifp->i->if_xcenter == xcenter && ifp->i->if_ycenter == ycenter &&
	ifp->i->if_xzoom == xzoom && ifp->i->if_yzoom == yzoom)
	return 0;

    ifp->i->if_xcenter = xcenter;
    ifp->i->if_ycenter = ycenter;
    ifp->i->if_xzoom = xzoom;
    ifp->i->if_yzoom = yzoom;
    ifp->i->if_zoomflag = (xzoom > 1 || yzoom > 1) ? 1 : 0;

    fb_clipper(ifp);
    OGL(ifp)->view_changed = 1;

    if ((ifp->i->if_mode & MODE_12MASK) != MODE_12DELAY_WRITES_TILL_FLUSH) {
	/* No new pixel data, just re-present */
	ogl_present(ifp);
    }
    return 0;
}

static int
ogl_getview(struct fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    *xcenter = ifp->i->if_xcenter;
    *ycenter = ifp->i->if_ycenter;
    *xzoom = ifp->i->if_xzoom;
    *yzoom = ifp->i->if_yzoom;
    return 0;
}

/******************* Read/Write Pixels ********************************/

static ssize_t
ogl_read(struct fb *ifp, int x, int y, unsigned char *pixelp, size_t count)
{
    if (x < 0 || x >= ifp->i->if_width ||
	y < 0 || y >= ifp->i->if_height)
	return -1;

    size_t remaining = count;
    ssize_t ret = 0;
    unsigned char *cp = pixelp;
    int cx = x;
    int cy = y;

    while (remaining) {
	if (cy >= ifp->i->if_height) break;
	size_t scan_count = remaining;
	size_t line_remain = (size_t)(ifp->i->if_width - cx);
	if (scan_count > line_remain) scan_count = line_remain;

	struct fb_pixel *oglp = (struct fb_pixel *)&ifp->i->if_mem[
				    ((size_t)cy*WIN(ifp)->mi_pixwidth + cx) * sizeof(struct fb_pixel)];

	for (size_t i=0; i<scan_count; i++) {
	    cp[0] = oglp[i].red;
	    cp[1] = oglp[i].green;
	    cp[2] = oglp[i].blue;
	    cp += 3;
	}

	ret += scan_count;
	remaining -= scan_count;
	cx = 0;
	cy++;
    }
    return ret;
}

static ssize_t
ogl_write(struct fb *ifp, int xstart, int ystart, const unsigned char *pixelp, size_t count)
{
    if (xstart < 0 || xstart >= ifp->i->if_width ||
	ystart < 0 || ystart >= ifp->i->if_height)
	return -1;
    if (count == 0) return 0;

    size_t remaining = count;
    ssize_t written = 0;
    const unsigned char *cp = pixelp;
    int cx = xstart;
    int cy = ystart;

    int minx = ifp->i->if_width, miny = ifp->i->if_height;
    int maxx = 0, maxy = 0;

    while (remaining) {
	if (cy >= ifp->i->if_height) break;
	size_t line_space = (size_t)(ifp->i->if_width - cx);
	size_t scan_count = (remaining < line_space) ? remaining : line_space;

	struct fb_pixel *oglp = (struct fb_pixel *)&ifp->i->if_mem[
				    ((size_t)cy*WIN(ifp)->mi_pixwidth + cx) * sizeof(struct fb_pixel)];

	size_t n = scan_count;
	while (n) {
	    oglp->red   = cp[0];
	    oglp->green = cp[1];
	    oglp->blue  = cp[2];
	    oglp->alpha = 0;
	    oglp++;
	    cp += 3;
	    n--;
	}

	if (cx < minx) minx = cx;
	if (cy < miny) miny = cy;
	int endx = cx + (int)scan_count - 1;
	if (endx > maxx) maxx = endx;
	if (cy > maxy) maxy = cy;

	written += scan_count;
	remaining -= scan_count;
	cx = 0;
	cy++;
    }

    if (written > 0) {
	ogl_dirty_add(ifp, minx, miny, (maxx - minx + 1), (maxy - miny + 1));
	if ((ifp->i->if_mode & MODE_12MASK) != MODE_12DELAY_WRITES_TILL_FLUSH) {
	    ogl_upload_dirty(ifp);
	    ogl_present(ifp);
	}
    }

    return written;
}

static int
ogl_writerect(struct fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    if (width <= 0 || height <= 0) return 0;
    if (xmin < 0 || xmin+width > ifp->i->if_width ||
	ymin < 0 || ymin+height > ifp->i->if_height)
	return -1;

    const unsigned char *cp = pp;
    for (int y = ymin; y < ymin+height; y++) {
	struct fb_pixel *oglp = (struct fb_pixel *)&ifp->i->if_mem[
				    ((size_t)y*WIN(ifp)->mi_pixwidth + xmin) * sizeof(struct fb_pixel)];
	for (int x = xmin; x < xmin+width; x++) {
	    oglp->red   = cp[RED];
	    oglp->green = cp[GRN];
	    oglp->blue  = cp[BLU];
	    oglp->alpha = 0;
	    oglp++;
	    cp += 3;
	}
    }

    ogl_dirty_add(ifp, xmin, ymin, width, height);
    if ((ifp->i->if_mode & MODE_12MASK) != MODE_12DELAY_WRITES_TILL_FLUSH) {
	ogl_upload_dirty(ifp);
	ogl_present(ifp);
    }

    return width * height;
}

static int
ogl_bwwriterect(struct fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    if (width <= 0 || height <= 0) return 0;
    if (xmin < 0 || xmin+width > ifp->i->if_width ||
	ymin < 0 || ymin+height > ifp->i->if_height)
	return -1;

    const unsigned char *cp = pp;
    for (int y = ymin; y < ymin+height; y++) {
	struct fb_pixel *oglp = (struct fb_pixel *)&ifp->i->if_mem[
				    ((size_t)y*WIN(ifp)->mi_pixwidth + xmin) * sizeof(struct fb_pixel)];
	for (int x = xmin; x < xmin+width; x++) {
	    int val = *cp++;
	    oglp->red = oglp->green = oglp->blue = (unsigned char)val;
	    oglp->alpha = 0;
	    oglp++;
	}
    }

    ogl_dirty_add(ifp, xmin, ymin, width, height);
    if ((ifp->i->if_mode & MODE_12MASK) != MODE_12DELAY_WRITES_TILL_FLUSH) {
	ogl_upload_dirty(ifp);
	ogl_present(ifp);
    }
    return width * height;
}

/******************* Colormap ****************************************/

static int
ogl_rmap(struct fb *ifp, ColorMap *cmp)
{
    for (int i=0; i<256; i++) {
	cmp->cm_red[i]   = CMR(ifp)[i] << 8;
	cmp->cm_green[i] = CMG(ifp)[i] << 8;
	cmp->cm_blue[i]  = CMB(ifp)[i] << 8;
    }
    return 0;
}

static int
ogl_wmap(struct fb *ifp, const ColorMap *cmp)
{
    int prev = WIN(ifp)->mi_cmap_flag;
    if (cmp == COLORMAP_NULL) {
	ogl_cminit(ifp);
    } else {
	for (int i=0; i<256; i++) {
	    CMR(ifp)[i] = cmp->cm_red[i]>>8;
	    CMG(ifp)[i] = cmp->cm_green[i]>>8;
	    CMB(ifp)[i] = cmp->cm_blue[i]>>8;
	}
    }
    WIN(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);

    if (!OGL(ifp)->use_ext_ctrl && OGL(ifp)->soft_cmap_flag) {
	/* If either previously or now non-linear, repaint. */
	if (WIN(ifp)->mi_cmap_flag || prev) {
	    ogl_dirty_add(ifp, 0, 0, ifp->i->if_width, ifp->i->if_height);
	    if ((ifp->i->if_mode & MODE_12MASK) != MODE_12DELAY_WRITES_TILL_FLUSH) {
		ogl_upload_dirty(ifp);
		ogl_present(ifp);
	    }
	}
    } else if (!OGL(ifp)->soft_cmap_flag) {
	for (int i=0; i<256; i++) {
	    color_cell[i].pixel = i;
	    color_cell[i].red   = CMR(ifp)[i];
	    color_cell[i].green = CMG(ifp)[i];
	    color_cell[i].blue  = CMB(ifp)[i];
	    color_cell[i].flags = DoRed | DoGreen | DoBlue;
	}
	XStoreColors(OGL(ifp)->dispp, OGL(ifp)->xcmap, color_cell, 256);
    }
    return 0;
}

/******************* Help ********************************************/

static int
ogl_help(struct fb *ifp)
{
    struct modeflags *mfp;
    XVisualInfo *visual = OGL(ifp)->vip;

    fb_log("Description: %s\n", ifp->i->if_type);
    fb_log("Device: %s\n", ifp->i->if_name);
    fb_log("Max width height: %d %d\n", ifp->i->if_max_width, ifp->i->if_max_height);
    fb_log("Default width height: %d %d\n", ifp->i->if_width, ifp->i->if_height);
    fb_log("Usage: /dev/ogl[option letters]\n");
    for (mfp = modeflags; mfp->c != '\0'; mfp++) {
	fb_log("   %c   %s\n", mfp->c, mfp->help);
    }

    fb_log("\nCurrent internal state:\n");
    fb_log("    mi_doublebuffer=%d\n", WIN(ifp)->mi_doublebuffer);
    fb_log("    mi_cmap_flag=%d\n", WIN(ifp)->mi_cmap_flag);
    fb_log("    ogl_nwindows=%d\n", ogl_nwindows);

    fb_log("X11 Visual:\n");
    switch (visual->class) {
	case DirectColor:
	    fb_log("\tDirectColor: Alterable RGB maps\n");
	    fb_log("\tRGB Masks: 0x%lx 0x%lx 0x%lx\n", visual->red_mask, visual->green_mask, visual->blue_mask);
	    break;
	case TrueColor:
	    fb_log("\tTrueColor: Fixed RGB maps\n");
	    fb_log("\tRGB Masks: 0x%lx 0x%lx 0x%lx\n", visual->red_mask, visual->green_mask, visual->blue_mask);
	    break;
	case PseudoColor:
	    fb_log("\tPseudoColor: Alterable RGB map, single index\n");
	    break;
	case StaticColor:
	    fb_log("\tStaticColor: Fixed RGB maps, single index\n");
	    break;
	case GrayScale:
	    fb_log("\tGrayScale: Alterable map (R=G=B)\n");
	    break;
	case StaticGray:
	    fb_log("\tStaticGray: Fixed map (R=G=B)\n");
	    break;
	default:
	    fb_log("\tUnknown visual class %d\n", visual->class);
	    break;
    }
    fb_log("\tColormap Size: %d\n", visual->colormap_size);
    fb_log("\tBits per RGB: %d\n", visual->bits_per_rgb);
    fb_log("\tscreen: %d\n", visual->screen);
    fb_log("\tdepth: %d\n", visual->depth);
    if (visual->depth < 24)
	fb_log("\tWARNING: <24-bit color depth may quantize image.\n");
    return 0;
}

/******************* Cursor ******************************************/

static int
ogl_setcursor(struct fb *ifp, const unsigned char *UNUSED(bits),
	      int UNUSED(xbits), int UNUSED(ybits),
	      int UNUSED(xorig), int UNUSED(yorig))
{
    FB_CK_FB(ifp->i);
    return 0;
}

static int
ogl_cursor(struct fb *ifp, int mode, int x, int y)
{
    if (mode) {
	int xx, xy;
	int delta;

	if (!OGL(ifp)->cursor) {
	    XSetWindowAttributes xswa;
	    XColor rgb_db_def;
	    XColor bg, bd;
	    XAllocNamedColor(OGL(ifp)->dispp, OGL(ifp)->xcmap, "black", &rgb_db_def, &bg);
	    XAllocNamedColor(OGL(ifp)->dispp, OGL(ifp)->xcmap, "white", &rgb_db_def, &bd);
	    xswa.background_pixel = bg.pixel;
	    xswa.border_pixel = bd.pixel;
	    xswa.colormap = OGL(ifp)->xcmap;
	    xswa.save_under = True;
	    OGL(ifp)->cursor = XCreateWindow(OGL(ifp)->dispp, OGL(ifp)->wind,
					     0, 0, 4, 4, 2, OGL(ifp)->vip->depth,
					     InputOutput, OGL(ifp)->vip->visual,
					     CWBackPixel | CWBorderPixel |
					     CWSaveUnder | CWColormap, &xswa);
	}

	delta = ifp->i->if_width/ifp->i->if_xzoom/2;
	xx = x - (ifp->i->if_xcenter - delta);
	xx *= ifp->i->if_xzoom;
	xx += ifp->i->if_xzoom/2;

	delta = ifp->i->if_height/ifp->i->if_yzoom/2;
	xy = y - (ifp->i->if_ycenter - delta);
	xy *= ifp->i->if_yzoom;
	xy += ifp->i->if_yzoom/2;
	xy = OGL(ifp)->win_height - xy;

	XMoveWindow(OGL(ifp)->dispp, OGL(ifp)->cursor, xx - 4, xy - 4);
	if (!ifp->i->if_cursmode)
	    XMapRaised(OGL(ifp)->dispp, OGL(ifp)->cursor);
    } else {
	if (OGL(ifp)->cursor && ifp->i->if_cursmode)
	    XUnmapWindow(OGL(ifp)->dispp, OGL(ifp)->cursor);
    }
    XFlush(OGL(ifp)->dispp);
    ifp->i->if_cursmode = mode;
    ifp->i->if_xcurs = x;
    ifp->i->if_ycurs = y;
    return 0;
}

/******************* Refresh *****************************************/

int
ogl_refresh(struct fb *ifp, int x, int y, int w, int h)
{
    if (w < 0) {
	w = -w;
	x -= w;
    }
    if (h < 0) {
	h = -h;
	y -= h;
    }
    if (w <= 0 || h <= 0) return 0;

    /* Ensure texture exists */
    if (!OGL(ifp)->tex_initialized) ogl_init_texture(ifp);

    /* Mark region dirty and optionally upload immediately */
    ogl_dirty_add(ifp, x, y, w, h);
    if ((ifp->i->if_mode & MODE_12MASK) != MODE_12DELAY_WRITES_TILL_FLUSH) {
	ogl_upload_dirty(ifp);
	ogl_present(ifp);
    }
    return 0;
}

/******************* Implementation Table ***************************/

struct fb_impl ogl_interface_impl =  {
    0,
    FB_OGL_MAGIC,
    fb_ogl_open,
    ogl_open_existing,
    ogl_close_existing,
    ogl_get_fbps,
    ogl_put_fbps,
    fb_ogl_close,
    ogl_clear,
    ogl_read,
    ogl_write,
    ogl_rmap,
    ogl_wmap,
    ogl_view,
    ogl_getview,
    ogl_setcursor,
    ogl_cursor,
    fb_sim_getcursor,
    fb_sim_readrect,
    ogl_writerect,
    fb_sim_bwreadrect,
    ogl_bwwriterect,
    ogl_configureWindow,
    ogl_refresh,
    ogl_poll,
    ogl_flush,
    ogl_free,
    ogl_help,
    "OpenGL (texture blit modernized)",
    FB_XMAXSCREEN,
    FB_YMAXSCREEN,
    "/dev/ogl",
    512,
    512,
    -1,
    -1,
    1, 1,
    256, 256,
    0, 0, 0,
    PIXEL_NULL,
    PIXEL_NULL,
    PIXEL_NULL,
    -1,
    0,
    0L,
    0L,
    0,
    50000,
    NULL,
    NULL,
    0,
    NULL,
    {0},
    {0},
    {0},
    {0},
    {0},
    {0}
};

struct fb ogl_interface = { &ogl_interface_impl };

#ifdef DM_PLUGIN
static const struct fb_plugin finfo = { &ogl_interface };
COMPILER_DLLEXPORT const struct fb_plugin *fb_plugin_info(void)
{
    return &finfo;
}
#endif

#undef class

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

/*                        D M - O G L . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2024 United States Government as represented by
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
/** @file libdm/dm-ogl.c
 *
 * An X11 OpenGL Display Manager.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#if defined(__clang__)
#  pragma clang diagnostic push /* start new diagnostic pragma */
#  pragma clang diagnostic ignored "-Wdocumentation"
#endif

#ifdef HAVE_X11_XOSDEFS_H
#  include <X11/Xfuncproto.h>
#  include <X11/Xosdefs.h>
#endif
#ifdef linux
#  undef X_NOT_STDC_ENV
#  undef X_NOT_POSIX
#endif

#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
#  include <X11/extensions/XInput.h>
#endif /* HAVE_X11_XINPUT_H */

/* glx.h on Mac OS X (and perhaps elsewhere) defines a slew of
 * parameter names that shadow system symbols.  protect the system
 * symbols by redefining the parameters prior to header inclusion.
 */
#define j1 J1
#define y1 Y1
#define read rd
#define index idx
#define access acs
#define remainder rem
#ifdef HAVE_GL_GLX_H
#  include <GL/glx.h>
#  ifdef HAVE_XRENDER
#    include <X11/extensions/Xrender.h>
#  endif
#endif
#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif

#undef remainder
#undef access
#undef index
#undef read
#undef y1
#undef j1

#include "png.h"

#include "tk.h"

#if defined(__clang__)
#  pragma clang diagnostic pop /* end ignoring warnings */
#endif


#undef VMIN		/* is used in vmath.h, too */

#include "vmath.h"
#include "bn.h"
#include "bv/defines.h"
#include "dm.h"
#include "../null/dm-Null.h"
#include "../dm-gl.h"
#include "./fb_ogl.h"
#include "./dm-ogl.h"

#include "../include/private.h"

#define ENABLE_POINT_SMOOTH 1

#define VIEWFACTOR      (1.0/(*dmp->i->dm_vp))
#define VIEWSIZE        (2.0*(*dmp->i->dm_vp))

/* these are from /usr/include/gl.h could be device dependent */
#define XMAXSCREEN	1279
#define YMAXSCREEN	1023
#define YSTEREO		491	/* subfield height, in scanlines */
#define YOFFSET_LEFT	532	/* YSTEREO + YBLANK ? */

static XVisualInfo *ogl_choose_visual(struct dm *dmp, Tk_Window tkwin);

/* Display Manager package interface */
#define IRBOUND 4095.9	/* Max magnification in Rot matrix */
#define PLOTBOUND 1000.0	/* Max magnification in Rot matrix */

struct dm *ogl_open(void *ctx, void *vinterp, int argc, const char **argv);
static int ogl_close(struct dm *dmp);
static int ogl_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);
static int ogl_configureWin_guts(struct dm *dmp, int force);
static int ogl_configureWin(struct dm *dmp, int force);
static int ogl_makeCurrent(struct dm *dmp);
static int ogl_SwapBuffers(struct dm *dmp);

/*
 * Either initially, or on resize/reshape of the window,
 * sense the actual size of the window, and perform any
 * other initializations of the window configuration.
 *
 * also change font size if necessary
 */
static int
ogl_configureWin_guts(struct dm *dmp, int force)
{
    XWindowAttributes xwa;
    XFontStruct *newfontstruct;

    struct dm_glxvars *pubvars = (struct dm_glxvars *)dmp->i->dm_vars.pub_vars;
    struct pogl_vars *privars = (struct pogl_vars *)dmp->i->dm_vars.priv_vars;

    gl_debug_print(dmp, "ogl_configureWin_guts", dmp->i->dm_debugLevel);

    XGetWindowAttributes(pubvars->dpy,
			 pubvars->win, &xwa);

    /* nothing to do */
    if (!force &&
	dmp->i->dm_height == xwa.height &&
	dmp->i->dm_width == xwa.width)
	return BRLCAD_OK;

    gl_reshape(dmp, xwa.width, xwa.height);

    /* First time through, load a font or quit */
    if (pubvars->fontstruct == NULL) {
	if ((pubvars->fontstruct =
	     XLoadQueryFont(pubvars->dpy,
			    FONT9)) == NULL) {
	    /* Try hardcoded backup font */
	    if ((pubvars->fontstruct =
		 XLoadQueryFont(pubvars->dpy,
				FONTBACK)) == NULL) {
		bu_log("ogl_configureWin_guts: Can't open font '%s' or '%s'\n", FONT9, FONTBACK);
		return BRLCAD_ERROR;
	    }
	}
	glXUseXFont(pubvars->fontstruct->fid,
		    0, 127, privars->fontOffset);
    }

    if (DM_VALID_FONT_SIZE(dmp->i->dm_fontsize)) {
	if (pubvars->fontstruct->per_char->width != dmp->i->dm_fontsize) {
	    if ((newfontstruct = XLoadQueryFont(pubvars->dpy,
						DM_FONT_SIZE_TO_NAME(dmp->i->dm_fontsize))) != NULL) {
		XFreeFont(pubvars->dpy,
			  pubvars->fontstruct);
		pubvars->fontstruct = newfontstruct;
		glXUseXFont(pubvars->fontstruct->fid,
			    0, 127, privars->fontOffset);
	    }
	}
    } else {
	/* Always try to choose a the font that best fits the window size.
	 */

	if (dmp->i->dm_width < 582) {
	    if (pubvars->fontstruct->per_char->width != 5) {
		if ((newfontstruct = XLoadQueryFont(pubvars->dpy,
						    FONT5)) != NULL) {
		    XFreeFont(pubvars->dpy,
			      pubvars->fontstruct);
		    pubvars->fontstruct = newfontstruct;
		    glXUseXFont(pubvars->fontstruct->fid,
				0, 127, privars->fontOffset);
		}
	    }
	} else if (dmp->i->dm_width < 679) {
	    if (pubvars->fontstruct->per_char->width != 6) {
		if ((newfontstruct = XLoadQueryFont(pubvars->dpy,
						    FONT6)) != NULL) {
		    XFreeFont(pubvars->dpy,
			      pubvars->fontstruct);
		    pubvars->fontstruct = newfontstruct;
		    glXUseXFont(pubvars->fontstruct->fid,
				0, 127, privars->fontOffset);
		}
	    }
	} else if (dmp->i->dm_width < 776) {
	    if (pubvars->fontstruct->per_char->width != 7) {
		if ((newfontstruct = XLoadQueryFont(pubvars->dpy,
						    FONT7)) != NULL) {
		    XFreeFont(pubvars->dpy,
			      pubvars->fontstruct);
		    pubvars->fontstruct = newfontstruct;
		    glXUseXFont(pubvars->fontstruct->fid,
				0, 127, privars->fontOffset);
		}
	    }
	} else if (dmp->i->dm_width < 873) {
	    if (pubvars->fontstruct->per_char->width != 8) {
		if ((newfontstruct = XLoadQueryFont(pubvars->dpy,
						    FONT8)) != NULL) {
		    XFreeFont(pubvars->dpy,
			      pubvars->fontstruct);
		    pubvars->fontstruct = newfontstruct;
		    glXUseXFont(pubvars->fontstruct->fid,
				0, 127, privars->fontOffset);
		}
	    }
	} else if (dmp->i->dm_width < 1455) {
	    if (pubvars->fontstruct->per_char->width != 9) {
		if ((newfontstruct = XLoadQueryFont(pubvars->dpy,
						    FONT9)) != NULL) {
		    XFreeFont(pubvars->dpy,
			      pubvars->fontstruct);
		    pubvars->fontstruct = newfontstruct;
		    glXUseXFont(pubvars->fontstruct->fid,
				0, 127, privars->fontOffset);
		}
	    }
	} else if (dmp->i->dm_width < 2037) {
	    if (pubvars->fontstruct->per_char->width != 10) {
		if ((newfontstruct = XLoadQueryFont(pubvars->dpy,
						    FONT10)) != NULL) {
		    XFreeFont(pubvars->dpy,
			      pubvars->fontstruct);
		    pubvars->fontstruct = newfontstruct;
		    glXUseXFont(pubvars->fontstruct->fid,
				0, 127, privars->fontOffset);
		}
	    }
	} else {
	    if (pubvars->fontstruct->per_char->width != 12) {
		if ((newfontstruct = XLoadQueryFont(pubvars->dpy,
						    FONT12)) != NULL) {
		    XFreeFont(pubvars->dpy,
			      pubvars->fontstruct);
		    pubvars->fontstruct = newfontstruct;
		    glXUseXFont(pubvars->fontstruct->fid,
				0, 127, privars->fontOffset);
		}
	    }
	}
    }

    return BRLCAD_OK;
}

static int
ogl_makeCurrent(struct dm *dmp)
{
    struct dm_glxvars *pubvars = (struct dm_glxvars *)dmp->i->dm_vars.pub_vars;
    struct pogl_vars *privars = (struct pogl_vars *)dmp->i->dm_vars.priv_vars;

    //gl_debug_print(dmp, "ogl_makeCurrent", dmp->i->dm_debugLevel);

    if (!glXMakeCurrent(pubvars->dpy,
			pubvars->win,
			privars->glxc)) {
	bu_log("ogl_makeCurrent: Couldn't make context current\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


static int
ogl_SwapBuffers(struct dm *dmp)
{
    struct dm_glxvars *pubvars = (struct dm_glxvars *)dmp->i->dm_vars.pub_vars;

    //gl_debug_print(dmp, "ogl_SwapBuffers", dmp->i->dm_debugLevel);

    glXSwapBuffers(pubvars->dpy, pubvars->win);

    return BRLCAD_OK;
}

static int
ogl_doevent(struct dm *dmp, void *UNUSED(vclientData), void *veventPtr)
{
    XEvent *eventPtr= (XEvent *)veventPtr;
    if (eventPtr->type == Expose && eventPtr->xexpose.count == 0) {
	(void)dm_make_current(dmp);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	dm_set_dirty(dmp, 1);
	return TCL_OK;
    }
    /* allow further processing of this event */
    return TCL_OK;
}

static int
ogl_configureWin(struct dm *dmp, int force)
{
    gl_debug_print(dmp, "ogl_configureWin", dmp->i->dm_debugLevel);

    if (dm_make_current(dmp) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }

    return ogl_configureWin_guts(dmp, force);
}

/**
 * currently, get a double buffered rgba visual that works with Tk and
 * OpenGL
 */
static XVisualInfo *
ogl_choose_visual(struct dm *dmp, Tk_Window tkwin)
{
    gl_debug_print(dmp, "ogl_choose_visual", dmp->i->dm_debugLevel);
    struct dm_glxvars *pubvars = (struct dm_glxvars *)dmp->i->dm_vars.pub_vars;
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    XVisualInfo *vip, vitemp, *vibase, *maxvip;
    int tries, baddepth;
    int num, i, j;
    int fail;
    int *good = NULL;

    /* requirements */
    int screen;
    int use;
    int rgba;
    int dbfr;

    /* desires */
    int m_zbuffer = 1; /* m_zbuffer - try to get zbuffer */
    int zbuffer;

    int m_stereo; /* m_stereo - try to get stereo */
    int stereo;

    if (dmp->i->dm_stereo) {
	m_stereo = 1;
    } else {
	m_stereo = 0;
    }

    memset((void *)&vitemp, 0, sizeof(XVisualInfo));
    /* Try to satisfy the above desires with a color visual of the
     * greatest depth */

    vibase = XGetVisualInfo(pubvars->dpy, 0, &vitemp, &num);
    screen = DefaultScreen(pubvars->dpy);

    good = (int *)bu_malloc(sizeof(int)*num, "alloc good visuals");

    while (1) {
	for (i=0, j=0, vip=vibase; i<num; i++, vip++) {
	    /* requirements */
	    if (vip->screen != screen)
		continue;

	    fail = glXGetConfig(pubvars->dpy,
				vip, GLX_USE_GL, &use);
	    if (fail || !use)
		continue;

	    fail = glXGetConfig(pubvars->dpy,
				vip, GLX_RGBA, &rgba);
	    if (fail || !rgba)
		continue;

	    fail = glXGetConfig(pubvars->dpy,
				vip, GLX_DOUBLEBUFFER, &dbfr);
	    if (fail || !dbfr)
		continue;

#ifdef HAVE_XRENDER
	    // https://stackoverflow.com/a/23836430
	    XRenderPictFormat *pict_format = XRenderFindVisualFormat(pubvars->dpy, vip->visual);
	    if(pict_format->direct.alphaMask > 0) {
		//printf("skipping visual with alphaMask\n");
		continue;
	    }
#endif

	    /* desires */
	    if (m_zbuffer) {
		fail = glXGetConfig(pubvars->dpy,
				    vip, GLX_DEPTH_SIZE, &zbuffer);
		if (fail || !zbuffer)
		    continue;
	    }

	    if (m_stereo) {
		fail = glXGetConfig(pubvars->dpy,
				    vip, GLX_STEREO, &stereo);
		if (fail || !stereo) {
		    bu_log("ogl_choose_visual: failed visual - GLX_STEREO\n");
		    continue;
		}
	    }

	    /* this visual meets criteria */
	    good[j++] = i;
	}

	/* j = number of acceptable visuals under consideration */
	if (j >= 1) {
	    baddepth = 1000;
	    for (tries = 0; tries < j; ++tries) {
		maxvip = vibase + good[0];
		for (i=1; i<j; i++) {
		    vip = vibase + good[i];
		    if ((vip->depth > maxvip->depth)&&(vip->depth < baddepth)) {
			maxvip = vip;
		    }
		}

		pubvars->cmap =
		    XCreateColormap(pubvars->dpy,
				    RootWindow(pubvars->dpy,
					       maxvip->screen), maxvip->visual, AllocNone);

		if (Tk_SetWindowVisual(tkwin,
				       maxvip->visual, maxvip->depth,
				       pubvars->cmap)) {

		    glXGetConfig(pubvars->dpy,
				 maxvip, GLX_DEPTH_SIZE,
				 &mvars->depth);
		    if (mvars->depth > 0)
			mvars->zbuf = 1;

		    bu_free(good, "dealloc good visuals");
		    return maxvip; /* success */
		} else {
		    /* retry with lesser depth */
		    baddepth = maxvip->depth;
		    XFreeColormap(pubvars->dpy,
				  pubvars->cmap);
		}
	    }
	}

	/* if no success at this point, relax a desire and try again */

	if (m_stereo) {
	    m_stereo = 0;
	    bu_log("Stereo not available.\n");
	    continue;
	}

	if (m_zbuffer) {
	    m_zbuffer = 0;
	    continue;
	}

	/* ran out of visuals, give up */
	break;
    }

    bu_free(good, "dealloc good visuals");
    return (XVisualInfo *)NULL; /* failure */
}

/*
 * Gracefully release the display.
 */
static int
ogl_close(struct dm *dmp)
{
    struct dm_glxvars *pubvars = (struct dm_glxvars *)dmp->i->dm_vars.pub_vars;
    struct pogl_vars *privars = (struct pogl_vars *)dmp->i->dm_vars.priv_vars;

    gl_debug_print(dmp, "ogl_close", dmp->i->dm_debugLevel);

    if (pubvars->dpy) {
	if (privars->glxc) {
	    glXMakeCurrent(pubvars->dpy, None, NULL);
	    glXDestroyContext(pubvars->dpy,
			      privars->glxc);
	}

	if (pubvars->cmap)
	    XFreeColormap(pubvars->dpy,
			  pubvars->cmap);

	if (pubvars->xtkwin)
	    Tk_DestroyWindow(pubvars->xtkwin);
    }

    bu_vls_free(&dmp->i->dm_pathName);
    bu_vls_free(&dmp->i->dm_tkName);
    bu_vls_free(&dmp->i->dm_dName);
    bu_free(dmp->i->dm_vars.priv_vars, "ogl_close: ogl_vars");
    bu_free(dmp->i->dm_vars.pub_vars, "ogl_close: dm_glxvars");
    BU_PUT(dmp->i, struct dm_impl);
    BU_PUT(dmp, struct dm);

    return BRLCAD_OK;
}

int
ogl_viable(const char *dpy_string)
{
    Display *dpy;
    int return_val;
    if ((dpy = XOpenDisplay(dpy_string)) != NULL) {
	if (XQueryExtension(dpy, "GLX", &return_val, &return_val, &return_val)) {
	    XCloseDisplay(dpy);
	    return 1;
	}
	XCloseDisplay(dpy);
    }
    return -1;
}

/*
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
ogl_open(void *UNUSED(ctx), void *vinterp, int argc, const char **argv)
{
    static int count = 0;
    GLfloat backgnd[4];
    int make_square = -1;
    Tcl_Interp *interp = (Tcl_Interp *)vinterp;

#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
    int j, k;
    int ndevices;
    int nclass = 0;
    int unused;
    XDeviceInfoPtr olist = NULL, list = NULL;
    XDevice *dev = NULL;
    XEventClass e_class[15];
    XInputClassInfo *cip;
#endif

    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls init_proc_vls = BU_VLS_INIT_ZERO;
    Display *tmp_dpy = (Display *)NULL;
    struct dm *dmp = NULL;
    struct dm_impl *dmpi = NULL;
    struct gl_vars *mvars = NULL;
    Tk_Window tkwin = (Tk_Window)NULL;
    int screen_number = -1;

    struct dm_glxvars *pubvars = NULL;
    struct pogl_vars *privvars = NULL;

    if ((tkwin = Tk_MainWindow(interp)) == NULL) {
	return DM_NULL;
    }

    BU_GET(dmp, struct dm);
    dmp->magic = DM_MAGIC;
    dmp->start_time = 0;

    BU_GET(dmpi, struct dm_impl);
    *dmpi = *dm_ogl.i; /* struct copy */
    dmp->i = dmpi;

    dmp->i->dm_interp = interp;
    dmp->i->dm_lineWidth = 1;
    dmp->i->dm_bytes_per_pixel = sizeof(GLuint);
    dmp->i->dm_bits_per_channel = 8;
    bu_vls_init(&(dmp->i->dm_log));

    BU_ALLOC(dmp->i->dm_vars.pub_vars, struct dm_glxvars);
    pubvars = (struct dm_glxvars *)dmp->i->dm_vars.pub_vars;

    BU_ALLOC(dmp->i->dm_vars.priv_vars, struct pogl_vars);
    privvars = (struct pogl_vars *)dmp->i->dm_vars.priv_vars;

    dmp->i->dm_get_internal(dmp);
    glvars_init(dmp);
    mvars = (struct gl_vars *)dmp->i->m_vars;

    dmp->i->dm_vp = &mvars->i.default_viewscale;

    bu_vls_init(&dmp->i->dm_pathName);
    bu_vls_init(&dmp->i->dm_tkName);
    bu_vls_init(&dmp->i->dm_dName);

    dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

    if (bu_vls_strlen(&dmp->i->dm_pathName) == 0)
	bu_vls_printf(&dmp->i->dm_pathName, ".dm_ogl%d", count);
    ++count;
    if (bu_vls_strlen(&dmp->i->dm_dName) == 0) {
	char *dp;

	dp = getenv("DISPLAY");
	if (dp)
	    bu_vls_strcpy(&dmp->i->dm_dName, dp);
	else
	    bu_vls_strcpy(&dmp->i->dm_dName, ":0.0");
    }

    /* initialize dm specific variables */
    pubvars->devmotionnotify = LASTEvent;
    pubvars->devbuttonpress = LASTEvent;
    pubvars->devbuttonrelease = LASTEvent;
    dmp->i->dm_aspect = 1.0;

    /* initialize modifiable variables */
    mvars->rgb = 1;
    mvars->doublebuffer = 1;
    mvars->fastfog = 1;
    mvars->fogdensity = 1.0;
    mvars->lighting_on = 1;
    mvars->zclipping_on = 0;
    mvars->bound = 1.0;
    mvars->boundFlag = 1;

    /* this is important so that ogl_configureWin knows to set the font */
    pubvars->fontstruct = NULL;

    if ((tmp_dpy = XOpenDisplay(bu_vls_addr(&dmp->i->dm_dName))) == NULL) {
	bu_vls_free(&init_proc_vls);
	(void)ogl_close(dmp);
	return DM_NULL;
    }

#ifdef HAVE_XQUERYEXTENSION
    {
	int return_val;

	if (!XQueryExtension(tmp_dpy, "GLX", &return_val, &return_val, &return_val)) {
	    bu_vls_free(&init_proc_vls);
	    (void)ogl_close(dmp);
	    return DM_NULL;
	}
    }
#endif

    screen_number = XDefaultScreen(tmp_dpy);
    if (screen_number < 0)
	bu_log("WARNING: screen number is [%d]\n", screen_number);

    if (dmp->i->dm_width == 0) {
	dmp->i->dm_width = XDisplayWidth(tmp_dpy, screen_number) - 30;
	++make_square;
    }
    if (dmp->i->dm_height == 0) {
	dmp->i->dm_height = XDisplayHeight(tmp_dpy, screen_number) - 30;
	++make_square;
    }

    if (make_square > 0) {
	/* Make window square */
	if (dmp->i->dm_height <
	    dmp->i->dm_width)
	    dmp->i->dm_width =
		dmp->i->dm_height;
	else
	    dmp->i->dm_height =
		dmp->i->dm_width;
    }

    XCloseDisplay(tmp_dpy);

    if (dmp->i->dm_top) {
	/* Make xtkwin a toplevel window */
	pubvars->xtkwin =
	    Tk_CreateWindowFromPath(interp,
				    tkwin,
				    bu_vls_addr(&dmp->i->dm_pathName),
				    bu_vls_addr(&dmp->i->dm_dName));
	pubvars->top = pubvars->xtkwin;
    } else {
	char *cp;

	cp = strrchr(bu_vls_addr(&dmp->i->dm_pathName), (int)'.');
	if (cp == bu_vls_addr(&dmp->i->dm_pathName)) {
	    pubvars->top = tkwin;
	} else {
	    struct bu_vls top_vls = BU_VLS_INIT_ZERO;

	    bu_vls_strncpy(&top_vls, (const char *)bu_vls_addr(&dmp->i->dm_pathName), cp - bu_vls_addr(&dmp->i->dm_pathName));

	    pubvars->top =
		Tk_NameToWindow(interp, bu_vls_addr(&top_vls), tkwin);
	    bu_vls_free(&top_vls);
	}

	/* Make xtkwin an embedded window */
	pubvars->xtkwin =
	    Tk_CreateWindow(interp, pubvars->top,
			    cp + 1, (char *)NULL);
    }

    if (pubvars->xtkwin == NULL) {
	bu_log("dm-Ogl: Failed to open %s\n", bu_vls_addr(&dmp->i->dm_pathName));
	bu_vls_free(&init_proc_vls);
	(void)ogl_close(dmp);
	return DM_NULL;
    }

    bu_vls_printf(&dmp->i->dm_tkName, "%s",
		  (char *)Tk_Name(pubvars->xtkwin));

    Tk_SetWindowBackground(pubvars->xtkwin, BlackPixelOfScreen(Tk_Screen(pubvars->xtkwin)));

    if (bu_vls_strlen(&init_proc_vls) > 0) {
	bu_vls_printf(&str, "%s %s\n", bu_vls_addr(&init_proc_vls), bu_vls_addr(&dmp->i->dm_pathName));

	if (Tcl_Eval(interp, bu_vls_addr(&str)) == BRLCAD_ERROR) {
	    bu_vls_free(&init_proc_vls);
	    bu_vls_free(&str);
	    (void)ogl_close(dmp);
	    return DM_NULL;
	}
    }

    bu_vls_free(&init_proc_vls);
    bu_vls_free(&str);

    pubvars->dpy =
	Tk_Display(pubvars->top);

    /* make sure there really is a display before proceeding. */
    if (!(pubvars->dpy)) {
	bu_vls_free(&init_proc_vls);
	bu_vls_free(&str);
	(void)ogl_close(dmp);
	return DM_NULL;
    }

    Tk_GeometryRequest(pubvars->xtkwin,
		       dmp->i->dm_width,
		       dmp->i->dm_height);

    /* must do this before MakeExist */
    if ((pubvars->vip=ogl_choose_visual(dmp, pubvars->xtkwin)) == NULL) {
	bu_log("ogl_open: Can't get an appropriate visual.\n");
	(void)ogl_close(dmp);
	return DM_NULL;
    }

    pubvars->depth = mvars->depth;

    Tk_MakeWindowExist(pubvars->xtkwin);

    pubvars->win = Tk_WindowId(pubvars->xtkwin);
    dmp->i->dm_id = pubvars->win;

    /* open GLX context */
    if ((privvars->glxc =
	 glXCreateContext(pubvars->dpy, pubvars->vip, (GLXContext)NULL, GL_TRUE))==NULL) {
	bu_log("ogl_open: couldn't create glXContext.\n");
	(void)ogl_close(dmp);
	return DM_NULL;
    }

    /* If we used an indirect context, then as far as sgi is concerned,
     * gl hasn't been used.
     */
    privvars->is_direct = (char) glXIsDirect(pubvars->dpy, privvars->glxc);

#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
    /*
     * Take a look at the available input devices. We're looking
     * for "dial+buttons".
     */
    if (XQueryExtension(pubvars->dpy, "XInputExtension", &unused, &unused, &unused)) {
	olist = list = (XDeviceInfoPtr)XListInputDevices(pubvars->dpy, &ndevices);
    }

    if (list == (XDeviceInfoPtr)NULL ||
	list == (XDeviceInfoPtr)1) goto Done;

    for (j = 0; j < ndevices; ++j, list++) {
	if (list->use == IsXExtensionDevice) {
	    if (BU_STR_EQUAL(list->name, "dial+buttons")) {
		if ((dev = XOpenDevice(pubvars->dpy,
				       list->id)) == (XDevice *)NULL) {
		    bu_log("ogl_open: Couldn't open the dials+buttons\n");
		    goto Done;
		}

		for (cip = dev->classes, k = 0; k < dev->num_classes;
		     ++k, ++cip) {
		    switch (cip->input_class) {
#ifdef IR_BUTTONS
			case ButtonClass:
			    DeviceButtonPress(dev, pubvars->devbuttonpress,
					      e_class[nclass]);
			    ++nclass;
			    DeviceButtonRelease(dev, pubvars->devbuttonrelease,
						e_class[nclass]);
			    ++nclass;
			    break;
#endif
#ifdef IR_KNOBS
			case ValuatorClass:
			    DeviceMotionNotify(dev, pubvars->devmotionnotify,
					       e_class[nclass]);
			    ++nclass;
			    break;
#endif
			default:
			    break;
		    }
		}

		XSelectExtensionEvent(pubvars->dpy, pubvars->win, e_class, nclass);
		goto Done;
	    }
	}
    }
Done:
    XFreeDeviceList(olist);

#endif /* HAVE_X11_EXTENSIONS_XINPUT_H */

    Tk_MapWindow(pubvars->xtkwin);

    if (dm_make_current(dmp) != BRLCAD_OK) {
	(void)ogl_close(dmp);
	return DM_NULL;
    }

    /* display list (fontOffset + char) will display a given ASCII char */
    if ((privvars->fontOffset = glGenLists(128))==0) {
	bu_log("dm-ogl: Can't make display lists for font.\n");
	(void)ogl_close(dmp);
	return DM_NULL;
    }

    /* This is the applications display list offset */
    dmp->i->dm_displaylist = privvars->fontOffset + 128;


    // FOR INITIALIZATION DEBUGGING - enable debugging/logging from the beginning
    //dmp->i->dm_debugLevel = 5;
    //bu_vls_sprintf(&dmp->i->dm_log, "mged.log");


    gl_setBGColor(dmp, 0, 0, 0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (mvars->doublebuffer)
	glDrawBuffer(GL_BACK);
    else
	glDrawBuffer(GL_FRONT);

    /* do viewport, ortho commands and initialize font */
    (void)ogl_configureWin_guts(dmp, 1);

    /* Lines will be solid when stippling disabled, dashed when enabled*/
    glLineStipple(1, 0xCF33);
    glDisable(GL_LINE_STIPPLE);

    backgnd[0] = backgnd[1] = backgnd[2] = backgnd[3] = 0.0;
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0.0);
    glFogf(GL_FOG_END, 2.0);
    glFogfv(GL_FOG_COLOR, backgnd);

    /*XXX Need to do something about VIEWFACTOR */
    glFogf(GL_FOG_DENSITY, VIEWFACTOR);

    /* Initialize matrices */
    /* Leave it in model_view mode normally */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-mvars->i.xlim_view, mvars->i.xlim_view, -mvars->i.ylim_view, mvars->i.ylim_view, 0.0, 2.0);
    glGetDoublev(GL_PROJECTION_MATRIX, mvars->i.faceplate_mat);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glPushMatrix();
    glLoadIdentity();
    mvars->i.faceFlag = 1;	/* faceplate matrix is on top of stack */

    gl_setZBuffer(dmp, mvars->zbuffer_on);
    gl_setLight(dmp, mvars->lighting_on);

    return dmp;
}


int
ogl_share_dlist(struct dm *dmp1, struct dm *dmp2)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp1->i->m_vars;
    struct pogl_vars *privars = (struct pogl_vars *)dmp1->i->dm_vars.priv_vars;
    GLfloat backgnd[4];
    GLfloat vf;
    GLXContext old_glxContext;

    if (dmp1 == (struct dm *)NULL)
	return BRLCAD_ERROR;

    if (dmp2 == (struct dm *)NULL) {
	/* create a new graphics context for dmp1 with private display lists */

	old_glxContext = privars->glxc;

	if ((privars->glxc =
	     glXCreateContext(((struct dm_glxvars *)dmp1->i->dm_vars.pub_vars)->dpy,
			      ((struct dm_glxvars *)dmp1->i->dm_vars.pub_vars)->vip,
			      (GLXContext)NULL, GL_TRUE))==NULL) {
	    bu_log("ogl_share_dlist: couldn't create glXContext.\nUsing old context\n.");
	    privars->glxc = old_glxContext;

	    return BRLCAD_ERROR;
	}

	if (dm_make_current(dmp1) != BRLCAD_OK) {
	    bu_log("ogl_share_dlist: Couldn't make context current\nUsing old context\n.");
	    privars->glxc = old_glxContext;

	    return BRLCAD_ERROR;
	}

	/* display list (fontOffset + char) will display a given ASCII char */
	if ((privars->fontOffset = glGenLists(128))==0) {
	    bu_log("dm-ogl: Can't make display lists for font.\nUsing old context\n.");
	    privars->glxc = old_glxContext;

	    return BRLCAD_ERROR;
	}

	/* This is the applications display list offset */
	dmp1->i->dm_displaylist = privars->fontOffset + 128;

	gl_setBGColor(dmp1, 0, 0, 0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (mvars->doublebuffer)
	    glDrawBuffer(GL_BACK);
	else
	    glDrawBuffer(GL_FRONT);

	/* this is important so that ogl_configureWin knows to set the font */
	((struct dm_glxvars *)dmp1->i->dm_vars.pub_vars)->fontstruct = NULL;

	/* do viewport, ortho commands and initialize font */
	(void)ogl_configureWin_guts(dmp1, 1);

	/* Lines will be solid when stippling disabled, dashed when enabled*/
	glLineStipple(1, 0xCF33);
	glDisable(GL_LINE_STIPPLE);

	backgnd[0] = backgnd[1] = backgnd[2] = backgnd[3] = 0.0;
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, 0.0);
	glFogf(GL_FOG_END, 2.0);
	glFogfv(GL_FOG_COLOR, backgnd);

	/*XXX Need to do something about VIEWFACTOR */
	vf = 1.0/(*dmp1->i->dm_vp);
	glFogf(GL_FOG_DENSITY, vf);

	/* Initialize matrices */
	/* Leave it in model_view mode normally */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-mvars->i.xlim_view, mvars->i.xlim_view, -mvars->i.ylim_view, mvars->i.ylim_view, 0.0, 2.0);
	glGetDoublev(GL_PROJECTION_MATRIX, mvars->i.faceplate_mat);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPushMatrix();
	glLoadIdentity();
	mvars->i.faceFlag = 1;	/* faceplate matrix is on top of stack */

	/* destroy old context */
	dm_make_current(dmp1);
	glXDestroyContext(((struct dm_glxvars *)dmp1->i->dm_vars.pub_vars)->dpy, old_glxContext);
    } else {
	/* dmp1 will share its display lists with dmp2 */

	if (!BU_STR_EQUAL(dmp1->i->dm_name, dmp2->i->dm_name)) {
	    return BRLCAD_ERROR;
	}
	if (bu_vls_strcmp(&dmp1->i->dm_dName, &dmp2->i->dm_dName)) {
	    return BRLCAD_ERROR;
	}

	old_glxContext = ((struct pogl_vars *)dmp2->i->dm_vars.priv_vars)->glxc;

	if ((((struct pogl_vars *)dmp2->i->dm_vars.priv_vars)->glxc =
	     glXCreateContext(((struct dm_glxvars *)dmp2->i->dm_vars.pub_vars)->dpy,
			      ((struct dm_glxvars *)dmp2->i->dm_vars.pub_vars)->vip,
			      privars->glxc,
			      GL_TRUE))==NULL) {
	    bu_log("ogl_share_dlist: couldn't create glXContext.\nUsing old context\n.");
	    ((struct pogl_vars *)dmp2->i->dm_vars.priv_vars)->glxc = old_glxContext;

	    return BRLCAD_ERROR;
	}

	if (dm_make_current(dmp2) != BRLCAD_OK) {
	    bu_log("ogl_share_dlist: Couldn't make context current\nUsing old context\n.");
	    ((struct pogl_vars *)dmp2->i->dm_vars.priv_vars)->glxc = old_glxContext;

	    return BRLCAD_ERROR;
	}

	((struct pogl_vars *)dmp2->i->dm_vars.priv_vars)->fontOffset = privars->fontOffset;
	dmp2->i->dm_displaylist = dmp1->i->dm_displaylist;

	gl_setBGColor(dmp2, 0, 0, 0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (mvars->doublebuffer)
	    glDrawBuffer(GL_BACK);
	else
	    glDrawBuffer(GL_FRONT);

	/* do viewport, ortho commands and initialize font */
	(void)ogl_configureWin_guts(dmp2, 1);

	/* Lines will be solid when stippling disabled, dashed when enabled*/
	glLineStipple(1, 0xCF33);
	glDisable(GL_LINE_STIPPLE);

	backgnd[0] = backgnd[1] = backgnd[2] = backgnd[3] = 0.0;
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, 0.0);
	glFogf(GL_FOG_END, 2.0);
	glFogfv(GL_FOG_COLOR, backgnd);

	/*XXX Need to do something about VIEWFACTOR */
	vf = 1.0/(*dmp2->i->dm_vp);
	glFogf(GL_FOG_DENSITY, vf);

	/* Initialize matrices */
	/* Leave it in model_view mode normally */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-mvars->i.xlim_view, mvars->i.xlim_view, -mvars->i.ylim_view, mvars->i.ylim_view, 0.0, 2.0);
	glGetDoublev(GL_PROJECTION_MATRIX, ((struct gl_vars *)dmp2->i->m_vars)->i.faceplate_mat);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPushMatrix();
	glLoadIdentity();
	((struct gl_vars *)dmp2->i->m_vars)->i.faceFlag = 1; /* faceplate matrix is on top of stack */

	/* destroy old context */
	dm_make_current(dmp2);
	glXDestroyContext(((struct dm_glxvars *)dmp2->i->dm_vars.pub_vars)->dpy, old_glxContext);
    }

    return BRLCAD_OK;
}

/*
 * Output a string.
 * The starting position of the beam is as specified.
 */
static int
ogl_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int UNUSED(size), int use_aspect)
{
    struct pogl_vars *privars = (struct pogl_vars *)dmp->i->dm_vars.priv_vars;
    gl_debug_print(dmp, "ogl_drawString2D", dmp->i->dm_debugLevel);

    if (use_aspect)
	glRasterPos2f(x, y * dmp->i->dm_aspect);
    else
	glRasterPos2f(x, y);

    glListBase(privars->fontOffset);
    glCallLists(strlen(str), GL_UNSIGNED_BYTE,  str);

    return BRLCAD_OK;
}

int
ogl_openFb(struct dm *dmp)
{
    struct fb_platform_specific *fb_ps;
    struct ogl_fb_info *ofb_ps;
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    struct dm_glxvars *pubvars = (struct dm_glxvars *)dmp->i->dm_vars.pub_vars;
    struct pogl_vars *privars = (struct pogl_vars *)dmp->i->dm_vars.priv_vars;
    gl_debug_print(dmp, "ogl_openFb", dmp->i->dm_debugLevel);

    fb_ps = fb_get_platform_specific(FB_OGL_MAGIC);
    ofb_ps = (struct ogl_fb_info *)fb_ps->data;
    ofb_ps->dpy = pubvars->dpy;
    ofb_ps->win = pubvars->win;
    ofb_ps->cmap = pubvars->cmap;
    ofb_ps->vip = pubvars->vip;
    ofb_ps->glxc = privars->glxc;
    ofb_ps->double_buffer = mvars->doublebuffer;
    ofb_ps->soft_cmap = 0;
    dmp->i->fbp = fb_open_existing("ogl", dm_get_width(dmp), dm_get_height(dmp), fb_ps);
    fb_put_platform_specific(fb_ps);

    if (dmp->i->dm_debugLevel > 1)
	gl_debug_print(dmp, "ogl_openFb after:", dmp->i->dm_debugLevel);

    return 0;
}

int
ogl_geometry_request(struct dm *dmp, int width, int height)
{
    if (!dmp) return -1;
    Tk_GeometryRequest(((struct dm_glxvars *)dmp->i->dm_vars.pub_vars)->xtkwin, width, height);
    return 0;
}

#define GLXVARS_MV_O(_m) offsetof(struct dm_glxvars, _m)

struct bu_structparse dm_glxvars_vparse[] = {
    {"%x",      1,      "dpy",                  GLXVARS_MV_O(dpy),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "win",                  GLXVARS_MV_O(win),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "top",                  GLXVARS_MV_O(top),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "tkwin",                GLXVARS_MV_O(xtkwin),     BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "depth",                GLXVARS_MV_O(depth),      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "cmap",                 GLXVARS_MV_O(cmap),       BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "vip",                  GLXVARS_MV_O(vip),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "fontstruct",           GLXVARS_MV_O(fontstruct), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devmotionnotify",      GLXVARS_MV_O(devmotionnotify),    BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devbuttonpress",       GLXVARS_MV_O(devbuttonpress),     BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devbuttonrelease",     GLXVARS_MV_O(devbuttonrelease),   BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",        0,      (char *)0,              0,                      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

int
ogl_internal_var(struct bu_vls *result, struct dm *dmp, const char *key)
{
    if (!dmp || !result) return -1;
    if (!key) {
        // Print all current vars
        bu_vls_struct_print2(result, "dm internal GLX variables", dm_glxvars_vparse, (const char *)dmp->i->dm_vars.pub_vars);
        return 0;
    }
    // Print specific var
    bu_vls_struct_item_named(result, dm_glxvars_vparse, key, (const char *)dmp->i->dm_vars.pub_vars, ',');
    return 0;
}


// TODO - this and getDisplayImage need to be consolidated...
int
ogl_write_image(struct bu_vls *msgs, FILE *fp, struct dm *dmp)
{
    png_structp png_p;
    png_infop info_p;
    XImage *ximage_p;
    unsigned char **rows;
    unsigned char *idata;
    unsigned char *irow;
    int bytes_per_pixel;
    int bits_per_channel = 8;  /* bits per color channel */
    int i, j, k;
    unsigned char *dbyte0, *dbyte1, *dbyte2, *dbyte3;
    int red_shift;
    int green_shift;
    int blue_shift;
    int red_bits;
    int green_bits;
    int blue_bits;

    png_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p) {
	if (msgs) {
	    bu_vls_printf(msgs, "ogl_write_image: could not create PNG write structure\n");
	}
	return -1;
    }

    info_p = png_create_info_struct(png_p);
    if (!info_p) {
	if (msgs) {
	    bu_vls_printf(msgs, "ogl_write_image: could not create PNG info structure\n");
	}
	png_destroy_write_struct(&png_p, NULL);
	return -1;
    }

    ximage_p = XGetImage(((struct dm_glxvars *)dmp->i->dm_vars.pub_vars)->dpy,
			 ((struct dm_glxvars *)dmp->i->dm_vars.pub_vars)->win,
			 0, 0,
			 dmp->i->dm_width,
			 dmp->i->dm_height,
			 ~0, ZPixmap);
    if (!ximage_p) {
	if (msgs) {
	    bu_vls_printf(msgs, "png: could not get XImage\n");
	}
	png_destroy_write_struct(&png_p, &info_p);
	return -1;
    }

    bytes_per_pixel = ximage_p->bytes_per_line / ximage_p->width;

    if (bytes_per_pixel == 4) {
	unsigned long mask;
	unsigned long tmask;

	/* This section assumes 8 bits per channel */

	mask = ximage_p->red_mask;
	tmask = 1;
	for (red_shift = 0; red_shift < 32; red_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}

	mask = ximage_p->green_mask;
	tmask = 1;
	for (green_shift = 0; green_shift < 32; green_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}

	mask = ximage_p->blue_mask;
	tmask = 1;
	for (blue_shift = 0; blue_shift < 32; blue_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}

	/*
	 * We need to reverse things if the image byte order
	 * is different from the system's byte order.
	 */
	if (((bu_byteorder() == BU_BIG_ENDIAN) && (ximage_p->byte_order == LSBFirst)) ||
	    ((bu_byteorder() == BU_LITTLE_ENDIAN) && (ximage_p->byte_order == MSBFirst))) {
	    DM_REVERSE_COLOR_BYTE_ORDER(red_shift, ximage_p->red_mask);
	    DM_REVERSE_COLOR_BYTE_ORDER(green_shift, ximage_p->green_mask);
	    DM_REVERSE_COLOR_BYTE_ORDER(blue_shift, ximage_p->blue_mask);
	}

    } else if (bytes_per_pixel == 2) {
	unsigned long mask;
	unsigned long tmask;
	int bpb = 8;   /* bits per byte */

	/*XXX
	 * This section probably needs logic similar
	 * to the previous section (i.e. bytes_per_pixel == 4).
	 * That is we may need to reverse things depending on
	 * the image byte order and the system's byte order.
	 */

	mask = ximage_p->red_mask;
	tmask = 1;
	for (red_shift = 0; red_shift < 16; red_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}
	for (red_bits = red_shift; red_bits < 16; red_bits++) {
	    if (!(tmask & mask))
		break;
	    tmask = tmask << 1;
	}

	red_bits = red_bits - red_shift;
	if (red_shift == 0)
	    red_shift = red_bits - bpb;
	else
	    red_shift = red_shift - (bpb - red_bits);

	mask = ximage_p->green_mask;
	tmask = 1;
	for (green_shift = 0; green_shift < 16; green_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}
	for (green_bits = green_shift; green_bits < 16; green_bits++) {
	    if (!(tmask & mask))
		break;
	    tmask = tmask << 1;
	}

	green_bits = green_bits - green_shift;
	green_shift = green_shift - (bpb - green_bits);

	mask = ximage_p->blue_mask;
	tmask = 1;
	for (blue_shift = 0; blue_shift < 16; blue_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}
	for (blue_bits = blue_shift; blue_bits < 16; blue_bits++) {
	    if (!(tmask & mask))
		break;
	    tmask = tmask << 1;
	}
	blue_bits = blue_bits - blue_shift;

	if (blue_shift == 0)
	    blue_shift = blue_bits - bpb;
	else
	    blue_shift = blue_shift - (bpb - blue_bits);
    } else {
	if (msgs) {
	    bu_vls_printf(msgs, "png: %d bytes per pixel is not yet supported\n", bytes_per_pixel);
	}
	png_destroy_write_struct(&png_p, &info_p);
	return -1;
    }

    rows = (unsigned char **)bu_calloc(ximage_p->height, sizeof(unsigned char *), "rows");
    idata = (unsigned char *)bu_calloc(ximage_p->height * ximage_p->width, 4, "png data");

    /* for each scanline */
    for (i = ximage_p->height - 1, j = 0; 0 <= i; --i, ++j) {
	/* irow points to the current scanline in ximage_p */
	irow = (unsigned char *)(ximage_p->data + ((ximage_p->height-i-1)*ximage_p->bytes_per_line));

	if (bytes_per_pixel == 4) {
	    unsigned int pixel;

	    /* rows[j] points to the current scanline in idata */
	    rows[j] = (unsigned char *)(idata + ((ximage_p->height-i-1)*ximage_p->bytes_per_line));

	    /* for each pixel in current scanline of ximage_p */
	    for (k = 0; k < ximage_p->bytes_per_line; k += bytes_per_pixel) {
		pixel = *((unsigned int *)(irow + k));

		dbyte0 = rows[j] + k;
		dbyte1 = dbyte0 + 1;
		dbyte2 = dbyte0 + 2;
		dbyte3 = dbyte0 + 3;

		*dbyte0 = (pixel & ximage_p->red_mask) >> red_shift;
		*dbyte1 = (pixel & ximage_p->green_mask) >> green_shift;
		*dbyte2 = (pixel & ximage_p->blue_mask) >> blue_shift;
		*dbyte3 = 255;
	    }
	} else if (bytes_per_pixel == 2) {
	    unsigned short spixel;
	    unsigned long pixel;

	    /* rows[j] points to the current scanline in idata */
	    rows[j] = (unsigned char *)(idata + ((ximage_p->height-i-1)*ximage_p->bytes_per_line*2));

	    /* for each pixel in current scanline of ximage_p */
	    for (k = 0; k < ximage_p->bytes_per_line; k += bytes_per_pixel) {
		spixel = *((unsigned short *)(irow + k));
		pixel = spixel;

		dbyte0 = rows[j] + k*2;
		dbyte1 = dbyte0 + 1;
		dbyte2 = dbyte0 + 2;
		dbyte3 = dbyte0 + 3;

		if (0 <= red_shift)
		    *dbyte0 = (pixel & ximage_p->red_mask) >> red_shift;
		else
		    *dbyte0 = (pixel & ximage_p->red_mask) << -red_shift;

		*dbyte1 = (pixel & ximage_p->green_mask) >> green_shift;

		if (0 <= blue_shift)
		    *dbyte2 = (pixel & ximage_p->blue_mask) >> blue_shift;
		else
		    *dbyte2 = (pixel & ximage_p->blue_mask) << -blue_shift;

		*dbyte3 = 255;
	    }
	} else {
	    bu_free(rows, "rows");
	    bu_free(idata, "image data");
	    png_destroy_write_struct(&png_p, &info_p);
	    if (msgs) {
		bu_vls_printf(msgs, "png: not supported for this platform\n");
	    }
	    return -1;
	}
    }

    png_init_io(png_p, fp);
    png_set_filter(png_p, 0, PNG_FILTER_NONE);
    png_set_compression_level(png_p, 9);
    png_set_IHDR(png_p, info_p, ximage_p->width, ximage_p->height, bits_per_channel,
		 PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_gAMA(png_p, info_p, 0.77);
    png_write_info(png_p, info_p);
    png_write_image(png_p, rows);
    png_write_end(png_p, NULL);

    png_destroy_write_struct(&png_p, &info_p);

    bu_free(rows, "rows");
    bu_free(idata, "image data");

    return 0;
}

int
ogl_event_cmp(struct dm *dmp, dm_event_t type, int event)
{
    struct dm_glxvars *pubvars = (struct dm_glxvars *)dmp->i->dm_vars.pub_vars;
    switch (type) {
	case DM_MOTION_NOTIFY:
	    return (event == pubvars->devmotionnotify) ? 1 : 0;
	    break;
	case DM_BUTTON_PRESS:
	    return (event == pubvars->devbuttonpress) ? 1 : 0;
	    break;
	case DM_BUTTON_RELEASE:
	    return (event == pubvars->devbuttonrelease) ? 1 : 0;
	    break;
	default:
	    return -1;
	    break;
    };
}

struct dm_impl dm_ogl_impl = {
    ogl_open,
    ogl_close,
    ogl_viable,
    gl_drawBegin,
    gl_drawEnd,
    gl_hud_begin,
    gl_hud_end,
    gl_loadMatrix,
    gl_loadPMatrix,
    gl_popPMatrix,
    ogl_drawString2D,
    null_String2DBBox,
    gl_drawLine2D,
    gl_drawLine3D,
    gl_drawLines3D,
    gl_drawPoint2D,
    gl_drawPoint3D,
    gl_drawPoints3D,
    gl_drawVList,
    gl_drawVListHiddenLine,
    null_draw_obj,
    gl_draw_data_axes,
    gl_draw,
    gl_setFGColor,
    gl_setBGColor,
    gl_setLineAttr,
    ogl_configureWin,
    gl_setWinBounds,
    gl_setLight,
    gl_getLight,
    gl_setTransparency,
    gl_getTransparency,
    gl_setDepthMask,
    gl_setZBuffer,
    gl_getZBuffer,
    gl_setZClip,
    gl_getZClip,
    gl_setBound,
    gl_getBound,
    gl_setBoundFlag,
    gl_getBoundFlag,
    gl_debug,
    gl_logfile,
    gl_beginDList,
    gl_endDList,
    gl_drawDList,
    gl_freeDLists,
    gl_genDLists,
    gl_draw_display_list,
    gl_getDisplayImage, /* display to image function */
    gl_reshape,
    ogl_makeCurrent,
    ogl_SwapBuffers,
    ogl_doevent,
    ogl_openFb,
    gl_get_internal,
    gl_put_internal,
    ogl_geometry_request,
    ogl_internal_var,
    ogl_write_image,
    NULL,
    NULL,
    ogl_event_cmp,
    gl_fogHint,
    ogl_share_dlist,
    0,
    1,				/* is graphical */
    "Tk",                       /* uses Tk graphics system */
    1,				/* has displaylist */
    0,                          /* no stereo by default */
    "ogl",
    "X Windows with OpenGL graphics",
    1, /* top */
    0, /* width */
    0, /* height */
    0, /* dirty */
    0, /* bytes per pixel */
    0, /* bits per channel */
    0,
    0,
    1.0, /* aspect ratio */
    0,
    {0, 0},
    NULL,
    NULL,
    BU_VLS_INIT_ZERO,		/* bu_vls path name*/
    BU_VLS_INIT_ZERO,		/* bu_vls full name drawing window */
    BU_VLS_INIT_ZERO,		/* bu_vls short name drawing window */
    BU_VLS_INIT_ZERO,		/* bu_vls logfile */
    {0, 0, 0},			/* bg1 color */
    {0, 0, 0},			/* bg2 color */
    {0, 0, 0},			/* fg color */
    {GED_MIN, GED_MIN, GED_MIN},	/* clipmin */
    {GED_MAX, GED_MAX, GED_MAX},	/* clipmax */
    0,				/* no debugging */
    0,				/* no perspective */
    1,				/* depth buffer is writable */
    0,                          /* clear back buffer after drawing and swap */
    0,                          /* not overriding the auto font size */
    gl_vparse,
    FB_NULL,
    0,				/* Tcl interpreter */
    NULL,                       /* Drawing context */
    NULL                        /* App data */
};

struct dm dm_ogl = { DM_MAGIC, &dm_ogl_impl, 0 };

#ifdef DM_PLUGIN
static const struct dm_plugin pinfo = { DM_API, &dm_ogl };

COMPILER_DLLEXPORT const struct dm_plugin *dm_plugin_info(void)
{
    return &pinfo;
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

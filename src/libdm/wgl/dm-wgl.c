/*                       D M - W G L . C
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
/** @file libdm/dm-wgl.c
 *
 *  A Windows OpenGL (wgl) Display Manager.
 *
 */

#include "common.h"

#include "tk.h"
/* needed for TkWinGetHWND() */
#include "tkWinInt.h"

#undef VMIN		/* is used in vmath.h, too */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "vmath.h"
#include "bn.h"
#include "bv/defines.h"
#include "dm.h"
#include "../null/dm-Null.h"
#include "../dm-gl.h"
#include "./dm-wgl.h"
#include "./fb_wgl.h"

#include "../include/private.h"

#define ENABLE_POINT_SMOOTH 1

#define VIEWFACTOR      (1.0/(*dmp->i->dm_vp))
#define VIEWSIZE        (2.0*(*dmp->i->dm_vp))

/* these are from /usr/include/gl.h could be device dependent */
#define XMAXSCREEN	1279
#define YMAXSCREEN	1023
#define YSTEREO		491	/* subfield height, in scanlines */
#define YOFFSET_LEFT	532	/* YSTEREO + YBLANK ? */

static int wgl_actively_drawing;
static int wgl_choose_visual();

/* Display Manager package interface */
#define IRBOUND	4095.9	/* Max magnification in Rot matrix */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
static int	wgl_close();
static int	wgl_drawString2D();
static int	wgl_configureWin_guts();

static void
WGLEventProc(ClientData clientData, XEvent *UNUSED(eventPtr))
{
    struct dm *dmp = (struct dm *)clientData;
    struct dm_wglvars *pubvars = (struct dm_wglvars *)dmp->i->dm_vars.pub_vars;
    /* Need to make things visible after a Window minimization, but don't
       want the out-of-date visual - for now, do two swaps.  If there's some
       way to trigger a Window re-draw without doing buffer swaps, that would
       be preferable... */
    SwapBuffers(pubvars->hdc);
    SwapBuffers(pubvars->hdc);
}

int
wgl_share_dlist(struct dm *dmp1, struct dm *dmp2)
{
    GLfloat backgnd[4];
    GLfloat vf;
    HGLRC old_glxContext;
    struct gl_vars *mvars = (struct gl_vars *)dmp1->i->m_vars;

    if (dmp1 == (struct dm *)NULL)
	return BRLCAD_ERROR;

    if (dmp2 == (struct dm *)NULL) {
	/* create a new graphics context for dmp1 with private display lists */

	old_glxContext = ((struct wgl_vars *)dmp1->i->dm_vars.priv_vars)->glxc;

	if ((((struct wgl_vars *)dmp1->i->dm_vars.priv_vars)->glxc =
	     wglCreateContext(((struct dm_wglvars *)dmp1->i->dm_vars.pub_vars)->hdc))==NULL) {
	    bu_log("wgl_share_dlist: couldn't create glXContext.\nUsing old context\n.");
	    ((struct wgl_vars *)dmp1->i->dm_vars.priv_vars)->glxc = old_glxContext;
	    return BRLCAD_ERROR;
	}

	if (!wglMakeCurrent(((struct dm_wglvars *)dmp1->i->dm_vars.pub_vars)->hdc,
			    ((struct wgl_vars *)dmp1->i->dm_vars.priv_vars)->glxc)) {
	    bu_log("wgl_share_dlist: Couldn't make context current\nUsing old context\n.");
	    ((struct wgl_vars *)dmp1->i->dm_vars.priv_vars)->glxc = old_glxContext;

	    return BRLCAD_ERROR;
	}

	/* display list (fontOffset + char) will display a given ASCII char */
	if ((((struct wgl_vars *)dmp1->i->dm_vars.priv_vars)->fontOffset = glGenLists(128))==0) {
	    bu_log("dm-wgl: Can't make display lists for font.\nUsing old context\n.");
	    ((struct wgl_vars *)dmp1->i->dm_vars.priv_vars)->glxc = old_glxContext;

	    return BRLCAD_ERROR;
	}

	/* This is the applications display list offset */
	dmp1->i->dm_displaylist = ((struct wgl_vars *)dmp1->i->dm_vars.priv_vars)->fontOffset + 128;

	gl_setBGColor(dmp1, 0, 0, 0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (mvars->doublebuffer)
	    glDrawBuffer(GL_BACK);
	else
	    glDrawBuffer(GL_FRONT);

	/* this is important so that wgl_configureWin knows to set the font */
	((struct dm_wglvars *)dmp1->i->dm_vars.pub_vars)->fontstruct = NULL;

	/* do viewport, ortho commands and initialize font */
	(void)wgl_configureWin_guts(dmp1, 1);

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
	mvars->i.faceFlag = 1; /* faceplate matrix is on top of stack */

	/* destroy old context */
	wglMakeCurrent(((struct dm_wglvars *)dmp1->i->dm_vars.pub_vars)->hdc,
		       ((struct wgl_vars *)dmp1->i->dm_vars.priv_vars)->glxc);
	wglDeleteContext(old_glxContext);
    } else {
	/* dmp1 will share its display lists with dmp2 */

	if (!BU_STR_EQUAL(dmp1->i->dm_name, dmp2->i->dm_name)) {
	    return BRLCAD_ERROR;
	}
	if (bu_vls_strcmp(&dmp1->i->dm_dName, &dmp2->i->dm_dName)) {
	    return BRLCAD_ERROR;
	}

	old_glxContext = ((struct wgl_vars *)dmp2->i->dm_vars.priv_vars)->glxc;

	if ((((struct wgl_vars *)dmp2->i->dm_vars.priv_vars)->glxc =
	     wglCreateContext(((struct dm_wglvars *)dmp1->i->dm_vars.pub_vars)->hdc))==NULL) {
	    bu_log("wgl_share_dlist: couldn't create glXContext.\nUsing old context\n.");
	    ((struct wgl_vars *)dmp2->i->dm_vars.priv_vars)->glxc = old_glxContext;

	    return BRLCAD_ERROR;
	}

	if (!wglMakeCurrent(((struct dm_wglvars *)dmp2->i->dm_vars.pub_vars)->hdc,
			    ((struct wgl_vars *)dmp2->i->dm_vars.priv_vars)->glxc)) {
	    bu_log("wgl_share_dlist: Couldn't make context current\nUsing old context\n.");
	    ((struct wgl_vars *)dmp2->i->dm_vars.priv_vars)->glxc = old_glxContext;

	    return BRLCAD_ERROR;
	}

	((struct wgl_vars *)dmp2->i->dm_vars.priv_vars)->fontOffset = ((struct wgl_vars *)dmp1->i->dm_vars.priv_vars)->fontOffset;
	dmp2->i->dm_displaylist = dmp1->i->dm_displaylist;

	gl_setBGColor(dmp2, 0, 0, 0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (mvars->doublebuffer)
	    glDrawBuffer(GL_BACK);
	else
	    glDrawBuffer(GL_FRONT);

	/* do viewport, ortho commands and initialize font */
	(void)wgl_configureWin_guts(dmp2, 1);

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
	glGetDoublev(GL_PROJECTION_MATRIX, mvars->i.faceplate_mat);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPushMatrix();
	glLoadIdentity();
	mvars->i.faceFlag = 1; /* faceplate matrix is on top of stack */

	/* destroy old context */
	wglMakeCurrent(((struct dm_wglvars *)dmp2->i->dm_vars.pub_vars)->hdc,
		       ((struct wgl_vars *)dmp2->i->dm_vars.priv_vars)->glxc);
	wglDeleteContext(old_glxContext);
    }

    return BRLCAD_OK;
}

/*
 *  Gracefully release the display.
 */
static int
wgl_close(struct dm *dmp)
{
    struct dm_wglvars *pubvars = (struct dm_wglvars *)dmp->i->dm_vars.pub_vars;
    struct wgl_vars *privars = (struct wgl_vars *)dmp->i->dm_vars.priv_vars;
    if (pubvars->dpy) {
	if (privars->glxc) {
	    wglMakeCurrent(pubvars->hdc, privars->glxc);
	    wglDeleteContext(privars->glxc);
	}

	if (pubvars->cmap)
	    XFreeColormap(pubvars->dpy, pubvars->cmap);

	if (pubvars->xtkwin) {
	    Tk_DeleteEventHandler(pubvars->xtkwin, VisibilityChangeMask, WGLEventProc, (ClientData)dmp);
	    Tk_DestroyWindow(pubvars->xtkwin);
	}
    }

    bu_vls_free(&dmp->i->dm_pathName);
    bu_vls_free(&dmp->i->dm_tkName);
    bu_vls_free(&dmp->i->dm_dName);
    bu_free(dmp->i->dm_vars.priv_vars, "wgl_close: wgl_vars");
    bu_free(dmp->i->dm_vars.pub_vars, "wgl_close: dm_wglvars");
    bu_free(dmp->i, "wgl_close: dmpi");
    bu_free(dmp, "wgl_close: dmp");

    return BRLCAD_OK;
}

int
wgl_viable(const char *UNUSED(dpy_string))
{
    return 1;
}


/*
 * Output a string.
 * The starting position of the beam is as specified.
 */
static int
wgl_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect)
{
    struct wgl_vars *privars = (struct wgl_vars *)dmp->i->dm_vars.priv_vars;
    if (dmp->i->dm_debugLevel)
	bu_log("wgl_drawString2D()\n");

    if (use_aspect)
	glRasterPos2f(x, y * dmp->i->dm_aspect);
    else
	glRasterPos2f(x, y);

    glListBase(privars->fontOffset);
    glCallLists((GLuint)strlen(str), GL_UNSIGNED_BYTE,  str);

    return BRLCAD_OK;
}


#define WGL_DO_STEREO 1
/* currently, get a double buffered rgba visual that works with Tk and
 * OpenGL
 */
static int
wgl_choose_visual(struct dm *dmp,
		  Tk_Window tkwin)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    struct dm_wglvars *pubvars = (struct dm_wglvars *)dmp->i->dm_vars.pub_vars;
    int iPixelFormat;
    PIXELFORMATDESCRIPTOR pfd;
    BOOL good;

    /* Try to satisfy the above desires with a color visual of the
     * greatest depth */

    memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));

    iPixelFormat = GetPixelFormat(pubvars->hdc);
    if (iPixelFormat) {
	LPVOID buf;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		      FORMAT_MESSAGE_FROM_SYSTEM |
		      FORMAT_MESSAGE_IGNORE_INSERTS,
		      NULL,
		      GetLastError(),
		      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		      (LPTSTR)&buf,
		      0,
		      NULL);
	bu_log("wgl_choose_visual: failed to get the pixel format\n");
	bu_log("wgl_choose_visual: %s", buf);
	LocalFree(buf);

	return 0;
    }

    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 32;
    pfd.iLayerType = PFD_MAIN_PLANE;

    mvars->zbuf = 1;
    iPixelFormat = ChoosePixelFormat(pubvars->hdc, &pfd);
    good = SetPixelFormat(pubvars->hdc, iPixelFormat, &pfd);

    if (good)
	return 1;
	return 0;
}


/*
 *  Either initially, or on resize/reshape of the window,
 *  sense the actual size of the window, and perform any
 *  other initializations of the window configuration.
 *
 * also change font size if necessary
 */
static int
wgl_configureWin_guts(struct dm *dmp,
		      int force)
{
    struct dm_wglvars *pubvars = (struct dm_wglvars *)dmp->i->dm_vars.pub_vars;
    struct wgl_vars *privars = (struct wgl_vars *)dmp->i->dm_vars.priv_vars;
    HFONT newfontstruct, oldfont;
    LOGFONT logfont;
    HWND hwnd;
    RECT xwa;

    if (dmp->i->dm_debugLevel)
	bu_log("wgl_configureWin_guts()\n");

    hwnd = WindowFromDC(pubvars->hdc);
    GetWindowRect(hwnd, &xwa);

    /* nothing to do */
    if (!force &&
	dmp->i->dm_height == (xwa.bottom-xwa.top) &&
	dmp->i->dm_width == (xwa.right-xwa.left))
	return BRLCAD_OK;

    gl_reshape(dmp, xwa.right-xwa.left, xwa.bottom-xwa.top);

    /* First time through, load a font or quit */
    if (pubvars->fontstruct == NULL) {
	logfont.lfHeight = 18;
	logfont.lfWidth = 0;
	logfont.lfEscapement = 0;
	logfont.lfOrientation = 10;
	logfont.lfWeight = FW_NORMAL;
	logfont.lfItalic = FALSE;
	logfont.lfUnderline = FALSE;
	logfont.lfStrikeOut = FALSE;
	logfont.lfCharSet = ANSI_CHARSET;
	logfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
	logfont.lfClipPrecision =  CLIP_DEFAULT_PRECIS;
	logfont.lfQuality = DEFAULT_QUALITY;
	logfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	logfont.lfFaceName[0] = (TCHAR)0;

	pubvars->fontstruct = CreateFontIndirect(&logfont);
	if (pubvars->fontstruct == NULL) {
	    /* ????? add backup later */
	    /* Try hardcoded backup font */
	    /*     if ((pubvars->fontstruct =
		   (HFONT *)CreateFontIndirect(&logfont)) == NULL) */
	    {
		bu_log("wgl_configureWin_guts: Can't open font '%s' or '%s'\n", FONT9, FONTBACK);
		return BRLCAD_ERROR;
	    }
	}

	oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
	wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

	if (oldfont != NULL)
	    DeleteObject(SelectObject(pubvars->hdc, oldfont));
    }

    /* Always try to choose a the font that best fits the window size.
     */

    if (!GetObject(pubvars->fontstruct, sizeof(LOGFONT), &logfont)) {
	logfont.lfHeight = 18;
	logfont.lfWidth = 0;
	logfont.lfEscapement = 0;
	logfont.lfOrientation = 10;
	logfont.lfWeight = FW_NORMAL;
	logfont.lfItalic = FALSE;
	logfont.lfUnderline = FALSE;
	logfont.lfStrikeOut = FALSE;
	logfont.lfCharSet = ANSI_CHARSET;
	logfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
	logfont.lfClipPrecision =  CLIP_DEFAULT_PRECIS;
	logfont.lfQuality = DEFAULT_QUALITY;
	logfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	logfont.lfFaceName[0] = (TCHAR) 0;

	if ((pubvars->fontstruct =
	     CreateFontIndirect(&logfont)) == NULL) {
	    bu_log("wgl_configureWin_guts: Can't open font '%s' or '%s'\n", FONT9, FONTBACK);
	    return BRLCAD_ERROR;
	}

	oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
	wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

	if (oldfont != NULL)
	    DeleteObject(SelectObject(pubvars->hdc, oldfont));
    }


    if (DM_VALID_FONT_SIZE(dmp->i->dm_fontsize)) {
	if (logfont.lfHeight != dmp->i->dm_fontsize) {
	    logfont.lfHeight = dmp->i->dm_fontsize;
	    logfont.lfWidth = 0;
	    if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {

		pubvars->fontstruct = newfontstruct;
		oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

		if (oldfont != NULL)
		    DeleteObject(SelectObject(pubvars->hdc, oldfont));
	    }
	}
    } else {
	if (dmp->i->dm_width < 582) {
	    if (logfont.lfHeight != 14) {
		logfont.lfHeight = 14;
		logfont.lfWidth = 0;
		if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {

		    pubvars->fontstruct = newfontstruct;
		    oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		    wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

		    if (oldfont != NULL)
			DeleteObject(SelectObject(pubvars->hdc, oldfont));
		}
	    }
	} else if (dmp->i->dm_width < 679) {
	    if (logfont.lfHeight != 15) {
		logfont.lfHeight = 15;
		logfont.lfWidth = 0;

		if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		    pubvars->fontstruct = newfontstruct;
		    oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		    wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

		    if (oldfont != NULL)
			DeleteObject(SelectObject(pubvars->hdc, oldfont));
		}
	    }
	} else if (dmp->i->dm_width < 776) {
	    if (logfont.lfHeight != 16) {
		logfont.lfHeight = 16;
		logfont.lfWidth = 0;

		if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		    pubvars->fontstruct = newfontstruct;
		    oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		    wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);
		    DeleteObject(SelectObject(pubvars->hdc, oldfont));
		}
	    }
	} else if (dmp->i->dm_width < 873) {
	    if (logfont.lfHeight != 17) {
		logfont.lfHeight = 17;
		logfont.lfWidth = 0;
		if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		    pubvars->fontstruct = newfontstruct;
		    oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		    wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

		    if (oldfont != NULL)
			DeleteObject(SelectObject(pubvars->hdc, oldfont));
		}
	    }
	} else if (dmp->i->dm_width < 970) {
	    if (logfont.lfWidth != 18) {
		logfont.lfHeight = 18;
		logfont.lfWidth = 0;
		if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		    pubvars->fontstruct = newfontstruct;
		    oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		    wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

		    if (oldfont != NULL)
			DeleteObject(SelectObject(pubvars->hdc, oldfont));
		}
	    }
	} else if (dmp->i->dm_width < 1067) {
	    if (logfont.lfWidth != 19) {
		logfont.lfHeight = 19;
		logfont.lfWidth = 0;
		if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		    pubvars->fontstruct = newfontstruct;
		    oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		    wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

		    if (oldfont != NULL)
			DeleteObject(SelectObject(pubvars->hdc, oldfont));
		}
	    }
	} else if (dmp->i->dm_width < 1164) {
	    if (logfont.lfWidth != 20) {
		logfont.lfHeight = 20;
		logfont.lfWidth = 0;
		if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		    pubvars->fontstruct = newfontstruct;
		    oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		    wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

		    if (oldfont != NULL)
			DeleteObject(SelectObject(pubvars->hdc, oldfont));
		}
	    }
	} else if (dmp->i->dm_width < 1261) {
	    if (logfont.lfWidth != 21) {
		logfont.lfHeight = 21;
		logfont.lfWidth = 0;
		if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		    pubvars->fontstruct = newfontstruct;
		    oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		    wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

		    if (oldfont != NULL)
			DeleteObject(SelectObject(pubvars->hdc, oldfont));
		}
	    }
	} else if (dmp->i->dm_width < 1358) {
	    if (logfont.lfWidth != 22) {
		logfont.lfHeight = 22;
		logfont.lfWidth = 0;
		if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		    pubvars->fontstruct = newfontstruct;
		    oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		    wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

		    if (oldfont != NULL)
			DeleteObject(SelectObject(pubvars->hdc, oldfont));
		}
	    }
	} else if (dmp->i->dm_width < 1455) {
	    if (logfont.lfWidth != 23) {
		logfont.lfHeight = 23;
		logfont.lfWidth = 0;
		if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		    pubvars->fontstruct = newfontstruct;
		    oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		    wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

		    if (oldfont != NULL)
			DeleteObject(SelectObject(pubvars->hdc, oldfont));
		}
	    }
	} else if (dmp->i->dm_width < 1552) {
	    if (logfont.lfWidth != 24) {
		logfont.lfHeight = 24;
		logfont.lfWidth = 0;
		if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		    pubvars->fontstruct = newfontstruct;
		    oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		    wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

		    if (oldfont != NULL)
			DeleteObject(SelectObject(pubvars->hdc, oldfont));
		}
	    }
	} else if (dmp->i->dm_width < 1649) {
	    if (logfont.lfWidth != 25) {
		logfont.lfHeight = 25;
		logfont.lfWidth = 0;
		if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		    pubvars->fontstruct = newfontstruct;
		    oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		    wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

		    if (oldfont != NULL)
			DeleteObject(SelectObject(pubvars->hdc, oldfont));
		}
	    }
	} else if (dmp->i->dm_width < 1746) {
	    if (logfont.lfWidth != 26) {
		logfont.lfHeight = 26;
		logfont.lfWidth = 0;
		if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		    pubvars->fontstruct = newfontstruct;
		    oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		    wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

		    if (oldfont != NULL)
			DeleteObject(SelectObject(pubvars->hdc, oldfont));
		}
	    }
	} else if (dmp->i->dm_width < 1843) {
	    if (logfont.lfWidth != 27) {
		logfont.lfHeight = 27;
		logfont.lfWidth = 0;
		if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		    pubvars->fontstruct = newfontstruct;
		    oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		    wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

		    if (oldfont != NULL)
			DeleteObject(SelectObject(pubvars->hdc, oldfont));
		}
	    }
	} else if (dmp->i->dm_width < 1940) {
	    if (logfont.lfWidth != 28) {
		logfont.lfHeight = 28;
		logfont.lfWidth = 0;
		if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		    pubvars->fontstruct = newfontstruct;
		    oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		    wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

		    if (oldfont != NULL)
			DeleteObject(SelectObject(pubvars->hdc, oldfont));
		}
	    }
	} else {
	    if (logfont.lfWidth != 29) {
		logfont.lfHeight = 29;
		logfont.lfWidth = 0;
		if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		    pubvars->fontstruct = newfontstruct;
		    oldfont = SelectObject(pubvars->hdc, ((struct dm_wglvars *)dmp->i->dm_vars.pub_vars)->fontstruct);
		    wglUseFontBitmaps(pubvars->hdc, 0, 256, privars->fontOffset);

		    if (oldfont != NULL)
			DeleteObject(SelectObject(pubvars->hdc, oldfont));
		}
	    }
	}
    }

    return BRLCAD_OK;
}


static int
wgl_makeCurrent(struct dm *dmp)
{
    struct dm_wglvars *pubvars = (struct dm_wglvars *)dmp->i->dm_vars.pub_vars;
    struct wgl_vars *privars = (struct wgl_vars *)dmp->i->dm_vars.priv_vars;
    if (dmp->i->dm_debugLevel)
	bu_log("wgl_makeCurrent()\n");

    if (!wglMakeCurrent(pubvars->hdc, privars->glxc)) {
	bu_log("wgl_makeCurrent: Couldn't make context current\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

static int
wgl_SwapBuffers(struct dm *dmp)
{
    struct dm_wglvars *pubvars = (struct dm_wglvars *)dmp->i->dm_vars.pub_vars;
    if (dmp->i->dm_debugLevel)
	bu_log("wgl_SwapBuffers()\n");

    SwapBuffers(pubvars->hdc);

    return BRLCAD_OK;
}

static int
wgl_doevent(struct dm *dmp, void *UNUSED(vclientData), void *veventPtr)
{
    XEvent *eventPtr= (XEvent *)veventPtr;
    if (!dm_make_current(dmp))
        /* allow further processing of this event */
        return TCL_OK;

    if (eventPtr->type == Expose && eventPtr->xexpose.count == 0) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        dm_set_dirty(dmp, 1);
        /* no further processing for this event */
        return TCL_RETURN;
    }
    /* allow further processing of this event */
    return TCL_OK;
}


static int
wgl_configureWin(struct dm *dmp, int force)
{
    struct dm_wglvars *pubvars = (struct dm_wglvars *)dmp->i->dm_vars.pub_vars;
    struct wgl_vars *privars = (struct wgl_vars *)dmp->i->dm_vars.priv_vars;
    if (!wglMakeCurrent(pubvars->hdc, privars->glxc)) {
	bu_log("wgl_configureWin: Couldn't make context current\n");
	return BRLCAD_ERROR;
    }

    return wgl_configureWin_guts(dmp, force);
}

int
wgl_openFb(struct dm *dmp)
{
    struct fb_platform_specific *fb_ps;
    struct wgl_fb_info *wfb_ps;
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    struct dm_wglvars *pubvars = (struct dm_wglvars *)dmp->i->dm_vars.pub_vars;
    struct wgl_vars *privars = (struct wgl_vars *)dmp->i->dm_vars.priv_vars;

    fb_ps = fb_get_platform_specific(FB_WGL_MAGIC);
    wfb_ps = (struct wgl_fb_info *)fb_ps->data;
    wfb_ps->dpy = pubvars->dpy;
    wfb_ps->win = pubvars->win;
    wfb_ps->cmap = pubvars->cmap;
    wfb_ps->vip = pubvars->vip;
    wfb_ps->hdc = pubvars->hdc;
    wfb_ps->glxc = privars->glxc;
    wfb_ps->double_buffer = mvars->doublebuffer;
    wfb_ps->soft_cmap = 0;
    dmp->i->fbp = fb_open_existing("wgl", dm_get_width(dmp), dm_get_height(dmp), fb_ps);
    fb_put_platform_specific(fb_ps);
    return 0;
}

int
wgl_geometry_request(struct dm *dmp, int width, int height)
{
    if (!dmp) return -1;
    struct dm_wglvars *pubvars = (struct dm_wglvars *)dmp->i->dm_vars.pub_vars;
    Tk_GeometryRequest(pubvars->xtkwin, width, height);
    return 0;
}

#define WGLVARS_MV_O(_m) offsetof(struct dm_wglvars, _m)

struct bu_structparse dm_wglvars_vparse[] = {
    {"%x",      1,      "dpy",                  WGLVARS_MV_O(dpy),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "win",                  WGLVARS_MV_O(win),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "top",                  WGLVARS_MV_O(top),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "tkwin",                WGLVARS_MV_O(xtkwin),     BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "depth",                WGLVARS_MV_O(depth),      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "cmap",                 WGLVARS_MV_O(cmap),       BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "vip",                  WGLVARS_MV_O(vip),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "fontstruct",           WGLVARS_MV_O(fontstruct), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "hdc",                  WGLVARS_MV_O(hdc),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devmotionnotify",      WGLVARS_MV_O(devmotionnotify),    BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devbuttonpress",       WGLVARS_MV_O(devbuttonpress),     BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devbuttonrelease",     WGLVARS_MV_O(devbuttonrelease),   BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",        0,      (char *)0,              0,                      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

int
wgl_internal_var(struct bu_vls *result, struct dm *dmp, const char *key)
{
    if (!dmp || !result) return -1;
    if (!key) {
        // Print all current vars
        bu_vls_struct_print2(result, "dm internal WGL variables", dm_wglvars_vparse, (const char *)dmp->i->dm_vars.pub_vars);
        return 0;
    }
    // Print specific var
    bu_vls_struct_item_named(result, dm_wglvars_vparse, key, (const char *)dmp->i->dm_vars.pub_vars, ',');
    return 0;
}

int
wgl_event_cmp(struct dm *dmp, dm_event_t type, int event)
{
    struct dm_wglvars *pubvars = (struct dm_wglvars *)dmp->i->dm_vars.pub_vars;
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

struct bu_structparse wgl_vparse[] = {
    {"%d",  1, "depthcue",         gl_MV_O(cueing_on),       gl_colorchange, NULL, NULL },
    {"%d",  1, "zclip",            gl_MV_O(zclipping_on),    gl_zclip_hook, NULL, NULL },
    {"%d",  1, "zbuffer",          gl_MV_O(zbuffer_on),      gl_zbuffer_hook, NULL, NULL },
    {"%d",  1, "lighting",         gl_MV_O(lighting_on),     gl_lighting_hook, NULL, NULL },
    {"%d",  1, "transparency",     gl_MV_O(transparency_on), gl_transparency_hook, NULL, NULL },
    {"%d",  1, "fastfog",          gl_MV_O(fastfog),         gl_fog_hook, NULL, NULL },
    {"%g",  1, "density",          gl_MV_O(fogdensity),      dm_generic_hook, NULL, NULL },
    {"%d",  1, "has_zbuf",         gl_MV_O(zbuf),            dm_generic_hook, NULL, NULL },
    {"%d",  1, "has_rgb",          gl_MV_O(rgb),             dm_generic_hook, NULL, NULL },
    {"%d",  1, "has_doublebuffer", gl_MV_O(doublebuffer),    dm_generic_hook, NULL, NULL },
    {"%d",  1, "depth",            gl_MV_O(depth),           dm_generic_hook, NULL, NULL },
    {"%V",  1, "log",              gl_MV_O(log),             gl_logfile_hook, NULL, NULL },
    {"%g",  1, "bound",            gl_MV_O(bound),           gl_bound_hook, NULL, NULL },
    {"%d",  1, "useBound",         gl_MV_O(boundFlag),       gl_bound_flag_hook, NULL, NULL },
    {"",    0, (char *)0,          0,                        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

/* Forward declare for dm_wgl_impl */
struct dm *wgl_open(void *ctx, void *vinterp, int argc, char *argv[]);

struct dm_impl dm_wgl_impl = {
    wgl_open,
    wgl_close,
    wgl_viable,
    gl_drawBegin,
    gl_drawEnd,
    gl_hud_begin,
    gl_hud_end,
    gl_loadMatrix,
    gl_loadPMatrix,
    gl_popPMatrix,
    wgl_drawString2D,
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
    wgl_configureWin,
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
    NULL,
    gl_beginDList,
    gl_endDList,
    gl_drawDList,
    gl_freeDLists,
    gl_genDLists,
    gl_draw_display_list,
    gl_getDisplayImage,	/* display to image function */
    gl_reshape,
    wgl_makeCurrent,
    wgl_SwapBuffers,
    wgl_doevent,
    wgl_openFb,
    gl_get_internal,
    gl_put_internal,
    wgl_geometry_request,
    wgl_internal_var,
    NULL,
    NULL,
    NULL,
    wgl_event_cmp,
    gl_fogHint,
    wgl_share_dlist,
    0,
    1,				/* is graphical */
    "Tk",                       /* uses Tk graphics system */
    1,				/* has displaylist */
    0,                          /* no stereo by default */
    "wgl",
    "Windows with OpenGL graphics",
    1, /* top */
    0, /* width */
    0, /* height */
    0, /* dirty */
    0, /* bytes per pixel */
    0, /* bits per channel */
    1,
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
    wgl_vparse,
    FB_NULL,
    0,				/* Tcl interpreter */
    NULL,                       /* Drawing context */
    NULL                        /* App data */
};

/*
 * Fire up the display manager, and the display processor.
 *
 * Note: with Visual C++ the dm_wgl_impl struct copy below
 * doesn't get set up unless we first define the struct,
 * then define the function.
 *
 */
struct dm *
wgl_open(void *UNUSED(ctx), void *vinterp, int argc, char *argv[])
{
    static int count = 0;
    GLfloat backgnd[4];
    int make_square = -1;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls init_proc_vls = BU_VLS_INIT_ZERO;
    struct gl_vars *mvars = NULL;
    struct dm *dmp = NULL;
    Tcl_Interp *interp = (Tcl_Interp *)vinterp;
    Tk_Window tkwin;
    HWND hwnd;
    HDC hdc;
    int gotvisual;

    if ((tkwin = Tk_MainWindow(interp)) == NULL) {
	return DM_NULL;
    }

    BU_ALLOC(dmp, struct dm);
    dmp->magic = DM_MAGIC;
    dmp->start_time = 0;
    BU_ALLOC(dmp->i, struct dm_impl);

    *dmp->i = dm_wgl_impl; /* struct copy */
    dmp->i->dm_interp = interp;

    BU_ALLOC(dmp->i->dm_vars.pub_vars, struct dm_wglvars);
    struct dm_wglvars *pubvars = (struct dm_wglvars *)dmp->i->dm_vars.pub_vars;
    BU_ALLOC(dmp->i->dm_vars.priv_vars, struct wgl_vars);
    struct wgl_vars *privars = (struct wgl_vars *)dmp->i->dm_vars.priv_vars;

    dmp->i->dm_get_internal(dmp);
    mvars = (struct gl_vars *)dmp->i->m_vars;
    glvars_init(dmp);

    dmp->i->dm_vp = &mvars->i.default_viewscale;

    bu_vls_init(&dmp->i->dm_pathName);
    bu_vls_init(&dmp->i->dm_tkName);
    bu_vls_init(&dmp->i->dm_dName);

    dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

    if (bu_vls_strlen(&dmp->i->dm_pathName) == 0)
	bu_vls_printf(&dmp->i->dm_pathName, ".dm_wgl%d", count);
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
    mvars->zbuffer_on = 1;
    mvars->zclipping_on = 0;
    mvars->bound = 1.0;
    mvars->boundFlag = 1;

    /* this is important so that wgl_configureWin knows to set the font */
    pubvars->fontstruct = NULL;

    if (dmp->i->dm_width == 0) {
	dmp->i->dm_width = GetSystemMetrics(SM_CXSCREEN) - 30;
	++make_square;
    }
    if (dmp->i->dm_height == 0) {
	dmp->i->dm_height = GetSystemMetrics(SM_CYSCREEN) - 30;
	++make_square;
    }

    if (make_square > 0) {
	/* Make window square */
	if (dmp->i->dm_height < dmp->i->dm_width)
	    dmp->i->dm_width = dmp->i->dm_height;
	else
	    dmp->i->dm_height =	dmp->i->dm_width;
    }

    if (dmp->i->dm_top) {
	/* Make xtkwin a toplevel window */
	Tcl_DString ds;

	Tcl_DStringInit(&ds);
	Tcl_DStringAppend(&ds, "toplevel ", -1);
	Tcl_DStringAppend(&ds, bu_vls_addr(&dmp->i->dm_pathName), -1);
	Tcl_DStringAppend(&ds, "; wm deiconify ", -1);
	Tcl_DStringAppend(&ds, bu_vls_addr(&dmp->i->dm_pathName), -1);
	if (Tcl_Eval(interp, Tcl_DStringValue(&ds)) != BRLCAD_OK) {
	    Tcl_DStringFree(&ds);
	    return DM_NULL;
	}
	pubvars->xtkwin = Tk_NameToWindow(interp, bu_vls_addr(&dmp->i->dm_pathName), tkwin);
	Tcl_DStringFree(&ds);
	pubvars->top = pubvars->xtkwin;
    }
    else {
	char *cp;

	cp = strrchr(bu_vls_addr(&dmp->i->dm_pathName), (int)'.');
	if (cp == bu_vls_addr(&dmp->i->dm_pathName)) {
	    pubvars->top = tkwin;
	}
	else {
	    struct bu_vls top_vls = BU_VLS_INIT_ZERO;

	    bu_vls_strncpy(&top_vls, (const char *)bu_vls_addr(&dmp->i->dm_pathName), cp - bu_vls_addr(&dmp->i->dm_pathName));

	    pubvars->top = Tk_NameToWindow(interp, bu_vls_addr(&top_vls), tkwin);
	    bu_vls_free(&top_vls);
	}

	/* Make xtkwin an embedded window */
	pubvars->xtkwin = Tk_CreateWindow(interp, pubvars->top, cp + 1, (char *)NULL);
    }

    if (pubvars->xtkwin == NULL) {
	bu_log("open_gl: Failed to open %s\n", bu_vls_addr(&dmp->i->dm_pathName));
	bu_vls_free(&init_proc_vls);
	(void)wgl_close(dmp);
	return DM_NULL;
    }

    bu_vls_printf(&dmp->i->dm_tkName, "%s",  (char *)Tk_Name(pubvars->xtkwin));

    if (bu_vls_strlen(&init_proc_vls) > 0) {
	bu_vls_printf(&str, "%s %s\n", bu_vls_addr(&init_proc_vls), bu_vls_addr(&dmp->i->dm_pathName));

	if (Tcl_Eval(interp, bu_vls_addr(&str)) == BRLCAD_ERROR) {
	    bu_log("open_wgl: dm init failed\n");
	    bu_vls_free(&init_proc_vls);
	    bu_vls_free(&str);
	    (void)wgl_close(dmp);
	    return DM_NULL;
	}
    }

    bu_vls_free(&init_proc_vls);
    bu_vls_free(&str);

    pubvars->dpy =
	Tk_Display(pubvars->top);

    /* make sure there really is a display before proceeding. */
    if (!pubvars->dpy) {
	(void)wgl_close(dmp);
	return DM_NULL;
    }

    Tk_GeometryRequest(pubvars->xtkwin, dmp->i->dm_width, dmp->i->dm_height);

    Tk_MakeWindowExist(pubvars->xtkwin);

    pubvars->win = Tk_WindowId(pubvars->xtkwin);
    dmp->i->dm_id = pubvars->win;

    hwnd = TkWinGetHWND(pubvars->win);
    hdc = GetDC(hwnd);
    pubvars->hdc = hdc;

    gotvisual = wgl_choose_visual(dmp, pubvars->xtkwin);
    if (!gotvisual) {
	bu_log("wgl_open: Can't get an appropriate visual.\n");
	(void)wgl_close(dmp);
	return DM_NULL;
    }

    pubvars->depth = mvars->depth;

    /* open GLX context */
    if ((privars->glxc = wglCreateContext(pubvars->hdc)) == NULL) {
	bu_log("wgl_open: couldn't create glXContext.\n");
	(void)wgl_close(dmp);
	return DM_NULL;
    }

    if (!wglMakeCurrent(pubvars->hdc, privars->glxc)) {
	bu_log("wgl_open: couldn't make context current\n");
	(void)wgl_close(dmp);
	return DM_NULL;
    }

    /* display list (fontOffset + char) will display a given ASCII char */
    if ((privars->fontOffset = glGenLists(128)) == 0) {
	bu_log("wgl_open: couldn't make display lists for font.\n");
	(void)wgl_close(dmp);
	return DM_NULL;
    }

    /* This is the applications display list offset */
    dmp->i->dm_displaylist = privars->fontOffset + 128;

    gl_setBGColor(dmp, 0, 0, 0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (mvars->doublebuffer)
	glDrawBuffer(GL_BACK);
    else
	glDrawBuffer(GL_FRONT);

    /* do viewport, ortho commands and initialize font */
    (void)wgl_configureWin_guts(dmp, 1);

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
    mvars->i.faceFlag = 1; /* faceplate matrix is on top of stack */

    gl_setZBuffer(dmp, mvars->zbuffer_on);
    gl_setLight(dmp, mvars->lighting_on);

    if (!wglMakeCurrent((HDC)NULL, (HGLRC)NULL)) {
	LPVOID buf;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&buf,
		0,
		NULL);
	bu_log("wgl_open: Couldn't release the current context.\n");
	bu_log("wgl_open: %s", buf);
	LocalFree(buf);

	return DM_NULL;
    }

    Tk_MapWindow(pubvars->xtkwin);

    Tk_CreateEventHandler(pubvars->xtkwin, VisibilityChangeMask, WGLEventProc, (ClientData)dmp);

    return dmp;
}

struct dm dm_wgl = { DM_MAGIC, &dm_wgl_impl, 0 };

#ifdef DM_PLUGIN
static const struct dm_plugin pinfo = { DM_API, &dm_wgl };

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

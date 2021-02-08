/*                        D M - T K . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2021 United States Government as represented by
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
/** @file libdm/dm-tk.c
 *
 * A Tk Mesa3d Display Manager.
 *
 */

#include "common.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "OSMesa/gl.h"
#include "OSMesa/osmesa.h"

#include "png.h"

#include "tk.h"

#undef VMIN		/* is used in vmath.h, too */

#include "vmath.h"
#include "bn.h"
#include "rt/solid.h"
#include "dm.h"
#include "../null/dm-Null.h"
#include "./fb_tk.h"
#include "./dm-tk.h"

#include "../include/private.h"

#define ENABLE_POINT_SMOOTH 1

#define VIEWFACTOR      (1.0/(*dmp->i->dm_vp))
#define VIEWSIZE        (2.0*(*dmp->i->dm_vp))

/* these are from /usr/include/gl.h could be device dependent */
#define XMAXSCREEN	1279
#define YMAXSCREEN	1023
#define YSTEREO		491	/* subfield height, in scanlines */
#define YOFFSET_LEFT	532	/* YSTEREO + YBLANK ? */

/* Display Manager package interface */
#define IRBOUND 4095.9	/* Max magnification in Rot matrix */
#define PLOTBOUND 1000.0	/* Max magnification in Rot matrix */

struct dm *tk_open(void *vinterp, int argc, const char **argv);
HIDDEN int tk_close(struct dm *dmp);
HIDDEN int tk_drawBegin(struct dm *dmp);
HIDDEN int tk_drawEnd(struct dm *dmp);
HIDDEN int tk_normal(struct dm *dmp);
HIDDEN int tk_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye);
HIDDEN int tk_loadPMatrix(struct dm *dmp, fastf_t *mat);
HIDDEN int tk_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);
HIDDEN int tk_drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2);
HIDDEN int tk_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2);
HIDDEN int tk_drawLines3D(struct dm *dmp, int npoints, point_t *points, int sflag);
HIDDEN int tk_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y);
HIDDEN int tk_drawPoint3D(struct dm *dmp, point_t point);
HIDDEN int tk_drawPoints3D(struct dm *dmp, int npoints, point_t *points);
HIDDEN int tk_drawVList(struct dm *dmp, register struct bn_vlist *vp);
HIDDEN int tk_drawVListHiddenLine(struct dm *dmp, register struct bn_vlist *vp);
HIDDEN int tk_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data);
HIDDEN int tk_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);
HIDDEN int tk_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b);
HIDDEN int tk_setLineAttr(struct dm *dmp, int width, int style);
HIDDEN int tk_configureWin_guts(struct dm *dmp, int force);
HIDDEN int tk_configureWin(struct dm *dmp, int force);
HIDDEN int tk_setLight(struct dm *dmp, int lighting_on);
HIDDEN int tk_setTransparency(struct dm *dmp, int transparency_on);
HIDDEN int tk_setDepthMask(struct dm *dmp, int depthMask_on);
HIDDEN int tk_setZBuffer(struct dm *dmp, int zbuffer_on);
HIDDEN int tk_setWinBounds(struct dm *dmp, fastf_t *w);
HIDDEN int tk_debug(struct dm *dmp, int vl);
HIDDEN int tk_logfile(struct dm *dmp, const char *filename);
HIDDEN int tk_beginDList(struct dm *dmp, unsigned int list);
HIDDEN int tk_endDList(struct dm *dmp);
HIDDEN int tk_drawDList(unsigned int list);
HIDDEN int tk_freeDLists(struct dm *dmp, unsigned int list, int range);
HIDDEN int tk_genDLists(struct dm *dmp, size_t range);
HIDDEN int tk_getDisplayImage(struct dm *dmp, unsigned char **image);
HIDDEN int tk_reshape(struct dm *dmp, int width, int height);
HIDDEN int tk_makeCurrent(struct dm *dmp);


static fastf_t default_viewscale = 1000.0;
static double xlim_view = 1.0;	/* args for glOrtho*/
static double ylim_view = 1.0;

/* lighting parameters */
static float amb_three[] = {0.3, 0.3, 0.3, 1.0};

static float light0_position[] = {0.0, 0.0, 1.0, 0.0};
static float light0_diffuse[] = {1.0, 1.0, 1.0, 1.0}; /* white */
static float wireColor[4];
static float ambientColor[4];
static float specularColor[4];
static float diffuseColor[4];
static float backDiffuseColorDark[4];
static float backDiffuseColorLight[4];

HIDDEN void
tk_printmat(struct bu_vls *tmp_vls, fastf_t *mat) {
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[0], mat[4], mat[8], mat[12]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[1], mat[5], mat[9], mat[13]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[2], mat[6], mat[10], mat[14]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[3], mat[7], mat[11], mat[15]);
}

HIDDEN void
tk_printglmat(struct bu_vls *tmp_vls, GLfloat *m) {
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", m[0], m[4], m[8], m[12]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", m[1], m[5], m[9], m[13]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", m[2], m[6], m[10], m[14]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", m[3], m[7], m[11], m[15]);
}


void
tk_fogHint(struct dm *dmp, int fastfog)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)dmp->i->m_vars;
    mvars->fastfog = fastfog;
    glHint(GL_FOG_HINT, fastfog ? GL_FASTEST : GL_NICEST);
}


HIDDEN int
tk_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    struct tk_vars *privars = (struct tk_vars *)dmp->i->dm_vars.priv_vars;
    if (dmp->i->dm_debugLevel == 1)
	bu_log("tk_setBGColor()\n");

    dmp->i->dm_bg[0] = r;
    dmp->i->dm_bg[1] = g;
    dmp->i->dm_bg[2] = b;

    privars->r = r / 255.0;
    privars->g = g / 255.0;
    privars->b = b / 255.0;

    glClearColor(privars->r, privars->g, privars->b, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    return BRLCAD_OK;
}


/*
 * Either initially, or on resize/reshape of the window,
 * sense the actual size of the window, and perform any
 * other initializations of the window configuration.
 *
 * also change font size if necessary
 */
HIDDEN int
tk_configureWin_guts(struct dm *dmp, int force)
{
    if (dmp->i->dm_debugLevel)
	bu_log("tk_configureWin_guts()\n");

    if (force)
	tk_reshape(dmp, dmp->i->dm_width, dmp->i->dm_height);

    return BRLCAD_OK;
}

int
tk_dm_image_update_data(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    struct dm *dmp = (struct dm *)clientData;

    if (argc == 4) {
	// Unpack the current width and height.  If these don't match what idata has
	// or thinks its working on, update the image size.
	// (Note: checking errno, although it may not truly be necessary if we
	// trust Tk to always give us valid coordinates...)
	char *p_end;
	errno = 0;
	long width = strtol(argv[2], &p_end, 10);
	if (errno == ERANGE || (errno != 0 && width == 0) || p_end == argv[1]) {
	    bu_log("Invalid width: %s\n", argv[2]);
	    return TCL_ERROR;
	}
	errno = 0;
	long height = strtol(argv[3], &p_end, 10);
	if (errno == ERANGE || (errno != 0 && height == 0) || p_end == argv[1]) {
	    bu_log("Invalid height: %s\n", argv[3]);
	    return TCL_ERROR;
	}

	Tk_PhotoImageBlock dm_data;
	Tk_PhotoHandle dm_img = Tk_FindPhoto(interp, argv[1]);
	Tk_PhotoGetImage(dm_img, &dm_data);
	if (dm_data.width != dmp->i->dm_width || dm_data.height != dmp->i->dm_height) {
	    Tk_PhotoSetSize(interp, dm_img, dmp->i->dm_width, dmp->i->dm_height);
	}
    }

    return TCL_OK;
}


HIDDEN int
tk_reshape(struct dm *dmp, int width, int height)
{
    GLint mm;

    struct tk_vars *privars = (struct tk_vars *)dmp->i->dm_vars.priv_vars;

    dmp->i->dm_height = height;
    dmp->i->dm_width = width;
    dmp->i->dm_aspect = (fastf_t)dmp->i->dm_width / (fastf_t)dmp->i->dm_height;

    Tk_PhotoImageBlock dm_data;
    Tk_PhotoGetImage(privars->dm_img, &dm_data);
    if (dm_data.width != dmp->i->dm_width || dm_data.height != dmp->i->dm_height) {
	Tk_PhotoSetSize((Tcl_Interp *)dmp->i->dm_interp, privars->dm_img, dmp->i->dm_width, dmp->i->dm_height);
    }

    if (dmp->i->dm_debugLevel) {
	GLfloat m[16];
	bu_log("tk_reshape()\n");
	bu_log("width = %d, height = %d\n", dmp->i->dm_width, dmp->i->dm_height);
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	glGetFloatv (GL_PROJECTION_MATRIX, m);
    }

    glViewport(0, 0, dmp->i->dm_width, dmp->i->dm_height);

    glClearColor(privars->r,
		 privars->g,
		 privars->b,
		 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->i->dm_clipmin[2], dmp->i->dm_clipmax[2]);
    glMatrixMode(mm);

    return 0;
}


HIDDEN int
tk_makeCurrent(struct dm *dmp)
{
    struct tk_vars *privars = (struct tk_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("tk_makeCurrent()\n");

    if (!OSMesaMakeCurrent(privars->glxc, privars->buf, GL_UNSIGNED_BYTE, dmp->i->dm_width, dmp->i->dm_height)) {
	bu_log("tk_makeCurrent: Couldn't make context current\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

HIDDEN int
tk_doevent(struct dm *dmp, void *UNUSED(vclientData), void *veventPtr)
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

HIDDEN int
tk_configureWin(struct dm *dmp, int force)
{
    struct tk_vars *privars = (struct tk_vars *)dmp->i->dm_vars.priv_vars;

    if (!OSMesaMakeCurrent(privars->glxc, privars->buf, GL_UNSIGNED_BYTE, dmp->i->dm_width, dmp->i->dm_height)) {
	bu_log("tk_configureWin: Couldn't make context current\n");
	return BRLCAD_ERROR;
    }

    return tk_configureWin_guts(dmp, force);
}


HIDDEN int
tk_setLight(struct dm *dmp, int lighting_on)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)dmp->i->m_vars;
    if (dmp->i->dm_debugLevel)
	bu_log("tk_setLight()\n");

    dmp->i->dm_light = lighting_on;
    mvars->lighting_on = dmp->i->dm_light;

    if (!dmp->i->dm_light) {
	/* Turn it off */
	glDisable(GL_LIGHTING);
    } else {
	/* Turn it on */

	if (1 < dmp->i->dm_light)
	    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	else
	    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb_three);
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);

	glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light0_diffuse);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
    }

    return BRLCAD_OK;
}

/*
 * Gracefully release the display.
 */
HIDDEN int
tk_close(struct dm *dmp)
{
    struct dm_tkvars *pubvars = (struct dm_tkvars *)dmp->i->dm_vars.pub_vars;
    struct tk_vars *privars = (struct tk_vars *)dmp->i->dm_vars.priv_vars;

    if (pubvars->dpy) {
	if (privars->glxc) {
	    OSMesaDestroyContext(privars->glxc);
	}
	if (pubvars->xtkwin)
	    Tk_DestroyWindow(pubvars->xtkwin);
    }

    bu_vls_free(&dmp->i->dm_pathName);
    bu_vls_free(&dmp->i->dm_tkName);
    bu_vls_free(&dmp->i->dm_dName);
    bu_free(dmp->i->dm_vars.priv_vars, "tk_close: tk_vars");
    bu_free(dmp->i->dm_vars.pub_vars, "tk_close: dm_tkvars");
    BU_PUT(dmp->i, struct dm_impl);
    BU_PUT(dmp, struct dm);

    return BRLCAD_OK;
}

int
tk_windows_setup(Tcl_Interp *interp, struct dm *dmp, Tk_Window tkwin, int *win_cnt, struct bu_vls *init_proc_vls)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)dmp->i->m_vars;
    struct dm_tkvars *pubvars = (struct dm_tkvars *)dmp->i->dm_vars.pub_vars;
    struct tk_vars *privvars = (struct tk_vars *)dmp->i->dm_vars.priv_vars;

    // Set the Tcl/Tk pathname for this window
    if (bu_vls_strlen(&dmp->i->dm_pathName) == 0) {
	bu_vls_printf(&dmp->i->dm_pathName, ".dm_tk%d", (*win_cnt));
    }

    // Increment the global Tk window counter
    ++(*win_cnt);

    // Find out (or make assumption about) display name
    if (bu_vls_strlen(&dmp->i->dm_dName) == 0) {
	char *dp = getenv("DISPLAY");
	if (dp) {
	    bu_vls_strcpy(&dmp->i->dm_dName, dp);
	} else {
	    bu_vls_strcpy(&dmp->i->dm_dName, ":0.0");
	}
    }

    // Toplevel or embedded window
    if (dmp->i->dm_top) {
	/* Make xtkwin a toplevel window */
	pubvars->xtkwin = Tk_CreateWindowFromPath(interp, tkwin,
	       	bu_vls_cstr(&dmp->i->dm_pathName), bu_vls_cstr(&dmp->i->dm_dName));
	pubvars->top = pubvars->xtkwin;
    } else {
	/* Make xtkwin an embedded window */
	char *cp = strrchr(bu_vls_addr(&dmp->i->dm_pathName), (int)'.');
	if (cp == bu_vls_addr(&dmp->i->dm_pathName)) {
	    pubvars->top = tkwin;
	} else {
	    struct bu_vls top_vls = BU_VLS_INIT_ZERO;
	    bu_vls_strncpy(&top_vls, bu_vls_cstr(&dmp->i->dm_pathName), cp - bu_vls_cstr(&dmp->i->dm_pathName));
	    pubvars->top = Tk_NameToWindow(interp, bu_vls_addr(&top_vls), tkwin);
	    bu_vls_free(&top_vls);
	}
	pubvars->xtkwin = Tk_CreateWindow(interp, pubvars->top, cp + 1, (char *)NULL);
    }

    if (pubvars->xtkwin == NULL) {
	bu_log("dm-tk: Failed to open %s\n", bu_vls_addr(&dmp->i->dm_pathName));
	return -1;
    }

    // Record the name Tk ended up picking
    bu_vls_printf(&dmp->i->dm_tkName, "%s", (char *)Tk_Name(pubvars->xtkwin));

    // Set the Tk window background
    Tk_SetWindowBackground(pubvars->xtkwin, BlackPixelOfScreen(Tk_Screen(pubvars->xtkwin)));

    // If we have a user supplied initialization routine, run it
    if (bu_vls_strlen(init_proc_vls) > 0) {
	struct bu_vls str = BU_VLS_INIT_ZERO;
	bu_vls_printf(&str, "%s %s\n", bu_vls_cstr(init_proc_vls), bu_vls_cstr(&dmp->i->dm_pathName));
	if (Tcl_Eval(interp, bu_vls_cstr(&str)) == BRLCAD_ERROR) {
	    bu_log("tk dm init - error running user supplied script (%s):\n%s\n", bu_vls_cstr(init_proc_vls), Tcl_GetStringResult(interp));
	    bu_vls_free(&str);
	    return -1;
	}
	bu_vls_free(&str);
    }

    /* make sure there really is a display before proceeding. */
    pubvars->dpy = Tk_Display(pubvars->top);
    if (!(pubvars->dpy)) {
	return -1;
    }

    Tk_GeometryRequest(pubvars->xtkwin, dmp->i->dm_width, dmp->i->dm_height);

    pubvars->depth = mvars->depth;

    /* Set up the Tk_Photo subwindow */
    struct bu_vls tcl_cmd = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&tcl_cmd, "image create photo %s.canvas.photo", bu_vls_cstr(&dmp->i->dm_pathName));
    if (Tcl_Eval(interp, bu_vls_cstr(&tcl_cmd)) == BRLCAD_ERROR) {
	bu_log("tk dm - error creating %s.canvas.photo:\n%s\n", bu_vls_cstr(&dmp->i->dm_pathName), Tcl_GetStringResult(interp));
	bu_vls_free(&tcl_cmd);
	return -1;
    }
    bu_vls_free(&tcl_cmd);

    bu_vls_sprintf(&tcl_cmd, "%s.canvas.photo", bu_vls_cstr(&dmp->i->dm_pathName));
    privvars->dm_img = Tk_FindPhoto(interp, bu_vls_cstr(&tcl_cmd));
    Tk_PhotoBlank(privvars->dm_img);
    Tk_PhotoSetSize(interp, privvars->dm_img, dmp->i->dm_width, dmp->i->dm_height);

    // Initialize the PhotoImageBlock information
    Tk_PhotoImageBlock dm_data;
    dm_data.width = dmp->i->dm_width;
    dm_data.height = dmp->i->dm_height;
    dm_data.pixelSize = 4;
    dm_data.pitch = dmp->i->dm_width * 4;
    dm_data.offset[0] = 0;
    dm_data.offset[1] = 1;
    dm_data.offset[2] = 2;
    dm_data.offset[3] = 3;
    
    // Actually create our memory for the image buffer.  Expects RGBA information
    dm_data.pixelPtr = (unsigned char *)Tcl_AttemptAlloc(dm_data.width * dm_data.height * 4);
    if (!dm_data.pixelPtr) {
	bu_vls_free(&tcl_cmd);
	bu_log("Tcl/Tk photo memory allocation failed\n");
	return -1;
    }

    // Initialize. This alters the actual data, but Tcl/Tk doesn't know about it yet.
    for (int i = 0; i < (dm_data.width * dm_data.height * 4); i+=4) {
	// Red
	dm_data.pixelPtr[i] = 0;
	// Green
	dm_data.pixelPtr[i+1] = 0;
	// Blue
	dm_data.pixelPtr[i+2] = 0;
	// Alpha at 255 - not transparent.
	dm_data.pixelPtr[i+3] = 255;
    }

    // Let Tk_Photo know we have data
    Tk_PhotoPutBlock(interp, privvars->dm_img, &dm_data, 0, 0, dm_data.width, dm_data.height, TK_PHOTO_COMPOSITE_SET);

    // Define a canvas widget, pack it into the root window, and associate the image with it
    bu_vls_sprintf(&tcl_cmd, "canvas %s.canvas -width %d -height %d -borderwidth 0",
	    bu_vls_cstr(&dmp->i->dm_pathName), dm_data.width, dm_data.height);
    if (Tcl_Eval(interp, bu_vls_cstr(&tcl_cmd)) == BRLCAD_ERROR) {
	bu_log("tk dm - error creating %s.canvas:\n%s\n", bu_vls_cstr(&dmp->i->dm_pathName), Tcl_GetStringResult(interp));
	bu_vls_free(&tcl_cmd);
	return -1;
    }
    bu_vls_sprintf(&tcl_cmd, "pack %s.canvas -fill both -expand 1", bu_vls_cstr(&dmp->i->dm_pathName));
    if (Tcl_Eval(interp, bu_vls_cstr(&tcl_cmd)) == BRLCAD_ERROR) {
	bu_vls_free(&tcl_cmd);
	bu_log("Tcl/Tk photo canvas packing failed\n");
	return -1;
    }
    bu_vls_sprintf(&tcl_cmd, "%s.canvas create image 0 0 -image %s.canvas.photo -anchor nw",
	    bu_vls_cstr(&dmp->i->dm_pathName), bu_vls_cstr(&dmp->i->dm_pathName));
    if (Tcl_Eval(interp, bu_vls_cstr(&tcl_cmd)) == BRLCAD_ERROR) {
	bu_vls_free(&tcl_cmd);
	bu_log("Tcl/Tk photo/canvas association failed\n");
	return -1;
    }

    // Register an update command so we can change the image contents
    (void)Tcl_CreateCommand(interp, "tk_dm_image_update", (Tcl_CmdProc *)tk_dm_image_update_data, (ClientData)dmp, (Tcl_CmdDeleteProc  * )NULL);

    bu_vls_sprintf(&tcl_cmd, "bind %s.canvas  <Configure> {tk_dm_image_update %s.canvas.photo [winfo width %%W] [winfo height %%W]}",
	    bu_vls_cstr(&dmp->i->dm_pathName), bu_vls_cstr(&dmp->i->dm_pathName));
    Tcl_Eval(interp, bu_vls_cstr(&tcl_cmd));
    if (Tcl_Eval(interp, bu_vls_cstr(&tcl_cmd)) == BRLCAD_ERROR) {
	bu_vls_free(&tcl_cmd);
	bu_log("Error binding Tk_Photo canvas\n");
	return -1;
    }

    Tk_MakeWindowExist(pubvars->xtkwin);

    pubvars->win = Tk_WindowId(pubvars->xtkwin);
    dmp->i->dm_id = pubvars->win;
    dmp->i->dm_interp = (void *)interp;

    return 0;
}

/*
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
tk_open(void *vinterp, int argc, const char **argv)
{
    static int count = 0;
    GLfloat backgnd[4];
    int make_square = -1;
    Tcl_Interp *interp = (Tcl_Interp *)vinterp;

    struct bu_vls init_proc_vls = BU_VLS_INIT_ZERO;
    struct dm *dmp = NULL;
    struct dm_impl *dmpi = NULL;
    struct modifiable_tk_vars *mvars = NULL;
    Tk_Window tkwin = (Tk_Window)NULL;

    struct dm_tkvars *pubvars = NULL;
    struct tk_vars *privvars = NULL;

    if ((tkwin = Tk_MainWindow(interp)) == NULL) {
	return DM_NULL;
    }

    BU_GET(dmp, struct dm);
    dmp->magic = DM_MAGIC;

    BU_GET(dmpi, struct dm_impl);
    *dmpi = *dm_tk.i; /* struct copy */
    dmp->i = dmpi;

    dmp->i->dm_interp = interp;
    dmp->i->dm_lineWidth = 1;
    dmp->i->dm_light = 1;
    dmp->i->dm_bytes_per_pixel = sizeof(GLuint);
    dmp->i->dm_bits_per_channel = 8;
    bu_vls_init(&(dmp->i->dm_log));

    BU_ALLOC(dmp->i->dm_vars.pub_vars, struct dm_tkvars);
    if (dmp->i->dm_vars.pub_vars == (void *)NULL) {
	bu_free(dmp, "tk_open: dmp");
	return DM_NULL;
    }
    pubvars = (struct dm_tkvars *)dmp->i->dm_vars.pub_vars;

    BU_ALLOC(dmp->i->dm_vars.priv_vars, struct tk_vars);
    if (dmp->i->dm_vars.priv_vars == (void *)NULL) {
	bu_free(dmp->i->dm_vars.pub_vars, "tk_open: dmp->i->dm_vars.pub_vars");
	bu_free(dmp, "tk_open: dmp");
	return DM_NULL;
    }
    privvars = (struct tk_vars *)dmp->i->dm_vars.priv_vars;

    dmp->i->dm_get_internal(dmp);
    mvars = (struct modifiable_tk_vars *)dmp->i->m_vars;

    dmp->i->dm_vp = &default_viewscale;

    bu_vls_init(&dmp->i->dm_pathName);
    bu_vls_init(&dmp->i->dm_tkName);
    bu_vls_init(&dmp->i->dm_dName);

    dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

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
    mvars->lighting_on = dmp->i->dm_light;
    mvars->zbuffer_on = dmp->i->dm_zbuffer;
    mvars->zclipping_on = dmp->i->dm_zclip;
    mvars->debug = dmp->i->dm_debugLevel;
    mvars->bound = dmp->i->dm_bound;
    mvars->boundFlag = dmp->i->dm_boundFlag;

    if (dmp->i->dm_width == 0) {
	dmp->i->dm_width = 512;
	++make_square;
    }
    if (dmp->i->dm_height == 0) {
	dmp->i->dm_height = 512;
	++make_square;
    }

    if (make_square > 0) {
	/* Make window square */
	if (dmp->i->dm_height < dmp->i->dm_width) {
	    dmp->i->dm_width = dmp->i->dm_height;
	} else {
	    dmp->i->dm_height =	dmp->i->dm_width;
	}
    }


    if (tk_windows_setup(interp, dmp, tkwin, &count, &init_proc_vls)) {
    	(void)tk_close(dmp);
	bu_vls_free(&init_proc_vls);
	return DM_NULL;
    }
    bu_vls_free(&init_proc_vls);


    /* open GL context */
    privvars->glxc = OSMesaCreateContextExt( OSMESA_RGBA, 16, 0, 0, NULL );
    if (privvars->glxc ==NULL) {
	bu_log("tk_open: couldn't create OSMesaContext.\n");
	(void)tk_close(dmp);
	return DM_NULL;
    }
    privvars->buf = bu_calloc(dmp->i->dm_width * dmp->i->dm_height, sizeof(long), "OSMesa buffer");
    if (!OSMesaMakeCurrent(privvars->glxc, privvars->buf, GL_UNSIGNED_BYTE, dmp->i->dm_width, dmp->i->dm_height)) {
	bu_log("tk_open: OSMesaMakeCurrent failed!\n");
	(void)tk_close(dmp);
	return DM_NULL;
    }

    Tk_MapWindow(pubvars->xtkwin);

    tk_setBGColor(dmp, 0, 0, 0);

    glDrawBuffer(GL_FRONT);

    /* do viewport, ortho commands and initialize font */
    (void)tk_configureWin_guts(dmp, 1);

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
    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0);
    glGetDoublev(GL_PROJECTION_MATRIX, privvars->faceplate_mat);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glPushMatrix();
    glLoadIdentity();
    privvars->face_flag = 1;	/* faceplate matrix is on top of stack */

    tk_setZBuffer(dmp, dmp->i->dm_zbuffer);
    tk_setLight(dmp, dmp->i->dm_light);

    return dmp;
}


int
tk_share_dlist(struct dm *dmp1, struct dm *dmp2)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)dmp1->i->m_vars;
    struct tk_vars *privars = (struct tk_vars *)dmp1->i->dm_vars.priv_vars;
    GLfloat backgnd[4];
    GLfloat vf;
    OSMesaContext old_glxContext;

    if (dmp1 == (struct dm *)NULL)
	return BRLCAD_ERROR;

    if (dmp2 == (struct dm *)NULL) {
	/* create a new graphics context for dmp1 with private display lists */

	old_glxContext = privars->glxc;

	privars->glxc = OSMesaCreateContextExt( OSMESA_RGBA, 16, 0, 0, NULL );
	if (privars->glxc ==NULL) {
	    bu_log("tk_share_dlist: couldn't create OSMesaContext.\nUsing old context\n.");
	    privars->glxc = old_glxContext;
	    return BRLCAD_ERROR;
	}

	if (!OSMesaMakeCurrent(privars->glxc, privars->buf, GL_UNSIGNED_BYTE, dmp1->i->dm_width, dmp1->i->dm_height)) {
	    bu_log("tk_share_dlist: Couldn't make context current\nUsing old context\n.");
	    privars->glxc = old_glxContext;

	    return BRLCAD_ERROR;
	}

	tk_setBGColor(dmp1, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (mvars->doublebuffer)
	    glDrawBuffer(GL_BACK);
	else
	    glDrawBuffer(GL_FRONT);

	/* do viewport, ortho commands and initialize font */
	(void)tk_configureWin_guts(dmp1, 1);

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
	glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0);
	glGetDoublev(GL_PROJECTION_MATRIX, privars->faceplate_mat);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPushMatrix();
	glLoadIdentity();
	privars->face_flag = 1; /* faceplate matrix is on top of stack */

	/* destroy old context */
	OSMesaDestroyContext(privars->glxc);
    } else {
	/* dmp1 will share its display lists with dmp2 */

	if (!BU_STR_EQUAL(dmp1->i->dm_name, dmp2->i->dm_name)) {
	    return BRLCAD_ERROR;
	}
	if (bu_vls_strcmp(&dmp1->i->dm_dName, &dmp2->i->dm_dName)) {
	    return BRLCAD_ERROR;
	}

	old_glxContext = ((struct tk_vars *)dmp2->i->dm_vars.priv_vars)->glxc;

	((struct tk_vars *)dmp2->i->dm_vars.priv_vars)->glxc = OSMesaCreateContextExt( OSMESA_RGBA, 16, 0, 0, NULL );
	if (((struct tk_vars *)dmp2->i->dm_vars.priv_vars)->glxc == NULL) {
	    bu_log("tk_share_dlist: couldn't create OSMesaContext.\nUsing old context\n.");
	    ((struct tk_vars *)dmp2->i->dm_vars.priv_vars)->glxc = old_glxContext;

	    return BRLCAD_ERROR;
	}

	if (!OSMesaMakeCurrent(privars->glxc, privars->buf, GL_UNSIGNED_BYTE, dmp2->i->dm_width, dmp2->i->dm_height)) {
	    bu_log("tk_share_dlist: Couldn't make context current\nUsing old context\n.");
	    ((struct tk_vars *)dmp2->i->dm_vars.priv_vars)->glxc = old_glxContext;

	    return BRLCAD_ERROR;
	}

	dmp2->i->dm_displaylist = dmp1->i->dm_displaylist;

	tk_setBGColor(dmp2, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (mvars->doublebuffer)
	    glDrawBuffer(GL_BACK);
	else
	    glDrawBuffer(GL_FRONT);

	/* do viewport, ortho commands and initialize font */
	(void)tk_configureWin_guts(dmp2, 1);

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
	glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0);
	glGetDoublev(GL_PROJECTION_MATRIX, ((struct tk_vars *)dmp2->i->dm_vars.priv_vars)->faceplate_mat);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPushMatrix();
	glLoadIdentity();
	((struct tk_vars *)dmp2->i->dm_vars.priv_vars)->face_flag = 1; /* faceplate matrix is on top of stack */

	/* destroy old context */
	OSMesaDestroyContext(old_glxContext);
    }

    return BRLCAD_OK;
}


/*
 * There are global variables which are parameters to this routine.
 */
HIDDEN int
tk_drawBegin(struct dm *dmp)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)dmp->i->m_vars;
    struct tk_vars *privars = (struct tk_vars *)dmp->i->dm_vars.priv_vars;

    GLfloat fogdepth;

    if (dmp->i->dm_debugLevel) {
	bu_log("tk_drawBegin\n");
    }

    if (dmp->i->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "initial view matrix = \n");

	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	tk_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "initial projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	tk_printglmat(&tmp_vls, m);

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }



    if (!OSMesaMakeCurrent(privars->glxc, privars->buf, GL_UNSIGNED_BYTE, dmp->i->dm_width, dmp->i->dm_height)) {
	bu_log("tk_drawBegin: Couldn't make context current\n");
	return BRLCAD_ERROR;
    }

    /* clear back buffer */
    if (!dmp->i->dm_clearBufferAfter && mvars->doublebuffer) {
	glClearColor(privars->r,
		     privars->g,
		     privars->b,
		     0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    if (privars->face_flag) {
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	privars->face_flag = 0;
	if (mvars->cueing_on) {
	    glEnable(GL_FOG);
	    /*XXX Need to do something with Viewscale */
	    fogdepth = 2.2 * (*dmp->i->dm_vp); /* 2.2 is heuristic */
	    glFogf(GL_FOG_END, fogdepth);
	    fogdepth = (GLfloat) (0.5*mvars->fogdensity/
				  (*dmp->i->dm_vp));
	    glFogf(GL_FOG_DENSITY, fogdepth);
	    glFogi(GL_FOG_MODE, dmp->i->dm_perspective ? GL_EXP : GL_LINEAR);
	}
	if (dmp->i->dm_light) {
	    glEnable(GL_LIGHTING);
	}
    }

    if (dmp->i->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "after begin view matrix = \n");

	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	tk_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "after begin projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	tk_printglmat(&tmp_vls, m);

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }


    return BRLCAD_OK;
}


HIDDEN int
tk_drawEnd(struct dm *dmp)
{
    struct tk_vars *privvars = (struct tk_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("tk_drawEnd\n");

    if (dmp->i->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "beginning of end view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	tk_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "beginning of end projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	tk_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }


    if (dmp->i->dm_light) {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
    }

    if (dmp->i->dm_debugLevel) {
	int error;
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "ANY ERRORS?\n");

	while ((error = glGetError())!=0) {
	    bu_vls_printf(&tmp_vls, "Error: %x\n", error);
	}

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    if (dmp->i->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "end of drawend view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	tk_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "end of drawend projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	tk_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    // Tk_Photo is the output of the draw, and that's an image as far
    // as this logic is concerned.
    tk_getDisplayImage(dmp, (unsigned char **)&privvars->buf);

    Tk_PhotoImageBlock dm_data;
    Tk_PhotoGetImage(privvars->dm_img, &dm_data);
    if (dm_data.width != dmp->i->dm_width || dm_data.height != dmp->i->dm_height) {
	Tk_PhotoSetSize((Tcl_Interp *)dmp->i->dm_interp, privvars->dm_img, dmp->i->dm_width, dmp->i->dm_height);
	Tk_PhotoGetImage(privvars->dm_img, &dm_data);
    }
    dm_data.pixelPtr = (unsigned char *)privvars->buf;
    Tk_PhotoPutBlock((Tcl_Interp *)dmp->i->dm_interp, privvars->dm_img, &dm_data, 0, 0, dm_data.width, dm_data.height, TK_PHOTO_COMPOSITE_SET);

    return BRLCAD_OK;
}


/*
 * Load a new transformation matrix.  This will be followed by
 * many calls to tk_draw().
 */
HIDDEN int
tk_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye)
{
    fastf_t *mptr;
    GLfloat gtmat[16];

    if (dmp->i->dm_debugLevel == 1)
	bu_log("tk_loadMatrix()\n");

    if (dmp->i->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "beginning of loadMatrix view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	tk_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "beginning of loadMatrix projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	tk_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }


    if (dmp->i->dm_debugLevel == 3) {
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "transformation matrix = \n");
	tk_printmat(&tmp_vls, mat);

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    switch (which_eye) {
	case 0:
	    /* Non-stereo */
	    break;
	case 1:
	    /* R eye */
	    glViewport(0,  0, (XMAXSCREEN)+1, (YSTEREO)+1);
	    glScissor(0,  0, (XMAXSCREEN)+1, (YSTEREO)+1);
	    tk_drawString2D(dmp, "R", 0.986, 0.0, 0, 1);
	    break;
	case 2:
	    /* L eye */
	    glViewport(0,  0+YOFFSET_LEFT, (XMAXSCREEN)+1,
		       (YSTEREO+YOFFSET_LEFT)-(YOFFSET_LEFT)+1);
	    glScissor(0,  0+YOFFSET_LEFT, (XMAXSCREEN)+1,
		      (YSTEREO+YOFFSET_LEFT)-(YOFFSET_LEFT)+1);
	    break;
    }

    mptr = mat;

    gtmat[0] = *(mptr++);
    gtmat[4] = *(mptr++);
    gtmat[8] = *(mptr++);
    gtmat[12] = *(mptr++);

    gtmat[1] = *(mptr++) * dmp->i->dm_aspect;
    gtmat[5] = *(mptr++) * dmp->i->dm_aspect;
    gtmat[9] = *(mptr++) * dmp->i->dm_aspect;
    gtmat[13] = *(mptr++) * dmp->i->dm_aspect;

    gtmat[2] = *(mptr++);
    gtmat[6] = *(mptr++);
    gtmat[10] = *(mptr++);
    gtmat[14] = *(mptr++);

    gtmat[3] = *(mptr++);
    gtmat[7] = *(mptr++);
    gtmat[11] = *(mptr++);
    gtmat[15] = *(mptr++);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glLoadMatrixf(gtmat);

    if (dmp->i->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "end of loadMatrix view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	tk_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "end of loadMatrix projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	tk_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    return BRLCAD_OK;
}


/*
 * Load a new projection matrix.
 *
 */
HIDDEN int
tk_loadPMatrix(struct dm *dmp, fastf_t *mat)
{
    fastf_t *mptr;
    GLfloat gtmat[16];

    struct tk_vars *privars = (struct tk_vars *)dmp->i->dm_vars.priv_vars;

    glMatrixMode(GL_PROJECTION);

    if (mat == (fastf_t *)NULL) {
	if (privars->face_flag) {
	    glPopMatrix();
	    glLoadIdentity();
	    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->i->dm_clipmin[2], dmp->i->dm_clipmax[2]);
	    glPushMatrix();
	    glLoadMatrixd(privars->faceplate_mat);
	} else {
	    glLoadIdentity();
	    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->i->dm_clipmin[2], dmp->i->dm_clipmax[2]);
	}

	return BRLCAD_OK;
    }

    mptr = mat;

    gtmat[0] = *(mptr++);
    gtmat[4] = *(mptr++);
    gtmat[8] = *(mptr++);
    gtmat[12] = *(mptr++);

    gtmat[1] = *(mptr++);
    gtmat[5] = *(mptr++);
    gtmat[9] = *(mptr++);
    gtmat[13] = *(mptr++);

    gtmat[2] = *(mptr++);
    gtmat[6] = *(mptr++);
    gtmat[10] = -*(mptr++);
    gtmat[14] = -*(mptr++);

    gtmat[3] = *(mptr++);
    gtmat[7] = *(mptr++);
    gtmat[11] = *(mptr++);
    gtmat[15] = *(mptr++);

    glLoadIdentity();
    glLoadMatrixf(gtmat);

    return BRLCAD_OK;
}


HIDDEN int
tk_drawVListHiddenLine(struct dm *dmp, register struct bn_vlist *vp)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)dmp->i->m_vars;
    struct tk_vars *privars = (struct tk_vars *)dmp->i->dm_vars.priv_vars;

    register struct bn_vlist *tvp;
    register int first;

    if (dmp->i->dm_debugLevel == 1)
	bu_log("tk_drawVList()\n");


    /* First, draw polygons using background color. */

    if (dmp->i->dm_light) {
	glDisable(GL_LIGHTING);
    }

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPolygonOffset(1.0, 1.0);

    /* Set color to background color for drawing polygons. */
    glColor3f(privars->r,
	      privars->g,
	      privars->b);

    /* Viewing region is from -1.0 to +1.0 */
    first = 1;
    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	register int i;
	register int nused = tvp->nused;
	register int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    GLdouble dpt[3];
	    VMOVE(dpt, *pt); /* fastf_t-to-double */
/*
	    if (dmp->i->dm_debugLevel > 2)
		bu_log(" %d (%g %g %g)\n", *cmd, V3ARGS(dpt));*/

	    switch (*cmd) {
		case BN_VLIST_LINE_MOVE:
		case BN_VLIST_LINE_DRAW:
		    break;
		case BN_VLIST_POLY_START:
		    /* Start poly marker & normal */
		    if (first == 0)
			glEnd();
		    first = 0;

		    glBegin(GL_POLYGON);
		    /* Set surface normal (vl_pnt points outward) */
		    glNormal3dv(dpt);
		    break;
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_TRI_MOVE:
		case BN_VLIST_TRI_DRAW:
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_POLY_END:
		    /* Draw, End Polygon */
		    glEnd();
		    first = 1;
		    break;
		case BN_VLIST_POLY_VERTNORM:
		case BN_VLIST_TRI_VERTNORM:
		    /* Set per-vertex normal.  Given before vert. */
		    glNormal3dv(dpt);
		    break;
		case BN_VLIST_TRI_START:
		    if (first)
			glBegin(GL_TRIANGLES);

		    first = 0;

		    /* Set surface normal (vl_pnt points outward) */
		    glNormal3dv(dpt);

		    break;
		case BN_VLIST_TRI_END:
		    break;
	    }
	}
    }

    if (first == 0)
	glEnd();

    /* Last, draw wireframe/edges. */

    /* Set color to wireColor for drawing wireframe/edges */
    glColor3f(wireColor[0], wireColor[1], wireColor[2]);

    /* Viewing region is from -1.0 to +1.0 */
    first = 1;
    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	register int i;
	register int nused = tvp->nused;
	register int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;

	for (i = 0; i < nused; i++, cmd++, pt++) {
	    GLdouble dpt[3];
	    VMOVE(dpt, *pt); /* fastf_t-to-double */
/*
	    if (dmp->i->dm_debugLevel > 2)
		bu_log(" %d (%g %g %g)\n", *cmd, V3ARGS(dpt));*/

	    switch (*cmd) {
		case BN_VLIST_LINE_MOVE:
		    /* Move, start line */
		    if (first == 0)
			glEnd();
		    first = 0;

		    glBegin(GL_LINE_STRIP);
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_POLY_START:
		case BN_VLIST_TRI_START:
		    /* Start poly marker & normal */
		    if (first == 0)
			glEnd();
		    first = 0;

		    glBegin(GL_LINE_STRIP);
		    break;
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_TRI_MOVE:
		case BN_VLIST_TRI_DRAW:
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_POLY_END:
		case BN_VLIST_TRI_END:
		    /* Draw, End Polygon */
		    glVertex3dv(dpt);
		    glEnd();
		    first = 1;
		    break;
		case BN_VLIST_POLY_VERTNORM:
		case BN_VLIST_TRI_VERTNORM:
		    /* Set per-vertex normal.  Given before vert. */
		    glNormal3dv(dpt);
		    break;
	    }
	}
    }

    if (first == 0)
	glEnd();

    if (dmp->i->dm_light) {
	glEnable(GL_LIGHTING);
    }

    if (!mvars->zbuffer_on)
	glDisable(GL_DEPTH_TEST);

    if (!dmp->i->dm_depthMask)
	glDepthMask(GL_FALSE);

    glDisable(GL_POLYGON_OFFSET_FILL);

    return BRLCAD_OK;
}


HIDDEN int
tk_drawVList(struct dm *dmp, struct bn_vlist *vp)
{
    struct bn_vlist *tvp;
    register int first;
    register int mflag = 1;
    static float black[4] = {0.0, 0.0, 0.0, 0.0};
    GLfloat originalPointSize, originalLineWidth;
    GLdouble m[16];
    GLdouble mt[16];
    GLdouble tlate[3];

    glGetFloatv(GL_POINT_SIZE, &originalPointSize);
    glGetFloatv(GL_LINE_WIDTH, &originalLineWidth);

    if (dmp->i->dm_debugLevel == 1)
	bu_log("tk_drawVList()\n");

    /* Viewing region is from -1.0 to +1.0 */
    first = 1;
    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	int i;
	int nused = tvp->nused;
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    GLdouble dpt[3];
	    VMOVE(dpt, *pt);
/*
	    if (dmp->i->dm_debugLevel > 2)
		bu_log(" %d (%g %g %g)\n", *cmd, V3ARGS(dpt));*/

	    switch (*cmd) {
		case BN_VLIST_LINE_MOVE:
		    /* Move, start line */
		    if (first == 0)
			glEnd();
		    first = 0;

		    if (dmp->i->dm_light && mflag) {
			mflag = 0;
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, wireColor);
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);

			if (dmp->i->dm_transparency)
			    glDisable(GL_BLEND);
		    }

		    glBegin(GL_LINE_STRIP);
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_MODEL_MAT:
		    if (first == 0) {
			glEnd();
			first = 1;
		    }

		    glMatrixMode(GL_MODELVIEW);
		    glPopMatrix();
		    break;
		case BN_VLIST_DISPLAY_MAT:
		    glMatrixMode(GL_MODELVIEW);
		    glGetDoublev(GL_MODELVIEW_MATRIX, m);

		    MAT_TRANSPOSE(mt, m);
		    MAT4X3PNT(tlate, mt, dpt);

		    glPushMatrix();
		    glLoadIdentity();
		    glTranslated(tlate[0], tlate[1], tlate[2]);
		    /* 96 dpi = 3.78 pixel/mm hardcoded */
		    glScaled(2. * 3.78 / dmp->i->dm_width,
		             2. * 3.78 / dmp->i->dm_height,
		             1.);
		    break;
		case BN_VLIST_POLY_START:
		case BN_VLIST_TRI_START:
		    /* Start poly marker & normal */

		    if (dmp->i->dm_light && mflag) {
			mflag = 0;
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambientColor);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);
			glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuseColor);

			switch (dmp->i->dm_light) {
			    case 1:
				break;
			    case 2:
				glMaterialfv(GL_BACK, GL_DIFFUSE, diffuseColor);
				break;
			    case 3:
				glMaterialfv(GL_BACK, GL_DIFFUSE, backDiffuseColorDark);
				break;
			    default:
				glMaterialfv(GL_BACK, GL_DIFFUSE, backDiffuseColorLight);
				break;
			}

			if (dmp->i->dm_transparency)
			    glEnable(GL_BLEND);
		    }

		    if (*cmd == BN_VLIST_POLY_START) {
			if (first == 0)
			    glEnd();

			glBegin(GL_POLYGON);
		    } else if (first)
			glBegin(GL_TRIANGLES);

		    /* Set surface normal (vl_pnt points outward) */
		    glNormal3dv(dpt);

		    first = 0;

		    break;
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_TRI_MOVE:
		case BN_VLIST_TRI_DRAW:
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_POLY_END:
		    /* Draw, End Polygon */
		    glEnd();
		    first = 1;
		    break;
		case BN_VLIST_TRI_END:
		    break;
		case BN_VLIST_POLY_VERTNORM:
		case BN_VLIST_TRI_VERTNORM:
		    /* Set per-vertex normal.  Given before vert. */
		    glNormal3dv(dpt);
		    break;
		case BN_VLIST_POINT_DRAW:
		    if (first == 0)
			glEnd();
		    first = 0;
#if ENABLE_POINT_SMOOTH
		    glEnable(GL_POINT_SMOOTH);
#endif
		    glBegin(GL_POINTS);
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_LINE_WIDTH: {
		    GLfloat lineWidth = (GLfloat)(*pt)[0];
		    if (lineWidth > 0.0) {
			glLineWidth(lineWidth);
		    }
		    break;
		}
		case BN_VLIST_POINT_SIZE: {
		    GLfloat pointSize = (GLfloat)(*pt)[0];
		    if (pointSize > 0.0) {
			glPointSize(pointSize);
		    }
		    break;
		}
	    }
	}
    }

    if (first == 0)
	glEnd();

    if (dmp->i->dm_light && dmp->i->dm_transparency)
	glDisable(GL_BLEND);

    glPointSize(originalPointSize);
    glLineWidth(originalLineWidth);

    return BRLCAD_OK;
}

int
tk_draw_data_axes(struct dm *dmp,
                  fastf_t sf,
                  struct bview_data_axes_state *bndasp)
{
    int npoints = bndasp->num_points * 6;
    if (npoints < 1)
        return 0;

    /* set color */
    dm_set_fg(dmp, bndasp->color[0], bndasp->color[1], bndasp->color[2], 1, 1.0);

    if (bndasp->draw > 1) {
        if (dmp->i->dm_light)
            glDisable(GL_LIGHTING);

        glPointSize(bndasp->size);
        dm_draw_points_3d(dmp, bndasp->num_points, bndasp->points);
        glPointSize(1);

        if (dmp->i->dm_light)
            glEnable(GL_LIGHTING);

	return 0;
    }

    int i, j;
    fastf_t halfAxesSize;               /* half the length of an axis */
    point_t ptA, ptB;
    point_t *points;
    /* Save the line attributes */
    int saveLineWidth = dmp->i->dm_lineWidth;
    int saveLineStyle = dmp->i->dm_lineStyle;

    points = (point_t *)bu_calloc(npoints, sizeof(point_t), "data axes points");
    halfAxesSize = bndasp->size * 0.5 * sf;

    /* set linewidth */
    dm_set_line_attr(dmp, bndasp->line_width, 0);  /* solid lines */

    for (i = 0, j = -1; i < bndasp->num_points; ++i) {
	/* draw X axis with x/y offsets */
	VSET(ptA, bndasp->points[i][X] - halfAxesSize, bndasp->points[i][Y], bndasp->points[i][Z]);
	VSET(ptB, bndasp->points[i][X] + halfAxesSize, bndasp->points[i][Y], bndasp->points[i][Z]);
	++j;
	VMOVE(points[j], ptA);
	++j;
	VMOVE(points[j], ptB);

	/* draw Y axis with x/y offsets */
	VSET(ptA, bndasp->points[i][X], bndasp->points[i][Y] - halfAxesSize, bndasp->points[i][Z]);
	VSET(ptB, bndasp->points[i][X], bndasp->points[i][Y] + halfAxesSize, bndasp->points[i][Z]);
	++j;
	VMOVE(points[j], ptA);
	++j;
	VMOVE(points[j], ptB);

	/* draw Z axis with x/y offsets */
	VSET(ptA, bndasp->points[i][X], bndasp->points[i][Y], bndasp->points[i][Z] - halfAxesSize);
	VSET(ptB, bndasp->points[i][X], bndasp->points[i][Y], bndasp->points[i][Z] + halfAxesSize);
	++j;
	VMOVE(points[j], ptA);
	++j;
	VMOVE(points[j], ptB);
    }

    dm_draw_lines_3d(dmp, npoints, points, 0);
    bu_free((void *)points, "data axes points");

    /* Restore the line attributes */
    dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);


    return 0;
}

HIDDEN int
tk_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data)
{
    struct bn_vlist *vp;
    if (!callback_function) {
	if (data) {
	    vp = (struct bn_vlist *)data;
	    tk_drawVList(dmp, vp);
	}
    } else {
	if (!data) {
	    return BRLCAD_ERROR;
	} else {
	    (void)callback_function(data);
	}
    }
    return BRLCAD_OK;
}


/*
 * Restore the display processor to a normal mode of operation
 * (i.e., not scaled, rotated, displaced, etc.).
 */
HIDDEN int
tk_normal(struct dm *dmp)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)dmp->i->m_vars;
    struct tk_vars *privars = (struct tk_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("tk_normal\n");

    if (dmp->i->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "beginning of tk_normal view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	tk_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "beginning of tk_normal projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	tk_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    if (!privars->face_flag) {
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixd(privars->faceplate_mat);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	privars->face_flag = 1;
	if (mvars->cueing_on)
	    glDisable(GL_FOG);
	if (dmp->i->dm_light)
	    glDisable(GL_LIGHTING);
    }

    if (dmp->i->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "end of tk_normal view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	tk_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "end of tk_normal projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	tk_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    return BRLCAD_OK;
}


/*
 * Output a string.
 * The starting position of the beam is as specified.
 */
HIDDEN int
tk_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int UNUSED(size), int use_aspect)
{
    if (dmp->i->dm_debugLevel)
	bu_log("tk_drawString2D()\n");

    if (use_aspect)
	glRasterPos2f(x, y * dmp->i->dm_aspect);
    else
	glRasterPos2f(x, y);

    glCallLists(strlen(str), GL_UNSIGNED_BYTE,  str);

    return BRLCAD_OK;
}


HIDDEN int
tk_drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2)
{
    return drawLine2D(dmp, X1, Y1, X2, Y2, "tk_drawLine2D()\n");
}


HIDDEN int
tk_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2)
{
    return drawLine3D(dmp, pt1, pt2, "tk_drawLine3D()\n", wireColor);
}


HIDDEN int
tk_drawLines3D(struct dm *dmp, int npoints, point_t *points, int sflag)
{
    return drawLines3D(dmp, npoints, points, sflag, "tk_drawLine3D()\n", wireColor);
}


HIDDEN int
tk_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y)
{
    if (dmp->i->dm_debugLevel) {
	bu_log("tk_drawPoint2D():\n");
	bu_log("\tdmp: %p\tx - %lf\ty - %lf\n", (void *)dmp, x, y);
    }

#if ENABLE_POINT_SMOOTH
    glEnable(GL_POINT_SMOOTH);
#endif
    glBegin(GL_POINTS);
    glVertex2f(x, y);
    glEnd();

    return BRLCAD_OK;
}


HIDDEN int
tk_drawPoint3D(struct dm *dmp, point_t point)
{
    GLdouble dpt[3];

    if (!dmp || !point)
	return BRLCAD_ERROR;

    if (dmp->i->dm_debugLevel) {
	bu_log("tk_drawPoint3D():\n");
	bu_log("\tdmp: %p\tpt - %lf %lf %lf\n", (void*)dmp, V3ARGS(point));
    }

    /* fastf_t to double */
    VMOVE(dpt, point);

#if ENABLE_POINT_SMOOTH
    glEnable(GL_POINT_SMOOTH);
#endif
    glBegin(GL_POINTS);
    glVertex3dv(dpt);
    glEnd();

    return BRLCAD_OK;
}


HIDDEN int
tk_drawPoints3D(struct dm *dmp, int npoints, point_t *points)
{
    GLdouble dpt[3];
    register int i;

    if (!dmp || npoints < 0 || !points)
	return BRLCAD_ERROR;

    if (dmp->i->dm_debugLevel) {
	bu_log("tk_drawPoint3D():\n");
    }

#if ENABLE_POINT_SMOOTH
    glEnable(GL_POINT_SMOOTH);
#endif
    glBegin(GL_POINTS);
    for (i = 0; i < npoints; ++i) {
	/* fastf_t to double */
	VMOVE(dpt, points[i]);
	glVertex3dv(dpt);
    }
    glEnd();

    return BRLCAD_OK;
}


HIDDEN int
tk_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    /*if (dmp->i->dm_debugLevel)
	bu_log("tk_setFGColor()\n");*/

    dmp->i->dm_fg[0] = r;
    dmp->i->dm_fg[1] = g;
    dmp->i->dm_fg[2] = b;

    /* wireColor gets the full rgb */
    wireColor[0] = r / 255.0;
    wireColor[1] = g / 255.0;
    wireColor[2] = b / 255.0;
    wireColor[3] = transparency;

    if (strict) {
	glColor3ub((GLubyte)r, (GLubyte)g, (GLubyte)b);
    } else {

	if (dmp->i->dm_light) {
	    /* Ambient = .2, Diffuse = .6, Specular = .2 */

	    ambientColor[0] = wireColor[0] * 0.2;
	    ambientColor[1] = wireColor[1] * 0.2;
	    ambientColor[2] = wireColor[2] * 0.2;
	    ambientColor[3] = wireColor[3];

	    specularColor[0] = ambientColor[0];
	    specularColor[1] = ambientColor[1];
	    specularColor[2] = ambientColor[2];
	    specularColor[3] = ambientColor[3];

	    diffuseColor[0] = wireColor[0] * 0.6;
	    diffuseColor[1] = wireColor[1] * 0.6;
	    diffuseColor[2] = wireColor[2] * 0.6;
	    diffuseColor[3] = wireColor[3];

	    backDiffuseColorDark[0] = wireColor[0] * 0.3;
	    backDiffuseColorDark[1] = wireColor[1] * 0.3;
	    backDiffuseColorDark[2] = wireColor[2] * 0.3;
	    backDiffuseColorDark[3] = wireColor[3];

	    backDiffuseColorLight[0] = wireColor[0] * 0.9;
	    backDiffuseColorLight[1] = wireColor[1] * 0.9;
	    backDiffuseColorLight[2] = wireColor[2] * 0.9;
	    backDiffuseColorLight[3] = wireColor[3];

	    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambientColor);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuseColor);
	} else {
	    glColor3ub((GLubyte)r,  (GLubyte)g,  (GLubyte)b);
	}
    }

    return BRLCAD_OK;
}


HIDDEN int
tk_setLineAttr(struct dm *dmp, int width, int style)
{
    /*if (dmp->i->dm_debugLevel)
	bu_log("tk_setLineAttr()\n");*/

    dmp->i->dm_lineWidth = width;
    dmp->i->dm_lineStyle = style;

    glLineWidth((GLfloat) width);

    if (style == DM_DASHED_LINE)
	glEnable(GL_LINE_STIPPLE);
    else
	glDisable(GL_LINE_STIPPLE);

    return BRLCAD_OK;
}


HIDDEN int
tk_debug(struct dm *dmp, int lvl)
{
    dmp->i->dm_debugLevel = lvl;

    return BRLCAD_OK;
}

HIDDEN int
tk_logfile(struct dm *dmp, const char *filename)
{
    bu_vls_sprintf(&dmp->i->dm_log, "%s", filename);

    return BRLCAD_OK;
}

HIDDEN int
tk_setWinBounds(struct dm *dmp, fastf_t *w)
{
    GLint mm;

    if (dmp->i->dm_debugLevel)
	bu_log("tk_setWinBounds()\n");

    dmp->i->dm_clipmin[0] = w[0];
    dmp->i->dm_clipmin[1] = w[2];
    dmp->i->dm_clipmin[2] = w[4];
    dmp->i->dm_clipmax[0] = w[1];
    dmp->i->dm_clipmax[1] = w[3];
    dmp->i->dm_clipmax[2] = w[5];

    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glLoadIdentity();
    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->i->dm_clipmin[2], dmp->i->dm_clipmax[2]);
    glPushMatrix();
    glMatrixMode(mm);

    return BRLCAD_OK;
}


HIDDEN int
tk_setTransparency(struct dm *dmp,
		    int transparency_on)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)dmp->i->m_vars;
    if (dmp->i->dm_debugLevel)
	bu_log("tk_setTransparency()\n");

    dmp->i->dm_transparency = transparency_on;
    mvars->transparency_on = dmp->i->dm_transparency;

    if (transparency_on) {
	/* Turn it on */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
	/* Turn it off */
	glDisable(GL_BLEND);
    }

    return BRLCAD_OK;
}


HIDDEN int
tk_setDepthMask(struct dm *dmp,
		 int enable) {
    if (dmp->i->dm_debugLevel)
	bu_log("tk_setDepthMask()\n");

    dmp->i->dm_depthMask = enable;

    if (enable)
	glDepthMask(GL_TRUE);
    else
	glDepthMask(GL_FALSE);

    return BRLCAD_OK;
}


HIDDEN int
tk_setZBuffer(struct dm *dmp, int zbuffer_on)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)dmp->i->m_vars;
    if (dmp->i->dm_debugLevel)
	bu_log("tk_setZBuffer:\n");

    dmp->i->dm_zbuffer = zbuffer_on;
    mvars->zbuffer_on = dmp->i->dm_zbuffer;

    if (mvars->zbuf == 0) {
	dmp->i->dm_zbuffer = 0;
	mvars->zbuffer_on = dmp->i->dm_zbuffer;
    }

    if (mvars->zbuffer_on) {
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
    } else {
	glDisable(GL_DEPTH_TEST);
    }

    return BRLCAD_OK;
}


HIDDEN int
tk_beginDList(struct dm *dmp, unsigned int list)
{
    if (dmp->i->dm_debugLevel)
	bu_log("tk_beginDList()\n");

    glNewList((GLuint)list, GL_COMPILE);
    return BRLCAD_OK;
}


HIDDEN int
tk_endDList(struct dm *dmp)
{
    if (dmp->i->dm_debugLevel)
	bu_log("tk_endDList()\n");

    glEndList();
    return BRLCAD_OK;
}


HIDDEN int
tk_drawDList(unsigned int list)
{
    glCallList((GLuint)list);
    return BRLCAD_OK;
}


HIDDEN int
tk_freeDLists(struct dm *dmp, unsigned int list, int range)
{
    if (dmp->i->dm_debugLevel)
	bu_log("tk_freeDLists()\n");

    glDeleteLists((GLuint)list, (GLsizei)range);
    return BRLCAD_OK;
}


HIDDEN int
tk_genDLists(struct dm *dmp, size_t range)
{
    if (dmp->i->dm_debugLevel)
	bu_log("tk_freeDLists()\n");

    return glGenLists((GLsizei)range);
}

HIDDEN int
tk_draw_obj(struct dm *dmp, struct display_list *obj)
{
    struct solid *sp;
    FOR_ALL_SOLIDS(sp, &obj->dl_headSolid) {
	if (sp->s_dlist == 0)
	    sp->s_dlist = dm_gen_dlists(dmp, 1);

	(void)dm_make_current(dmp);
	(void)dm_begin_dlist(dmp, sp->s_dlist);
	if (sp->s_iflag == UP)
	    (void)dm_set_fg(dmp, 255, 255, 255, 0, sp->s_transparency);
	else
	    (void)dm_set_fg(dmp,
		    (unsigned char)sp->s_color[0],
		    (unsigned char)sp->s_color[1],
		    (unsigned char)sp->s_color[2], 0, sp->s_transparency);
	(void)dm_draw_vlist(dmp, (struct bn_vlist *)&sp->s_vlist);
	(void)dm_end_dlist(dmp);
    }
    return 0;
}

HIDDEN int
tk_getDisplayImage(struct dm *dmp, unsigned char **image)
{
    unsigned char *idata;
    int width;
    int height;

    width = dmp->i->dm_width;
    height = dmp->i->dm_height;

    idata = (unsigned char*)bu_calloc(height * width * 3, sizeof(unsigned char), "rgb data");

    glReadBuffer(GL_FRONT);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, idata);
    *image = idata;
    flip_display_image_vertically(*image, width, height);

    return BRLCAD_OK; /* caller will need to bu_free(idata, "image data"); */
}

int
tk_openFb(struct dm *dmp)
{
    struct fb_platform_specific *fb_ps;
    struct tk_fb_info *ofb_ps;
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)dmp->i->m_vars;
    struct dm_tkvars *pubvars = (struct dm_tkvars *)dmp->i->dm_vars.pub_vars;
    struct tk_vars *privars = (struct tk_vars *)dmp->i->dm_vars.priv_vars;

    fb_ps = fb_get_platform_specific(FB_OGL_MAGIC);
    ofb_ps = (struct tk_fb_info *)fb_ps->data;
    ofb_ps->dpy = pubvars->dpy;
    ofb_ps->win = pubvars->win;
    ofb_ps->glxc = privars->glxc;
    ofb_ps->double_buffer = mvars->doublebuffer;
    ofb_ps->soft_cmap = 0;
    dmp->i->fbp = fb_open_existing("tk", dm_get_width(dmp), dm_get_height(dmp), fb_ps);
    fb_put_platform_specific(fb_ps);
    return 0;
}

int
tk_get_internal(struct dm *dmp)
{
    struct modifiable_tk_vars *mvars = NULL;
    if (!dmp->i->m_vars) {
	BU_GET(dmp->i->m_vars, struct modifiable_tk_vars);
	mvars = (struct modifiable_tk_vars *)dmp->i->m_vars;
	mvars->this_dm = dmp;
	bu_vls_init(&(mvars->log));
    }
    return 0;
}

int
tk_put_internal(struct dm *dmp)
{
    struct modifiable_tk_vars *mvars = NULL;
    if (dmp->i->m_vars) {
	mvars = (struct modifiable_tk_vars *)dmp->i->m_vars;
	bu_vls_free(&(mvars->log));
	BU_PUT(dmp->i->m_vars, struct modifiable_tk_vars);
    }
    return 0;
}

void
Tk_colorchange(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)base;
    if (mvars->cueing_on) {
	glEnable(GL_FOG);
    } else {
	glDisable(GL_FOG);
    }

    dm_generic_hook(sdp, name, base, value, data);
}

static void
tk_zclip_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)base;
    struct dm *dmp = mvars->this_dm;
    fastf_t bounds[6] = { GED_MIN, GED_MAX, GED_MIN, GED_MAX, GED_MIN, GED_MAX };

    dmp->i->dm_zclip = mvars->zclipping_on;

    if (dmp->i->dm_zclip) {
	bounds[4] = -1.0;
	bounds[5] = 1.0;
    }

    (void)dm_make_current(dmp);
    (void)dm_set_win_bounds(dmp, bounds);

    dm_generic_hook(sdp, name, base, value, data);
}

static void
tk_debug_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)base;
    struct dm *dmp = mvars->this_dm;

    dm_debug(dmp, mvars->debug);

    dm_generic_hook(sdp, name, base, value, data);
}


static void
tk_logfile_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)base;
    struct dm *dmp = mvars->this_dm;

    dm_logfile(dmp, bu_vls_addr(&mvars->log));

    dm_generic_hook(sdp, name, base, value, data);
}

static void
tk_bound_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)base;
    struct dm *dmp = mvars->this_dm;

    dmp->i->dm_bound = mvars->bound;

    dm_generic_hook(sdp, name, base, value, data);
}

static void
tk_bound_flag_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)base;
    struct dm *dmp = mvars->this_dm;

    dmp->i->dm_boundFlag = mvars->boundFlag;

    dm_generic_hook(sdp, name, base, value, data);
}

static void
tk_zbuffer_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)base;
    struct dm *dmp = mvars->this_dm;

    (void)dm_make_current(dmp);
    (void)dm_set_zbuffer(dmp, mvars->zbuffer_on);

    dm_generic_hook(sdp, name, base, value, data);
}

static void
tk_lighting_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)base;
    struct dm *dmp = mvars->this_dm;

    (void)dm_make_current(dmp);
    (void)dm_set_light(dmp, mvars->lighting_on);

    dm_generic_hook(sdp, name, base, value, data);
}

static void
tk_transparency_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)base;
    struct dm *dmp = mvars->this_dm;

    (void)dm_make_current(dmp);
    (void)dm_set_transparency(dmp, mvars->transparency_on);

    dm_generic_hook(sdp, name, base, value, data);
}

static void
tk_fog_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_tk_vars *mvars = (struct modifiable_tk_vars *)base;
    struct dm *dmp = mvars->this_dm;

    dm_fogHint(dmp, mvars->fastfog);

    dm_generic_hook(sdp, name, base, value, data);
}

struct bu_structparse Tk_vparse[] = {
    {"%d",  1, "depthcue",              Tk_MV_O(cueing_on),    Tk_colorchange, NULL, NULL },
    {"%d",  1, "zclip",         	Tk_MV_O(zclipping_on), tk_zclip_hook, NULL, NULL },
    {"%d",  1, "zbuffer",               Tk_MV_O(zbuffer_on),   tk_zbuffer_hook, NULL, NULL },
    {"%d",  1, "lighting",              Tk_MV_O(lighting_on),  tk_lighting_hook, NULL, NULL },
    {"%d",  1, "transparency",  	Tk_MV_O(transparency_on), tk_transparency_hook, NULL, NULL },
    {"%d",  1, "fastfog",               Tk_MV_O(fastfog),      tk_fog_hook, NULL, NULL },
    {"%g",  1, "density",               Tk_MV_O(fogdensity),   dm_generic_hook, NULL, NULL },
    {"%d",  1, "has_zbuf",              Tk_MV_O(zbuf),         dm_generic_hook, NULL, NULL },
    {"%d",  1, "has_rgb",               Tk_MV_O(rgb),          dm_generic_hook, NULL, NULL },
    {"%d",  1, "has_doublebuffer",      Tk_MV_O(doublebuffer), dm_generic_hook, NULL, NULL },
    {"%d",  1, "depth",         	Tk_MV_O(depth),        dm_generic_hook, NULL, NULL },
    {"%d",  1, "debug",         	Tk_MV_O(debug),        tk_debug_hook, NULL, NULL },
    {"%V",  1, "log",   		Tk_MV_O(log),  	 tk_logfile_hook, NULL, NULL },
    {"%g",  1, "bound",         	Tk_MV_O(bound),        tk_bound_hook, NULL, NULL },
    {"%d",  1, "useBound",              Tk_MV_O(boundFlag),    tk_bound_flag_hook, NULL, NULL },
    {"",        0,  (char *)0,          0,                      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

int
tk_geometry_request(struct dm *dmp, int width, int height)
{
    if (!dmp) return -1;
    Tk_GeometryRequest(((struct dm_tkvars *)dmp->i->dm_vars.pub_vars)->xtkwin, width, height);
    return 0;
}

#define GLXVARS_MV_O(_m) offsetof(struct dm_tkvars, _m)

struct bu_structparse dm_tkvars_vparse[] = {
    {"%x",      1,      "dpy",                  GLXVARS_MV_O(dpy),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "win",                  GLXVARS_MV_O(win),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "top",                  GLXVARS_MV_O(top),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "tkwin",                GLXVARS_MV_O(xtkwin),     BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "depth",                GLXVARS_MV_O(depth),      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devmotionnotify",      GLXVARS_MV_O(devmotionnotify),    BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devbuttonpress",       GLXVARS_MV_O(devbuttonpress),     BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devbuttonrelease",     GLXVARS_MV_O(devbuttonrelease),   BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",        0,      (char *)0,              0,                      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

int
tk_internal_var(struct bu_vls *result, struct dm *dmp, const char *key)
{
    if (!dmp || !result) return -1;
    if (!key) {
        // Print all current vars
        bu_vls_struct_print2(result, "dm internal GLX variables", dm_tkvars_vparse, (const char *)dmp->i->dm_vars.pub_vars);
        return 0;
    }
    // Print specific var
    bu_vls_struct_item_named(result, dm_tkvars_vparse, key, (const char *)dmp->i->dm_vars.pub_vars, ',');
    return 0;
}


// TODO - this and getDisplayImage need to be consolidated...
int
tk_write_image(struct bu_vls *msgs, FILE *fp, struct dm *dmp)
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
	    bu_vls_printf(msgs, "tk_write_image: could not create PNG write structure\n");
	}
	return -1;
    }

    info_p = png_create_info_struct(png_p);
    if (!info_p) {
	if (msgs) {
	    bu_vls_printf(msgs, "tk_write_image: could not create PNG info structure\n");
	}
	png_destroy_write_struct(&png_p, NULL);
	return -1;
    }

    ximage_p = XGetImage(((struct dm_tkvars *)dmp->i->dm_vars.pub_vars)->dpy,
			 ((struct dm_tkvars *)dmp->i->dm_vars.pub_vars)->win,
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
tk_event_cmp(struct dm *dmp, dm_event_t type, int event)
{
    struct dm_tkvars *pubvars = (struct dm_tkvars *)dmp->i->dm_vars.pub_vars;
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

int tk_viable(const char *dpy_string)
{
    if (!dpy_string) return -1;
    return 1;
}

struct dm_impl dm_tk_impl = {
    tk_open,
    tk_close,
    tk_viable,
    tk_drawBegin,
    tk_drawEnd,
    tk_normal,
    tk_loadMatrix,
    tk_loadPMatrix,
    tk_drawString2D,
    tk_drawLine2D,
    tk_drawLine3D,
    tk_drawLines3D,
    tk_drawPoint2D,
    tk_drawPoint3D,
    tk_drawPoints3D,
    tk_drawVList,
    tk_drawVListHiddenLine,
    tk_draw_data_axes,
    tk_draw,
    tk_setFGColor,
    tk_setBGColor,
    tk_setLineAttr,
    tk_configureWin,
    tk_setWinBounds,
    tk_setLight,
    tk_setTransparency,
    tk_setDepthMask,
    tk_setZBuffer,
    tk_debug,
    tk_logfile,
    tk_beginDList,
    tk_endDList,
    tk_drawDList,
    tk_freeDLists,
    tk_genDLists,
    tk_draw_obj,
    tk_getDisplayImage, /* display to image function */
    tk_reshape,
    tk_makeCurrent,
    tk_doevent,
    tk_openFb,
    tk_get_internal,
    tk_put_internal,
    tk_geometry_request,
    tk_internal_var,
    tk_write_image,
    NULL,
    NULL,
    tk_event_cmp,
    tk_fogHint,
    tk_share_dlist,
    0,
    1,				/* is graphical */
    "Tk",                       /* uses Tk graphics system */
    1,				/* has displaylist */
    0,                          /* no stereo by default */
    1.0,			/* zoom-in limit */
    1,				/* bound flag */
    "tk",
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
    {0, 0, 0},			/* bg color */
    {0, 0, 0},			/* fg color */
    {GED_MIN, GED_MIN, GED_MIN},	/* clipmin */
    {GED_MAX, GED_MAX, GED_MAX},	/* clipmax */
    0,				/* no debugging */
    0,				/* no perspective */
    0,				/* no lighting */
    0,				/* no transparency */
    1,				/* depth buffer is writable */
    1,				/* zbuffer */
    0,				/* no zclipping */
    0,                          /* clear back buffer after drawing and swap */
    0,                          /* not overriding the auto font size */
    Tk_vparse,
    FB_NULL,
    0				/* Tcl interpreter */
};

struct dm dm_tk = { DM_MAGIC, &dm_tk_impl };

#ifdef DM_PLUGIN
static const struct dm_plugin pinfo = { DM_API, &dm_tk };

COMPILER_DLLEXPORT const struct dm_plugin *dm_plugin_info()
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

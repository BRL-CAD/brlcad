/*                        D M - T O G L . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2010 United States Government as represented by
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
/** @file dm-togl.c
 *
 * A Tk OpenGL Display Manager.
 *
 */

#include "common.h"

#ifdef DM_TOGL

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "tk.h"

#define USE_TOGL_STUBS

#include "togl.h"

#undef VMIN		/* is used in vmath.h, too */

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "dm.h"
#include "dm-togl.h"
#include "dm_xvars.h"
#include "solid.h"


#define VIEWFACTOR      (1.0/(*dmp->dm_vp))
#define VIEWSIZE        (2.0*(*dmp->dm_vp))

/* these are from /usr/include/gl.h could be device dependent */
#define XMAXSCREEN	1279
#define YMAXSCREEN	1023
#define YSTEREO		491	/* subfield height, in scanlines */
#define YOFFSET_LEFT	532	/* YSTEREO + YBLANK ? */

static int togl_actively_drawing = 0;

/* Display Manager package interface */
#define IRBOUND 4095.9	/* Max magnification in Rot matrix */
#define PLOTBOUND 1000.0	/* Max magnification in Rot matrix */

struct dm	*togl_open(Tcl_Interp *interp, int argc, char **argv);
HIDDEN int	togl_close(struct dm *dmp);
HIDDEN int	togl_drawBegin(struct dm *dmp);
HIDDEN int      togl_drawEnd(struct dm *dmp);
HIDDEN int	togl_normal(struct dm *dmp);
HIDDEN int	togl_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye);
HIDDEN int	togl_drawString2D(struct dm *dmp, register char *str, fastf_t x, fastf_t y, int size, int use_aspect);
HIDDEN int	togl_drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2);
HIDDEN int	togl_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2);
HIDDEN int	togl_drawLines3D(struct dm *dmp, int npoints, point_t *points);
HIDDEN int      togl_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y);
HIDDEN int      togl_drawVList(struct dm *dmp, register struct bn_vlist *vp);
HIDDEN int      togl_drawVListHiddenLine(struct dm *dmp, register struct bn_vlist *vp);
HIDDEN int 	togl_draw(struct dm *dmp, struct bn_vlist *(*callback_function)BU_ARGS((void *)), genptr_t *data);
HIDDEN int      togl_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);
HIDDEN int	togl_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b);
HIDDEN int	togl_setLineAttr(struct dm *dmp, int width, int style);
HIDDEN int	togl_configureWin_guts(struct dm *dmp, int force);
HIDDEN int	togl_configureWin(struct dm *dmp);
HIDDEN int	togl_setLight(struct dm *dmp, int lighting_on);
HIDDEN int	togl_setTransparency(struct dm *dmp, int transparency_on);
HIDDEN int	togl_setDepthMask(struct dm *dmp, int depthMask_on);
HIDDEN int	togl_setZBuffer(struct dm *dmp, int zbuffer_on);
HIDDEN int	togl_setWinBounds(struct dm *dmp, int *w);
HIDDEN int	togl_debug(struct dm *dmp, int lvl);
HIDDEN int      togl_beginDList(struct dm *dmp, unsigned int list);
HIDDEN int	togl_endDList(struct dm *dmp);
HIDDEN int      togl_drawDList(struct dm *dmp, unsigned int list);
HIDDEN int      togl_freeDLists(struct dm *dmp, unsigned int list, int range);

struct dm dm_togl = {
    togl_close,
    togl_drawBegin,
    togl_drawEnd,
    togl_normal,
    togl_loadMatrix,
    togl_drawString2D,
    togl_drawLine2D,
    togl_drawLine3D,
    togl_drawLines3D,
    togl_drawPoint2D,
    togl_drawVList,
    togl_drawVListHiddenLine,
    togl_draw,
    togl_setFGColor,
    togl_setBGColor,
    togl_setLineAttr,
    togl_configureWin,
    togl_setWinBounds,
    togl_setLight,
    togl_setTransparency,
    togl_setDepthMask,
    togl_setZBuffer,
    togl_debug,
    togl_beginDList,
    togl_endDList,
    togl_drawDList,
    togl_freeDLists,
    0,
    1,				/* has displaylist */
    0,                            /* no stereo by default */
    1.0,				/* zoom-in limit */
    1,				/* bound flag */
    "togl",
    "Tk OpenGL Widget graphics",
    DM_TYPE_TOGL,
    1,
    0,
    0,
    0,
    0,
    1.0, /* aspect ratio */
    0,
    {0, 0},
    {0, 0, 0, 0, 0},		/* bu_vls path name*/
    {0, 0, 0, 0, 0},		/* bu_vls full name drawing window */
    {0, 0, 0, 0, 0},		/* bu_vls short name drawing window */
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
    0				/* Tcl interpreter */
};

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
static float backColor[] = {1.0, 1.0, 0.0, 1.0}; /* yellow */


#ifdef USE_PROTOTYPES
HIDDEN int togl_drawString2D(struct dm *dmp, char *str, fastf_t x, fastf_t y, int size, int use_aspect);
HIDDEN int togl_setLight(struct dm *dmp, int lighting_on);
HIDDEN int togl_setZBuffer(struct dm *dmp, int zbuffer_on);
#else
HIDDEN int togl_drawString2D();
HIDDEN int togl_setLight();
HIDDEN int togl_setZBuffer();
#endif

void
togl_fogHint(struct dm *dmp, int fastfog)
{
    ((struct togl_vars *)dmp->dm_vars.priv_vars)->mvars.fastfog = fastfog;
    glHint(GL_FOG_HINT, fastfog ? GL_FASTEST : GL_NICEST);
}


HIDDEN int
togl_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    if (dmp->dm_debugLevel)
	bu_log("togl_setBGColor()\n");

    dmp->dm_bg[0] = r;
    dmp->dm_bg[1] = g;
    dmp->dm_bg[2] = b;

    ((struct togl_vars *)dmp->dm_vars.priv_vars)->r = r / 255.0;
    ((struct togl_vars *)dmp->dm_vars.priv_vars)->g = g / 255.0;
    ((struct togl_vars *)dmp->dm_vars.priv_vars)->b = b / 255.0;

    if (((struct togl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer) {
	Togl_MakeCurrent(((struct togl_vars *)dmp->dm_vars.priv_vars)->togl);

	Togl_SwapBuffers(((struct togl_vars *)dmp->dm_vars.priv_vars)->togl);
	glClearColor(((struct togl_vars *)dmp->dm_vars.priv_vars)->r,
		     ((struct togl_vars *)dmp->dm_vars.priv_vars)->g,
		     ((struct togl_vars *)dmp->dm_vars.priv_vars)->b,
		     0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    return TCL_OK;
}


/*
 * O G L _ C O N F I G U R E W I N
 *
 * Either initially, or on resize/reshape of the window,
 * sense the actual size of the window, and perform any
 * other initializations of the window configuration.
 *
 * also change font size if necessary
 */
HIDDEN int
togl_configureWin_guts(struct dm *dmp, int force)
{
    GLint mm;
    double width, height;

    if (dmp->dm_debugLevel)
	bu_log("togl_configureWin_guts()\n");

    Togl_MakeCurrent(((struct togl_vars *)dmp->dm_vars.priv_vars)->togl);

    width = Togl_Width(((struct togl_vars *)dmp->dm_vars.priv_vars)->togl);
    height = Togl_Height(((struct togl_vars *)dmp->dm_vars.priv_vars)->togl);

    /* nothing to do */
    if (!force &&
	dmp->dm_height == width &&
	dmp->dm_width == height )
	return TCL_OK;

    dmp->dm_height = height;
    dmp->dm_width = width;
    dmp->dm_aspect = (fastf_t)dmp->dm_width / (fastf_t)dmp->dm_height;

    if (dmp->dm_debugLevel) {
	bu_log("togl_configureWin_guts()\n");
	bu_log("width = %d, height = %d\n", dmp->dm_width, dmp->dm_height);
    }

    glViewport(0, 0, dmp->dm_width, dmp->dm_height);

    if (dmp->dm_zbuffer)
	togl_setZBuffer(dmp, dmp->dm_zbuffer);

    togl_setLight(dmp, dmp->dm_light);

    glClearColor(((struct togl_vars *)dmp->dm_vars.priv_vars)->r,
		 ((struct togl_vars *)dmp->dm_vars.priv_vars)->g,
		 ((struct togl_vars *)dmp->dm_vars.priv_vars)->b,
		 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /*CJXX this might cause problems in perspective mode? */
    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0);
    glMatrixMode(mm);

    return TCL_OK;
}


HIDDEN int
togl_configureWin(struct dm *dmp)
{
    return togl_configureWin_guts(dmp, 0);
}


HIDDEN int
togl_setLight(struct dm *dmp, int lighting_on)
{
    if (dmp->dm_debugLevel)
	bu_log("togl_setLight()\n");

    dmp->dm_light = lighting_on;
    ((struct togl_vars *)dmp->dm_vars.priv_vars)->mvars.lighting_on = dmp->dm_light;

    Togl_MakeCurrent(((struct togl_vars *)dmp->dm_vars.priv_vars)->togl);

    if (!dmp->dm_light) {
	/* Turn it off */
	glDisable(GL_LIGHTING);
    } else {
	/* Turn it on */

	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb_three);
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);

	glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light0_diffuse);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
    }

    return TCL_OK;
}

/*
 * T O G L _ C L O S E
 *
 * Gracefully release the display. Will probably use either Tcl_Eval and
 * destroy or ToglCmdDeletedProc, if the Tk_DestroyWindow command doesn't
 * take care of everything itself (it should)
 */
HIDDEN int
togl_close(struct dm *dmp)
{

    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin)
            Tk_DestroyWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);

    bu_vls_free(&dmp->dm_pathName);
    bu_vls_free(&dmp->dm_tkName);
    bu_vls_free(&dmp->dm_dName);
    bu_free(dmp->dm_vars.priv_vars, "togl_close: ogl_vars");
    bu_free(dmp->dm_vars.pub_vars, "togl_close: dm_xvars");
    bu_free(dmp, "togl_close: dmp");

    return TCL_OK;
}


/*
 * T O G L _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
togl_open(Tcl_Interp *interp, int argc, char **argv)
{
    static int count = 0;
    GLfloat backgnd[4];
    int j, k;
    int make_square = -1;
    int ndevices;
    int nclass = 0;
    int unused;
    struct bu_vls tclcmd;
 
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL
	|| Togl_InitStubs(interp, "2.0", 0) == NULL) {
    	return DM_NULL;
    }	 

    struct bu_vls str;
    struct bu_vls init_proc_vls;
    Display *tmp_dpy = (Display *)NULL;
    struct dm *dmp = (struct dm *)NULL;
    Tk_Window tkwin = (Tk_Window)NULL;
    int screen_number = -1;
    int screenwidth, screenheight;

    struct dm_xvars *pubvars = NULL;
    struct togl_vars *privvars = NULL;

    extern struct dm dm_togl;

    if ((tkwin = Tk_MainWindow(interp)) == NULL) {
	return DM_NULL;
    }

    BU_GETSTRUCT(dmp, dm);
    if (dmp == DM_NULL) {
	return DM_NULL;
    }

    *dmp = dm_togl; /* struct copy */
    dmp->dm_interp = interp;
    dmp->dm_lineWidth = 1;

    dmp->dm_vars.pub_vars = (genptr_t)bu_calloc(1, sizeof(struct dm_xvars), "togl_open: dm_xvars");
    if (dmp->dm_vars.pub_vars == (genptr_t)NULL) {
	bu_free(dmp, "togl_open: dmp");
	return DM_NULL;
    }
    pubvars = (struct dm_xvars *)dmp->dm_vars.pub_vars;

    dmp->dm_vars.priv_vars = (genptr_t)bu_calloc(1, sizeof(struct togl_vars), "togl_open: togl_vars");
    if (dmp->dm_vars.priv_vars == (genptr_t)NULL) {
	bu_free(dmp->dm_vars.pub_vars, "togl_open: dmp->dm_vars.pub_vars");
	bu_free(dmp, "togl_open: dmp");
	return DM_NULL;
    }
    privvars = (struct togl_vars *)dmp->dm_vars.priv_vars;

    dmp->dm_vp = &default_viewscale;

    bu_vls_init(&dmp->dm_pathName);
    bu_vls_init(&dmp->dm_tkName);
    bu_vls_init(&dmp->dm_dName);
    bu_vls_init(&init_proc_vls);
    bu_vls_init(&tclcmd);

    dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

    if (bu_vls_strlen(&dmp->dm_pathName) == 0)
	bu_vls_printf(&dmp->dm_pathName, ".dm_togl%d", count);
    ++count;

    if (bu_vls_strlen(&dmp->dm_dName) == 0) {
	char *dp;

	dp = DisplayString(Tk_Display(tkwin));
	if (dp)
	    bu_vls_strcpy(&dmp->dm_dName, dp);
	else
	    bu_vls_strcpy(&dmp->dm_dName, ":0.0");
    }
    if (bu_vls_strlen(&init_proc_vls) == 0)
	bu_vls_strcpy(&init_proc_vls, "bind_dm");

    /* initialize dm specific variables */
    pubvars->devmotionnotify = LASTEvent;
    pubvars->devbuttonpress = LASTEvent;
    pubvars->devbuttonrelease = LASTEvent;
    dmp->dm_aspect = 1.0;

    /* initialize modifiable variables */
    privvars->mvars.rgb = 1;
    privvars->mvars.doublebuffer = 1;
    privvars->mvars.fastfog = 1;
    privvars->mvars.fogdensity = 1.0;
    privvars->mvars.lighting_on = dmp->dm_light;
    privvars->mvars.zbuffer_on = dmp->dm_zbuffer;
    privvars->mvars.zclipping_on = dmp->dm_zclip;
    privvars->mvars.debug = dmp->dm_debugLevel;
    privvars->mvars.bound = dmp->dm_bound;
    privvars->mvars.boundFlag = dmp->dm_boundFlag;

    if (dmp->dm_width == 0) {
	bu_vls_sprintf(&tclcmd, "winfo screenwidth .");
	Tcl_Eval(interp, bu_vls_addr(&tclcmd));
	Tcl_GetIntFromObj(interp, Tcl_GetObjResult(interp), &screenwidth);
	dmp->dm_width = (int)(screenwidth * 2/3);
	++make_square;
    }
    if (dmp->dm_height == 0) {
	bu_vls_sprintf(&tclcmd, "winfo screenheight .");
	Tcl_Eval(interp, bu_vls_addr(&tclcmd));
	Tcl_GetIntFromObj(interp, Tcl_GetObjResult(interp), &screenheight);
	dmp->dm_height = (int)(screenheight * 4/5);
	++make_square;
    }

    if (make_square > 0) {
	/* Make window square */
	if (dmp->dm_height <
	    dmp->dm_width)
	    dmp->dm_width =
		dmp->dm_height;
	else
	    dmp->dm_height =
		dmp->dm_width;
    }

    struct bu_vls top_vls;
    bu_vls_init(&top_vls);
 
    if (dmp->dm_top) {
        /* Make xtkwin a toplevel window */
        ((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin = Tk_CreateWindowFromPath(interp, tkwin,
                                                                                     bu_vls_addr(&dmp->dm_pathName),
                                                                                     bu_vls_addr(&dmp->dm_dName));
        ((struct dm_xvars *)dmp->dm_vars.pub_vars)->top = ((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin;
        bu_vls_trunc(&top_vls,"0");
    } else {
        char *cp;

        cp = strrchr(bu_vls_addr(&dmp->dm_pathName), (int)'.');
        if (cp == bu_vls_addr(&dmp->dm_pathName)) {
            ((struct dm_xvars *)dmp->dm_vars.pub_vars)->top = tkwin;
        } else {
            struct bu_vls top_vls;

            bu_vls_init(&top_vls);
            bu_vls_printf(&top_vls, "%*s", cp - bu_vls_addr(&dmp->dm_pathName),
                          bu_vls_addr(&dmp->dm_pathName));
            ((struct dm_xvars *)dmp->dm_vars.pub_vars)->top =
                Tk_NameToWindow(interp, bu_vls_addr(&top_vls), tkwin);
        }

        /* Make xtkwin an embedded window */
        ((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin =
            Tk_CreateWindow(interp, ((struct dm_xvars *)dmp->dm_vars.pub_vars)->top,
                            cp + 1, (char *)NULL);

        bu_vls_sprintf(&top_vls, ".%s", (char *)Tk_Name(Tk_Parent(pubvars->xtkwin)));
    }

    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin == NULL) {
        bu_log("tk_open: Failed to open %s\n", bu_vls_addr(&dmp->dm_pathName));
        (void)tk_close(dmp);
        bu_vls_free(&top_vls);
        return DM_NULL;
    }

    bu_vls_printf(&dmp->dm_tkName, "%s",
		  (char *)Tk_Name(pubvars->xtkwin));

    bu_vls_init(&str);
    bu_vls_printf(&str, "_init_dm %V %V\n",
		  &init_proc_vls,
		  &dmp->dm_pathName);

    if (Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR) {
	bu_vls_free(&init_proc_vls);
	bu_vls_free(&str);
	(void)togl_close(dmp);
        bu_vls_free(&top_vls);
	return DM_NULL;
    }

    bu_vls_free(&init_proc_vls);
    bu_vls_free(&str);

    pubvars->dpy =
	Tk_Display(pubvars->top);

    /* make sure there really is a display before proceeding. */
    if (!(pubvars->dpy)) {
	bu_vls_free(&init_proc_vls);
	bu_vls_free(&str);
	(void)togl_close(dmp);
        bu_vls_free(&top_vls);
	return DM_NULL;
    }

    Tk_GeometryRequest(pubvars->xtkwin,
		       dmp->dm_width,
		       dmp->dm_height);

    pubvars->depth = privvars->mvars.depth;

    Tk_MakeWindowExist(pubvars->xtkwin);

    pubvars->win = Tk_WindowId(pubvars->xtkwin);
    dmp->dm_id = pubvars->win;

      
    Tk_MapWindow(pubvars->xtkwin);

    /* Now that we have a parent tk window, pack the togl widget */
    bu_vls_sprintf(&tclcmd, "package require Togl; togl %s.%s.togl -width %d -height %d -rgba true -double true", bu_vls_addr(&top_vls), (char *)Tk_Name(pubvars->xtkwin), dmp->dm_width, dmp->dm_height);
    printf("%s\n", bu_vls_addr(&tclcmd));
    Tcl_Eval(interp,  bu_vls_addr(&tclcmd));
    bu_log("%s\n", Tcl_GetStringResult(interp));
    bu_vls_sprintf(&tclcmd, "pack %s.%s.togl; update", bu_vls_addr(&top_vls), (char *)Tk_Name(pubvars->xtkwin));
    Tcl_Eval(interp,  bu_vls_addr(&tclcmd));
    bu_vls_sprintf(&tclcmd, "%s.%s.togl", bu_vls_addr(&top_vls), (char *)Tk_Name(pubvars->xtkwin));
    Togl_GetToglFromObj(interp, Tcl_NewStringObj(bu_vls_addr(&tclcmd), -1), &(privvars->togl));

    Togl_MakeCurrent(privvars->togl);
    
    /* display list (fontOffset + char) will display a given ASCII char */
    if ((privvars->fontOffset = glGenLists(128))==0) {
	bu_log("dm-togl: Can't make display lists for font.\n");
	(void)togl_close(dmp);
        bu_vls_free(&top_vls);
	return DM_NULL;
    }
    
    bu_vls_free(&top_vls);
   
    /* This is the applications display list offset */
    dmp->dm_displaylist = privvars->fontOffset + 128;

    togl_setBGColor(dmp, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (privvars->mvars.doublebuffer)
	glDrawBuffer(GL_BACK);
    else
	glDrawBuffer(GL_FRONT);

    /* do viewport, ortho commands and initialize font */
    (void)togl_configureWin_guts(dmp, 1);

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
    glTranslatef(0.0, 0.0, -1.0);
    glPushMatrix();
    glLoadIdentity();
    privvars->face_flag = 1;	/* faceplate matrix is on top of stack */

    return dmp;
}

int
togl_share_dlist(struct dm *dmp1, struct dm *dmp2)
{
    GLfloat backgnd[4];
    GLfloat vf;
    Togl *old_togl;

    if (dmp1 == (struct dm *)NULL)
        return TCL_ERROR;

    if (dmp2 == (struct dm *)NULL) {
        /* create a new graphics context for dmp1 with private display lists */
	old_togl = ((struct togl_vars *)dmp1->dm_vars.priv_vars)->togl;
    }	
    return 0;

}
/*
 * T O G L _ D R A W B E G I N
 *
 * There are global variables which are parameters to this routine.
 */
HIDDEN int
togl_drawBegin(struct dm *dmp)
{
    GLfloat fogdepth;

    if (dmp->dm_debugLevel) {
	bu_log("togl_drawBegin\n");

	if (togl_actively_drawing)
	    bu_log("togl_drawBegin: already actively drawing\n");
    }

    togl_actively_drawing = 1;

    Togl_MakeCurrent(((struct togl_vars *)dmp->dm_vars.priv_vars)->togl);

    /* clear back buffer */
    if (!dmp->dm_clearBufferAfter &&
	((struct togl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer) {
	glClearColor(((struct togl_vars *)dmp->dm_vars.priv_vars)->r,
		     ((struct togl_vars *)dmp->dm_vars.priv_vars)->g,
		     ((struct togl_vars *)dmp->dm_vars.priv_vars)->b,
		     0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    if (((struct togl_vars *)dmp->dm_vars.priv_vars)->face_flag) {
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	((struct togl_vars *)dmp->dm_vars.priv_vars)->face_flag = 0;
	if (((struct togl_vars *)dmp->dm_vars.priv_vars)->mvars.cueing_on) {
	    glEnable(GL_FOG);
	    /*XXX Need to do something with Viewscale */
	    fogdepth = 2.2 * (*dmp->dm_vp); /* 2.2 is heuristic */
	    glFogf(GL_FOG_END, fogdepth);
	    fogdepth = (GLfloat) (0.5*((struct togl_vars *)dmp->dm_vars.priv_vars)->mvars.fogdensity/
				  (*dmp->dm_vp));
	    glFogf(GL_FOG_DENSITY, fogdepth);
	    glFogi(GL_FOG_MODE, dmp->dm_perspective ? GL_EXP : GL_LINEAR);
	}
	if (dmp->dm_light) {
	    glEnable(GL_LIGHTING);
	}
    }

    return TCL_OK;
}


/*
 * O G L _ D R A W E N D
 */
HIDDEN int
togl_drawEnd(struct dm *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("ogl_drawEnd\n");


    if (dmp->dm_light) {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
    }

    if (((struct togl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer) {
	Togl_SwapBuffers(((struct togl_vars *)dmp->dm_vars.priv_vars)->togl);

	if (dmp->dm_clearBufferAfter) {
	    /* give Graphics pipe time to work */
	    glClearColor(((struct togl_vars *)dmp->dm_vars.priv_vars)->r,
			 ((struct togl_vars *)dmp->dm_vars.priv_vars)->g,
			 ((struct togl_vars *)dmp->dm_vars.priv_vars)->b,
			 0.0);
	    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
    }

    if (dmp->dm_debugLevel) {
	int error;
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "ANY ERRORS?\n");

	while ((error = glGetError())!=0) {
	    bu_vls_printf(&tmp_vls, "Error: %x\n", error);
	}

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

/*XXX Keep this off unless testing */
#if 0
    glFinish();
#endif

    togl_actively_drawing = 0;
    return TCL_OK;
}


/*
 * T O G L _ L O A D M A T R I X
 *
 * Load a new transformation matrix.  This will be followed by
 * many calls to togl_draw().
 */
HIDDEN int
togl_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye)
{
    fastf_t *mptr;
    GLfloat gtmat[16];
    mat_t newm;

    if (dmp->dm_debugLevel) {
	struct bu_vls tmp_vls;

	bu_log("togl_loadMatrix()\n");

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "which eye = %d\t", which_eye);
	bu_vls_printf(&tmp_vls, "transformation matrix = \n");
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[0], mat[4], mat[8], mat[12]);
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[1], mat[5], mat[9], mat[13]);
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[2], mat[6], mat[10], mat[14]);
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[3], mat[7], mat[11], mat[15]);

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
	    togl_drawString2D(dmp, "R", 0.986, 0.0, 0, 1);
	    break;
	case 2:
	    /* L eye */
	    glViewport(0,  0+YOFFSET_LEFT, (XMAXSCREEN)+1,
		       (YSTEREO+YOFFSET_LEFT)-(YOFFSET_LEFT)+1);
	    glScissor(0,  0+YOFFSET_LEFT, (XMAXSCREEN)+1,
		      (YSTEREO+YOFFSET_LEFT)-(YOFFSET_LEFT)+1);
	    break;
    }

    if (!dmp->dm_zclip) {
	mat_t nozclip;

	MAT_IDN(nozclip);
	nozclip[10] = 1.0e-20;
	bn_mat_mul(newm, nozclip, mat);
	mptr = newm;
    } else {
	mat_t nozclip;

	MAT_IDN(nozclip);
	nozclip[10] = dmp->dm_bound;
	bn_mat_mul(newm, nozclip, mat);
	mptr = newm;
    }

    gtmat[0] = *(mptr++);
    gtmat[4] = *(mptr++);
    gtmat[8] = *(mptr++);
    gtmat[12] = *(mptr++);

    gtmat[1] = *(mptr++) * dmp->dm_aspect;
    gtmat[5] = *(mptr++) * dmp->dm_aspect;
    gtmat[9] = *(mptr++) * dmp->dm_aspect;
    gtmat[13] = *(mptr++) * dmp->dm_aspect;

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
    glTranslatef(0.0, 0.0, -1.0);
    glMultMatrixf(gtmat);

    return TCL_OK;
}


/*
 *  			T O G L _ D R A W V L I S T H I D D E N L I N E
 *
 */
HIDDEN int
togl_drawVListHiddenLine(struct dm *dmp, register struct bn_vlist *vp)
{
    register struct bn_vlist	*tvp;
    int				first;

    if (dmp->dm_debugLevel)
	bu_log("togl_drawVList()\n");


    /* First, draw polygons using background color. */

    if (dmp->dm_light) {
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
    glColor3f(((struct togl_vars *)dmp->dm_vars.priv_vars)->r,
	       ((struct togl_vars *)dmp->dm_vars.priv_vars)->g,
	       ((struct togl_vars *)dmp->dm_vars.priv_vars)->b);

    /* Viewing region is from -1.0 to +1.0 */
    first = 1;
    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	register int	i;
	register int	nused = tvp->nused;
	register int	*cmd = tvp->cmd;
	register point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    if (dmp->dm_debugLevel > 2)
		bu_log(" %d (%g %g %g)\n", *cmd, V3ARGS(pt));
	    switch (*cmd) {
		case BN_VLIST_LINE_MOVE:
		    break;
		case BN_VLIST_POLY_START:

		    /* Start poly marker & normal */
		    if (first == 0)
			glEnd();

		    glBegin(GL_POLYGON);
		    /* Set surface normal (vl_pnt points outward) */
		    glNormal3dv(*pt);
		    break;
		case BN_VLIST_LINE_DRAW:
		    break;
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		    glVertex3dv(*pt);
		    break;
		case BN_VLIST_POLY_END:
		    /* Draw, End Polygon */
		    glVertex3dv(*pt);
		    glEnd();
		    first = 1;
		    break;
		case BN_VLIST_POLY_VERTNORM:
		    /* Set per-vertex normal.  Given before vert. */
		    glNormal3dv(*pt);
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
	register int	i;
	register int	nused = tvp->nused;
	register int	*cmd = tvp->cmd;
	register point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    if (dmp->dm_debugLevel > 2)
		bu_log(" %d (%g %g %g)\n", *cmd, V3ARGS(pt));
	    switch (*cmd) {
		case BN_VLIST_LINE_MOVE:
		    /* Move, start line */
		    if (first == 0)
			glEnd();
		    first = 0;

		    glBegin(GL_LINE_STRIP);
		    glVertex3dv(*pt);
		    break;
		case BN_VLIST_POLY_START:
		    /* Start poly marker & normal */
		    if (first == 0)
			glEnd();

		    glBegin(GL_LINE_STRIP);
		    break;
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		    glVertex3dv(*pt);
		    break;
		case BN_VLIST_POLY_END:
		    /* Draw, End Polygon */
		    glVertex3dv(*pt);
		    glEnd();
		    first = 1;
		    break;
		case BN_VLIST_POLY_VERTNORM:
		    /* Set per-vertex normal.  Given before vert. */
		    glNormal3dv(*pt);
		    break;
	    }
	}
    }

    if (first == 0)
	glEnd();

    if (dmp->dm_light) {
	glEnable(GL_LIGHTING);
    }

    if (!((struct togl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on)
	glDisable(GL_DEPTH_TEST);

    if (!dmp->dm_depthMask)
	glDepthMask(GL_FALSE);

    glDisable(GL_POLYGON_OFFSET_FILL);

    return TCL_OK;
}

/*
 * O G L _ D R A W V L I S T
 *
 */
HIDDEN int
togl_drawVList(struct dm *dmp, struct bn_vlist *vp)
{
    struct bn_vlist *tvp;
    int first;
    int mflag = 1;
    static float black[4] = {0.0, 0.0, 0.0, 0.0};

    if (dmp->dm_debugLevel)
	bu_log("togl_drawVList()\n");

    /* Viewing region is from -1.0 to +1.0 */
    first = 1;
    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	int i;
	int nused = tvp->nused;
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    if (dmp->dm_debugLevel > 2)
		bu_log(" %d (%g %g %g)\n", *cmd, V3ARGS(pt));
	    switch (*cmd) {
		case BN_VLIST_LINE_MOVE:
		    /* Move, start line */
		    if (first == 0)
			glEnd();
		    first = 0;

		    if (dmp->dm_light && mflag) {
			mflag = 0;
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, wireColor);
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);

			if (dmp->dm_transparency)
			    glDisable(GL_BLEND);
		    }

		    glBegin(GL_LINE_STRIP);
		    glVertex3dv(*pt);
		    break;
		case BN_VLIST_POLY_START:
		    /* Start poly marker & normal */
		    if (first == 0)
			glEnd();

		    if (dmp->dm_light && mflag) {
			mflag = 0;
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambientColor);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);

			if (1 < dmp->dm_light) {
			    glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuseColor);
			    glMaterialfv(GL_BACK, GL_DIFFUSE, backColor);
			} else
			    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuseColor);

			if (dmp->dm_transparency)
			    glEnable(GL_BLEND);
		    }

		    glBegin(GL_POLYGON);
		    /* Set surface normal (vl_pnt points outward) */
		    glNormal3dv(*pt);
		    break;
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		    glVertex3dv(*pt);
		    break;
		case BN_VLIST_POLY_END:
		    /* Draw, End Polygon */
		    glVertex3dv(*pt);
		    glEnd();
		    first = 1;
		    break;
		case BN_VLIST_POLY_VERTNORM:
		    /* Set per-vertex normal.  Given before vert. */
		    glNormal3dv(*pt);
		    break;
	    }
	}
    }

    if (first == 0)
	glEnd();

    return TCL_OK;
}


/*
 * T O G L _ D R A W
 *
 */
HIDDEN int
togl_draw(struct dm *dmp, struct bn_vlist *(*callback_function)BU_ARGS((void *)), genptr_t *data)
{
    struct bn_vlist *vp;
    if (!callback_function) {
	if (data) {
	    vp = (struct bn_vlist *)data;
	    togl_drawVList(dmp, vp);
	}
    } else {
	if (!data) {
	    return TCL_ERROR;
	} else {
	    vp = callback_function(data);
	}
    }
    return TCL_OK;
}


/*
 * T O G L _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 */
HIDDEN int
togl_normal(struct dm *dmp)
{

    if (dmp->dm_debugLevel)
	bu_log("togl_normal\n");

    if (!((struct togl_vars *)dmp->dm_vars.priv_vars)->face_flag) {
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixd(((struct togl_vars *)dmp->dm_vars.priv_vars)->faceplate_mat);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	((struct togl_vars *)dmp->dm_vars.priv_vars)->face_flag = 1;
	if (((struct togl_vars *)dmp->dm_vars.priv_vars)->mvars.cueing_on)
	    glDisable(GL_FOG);
	if (dmp->dm_light)
	    glDisable(GL_LIGHTING);
    }

    return TCL_OK;
}


/*
 * T O G L _ D R A W S T R I N G 2 D
 *
 * Output a string.
 * The starting position of the beam is as specified.
 */
HIDDEN int
togl_drawString2D(struct dm *dmp, char *str, fastf_t x, fastf_t y, int UNUSED(size), int use_aspect)
{
    if (dmp->dm_debugLevel)
	bu_log("togl_drawString2D()\n");

    if (use_aspect)
	glRasterPos2f(x, y * dmp->dm_aspect);
    else
	glRasterPos2f(x, y);

    glListBase(((struct togl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
    glCallLists(strlen(str), GL_UNSIGNED_BYTE,  str);

    return TCL_OK;
}


/*
 * T O G L _ D R A W L I N E 2 D
 *
 */
HIDDEN int
togl_drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2)
{

    if (dmp->dm_debugLevel)
	bu_log("togl_drawLine2D()\n");

    if (dmp->dm_debugLevel) {
	GLfloat pmat[16];

	glGetFloatv(GL_PROJECTION_MATRIX, pmat);
	bu_log("projection matrix:\n");
	bu_log("%g %g %g %g\n", pmat[0], pmat[4], pmat[8], pmat[12]);
	bu_log("%g %g %g %g\n", pmat[1], pmat[5], pmat[9], pmat[13]);
	bu_log("%g %g %g %g\n", pmat[2], pmat[6], pmat[10], pmat[14]);
	bu_log("%g %g %g %g\n", pmat[3], pmat[7], pmat[11], pmat[15]);
	glGetFloatv(GL_MODELVIEW_MATRIX, pmat);
	bu_log("modelview matrix:\n");
	bu_log("%g %g %g %g\n", pmat[0], pmat[4], pmat[8], pmat[12]);
	bu_log("%g %g %g %g\n", pmat[1], pmat[5], pmat[9], pmat[13]);
	bu_log("%g %g %g %g\n", pmat[2], pmat[6], pmat[10], pmat[14]);
	bu_log("%g %g %g %g\n", pmat[3], pmat[7], pmat[11], pmat[15]);
    }

    glBegin(GL_LINES);
    glVertex2f(X1, Y1);
    glVertex2f(X2, Y2);
    glEnd();

    return TCL_OK;
}


/*
 * T O G L _ D R A W L I N E 3 D
 *
 */
HIDDEN int
togl_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2)
{
    static float black[4] = {0.0, 0.0, 0.0, 0.0};

    if (dmp->dm_debugLevel)
	bu_log("togl_drawLine3D()\n");

    if (dmp->dm_debugLevel) {
	GLfloat pmat[16];

	glGetFloatv(GL_PROJECTION_MATRIX, pmat);
	bu_log("projection matrix:\n");
	bu_log("%g %g %g %g\n", pmat[0], pmat[4], pmat[8], pmat[12]);
	bu_log("%g %g %g %g\n", pmat[1], pmat[5], pmat[9], pmat[13]);
	bu_log("%g %g %g %g\n", pmat[2], pmat[6], pmat[10], pmat[14]);
	bu_log("%g %g %g %g\n", pmat[3], pmat[7], pmat[11], pmat[15]);
	glGetFloatv(GL_MODELVIEW_MATRIX, pmat);
	bu_log("modelview matrix:\n");
	bu_log("%g %g %g %g\n", pmat[0], pmat[4], pmat[8], pmat[12]);
	bu_log("%g %g %g %g\n", pmat[1], pmat[5], pmat[9], pmat[13]);
	bu_log("%g %g %g %g\n", pmat[2], pmat[6], pmat[10], pmat[14]);
	bu_log("%g %g %g %g\n", pmat[3], pmat[7], pmat[11], pmat[15]);
    }

    if (dmp->dm_light) {
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, wireColor);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);

	if (dmp->dm_transparency)
	    glDisable(GL_BLEND);
    }

    glBegin(GL_LINES);
    glVertex3dv(pt1);
    glVertex3dv(pt2);
    glEnd();

    return TCL_OK;
}


/*
 * T O G L _ D R A W L I N E S 3 D
 *
 */
HIDDEN int
togl_drawLines3D(struct dm *dmp, int npoints, point_t *points)
{
    int i;
    static float black[4] = {0.0, 0.0, 0.0, 0.0};

    if (dmp->dm_debugLevel)
	bu_log("togl_drawLine3D()\n");

    if (dmp->dm_debugLevel) {
	GLfloat pmat[16];

	glGetFloatv(GL_PROJECTION_MATRIX, pmat);
	bu_log("projection matrix:\n");
	bu_log("%g %g %g %g\n", pmat[0], pmat[4], pmat[8], pmat[12]);
	bu_log("%g %g %g %g\n", pmat[1], pmat[5], pmat[9], pmat[13]);
	bu_log("%g %g %g %g\n", pmat[2], pmat[6], pmat[10], pmat[14]);
	bu_log("%g %g %g %g\n", pmat[3], pmat[7], pmat[11], pmat[15]);
	glGetFloatv(GL_MODELVIEW_MATRIX, pmat);
	bu_log("modelview matrix:\n");
	bu_log("%g %g %g %g\n", pmat[0], pmat[4], pmat[8], pmat[12]);
	bu_log("%g %g %g %g\n", pmat[1], pmat[5], pmat[9], pmat[13]);
	bu_log("%g %g %g %g\n", pmat[2], pmat[6], pmat[10], pmat[14]);
	bu_log("%g %g %g %g\n", pmat[3], pmat[7], pmat[11], pmat[15]);
    }

    /* Must be an even number of points */
    if (npoints%2)
	return TCL_OK;

    if (dmp->dm_light) {
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, wireColor);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);

	if (dmp->dm_transparency)
	    glDisable(GL_BLEND);
    }

    glBegin(GL_LINES);
    for (i = 0; i < npoints; ++i)
	glVertex3dv(points[i]);
    glEnd();

    return TCL_OK;
}


HIDDEN int
togl_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y)
{
    if (dmp->dm_debugLevel) {
	bu_log("togl_drawPoint2D():\n");
	bu_log("\tdmp: %p\tx - %lf\ty - %lf\n", (void *)dmp, x, y);
    }

    glBegin(GL_POINTS);
    glVertex2f(x, y);
    glEnd();

    return TCL_OK;
}


HIDDEN int
togl_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    if (dmp->dm_debugLevel)
	bu_log("togl_setFGColor()\n");

    dmp->dm_fg[0] = r;
    dmp->dm_fg[1] = g;
    dmp->dm_fg[2] = b;

    /* wireColor gets the full rgb */
    wireColor[0] = r / 255.0;
    wireColor[1] = g / 255.0;
    wireColor[2] = b / 255.0;
    wireColor[3] = transparency;

    if (strict) {
	glColor3ub((GLubyte)r, (GLubyte)g, (GLubyte)b);
    } else {

	if (dmp->dm_light) {
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

#if 1
	    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambientColor);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuseColor);
#endif

	} else {
	    glColor3ub((GLubyte)r,  (GLubyte)g,  (GLubyte)b);
	}
    }

    return TCL_OK;
}


HIDDEN int
togl_setLineAttr(struct dm *dmp, int width, int style)
{
    if (dmp->dm_debugLevel)
	bu_log("togl_setLineAttr()\n");

    dmp->dm_lineWidth = width;
    dmp->dm_lineStyle = style;

    glLineWidth((GLfloat) width);

    if (style == DM_DASHED_LINE)
	glEnable(GL_LINE_STIPPLE);
    else
	glDisable(GL_LINE_STIPPLE);

    return TCL_OK;
}


HIDDEN int
togl_debug(struct dm *dmp, int lvl)
{
    dmp->dm_debugLevel = lvl;

    return TCL_OK;
}


HIDDEN int
togl_setWinBounds(struct dm *dmp, int *w)
{
    if (dmp->dm_debugLevel)
	bu_log("togl_setWinBounds()\n");

    dmp->dm_clipmin[0] = w[0];
    dmp->dm_clipmin[1] = w[2];
    dmp->dm_clipmin[2] = w[4];
    dmp->dm_clipmax[0] = w[1];
    dmp->dm_clipmax[1] = w[3];
    dmp->dm_clipmax[2] = w[5];

    if (dmp->dm_clipmax[2] <= GED_MAX)
	dmp->dm_bound = 1.0;
    else
	dmp->dm_bound = GED_MAX / dmp->dm_clipmax[2];

    return TCL_OK;
}


HIDDEN int
togl_setTransparency(struct dm *dmp,
		    int transparency_on)
{
    if (dmp->dm_debugLevel)
	bu_log("togl_setTransparency()\n");

    dmp->dm_transparency = transparency_on;
    ((struct togl_vars *)dmp->dm_vars.priv_vars)->mvars.transparency_on = dmp->dm_transparency;

    Togl_MakeCurrent(((struct togl_vars *)dmp->dm_vars.priv_vars)->togl);

    if (transparency_on) {
	/* Turn it on */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
	/* Turn it off */
	glDisable(GL_BLEND);
    }

    return TCL_OK;
}


HIDDEN int
togl_setDepthMask(struct dm *dmp,
		 int enable) {
    if (dmp->dm_debugLevel)
	bu_log("togl_setDepthMask()\n");

    dmp->dm_depthMask = enable;

    Togl_MakeCurrent(((struct togl_vars *)dmp->dm_vars.priv_vars)->togl);

    if (enable)
	glDepthMask(GL_TRUE);
    else
	glDepthMask(GL_FALSE);

    return TCL_OK;
}

HIDDEN int
togl_setZBuffer(struct dm *dmp, int zbuffer_on)
{
    if (dmp->dm_debugLevel)
	bu_log("togl_setZBuffer:\n");

    dmp->dm_zbuffer = zbuffer_on;
    ((struct togl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on = dmp->dm_zbuffer;

    Togl_MakeCurrent(((struct togl_vars *)dmp->dm_vars.priv_vars)->togl);

    if (((struct togl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuf == 0) {
	dmp->dm_zbuffer = 0;
	((struct togl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on = dmp->dm_zbuffer;
    }

    if (((struct togl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on) {
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
    } else {
	glDisable(GL_DEPTH_TEST);
    }

    return TCL_OK;
}

int
togl_beginDList(struct dm *dmp, unsigned int list)
{
    if (dmp->dm_debugLevel)
	bu_log("ogl_beginDList()\n");

    Togl_MakeCurrent(((struct togl_vars *)dmp->dm_vars.priv_vars)->togl);

    glNewList(dmp->dm_displaylist + list, GL_COMPILE);
    return TCL_OK;
}

int
togl_endDList(struct dm *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("ogl_endDList()\n");

    glEndList();
    return TCL_OK;
}

int
togl_drawDList(struct dm *dmp, unsigned int list)
{
    if (dmp->dm_debugLevel)
	bu_log("ogl_drawDList()\n");

    glCallList(dmp->dm_displaylist + list);
    return TCL_OK;
}

int
togl_freeDLists(struct dm *dmp, unsigned int list, int range)
{
    if (dmp->dm_debugLevel)
	bu_log("ogl_freeDLists()\n");

    Togl_MakeCurrent(((struct togl_vars *)dmp->dm_vars.priv_vars)->togl);

    glDeleteLists(dmp->dm_displaylist + list, (GLsizei)range);
    return TCL_OK;
}

#endif /* DM_TOGL */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

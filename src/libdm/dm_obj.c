/*                        D M _ O B J . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file dm_obj.c
 *
 * A display manager object contains the attributes and
 * methods for controlling display managers.
 *
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 * Author -
 *	Robert G. Parker
 */

#include "common.h"

#include <stdlib.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#include <math.h>
#include "tcl.h"

#ifdef HAVE_GL_GLX_H
#  include <GL/glx.h>
#endif
#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif

#include "machine.h"
#include "cmd.h"                  /* includes bu.h */
#include "vmath.h"
#include "bn.h"
#include "db.h"
#include "mater.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "nurb.h"
#include "solid.h"
#include "dm.h"
#include "png.h"
#include "zlib.h"
#include "dm_xvars.h"

#ifdef DM_X
#  include "tk.h"
#  include <X11/Xutil.h>
#  include "dm-X.h"
#endif /* DM_X */

#ifdef DM_OGL
#  include "dm-ogl.h"
#endif /* DM_OGL */

#ifdef DM_WGL
#  include <tkwinport.h>
#  include "dm-wgl.h"
#endif /* DM_WGL */

#ifdef USE_FBSERV
#  include "fb.h"
#endif

static int dmo_open_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
#if 0
static int dmo_close_tcl();
#endif
static int dmo_drawBegin_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_drawEnd_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_clear_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_normal_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_loadmat_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_drawModelAxes_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_drawViewAxes_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_drawCenterDot_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_drawString_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_drawPoint_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_drawLine_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_drawVList_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_drawSList_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_drawGeom_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_drawLabels_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_drawScale_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_fg_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_bg_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_lineWidth_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_lineStyle_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_configure_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_zclip_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_zbuffer_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_light_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_transparency_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_depthMask_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_bounds_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_perspective_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_debug_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
#ifdef USE_FBSERV
static int dmo_openFb();
static int dmo_closeFb();
static int dmo_listen_tcl();
static int dmo_refreshFb_tcl();
static void dmo_fbs_callback();
#endif
static int dmo_flush_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_sync_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_size_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_get_aspect_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_observer_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
#ifndef _WIN32
static int dmo_png_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
#endif
static int dmo_clearBufferAfter_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_getDrawLabelsHook_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int dmo_setDrawLabelsHook_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);


static struct dm_obj HeadDMObj;	/* head of display manager object list */

static struct bu_cmdtab dmo_cmds[] = {
	{"bg",			dmo_bg_tcl},
	{"bounds",		dmo_bounds_tcl},
	{"clear",		dmo_clear_tcl},
#if 0
	{"close",		dmo_close_tcl},
#endif
	{"configure",		dmo_configure_tcl},
	{"debug",		dmo_debug_tcl},
	{"depthMask",		dmo_depthMask_tcl},
	{"drawBegin",		dmo_drawBegin_tcl},
	{"drawEnd",		dmo_drawEnd_tcl},
	{"drawGeom",		dmo_drawGeom_tcl},
	{"drawLabels",		dmo_drawLabels_tcl},
	{"drawLine",		dmo_drawLine_tcl},
	{"drawPoint",		dmo_drawPoint_tcl},
	{"drawScale",		dmo_drawScale_tcl},
	{"drawSList",		dmo_drawSList_tcl},
	{"drawString",		dmo_drawString_tcl},
	{"drawVList",		dmo_drawVList_tcl},
	{"drawModelAxes",	dmo_drawModelAxes_tcl},
	{"drawViewAxes",	dmo_drawViewAxes_tcl},
	{"drawCenterDot",	dmo_drawCenterDot_tcl},
	{"fg",			dmo_fg_tcl},
	{"flush",		dmo_flush_tcl},
	{"get_aspect",		dmo_get_aspect_tcl},
	{"getDrawLabelsHook",	dmo_getDrawLabelsHook_tcl},
	{"light",		dmo_light_tcl},
	{"linestyle",		dmo_lineStyle_tcl},
	{"linewidth",		dmo_lineWidth_tcl},
#ifdef USE_FBSERV
	{"listen",		dmo_listen_tcl},
#endif
	{"loadmat",		dmo_loadmat_tcl},
	{"normal",		dmo_normal_tcl},
	{"observer",		dmo_observer_tcl},
	{"perspective",		dmo_perspective_tcl},
#ifndef _WIN32
	{"png",		        dmo_png_tcl},
#endif
#ifdef USE_FBSERV
	{"refreshfb",		dmo_refreshFb_tcl},
#endif
	{"clearBufferAfter",    dmo_clearBufferAfter_tcl},
	{"setDrawLabelsHook",	dmo_setDrawLabelsHook_tcl},
	{"size",		dmo_size_tcl},
	{"sync",		dmo_sync_tcl},
	{"transparency",	dmo_transparency_tcl},
	{"zbuffer",		dmo_zbuffer_tcl},
	{"zclip",		dmo_zclip_tcl},
	{(char *)0,		(int (*)())0}
};

/*
 *			D M _ C M D
 *
 * Generic interface for display manager object routines.
 * Usage:
 *        objname cmd ?args?
 *
 * Returns: result of DM command.
 */
static int
dmo_cmd(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	return bu_cmd(clientData, interp, argc, argv, dmo_cmds, 1);
}

int
Dmo_Init(Tcl_Interp *interp)
{
	BU_LIST_INIT(&HeadDMObj.l);
	(void)Tcl_CreateCommand(interp, "dm_open", (Tcl_CmdProc *)dmo_open_tcl,(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	return TCL_OK;
}

/*
 * Called by Tcl when the object is destroyed.
 */
static void
dmo_deleteProc(ClientData clientData)
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;

	/* free observers */
	bu_observer_free(&dmop->dmo_observers);

#ifdef USE_FBSERV
	/* close framebuffer */
	dmo_closeFb(dmop);
#endif

	bu_vls_free(&dmop->dmo_name);
	DM_CLOSE(dmop->dmo_dmp);
	BU_LIST_DEQUEUE(&dmop->l);
	bu_free((genptr_t)dmop, "dmo_deleteProc: dmop");

}

#if 0
/*
 * Close a display manager object.
 *
 * Usage:
 *	  objname close
 */
static int
dmo_close_tcl(clientData, interp, argc, argv)
     ClientData	clientData;
     Tcl_Interp	*interp;
     int	argc;
     char	**argv;
{
	struct bu_vls vls;
	struct dm_obj *dmop = (struct dm_obj *)clientData;

	if (argc != 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib_alias dm_close %s", argv[1]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* Among other things, this will call dmo_deleteProc. */
	Tcl_DeleteCommand(interp, bu_vls_addr(&dmop->dmo_name));

	return TCL_OK;
}
#endif

/*
 * Open/create a display manager object.
 *
 * Usage:
 *	  dm_open [name type [args]]
 */
static int
dmo_open_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj		*dmop;
	struct dm		*dmp;
	struct bu_vls		vls;
	int			name_index = 1;
	int			type = DM_TYPE_BAD;
	Tcl_Obj			*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	if (argc == 1) {
		/* get list of display manager objects */
		for (BU_LIST_FOR(dmop, dm_obj, &HeadDMObj.l))
			Tcl_AppendStringsToObj(obj, bu_vls_addr(&dmop->dmo_name), " ", (char *)NULL);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	if (argc < 3) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib_alias dm_open %s", argv[1]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* check to see if display manager object exists */
	for (BU_LIST_FOR(dmop, dm_obj, &HeadDMObj.l)) {
		if (strcmp(argv[name_index],bu_vls_addr(&dmop->dmo_name)) == 0) {
			Tcl_AppendStringsToObj(obj, "dmo_open: ", argv[name_index],
					       " exists.", (char *)NULL);
			Tcl_SetObjResult(interp, obj);
			return TCL_ERROR;
		}
	}

	/* find display manager type */
#ifdef DM_X
	if (argv[2][0] == 'X' || argv[2][0] == 'x')
	    type = DM_TYPE_X;
#endif /* DM_X */

#ifdef DM_OGL
	if (!strcmp(argv[2], "ogl"))
	    type = DM_TYPE_OGL;
#endif /* DM_OGL */

#ifdef DM_WGL
	if (!strcmp(argv[2], "wgl"))
	    type = DM_TYPE_WGL;
#endif /* DM_WGL */

	if (type == DM_TYPE_BAD) {
		Tcl_AppendStringsToObj(obj,
				       "Unsupported display manager type - ",
				       argv[2], "\n",
				       "The supported types are: X, ogl, wgl, and nu",
				       (char *)NULL);
		Tcl_SetObjResult(interp, obj);
		return TCL_ERROR;
	}

	{
		int i;
		int arg_start = 3;
		int newargs = 2;
		int ac;
		char **av;

		ac = argc + newargs;
		av = (char **)bu_malloc(sizeof(char *) * (ac+1), "dmo_open_tcl: av");
		av[0] = argv[0];

		/* Insert new args (i.e. arrange to call init_dm_obj from dm_open()) */
		av[1] = "-i";
		av[2] = "init_dm_obj";

		/*
		 * Stuff name into argument list.
		 */
		av[3] = "-n";
		av[4] = argv[name_index];

		/* copy the rest */
		for (i = arg_start; i < argc; ++i)
			av[i+newargs] = argv[i];
		av[i+newargs] = (char *)NULL;

		if ((dmp = dm_open(interp, type, ac, av)) == DM_NULL) {
			if (Tcl_IsShared(obj))
				obj = Tcl_DuplicateObj(obj);

			Tcl_AppendStringsToObj(obj,
					       "dmo_open_tcl: Failed to open - ",
					       argv[name_index],
					       "\n",
					       (char *)NULL);
			bu_free((genptr_t)av, "dmo_open_tcl: av");

			Tcl_SetObjResult(interp, obj);
			return TCL_ERROR;
		}

		bu_free((genptr_t)av, "dmo_open_tcl: av");
	}

	/* acquire dm_obj struct */
	BU_GETSTRUCT(dmop,dm_obj);

	/* initialize dm_obj */
	bu_vls_init(&dmop->dmo_name);
	bu_vls_strcpy(&dmop->dmo_name,argv[name_index]);
	dmop->dmo_dmp = dmp;
	VSETALL(dmop->dmo_dmp->dm_clipmin, -2048.0);
	VSETALL(dmop->dmo_dmp->dm_clipmax, 2047.0);
	dmop->dmo_drawLabelsHook = (int (*)())0;

#ifdef USE_FBSERV
	dmop->dmo_fbs.fbs_listener.fbsl_fbsp = &dmop->dmo_fbs;
	dmop->dmo_fbs.fbs_listener.fbsl_fd = -1;
	dmop->dmo_fbs.fbs_listener.fbsl_port = -1;
	dmop->dmo_fbs.fbs_fbp = FBIO_NULL;
	dmop->dmo_fbs.fbs_callback = dmo_fbs_callback;
	dmop->dmo_fbs.fbs_clientData = dmop;
#endif

	BU_LIST_INIT(&dmop->dmo_observers.l);

	/* append to list of dm_obj's */
	BU_LIST_APPEND(&HeadDMObj.l,&dmop->l);

	(void)Tcl_CreateCommand(interp,
				bu_vls_addr(&dmop->dmo_name),
				(Tcl_CmdProc *)dmo_cmd,
				(ClientData)dmop,
				dmo_deleteProc);

	/* send Configure event */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "event generate %s <Configure>; update", bu_vls_addr(&dmop->dmo_name));
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

#ifdef USE_FBSERV
	/* open the framebuffer */
	dmo_openFb(dmop, interp);
#endif

	/* Return new function name as result */
	Tcl_SetResult(interp, bu_vls_addr(&dmop->dmo_name), TCL_VOLATILE);
	return TCL_OK;
}

static int
dmo_parseAxesArgs(Tcl_Interp *interp,
		  int argc,
		  char **argv,
		  fastf_t *viewSize,
		  mat_t rmat,
		  point_t axesPos,
		  fastf_t *axesSize,
		  int *axesColor,
		  int *labelColor,
		  int *lineWidth,
		  int *posOnly,
		  int *threeColor,
		  struct bu_vls *vlsp)
{
  if (sscanf(argv[2], "%lf", viewSize) != 1) {
    bu_vls_printf(vlsp, "parseAxesArgs: bad view size - %s\n", argv[2]);
    return TCL_ERROR;
  }

  if (bn_decode_mat(rmat, argv[3]) != 16) {
    bu_vls_printf(vlsp, "parseAxesArgs: bad rmat - %s\n", argv[3]);
    return TCL_ERROR;
  }

  if (bn_decode_vect(axesPos, argv[4]) != 3) {
    bu_vls_printf(vlsp, "parseAxesArgs: bad axes position - %s\n", argv[4]);
    return TCL_ERROR;
  }

  if (sscanf(argv[5], "%lf", axesSize) != 1) {
    bu_vls_printf(vlsp, "parseAxesArgs: bad axes size - %s\n", argv[5]);
    return TCL_ERROR;
  }

  if (sscanf(argv[6], "%d %d %d",
	     &axesColor[0],
	     &axesColor[1],
	     &axesColor[2]) != 3) {

    bu_vls_printf(vlsp, "parseAxesArgs: bad axes color - %s\n", argv[6]);
    return TCL_ERROR;
  }

  /* validate color */
  if (axesColor[0] < 0 || 255 < axesColor[0] ||
      axesColor[1] < 0 || 255 < axesColor[1] ||
      axesColor[2] < 0 || 255 < axesColor[2]) {

    bu_vls_printf(vlsp, "parseAxesArgs: bad axes color - %s\n", argv[6]);
    return TCL_ERROR;
  }

  if (sscanf(argv[7], "%d %d %d",
	     &labelColor[0],
	     &labelColor[1],
	     &labelColor[2]) != 3) {

    bu_vls_printf(vlsp, "parseAxesArgs: bad label color - %s\n", argv[7]);
    return TCL_ERROR;
  }

  /* validate color */
  if (labelColor[0] < 0 || 255 < labelColor[0] ||
      labelColor[1] < 0 || 255 < labelColor[1] ||
      labelColor[2] < 0 || 255 < labelColor[2]) {

    bu_vls_printf(vlsp, "parseAxesArgs: bad label color - %s\n", argv[7]);
    return TCL_ERROR;
  }

  if (sscanf(argv[8], "%d", lineWidth) != 1) {
    bu_vls_printf(vlsp, "parseAxesArgs: bad line width - %s\n", argv[8]);
    return TCL_ERROR;
  }

  /* validate lineWidth */
  if (*lineWidth < 0) {
    bu_vls_printf(vlsp, "parseAxesArgs: line width must be greater than 0\n");
    return TCL_ERROR;
  }

  /* parse positive only flag */
  if (sscanf(argv[9], "%d", posOnly) != 1) {
    bu_vls_printf(vlsp, "parseAxesArgs: bad positive only flag - %s\n", argv[9]);
    return TCL_ERROR;
  }

  /* validate tick enable flag */
  if (*posOnly < 0) {
    bu_vls_printf(vlsp, "parseAxesArgs: positive only flag must be >= 0\n");
    return TCL_ERROR;
  }

  /* parse three color flag */
  if (sscanf(argv[10], "%d", threeColor) != 1) {
    bu_vls_printf(vlsp, "parseAxesArgs: bad three color flag - %s\n", argv[10]);
    return TCL_ERROR;
  }

  /* validate tick enable flag */
  if (*threeColor < 0) {
    bu_vls_printf(vlsp, "parseAxesArgs: three color flag must be >= 0\n");
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 * Draw the view axes.
 *
 * Usage:
 *	  objname drawViewAxes args
 *
 */
static int
dmo_drawViewAxes_tcl(ClientData	clientData,
		     Tcl_Interp	*interp,
		     int	argc,
		     char	**argv)
{
  point_t axesPos;
  fastf_t viewSize;
  mat_t rmat;
  fastf_t axesSize;
  int axesColor[3];
  int labelColor[3];
  int lineWidth;
  int posOnly;
  int threeColor;
  struct bu_vls vls;
  struct dm_obj *dmop = (struct dm_obj *)clientData;

  bu_vls_init(&vls);

  if (argc != 11) {
    /* return help message */
    bu_vls_printf(&vls, "helplib_alias dm_drawViewAxes %s", argv[1]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if (dmo_parseAxesArgs(interp, argc, argv, &viewSize, rmat, axesPos, &axesSize,
			axesColor, labelColor, &lineWidth,
			&posOnly, &threeColor, &vls) == TCL_ERROR) {
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  dmo_drawAxes_cmd(dmop->dmo_dmp, viewSize, rmat,
		   axesPos, axesSize, axesColor,
		   labelColor, lineWidth,
		   posOnly, /* positive direction only */
		   threeColor, /* three colors (i.e. X-red, Y-green, Z-blue) */
		   0, /* no ticks */
		   0, /* tick len */
		   0, /* major tick len */
		   0, /* tick interval */
		   0, /* ticks per major */
		   NULL, /* tick color */
		   NULL, /* major tick color */
		   0 /* tick threshold */);

  bu_vls_free(&vls);
  return TCL_OK;
}

/*
 * Draw the center dot.
 *
 * Usage:
 *	  drawCenterDot color
 *
 */
static int
dmo_drawCenterDot_cmd(struct dm_obj	*dmop,
		      Tcl_Interp	*interp,
		      int		argc,
		      char 		**argv)
{
    int color[3];

    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dm_drawCenterDot %s", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (sscanf(argv[1], "%d %d %d",
	       &color[0],
	       &color[1],
	       &color[2]) != 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "drawCenterDot: bad color - %s\n", argv[1]);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    /* validate color */
    if (color[0] < 0 || 255 < color[0] ||
	color[1] < 0 || 255 < color[1] ||
	color[2] < 0 || 255 < color[2]) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "drawCenterDot: bad color - %s\n", argv[1]);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    DM_SET_FGCOLOR(dmop->dmo_dmp,
		   (unsigned char)color[0],
		   (unsigned char)color[1],
		   (unsigned char)color[2], 1, 1.0);

    DM_DRAW_POINT_2D(dmop->dmo_dmp, 0.0, 0.0);

    return TCL_OK;
}

/*
 * Draw the center dot.
 *
 * Usage:
 *	  objname drawCenterDot color
 *
 */
static int
dmo_drawCenterDot_tcl(ClientData	clientData,
		      Tcl_Interp	*interp,
		      int		argc,
		      char		**argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    return dmo_drawCenterDot_cmd(dmop, interp, argc-1, argv+1);
}

static int
dmo_parseModelAxesArgs(Tcl_Interp *interp,
		       int argc,
		       char **argv,
		       fastf_t *viewSize,
		       mat_t rmat,
		       point_t axesPos,
		       fastf_t *axesSize,
		       int *axesColor,
		       int *labelColor,
		       int *lineWidth,
		       int *posOnly,
		       int *threeColor,
		       mat_t model2view,
		       int *tickEnable,
		       int *tickLength,
		       int *majorTickLength,
		       fastf_t *tickInterval,
		       int *ticksPerMajor,
		       int *tickColor,
		       int *majorTickColor,
		       int *tickThreshold,
		       struct bu_vls *vlsp)
{
  if (dmo_parseAxesArgs(interp, argc, argv, viewSize, rmat, axesPos, axesSize,
			axesColor, labelColor, lineWidth,
			posOnly, threeColor, vlsp) == TCL_ERROR)
    return TCL_ERROR;

  /* parse model to view matrix */
  if (bn_decode_mat(model2view, argv[11]) != 16) {
    bu_vls_printf(vlsp, "parseModelAxesArgs: bad model2view - %s\n", argv[11]);
    return TCL_ERROR;
  }

  /* parse tick enable flag */
  if (sscanf(argv[12], "%d", tickEnable) != 1) {
    bu_vls_printf(vlsp, "parseModelAxesArgs: bad tick enable flag - %s\n", argv[12]);
    return TCL_ERROR;
  }

  /* validate tick enable flag */
  if (*tickEnable < 0) {
    bu_vls_printf(vlsp, "parseModelAxesArgs: tick enable flag must be >= 0\n");
    return TCL_ERROR;
  }

  /* parse tick length */
  if (sscanf(argv[13], "%d", tickLength) != 1) {
    bu_vls_printf(vlsp, "parseModelAxesArgs: bad tick length - %s\n", argv[13]);
    return TCL_ERROR;
  }

  /* validate tick length */
  if (*tickLength < 1) {
    bu_vls_printf(vlsp, "parseModelAxesArgs: tick length must be >= 1\n");
    return TCL_ERROR;
  }

  /* parse major tick length */
  if (sscanf(argv[14], "%d", majorTickLength) != 1) {
    bu_vls_printf(vlsp, "parseModelAxesArgs: bad major tick length - %s\n", argv[14]);
    return TCL_ERROR;
  }

  /* validate major tick length */
  if (*majorTickLength < 1) {
    bu_vls_printf(vlsp, "parseModelAxesArgs: major tick length must be >= 1\n");
    return TCL_ERROR;
  }

  /* parse tick interval */
  if (sscanf(argv[15], "%lf", tickInterval) != 1) {
    bu_vls_printf(vlsp, "parseModelAxesArgs: tick interval must be > 0");
    return TCL_ERROR;
  }

  /* validate tick interval */
  if (*tickInterval <= 0) {
    bu_vls_printf(vlsp, "parseModelAxesArgs: tick interval must be > 0");
    return TCL_ERROR;
  }

  /* parse ticks per major */
  if (sscanf(argv[16], "%d", ticksPerMajor) != 1) {
    bu_vls_printf(vlsp, "parseModelAxesArgs: bad ticks per major - %s\n", argv[16]);
    return TCL_ERROR;
  }

  /* validate ticks per major */
  if (*ticksPerMajor < 0) {
    bu_vls_printf(vlsp, "parseModelAxesArgs: ticks per major must be >= 0\n");
    return TCL_ERROR;
  }

  /* parse tick color */
  if (sscanf(argv[17], "%d %d %d",
	     &tickColor[0],
	     &tickColor[1],
	     &tickColor[2]) != 3) {

    bu_vls_printf(vlsp, "parseModelAxesArgs: bad tick color - %s\n", argv[17]);
    return TCL_ERROR;
  }

  /* validate tick color */
  if (tickColor[0] < 0 || 255 < tickColor[0] ||
      tickColor[1] < 0 || 255 < tickColor[1] ||
      tickColor[2] < 0 || 255 < tickColor[2]) {

    bu_vls_printf(vlsp, "parseModelAxesArgs: bad tick color - %s\n", argv[17]);
    return TCL_ERROR;
  }

  /* parse major tick color */
  if (sscanf(argv[18], "%d %d %d",
	     &majorTickColor[0],
	     &majorTickColor[1],
	     &majorTickColor[2]) != 3) {

    bu_vls_printf(vlsp, "parseModelAxesArgs: bad major tick color - %s\n", argv[18]);
    return TCL_ERROR;
  }

  /* validate tick color */
  if (majorTickColor[0] < 0 || 255 < majorTickColor[0] ||
      majorTickColor[1] < 0 || 255 < majorTickColor[1] ||
      majorTickColor[2] < 0 || 255 < majorTickColor[2]) {

    bu_vls_printf(vlsp, "parseModelAxesArgs: bad major tick color - %s\n", argv[18]);
    return TCL_ERROR;
  }

  /* parse tick threshold */
  if (sscanf(argv[19], "%d", tickThreshold) != 1) {
    bu_vls_printf(vlsp, "parseModelAxesArgs: bad tick threshold - %s\n", argv[19]);
    return TCL_ERROR;
  }

  /* validate tick threshold */
  if (*tickThreshold <= 0) {
    bu_vls_printf(vlsp, "parseModelAxesArgs: tick threshold must be > 0\n");
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 * Draw the model axes.
 *
 * Usage:
 *	  objname drawModelAxes args
 *
 */
static int
dmo_drawModelAxes_tcl(ClientData	clientData,
		      Tcl_Interp	*interp,
		      int		argc,
		      char		**argv)
{
  point_t modelAxesPos;
  point_t viewAxesPos;
  fastf_t viewSize;
  mat_t rmat;
  mat_t model2view;
  fastf_t axesSize;
  int axesColor[3];
  int labelColor[3];
  int lineWidth;
  int posOnly;
  int threeColor;
  int tickEnable;
  int tickLength;
  int majorTickLength;
  fastf_t tickInterval;
  int ticksPerMajor;
  int tickColor[3];
  int majorTickColor[3];
  int tickThreshold;
  struct bu_vls vls;
  struct dm_obj *dmop = (struct dm_obj *)clientData;

  bu_vls_init(&vls);

  if (argc != 20) {
    /* return help message */
    bu_vls_printf(&vls, "helplib_alias dm_drawModelAxes %s", argv[1]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if (dmo_parseModelAxesArgs(interp, argc, argv,
			     &viewSize, rmat, modelAxesPos,
			     &axesSize, axesColor,
			     labelColor, &lineWidth,
			     &posOnly, &threeColor,
			     model2view, &tickEnable,
			     &tickLength, &majorTickLength,
			     &tickInterval, &ticksPerMajor,
			     tickColor, majorTickColor,
			     &tickThreshold, &vls) == TCL_ERROR) {
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  MAT4X3PNT(viewAxesPos, model2view, modelAxesPos);

  dmo_drawAxes_cmd(dmop->dmo_dmp, viewSize, rmat,
		   viewAxesPos, axesSize, axesColor,
		   labelColor, lineWidth,
		   posOnly, threeColor,
		   tickEnable,
		   tickLength, majorTickLength,
		   tickInterval, ticksPerMajor,
		   tickColor, majorTickColor,
		   tickThreshold);

  bu_vls_free(&vls);
  return TCL_OK;
}

/*
 * Begin the draw cycle.
 *
 * Usage:
 *	  objname drawBegin
 *
 */
static int
dmo_drawBegin_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;

	return DM_DRAW_BEGIN(dmop->dmo_dmp);
}

static int
dmo_drawEnd_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;

	return DM_DRAW_END(dmop->dmo_dmp);
}

/*
 * End the draw cycle.
 *
 * Usage:
 *	  objname drawEnd
 *
 */
static int
dmo_clear_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;
	int status;

	if ((status = DM_DRAW_BEGIN(dmop->dmo_dmp)) != TCL_OK)
		return status;

	return DM_DRAW_END(dmop->dmo_dmp);
}

/*
 * Clear the display.
 *
 * Usage:
 *	  objname clear
 *
 */
static int
dmo_normal_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;

	return DM_NORMAL(dmop->dmo_dmp);
}

/*
 * Reset the viewing transform.
 *
 * Usage:
 *	  objname normal
 *
 */
static int
dmo_loadmat_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	mat_t		mat;
	int		which_eye;

	if (argc != 4) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib_alias dm_loadmat %s", argv[1]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}


	if (bn_decode_mat(mat, argv[2]) != 16)
		return TCL_ERROR;

	if (sscanf(argv[3], "%d", &which_eye) != 1) {
		Tcl_Obj		*obj;

		obj = Tcl_GetObjResult(interp);
		if (Tcl_IsShared(obj))
			obj = Tcl_DuplicateObj(obj);

		Tcl_AppendStringsToObj(obj, "bad eye value - ", argv[3], (char *)NULL);
		Tcl_SetObjResult(interp, obj);
		return TCL_ERROR;
	}

	MAT_COPY(dmop->viewMat, mat);

	return DM_LOADMATRIX(dmop->dmo_dmp, mat, which_eye);
}

/*
 * Draw a string on the display.
 *
 * Usage:
 *	  objname drawString args
 *
 */
static int
dmo_drawString_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;
	fastf_t x, y;
	int size;
	int use_aspect;

	if (argc != 7) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib_alias dm_drawString %s", argv[1]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/*XXX use sscanf */
	x = atof(argv[3]);
	y = atof(argv[4]);
	size = atoi(argv[5]);
	use_aspect = atoi(argv[6]);

	return DM_DRAW_STRING_2D(dmop->dmo_dmp,argv[2],x,y,size,use_aspect);
}

static int
dmo_drawPoint_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	fastf_t		x, y;

	if (argc != 4) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib_alias dm_drawPoint %s", argv[1]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/*XXX use sscanf */
	x = atof(argv[2]);
	y = atof(argv[3]);

	return DM_DRAW_POINT_2D(dmop->dmo_dmp,x,y);
}

/*
 * Draw the line.
 *
 * Usage:
 *	  objname drawLine x1 y1 x2 y2
 *
 */
static int
dmo_drawLine_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;
	fastf_t x1, y1, x2, y2;

	if (argc != 6) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib_alias dm_drawLine %s", argv[1]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/*XXX use sscanf */
	x1 = atof(argv[2]);
	y1 = atof(argv[3]);
	x2 = atof(argv[4]);
	y2 = atof(argv[5]);

	return DM_DRAW_LINE_2D(dmop->dmo_dmp,x1,y1,x2,y2);
}

/*
 * Draw the vlist.
 *
 * Usage:
 *	  objname drawVList vid
 */
static int
dmo_drawVList_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;
	struct rt_vlist *vp;

	if (argc != 3) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib_alias dm_drawVList %s", argv[1]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lu", (unsigned long *)&vp) != 1) {
		Tcl_Obj	*obj;

		obj = Tcl_GetObjResult(interp);
		if (Tcl_IsShared(obj))
			obj = Tcl_DuplicateObj(obj);

		Tcl_AppendStringsToObj(obj, "invalid vlist pointer - ", argv[2], (char *)NULL);
		Tcl_SetObjResult(interp, obj);
		return TCL_ERROR;
	}

	/* XXX this causes a core dump if vp is bogus */
	BN_CK_VLIST_TCL(interp,vp);

	return DM_DRAW_VLIST(dmop->dmo_dmp, vp);
}

static void
dmo_drawSolid(struct dm_obj	*dmop,
	      struct solid	*sp)
{
	if (sp->s_iflag == UP)
		DM_SET_FGCOLOR(dmop->dmo_dmp, 255, 255, 255, 0, sp->s_transparency);
	else
		DM_SET_FGCOLOR(dmop->dmo_dmp,
			       (unsigned char)sp->s_color[0],
			       (unsigned char)sp->s_color[1],
			       (unsigned char)sp->s_color[2], 0, sp->s_transparency);

	DM_DRAW_VLIST(dmop->dmo_dmp, (struct rt_vlist *)&sp->s_vlist);
}

/*
 * Draw a scale.
 *
 * Usage:
 *       drawScale vsize color
 *
 */
int
dmo_drawScale_cmd(struct dm_obj	*dmop,
		  Tcl_Interp	*interp,
		  int		argc,
		  char 		**argv)
{
    int color[3];
    int soffset;
    fastf_t viewSize;
    fastf_t x1, x2;
    fastf_t y1, y2;
    struct bu_vls vls;

    if (argc != 3) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dm_drawScale %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (sscanf(argv[1], "%lf", &viewSize) != 1) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "drawScale: bad view size - %s\n", argv[1]);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (sscanf(argv[2], "%d %d %d",
	       &color[0],
	       &color[1],
	       &color[2]) != 3) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "drawScale: bad color - %s\n", argv[2]);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    /* validate color */
    if (color[0] < 0 || 255 < color[0] ||
	color[1] < 0 || 255 < color[1] ||
	color[2] < 0 || 255 < color[2]) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "drawScale: bad color - %s\n", argv[2]);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    DM_SET_FGCOLOR(dmop->dmo_dmp,
		   (unsigned char)color[0],
		   (unsigned char)color[1],
		   (unsigned char)color[2], 1, 1.0);


    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%g", viewSize*0.5);
    soffset = (int)(strlen(bu_vls_addr(&vls)) * 0.5);

    x1 = -0.5;
    x2 = 0.5;
    y1 = -0.8;
    y2 = -0.8;
    DM_DRAW_LINE_2D(dmop->dmo_dmp,x1,y1,x2,y2);
    DM_DRAW_LINE_2D(dmop->dmo_dmp,x1,y1+0.01,x1,y1-0.01);
    DM_DRAW_LINE_2D(dmop->dmo_dmp,x2,y1+0.01,x2,y1-0.01);
    DM_DRAW_STRING_2D(dmop->dmo_dmp, "0", x1-0.005, y1 + 0.02, 1, 0);
    DM_DRAW_STRING_2D(dmop->dmo_dmp, bu_vls_addr(&vls),
		      x2-(soffset * 0.015),
		      y1 + 0.02, 1, 0);

    bu_vls_free(&vls);

    return TCL_OK;
}

/*
 * Usage:
 *	  objname drawScale vsize color
 */
static int
dmo_drawScale_tcl(ClientData clientData,
		  Tcl_Interp *interp,
		  int	     argc,
		  char	     **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    return dmo_drawScale_cmd(dmop, interp, argc-1, argv+1);
}

/*
 * Usage:
 *	  objname drawSList hsp
 */
static int
dmo_drawSList(struct dm_obj	*dmop,
	      struct bu_list	*hsp)
{
	struct solid *sp;
	int linestyle = -1;

	if (dmop->dmo_dmp->dm_transparency) {
	  /* First, draw opaque stuff */
	  FOR_ALL_SOLIDS(sp, hsp) {
	    if (sp->s_transparency < 1.0)
	      continue;

	    if (linestyle != sp->s_soldash) {
	      linestyle = sp->s_soldash;
	      DM_SET_LINE_ATTR(dmop->dmo_dmp, dmop->dmo_dmp->dm_lineWidth, linestyle);
	    }

	    dmo_drawSolid(dmop, sp);
	  }

	  /* disable write to depth buffer */
	  DM_SET_DEPTH_MASK(dmop->dmo_dmp, 0);

	  /* Second, draw transparent stuff */
	  FOR_ALL_SOLIDS(sp, hsp) {
	    /* already drawn above */
	    if (sp->s_transparency == 1.0)
	      continue;

	    if (linestyle != sp->s_soldash) {
	      linestyle = sp->s_soldash;
	      DM_SET_LINE_ATTR(dmop->dmo_dmp, dmop->dmo_dmp->dm_lineWidth, linestyle);
	    }

	    dmo_drawSolid(dmop, sp);
	  }

	  /* re-enable write to depth buffer */
	  DM_SET_DEPTH_MASK(dmop->dmo_dmp, 1);
	} else {

	  FOR_ALL_SOLIDS(sp, hsp) {
	    if (linestyle != sp->s_soldash) {
	      linestyle = sp->s_soldash;
	      DM_SET_LINE_ATTR(dmop->dmo_dmp, dmop->dmo_dmp->dm_lineWidth, linestyle);
	    }

	    dmo_drawSolid(dmop, sp);
	  }
	}

	return TCL_OK;
}

/*
 * Usage:
 *	  objname drawSList sid
 */
static int
dmo_drawSList_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_list	*hsp;

	if (argc != 3) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib_alias dm_drawSList %s", argv[1]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lu", (unsigned long *)&hsp) != 1) {
		Tcl_Obj	*obj;

		obj = Tcl_GetObjResult(interp);
		if (Tcl_IsShared(obj))
			obj = Tcl_DuplicateObj(obj);

		Tcl_AppendStringsToObj(obj, "invalid solid list pointer - ",
				 argv[2], "\n", (char *)NULL);

		Tcl_SetObjResult(interp, obj);
		return TCL_ERROR;
	}
	dmo_drawSList(dmop, hsp);

	return TCL_OK;
}

/*
 * Draw "drawable geometry" objects.
 *
 * Usage:
 *	  objname drawGeom dg_obj(s)
 */
static int
dmo_drawGeom_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;
	struct dg_obj *dgop;
	struct bu_vls vls;
	register int i;

	if (argc < 3) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib_alias dm_drawGeom %s", argv[1]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	argc -= 2;
	argv += 2;
	for (i = 0; i < argc; ++i) {
		for (BU_LIST_FOR(dgop, dg_obj, &HeadDGObj.l)) {
			if (strcmp(bu_vls_addr(&dgop->dgo_name), argv[i]) == 0) {
				dmo_drawSList(dmop, &dgop->dgo_headSolid);
				break;
			}
		}
	}

	return TCL_OK;
}

/*
 *			L A B E L _ E D I T E D _ S O L I D
 *
 *  Put labels on the vertices of the currently edited solid.
 *  XXX This really should use import/export interface!!!  Or be part of it.
 *
 *  This code was lifted from mged/edsol.c.
 *  XXX This should probably live in librt.
 */
void
dmo_labelPrimitive(struct dg_obj		*dgop,
		   int				*num_lines,
		   point_t			*lines,
		   struct rt_point_labels	pl[],
		   int				max_pl,
		   const mat_t			xform,
		   struct rt_db_internal	*ip)
{
    register int	i;
    point_t		work;
    point_t		pos_view;
    int		npl = 0;


#define	POINT_LABEL( _pt, _char )	{ \
	VMOVE( pl[npl].pt, _pt ); \
	pl[npl].str[0] = _char; \
	pl[npl++].str[1] = '\0'; }

#define	POINT_LABEL_STR( _pt, _str )	{ \
	VMOVE( pl[npl].pt, _pt ); \
	strncpy( pl[npl++].str, _str, sizeof(pl[0].str)-1 ); }


    RT_CK_DB_INTERNAL(ip);

    if (ip->idb_major_type != DB5_MAJORTYPE_BRLCAD)
	/* silently ignore */
	return;

    switch (ip->idb_minor_type) {
    case DB5_MINORTYPE_BRLCAD_TOR: {
	struct rt_tor_internal	*tor =
	    (struct rt_tor_internal *)ip->idb_ptr;
	fastf_t	r3, r4;
	vect_t	adir;

	RT_TOR_CK_MAGIC(tor);

	bn_vec_ortho(adir, tor->h);

	MAT4X3PNT(pos_view, xform, tor->v);
	POINT_LABEL(pos_view, 'V');

	r3 = tor->r_a - tor->r_h;
	VJOIN1(work, tor->v, r3, adir);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'I');

	r4 = tor->r_a + tor->r_h;
	VJOIN1(work, tor->v, r4, adir);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'O');

	VJOIN1(work, tor->v, tor->r_a, adir);
	VADD2(work, work, tor->h);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'H');
    }

	break;
    case DB5_MINORTYPE_BRLCAD_TGC: {
	struct rt_tgc_internal	*tgc =
	    (struct rt_tgc_internal *)ip->idb_ptr;

	RT_TGC_CK_MAGIC(tgc);
	MAT4X3PNT(pos_view, xform, tgc->v);
	POINT_LABEL(pos_view, 'V');

	VADD2(work, tgc->v, tgc->a);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'A');

	VADD2(work, tgc->v, tgc->b);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'B');

	VADD3(work, tgc->v, tgc->h, tgc->c);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'C');

	VADD3(work, tgc->v, tgc->h, tgc->d);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'D');
    }

	break;
    case DB5_MINORTYPE_BRLCAD_SPH:
    case DB5_MINORTYPE_BRLCAD_ELL: {
	point_t	work;
	point_t	pos_view;
	struct rt_ell_internal	*ell =
	    (struct rt_ell_internal *)ip->idb_ptr;

	RT_ELL_CK_MAGIC(ell);

	MAT4X3PNT(pos_view, xform, ell->v);
	POINT_LABEL(pos_view, 'V');

	VADD2(work, ell->v, ell->a);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'A');

	VADD2( work, ell->v, ell->b );
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL( pos_view, 'B' );

	VADD2(work, ell->v, ell->c);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'C');
    }

	break;
    case DB5_MINORTYPE_BRLCAD_ARB8: {
	int arbType;
	struct rt_arb_internal *arb=
	    (struct rt_arb_internal *)ip->idb_ptr;

	RT_ARB_CK_MAGIC(arb);
	arbType = rt_arb_std_type(ip, &dgop->dgo_wdbp->wdb_tol);

	switch (arbType) {
	case ARB8:
	    for (i=0 ; i<8 ; i++) {
		MAT4X3PNT(pos_view, xform, arb->pt[i]);
		POINT_LABEL(pos_view, i+'1');
	    }

	    break;
	case ARB7:
	    for (i=0 ; i<7 ; i++) {
		MAT4X3PNT(pos_view, xform, arb->pt[i]);
		POINT_LABEL(pos_view, i+'1');
	    }

	    break;
	case ARB6:
	    for (i=0 ; i<5 ; i++) {
		MAT4X3PNT( pos_view, xform, arb->pt[i] );
		POINT_LABEL( pos_view, i+'1' );
	    }
	    MAT4X3PNT( pos_view, xform, arb->pt[6] );
	    POINT_LABEL( pos_view, '6' );

	    break;
	case ARB5:
	    for (i=0 ; i<5 ; i++) {
		MAT4X3PNT( pos_view, xform, arb->pt[i] );
		POINT_LABEL( pos_view, i+'1' );
	    }

	    break;
	case ARB4:
	    for (i=0 ; i<3 ; i++) {
		MAT4X3PNT( pos_view, xform, arb->pt[i] );
		POINT_LABEL( pos_view, i+'1' );
	    }
	    MAT4X3PNT( pos_view, xform, arb->pt[4] );
	    POINT_LABEL( pos_view, '4' );

	    break;
	}
    }

	break;
    case DB5_MINORTYPE_BRLCAD_ARS: {
	/*XXX Needs work */
	register struct rt_ars_internal *ars=
	    (struct rt_ars_internal *)ip->idb_ptr;
	int ars_crv = 0;
	int ars_col = 0;

	RT_ARS_CK_MAGIC(ars);

	MAT4X3PNT(pos_view, xform, ars->curves[0])

	    if (ars_crv >= 0 && ars_col >= 0) {
		point_t work;
		point_t ars_pt;

		VMOVE(work, &ars->curves[ars_crv][ars_col*3]);
		MAT4X3PNT(ars_pt, xform, work);
		POINT_LABEL_STR(ars_pt, "pt");
	    }
    }
	POINT_LABEL(pos_view, 'V');

	break;
    case DB5_MINORTYPE_BRLCAD_HALF:
	break;
    case DB5_MINORTYPE_BRLCAD_REC:
	break;
    case DB5_MINORTYPE_BRLCAD_POLY:
	break;
    case DB5_MINORTYPE_BRLCAD_BSPLINE: {
	/*XXX Needs work */
	register struct rt_nurb_internal *sip =
	    (struct rt_nurb_internal *) ip->idb_ptr;
	register struct face_g_snurb	*surf;
	register fastf_t	*fp;
	int spl_surfno = 0;
	int spl_ui = 0;
	int spl_vi = 0;

	RT_NURB_CK_MAGIC(sip);
	surf = sip->srfs[spl_surfno];
	NMG_CK_SNURB(surf);
	fp = &RT_NURB_GET_CONTROL_POINT( surf, spl_ui, spl_vi );
	MAT4X3PNT(pos_view, xform, fp);
	POINT_LABEL( pos_view, 'V' );

	fp = &RT_NURB_GET_CONTROL_POINT( surf, 0, 0 );
	MAT4X3PNT(pos_view, xform, fp);
	POINT_LABEL_STR( pos_view, " 0,0" );
	fp = &RT_NURB_GET_CONTROL_POINT( surf, 0, surf->s_size[1]-1 );
	MAT4X3PNT(pos_view, xform, fp);
	POINT_LABEL_STR( pos_view, " 0,u" );
	fp = &RT_NURB_GET_CONTROL_POINT( surf, surf->s_size[0]-1, 0 );
	MAT4X3PNT(pos_view, xform, fp);
	POINT_LABEL_STR( pos_view, " v,0" );
	fp = &RT_NURB_GET_CONTROL_POINT( surf, surf->s_size[0]-1, surf->s_size[1]-1 );
	MAT4X3PNT(pos_view, xform, fp);
	POINT_LABEL_STR( pos_view, " u,v" );
    }

	break;
    case DB5_MINORTYPE_BRLCAD_NMG:
	/*XXX Needs work */
#if 0
	/* New way only */
    {
#ifndef NO_MAGIC_CHECKING
	register struct model *m =
	    (struct model *) ip->idb_ptr;
	NMG_CK_MODEL(m);
#endif

	if (es_eu)  {
	    point_t	cent;
	    NMG_CK_EDGEUSE(es_eu);
	    VADD2SCALE(cent,
		       es_eu->vu_p->v_p->vg_p->coord,
		       es_eu->eumate_p->vu_p->v_p->vg_p->coord,
		       0.5);
	    MAT4X3PNT(pos_view, xform, cent);
	    POINT_LABEL_STR(pos_view, " eu");
	}
    }
#endif

	break;
    case DB5_MINORTYPE_BRLCAD_EBM:
	break;
    case DB5_MINORTYPE_BRLCAD_VOL:
	break;
    case DB5_MINORTYPE_BRLCAD_ARBN:
	break;
    case DB5_MINORTYPE_BRLCAD_PIPE:
	/*XXX Needs work */
#if 0
    {
#ifndef NO_MAGIC_CHECKING
	register struct rt_pipe_internal *pipe =
	    (struct rt_pipe_internal *)ip->idb_ptr;

	RT_PIPE_CK_MAGIC(pipe);
#endif

	if (es_pipept) {
	    BU_CKMAG(es_pipept, WDB_PIPESEG_MAGIC, "wdb_pipept");

	    MAT4X3PNT(pos_view, xform, es_pipept->pp_coord);
	    POINT_LABEL_STR(pos_view, "pt");
	}
    }
#endif

	break;
    case DB5_MINORTYPE_BRLCAD_PARTICLE: {
	struct rt_part_internal	*part =
	    (struct rt_part_internal *)ip->idb_ptr;
	vect_t	Ru, ortho;

	RT_PART_CK_MAGIC(part);

	MAT4X3PNT(pos_view, xform, part->part_V);
	POINT_LABEL(pos_view, 'V');

	VADD2(work, part->part_V, part->part_H);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'H');

	VMOVE(Ru, part->part_H);
	VUNITIZE(Ru);
	bn_vec_ortho(ortho, Ru);
	VSCALE(work, ortho, part->part_vrad);
	VADD2(work, part->part_V, work);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'v');

	VSCALE(work, ortho, part->part_hrad);
	VADD3(work, part->part_V, part->part_H, work);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'h');
    }

	break;
    case DB5_MINORTYPE_BRLCAD_RPC: {
	struct rt_rpc_internal	*rpc =
	    (struct rt_rpc_internal *)ip->idb_ptr;
	vect_t	Ru;

	RT_RPC_CK_MAGIC(rpc);

	MAT4X3PNT(pos_view, xform, rpc->rpc_V);
	POINT_LABEL(pos_view, 'V');

	VADD2(work, rpc->rpc_V, rpc->rpc_B);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'B');

	VADD2(work, rpc->rpc_V, rpc->rpc_H);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'H');

	VCROSS(Ru, rpc->rpc_B, rpc->rpc_H);
	VUNITIZE(Ru);
	VSCALE(Ru, Ru, rpc->rpc_r);
	VADD2(work, rpc->rpc_V, Ru);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'r');
    }

	break;
    case DB5_MINORTYPE_BRLCAD_RHC: {
	struct rt_rhc_internal	*rhc =
	    (struct rt_rhc_internal *)ip->idb_ptr;
	vect_t	Ru;

	RT_RHC_CK_MAGIC(rhc);

	MAT4X3PNT(pos_view, xform, rhc->rhc_V);
	POINT_LABEL(pos_view, 'V' );

	VADD2(work, rhc->rhc_V, rhc->rhc_B);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'B');

	VADD2(work, rhc->rhc_V, rhc->rhc_H);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'H');

	VCROSS(Ru, rhc->rhc_B, rhc->rhc_H);
	VUNITIZE(Ru);
	VSCALE(Ru, Ru, rhc->rhc_r);
	VADD2(work, rhc->rhc_V, Ru);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'r');

	VMOVE(work, rhc->rhc_B);
	VUNITIZE(work);
	VSCALE(work, work,
	       MAGNITUDE(rhc->rhc_B) + rhc->rhc_c);
	VADD2(work, work, rhc->rhc_V);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'c');
    }

	break;
    case DB5_MINORTYPE_BRLCAD_EPA: {
	struct rt_epa_internal	*epa =
	    (struct rt_epa_internal *)ip->idb_ptr;
	vect_t	A, B;

	RT_EPA_CK_MAGIC(epa);

	MAT4X3PNT(pos_view, xform, epa->epa_V);
	POINT_LABEL(pos_view, 'V');

	VADD2(work, epa->epa_V, epa->epa_H);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'H');

	VSCALE(A, epa->epa_Au, epa->epa_r1);
	VADD2(work, epa->epa_V, A);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'A');

	VCROSS(B, epa->epa_Au, epa->epa_H);
	VUNITIZE(B);
	VSCALE(B, B, epa->epa_r2);
	VADD2(work, epa->epa_V, B);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'B');
    }

	break;
    case DB5_MINORTYPE_BRLCAD_EHY: {
	struct rt_ehy_internal	*ehy =
	    (struct rt_ehy_internal *)ip->idb_ptr;
	vect_t	A, B;

	RT_EHY_CK_MAGIC(ehy);

	MAT4X3PNT(pos_view, xform, ehy->ehy_V);
	POINT_LABEL(pos_view, 'V');

	VADD2(work, ehy->ehy_V, ehy->ehy_H);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'H');

	VSCALE(A, ehy->ehy_Au, ehy->ehy_r1);
	VADD2(work, ehy->ehy_V, A);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'A');

	VCROSS(B, ehy->ehy_Au, ehy->ehy_H);
	VUNITIZE(B);
	VSCALE(B, B, ehy->ehy_r2);
	VADD2(work, ehy->ehy_V, B);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'B');

	VMOVE(work, ehy->ehy_H);
	VUNITIZE(work);
	VSCALE(work, work,
	       MAGNITUDE(ehy->ehy_H) + ehy->ehy_c);
	VADD2(work, ehy->ehy_V, work);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'c');
    }

	break;
    case DB5_MINORTYPE_BRLCAD_ETO: {
	struct rt_eto_internal	*eto =
	    (struct rt_eto_internal *)ip->idb_ptr;
	fastf_t	ch, cv, dh, dv, cmag, phi;
	vect_t	Au, Nu;

	RT_ETO_CK_MAGIC(eto);

	MAT4X3PNT(pos_view, xform, eto->eto_V);
	POINT_LABEL(pos_view, 'V');

	VMOVE(Nu, eto->eto_N);
	VUNITIZE(Nu);
	bn_vec_ortho(Au, Nu);
	VUNITIZE(Au);

	cmag = MAGNITUDE(eto->eto_C);
	/* get horizontal and vertical components of C and Rd */
	cv = VDOT(eto->eto_C, Nu);
	ch = sqrt(cmag*cmag - cv*cv);
	/* angle between C and Nu */
	phi = acos(cv / cmag);
	dv = -eto->eto_rd * sin(phi);
	dh = eto->eto_rd * cos(phi);

	VJOIN2(work, eto->eto_V, eto->eto_r+ch, Au, cv, Nu);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'C');

	VJOIN2(work, eto->eto_V, eto->eto_r+dh, Au, dv, Nu);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'D');

	VJOIN1(work, eto->eto_V, eto->eto_r, Au);
	MAT4X3PNT(pos_view, xform, work);
	POINT_LABEL(pos_view, 'r');
    }

	break;
    case DB5_MINORTYPE_BRLCAD_GRIP:
	break;
    case DB5_MINORTYPE_BRLCAD_JOINT:
	break;
    case DB5_MINORTYPE_BRLCAD_HF:
	break;
    case DB5_MINORTYPE_BRLCAD_DSP:
	break;
    case DB5_MINORTYPE_BRLCAD_SKETCH:
	break;
    case DB5_MINORTYPE_BRLCAD_EXTRUDE:
	break;
    case DB5_MINORTYPE_BRLCAD_SUBMODEL:
	break;
    case DB5_MINORTYPE_BRLCAD_CLINE: {
	register struct rt_cline_internal *cli =
	    (struct rt_cline_internal *)ip->idb_ptr;
	point_t work1;

	RT_CLINE_CK_MAGIC(cli);

	MAT4X3PNT(pos_view, xform, cli->v);
	POINT_LABEL(pos_view, 'V');

	VADD2(work1, cli->v, cli->h);
	MAT4X3PNT(pos_view, xform, work1);
	POINT_LABEL(pos_view, 'H');
    }

	break;
    case DB5_MINORTYPE_BRLCAD_BOT:
	/*XXX Needs work */
#if 0
    {
	register struct rt_bot_internal *bot =
	    (struct rt_bot_internal *)ip->idb_ptr;

	RT_BOT_CK_MAGIC( bot );

	if (bot_verts[2] > -1 &&
	    bot_verts[1] > -1 &&
	    bot_verts[0] > -1) {
	    /* editing a face */
	    point_t mid_pt;
	    point_t p1, p2, p3;
	    fastf_t one_third=1.0/3.0;

	    MAT4X3PNT(p1, xform, &bot->vertices[bot_verts[0]*3]);
	    MAT4X3PNT(p2, xform, &bot->vertices[bot_verts[1]*3]);
	    MAT4X3PNT(p3, xform, &bot->vertices[bot_verts[2]*3]);
	    VADD3(mid_pt, p1, p2, p3);

	    VSCALE(mid_pt, mid_pt, one_third);

	    *num_lines = 3;
	    VMOVE(lines[0], mid_pt);
	    VMOVE(lines[1], p1);
	    VMOVE(lines[2], mid_pt);
	    VMOVE(lines[3], p2);
	    VMOVE(lines[4], mid_pt);
	    VMOVE(lines[5], p3);
	} else if (bot_verts[1] > -1 && bot_verts[0] > -1) {
	    /* editing an edge */
	    point_t mid_pt;

	    VBLEND2(mid_pt, 0.5, &bot->vertices[bot_verts[0]*3],
		    0.5, &bot->vertices[bot_verts[1]*3]);

	    MAT4X3PNT(pos_view, xform, mid_pt);
	    POINT_LABEL_STR(pos_view, "edge");
	}

	if (bot_verts[0] > -1) {
	    /* editing something, always label the vertex (this is the keypoint) */
	    MAT4X3PNT(pos_view, xform, &bot->vertices[bot_verts[0]*3]);
	    POINT_LABEL_STR(pos_view, "pt");
	}
    }
#endif
	break;
    case DB5_MINORTYPE_BRLCAD_COMBINATION:
	break;
    default:
	break;
    }

    pl[npl].str[0] = '\0';	/* Mark ending */
}

static int
dmo_drawLabels_cmd(struct dm_obj	*dmop,
		   struct dg_obj	*dgop,
		   Tcl_Interp		*interp,
		   int			*labelColor,
		   char			*name)
{
    struct rt_point_labels pl[8+1];
    point_t lines[2*4];	/* up to 4 lines to draw */
    int num_lines=0;
    struct rt_db_internal intern;
    register struct directory *dp;
    register int i;
    int id;
    struct db_tree_state ts;
    struct db_full_path path;

    if (dmop->dmo_drawLabelsHook != (int (*)())0)
	return dmop->dmo_drawLabelsHook(dmop, dgop, interp,
					labelColor, name,
					dmop->dmo_drawLabelsHookClientData);

    if (dgop->dgo_wdbp == (struct rt_wdb *)NULL ||
	name == (char *)NULL)
	return TCL_ERROR;

    db_full_path_init( &path );
    ts = dgop->dgo_wdbp->wdb_initial_tree_state;     /* struct copy */
    ts.ts_dbip = dgop->dgo_wdbp->dbip;
    ts.ts_resp = &rt_uniresource;
    MAT_IDN(ts.ts_mat);

    if (db_follow_path_for_state(&ts, &path, name, 1))
	return TCL_ERROR;

    dp = DB_FULL_PATH_CUR_DIR(&path);

    id = rt_db_get_internal(&intern, dp, dgop->dgo_wdbp->dbip, ts.ts_mat, &rt_uniresource);
    if (id < 0) {
	Tcl_AppendResult(interp, "rt_db_get_internal(", dp->d_namep,
			 ") failure", (char *)NULL );
    }

    dmo_labelPrimitive(dgop, &num_lines, lines, pl, 8+1, dmop->viewMat, &intern);

    DM_SET_FGCOLOR(dmop->dmo_dmp,
		   (unsigned char)labelColor[0],
		   (unsigned char)labelColor[1],
		   (unsigned char)labelColor[2],
		   1, 1.0);
    for (i=0 ; i<num_lines ; i++)
	DM_DRAW_LINE_2D(dmop->dmo_dmp,
			((int)(lines[i*2][X]*GED_MAX))*INV_GED,
			((int)(lines[i*2][Y]*GED_MAX))*dmop->dmo_dmp->dm_aspect*INV_GED,
			((int)(lines[i*2+1][X]*GED_MAX))*INV_GED,
			((int)(lines[i*2+1][Y]*GED_MAX))*dmop->dmo_dmp->dm_aspect*INV_GED);

    for (i=0; i<8+1; i++) {
	if (pl[i].str[0] == '\0')
	    break;

	DM_DRAW_STRING_2D(dmop->dmo_dmp, pl[i].str,
			  (((int)(pl[i].pt[X]*GED_MAX))+15)*INV_GED,
			  (((int)(pl[i].pt[Y]*GED_MAX))+15)*INV_GED, 0, 1);
    }

    db_free_full_path(&path);
    rt_db_free_internal(&intern, &rt_uniresource);

    return TCL_OK;
}

/*
 * Draw labels for the specified dg_obj's primitive object(s).
 *
 * Usage:
 *	  objname drawLabels dg_obj color primitive(s)
 */
static int
dmo_drawLabels_tcl(ClientData	clientData,
		   Tcl_Interp	*interp,
		   int		argc,
		   char		**argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct dg_obj *dgop;
    struct bu_vls vls;
    register int i;
    int labelColor[3];

    if (argc < 5) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dm_drawLabels %s", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (sscanf(argv[3], "%d %d %d",
	       &labelColor[0],
	       &labelColor[1],
	       &labelColor[2]) != 3) {
	bu_vls_printf(&vls, "drawLabels: bad label color - %s\n", argv[3]);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

	return TCL_ERROR;
    }

    /* validate color */
    if (labelColor[0] < 0 || 255 < labelColor[0] ||
	labelColor[1] < 0 || 255 < labelColor[1] ||
	labelColor[2] < 0 || 255 < labelColor[2]) {

	bu_vls_printf(&vls, "drawLabels: bad label color - %s\n", argv[3]);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

	return TCL_ERROR;
    }

    for (BU_LIST_FOR(dgop, dg_obj, &HeadDGObj.l)) {
	if (strcmp(bu_vls_addr(&dgop->dgo_name), argv[2]) == 0) {
	    /* for each primitive */
	    for (i = 4; i < argc; ++i)
		dmo_drawLabels_cmd(dmop, dgop, interp, labelColor, argv[i]);

	    break;
	}
    }

    return TCL_OK;
}

/*
 * Get/set the display manager's foreground color.
 *
 * Usage:
 *	  objname fg [rgb]
 */
static int
dmo_fg_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;
	int		r, g, b;
	Tcl_Obj		*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	bu_vls_init(&vls);

	/* get foreground color */
	if (argc == 2) {
		bu_vls_printf(&vls, "%d %d %d",
			      dmop->dmo_dmp->dm_fg[0],
			      dmop->dmo_dmp->dm_fg[1],
			      dmop->dmo_dmp->dm_fg[2]);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	/* set foreground color */
	if (argc == 3) {
		if (sscanf(argv[2], "%d %d %d", &r, &g, &b) != 3)
			goto bad_color;

		/* validate color */
		if (r < 0 || 255 < r ||
		    g < 0 || 255 < g ||
		    b < 0 || 255 < b)
			goto bad_color;

		bu_vls_free(&vls);
		return DM_SET_FGCOLOR(dmop->dmo_dmp,
				      (unsigned char)r,
				      (unsigned char)g,
				      (unsigned char)b,
				      1,1.0);
	}

	/* wrong number of arguments */
	bu_vls_printf(&vls, "helplib_alias dm_fg %s", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;

 bad_color:
	bu_vls_printf(&vls, "bad rgb color - %s\n", argv[2]);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(interp, obj);
	return TCL_ERROR;
}

/*
 * Get/set the display manager's background color.
 *
 * Usage:
 *	  objname bg [rgb]
 */
static int
dmo_bg_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;
	int		r, g, b;
	Tcl_Obj		*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	bu_vls_init(&vls);

	/* get background color */
	if (argc == 2) {
		bu_vls_printf(&vls, "%d %d %d",
			      dmop->dmo_dmp->dm_bg[0],
			      dmop->dmo_dmp->dm_bg[1],
			      dmop->dmo_dmp->dm_bg[2]);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	/* set background color */
	if (argc == 3) {
		if (sscanf(argv[2], "%d %d %d", &r, &g, &b) != 3)
			goto bad_color;

		/* validate color */
		if (r < 0 || 255 < r ||
		    g < 0 || 255 < g ||
		    b < 0 || 255 < b)
			goto bad_color;

		bu_vls_free(&vls);
		return DM_SET_BGCOLOR(dmop->dmo_dmp,
				      (unsigned char)r,
				      (unsigned char)g,
				      (unsigned char)b);
	}

	/* wrong number of arguments */
	bu_vls_printf(&vls, "helplib_alias dm_bg %s", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;

 bad_color:
	bu_vls_printf(&vls, "bad rgb color - %s\n", argv[2]);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(interp, obj);
	return TCL_ERROR;
}

/*
 * Get/set the display manager's linewidth.
 *
 * Usage:
 *	  objname linewidth [n]
 */
static int
dmo_lineWidth_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;
	int		lineWidth;
	Tcl_Obj		*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	bu_vls_init(&vls);

	/* get linewidth */
	if (argc == 2) {
		bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_lineWidth);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

 	/* set lineWidth */
	if (argc == 3) {
		if (sscanf(argv[2], "%d", &lineWidth) != 1)
			goto bad_lineWidth;

		/* validate lineWidth */
		if (lineWidth < 0 || 20 < lineWidth)
			goto bad_lineWidth;

		bu_vls_free(&vls);
		return DM_SET_LINE_ATTR(dmop->dmo_dmp, lineWidth, dmop->dmo_dmp->dm_lineStyle);
	}

	/* wrong number of arguments */
	bu_vls_printf(&vls, "helplib_alias dm_linewidth %s", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;

 bad_lineWidth:
	bu_vls_printf(&vls, "bad linewidth - %s\n", argv[2]);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(interp, obj);
	return TCL_ERROR;
}

/*
 * Get/set the display manager's linestyle.
 *
 * Usage:
 *	  objname linestyle [0|1]
 */
static int
dmo_lineStyle_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;
	int		linestyle;
	Tcl_Obj		*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	bu_vls_init(&vls);

	/* get linestyle */
	if (argc == 2) {
		bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_lineStyle);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	/* set linestyle */
	if (argc == 3) {
		if (sscanf(argv[2], "%d", &linestyle) != 1)
			goto bad_linestyle;

		/* validate linestyle */
		if (linestyle < 0 || 1 < linestyle)
			goto bad_linestyle;

		bu_vls_free(&vls);
		return DM_SET_LINE_ATTR(dmop->dmo_dmp, dmop->dmo_dmp->dm_lineWidth, linestyle);
	}

	/* wrong number of arguments */
	bu_vls_printf(&vls, "helplib_alias dm_linestyle %1", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;

 bad_linestyle:
	bu_vls_printf(&vls, "bad linestyle - %s\n", argv[2]);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(interp, obj);
	return TCL_ERROR;
}

/*
 * Configure the display manager window. This is typically
 * called as a result of a ConfigureNotify event.
 *
 * Usage:
 *	  objname configure
 */
static int
dmo_configure_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	int		status;

	if (argc != 2) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib_alias dm_configure %s", argv[1]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* configure the display manager window */
	status = DM_CONFIGURE_WIN(dmop->dmo_dmp);

#ifdef USE_FBSERV
	/* configure the framebuffer window */
	if (dmop->dmo_fbs.fbs_fbp != FBIO_NULL)
		fb_configureWindow(dmop->dmo_fbs.fbs_fbp,
				   dmop->dmo_dmp->dm_width,
				   dmop->dmo_dmp->dm_height);
#endif

	return status;
}

/*
 * Get/set the display manager's zclip flag.
 *
 * Usage:
 *	  objname zclip [0|1]
 */
static int
dmo_zclip_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;
	int		zclip;
	Tcl_Obj		*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	/* get zclip flag */
	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_zclip);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	/* set zclip flag */
	if (argc == 3) {
		if (sscanf(argv[2], "%d", &zclip) != 1) {
			Tcl_AppendStringsToObj(obj, "dmo_zclip: invalid zclip value - ",
					       argv[2], "\n", (char *)NULL);
			Tcl_SetObjResult(interp, obj);
			return TCL_ERROR;
		}

		dmop->dmo_dmp->dm_zclip = zclip;
		return TCL_OK;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dm_zclip %s", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
}

/*
 * Get/set the display manager's zbuffer flag.
 *
 * Usage:
 *	  objname zbuffer [0|1]
 */
static int
dmo_zbuffer_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;
	int		zbuffer;
	Tcl_Obj		*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	/* get zbuffer flag */
	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_zbuffer);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	/* set zbuffer flag */
	if (argc == 3) {
		if (sscanf(argv[2], "%d", &zbuffer) != 1) {
			Tcl_AppendStringsToObj(obj, "dmo_zbuffer: invalid zbuffer value - ",
					 argv[2], "\n", (char *)NULL);
			Tcl_SetObjResult(interp, obj);
			return TCL_ERROR;
		}

		DM_SET_ZBUFFER(dmop->dmo_dmp, zbuffer);
		return TCL_OK;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dm_zbuffer %s", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
}

/*
 * Get/set the display manager's light flag.
 *
 * Usage:
 *	  objname light [0|1]
 */
static int
dmo_light_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;
	int		light;
	Tcl_Obj		*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	/* get light flag */
	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_light);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	/* set light flag */
	if (argc == 3) {
		if (sscanf(argv[2], "%d", &light) != 1) {
			Tcl_AppendStringsToObj(obj, "dmo_light: invalid light value - ",
					 argv[2], "\n", (char *)NULL);

			Tcl_SetObjResult(interp, obj);
			return TCL_ERROR;
		}

		DM_SET_LIGHT(dmop->dmo_dmp, light);
		return TCL_OK;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dm_light %s", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
}

/*
 * Get/set the display manager's transparency flag.
 *
 * Usage:
 *	  objname transparency [0|1]
 */
static int
dmo_transparency_tcl(ClientData	clientData,
		     Tcl_Interp	*interp,
		     int	argc,
		     char	**argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;
	int		transparency;
	Tcl_Obj		*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	/* get transparency flag */
	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_transparency);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	/* set transparency flag */
	if (argc == 3) {
		if (sscanf(argv[2], "%d", &transparency) != 1) {
			Tcl_AppendStringsToObj(obj, "dmo_transparency: invalid transparency value - ",
					 argv[2], "\n", (char *)NULL);

			Tcl_SetObjResult(interp, obj);
			return TCL_ERROR;
		}

		DM_SET_TRANSPARENCY(dmop->dmo_dmp, transparency);
		return TCL_OK;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dm_transparency %s", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
}

/*
 * Get/set the display manager's depth mask flag.
 *
 * Usage:
 *	  objname depthMask [0|1]
 */
static int
dmo_depthMask_tcl(ClientData	clientData,
		  Tcl_Interp	*interp,
		  int	argc,
		  char	**argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;
	int		depthMask;
	Tcl_Obj		*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	/* get depthMask flag */
	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_depthMask);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	/* set depthMask flag */
	if (argc == 3) {
		if (sscanf(argv[2], "%d", &depthMask) != 1) {
			Tcl_AppendStringsToObj(obj, "dmo_depthMask: invalid depthMask value - ",
					 argv[2], "\n", (char *)NULL);

			Tcl_SetObjResult(interp, obj);
			return TCL_ERROR;
		}

		DM_SET_DEPTH_MASK(dmop->dmo_dmp, depthMask);
		return TCL_OK;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dm_depthMask %s", argv[1]);
  	Tcl_Eval(interp, bu_vls_addr(&vls));
  	bu_vls_free(&vls);
  	return TCL_ERROR;
}

/*
 * Get/set the display manager's window bounds.
 *
 * Usage:
 *	  objname bounds ["xmin xmax ymin ymax zmin zmax"]
 */
static int
dmo_bounds_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;
	vect_t		clipmin, clipmax;
	Tcl_Obj		*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	/* get window bounds */
	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%g %g %g %g %g %g",
			      dmop->dmo_dmp->dm_clipmin[X],
			      dmop->dmo_dmp->dm_clipmax[X],
			      dmop->dmo_dmp->dm_clipmin[Y],
			      dmop->dmo_dmp->dm_clipmax[Y],
			      dmop->dmo_dmp->dm_clipmin[Z],
			      dmop->dmo_dmp->dm_clipmax[Z]);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	/* set window bounds */
	if (argc == 3) {
		if (sscanf(argv[2], "%lf %lf %lf %lf %lf %lf",
			   &clipmin[X], &clipmax[X],
			   &clipmin[Y], &clipmax[Y],
			   &clipmin[Z], &clipmax[Z]) != 6) {
			Tcl_AppendStringsToObj(obj, "dmo_bounds: invalid bounds - ",
					 argv[2], "\n", (char *)NULL);

			Tcl_SetObjResult(interp, obj);
			return TCL_ERROR;
		}

		VMOVE(dmop->dmo_dmp->dm_clipmin, clipmin);
		VMOVE(dmop->dmo_dmp->dm_clipmax, clipmax);

		/*
		 * Since dm_bound doesn't appear to be used anywhere,
		 * I'm going to use it for controlling the location
		 * of the zclipping plane in dm-ogl.c. dm-X.c uses
		 * dm_clipmin and dm_clipmax.
		 */
		if (dmop->dmo_dmp->dm_clipmax[2] <= GED_MAX)
		  dmop->dmo_dmp->dm_bound = 1.0;
		else
		  dmop->dmo_dmp->dm_bound = GED_MAX / dmop->dmo_dmp->dm_clipmax[2];

		return TCL_OK;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dm_bounds %s", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}

/*
 * Get/set the display manager's perspective mode.
 *
 * Usage:
 *	  objname perspective [n]
 */
static int
dmo_perspective_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;
	int		perspective;
	Tcl_Obj		*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	/* get perspective mode */
	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_perspective);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	/* set perspective mode */
	if (argc == 3) {
		if (sscanf(argv[2], "%d", &perspective) != 1) {
			Tcl_AppendStringsToObj(obj,
					       "dmo_perspective: invalid perspective mode - ",
					       argv[2], "\n", (char *)NULL);

			Tcl_SetObjResult(interp, obj);
			return TCL_ERROR;
		}

		dmop->dmo_dmp->dm_perspective = perspective;
		return TCL_OK;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dm_perspective %s", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
}


#ifndef _WIN32

#if 1
#define DM_REVERSE_COLOR_BYTE_ORDER(_shift,_mask) { \
        _shift = 24 - _shift; \
	switch (_shift) { \
	case 0: \
	     _mask >>= 24; \
	     break; \
	case 8: \
	     _mask >>= 8; \
	     break; \
	case 16: \
	     _mask <<= 8; \
	     break; \
	case 24: \
	     _mask <<= 24; \
	     break; \
	} \
}
#else
/* Do nothing */
#define DM_REVERSE_COLOR_BYTE_ORDER(_shift,_mask)
#endif

static int
dmo_png_cmd(struct dm_obj	*dmop,
	    Tcl_Interp		*interp,
	    int			argc,
	    char 		**argv)
{
    FILE *fp;
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

    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dm_png %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((fp = fopen(argv[1], "w")) == NULL) {
	Tcl_AppendResult(interp, "png: cannot open \"", argv[1], " for writing\n", (char *)NULL);
	return TCL_ERROR;
    }

    png_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p) {
	Tcl_AppendResult(interp, "png: could not create PNG write structure\n", (char *)NULL);
	fclose(fp);
	return TCL_ERROR;
    }

    info_p = png_create_info_struct(png_p);
    if (!info_p) {
	Tcl_AppendResult(interp, "png: could not create PNG info structure\n", (char *)NULL);
	fclose(fp);
	return TCL_ERROR;
    }

    ximage_p = XGetImage(((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->dpy,
			 ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->win,
			 0, 0,
			 dmop->dmo_dmp->dm_width,
			 dmop->dmo_dmp->dm_height,
			 ~0, ZPixmap);
    if (!ximage_p) {
	Tcl_AppendResult(interp, "png: could not get XImage\n", (char *)NULL);
	fclose(fp);
	return TCL_ERROR;
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
#ifdef NATURAL_IEEE
#  define CHECK_ORDER (ximage_p->byte_order == MSBFirst)
#elif defined(REVERSE_IEEE)
#  define CHECK_ORDER (ximage_p->byte_order == LSBFirst)
#endif
	if (CHECK_ORDER) {
#if 0
	    bu_log("red mask - %ld\n", ximage_p->red_mask);
	    bu_log("green mask - %ld\n", ximage_p->green_mask);
	    bu_log("blue mask - %ld\n", ximage_p->blue_mask);

	    bu_log("red_shift - %ld\n", red_shift);
	    bu_log("green_shift - %ld\n", green_shift);
	    bu_log("blue_shift - %ld\n", blue_shift);
	    bu_log("Reversing the byte order!!!\n");
#endif
	    DM_REVERSE_COLOR_BYTE_ORDER(red_shift,ximage_p->red_mask);
	    DM_REVERSE_COLOR_BYTE_ORDER(green_shift,ximage_p->green_mask);
	    DM_REVERSE_COLOR_BYTE_ORDER(blue_shift,ximage_p->blue_mask);
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
#if 0
	bu_log("red shift - %d\n", red_shift);
#endif
	if (red_shift == 0)
	    red_shift = red_bits - bpb;
	else
	    red_shift = red_shift - (bpb - red_bits);
#if 0
	bu_log("red shift - %d\n", red_shift);
	bu_log("red bits - %d\n", red_bits);
#endif

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
#if 0
	bu_log("green shift- %d\n", green_shift);
#endif
	green_shift = green_shift - (bpb - green_bits);
#if 0
	bu_log("green shift - %d\n", green_shift);
	bu_log("green bits - %d\n", green_bits);
#endif

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

#if 0
	bu_log("blue shift - %d\n", blue_shift);
#endif
	if (blue_shift == 0)
	    blue_shift = blue_bits - bpb;
	else
	    blue_shift = blue_shift - (bpb - blue_bits);
#if 0
	bu_log("blue shift - %d\n", blue_shift);
	bu_log("blue bits - %d\n", blue_bits);
#endif
    } else {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "png: %d bytes per pixel is not yet supported\n", bytes_per_pixel);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	fclose(fp);
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

#if 0
    bu_log("bytes_per_pixel - %ld\n", bytes_per_pixel);
    bu_log("byte_order - %d\n", ximage_p->byte_order);
    bu_log("size of unsigned short - %d\n", sizeof(unsigned short));
    bu_log("size of unsigned int - %d\n", sizeof(unsigned int));
    bu_log("size of unsigned long - %d\n", sizeof(unsigned long));

    bu_log("red mask - %ld\n", ximage_p->red_mask);
    bu_log("green mask - %ld\n", ximage_p->green_mask);
    bu_log("blue mask - %ld\n", ximage_p->blue_mask);

    bu_log("red_shift - %ld\n", red_shift);
    bu_log("green_shift - %ld\n", green_shift);
    bu_log("blue_shift - %ld\n", blue_shift);
#endif

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
	    fclose(fp);

	    Tcl_AppendResult(interp, "png: not supported for this platform\n", (char *)NULL);
	    return TCL_ERROR;
	}
    }

    png_init_io(png_p, fp);
    png_set_filter(png_p, 0, PNG_FILTER_NONE);
    png_set_compression_level(png_p, Z_BEST_COMPRESSION);
    png_set_IHDR(png_p, info_p, ximage_p->width, ximage_p->height, bits_per_channel,
		 PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_gAMA(png_p, info_p, 0.77);
    png_write_info(png_p, info_p);
    png_write_image(png_p, rows);
    png_write_end(png_p, NULL);

    bu_free(rows, "rows");
    bu_free(idata, "image data");
    fclose(fp);

    return TCL_OK;
}

/*
 * Write the pixels to a file in png format.
 *
 * Usage:
 *	  objname png args
 *
 */
static int
dmo_png_tcl(ClientData	clientData,
	    Tcl_Interp	*interp,
	    int		argc,
	    char	**argv)
{
    struct dm_obj	*dmop = (struct dm_obj *)clientData;

    return dmo_png_cmd(dmop, interp, argc-1, argv+1);
}
#endif

/*
 * Get/set the clearBufferAfter flag.
 *
 * Usage:
 *	  objname clearBufferAfter [flag]
 *
 */
static int
dmo_clearBufferAfter_tcl(ClientData	clientData,
			 Tcl_Interp	*interp,
			 int		argc,
			 char           **argv) {
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;
	int		clearBufferAfter;
	Tcl_Obj		*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	/* get clearBufferAfter flag */
	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_clearBufferAfter);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	/* set clearBufferAfter flag */
	if (argc == 3) {
		if (sscanf(argv[2], "%d", &clearBufferAfter) != 1) {
			Tcl_AppendStringsToObj(obj, "dmo_clearBufferAfter: invalid clearBufferAfter value - ",
					       argv[2], "\n", (char *)NULL);
			Tcl_SetObjResult(interp, obj);
			return TCL_ERROR;
		}

		dmop->dmo_dmp->dm_clearBufferAfter = clearBufferAfter;
		return TCL_OK;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dm_clearBufferAfter %s", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
}


/*
 * Get/set the display manager's debug level.
 *
 * Usage:
 *	  objname debug [n]
 */
static int
dmo_debug_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;
	int		level;
	Tcl_Obj		*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	/* get debug level */
	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_debugLevel);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	/* set debug level */
	if (argc == 3) {
		if (sscanf(argv[2], "%d", &level) != 1) {
			Tcl_AppendStringsToObj(obj, "dmo_debug: invalid debug level - ",
					 argv[2], "\n", (char *)NULL);

			Tcl_SetObjResult(interp, obj);
			return TCL_ERROR;
		}

		return DM_DEBUG(dmop->dmo_dmp,level);
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dm_debug %s", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
}

#ifdef USE_FBSERV
/*
 * Open/activate the display managers framebuffer.
 */
static int
dmo_openFb(dmop, interp)
     struct dm_obj	*dmop;
     Tcl_Interp		*interp;
{

	/* already open */
	if (dmop->dmo_fbs.fbs_fbp != FBIO_NULL)
		return TCL_OK;

	/* don't use bu_calloc so we can fail slightly more gradefully */
	if ((dmop->dmo_fbs.fbs_fbp = (FBIO *)calloc(sizeof(FBIO), 1)) == FBIO_NULL) {
		Tcl_Obj	*obj;

		obj = Tcl_GetObjResult(interp);
		if (Tcl_IsShared(obj))
			obj = Tcl_DuplicateObj(obj);

		Tcl_AppendStringsToObj(obj, "openfb: failed to allocate framebuffer memory\n",
				 (char *)NULL);

		Tcl_SetObjResult(interp, obj);
		return TCL_ERROR;
	}

	switch (dmop->dmo_dmp->dm_type) {
#ifdef DM_X
	case DM_TYPE_X:
		*dmop->dmo_fbs.fbs_fbp = X24_interface; /* struct copy */

		dmop->dmo_fbs.fbs_fbp->if_name = bu_malloc((unsigned)strlen("/dev/X") + 1, "if_name");
		(void)strcpy(dmop->dmo_fbs.fbs_fbp->if_name, "/dev/X");

		/* Mark OK by filling in magic number */
		dmop->dmo_fbs.fbs_fbp->if_magic = FB_MAGIC;

		_X24_open_existing(dmop->dmo_fbs.fbs_fbp,
				   ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->dpy,
				   ((struct x_vars *)dmop->dmo_dmp->dm_vars.priv_vars)->pix,
				   ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->win,
				   ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->cmap,
				   ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->vip,
				   dmop->dmo_dmp->dm_width,
				   dmop->dmo_dmp->dm_height,
				   ((struct x_vars *)dmop->dmo_dmp->dm_vars.priv_vars)->gc);
		break;
#endif
#ifdef DM_OGL
	case DM_TYPE_OGL:
		*dmop->dmo_fbs.fbs_fbp = ogl_interface; /* struct copy */

		dmop->dmo_fbs.fbs_fbp->if_name = bu_malloc((unsigned)strlen("/dev/ogl") + 1, "if_name");
		(void)strcpy(dmop->dmo_fbs.fbs_fbp->if_name, "/dev/ogl");

		/* Mark OK by filling in magic number */
		dmop->dmo_fbs.fbs_fbp->if_magic = FB_MAGIC;

		_ogl_open_existing(dmop->dmo_fbs.fbs_fbp,
				   ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->dpy,
				   ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->win,
				   ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->cmap,
				   ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->vip,
				   dmop->dmo_dmp->dm_width,
				   dmop->dmo_dmp->dm_height,
				   ((struct ogl_vars *)dmop->dmo_dmp->dm_vars.priv_vars)->glxc,
				   ((struct ogl_vars *)dmop->dmo_dmp->dm_vars.priv_vars)->mvars.doublebuffer,
				   0);
		break;
#endif
#ifdef DM_WGL
	case DM_TYPE_WGL:
		*dmop->dmo_fbs.fbs_fbp = wgl_interface; /* struct copy */

		dmop->dmo_fbs.fbs_fbp->if_name = bu_malloc((unsigned)strlen("/dev/wgl") + 1, "if_name");
		(void)strcpy(dmop->dmo_fbs.fbs_fbp->if_name, "/dev/wgl");

		/* Mark OK by filling in magic number */
		dmop->dmo_fbs.fbs_fbp->if_magic = FB_MAGIC;

		_wgl_open_existing(dmop->dmo_fbs.fbs_fbp,
				   ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->dpy,
				   ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->win,
				   ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->cmap,
				   ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->vip,
				   dmop->dmo_dmp->dm_width,
				   dmop->dmo_dmp->dm_height,
				   ((struct wgl_vars *)dmop->dmo_dmp->dm_vars.priv_vars)->glxc,
				   ((struct wgl_vars *)dmop->dmo_dmp->dm_vars.priv_vars)->mvars.doublebuffer,
				   0);
		break;
#endif
	}

	return TCL_OK;
}

/*
 * Draw the point.
 *
 * Usage:
 *	  objname drawPoint x y
 *
 */
static int
dmo_closeFb(dmop)
     struct dm_obj *dmop;
{
	if (dmop->dmo_fbs.fbs_fbp == FBIO_NULL)
		return TCL_OK;

	_fb_pgflush(dmop->dmo_fbs.fbs_fbp);

	switch (dmop->dmo_dmp->dm_type) {
#ifdef DM_X
	case DM_TYPE_X:
		X24_close_existing(dmop->dmo_fbs.fbs_fbp);
		break;
#endif
#ifdef DM_OGL
	case DM_TYPE_OGL:
		ogl_close_existing(dmop->dmo_fbs.fbs_fbp);
		break;
#endif
#ifdef DM_WGL
	case DM_TYPE_WGL:
		wgl_close_existing(dmop->dmo_fbs.fbs_fbp);
		break;
#endif
	}

	/* free framebuffer memory */
	if (dmop->dmo_fbs.fbs_fbp->if_pbase != PIXEL_NULL)
		free((void *)dmop->dmo_fbs.fbs_fbp->if_pbase);
	free((void *)dmop->dmo_fbs.fbs_fbp->if_name);
	free((void *)dmop->dmo_fbs.fbs_fbp);
	dmop->dmo_fbs.fbs_fbp = FBIO_NULL;

	return TCL_OK;
}

#if 0
/*
 * Close/de-activate the display managers framebuffer.
 *
 * Usage:
 *	  objname closefb
 *
 */
static int
dmo_closeFb_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;

	return dmo_closeFb(dmop);
}
#endif

/*
 * Set/get the port used to listen for framebuffer clients.
 *
 * Usage:
 *	  objname listen [port]
 *
 * Returns the port number actually used.
 *
 */
static int
dmo_listen_tcl(clientData, interp, argc, argv)
     ClientData	clientData;
     Tcl_Interp	*interp;
     int	argc;
     char	**argv;
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;
	Tcl_Obj		*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	bu_vls_init(&vls);

	if (dmop->dmo_fbs.fbs_fbp == FBIO_NULL) {
		bu_vls_printf(&vls, "%s listen: framebuffer not open!\n", argv[0]);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_ERROR;
	}

	/* return the port number */
	if (argc == 2) {
		bu_vls_printf(&vls, "%d", dmop->dmo_fbs.fbs_listener.fbsl_port);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	if (argc == 3) {
		int port;

		if (sscanf(argv[2], "%d", &port) != 1) {
			Tcl_AppendStringsToObj(obj, "listen: bad value - ", argv[2], "\n", (char *)NULL);
			Tcl_SetObjResult(interp, obj);
			return TCL_ERROR;
		}

		if (port >= 0)
			fbs_open(interp, &dmop->dmo_fbs, port);
		else {
			fbs_close(interp, &dmop->dmo_fbs);
		}
		bu_vls_printf(&vls, "%d", dmop->dmo_fbs.fbs_listener.fbsl_port);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	bu_vls_printf(&vls, "helplib_alias dm_listen %s", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}

/*
 * Refresh the display managers framebuffer.
 *
 * Usage:
 *	  objname refresh
 *
 */
static int
dmo_refreshFb_tcl(clientData, interp, argc, argv)
     ClientData	clientData;
     Tcl_Interp	*interp;
     int	argc;
     char	**argv;
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;

	if (dmop->dmo_fbs.fbs_fbp == FBIO_NULL) {
		Tcl_Obj	*obj;

		obj = Tcl_GetObjResult(interp);
		if (Tcl_IsShared(obj))
			obj = Tcl_DuplicateObj(obj);

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%s refresh: framebuffer not open!\n", argv[0]);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_ERROR;
	}

	fb_refresh(dmop->dmo_fbs.fbs_fbp, 0, 0,
		   dmop->dmo_dmp->dm_width, dmop->dmo_dmp->dm_height);

	return TCL_OK;
}
#endif

/*
 * Flush the output buffer.
 *
 * Usage:
 *	  objname flush
 *
 */
static int
dmo_flush_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
#ifdef DM_X
	struct dm_obj *dmop = (struct dm_obj *)clientData;

	XFlush(((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->dpy);
#endif

	return TCL_OK;
}

/*
 * Flush the output buffer and process all events.
 *
 * Usage:
 *	  objname sync
 *
 */
static int
dmo_sync_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
#ifdef DM_X
	struct dm_obj	*dmop = (struct dm_obj *)clientData;

	XSync(((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->dpy, 0);
#endif

	return TCL_OK;
}

/*
 * Set/get window size.
 *
 * Usage:
 *	  objname size [width [height]]
 *
 */
static int
dmo_size_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;
	Tcl_Obj		*obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%d %d", dmop->dmo_dmp->dm_width, dmop->dmo_dmp->dm_height);
		Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		Tcl_SetObjResult(interp, obj);
		return TCL_OK;
	}

	if (argc == 3 || argc == 4) {
		int width, height;

		if (sscanf(argv[2], "%d", &width) != 1) {
			Tcl_AppendStringsToObj(obj, "size: bad width - ", argv[2], "\n", (char *)NULL);

			Tcl_SetObjResult(interp, obj);
			return TCL_ERROR;
		}

		if (argc == 3)
			height = width;
		else {
			if (sscanf(argv[3], "%d", &height) != 1) {
				Tcl_AppendStringsToObj(obj, "size: bad height - ", argv[3], "\n", (char *)NULL);
				Tcl_SetObjResult(interp, obj);
				return TCL_ERROR;
			}
		}

		Tk_GeometryRequest(((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->xtkwin,
				   width, height);
		return TCL_OK;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dm_size %s", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
}

/*
 * Get window aspect ratio (i.e. width / height)
 *
 * Usage:
 *	  objname get_aspect
 *
 */
static int
dmo_get_aspect_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj	*dmop = (struct dm_obj *)clientData;
	struct bu_vls	vls;
	Tcl_Obj		*obj;

	if (argc != 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib_alias dm_getaspect %s", argv[1]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%g", dmop->dmo_dmp->dm_aspect);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(interp, obj);
	return TCL_OK;
}

/*
 * Attach/detach observers to/from list.
 *
 * Usage:
 *	  objname observer cmd [args]
 *
 */
static int
dmo_observer_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;

	if (argc < 3) {
		struct bu_vls vls;

		/* return help message */
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib_alias dm_observer %s", argv[1]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	return bu_cmd((ClientData)&dmop->dmo_observers,
		      interp, argc - 2, argv + 2, bu_observer_cmds, 0);
}

#ifdef USE_FBSERV
static void
dmo_fbs_callback(clientData)
     genptr_t clientData;
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;

	bu_observer_notify(dmop->dmo_dmp->dm_interp, &dmop->dmo_observers,
			   bu_vls_addr(&dmop->dmo_name));
}
#endif

static int
dmo_getDrawLabelsHook_cmd(struct dm_obj	*dmop,
			  Tcl_Interp	*interp,
			  int		argc,
			  char 		**argv)
{
    char buf[64];
    Tcl_DString ds;

    if (argc != 1) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dm_getDrawLabelsHook %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    sprintf(buf, "%lu %lu",
	    (unsigned long)dmop->dmo_drawLabelsHook,
	    (unsigned long)dmop->dmo_drawLabelsHookClientData);
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, buf, -1);
    Tcl_DStringResult(interp, &ds);

    return TCL_OK;
}

static int
dmo_getDrawLabelsHook_tcl(ClientData clientData,
			  Tcl_Interp *interp,
			  int	     argc,
			  char	     **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    return dmo_getDrawLabelsHook_cmd(dmop, interp, argc-1, argv+1);
}

static int
dmo_setDrawLabelsHook_cmd(struct dm_obj	*dmop,
			  Tcl_Interp	*interp,
			  int		argc,
			  char 		**argv)
{
    unsigned long hook;
    unsigned long clientData;

    if (argc != 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias dm_setDrawLabelsHook %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (sscanf(argv[1], "%lu", (unsigned long *)&hook) != 1) {
	Tcl_DString ds;

	Tcl_DStringInit(&ds);
	Tcl_DStringAppend(&ds, argv[0], -1);
	Tcl_DStringAppend(&ds, ": failed to set the drawLabels hook", -1);
	Tcl_DStringResult(interp, &ds);

	dmop->dmo_drawLabelsHook = (int (*)())0;

	return TCL_ERROR;
    }

    if (sscanf(argv[2], "%lu", (unsigned long *)&clientData) != 1) {
	Tcl_DString ds;

	Tcl_DStringInit(&ds);
	Tcl_DStringAppend(&ds, argv[0], -1);
	Tcl_DStringAppend(&ds, ": failed to set the drawLabels hook", -1);
	Tcl_DStringResult(interp, &ds);

	dmop->dmo_drawLabelsHook = (int (*)())0;

	return TCL_ERROR;
    }

    dmop->dmo_drawLabelsHook = (int (*)())hook;
    dmop->dmo_drawLabelsHookClientData = (void *)clientData;

    return TCL_OK;
}

static int
dmo_setDrawLabelsHook_tcl(ClientData clientData,
			  Tcl_Interp *interp,
			  int	     argc,
			  char	     **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    return dmo_setDrawLabelsHook_cmd(dmop, interp, argc-1, argv+1);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

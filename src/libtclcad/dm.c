/*                            D M . C
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
/**
 *
 * Tcl logic specific to libdm.
 *
 */

#include "common.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "tcl.h"
#include "vmath.h"
#include "dm.h"
#include "bu/cmd.h"
#include "bu/log.h"
#include "bu/vls.h"
#include "tclcad.h"
/* private headers */
#include "../libdm/include/private.h"
#include "brlcad_version.h"
#include "./tclcad_private.h"

/**
 *@brief
 * A display manager object is used for interacting with a display manager.
 */
struct dm_obj {
    struct bu_list l;
    struct bu_vls dmo_name;		/**< @brief display manager object name/cmd */
    struct dm *dmo_dmp;		/**< @brief display manager pointer */
#ifdef USE_FBSERV
    struct fbserv_obj dmo_fbs;		/**< @brief fbserv object */
#endif
    struct bu_observer_list dmo_observers;		/**< @brief fbserv observers */
    mat_t viewMat;
    int (*dmo_drawLabelsHook)(struct dm *, struct rt_wdb *, const char *, mat_t, int *, ClientData);
    void *dmo_drawLabelsHookClientData;
    Tcl_Interp *interp;
};


static struct dm_obj HeadDMObj;	/* head of display manager object list */


#ifdef USE_FBSERV
/*
 * Open/activate the display managers framebuffer.
 */
HIDDEN int
dmo_openFb(struct dm_obj *dmop)
{
    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    /* already open */
    if (dmop->dmo_fbs.fbs_fbp != FB_NULL)
	return BRLCAD_OK;

    dmop->dmo_fbs.fbs_fbp = dm_get_fb(dmop->dmo_dmp);

    if (dmop->dmo_fbs.fbs_fbp == FB_NULL) {
	Tcl_Obj *obj;

	obj = Tcl_GetObjResult(dmop->interp);
	if (Tcl_IsShared(obj))
	    obj = Tcl_DuplicateObj(obj);

	Tcl_AppendStringsToObj(obj, "openfb: failed to allocate framebuffer memory\n", (char *)NULL);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


/*
 * Draw the point.
 *
 * Usage:
 * objname drawPoint x y
 *
 */
HIDDEN int
dmo_closeFb(struct dm_obj *dmop)
{
    if (dmop->dmo_fbs.fbs_fbp == FB_NULL)
	return BRLCAD_OK;

    fb_flush(dmop->dmo_fbs.fbs_fbp);
    fb_close_existing(dmop->dmo_fbs.fbs_fbp);

    dmop->dmo_fbs.fbs_fbp = FB_NULL;

    return BRLCAD_OK;
}


/*
 * Set/get the port used to listen for framebuffer clients.
 *
 * Usage:
 * objname listen [port]
 *
 * Returns the port number actually used.
 *
 */
HIDDEN int
dmo_listen_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    Tcl_Obj *obj;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    if (dmop->dmo_fbs.fbs_fbp == FB_NULL) {
	bu_vls_printf(&vls, "%s listen: framebuffer not open!\n", argv[0]);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_ERROR;
    }

    /* return the port number */
    if (argc == 2) {
	bu_vls_printf(&vls, "%d", dmop->dmo_fbs.fbs_listener.fbsl_port);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
    }

    if (argc == 3) {
	int port;

	if (sscanf(argv[2], "%d", &port) != 1) {
	    Tcl_AppendStringsToObj(obj, "listen: bad value - ", argv[2], "\n", (char *)NULL);
	    Tcl_SetObjResult(dmop->interp, obj);
	    return BRLCAD_ERROR;
	}

	if (port >= 0)
	    fbs_open(&dmop->dmo_fbs, port);
	else {
	    fbs_close(&dmop->dmo_fbs);
	}
	bu_vls_printf(&vls, "%d", dmop->dmo_fbs.fbs_listener.fbsl_port);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
    }

    bu_vls_printf(&vls, "helplib_alias dm_listen %s", argv[1]);
    Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return BRLCAD_ERROR;
}


/*
 * Refresh the display managers framebuffer.
 *
 * Usage:
 * objname refresh
 *
 */
HIDDEN int
dmo_refreshFb_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (!dmop || !dmop->interp || argc < 1 ||  !argv)
	return BRLCAD_ERROR;

    if (dmop->dmo_fbs.fbs_fbp == FB_NULL) {
	Tcl_Obj *obj;

	obj = Tcl_GetObjResult(dmop->interp);
	if (Tcl_IsShared(obj))
	    obj = Tcl_DuplicateObj(obj);

	bu_vls_printf(&vls, "%s refresh: framebuffer not open!\n", argv[0]);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_ERROR;
    }

    fb_refresh(dmop->dmo_fbs.fbs_fbp, 0, 0,
	       dmop->dmo_dmp->i->dm_width, dmop->dmo_dmp->i->dm_height);

    return BRLCAD_OK;
}
#endif


HIDDEN int
dmo_parseAxesArgs(int argc,
		  const char **argv,
		  fastf_t *viewSize,
		  mat_t rmat,
		  point_t axesPos,
		  fastf_t *axesSize,
		  int *axesColor,
		  int *labelColor,
		  int *lineWidth,
		  int *posOnly,
		  int *tripleColor,
		  struct bu_vls *vlsp)
{
    double scan;

    if (argc < 3 || sscanf(argv[2], "%lf", &scan) != 1) {
	bu_vls_printf(vlsp, "parseAxesArgs: bad view size - %s\n", argv[2]);
	return BRLCAD_ERROR;
    }
    /* convert double to fastf_t */
    *viewSize = scan;

    if (argc < 4 || bn_decode_mat(rmat, argv[3]) != 16) {
	bu_vls_printf(vlsp, "parseAxesArgs: bad rmat - %s\n", argv[3]);
	return BRLCAD_ERROR;
    }

    if (argc < 5 || bn_decode_vect(axesPos, argv[4]) != 3) {
	bu_vls_printf(vlsp, "parseAxesArgs: bad axes position - %s\n", argv[4]);
	return BRLCAD_ERROR;
    }

    if (argc < 6 || sscanf(argv[5], "%lf", &scan) != 1) {
	bu_vls_printf(vlsp, "parseAxesArgs: bad axes size - %s\n", argv[5]);
	return BRLCAD_ERROR;
    }
    /* convert double to fastf_t */
    *axesSize = scan;

    if (argc < 7 || sscanf(argv[6], "%d %d %d",
			   &axesColor[0],
			   &axesColor[1],
			   &axesColor[2]) != 3) {

	bu_vls_printf(vlsp, "parseAxesArgs: bad axes color - %s\n", argv[6]);
	return BRLCAD_ERROR;
    }

    /* validate color */
    if (axesColor[0] < 0 || 255 < axesColor[0] ||
	axesColor[1] < 0 || 255 < axesColor[1] ||
	axesColor[2] < 0 || 255 < axesColor[2]) {

	bu_vls_printf(vlsp, "parseAxesArgs: bad axes color - %s\n", argv[6]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[7], "%d %d %d",
	       &labelColor[0],
	       &labelColor[1],
	       &labelColor[2]) != 3) {

	bu_vls_printf(vlsp, "parseAxesArgs: bad label color - %s\n", argv[7]);
	return BRLCAD_ERROR;
    }

    /* validate color */
    if (labelColor[0] < 0 || 255 < labelColor[0] ||
	labelColor[1] < 0 || 255 < labelColor[1] ||
	labelColor[2] < 0 || 255 < labelColor[2]) {

	bu_vls_printf(vlsp, "parseAxesArgs: bad label color - %s\n", argv[7]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[8], "%d", lineWidth) != 1) {
	bu_vls_printf(vlsp, "parseAxesArgs: bad line width - %s\n", argv[8]);
	return BRLCAD_ERROR;
    }

    /* validate lineWidth */
    if (*lineWidth < 0) {
	bu_vls_printf(vlsp, "parseAxesArgs: line width must be greater than 0\n");
	return BRLCAD_ERROR;
    }

    /* parse positive only flag */
    if (sscanf(argv[9], "%d", posOnly) != 1) {
	bu_vls_printf(vlsp, "parseAxesArgs: bad positive only flag - %s\n", argv[9]);
	return BRLCAD_ERROR;
    }

    /* validate tick enable flag */
    if (*posOnly < 0) {
	bu_vls_printf(vlsp, "parseAxesArgs: positive only flag must be >= 0\n");
	return BRLCAD_ERROR;
    }

    /* parse three color flag */
    if (sscanf(argv[10], "%d", tripleColor) != 1) {
	bu_vls_printf(vlsp, "parseAxesArgs: bad three color flag - %s\n", argv[10]);
	return BRLCAD_ERROR;
    }

    /* validate tick enable flag */
    if (*tripleColor < 0) {
	bu_vls_printf(vlsp, "parseAxesArgs: three color flag must be >= 0\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


/*
 * Draw the view axes.
 *
 * Usage:
 * objname drawViewAxes args
 *
 */
HIDDEN int
dmo_drawViewAxes_tcl(void *clientData, int argc, const char **argv)
{
    point_t axesPos;
    fastf_t viewSize;
    mat_t rmat;
    fastf_t axesSize;
    int axesColor[3];
    int labelColor[3];
    int lineWidth;
    int posOnly;
    int tripleColor;
    struct bv_axes bnas;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    if (argc != 11) {
	/* return help message */
	bu_vls_printf(&vls, "helplib_alias dm_drawViewAxes %s", argv[1]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    memset(&bnas, 0, sizeof(struct bv_axes));

    if (dmo_parseAxesArgs(argc, argv, &viewSize, rmat, axesPos, &axesSize,
			  axesColor, labelColor, &lineWidth,
			  &posOnly, &tripleColor, &vls) == BRLCAD_ERROR) {
	Tcl_AppendResult(dmop->interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    VMOVE(bnas.axes_pos, axesPos);
    bnas.axes_size = axesSize;
    VMOVE(bnas.axes_color, axesColor);
    VMOVE(bnas.label_color, labelColor);
    bnas.line_width = lineWidth;
    bnas.pos_only = posOnly;
    bnas.triple_color = tripleColor;

    dm_draw_hud_axes(dmop->dmo_dmp, viewSize, rmat, &bnas);

    bu_vls_free(&vls);
    return BRLCAD_OK;
}


/*
 * Draw the center dot.
 *
 * Usage:
 * drawCenterDot color
 *
 */
HIDDEN int
dmo_drawCenterDot_cmd(struct dm_obj *dmop,
		      int argc,
		      const char **argv)
{
    int color[3];

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    if (argc != 2) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helplib_alias dm_drawCenterDot %s", argv[1]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[1], "%d %d %d",
	       &color[0],
	       &color[1],
	       &color[2]) != 3) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "drawCenterDot: bad color - %s\n", argv[1]);
	Tcl_AppendResult(dmop->interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return BRLCAD_ERROR;
    }

    /* validate color */
    if (color[0] < 0 || 255 < color[0] ||
	color[1] < 0 || 255 < color[1] ||
	color[2] < 0 || 255 < color[2]) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "drawCenterDot: bad color - %s\n", argv[1]);
	Tcl_AppendResult(dmop->interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return BRLCAD_ERROR;
    }

    dm_set_fg(dmop->dmo_dmp,
		   (unsigned char)color[0],
		   (unsigned char)color[1],
		   (unsigned char)color[2], 1, 1.0);

    dm_draw_point_2d(dmop->dmo_dmp, 0.0, 0.0);

    return BRLCAD_OK;
}


/*
 * Draw the center dot.
 *
 * Usage:
 * objname drawCenterDot color
 *
 */
HIDDEN int
dmo_drawCenterDot_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    return dmo_drawCenterDot_cmd(dmop, argc-1, argv+1);
}


HIDDEN int
dmo_parseDataAxesArgs(int argc,
		      const char **argv,
		      fastf_t *viewSize,
		      mat_t rmat,
		      mat_t model2view,
		      point_t axesPos,
		      fastf_t *axesSize,
		      int *axesColor,
		      int *lineWidth,
		      struct bu_vls *vlsp)
{
    double scan;

    if (argc < 3 || sscanf(argv[2], "%lf", &scan) != 1) {
	bu_vls_printf(vlsp, "parseDataAxesArgs: bad view size - %s\n", argv[2]);
	return BRLCAD_ERROR;
    }
    /* convert double to fastf_t */
    *viewSize = scan;

    if (argc < 4 || bn_decode_mat(rmat, argv[3]) != 16) {
	bu_vls_printf(vlsp, "parseDataAxesArgs: bad rmat - %s\n", argv[3]);
	return BRLCAD_ERROR;
    }

    /* parse model to view matrix */
    if (argc < 5 || bn_decode_mat(model2view, argv[4]) != 16) {
	bu_vls_printf(vlsp, "parseDataAxesArgs: bad model2view - %s\n", argv[4]);
	return BRLCAD_ERROR;
    }

    if (argc < 6 || bn_decode_vect(axesPos, argv[5]) != 3) {
	bu_vls_printf(vlsp, "parseDataAxesArgs: bad axes position - %s\n", argv[5]);
	return BRLCAD_ERROR;
    }

    if (argc < 7 || sscanf(argv[6], "%lf", &scan) != 1) {
	bu_vls_printf(vlsp, "parseDataAxesArgs: bad axes size - %s\n", argv[6]);
	return BRLCAD_ERROR;
    }
    /* convert double to fastf_t */
    *axesSize = scan;

    if (argc < 8 || sscanf(argv[7], "%d %d %d",
			   &axesColor[0],
			   &axesColor[1],
			   &axesColor[2]) != 3) {
	bu_vls_printf(vlsp, "parseDataAxesArgs: bad axes color - %s\n", argv[7]);
	return BRLCAD_ERROR;
    }

    /* validate color */
    if (axesColor[0] < 0 || 255 < axesColor[0] ||
	axesColor[1] < 0 || 255 < axesColor[1] ||
	axesColor[2] < 0 || 255 < axesColor[2]) {

	bu_vls_printf(vlsp, "parseDataAxesArgs: bad axes color - %s\n", argv[7]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[8], "%d", lineWidth) != 1) {
	bu_vls_printf(vlsp, "parseDataAxesArgs: bad line width - %s\n", argv[8]);
	return BRLCAD_ERROR;
    }

    /* validate lineWidth */
    if (*lineWidth < 0) {
	bu_vls_printf(vlsp, "parseDataAxesArgs: line width must be greater than 0\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


/*
 * Draw the data axes.
 *
 * Usage:
 * objname drawDataAxes args
 *
 *XXX This needs to be modified to handle an array/list of data points
 */
HIDDEN int
dmo_drawDataAxes_tcl(void *clientData, int argc, const char **argv)
{
    point_t modelAxesPos;
    fastf_t viewSize;
    mat_t rmat;
    mat_t model2view;
    fastf_t axesSize;
    int axesColor[3];
    int lineWidth;
    struct bv_data_axes_state bndas;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    if (argc != 9) {
	/* return help message */
	bu_vls_printf(&vls, "helplib_alias dm_drawDataAxes %s", argv[1]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    if (dmo_parseDataAxesArgs(argc,
			      argv,
			      &viewSize,
			      rmat,
			      model2view,
			      modelAxesPos,
			      &axesSize,
			      axesColor,
			      &lineWidth,
			      &vls) == BRLCAD_ERROR) {
	Tcl_AppendResult(dmop->interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    point_t mapos = VINIT_ZERO;
    memset(&bndas, 0, sizeof(struct bv_data_axes_state));
    bndas.points = &mapos;
    VMOVE(bndas.points[0], modelAxesPos);
    bndas.size = axesSize;
    VMOVE(bndas.color, axesColor);
    bndas.line_width = lineWidth;

    dm_draw_data_axes(dmop->dmo_dmp, viewSize, &bndas);

    bu_vls_free(&vls);
    return BRLCAD_OK;
}


HIDDEN int
dmo_parseModelAxesArgs(int argc,
		       const char **argv,
		       fastf_t *viewSize,
		       mat_t rmat,
		       point_t axesPos,
		       fastf_t *axesSize,
		       int *axesColor,
		       int *labelColor,
		       int *lineWidth,
		       int *posOnly,
		       int *tripleColor,
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
    double scan;

    if (dmo_parseAxesArgs(argc, argv, viewSize, rmat, axesPos, axesSize,
			  axesColor, labelColor, lineWidth,
			  posOnly, tripleColor, vlsp) == BRLCAD_ERROR)
	return BRLCAD_ERROR;

    /* parse model to view matrix */
    if (bn_decode_mat(model2view, argv[11]) != 16) {
	bu_vls_printf(vlsp, "parseModelAxesArgs: bad model2view - %s\n", argv[11]);
	return BRLCAD_ERROR;
    }

/* parse tick enable flag */
    if (sscanf(argv[12], "%d", tickEnable) != 1) {
	bu_vls_printf(vlsp, "parseModelAxesArgs: bad tick enable flag - %s\n", argv[12]);
	return BRLCAD_ERROR;
    }

/* validate tick enable flag */
    if (*tickEnable < 0) {
	bu_vls_printf(vlsp, "parseModelAxesArgs: tick enable flag must be >= 0\n");
	return BRLCAD_ERROR;
    }

/* parse tick length */
    if (sscanf(argv[13], "%d", tickLength) != 1) {
	bu_vls_printf(vlsp, "parseModelAxesArgs: bad tick length - %s\n", argv[13]);
	return BRLCAD_ERROR;
    }

/* validate tick length */
    if (*tickLength < 1) {
	bu_vls_printf(vlsp, "parseModelAxesArgs: tick length must be >= 1\n");
	return BRLCAD_ERROR;
    }

/* parse major tick length */
    if (sscanf(argv[14], "%d", majorTickLength) != 1) {
	bu_vls_printf(vlsp, "parseModelAxesArgs: bad major tick length - %s\n", argv[14]);
	return BRLCAD_ERROR;
    }

/* validate major tick length */
    if (*majorTickLength < 1) {
	bu_vls_printf(vlsp, "parseModelAxesArgs: major tick length must be >= 1\n");
	return BRLCAD_ERROR;
    }

/* parse tick interval */
    if (sscanf(argv[15], "%lf", &scan) != 1) {
	bu_vls_printf(vlsp, "parseModelAxesArgs: tick interval must be > 0");
	return BRLCAD_ERROR;
    }
    /* convert double to fastf_t */
    *tickInterval = scan;

/* validate tick interval */
    if (*tickInterval <= 0) {
	bu_vls_printf(vlsp, "parseModelAxesArgs: tick interval must be > 0");
	return BRLCAD_ERROR;
    }

/* parse ticks per major */
    if (sscanf(argv[16], "%d", ticksPerMajor) != 1) {
	bu_vls_printf(vlsp, "parseModelAxesArgs: bad ticks per major - %s\n", argv[16]);
	return BRLCAD_ERROR;
    }

/* validate ticks per major */
    if (*ticksPerMajor < 0) {
	bu_vls_printf(vlsp, "parseModelAxesArgs: ticks per major must be >= 0\n");
	return BRLCAD_ERROR;
    }

/* parse tick color */
    if (sscanf(argv[17], "%d %d %d",
	       &tickColor[0],
	       &tickColor[1],
	       &tickColor[2]) != 3) {

	bu_vls_printf(vlsp, "parseModelAxesArgs: bad tick color - %s\n", argv[17]);
	return BRLCAD_ERROR;
    }

/* validate tick color */
    if (tickColor[0] < 0 || 255 < tickColor[0] ||
	tickColor[1] < 0 || 255 < tickColor[1] ||
	tickColor[2] < 0 || 255 < tickColor[2]) {

	bu_vls_printf(vlsp, "parseModelAxesArgs: bad tick color - %s\n", argv[17]);
	return BRLCAD_ERROR;
    }

/* parse major tick color */
    if (sscanf(argv[18], "%d %d %d",
	       &majorTickColor[0],
	       &majorTickColor[1],
	       &majorTickColor[2]) != 3) {

	bu_vls_printf(vlsp, "parseModelAxesArgs: bad major tick color - %s\n", argv[18]);
	return BRLCAD_ERROR;
    }

/* validate tick color */
    if (majorTickColor[0] < 0 || 255 < majorTickColor[0] ||
	majorTickColor[1] < 0 || 255 < majorTickColor[1] ||
	majorTickColor[2] < 0 || 255 < majorTickColor[2]) {

	bu_vls_printf(vlsp, "parseModelAxesArgs: bad major tick color - %s\n", argv[18]);
	return BRLCAD_ERROR;
    }

/* parse tick threshold */
    if (sscanf(argv[19], "%d", tickThreshold) != 1) {
	bu_vls_printf(vlsp, "parseModelAxesArgs: bad tick threshold - %s\n", argv[19]);
	return BRLCAD_ERROR;
    }

/* validate tick threshold */
    if (*tickThreshold <= 0) {
	bu_vls_printf(vlsp, "parseModelAxesArgs: tick threshold must be > 0\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


/*
 * Draw the model axes.
 *
 * Usage:
 * objname drawModelAxes args
 *
 */
HIDDEN int
dmo_drawModelAxes_tcl(void *clientData, int argc, const char **argv)
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
    int tripleColor;
    int tickEnable;
    int tickLength;
    int majorTickLength;
    fastf_t tickInterval;
    int ticksPerMajor;
    int tickColor[3];
    int majorTickColor[3];
    int tickThreshold;
    struct bv_axes bnas;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    if (argc != 20) {
	/* return help message */
	bu_vls_printf(&vls, "helplib_alias dm_drawModelAxes %s", argv[1]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    if (dmo_parseModelAxesArgs(argc, argv,
			       &viewSize, rmat, modelAxesPos,
			       &axesSize, axesColor,
			       labelColor, &lineWidth,
			       &posOnly, &tripleColor,
			       model2view, &tickEnable,
			       &tickLength, &majorTickLength,
			       &tickInterval, &ticksPerMajor,
			       tickColor, majorTickColor,
			       &tickThreshold, &vls) == BRLCAD_ERROR) {
	Tcl_AppendResult(dmop->interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    MAT4X3PNT(viewAxesPos, model2view, modelAxesPos);

    memset(&bnas, 0, sizeof(struct bv_axes));
    VMOVE(bnas.axes_pos, viewAxesPos);
    bnas.axes_size = axesSize;
    VMOVE(bnas.axes_color, axesColor);
    VMOVE(bnas.label_color, labelColor);
    bnas.line_width = lineWidth;
    bnas.pos_only = posOnly;
    bnas.triple_color = tripleColor;
    bnas.tick_enabled = tickEnable;
    bnas.tick_length = tickLength;
    bnas.tick_major_length = majorTickLength;
    bnas.tick_interval = tickInterval;
    bnas.ticks_per_major = ticksPerMajor;
    VMOVE(bnas.tick_color, tickColor);
    VMOVE(bnas.tick_major_color, majorTickColor);
    bnas.tick_threshold = tickThreshold;

    dm_draw_hud_axes(dmop->dmo_dmp, viewSize, rmat, &bnas);

    bu_vls_free(&vls);
    return BRLCAD_OK;
}


/*
 * Begin the draw cycle.
 *
 * Usage:
 * objname drawBegin
 *
 */
HIDDEN int
dmo_drawBegin_tcl(void *clientData, int UNUSED(argc), const char **UNUSED(argv))
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    return dm_draw_begin(dmop->dmo_dmp);
}


HIDDEN int
dmo_drawEnd_tcl(void *clientData, int UNUSED(argc), const char **UNUSED(argv))
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    return dm_draw_end(dmop->dmo_dmp);
}


/*
 * End the draw cycle.
 *
 * Usage:
 * objname drawEnd
 *
 */
HIDDEN int
dmo_clear_tcl(void *clientData, int UNUSED(argc), const char **UNUSED(argv))
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    int status;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    if ((status = dm_draw_begin(dmop->dmo_dmp)) != BRLCAD_OK)
	return status;

    return dm_draw_end(dmop->dmo_dmp);
}


/*
 * Clear the display.
 *
 * Usage:
 * objname clear
 *
 */
HIDDEN int
dmo_normal_tcl(void *clientData, int UNUSED(argc), const char **UNUSED(argv))
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    return dm_hud_begin(dmop->dmo_dmp);
}


/*
 * Reset the viewing transform.
 *
 * Usage:
 * objname normal
 *
 */
HIDDEN int
dmo_loadmat_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    mat_t mat;
    int which_eye;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    if (argc != 4) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helplib_alias dm_loadmat %s", argv[1]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }


    if (bn_decode_mat(mat, argv[2]) != 16)
	return BRLCAD_ERROR;

    if (sscanf(argv[3], "%d", &which_eye) != 1) {
	Tcl_Obj *obj;

	obj = Tcl_GetObjResult(dmop->interp);
	if (Tcl_IsShared(obj))
	    obj = Tcl_DuplicateObj(obj);

	Tcl_AppendStringsToObj(obj, "bad eye value - ", argv[3], (char *)NULL);
	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_ERROR;
    }

    MAT_COPY(dmop->viewMat, mat);

    return dm_loadmatrix(dmop->dmo_dmp, mat, which_eye);
}


/*
 * Draw a string on the display.
 *
 * Usage:
 * objname drawString args
 *
 */
HIDDEN int
dmo_drawString_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    fastf_t x, y;
    int size;
    int use_aspect;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    if (argc != 7) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helplib_alias dm_drawString %s", argv[1]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    /*XXX use sscanf */
    x = atof(argv[3]);
    y = atof(argv[4]);
    size = atoi(argv[5]);
    use_aspect = atoi(argv[6]);

    return dm_draw_string_2d(dmop->dmo_dmp, argv[2], x, y, size, use_aspect);
}


HIDDEN int
dmo_drawPoint_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    fastf_t x, y;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    if (argc != 4) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helplib_alias dm_drawPoint %s", argv[1]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    /*XXX use sscanf */
    x = atof(argv[2]);
    y = atof(argv[3]);

    return dm_draw_point_2d(dmop->dmo_dmp, x, y);
}


/*
 * Draw the line.
 *
 * Usage:
 * objname drawLine x1 y1 x2 y2
 *
 */
HIDDEN int
dmo_drawLine_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    fastf_t xpos1, ypos1, xpos2, ypos2;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    if (argc != 6) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helplib_alias dm_drawLine %s", argv[1]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    /*XXX use sscanf */
    xpos1 = atof(argv[2]);
    ypos1 = atof(argv[3]);
    xpos2 = atof(argv[4]);
    ypos2 = atof(argv[5]);

    return dm_draw_line_2d(dmop->dmo_dmp, xpos1, ypos1, xpos2, ypos2);
}


/*
 * Draw the vlist.
 *
 * Usage:
 * objname drawVList vid
 */
HIDDEN int
dmo_drawVList_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bv_vlist *vp;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    if (argc != 3) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helplib_alias dm_drawVList %s", argv[1]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lu", (unsigned long *)&vp) != 1) {
	Tcl_Obj *obj;

	obj = Tcl_GetObjResult(dmop->interp);
	if (Tcl_IsShared(obj))
	    obj = Tcl_DuplicateObj(obj);

	Tcl_AppendStringsToObj(obj, "invalid vlist pointer - ", argv[2], (char *)NULL);
	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_ERROR;
    }

    BV_CK_VLIST(vp);

    return dm_draw_vlist(dmop->dmo_dmp, vp);
}


HIDDEN void
dmo_drawSolid(struct dm_obj *dmop,
	      struct bv_scene_obj *sp)
{
    if (sp->s_iflag == UP)
	dm_set_fg(dmop->dmo_dmp, 255, 255, 255, 0, sp->s_os.transparency);
    else
	dm_set_fg(dmop->dmo_dmp,
		       (unsigned char)sp->s_color[0],
		       (unsigned char)sp->s_color[1],
		       (unsigned char)sp->s_color[2], 0, sp->s_os.transparency);
    dm_draw_vlist(dmop->dmo_dmp, (struct bv_vlist *)&sp->s_vlist);
}


/*
 * Draw a scale.
 *
 * Usage:
 * drawScale vsize unit color
 *
 */
int
dmo_drawScale_cmd(struct dm_obj *dmop,
		  int argc,
		  const char **argv)
{
    const char *unit;
    int color[3];
    double scan;
    fastf_t viewSize;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 4) {
	bu_vls_printf(&vls, "helplib_alias dm_drawScale %s", argv[0]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[1], "%lf", &scan) != 1) {
	bu_vls_printf(&vls, "drawScale: bad view size - %s\n", argv[1]);
	Tcl_AppendResult(dmop->interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return BRLCAD_ERROR;
    }
    /* convert double to fastf_t */
    viewSize = scan;


    unit = argv[2];

    if (sscanf(argv[3], "%d %d %d",
	       &color[0],
	       &color[1],
	       &color[2]) != 3) {
	bu_vls_printf(&vls, "drawScale: bad color - %s\n", argv[2]);
	Tcl_AppendResult(dmop->interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return BRLCAD_ERROR;
    }

    /* validate color */
    if (color[0] < 0 || 255 < color[0]
	|| color[1] < 0 || 255 < color[1]
	|| color[2] < 0 || 255 < color[2])
    {

	bu_vls_printf(&vls, "drawScale: bad color - %s\n", argv[2]);
	Tcl_AppendResult(dmop->interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return BRLCAD_ERROR;
    }

    dm_draw_scale(dmop->dmo_dmp, viewSize, unit, color, color);

    return BRLCAD_OK;
}


/*
 * Usage:
 * objname drawScale vsize unit color
 */
HIDDEN int
dmo_drawScale_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    return dmo_drawScale_cmd(dmop, argc-1, argv+1);
}


/*
 * Usage:
 * objname drawSList hsp
 */
HIDDEN int
dmo_drawSList(struct dm_obj *dmop,
	      struct bu_list *hsp)
{
    struct bv_scene_obj *sp;
    int linestyle = -1;

    if (!dmop)
	return BRLCAD_ERROR;

    int dm_transparency = dm_get_transparency(dmop->dmo_dmp);
    if (dm_transparency) {
	/* First, draw opaque stuff */
	for (BU_LIST_FOR(sp, bv_scene_obj, hsp)) {
	    if (sp->s_os.transparency < 1.0)
		continue;

	    if (linestyle != sp->s_soldash) {
		linestyle = sp->s_soldash;
		dm_set_line_attr(dmop->dmo_dmp, dmop->dmo_dmp->i->dm_lineWidth, linestyle);
	    }

	    dmo_drawSolid(dmop, sp);
	}

	/* disable write to depth buffer */
	dm_set_depth_mask(dmop->dmo_dmp, 0);

	/* Second, draw transparent stuff */
	for (BU_LIST_FOR(sp, bv_scene_obj, hsp)) {
	    /* already drawn above */
	    if (ZERO(sp->s_os.transparency - 1.0))
		continue;

	    if (linestyle != sp->s_soldash) {
		linestyle = sp->s_soldash;
		dm_set_line_attr(dmop->dmo_dmp, dmop->dmo_dmp->i->dm_lineWidth, linestyle);
	    }

	    dmo_drawSolid(dmop, sp);
	}

	/* re-enable write to depth buffer */
	dm_set_depth_mask(dmop->dmo_dmp, 1);
    } else {

	for (BU_LIST_FOR(sp, bv_scene_obj, hsp)) {
	    if (linestyle != sp->s_soldash) {
		linestyle = sp->s_soldash;
		dm_set_line_attr(dmop->dmo_dmp, dmop->dmo_dmp->i->dm_lineWidth, linestyle);
	    }

	    dmo_drawSolid(dmop, sp);
	}
    }

    return BRLCAD_OK;
}


/*
 * Usage:
 * objname drawSList sid
 */
HIDDEN int
dmo_drawSList_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_list *hsp;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    if (argc != 3) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helplib_alias dm_drawSList %s", argv[1]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lu", (unsigned long *)&hsp) != 1) {
	Tcl_Obj *obj;

	obj = Tcl_GetObjResult(dmop->interp);
	if (Tcl_IsShared(obj))
	    obj = Tcl_DuplicateObj(obj);

	Tcl_AppendStringsToObj(obj, "invalid solid list pointer - ",
			       argv[2], "\n", (char *)NULL);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_ERROR;
    }
    dmo_drawSList(dmop, hsp);

    return BRLCAD_OK;
}


/*
 * Get/set the display manager's foreground color.
 *
 * Usage:
 * objname fg [rgb]
 */
HIDDEN int
dmo_fg_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int r, g, b;
    Tcl_Obj *obj;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    /* get foreground color */
    if (argc == 2) {
	bu_vls_printf(&vls, "%d %d %d",
		      dmop->dmo_dmp->i->dm_fg[0],
		      dmop->dmo_dmp->i->dm_fg[1],
		      dmop->dmo_dmp->i->dm_fg[2]);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
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
	return dm_set_fg(dmop->dmo_dmp,
			      (unsigned char)r,
			      (unsigned char)g,
			      (unsigned char)b,
			      1, 1.0);
    }

    /* wrong number of arguments */
    bu_vls_printf(&vls, "helplib_alias dm_fg %s", argv[1]);
    Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return BRLCAD_ERROR;

bad_color:
    bu_vls_printf(&vls, "bad rgb color - %s\n", argv[2]);
    Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    Tcl_SetObjResult(dmop->interp, obj);
    return BRLCAD_ERROR;
}


/*
 * Get/set the display manager's background color.
 *
 * Usage:
 * objname bg [rgb]
 */
HIDDEN int
dmo_bg_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int r, g, b;
    Tcl_Obj *obj;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    /* get background color */
    if (argc == 2) {
	bu_vls_printf(&vls, "%d %d %d",
		      dmop->dmo_dmp->i->dm_bg[0],
		      dmop->dmo_dmp->i->dm_bg[1],
		      dmop->dmo_dmp->i->dm_bg[2]);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
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
	return dm_set_bg(dmop->dmo_dmp, (unsigned char)r, (unsigned char)g, (unsigned char)b);
    }

    /* wrong number of arguments */
    bu_vls_printf(&vls, "helplib_alias dm_bg %s", argv[1]);
    Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return BRLCAD_ERROR;

bad_color:
    bu_vls_printf(&vls, "bad rgb color - %s\n", argv[2]);
    Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    Tcl_SetObjResult(dmop->interp, obj);
    return BRLCAD_ERROR;
}


/*
 * Get/set the display manager's linewidth.
 *
 * Usage:
 * objname linewidth [n]
 */
HIDDEN int
dmo_lineWidth_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int lineWidth;
    Tcl_Obj *obj;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    /* get linewidth */
    if (argc == 2) {
	bu_vls_printf(&vls, "%d", dmop->dmo_dmp->i->dm_lineWidth);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
    }

    /* set lineWidth */
    if (argc == 3) {
	if (sscanf(argv[2], "%d", &lineWidth) != 1)
	    goto bad_lineWidth;

	/* validate lineWidth */
	if (lineWidth < 0 || 20 < lineWidth)
	    goto bad_lineWidth;

	bu_vls_free(&vls);
	return dm_set_line_attr(dmop->dmo_dmp, lineWidth, dmop->dmo_dmp->i->dm_lineStyle);
    }

    /* wrong number of arguments */
    bu_vls_printf(&vls, "helplib_alias dm_linewidth %s", argv[1]);
    Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return BRLCAD_ERROR;

bad_lineWidth:
    bu_vls_printf(&vls, "bad linewidth - %s\n", argv[2]);
    Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    Tcl_SetObjResult(dmop->interp, obj);
    return BRLCAD_ERROR;
}


/*
 * Get/set the display manager's linestyle.
 *
 * Usage:
 * objname linestyle [0|1]
 */
HIDDEN int
dmo_lineStyle_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int linestyle;
    Tcl_Obj *obj;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    /* get linestyle */
    if (argc == 2) {
	bu_vls_printf(&vls, "%d", dmop->dmo_dmp->i->dm_lineStyle);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
    }

    /* set linestyle */
    if (argc == 3) {
	if (sscanf(argv[2], "%d", &linestyle) != 1)
	    goto bad_linestyle;

	/* validate linestyle */
	if (linestyle < 0 || 1 < linestyle)
	    goto bad_linestyle;

	bu_vls_free(&vls);
	return dm_set_line_attr(dmop->dmo_dmp, dmop->dmo_dmp->i->dm_lineWidth, linestyle);
    }

    /* wrong number of arguments */
    bu_vls_printf(&vls, "helplib_alias dm_linestyle %s", argv[1]);
    Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return BRLCAD_ERROR;

bad_linestyle:
    bu_vls_printf(&vls, "bad linestyle - %s\n", argv[2]);
    Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    Tcl_SetObjResult(dmop->interp, obj);
    return BRLCAD_ERROR;
}


/*
 * Configure the display manager window. This is typically
 * called as a result of a ConfigureNotify event.
 *
 * Usage:
 * objname configure
 */
HIDDEN int
dmo_configure_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    int status;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    if (argc != 2) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helplib_alias dm_configure %s", argv[1]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    /* configure the display manager window */
    status = dm_configure_win(dmop->dmo_dmp, 0);

#ifdef USE_FBSERV
    /* configure the framebuffer window */
    if (dmop->dmo_fbs.fbs_fbp != FB_NULL)
	(void)fb_configure_window(dmop->dmo_fbs.fbs_fbp,
			   dmop->dmo_dmp->i->dm_width,
			   dmop->dmo_dmp->i->dm_height);
#endif

    return status;
}


/*
 * Get/set the display manager's zclip flag.
 *
 * Usage:
 * objname zclip [0|1]
 */
HIDDEN int
dmo_zclip_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int zclip;
    Tcl_Obj *obj;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    /* get zclip flag */
    if (argc == 2) {
	bu_vls_printf(&vls, "%d", dmop->dmo_dmp->i->dm_zclip);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
    }

    /* set zclip flag */
    if (argc == 3) {
	if (sscanf(argv[2], "%d", &zclip) != 1) {
	    Tcl_AppendStringsToObj(obj, "dmo_zclip: invalid zclip value - ",
				   argv[2], "\n", (char *)NULL);
	    Tcl_SetObjResult(dmop->interp, obj);
	    return BRLCAD_ERROR;
	}

	dmop->dmo_dmp->i->dm_zclip = zclip;
	return BRLCAD_OK;
    }

    bu_vls_printf(&vls, "helplib_alias dm_zclip %s", argv[1]);
    Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return BRLCAD_ERROR;
}


/*
 * Get/set the display manager's zbuffer flag.
 *
 * Usage:
 * objname zbuffer [0|1]
 */
HIDDEN int
dmo_zbuffer_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int zbuffer;
    Tcl_Obj *obj;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    /* get zbuffer flag */
    if (argc == 2) {
	bu_vls_printf(&vls, "%d", dm_get_zbuffer(dmop->dmo_dmp));
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
    }

    /* set zbuffer flag */
    if (argc == 3) {
	if (sscanf(argv[2], "%d", &zbuffer) != 1) {
	    Tcl_AppendStringsToObj(obj, "dmo_zbuffer: invalid zbuffer value - ",
				   argv[2], "\n", (char *)NULL);
	    Tcl_SetObjResult(dmop->interp, obj);
	    return BRLCAD_ERROR;
	}

	dm_set_zbuffer(dmop->dmo_dmp, zbuffer);
	return BRLCAD_OK;
    }

    bu_vls_printf(&vls, "helplib_alias dm_zbuffer %s", argv[1]);
    Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return BRLCAD_ERROR;
}


/*
 * Get/set the display manager's light flag.
 *
 * Usage:
 * objname light [0|1]
 */
HIDDEN int
dmo_light_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int light;
    Tcl_Obj *obj;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    /* get light flag */
    if (argc == 2) {
	bu_vls_printf(&vls, "%d", dm_get_light(dmop->dmo_dmp));
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
    }

    /* set light flag */
    if (argc == 3) {
	if (sscanf(argv[2], "%d", &light) != 1) {
	    Tcl_AppendStringsToObj(obj, "dmo_light: invalid light value - ",
				   argv[2], "\n", (char *)NULL);

	    Tcl_SetObjResult(dmop->interp, obj);
	    return BRLCAD_ERROR;
	}

	(void)dm_set_light(dmop->dmo_dmp, light);
	return BRLCAD_OK;
    }

    bu_vls_printf(&vls, "helplib_alias dm_light %s", argv[1]);
    Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return BRLCAD_ERROR;
}


/*
 * Get/set the display manager's transparency flag.
 *
 * Usage:
 * objname transparency [0|1]
 */
HIDDEN int
dmo_transparency_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int transparency;
    Tcl_Obj *obj;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    /* get transparency flag */
    if (argc == 2) {
	int dm_transparency = dm_get_transparency(dmop->dmo_dmp);
	bu_vls_printf(&vls, "%d", dm_transparency);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
    }

    /* set transparency flag */
    if (argc == 3) {
	if (sscanf(argv[2], "%d", &transparency) != 1) {
	    Tcl_AppendStringsToObj(obj, "dmo_transparency: invalid transparency value - ",
				   argv[2], "\n", (char *)NULL);

	    Tcl_SetObjResult(dmop->interp, obj);
	    return BRLCAD_ERROR;
	}

	(void)dm_set_transparency(dmop->dmo_dmp, transparency);
	return BRLCAD_OK;
    }

    bu_vls_printf(&vls, "helplib_alias dm_transparency %s", argv[1]);
    Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return BRLCAD_ERROR;
}


/*
 * Get/set the display manager's depth mask flag.
 *
 * Usage:
 * objname depthMask [0|1]
 */
HIDDEN int
dmo_depthMask_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int depthMask;
    Tcl_Obj *obj;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    /* get depthMask flag */
    if (argc == 2) {
	bu_vls_printf(&vls, "%d", dmop->dmo_dmp->i->dm_depthMask);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
    }

    /* set depthMask flag */
    if (argc == 3) {
	if (sscanf(argv[2], "%d", &depthMask) != 1) {
	    Tcl_AppendStringsToObj(obj, "dmo_depthMask: invalid depthMask value - ",
				   argv[2], "\n", (char *)NULL);

	    Tcl_SetObjResult(dmop->interp, obj);
	    return BRLCAD_ERROR;
	}

	dm_set_depth_mask(dmop->dmo_dmp, depthMask);
	return BRLCAD_OK;
    }

    bu_vls_printf(&vls, "helplib_alias dm_depthMask %s", argv[1]);
    Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return BRLCAD_ERROR;
}


/*
 * Get/set the display manager's window bounds.
 *
 * Usage:
 * objname bounds ["xmin xmax ymin ymax zmin zmax"]
 */
HIDDEN int
dmo_bounds_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    Tcl_Obj *obj;

    /* intentionally double for scan */
    double clipmin[3];
    double clipmax[3];

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    /* get window bounds */
    if (argc == 2) {
	bu_vls_printf(&vls, "%g %g %g %g %g %g",
		      dmop->dmo_dmp->i->dm_clipmin[X],
		      dmop->dmo_dmp->i->dm_clipmax[X],
		      dmop->dmo_dmp->i->dm_clipmin[Y],
		      dmop->dmo_dmp->i->dm_clipmax[Y],
		      dmop->dmo_dmp->i->dm_clipmin[Z],
		      dmop->dmo_dmp->i->dm_clipmax[Z]);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
    }

    /* set window bounds */
    if (argc == 3) {
	if (sscanf(argv[2], "%lf %lf %lf %lf %lf %lf",
		   &clipmin[X], &clipmax[X],
		   &clipmin[Y], &clipmax[Y],
		   &clipmin[Z], &clipmax[Z]) != 6) {
	    Tcl_AppendStringsToObj(obj, "dmo_bounds: invalid bounds - ",
				   argv[2], "\n", (char *)NULL);

	    Tcl_SetObjResult(dmop->interp, obj);
	    return BRLCAD_ERROR;
	}

	VMOVE(dmop->dmo_dmp->i->dm_clipmin, clipmin);
	VMOVE(dmop->dmo_dmp->i->dm_clipmax, clipmax);

	/*
	 * Since dm_bound doesn't appear to be used anywhere,
	 * I'm going to use it for controlling the location
	 * of the zclipping plane in dm-ogl.c. dm-X.c uses
	 * dm_clipmin and dm_clipmax.
	 */
	if (dmop->dmo_dmp->i->dm_clipmax[2] <= GED_MAX)
	    dmop->dmo_dmp->i->dm_bound = 1.0;
	else
	    dmop->dmo_dmp->i->dm_bound = GED_MAX / dmop->dmo_dmp->i->dm_clipmax[2];

	return BRLCAD_OK;
    }

    bu_vls_printf(&vls, "helplib_alias dm_bounds %s", argv[1]);
    Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return BRLCAD_ERROR;
}


/*
 * Get/set the display manager's perspective mode.
 *
 * Usage:
 * objname perspective [n]
 */
HIDDEN int
dmo_perspective_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int perspective;
    Tcl_Obj *obj;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    /* get perspective mode */
    if (argc == 2) {
	bu_vls_printf(&vls, "%d", dmop->dmo_dmp->i->dm_perspective);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
    }

    /* set perspective mode */
    if (argc == 3) {
	if (sscanf(argv[2], "%d", &perspective) != 1) {
	    Tcl_AppendStringsToObj(obj,
				   "dmo_perspective: invalid perspective mode - ",
				   argv[2], "\n", (char *)NULL);

	    Tcl_SetObjResult(dmop->interp, obj);
	    return BRLCAD_ERROR;
	}

	dmop->dmo_dmp->i->dm_perspective = perspective;
	return BRLCAD_OK;
    }

    bu_vls_printf(&vls, "helplib_alias dm_perspective %s", argv[1]);
    Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return BRLCAD_ERROR;
}


HIDDEN int
dmo_png_cmd(struct dm_obj *dmop,
	    int argc,
	    const char **argv)
{
    struct bu_vls msgs = BU_VLS_INIT_ZERO;
    FILE *fp;

    if (argc != 2) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helplib_alias dm_png %s", argv[0]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    if (!dmop->dmo_dmp->i->dm_write_image) {
	Tcl_AppendResult(dmop->interp, "png: writing unsupported with this display manager type.\n", (char *)NULL);
	return BRLCAD_ERROR;
    }

    if ((fp = fopen(argv[1], "wb")) == NULL) {
	Tcl_AppendResult(dmop->interp, "png: cannot open \"", argv[1], " for writing\n", (char *)NULL);
	return BRLCAD_ERROR;
    }

    int ret = dmop->dmo_dmp->i->dm_write_image(&msgs, fp, dmop->dmo_dmp);

    if (ret) {
	Tcl_AppendResult(dmop->interp, bu_vls_cstr(&msgs), (char *)NULL);
    }

    bu_vls_free(&msgs);
    fclose(fp);

    return (ret) ? BRLCAD_ERROR : BRLCAD_OK;
}

/*
 * Write the pixels to a file in png format.
 *
 * Usage:
 * objname png args
 *
 */
HIDDEN int
dmo_png_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    return dmo_png_cmd(dmop, argc-1, argv+1);
}


/*
 * Get/set the clearBufferAfter flag.
 *
 * Usage:
 * objname clearBufferAfter [flag]
 *
 */
HIDDEN int
dmo_clearBufferAfter_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int clearBufferAfter;
    Tcl_Obj *obj;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    /* get clearBufferAfter flag */
    if (argc == 2) {
	bu_vls_printf(&vls, "%d", dmop->dmo_dmp->i->dm_clearBufferAfter);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
    }

    /* set clearBufferAfter flag */
    if (argc == 3) {
	if (sscanf(argv[2], "%d", &clearBufferAfter) != 1) {
	    Tcl_AppendStringsToObj(obj, "dmo_clearBufferAfter: invalid clearBufferAfter value - ",
				   argv[2], "\n", (char *)NULL);
	    Tcl_SetObjResult(dmop->interp, obj);
	    return BRLCAD_ERROR;
	}

	dmop->dmo_dmp->i->dm_clearBufferAfter = clearBufferAfter;
	return BRLCAD_OK;
    }

    bu_vls_printf(&vls, "helplib_alias dm_clearBufferAfter %s", argv[1]);
    Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return BRLCAD_ERROR;
}


/*
 * Get/set the display manager's debug level.
 *
 * Usage:
 * objname debug [n]
 */
HIDDEN int
dmo_debug_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int level;
    Tcl_Obj *obj;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    /* get debug level */
    if (argc == 2) {
	bu_vls_printf(&vls, "%d", dm_get_debug(dmop->dmo_dmp));
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
    }

    /* set debug level */
    if (argc == 3) {
	if (sscanf(argv[2], "%d", &level) != 1) {
	    Tcl_AppendStringsToObj(obj, "dmo_debug: invalid debug level - ",
				   argv[2], "\n", (char *)NULL);

	    Tcl_SetObjResult(dmop->interp, obj);
	    return BRLCAD_ERROR;
	}

	return dm_set_debug(dmop->dmo_dmp, level);
    }

    bu_vls_printf(&vls, "helplib_alias dm_debug %s", argv[1]);
    Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return BRLCAD_ERROR;
}

/*
 * Get/set the display manager's log file.
 *
 * Usage:
 * objname logfile [filename]
 */
HIDDEN int
dmo_logfile_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    Tcl_Obj *obj;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    /* get log file */
    if (argc == 2) {
	bu_vls_printf(&vls, "%s", bu_vls_addr(&(dmop->dmo_dmp->i->dm_log)));
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
    }

    /* set log file */
    if (argc == 3) {
	return dm_logfile(dmop->dmo_dmp, argv[3]);
    }

    bu_vls_printf(&vls, "helplib_alias dm_debug %s", argv[1]);
    Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return BRLCAD_ERROR;
}



/*
 * Flush the output buffer.
 *
 * Usage:
 * objname flush
 *
 */
HIDDEN int
dmo_flush_tcl(void *clientData, int UNUSED(argc), const char **UNUSED(argv))
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    if (!dmop)
	return BRLCAD_ERROR;

    if (dmop->dmo_dmp->i->dm_flush) {
	dmop->dmo_dmp->i->dm_flush(dmop->dmo_dmp);
    }

    return BRLCAD_OK;
}


/*
 * Flush the output buffer and process all events.
 *
 * Usage:
 * objname sync
 *
 */
HIDDEN int
dmo_sync_tcl(void *clientData, int UNUSED(argc), const char **UNUSED(argv))
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    if (!dmop)
	return BRLCAD_ERROR;

    if (dmop->dmo_dmp->i->dm_sync) {
	dmop->dmo_dmp->i->dm_sync(dmop->dmo_dmp);
    }

    return BRLCAD_OK;
}


/*
 * Set/get window size.
 *
 * Usage:
 * objname size [width [height]]
 *
 */
HIDDEN int
dmo_size_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    Tcl_Obj *obj;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    if (argc == 2) {
	bu_vls_printf(&vls, "%d %d", dmop->dmo_dmp->i->dm_width, dmop->dmo_dmp->i->dm_height);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(dmop->interp, obj);
	return BRLCAD_OK;
    }

    if (argc == 3 || argc == 4) {
	int width, height;

	if (sscanf(argv[2], "%d", &width) != 1) {
	    Tcl_AppendStringsToObj(obj, "size: bad width - ", argv[2], "\n", (char *)NULL);

	    Tcl_SetObjResult(dmop->interp, obj);
	    return BRLCAD_ERROR;
	}

	if (argc == 3)
	    height = width;
	else {
	    if (sscanf(argv[3], "%d", &height) != 1) {
		Tcl_AppendStringsToObj(obj, "size: bad height - ", argv[3], "\n", (char *)NULL);
		Tcl_SetObjResult(dmop->interp, obj);
		return BRLCAD_ERROR;
	    }
	}

	if (dmop->dmo_dmp->i->dm_geometry_request) {
	    dmop->dmo_dmp->i->dm_geometry_request(dmop->dmo_dmp, width, height);
	    return BRLCAD_OK;
	} else {
	    bu_log("Sorry, support for 'size' command is unavailable.\n");
	    return BRLCAD_ERROR;
	}
    }

    bu_vls_printf(&vls, "helplib_alias dm_size %s", argv[1]);
    Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return BRLCAD_ERROR;
}


/*
 * Get window aspect ratio (i.e. width / height)
 *
 * Usage:
 * objname get_aspect
 *
 */
HIDDEN int
dmo_get_aspect_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    Tcl_Obj *obj;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    if (argc != 2) {
	bu_vls_printf(&vls, "helplib_alias dm_getaspect %s", argv[1]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    obj = Tcl_GetObjResult(dmop->interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    bu_vls_printf(&vls, "%g", dmop->dmo_dmp->i->dm_aspect);
    Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    Tcl_SetObjResult(dmop->interp, obj);
    return BRLCAD_OK;
}


/*
 * Attach/detach observers to/from list.
 *
 * Usage:
 * objname observer cmd [args]
 *
 */
HIDDEN int
dmo_observer_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    if (argc < 3) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	/* return help message */
	bu_vls_printf(&vls, "helplib_alias dm_observer %s", argv[1]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    return bu_observer_cmd((ClientData)&dmop->dmo_observers, argc-2, (const char **)argv+2);
}

HIDDEN void
_dm_obj_eval(void *context, const char *cmd) {
    Tcl_Interp *interp = (Tcl_Interp *)context;
    Tcl_Eval(interp, cmd);
}

#ifdef USE_FBSERV
HIDDEN void
dmo_fbs_callback(void *clientData)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    if (!dmop)
	return;

    bu_observer_notify((void *)dmop->interp, &dmop->dmo_observers, bu_vls_addr(&dmop->dmo_name), &(_dm_obj_eval));
}
#endif


HIDDEN int
dmo_getDrawLabelsHook_cmd(struct dm_obj *dmop, int argc, const char **argv)
{
    char buf[64];
    Tcl_DString ds;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    if (argc != 1) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helplib_alias dm_getDrawLabelsHook %s", argv[0]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    /* FIXME: the standard forbids this kind of crap.  candidate for removal. */
    sprintf(buf, "%p %p",
	    (void *)(uintptr_t)dmop->dmo_drawLabelsHook,
	    (void *)(uintptr_t)dmop->dmo_drawLabelsHookClientData);
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, buf, -1);
    Tcl_DStringResult(dmop->interp, &ds);

    return BRLCAD_OK;
}


HIDDEN int
dmo_getDrawLabelsHook_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    return dmo_getDrawLabelsHook_cmd(dmop, argc-1, argv+1);
}


HIDDEN int
dmo_setDrawLabelsHook_cmd(struct dm_obj *dmop, int argc, const char **argv)
{
    int (*hook)(struct dm *, struct rt_wdb *, const char *, mat_t, int *, ClientData);
    void *clientData;

    if (!dmop || !dmop->interp)
	return BRLCAD_ERROR;

    if (argc != 3) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helplib_alias dm_setDrawLabelsHook %s", argv[0]);
	Tcl_Eval(dmop->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[1], "%p", (void **)((unsigned char *)&hook)) != 1) {
	Tcl_DString ds;

	Tcl_DStringInit(&ds);
	Tcl_DStringAppend(&ds, argv[0], -1);
	Tcl_DStringAppend(&ds, ": failed to set the drawLabels hook", -1);
	Tcl_DStringResult(dmop->interp, &ds);

	dmop->dmo_drawLabelsHook = (int (*)(struct dm *, struct rt_wdb *, const char *, mat_t, int *, ClientData))0;

	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%p", &clientData) != 1) {
	Tcl_DString ds;

	Tcl_DStringInit(&ds);
	Tcl_DStringAppend(&ds, argv[0], -1);
	Tcl_DStringAppend(&ds, ": failed to set the drawLabels hook", -1);
	Tcl_DStringResult(dmop->interp, &ds);

	dmop->dmo_drawLabelsHook = (int (*)(struct dm *, struct rt_wdb *, const char *, mat_t, int *, ClientData))0;

	return BRLCAD_ERROR;
    }

    /* FIXME: standard prohibits casting between function pointers and
     * void *.  find a better way.
     */
    dmop->dmo_drawLabelsHook = hook;
    dmop->dmo_drawLabelsHookClientData = clientData;

    return BRLCAD_OK;
}


HIDDEN int
dmo_setDrawLabelsHook_tcl(void *clientData, int argc, const char **argv)
{
    struct dm_obj *dmop = (struct dm_obj *)clientData;

    return dmo_setDrawLabelsHook_cmd(dmop, argc-1, argv+1);
}


/*
 * Called by Tcl when the object is destroyed.
 */
HIDDEN void
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
    (void)dm_close(dmop->dmo_dmp);
    BU_LIST_DEQUEUE(&dmop->l);
    bu_free((void *)dmop, "dmo_deleteProc: dmop");

}


/*
 * Generic interface for display manager object routines.
 * Usage:
 * objname cmd ?args?
 *
 * Returns: result of DM command.
 */
HIDDEN int
dmo_cmd(ClientData clientData, Tcl_Interp *UNUSED(interp), int argc, const char **argv)
{
    int ret;

    static struct bu_cmdtab dmo_cmds[] = {
	{"bg",			dmo_bg_tcl},
	{"bounds",		dmo_bounds_tcl},
	{"clear",		dmo_clear_tcl},
	{"configure",		dmo_configure_tcl},
	{"debug",		dmo_debug_tcl},
	{"depthMask",		dmo_depthMask_tcl},
	{"drawBegin",		dmo_drawBegin_tcl},
	{"drawEnd",		dmo_drawEnd_tcl},
	{"drawGeom",	BU_CMD_NULL},
	{"drawLabels",	BU_CMD_NULL},
	{"drawLine",		dmo_drawLine_tcl},
	{"drawPoint",		dmo_drawPoint_tcl},
	{"drawScale",		dmo_drawScale_tcl},
	{"drawSList",		dmo_drawSList_tcl},
	{"drawString",		dmo_drawString_tcl},
	{"drawVList",		dmo_drawVList_tcl},
	{"drawDataAxes",	dmo_drawDataAxes_tcl},
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
	{"logfile",		dmo_logfile_tcl},
	{"normal",		dmo_normal_tcl},
	{"observer",		dmo_observer_tcl},
	{"perspective",		dmo_perspective_tcl},
	{"png",		        dmo_png_tcl},
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
	{(const char *)NULL, BU_CMD_NULL}
    };

    if (bu_cmd(dmo_cmds, argc, argv, 1, clientData, &ret) == BRLCAD_OK)
	return ret;

    bu_log("ERROR: '%s' command not found\n", argv[1]);
    return BRLCAD_ERROR;
}


/*
 * Open/create a display manager object.
 *
 * Usage:
 * dm_open [name type [args]]
 */
HIDDEN int
dmo_open_tcl(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct dm_obj *dmop;
    struct dm *dmp;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int name_index = 1;
    const char *type = NULL;
    Tcl_Obj *obj;

    obj = Tcl_GetObjResult(interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    if (argc == 1) {
	/* get list of display manager objects */
	for (BU_LIST_FOR(dmop, dm_obj, &HeadDMObj.l))
	    Tcl_AppendStringsToObj(obj, bu_vls_addr(&dmop->dmo_name), " ", (char *)NULL);

	Tcl_SetObjResult(interp, obj);
	return BRLCAD_OK;
    }

    if (argc < 3) {
	bu_vls_printf(&vls, "helplib_alias dm_open %s", argv[1]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    /* check to see if display manager object exists */
    for (BU_LIST_FOR(dmop, dm_obj, &HeadDMObj.l)) {
	if (BU_STR_EQUAL(argv[name_index], bu_vls_addr(&dmop->dmo_name))) {
	    Tcl_AppendStringsToObj(obj, "dmo_open: ", argv[name_index],
				   " exists.", (char *)NULL);
	    Tcl_SetObjResult(interp, obj);
	    return BRLCAD_ERROR;
	}
    }

    /* find display manager type */
    if (argv[2][0] == 'X' || argv[2][0] == 'x')
	type = argv[2];

    if (BU_STR_EQUAL(argv[2], "tk"))
	type = argv[2];

    if (BU_STR_EQUAL(argv[2], "ogl"))
	type = argv[2];

    if (BU_STR_EQUAL(argv[2], "wgl"))
	type = argv[2];

    if (!type) {
	Tcl_AppendStringsToObj(obj,
			       "Unsupported display manager type - ",
			       argv[2], "\n",
			       "The supported types are: X, ogl, wgl, and nu",
			       (char *)NULL);
	Tcl_SetObjResult(interp, obj);
	return BRLCAD_ERROR;
    }

    {
	int i;
	int arg_start = 3;
	int newargs = 2;
	int ac;
	const char **av;

	ac = argc + newargs;
	av = (const char **)bu_malloc(sizeof(char *) * (ac+1), "dmo_open_tcl: av");
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
	av[i+newargs] = (const char *)NULL;

	if ((dmp = dm_open(NULL, (void *)interp, type, ac, av)) == DM_NULL) {
	    if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);

	    Tcl_AppendStringsToObj(obj,
				   "dmo_open_tcl: Failed to open - ",
				   argv[name_index],
				   "\n",
				   (char *)NULL);
	    bu_free((void *)av, "dmo_open_tcl: av");

	    Tcl_SetObjResult(interp, obj);
	    return BRLCAD_ERROR;
	}

	bu_free((void *)av, "dmo_open_tcl: av");
    }

    /* acquire dm_obj struct */
    BU_ALLOC(dmop, struct dm_obj);

    /* initialize dm_obj */
    bu_vls_init(&dmop->dmo_name);
    bu_vls_strcpy(&dmop->dmo_name, argv[name_index]);
    dmop->dmo_dmp = dmp;
    VSETALL(dmop->dmo_dmp->i->dm_clipmin, -2048.0);
    VSETALL(dmop->dmo_dmp->i->dm_clipmax, 2047.0);
    dmop->dmo_drawLabelsHook = (int (*)(struct dm *, struct rt_wdb *, const char *, mat_t, int *, ClientData))0;

#ifdef USE_FBSERV
    dmop->dmo_fbs.fbs_listener.fbsl_fbsp = &dmop->dmo_fbs;
    dmop->dmo_fbs.fbs_listener.fbsl_fd = -1;
    dmop->dmo_fbs.fbs_listener.fbsl_port = -1;
    dmop->dmo_fbs.fbs_fbp = FB_NULL;
    dmop->dmo_fbs.fbs_callback = dmo_fbs_callback;
    dmop->dmo_fbs.fbs_clientData = dmop;
    dmop->dmo_fbs.fbs_interp = interp;
#endif
    dmop->interp = interp;

    /* append to list of dm_obj's */
    BU_LIST_APPEND(&HeadDMObj.l, &dmop->l);

    (void)Tcl_CreateCommand(interp,
			    bu_vls_addr(&dmop->dmo_name),
			    (Tcl_CmdProc *)dmo_cmd,
			    (ClientData)dmop,
			    dmo_deleteProc);

    /* send Configure event */
    bu_vls_printf(&vls, "event generate %s <Configure>; update", bu_vls_addr(&dmop->dmo_name));
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

#ifdef USE_FBSERV
    /* open the framebuffer */
    dmo_openFb(dmop);
#endif

    /* Return new function name as result */
    Tcl_SetResult(interp, bu_vls_addr(&dmop->dmo_name), TCL_VOLATILE);
    return BRLCAD_OK;
}


int
Dmo_Init(Tcl_Interp *interp)
{
    BU_LIST_INIT(&HeadDMObj.l);
    BU_VLS_INIT(&HeadDMObj.dmo_name);
    (void)Tcl_CreateCommand(interp, "dm_open", (Tcl_CmdProc *)dmo_open_tcl, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    return BRLCAD_OK;
}


/* from libdm/query.c */
extern int dm_validXType(const char *dpy_string, const char *name);

HIDDEN int
dm_validXType_tcl(void *clientData, int argc, const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    Tcl_Obj *obj;

    if (argc != 3) {
	bu_vls_printf(&vls, "helplib dm_validXType");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    int valid = dm_validXType(argv[1], argv[2]);

    bu_vls_printf(&vls, "%d", (valid == 1) ? 1 : 0);
    obj = Tcl_GetObjResult(interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);
    Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    Tcl_SetObjResult(interp, obj);
    return BRLCAD_OK;
}


HIDDEN int
dm_bestXType_tcl(void *clientData, int argc, const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    Tcl_Obj *obj;
    const char *best_dm;
    char buffer[256] = {0};

    if (argc != 2) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helplib dm_bestXType");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    obj = Tcl_GetObjResult(interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);
    snprintf(buffer, 256, "%s", argv[1]);
    best_dm = dm_bestXType(buffer);
    if (best_dm) {
	Tcl_AppendStringsToObj(obj, best_dm, (char *)NULL);
	Tcl_SetObjResult(interp, obj);
	return BRLCAD_OK;
    }
    return BRLCAD_ERROR;
}

/**
 * Hook function wrapper to the fb_common_file_size Tcl command
 */
int
fb_cmd_common_file_size(ClientData clientData, int argc, const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    size_t width, height;
    int pixel_size = 3;

    if (argc != 2 && argc != 3) {
	bu_log("wrong #args: should be \" fileName [#bytes/pixel]\"");
	return TCL_ERROR;
    }

    if (argc >= 3) {
	pixel_size = atoi(argv[2]);
    }

    if (fb_common_file_size(&width, &height, argv[1], pixel_size) > 0) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&vls, "%lu %lu", (unsigned long)width, (unsigned long)height);
	Tcl_SetObjResult(interp,
			 Tcl_NewStringObj(bu_vls_addr(&vls), bu_vls_strlen(&vls)));
	bu_vls_free(&vls);
	return TCL_OK;
    }

    /* Signal error */
    Tcl_SetResult(interp, "0 0", TCL_STATIC);
    return TCL_OK;
}

static int
wrapper_func(ClientData data, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct bu_cmdtab *ctp = (struct bu_cmdtab *)data;

    return ctp->ct_func(interp, argc, argv);
}

static void
register_cmds(Tcl_Interp *interp, struct bu_cmdtab *cmds)
{
    struct bu_cmdtab *ctp = NULL;

    for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	(void)Tcl_CreateCommand(interp, ctp->ct_name, wrapper_func, (ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
    }
}


int
Dm_Init(Tcl_Interp *interp)
{
    static struct bu_cmdtab cmdtab[] = {
	{"dm_validXType", dm_validXType_tcl},
	{"dm_bestXType", dm_bestXType_tcl},
	{"fb_common_file_size",	 fb_cmd_common_file_size},
	{(const char *)NULL, BU_CMD_NULL}
    };

    /* register commands */
    register_cmds(interp, cmdtab);

    /* initialize display manager object code */
    Dmo_Init(interp);

    /* initialize framebuffer object code */
    Fbo_Init(interp);

    Tcl_PkgProvide(interp,  "Dm", brlcad_version());
    Tcl_PkgProvide(interp,  "Fb", brlcad_version());

    return BRLCAD_OK;
}

/**
 * @brief
 * A TCL interface to dm_list_types()).
 *
 * @return a list of available dm types.
 */
int
dm_list_tcl(ClientData UNUSED(clientData),
	    Tcl_Interp *interp,
	    int UNUSED(argc),
	    const char **UNUSED(argv))
{
    struct bu_vls list = BU_VLS_INIT_ZERO;
    dm_list_types(&list, ",");
    Tcl_SetResult(interp, bu_vls_addr(&list), TCL_VOLATILE);
    bu_vls_free(&list);
    return TCL_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

/*                       D M - P L O T . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @file libdm/dm-plot.c
 *
 * An unsatisfying (but useful) hack to allow GED to generate
 * UNIX-plot files that not only contain the drawn objects, but also
 * contain the faceplate display as well.  Mostly, a useful hack for
 * making viewgraphs and photographs of an editing session.  We assume
 * that the UNIX-plot filter used can at least discard the
 * non-standard extention to specify color (a Doug Gwyn addition).
 *
 */

#include "common.h"

#define __USE_POSIX2 1
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>		/* for struct timeval */
#endif
#ifdef HAVE_UNISTED_H
#  include <unistd.h>
#endif
#include "bio.h"

#include "tcl.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "mater.h"
#include "raytrace.h"
#include "dm.h"
#include "dm-plot.h"
#include "solid.h"
#include "plot3.h"

/* Display Manager package interface */

#define PLOTBOUND 1000.0	/* Max magnification in Rot matrix */

struct plot_vars head_plot_vars;
static mat_t plotmat;


/**
 * P L O T _ C L O S E
 *
 * Gracefully release the display.
 */
HIDDEN int
plot_close(struct dm *dmp)
{
    if (!dmp)
	return TCL_ERROR;

    (void)fflush(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp);

    if (((struct plot_vars *)dmp->dm_vars.priv_vars)->is_pipe)
	pclose(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp); /* close pipe, eat dead children */
    else
	fclose(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp);

    bu_vls_free(&dmp->dm_pathName);
    bu_free((genptr_t)dmp->dm_vars.priv_vars, "plot_close: plot_vars");
    bu_free((genptr_t)dmp, "plot_close: dmp");

    return TCL_OK;
}


/**
 * P L O T _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
HIDDEN int
plot_drawBegin(struct dm *dmp)
{
    if (!dmp)
	return TCL_ERROR;

    /* We expect the screen to be blank so far, from last frame flush */

    return TCL_OK;
}


/**
 * P L O T _ E P I L O G
 */
HIDDEN int
plot_drawEnd(struct dm *dmp)
{
    if (!dmp)
	return TCL_ERROR;

    pl_flush(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp); /* BRL-specific command */
    pl_erase(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp); /* forces drawing */
    (void)fflush(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp);

    return TCL_OK;
}


/**
 * P L O T _ L O A D M A T R I X
 *
 * Load a new transformation matrix.  This will be followed by
 * many calls to plot_draw().
 */
HIDDEN int
plot_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye)
{
    Tcl_Obj *obj;

    if (!dmp)
	return TCL_ERROR;

    obj = Tcl_GetObjResult(dmp->dm_interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    if (((struct plot_vars *)dmp->dm_vars.priv_vars)->debug) {
	struct bu_vls tmp_vls;

	Tcl_AppendStringsToObj(obj, "plot_loadMatrix()\n", (char *)NULL);

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "which eye = %d\t", which_eye);
	bu_vls_printf(&tmp_vls, "transformation matrix = \n");
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[0], mat[4], mat[8], mat[12]);
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[1], mat[5], mat[9], mat[13]);
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[2], mat[6], mat[10], mat[14]);
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[3], mat[7], mat[11], mat[15]);

	Tcl_AppendStringsToObj(obj, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
    }

    MAT_COPY(plotmat, mat);
    Tcl_SetObjResult(dmp->dm_interp, obj);
    return TCL_OK;
}


/**
 * P L O T _ O B J E C T
 *
 * Set up for an object, transformed as indicated, and with an
 * object center as specified.  The ratio of object to screen size
 * is passed in as a convienience.
 *
 * Returns 0 if object could be drawn, !0 if object was omitted.
 */
HIDDEN int
plot_drawVList(struct dm *dmp, struct bn_vlist *vp)
{
    static vect_t last;
    struct bn_vlist *tvp;
    point_t *pt_prev=NULL;
    fastf_t dist_prev=1.0;
    fastf_t dist;
    fastf_t delta;
    int useful = 0;

    if (((struct plot_vars *)dmp->dm_vars.priv_vars)->floating) {
	rt_vlist_to_uplot(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, &vp->l);

	return TCL_OK;
    }

    /* delta is used in clipping to insure clipped endpoint is
     * slightly in front of eye plane (perspective mode only).  This
     * value is a SWAG that seems to work OK.
     */
    delta = plotmat[15]*0.0001;
    if (delta < 0.0)
	delta = -delta;
    if (delta < SQRT_SMALL_FASTF)
	delta = SQRT_SMALL_FASTF;

    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	int i;
	int nused = tvp->nused;
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    static vect_t start, fin;
	    switch (*cmd) {
		case BN_VLIST_POLY_START:
		case BN_VLIST_POLY_VERTNORM:
		    continue;
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_LINE_MOVE:
		    /* Move, not draw */
		    if (dmp->dm_perspective > 0) {
			/* cannot apply perspective transformation to
			 * points behind eye plane!!!!
			 */
			dist = VDOT(*pt, &plotmat[12]) + plotmat[15];
			if (dist <= 0.0) {
			    pt_prev = pt;
			    dist_prev = dist;
			    continue;
			} else {
			    MAT4X3PNT(last, plotmat, *pt);
			    dist_prev = dist;
			    pt_prev = pt;
			}
		    } else
			MAT4X3PNT(last, plotmat, *pt);
		    continue;
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_POLY_END:
		case BN_VLIST_LINE_DRAW:
		    /* draw */
		    if (dmp->dm_perspective > 0) {
			/* cannot apply perspective transformation to
			 * points behind eye plane!!!!
			 */
			dist = VDOT(*pt, &plotmat[12]) + plotmat[15];
			if (dist <= 0.0) {
			    if (dist_prev <= 0.0) {
				/* nothing to plot */
				dist_prev = dist;
				pt_prev = pt;
				continue;
			    } else {
				fastf_t alpha;
				vect_t diff;
				point_t tmp_pt;

				/* clip this end */
				VSUB2(diff, *pt, *pt_prev);
				alpha = (dist_prev - delta) / (dist_prev - dist);
				VJOIN1(tmp_pt, *pt_prev, alpha, diff);
				MAT4X3PNT(fin, plotmat, tmp_pt);
			    }
			} else {
			    if (dist_prev <= 0.0) {
				fastf_t alpha;
				vect_t diff;
				point_t tmp_pt;

				/* clip other end */
				VSUB2(diff, *pt, *pt_prev);
				alpha = (-dist_prev + delta) / (dist - dist_prev);
				VJOIN1(tmp_pt, *pt_prev, alpha, diff);
				MAT4X3PNT(last, plotmat, tmp_pt);
				MAT4X3PNT(fin, plotmat, *pt);
			    } else {
				MAT4X3PNT(fin, plotmat, *pt);
			    }
			}
		    } else
			MAT4X3PNT(fin, plotmat, *pt);
		    VMOVE(start, last);
		    VMOVE(last, fin);
		    break;
	    }
	    if (vclip(start, fin, dmp->dm_clipmin, dmp->dm_clipmax) == 0)
		continue;

	    if (((struct plot_vars *)dmp->dm_vars.priv_vars)->is_3D)
		pl_3line(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp,
			 (int)(start[X] * 2047),
			 (int)(start[Y] * 2047),
			 (int)(start[Z] * 2047),
			 (int)(fin[X] * 2047),
			 (int)(fin[Y] * 2047),
			 (int)(fin[Z] * 2047));
	    else
		pl_line(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp,
			(int)(start[X] * 2047),
			(int)(start[Y] * 2047),
			(int)(fin[X] * 2047),
			(int)(fin[Y] * 2047));

	    useful = 1;
	}
    }

    if (useful)
	return TCL_OK;

    return TCL_ERROR;
}


/**
 * P L O T _ D R A W
 */
HIDDEN int
plot_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), genptr_t *data)
{
    struct bn_vlist *vp;
    if (!callback_function) {
	if (data) {
	    vp = (struct bn_vlist *)data;
	    plot_drawVList(dmp, vp);
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


/**
 * P L O T _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation (ie,
 * not scaled, rotated, displaced, etc).  Turns off windowing.
 */
HIDDEN int
plot_normal(struct dm *dmp)
{
    if (!dmp)
	return TCL_ERROR;

    return TCL_OK;
}


/**
 * P L O T _ P U T S
 *
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
HIDDEN int
plot_drawString2D(struct dm *dmp, char *str, fastf_t x, fastf_t y, int size, int UNUSED(use_aspect))
{
    int sx, sy;

    if (!dmp || !str || size < 0) {
	return TCL_ERROR;
    }

    sx = x * 2047;
    sy = y + 2047;
    pl_move(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, sx, sy);
    pl_label(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, str);

    return TCL_OK;
}


/**
 * P L O T _ 2 D _ G O T O
 */
HIDDEN int
plot_drawLine2D(struct dm *dmp, fastf_t xpos1, fastf_t ypos1, fastf_t xpos2, fastf_t ypos2)
{
    int sx1, sy1;
    int sx2, sy2;

    sx1 = xpos1 * 2047;
    sx2 = xpos2 * 2047;
    sy1 = ypos1 + 2047;
    sy2 = ypos2 + 2047;
    pl_move(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, sx1, sy1);
    pl_cont(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, sx2, sy2);

    return TCL_OK;
}


HIDDEN int
plot_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2)
{
    if (!dmp)
	return TCL_ERROR;

    if (bn_pt3_pt3_equal(pt1, pt2, NULL)) {
	/* nothing to do for a singular point */
	return TCL_OK;
    }

    return TCL_OK;
}


HIDDEN int
plot_drawLines3D(struct dm *dmp, int npoints, point_t *points, int UNUSED(sflag))
{
    if (!dmp || npoints < 0 || !points)
	return TCL_ERROR;

    return TCL_OK;
}


HIDDEN int
plot_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y)
{
    return plot_drawLine2D(dmp, x, y, x, y);
}


HIDDEN int
plot_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    if (!dmp) {
	bu_log("WARNING: NULL display (r/g/b => %d/%d/%d; strict => %d; transparency => %f)\n", r, g, b, strict, transparency);
	return TCL_ERROR;
    }

    pl_color(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, (int)r, (int)g, (int)b);
    return TCL_OK;
}
HIDDEN int
plot_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    if (!dmp) {
	bu_log("WARNING: Null display (r/g/b==%d/%d/%d)\n", r, g, b);
	return TCL_ERROR;
    }

    return TCL_OK;
}


HIDDEN int
plot_setLineAttr(struct dm *dmp, int width, int style)
{
    dmp->dm_lineWidth = width;
    dmp->dm_lineStyle = style;

    if (style == DM_DASHED_LINE)
	pl_linmod(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, "dotdashed");
    else
	pl_linmod(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp, "solid");

    return TCL_OK;
}


HIDDEN int
plot_debug(struct dm *dmp, int lvl)
{
    Tcl_Obj *obj;

    obj = Tcl_GetObjResult(dmp->dm_interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    dmp->dm_debugLevel = lvl;
    (void)fflush(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp);
    Tcl_AppendStringsToObj(obj, "flushed\n", (char *)NULL);

    Tcl_SetObjResult(dmp->dm_interp, obj);
    return TCL_OK;
}


HIDDEN int
plot_setWinBounds(struct dm *dmp, fastf_t *w)
{
    /* Compute the clipping bounds */
    dmp->dm_clipmin[0] = w[0] / 2048.;
    dmp->dm_clipmax[0] = w[1] / 2047.;
    dmp->dm_clipmin[1] = w[2] / 2048.;
    dmp->dm_clipmax[1] = w[3] / 2047.;

    if (dmp->dm_zclip) {
	dmp->dm_clipmin[2] = w[4] / 2048.;
	dmp->dm_clipmax[2] = w[5] / 2047.;
    } else {
	dmp->dm_clipmin[2] = -1.0e20;
	dmp->dm_clipmax[2] = 1.0e20;
    }

    return TCL_OK;
}


struct dm dm_plot = {
    plot_close,
    plot_drawBegin,
    plot_drawEnd,
    plot_normal,
    plot_loadMatrix,
    plot_drawString2D,
    plot_drawLine2D,
    plot_drawLine3D,
    plot_drawLines3D,
    plot_drawPoint2D,
    plot_drawVList,
    plot_drawVList,
    plot_draw,
    plot_setFGColor,
    plot_setBGColor,
    plot_setLineAttr,
    Nu_int0,
    plot_setWinBounds,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    plot_debug,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0, /* display to image function */
    Nu_void,
    0,
    0,				/* no displaylist */
    0,				/* no stereo */
    PLOTBOUND,			/* zoom-in limit */
    1,				/* bound flag */
    "plot",
    "Screen to UNIX-Plot",
    DM_TYPE_PLOT,
    0,
    0,
    0,
    0, /* bytes per pixel */
    0, /* bits per channel */
    0,
    0,
    1.0, /* aspect ratio */
    NULL,
    {0, 0},
    BU_VLS_INIT_ZERO,		/* bu_vls path name*/
    BU_VLS_INIT_ZERO,		/* bu_vls full name drawing window */
    BU_VLS_INIT_ZERO,		/* bu_vls short name drawing window */
    {0, 0, 0},			/* bg color */
    {0, 0, 0},			/* fg color */
    VINIT_ZERO,			/* clipmin */
    VINIT_ZERO,			/* clipmax */
    0,				/* no debugging */
    0,				/* no perspective */
    0,				/* no lighting */
    0,				/* no transparency */
    0,				/* depth buffer is not writable */
    0,				/* no zbuffer */
    0,				/* no zclipping */
    1,                          /* clear back buffer after drawing and swap */
    0,                          /* not overriding the auto font size */
    NULL			/* Tcl interpreter */
};


/*
 * P L O T _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
plot_open(Tcl_Interp *interp, int argc, const char *argv[])
{
    static int count = 0;
    struct dm *dmp;
    Tcl_Obj *obj;

    BU_GETSTRUCT(dmp, dm);
    if (dmp == DM_NULL)
	return DM_NULL;

    *dmp = dm_plot; /* struct copy */
    dmp->dm_interp = interp;

    dmp->dm_vars.priv_vars = (genptr_t)bu_calloc(1, sizeof(struct plot_vars), "plot_open: plot_vars");
    BU_GETSTRUCT(dmp->dm_vars.priv_vars, plot_vars);
    if (dmp->dm_vars.priv_vars == (genptr_t)NULL) {
	bu_free((genptr_t)dmp, "plot_open: dmp");
	return DM_NULL;
    }

    obj = Tcl_GetObjResult(interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    bu_vls_init(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls);
    bu_vls_init(&dmp->dm_pathName);
    bu_vls_init(&dmp->dm_tkName);
    bu_vls_printf(&dmp->dm_pathName, ".dm_plot%d", count++);
    bu_vls_printf(&dmp->dm_tkName, "dm_plot%d", count++);

    /* skip first argument */
    --argc; ++argv;

    /* Process any options */
    ((struct plot_vars *)dmp->dm_vars.priv_vars)->is_3D = 1;          /* 3-D w/color, by default */
    while (argv[0] != (char *)0 && argv[0][0] == '-') {
	switch (argv[0][1]) {
	    case '3':
		break;
	    case '2':
		((struct plot_vars *)dmp->dm_vars.priv_vars)->is_3D = 0;		/* 2-D, for portability */
		break;
	    case 'g':
		((struct plot_vars *)dmp->dm_vars.priv_vars)->grid = 1;
		break;
	    case 'f':
		((struct plot_vars *)dmp->dm_vars.priv_vars)->floating = 1;
		break;
	    case 'z':
	    case 'Z':
		/* Enable Z clipping */
		Tcl_AppendStringsToObj(obj, "Clipped in Z to viewing cube\n", (char *)NULL);

		dmp->dm_zclip = 1;
		break;
	    default:
		Tcl_AppendStringsToObj(obj, "bad PLOT option ", argv[0], "\n", (char *)NULL);
		(void)plot_close(dmp);

		Tcl_SetObjResult(interp, obj);
		return DM_NULL;
	}
	argv++;
    }
    if (argv[0] == (char *)0) {
	Tcl_AppendStringsToObj(obj, "no filename or filter specified\n", (char *)NULL);
	(void)plot_close(dmp);

	Tcl_SetObjResult(interp, obj);
	return DM_NULL;
    }

    if (argv[0][0] == '|') {
	bu_vls_strcpy(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls, &argv[0][1]);
	while ((++argv)[0] != (char *)0) {
	    bu_vls_strcat(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls, " ");
	    bu_vls_strcat(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls, argv[0]);
	}

	((struct plot_vars *)dmp->dm_vars.priv_vars)->is_pipe = 1;
    } else {
	bu_vls_strcpy(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls, argv[0]);
    }

    if (((struct plot_vars *)dmp->dm_vars.priv_vars)->is_pipe) {
	if ((((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp =
	     popen(bu_vls_addr(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls), "w")) == NULL) {
	    perror(bu_vls_addr(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls));
	    (void)plot_close(dmp);
	    Tcl_SetObjResult(interp, obj);
	    return DM_NULL;
	}

	Tcl_AppendStringsToObj(obj, "piped to ",
			       bu_vls_addr(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls),
			       "\n", (char *)NULL);
    } else {
	if ((((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp =
	     fopen(bu_vls_addr(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls), "wb")) == NULL) {
	    perror(bu_vls_addr(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls));
	    (void)plot_close(dmp);
	    Tcl_SetObjResult(interp, obj);
	    return DM_NULL;
	}

	Tcl_AppendStringsToObj(obj, "plot stored in ",
			       bu_vls_addr(&((struct plot_vars *)dmp->dm_vars.priv_vars)->vls),
			       "\n", (char *)NULL);
    }

    setbuf(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp,
	   ((struct plot_vars *)dmp->dm_vars.priv_vars)->ttybuf);

    if (((struct plot_vars *)dmp->dm_vars.priv_vars)->is_3D)
	pl_3space(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp,
		  -2048, -2048, -2048, 2048, 2048, 2048);
    else
	pl_space(((struct plot_vars *)dmp->dm_vars.priv_vars)->up_fp,
		 -2048, -2048, 2048, 2048);

    MAT_IDN(plotmat);

    Tcl_SetObjResult(interp, obj);
    return dmp;
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

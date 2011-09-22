/*                         D M - P S . C
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
/** @file libdm/dm-ps.c
 *
 * A useful hack to allow GED to generate
 * PostScript files that not only contain the drawn objects, but
 * also contain the faceplate display as well.
 * Mostly, used for making viewgraphs and photographs
 * of an editing session.
 *
 */

#include "common.h"
#include "bio.h"

#include <stdio.h>
#include <string.h>

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>		/* for struct timeval */
#endif

#include "tcl.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "mater.h"
#include "raytrace.h"
#include "dm.h"
#include "dm-ps.h"
#include "solid.h"

#define EPSILON 0.0001

/* Display Manager package interface */

#define PLOTBOUND 1000.0	/* Max magnification in Rot matrix */

char ps_usage[] = "Usage: ps [-f font] [-t title] [-c creator] [-s size in inches]\
 [-l linewidth] file";

struct ps_vars head_ps_vars;
static mat_t psmat;


/*
 * P S _ C L O S E
 *
 * Gracefully release the display.
 */
HIDDEN int
ps_close(struct dm *dmp)
{
    if (!((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp)
	return TCL_ERROR;

    fputs("%end(plot)\n", ((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp);
    (void)fclose(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp);

    bu_vls_free(&dmp->dm_pathName);
    bu_vls_free(&dmp->dm_tkName);
    bu_vls_free(&((struct ps_vars *)dmp->dm_vars.priv_vars)->fname);
    bu_vls_free(&((struct ps_vars *)dmp->dm_vars.priv_vars)->font);
    bu_vls_free(&((struct ps_vars *)dmp->dm_vars.priv_vars)->title);
    bu_vls_free(&((struct ps_vars *)dmp->dm_vars.priv_vars)->creator);
    bu_free((genptr_t)dmp->dm_vars.priv_vars, "ps_close: ps_vars");
    bu_free((genptr_t)dmp, "ps_close: dmp");

    return TCL_OK;
}


/*
 * P S _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
HIDDEN int
ps_drawBegin(struct dm *dmp)
{
    if (!dmp)
	return TCL_ERROR;

    return TCL_OK;
}


/*
 * P S _ E P I L O G
 */
HIDDEN int
ps_drawEnd(struct dm *dmp)
{
    if (!dmp)
	return TCL_ERROR;

    if (!((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp)
	return TCL_ERROR;

    fputs("% showpage	% uncomment to use raw file\n",
	  ((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp);
    (void)fflush(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp);

    return TCL_OK;
}


/*
 * P S _ N E W R O T
 *
 * Load a new transformation matrix.  This will be followed by
 * many calls to ps_draw().
 */
HIDDEN int
ps_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye)
{
    Tcl_Obj *obj;

    obj = Tcl_GetObjResult(dmp->dm_interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    if (((struct ps_vars *)dmp->dm_vars.priv_vars)->debug) {
	struct bu_vls tmp_vls;

	Tcl_AppendStringsToObj(obj, "ps_loadMatrix()\n", (char *)NULL);

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

    MAT_COPY(psmat, mat);

    Tcl_SetObjResult(dmp->dm_interp, obj);
    return TCL_OK;
}


/*
 * P S _ D R A W V L I S T
 */
/* ARGSUSED */
HIDDEN int
ps_drawVList(struct dm *dmp, struct bn_vlist *vp)
{
    static vect_t last;
    struct bn_vlist *tvp;
    point_t *pt_prev=NULL;
    fastf_t dist_prev=1.0;
    fastf_t dist;
    fastf_t delta;
    int useful = 0;

    if (!((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp)
	return TCL_ERROR;

    /* delta is used in clipping to insure clipped endpoint is slightly
     * in front of eye plane (perspective mode only).
     * This value is a SWAG that seems to work OK.
     */
    delta = psmat[15]*0.0001;
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
			dist = VDOT(*pt, &psmat[12]) + psmat[15];
			if (dist <= 0.0) {
			    pt_prev = pt;
			    dist_prev = dist;
			    continue;
			} else {
			    MAT4X3PNT(last, psmat, *pt);
			    dist_prev = dist;
			    pt_prev = pt;
			}
		    } else
			MAT4X3PNT(last, psmat, *pt);
		    continue;
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_POLY_END:
		case BN_VLIST_LINE_DRAW:
		    /* draw */
		    if (dmp->dm_perspective > 0) {
			/* cannot apply perspective transformation to
			 * points behind eye plane!!!!
			 */
			dist = VDOT(*pt, &psmat[12]) + psmat[15];
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
				MAT4X3PNT(fin, psmat, tmp_pt);
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
				MAT4X3PNT(last, psmat, tmp_pt);
				MAT4X3PNT(fin, psmat, *pt);
			    } else {
				MAT4X3PNT(fin, psmat, *pt);
			    }
			}
		    } else
			MAT4X3PNT(fin, psmat, *pt);
		    VMOVE(start, last);
		    VMOVE(last, fin);
		    break;
	    }

	    if (vclip(start, fin, dmp->dm_clipmin,
		      dmp->dm_clipmax) == 0)
		continue;

	    fprintf(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp,
		    "newpath %d %d moveto %d %d lineto stroke\n",
		    GED_TO_PS(start[0] * 2047),
		    GED_TO_PS(start[1] * 2047),
		    GED_TO_PS(fin[0] * 2047),
		    GED_TO_PS(fin[1] * 2047));
	    useful = 1;
	}
    }

    if (useful)
	return TCL_OK;

    return TCL_ERROR;
}


/*
 * P S _ D R A W
 */
/* ARGSUSED */
HIDDEN int
ps_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), genptr_t *data)
{
    struct bn_vlist *vp;
    if (!callback_function) {
        if (data) {
            vp = (struct bn_vlist *)data;
	    ps_drawVList(dmp, vp);
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
 * P S _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
HIDDEN int
ps_normal(struct dm *dmp)
{
    if (!dmp)
	return TCL_ERROR;

    return TCL_OK;
}


/*
 * P S _ D R A W S T R I N G 2 D
 *
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
/* ARGSUSED */
HIDDEN int
ps_drawString2D(struct dm *dmp, char *str, fastf_t x, fastf_t y, int size, int UNUSED(use_aspect))
{
    int sx, sy;

    if (!((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp)
	return TCL_ERROR;

    switch (size) {
	default:
	    /* Smallest */
	    fprintf(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp, "DFntS ");
	    break;
	case 1:
	    fprintf(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp, "DFntM ");
	    break;
	case 2:
	    fprintf(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp, "DFntL ");
	    break;
	case 3:
	    /* Largest */
	    fprintf(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp, "FntH ");
	    break;
    }

    sx = x * 2047.0 + 2048;
    sy = y * 2047.0 + 2048;
    fprintf(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp,
	    "(%s) %d %d moveto show\n", str, sx, sy);

    return TCL_OK;
}


/*
 * P S _ D R A W L I N E 2 D
 *
 */
HIDDEN int
ps_drawLine2D(struct dm *dmp, fastf_t xpos1, fastf_t ypos1, fastf_t xpos2, fastf_t ypos2)
{
    int sx1, sy1;
    int sx2, sy2;

    if (!((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp)
	return TCL_ERROR;

    sx1 = xpos1 * 2047.0 + 2048;
    sx2 = xpos2 * 2047.0 + 2048;
    sy1 = ypos1 * 2047.0 + 2048;
    sy2 = ypos2 * 2047.0 + 2048;

    fprintf(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp,
	    "newpath %d %d moveto %d %d lineto stroke\n",
	    sx1, sy1, sx2, sy2);

    return TCL_OK;
}


HIDDEN int
ps_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2)
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
ps_drawLines3D(struct dm *dmp, int npoints, point_t *points)
{
    if (!dmp || npoints < 0 || !points)
	return TCL_ERROR;

    return TCL_OK;
}


HIDDEN int
ps_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y)
{
    return ps_drawLine2D(dmp, x, y, x, y);
}


HIDDEN int
ps_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    if (!dmp) {
	bu_log("WARNING: NULL display (r/g/b => %d/%d/%d; strict => %d; transparency => %f)\n", r, g, b, strict, transparency);
	return TCL_ERROR;
    }

    return TCL_OK;
}


HIDDEN int
ps_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    if (!dmp) {
	bu_log("WARNING: Null display (r/g/b==%d/%d/%d)\n", r, g, b);
	return TCL_ERROR;
    }

    return TCL_OK;
}


HIDDEN int
ps_setLineAttr(struct dm *dmp, int width, int style)
{
    dmp->dm_lineWidth = width;
    dmp->dm_lineStyle = style;

    if (style == DM_DASHED_LINE)
	fprintf(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp, "DDV "); /* Dot-dashed vectors */
    else
	fprintf(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp, "NV "); /* Normal vectors */

    return TCL_OK;
}


/* ARGSUSED */
HIDDEN int
ps_debug(struct dm *dmp, int lvl)
{
    dmp->dm_debugLevel = lvl;
    return TCL_OK;
}


HIDDEN int
ps_setWinBounds(struct dm *dmp, fastf_t *w)
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


struct dm dm_ps = {
    ps_close,
    ps_drawBegin,
    ps_drawEnd,
    ps_normal,
    ps_loadMatrix,
    ps_drawString2D,
    ps_drawLine2D,
    ps_drawLine3D,
    ps_drawLines3D,
    ps_drawPoint2D,
    ps_drawVList,
    ps_drawVList,
    ps_draw,
    ps_setFGColor,
    ps_setBGColor,
    ps_setLineAttr,
    Nu_int0,
    ps_setWinBounds,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    ps_debug,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0,
    Nu_int0, /* display to image function */
    Nu_void,
    0,
    0,				/* no displaylist */
    0,                            /* no stereo */
    PLOTBOUND,			/* zoom-in limit */
    1,				/* bound flag */
    "ps",
    "Screen to PostScript",
    DM_TYPE_PS,
    0,
    0,
    0,
    0, /* bytes per pixel */
    0, /* bits per channel */
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
    {0.0, 0.0, 0.0},		/* clipmin */
    {0.0, 0.0, 0.0},		/* clipmax */
    0,				/* no debugging */
    0,				/* no perspective */
    0,				/* no lighting */
    0,				/* no transparency */
    0,				/* depth buffer is not writable */
    0,				/* no zbuffer */
    0,				/* no zclipping */
    1,                          /* clear back buffer after drawing and swap */
    0,                          /* not overriding the auto font size */
    0				/* Tcl interpreter */
};


/*
 * P S _ O P E N
 *
 * Open the output file, and output the PostScript prolog.
 *
 */
struct dm *
ps_open(Tcl_Interp *interp, int argc, const char *argv[])
{
    static int count = 0;
    struct dm *dmp;
    Tcl_Obj *obj;

    BU_GETSTRUCT(dmp, dm);
    if (dmp == DM_NULL)
	return DM_NULL;

    *dmp = dm_ps;  /* struct copy */
    dmp->dm_interp = interp;

    dmp->dm_vars.priv_vars = (genptr_t)bu_calloc(1, sizeof(struct ps_vars), "ps_open: ps_vars");
    if (dmp->dm_vars.priv_vars == (genptr_t)NULL) {
	bu_free((genptr_t)dmp, "ps_open: dmp");
	return DM_NULL;
    }

    obj = Tcl_GetObjResult(interp);
    if (Tcl_IsShared(obj))
	obj = Tcl_DuplicateObj(obj);

    bu_vls_init(&dmp->dm_pathName);
    bu_vls_init(&dmp->dm_tkName);
    bu_vls_printf(&dmp->dm_pathName, ".dm_ps%d", count++);
    bu_vls_printf(&dmp->dm_tkName, "dm_ps%d", count++);

    bu_vls_init(&((struct ps_vars *)dmp->dm_vars.priv_vars)->fname);
    bu_vls_init(&((struct ps_vars *)dmp->dm_vars.priv_vars)->font);
    bu_vls_init(&((struct ps_vars *)dmp->dm_vars.priv_vars)->title);
    bu_vls_init(&((struct ps_vars *)dmp->dm_vars.priv_vars)->creator);

    /* set defaults */
    bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars.priv_vars)->font, "Courier");
    bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars.priv_vars)->title, "No Title");
    bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars.priv_vars)->creator, "LIBDM dm-ps");
    ((struct ps_vars *)dmp->dm_vars.priv_vars)->scale = 0.0791;
    ((struct ps_vars *)dmp->dm_vars.priv_vars)->linewidth = 4;
    ((struct ps_vars *)dmp->dm_vars.priv_vars)->zclip = 0;

    /* skip first argument */
    --argc; ++argv;

    /* Process any options */
    while (argv[0] != (char *)0 && argv[0][0] == '-') {
	switch (argv[0][1]) {
	    case 'f':               /* font */
		if (argv[0][2] != '\0')
		    bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars.priv_vars)->font, &argv[0][2]);
		else {
		    argv++;
		    if (argv[0] == (char *)0 || argv[0][0] == '-') {
			Tcl_AppendStringsToObj(obj, ps_usage, (char *)0);
			(void)ps_close(dmp);

			Tcl_SetObjResult(interp, obj);
			return DM_NULL;
		    } else
			bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars.priv_vars)->font, &argv[0][0]);
		}
		break;
	    case 't':               /* title */
		if (argv[0][2] != '\0')
		    bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars.priv_vars)->title, &argv[0][2]);
		else {
		    argv++;
		    if (argv[0] == (char *)0 || argv[0][0] == '-') {
			Tcl_AppendStringsToObj(obj, ps_usage, (char *)0);
			(void)ps_close(dmp);

			Tcl_SetObjResult(interp, obj);
			return DM_NULL;
		    } else
			bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars.priv_vars)->title, &argv[0][0]);
		}
		break;
	    case 'c':               /* creator */
		if (argv[0][2] != '\0')
		    bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars.priv_vars)->creator, &argv[0][2]);
		else {
		    argv++;
		    if (argv[0] == (char *)0 || argv[0][0] == '-') {
			Tcl_AppendStringsToObj(obj, ps_usage, (char *)0);
			(void)ps_close(dmp);

			Tcl_SetObjResult(interp, obj);
			return DM_NULL;
		    } else
			bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars.priv_vars)->creator, &argv[0][0]);
		}
		break;
	    case 's':               /* size in inches */
		{
		    fastf_t size;

		    if (argv[0][2] != '\0')
			sscanf(&argv[0][2], "%lf", &size);
		    else {
			argv++;
			if (argv[0] == (char *)0 || argv[0][0] == '-') {
			    Tcl_AppendStringsToObj(obj, ps_usage, (char *)0);
			    (void)ps_close(dmp);

			    Tcl_SetObjResult(interp, obj);
			    return DM_NULL;
			} else
			    sscanf(&argv[0][0], "%lf", &size);
		    }

		    ((struct ps_vars *)dmp->dm_vars.priv_vars)->scale = size * 0.017578125;
		}
		break;
	    case 'l':               /* line width */
		if (argv[0][2] != '\0')
		    sscanf(&argv[0][2], "%d", &((struct ps_vars *)dmp->dm_vars.priv_vars)->linewidth);
		else {
		    argv++;
		    if (argv[0] == (char *)0 || argv[0][0] == '-') {
			Tcl_AppendStringsToObj(obj, ps_usage, (char *)0);
			(void)ps_close(dmp);

			Tcl_SetObjResult(interp, obj);
			return DM_NULL;
		    } else
			sscanf(&argv[0][0], "%d", &((struct ps_vars *)dmp->dm_vars.priv_vars)->linewidth);
		}
		break;
	    case 'z':
		dmp->dm_zclip = 1;
		break;
	    default:
		Tcl_AppendStringsToObj(obj, ps_usage, (char *)0);
		(void)ps_close(dmp);

		Tcl_SetObjResult(interp, obj);
		return DM_NULL;
	}
	argv++;
    }

    if (argv[0] == (char *)0) {
	Tcl_AppendStringsToObj(obj, "no filename specified\n", (char *)NULL);
	(void)ps_close(dmp);

	Tcl_SetObjResult(interp, obj);
	return DM_NULL;
    }

    bu_vls_strcpy(&((struct ps_vars *)dmp->dm_vars.priv_vars)->fname, argv[0]);

    if ((((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp =
	 fopen(bu_vls_addr(&((struct ps_vars *)dmp->dm_vars.priv_vars)->fname), "wb")) == NULL) {
	Tcl_AppendStringsToObj(obj, "f_ps: Error opening file - ",
			       ((struct ps_vars *)dmp->dm_vars.priv_vars)->fname,
			       "\n", (char *)NULL);
	(void)ps_close(dmp);

	Tcl_SetObjResult(interp, obj);
	return DM_NULL;
    }

    setbuf(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp,
	   ((struct ps_vars *)dmp->dm_vars.priv_vars)->ttybuf);
    fprintf(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp, "%%!PS-Adobe-1.0\n\
%%begin(plot)\n\
%%%%DocumentFonts:  %s\n",
	    bu_vls_addr(&((struct ps_vars *)dmp->dm_vars.priv_vars)->font));

    fprintf(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp, "%%%%Title: %s\n",
	    bu_vls_addr(&((struct ps_vars *)dmp->dm_vars.priv_vars)->title));

    fprintf(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp, "\
%%%%Creator: %s\n\
%%%%BoundingBox: 0 0 324 324	%% 4.5in square, for TeX\n\
%%%%EndComments\n\
\n",
	    bu_vls_addr(&((struct ps_vars *)dmp->dm_vars.priv_vars)->creator));

    fprintf(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp, "\
%d setlinewidth\n\
\n\
%% Sizes, made functions to avoid scaling if not needed\n\
/FntH /%s findfont 80 scalefont def\n\
/DFntL { /FntL /%s findfont 73.4 scalefont def } def\n\
/DFntM { /FntM /%s findfont 50.2 scalefont def } def\n\
/DFntS { /FntS /%s findfont 44 scalefont def } def\n\
",
	    ((struct ps_vars *)dmp->dm_vars.priv_vars)->linewidth,
	    bu_vls_addr(&((struct ps_vars *)dmp->dm_vars.priv_vars)->font),
	    bu_vls_addr(&((struct ps_vars *)dmp->dm_vars.priv_vars)->font),
	    bu_vls_addr(&((struct ps_vars *)dmp->dm_vars.priv_vars)->font),
	    bu_vls_addr(&((struct ps_vars *)dmp->dm_vars.priv_vars)->font));

    fprintf(((struct ps_vars *)dmp->dm_vars.priv_vars)->ps_fp, "\
\n\
%% line styles\n\
/NV { [] 0 setdash } def	%% normal vectors\n\
/DV { [8] 0 setdash } def	%% dotted vectors\n\
/DDV { [8 8 32 8] 0 setdash } def	%% dot-dash vectors\n\
/SDV { [32 8] 0 setdash } def	%% short-dash vectors\n\
/LDV { [64 8] 0 setdash } def	%% long-dash vectors\n\
\n\
/NEWPG {\n\
	%f %f scale	%% 0-4096 to 324 units (4.5 inches)\n\
} def\n\
\n\
FntH setfont\n\
NEWPG\n\
",
	    ((struct ps_vars *)dmp->dm_vars.priv_vars)->scale,
	    ((struct ps_vars *)dmp->dm_vars.priv_vars)->scale);

    MAT_IDN(psmat);

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

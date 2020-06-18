/*                          D M - T K . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2020 United States Government as represented by
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
 * A Display Manager that should work wherever tk does.
 *
 */

#include "common.h"

#ifdef DM_TK

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include <tcl.h>
#include <tk.h>


#include "vmath.h"
#include "bn.h"
#include "dm.h"
#include "dm-tk.h"
#include "dm-Null.h"
#include "dm/dm_xvars.h"
#include "dm_private.h"
#include "rt/solid.h"

#define PLOTBOUND 1000.0        /* Max magnification in Rot matrix */

/**
 * @proc tk_close
 *
 * Gracefully release the display.
 */
HIDDEN int
tk_close(struct dm_internal *dmp)
{
    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy) {
	if (((struct tk_vars *)dmp->dm_vars.priv_vars)->gc)
	    Tk_FreeGC(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		      ((struct tk_vars *)dmp->dm_vars.priv_vars)->gc);

	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin)
	    Tk_DestroyWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);

    }

    bu_vls_free(&dmp->dm_pathName);
    bu_vls_free(&dmp->dm_tkName);
    bu_vls_free(&dmp->dm_dName);
    bu_free(dmp->dm_vars.priv_vars, "tk_close: tk_vars");
    bu_free(dmp->dm_vars.pub_vars, "tk_close: dm_tkvars");
    bu_free(dmp, "tk_close: dmp");

    return BRLCAD_OK;
}


/**
 * @proc tk_drawBegin
 * This white-washes the dm's pixmap with the background color.
 */
HIDDEN int
tk_drawBegin(struct dm_internal *dmp)
{

    if (dmp->dm_debugLevel)
	bu_log("tk_drawBegin()\n");
#if 0
    XGCValues gcv;
    /* clear pixmap */
    gcv.foreground = ((struct tk_vars *)dmp->dm_vars.priv_vars)->bg;
    XChangeGC(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
	      ((struct tk_vars *)dmp->dm_vars.priv_vars)->gc,
	      GCForeground, &gcv);
    XFillRectangle(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		   ((struct tk_vars *)dmp->dm_vars.priv_vars)->pix,
		   ((struct tk_vars *)dmp->dm_vars.priv_vars)->gc, 0,
		   0, dmp->dm_width + 1,
		   dmp->dm_height + 1);

    /* reset foreground */

    gcv.foreground = ((struct tk_vars *)dmp->dm_vars.priv_vars)->fg;
    XChangeGC(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
	      ((struct tk_vars *)dmp->dm_vars.priv_vars)->gc,
	      GCForeground, &gcv);
#endif
    return BRLCAD_OK;
}


/**
 * tk_drawEnd
 * This copies the pixmap into the window.
 */
HIDDEN int
tk_drawEnd(struct dm_internal *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("tk_drawEnd()\n");
#if 0
    XCopyArea(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
	      ((struct tk_vars *)dmp->dm_vars.priv_vars)->pix,
	      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
	      ((struct tk_vars *)dmp->dm_vars.priv_vars)->gc,
	      0, 0, dmp->dm_width,
	      dmp->dm_height, 0, 0);


    /* Prevent lag between events and updates */
    XSync(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy, 0);
#endif
    return BRLCAD_OK;
}


/**
 * @proc tk_loadMatrix
 *
 * Load a new transformation matrix.  This will be followed by
 * many calls to tk_drawVList().
 */
/* ARGSUSED */
HIDDEN int
tk_loadMatrix(struct dm_internal *dmp, fastf_t *mat, int which_eye)
{
    if (dmp->dm_debugLevel) {
	bu_log("tk_loadMatrix()\n");

	bu_log("which eye = %d\t", which_eye);
	bu_log("transformation matrix = \n");

	bu_log("%g %g %g %g\n", mat[0], mat[1], mat[2], mat[3]);
	bu_log("%g %g %g %g\n", mat[4], mat[5], mat[6], mat[7]);
	bu_log("%g %g %g %g\n", mat[8], mat[9], mat[10], mat[11]);
	bu_log("%g %g %g %g\n", mat[12], mat[13], mat[14], mat[15]);
    }
#if 0
    MAT_COPY(((struct tk_vars *)dmp->dm_vars.priv_vars)->mod_mat, mat);
#endif
    return BRLCAD_OK;
}


/**
 * tk_drawVList
 *
 */

HIDDEN int
tk_drawVList(struct dm_internal *dmp, struct bn_vlist *vp)
{
    if (dmp->dm_debugLevel) {
	bu_log("tk_drawVList()\n");
	bu_log("vp - %p, perspective - %d\n", (void *)vp, dmp->dm_perspective);
    }

#if 0
    static vect_t spnt, lpnt, pnt;
    struct bn_vlist *tvp;
    XSegment segbuf[1024];		/* XDrawSegments list */
    XSegment *segp;			/* current segment */
    int nseg;		        /* number of segments */
    fastf_t delta;
    point_t *pt_prev = NULL;
    point_t tlate;
    fastf_t dist_prev=1.0;
    static int nvectors = 0;

    struct tk_vars *privars = (struct tk_vars *)dmp->dm_vars.priv_vars;


    /* delta is used in clipping to insure clipped endpoint is slightly
     * in front of eye plane (perspective mode only).
     * This value is a SWAG that seems to work OK.
     */
    delta = (privars->xmat)[15]*0.0001;
    if (delta < 0.0)
	delta = -delta;
    if (delta < SQRT_SMALL_FASTF)
	delta = SQRT_SMALL_FASTF;

    nseg = 0;
    segp = segbuf;
    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	int i;
	int nused = tvp->nused;
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	fastf_t dist;

	/* Viewing region is from -1.0 to +1.0 */
	/* 2^31 ~= 2e9 -- dynamic range of a long int */
	/* 2^(31-11) = 2^20 ~= 1e6 */
	/* Integerize and let the X server do the clipping */
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    switch (*cmd) {
		case BN_VLIST_POLY_START:
		case BN_VLIST_POLY_VERTNORM:
		case BN_VLIST_TRI_START:
		case BN_VLIST_TRI_VERTNORM:
		    continue;
		case BN_VLIST_MODEL_MAT:
		    privars->xmat = &(privars->mod_mat[0]);
		    continue;
		case BN_VLIST_DISPLAY_MAT:
		    MAT4X3PNT(tlate, privars->mod_mat, *pt);
		    privars->disp_mat[3] = tlate[0];
		    privars->disp_mat[7] = tlate[1];
		    privars->disp_mat[11] = tlate[2];
		    privars->xmat = &(privars->disp_mat[0]);
		    continue;
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_LINE_MOVE:
		case BN_VLIST_TRI_MOVE:
		    /* Move, not draw */
		    if (dmp->dm_debugLevel > 2) {
			bu_log("before transformation:\n");
			bu_log("pt - %lf %lf %lf\n", V3ARGS(*pt));
		    }

		    if (dmp->dm_perspective > 0) {
			/* cannot apply perspective transformation to
			 * points behind eye plane!!!!
			 */
			dist = VDOT(*pt, &(privars->xmat)[12]) + privars->xmat[15];
			if (dist <= 0.0) {
			    pt_prev = pt;
			    dist_prev = dist;
			    continue;
			} else {
			    MAT4X3PNT(lpnt, privars->xmat, *pt);
			    dist_prev = dist;
			    pt_prev = pt;
			}
		    } else {
			    MAT4X3PNT(lpnt, privars->xmat, *pt);
		    }

		    lpnt[0] *= 2047;
		    lpnt[1] *= 2047 * dmp->dm_aspect;
		    lpnt[2] *= 2047;
		    continue;
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_POLY_END:
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_TRI_DRAW:
		case BN_VLIST_TRI_END:
		    /* draw */
		    if (dmp->dm_debugLevel > 2) {
			bu_log("before transformation:\n");
			bu_log("pt - %lf %lf %lf\n", V3ARGS(*pt));
		    }

		    if (dmp->dm_perspective > 0) {
			/* cannot apply perspective transformation to
			 * points behind eye plane!!!!
			 */
			dist = VDOT(*pt, &(privars->xmat)[12]) + privars->xmat[15];
			if (dmp->dm_debugLevel > 2)
			    bu_log("dist=%g, dist_prev=%g\n", dist, dist_prev);
			if (dist <= 0.0) {
			    if (dist_prev <= 0.0) {
				/* nothing to plot */
				dist_prev = dist;
				pt_prev = pt;
				continue;
			    } else {
				if (pt_prev) {
				fastf_t alpha;
				vect_t diff;
				point_t tmp_pt;

				/* clip this end */
				VSUB2(diff, *pt, *pt_prev);
				alpha = (dist_prev - delta) / (dist_prev - dist);
				VJOIN1(tmp_pt, *pt_prev, alpha, diff);
				MAT4X3PNT(pnt, privars->xmat, tmp_pt);
				}
			    }
			} else {
			    if (dist_prev <= 0.0) {
				if (pt_prev) {
				fastf_t alpha;
				vect_t diff;
				point_t tmp_pt;

				/* clip other end */
				VSUB2(diff, *pt, *pt_prev);
				alpha = (-dist_prev + delta) / (dist - dist_prev);
				VJOIN1(tmp_pt, *pt_prev, alpha, diff);
				MAT4X3PNT(pnt, privars->xmat, tmp_pt);
				lpnt[0] *= 2047;
				lpnt[1] *= 2047 * dmp->dm_aspect;
				lpnt[2] *= 2047;
				MAT4X3PNT(pnt, privars->xmat, *pt);
				}
			    } else {
				MAT4X3PNT(pnt, privars->xmat, *pt);
			    }
			}
			dist_prev = dist;
		    } else {
			MAT4X3PNT(pnt, privars->xmat, *pt);
		    }

		    pnt[0] *= 2047;
		    pnt[1] *= 2047 * dmp->dm_aspect;
		    pnt[2] *= 2047;

		    /* save pnt --- it might get changed by clip() */
		    VMOVE(spnt, pnt);
		    pt_prev = pt;

		    if (dmp->dm_debugLevel > 2) {
			bu_log("before clipping:\n");
			bu_log("clipmin - %lf %lf %lf\n",
			       dmp->dm_clipmin[X],
			       dmp->dm_clipmin[Y],
			       dmp->dm_clipmin[Z]);
			bu_log("clipmax - %lf %lf %lf\n",
			       dmp->dm_clipmax[X],
			       dmp->dm_clipmax[Y],
			       dmp->dm_clipmax[Z]);
			bu_log("pt1 - %lf %lf %lf\n", lpnt[X], lpnt[Y], lpnt[Z]);
			bu_log("pt2 - %lf %lf %lf\n", pnt[X], pnt[Y], pnt[Z]);
		    }

		    if (dmp->dm_zclip) {
			if (vclip(lpnt, pnt,
				  dmp->dm_clipmin,
				  dmp->dm_clipmax) == 0) {
			    VMOVE(lpnt, spnt);
			    continue;
			}
		    } else {
			/* Check to see if lpnt or pnt contain values that exceed
			   the capacity of a short (segbuf is an array of XSegments which
			   contain shorts). If so, do clipping now. Otherwise, let the
			   X server do the clipping */
			if (lpnt[0] < min_short || max_short < lpnt[0] ||
			    lpnt[1] < min_short || max_short < lpnt[1] ||
			    pnt[0] < min_short || max_short < pnt[0] ||
			    pnt[1] < min_short || max_short < pnt[1]) {
			    /* if the entire line segment will not be visible then ignore it */
			    if (clip(&lpnt[0], &lpnt[1], &pnt[0], &pnt[1]) == -1) {
				VMOVE(lpnt, spnt);
				continue;
			    }
			}
		    }

		    if (dmp->dm_debugLevel > 2) {
			bu_log("after clipping:\n");
			bu_log("pt1 - %lf %lf %lf\n", lpnt[X], lpnt[Y], lpnt[Z]);
			bu_log("pt2 - %lf %lf %lf\n", pnt[X], pnt[Y], pnt[Z]);
		    }

		    /* convert to X window coordinates */
		    segp->x1 = (short)GED_TO_Xx(dmp, lpnt[0]);
		    segp->y1 = (short)GED_TO_Xy(dmp, lpnt[1]);
		    segp->x2 = (short)GED_TO_Xx(dmp, pnt[0]);
		    segp->y2 = (short)GED_TO_Xy(dmp, pnt[1]);

		    nseg++;
		    segp++;
		    VMOVE(lpnt, spnt);

		    if (nseg == 1024) {
			XDrawSegments(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				      ((struct tk_vars *)dmp->dm_vars.priv_vars)->pix,
				      ((struct tk_vars *)dmp->dm_vars.priv_vars)->gc, segbuf, nseg);

			nseg = 0;
			segp = segbuf;
		    }
		    break;
	    }
	}

	nvectors += nused;
	if (nvectors >= vectorThreshold) {
	    if (dmp->dm_debugLevel)
		bu_log("tk_drawVList(): handle Tcl events\n");

	    nvectors = 0;

	    /* Handle events in the queue */
	    while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));
	}
    }

    if (nseg) {
	XDrawSegments(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		      ((struct tk_vars *)dmp->dm_vars.priv_vars)->pix,
		      ((struct tk_vars *)dmp->dm_vars.priv_vars)->gc, segbuf, nseg);
    }
#endif
    return BRLCAD_OK;
}


int
tk_draw(struct dm_internal *dmp, struct bn_vlist *(*callback_function)(void *), void **data)
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
tk_normal(struct dm_internal *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("tk_normal()\n");

    return BRLCAD_OK;
}


/*
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
/* ARGSUSED */
HIDDEN int
tk_drawString2D(struct dm_internal *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect)
{

    if (dmp->dm_debugLevel) {
	bu_log("tk_drawString2D():\n");
	bu_log("\tstr - %s\n", str);
	bu_log("\tx - %g\n", x);
	bu_log("\ty - %g\n", y);
	bu_log("\tsize - %d\n", size);

	bu_log("color = %lu\n", ((struct tk_vars *)dmp->dm_vars.priv_vars)->fg);
	/* bu_log("real_color = %d\n", ((struct tk_vars *)dmp->dm_vars.priv_vars)->gc->foreground); */

	if (use_aspect) {
	    bu_log("\tuse_aspect - %d\t\taspect ratio - %g\n", use_aspect, dmp->dm_aspect);
	} else
	    bu_log("\tuse_aspect - 0");
    }
#if 0
    int sx, sy;
    sx = dm_Normal2Xx(dmp, x);
    sy = dm_Normal2Xy(dmp, y, use_aspect);


    Tk_DrawChars(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		 ((struct tk_vars *)dmp->dm_vars.priv_vars)->pix,
		 ((struct tk_vars *)dmp->dm_vars.priv_vars)->gc,
		 ((struct dm_xvars *)dmp->dm_vars.pub_vars)->tkfontstruct,
		 str, strlen(str), sx, sy);
#endif
    return BRLCAD_OK;
}


HIDDEN int
tk_drawLine2D(struct dm_internal *dmp, fastf_t xpos1, fastf_t ypos1, fastf_t xpos2, fastf_t ypos2)
{
    int sx1, sy1, sx2, sy2;

    sx1 = dm_Normal2Xx(dmp, xpos1);
    sx2 = dm_Normal2Xx(dmp, xpos2);
    sy1 = dm_Normal2Xy(dmp, ypos1, 0);
    sy2 = dm_Normal2Xy(dmp, ypos2, 0);

    if (dmp->dm_debugLevel) {
	bu_log("tk_drawLine2D()\n");
	bu_log("x1 = %g, y1 = %g\n", xpos1, ypos1);
	bu_log("x2 = %g, y2 = %g\n", xpos2, ypos2);
	bu_log("sx1 = %d, sy1 = %d\n", sx1, sy1);
	bu_log("sx2 = %d, sy2 = %d\n", sx2, sy2);
	bu_log("color = %lu\n", ((struct tk_vars *)dmp->dm_vars.priv_vars)->fg);
    }

#if 0

    XDrawLine(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
	      ((struct tk_vars *)dmp->dm_vars.priv_vars)->pix,
	      ((struct tk_vars *)dmp->dm_vars.priv_vars)->gc,
	      sx1, sy1, sx2, sy2);
#endif

    return BRLCAD_OK;
}


HIDDEN int
tk_drawLine3D(struct dm_internal *dmp, point_t UNUSED(pt1), point_t UNUSED(pt2))
{
    if (!dmp)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}


HIDDEN int
tk_drawLines3D(struct dm_internal *dmp, int npoints, point_t *points, int UNUSED(sflag))
{
    if (!dmp || npoints < 0 || (npoints > 0 && !points))
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}


HIDDEN int
tk_drawPoint2D(struct dm_internal *dmp, fastf_t x, fastf_t y)
{
    int sx, sy;
    sx = dm_Normal2Xx(dmp, x);
    sy = dm_Normal2Xy(dmp, y, 0);

    if (dmp->dm_debugLevel) {
	bu_log("tk_drawPoint2D()\n");
	bu_log("x = %g, y = %g\n", x, y);
	bu_log("sx = %d, sy = %d\n", sx, sy);
    }

#if 0
    XDrawPoint(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
	       ((struct tk_vars *)dmp->dm_vars.priv_vars)->pix,
	       ((struct tk_vars *)dmp->dm_vars.priv_vars)->gc, sx, sy);
#endif
    return BRLCAD_OK;
}


HIDDEN int
tk_setFGColor(struct dm_internal *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    if (!dmp) {
	bu_log("WARNING: NULL display (r/g/b => %d/%d/%d; strict => %d; transparency => %f)\n", r, g, b, strict, transparency);
	return BRLCAD_ERROR;
    }

    if (dmp->dm_debugLevel)
	bu_log("tk_setFGColor(%d %d %d)\n", r, g, b);

#if 0
    XColor color;

    INIT_XCOLOR(&color);


    dmp->dm_fg[0] = r;
    dmp->dm_fg[1] = g;
    dmp->dm_fg[2] = b;

    color.red = r << 8;
    color.green = g << 8;
    color.blue = b << 8;

    ((struct tk_vars *)dmp->dm_vars.priv_vars)->fg = Tk_GetColorByValue
	(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin, &color)->pixel;

    XSetForeground(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		   ((struct tk_vars *)dmp->dm_vars.priv_vars)->gc,
		   ((struct tk_vars *)dmp->dm_vars.priv_vars)->fg);
#endif
    return BRLCAD_OK;
}


HIDDEN int
tk_setBGColor(struct dm_internal *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    if (!dmp) {
	bu_log("WARNING: NULL display (r/g/b==%d/%d/%d)\n", r, g, b);
	return BRLCAD_ERROR;
    }

    if (dmp->dm_debugLevel)
	bu_log("tk_setBGColor()\n");

#if 0
    XColor color;

    INIT_XCOLOR(&color);


    dmp->dm_bg[0] = r;
    dmp->dm_bg[1] = g;
    dmp->dm_bg[2] = b;

    color.red = r << 8;
    color.green = g << 8;
    color.blue = b << 8;

    XSetBackground(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		   ((struct tk_vars *)dmp->dm_vars.priv_vars)->gc,
		   Tk_GetColorByValue(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin,
				      &color)->pixel);
#endif

    return BRLCAD_OK;
}


HIDDEN int
tk_setLineAttr(struct dm_internal *dmp, int width, int style)
{
    if (dmp->dm_debugLevel)
	bu_log("tk_setLineAttr(width: %d, style: %d)\n", width, style);

#if 0
    int linestyle;
    dmp->dm_lineWidth = width;
    dmp->dm_lineStyle = style;

    if (width < 1)
	width = 1;

    if (style == DM_DASHED_LINE)
	linestyle = LineOnOffDash;
    else
	linestyle = LineSolid;

    XSetLineAttributes(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		       ((struct tk_vars *)dmp->dm_vars.priv_vars)->gc,
		       width, linestyle, CapButt, JoinMiter);
#endif

    return BRLCAD_OK;
}


/* ARGSUSED */
HIDDEN int
tk_debug(struct dm_internal *dmp, int lvl)
{
    dmp->dm_debugLevel = lvl;

    return BRLCAD_OK;
}

HIDDEN int
tk_logfile(struct dm_internal *dmp, const char *filename)
{
    bu_vls_sprintf(&dmp->dm_log, "%s", filename);

    return BRLCAD_OK;
}



HIDDEN int
tk_setWinBounds(struct dm_internal *dmp, fastf_t *w)
{
    if (dmp->dm_debugLevel)
	bu_log("tk_setWinBounds()\n");

    dmp->dm_clipmin[0] = w[0];
    dmp->dm_clipmin[1] = w[2];
    dmp->dm_clipmin[2] = w[4];
    dmp->dm_clipmax[0] = w[1];
    dmp->dm_clipmax[1] = w[3];
    dmp->dm_clipmax[2] = w[5];

    return BRLCAD_OK;
}


HIDDEN int
tk_configureWin_guts(struct dm_internal *dmp, int force)
{
    int h, w;

    /* nothing to do */
    h = Tk_Height(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);
    w = Tk_Width(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);

    if (!force && dmp->dm_width==w && dmp->dm_height == h)
	return BRLCAD_OK;

#if 0
    dmp->dm_width=w;
    dmp->dm_width=h;

    dmp->dm_aspect = (fastf_t)dmp->dm_width / (fastf_t)dmp->dm_height;

    if (dmp->dm_debugLevel) {
	bu_log("tk_configureWin_guts()\n");
	bu_log("width = %d, height = %d\n", dmp->dm_width, dmp->dm_height);
    }

    Tk_FreePixmap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		  ((struct tk_vars *)dmp->dm_vars.priv_vars)->pix);
    ((struct tk_vars *)dmp->dm_vars.priv_vars)->pix =
	Tk_GetPixmap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		     DefaultRootWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy),
		     dmp->dm_width,
		     dmp->dm_height,
		     Tk_Depth(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin));

    /* First time through, load a font or quit */
    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->tkfontset == 0) {

	((struct dm_xvars *)dmp->dm_vars.pub_vars)->tkfontstruct =
	    Tk_GetFont(dmp->dm_interp, ((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin, FONT9);

	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->tkfontstruct == NULL) {
	    /* Try hardcoded backup font */

	    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->tkfontstruct =
		Tk_GetFont(dmp->dm_interp, ((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin, FONTBACK);

	    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->tkfontstruct == NULL) {
		bu_log("dm-Tk: Can't open font '%s' or '%s'\n", FONT9, FONTBACK);
		return BRLCAD_ERROR;
	    }
	}
	((struct dm_xvars *)dmp->dm_vars.pub_vars)->tkfontset = 1;
    }

    /* XXX:  I removed the font-sizing routine from dm-X from here.  Something
       should be devised to replace it.  --TJM*/
#endif

    return BRLCAD_OK;
}


HIDDEN int
tk_configureWin(struct dm_internal *dmp, int force)
{
    /* don't force */
    return tk_configureWin_guts(dmp, force);
}


HIDDEN int
tk_setLight(struct dm_internal *dmp, int light_on)
{
    if (dmp->dm_debugLevel)
	bu_log("tk_setLight:\n");

    dmp->dm_light = light_on;

    return BRLCAD_OK;
}


HIDDEN int
tk_setZBuffer(struct dm_internal *dmp, int zbuffer_on)
{
    if (dmp->dm_debugLevel)
	bu_log("tk_setZBuffer:\n");

    dmp->dm_zbuffer = zbuffer_on;

    return BRLCAD_OK;
}

struct bu_structparse Tk_vparse[] = {
    {"%g",  1, "bound",         DM_O(dm_bound),         dm_generic_hook, NULL, NULL},
    {"%d",  1, "useBound",      DM_O(dm_boundFlag),     dm_generic_hook, NULL, NULL},
    {"%d",  1, "zclip",         DM_O(dm_zclip),         dm_generic_hook, NULL, NULL},
    {"%d",  1, "debug",         DM_O(dm_debugLevel),    dm_generic_hook, NULL, NULL},
    {"",    0, (char *)0,       0,                      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};

struct dm_internal dm_tk = {
    tk_close,
    tk_drawBegin,
    tk_drawEnd,
    tk_normal,
    tk_loadMatrix,
    null_loadPMatrix,
    tk_drawString2D,
    tk_drawLine2D,
    tk_drawLine3D,
    tk_drawLines3D,
    tk_drawPoint2D,
    null_drawPoint3D,
    null_drawPoints3D,
    tk_drawVList,
    tk_drawVList,
    tk_draw,
    tk_setFGColor,
    tk_setBGColor,
    tk_setLineAttr,
    tk_configureWin,
    tk_setWinBounds,
    tk_setLight,
    null_setTransparency,
    null_setDepthMask,
    tk_setZBuffer,
    tk_debug,
    tk_logfile,
    null_beginDList,
    null_endDList,
    null_drawDList,
    null_freeDLists,
    null_genDLists,
    NULL,
    null_getDisplayImage,	/* display to image function */
    null_reshape,
    null_makeCurrent,
    null_openFb,
    NULL,
    NULL,
    0,
    0,				/* no displaylist */
    0,				/* no stereo */
    PLOTBOUND,			/* zoom-in limit */
    1,				/* bound flag */
    "Tk",
    "Tk Abstraction Layer",
    DM_TYPE_TK,
    1,
    0,
    0,
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
    {0, 0, 0},			/* bg color */
    {0, 0, 0},			/* fg color */
    {GED_MIN, GED_MIN, GED_MIN},	/* clipmin */
    {GED_MAX, GED_MAX, GED_MAX},	/* clipmax */
    0,				/* no debugging */
    BU_VLS_INIT_ZERO,		/* bu_vls logfile */
    0,				/* no perspective */
    0,				/* no lighting */
    0,				/* no transparency */
    0,				/* depth buffer is not writable */
    0,				/* no zbuffer */
    0,				/* no zclipping */
    1,                          /* clear back buffer after drawing and swap */
    0,                          /* not overriding the auto font size */
    Tk_vparse,
    FB_NULL,
    0				/* Tcl interpreter */
};


struct dm_internal *tk_open_dm(Tcl_Interp *interp, int argc, char **argv);

/* Display Manager package interface */


/*
 * Fire up the display manager, and the display processor.
 *
 */
struct dm_internal *
tk_open_dm(Tcl_Interp *interp, int argc, char **argv)
{
    int make_square = -1;

    struct dm_internal *dmp = (struct dm_internal *)NULL;
    Tk_Window tkwin;

    if ((tkwin = Tk_MainWindow(interp)) == NULL) {
	return DM_NULL;
    }

    BU_ALLOC(dmp, struct dm_internal);

    *dmp = dm_tk; /* struct copy */
    dmp->dm_interp = interp;

    BU_ALLOC(dmp->dm_vars.pub_vars, struct dm_xvars);
    BU_ALLOC(dmp->dm_vars.priv_vars, struct tk_vars);

    bu_vls_init(&dmp->dm_pathName);
    bu_vls_init(&dmp->dm_tkName);
    bu_vls_init(&dmp->dm_dName);

    struct dm_xvars *pub_vars = (struct dm_xvars *)dmp->dm_vars.pub_vars;
    struct tk_vars *privvars = (struct tk_vars *)dmp->dm_vars.priv_vars;

    /* initialize dm specific variables */
    dmp->dm_aspect = 1.0;
    pub_vars->tkfontset = 0;

    /* Things like width, height, name and initialization commands can be set
     * via argv entries - check */
    struct bu_vls init_proc_vls = BU_VLS_INIT_ZERO;
    dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

    // Find an open Tk dm name
    int win_id = 0;
    if (bu_vls_strlen(&dmp->dm_pathName) == 0) {
	bu_vls_sprintf(&dmp->dm_pathName, ".dm_tk%d", win_id);
    }
    Tk_Window *winPtr = (Tk_Window *)Tk_NameToWindow(interp, bu_vls_cstr(&dmp->dm_pathName), tkwin);
    if (winPtr) {
	while (winPtr) {
	    win_id++;
	    if (win_id > 1000) {
		tk_close(dmp);
		return DM_NULL;
	    }
	    bu_vls_sprintf(&dmp->dm_pathName, ".dm_tk%d", win_id);
	    winPtr = (Tk_Window *)Tk_NameToWindow(interp, bu_vls_cstr(&dmp->dm_pathName), tkwin);
	}
    }

    // TODO - what roll does this play on Windows?
    if (bu_vls_strlen(&dmp->dm_dName) == 0) {
	char *dp = DisplayString(Tk_Display(tkwin));
	if (dp) {
	    bu_vls_strcpy(&dmp->dm_dName, dp);
	} else {
	    bu_vls_strcpy(&dmp->dm_dName, ":0.0");
	}
    }

    if (dmp->dm_top) {
	/* Make xtkwin a toplevel window */
	pub_vars->xtkwin = Tk_CreateWindowFromPath(interp, tkwin, bu_vls_addr(&dmp->dm_pathName), bu_vls_addr(&dmp->dm_dName));
	pub_vars->top = pub_vars->xtkwin;
    } else {
	// Check for a "." in the specified pathname.  If present, the main window is the top window for dm_pathName
	char *cp = strrchr(bu_vls_addr(&dmp->dm_pathName), (int)'.');
	if (cp == bu_vls_addr(&dmp->dm_pathName)) {
	    pub_vars->top = tkwin;
	} else {
	    // "." wasn't the top according to dm_pathName - figure out which window should be top
	    struct bu_vls top_vls = BU_VLS_INIT_ZERO;
	    bu_vls_strncpy(&top_vls, (const char *)bu_vls_addr(&dmp->dm_pathName), cp - bu_vls_addr(&dmp->dm_pathName));
	    pub_vars->top = Tk_NameToWindow(interp, bu_vls_addr(&top_vls), tkwin);
	    bu_vls_free(&top_vls);
	}

	/* Make xtkwin an embedded window */
	pub_vars->xtkwin = Tk_CreateWindow(interp, pub_vars->top, cp + 1, (char *)NULL);
    }

    if (pub_vars->xtkwin == NULL) {
	bu_log("tk_open: Failed to open %s\n", bu_vls_addr(&dmp->dm_pathName));
	(void)tk_close(dmp);
	return DM_NULL;
    }

    bu_vls_printf(&dmp->dm_tkName, "%s", (char *)Tk_Name(pub_vars->xtkwin));

    if (bu_vls_strlen(&init_proc_vls) > 0) {
	struct bu_vls str = BU_VLS_INIT_ZERO;
	bu_vls_printf(&str, "%s %s\n", bu_vls_cstr(&init_proc_vls), bu_vls_cstr(&dmp->dm_pathName));
	if (Tcl_Eval(interp, bu_vls_cstr(&str)) == BRLCAD_ERROR) {
	    bu_log("Failure initializing Tk dm with command \"%s %s\":\n%s\n", bu_vls_cstr(&init_proc_vls), bu_vls_cstr(&dmp->dm_pathName), Tcl_GetStringResult(interp));
	    bu_vls_free(&str);
	    (void)tk_close(dmp);
	    return DM_NULL;
	}
	bu_vls_free(&str);
    }
    bu_vls_free(&init_proc_vls);

    pub_vars->dpy = Tk_Display(pub_vars->top);

    /* make sure there really is a display before proceeding. */
    if (!pub_vars->dpy) {
	(void)tk_close(dmp);
	return DM_NULL;
    }

    /* Finalize the dimensions of the window */
    if (dmp->dm_width == 0) {
	dmp->dm_width = WidthOfScreen(Tk_Screen(pub_vars->xtkwin));
	make_square++;
    }
    if (dmp->dm_height == 0) {
	dmp->dm_height = HeightOfScreen(Tk_Screen(pub_vars->xtkwin));
	make_square++;
    }
    if (make_square > 0) {
	/* Make window square */
	if (dmp->dm_height < dmp->dm_width) {
	    dmp->dm_width = dmp->dm_height;
	} else {
	    dmp->dm_height = dmp->dm_width;
	}
    }

    // Set up a Tk_Photo
    struct bu_vls photo_cmd = BU_VLS_INIT_ZERO;

    // Create a canvas to hold the photo
    bu_vls_sprintf(&photo_cmd, "canvas %s.canvas -width %d -height %d -borderwidth 0", bu_vls_cstr(&dmp->dm_pathName), dmp->dm_width, dmp->dm_height);
    Tcl_Eval(interp, bu_vls_cstr(&photo_cmd));
    bu_log("Canvas result::\n%s\n", Tcl_GetStringResult(interp));
    bu_vls_sprintf(&photo_cmd, "pack %s.canvas -fill both -expand 1", bu_vls_cstr(&dmp->dm_pathName));
    Tcl_Eval(interp, bu_vls_cstr(&photo_cmd));
    bu_log("pack result::\n%s\n", Tcl_GetStringResult(interp));

    // Note: confirmed with Tcl/Tk community that (at least as of Tcl/Tk 8.6)
    // Tcl_Eval is the ONLY way to create an image object.  The C API doesn't
    // expose that ability, although it does support manipulation of the
    // created object.
    bu_vls_sprintf(&photo_cmd, "image create photo %s.canvas.photo", bu_vls_cstr(&dmp->dm_pathName));
    Tcl_Eval(interp, bu_vls_cstr(&photo_cmd));
    bu_log("photo result::\n%s\n", Tcl_GetStringResult(interp));
    bu_vls_sprintf(&photo_cmd, "%s.canvas.photo", bu_vls_cstr(&dmp->dm_pathName));
    privvars->img = Tk_FindPhoto(interp, bu_vls_cstr(&photo_cmd));
    Tk_PhotoBlank(privvars->img);
    Tk_PhotoSetSize(interp, privvars->img, dmp->dm_width, dmp->dm_height);

    // Initialize the PhotoImageBlock information for a color image of size
    // 500x500 pixels.
    Tk_PhotoImageBlock dm_data;
    dm_data.width = dmp->dm_width;
    dm_data.height = dmp->dm_height;
    dm_data.pixelSize = 4;
    dm_data.pitch = dmp->dm_width * 4;
    dm_data.offset[0] = 0;
    dm_data.offset[1] = 1;
    dm_data.offset[2] = 2;
    dm_data.offset[3] = 3;

    // Actually create our memory for the image buffer.  Expects RGBA information
    dm_data.pixelPtr = (unsigned char *)Tcl_AttemptAlloc(dm_data.width * dm_data.height * 4);
    if (!dm_data.pixelPtr) {
	bu_log("Tcl/Tk photo memory allocation failed!\n");
	bu_vls_free(&photo_cmd);
	(void)tk_close(dmp);
	return DM_NULL;
    }

    // Initialize. This alters the actual data, but Tcl/Tk doesn't know about it yet.
    for (int i = 0; i < (dm_data.width * dm_data.height * 4); i+=4) {
        // Red
        dm_data.pixelPtr[i] = 0;
        // Green
        dm_data.pixelPtr[i+1] = 255;
        // Blue
        dm_data.pixelPtr[i+2] = 0;
        // Alpha at 255 - we dont' want transparency for this demo.
        dm_data.pixelPtr[i+3] = 255;
    }

    // Let Tk_Photo know we have data
    Tk_PhotoPutBlock(interp, privvars->img, &dm_data, 0, 0, dm_data.width, dm_data.height, TK_PHOTO_COMPOSITE_SET);

    // Put block operation complete - free local allocated buffer
    Tcl_Free((char *)dm_data.pixelPtr);

    bu_vls_sprintf(&photo_cmd, "%s.canvas create image 0 0 -image %s.canvas.photo -anchor nw", bu_vls_cstr(&dmp->dm_pathName), bu_vls_cstr(&dmp->dm_pathName));
    Tcl_Eval(interp, bu_vls_cstr(&photo_cmd));

    bu_vls_free(&photo_cmd);

    // Actually create the Tk Window itself
    Tk_GeometryRequest(pub_vars->xtkwin, dmp->dm_width, dmp->dm_height);
    Tk_MakeWindowExist(pub_vars->xtkwin);
    Tk_MapWindow(pub_vars->xtkwin);

    // Store some information about the window
    pub_vars->win = Tk_WindowId(pub_vars->xtkwin);
    dmp->dm_id = pub_vars->win;


    return dmp;
}


#endif /* DM_TK */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

/*                       D M - Q T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file libdm/dm-qt.cpp
 *
 */
#include "common.h"
#include <locale.h>

#ifdef DM_QT

#include "bio.h"

#include <stdio.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include "dm/dm-qt.h"

#include "tcl.h"
#include "tk.h"
#include "bu.h"
#include "dm.h"
#include "dm/dm_xvars.h"
#include "dm/dm-Null.h"

#define DM_QT_DEFAULT_POINT_SIZE 1.0


/* token used to cancel previous scheduled function using Tcl_CreateTimerHandler */
Tcl_TimerToken token = NULL;


HIDDEN bool
qt_sendRepaintEvent(struct dm *dmp)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;
    QEvent e(QEvent::UpdateRequest);
    return privars->qapp->sendEvent(privars->win, &e);
}

/**
 * Release the display manager
 */
HIDDEN int
qt_close(struct dm *dmp)
{
    struct dm_xvars *pubvars = (struct dm_xvars *)dmp->dm_vars.pub_vars;
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    if (dmp->dm_debugLevel) {
	bu_log("qt_close\n");
    }

    delete privars->font;
    delete privars->painter;
    delete privars->pix;
    delete privars->win;
    delete privars->parent;

    privars->qapp->quit();
    Tk_DestroyWindow(pubvars->xtkwin);

    bu_vls_free(&dmp->dm_pathName);
    bu_vls_free(&dmp->dm_tkName);
    bu_vls_free(&dmp->dm_dName);
    bu_free((void *)dmp->dm_vars.priv_vars, "qt_close: qt_vars");
    bu_free((void *)dmp->dm_vars.pub_vars, "qt_close: dm_xvars");
    bu_free((void *)dmp, "qt_close: dmp");

    return TCL_OK;
}


HIDDEN int
qt_drawBegin(struct dm *dmp)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    if (dmp->dm_debugLevel) {
	bu_log("qt_drawBegin\n");
    }

    privars->pix->fill(privars->bg);

    privars->painter->setPen(privars->fg);
    privars->painter->setFont(*privars->font);

    return TCL_OK;
}


HIDDEN int
qt_drawEnd(struct dm *dmp)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    if (dmp->dm_debugLevel) {
	bu_log("qt_drawEnd\n");
    }
    privars->qapp->processEvents();
    qt_sendRepaintEvent(dmp);
    privars->qapp->processEvents();

    return TCL_OK;
}

/**
 * Restore the display processor to a normal mode of operation (i.e.,
 * not scaled, rotated, displaced, etc.).
 */
HIDDEN int
qt_normal(struct dm *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("qt_normal()\n");

    return TCL_OK;
}

/**
 * Load a new transformation matrix.  This will be followed by many
 * calls to qt_draw().
 */
HIDDEN int
qt_loadMatrix(struct dm *dmp, fastf_t *mat, int UNUSED(which_eye))
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    if (dmp->dm_debugLevel) {
	bu_log("qt_loadMatrix\n");
    }

    MAT_COPY(privars->qmat, mat);

    return 0;
}


/**
 * Output a string into the displaylist. The starting position of the
 * beam is as specified.
 */
HIDDEN int
qt_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int UNUSED(size), int use_aspect)
{
    int sx, sy;
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    if (dmp->dm_debugLevel) {
	bu_log("qt_drawString2D\n");
    }

    sx = dm_Normal2Xx(dmp, x);
    sy = dm_Normal2Xy(dmp, y, use_aspect);

    if (privars->painter == NULL)
	return TCL_ERROR;
    privars->painter->drawText(sx, sy, str);

    return TCL_OK;
}


HIDDEN int
qt_drawLine2D(struct dm *dmp, fastf_t x_1, fastf_t y_1, fastf_t x_2, fastf_t y_2)
{
    int sx1, sy1, sx2, sy2;
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    if (dmp->dm_debugLevel) {
	bu_log("qt_drawLine2D\n");
    }

    sx1 = dm_Normal2Xx(dmp, x_1);
    sx2 = dm_Normal2Xx(dmp, x_2);
    sy1 = dm_Normal2Xy(dmp, y_1, 0);
    sy2 = dm_Normal2Xy(dmp, y_2, 0);

    if (privars->painter == NULL)
	return TCL_ERROR;
    privars->painter->drawLine(sx1, sy1, sx2, sy2);

    return TCL_OK;
}


HIDDEN int
qt_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y)
{
    int sx, sy;
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    if (dmp->dm_debugLevel) {
	bu_log("qt_drawPoint2D\n");
    }

    sx = dm_Normal2Xx(dmp, x);
    sy = dm_Normal2Xy(dmp, y, 0);

    if (privars->painter == NULL)
	return TCL_ERROR;
    privars->painter->drawPoint(sx, sy);

    return TCL_OK;
}


HIDDEN int
qt_drawVList(struct dm *dmp, struct bn_vlist *vp)
{
    static vect_t spnt, lpnt, pnt;
    struct bn_vlist *tvp;
    QLine lines[1024];
    QLine *linep;
    int nseg;
    fastf_t delta;
    point_t *pt_prev = NULL;
    fastf_t dist_prev=1.0;
    fastf_t pointSize = DM_QT_DEFAULT_POINT_SIZE;
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    if (dmp->dm_debugLevel) {
	bu_log("qt_drawVList\n");
    }

    /* delta is used in clipping to insure clipped endpoint is
     * slightly in front of eye plane (perspective mode only).  This
     * value is a SWAG that seems to work OK.
     */
    delta = privars->qmat[15]*0.0001;
    if (delta < 0.0)
	delta = -delta;
    if (delta < SQRT_SMALL_FASTF)
	delta = SQRT_SMALL_FASTF;

    nseg = 0;
    linep = lines;
    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	int i;
	int nused = tvp->nused;
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	fastf_t dist;

	for (i = 0; i < nused; i++, cmd++, pt++) {
	    switch (*cmd) {
		case BN_VLIST_POLY_START:
		case BN_VLIST_POLY_VERTNORM:
		case BN_VLIST_TRI_START:
		case BN_VLIST_TRI_VERTNORM:
		    continue;
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_LINE_MOVE:
		case BN_VLIST_TRI_MOVE:
		    /* Move, not draw */
		    if (dmp->dm_perspective > 0) {
			/* cannot apply perspective transformation to
			 * points behind eye plane!!!!
			 */
			dist = VDOT(*pt, &privars->qmat[12]) + privars->qmat[15];
			if (dist <= 0.0) {
			    pt_prev = pt;
			    dist_prev = dist;
			    continue;
			} else {
			    MAT4X3PNT(lpnt, privars->qmat, *pt);
			    dist_prev = dist;
			    pt_prev = pt;
			}
		    } else {
			MAT4X3PNT(lpnt, privars->qmat, *pt);
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
		    if (dmp->dm_perspective > 0) {
			dist = VDOT(*pt, &privars->qmat[12]) + privars->qmat[15];
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
				MAT4X3PNT(pnt, privars->qmat, tmp_pt);
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
				MAT4X3PNT(lpnt, privars->qmat, tmp_pt);
				lpnt[0] *= 2047;
				lpnt[1] *= 2047 * dmp->dm_aspect;
				lpnt[2] *= 2047;
				MAT4X3PNT(pnt, privars->qmat, *pt);
				}
			    } else {
				MAT4X3PNT(pnt, privars->qmat, *pt);
			    }
			}
			dist_prev = dist;
		    } else {
			MAT4X3PNT(pnt, privars->qmat, *pt);
		    }

		    pnt[0] *= 2047;
		    pnt[1] *= 2047 * dmp->dm_aspect;
		    pnt[2] *= 2047;

		    /* save pnt --- it might get changed by clip() */
		    VMOVE(spnt, pnt);
		    pt_prev = pt;

		    if (dmp->dm_zclip) {
			if (vclip(lpnt, pnt,
				  dmp->dm_clipmin,
				  dmp->dm_clipmax) == 0) {
			    VMOVE(lpnt, spnt);
			    continue;
			}
		    }
		    /* convert to Qt window coordinates */
		    linep->setLine ((short)GED_TO_Xx(dmp, lpnt[0]),
			(short)GED_TO_Xy(dmp, lpnt[1]),
			(short)GED_TO_Xx(dmp, pnt[0]),
			(short)GED_TO_Xy(dmp, pnt[1])
			);

		    nseg++;
		    linep++;
		    VMOVE(lpnt, spnt);

		    if (nseg == 1024) {
			if (privars->painter != NULL)
			    privars->painter->drawLines(lines, nseg);
			nseg = 0;
			linep = lines;
		    }
		    break;
		case BN_VLIST_POINT_DRAW:
		    if (dmp->dm_debugLevel > 2) {
			bu_log("before transformation:\n");
			bu_log("pt - %lf %lf %lf\n", V3ARGS(*pt));
		    }

		    if (dmp->dm_perspective > 0) {
			dist = VDOT(*pt, &privars->qmat[12]) + privars->qmat[15];

			if (dist <= 0.0) {
			    /* nothing to plot - point is behind eye plane */
			    continue;
			}
		    }

		    MAT4X3PNT(pnt, privars->qmat, *pt);

		    pnt[0] *= 2047;
		    pnt[1] *= 2047 * dmp->dm_aspect;
		    pnt[2] *= 2047;

		    if (dmp->dm_debugLevel > 2) {
			bu_log("after clipping:\n");
			bu_log("pt - %lf %lf %lf\n", pnt[X], pnt[Y], pnt[Z]);
		    }

		    if (pointSize <= DM_QT_DEFAULT_POINT_SIZE) {
			privars->painter->drawPoint(GED_TO_Xx(dmp, pnt[0]), GED_TO_Xy(dmp, pnt[1]));
		    } else {
			int upperLeft[2];

			upperLeft[X] = GED_TO_Xx(dmp, pnt[0]) - pointSize / 2.0;
			upperLeft[Y] = GED_TO_Xy(dmp, pnt[1]) - pointSize / 2.0;

			privars->painter->drawRect(upperLeft[X], upperLeft[Y], pointSize, pointSize);
		    }
		    break;
		case BN_VLIST_POINT_SIZE:
		    pointSize = (*pt)[0];
		    if (pointSize < DM_QT_DEFAULT_POINT_SIZE) {
			pointSize = DM_QT_DEFAULT_POINT_SIZE;
		    }
		    break;
	    }
	}
    }

    if (nseg) {
	if (privars->painter != NULL)
	    privars->painter->drawLines(lines, nseg);
    }

    return TCL_OK;
}


HIDDEN int
qt_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data)
{
    struct bn_vlist *vp;

    if (dmp->dm_debugLevel) {
	bu_log("qt_draw\n");
    }

    if (!callback_function) {
	if (data) {
	    vp = (struct bn_vlist *)data;
	    qt_drawVList(dmp, vp);
	}
    } else {
	if (!data) {
	    return TCL_ERROR;
	} else {
	    (void)callback_function(data);
	}
    }
    return TCL_OK;
}


HIDDEN int
qt_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int UNUSED(strict), fastf_t UNUSED(transparency))
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    if (dmp->dm_debugLevel) {
	bu_log("qt_setFGColor\n");
    }

    dmp->dm_fg[0] = r;
    dmp->dm_fg[1] = g;
    dmp->dm_fg[2] = b;

    privars->fg.setRgb(r, g, b);

    if (privars->painter != NULL) {
	QPen p = privars->painter->pen();
	p.setColor(privars->fg);
	privars->painter->setPen(p);
    }

    return TCL_OK;
}


HIDDEN int
qt_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    if (dmp->dm_debugLevel) {
	bu_log("qt_setBGColor\n");
    }


    privars->bg.setRgb(r, g, b);

    dmp->dm_bg[0] = r;
    dmp->dm_bg[1] = g;
    dmp->dm_bg[2] = b;

    if (privars->pix == NULL)
	return TCL_ERROR;
    privars->pix->fill(privars->bg);

    return TCL_OK;
}


HIDDEN int
qt_setLineAttr(struct dm *dmp, int width, int style)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    if (dmp->dm_debugLevel) {
	bu_log("qt_setLineAttr\n");
    }

    dmp->dm_lineWidth = width;
    dmp->dm_lineStyle = style;

    if (width <= 1)
	width = 0;

    if (privars->painter == NULL)
	return TCL_ERROR;
    QPen p = privars->painter->pen();
    p.setWidth(width);
    if (style == DM_DASHED_LINE)
	p.setStyle(Qt::DashLine);
    else
	p.setStyle(Qt::SolidLine);
    privars->painter->setPen(p);

    return TCL_OK;
}


HIDDEN void
qt_reshape(struct dm *dmp, int width, int height)
{
    if (dmp->dm_debugLevel) {
	bu_log("qt_reshape\n");
    }

    dmp->dm_height = height;
    dmp->dm_width = width;
    dmp->dm_aspect = (fastf_t)dmp->dm_width / (fastf_t)dmp->dm_height;

}


HIDDEN int
qt_configureWin(struct dm *dmp, int force)
{
    struct dm_xvars *pubvars = (struct dm_xvars *)dmp->dm_vars.pub_vars;
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    int width = Tk_Width(pubvars->xtkwin);
    int height = Tk_Height(pubvars->xtkwin);

    if (!force &&
	dmp->dm_height == height &&
	dmp->dm_width == width)
	return TCL_OK;

    qt_reshape(dmp, width, height);
    privars->win->resize(width, height);

    privars->painter->end();
    *privars->pix = privars->pix->scaled(width, height);
    privars->painter->begin(privars->pix);

    if (dmp->dm_debugLevel) {
	bu_log("qt_configureWin()\n");
	bu_log("width = %d, height = %d\n", dmp->dm_width, dmp->dm_height);
    }

    /* set font according to window size */
    if (privars->font == NULL) {
	privars->font = new QFont(QString(FONTBACK));
    }

    if (dmp->dm_width < 582) {
	if (privars->font->pointSize() != 5) {
	    privars->font->setPointSize(5);
	}
    } else if (dmp->dm_width < 679) {
	if (privars->font->pointSize() != 6) {
	    privars->font->setPointSize(6);
	}
    } else if (dmp->dm_width < 776) {
	if (privars->font->pointSize() != 7) {
	    privars->font->setPointSize(7);
	}
    } else if (dmp->dm_width < 874) {
	if (privars->font->pointSize() != 8) {
	    privars->font->setPointSize(8);
	}
    } else {
	if (privars->font->pointSize() != 9) {
	    privars->font->setPointSize(9);
	}
    }

    return TCL_OK;
}


HIDDEN int
qt_setWinBounds(struct dm *dmp, fastf_t *w)
{
    if (dmp->dm_debugLevel) {
	bu_log("qt_setWinBounds\n");
    }

    dmp->dm_clipmin[0] = w[0];
    dmp->dm_clipmin[1] = w[2];
    dmp->dm_clipmin[2] = w[4];
    dmp->dm_clipmax[0] = w[1];
    dmp->dm_clipmax[1] = w[3];
    dmp->dm_clipmax[2] = w[5];

    return TCL_OK;
}


HIDDEN int
qt_setZBuffer(struct dm *dmp, int zbuffer_on)
{
    if (dmp->dm_debugLevel) {
	bu_log("qt_setZBuffer\n");
    }

    dmp->dm_zbuffer = zbuffer_on;

    return TCL_OK;
}


HIDDEN int
qt_debug(struct dm *dmp, int lvl)
{
    dmp->dm_debugLevel = lvl;

    return TCL_OK;
}


HIDDEN int
qt_logfile(struct dm *dmp, const char *filename)
{
    bu_vls_sprintf(&dmp->dm_log, "%s", filename);

    return TCL_OK;
}


HIDDEN int
qt_getDisplayImage(struct dm *dmp, unsigned char **image)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;
    int i,j;
    int height, width;

    if (dmp->dm_debugLevel) {
	bu_log("qt_getDisplayImage\n");
    }

    QImage qimage = privars->pix->toImage();
    height = qimage.height();
    width = qimage.width();

    for (i = 0; i < height; i++) {
	for (j = 0; j < width; j++) {
	    image[i][j] = qimage.pixel(i,j);
	}
    }

    return TCL_OK;
}


HIDDEN int
qt_setLight(struct dm *dmp, int light_on)
{
    if (dmp->dm_debugLevel)
	bu_log("qt_setLight:\n");

    dmp->dm_light = light_on;

    return TCL_OK;
}


HIDDEN void
qt_processEvents(struct dm *dmp)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;
    privars->qapp->processEvents();
}


HIDDEN int
qt_openFb(struct dm *UNUSED(dmp), FBIO *UNUSED(ifp)) {
    bu_log("openFb\n");
    return 0;
}


/**
 * Function called in Tk event loop. It simply processes any
 * pending Qt events.
 *
 */
void processQtEvents(ClientData UNUSED(clientData), int UNUSED(flags)) {
    qt_processEvents(&dm_qt);
}


/**
 * Call when Tk is idle. It process Qt events then
 * reschedules itself.
 *
 */
void IdleCall(ClientData UNUSED(clientData)) {
    qt_processEvents(&dm_qt);
    Tcl_DeleteTimerHandler(token);

    /* Reschedule the function so that it continuously tries to process Qt events */
    token = Tcl_CreateTimerHandler(1, IdleCall, NULL);
}

__BEGIN_DECLS

/*
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
qt_open(Tcl_Interp *interp, int argc, char **argv)
{
    static int count = 0;
    int make_square = -1;
    struct dm *dmp = (struct dm *)NULL;
    struct bu_vls init_proc_vls = BU_VLS_INIT_ZERO;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    Tk_Window tkwin;

    struct dm_xvars *pubvars = NULL;
    struct qt_vars *privars = NULL;

    if (argc < 0 || !argv) {
	return DM_NULL;
    }

    if ((tkwin = Tk_MainWindow(interp)) == NULL) {
	return DM_NULL;
    }

    BU_ALLOC(dmp, struct dm);

    *dmp = dm_qt; /* struct copy */
    dmp->dm_interp = interp;

    BU_ALLOC(dmp->dm_vars.pub_vars, struct dm_xvars);
    pubvars = (struct dm_xvars *)dmp->dm_vars.pub_vars;

    BU_ALLOC(dmp->dm_vars.priv_vars, struct qt_vars);
    privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    bu_vls_init(&dmp->dm_pathName);
    bu_vls_init(&dmp->dm_tkName);
    bu_vls_init(&dmp->dm_dName);

    dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

    if (bu_vls_strlen(&dmp->dm_pathName) == 0) {
	bu_vls_printf(&dmp->dm_pathName, ".dm_qt%d", count);
    }
    ++count;

    if (bu_vls_strlen(&dmp->dm_dName) == 0) {
	char *dp;

	dp = getenv("DISPLAY");
	if (dp)
	    bu_vls_strcpy(&dmp->dm_dName, dp);
	else
	    bu_vls_strcpy(&dmp->dm_dName, ":0.0");
    }
    if (bu_vls_strlen(&init_proc_vls) == 0) {
	bu_vls_strcpy(&init_proc_vls, "bind_dm");
    }

    /* initialize dm specific variables */
    pubvars->devmotionnotify = LASTEvent;
    pubvars->devbuttonpress = LASTEvent;
    pubvars->devbuttonrelease = LASTEvent;
    dmp->dm_aspect = 1.0;

    if (dmp->dm_top) {
	/* Make xtkwin a toplevel window */
	pubvars->xtkwin = Tk_CreateWindowFromPath(interp, tkwin,
						  bu_vls_addr(&dmp->dm_pathName),
						  bu_vls_addr(&dmp->dm_dName));
	pubvars->top = pubvars->xtkwin;
    } else {
	char *cp;

	cp = strrchr(bu_vls_addr(&dmp->dm_pathName), (int)'.');
	if (cp == bu_vls_addr(&dmp->dm_pathName)) {
	    pubvars->top = tkwin;
	} else {
	    struct bu_vls top_vls = BU_VLS_INIT_ZERO;

	    bu_vls_strncpy(&top_vls, (const char *)bu_vls_addr(&dmp->dm_pathName), cp - bu_vls_addr(&dmp->dm_pathName));

	    pubvars->top = Tk_NameToWindow(interp, bu_vls_addr(&top_vls), tkwin);
	    bu_vls_free(&top_vls);
	}

	/* Make xtkwin an embedded window */
	pubvars->xtkwin =
	    Tk_CreateWindow(interp, pubvars->top,
			    cp + 1, (char *)NULL);
    }

    if (pubvars->xtkwin == NULL) {
	bu_log("qt_open: Failed to open %s\n", bu_vls_addr(&dmp->dm_pathName));
	(void)qt_close(dmp);
	return DM_NULL;
    }

    bu_vls_printf(&dmp->dm_tkName, "%s", (char *)Tk_Name(pubvars->xtkwin));

    bu_vls_printf(&str, "_init_dm %s %s\n", bu_vls_addr(&init_proc_vls), bu_vls_addr(&dmp->dm_pathName));

    if (Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR) {
	bu_log("qt_open: _init_dm failed\n");
	bu_vls_free(&init_proc_vls);
	bu_vls_free(&str);
	(void)qt_close(dmp);
	return DM_NULL;
    }

    bu_vls_free(&init_proc_vls);
    bu_vls_free(&str);

    pubvars->dpy = Tk_Display(pubvars->top);

    /* make sure there really is a display before proceeding. */
    if (!pubvars->dpy) {
	bu_log("qt_open: Unable to attach to display (%s)\n", bu_vls_addr(&dmp->dm_pathName));
	(void)qt_close(dmp);
	return DM_NULL;
    }

    if (dmp->dm_width == 0) {
	dmp->dm_width =
	    WidthOfScreen(Tk_Screen(pubvars->xtkwin)) - 30;
	++make_square;
    }

    if (dmp->dm_height == 0) {
	dmp->dm_height =
	    HeightOfScreen(Tk_Screen(pubvars->xtkwin)) - 30;
	++make_square;
    }

    if (make_square > 0) {
	/* Make window square */
	if (dmp->dm_height <
	    dmp->dm_width)
	    dmp->dm_width = dmp->dm_height;
	else
	    dmp->dm_height = dmp->dm_width;
    }

    Tk_GeometryRequest(pubvars->xtkwin, dmp->dm_width, dmp->dm_height);

    Tk_MakeWindowExist(pubvars->xtkwin);
    pubvars->win = Tk_WindowId(pubvars->xtkwin);
    dmp->dm_id = pubvars->win;

    Tk_SetWindowBackground(pubvars->xtkwin, 0);
    Tk_MapWindow(pubvars->xtkwin);
    privars->qapp = new QApplication(argc, argv);

    privars->parent = QWindow::fromWinId(pubvars->win);

    privars->pix = new QPixmap(dmp->dm_width, dmp->dm_height);

    privars->win = new QTkMainWindow(privars->pix, privars->parent, dmp);
    privars->win->resize(dmp->dm_width, dmp->dm_height);
    privars->win->show();

    privars->font = NULL;

    privars->painter = new QPainter(privars->pix);
    qt_setFGColor(dmp, 1, 0, 0, 0, 0);
    qt_setBGColor(dmp, 0, 0, 0);

    qt_configureWin(dmp, 1);

    MAT_IDN(privars->qmat);

    /* inputs and outputs assume POSIX/C locale settings */
    setlocale(LC_ALL, "POSIX");

    /* Make Tcl_DoOneEvent call QApplication::processEvents */
    Tcl_CreateEventSource(NULL, processQtEvents, NULL);

    /* Try to process Qt events when idle */
    Tcl_DoWhenIdle(IdleCall, NULL);

    return dmp;
}

__END_DECLS


struct dm dm_qt = {
    qt_close,
    qt_drawBegin,
    qt_drawEnd,
    qt_normal,
    qt_loadMatrix,
    null_loadPMatrix,
    qt_drawString2D,
    qt_drawLine2D,
    null_drawLine3D,
    null_drawLines3D,
    qt_drawPoint2D,
    null_drawPoint3D,
    null_drawPoints3D,
    qt_drawVList,
    qt_drawVList,
    qt_draw,
    qt_setFGColor,
    qt_setBGColor,
    qt_setLineAttr,
    qt_configureWin,
    qt_setWinBounds,
    qt_setLight,
    null_setTransparency,
    null_setDepthMask,
    qt_setZBuffer,
    qt_debug,
    qt_logfile,
    null_beginDList,
    null_endDList,
    null_drawDList,
    null_freeDLists,
    null_genDLists,
    qt_getDisplayImage,
    qt_reshape,
    null_makeCurrent,
    qt_openFb,
    0,
    0,				/* no displaylist */
    0,				/* no stereo */
    0.0,			/* zoom-in limit */
    1,				/* bound flag */
    "qt",
    "Qt Display",
    DM_TYPE_QT,
    1,
    0,/* width */
    0,/* height */
    0,/* bytes per pixel */
    0,/* bits per channel */
    0,
    0,
    1.0,/* aspect ratio */
    0,
    {0, 0},
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
    0				/* Tcl interpreter */
};


/**
 * ================================================== Event bindings declaration ==========================================================
 */

/* left click press */
char* qt_mouseButton1Press(QEvent *event) {
    if (event->type() ==  QEvent::MouseButtonPress) {
	QMouseEvent *mouseEv = (QMouseEvent *)event;
	if (mouseEv->button() == Qt::LeftButton) {
	    struct bu_vls str = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&str, "<1> -x %d -y %d", mouseEv->x(), mouseEv->y());
	    return bu_vls_addr(&str);
	}
    }
    return NULL;
}

/* left click release */
char* qt_mouseButton1Release(QEvent *event) {
    if (event->type() ==  QEvent::MouseButtonRelease) {
	QMouseEvent *mouseEv = (QMouseEvent *)event;
	if (mouseEv->button() == Qt::LeftButton) {
	    struct bu_vls str = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&str, "<ButtonRelease-1>");
	    return bu_vls_addr(&str);
	}
    }
    return NULL;
}

/* right click press */
char* qt_mouseButton3Press(QEvent *event) {
    if (event->type() ==  QEvent::MouseButtonPress) {
	QMouseEvent *mouseEv = (QMouseEvent *)event;
	if (mouseEv->button() == Qt::RightButton) {
	    struct bu_vls str = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&str, "<3> -x %d -y %d", mouseEv->x(), mouseEv->y());
	    return bu_vls_addr(&str);
	}
    }
    return NULL;
}

/* right click release */
char* qt_mouseButton3Release(QEvent *event) {
    if (event->type() ==  QEvent::MouseButtonPress) {
	QMouseEvent *mouseEv = (QMouseEvent *)event;
	if (mouseEv->button() == Qt::RightButton) {
	    struct bu_vls str = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&str, "<ButtonRelease-3>");
	    return bu_vls_addr(&str);
	}
    }
    return NULL;
}

/* middle mouse button press */
char* qt_mouseButton2Press(QEvent *event) {
    if (event->type() ==  QEvent::MouseButtonPress) {
	QMouseEvent *mouseEv = (QMouseEvent *)event;
	if (mouseEv->button() == Qt::MiddleButton) {
	    struct bu_vls str = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&str, "<2> -x %d -y %d", mouseEv->x(), mouseEv->y());
	    return bu_vls_addr(&str);
	}
    }
    return NULL;
}

/* middle mouse button release */
char* qt_mouseButton2Release(QEvent *event) {
    if (event->type() ==  QEvent::MouseButtonPress) {
	QMouseEvent *mouseEv = (QMouseEvent *)event;
	if (mouseEv->button() == Qt::MiddleButton) {
	    struct bu_vls str = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&str, "<ButtonRelease-2>");
	    return bu_vls_addr(&str);
	}
    }
    return NULL;
}

char* qt_controlMousePress(QEvent *event) {
    if (event->type() ==  QEvent::MouseButtonPress) {
	QMouseEvent *mouseEv = (QMouseEvent *)event;
	if (mouseEv->button() == Qt::LeftButton && mouseEv->modifiers() == Qt::ControlModifier) {
	    struct bu_vls str = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&str, "<Control-ButtonPress-1> -x %d -y %d", mouseEv->x(), mouseEv->y());
	    return bu_vls_addr(&str);
	}
    }
    return NULL;
}

char* qt_altMousePress(QEvent *event) {
    if (event->type() ==  QEvent::MouseButtonPress) {
	QMouseEvent *mouseEv = (QMouseEvent *)event;
	if (mouseEv->button() == Qt::LeftButton && mouseEv->modifiers() == Qt::AltModifier) {
	    struct bu_vls str = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&str, "<Alt-ButtonPress-1> -x %d -y %d", mouseEv->x(), mouseEv->y());
	    return bu_vls_addr(&str);
	}
    }
    return NULL;
}

char* qt_altControlMousePress(QEvent *event) {
    if (event->type() ==  QEvent::MouseButtonPress) {
	QMouseEvent *mouseEv = (QMouseEvent *)event;
	if (mouseEv->button() == Qt::LeftButton && mouseEv->modifiers() & Qt::AltModifier && mouseEv->modifiers() & Qt::ControlModifier) {
	    struct bu_vls str = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&str, "<Control-Alt-ButtonPress-1> -x %d -y %d", mouseEv->x(), mouseEv->y());
	    return bu_vls_addr(&str);
	}
    }
    return NULL;
}

char* qt_controlShiftMousePress(QEvent *event) {
    if (event->type() ==  QEvent::MouseButtonPress) {
	QMouseEvent *mouseEv = (QMouseEvent *)event;
	if (mouseEv->button() == Qt::LeftButton && mouseEv->modifiers() & Qt::ShiftModifier && mouseEv->modifiers() & Qt::ControlModifier) {
	    struct bu_vls str = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&str, "<Shift-Alt-ButtonPress-1> -x %d -y %d", mouseEv->x(), mouseEv->y());
	    return bu_vls_addr(&str);
	}
    }
    return NULL;
}

char* qt_mouseMove(QEvent *event) {
    if (event->type() ==  QEvent::MouseMove) {
	QMouseEvent *mouseEv = (QMouseEvent *)event;
	struct bu_vls str = BU_VLS_INIT_ZERO;
	bu_vls_printf(&str, "<Motion> -x %d -y %d", mouseEv->x(), mouseEv->y());
	return bu_vls_addr(&str);
    }
    return NULL;
}

char* qt_keyPress(QEvent *event) {
    /* FIXME numeric constant needs to be changed to QEvent::KeyPress but at this moment this does not compile */
    if (event->type() ==  6 /* QEvent::KeyPress */) {
	QKeyEvent *keyEv = (QKeyEvent *)event;
	struct bu_vls str = BU_VLS_INIT_ZERO;
	bu_vls_printf(&str, "<KeyPress-%s>", keyEv->text().data());
	return bu_vls_addr(&str);
    }
    return NULL;
}

char* qt_keyRelease(QEvent *event) {
    /* FIXME numeric constant needs to be changed to QEvent::KeyRelease but at this moment this does not compile */
    if (event->type() ==  7 /* QEvent::KeyRelease */) {
	QKeyEvent *keyEv = (QKeyEvent *)event;
	struct bu_vls str = BU_VLS_INIT_ZERO;
	bu_vls_printf(&str, "<KeyRelease-%s>", keyEv->text().data());
	return bu_vls_addr(&str);
    }
    return NULL;
}

static struct qt_tk_bind qt_bindings[] = {
    {qt_keyPress, "keypress"},
    {qt_keyRelease, "keyrelease"},
    {qt_controlMousePress, "controlbutton1"},
    {qt_altMousePress, "altbutton1"},
    {qt_altControlMousePress, "altcontrolbutton1"},
    {qt_controlShiftMousePress, "controlshiftbutton1"},
    {qt_mouseButton1Press, "button1press"},
    {qt_mouseButton1Release, "button1release"},
    {qt_mouseButton3Press, "button3press"},
    {qt_mouseButton3Release, "button3release"},
    {qt_mouseButton2Press, "button2press"},
    {qt_mouseButton2Release, "button2release"},
    {qt_mouseMove, "mouseMove"},
    {NULL, NULL}
};

/**
 * ===================================================== Main window class ===============================================
 */

QTkMainWindow::QTkMainWindow(QPixmap *p, QWindow *win, struct dm *d)
    : QWindow(win)
    , m_update_pending(false)
{
    m_backingStore = new QBackingStore(this);
    create();
    pixmap = p;
    dmp = d;
}

QTkMainWindow::~QTkMainWindow()
{
    delete m_backingStore;
    close();
}

void QTkMainWindow::exposeEvent(QExposeEvent *)
{
    if (isExposed()) {
	renderNow();
    }
}

void QTkMainWindow::resizeEvent(QResizeEvent *resizeEv)
{
    m_backingStore->resize(resizeEv->size());
    if (isExposed())
	renderNow();
}

bool QTkMainWindow::event(QEvent *ev)
{
    int index = 0;
    if (ev->type() == QEvent::UpdateRequest) {
	m_update_pending = false;
	renderNow();
	return true;
    }
    while (qt_bindings[index].name != NULL) {
	char *tk_event = qt_bindings[index].bind_function(ev);
	if (tk_event != NULL) {
	    struct bu_vls str = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&str, "event generate %s %s", bu_vls_addr(&dmp->dm_pathName), tk_event);
	    if (Tcl_Eval(dmp->dm_interp, bu_vls_addr(&str)) == TCL_ERROR) {
		bu_log("error generate event %s\n", tk_event);
	    }
	    return true;
	}
	index++;
    }
    return QWindow::event(ev);
}

void QTkMainWindow::renderNow()
{
    if (!isExposed()) {
	return;
    }

    QRect rect(0, 0, width(), height());
    m_backingStore->beginPaint(rect);

    QPaintDevice *device = m_backingStore->paintDevice();
    QPainter painter(device);

    render(&painter);

    m_backingStore->endPaint();
    m_backingStore->flush(rect);
}

void QTkMainWindow::render(QPainter *painter)
{
    painter->drawPixmap(0, 0, *pixmap);
}

#endif /* DM_QT */
/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

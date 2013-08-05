/*                       D M - Q T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#ifdef DM_QT

#include "bio.h"

#include <stdio.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include "dm-qt.h"

#include "tcl.h"
#include "tk.h"
#include "bu.h"
#include "dm.h"
#include "dm_xvars.h"


HIDDEN bool
qt_sendRepaintEvent(struct dm *dmp)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;
    QEvent e(QEvent::UpdateRequest);
    return privars->qapp->sendEvent(privars->win, &e);
}
/*
 * Q T _ C L O S E
 *
 * Release the display manager
 */
HIDDEN int
qt_close(struct dm *dmp)
{
    struct dm_xvars *pubvars = (struct dm_xvars *)dmp->dm_vars.pub_vars;
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    delete privars->font;
    privars->win->close();
    delete privars->win;

    if (privars->qapp)
	privars->qapp->quit();
    if (pubvars->xtkwin)
	Tk_DestroyWindow(pubvars->xtkwin);

    bu_vls_free(&dmp->dm_pathName);
    bu_vls_free(&dmp->dm_tkName);
    bu_vls_free(&dmp->dm_dName);
    bu_free((genptr_t)dmp->dm_vars.priv_vars, "qt_close: qt_vars");
    bu_free((genptr_t)dmp->dm_vars.pub_vars, "qt_close: dm_xvars");
    bu_free((genptr_t)dmp, "qt_close: dmp");

    return TCL_OK;
}


HIDDEN int
qt_drawBegin(struct dm *dmp)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    privars->pix->fill(privars->bg);

    privars->painter = new QPainter(privars->pix);
    privars->painter->setPen(privars->fg);
    privars->painter->setFont(*privars->font);

    bu_log("qt_drawBegin called\n");
    return TCL_OK;
}


HIDDEN int
qt_drawEnd(struct dm *dmp)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    privars->painter->end();
    delete privars->painter;
    privars->painter = NULL;
    qt_sendRepaintEvent(dmp);
    dmp->dm_processEvents(dmp);

    bu_log("qt_drawEnd called\n");
    return TCL_OK;
}


HIDDEN int
qt_normal(struct dm *UNUSED(dmp))
{
    bu_log("qt_normal not implemented\n");
    return 0;
}


HIDDEN int
qt_loadMatrix(struct dm *dmp, fastf_t *mat, int UNUSED(which_eye))
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    MAT_COPY(privars->qmat, mat);

    bu_log("qt_loadMatrix called\n");
    return 0;
}


HIDDEN int
qt_loadPMatrix(struct dm *UNUSED(dmp), fastf_t *UNUSED(mat))
{
    bu_log("qt_loadPMatrix not implemented\n");
    return 0;
}


HIDDEN int
qt_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int UNUSED(size), int use_aspect)
{
    int sx, sy;
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    sx = dm_Normal2Xx(dmp, x);
    sy = dm_Normal2Xy(dmp, y, use_aspect);

    if (privars->painter == NULL)
	return TCL_ERROR;
    privars->painter->drawText(sx, sy, str);

    bu_log("qt_drawString2D called\n");
    return TCL_OK;
}


HIDDEN int
qt_drawLine2D(struct dm *dmp, fastf_t x_1, fastf_t y_1, fastf_t x_2, fastf_t y_2)
{
    int sx1, sy1, sx2, sy2;
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    sx1 = dm_Normal2Xx(dmp, x_1);
    sx2 = dm_Normal2Xx(dmp, x_2);
    sy1 = dm_Normal2Xy(dmp, y_1, 0);
    sy2 = dm_Normal2Xy(dmp, y_2, 0);

    if (privars->painter == NULL)
	return TCL_ERROR;
    privars->painter->drawLine(sx1, sy1, sx2, sy2);

    bu_log("qt_drawLine2D called\n");
    return TCL_OK;
}


HIDDEN int
qt_drawLine3D(struct dm *UNUSED(dmp), point_t UNUSED(pt1), point_t UNUSED(pt2))
{
    bu_log("qt_drawLine3D not implemented\n");
    return 0;
}


HIDDEN int
qt_drawLines3D(struct dm *UNUSED(dmp), int UNUSED(npoints), point_t *UNUSED(points), int UNUSED(sflag))
{
    bu_log("qt_drawLines3D not implemented\n");
    return 0;
}


HIDDEN int
qt_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y)
{
    int sx, sy;
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    sx = dm_Normal2Xx(dmp, x);
    sy = dm_Normal2Xy(dmp, y, 0);

    if (privars->painter == NULL)
	return TCL_ERROR;
    privars->painter->drawPoint(sx, sy);

    bu_log("qt_drawPoint2D called\n");
    return TCL_OK;
}


HIDDEN int
qt_drawPoint3D(struct dm *UNUSED(dmp), point_t UNUSED(point))
{
    bu_log("qt_drawPoint3D not implemented\n");
    return 0;
}


HIDDEN int
qt_drawPoints3D(struct dm *UNUSED(dmp), int UNUSED(npoints), point_t *UNUSED(points))
{
    bu_log("qt_drawPoints3D not implemented\n");
    return 0;
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
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

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
			privars->painter->drawLines(lines, nseg);
			nseg = 0;
			linep = lines;
		    }
		    break;
		case BN_VLIST_POINT_DRAW:
		    break;
	    }
	}
    }

    if (nseg) {
	privars->painter->drawLines(lines, nseg);
    }

    bu_log("qt_drawVList called\n");
    return TCL_OK;
}


HIDDEN int
qt_drawVListHiddenLine(struct dm *UNUSED(dmp), struct bn_vlist *UNUSED(vp))
{
    bu_log("qt_drawVListHiddenLine not implemented\n");
    return 0;
}


HIDDEN int
qt_draw(struct dm *UNUSED(dmp), struct bn_vlist *(*callback_function)(void *), genptr_t *UNUSED(data))
{
    /* set callback_function as unused */
    (void)callback_function;
    bu_log("qt_draw not implemented\n");
    return 0;
}


HIDDEN int
qt_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int UNUSED(strict), fastf_t UNUSED(transparency))
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    dmp->dm_fg[0] = r;
    dmp->dm_fg[1] = g;
    dmp->dm_fg[2] = b;

    privars->fg.setRgb(r, g, b);

    if (privars->painter != NULL) {
	QPen p = privars->painter->pen();
	p.setColor(privars->fg);
	privars->painter->setPen(p);
    }

    bu_log("qt_setFGColor called\n");
    return TCL_OK;
}


HIDDEN int
qt_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    privars->bg.setRgb(r, g, b);

    dmp->dm_bg[0] = r;
    dmp->dm_bg[1] = g;
    dmp->dm_bg[2] = b;

    if(privars->pix == NULL)
	return TCL_ERROR;
    privars->pix->fill(privars->bg);

    bu_log("qt_setBGColor called\n");
    return TCL_OK;
}


HIDDEN int
qt_setLineAttr(struct dm *dmp, int width, int style)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

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

    bu_log("qt_setLineAttr called\n");
    return TCL_OK;
}


HIDDEN void
qt_reshape(struct dm *dmp, int width, int height)
{
    dmp->dm_height = height;
    dmp->dm_width = width;
    dmp->dm_aspect = (fastf_t)dmp->dm_width / (fastf_t)dmp->dm_height;

    bu_log("qt_reshape called\n");
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
    *privars->pix = privars->pix->scaled(width, height);

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

    bu_log("qt_configureWin called\n");
    return TCL_OK;
}


HIDDEN int
qt_setWinBounds(struct dm *dmp, fastf_t *w)
{
    dmp->dm_clipmin[0] = w[0];
    dmp->dm_clipmin[1] = w[2];
    dmp->dm_clipmin[2] = w[4];
    dmp->dm_clipmax[0] = w[1];
    dmp->dm_clipmax[1] = w[3];
    dmp->dm_clipmax[2] = w[5];

    bu_log("qt_setWinBounds called\n");
    return TCL_OK;
}


HIDDEN int
qt_setLight(struct dm *UNUSED(dmp), int UNUSED(light_on))
{
    bu_log("qt_setLight not implemented\n");
    return 0;
}


HIDDEN int
qt_setTransparency(struct dm *UNUSED(dmp), int UNUSED(transparency))
{
    bu_log("qt_setTransparency not implemented\n");
    return 0;
}


HIDDEN int
qt_setDepthMask(struct dm *UNUSED(dmp), int UNUSED(mask))
{
    bu_log("qt_setDepthMask not implemented\n");
    return 0;
}


HIDDEN int
qt_setZBuffer(struct dm *UNUSED(dmp), int UNUSED(zbuffer_on))
{
    bu_log("qt_setZBuffer not implemented\n");
    return 0;
}


HIDDEN int
qt_debug(struct dm *UNUSED(dmp), int UNUSED(lvl))
{
    bu_log("qt_debug not implemented\n");
    return 0;
}


HIDDEN int
qt_beginDList(struct dm *UNUSED(dmp), unsigned int UNUSED(list))
{
    bu_log("qt_beginDList not implemented\n");
    return 0;
}


HIDDEN int
qt_endDList(struct dm *UNUSED(dmp))
{
    bu_log("qt_endDList not implemented\n");
    return 0;
}


HIDDEN void
qt_drawDList(unsigned int UNUSED(list))
{
    bu_log("qt_drawDList not implemented\n");
}


HIDDEN int
qt_freeDLists(struct dm *UNUSED(dmp), unsigned int UNUSED(list), int UNUSED(range))
{
    bu_log("qt_freeDList not implemented\n");
    return 0;
}


HIDDEN int
qt_genDLists(struct dm *UNUSED(dmp), size_t UNUSED(range))
{
    bu_log("qt_genDLists not implemented\n");
    return 0;
}


HIDDEN int
qt_getDisplayImage(struct dm *UNUSED(dmp), unsigned char **UNUSED(image))
{
    bu_log("qt_getDisplayImage not implemented\n");
    return 0;
}


HIDDEN int
qt_makeCurrent(struct dm *UNUSED(dmp))
{
    bu_log("qt_makeCurrent not implemented\n");
    return 0;
}


HIDDEN void
qt_processEvents(struct dm *dmp)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;
    privars->qapp->processEvents();
}


struct dm dm_qt = {
    qt_close,
    qt_drawBegin,
    qt_drawEnd,
    qt_normal,
    qt_loadMatrix,
    qt_loadPMatrix,
    qt_drawString2D,
    qt_drawLine2D,
    qt_drawLine3D,
    qt_drawLines3D,
    qt_drawPoint2D,
    qt_drawPoint3D,
    qt_drawPoints3D,
    qt_drawVList,
    qt_drawVListHiddenLine,
    qt_draw,
    qt_setFGColor,
    qt_setBGColor,
    qt_setLineAttr,
    qt_configureWin,
    qt_setWinBounds,
    qt_setLight,
    qt_setTransparency,
    qt_setDepthMask,
    qt_setZBuffer,
    qt_debug,
    qt_beginDList,
    qt_endDList,
    qt_drawDList,
    qt_freeDLists,
    qt_genDLists,
    qt_getDisplayImage,
    qt_reshape,
    qt_makeCurrent,
    qt_processEvents,
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


QTkMainWindow::QTkMainWindow(QPixmap *p, QWindow *win, struct dm *d)
    : QWindow(win)
    , m_update_pending(false)
{
    m_backingStore = new QBackingStore(this);
    create();
    pixmap = p;
    dmp = d;
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
    if (ev->type() == QEvent::UpdateRequest) {
	m_update_pending = false;
	renderNow();
	return true;
    }
    else if (ev->type() == QEvent::MouseButtonPress) {
	struct bu_vls str = BU_VLS_INIT_ZERO;
	QMouseEvent *mouseEv = (QMouseEvent *)ev;

	if (mouseEv->button() == Qt::LeftButton)
	    bu_vls_printf(&str, "event generate %V <1>", &dmp->dm_pathName);
	else
	    bu_vls_printf(&str, "event generate %V <3>", &dmp->dm_pathName);

	if (Tcl_Eval(dmp->dm_interp, bu_vls_addr(&str)) == TCL_ERROR) {
	    bu_log("error generate event\n");
	}
	bu_log("AICI\n");
	return true;
    }
    return QWindow::event(ev);
}

void QTkMainWindow::renderNow()
{
    if (!isExposed())
	return;

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

__BEGIN_DECLS

/*
 * Q T _ O P E N
 *
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

    if (argc < 0 || !argv)
	return DM_NULL;

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
    if (bu_vls_strlen(&init_proc_vls) == 0)
	bu_vls_strcpy(&init_proc_vls, "bind_dm");
    
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

    bu_vls_printf(&str, "_init_dm %V %V\n", &init_proc_vls, &dmp->dm_pathName);

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

    QWindow *window = QWindow::fromWinId(pubvars->win);

    privars->pix = new QPixmap(dmp->dm_width, dmp->dm_height);

    privars->win = new QTkMainWindow(privars->pix, window, dmp);
    privars->win->resize(dmp->dm_width, dmp->dm_height);
    privars->win->show();

    privars->font = NULL;

    qt_configureWin(dmp, 1);

    privars->painter = NULL;
    qt_setFGColor(dmp, 1, 0, 0, 0, 0);
    qt_setBGColor(dmp, 0, 0, 0);

    MAT_IDN(privars->qmat);

    bu_log("qt_open called\n");
    return dmp;
}

__END_DECLS

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

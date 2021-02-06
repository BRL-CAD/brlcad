/*                       D M - Q T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2021 United States Government as represented by
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

#include "./fb_qt.h"
#include "./dm-qt.h"

#include <locale.h>

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#include "tcl.h"
#include "tk.h"
#include "bu/log.h"
#include "bu/vls.h"
#include "bu/parse.h"
#include "dm.h"
#include "private.h"
#include "../null/dm-Null.h"


#define DM_QT_DEFAULT_POINT_SIZE 1.0


/* token used to cancel previous scheduled function using Tcl_CreateTimerHandler */
Tcl_TimerToken token = NULL;

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
	bu_vls_printf(&str, "<KeyPress-%c>", keyEv->text().data()->toLatin1());
	return bu_vls_addr(&str);
    }
    return NULL;
}

char* qt_keyRelease(QEvent *event) {
    /* FIXME numeric constant needs to be changed to QEvent::KeyRelease but at this moment this does not compile */
    if (event->type() ==  7 /* QEvent::KeyRelease */) {
	QKeyEvent *keyEv = (QKeyEvent *)event;
	struct bu_vls str = BU_VLS_INIT_ZERO;
	bu_vls_printf(&str, "<KeyRelease-%c>", keyEv->text().data()->toLatin1());
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

QTkMainWindow::QTkMainWindow(QPixmap *p, QWindow *win, void *d)
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
	    //struct bu_vls str = BU_VLS_INIT_ZERO;
	    //bu_vls_printf(&str, "event generate %s %s", bu_vls_addr(&((struct dm *)dmp)->dm_pathName), tk_event);
	    //if (Tcl_Eval(((struct dm *)dmp)->dm_interp, bu_vls_addr(&str)) == TCL_ERROR) {
	//	bu_log("error generate event %s\n", tk_event);
	 //   }
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


HIDDEN bool
qt_sendRepaintEvent(struct dm *dmp)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;
    QEvent e(QEvent::UpdateRequest);
    return privars->qapp->sendEvent(privars->win, &e);
}

/*
 * Fire up the display manager, and the display processor.
 *
 */
extern "C" struct dm *
qt_open(void *vinterp, int argc, const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)vinterp;
    static int count = 0;
    //int make_square = -1;
    struct dm *dmp = (struct dm *)NULL;
    struct bu_vls init_proc_vls = BU_VLS_INIT_ZERO;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    Tk_Window tkwin;

    struct dm_qtvars *pubvars = NULL;
    struct qt_vars *privars = NULL;

    if (argc < 0 || !argv) {
	return DM_NULL;
    }

    if ((tkwin = Tk_MainWindow(interp)) == NULL) {
	return DM_NULL;
    }

    BU_ALLOC(dmp, struct dm);
    dmp->magic = DM_MAGIC;

    BU_ALLOC(dmp->i, struct dm_impl);

    *dmp->i = *dm_qt.i; /* struct copy */
    dmp->i->dm_interp = interp;

    BU_ALLOC(dmp->i->dm_vars.pub_vars, struct dm_qtvars);
    pubvars = (struct dm_qtvars *)dmp->i->dm_vars.pub_vars;

    BU_ALLOC(dmp->i->dm_vars.priv_vars, struct qt_vars);
    privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;

    bu_vls_init(&dmp->i->dm_pathName);
    bu_vls_init(&dmp->i->dm_tkName);
    bu_vls_init(&dmp->i->dm_dName);

    dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

    if (bu_vls_strlen(&dmp->i->dm_pathName) == 0) {
	bu_vls_printf(&dmp->i->dm_pathName, ".dm_qt%d", count);
    }
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

    if (dmp->i->dm_top) {
	/* Make xtkwin a toplevel window */
	//pubvars->xtkwin = Tk_CreateWindowFromPath(interp, tkwin, bu_vls_addr(&dmp->i->dm_pathName), bu_vls_addr(&dmp->i->dm_dName));
	//pubvars->top = pubvars->xtkwin;
    } else {
	char *cp;

	cp = strrchr(bu_vls_addr(&dmp->i->dm_pathName), (int)'.');
	if (cp == bu_vls_addr(&dmp->i->dm_pathName)) {
	    //pubvars->top = tkwin;
	} else {
	    struct bu_vls top_vls = BU_VLS_INIT_ZERO;

	    bu_vls_strncpy(&top_vls, (const char *)bu_vls_addr(&dmp->i->dm_pathName), cp - bu_vls_addr(&dmp->i->dm_pathName));

	    //pubvars->top = Tk_NameToWindow(interp, bu_vls_addr(&top_vls), tkwin);
	    bu_vls_free(&top_vls);
	}

	/* Make xtkwin an embedded window */
	//pubvars->xtkwin =  Tk_CreateWindow(interp, pubvars->top,  cp + 1, (char *)NULL);
    }

#if 0
    if (pubvars->xtkwin == NULL) {
	bu_log("qt_open: Failed to open %s\n", bu_vls_addr(&dmp->i->dm_pathName));
	//(void)qt_close(dmp);
	return DM_NULL;
    }
#endif

    //bu_vls_printf(&dmp->i->dm_tkName, "%s", (char *)Tk_Name(pubvars->xtkwin));

    if (bu_vls_strlen(&init_proc_vls) > 0) {
	bu_vls_printf(&str, "%s %s\n", bu_vls_addr(&init_proc_vls), bu_vls_addr(&dmp->i->dm_pathName));

	if (Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR) {
	    bu_log("qt_open: dm init failed\n");
	    bu_vls_free(&init_proc_vls);
	    bu_vls_free(&str);
	    //(void)qt_close(dmp);
	    return DM_NULL;
	}
    }

    bu_vls_free(&init_proc_vls);
    bu_vls_free(&str);

    //pubvars->dpy = Tk_Display(pubvars->top);

#if 0
    /* make sure there really is a display before proceeding. */
    if (!pubvars->dpy) {
	bu_log("qt_open: Unable to attach to display (%s)\n", bu_vls_addr(&dmp->i->dm_pathName));
	(void)qt_close(dmp);
	return DM_NULL;
    }

    if (dmp->i->dm_width == 0) {
	dmp->i->dm_width =
	    WidthOfScreen(Tk_Screen(pubvars->xtkwin)) - 30;
	++make_square;
    }

    if (dmp->i->dm_height == 0) {
	dmp->i->dm_height =
	    HeightOfScreen(Tk_Screen(pubvars->xtkwin)) - 30;
	++make_square;
    }

    if (make_square > 0) {
	/* Make window square */
	if (dmp->i->dm_height <
	    dmp->i->dm_width)
	    dmp->i->dm_width = dmp->i->dm_height;
	else
	    dmp->i->dm_height = dmp->i->dm_width;
    }

    Tk_GeometryRequest(pubvars->xtkwin, dmp->i->dm_width, dmp->i->dm_height);

    Tk_MakeWindowExist(pubvars->xtkwin);
    pubvars->win = Tk_WindowId(pubvars->xtkwin);
    dmp->i->dm_id = pubvars->win;

    Tk_SetWindowBackground(pubvars->xtkwin, 0);
    Tk_MapWindow(pubvars->xtkwin);

#endif
    privars->qapp = new QApplication(argc, (char **)argv);

    //privars->parent = QWindow::fromWinId(pubvars->win);

    privars->pix = new QPixmap(dmp->i->dm_width, dmp->i->dm_height);

    privars->win = new QTkMainWindow(privars->pix, privars->parent, dmp);
    privars->win->resize(dmp->i->dm_width, dmp->i->dm_height);
    privars->win->show();

    privars->font = NULL;

    privars->painter = new QPainter(privars->pix);
    //qt_setFGColor(dmp, 1, 0, 0, 0, 0);
    //qt_setBGColor(dmp, 0, 0, 0);

    //qt_configureWin(dmp, 1);

    MAT_IDN(privars->mod_mat);
    MAT_IDN(privars->disp_mat);

    privars->qmat = &(privars->mod_mat[0]);
    /* inputs and outputs assume POSIX/C locale settings */
    setlocale(LC_ALL, "POSIX");

#if 0
    /* Make Tcl_DoOneEvent call QApplication::processEvents */
    Tcl_CreateEventSource(NULL, processQtEvents, NULL);

    /* Try to process Qt events when idle */
    Tcl_DoWhenIdle(IdleCall, NULL);
#endif
    return dmp;
}

/**
 * Release the display manager
 */
HIDDEN int
qt_close(struct dm *dmp)
{
    //struct dm_qtvars *pubvars = (struct dm_qtvars *)dmp->i->dm_vars.pub_vars;
    struct qt_vars *privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel) {
	bu_log("qt_close\n");
    }

    delete privars->font;
    delete privars->painter;
    delete privars->pix;
    delete privars->win;
    delete privars->parent;

    privars->qapp->quit();
    //Tk_DestroyWindow(pubvars->xtkwin);

    bu_vls_free(&dmp->i->dm_pathName);
    bu_vls_free(&dmp->i->dm_tkName);
    bu_vls_free(&dmp->i->dm_dName);
    bu_free((void *)dmp->i->dm_vars.priv_vars, "qt_close: qt_vars");
    bu_free((void *)dmp->i->dm_vars.pub_vars, "qt_close: dm_qtvars");
    bu_free((void *)dmp->i, "qt_close: dmp impl");
    bu_free((void *)dmp, "qt_close: dmp");

    return TCL_OK;
}

int
qt_viable(const char *UNUSED(dpy_string))
{
    return 1;
}


HIDDEN int
qt_drawBegin(struct dm *dmp)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel) {
	bu_log("qt_drawBegin\n");
    }

    privars->pix->fill(privars->bg);

    privars->painter->setPen(privars->fg);
    privars->painter->setFont(*privars->font);

    if (privars->img != NULL && privars->drawFb == 1) {
	privars->painter->drawImage(0, 0, *privars->img, 0, 0, dmp->i->dm_width - 1, dmp->i->dm_height - 1);
    }

    return TCL_OK;
}


HIDDEN int
qt_drawEnd(struct dm *dmp)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel) {
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
    if (dmp->i->dm_debugLevel)
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
    struct qt_vars *privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel) {
	bu_log("qt_loadMatrix\n");
    }

    MAT_COPY(privars->mod_mat, mat);

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
    struct qt_vars *privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel) {
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
    struct qt_vars *privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel) {
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
    struct qt_vars *privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel) {
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
    struct qt_vars *privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel) {
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
	point_t tlate;
	fastf_t dist;

	for (i = 0; i < nused; i++, cmd++, pt++) {
	    switch (*cmd) {
		case BN_VLIST_POLY_START:
		case BN_VLIST_POLY_VERTNORM:
		case BN_VLIST_TRI_START:
		case BN_VLIST_TRI_VERTNORM:
		    continue;
		case BN_VLIST_MODEL_MAT:
		    privars->qmat = &(privars->mod_mat[0]);
		    continue;
		case BN_VLIST_DISPLAY_MAT:
		    MAT4X3PNT(tlate, (privars->mod_mat), *pt);
		    privars->disp_mat[3] = tlate[0];
		    privars->disp_mat[7] = tlate[1];
		    privars->disp_mat[11] = tlate[2];
		    privars->qmat = &(privars->disp_mat[0]);
		    continue;
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_LINE_MOVE:
		case BN_VLIST_TRI_MOVE:
		    /* Move, not draw */
		    if (dmp->i->dm_perspective > 0) {
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
		    lpnt[1] *= 2047 * dmp->i->dm_aspect;
		    lpnt[2] *= 2047;
		    continue;
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_POLY_END:
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_TRI_DRAW:
		case BN_VLIST_TRI_END:
		    /* draw */
		    if (dmp->i->dm_perspective > 0) {
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
				lpnt[1] *= 2047 * dmp->i->dm_aspect;
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
		    pnt[1] *= 2047 * dmp->i->dm_aspect;
		    pnt[2] *= 2047;

		    /* save pnt --- it might get changed by clip() */
		    VMOVE(spnt, pnt);
		    pt_prev = pt;

		    if (dmp->i->dm_zclip) {
			if (vclip(lpnt, pnt,
				  dmp->i->dm_clipmin,
				  dmp->i->dm_clipmax) == 0) {
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
		    if (dmp->i->dm_debugLevel > 2) {
			bu_log("before transformation:\n");
			bu_log("pt - %lf %lf %lf\n", V3ARGS(*pt));
		    }

		    if (dmp->i->dm_perspective > 0) {
			dist = VDOT(*pt, &privars->qmat[12]) + privars->qmat[15];

			if (dist <= 0.0) {
			    /* nothing to plot - point is behind eye plane */
			    continue;
			}
		    }

		    MAT4X3PNT(pnt, privars->qmat, *pt);

		    pnt[0] *= 2047;
		    pnt[1] *= 2047 * dmp->i->dm_aspect;
		    pnt[2] *= 2047;
		    VMOVE(lpnt, pnt);

		    if (dmp->i->dm_debugLevel > 2) {
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

    if (dmp->i->dm_debugLevel) {
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
    struct qt_vars *privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel) {
	bu_log("qt_setFGColor\n");
    }

    dmp->i->dm_fg[0] = r;
    dmp->i->dm_fg[1] = g;
    dmp->i->dm_fg[2] = b;

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
    struct qt_vars *privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel) {
	bu_log("qt_setBGColor\n");
    }


    privars->bg.setRgb(r, g, b);

    dmp->i->dm_bg[0] = r;
    dmp->i->dm_bg[1] = g;
    dmp->i->dm_bg[2] = b;

    if (privars->pix == NULL)
	return TCL_ERROR;
    privars->pix->fill(privars->bg);

    return TCL_OK;
}


HIDDEN int
qt_setLineAttr(struct dm *dmp, int width, int style)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel) {
	bu_log("qt_setLineAttr\n");
    }

    dmp->i->dm_lineWidth = width;
    dmp->i->dm_lineStyle = style;

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


HIDDEN int
qt_reshape(struct dm *dmp, int width, int height)
{
    if (dmp->i->dm_debugLevel) {
	bu_log("qt_reshape\n");
    }

    dmp->i->dm_height = height;
    dmp->i->dm_width = width;
    dmp->i->dm_aspect = (fastf_t)dmp->i->dm_width / (fastf_t)dmp->i->dm_height;

    return 0;
}


HIDDEN int
qt_configureWin(struct dm *dmp, int force)
{
    //struct dm_qtvars *pubvars = (struct dm_qtvars *)dmp->i->dm_vars.pub_vars;
    struct qt_vars *privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;

    int width = 0;
    int height = 0;

    if (!force &&
	dmp->i->dm_height == height &&
	dmp->i->dm_width == width)
	return TCL_OK;

    qt_reshape(dmp, width, height);
    privars->win->resize(width, height);

    privars->painter->end();
    *privars->pix = privars->pix->scaled(width, height);
    privars->painter->begin(privars->pix);

    if (dmp->i->dm_debugLevel) {
	bu_log("qt_configureWin()\n");
	bu_log("width = %d, height = %d\n", dmp->i->dm_width, dmp->i->dm_height);
    }

    /* set font according to window size */
    if (privars->font == NULL) {
	privars->font = new QFont(QString(FONTBACK));
    }

    if (dmp->i->dm_width < 582) {
	if (privars->font->pointSize() != 5) {
	    privars->font->setPointSize(5);
	}
    } else if (dmp->i->dm_width < 679) {
	if (privars->font->pointSize() != 6) {
	    privars->font->setPointSize(6);
	}
    } else if (dmp->i->dm_width < 776) {
	if (privars->font->pointSize() != 7) {
	    privars->font->setPointSize(7);
	}
    } else if (dmp->i->dm_width < 874) {
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
    if (dmp->i->dm_debugLevel) {
	bu_log("qt_setWinBounds\n");
    }

    dmp->i->dm_clipmin[0] = w[0];
    dmp->i->dm_clipmin[1] = w[2];
    dmp->i->dm_clipmin[2] = w[4];
    dmp->i->dm_clipmax[0] = w[1];
    dmp->i->dm_clipmax[1] = w[3];
    dmp->i->dm_clipmax[2] = w[5];

    return TCL_OK;
}


HIDDEN int
qt_setZBuffer(struct dm *dmp, int zbuffer_on)
{
    if (dmp->i->dm_debugLevel) {
	bu_log("qt_setZBuffer\n");
    }

    dmp->i->dm_zbuffer = zbuffer_on;

    return TCL_OK;
}


HIDDEN int
qt_debug(struct dm *dmp, int lvl)
{
    dmp->i->dm_debugLevel = lvl;

    return TCL_OK;
}


HIDDEN int
qt_logfile(struct dm *dmp, const char *filename)
{
    bu_vls_sprintf(&dmp->i->dm_log, "%s", filename);

    return TCL_OK;
}


HIDDEN int
qt_getDisplayImage(struct dm *dmp, unsigned char **image)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;
    int i,j;
    int height, width;

    if (dmp->i->dm_debugLevel) {
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
    if (dmp->i->dm_debugLevel)
	bu_log("qt_setLight:\n");

    dmp->i->dm_light = light_on;

    return TCL_OK;
}


HIDDEN void
qt_processEvents(struct dm *dmp)
{
    struct qt_vars *privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;
    privars->qapp->processEvents();
}


HIDDEN int
qt_openFb(struct dm *dmp)
{

    struct fb_platform_specific *fb_ps;
    struct qt_fb_info *qtfb_ps;
    struct qt_vars *privars = (struct qt_vars *)dmp->i->dm_vars.priv_vars;

    fb_ps = fb_get_platform_specific(FB_QT_MAGIC);
    qtfb_ps = (struct qt_fb_info *)fb_ps->data;
    qtfb_ps->qapp = privars->qapp;
    qtfb_ps->qwin = privars->win;
    qtfb_ps->qpainter = privars->painter;
    qtfb_ps->draw = &privars->drawFb;
    qtfb_ps->qimg = (void **)&privars->img;

    dmp->i->fbp = fb_open_existing("Qt", dm_get_width(dmp), dm_get_height(dmp), fb_ps);
    fb_put_platform_specific(fb_ps);
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

static void
Qt_zclip_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct dm *dmp = (struct dm *)base;
    fastf_t bounds[6] = { GED_MIN, GED_MAX, GED_MIN, GED_MAX, GED_MIN, GED_MAX };

    if (dmp->i->dm_zclip) {
	bounds[4] = -1.0;
	bounds[5] = 1.0;
    }

    (void)dm_make_current(dmp);
    (void)dm_set_win_bounds(dmp, bounds);

    dm_generic_hook(sdp, name, base, value, data);
}


struct bu_structparse Qt_vparse[] = {
    {"%g",  1, "bound",         DM_O(dm_bound),         dm_generic_hook, NULL, NULL},
    {"%d",  1, "useBound",      DM_O(dm_boundFlag),     dm_generic_hook, NULL, NULL},
    {"%d",  1, "zclip",         DM_O(dm_zclip),         Qt_zclip_hook, NULL, NULL},
    {"%d",  1, "debug",         DM_O(dm_debugLevel),    dm_generic_hook, NULL, NULL},
    {"",    0, (char *)0,       0,                      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};


int
qt_geometry_request(struct dm *dmp, int UNUSED(width), int UNUSED(height))
{
    if (!dmp) return -1;
    //Tk_GeometryRequest(((struct dm_qtvars *)dmp->i->dm_vars.pub_vars)->xtkwin, width, height);
    return 0;
}

#define QTVARS_MV_O(_m) offsetof(struct dm_qtvars, _m)

struct bu_structparse dm_qtvars_vparse[] = {
    {"%d",      1,      "devmotionnotify",      QTVARS_MV_O(devmotionnotify),    BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devbuttonpress",       QTVARS_MV_O(devbuttonpress),     BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devbuttonrelease",     QTVARS_MV_O(devbuttonrelease),   BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",        0,      (char *)0,              0,                      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

int
qt_internal_var(struct bu_vls *result, struct dm *dmp, const char *key)
{
    if (!dmp || !result) return -1;
    if (!key) {
        // Print all current vars
        bu_vls_struct_print2(result, "dm internal Qt variables", dm_qtvars_vparse, (const char *)dmp->i->dm_vars.pub_vars);
        return 0;
    }
    // Print specific var
    bu_vls_struct_item_named(result, dm_qtvars_vparse, key, (const char *)dmp->i->dm_vars.pub_vars, ',');

    return 0;
}

int
qt_event_cmp(struct dm *dmp, dm_event_t type, int event)
{
    struct dm_qtvars *pubvars = (struct dm_qtvars *)dmp->i->dm_vars.pub_vars;
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

__END_DECLS

struct dm_impl dm_qt_impl = {
    qt_open,
    qt_close,
    qt_viable,
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
    NULL,
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
    NULL,
    qt_getDisplayImage,
    qt_reshape,
    null_makeCurrent,
    null_doevent,
    qt_openFb,
    NULL,
    NULL,
    qt_geometry_request,
    qt_internal_var,
    NULL,
    NULL,
    NULL,
    qt_event_cmp,
    NULL,
    NULL,
    0,
    1,				/* is graphical */
    "Qt",                       /* uses Qt graphics system */
    0,				/* no displaylist */
    0,				/* no stereo */
    0.0,			/* zoom-in limit */
    1,				/* bound flag */
    "qt",
    "Qt Display",
    1,
    0,/* width */
    0,/* height */
    0,/* dirty */
    0,/* bytes per pixel */
    0,/* bits per channel */
    0,
    0,
    1.0,/* aspect ratio */
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
    0,				/* depth buffer is not writable */
    0,				/* no zbuffer */
    0,				/* no zclipping */
    1,                          /* clear back buffer after drawing and swap */
    0,                          /* not overriding the auto font size */
    Qt_vparse,
    FB_NULL,
    0				/* Tcl interpreter */
};


extern "C" {
    struct dm dm_qt = { DM_MAGIC, &dm_qt_impl };

#ifdef DM_PLUGIN
    static const struct dm_plugin pinfo = { DM_API, &dm_qt };

    COMPILER_DLLEXPORT const struct dm_plugin *dm_plugin_info()
    {
	return &pinfo;
    }
#endif
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

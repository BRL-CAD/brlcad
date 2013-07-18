/*                       D M - Q T . C
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
#include <QPainter>

#include "tcl.h"
#include "tk.h"
#include "bu.h"
#include "vmath.h"
#include "dm.h"
#include "dm_xvars.h"

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

    bu_log("qt_drawBegin not implemented\n");
    return 0;
}


HIDDEN int
qt_drawEnd(struct dm *UNUSED(dmp))
{
    bu_log("qt_drawEnd not implemented\n");
    return 0;
}


HIDDEN int
qt_normal(struct dm *UNUSED(dmp))
{
    bu_log("qt_normal not implemented\n");
    return 0;
}


HIDDEN int
qt_loadMatrix(struct dm *UNUSED(dmp), fastf_t *UNUSED(mat), int UNUSED(which_eye))
{
    bu_log("qt_loadMatrix not implemented\n");
    return 0;
}


HIDDEN int
qt_loadPMatrix(struct dm *UNUSED(dmp), fastf_t *UNUSED(mat))
{
    bu_log("qt_loadPMatrix not implemented\n");
    return 0;
}


HIDDEN int
qt_drawString2D(struct dm *UNUSED(dmp), const char *UNUSED(str), fastf_t UNUSED(x), fastf_t UNUSED(y), int UNUSED(size), int UNUSED(use_aspect))
{
    bu_log("qt_drawString2D not implemented\n");
    return 0;
}


HIDDEN int
qt_drawLine2D(struct dm *dmp, fastf_t x_1, fastf_t y_1, fastf_t x_2, fastf_t y_2)
{
    int sx1, sy1, sx2, sy2;
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;
    QPainter * p = new QPainter(privars->pix);

    p->setPen(Qt::white);

    sx1 = dm_Normal2Xx(dmp, x_1);
    sx2 = dm_Normal2Xx(dmp, x_2);
    sy1 = dm_Normal2Xy(dmp, y_1, 0);
    sy2 = dm_Normal2Xy(dmp, y_2, 0);

    p->drawLine(sx1, sy1, sx2, sy2);
    p->end();

    delete p;


    return 0;
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
qt_drawPoint2D(struct dm *UNUSED(dmp), fastf_t UNUSED(x), fastf_t UNUSED(y))
{
    bu_log("qt_drawPoint2D not implemented\n");
    return 0;
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
qt_drawVList(struct dm *UNUSED(dmp), struct bn_vlist *UNUSED(vp))
{
    bu_log("qt_drawVList not implemented\n");
    return 0;
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
qt_setFGColor(struct dm *UNUSED(dmp), unsigned char UNUSED(r), unsigned char UNUSED(g), unsigned char UNUSED(b), int UNUSED(strict), fastf_t UNUSED(transparency))
{
    bu_log("qt_setFGColor not implemented\n");
    return 0;
}


HIDDEN int
qt_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    QColor q;
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    q.setRgb(r, g, b);

    dmp->dm_bg[0] = r;
    dmp->dm_bg[1] = g;
    dmp->dm_bg[2] = b;

    privars->bg = q;

    QPalette pal = privars->win->palette();
    pal.setColor(QPalette::Background, q);
    privars->win->setPalette(pal);

    privars->pix->fill(q);

    return TCL_OK;
}


HIDDEN int
qt_setLineAttr(struct dm *UNUSED(dmp), int UNUSED(width), int UNUSED(style))
{
    bu_log("qt_setLineAttr not implemented\n");
    return 0;
}


HIDDEN void
qt_reshape(struct dm *dmp, int width, int height)
{
    dmp->dm_height = height;
    dmp->dm_width = width;
    dmp->dm_aspect = (fastf_t)dmp->dm_width / (fastf_t)dmp->dm_height;
}


HIDDEN int
qt_configureWin(struct dm *dmp, int force)
{
     /* struct dm_xvars *pubvars = (struct dm_xvars *)dmp->dm_vars.pub_vars; */
    struct qt_vars *privars = (struct qt_vars *)dmp->dm_vars.priv_vars;

    int width = privars->win->width();
    int height = privars->win->height();

    if (!force &&
	dmp->dm_height == height &&
	dmp->dm_width == width)
	return TCL_OK;

    qt_reshape(dmp, width, height);
    *privars->pix = privars->pix->scaled(width, height);

    if (dmp->dm_debugLevel) {
	bu_log("qt_configureWin_guts()\n");
	bu_log("width = %d, height = %d\n", dmp->dm_width, dmp->dm_height);
    }

    /* TODO select font */

    return TCL_OK;
}


HIDDEN int
qt_setWinBounds(struct dm *UNUSED(dmp), fastf_t *UNUSED(w))
{
    bu_log("qt_setWinBounds not implemented\n");
    return 0;
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


QTkMainWindow::QTkMainWindow(QPixmap *pix, WId win)
:QWidget()
{
    create(win, false, true);
    pixmap = pix;
}

void QTkMainWindow::paintEvent(QPaintEvent *UNUSED(event))
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, *pixmap);
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

    privars->qapp = new QApplication(argc, argv);

    privars->pix = new QPixmap(dmp->dm_width, dmp->dm_height);
    privars->win = new QTkMainWindow(privars->pix, (WId)pubvars->win);
    privars->win->resize(dmp->dm_width, dmp->dm_height);

    privars->win->setAutoFillBackground(true);

    qt_configureWin(dmp, 1);

    qt_setBGColor(dmp, 0, 0, 0);
    privars->win->show();

    Tk_SetWindowBackground(pubvars->xtkwin, 0);
    Tk_MapWindow(pubvars->xtkwin);

    bu_log("Tk: %ld Qt: %ld\n", pubvars->win, privars->win->winId());
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

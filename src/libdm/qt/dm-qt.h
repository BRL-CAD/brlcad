/*                          D M -  Q T . H
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
/** @addtogroup libstruct dm */
/** @{ */
/** @file dm-qt.h
 *
 */

#ifndef DM_QT_H
#define DM_QT_H

#include "vmath.h"

#  include <QApplication>
#  include <QPainter>
#  include <QWindow>
#  include <QBackingStore>
#  include <QResizeEvent>
#  include <QImage>

class QTkMainWindow: public QWindow {

public:
    QTkMainWindow(QPixmap *p, QWindow *parent = 0, void *d = NULL);
    ~QTkMainWindow();

    virtual void render(QPainter *painter);
public slots:
    void renderNow();

protected:
    bool event(QEvent *event);

    void resizeEvent(QResizeEvent *event);
    void exposeEvent(QExposeEvent *event);

private:
    QPixmap *pixmap;
    QBackingStore *m_backingStore;
    bool m_update_pending;
    void *dmp;
};


struct qt_vars {
    QApplication *qapp;
    QWindow *parent;
    QTkMainWindow *win;
    QColor fg, bg;
    QPixmap *pix;
    QPainter *painter;
    QFont *font;
    fastf_t *qmat;
    mat_t mod_mat;      /* default model transformation matrix */
    mat_t disp_mat;     /* display transformation matrix */
    QImage *img;
    int drawFb;
};

struct qt_tk_bind {
    char* (*bind_function)(QEvent *event);
    const char *name;
};

struct dm_qtvars {
    int devmotionnotify;
    int devbuttonpress;
    int devbuttonrelease;
};

extern "C" struct dm dm_qt;

#endif /* DM_QT_H */

/** @} */
/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

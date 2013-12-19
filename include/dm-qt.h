/*                          D M -  Q T . H
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
/** @addtogroup libdm */
/** @{ */
/** @file dm-qt.h
 *
 */

#ifndef DM_QT_H
#define DM_QT_H

#include "vmath.h"

#ifdef DM_QT
#  include <QApplication>
#  include <QPainter>
#  include <QWindow>
#  include <QBackingStore>
#  include <QResizeEvent>

class QTkMainWindow: public QWindow {

public:
    QTkMainWindow(QPixmap *p, QWindow *parent = 0, struct dm *d = NULL);
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
    struct dm *dmp;
};


struct qt_vars {
    QApplication *qapp;
    QWindow *parent;
    QTkMainWindow *win;
    QColor fg, bg;
    QPixmap *pix;
    QPainter *painter;
    QFont *font;
    mat_t qmat;
};

struct qt_tk_bind {
    char* (*bind_function)(QEvent *event);
    const char *name;
};

#endif /* DM_QT */

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

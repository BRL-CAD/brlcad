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

#ifndef __DM_QT__
#define __DM_QT__

#ifdef DM_QT
#  include <QApplication>
#  include <QWidget>
#  include <QPainter>

class QTkMainWindow: public QWidget {

public:
    QTkMainWindow(QPixmap *pix, WId win);

protected:
    void paintEvent(QPaintEvent *event);

private:
    QPixmap *pixmap;

};

struct qt_vars {
    QApplication *qapp;
    QTkMainWindow *win;
    QColor fg, bg;
    QPixmap *pix;
    QPainter *painter;
};

#endif /* DM_QT */

#endif /* __DM_QT__ */

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

/*                   Q G D O C K W I D G E T . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file QgDockWidget.h
 *
 * Defines a QDockWidget customized for BRL-CAD usage.
 *
 */

#ifndef QGDOCKWIDGET_H
#define QGDOCKWIDGET_H
#include <QDockWidget>
#include "qtcad/defines.h"
#include "qtcad/QgModel.h"

// https://stackoverflow.com/q/44707344/2037687
class QTCAD_EXPORT QgDockWidget : public QDockWidget
{
    Q_OBJECT

    public:
	QgDockWidget(const QString &title, QWidget *parent);
	bool event(QEvent *e);

	QgModel *m = NULL;

    signals:
	void banner_click();

    public slots:
       void toWindow(bool floating);

    private:
        bool moving = false;
};

#endif /* QGDOCKWIDGET_H */

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */


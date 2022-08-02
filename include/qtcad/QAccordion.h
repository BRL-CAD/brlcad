/*                  Q A C C O R D I O N . H
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
/** @file QAccordionWidget.h
 *
 * Show one of a stack of widgets
 */

#ifndef QACCORDIANWIDGET_H
#define QACCORDIANWIDGET_H

#include "common.h"


#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>

#include "qtcad/defines.h"

class QTCAD_EXPORT QAccordionObject : public QWidget
{
    Q_OBJECT

    public:
	QAccordionObject(QWidget *pparent = 0, QWidget *object = 0, QString header_title = QString(""));
	~QAccordionObject();
	QPushButton *toggle;
	QScrollArea *objscrollarea;

    signals:
	void select(QAccordionObject *);

    public slots:
	void toggleVisibility();

    private:
	QVBoxLayout *objlayout;
	QString title;
};

class QTCAD_EXPORT QAccordion : public QWidget
{
    Q_OBJECT

    public:
	QAccordion(QWidget *pparent = 0);
	~QAccordion();
	void addObject(QAccordionObject *object);

    public slots:
	void open(QAccordionObject *);

    private:
        QSet<QAccordionObject *> objs;
        QAccordionObject *selected = NULL;
        QVBoxLayout *mlayout;
};

#endif /* QACCORDIANWIDGET_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */


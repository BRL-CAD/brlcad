/*                     Q C O L O R R G B . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file QColorRGB.h
 *
 * Set a color either via dialog or r/g/b text entry
 */

#ifndef QCOLORRGB_H
#define QCOLORRGB_H

#include "common.h"

#include <QColor>
#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <QColorDialog>
#include <QLineEdit>

#include "bu/color.h"
#include "qtcad/defines.h"

class QTCAD_EXPORT QColorRGB: public QWidget
{
    Q_OBJECT

    public:
	QColorRGB(QWidget *p = NULL, QString lstr = QString("Color:"), QColor dcolor = QColor(Qt::red));
	~QColorRGB();

	QLineEdit *rgbtext;
	QPushButton *rgbcolor;
	struct bu_color bc;

    private slots:
	void set_color_text();
	void set_button_color();

    private:
	QColorDialog *d;
        QHBoxLayout *mlayout;
	QColor qc;
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


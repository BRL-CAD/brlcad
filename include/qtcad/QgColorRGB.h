/*                     Q G C O L O R R G B . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2023 United States Government as represented by
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
/** @file QgColorRGB.h
 *
 * Set a color either via dialog or r/g/b text entry
 */

#ifndef QGCOLORRGB_H
#define QGCOLORRGB_H

#include "common.h"

#include <QColor>
#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <QColorDialog>
#include <QLineEdit>

#include "bu/color.h"
#include "bu/opt.h"
#include "qtcad/defines.h"

class QTCAD_EXPORT QgColorRGB: public QWidget
{
    Q_OBJECT

    public:
	QgColorRGB(QWidget *p = NULL, QString lstr = QString("Color:"), QColor dcolor = QColor(Qt::red));
	~QgColorRGB();

	QLineEdit *rgbtext;
	QPushButton *rgbcolor;
	struct bu_color bc;

    signals:
	void color_changed(struct bu_color *);

    public slots:
	void set_color_from_text();

    private slots:
	void set_color_from_button();

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


/*                        Q V I E W C T R L . H
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
/** @file QViewCtrl.h
 *
 * Array of buttons for toggling between various view control modes.
 * Provides only the buttons and the signals - must be connected up
 * to an actual view widget to impact a view
 */

#ifndef QVIEWCTRL_H
#define QVIEWCTRL_H

#include "common.h"

#include <QButtonGroup>
#include <QIcon>
#include <QImage>
#include <QPushButton>
#include <QWidget>
#include "qtcad/defines.h"
#include "qtcad/QFlowLayout.h"

// TODO - need a view image control widget for raytrace run, raytrace
// abort, overlay/underlay/off, and save scene image
//
// component picking, etc. will most likely need to be a tool - we need to
// be able to build up and manipulate selection sets, of which one is the
// active drawn set.  Operations needed include clearing first hit drawn
// instances under the mouse click, adding and removing objects from
// selection sets, and creating an initial set with a rectangle selection

#define VIEWCTL_SCA 0
#define VIEWCTL_ROT 1
#define VIEWCTL_TRA 2
#define VIEWCTL_CENTER 3

class QTCAD_EXPORT QViewCtrl : public QWidget
{
    Q_OBJECT

    public:
        QViewCtrl(QWidget *p);
        ~QViewCtrl();

	int icon_width;
	int icon_height;

	void resizeEvent(QResizeEvent *pevent);

	int icon_size = 50;

    signals:
	void mode_change(int);

    public slots:
	void do_sca();
	void do_rot();
	void do_tra();
	void do_center();

    private:
	QFlowLayout *bl;
	QButtonGroup *bg;

	QPushButton *sca;
	QPushButton *rot;
	QPushButton *tra;
	QPushButton *center;
};


#endif //QVIEWCTRL_H

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


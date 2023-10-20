/*                        Q G V I E W C T R L . H
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
/** @file QgViewCtrl.h
 *
 * Array of buttons for toggling between various view control modes.
 * Provides only the buttons and the signals - must be connected up
 * to an actual view widget to impact a view
 */

#ifndef QGVIEWCTRL_H
#define QGVIEWCTRL_H

#include "common.h"

#include <QToolBar>
#include <QWidget>
#include "ged.h"
#include "qtcad/defines.h"

// TODO - add save scene image
//
// component picking, etc. will most likely need to be a tool - we need to
// be able to build up and manipulate selection sets, of which one is the
// active drawn set.  Operations needed include clearing first hit drawn
// instances under the mouse click, adding and removing objects from
// selection sets, and creating an initial set with a rectangle selection

class QTCAD_EXPORT QgViewCtrl : public QToolBar
{
    Q_OBJECT

    public:
        QgViewCtrl(QWidget *p, struct ged *pgedp);
        ~QgViewCtrl();

	struct ged *gedp = NULL;
	int icon_size = 25;

    signals:
	void view_changed(unsigned long long);
	void lmouse_mode(int);

    public slots:

	void sca_mode();
	void rot_mode();
	void tra_mode();
	void center_mode();

	void fbclear_cmd();

	void fb_mode_cmd();

	void do_view_update(unsigned long long);

	// Unlike the other commands, the raytrace button
	// has to reflect a potentially long-running state -
	// that of the child rt process  Consequently the
	// icon controls need to be independent of the command
	// execution logic.
	void raytrace_cmd();
	void raytrace_start(int);
	void raytrace_done(int);

    public:
	// Left mouse behavior controls (when not using a tool or editing)
	QAction *sca;
	QAction *rot;
	QAction *tra;
	QAction *center;

	// Raytrace/framebuffer controls
	QAction *raytrace;
	QAction *fb_mode;
	QAction *fb_clear;

    private:
	bool raytrace_running = false;
	int pid = -1;
};


#endif //QGVIEWCTRL_H

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


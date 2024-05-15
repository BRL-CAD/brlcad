/*                   S W R A S T W I N . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2024 United States Government as represented by
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
/** @file main_window.h
 *
 * Defines the toplevel Qt window for a stand-alone swrast dm.
 *
 */

#ifndef QTSWRASTWIN_H
#define QTSWRASTWIN_H

#include <QMainWindow>
#include "dm.h"
#include "qtcad/QgSW.h"

class QgSWWin : public QMainWindow
{
    Q_OBJECT
    public:
	QgSWWin(struct fb *fbp);
	QgSW *canvas = NULL;
};

#endif /* QTSWRASTWIN_H */

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


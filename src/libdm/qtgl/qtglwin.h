/*                     Q T G L W I N . H
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
/** @file main_window.h
 *
 * Defines the toplevel window for a stand-alone QtGL dm.
 *
 */

#ifndef QTGLWIN_H
#define QTGLWIN_H

#include <QMainWindow>
#include "dm.h"
#include "qtcad/QtGL.h"

class QtGLWin : public QMainWindow
{
    Q_OBJECT
    public:
      QtGLWin(struct fb *fbp);

      QtGL *getCanvas() { return canvas; }
    private:
	QtGL *canvas = NULL;
};

#endif /* QTGLWIN_H */

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


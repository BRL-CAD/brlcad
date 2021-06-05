/*                       C A D V I E W . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file cadview.h
 *
 * Brief description
 *
 */

#ifndef CAD_VIEW_H
#define CAD_VIEW_H

#include <QImage>
#include <QObject>
#include <QGLWidget>

#ifndef Q_MOC_RUN
#include "bu/avs.h"
#include "raytrace.h"
#include "ged.h"
#endif

class CADView : public QWidget
{
    Q_OBJECT

    public:
	CADView(QWidget *pparent = 0, int type = 0);
	~CADView();

    private:
	QGLWidget *canvas;
};

#endif /*CAD_VIEW_H*/

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */


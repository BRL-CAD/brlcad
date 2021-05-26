/*                      Q T G L Q U A D . H
 * BRL-CAD
 *
 * Copyright (c) 2021 United States Government as represented by
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
/** @file QtGLQuad.h
 *
 */

#ifndef QTGLQUAD_H
#define QTGLQUAD_H

#include "common.h"

#include <QWidget>

extern "C" {
#include "bu/ptbl.h"
#include "bv.h"
#include "dm.h"
}

#include "qtcad/defines.h"
#include "qtcad/QtGL.h"

class QTCAD_EXPORT QtGLQuad : public QWidget
{
    Q_OBJECT

    public:
	explicit QtGLQuad(QWidget *parent = nullptr);
	~QtGLQuad();

	QtGL *get(int quadrant_num);
	void select(int quadrant_num);
	void select(const char *id); // valid inputs: ur, ul, ll and lr

	QtGL *ur = NULL; // Quadrant 1
	QtGL *ul = NULL; // Quadrant 2
	QtGL *ll = NULL; // Quadrant 3
	QtGL *lr = NULL; // Quadrant 4

	QtGL *c = NULL; // Currently selected quadrant

    public slots:
	void need_update();
};

#endif /* QTGLQUAD_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


/*                  C A D A C C O R D I O N . H
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
/** @file cadaccordion.h
 *
 * Brief description
 *
 */

#ifndef CADPALETTES_H
#define CADPALETTES_H
#include "qtcad/QToolPalette.h"

class CADViewControls : public QWidget
{
    Q_OBJECT

    public:
	CADViewControls(QWidget *pparent = 0);
	~CADViewControls();

    private:
	QToolPalette *tpalette;
	QWidget *info_view;
};

class CADInstanceEdit : public QWidget
{
    Q_OBJECT

    public:
	CADInstanceEdit(QWidget *pparent = 0);
	~CADInstanceEdit();

    private:
	QToolPalette *tpalette;
	QWidget *info_view;
};

class CADPrimitiveEdit : public QWidget
{
    Q_OBJECT

    public:
	CADPrimitiveEdit(QWidget *pparent = 0);
	~CADPrimitiveEdit();

    private:
	QToolPalette *tpalette;
	QWidget *shape_properties;
};

#endif /* CADPALETTES_H */

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


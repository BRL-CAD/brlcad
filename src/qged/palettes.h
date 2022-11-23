/*                    C A D P A L E T T E . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file cadpalette.h
 *
 * Brief description
 *
 */

#ifndef CADPALETTES_H
#define CADPALETTES_H
#include <iostream>
#include "qtcad/QToolPalette.h"

class CADPalette : public QWidget
{
    Q_OBJECT

    public:
	CADPalette(int mode = 0, QWidget *pparent = 0);
	~CADPalette();

	void addTool(QToolPaletteElement *e);

	QToolPalette *tpalette;

    signals:
	void current(QWidget *);
	void interaction_mode(int);

    public slots:
	void reflow();
	void makeCurrent(QWidget *);

    private:
	int m_mode = 0;
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


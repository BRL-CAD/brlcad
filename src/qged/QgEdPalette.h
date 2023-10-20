/*                    Q G E D P A L E T T E . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2023 United States Government as represented by
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
/** @file QgEdPalette.h
 *
 * The main job of QgEdPalette is to handle the population of the
 * QgToolPalette widget with the QGED tools supplied by plugins.
 */

#ifndef QGEDPALETTE_H
#define QGEDPALETTE_H
#include <iostream>
#include "qtcad/QgToolPalette.h"

class QgEdPalette : public QgToolPalette
{
    Q_OBJECT

    public:
	QgEdPalette(int mode = 0, QWidget *pparent = 0);
	~QgEdPalette();

    signals:
	void current(QWidget *);
	void interaction_mode(int);

    public slots:
	void makeCurrent(QWidget *);

    private:
	int m_mode = 0;
};

#endif /* QGEDPALETTE_H */

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


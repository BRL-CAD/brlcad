/*                   Q G E D P A L E T T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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
/** @file QgEdPalette.cpp
 *
 */

#include <QColor>
#include "QgEdPalette.h"

QgEdPalette::QgEdPalette(int mode, QWidget *pparent)
    : QgToolPalette(pparent)
{
    m_mode = mode;
    /* Tool elements are added by QgPaletteController::populate() from
     * Qt-native IQgToolFactory plugins. */
}

void
QgEdPalette::makeCurrent(QWidget *w)
{
    QTCAD_SLOT("QgEdPalette::makeCurrent", 1);
    if (w == this) {
	if (selectedElement())
	    palette_displayElement(selectedElement());
	emit interaction_mode(m_mode);
    } else {
	if (selectedElement())
	    selectedElement()->buttonWidget()->setStyleSheet("");
    }
}

QgEdPalette::~QgEdPalette()
{
}

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

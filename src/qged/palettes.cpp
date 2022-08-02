/*                   C A D P A L E T T E . C P P
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
/** @file cadpalette.cpp
 *
 * Brief description
 *
 */

#include <QColor>
#include "palettes.h"

CADPalette::CADPalette(int mode, QWidget *pparent)
    : QWidget(pparent)
{
    m_mode = mode;
    QVBoxLayout *mlayout = new QVBoxLayout();
    mlayout->setSpacing(0);
    mlayout->setContentsMargins(0,0,0,0);
    tpalette = new QToolPalette(this);
    tpalette->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    mlayout->addWidget(tpalette);

    tpalette->selected_style = QString("border: 1px solid rgb(255, 255, 0)");

    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    this->setLayout(mlayout);
}

void
CADPalette::addTool(QToolPaletteElement *e)
{
    tpalette->addElement(e);
}

void
CADPalette::makeCurrent(QWidget *w)
{
    if (w == this) {
	if (this->tpalette->selected)
	    this->tpalette->displayElement(this->tpalette->selected);
	emit interaction_mode(m_mode);
    } else {
	if (this->tpalette->selected)
	    this->tpalette->selected->button->setStyleSheet("");
    }
}

void
CADPalette::reflow()
{
    tpalette->button_layout_resize();
}

CADPalette::~CADPalette()
{
    delete tpalette;
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


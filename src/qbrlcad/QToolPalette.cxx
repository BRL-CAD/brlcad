/*                Q T O O L P A L E T T E . C X X
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @file QToolPalette.cxx
 *
 * Qt Tool Palette implementation
 *
 */

#include <iostream>
#include "QToolPalette.h"

QToolPaletteButton::QToolPaletteButton(QWidget *bparent, QIcon *iicon, QToolPaletteElement *eparent) : QPushButton(bparent)
{
    setIcon(*iicon);
    element = eparent;
    QObject::connect(this, SIGNAL(clicked()), this, SLOT(select_element()));
}


void
QToolPaletteButton::select_element()
{
   emit element_selected(element);
}

void
QToolPaletteButton::setButtonElement(QIcon *iicon, QToolPaletteElement *n_element)
{
    setIcon(*iicon);
    element = n_element;
}


QToolPaletteElement::QToolPaletteElement(QWidget *eparent, QIcon *iicon, QWidget *control) : QWidget(eparent)
{
    button = new QToolPaletteButton(this, iicon, this);
    button->setCheckable(true);
    controls = control;
}

QToolPaletteElement::~QToolPaletteElement()
{
    delete button;
}

void
QToolPaletteElement::setButton(QToolPaletteButton *n_button)
{
    button = n_button;
}

void
QToolPaletteElement::setControls(QWidget *n_control)
{
    controls = n_control;
}


QToolPalette::QToolPalette(QWidget *pparent) : QWidget(pparent)
{
    icon_width = 30;
    icon_height = 30;
    columns = 6;
    QVBoxLayout *mlayout = new QVBoxLayout();
    mlayout->setSpacing(0);
    mlayout->setContentsMargins(1,1,1,1);

    splitter = new QSplitter();
    splitter->setOrientation(Qt::Vertical);
    splitter->setChildrenCollapsible(false);
    mlayout->addWidget(splitter);

    button_container = new QWidget();
    //button_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    button_layout = new QFlowLayout();
    button_layout->setHorizontalSpacing(0);
    button_layout->setVerticalSpacing(0);
    button_layout->setContentsMargins(0,0,0,0);
    button_container->setMinimumHeight(icon_height);
    button_container->setMinimumWidth(icon_width * columns);
    button_container->setLayout(button_layout);
    button_container->show();
    control_container = new QWidget();
    //control_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    control_layout = new QVBoxLayout();
    control_layout->setSpacing(0);
    control_layout->setContentsMargins(0,0,0,0);
    control_container->setLayout(control_layout);
    control_container->setMinimumHeight(icon_height);
    control_container->setMinimumWidth(icon_width * columns);
    control_container->show();

    splitter->addWidget(button_container);
    splitter->setStretchFactor(0, 0);
    splitter->addWidget(control_container);
    splitter->setStretchFactor(1, 1);

    selected = NULL;

    this->setLayout(mlayout);
}

QToolPalette::~QToolPalette()
{
    delete splitter;
}

void
QToolPalette::setIconWidth(int iwidth)
{
    icon_width = iwidth;
    foreach(QToolPaletteElement *el, elements) {
	el->button->setMinimumWidth(icon_height);
	el->button->setMaximumWidth(icon_height);
    }

}

void
QToolPalette::setIconHeight(int iheight)
{
    icon_height = iheight;
    foreach(QToolPaletteElement *el, elements) {
	el->button->setMinimumHeight(icon_height);
	el->button->setMaximumHeight(icon_height);
    }
}

void
QToolPalette::addElement(QToolPaletteElement *element)
{
    element->button->setMinimumWidth(icon_width);
    element->button->setMaximumWidth(icon_width);
    element->button->setMinimumHeight(icon_height);
    element->button->setMaximumHeight(icon_height);
    button_layout->addWidget(element->button);
    elements.insert(element);
    QObject::connect(element->button, SIGNAL(element_selected(QToolPaletteElement *)), this, SLOT(displayElement(QToolPaletteElement *)));
}

void
QToolPalette::deleteElement(QToolPaletteElement *element)
{
    elements.remove(element);
    if (selected == element) {
	displayElement(*elements.begin());
    }
    button_layout->removeWidget(element->button);
    delete element;
}

void
QToolPalette::displayElement(QToolPaletteElement *element)
{
    if (element) {
	if (element == selected) {
	    if (element->button->isChecked()) element->button->setChecked(false);
	    element->controls->hide();
	    selected = NULL;
	} else {
	    if (!element->button->isChecked()) element->button->setChecked(true);
	    if (selected) {
		control_layout->removeWidget(selected->controls);
		selected->controls->hide();
		if (selected->button->isChecked()) selected->button->setChecked(false);
	    }
	    control_layout->addWidget(element->controls);
	    element->controls->show();
	    selected = element;
	    foreach(QToolPaletteElement *el, elements) {
		if (el != selected)	el->button->setDown(false);
	    }
	}
    }
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */


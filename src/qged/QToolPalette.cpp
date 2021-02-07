/*                Q T O O L P A L E T T E . C X X
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
    controls->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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
    always_selected = 1;
    icon_width = 30;
    icon_height = 30;
    QVBoxLayout *mlayout = new QVBoxLayout();
    mlayout->setSpacing(0);
    mlayout->setContentsMargins(1,1,1,1);

    splitter = new QSplitter();
    splitter->setOrientation(Qt::Vertical);
    splitter->setChildrenCollapsible(false);
    mlayout->addWidget(splitter);

    button_container = new QWidget();
    button_layout = new QFlowLayout();
    button_layout->setHorizontalSpacing(0);
    button_layout->setVerticalSpacing(0);
    button_layout->setContentsMargins(0,0,0,0);
    button_container->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button_container->setLayout(button_layout);
    control_container = new QWidget();
    control_layout = new QVBoxLayout();
    control_layout->setSpacing(0);
    control_layout->setContentsMargins(0,0,0,0);
    control_container->setLayout(control_layout);
    control_container->setMinimumHeight(icon_height);
    control_container->setMinimumWidth(icon_width);
    control_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    splitter->addWidget(button_container);
    splitter->setStretchFactor(0, 1);
    splitter->addWidget(control_container);
    splitter->setStretchFactor(1, 100000);
    splitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    selected = NULL;

    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(mlayout);
}

QToolPalette::~QToolPalette()
{
    delete splitter;
}

void
QToolPalette::button_layout_resize()
{
    button_container->setMinimumWidth(control_container->minimumWidth());
    div_t layout_dim = div(button_container->size().width()-1, icon_width);
    div_t layout_grid = div(elements.count(), layout_dim.quot);
    if (layout_grid.rem > 0) {
	button_container->setMinimumHeight((layout_grid.quot + 1) * icon_height);
	button_container->setMaximumHeight((layout_grid.quot + 1) * icon_height);
    } else {
	button_container->setMinimumHeight((layout_grid.quot) * icon_height);
	button_container->setMaximumHeight((layout_grid.quot) * icon_height);
    }
}

void
QToolPalette::resizeEvent(QResizeEvent *pevent)
{
    QWidget::resizeEvent(pevent);
    button_layout_resize();
}

void
QToolPalette::setIconWidth(int iwidth)
{
    icon_width = iwidth;
    foreach(QToolPaletteElement *el, elements) {
	el->button->setMinimumWidth(icon_height);
	el->button->setMaximumWidth(icon_height);
    }
    updateGeometry();
}

void
QToolPalette::setIconHeight(int iheight)
{
    icon_height = iheight;
    foreach(QToolPaletteElement *el, elements) {
	el->button->setMinimumHeight(icon_height);
	el->button->setMaximumHeight(icon_height);
    }
    updateGeometry();
}


void
QToolPalette::setAlwaysSelected(int toggle)
{
    always_selected = toggle;
    if (always_selected && selected == NULL) {
	displayElement(*(elements.begin()));
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
    updateGeometry();
    if (!selected && always_selected) {
	displayElement(element);
    }
}

void
QToolPalette::deleteElement(QToolPaletteElement *element)
{
    elements.remove(element);
    if (selected == element) {
	displayElement(*elements.begin());
    }
    button_layout->removeWidget(element->button);
    updateGeometry();
    delete element;
}

void
QToolPalette::displayElement(QToolPaletteElement *element)
{
    if (element) {
	if (element == selected && !always_selected) {
	    if (element->button->isChecked()) element->button->setChecked(false);
	    element->controls->hide();
	    control_container->setMinimumHeight(icon_height);
	    selected = NULL;
	} else {
	    if (!element->button->isChecked()) element->button->setChecked(true);
	    if (selected && element != selected) {
		control_layout->removeWidget(selected->controls);
		selected->controls->hide();
		if (selected->button->isChecked()) selected->button->setChecked(false);
	    }
	    control_layout->addWidget(element->controls);
	    control_container->setMinimumHeight(element->controls->minimumHeight());
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


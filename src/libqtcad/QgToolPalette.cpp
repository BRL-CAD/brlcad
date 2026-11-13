/*                Q G T O O L P A L E T T E . C X X
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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
/** @file QgToolPalette.cxx
 *
 * Qt Tool Palette implementation
 *
 */

#include "common.h"

#include <algorithm>
#include <QScrollBar>
#include "qtcad/QgToolPalette.h"

QgToolPaletteButton::QgToolPaletteButton(QWidget *bparent, QIcon *iicon, QgToolPaletteElement *eparent) : QPushButton(bparent)
{
    if (iicon)
	setIcon(*iicon);
    element = eparent;
    QObject::connect(this, &QgToolPaletteButton::clicked, this, &QgToolPaletteButton::select_element);
}


void
QgToolPaletteButton::select_element()
{
   QTCAD_SLOT("QgToolPaletteButton::select_element", 1);
   emit element_selected(element);
}

void
QgToolPaletteButton::setButtonElement(QIcon *iicon, QgToolPaletteElement *n_element)
{
    if (iicon)
	setIcon(*iicon);
    element = n_element;
}


QgToolPaletteElement::QgToolPaletteElement(QIcon *iicon, QWidget *control)
{
    button = new QgToolPaletteButton(this, iicon, this);
    button->setCheckable(true);
    controls = control;
    if (controls)
	controls->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

QgToolPaletteElement::~QgToolPaletteElement()
{
    delete button.data();
    delete controls.data();
}

void
QgToolPaletteElement::element_view_changed(QgViewUpdateFlags flags)
{
    QTCAD_SLOT("QgToolPaletteElement::element_view_changed", 1);
    emit view_changed(flags);
}

void
QgToolPaletteElement::do_view_update(QgViewUpdateFlags flags)
{
    QTCAD_SLOT("QgToolPaletteElement::do_view_update", 1);
    // TODO - do any element level updating (button highlighting?)
    emit element_view_update(flags);
}

void
QgToolPaletteElement::do_element_unhide(void *)
{
    QTCAD_SLOT("QgToolPaletteElement::do_element_unhide", 1);
    emit element_unhide();
}

QgToolPalette::QgToolPalette(QWidget *pparent) : QWidget(pparent)
{
    always_selected = 1;
    icon_width = 30;
    icon_height = 30;
    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(0);
    layout->setContentsMargins(1,1,1,1);


    button_container = new QWidget();
    button_layout = new QgFlowLayout();
    button_layout->setHorizontalSpacing(0);
    button_layout->setVerticalSpacing(0);
    button_layout->setContentsMargins(0,0,0,0);
    button_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    button_container->setMinimumHeight(icon_height);
    button_container->setMinimumWidth(icon_width*5+1);
    button_container->setLayout(button_layout);

    control_container = new QScrollArea();
    control_container->setWidgetResizable(true);
    layout->addWidget(button_container);
    layout->addWidget(control_container);

    selected = NULL;

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    setLayout(layout);

    selected_style = QString("border: 1px solid rgb(255, 255, 0)");
}

QgToolPalette::~QgToolPalette()
{
}

void
QgToolPalette::button_layout_resize()
{
    QTCAD_SLOT("QgToolPalette::button_layout_resize", 1);
    const int columns = std::max(1,
	(button_container->size().width() - 1) / icon_width);
    div_t layout_grid = div((int)elements.count(), columns);
    if (layout_grid.rem > 0) {
	button_container->setMinimumHeight((layout_grid.quot + 1) * icon_height);
	button_container->setMaximumHeight((layout_grid.quot + 1) * icon_height);
    } else {
	button_container->setMinimumHeight((layout_grid.quot) * icon_height);
	button_container->setMaximumHeight((layout_grid.quot) * icon_height);
    }
}

void
QgToolPalette::resizeEvent(QResizeEvent *pevent)
{
    QWidget::resizeEvent(pevent);
    button_layout_resize();
}

void
QgToolPalette::setIconWidth(int iwidth)
{
    icon_width = std::max(1, iwidth);
    foreach(QgToolPaletteElement *el, elements) {
	el->buttonWidget()->setMinimumWidth(icon_width);
	el->buttonWidget()->setMaximumWidth(icon_width);
    }
    updateGeometry();
}

void
QgToolPalette::setIconHeight(int iheight)
{
    icon_height = std::max(1, iheight);
    foreach(QgToolPaletteElement *el, elements) {
	el->buttonWidget()->setMinimumHeight(icon_height);
	el->buttonWidget()->setMaximumHeight(icon_height);
    }
    updateGeometry();
}


void
QgToolPalette::setAlwaysSelected(int toggle)
{
    always_selected = toggle;
    if (always_selected && selected == NULL && !elements.isEmpty()) {
	palette_displayElement(*(elements.begin()));
    }
}

void
QgToolPalette::do_view_update(QgViewUpdateFlags flags)
{
    QTCAD_SLOT("QgToolPalette::do_element_unhide", 1);
    emit palette_view_update(flags);
}


void
QgToolPalette::palette_do_view_changed(QgViewUpdateFlags flags)
{
    QTCAD_SLOT("QgToolPalette::palette_do_view_changed", 1);
    emit view_changed(flags);
}

void
QgToolPalette::addElement(QgToolPaletteElement *element)
{
    if (!element || !element->buttonWidget() || elements.contains(element))
	return;
    element->setParent(this);
    element->buttonWidget()->setMinimumWidth(icon_width);
    element->buttonWidget()->setMaximumWidth(icon_width);
    element->buttonWidget()->setMinimumHeight(icon_height);
    element->buttonWidget()->setMaximumHeight(icon_height);
    button_layout->addWidget(element->buttonWidget());
    elements.insert(element);

    QObject::connect(element->buttonWidget(), &QgToolPaletteButton::element_selected, this, &QgToolPalette::palette_displayElement);

    QObject::connect(this, &QgToolPalette::palette_view_update, element, &QgToolPaletteElement::do_view_update);
    QObject::connect(element, &QgToolPaletteElement::view_changed, this, &QgToolPalette::palette_do_view_changed);


    updateGeometry();
    if (!selected && always_selected) {
	palette_displayElement(element);
	selected->buttonWidget()->setStyleSheet("");
    }
}

void
QgToolPalette::deleteElement(QgToolPaletteElement *element,
	bool selectReplacement)
{
    if (!element || !elements.contains(element))
	return;
    const bool wasSelected = selected == element;
    if (wasSelected) {
	if (control_container->widget() == element->controlsWidget())
	    control_container->takeWidget();
	selected = NULL;
    }
    elements.remove(element);
    button_layout->removeWidget(element->buttonWidget());
    delete element;
    if (wasSelected) {
	if (selectReplacement && always_selected && !elements.isEmpty())
	    palette_displayElement(*elements.begin());
	else
	    emit palette_element_selected(NULL);
    }
    button_layout_resize();
    updateGeometry();
}

void
QgToolPalette::clearSelection()
{
    if (!selected)
	return;

    QgToolPaletteElement *old_selected = selected;
    if (control_container->widget() == old_selected->controlsWidget())
	control_container->takeWidget();
    if (old_selected->controlsWidget())
	old_selected->controlsWidget()->hide();
    if (old_selected->buttonWidget()) {
	old_selected->buttonWidget()->setChecked(false);
	old_selected->buttonWidget()->setDown(false);
	old_selected->buttonWidget()->setStyleSheet("");
    }
    selected = NULL;
    emit palette_element_selected(NULL);
}

void
QgToolPalette::palette_displayElement(QgToolPaletteElement *element)
{
    QTCAD_SLOT("QgToolPalette::palette_displayElement", 1);
    if (element) {
	QWidget *controls = element->controlsWidget();
	if (element == selected) {
	    if (!always_selected) {
		if (element->buttonWidget()->isChecked()) element->buttonWidget()->setChecked(false);
		if (controls)
		    controls->hide();
		selected = NULL;
	    } else {
		element->buttonWidget()->setStyleSheet(selected_style);
	    }
	} else {
	    if (!element->buttonWidget()->isChecked()) element->buttonWidget()->setChecked(true);
	    if (selected && element != selected) {
		selected->setScrollPosition(control_container->verticalScrollBar()->sliderPosition());
		if (selected->controlsWidget())
		    selected->controlsWidget()->hide();
		if (selected->buttonWidget()->isChecked()) selected->buttonWidget()->setChecked(false);
	    }
	    control_container->takeWidget();
	    if (controls) {
		control_container->setWidget(controls);
		controls->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
		controls->show();
	    }
	    element->do_element_unhide(NULL);
	    control_container->verticalScrollBar()->setSliderPosition(element->scrollPosition());
	    selected = element;
	    foreach(QgToolPaletteElement *el, elements) {
		if (el != selected) {
		    el->buttonWidget()->setDown(false);
		    el->buttonWidget()->setStyleSheet("");
		} else {
		    el->buttonWidget()->setStyleSheet(selected_style);
		}
	    }
	}
	emit palette_element_selected(selected);
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8



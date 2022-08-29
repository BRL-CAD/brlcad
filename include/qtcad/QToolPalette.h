/*                  Q T O O L P A L E T T E . H
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
/** @file QToolPalette.h
 *
 * Signals/slots in Palettes and Elements
 *
 * In order to keep the scope of the various plugins local, we define
 * a tiered system of signals and slots which are connected to by
 * various aspects of this system:
 *
 * The QToolPalette widget is what is directly added to application
 * widget hierarchies, and it is that widget's signals and slots which
 * are connected to the top level view updating signal/slot system as
 * articulated in SignalFlags.h.  Individual elements (i.e.tools) are
 * connected to QToolPalette, and the implementation specific "guts"
 * of the various tools connect to their parent element.
 *
 */

#ifndef QTOOLPALETTE_H
#define QTOOLPALETTE_H

#include "common.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSet>
#include <QIcon>
#include <QScrollArea>
#include <QSplitter>
#include "qtcad/defines.h"
#include "qtcad/QFlowLayout.h"
#include "qtcad/SignalFlags.h"

class QTCAD_EXPORT QToolPaletteElement;

class QTCAD_EXPORT QToolPaletteButton: public QPushButton
{
    Q_OBJECT

    public:
	QToolPaletteButton(QWidget *bparent, QIcon *iicon = 0, QToolPaletteElement *eparent = 0);
	~QToolPaletteButton(){};

	void setButtonElement(QIcon *iicon, QToolPaletteElement *n_element);

    public slots:
	void select_element();

    signals:
	void element_selected(QToolPaletteElement *);

    private:
	QToolPaletteElement *element;
};


class QTCAD_EXPORT QToolPaletteElement: public QWidget
{
    Q_OBJECT

    public:
	QToolPaletteElement(QIcon *iicon = 0, QWidget *control = 0);
	~QToolPaletteElement();

	void setButton(QToolPaletteButton *n_button);
	void setControls(QWidget *n_controls);

    signals:
	// PUBLIC, for palette:
	// Signal the application can listen to to see if the Element has
	// changed anything in the view.  Emitted by element_do_view_update slot,
	// which is connected to internal widget signals in the controls. This
	// provide a generic, public "signal interface" for widget internals.
	void view_changed(unsigned long long);

    public slots:
	// PUBLIC, for palette:
	// These slots are intended to be connected to parent signals when the
	// Element is added to a Palette.  They will in turn emit the local
	// signals below for internal Element use.  These slots are used to
	// hide any internal signal/slot implementation details from the
	// application while still allowing changes at the app level to drive
	// updates in the Element contents.
	void do_view_update(unsigned long long);


	void do_element_unhide(void *);

     signals:
	// INTERNAL:
	// These signals are emitted by the below slots.  Subcomponents will
	// connect to these in lieu of connecting directly to application
	// signals, to decouple the implementation of the Element internals
	// from the application.  I.e. the application connects to the above
	// slots, and internal connections listen for the emission of the below
	// signals in lieu of the application signals.
	//
	// Note that these signals should NEVER be emitted directly by any of
	// the Element subcomponents.  Nor should they be connected to by
	// application code.
	void element_view_update(unsigned long long);

	void element_unhide();

    public slots:
	// INTERNAL:
	void element_view_changed(unsigned long long);

    public:
	QToolPaletteButton *button;
	QWidget *controls;
	int scroll_pos = 0;

	bool use_event_filter = false;

};

class QTCAD_EXPORT QToolPalette: public QWidget
{
    Q_OBJECT

    public:
	QToolPalette(QWidget *pparent = 0);
	~QToolPalette();
	void addElement(QToolPaletteElement *element);
	void deleteElement(QToolPaletteElement *element);
	void setIconWidth(int iwidth);
	void setIconHeight(int iheight);
	void setAlwaysSelected(int iheight);  // If 0 can disable all tools, if 1 some tool is always selected

	void resizeEvent(QResizeEvent *pevent);

	QToolPaletteElement *selected;
	QString selected_style = QString("");

	QVBoxLayout *mlayout;

   signals:

	// PUBLIC, for parent application:
	// Signal the application can listen to to see if any Element has
	// changed anything in the view.
	void view_changed(unsigned long long);


	// INTERNAL, for elements:
	// Emitted when an element is selected.
	void palette_element_selected(QToolPaletteElement *);

	// INTERNAL, for elements:
	// Emitted when the palette gets word of an app level view change.
	// This is used so elements don't have to connect directly to the
	// parent applications view changed signal (which they can't do without
	// knowing about the parent applications top level class, or requiring
	// the application to individually connect directly to each element.)
	void palette_view_update(unsigned long long);

   public slots:
       // PUBLIC, for parent application:
       // Trigger any needed updates in any elements in response to an
       // app level view change. 
       void do_view_update(unsigned long long);


        // INTERNAL
        // TODO - I think we're going to need an activateElement and drawElement
	// distinction here for editing - we will want the current panel shown
	// (and highlighted button, if we can figure out how to highlight without
	// selecting) to reflect the most recent selection, and we'll probably
	// want to highlight both the comb and appropriate solid button when when
	// we're selecting  instances in the tree, but we don't want to kick in
	// the event filters and create the edit wireframes until one of the buttons
	// is actually selected.
	void palette_displayElement(QToolPaletteElement *);
	void button_layout_resize();
	void palette_do_view_changed(unsigned long long);

    private:
	int always_selected;
	int icon_width;
	int icon_height;
	QSplitter *splitter;
	QWidget *button_container;
	QFlowLayout *button_layout;
	QScrollArea *control_container;
	QSet<QToolPaletteElement *> elements;
};

#endif //QTOOLPALETTE_H

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */


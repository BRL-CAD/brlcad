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
 * Brief description
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
	// PUBLIC:
	// Signal the application can listen to to see if the Element has
	// changed anything in the view.  Emitted by signal_view_update slot,
	// which is connected to internal widget signals in the controls. This
	// provide a generic, public "signal interface" for widget internals.
	void view_changed(struct bview **);

	// Signal the application can listen to to see if the Element has
	// changed anything in the database.
	void db_changed();

    public slots:
	// PUBLIC:
	// These slots are intended to be connected to parent signals when the
	// Element is added to a Palette.  They will in turn emit the local
	// signals below for internal Element use.  These slots are used to
	// hide any internal signal/slot implementation details from the
	// application while still allowing changes at the app level to drive
	// updates in the Element contents.
	void do_view_sync(struct bview **);
	void do_db_sync(void *);
	void do_palette_unhide(void *);

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
	void app_view_sync(struct bview **);
	void app_db_sync(void *);
	void palette_unhide();

    public slots:
	// INTERNAL:
	// The following slots are for connecting to by internal control widget
	// signals when the widget makes a view change, and handle emission
	// of the public facing signals.
	void do_view_changed(struct bview **);
        void do_db_changed();

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
	// Emitted when an element is selected.  The app's view needs to have
	// the correct filter for the current tool (if defined) or at least
	// remove any old filter from the previous tool.
	void element_selected(QToolPaletteElement *);

   public slots:
	void displayElement(QToolPaletteElement *);
	void button_layout_resize();

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


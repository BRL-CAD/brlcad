/*              Q A C C O R D I O N W I D G E T . H
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
/** @file QAccordionWidget.h
 *
 * Provides a structured container into which tool panels or objects can be
 * added and then shown or hidden with a controlling button.  Can optionally
 * enforce a rule that at most one object is open at a time.
 *
 * TODO - need to switch the custom resizing code that tries to "fill" the
 * column to including the stack of widgets in a scroll area, instead.
 * Need to spell out in detail what expected behavior is, but what we're
 * currently trying to do here isn't really workable.  What we need to do:
 *
 * 1.  Default added widget to its preferred size.  That may mean for the
 * default all-open stack we're taller than the window, which is why we
 * need the scroll area.  Hide overall column scrollbars until we need them.
 *
 * 2.  For each open widget, store its last size (if the user resizes the
 * splitter position) so we can remember what size to re-open it to if
 * we close it.  Unlike current behavior, this should only change if the
 * palette tool options minimum size requires growth or the user resizes
 * the window.
 *
 * 3.  If we shrink or expand a widget via the splitter bar, move widgets
 * below in the stack down or up rather than resizing the widget immediately
 * below the splitter being moved.  (i.e. we want widget resizing to
 * be local - the current non-local impact is just too messy.  However,
 * we DO want to allow that interactive resizing, and so far I'm not
 * seeing a pre-existing widget that will do so...)
 *
 * Crazy idea - noticed more or less by accident that stacking multiple dock
 * widgets in the right dock produces more or less the desired behavior.  What
 * if we make a second QMainWindow (https://stackoverflow.com/questions/318641/multiple-qmainwindow-instances)
 * with a placeholder main widget, disable docking in all but one of the docks,
 * and turn all the individual palettes into first class dockable widgets?  Then we put
 * the QMainWindow into a widget and make it a dockable widget into the actual
 * main window?  I.e., a dockable dock?  That way we could float any palette
 * we want to float, and rather than locking in the structure of the edit
 * panel things would be arbitrarily reconfigurable.  (The latter might be
 * of real help when defining other applications using the same gui elements...)
 */

#ifndef QACCORDIANWIDGET_H
#define QACCORDIANWIDGET_H

#include "common.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSet>
#include <QMap>
#include <QScrollArea>
#include <QSplitter>
#include <QSpacerItem>

#include "qtcad/defines.h"

class QTCAD_EXPORT QAccordionWidget;

class QTCAD_EXPORT QAccordionObject : public QWidget
{
    Q_OBJECT

    public:
	QAccordionObject(QWidget *pparent = 0, QWidget *object = 0, QString header_title = QString(""));
	~QAccordionObject();
	void setWidget(QWidget *object);
	QWidget *getWidget();
	void setVisibility(int val);
	QPushButton *toggle;
	QVBoxLayout *objlayout;
	int visible;
	int idx;

    signals:
	void made_visible(QAccordionObject *);
	void made_hidden(QAccordionObject *);

    public slots:
	void toggleVisibility();

    private:
	QString title;
	QWidget *child_object;
};

class QTCAD_EXPORT QAccordionWidget : public QWidget
{
    Q_OBJECT

    public:
	QAccordionWidget(QWidget *pparent = 0);
	~QAccordionWidget();
	void addObject(QAccordionObject *object);
	void insertObject(int idx, QAccordionObject *object);
	void deleteObject(QAccordionObject *object);

	void setUniqueVisibility(int val);
	int count();

    public slots:
	void update_sizes(int, int);
	void stateUpdate(QAccordionObject *);

    public:
	int unique_visibility;
	QAccordionObject *open_object;
	QSplitter *splitter;

    private:
	QSet<QAccordionObject *> objects;
	QMap<QString, QList<int> > size_states;
	QSpacerItem *buffer;
};

#endif /* QACCORDIANWIDGET_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */


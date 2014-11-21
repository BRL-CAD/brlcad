/*              Q A C C O R D I A N W I D G E T . H
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
/** @file QAccordianWidget.h
 *
 * Provides a structured container into which tool panels or objects can be
 * added and then shown or hidden with a controlling button.  Can optionally
 * enforce a rule that at most one object is open at a time.
 *
 */

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSet>
#include <QMap>
#include <QScrollArea>
#include <QSplitter>

class QAccordianWidget;

class QAccordianObject : public QWidget
{
    Q_OBJECT

    public:
	QAccordianObject(QWidget *pparent = 0, QWidget *object = 0, QString header_title = QString(""));
	~QAccordianObject();
	void setWidget(QWidget *object);
	QWidget *getWidget();
	void setVisibility(int val);
	QPushButton *toggle;
	QVBoxLayout *objlayout;
	int visible;
	int idx;

    signals:
	void made_visible(QAccordianObject *);
	void made_hidden(QAccordianObject *);

    public slots:
	void toggleVisibility();

    private:
	QString title;
	QWidget *child_object;
};

class QAccordianWidget : public QWidget
{
    Q_OBJECT

    public:
	QAccordianWidget(QWidget *pparent = 0);
	~QAccordianWidget();
	void addObject(QAccordianObject *object);
	void insertObject(int idx, QAccordianObject *object);
	void deleteObject(QAccordianObject *object);

	void setUniqueVisibility(int val);
	int count();

    public slots:
	void update_sizes(int, int);
	void stateUpdate(QAccordianObject *);

    public:
	int unique_visibility;
	QAccordianObject *open_object;
	QSplitter *splitter;

    private:
	QSet<QAccordianObject *> objects;
	QMap<QString, QList<int> > size_states;
};


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */


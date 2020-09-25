/*              Q A C C O R D I O N W I D G E T . H
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
/** @file QAccordionWidget.h
 *
 * Provides a structured container into which tool panels or objects can be
 * added and then shown or hidden with a controlling button.  Can optionally
 * enforce a rule that at most one object is open at a time.
 *
 */

#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ < 6) && !defined(__clang__)
#  pragma message "Disabling GCC float equality comparison warnings via pragma due to Qt headers..."
#endif
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif
#undef Success
#include <QWidget>
#undef Success
#include <QVBoxLayout>
#undef Success
#include <QPushButton>
#undef Success
#include <QSet>
#undef Success
#include <QMap>
#undef Success
#include <QScrollArea>
#undef Success
#include <QSplitter>
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

class QAccordionWidget;

class QAccordionObject : public QWidget
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

class QAccordionWidget : public QWidget
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


/*                  Q T O O L P A L E T T E . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
#include <QIcon>
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

#include "QFlowLayout.h"

class QToolPaletteElement;

class QToolPaletteButton: public QPushButton
{
    Q_OBJECT

    public:
	QToolPaletteButton(QWidget *bparent = 0, QIcon *iicon = 0, QToolPaletteElement *eparent = 0);
	~QToolPaletteButton(){};

	void setButtonElement(QIcon *iicon, QToolPaletteElement *n_element);

    public slots:
	void select_element();

    signals:
	void element_selected(QToolPaletteElement *);

    private:
	QToolPaletteElement *element;
};


class QToolPaletteElement: public QWidget
{
    Q_OBJECT

    public:
	QToolPaletteElement(QWidget *eparent = 0, QIcon *iicon = 0, QWidget *control = 0);
	~QToolPaletteElement();

	void setButton(QToolPaletteButton *n_button);
	void setControls(QWidget *n_controls);

    public:
	QToolPaletteButton *button;
	QWidget *controls;
};

class QToolPalette: public QWidget
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
      QWidget *control_container;
      QVBoxLayout *control_layout;
      QToolPaletteElement *selected;
      QSet<QToolPaletteElement *> elements;
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


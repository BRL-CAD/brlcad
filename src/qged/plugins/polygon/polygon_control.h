/*         P O L Y G O N _ C O N T R O L . H
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
/** @file polygon_circle_control.h
 *
 */

#include <QLineEdit>
#include <QComboBox>
#include <QGroupBox>
#include <QRadioButton>

class QPolyControl : public QWidget
{
    Q_OBJECT

    public:
	QPolyControl();
	~QPolyControl();

	QLineEdit *view_name;
	QRadioButton *circle_mode;
	QRadioButton *ellipse_mode;
	QRadioButton *square_mode;
	QRadioButton *rectangle_mode;
	QRadioButton *general_mode;
	QGroupBox *general_mode_opts;
	QRadioButton *new_general_poly;
	QRadioButton *append_general_poly;
	QRadioButton *close_general_poly;

    signals:
	void view_updated(struct bview **);

    private slots:
	void toggle_general_opts(bool);

    protected:
	bool eventFilter(QObject *, QEvent *);

    private:
	int poly_cnt = 0;
	struct bv_scene_obj *p = NULL;
};


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

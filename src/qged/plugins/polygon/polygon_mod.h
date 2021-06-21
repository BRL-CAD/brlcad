/*                   P O L Y G O N _ M O D . H
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
/** @file polygon_mod.h
 *
 */

#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QPushButton>
#include <QRadioButton>
#include "qtcad/QColorRGB.h"
#include "polygon_settings.h"

class QPolyMod : public QWidget
{
    Q_OBJECT

    public:
	QPolyMod();
	~QPolyMod();

	// Boolean Operation Mode
	QComboBox *csg_modes;

	// Modifying polygons
	QComboBox *mod_names;
	QRadioButton *select_mode;
	QRadioButton *move_mode;
	QRadioButton *update_mode;

	QGroupBox *general_mode_opts;
	QCheckBox *close_general_poly;
	QRadioButton *append_pnt;
	QRadioButton *select_pnt;

	QPolySettings *ps;

	// Selected Edge color
	QCheckBox *selected_fill_poly;
	QColorRGB *selected_fill_color;
	QColorRGB *selected_edge_color;
	// TODO - maybe use sliders here?
	QLineEdit *selected_fill_angle;
	QLineEdit *selected_fill_density;

    signals:
	void view_updated(struct bview **);

    public slots:
	void app_mod_names_reset(void *);
	void mod_names_reset();
	void polygon_update();

    private slots:
	void toplevel_config(bool);

	void toggle_closed_poly(bool);
	void select(const QString &t);
	void clear_pnt_selection(bool);

    protected:
	bool eventFilter(QObject *, QEvent *);

    private:
	void poly_type_settings(struct bv_polygon *ip);
	int poly_cnt = 0;
	struct bv_scene_obj *p = NULL;
	bool do_bool = false;
};


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

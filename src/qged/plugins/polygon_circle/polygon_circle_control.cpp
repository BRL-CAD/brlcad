/*         P O L Y G O N _ C I R C L E _ C O N T R O L . C P P
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
/** @file polygon_circle_control.cpp
 *
 */

#include <QLabel>
#include <QLineEdit>
#include <QGroupBox>
#include "../../app.h"
#include "polygon_circle_control.h"

QCirclePolyControl::QCirclePolyControl()
    : QWidget()
{
    QVBoxLayout *l = new QVBoxLayout;

    QGroupBox *groupBox = new QGroupBox("Add Polygon");
    QLabel *vn_label = new QLabel("Name of next polygon:");
    view_name = new QLineEdit(this);

    // Set an initial name (user can change, but need
    // something if they don't have a specific name in
    // mind.)
    struct bu_vls pname = BU_VLS_INIT_ZERO;
    cpoly_cnt++;
    bu_vls_sprintf(&pname, "polygon_%06d", cpoly_cnt);
    view_name->setPlaceholderText(QString(bu_vls_cstr(&pname)));
    bu_vls_free(&pname);

    QVBoxLayout *gl = new QVBoxLayout;
    circle_mode = new QRadioButton("Circle");
    circle_mode->setIcon(QIcon(QPixmap(":circle.svg")));
    circle_mode->setChecked(true);
    ellipse_mode = new QRadioButton("Ellipse");
    ellipse_mode->setIcon(QIcon(QPixmap(":ellipse.svg")));
    square_mode = new QRadioButton("Square");
    square_mode->setIcon(QIcon(QPixmap(":square.svg")));
    rectangle_mode = new QRadioButton("Rectangle");
    rectangle_mode->setIcon(QIcon(QPixmap(":rectangle.svg")));
    general_mode = new QRadioButton("General");
    general_mode->setIcon(QIcon(QPixmap(":polygon.svg")));
    gl->addWidget(vn_label);
    gl->addWidget(view_name);
    gl->addWidget(circle_mode);
    gl->addWidget(ellipse_mode);
    gl->addWidget(square_mode);
    gl->addWidget(rectangle_mode);
    gl->addWidget(general_mode);
    gl->setAlignment(Qt::AlignTop);
    groupBox->setLayout(gl);

    l->addWidget(groupBox);

    l->setAlignment(Qt::AlignTop);
    this->setLayout(l);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

}

QCirclePolyControl::~QCirclePolyControl()
{
}

bool
QCirclePolyControl::eventFilter(QObject *, QEvent *e)
{

    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!gedp) {
	return false;
    }

    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease || e->type() == QEvent::MouseButtonDblClick || e->type() == QEvent::MouseMove) {

	printf("polygon circle filter mouse\n");
	QMouseEvent *m_e = (QMouseEvent *)e;

	if (m_e->type() == QEvent::MouseButtonPress && m_e->buttons().testFlag(Qt::LeftButton)) {
	    if (!p) {
		int ptype = BV_POLYGON_CIRCLE;
		if (ellipse_mode->isChecked()) {
		    ptype = BV_POLYGON_ELLIPSE;
		}
		if (square_mode->isChecked()) {
		    ptype = BV_POLYGON_SQUARE;
		}
		if (rectangle_mode->isChecked()) {
		    ptype = BV_POLYGON_RECTANGLE;
		}
		if (general_mode->isChecked()) {
		    ptype = BV_POLYGON_GENERAL;
		}
		p = bv_create_polygon(gedp->ged_gvp, ptype, m_e->x(), m_e->y(), gedp->free_scene_obj);

		bu_vls_init(&p->s_uuid);
		if (view_name->text().length()) {
		    bu_vls_printf(&p->s_uuid, "%s", view_name->text().toLocal8Bit().data());
		} else {
		    bu_vls_printf(&p->s_uuid, "%s", view_name->placeholderText().toLocal8Bit().data());
		}
		bu_ptbl_ins(gedp->ged_gvp->gv_view_objs, (long *)p);

		cpoly_cnt++;

		view_name->clear();
		struct bu_vls pname = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&pname, "polygon_%06d", cpoly_cnt);
		view_name->setPlaceholderText(QString(bu_vls_cstr(&pname)));
		bu_vls_free(&pname);

		emit view_updated(&gedp->ged_gvp);
		return true;
	    }
	}

	if (m_e->type() == QEvent::MouseMove) {
	    if (p && m_e->buttons().testFlag(Qt::LeftButton) && m_e->modifiers() == Qt::NoModifier) {
		p->s_changed = 0;
		p->s_v = gedp->ged_gvp;
		gedp->ged_gvp->gv_mouse_x = m_e->x();
		gedp->ged_gvp->gv_mouse_y = m_e->y();
		(*p->s_update_callback)(p);
		emit view_updated(&gedp->ged_gvp);
		return true;
	    }
	}

	if (m_e->type() == QEvent::MouseButtonRelease) {
	    p = NULL;
	    emit view_updated(&gedp->ged_gvp);
	    return true;
	}
    }

    return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

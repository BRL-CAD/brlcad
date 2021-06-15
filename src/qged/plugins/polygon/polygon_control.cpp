/*         P O L Y G O N _ C O N T R O L . C P P
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
/** @file polygon_control.cpp
 *
 */

#include <QLabel>
#include <QLineEdit>
#include <QButtonGroup>
#include <QGroupBox>
#include "../../app.h"
#include "polygon_control.h"

QPolyControl::QPolyControl()
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
    poly_cnt++;
    bu_vls_sprintf(&pname, "polygon_%06d", poly_cnt);
    view_name->setPlaceholderText(QString(bu_vls_cstr(&pname)));
    bu_vls_free(&pname);

    QVBoxLayout *gl = new QVBoxLayout;
    circle_mode = new QRadioButton("");
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

    general_mode_opts = new QGroupBox("General Polygon Modes");
    QVBoxLayout *go_l = new QVBoxLayout();
    QButtonGroup *gm_box = new QButtonGroup();
    new_general_poly = new QRadioButton("Add new general polygon");
    new_general_poly->setChecked(true);
    gm_box->addButton(new_general_poly);
    go_l->addWidget(new_general_poly);
    append_general_poly = new QRadioButton("Append polygon point");
    gm_box->addButton(append_general_poly);
    go_l->addWidget(append_general_poly);
    close_general_poly = new QRadioButton("Close polygon");
    gm_box->addButton(close_general_poly);
    go_l->addWidget(close_general_poly);
    go_l->setAlignment(Qt::AlignTop);
    general_mode_opts->setLayout(go_l);
    QObject::connect(general_mode, &QRadioButton::toggled, this, &QPolyControl::toggle_general_opts);

    gl->addWidget(vn_label);
    gl->addWidget(view_name);
    gl->addWidget(circle_mode);
    gl->addWidget(ellipse_mode);
    gl->addWidget(square_mode);
    gl->addWidget(rectangle_mode);
    gl->addWidget(general_mode);
    gl->addWidget(general_mode_opts);
    gl->setAlignment(Qt::AlignTop);
    groupBox->setLayout(gl);

    l->addWidget(groupBox);

    l->setAlignment(Qt::AlignTop);
    this->setLayout(l);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    general_mode_opts->setDisabled(true);
}

QPolyControl::~QPolyControl()
{
}

void
QPolyControl::toggle_general_opts(bool checked)
{
    printf("got here\n");
    if (checked) {
	general_mode_opts->setEnabled(true);
    } else {
	general_mode_opts->setDisabled(true);
    }
}

bool
QPolyControl::eventFilter(QObject *, QEvent *e)
{

    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!gedp) {
	return false;
    }

    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease || e->type() == QEvent::MouseButtonDblClick || e->type() == QEvent::MouseMove) {

	printf("polygon filter mouse\n");
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
		if (ptype != BV_POLYGON_GENERAL || (ptype == BV_POLYGON_GENERAL && new_general_poly->isChecked())) {
		    p = bv_create_polygon(gedp->ged_gvp, ptype, m_e->x(), m_e->y(), gedp->free_scene_obj);
		    bu_vls_init(&p->s_uuid);
		    if (view_name->text().length()) {
			bu_vls_printf(&p->s_uuid, "%s", view_name->text().toLocal8Bit().data());
		    } else {
			bu_vls_printf(&p->s_uuid, "%s", view_name->placeholderText().toLocal8Bit().data());
		    }
		    bu_ptbl_ins(gedp->ged_gvp->gv_view_objs, (long *)p);

		    if (ptype == BV_POLYGON_GENERAL) {
			append_general_poly->toggle();
			struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
			ip->curr_contour_i = 0;
			emit view_updated(&gedp->ged_gvp);
			return true;
		    }
		}

		poly_cnt++;

		view_name->clear();
		struct bu_vls pname = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&pname, "polygon_%06d", poly_cnt);
		view_name->setPlaceholderText(QString(bu_vls_cstr(&pname)));
		bu_vls_free(&pname);

		emit view_updated(&gedp->ged_gvp);
		return true;

	    } else if (append_general_poly->isChecked()) {
		struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
		ip->sflag = 0;
		ip->mflag = 0;
		ip->aflag = 1;

		p->s_v->gv_mouse_x = m_e->x();
		p->s_v->gv_mouse_y = m_e->y();
		bv_update_polygon(p);

		emit view_updated(&gedp->ged_gvp);
		return true;
	    } else if (close_general_poly->isChecked()) {
		struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;

		ip->polygon.contour[0].open = 0;

		ip->sflag = 0;
		ip->mflag = 0;
		ip->aflag = 0;

		bv_update_polygon(p);

		new_general_poly->toggle();

		p = NULL;
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
	    if (!general_mode->isChecked()) {
		p = NULL;
	    }
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

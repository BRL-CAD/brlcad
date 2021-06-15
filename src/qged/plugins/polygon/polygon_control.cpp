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


    QButtonGroup *t_grp = new QButtonGroup();

    QLabel *csg_modes_label = new QLabel("Applying Boolean Operation:");
    l->addWidget(csg_modes_label);
    csg_modes = new QComboBox();
    csg_modes->addItem("None");
    csg_modes->addItem("Union");
    csg_modes->addItem("Subtraction");
    csg_modes->addItem("Intersection");
    csg_modes->setCurrentIndex(0);
    l->addWidget(csg_modes);


    QGroupBox *addpolyBox = new QGroupBox("Add Polygon");
    QVBoxLayout *add_poly_gl = new QVBoxLayout;
    add_poly_gl->setAlignment(Qt::AlignTop);

    QLabel *vn_label = new QLabel("Name of next polygon:");
    view_name = new QLineEdit(this);
    // Set an initial name (user can change, but we need something if they
    // don't have a specific name in mind.)
    struct bu_vls pname = BU_VLS_INIT_ZERO;
    poly_cnt++;
    bu_vls_sprintf(&pname, "polygon_%06d", poly_cnt);
    view_name->setPlaceholderText(QString(bu_vls_cstr(&pname)));
    bu_vls_free(&pname);

    circle_mode = new QRadioButton("Circle");
    circle_mode->setIcon(QIcon(QPixmap(":circle.svg")));
    circle_mode->setChecked(true);
    t_grp->addButton(circle_mode);
    ellipse_mode = new QRadioButton("Ellipse");
    ellipse_mode->setIcon(QIcon(QPixmap(":ellipse.svg")));
    t_grp->addButton(ellipse_mode);
    square_mode = new QRadioButton("Square");
    square_mode->setIcon(QIcon(QPixmap(":square.svg")));
    t_grp->addButton(square_mode);
    rectangle_mode = new QRadioButton("Rectangle");
    rectangle_mode->setIcon(QIcon(QPixmap(":rectangle.svg")));
    t_grp->addButton(rectangle_mode);
    general_mode = new QRadioButton("General");
    general_mode->setIcon(QIcon(QPixmap(":polygon.svg")));
    t_grp->addButton(general_mode);

    add_poly_gl->addWidget(vn_label);
    add_poly_gl->addWidget(view_name);
    add_poly_gl->addWidget(circle_mode);
    add_poly_gl->addWidget(ellipse_mode);
    add_poly_gl->addWidget(square_mode);
    add_poly_gl->addWidget(rectangle_mode);
    add_poly_gl->addWidget(general_mode);
    
    addpolyBox->setLayout(add_poly_gl);
    l->addWidget(addpolyBox);


    QGroupBox *modpolyBox = new QGroupBox("Modify Polygon");
    QVBoxLayout *mod_poly_gl = new QVBoxLayout;
    QLabel *cs_label = new QLabel("Currently selected polygon:");
    mod_names = new QComboBox(this);

    select_mode = new QRadioButton("Select");
    t_grp->addButton(select_mode);
    move_mode = new QRadioButton("Move");
    t_grp->addButton(move_mode);
    update_mode = new QRadioButton("Update");
    t_grp->addButton(update_mode);

    close_general_poly = new QCheckBox("Close polygon");
    // Disabled if we're not a general polygon
    close_general_poly->setChecked(true);
    close_general_poly->setDisabled(true);
    // Append polygon pnt if above is true (switch to select).
    QObject::connect(close_general_poly, &QCheckBox::toggled, this, &QPolyControl::toggle_closed_poly);

    // Basic shape updating is simple from an interaction standpoint, but the
    // general case is a bit more involved.
    general_mode_opts = new QGroupBox("General Polygon Modes");
    QVBoxLayout *go_l = new QVBoxLayout();
    go_l->setAlignment(Qt::AlignTop);
    
    QButtonGroup *gm_box = new QButtonGroup();
    append_pnt = new QRadioButton("Append polygon pnt");
    append_pnt->setChecked(true);
    gm_box->addButton(append_pnt);
    go_l->addWidget(append_pnt);
    select_pnt = new QRadioButton("Select polygon pnt");
    gm_box->addButton(select_pnt);
    go_l->addWidget(select_pnt);
    general_mode_opts->setLayout(go_l);
    QObject::connect(update_mode, &QRadioButton::toggled, this, &QPolyControl::toggle_general_opts);
    general_mode_opts->setDisabled(true);

    mod_poly_gl->addWidget(cs_label);
    mod_poly_gl->addWidget(mod_names);
    mod_poly_gl->addWidget(select_mode);
    mod_poly_gl->addWidget(move_mode);
    mod_poly_gl->addWidget(update_mode);
    mod_poly_gl->addWidget(close_general_poly);
    mod_poly_gl->addWidget(general_mode_opts);

    modpolyBox->setLayout(mod_poly_gl);
    l->addWidget(modpolyBox);


    l->setAlignment(Qt::AlignTop);
    this->setLayout(l);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

}

QPolyControl::~QPolyControl()
{
}

void
QPolyControl::toggle_general_opts(bool checked)
{
    int ptype = -1;
    struct bv_polygon *ip = NULL;
    if (p) {
	ip = (struct bv_polygon *)p->s_i_data;
	ptype = ip->type;
    }
    if (checked && ptype == BV_POLYGON_GENERAL) {
	general_mode_opts->setEnabled(true);
	if (ip && !ip->polygon.contour[0].open) { 
	    close_general_poly->setChecked(true);
	} else {
	    close_general_poly->setChecked(false);
	}
    } else {
	general_mode_opts->setDisabled(true);
	close_general_poly->setChecked(true);
	close_general_poly->setEnabled(false);
    }
}

void
QPolyControl::toggle_closed_poly(bool checked)
{
    int ptype = -1;
    struct bv_polygon *ip = NULL;
    if (p) {
	ip = (struct bv_polygon *)p->s_i_data;
	ptype = ip->type;
    }
    if (!ip || ptype != BV_POLYGON_GENERAL) {
	if (!checked)
	    close_general_poly->setChecked(true);
	return;
    }

    // A contour with less than 3 points can't be closed
    if (checked && ptype == BV_POLYGON_GENERAL) {
	if (ip->polygon.contour[0].num_points < 3) {
	    close_general_poly->setChecked(false);
	    return;
	}
    }

    if (checked && ptype == BV_POLYGON_GENERAL) {
	ip->polygon.contour[0].open = 0; 
	append_pnt->setDisabled(true);
    } else {
	ip->polygon.contour[0].open = 1; 
	append_pnt->setEnabled(true);
    }

    ip->sflag = 0;
    ip->mflag = 0;
    ip->aflag = 0;

    bv_update_polygon(p);

    struct ged *gedp = ((CADApp *)qApp)->gedp;
    emit view_updated(&gedp->ged_gvp);
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

		p = bv_create_polygon(gedp->ged_gvp, ptype, m_e->x(), m_e->y(), gedp->free_scene_obj);
		
		if (ptype == BV_POLYGON_GENERAL) {
		    // For general polygons, we need to identify the active contour
		    // for update operations to work.
		    //
		    // At some point we'll need to add support for adding and removing
		    // contours...
		    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
		    ip->curr_contour_i = 0;

		    // Out of the gate, general polygons are not closed
		    close_general_poly->setChecked(false);
		    close_general_poly->setEnabled(true);
		}

		bu_vls_init(&p->s_uuid);
		if (view_name->text().length()) {
		    bu_vls_printf(&p->s_uuid, "%s", view_name->text().toLocal8Bit().data());
		} else {
		    bu_vls_printf(&p->s_uuid, "%s", view_name->placeholderText().toLocal8Bit().data());
		}
		bu_ptbl_ins(gedp->ged_gvp->gv_view_objs, (long *)p);

		// Having added a polygon, we now update the combo box of available polygons to select:
		mod_names->clear();
		for (size_t i = 0; i < BU_PTBL_LEN(gedp->ged_gvp->gv_view_objs); i++) {
		    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(gedp->ged_gvp->gv_view_objs, i);
		    if (s->s_type_flags & BV_POLYGONS) {
			mod_names->addItem(bu_vls_cstr(&s->s_uuid));
		    }
		}
		int cind = mod_names->findText(bu_vls_cstr(&p->s_uuid));
		mod_names->setCurrentIndex(cind);
		update_mode->toggle();


		poly_cnt++;

		view_name->clear();
		struct bu_vls pname = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&pname, "polygon_%06d", poly_cnt);
		view_name->setPlaceholderText(QString(bu_vls_cstr(&pname)));
		bu_vls_free(&pname);

		// TODO - kick GUI into modify mode now that the initial creation has taken place

		emit view_updated(&gedp->ged_gvp);
		return true;
	    }

	    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
	    if (append_pnt->isChecked() && ip->type == BV_POLYGON_GENERAL) {
		ip->sflag = 0;
		ip->mflag = 0;
		ip->aflag = 1;

		p->s_v->gv_mouse_x = m_e->x();
		p->s_v->gv_mouse_y = m_e->y();
		bv_update_polygon(p);

		emit view_updated(&gedp->ged_gvp);
		return true;
	    }
	}
#if 0
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
#endif

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
	    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
	    switch (ip->type) {
		case BV_POLYGON_ELLIPSE:
		    ellipse_mode->toggle();
		    break;
		case BV_POLYGON_SQUARE:
		    square_mode->toggle();
		    break;
		case BV_POLYGON_RECTANGLE:
		    rectangle_mode->toggle();
		    break;
		case BV_POLYGON_GENERAL:
		    // Unlike the other modes, creation of a general polygon
		    // typically involves user interaction beyond the initial
		    // click-and-drag
		    break;
		default:
		    circle_mode->toggle();
		    break;
	    };

	    if (ip->type != BV_POLYGON_GENERAL) {
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

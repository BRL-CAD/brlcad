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
    QObject::connect(circle_mode, &QCheckBox::toggled, this, &QPolyControl::toplevel_config);
    t_grp->addButton(circle_mode);
    ellipse_mode = new QRadioButton("Ellipse");
    ellipse_mode->setIcon(QIcon(QPixmap(":ellipse.svg")));
    QObject::connect(ellipse_mode, &QCheckBox::toggled, this, &QPolyControl::toplevel_config);
    t_grp->addButton(ellipse_mode);
    square_mode = new QRadioButton("Square");
    square_mode->setIcon(QIcon(QPixmap(":square.svg")));
    QObject::connect(square_mode, &QCheckBox::toggled, this, &QPolyControl::toplevel_config);
    t_grp->addButton(square_mode);
    rectangle_mode = new QRadioButton("Rectangle");
    rectangle_mode->setIcon(QIcon(QPixmap(":rectangle.svg")));
    QObject::connect(rectangle_mode, &QCheckBox::toggled, this, &QPolyControl::toplevel_config);
    t_grp->addButton(rectangle_mode);
    general_mode = new QRadioButton("General");
    general_mode->setIcon(QIcon(QPixmap(":polygon.svg")));
    QObject::connect(general_mode, &QCheckBox::toggled, this, &QPolyControl::toplevel_config);
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
    QObject::connect(mod_names, &QComboBox::currentTextChanged, this, &QPolyControl::select);

    select_mode = new QRadioButton("Select");
    t_grp->addButton(select_mode);
    move_mode = new QRadioButton("Move");
    QObject::connect(move_mode, &QRadioButton::toggled, this, &QPolyControl::toplevel_config);
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
    QObject::connect(select_pnt, &QRadioButton::toggled, this, &QPolyControl::clear_pnt_selection);
    gm_box->addButton(select_pnt);
    go_l->addWidget(select_pnt);
    general_mode_opts->setLayout(go_l);
    QObject::connect(update_mode, &QRadioButton::toggled, this, &QPolyControl::toplevel_config);
    general_mode_opts->setDisabled(true);

    mod_poly_gl->addWidget(select_mode);
    mod_poly_gl->addWidget(cs_label);
    mod_poly_gl->addWidget(mod_names);
    mod_poly_gl->addWidget(move_mode);
    mod_poly_gl->addWidget(update_mode);
    mod_poly_gl->addWidget(close_general_poly);
    mod_poly_gl->addWidget(general_mode_opts);

    modpolyBox->setLayout(mod_poly_gl);
    l->addWidget(modpolyBox);


    l->setAlignment(Qt::AlignTop);
    this->setLayout(l);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    // By default, start in circle addition mode
    circle_mode->setChecked(true);
    toplevel_config(true);
}

QPolyControl::~QPolyControl()
{
}

void
QPolyControl::poly_type_settings(struct bv_polygon *ip)
{
    if (ip->type == BV_POLYGON_GENERAL) {
	general_mode_opts->setEnabled(true);
	close_general_poly->setEnabled(true);
	close_general_poly->blockSignals(true);
	append_pnt->blockSignals(true);
	select_pnt->blockSignals(true);
	if (!ip->polygon.contour[0].open) {
	    close_general_poly->setChecked(true);
	    select_pnt->setChecked(true);
	    append_pnt->setChecked(false);
	    append_pnt->setEnabled(false);
	} else {
	    close_general_poly->setChecked(false);
	    append_pnt->setEnabled(true);
	    append_pnt->setChecked(true);
	    select_pnt->setChecked(false);
	}
	close_general_poly->blockSignals(false);
	append_pnt->blockSignals(false);
	select_pnt->blockSignals(false);
    } else {
	close_general_poly->blockSignals(true);
	close_general_poly->setChecked(true);
	close_general_poly->setEnabled(false);
	close_general_poly->blockSignals(false);
	general_mode_opts->setEnabled(false);
    }
}

void
QPolyControl::toplevel_config(bool)
{
    // Initialize
    struct ged *gedp = ((CADApp *)qApp)->gedp;
    bool draw_change = false;
    add_mode = false;
    mod_mode = false;

    // This function is called when a top level mode change was initiated
    // by a selection button.  Clear any selected points being displayed -
    // when we're switching modes at this level, we always start with a
    // blank slate for points.
    if (gedp) {
	for (size_t i = 0; i < BU_PTBL_LEN(gedp->ged_gvp->gv_view_objs); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(gedp->ged_gvp->gv_view_objs, i);
	    if (s->s_type_flags & BV_POLYGONS) {
		// clear any selected points in non-current polygons
		struct bv_polygon *ip = (struct bv_polygon *)s->s_i_data;
		ip->sflag = 0;
		ip->mflag = 0;
		ip->aflag = 0;
		if (ip->curr_point_i != -1) {
		    bu_log("Clear pnt selection\n");
		    draw_change = true;
		    ip->curr_point_i = -1;
		    ip->curr_contour_i = 0;
		    bv_update_polygon(s);
		}
	    }
	}
    }

    if (circle_mode->isChecked()) {
	add_mode = true;
    }

    if (ellipse_mode->isChecked()) {
	add_mode = true;
    }

    if (square_mode->isChecked()) {
	add_mode = true;
    }

    if (rectangle_mode->isChecked()) {
	add_mode = true;
    }

    if (general_mode->isChecked()) {
	add_mode = true;
    }

    if (add_mode) {
	p = NULL;
	general_mode_opts->setDisabled(true);
	if (draw_change && gedp)
	    emit view_updated(&gedp->ged_gvp);
	return;
    }

    // Mods are a bit more complicated.  What options are active depends on the
    // type of the selected object, not on which option is selected
    mod_mode = true;

    // Make sure the Combo box list is current.
    mod_names->blockSignals(true);
    mod_names->clear();
    // An empty selection (clearing the selected polygon) is allowed
    mod_names->addItem("");
    if (gedp) {
	for (size_t i = 0; i < BU_PTBL_LEN(gedp->ged_gvp->gv_view_objs); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(gedp->ged_gvp->gv_view_objs, i);
	    if (s->s_type_flags & BV_POLYGONS) {
		mod_names->addItem(bu_vls_cstr(&s->s_uuid));
	    }
	}
    }
    if (p) {
	int cind = mod_names->findText(bu_vls_cstr(&p->s_uuid));
	mod_names->setCurrentIndex(cind);
	bu_log("select %s (%d)\n", bu_vls_cstr(&p->s_uuid), cind);
    } else {
	mod_names->setCurrentIndex(0);
    }
    mod_names->blockSignals(false);

    // If we have a current p, we know the current type - enable/disable
    // the general settings on that basis

    if (p) {
	struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
	poly_type_settings(ip);
    }

    if (draw_change && gedp)
	emit view_updated(&gedp->ged_gvp);
}


void
QPolyControl::clear_pnt_selection(bool checked)
{
    if (checked)
	return;
    int ptype = -1;
    struct bv_polygon *ip = NULL;
    if (p) {
	ip = (struct bv_polygon *)p->s_i_data;
	ptype = ip->type;
    }
    if (!ip || ptype != BV_POLYGON_GENERAL) {
	return;
    }
    bu_log("got pnt selection clear\n");
    ip->curr_point_i = -1;
    ip->curr_contour_i = 0;

    bv_update_polygon(p);

    struct ged *gedp = ((CADApp *)qApp)->gedp;
    emit view_updated(&gedp->ged_gvp);
}

void
QPolyControl::select(const QString &poly)
{
    struct ged *gedp = ((CADApp *)qApp)->gedp;
    p = NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(gedp->ged_gvp->gv_view_objs); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(gedp->ged_gvp->gv_view_objs, i);
	if (s->s_type_flags & BV_POLYGONS) {
	    QString pname(bu_vls_cstr(&s->s_uuid));
	    if (pname == poly) {
		p = s;
		struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
		poly_type_settings(ip);
		return;
	    }
	}
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
	if (!checked) {
	    close_general_poly->blockSignals(true);
	    close_general_poly->setChecked(true);
	    close_general_poly->setEnabled(false);
	    close_general_poly->blockSignals(false);
	    general_mode_opts->setEnabled(false);
	}
	return;
    }

    clear_pnt_selection(false);

    if (checked && ptype == BV_POLYGON_GENERAL) {
	// A contour with less than 3 points can't be closed
	if (ip->polygon.contour[0].num_points < 3) {
	    ip->polygon.contour[0].open = 1;
	} else {
	    ip->polygon.contour[0].open = 0;
	}
    } else {
	ip->polygon.contour[0].open = 1;
    }

    close_general_poly->blockSignals(true);
    append_pnt->blockSignals(true);
    select_pnt->blockSignals(true);

    if (!ip->polygon.contour[0].open) {
	close_general_poly->setChecked(true);
	select_pnt->setChecked(true);
	append_pnt->setChecked(false);
	append_pnt->setEnabled(false);
    } else {
	close_general_poly->setChecked(false);
	append_pnt->setEnabled(true);
	append_pnt->setChecked(true);
	select_pnt->setChecked(false);
    }

    close_general_poly->blockSignals(false);
    append_pnt->blockSignals(false);
    select_pnt->blockSignals(false);

    ip->sflag = 0;
    ip->mflag = 0;
    ip->aflag = 0;

    bv_update_polygon(p);

    struct ged *gedp = ((CADApp *)qApp)->gedp;
    emit view_updated(&gedp->ged_gvp);
}

bool
QPolyControl::add_events(QObject *, QMouseEvent *m_e)
{
    printf("polygon add\n");

    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!gedp) {
	return false;
    }

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
	    p->s_v = gedp->ged_gvp;

	    if (ptype == BV_POLYGON_GENERAL) {

		struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
		// For general polygons, we need to identify the active contour
		// for update operations to work.
		//
		// At some point we'll need to add support for adding and removing
		// contours...
		ip->curr_contour_i = 0;

		poly_type_settings(ip);
	    }

	    bu_vls_init(&p->s_uuid);
	    if (view_name->text().length()) {
		bu_vls_printf(&p->s_uuid, "%s", view_name->text().toLocal8Bit().data());
	    } else {
		bu_vls_printf(&p->s_uuid, "%s", view_name->placeholderText().toLocal8Bit().data());
	    }
	    bu_ptbl_ins(gedp->ged_gvp->gv_view_objs, (long *)p);


	    poly_cnt++;
	    view_name->clear();
	    struct bu_vls pname = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&pname, "polygon_%06d", poly_cnt);
	    view_name->setPlaceholderText(QString(bu_vls_cstr(&pname)));
	    bu_vls_free(&pname);

	    if (ptype == BV_POLYGON_GENERAL) {
		// Unlike the other polygon types, we need to use interactive
		// mode to properly define a general polygon.
		update_mode->toggle();
	    }

	    emit view_updated(&gedp->ged_gvp);
	    return true;
	}

	// When we're dealing with polygons stray left clicks shouldn't zoom - just
	// consume them if we're not using them above.
	return true;
    }

    // During initial add/creation, we're just adjusting the shape
    if (m_e->type() == QEvent::MouseMove) {
	if (p && m_e->buttons().testFlag(Qt::LeftButton) && m_e->modifiers() == Qt::NoModifier) {

	    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
	    ip->aflag = 0;
	    ip->mflag = 0;
	    ip->sflag = 0;
	    bv_update_polygon(p);
	    emit view_updated(&gedp->ged_gvp);
	    return true;
	}
    }

    if (m_e->type() == QEvent::MouseButtonRelease) {
	emit view_updated(&gedp->ged_gvp);
	return true;
    }

    return false;
}

bool
QPolyControl::mod_events(QObject *, QMouseEvent *m_e)
{

    printf("polygon mod\n");

    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!gedp) {
	return false;
    }

    if (m_e->type() == QEvent::MouseButtonPress && m_e->buttons().testFlag(Qt::LeftButton)) {

	if (select_mode->isChecked()) {
	    p = bv_select_polygon(gedp->ged_gvp->gv_view_objs, gedp->ged_gvp);
	    if (p) {
		int cind = mod_names->findText(bu_vls_cstr(&p->s_uuid));
		mod_names->blockSignals(true);
		mod_names->setCurrentIndex(cind);
		mod_names->blockSignals(false);
		struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
		poly_type_settings(ip);
	    }
	    return true;
	}

	if (move_mode->isChecked()) {
	    // Left click is a no-op in move mode
	    return true;
	}

	if (!p) {
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

	if (!move_mode->isChecked() && select_pnt->isChecked() && ip->type == BV_POLYGON_GENERAL) {
	    ip->sflag = 1;
	    ip->mflag = 0;
	    ip->aflag = 0;
	    p->s_v->gv_mouse_x = m_e->x();
	    p->s_v->gv_mouse_y = m_e->y();
	    bv_update_polygon(p);
	    emit view_updated(&gedp->ged_gvp);
	    return true;
	}

	// When we're dealing with polygons stray left clicks shouldn't zoom - just
	// consume them if we're not using them above.
	return true;
    }

    if (m_e->type() == QEvent::MouseMove) {
	if (p && m_e->buttons().testFlag(Qt::LeftButton) && m_e->modifiers() == Qt::NoModifier) {

	    if (select_mode->isChecked()) {
		// Move is a no-op in select mode
		return true;
	    }

	    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
	    if (!move_mode->isChecked() && select_pnt->isChecked() && ip->type == BV_POLYGON_GENERAL) {
		ip->aflag = 0;
		ip->mflag = 1;
		ip->sflag = 0;
		bv_update_polygon(p);
		emit view_updated(&gedp->ged_gvp);
	    } else if (move_mode->isChecked()) {
		bu_log("move polygon mode\n");
		clear_pnt_selection(false);
		ip->aflag = 0;
		ip->mflag = 0;
		ip->sflag = 0;
		bv_move_polygon(p);
		emit view_updated(&gedp->ged_gvp);
	    } else {
		ip->aflag = 0;
		ip->mflag = 0;
		ip->sflag = 0;
		bv_update_polygon(p);
		emit view_updated(&gedp->ged_gvp);
	    }
	    return true;
	}
    }

    if (m_e->type() == QEvent::MouseButtonRelease) {
	emit view_updated(&gedp->ged_gvp);
	return true;
    }

    return false;
}

bool
QPolyControl::eventFilter(QObject *, QEvent *e)
{
    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!gedp) {
	return false;
    }

    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease || e->type() == QEvent::MouseButtonDblClick || e->type() == QEvent::MouseMove) {

	QMouseEvent *m_e = (QMouseEvent *)e;

	gedp->ged_gvp->gv_prevMouseX = gedp->ged_gvp->gv_mouse_x;
	gedp->ged_gvp->gv_prevMouseY = gedp->ged_gvp->gv_mouse_y;

	gedp->ged_gvp->gv_mouse_x = m_e->x();
	gedp->ged_gvp->gv_mouse_y = m_e->y();

	if (add_mode) {
	    return add_events(NULL, m_e);
	}
	if (mod_mode) {
	    return mod_events(NULL, m_e);
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

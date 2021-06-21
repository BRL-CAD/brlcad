/*                 P O L Y G O N _ M O D . C P P
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
/** @file polygon_mod.cpp
 *
 */

#include <QLabel>
#include <QLineEdit>
#include <QButtonGroup>
#include <QGroupBox>
#include "../../app.h"
#include "polygon_create.h"
#include "polygon_mod.h"

#define FREE_BV_SCENE_OBJ(p, fp) { \
    for (size_t c_i = 0; c_i < BU_PTBL_LEN(&p->children); c_i++) { \
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&p->children, c_i); \
	BU_LIST_APPEND(fp, &((s_c)->l)); \
	BV_FREE_VLIST(&gedp->vlfree, &((s_c)->s_vlist)); \
    } \
    BU_LIST_APPEND(fp, &((p)->l)); \
    BV_FREE_VLIST(&gedp->vlfree, &((p)->s_vlist)); \
}

QPolyMod::QPolyMod()
    : QWidget()
{
    QVBoxLayout *l = new QVBoxLayout;

    QButtonGroup *t_grp = new QButtonGroup();

    select_mode = new QRadioButton("Select");
    t_grp->addButton(select_mode);
    l->addWidget(select_mode);
    QLabel *cs_label = new QLabel("Currently selected polygon:");
    l->addWidget(cs_label);
    mod_names = new QComboBox(this);
    QObject::connect(mod_names, &QComboBox::currentTextChanged, this, &QPolyMod::select);
    l->addWidget(mod_names);


    QGroupBox *defaultBox = new QGroupBox("Settings");
    QVBoxLayout *default_gl = new QVBoxLayout;
    default_gl->setAlignment(Qt::AlignTop);
    ps = new QPolySettings();
    default_gl->addWidget(ps);
    defaultBox->setLayout(default_gl);
    l->addWidget(defaultBox);
    QObject::connect(ps, &QPolySettings::settings_changed, this, &QPolyMod::polygon_update);

    QGroupBox *modpolyBox = new QGroupBox("Modify Polygon");
    QVBoxLayout *mod_poly_gl = new QVBoxLayout;

    move_mode = new QRadioButton("Move");
    QObject::connect(move_mode, &QRadioButton::toggled, this, &QPolyMod::toplevel_config);
    t_grp->addButton(move_mode);
    update_mode = new QRadioButton("Update geometry");
    t_grp->addButton(update_mode);

    close_general_poly = new QCheckBox("Close polygon");
    // Disabled if we're not a general polygon
    close_general_poly->setChecked(true);
    close_general_poly->setDisabled(true);
    // Append polygon pnt if above is true (switch to select).
    QObject::connect(close_general_poly, &QCheckBox::toggled, this, &QPolyMod::toggle_closed_poly);

    // Basic shape updating is simple from an interaction standpoint, but the
    // general case is a bit more involved.
    general_mode_opts = new QGroupBox("General Polygon Modes");
    QVBoxLayout *go_l = new QVBoxLayout();
    go_l->setAlignment(Qt::AlignTop);
    //go_l->setSpacing(0);
    //go_l->setContentsMargins(1,1,1,1);

    QButtonGroup *gm_box = new QButtonGroup();
    append_pnt = new QRadioButton("Append polygon pnt");
    append_pnt->setChecked(true);
    gm_box->addButton(append_pnt);
    go_l->addWidget(append_pnt);
    select_pnt = new QRadioButton("Select polygon pnt");
    QObject::connect(select_pnt, &QRadioButton::toggled, this, &QPolyMod::clear_pnt_selection);
    gm_box->addButton(select_pnt);
    go_l->addWidget(select_pnt);
    general_mode_opts->setLayout(go_l);
    QObject::connect(update_mode, &QRadioButton::toggled, this, &QPolyMod::toplevel_config);
    general_mode_opts->setDisabled(true);

    mod_poly_gl->addWidget(move_mode);
    mod_poly_gl->addWidget(update_mode);
    mod_poly_gl->addWidget(close_general_poly);
    mod_poly_gl->addWidget(general_mode_opts);

    modpolyBox->setLayout(mod_poly_gl);
    l->addWidget(modpolyBox);


    QGroupBox *boolBox = new QGroupBox("Apply Boolean Op");
    QVBoxLayout *bool_gl = new QVBoxLayout;
    bool_gl->setAlignment(Qt::AlignTop);
    QLabel *csg_modes_label = new QLabel("Current Operation:");
    bool_gl->addWidget(csg_modes_label);
    csg_modes = new QComboBox();
    csg_modes->addItem("Union");
    csg_modes->addItem("Subtraction");
    csg_modes->addItem("Intersection");
    csg_modes->setCurrentIndex(0);
    bool_gl->addWidget(csg_modes);
    apply_bool = new QPushButton("Apply");
    bool_gl->addWidget(apply_bool);
    QObject::connect(apply_bool, &QPushButton::released, this, &QPolyMod::apply_bool_op);
    boolBox->setLayout(bool_gl);
    l->addWidget(boolBox);


    QGroupBox *removeBox = new QGroupBox("Remove Polygon");
    QVBoxLayout *remove_gl = new QVBoxLayout;
    remove_gl->setAlignment(Qt::AlignTop);
    remove_poly = new QPushButton("Delete");
    remove_poly->setStyleSheet("QPushButton {color: red;}");
    remove_gl->addWidget(remove_poly);
    QObject::connect(remove_poly, &QPushButton::released, this, &QPolyMod::delete_poly);
    removeBox->setLayout(remove_gl);
    l->addWidget(removeBox);


    l->setAlignment(Qt::AlignTop);
    this->setLayout(l);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    select_mode->setChecked(true);
    mod_names_reset();
    toplevel_config(true);
}

QPolyMod::~QPolyMod()
{
}

void
QPolyMod::app_mod_names_reset(void *)
{
    mod_names_reset();
}

void
QPolyMod::mod_names_reset()
{
    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!gedp) {
	return;
    }

    // Make sure the Combo box list is current.
    mod_names->blockSignals(true);
    mod_names->clear();
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
    } else {
	mod_names->setCurrentIndex(0);
	if (mod_names->currentText().length()) {
	    select(mod_names->currentText());
	}
    }
    mod_names->blockSignals(false);
}

void
QPolyMod::poly_type_settings(struct bv_polygon *ip)
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
QPolyMod::polygon_update()
{
    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!gedp)
	return;
    
    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;

    // Pull settings
    bu_color_to_rgb_chars(&ps->edge_color->bc, p->s_color);
    BU_COLOR_CPY(&ip->fill_color, &ps->fill_color->bc);

    vect2d_t vdir = V2INIT_ZERO;
    vdir[0] = (fastf_t)(ps->fill_slope_x->text().toDouble());
    vdir[1] = (fastf_t)(ps->fill_slope_y->text().toDouble());
    V2MOVE(ip->fill_dir, vdir);
    ip->fill_delta = (fastf_t)(ps->fill_density->text().toDouble());

    // Set fill
    if (ps->fill_poly->isChecked()) {
	ip->fill_flag = 1;
    } else {
	ip->fill_flag = 0;
    }

    ip->sflag = 0;
    ip->mflag = 0;
    ip->aflag = 0;
    bv_update_polygon(p);
    emit view_updated(&gedp->ged_gvp);
}

void
QPolyMod::toplevel_config(bool)
{
    // Initialize
    struct ged *gedp = ((CADApp *)qApp)->gedp;
    bool draw_change = false;

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

    // Make sure the Combo box list is current.
    mod_names_reset();

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
QPolyMod::clear_pnt_selection(bool checked)
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
QPolyMod::select(const QString &poly)
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
QPolyMod::toggle_closed_poly(bool checked)
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

    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (do_bool && ip->type == BV_POLYGON_GENERAL && close_general_poly->isChecked()) {
	bg_clip_t op = bg_Union;
	if (do_bool) {
	    if (csg_modes->currentText() == "Subtraction") {
		op = bg_Difference;
	    }
	    if (csg_modes->currentText() == "Intersection") {
		op = bg_Intersection;
	    }
	}
	// If we're closing a general polygon and we're in boolean op mode,
	// that's our signal to complete the operation
	int pcnt = bv_polygon_csg(gedp->ged_gvp->gv_view_objs, p, op, 1);
	if (pcnt || op != bg_Union) {
	    bg_polygon_free(&ip->polygon);
	    BU_PUT(ip, struct bv_polygon);
	    bu_ptbl_rm(gedp->ged_gvp->gv_view_objs, (long *)p);
	    FREE_BV_SCENE_OBJ(p, &gedp->free_scene_obj->l);
	    p = NULL;
	}
	do_bool = false;
    }

    if (p) {
	ip->sflag = 0;
	ip->mflag = 0;
	ip->aflag = 0;

	bv_update_polygon(p);
    }

    toplevel_config(false);

    emit view_updated(&gedp->ged_gvp);
}

void
QPolyMod::apply_bool_op()
{
    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!gedp) {
	return;
    }

    if (!p)
	return;

    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
    if (ip->polygon.contour[0].open) {
	return;
    }

    bg_clip_t op = bg_Union;
    if (csg_modes->currentText() == "Subtraction") {
	op = bg_Difference;
    }
    if (csg_modes->currentText() == "Intersection") {
	op = bg_Intersection;
    }

    int pcnt = bv_polygon_csg(gedp->ged_gvp->gv_view_objs, p, op, 1);
    if (pcnt || op != bg_Union) {
	bg_polygon_free(&ip->polygon);
	BU_PUT(ip, struct bv_polygon);
	bu_ptbl_rm(gedp->ged_gvp->gv_view_objs, (long *)p);
	FREE_BV_SCENE_OBJ(p, &gedp->free_scene_obj->l);
	mod_names->setCurrentIndex(0);
	if (mod_names->currentText().length()) {
	    select(mod_names->currentText());
	} else {
	    p = NULL;
	}
    }

    emit view_updated(&gedp->ged_gvp);
}

void
QPolyMod::delete_poly()
{
    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!gedp) {
	return;
    }

    if (!p)
	return;

    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
    bg_polygon_free(&ip->polygon);
    BU_PUT(ip, struct bv_polygon);
    bu_ptbl_rm(gedp->ged_gvp->gv_view_objs, (long *)p);
    FREE_BV_SCENE_OBJ(p, &gedp->free_scene_obj->l);
    mod_names->setCurrentIndex(0);
    if (mod_names->currentText().length()) {
	select(mod_names->currentText());
    } else {
	p = NULL;
    }

    emit view_updated(&gedp->ged_gvp);
}

bool
QPolyMod::eventFilter(QObject *, QEvent *e)
{
    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!gedp) {
	return false;
    }

    QMouseEvent *m_e = NULL;

    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease || e->type() == QEvent::MouseButtonDblClick || e->type() == QEvent::MouseMove) {

	m_e = (QMouseEvent *)e;

	gedp->ged_gvp->gv_prevMouseX = gedp->ged_gvp->gv_mouse_x;
	gedp->ged_gvp->gv_prevMouseY = gedp->ged_gvp->gv_mouse_y;

	gedp->ged_gvp->gv_mouse_x = m_e->x();
	gedp->ged_gvp->gv_mouse_y = m_e->y();
    }

    if (!m_e)
	return false;

    printf("polygon mod\n");

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


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

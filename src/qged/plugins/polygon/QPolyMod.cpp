/*                     Q P O L Y M O D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2024 United States Government as represented by
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
/** @file QPolyMod.cpp
 *
 */

#include "common.h"
#include <vector>
#include <QLabel>
#include <QLineEdit>
#include <QButtonGroup>
#include <QGroupBox>
#include <QtGlobal>
#include "../../QgEdApp.h"
#include "QPolyCreate.h"
#include "QPolyMod.h"

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
    QObject::connect(ps, &QPolySettings::settings_changed, this, &QPolyMod::polygon_update_props);
    // We'll need to be aware if we specify a colliding view obj name
    QObject::connect(ps->view_name, &QLineEdit::textEdited, this, &QPolyMod::view_name_edit_str);
    QObject::connect(ps->view_name, &QLineEdit::editingFinished, this, &QPolyMod::view_name_update);
    // The sketch name gets enabled/disabled and in some modes syncs with the
    // view name.
    QObject::connect(ps->sketch_sync, &QCheckBox::toggled, this, &QPolyMod::sketch_sync_bool);
    QObject::connect(ps->sketch_name, &QLineEdit::textEdited, this, &QPolyMod::sketch_name_edit_str);
    QObject::connect(ps->view_name, &QLineEdit::editingFinished, this, &QPolyMod::sketch_name_edit);
    QObject::connect(ps->sketch_name, &QLineEdit::editingFinished, this, &QPolyMod::sketch_name_update);
    QObject::connect(ps, &QPolySettings::grid_snapping_changed, this, &QPolyMod::toggle_grid_snapping);
    QObject::connect(ps, &QPolySettings::line_snapping_changed, this, &QPolyMod::toggle_line_snapping);

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


    QGroupBox *viewsnapBox = new QGroupBox("Align View to Polygon");
    QVBoxLayout *viewsnap_gl = new QVBoxLayout;
    viewsnap_gl->setAlignment(Qt::AlignTop);
    viewsnap_poly = new QPushButton("Align");
    viewsnap_gl->addWidget(viewsnap_poly);
    QObject::connect(viewsnap_poly, &QPushButton::released, this, &QPolyMod::align_to_poly);
    viewsnapBox->setLayout(viewsnap_gl);
    l->addWidget(viewsnapBox);


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

    puf = new QPolyUpdateFilter();
    psf = new QPolySelectFilter();
    ppf = new QPolyPointFilter();
    pmf = new QPolyMoveFilter();
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
    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    // Make sure the Combo box list is current.
    mod_names->blockSignals(true);
    mod_names->clear();
    if (gedp) {
	struct bu_ptbl *view_objs = bv_view_objs(gedp->ged_gvp, BV_VIEW_OBJS);
	if (view_objs) {
	    for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
		if (s->s_type_flags & BV_POLYGONS) {
		    mod_names->addItem(bu_vls_cstr(&s->s_name));
		}
	    }
	}
    }
    if (p) {
	int cind = mod_names->findText(bu_vls_cstr(&p->s_name));
	mod_names->setCurrentIndex(cind);
    } else {
	mod_names->setCurrentIndex(0);

    }
    if (mod_names->currentText().length()) {
	select(mod_names->currentText());
    }
    mod_names->blockSignals(false);
}

void
QPolyMod::poly_type_settings(struct bv_polygon *ip)
{
    if (!ip || !ip->polygon.contour)
	return;
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
QPolyMod::polygon_update_props()
{
    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
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

    // TODO - this should be a visual-properties-only update, but libbg doesn't support that yet.
    bv_update_polygon(p, p->s_v, BV_POLYGON_UPDATE_PROPS_ONLY);
    emit view_updated(QG_VIEW_REFRESH);
}

void
QPolyMod::toplevel_config(bool)
{
    // Initialize
    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    bool draw_change = false;

    // This function is called when a top level mode change was initiated
    // by a selection button.  Clear any selected points being displayed -
    // when we're switching modes at this level, we always start with a
    // blank slate for points.
    if (gedp) {
	struct bu_ptbl *view_objs = bv_view_objs(gedp->ged_gvp, BV_VIEW_OBJS);
	if (view_objs) {
	    for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
		if (s->s_type_flags & BV_POLYGONS) {
		    // clear any selected points in non-current polygons
		    struct bv_polygon *ip = (struct bv_polygon *)s->s_i_data;
		    if (ip->curr_point_i != -1) {
			bu_log("Clear pnt selection\n");
			draw_change = true;
			ip->curr_point_i = -1;
			ip->curr_contour_i = 0;
			bv_update_polygon(s, s->s_v, BV_POLYGON_UPDATE_PROPS_ONLY);
		    }
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
	emit view_updated(QG_VIEW_REFRESH);
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
	if (!ip)
	    return;
	ptype = ip->type;
    }
    if (ptype != BV_POLYGON_GENERAL) {
	return;
    }
    bu_log("got pnt selection clear\n");
    ip->curr_point_i = -1;
    ip->curr_contour_i = 0;

    bv_update_polygon(p, p->s_v, BV_POLYGON_UPDATE_PROPS_ONLY);

    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    emit view_updated(QG_VIEW_REFRESH);
}

void
QPolyMod::select(const QString &poly)
{
    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    p = NULL;
    struct bu_ptbl *view_objs = bv_view_objs(gedp->ged_gvp, BV_VIEW_OBJS);
    if (view_objs) {
	for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
	    if (s->s_type_flags & BV_POLYGONS) {
		QString pname(bu_vls_cstr(&s->s_name));
		if (pname == poly) {
		    p = s;
		    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
		    poly_type_settings(ip);
		    ps->settings_sync(p);
		    ps->view_name->setText(pname);
		    if (ip->u_data) {
			struct directory *dp = (struct directory *)ip->u_data;
			ps->sketch_sync->blockSignals(true);
			ps->sketch_sync->setChecked(true);
			ps->sketch_sync->blockSignals(false);
			ps->sketch_name->blockSignals(true);
			ps->sketch_name->setText(dp->d_namep);
			ps->sketch_name->setEnabled(true);
			ps->sketch_name->blockSignals(false);
		    }

		    return;
		}
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

    // If we don't have a valid polygon, bail
    if (!ip->polygon.contour)
	return;

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

    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    if (do_bool && ip->type == BV_POLYGON_GENERAL && close_general_poly->isChecked()) {
	struct bu_ptbl *view_objs = bv_view_objs(gedp->ged_gvp, BV_VIEW_OBJS);
	if (view_objs) {
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
	    int pcnt = 0;
	    std::vector<struct bv_scene_obj *> cleanup;
	    for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
		struct bv_scene_obj *target = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
		if (target == p)
		    continue;
		if (!(target->s_type_flags & BV_POLYGONS))
		    continue;
		pcnt += bv_polygon_csg(target, p, op);
		struct bv_polygon *vp = (struct bv_polygon *)target->s_i_data;
		if (!vp->polygon.num_contours || !vp->polygon.contour)
		    cleanup.push_back(target);
	    }
	    for (size_t i = 0; i < cleanup.size(); i++) {
		struct bv_polygon *vp = (struct bv_polygon *)cleanup[i]->s_i_data;
		bg_polygon_free(&vp->polygon);
		BU_PUT(vp, struct bv_polygon);
		cleanup[i]->s_i_data = NULL;
		bv_obj_put(cleanup[i]);
	    }
	    if (pcnt || op != bg_Union) {
		bg_polygon_free(&ip->polygon);
		BU_PUT(ip, struct bv_polygon);
		bv_obj_put(p);
		p = NULL;
	    }
	    do_bool = false;
	}
    }

    if (p) {
	bv_update_polygon(p, p->s_v, BV_POLYGON_UPDATE_DEFAULT);
    }

    toplevel_config(false);

    emit view_updated(QG_VIEW_REFRESH);
}

void
QPolyMod::apply_bool_op()
{
    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m || !p)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;

    if (!ip->polygon.contour)
	return;

    if (ip->polygon.contour[0].open) {
	return;
    }

    bg_clip_t op = bg_Union;
    if (csg_modes->currentText() == "Union") {
	op = bg_Union;
    }
    if (csg_modes->currentText() == "Subtraction") {
	op = bg_Difference;
    }
    if (csg_modes->currentText() == "Intersection") {
	op = bg_Intersection;
    }

    struct bu_ptbl *view_objs = bv_view_objs(gedp->ged_gvp, BV_VIEW_OBJS);
    if (view_objs) {
	std::vector<struct bv_scene_obj *> cleanup;
	for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	    struct bv_scene_obj *target = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
	    if (target == p)
		continue;
	    if (!(target->s_type_flags & BV_POLYGONS))
		continue;
	    bv_polygon_csg(target, p, op);
	    struct bv_polygon *vp = (struct bv_polygon *)target->s_i_data;
	    if (!vp->polygon.num_contours || !vp->polygon.contour)
		cleanup.push_back(target);
	}
	for (size_t i = 0; i < cleanup.size(); i++) {
	    struct bv_polygon *vp = (struct bv_polygon *)cleanup[i]->s_i_data;
	    bg_polygon_free(&vp->polygon);
	    BU_PUT(vp, struct bv_polygon);
	    cleanup[i]->s_i_data = NULL;
	    bv_obj_put(cleanup[i]);
	}
    }

    emit view_updated(QG_VIEW_REFRESH);
}

void
QPolyMod::align_to_poly()
{
    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m || !p)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;

    point_t center;
    vect_t dir = VINIT_ZERO;
    VSET(dir, ip->vp[0], ip->vp[1], ip->vp[2]);
    bg_plane_pt_at(&center, ip->vp, 0, 0);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, center);
    bn_ae_vec(&gedp->ged_gvp->gv_aet[0], &gedp->ged_gvp->gv_aet[1], dir);
    gedp->ged_gvp->gv_aet[2] = 0;
    bv_mat_aet(gedp->ged_gvp);

    bv_update(gedp->ged_gvp);

    emit view_updated(QG_VIEW_REFRESH);
}

void
QPolyMod::delete_poly()
{
    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m || !p)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
    bg_polygon_free(&ip->polygon);
    BU_PUT(ip, struct bv_polygon);
    bv_obj_put(p);
    mod_names->setCurrentIndex(0);
    if (mod_names->currentText().length()) {
	select(mod_names->currentText());
    } else {
	p = NULL;
    }

    emit view_updated(QG_VIEW_REFRESH);
}

void
QPolyMod::sketch_sync_bool(bool)
{
    sketch_name_edit();
}

void
QPolyMod::sketch_name_edit_str(const QString &)
{
    sketch_name_edit();
}


void
QPolyMod::sketch_name_edit()
{
    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp) {
	ps->sketch_name->setPlaceholderText("No .g file open");
	ps->sketch_name->setStyleSheet("color: rgb(200,200,200)");
	ps->sketch_name->setEnabled(false);
	return;
    }

    if (ps->sketch_sync->isChecked()) {
	char *sname = NULL;
	if (!ps->sketch_name->placeholderText().length()) {
	    if (ps->view_name->placeholderText().length()) {
		ps->sketch_name->setPlaceholderText(ps->view_name->placeholderText());
		sname = bu_strdup(ps->sketch_name->placeholderText().toLocal8Bit().data());
	    }
	} else {
	    sname = bu_strdup(ps->sketch_name->placeholderText().toLocal8Bit().data());
	}
	if (!ps->sketch_name->text().length()) {
	    if (ps->view_name->text().length()) {
		ps->sketch_name->setPlaceholderText(ps->view_name->text());
		if (sname)
		    bu_free(sname, "sname");
		sname = bu_strdup(ps->sketch_name->placeholderText().toLocal8Bit().data());
	    }
	} else {
	    if (sname)
		bu_free(sname, "sname");
	    sname = bu_strdup(ps->sketch_name->text().toLocal8Bit().data());
	}

	struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
	if (!sname && ip->u_data) {
	    struct directory *dp = (struct directory *)ip->u_data;
	    ps->sketch_name->setPlaceholderText(QString(dp->d_namep));
	    sname = bu_strdup(ps->sketch_name->placeholderText().toLocal8Bit().data());
	}

	if (sname) {
	    // There may be a dp associated with the polygon.  If so, and the name
	    // matches, then a save operation will replace the old copy with an
	    // updated version.
	    bool match_curr = false;
	    if (ip->u_data) {
		struct directory *dp = (struct directory *)ip->u_data;
		if (BU_STR_EQUAL(dp->d_namep, sname)) {
		    match_curr = true;
		}
	    }
	    if (!match_curr) {
		// Not a name match to existing dp - fall back on db check.
		if (db_lookup(gedp->dbip, sname, LOOKUP_QUIET) != RT_DIR_NULL) {
		    ps->sketch_name->setStyleSheet("color: rgb(255,0,0)");
		} else {
		    ps->sketch_name->setStyleSheet("");
		}
	    } else {
		ps->sketch_name->setStyleSheet("");
	    }
	    bu_free(sname, "sname");
	} else {
	    ps->sketch_name->setStyleSheet("");
	}
	ps->sketch_name->setEnabled(true);
    } else {
	ps->sketch_name->setPlaceholderText("Enable to save sketch");
	ps->sketch_name->setStyleSheet("color: rgb(200,200,200)");
	ps->sketch_name->setEnabled(false);
    }
}

void
QPolyMod::sketch_name_update()
{
    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    if (!p || !ps->sketch_sync->isChecked()) {
	return;
    }

    char *sk_name = NULL;
    if (!ps->sketch_name->placeholderText().length()) {
	if (ps->view_name->placeholderText().length()) {
	    ps->sketch_name->setPlaceholderText(ps->view_name->placeholderText());
	    sk_name = bu_strdup(ps->sketch_name->placeholderText().toLocal8Bit().data());
	}
    } else {
	sk_name = bu_strdup(ps->sketch_name->placeholderText().toLocal8Bit().data());
    }
    if (!ps->sketch_name->text().length()) {
	if (ps->view_name->text().length()) {
	    ps->sketch_name->setPlaceholderText(ps->view_name->text());
	    bu_free(sk_name, "sk_name");
	    sk_name = bu_strdup(ps->sketch_name->placeholderText().toLocal8Bit().data());
	}
    } else {
	bu_free(sk_name, "sk_name");
	sk_name = bu_strdup(ps->sketch_name->text().toLocal8Bit().data());
    }

    if (!sk_name)
	return;

    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
    if (ip->u_data) {
	// remove previous dp, if name is different.  If name
	// matches, we're done
	struct directory *dp = (struct directory *)ip->u_data;
	if (BU_STR_EQUAL(sk_name, dp->d_namep)) {
	    bu_free(sk_name, "name copy");
	    return;
	}

	// If the proposed new name collides with an existing object,
	// we're done.
	if (db_lookup(gedp->dbip, sk_name, LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_free(sk_name, "name copy");
	    return;
	}

	// Passed the tests - remove old object.
	int ac = 2;
	const char *av[3];
	av[0] = "kill";
	av[1] = dp->d_namep;
	av[2] = NULL;
	ged_exec(gedp, ac, av);
    }

    ip->u_data = (void *)db_scene_obj_to_sketch(gedp->dbip, sk_name, p);
    emit view_updated(QG_VIEW_DB);

    bu_free(sk_name, "name copy");
}


void
QPolyMod::view_name_edit_str(const QString &)
{
    view_name_edit();
}


void
QPolyMod::view_name_edit()
{
    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    if (!ps->uniq_obj_name(NULL, gedp->ged_gvp)) {
	ps->view_name->setStyleSheet("color: rgb(255,0,0)");
    } else {
	ps->view_name->setStyleSheet("");
    }
}

void
QPolyMod::view_name_update()
{
    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m || !p)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    // Make sure the name is unique
    struct bu_vls vname = BU_VLS_INIT_ZERO;
    if (!ps->uniq_obj_name(&vname, gedp->ged_gvp)) {
	bu_vls_free(&vname);
	return;
    }

    bu_vls_sprintf(&p->s_name, "%s", bu_vls_cstr(&vname));
    bu_vls_free(&vname);
    emit view_updated(QG_VIEW_REFRESH);
}


void
QPolyMod::toggle_line_snapping(bool s)
{
    struct bview *v = (cf) ? cf->v : NULL;
    struct bv_scene_obj *co = (cf) ? cf->wp : NULL;
    if (!v || !co)
	return;

    v->gv_s->gv_snap_flags = BV_SNAP_VIEW;
    bu_ptbl_reset(&v->gv_s->gv_snap_objs);
    if (!s) {
	v->gv_s->gv_snap_lines = 0;
    } else {
	// Turn snapping on if we have other polygons to snap to
	struct bu_ptbl *view_objs = bv_view_objs(v, BV_VIEW_OBJS);
	if (!view_objs)
	    return;
	for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	    struct bv_scene_obj *so = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
	    if (so == co)
		continue;
	    if (so->s_type_flags & BV_POLYGONS)
		bu_ptbl_ins(&v->gv_s->gv_snap_objs, (long *)so);
	}
	if (BU_PTBL_LEN(&v->gv_s->gv_snap_objs)) {
	    v->gv_s->gv_snap_lines = 1;
	} else {
	    v->gv_s->gv_snap_lines = 0;
	}
    }

    emit settings_changed(QG_VIEW_DRAWN);
}

void
QPolyMod::toggle_grid_snapping(bool s)
{
    struct bview *v = (cf) ? cf->v : NULL;
    if (!v)
	return;

    v->gv_s->gv_snap_flags = BV_SNAP_VIEW;
    if (!s) {
	v->gv_s->gv_grid.snap = 0;
    } else {
	v->gv_s->gv_grid.snap = 1;
    }

    emit settings_changed(QG_VIEW_DRAWN);
}

void
QPolyMod::checkbox_refresh(unsigned long long)
{
    struct bview *v = (cf) ? cf->v : NULL;
    if (!v)
	return;

    ps->grid_snapping->blockSignals(true);
    if (v->gv_s->gv_grid.snap) {
	ps->grid_snapping->setCheckState(Qt::Checked);
    } else {
	ps->grid_snapping->setCheckState(Qt::Unchecked);
    }
    ps->grid_snapping->blockSignals(false);

    ps->line_snapping->blockSignals(true);
    if (v->gv_s->gv_snap_lines) {
	ps->line_snapping->setCheckState(Qt::Checked);
    } else {
	ps->line_snapping->setCheckState(Qt::Unchecked);
    }
    ps->line_snapping->blockSignals(false);
}

void
QPolyMod::propagate_update(int)
{
    emit view_updated(QG_VIEW_REFRESH);
}


bool
QPolyMod::eventFilter(QObject *, QEvent *e)
{
    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m)
	return false;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return false;

    // We might be selecting or modifying - if the former, we may
    // not have a current polygon.
    struct bv_polygon *ip = (p) ? (struct bv_polygon *)p->s_i_data : NULL;

    // The mouse filter to use depends on the mode - find out
    cf = puf;
    if (select_mode->isChecked())
	cf = psf;
    if (move_mode->isChecked())
	cf = pmf;
    if (update_mode->isChecked() && ip && ip->type == BV_POLYGON_GENERAL) {
	if (append_pnt->isChecked() || select_pnt->isChecked())
	    cf = ppf;
    }

    // Set libqtcad know what the current polygon is
    cf->wp = p;
    cf->v = (p) ? p->s_v : gedp->ged_gvp;
    cf->ptype = (ip) ? ip->type : BV_POLYGON_GENERAL;
    checkbox_refresh(0);

    // Connect whatever the current filter is to pass on updating signals from
    // the libqtcad logic.
    QObject::connect(cf, &QgPolyFilter::view_updated, this, &QPolyMod::propagate_update);

    // Match the settings
    cf->op = bg_None;
    cf->fill_poly = (ps->fill_poly->isChecked()) ? true : false;
    cf->fill_slope_x = (fastf_t)(ps->fill_slope_x->text().toDouble());
    cf->fill_slope_y = (fastf_t)(ps->fill_slope_y->text().toDouble());
    cf->fill_density = (fastf_t)(ps->fill_density->text().toDouble());
    BU_COLOR_CPY(&cf->fill_color, &ps->fill_color->bc);
    BU_COLOR_CPY(&cf->edge_color, &ps->edge_color->bc);
    cf->vZ = (fastf_t)(ps->vZ->text().toDouble());

    // Run the guts of the libqtcad filter
    bool ret = cf->eventFilter(NULL, e);

    // Retrieve the scene object from the libqtcad data container
    p = cf->wp;
    ip = (p) ? (struct bv_polygon *)p->s_i_data : NULL;

    // If we need to, update our selected list entry
    if (select_mode->isChecked() && p) {
	int cind = mod_names->findText(bu_vls_cstr(&p->s_name));
	mod_names->blockSignals(true);
	mod_names->setCurrentIndex(cind);
	mod_names->blockSignals(false);
	poly_type_settings(ip);
	ps->settings_sync(p);
    }

    // Because the active filter may change, we only maintain the
    // signal connection for the duration of the event
    QObject::disconnect(cf, &QgPolyFilter::view_updated, this, &QPolyMod::propagate_update);

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

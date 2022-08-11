/*                     Q P O L Y M O D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
#include <QLabel>
#include <QLineEdit>
#include <QButtonGroup>
#include <QGroupBox>
#include "../../app.h"
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
    QgModel *m = ((CADApp *)qApp)->mdl;
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
	for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
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

    }
    if (mod_names->currentText().length()) {
	select(mod_names->currentText());
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
QPolyMod::polygon_update_props()
{
    QgModel *m = ((CADApp *)qApp)->mdl;
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
    emit view_updated(QTCAD_VIEW_REFRESH);
}

void
QPolyMod::toplevel_config(bool)
{
    // Initialize
    QgModel *m = ((CADApp *)qApp)->mdl;
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
		    bv_update_polygon(p, p->s_v, BV_POLYGON_UPDATE_PROPS_ONLY);
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
	emit view_updated(QTCAD_VIEW_REFRESH);
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

    bv_update_polygon(p, p->s_v, BV_POLYGON_UPDATE_PROPS_ONLY);

    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    emit view_updated(QTCAD_VIEW_REFRESH);
}

void
QPolyMod::select(const QString &poly)
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    p = NULL;
    struct bu_ptbl *view_objs = bv_view_objs(gedp->ged_gvp, BV_VIEW_OBJS);
    for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
	if (s->s_type_flags & BV_POLYGONS) {
	    QString pname(bu_vls_cstr(&s->s_uuid));
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

    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    if (do_bool && ip->type == BV_POLYGON_GENERAL && close_general_poly->isChecked()) {
	struct bu_ptbl *view_objs = bv_view_objs(gedp->ged_gvp, BV_VIEW_OBJS);
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
	int pcnt = bv_polygon_csg(view_objs, p, op, 1);
	if (pcnt || op != bg_Union) {
	    bg_polygon_free(&ip->polygon);
	    BU_PUT(ip, struct bv_polygon);
	    bv_obj_put(p);
	    p = NULL;
	}
	do_bool = false;
    }

    if (p) {
	bv_update_polygon(p, p->s_v, BV_POLYGON_UPDATE_DEFAULT);
    }

    toplevel_config(false);

    emit view_updated(QTCAD_VIEW_REFRESH);
}

void
QPolyMod::apply_bool_op()
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m || !p)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
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

    struct bu_ptbl *view_objs = bv_view_objs(gedp->ged_gvp, BV_VIEW_OBJS);
    int pcnt = bv_polygon_csg(view_objs, p, op, 1);
    if (pcnt || op != bg_Union) {
	bg_polygon_free(&ip->polygon);
	BU_PUT(ip, struct bv_polygon);
	bv_obj_put(p);
	mod_names->setCurrentIndex(0);
	if (mod_names->currentText().length()) {
	    select(mod_names->currentText());
	} else {
	    p = NULL;
	}
    }

    emit view_updated(QTCAD_VIEW_REFRESH);
}

void
QPolyMod::align_to_poly()
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m || !p)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
    bv_sync(gedp->ged_gvp, &ip->v);
    bv_update(gedp->ged_gvp);

    emit view_updated(QTCAD_VIEW_REFRESH);
}

void
QPolyMod::delete_poly()
{
    QgModel *m = ((CADApp *)qApp)->mdl;
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

    emit view_updated(QTCAD_VIEW_REFRESH);
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
    QgModel *m = ((CADApp *)qApp)->mdl;
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
    QgModel *m = ((CADApp *)qApp)->mdl;
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
    emit view_updated(QTCAD_VIEW_DB);

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
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    char *vname = NULL;
    if (ps->view_name->placeholderText().length()) {
	vname = bu_strdup(ps->view_name->placeholderText().toLocal8Bit().data());
    }
    if (ps->view_name->text().length()) {
	bu_free(vname, "vname");
	vname = bu_strdup(ps->view_name->text().toLocal8Bit().data());
    }
    bool colliding = false;
    struct bu_ptbl *view_objs = bv_view_objs(gedp->ged_gvp, BV_VIEW_OBJS);
    for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
	if (p != s && BU_STR_EQUAL(bu_vls_cstr(&s->s_uuid), vname)) {
	    colliding = true;
	}
    }
    if (colliding) {
	ps->view_name->setStyleSheet("color: rgb(255,0,0)");
    } else {
	ps->view_name->setStyleSheet("");
    }

    bu_free(vname, "vname");
}

void
QPolyMod::view_name_update()
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    char *vname = NULL;
    if (ps->view_name->placeholderText().length()) {
	vname = bu_strdup(ps->view_name->placeholderText().toLocal8Bit().data());
    }
    if (ps->view_name->text().length()) {
	bu_free(vname, "vname");
	vname = bu_strdup(ps->view_name->text().toLocal8Bit().data());
    }

    if (!vname)
	return;

    bool colliding = false;
    struct bu_ptbl *view_objs = bv_view_objs(gedp->ged_gvp, BV_VIEW_OBJS);
    for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
	if (p != s && BU_STR_EQUAL(bu_vls_cstr(&s->s_uuid), vname)) {
	    colliding = true;
	}
    }
    if (colliding) {
	bu_free(vname, "vname");
	return;
    }

    bu_vls_sprintf(&p->s_uuid, "%s", vname);
    bu_free(vname, "vname");
    emit view_updated(QTCAD_VIEW_REFRESH);

}

bool
QPolyMod::eventFilter(QObject *, QEvent *e)
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return false;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return false;

    QMouseEvent *m_e = NULL;

    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease || e->type() == QEvent::MouseButtonDblClick || e->type() == QEvent::MouseMove) {

	m_e = (QMouseEvent *)e;

	gedp->ged_gvp->gv_prevMouseX = gedp->ged_gvp->gv_mouse_x;
	gedp->ged_gvp->gv_prevMouseY = gedp->ged_gvp->gv_mouse_y;

#ifdef USE_QT6
	gedp->ged_gvp->gv_mouse_x = m_e->position().x();
	gedp->ged_gvp->gv_mouse_y = m_e->position().y();
#else
	gedp->ged_gvp->gv_mouse_x = m_e->x();
	gedp->ged_gvp->gv_mouse_y = m_e->y();
#endif
    }

    if (!m_e)
	return false;

    printf("polygon mod\n");

    if (m_e->type() == QEvent::MouseButtonPress && m_e->buttons().testFlag(Qt::LeftButton)) {

	if (select_mode->isChecked()) {
	    struct bu_ptbl *view_objs = bv_view_objs(gedp->ged_gvp, BV_VIEW_OBJS);
	    p = bv_select_polygon(view_objs, gedp->ged_gvp);
	    if (p) {
		int cind = mod_names->findText(bu_vls_cstr(&p->s_uuid));
		mod_names->blockSignals(true);
		mod_names->setCurrentIndex(cind);
		mod_names->blockSignals(false);
		struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
		poly_type_settings(ip);
		ps->settings_sync(p);
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
#ifdef USE_QT6
	    p->s_v->gv_mouse_x = m_e->position().x();
	    p->s_v->gv_mouse_y = m_e->position().y();
#else
	    p->s_v->gv_mouse_x = m_e->x();
	    p->s_v->gv_mouse_y = m_e->y();
#endif
	    bv_update_polygon(p, p->s_v, BV_POLYGON_UPDATE_PT_APPEND);

	    emit view_updated(QTCAD_VIEW_REFRESH);
	    return true;
	}

	if (!move_mode->isChecked() && select_pnt->isChecked() && ip->type == BV_POLYGON_GENERAL) {
#ifdef USE_QT6
	    p->s_v->gv_mouse_x = m_e->position().x();
	    p->s_v->gv_mouse_y = m_e->position().y();
#else
	    p->s_v->gv_mouse_x = m_e->x();
	    p->s_v->gv_mouse_y = m_e->y();
#endif
	    bv_update_polygon(p, p->s_v, BV_POLYGON_UPDATE_PT_SELECT);
	    emit view_updated(QTCAD_VIEW_REFRESH);
	    return true;
	}

	// When we're dealing with polygons stray left clicks shouldn't zoom - just
	// consume them if we're not using them above.
	return true;
    }

    if (m_e->type() == QEvent::MouseButtonPress) {
	// We also don't want other stray mouse clicks to do something surprising
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
		bv_update_polygon(p, p->s_v, BV_POLYGON_UPDATE_PT_MOVE);
		emit view_updated(QTCAD_VIEW_REFRESH);
	    } else if (move_mode->isChecked()) {
		bu_log("move polygon mode\n");
		clear_pnt_selection(false);
		bv_move_polygon(p);
		emit view_updated(QTCAD_VIEW_REFRESH);
	    } else {
		bv_update_polygon(p, p->s_v, BV_POLYGON_UPDATE_DEFAULT);
		emit view_updated(QTCAD_VIEW_REFRESH);
	    }
	    return true;
	}
    }

    if (m_e->type() == QEvent::MouseButtonRelease) {
	emit view_updated(QTCAD_VIEW_REFRESH);
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

/*                  Q P O L Y C R E A T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2023 United States Government as represented by
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
/** @file QPolyCreate.cpp
 *
 */

#include "common.h"
#include <vector>
#include <QLabel>
#include <QLineEdit>
#include <QButtonGroup>
#include <QGroupBox>
#include <QtGlobal>
#include "../../app.h"
#include "QPolyCreate.h"
#include "qtcad/SignalFlags.h"

QPolyCreate::QPolyCreate()
    : QWidget()
{
    QVBoxLayout *l = new QVBoxLayout;

    QButtonGroup *t_grp = new QButtonGroup();

    QGroupBox *addpolyBox = new QGroupBox("Add Polygon");
    QVBoxLayout *add_poly_gl = new QVBoxLayout;
    add_poly_gl->setAlignment(Qt::AlignTop);

    QLabel *csg_modes_label = new QLabel("Boolean Operation:");
    add_poly_gl->addWidget(csg_modes_label);
    csg_modes = new QComboBox();
    csg_modes->addItem("None");
    csg_modes->addItem("Union");
    csg_modes->addItem("Subtraction");
    csg_modes->addItem("Intersection");
    csg_modes->setCurrentIndex(0);
    add_poly_gl->addWidget(csg_modes);

    circle_mode = new QRadioButton("Circle");
    circle_mode->setIcon(QIcon(QPixmap(":circle.svg")));
    QObject::connect(circle_mode, &QCheckBox::toggled, this, &QPolyCreate::toplevel_config);
    t_grp->addButton(circle_mode);
    ellipse_mode = new QRadioButton("Ellipse");
    ellipse_mode->setIcon(QIcon(QPixmap(":ellipse.svg")));
    QObject::connect(ellipse_mode, &QCheckBox::toggled, this, &QPolyCreate::toplevel_config);
    t_grp->addButton(ellipse_mode);
    square_mode = new QRadioButton("Square");
    square_mode->setIcon(QIcon(QPixmap(":square.svg")));
    QObject::connect(square_mode, &QCheckBox::toggled, this, &QPolyCreate::toplevel_config);
    t_grp->addButton(square_mode);
    rectangle_mode = new QRadioButton("Rectangle");
    rectangle_mode->setIcon(QIcon(QPixmap(":rectangle.svg")));
    QObject::connect(rectangle_mode, &QCheckBox::toggled, this, &QPolyCreate::toplevel_config);
    t_grp->addButton(rectangle_mode);
    general_mode = new QRadioButton("General");
    general_mode->setIcon(QIcon(QPixmap(":polygon.svg")));
    QObject::connect(general_mode, &QCheckBox::toggled, this, &QPolyCreate::toplevel_config);
    t_grp->addButton(general_mode);

    add_poly_gl->addWidget(circle_mode);
    add_poly_gl->addWidget(ellipse_mode);
    add_poly_gl->addWidget(square_mode);
    add_poly_gl->addWidget(rectangle_mode);
    add_poly_gl->addWidget(general_mode);

    addpolyBox->setLayout(add_poly_gl);
    l->addWidget(addpolyBox);

    close_general_poly = new QCheckBox("Close polygon");
    // Disabled if we're not a general polygon
    close_general_poly->setChecked(true);
    close_general_poly->setDisabled(true);
    QObject::connect(close_general_poly, &QCheckBox::toggled, this, &QPolyCreate::finalize);
    l->addWidget(close_general_poly);

    QGroupBox *defaultBox = new QGroupBox("Settings");
    QVBoxLayout *default_gl = new QVBoxLayout;
    default_gl->setAlignment(Qt::AlignTop);
    ps = new QPolySettings();
    // Set an initial name (user can change, but we need something if they
    // don't have a specific name in mind.)
    struct bu_vls pname = BU_VLS_INIT_ZERO;
    poly_cnt++;
    bu_vls_sprintf(&pname, "polygon_%09d", poly_cnt);
    ps->view_name->setPlaceholderText(QString(bu_vls_cstr(&pname)));
    bu_vls_free(&pname);

    // We'll need to be aware if we specify a colliding view obj name
    QObject::connect(ps->view_name, &QLineEdit::textEdited, this, &QPolyCreate::view_sync_str);

    // The sketch name gets enabled/disabled and in some modes syncs with the
    // view name.
    QObject::connect(ps->sketch_sync, &QCheckBox::toggled, this, &QPolyCreate::sketch_sync_bool);
    QObject::connect(ps->view_name, &QLineEdit::textEdited, this, &QPolyCreate::sketch_sync_str);
    QObject::connect(ps->sketch_name, &QLineEdit::textEdited, this, &QPolyCreate::sketch_sync_str);

    default_gl->addWidget(ps);
    defaultBox->setLayout(default_gl);
    l->addWidget(defaultBox);

    QGroupBox *copyBox = new QGroupBox("Create Polygon from Data");
    QVBoxLayout *copy_gl = new QVBoxLayout;
    copy_gl->setAlignment(Qt::AlignTop);

    QLabel *vpoly_label = new QLabel("Copy view polygon:");
    vpoly_name = new QLineEdit();
    vpoly_copy = new QPushButton("Copy");
    QObject::connect(vpoly_copy, &QPushButton::released, this, &QPolyCreate::do_vpoly_copy);
    copy_gl->addWidget(vpoly_label);
    copy_gl->addWidget(vpoly_name);
    copy_gl->addWidget(vpoly_copy);

    QLabel *import_label = new QLabel("Import from sketch:");
    import_name = new QLineEdit();
    import_sketch = new QPushButton("Import");
    QObject::connect(import_sketch, &QPushButton::released, this, &QPolyCreate::do_import_sketch);
    copy_gl->addWidget(import_label);
    copy_gl->addWidget(import_name);
    copy_gl->addWidget(import_sketch);
    copyBox->setLayout(copy_gl);
    l->addWidget(copyBox);

    l->setAlignment(Qt::AlignTop);
    this->setLayout(l);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    // By default, start in circle addition mode
    circle_mode->setChecked(true);
    toplevel_config(true);

    pcf = new QPolyCreateFilter();
}

QPolyCreate::~QPolyCreate()
{
}

void
QPolyCreate::finalize(bool)
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m || !p)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    // Close the general polygon - if that's what we're creating,
    // at this point it will still be open.
    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
    if (ip->polygon.contour[0].open) {

	if (ip->polygon.contour[0].num_points < 3) {
	    // If we're trying to finalize and we have less than
	    // three points, just remove - we didn't get enough
	    // to make a closed polygon.
	    bg_polygon_free(&ip->polygon);
	    BU_PUT(ip, struct bv_polygon);
	    bv_obj_put(p);
	    do_bool = false;
	    p = NULL;
	    emit view_updated(QTCAD_VIEW_REFRESH);
	    return;
	}

	ip->polygon.contour[0].open = 0;
	bv_update_polygon(p, p->s_v, BV_POLYGON_UPDATE_DEFAULT);
    }

    close_general_poly->blockSignals(true);
    close_general_poly->setChecked(true);
    close_general_poly->blockSignals(false);
    close_general_poly->setDisabled(true);

    // Have close polygon - do boolean operation, if any.  Whether the new
    // polygon becomes its own object or just alters existing object
    // definitions depends on the boolean op setting.
    op = bg_Union;
    if (do_bool) {
	if (csg_modes->currentText() == "Subtraction") {
	    op = bg_Difference;
	}
	if (csg_modes->currentText() == "Intersection") {
	    op = bg_Intersection;
	}
    }

    int pcnt = 0;
    struct bu_ptbl *view_objs = bv_view_objs(gedp->ged_gvp, BV_VIEW_OBJS);
    if (!view_objs)
	return;
    if (do_bool) {
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
    }
    if (pcnt || op != bg_Union) {
	bg_polygon_free(&ip->polygon);
	BU_PUT(ip, struct bv_polygon);
	bv_obj_put(p);
    } else {

	// Check if we have a name collision - if we do, it's no go
	char *vname = NULL;
	if (ps->view_name->placeholderText().length()) {
	    vname = bu_strdup(ps->view_name->placeholderText().toLocal8Bit().data());
	}
	if (ps->view_name->text().length()) {
	    bu_free(vname, "vname");
	    vname = bu_strdup(ps->view_name->text().toLocal8Bit().data());
	}
	bool colliding = false;
	for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
	    if (BU_STR_EQUAL(bu_vls_cstr(&s->s_uuid), vname)) {
		colliding = true;
	    }
	}
	bu_free(vname, "vname");
	if (colliding) {
	    bg_polygon_free(&ip->polygon);
	    BU_PUT(ip, struct bv_polygon);
	    bv_obj_put(p);
	    do_bool = false;
	    p = NULL;
	    emit view_updated(QTCAD_VIEW_REFRESH);
	    return;
	}

	// Either a non-boolean creation or a Union with no interactions -
	// either way we're keeping it, so assign a proper name
	if (ps->view_name->text().length()) {
	    bu_vls_sprintf(&p->s_uuid, "%s", ps->view_name->text().toLocal8Bit().data());
	} else {
	    bu_vls_sprintf(&p->s_uuid, "%s", ps->view_name->placeholderText().toLocal8Bit().data());
	}

	// Done processing view object - increment name
	poly_cnt++;
	ps->view_name->clear();
	struct bu_vls pname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&pname, "polygon_%09d", poly_cnt);
	ps->view_name->setPlaceholderText(QString(bu_vls_cstr(&pname)));
	bu_vls_free(&pname);

	// No longer need mouse movements to adjust parameters - turn off callback
	p->s_update_callback = NULL;

	// If we're also writing this out as a sketch, take care of that.
	if (ps->sketch_sync->isChecked()) {
	    char *sk_name = NULL;
	    if (ps->sketch_name->placeholderText().length()) {
		sk_name = bu_strdup(ps->sketch_name->placeholderText().toLocal8Bit().data());
	    }
	    if (ps->sketch_name->text().length()) {
		bu_free(sk_name, "sk_name");
		sk_name = bu_strdup(ps->sketch_name->text().toLocal8Bit().data());
	    }
	    if (sk_name && db_lookup(gedp->dbip, sk_name, LOOKUP_QUIET) == RT_DIR_NULL) {
		ip->u_data = (void *)db_scene_obj_to_sketch(gedp->dbip, sk_name, p);
		emit view_updated(QTCAD_VIEW_DB);
	    }
	    bu_free(sk_name, "name cpy");
	} else {
	    ip->u_data = NULL;
	}

	// Done with sketch - update name for next polygon
	ps->sketch_name->setPlaceholderText("");
	ps->sketch_name->setText("");
	sketch_sync();
    }

    do_bool = false;
    p = NULL;
    emit view_updated(QTCAD_VIEW_REFRESH);
}

void
QPolyCreate::do_vpoly_copy()
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    // Check if we have a name collision - if we do, it's no go
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
    if (!view_objs)
	return;
    for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
	if (BU_STR_EQUAL(bu_vls_cstr(&s->s_uuid), vname)) {
	    colliding = true;
	}
    }
    if (colliding) {
	bu_free(vname, "name cpy");
	return;
    }

    // See if we've got a valid dp name
    if (!vpoly_name->text().length()) {
	bu_free(vname, "name cpy");
	return;
    }
    char *sname = bu_strdup(vpoly_name->text().toLocal8Bit().data());
    struct bv_scene_obj *src_obj = NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	struct bv_scene_obj *cobj = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
	if (BU_STR_EQUAL(bu_vls_cstr(&cobj->s_uuid), sname)) {
	    src_obj = cobj;
	}
    }
    bu_free(sname, "name cpy");
    if (!src_obj) {
	bu_free(vname, "name cpy");
	return;
    }

    // Names are valid, src_obj is ready - do the copy
    p = bg_dup_view_polygon(vname, src_obj);
    bu_free(vname, "name cpy");
    if (!p) {
	return;
    }
    p->s_v = gedp->ged_gvp;

    // Done processing view object - increment name
    poly_cnt++;
    ps->view_name->clear();
    struct bu_vls pname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&pname, "polygon_%09d", poly_cnt);
    ps->view_name->setPlaceholderText(QString(bu_vls_cstr(&pname)));
    bu_vls_free(&pname);

    do_bool = false;
    p = NULL;
    emit view_updated(QTCAD_VIEW_REFRESH);
}

void
QPolyCreate::do_import_sketch()
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    // Check if we have a name collision - if we do, it's no go
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
    if (!view_objs)
	return;
    for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
	if (BU_STR_EQUAL(bu_vls_cstr(&s->s_uuid), vname)) {
	    colliding = true;
	}
    }
    if (colliding) {
	bu_free(vname, "vname");
	return;
    }

    // See if we've got a valid dp name
    if (!import_name->text().length()) {
	bu_free(vname, "name cpy");
	return;
    }
    char *sname = bu_strdup(import_name->text().toLocal8Bit().data());
    struct directory *dp = db_lookup(gedp->dbip, sname, LOOKUP_QUIET);
    bu_free(sname, "name cpy");
    if (dp == RT_DIR_NULL) {
	bu_free(vname, "name cpy");
	return;
    }

    // Names are valid, dp is ready - try the sketch import
    p = db_sketch_to_scene_obj(vname, gedp->dbip, dp, gedp->ged_gvp, BV_VIEW_OBJS);
    bu_free(vname, "name cpy");
    if (!p) {
	return;
    }
    p->s_v = gedp->ged_gvp;

    // Done processing view object - increment name
    poly_cnt++;
    ps->view_name->clear();
    struct bu_vls pname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&pname, "polygon_%09d", poly_cnt);
    ps->view_name->setPlaceholderText(QString(bu_vls_cstr(&pname)));
    bu_vls_free(&pname);

    do_bool = false;
    p = NULL;
    emit view_updated(QTCAD_VIEW_REFRESH);
}

void
QPolyCreate::sketch_sync_bool(bool)
{
    sketch_sync();
}

void
QPolyCreate::sketch_sync_str(const QString &)
{
    sketch_sync();
}

void
QPolyCreate::sketch_sync()
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
		bu_free(sname, "sname");
		sname = bu_strdup(ps->sketch_name->placeholderText().toLocal8Bit().data());
	    }
	} else {
	    bu_free(sname, "sname");
	    sname = bu_strdup(ps->sketch_name->text().toLocal8Bit().data());
	}

	if (sname) {
	    if (db_lookup(gedp->dbip, sname, LOOKUP_QUIET) != RT_DIR_NULL) {
		ps->sketch_name->setStyleSheet("color: rgb(255,0,0)");
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
QPolyCreate::view_sync_str(const QString &)
{
    view_sync();
}

void
QPolyCreate::view_sync()
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
    if (!view_objs)
	return;
    for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
	if (BU_STR_EQUAL(bu_vls_cstr(&s->s_uuid), vname)) {
	    colliding = true;
	}
    }
    bu_free(vname, "vname");
    if (colliding) {
	ps->view_name->setStyleSheet("color: rgb(255,0,0)");
    } else {
	ps->view_name->setStyleSheet("");
    }
}

void
QPolyCreate::toplevel_config(bool)
{
    // Initialize
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    if (p) {
	finalize(true);
    }

    bool draw_change = false;

    // This function is called when a top level mode change was initiated
    // by a selection button.  Clear any selected points being displayed.
    if (gedp) {
	struct bu_ptbl *view_objs = bv_view_objs(gedp->ged_gvp, BV_VIEW_OBJS);
	if (!view_objs)
	    return;
	for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
	    if (s->s_type_flags & BV_POLYGONS) {
		// clear any selected points in non-current polygons
		struct bv_polygon *ip = (struct bv_polygon *)s->s_i_data;
		if (ip->curr_point_i != -1) {
		    draw_change = true;
		    ip->curr_point_i = -1;
		    ip->curr_contour_i = 0;
		    bv_update_polygon(s, s->s_v, BV_POLYGON_UPDATE_PROPS_ONLY);
		}
	    }
	}
    }

    if (draw_change && gedp)
	emit view_updated(QTCAD_VIEW_REFRESH);
}

void
QPolyCreate::propagate_update(int)
{
    emit view_updated(QTCAD_VIEW_REFRESH);
}

bool
QPolyCreate::eventFilter(QObject *, QEvent *e)
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return false;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return false;
    if (!gedp->ged_gvp)
	return false;

    cf = pcf;

    // If we're mid-creation (i.e. p != NULL) we need to keep processing the
    // polygon from the last event - otherwise, start fresh with p == NULL
    cf->wp = p;
    cf->v = (p) ? p->s_v : gedp->ged_gvp;

    // Connect whatever the current filter is to pass on updating signals from
    // the libqtcad logic.  (For the moment we've only got the one filter, but
    // defining the connect/disconnect pattern this way to be flexible if we
    // need to shift filters at some point in the future.  Polygon modding, for
    // example, has multiple filters depending on the activity and will need
    // this...)
    QObject::connect(cf, &QPolyFilter::view_updated, this, &QPolyCreate::propagate_update);

    //  If we're continuing to edit the existing polygon, the options are
    //  whatever is already established - otherwise, grab from the widget
    //  settings
    if (p) {
	struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
	cf->ptype = ip->type;
    } else {
	if (ellipse_mode->isChecked()) {
	    cf->ptype = BV_POLYGON_ELLIPSE;
	}
	if (square_mode->isChecked()) {
	    cf->ptype = BV_POLYGON_SQUARE;
	}
	if (rectangle_mode->isChecked()) {
	    cf->ptype = BV_POLYGON_RECTANGLE;
	}
	if (general_mode->isChecked()) {
	    cf->ptype = BV_POLYGON_GENERAL;
	}

	cf->fill_poly = (ps->fill_poly->isChecked()) ? true : false;
	cf->fill_slope_x = (fastf_t)(ps->fill_slope_x->text().toDouble());
	cf->fill_slope_y = (fastf_t)(ps->fill_slope_y->text().toDouble());
	cf->fill_density = (fastf_t)(ps->fill_density->text().toDouble());
    	BU_COLOR_CPY(&cf->fill_color, &ps->fill_color->bc);
	BU_COLOR_CPY(&cf->edge_color, &ps->edge_color->bc);
    }

    bool ret = cf->eventFilter(NULL, e);

    // Retrieve the scene object from the libqtcad data container
    p = cf->wp;

    if (cf->ptype == BV_POLYGON_GENERAL) {
	close_general_poly->setEnabled(true);
	close_general_poly->blockSignals(true);
	close_general_poly->setChecked(false);
	close_general_poly->blockSignals(false);
    } else {
	close_general_poly->setEnabled(false);
    }

    // Because the active filter may change, we only maintain the
    // signal connection for the duration of the event
    QObject::disconnect(cf, &QPolyFilter::view_updated, this, &QPolyCreate::propagate_update);


    // TODO - also handle signal/slot for finalize here...

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

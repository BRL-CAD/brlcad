/*                  Q P O L Y C R E A T E . C P P
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
/** @file QPolyCreate.cpp
 *
 */

#include <QLabel>
#include <QLineEdit>
#include <QButtonGroup>
#include <QGroupBox>
#include "../../app.h"
#include "QPolyCreate.h"

#define FREE_BV_SCENE_OBJ(p, fp) { \
    BU_LIST_APPEND(fp, &((p)->l)); }

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
}

QPolyCreate::~QPolyCreate()
{
}

void
QPolyCreate::finalize(bool)
{
    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!p || !gedp)
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
	    bu_ptbl_rm(gedp->ged_gvp->gv_view_objs, (long *)p);
	    FREE_BV_SCENE_OBJ(p, &gedp->free_scene_obj->l);
	    do_bool = false;
	    p = NULL;
	    emit view_updated(&gedp->ged_gvp);
	    return;
	}

	ip->polygon.contour[0].open = 0;
	bv_update_polygon(p, BV_POLYGON_UPDATE_DEFAULT);
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
    if (do_bool) {
	pcnt = bv_polygon_csg(gedp->ged_gvp->gv_view_objs, p, op, 1);
    }
    if (pcnt || op != bg_Union) {
	bg_polygon_free(&ip->polygon);
	BU_PUT(ip, struct bv_polygon);
	bu_ptbl_rm(gedp->ged_gvp->gv_view_objs, (long *)p);
	FREE_BV_SCENE_OBJ(p, &gedp->free_scene_obj->l);
    } else {

	// Check if we have a name collision - if we do, it's no go
	const char *vname = NULL;
	if (ps->view_name->placeholderText().length()) {
	    vname = ps->view_name->placeholderText().toLocal8Bit().data();
	}
	if (ps->view_name->text().length()) {
	    vname = ps->view_name->text().toLocal8Bit().data();
	}
	bool colliding = false;
	for (size_t i = 0; i < BU_PTBL_LEN(gedp->ged_gvp->gv_view_objs); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(gedp->ged_gvp->gv_view_objs, i);
	    if (BU_STR_EQUAL(bu_vls_cstr(&s->s_uuid), vname)) {
		colliding = true;
	    }
	}
	if (colliding) {
	    bg_polygon_free(&ip->polygon);
	    BU_PUT(ip, struct bv_polygon);
	    bu_ptbl_rm(gedp->ged_gvp->gv_view_objs, (long *)p);
	    FREE_BV_SCENE_OBJ(p, &gedp->free_scene_obj->l);
	    do_bool = false;
	    p = NULL;
	    emit view_updated(&gedp->ged_gvp);
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

	// If we're also writing this out as a sketch, take care of that.
	if (ps->sketch_sync->isChecked()) {
	    const char *sname = NULL;
	    if (ps->sketch_name->placeholderText().length()) {
		sname = ps->sketch_name->placeholderText().toLocal8Bit().data();
	    }
	    if (ps->sketch_name->text().length()) {
		sname = ps->sketch_name->text().toLocal8Bit().data();
	    }
	    char *sk_name = bu_strdup(sname);
	    if (sname && db_lookup(gedp->ged_wdbp->dbip, sname, LOOKUP_QUIET) == RT_DIR_NULL) {
		ip->u_data = (void *)db_scene_obj_to_sketch(gedp->ged_wdbp->dbip, sk_name, p);
		emit db_updated();
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
    emit view_updated(&gedp->ged_gvp);
}

void
QPolyCreate::do_vpoly_copy()
{
    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!gedp)
	return;

    // Check if we have a name collision - if we do, it's no go
    char *vname = NULL;
    if (ps->view_name->placeholderText().length()) {
	vname = ps->view_name->placeholderText().toLocal8Bit().data();
    }
    if (ps->view_name->text().length()) {
	vname = ps->view_name->text().toLocal8Bit().data();
    }
    bool colliding = false;
    for (size_t i = 0; i < BU_PTBL_LEN(gedp->ged_gvp->gv_view_objs); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(gedp->ged_gvp->gv_view_objs, i);
	if (BU_STR_EQUAL(bu_vls_cstr(&s->s_uuid), vname)) {
	    colliding = true;
	}
    }
    if (colliding) {
	bu_free(vname, "name cpy");
	return;
    }

    // Stash a copy of the name immediately for use later - a data pointer to
    // the QString isn't stable.
    char *nname = bu_strdup(vname);

    // See if we've got a valid dp name
    if (!vpoly_name->text().length()) {
	bu_free(nname, "name cpy");
	return;
    }
    char *sname = bu_strdup(vpoly_name->text().toLocal8Bit().data());
    struct bv_scene_obj *src_obj = NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(gedp->ged_gvp->gv_view_objs); i++) {
	struct bv_scene_obj *cobj = (struct bv_scene_obj *)BU_PTBL_GET(gedp->ged_gvp->gv_view_objs, i);
	if (BU_STR_EQUAL(bu_vls_cstr(&cobj->s_uuid), sname)) {
	    src_obj = cobj;
	}
    }
    bu_free(sname, "name cpy");
    if (!src_obj) {
	bu_free(nname, "name cpy");
	return;
    }

    // Names are valid, src_obj is ready - do the copy
    p = bg_dup_view_polygon(nname, src_obj);
    bu_free(nname, "name cpy");
    if (!p) {
	return;
    }
    p->s_v = gedp->ged_gvp;

    // Let the view know the polygon is there
    bu_ptbl_ins(gedp->ged_gvp->gv_view_objs, (long *)p);

    // Done processing view object - increment name
    poly_cnt++;
    ps->view_name->clear();
    struct bu_vls pname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&pname, "polygon_%09d", poly_cnt);
    ps->view_name->setPlaceholderText(QString(bu_vls_cstr(&pname)));
    bu_vls_free(&pname);

    do_bool = false;
    p = NULL;
    emit view_updated(&gedp->ged_gvp);
}

void
QPolyCreate::do_import_sketch()
{
    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!gedp)
	return;

    // Check if we have a name collision - if we do, it's no go
    const char *vname = NULL;
    if (ps->view_name->placeholderText().length()) {
	vname = ps->view_name->placeholderText().toLocal8Bit().data();
    }
    if (ps->view_name->text().length()) {
	vname = ps->view_name->text().toLocal8Bit().data();
    }
    bool colliding = false;
    for (size_t i = 0; i < BU_PTBL_LEN(gedp->ged_gvp->gv_view_objs); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(gedp->ged_gvp->gv_view_objs, i);
	if (BU_STR_EQUAL(bu_vls_cstr(&s->s_uuid), vname)) {
	    colliding = true;
	}
    }
    if (colliding) {
	return;
    }

    // Stash a copy of the name immediately for use later - a data pointer to
    // the QString isn't stable.
    char *nname = bu_strdup(vname);

    // See if we've got a valid dp name
    if (!import_name->text().length()) {
	bu_free(nname, "name cpy");
	return;
    }
    char *sname = bu_strdup(import_name->text().toLocal8Bit().data());
    struct directory *dp = db_lookup(gedp->ged_wdbp->dbip, sname, LOOKUP_QUIET);
    bu_free(sname, "name cpy");
    if (dp == RT_DIR_NULL) {
	bu_free(nname, "name cpy");
	return;
    }

    // Names are valid, dp is ready - try the sketch import
    p = db_sketch_to_scene_obj(nname, gedp->ged_wdbp->dbip, dp, gedp->ged_gvp, gedp->free_scene_obj);
    bu_free(nname, "name cpy");
    if (!p) {
	return;
    }
    p->s_v = gedp->ged_gvp;

    // Let the view know the polygon is there
    bu_ptbl_ins(gedp->ged_gvp->gv_view_objs, (long *)p);

    // Done processing view object - increment name
    poly_cnt++;
    ps->view_name->clear();
    struct bu_vls pname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&pname, "polygon_%09d", poly_cnt);
    ps->view_name->setPlaceholderText(QString(bu_vls_cstr(&pname)));
    bu_vls_free(&pname);

    do_bool = false;
    p = NULL;
    emit view_updated(&gedp->ged_gvp);
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
    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!gedp) {
	ps->sketch_name->setPlaceholderText("No .g file open");
	ps->sketch_name->setStyleSheet("color: rgba(200,200,200)");
	ps->sketch_name->setEnabled(false);
	return;
    }

    if (ps->sketch_sync->isChecked()) {
	const char *sname = NULL;
	if (!ps->sketch_name->placeholderText().length()) {
	    if (ps->view_name->placeholderText().length()) {
		ps->sketch_name->setPlaceholderText(ps->view_name->placeholderText());
		sname = ps->sketch_name->placeholderText().toLocal8Bit().data();
	    }
	} else {
	    sname = ps->sketch_name->placeholderText().toLocal8Bit().data();
	}
	if (!ps->sketch_name->text().length()) {
	    if (ps->view_name->text().length()) {
		ps->sketch_name->setPlaceholderText(ps->view_name->text());
		sname = ps->sketch_name->placeholderText().toLocal8Bit().data();
	    }
	} else {
	    sname = ps->sketch_name->text().toLocal8Bit().data();
	}

	if (sname) {
	    if (db_lookup(gedp->ged_wdbp->dbip, sname, LOOKUP_QUIET) != RT_DIR_NULL) {
		ps->sketch_name->setStyleSheet("color: rgba(255,0,0)");
	    } else {
		ps->sketch_name->setStyleSheet("");
	    }
	} else {
	    ps->sketch_name->setStyleSheet("");
	}
	ps->sketch_name->setEnabled(true);
    } else {
	ps->sketch_name->setPlaceholderText("Enable to save sketch");
	ps->sketch_name->setStyleSheet("color: rgba(200,200,200)");
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
    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!gedp) {
	return;
    }

    const char *vname = NULL;
    if (ps->view_name->placeholderText().length()) {
	vname = ps->view_name->placeholderText().toLocal8Bit().data();
    }
    if (ps->view_name->text().length()) {
	vname = ps->view_name->text().toLocal8Bit().data();
    }
    bool colliding = false;
    for (size_t i = 0; i < BU_PTBL_LEN(gedp->ged_gvp->gv_view_objs); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(gedp->ged_gvp->gv_view_objs, i);
	if (BU_STR_EQUAL(bu_vls_cstr(&s->s_uuid), vname)) {
	    colliding = true;
	}
    }
    if (colliding) {
	ps->view_name->setStyleSheet("color: rgba(255,0,0)");
    } else {
	ps->view_name->setStyleSheet("");
    }
}

void
QPolyCreate::toplevel_config(bool)
{
    // Initialize
    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!gedp)
	return;

    if (p) {
	finalize(true);
    }

    bool draw_change = false;

    // This function is called when a top level mode change was initiated
    // by a selection button.  Clear any selected points being displayed.
    if (gedp) {
	for (size_t i = 0; i < BU_PTBL_LEN(gedp->ged_gvp->gv_view_objs); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(gedp->ged_gvp->gv_view_objs, i);
	    if (s->s_type_flags & BV_POLYGONS) {
		// clear any selected points in non-current polygons
		struct bv_polygon *ip = (struct bv_polygon *)s->s_i_data;
		if (ip->curr_point_i != -1) {
		    draw_change = true;
		    ip->curr_point_i = -1;
		    ip->curr_contour_i = 0;
		    bv_update_polygon(s, BV_POLYGON_UPDATE_PROPS_ONLY);
		}
	    }
	}
    }

    if (draw_change && gedp)
	emit view_updated(&gedp->ged_gvp);
}

bool
QPolyCreate::eventFilter(QObject *, QEvent *e)
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

    printf("polygon add\n");

    do_bool = false;
    if (csg_modes->currentText() != "None") {
	do_bool = true;
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
	    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;

	    if (ptype == BV_POLYGON_GENERAL) {

		// For general polygons, we need to identify the active contour
		// for update operations to work.
		//
		// At some point we'll need to add support for adding and removing
		// contours...
		ip->curr_contour_i = 0;

		close_general_poly->setEnabled(true);
		close_general_poly->blockSignals(true);
		close_general_poly->setChecked(false);
		close_general_poly->blockSignals(false);
	    } else {
		close_general_poly->setEnabled(false);
	    }

	    // Get edge color
	    bu_color_to_rgb_chars(&ps->edge_color->bc, p->s_color);

	    // fill color
	    BU_COLOR_CPY(&ip->fill_color, &ps->fill_color->bc);

	    // fill settings
	    vect2d_t vdir = V2INIT_ZERO;
	    vdir[0] = (fastf_t)(ps->fill_slope_x->text().toDouble());
	    vdir[1] = (fastf_t)(ps->fill_slope_y->text().toDouble());
	    V2MOVE(ip->fill_dir, vdir);
	    ip->fill_delta = (fastf_t)(ps->fill_density->text().toDouble());

	    // Set fill
	    if (ps->fill_poly->isChecked()) {
		ip->fill_flag = 1;
		bv_update_polygon(p, BV_POLYGON_UPDATE_PROPS_ONLY);
	    }

	    // Let the view know the polygon is there
	    bu_ptbl_ins(gedp->ged_gvp->gv_view_objs, (long *)p);

	    // Name appropriately
	    bu_vls_init(&p->s_uuid);

	    // It doesn't get a "proper" name until its finalized
	    bu_vls_printf(&p->s_uuid, "_tmp_view_polygon");

	    emit view_updated(&gedp->ged_gvp);
	    return true;
	}

	// If we're creating a general polygon, we're appending points after
	// the initial creation
	struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
	if (ip->type == BV_POLYGON_GENERAL) {
	    p->s_v->gv_mouse_x = m_e->x();
	    p->s_v->gv_mouse_y = m_e->y();
	    bv_update_polygon(p, BV_POLYGON_UPDATE_PT_APPEND);

	    emit view_updated(&gedp->ged_gvp);
	    return true;
	}

	// When we're dealing with polygons stray left clicks shouldn't zoom - just
	// consume them if we're not using them above.
	return true;
    }

    if (m_e->type() == QEvent::MouseButtonPress && m_e->buttons().testFlag(Qt::RightButton)) {
	// No-op if no current polygon is defined
	if (!p)
	    return true;

	// Non-general polygon creation doesn't use right click.
	struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
	if (ip->type != BV_POLYGON_GENERAL) {
	    return true;
	}

	// General polygon, have right click - finish up.
	finalize(true);
	return true;
    }

    if (m_e->type() == QEvent::MouseButtonPress) {
	// We also don't want other stray mouse clicks to do something surprising
	return true;
    }

    // During initial add/creation of non-general polygons, mouse movement
    // adjusts the shape
    if (m_e->type() == QEvent::MouseMove) {
	// No-op if no current polygon is defined
	if (!p)
	    return true;

	// General polygon creation doesn't use mouse movement.
	struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
	if (ip->type == BV_POLYGON_GENERAL) {
	    return true;
	}

	// For every other polygon type, call the libbv update routine
	// with the view's x,y coordinates
	if (m_e->buttons().testFlag(Qt::LeftButton) && m_e->modifiers() == Qt::NoModifier) {
	    bv_update_polygon(p, BV_POLYGON_UPDATE_DEFAULT);
	    emit view_updated(&gedp->ged_gvp);
	    return true;
	}
    }

    if (m_e->type() == QEvent::MouseButtonRelease) {

	// No-op if no current polygon is defined
	if (!p)
	    return true;

	struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;
	if (ip->type == BV_POLYGON_GENERAL) {
	    // General polygons are finalized by an explicit close
	    // (either right mouse click or the close checkbox)
	    return true;
	}

	// For all non-general polygons, mouse release is the signal
	// to finish up.
	finalize(true);

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

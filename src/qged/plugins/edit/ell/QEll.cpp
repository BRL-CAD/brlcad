/*                         Q E L L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022-2026 United States Government as represented by
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
/** @file QEll.cpp
 *
 */

#include "common.h"
#include <string.h>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QButtonGroup>
#include <QGroupBox>
#include "ged.h"
#include "rt/db_io.h"
#include "rt/directory.h"
#include "bsg/feature.h"
#include "bsg/hud.h"
#include "bsg/overlay.h"
#include "qtcad/QgGedEventBatch.h"
#include "qtcad/QgPluginContext.h"
#include "qtcad/QgSignalFlags.h"
#include "ged/bsg_ged_draw.h"
#include "../qged_edit_preview_util.h"
#include "QEll.h"

QEll::QEll()
    : QWidget()
{
    // TODO - in an ideal world the "default" values would be set
    // and updated in response to view changes (if widget is continually
    // visible) or when it becomes visible...
    ell.magic = RT_ELL_INTERNAL_MAGIC;
    VSET(ell.v, 0, 0, 0);
    VSET(ell.a, 100, 0, 0);
    VSET(ell.b, 0, 200, 0);
    VSET(ell.c, 0, 0, 300);


    QVBoxLayout *l = new QVBoxLayout;

    QLabel *ell_name_label = new QLabel("Object name:");
    l->addWidget(ell_name_label);
    ell_name = new QLineEdit();
    l->addWidget(ell_name);

    QGroupBox *abox = new QGroupBox("Elements");
    QVBoxLayout *abl = new QVBoxLayout;
    abl->setAlignment(Qt::AlignTop);
    O_pnt = new QCheckBox("O:");
    abl->addWidget(O_pnt);
    A_axis = new QCheckBox("A:");
    abl->addWidget(A_axis);
    B_axis = new QCheckBox("B:");
    abl->addWidget(B_axis);
    C_axis = new QCheckBox("C:");
    abl->addWidget(C_axis);
    abox->setLayout(abl);
    l->addWidget(abox);

    QGroupBox *ac_box = new QGroupBox("Actions");
    QVBoxLayout *acl = new QVBoxLayout;
    write_edit = new QPushButton("Apply");
    acl->addWidget(write_edit);
    make_sph = new QPushButton("Make sph");
    acl->addWidget(make_sph);
    reset_values = new QPushButton("Reset");
    acl->addWidget(reset_values);
    ac_box->setLayout(acl);
    l->addWidget(ac_box);


    l->setAlignment(Qt::AlignTop);
    this->setLayout(l);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);


    QObject::connect(ell_name, &QLineEdit::textEdited, this, &QEll::update_viewobj_name);
}

QEll::~QEll()
{
    struct bsg_view *v = getView();
    if (v) {
	bsg_feature_remove(v, "_ell_edit");
	bsg_feature_remove(v, "_ell_edit_labels");
    }
    p = BSG_FEATURE_REF_NULL_INIT;
    bu_vls_free(&oname);
}

struct ged *
QEll::getGed() const
{
    return m_ctx ? m_ctx->getGed() : nullptr;
}

struct bsg_view *
QEll::getView() const
{
    return m_ctx ? m_ctx->getView() : nullptr;
}

void
QEll::read_from_db()
{
    struct ged *gedp = getGed();
    if (!gedp)
	return;
    struct db_i *dbip = gedp->dbip;
    if (!dbip)
	return;

    if (!dp || dp->d_minor_type != DB5_MINORTYPE_BRLCAD_ELL) {
	return;
    }

    struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0)
	return;
    struct rt_ell_internal *ellp = (struct rt_ell_internal *)intern.idb_ptr;
    RT_ELL_CK_MAGIC(ellp);
    VMOVE(ell.v, ellp->v);
    VMOVE(ell.a, ellp->a);
    VMOVE(ell.b, ellp->b);
    VMOVE(ell.c, ellp->c);
    rt_db_free_internal(&intern);

    // We have pulled new data from disk - let the wireframe know
    update_obj_wireframe();
}

void
QEll::write_to_db()
{
    if (!bu_vls_strlen(&oname))
	return;
    struct ged *gedp = getGed();
    if (!gedp)
	return;
    struct db_i *dbip = gedp->dbip;
    if (!dbip)
	return;

    struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_ELL;
    intern.idb_ptr = &ell;
    intern.idb_meth = &OBJ[intern.idb_type];

    dp = db_lookup(dbip, bu_vls_cstr(&oname), LOOKUP_QUIET);

    {
	QgGedEventBatch event_batch(gedp);

	if (dp == RT_DIR_NULL)
	    dp = db_diradd(dbip, bu_vls_cstr(&oname), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);

	if (dp == RT_DIR_NULL) {
	    rt_db_free_internal(&intern);
	    return;
	}

	if (rt_db_put_internal(dp, dbip, &intern) < 0) {
	    rt_db_free_internal(&intern);
	    return;
	}
    }

    rt_db_free_internal(&intern);

    emit view_updated(QG_VIEW_DB);
}

void
QEll::update_obj_wireframe()
{
    struct ged *gedp = getGed();
    if (!gedp)
	return;
    struct bsg_view *v = getView();
    if (!v)
	return;

    // Resolve the edit object fresh in case it was removed externally
    // (e.g. by a clear/zap command).
    p = bsg_feature_find(v, "_ell_edit");
    if (bsg_feature_ref_is_null(p)) {
	p = bsg_feature_create_overlay(v, "_ell_edit", 1/*local*/);
	if (!bsg_feature_ref_is_null(p))
	    bsg_feature_overlay_register_owner(p, this,
		    BSG_OVERLAY_ROLE_MODEL,
		    BSG_OVERLAY_CLASS_EDIT_HANDLE,
		    BSG_OVERLAY_LC_PER_TOOL,
		    BSG_OVERLAY_ORDER_POST_TRANSPARENT,
		    NULL, 0);
    }
    if (bsg_feature_ref_is_null(p))
	return;

    // No active db or object name means there is nothing to edit - make sure
    // the edit wireframe is hidden.
    if (!gedp->dbip || !bu_vls_strlen(&oname)) {
	qged_edit_feature_clear_geometry(p);
	bsg_feature_set_visible(p, 0);
	bsg_feature_remove(v, "_ell_edit_labels");
	return;
    }

    // Refresh the directory pointer from the current object name.  This avoids
    // stale pointers if scene/database content changed.
    dp = db_lookup(gedp->dbip, bu_vls_cstr(&oname), LOOKUP_QUIET);
    if (!dp || dp->d_minor_type != DB5_MINORTYPE_BRLCAD_ELL) {
	qged_edit_feature_clear_geometry(p);
	bsg_feature_set_visible(p, 0);
	bsg_feature_remove(v, "_ell_edit_labels");
	return;
    }

    qged_edit_feature_clear_geometry(p);
    bsg_feature_set_view(p, v);

    // Set up the rt_db_internal and trigger the plotting routine with the
    // current ell parameters
    struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_ELL;
    intern.idb_ptr = &ell;
    intern.idb_meth = &OBJ[intern.idb_type];
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (!wdbp)
	return;
    struct bn_tol *tol = &wdbp->wdb_tol;
    qged_edit_feature_replace_ell_wireframe(p,
	    BSG_FEATURE_TRANSIENT_PREVIEW,
	    (const struct rt_ell_internal *)intern.idb_ptr);

    // At least for now, mimic the MGED behavior and make editing wireframes white
    const char *wcolor = "255/255/255";
    const char *av[2] = {wcolor, NULL};
    struct bu_color cval;
    bu_opt_color(NULL, 1, (const char **)&av[0], (void *)&cval);
    unsigned char rgb[3] = {0, 0, 0};
    bu_color_to_rgb_chars(&cval, rgb);
    bsg_feature_set_color(p, rgb[0], rgb[1], rgb[2]);

    // When editing, we show the labels (if any)
    struct rt_point_labels pl[8+1];
    int lcnt = 0;
    mat_t idn_mat;
    MAT_IDN(idn_mat);
    if (intern.idb_meth->ft_labels)
	lcnt = intern.idb_meth->ft_labels(pl, 8, idn_mat, &intern, tol);

    bsg_feature_ref labels_ref = bsg_feature_find(v, "_ell_edit_labels");
    if (bsg_feature_ref_is_null(labels_ref))
	labels_ref = bsg_feature_create_label(v, "_ell_edit_labels", 1/*local*/);
    if (!bsg_feature_ref_is_null(labels_ref)) {
	bsg_feature_overlay_register_owner(labels_ref, this,
		BSG_OVERLAY_ROLE_MODEL,
		BSG_OVERLAY_CLASS_EDIT_HANDLE,
		BSG_OVERLAY_LC_PER_TOOL,
		BSG_OVERLAY_ORDER_POST_TRANSPARENT,
		NULL, 1);
	struct bsg_feature_label_data *labels = NULL;
	if (lcnt > 0)
	    labels = (struct bsg_feature_label_data *)bu_calloc((size_t)lcnt,
		    sizeof(struct bsg_feature_label_data), "ell edit labels");
	for (int i = 0; i < lcnt; i++) {
	    labels[i].text = pl[i].str;
	    VMOVE(labels[i].point, pl[i].pt);
	    labels[i].color_valid = 1;
	    VSET(labels[i].color, 255, 255, 0);
	    labels[i].anchor = BSG_ANCHOR_AUTO;
	}
	bsg_feature_labels_replace(labels_ref, labels, (size_t)lcnt);
	bsg_feature_set_visible(labels_ref, lcnt > 0 ? 1 : 0);
	if (labels)
	    bu_free(labels, "ell edit labels");
    }

    bsg_feature_set_visible(p, 1);
    // TODO - we should be able to set UP or DOWN on the various labels
    // when their respective controls are enabled/disabled...

    emit view_updated(QG_VIEW_REFRESH);
}

void
QEll::update_viewobj_name(const QString &)
{
    struct ged *gedp = getGed();
    if (!gedp || !gedp->dbip)
	return;
    struct bsg_view *v = getView();
    if (!v)
	return;

    // Resolve/create the edit view feature.  Don't trust cached pointers here
    // since clear/zap may have removed it.
    p = bsg_feature_find(v, "_ell_edit");
    if (bsg_feature_ref_is_null(p)) {
	p = bsg_feature_create_overlay(v, "_ell_edit", 1/*local*/);
	if (!bsg_feature_ref_is_null(p))
	    bsg_feature_overlay_register_owner(p, this,
		    BSG_OVERLAY_ROLE_MODEL,
		    BSG_OVERLAY_CLASS_EDIT_HANDLE,
		    BSG_OVERLAY_LC_PER_TOOL,
		    BSG_OVERLAY_ORDER_POST_TRANSPARENT,
		    NULL, 0);
    }
    if (bsg_feature_ref_is_null(p))
	return;

    // Make sure the view feature names match whatever the dialog says
    // is the current (proposed) name for the written object
    bu_vls_trunc(&oname, 0);
    if (ell_name->placeholderText().length())
	bu_vls_sprintf(&oname, "%s", ell_name->placeholderText().toLocal8Bit().data());
    if (ell_name->text().length())
	bu_vls_sprintf(&oname, "%s", ell_name->text().toLocal8Bit().data());
    if (!bu_vls_strlen(&oname))
	return;

    // Update the directory pointer to reflect the name.  If there is a change,
    // and that change points us to a new object, we need to read the info from
    // that object
    struct directory *ndp = db_lookup(gedp->dbip, bu_vls_cstr(&oname), LOOKUP_QUIET);
    if (ndp != dp) {
	dp = ndp;
	if (dp) {
	    ged_draw_highlight_shape_ref_by_name(gedp, bu_vls_cstr(&oname));
	    read_from_db();
	} else {
	    ged_draw_set_highlighted_shape_ref(gedp, GED_DRAW_SHAPE_REF_NULL);
	    qged_edit_feature_clear_geometry(p);
	    bsg_feature_set_visible(p, 0);
	    bsg_feature_remove(v, "_ell_edit_labels");
	    emit view_updated(QG_VIEW_REFRESH);
	}
    }
}

bool
QEll::eventFilter(QObject *, QEvent *e)
{
    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease ||   e->type() == QEvent::MouseButtonDblClick || e->type() == QEvent::MouseMove) {
	bu_log("ell mouse event\n");
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

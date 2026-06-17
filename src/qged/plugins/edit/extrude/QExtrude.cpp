/*                    Q E X T R U D E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file QExtrude.cpp
 *
 * Edit-preview extrude-solid editor.
 *
 * The transient preview feature owns typed edit-preview callbacks that rebuild
 * draft geometry and publish edit-preview interaction records.
 */

#include "common.h"
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QGroupBox>
#include <QVBoxLayout>
#include "ged.h"
#include "rt/db_io.h"
#include "rt/directory.h"
#include "bsg/feature.h"
#include "bsg/interaction.h"
#include "bsg/overlay.h"
#include "qtcad/QgGedEventBatch.h"
#include "qtcad/QgPluginContext.h"
#include "qtcad/QgSignalFlags.h"
#include "ged/bsg_ged_draw.h"
#include "../qged_edit_preview_util.h"
#include "QExtrude.h"


/* ---- edit-preview callbacks -------------------------------------------- */

static uint64_t
_extrude_preview_revision(void *preview_ctx)
{
    (void)preview_ctx;
    /* Placeholder — returns 0 until extrude editor tracks its own epoch. */
    return 0;
}

static int
_extrude_preview_update(void *preview_ctx, struct bsg_view *UNUSED(v))
{
    QExtrude *self = (QExtrude *)preview_ctx;
    if (!self)
	return 0;
    /* Invoke via meta-object so the call works whether or not signals are
     * blocked (same pattern used in EditEllTool::refresh). */
    QMetaObject::invokeMethod(self, "update_obj_wireframe", Qt::DirectConnection);
    return 1;
}

static void
_extrude_publish_preview(struct bsg_view *v, bsg_feature_ref feature,
	bsg_edit_preview_op op, const char *source_path)
{
    struct bsg_interaction_record *record =
	bsg_interaction_edit_preview_record(v, feature, op, source_path);
    bsg_interaction_record_free(record);
}

/* ---- QExtrude constructor ----------------------------------------------- */

QExtrude::QExtrude()
    : QWidget()
{
    extr.magic = RT_EXTRUDE_INTERNAL_MAGIC;

    QVBoxLayout *l = new QVBoxLayout;

    QLabel *name_label = new QLabel("Object name:");
    l->addWidget(name_label);
    extrude_name = new QLineEdit();
    l->addWidget(extrude_name);

    QGroupBox *pbox = new QGroupBox("Parameters");
    QVBoxLayout *pbl = new QVBoxLayout;
    pbl->setAlignment(Qt::AlignTop);
    QLabel *h_label = new QLabel("H length:");
    pbl->addWidget(h_label);
    h_len = new QLineEdit();
    pbl->addWidget(h_len);
    pbox->setLayout(pbl);
    l->addWidget(pbox);

    QGroupBox *ac_box = new QGroupBox("Actions");
    QVBoxLayout *acl = new QVBoxLayout;
    write_edit = new QPushButton("Apply");
    acl->addWidget(write_edit);
    reset_values = new QPushButton("Reset");
    acl->addWidget(reset_values);
    ac_box->setLayout(acl);
    l->addWidget(ac_box);

    l->setAlignment(Qt::AlignTop);
    this->setLayout(l);

    QObject::connect(extrude_name, &QLineEdit::textChanged,
		     this, &QExtrude::update_viewobj_name);
    QObject::connect(write_edit, &QPushButton::clicked,
		     this, &QExtrude::write_to_db);
    QObject::connect(reset_values, &QPushButton::clicked,
		     this, &QExtrude::read_from_db);
}

QExtrude::~QExtrude()
{
    struct bsg_view *v = getView();
    if (!bsg_feature_ref_is_null(p) && v) {
	bsg_feature_remove(v, "_extrude_edit");
	p = BSG_FEATURE_REF_NULL_INIT;
    }
    bu_vls_free(&oname);
}


struct ged *
QExtrude::getGed() const
{
    if (!m_ctx)
	return nullptr;
    return m_ctx->getGed();
}

struct bsg_view *
QExtrude::getView() const
{
    if (!m_ctx)
	return nullptr;
    return m_ctx->getView();
}


void
QExtrude::read_from_db()
{
    struct ged *gedp = getGed();
    if (!gedp || !gedp->dbip || !bu_vls_strlen(&oname))
	return;

    struct directory *ldp = db_lookup(gedp->dbip, bu_vls_cstr(&oname), LOOKUP_QUIET);
    if (!ldp || ldp->d_minor_type != DB5_MINORTYPE_BRLCAD_EXTRUDE)
	return;

    struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
    if (rt_db_get_internal(&intern, ldp, gedp->dbip, NULL) < 0)
	return;

    struct rt_extrude_internal *ep = (struct rt_extrude_internal *)intern.idb_ptr;
    RT_EXTRUDE_CK_MAGIC(ep);

    extr = *ep;  /* shallow copy for parameter display */

    rt_db_free_internal(&intern);

    update_obj_wireframe();
    emit view_updated(QG_VIEW_REFRESH);
}


void
QExtrude::write_to_db()
{
    struct ged *gedp = getGed();
    if (!gedp || !gedp->dbip || !bu_vls_strlen(&oname))
	return;

    struct directory *ldp = db_lookup(gedp->dbip, bu_vls_cstr(&oname), LOOKUP_QUIET);
    if (!ldp || ldp->d_minor_type != DB5_MINORTYPE_BRLCAD_EXTRUDE)
	return;

    struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_EXTRUDE;
    intern.idb_ptr = &extr;
    intern.idb_meth = &OBJ[ID_EXTRUDE];

    {
	QgGedEventBatch event_batch(gedp);
	if (rt_db_put_internal(ldp, gedp->dbip, &intern) < 0)
	    return;
    }

    _extrude_publish_preview(getView(), p, BSG_EDIT_PREVIEW_COMMIT,
	    bu_vls_cstr(&oname));
    emit view_updated(QG_VIEW_DB);
}


void
QExtrude::update_obj_wireframe()
{
    struct ged *gedp = getGed();
    if (!gedp)
	return;
    struct bsg_view *v = getView();
    if (!v)
	return;

    p = bsg_feature_find(v, "_extrude_edit");
    if (bsg_feature_ref_is_null(p)) {
	p = bsg_feature_create_overlay(v, "_extrude_edit", 1/*local*/);
	if (!bsg_feature_ref_is_null(p))
	    bsg_feature_overlay_register_owner(p, this,
		    BSG_OVERLAY_ROLE_MODEL,
		    BSG_OVERLAY_CLASS_EDIT_HANDLE,
		    BSG_OVERLAY_LC_PER_TOOL,
		    BSG_OVERLAY_ORDER_POST_TRANSPARENT,
		    NULL, 0);

	if (!bsg_feature_ref_is_null(p)) {
	    struct bsg_edit_preview_ops ops = BSG_EDIT_PREVIEW_OPS_INIT;
	    ops.revision_cb = _extrude_preview_revision;
	    ops.update_cb = _extrude_preview_update;
	    bsg_feature_edit_preview_attach(p, this, NULL, &ops);
	    _extrude_publish_preview(v, p, BSG_EDIT_PREVIEW_BEGIN,
		    bu_vls_cstr(&oname));
	}
    }
    if (bsg_feature_ref_is_null(p))
	return;

    if (!gedp->dbip || !bu_vls_strlen(&oname)) {
	qged_edit_feature_clear_geometry(p);
	bsg_feature_set_visible(p, 0);
	return;
    }

    dp = db_lookup(gedp->dbip, bu_vls_cstr(&oname), LOOKUP_QUIET);
    if (!dp || dp->d_minor_type != DB5_MINORTYPE_BRLCAD_EXTRUDE) {
	qged_edit_feature_clear_geometry(p);
	bsg_feature_set_visible(p, 0);
	return;
    }

    qged_edit_feature_clear_geometry(p);
    bsg_feature_set_view(p, v);
    bsg_feature_set_visible(p, 1);

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (!wdbp)
	return;
    struct bg_tess_tol *ttol = &wdbp->wdb_ttol;
    if (qged_edit_feature_replace_extrude_wireframe(p,
	    BSG_FEATURE_TRANSIENT_PREVIEW, &extr, ttol))
	_extrude_publish_preview(v, p, BSG_EDIT_PREVIEW_UPDATE,
		bu_vls_cstr(&oname));

    const char *wcolor = "255/255/255";
    const char *av[2] = {wcolor, NULL};
    struct bu_color cval;
    bu_opt_color(NULL, 1, (const char **)&av[0], (void *)&cval);
    unsigned char rgb[3] = {0, 0, 0};
    bu_color_to_rgb_chars(&cval, rgb);
    bsg_feature_set_color(p, rgb[0], rgb[1], rgb[2]);

    bsg_feature_edit_preview_touch(p);
}


void
QExtrude::update_viewobj_name(const QString &ostr)
{
    bu_vls_sprintf(&oname, "%s", ostr.toLocal8Bit().data());
    update_obj_wireframe();
    emit view_updated(QG_VIEW_REFRESH);
}


bool
QExtrude::eventFilter(QObject *, QEvent *)
{
    return false;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

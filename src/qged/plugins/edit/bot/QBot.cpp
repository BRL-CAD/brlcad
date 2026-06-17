/*                       Q B O T . C P P
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
/** @file QBot.cpp
 *
 * Edit-preview BOT editor.
 *
 * The transient preview feature owns typed edit-preview callbacks and emits
 * edit-preview interaction records.
 */

#include "common.h"
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include "ged.h"
#include "rt/db_io.h"
#include "rt/directory.h"
#include "bsg/feature.h"
#include "bsg/interaction.h"
#include "bsg/overlay.h"
#include "qtcad/QgPluginContext.h"
#include "qtcad/QgSignalFlags.h"
#include "ged/bsg_ged_draw.h"
#include "../qged_edit_preview_util.h"
#include "QBot.h"


/* ---- edit-preview callbacks -------------------------------------------- */

static uint64_t
_bot_preview_revision(void *UNUSED(preview_ctx))
{
    /* Placeholder — a real implementation would track the edit epoch. */
    return 0;
}

static int
_bot_preview_update(void *preview_ctx, struct bsg_view *UNUSED(v))
{
    QBot *self = (QBot *)preview_ctx;
    if (!self)
	return 0;
    QMetaObject::invokeMethod(self, "update_obj_wireframe", Qt::DirectConnection);
    return 1;
}

/* pick_cb stub: ray-pick for face / vertex selection.
 * Filled in once the BOT editor gains interactive selection logic. */
static int
_bot_preview_pick(void *UNUSED(preview_ctx), struct bsg_view *UNUSED(v),
	       int UNUSED(x), int UNUSED(y), void *UNUSED(pick_out))
{
    /* TODO: intersect sample ray against BOT faces/vertices. */
    return 0;
}

static void
_bot_publish_preview(struct bsg_view *v, bsg_feature_ref feature,
	bsg_edit_preview_op op, const char *source_path)
{
    struct bsg_interaction_record *record =
	bsg_interaction_edit_preview_record(v, feature, op, source_path);
    bsg_interaction_record_free(record);
}


/* ---- QBot constructor --------------------------------------------------- */

QBot::QBot()
    : QWidget()
{
    QVBoxLayout *l = new QVBoxLayout;

    QLabel *name_label = new QLabel("Object name:");
    l->addWidget(name_label);
    bot_name = new QLineEdit();
    l->addWidget(bot_name);

    QGroupBox *mode_box = new QGroupBox("Edit mode");
    QVBoxLayout *mbl = new QVBoxLayout;
    edit_mode = new QComboBox();
    edit_mode->addItem("Vertex");
    edit_mode->addItem("Face");
    edit_mode->addItem("Edge");
    mbl->addWidget(edit_mode);
    mode_box->setLayout(mbl);
    l->addWidget(mode_box);

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

    QObject::connect(bot_name, &QLineEdit::textChanged,
		     this, &QBot::update_viewobj_name);
    QObject::connect(write_edit, &QPushButton::clicked,
		     this, &QBot::write_to_db);
    QObject::connect(reset_values, &QPushButton::clicked,
		     this, &QBot::read_from_db);
}

QBot::~QBot()
{
    struct bsg_view *v = getView();
    if (!bsg_feature_ref_is_null(p) && v) {
	bsg_feature_remove(v, "_bot_edit");
	p = BSG_FEATURE_REF_NULL_INIT;
    }
    bu_vls_free(&oname);
}

struct ged *
QBot::getGed() const
{
    if (!m_ctx)
	return nullptr;
    return m_ctx->getGed();
}

struct bsg_view *
QBot::getView() const
{
    if (!m_ctx)
	return nullptr;
    return m_ctx->getView();
}

void
QBot::read_from_db()
{
    struct ged *gedp = getGed();
    if (!gedp || !gedp->dbip || !bu_vls_strlen(&oname))
	return;

    /* BOT loading is deferred until full edit logic is implemented. */
    update_obj_wireframe();
    emit view_updated(QG_VIEW_REFRESH);
}

void
QBot::write_to_db()
{
    _bot_publish_preview(getView(), p, BSG_EDIT_PREVIEW_COMMIT,
	    bu_vls_cstr(&oname));
    emit view_updated(QG_VIEW_DB);
}

void
QBot::update_obj_wireframe()
{
    struct ged *gedp = getGed();
    if (!gedp)
	return;
    struct bsg_view *v = getView();
    if (!v)
	return;

    p = bsg_feature_find(v, "_bot_edit");
    if (bsg_feature_ref_is_null(p)) {
	p = bsg_feature_create_overlay(v, "_bot_edit", 1/*local*/);
	if (!bsg_feature_ref_is_null(p))
	    bsg_feature_overlay_register_owner(p, this,
		    BSG_OVERLAY_ROLE_MODEL,
		    BSG_OVERLAY_CLASS_EDIT_HANDLE,
		    BSG_OVERLAY_LC_PER_TOOL,
		    BSG_OVERLAY_ORDER_POST_TRANSPARENT,
		    NULL, 0);

	if (!bsg_feature_ref_is_null(p)) {
	    struct bsg_edit_preview_ops ops = BSG_EDIT_PREVIEW_OPS_INIT;
	    ops.revision_cb = _bot_preview_revision;
	    ops.update_cb = _bot_preview_update;
	    ops.pick_cb = _bot_preview_pick;
	    bsg_feature_edit_preview_attach(p, this, NULL, &ops);
	    _bot_publish_preview(v, p, BSG_EDIT_PREVIEW_BEGIN,
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
    if (!dp || dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	qged_edit_feature_clear_geometry(p);
	bsg_feature_set_visible(p, 0);
	return;
    }

    qged_edit_feature_clear_geometry(p);
    bsg_feature_set_view(p, v);
    bsg_feature_set_visible(p, 1);

    /* Load and plot the BOT geometry. */
    struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
    if (rt_db_get_internal(&intern, dp, gedp->dbip, NULL) < 0)
	return;
    if (intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	rt_db_free_internal(&intern);
	return;
    }

    const struct rt_bot_internal *bot_ip =
	(const struct rt_bot_internal *)intern.idb_ptr;
    if (qged_edit_feature_replace_bot_face_lines(p,
	    BSG_FEATURE_TRANSIENT_PREVIEW, bot_ip)) {
	_bot_publish_preview(v, p, BSG_EDIT_PREVIEW_UPDATE,
		bu_vls_cstr(&oname));
    }
    rt_db_free_internal(&intern);

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
QBot::update_viewobj_name(const QString &ostr)
{
    bu_vls_sprintf(&oname, "%s", ostr.toLocal8Bit().data());
    update_obj_wireframe();
    emit view_updated(QG_VIEW_REFRESH);
}

bool
QBot::eventFilter(QObject *, QEvent *)
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

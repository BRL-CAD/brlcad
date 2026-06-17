/*                          Q B O T . H
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
/** @file QBot.h
 *
 * stub bag-of-triangles (BOT) editor that demonstrates typed edit-preview
 * callbacks for mesh-editing plugins.
 *
 * Workflow
 * --------
 * 1. An overlay feature (_bot_edit) is created and typed edit-preview
 *    callbacks are attached when BOT editing begins.
 * 2. The update_cb bumps the revision whenever vertex/face data changes so
 *    downstream renderers can detect edits without polling s_changed.
 * 3. The pick_cb stub is the first extension point for face/vertex selection.
 * 4. Preview teardown is owned by the feature.
 *
 * Future work
 * -----------
 * - Wire pick_cb to face-selection logic (ray/BV intersection).
 * - Wire snap_cb to vertex snapping during drag.
 * - Add a BSG_OVERLAY_CLASS_EDIT_HANDLE child node per selected vertex.
 */

#ifndef QBOT_H
#define QBOT_H

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include "bsg/feature.h"
#include "raytrace.h"
#include "qtcad/QgTypes.h"

class QgPluginContext;

class QBot : public QWidget
{
    Q_OBJECT

    public:
	QBot();
	~QBot();

	void setContext(QgPluginContext *ctx) { m_ctx = ctx; }

	/* Primitive name */
	QLineEdit *bot_name;
	/* Mode selector (vertex / face / edge) */
	QComboBox *edit_mode;
	QPushButton *write_edit;
	QPushButton *reset_values;

    signals:
	void view_updated(QgViewUpdateFlags);

    private slots:
	void read_from_db();
	void write_to_db();
	void update_obj_wireframe();
	void update_viewobj_name(const QString &);

    protected:
	bool eventFilter(QObject *, QEvent *);

    private:
	struct directory *dp = NULL;
	struct rt_bot_internal *bot = NULL;  /* shallow pointer into rt_db_internal */
	/* Edit-preview overlay feature. */
	bsg_feature_ref p = BSG_FEATURE_REF_NULL_INIT;
	struct bu_vls oname = BU_VLS_INIT_ZERO;
	QgPluginContext *m_ctx = nullptr;

	struct ged *getGed() const;
	struct bsg_view *getView() const;
};

#endif /* QBOT_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

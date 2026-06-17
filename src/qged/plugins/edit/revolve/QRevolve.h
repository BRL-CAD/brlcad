/*                       Q R E V O L V E . H
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
/** @file QRevolve.h
 *
 * stub revolve-solid editor that demonstrates typed edit-preview callbacks
 * for solid-editing plugins.
 *
 * Workflow (same contract as edit/extrude)
 * ----------------------------------------
 * 1. An overlay feature (_revolve_edit) is created and typed edit-preview
 *    callbacks are attached.
 * 2. The update_cb advances the revision whenever edit parameters change.
 * 3. Preview teardown is owned by the feature.
 */

#ifndef QREVOLVE_H
#define QREVOLVE_H

#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include "bsg/feature.h"
#include "raytrace.h"
#include "qtcad/QgTypes.h"

class QgPluginContext;

class QRevolve : public QWidget
{
    Q_OBJECT

    public:
	QRevolve();
	~QRevolve();

	void setContext(QgPluginContext *ctx) { m_ctx = ctx; }

	/* Revolve angle (degrees) */
	QLineEdit *angle;
	/* Primitive name */
	QLineEdit *revolve_name;
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
	struct rt_revolve_internal rev;
	/* Edit-preview overlay feature. */
	bsg_feature_ref p = BSG_FEATURE_REF_NULL_INIT;
	struct bu_vls oname = BU_VLS_INIT_ZERO;
	QgPluginContext *m_ctx = nullptr;

	struct ged *getGed() const;
	struct bsg_view *getView() const;
};

#endif /* QREVOLVE_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

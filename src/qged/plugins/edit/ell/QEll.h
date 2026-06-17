/*                        Q E L L. H
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
/** @file QEll.h
 *
 */

#ifndef QELL_H
#define QELL_H

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include "bsg/feature.h"
#include "raytrace.h"
#include "qtcad/QgColorRGB.h"
#include "qtcad/QgTypes.h"

class QgPluginContext;

class QEll : public QWidget
{
    Q_OBJECT

    public:
	QEll();
	~QEll();

	void setContext(QgPluginContext *ctx) { m_ctx = ctx; }

	// Ell origin
	QCheckBox *O_pnt;

	// Select one or more axes to set the length on
	QCheckBox *A_axis;
	QCheckBox *B_axis;
	QCheckBox *C_axis;

	// Primitive name
	QLineEdit *ell_name;
	QPushButton *write_edit;
	QPushButton *make_sph; // ell -> sph
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
	struct rt_ell_internal ell;
	bsg_feature_ref p = BSG_FEATURE_REF_NULL_INIT;
	struct bu_vls oname = BU_VLS_INIT_ZERO;
	QgPluginContext *m_ctx = nullptr;

	struct ged *getGed() const;
	struct bsg_view *getView() const;
};

#endif //QELL_H

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

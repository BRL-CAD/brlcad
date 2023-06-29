/*               Q G A T T R I B U T E S M O D E L . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2023 United States Government as represented by
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
/** @file QgAttributesModel.h
 *
 * Model for reflecting a BRL-CAD object's attribute keys and values in a way
 * suitable for use in a Qt view.
 */

#ifndef QGATTRIBUTESMODEL_H
#define QGATTRIBUTESMODEL_H

#ifndef Q_MOC_RUN
#include "bu/avs.h"
#include "bv.h"
#include "raytrace.h"
#include "ged.h"
#endif

#include "qtcad/defines.h"
#include "qtcad/QgKeyVal.h"

class QTCAD_EXPORT QgAttributesModel : public QgKeyValModel
{
    Q_OBJECT

    public:  // "standard" custom tree model functions
	explicit QgAttributesModel(QObject *parent = 0, struct db_i *dbip = DBI_NULL, struct directory *dp = RT_DIR_NULL, int show_std = 0, int show_user = 0);
	~QgAttributesModel();

	bool hasChildren(const QModelIndex &parent) const;
	int update(struct db_i *new_dbip, struct directory *dp);

    public slots:
	// Used when a selection in the model has changed
	void refresh(const QModelIndex &idx);
        // Used when the values may have changed in the underlying .g
	void db_change_refresh();
	// Used when the currently opened database changes
	void do_dbi_update(struct db_i *dbip);

    protected:
	bool canFetchMore(const QModelIndex &parent) const;
	void fetchMore(const QModelIndex &parent);

    private:
	void add_Children(const char *name, QgKeyValNode *curr_node);
	struct db_i *current_dbip;
	struct directory *current_dp;
	struct bu_attribute_value_set *avs;
	int std_visible;
	int user_visible;
};

#endif /* QGATTRIBUTESMODEL */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


/*                 C A D A T T R I B U T E S . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file cadattributes.h
 *
 * Brief description
 *
 */

#ifndef CAD_ATTRIBUTES_H
#define CAD_ATTRIBUTES_H

#include "qtcad/QKeyVal.h"

#ifndef Q_MOC_RUN
#include "bu/avs.h"
#include "bv.h"
#include "raytrace.h"
#include "ged.h"
#endif

class CADViewModel : public QKeyValModel
{
    Q_OBJECT

    public:
	explicit CADViewModel(QObject *parent = 0, struct bview *v = NULL);
	~CADViewModel();

    public slots:
	void refresh(struct bview *);

    private:
	struct bview *m_v;
};

class CADAttributesModel : public QKeyValModel
{
    Q_OBJECT

    public:  // "standard" custom tree model functions
	explicit CADAttributesModel(QObject *parent = 0, struct db_i *dbip = DBI_NULL, struct directory *dp = RT_DIR_NULL, int show_std = 0, int show_user = 0);
	~CADAttributesModel();

	bool hasChildren(const QModelIndex &parent) const;
	int update(struct db_i *new_dbip, struct directory *dp);

    public slots:
	void refresh(const QModelIndex &idx);

    protected:
	bool canFetchMore(const QModelIndex &parent) const;
	void fetchMore(const QModelIndex &parent);

    private:
	void add_Children(const char *name, QKeyValNode *curr_node);
	struct db_i *current_dbip;
	struct directory *current_dp;
	struct bu_attribute_value_set *avs;
	int std_visible;
	int user_visible;
};


#endif /*CAD_ATTRIBUTES_H*/

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */


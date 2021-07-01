/*                        Q G M O D E L . H
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
/** @file QgModel.h
 *
 * Abstract Qt model of a BRL-CAD .g database
 *
 * NOTE - the QtCADTree model has many of the pieces we will need,
 * but has limitations.  The intent here is to start "from the
 * ground up" and be guided by the Qt editable example:
 *
 * https://doc.qt.io/qt-5/qtwidgets-itemviews-editabletreemodel-example.html
 *
 * Also investigate https://wiki.qt.io/Model_Test to see if it may be
 * useful for this code.
 */

#ifndef QGMODEL_H
#define QGMODEL_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <QAbstractItemModel>
#include <QModelIndex>

#include "xxhash.h"

#include "qtcad/defines.h"

#ifndef Q_MOC_RUN
#include "raytrace.h"
#include "ged.h"
#endif

class QTCAD_EXPORT QgInstance
{
    public:
	explicit QgInstance();
	~QgInstance();


	unsigned long long hash(int mode);

	struct directory *parent = NULL;
	struct directory *dp = NULL;
	std::string dp_name;
	db_op_t op = DB_OP_NULL;
	mat_t c_m;

	// The following value holds the index of the active flags array to
	// check when determining if this object is active.  I.e. activity is
	// checked by looking at:
	//
	// ctx->active_flags[instance->obj->active_ind];
	int active_ind = -1;

    private:
	XXH64_state_t *h_state;
};

class QTCAD_EXPORT QgItem
{
    public:
	explicit QgItem();
	~QgItem();

	QgInstance *inst;
	unsigned long long hash;

	QgItem *parent;
	std::vector<QgItem *>children;
};

class QTCAD_EXPORT QgModel;

class QgModel_ctx
{
    public:
	explicit QgModel_ctx(QgModel *pmdl = NULL, struct ged *ngedp = NULL);
	~QgModel_ctx();

	// QgModel associated with this context
	QgModel *mdl;

	// .g Db interface and containers
	struct ged *gedp;

	QgInstance *add(std::unordered_map<unsigned long long, QgInstance *> *, QgInstance *);


	std::unordered_map<unsigned long long, QgInstance *> instances;
	// The parent->child storage is (potentially) 1 to many.
	std::unordered_map<struct directory *, std::vector<QgInstance *>> parent_children;
	std::queue<QgInstance *> free_instances;

	// Hierarchy items
	//
	// One of the subtle points of a .g hierarchy representation is that a
	// QgInstance may be locally unique, but not globally unique.  For
	// example, if comb A has underneath it two different instances of comb
	// B using different placement matrices, and B is defined using C:
	//
	// A
	//   u [M1] B
	//            u C
	//   u [M2] B
	//            u C
	//
	// C has only one unique QgInstance definition in this hierarchy - IDN
	// matrix union of C into B - but that instance has TWO manifestations
	// in the overall tree that are NOT the same model items by virtue of
	// having different parent items: Au[M1]BuC and Au[M2]BuC.
	//
	// To represent this, in addition to the parent_child map, we define an
	// items hierarchy which holds QgItems that encode a reference to a
	// QgInstance and the parent/child data specific to an individual place
	// in the hierarchy.  It is a QgItem, not a QgInstance, which can
	// correspond to a row in a Qt abstract model.  Unlike instances,
	// QgItems are created lazily in response to view requests, working
	// from a seed set created from the top level objects in a database.
	//
	// The parent_children hierarchy guides the creation of QgItem hierarchies
	// as follows:  a QgItem has an associated dp (unless it is an invalid
	// leaf) and that dp, if it is not a solid or empty comb, has an associated
	// set of QgInstances that correspond to its children.  Those QgInstances
	// are used, when the children array is populated in the QgItem, to create
	// a new vector of QgItems using those QgInstances as their .g definitions.
	// The hierarchy parent and child relationships are thus established
	// uniquely, by the QgItems, but following the hierarchy implicit in the
	// .g information.
	std::vector<QgItem *> tops;
	std::queue<QgItem *> free_items;

	// Activity flags (used for relevance highlighting) need to be updated
	// whenever a selection changes.  Because the question of whether an
	// object is related to the current selection hinges on the dp (a child
	// being edited included in different combs with different matrices and/
	// or boolean operations still impacts all those combs) the activity
	// flags are dp centric.  We define one flag per dp, which is then referred
	// to by all QgInstances using that dp as a reference
	std::unordered_map<struct directory *, int> dp_to_active_ind;
	std::vector<bool> active_flags;
};

class QTCAD_EXPORT QgModel : public QAbstractItemModel
{
    Q_OBJECT

    public:
	explicit QgModel(QObject *p = NULL, struct ged *ngedp = NULL);
	~QgModel();

	// Qt Model interface
	QModelIndex index(int row, int column, const QModelIndex &p) const;
	QModelIndex parent(const QModelIndex &child) const;
	Qt::ItemFlags flags(const QModelIndex &index);
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole);
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex &parent = QModelIndex()) const;

	bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
	bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role = Qt::EditRole);

	bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex());
	bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
	bool insertColumns(int column, int count, const QModelIndex &parent = QModelIndex());
	bool removeColumns(int column, int count, const QModelIndex &parent = QModelIndex());

	// .g Db interface
	bool run_cmd(struct bu_vls *msg, int argc, const char **argv);
	int opendb(QString filename);
	void closedb();

	QgModel_ctx *ctx;
	bool need_update_nref = false;
	std::unordered_set<struct directory *> changed_dp;

};

#endif //QGMODEL_H

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


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

#include "qtcad/defines.h"

#ifndef Q_MOC_RUN
#include "raytrace.h"
#endif

/*
Generally speaking, the atomic unit of a hierarchical .g model is a comb
tree instance: it's unique matrix and boolean operation + the full path to
that particular instance.  TODO - A further complication is the question of
what to do if a comb tree does encode the exact same child twice - in that
case the only unique reference data wise is the pointer to that particular
comb's data, which is ephemeral... - how do we address an edit operation to
the comb instance in question?  Do we need to store a counter to distinguish
multiple instances?

Design thoughts:

two callbacks and a context pointer added to db_i struct:

dbi_changed
dbi_update_nref
ctx

the former is called when directory entries are added, removed or modified.
the latter is called when ever db_update_nref identifies a parent/child
relationship in the database.


Using those callbacks, we can do our model management based on the db info
in question.  Current thought:

When a QgInstance is selected, its immediate comb parents will be "activated" and
placed in a queue, and for each queued dp any of its parents not already
active will also be activated and placed in the queue.  Eventually, once the
queue is empty, all impacted combs will have "active" flags set which means
the model items (which will know about the QgInstance which in turn will
know its own leaf dp, and can report the activity flag to the highlighting
delegate).  If the above ends up being performant it's worth exploring, since
the highlighting operation will need to be done every time either the tree
selection or the highlighting mode changes.
*/

class QTCAD_EXPORT QgInstance
{
    public:
	explicit QgInstance(QgInstance *p = NULL);
	~QgInstance();

	struct directory *dp = NULL;
	std::string dp_name;
	db_op_t op = DB_OP_NULL;
	mat_t c_m;
	QgInstance *parent = NULL;

};

class QTCAD_EXPORT QgModel;

class QgModel_ctx
{
    public:

	// .g Db interface and containers
	struct db_i *dbip;

	// The parent->child storage is (potentially) 1 to many.
	std::unordered_map<std::string, std::vector<QgInstance *>> parent_children;

	// Because child names do not map uniquely to QgInstances (the same dp
	// may be used in many trees with different matrices and boolean ops)
	// the mapping must be more complex.  The structure is:
	//
	// child -> <parent -> <instances matching parent->child relationship>>
	//
	// So a child may have multiple parents.  For each parent, the child
	// may be present in more than one instance in that hierarchy.
	//
	// To add an entry, we first assign
	std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<QgInstance *>>> child_parents;

	// QgModel associated with this context
	QgModel *mdl;

    private:
	bool model_needs_update = true;
	std::queue<QgInstance *> free_instances;

};

class QTCAD_EXPORT QgModel : public QAbstractItemModel
{
    Q_OBJECT

    public:
	explicit QgModel(QObject *p = NULL);
	~QgModel();

	// Qt Model interface
	Qt::ItemFlags flags(const QModelIndex &index);
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole);
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole);
	int rowCount(const QModelIndex &parent = QModelIndex());
	int columnCount(const QModelIndex &parent = QModelIndex());

	bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
	bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role = Qt::EditRole);

	bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex());
	bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
	bool insertColumns(int column, int count, const QModelIndex &parent = QModelIndex());
	bool removeColumns(int column, int count, const QModelIndex &parent = QModelIndex());

	// .g Db interface
	QgModel_ctx *ctx;

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


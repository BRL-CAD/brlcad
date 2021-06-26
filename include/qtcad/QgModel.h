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

add a callback to librt's db_i that has the following signature:

void db_changed(struct directory *dp, void *internal, int change_type, void *ctx);

ctx will also be a structure element added to db_i

Then, in db_put_internal, db_diradd, and db_dirremove(?) check if the
db_i callback is defined.  If it is, call it with the appropriate change
type set.  pass ctx from the db_i slot and the dp and (optional) internal
from the local context's info

Then, in the callback, we can do our model management based on the db change
in question.  Current thought:

The first generation CADTreeModel defined a function that walked the
subtrees of each open node to determine relationships.  Another possibility
will be to maintain child->parents model-level containers of the form:

unordered_map(parent_dp, unordered_set<child dps>)
unordered_map(child_dp, unordered_set<parent dps>)

the above, if maintained, give us enough info so that each db_change
we can add/remove in a targeted fashion rather than having to scan
everything each time.  If a dp is removed, we look up with it as a
child key, find its parents, and remove the dp from each parent's
child unordered set.  Then, we remove the child entry from the other
map.

We also need to maintain dp->QgInstance mappings, so we can mange the instances
based on the db_change info.  In particular, if the operation is an edit or add
of a comb dp we need to use the comb tree to update the instances.  We may
have to also maintain a names-as-lookup-keys mapping system for defining parent
child relationships...  comb trees use names as specifiers, so we will need to
translate those names to both dps and QgInstances in order to specify the model
parent child relationships.

We will also need to maintain a set of vectors of strings to indicate which
paths are populated - if we delete a comb instance and all its children, we need
to rebuild the parent comb's tree with as close to the original "opened" state
as the new tree structure will allow


The hierarchy inherent in tree models means when adding an instance, we only
need to process it in the local comb tree context. A comb reused in multiple
places in the hierarchy will produce identical instances in a data sense,
but because the parent chains of each will be different they may be
considered in isolation.  Also, if we need to invalidate and remove
QgInstances after a db edit, we can work top down - if a particular child
has been removed its parent, all QgInstances below that instance are also
invalidated.

The intent is for these to be lazily populated, starting with only the top
level objects with NULL as their parent.  Production .g hierarchies can be
enormous, and we don't want to consume any more resources representing and
viewing them than are necessary for what the user has requested.  However,
that leaves us with a problem in that the highlighting options we want to
support (the ones identifying other portions of the tree related to a
selected item) need comprehensive knowledge of the .g hierarchy.

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

#if 0
class QTCAD_EXPORT QgInstance
{
    public:
	explicit QgInstance(QgInstance *p = NULL);
	~QgModel();

	QgInstance *parent();
	bool valid();

	struct directory *dp = NULL;
	db_op_t op = DB_OP_NULL;
	mat_t c_m;
	QgInstance *parent = NULL;

}
#endif

class QTCAD_EXPORT QgModel : public QAbstractItemModel
{
    Q_OBJECT

    public:
	explicit QgModel(QObject *p = NULL);
	~QgModel();

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

	struct db_i *dbip;
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


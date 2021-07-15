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

	// QgInstance content based hash for quick lookup/comparison of two
	// QgInstances.  By default incorporate everything into the hash -
	// other modes are allowed for in case we want to do partial hashing.
	unsigned long long hash(int mode = 3);

	// dp of parent comb (NULL for tops objects)
	struct directory *parent = NULL;
	// dp of comb instance (NULL for instances with invalid object names)
	struct directory *dp = NULL;
	// instance name as string
	std::string dp_name;
	// instance boolean operation incorporating it into the comb tree (u/-/+)
	db_op_t op = DB_OP_NULL;
	// Matrix above comb instance in comb tree (default is IDN)
	mat_t c_m;

	// BRL-CAD's combs do not have internal guarantees of instance
	// uniqueness within a comb tree - i.e., it is structurally possible
	// for two absolutely identical combination instances to exist
	// simultaneously in the same comb.  Generally speaking this is not a
	// desired condition, but because it is not precluded we must make
	// arrangements to handle it.
	//
	// As long as QgInstances are unique within their parent comb's
	// immediate tree, they are globally unique - the parent is a unique
	// object, so even if an otherwise identical instance exists in another
	// comb the difference is clear from a data standpoint.  Therefore, in
	// order to produce a QgInstance that is globally unique, we must
	// provide a mechanism for encoding that uniqueness at the QgInstance
	// level.
	//
	// The solution is quite simple - a simple incremented instance counter
	// that gets incremented each time another duplicate of a particular
	// comb instance is encountered within the same comb's immediate tree.
	// For the instance in question, the current counter value is assigned
	// to the icnt variable.  Since we are incrementing each time an
	// instance is encountered, this produces a QgInstance that can be
	// identified uniquely.  Because we must have global knowledge of the
	// comb tree's contents to generate the counts, we must assign these
	// values during the tree walk which creates the instances.
	//
	// There is a limit to how "neatly" we can associate a QgInstance with
	// an exact on-disk comb tree instance - if a tree with multiple such
	// instances is edited, icnt == 3 in the original tree may become icnt
	// == 2 or icnt == 4 in the next version.  If we want to preserve the
	// "open" and "closed" states of such instances through editing, we
	// would have to take particular care with bookkeeping to index which
	// duplicates are open and adjust those indices to reflect the changes
	// made to the comb's tree during an edit.  Whether we would actually
	// match the original instances even then depends on the details of the
	// comb tree storage and walking - it is unlikely that the code
	// complexity inherent in such tracking is worthwhile given this case
	// is rare and a borderline modeling error in the first place.
	size_t icnt = 0;

	// The following value holds the index of the active flags array to
	// check when determining if this object is active.  I.e. activity is
	// checked by looking at:
	//
	// ctx->active_flags[instance->obj->active_ind];
	//
	// We want to record an instance's active state in this fashion because
	// it lets us use active_flags to iteratively set flags "up" the tree
	// hierarchy (i.e. for each active QgInstance, set its parent's
	// QgInstance flag as active.  Count currently set active_flags
	// entries, and if that count is not equal to the prior count repeat
	// the process.  When no new flags are set, we have identified all
	// QgInstances activated by the current selection, and thus each QgItem
	// will know enough to be able to tell the drawing delegate how it
	// needs to display itself.  We can also trivially clear the
	// active_flags at the beginning of each selection action by clearing
	// the active_flags array.
	//
	// Note that when doing such flag setting, it doesn't matter whether
	// the QgInstance has any associated QgItems.  Even if it doesn't,
	// somewhere further up the tree a parent comb will need to display
	// its state change and the assignments must take place in order to
	// propagate the necessary awareness up to the visible level.
	int active_ind = -1;

    private:
	XXH64_state_t *h_state;
};

// QgItems correspond to the actual Qt entries displayed in the view.  If a
// comb is reused in multiple places in the tree a QgInstance may be referenced
// by multiple QgItems, but each QgItem corresponds uniquely to a single string
// of directory pointers, ops, matrices and (if needed icnt values) from a
// series of QgInstance structures generated by walking comb trees.  A single
// QgItem being selected may activate multiple QgItems by virtue of their using
// the same QgInstance as the selected QgItem (this reflects the impact any
// changes will have on the database contents) but from the Qt perspective the
// unique selection item is the QgItem.  If the QgItem is changed (added,
// moved, deleted, etc.) other QgItems with the same QgInstance will have to be
// separately addressed to reflect the same changes.
//
// A QgItem will store parents and children, but remember that both of those
// relationships are informed by the QgInstance relationships rather than directly
// representing them.  A QgItem always has a unique parent, even if its QgInstance
// is associated with multiple active QgItems elsewhere in the tree.  The children
// array reflects the open/closed state of the item - if it is closed, the chilren
// array is empty even though the comb it corresponds to may have many children.
// It is only when the QgItem is opened that the inst QgInstance is queried for
// its children and new QgItems based on the QgInstance children are created.
// Conversely, QgItem children are destroyed when the QgItem is closed, even
// though the QgInstances are not.
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

// Forward declaration for context.  It may be in the end that we merge
// the ctx with QgModel itself, but for the moment they are distinct...
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

	// The parent->child storage is (potentially) 1 to many. TODO -
	// we may just do this with the localized tree walk of the comb
	// and a hash lookup of each leaf, rather than maintaining a
	// separate parent_children container - this probably has too
	// much opportunity for getting out of sync with the real comb state,
	// and may not really have any simplicity benefits either, given
	// the one-to-many issues...
	std::unordered_map<struct directory *, std::vector<QgInstance *>> parent_children;

	// We maintain the vector above for leaf ordering, but we also build a
	// map of QgInstance hashes to instances for easy lookup.  This is for
	// QgInstance reuse - we in fact have to construct a temporary
	// QgInstance during the tree walk to get the lookup hash, but in trees
	// that heavily reuse combs avoiding the storing of duplicate
	// QgInstances will save memory.
	//
	// TODO - the nested map with the parent dp as the first key mainly
	// serves to reduce the chance of hash collisions, but if that's not a
	// practical issue we should simplify this
	std::unordered_map<struct directory *, std::unordered_map<unsigned long long, QgInstance *>> ilookup;

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
	//                u D
	//   u [M2] B
	//            u C
	//                u D
	//
	// D has only one unique QgInstance definition in this hierarchy - IDN
	// matrix union of D into C - but that instance has TWO manifestations
	// in the overall tree that are NOT the same model items by virtue of
	// having different B parent instances further up in the hierarchy:
	// Au[M1]B and Au[M2]B.
	//
	// To with this the primary model hierarchy is an items hierarchy which
	// holds QgItems that encode a reference to a QgInstance and the
	// parent/child data specific to an individual place in the hierarchy.
	// It is a QgItem, not a QgInstance, which can correspond to a row in a
	// Qt abstract model.  Unlike QgInstances, QgItems are created lazily in
	// response to view requests, working from a seed set created from the
	// top level objects in a database.
	//
	// The seed set is the set of objects with no parents in the hierarchy.
	// (What users of MGED think of as the "tops" set.)  This set may change
	// after each database edit, but there will always be at least one object
	// in a valid .g hierarchy that has no parent.
	//
	// TODO - check what tops does about a database with a cyclic definition
	// of an otherwise top level comb...  I suspect in that case we may have
	// to break the cycle to generate a pseudo-tops object...
	std::vector<QgItem *> tops;

	// Activity flags (used for relevance highlighting) need to be updated
	// whenever a selection changes.  We define one flag per QgInstance,
	// and update flags accordingly based on current app settings.
	// Highlighting is then keyed in each QgItem based on the current value
	// of these flags.
	std::vector<int> active_flags;
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


/*                        Q G M O D E L . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
 * Qt uses a Model/View approach to visualizing information such as trees, and
 * for BRL-CAD hierarchy visualization the most natural fit in Qt is to define
 * a Qt Model that corresponds to the .g data.  There are challenges in
 * defining such a model that aren't immediately apparent, and they complicate
 * the implication considerably.  In essence it is necessary to translate
 * BRL-CAD's data concepts to those needed by Qt and vice versa.  To attempt
 * to articulate the motivations for the model as defined here, the following
 * discussion tries to detail the issues encountered.
 *
 * The most fundamental difficulty is that comb object instances - the primary
 * hierarchical unit defined in .g files - are not necessarily globally unique
 * in the .g file. To illustrate this, consider a combination A that has
 * underneath it two different instances of a combination B using different
 * placement matrices (M1 and M2). B is in turn defined using combination C
 * which is defined using object D:
 *
 * A
 *   u [M1] B
 *            u C
 *                u D
 *   u [M2] B
 *            u C
 *                u D
 *
 * The objects A, B, C and D are unique (all .g objects are uniquely named,
 * which ensures uniqueness in at least one sense) but the Qt model hierarchy
 * requires the expression of comb INSTANCES, not objects.  Moreover, it needs
 * those instances to be not just locally unique, but GLOBALLY unique.  The
 * union of D into C using an identity matrix is unique with respect to B, but
 * not when B is combined with A multiple times.  That inclusion of B into A
 * (which is completely non-local as far as C's or B's definitions are
 * concerned) results in TWO manifestations of CuD in the overall model that
 * are NOT the same model item, despite mapping to exactly the same .g data.
 * Or, stated another way, we have multiple items of CuD in the hierarchy:
 * Au[M1]BuCuD and Au[M2]BuCuD.  However, from the .g data perspective, CuD IS
 * a unique combination instance - it is stored only once, and any edit to C or
 * D will impact BOTH Au[M1]BuCuD and Au[M2]BuCuD. This means that .g comb
 * instances cannot, by themselves, map directly to items in a Qt model.
 *
 * Conversely, in a Qt .g model, if a Qt item which is going to express .g
 * instance editing the mode must be prepared to cope with changes having non
 * local side effects - i.e. any change to one item can potentially change an
 * arbitrary number of additional items, based on what else in the .g database
 * is referencing the object being changed.
 *
 * The approach taken to cope with these issues is to conceptually split the
 * wrapping of .g comb instances into two parts - a QgInstance that corresponds
 * to one unique parent+bool_op+matrix+child instance in a .g combination tree,
 * and a QgItem which corresponds to a globally unique instance of a QgInstance.
 * So, in the above example, CuD would be expressed as a QgInstance, and the
 * Au[M1]BuCuD QgItem would reference the CuD QgInstance to connect the .g
 * database information to the unique model hierarchy item.  Likewise, the
 * Au[M2]BuCuD QgItem would reference the same QgInstance, despite being a
 * different item in the model.
 *
 * QgInstances are created to reflect the .g database state, and they are what
 * is updated as a direct consequence of callbacks registered with the low
 * level database I/O routines.  QgItems must then be updated (or
 * added/removed) in response to QgInstance changes.  The only QgItems that
 * will be populated by default are the top level items - all others will be
 * lazily created in response as the user specifies which combs to open in
 * the view.  (The latter is necessary for large databases, which may have
 * very large and deep hierarchies.)
 *
 *
 *
 *
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

class QTCAD_EXPORT QgModel;

class QTCAD_EXPORT QgInstance
{
    public:
	explicit QgInstance();
	~QgInstance();

	// Debugging function for printing out data in container
	std::string print();

	// QgInstance content based hash for quick lookup/comparison of two
	// QgInstances.  By default incorporate everything into the hash -
	// other modes are allowed for in case we want to do fuzzy matching.
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

	// Return hashes of child instances, if any
	std::vector<unsigned long long> children();

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

	// If available, the model context in which this instance is defined
	QgModel *ctx;

    private:
	XXH64_state_t *h_state;
};

// QgItems correspond to the actual Qt entries displayed in the view.  If a
// comb is reused in multiple places in the tree a QgInstance may be referenced
// by multiple QgItems, but each QgItem corresponds uniquely to a single string
// of directory pointers, ops, matrices and (if needed) icnt values from a
// series of QgInstance structures generated by walking comb trees.  A single
// QgItem being selected may activate multiple QgItems by virtue of those other
// QgItems using the same QgInstance as the selected QgItem (this reflects the
// impact any changes will have on the database contents) but from the Qt
// perspective the unique selection item is the QgItem.  If the QgItem is
// changed (added, moved, deleted, etc.) other QgItems with the same QgInstance
// will have to be separately handled to reflect the same changes.
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


	// Testing functions to simulate GUI activities
	void open();
	void close();

	// If a database edit has occurred, everything in
	// the tree must be validated to see if it is still
	// current.
	bool update_children();
	void remove_children();


	void appendChild(QgItem *C);
	QgItem *child(int n);
	int childCount() const;
#if 0
	int columnCount() const;
	QVariant data(int col) const;
	bool insertChildren(int pos, int cnt, int col);
	bool insertColumns(int pos, int col);
	QgItem *parent();
	bool removeChildren(int pos, int cnt);
	bool removeColumns(int pos, int cnt);
	int childNumber() const;
	bool setData(int col, const QVariant &v);
#endif

	unsigned long long ihash = 0;
	bool open_itm = false;
	QgModel *ctx = NULL;
	QgItem *parent = NULL;

    private:
	std::vector<QgItem *> children;
};

class QTCAD_EXPORT QgModel : public QAbstractItemModel
{
    Q_OBJECT

    public:
	explicit QgModel(QObject *p = NULL, const char *npath = NULL);
	~QgModel();

	// Certain .g objects (comb, extrude, etc.) will define one or more
	// implicit instances.  We need to create those instances both on
	// initialization of an existing .g file and on database editing.
	void add_instances(struct directory *dp);

	// A modification in the .g file to an object may necessitate a variety
	// of updates to the QgInstance containers
	void update_instances(struct directory *dp);

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
	//std::unordered_map<struct directory *, std::unordered_map<unsigned long long, QgInstance *>> ilookup;

	std::unordered_map<unsigned long long, QgInstance *> *instances = NULL;
	std::unordered_map<unsigned long long, QgInstance *> *tops_instances = NULL;

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
	// TODO - tops does not work if we have a database with a cyclic definition
	// of an otherwise top level comb.  There are a couple of possible options
	// for that case:  1) fall back on a straight-up listing of all objects
	// IFF tops is empty 2) fall back on the all-objs listing if a cyclic path
	// is detected in the database 3) do a more sophisticated analysis of the
	// database trees and generate a "pseudo-tops" list with the cycles broken
	// #1 is the simplest but will result in some portion of the .g file not
	// being visible in the tree view at all if we do have some tops objects
	// but one or more cyclic cases.  #2 requires cyclic path detection and
	// could result in an unusably large listing of objects for big .g files.
	// #3 would be the best answer but has a currently unknown implementation
	// difficulty and performance cost (remember the tops set has to be
	// regenerated after every database edit since each edit hast the potential
	// to alter the tops set.)
	std::vector<QgItem *> tops_items;

	// Activity flags (used for relevance highlighting) need to be updated
	// whenever a selection changes.  We define one flag per QgInstance,
	// and update flags accordingly based on current app settings.
	// Highlighting is then keyed in each QgItem based on the current value
	// of these flags.
	std::vector<int> active_flags;


	// Qt Model interface
	QModelIndex index(int row, int column, const QModelIndex &p) const;
	QModelIndex parent(const QModelIndex &child) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex &parent = QModelIndex()) const;

	bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
	bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role = Qt::EditRole);

	bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex());
	bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
	bool insertColumns(int column, int count, const QModelIndex &parent = QModelIndex());
	bool removeColumns(int column, int count, const QModelIndex &parent = QModelIndex());

	// .g Db interface and containers
	bool run_cmd(struct bu_vls *msg, int argc, const char **argv);
	int opendb(QString filename);
	int opendb(const char *npath);
	void closedb();
	bool IsValid();
	struct ged *gedp;

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


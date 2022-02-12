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
 * wrapping of .g comb instances into two parts - a gInstance that corresponds
 * to one unique parent+bool_op+matrix+child instance in a .g combination tree,
 * and a QgItem which corresponds to a globally unique instance of a gInstance.
 * So, in the above example, CuD would be expressed as a gInstance, and the
 * Au[M1]BuCuD QgItem would reference the CuD gInstance to connect the .g
 * database information to the unique model hierarchy item.  Likewise, the
 * Au[M2]BuCuD QgItem would reference the same gInstance, despite being a
 * different item in the model.
 *
 * gInstances are created to reflect the .g database state, and they are what
 * is updated as a direct consequence of callbacks registered with the low
 * level database I/O routines.  QgItems must then be updated (or
 * added/removed) in response to gInstance changes.  The only QgItems that
 * will be populated by default are the top level items - all others will be
 * lazily created in response as the user specifies which combs to open in
 * the view.  (The latter is necessary for large databases, which may have
 * very large and deep hierarchies.)
 *
 *
 * Notes:
 * Qt is very particular about adding and removing rows into a model, and
 * I suspect it will be necessary to alter QgItems only in very special
 * circumstances (i.e. we will have to only alter gInstances from librt
 * callbacks, and update the QgItems later when we can ensure it is "OK"
 * to do so from Qt's perspective.)
 *
 * Among other considerations, that probably means we'll have to change the
 * QgItem to be somewhat more "self contained" as far as its display data
 * rather than pulling from the gInstance, since the gInstance will be altered
 * or removed by the librt callbacks and may vanish at an inconvenient time for
 * Qt...
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
#include <QImage>
#include <QModelIndex>

#include "xxhash.h"

#include "qtcad/defines.h"

#ifndef Q_MOC_RUN
#include "raytrace.h"
#include "ged.h"
#endif

class QTCAD_EXPORT gInstance
{
    public:
	explicit gInstance();
	~gInstance();

	// Debugging function for printing out data in container
	std::string print();

	// gInstance content based hash for quick lookup/comparison of two
	// gInstances.  By default incorporate everything into the hash -
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

	// This is a flag that may be set or unset by parent applications.
	// Used primarily to assist in visual identification of components
	// in hierarchical structures.
	int active_flag = 0;

	// The model context in which this instance is defined
	void *ctx;

    private:
	XXH64_state_t *h_state;
};

// QgItems correspond to the actual Qt entries displayed in the view.  If a
// comb is reused in multiple places in the tree a gInstance may be referenced
// by multiple QgItems, but each QgItem corresponds uniquely to a single string
// of directory pointers, ops, matrices and (if needed) icnt values from a
// series of gInstance structures generated by walking comb trees.  A single
// QgItem being selected may activate multiple QgItems by virtue of those other
// QgItems using the same gInstance as the selected QgItem (this reflects the
// impact any changes will have on the database contents) but from the Qt
// perspective the unique selection item is the QgItem.  If the QgItem is
// instructed to conduct an operation with implications at the gInstance level
// (modification of primitive parameters, for example) other QgItems with the
// same gInstance will have to be separately handled to reflect the same
// changes.
//
// A QgItem will store parents and children, but remember that both of those
// relationships are informed by the gInstance relationships rather than directly
// representing them.  A QgItem always has a unique parent, even if its
// gInstance is associated with multiple active QgItems elsewhere in the tree.
// The children array is initially empty even though the comb it corresponds to
// may have many children.  It is only when the QgItem is opened that the
// gInstance is queried for its children and new QgItems based on the
// gInstance children are created.  Because we don't want to collapse all
// opened/closed state in a subtree if we close a parent QgItem, the children
// QgItems remain populated through a close to persist that state.

class QTCAD_EXPORT QgModel;

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
	void remove_children();

	// Return a pointer to the gInstance associated with this QgItem, or
	// NULL if no valid instance is found.
	gInstance *instance();

	void appendChild(QgItem *C);
	QgItem *child(int n);
	int childCount() const;
	std::vector<QgItem *> children;

	// NOTE - for now this is 1 - the model will have to
	// incorporate some notion of exposed attributes and
	// their ordering before that changes
	int columnCount() const;
#if 0
	QVariant data(int col) const;
	bool insertChildren(int pos, int cnt, int col);
	bool insertColumns(int pos, int col);
#endif
	QgItem *parent();
#if 0
	bool removeChildren(int pos, int cnt);
	bool removeColumns(int pos, int cnt);
#endif
	int childNumber() const;
#if 0
	bool setData(int col, const QVariant &v);
#endif

	/* To a BRL-CAD developer's eyes this is rather abstract, but the
	 * combination of the ihash and ctx points to the BRL-CAD specific data
	 * for this item via the ability to look up its related gInstance in
	 * the ctx map via the hash.  The parent QgItem in turn provides the
	 * relationship which makes this entity distinct from all other
	 * gInstances present in the .g file.
	 *
	 * We do this rather than storing a gInstance pointer, because we will
	 * need to be able to determine if a given QgItem is valid at various
	 * points in the logic, and one of the validity tests is whether the
	 * gInstance it represents is still active in the model.  Unlike a
	 * pointer, the ihash lookup will let us make a valid/invalid
	 * determination with a find lookup on the model's instance map.
	 */
	unsigned long long ihash = 0;
	QgModel *ctx = NULL;
	QgItem *parentItem = NULL;

	/* Flag to determine if this QgItem should be viewed as opened or closed -
	 * in order to preserve subtree state, we don't want to obliterate the
	 * subtree related data structures, so we can't test for an empty children
	 * vector to make this determination. */
	bool open_itm = false;

	// Cached data from the gInstance, so we can keep
	// displaying while librt does work on the gInstances.
	struct bu_vls name = BU_VLS_INIT_ZERO;
	db_op_t op = DB_OP_UNION;
	struct directory *dp = NULL;
	QImage icon;
};

/* The primary expression in a Qt context of a .g database and its contents.
 * This is an important data structure for the construction of .g graphical
 * interfaces.
 *
 * The concept of "columns" in this model best maps to attributes on .g objects
 * - by default, the only attribute data visible is implicit in boolean labels
 * and icons, so there is only one column in the model.  That said, there is
 * definitely interest in an ability for users to visualize and modify
 * attributes as additional columns in a tree view, so we need to design and
 * maintain this code with that eventual use case in mind.
 */
class QTCAD_EXPORT QgModel : public QAbstractItemModel
{
    Q_OBJECT

    public:
	explicit QgModel(QObject *p = NULL, const char *npath = NULL);
	~QgModel();

	// Get the root QgItem
	QgItem *root();

	// Certain .g objects (comb, extrude, etc.) will define one or more
	// implicit instances.  We need to create those instances both on
	// initialization of an existing .g file and on database editing.
	void add_instances(struct directory *dp);

	// A modification in the .g file to an object may necessitate a variety
	// of updates to the gInstance containers
	void update_instances(struct directory *dp);

	// After a major change (opening or re-opening) rebuild the model
	void refresh();

	// Build a map of gInstance hashes to instances for easy lookup.  This
	// is for gInstance reuse.  In trees that heavily reuse combs avoiding
	// the storing of duplicate gInstances will save memory.
	std::unordered_map<unsigned long long, gInstance *> *instances = NULL;

	// Hierarchy items
	//
	// One of the subtle points of a .g hierarchy representation is that a
	// gInstance may be locally unique, but not globally unique.  For
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
	// D has only one unique gInstance definition in this hierarchy - IDN
	// matrix union of D into C - but that instance has TWO manifestations
	// in the overall tree that are NOT the same model items by virtue of
	// having different B parent instances further up in the hierarchy:
	// Au[M1]B and Au[M2]B.
	//
	// To with this the primary model hierarchy is an items hierarchy which
	// holds QgItems that encode a reference to a gInstance and the
	// parent/child data specific to an individual place in the hierarchy.
	// It is a QgItem, not a gInstance, which can correspond to a row in a
	// Qt abstract model.  Unlike gInstances, QgItems are created lazily in
	// response to view requests, working from a seed set created from the
	// top level objects in a database.
	//
	// The seed set is the set of objects with no parents in the hierarchy.
	// (What users of MGED think of as the "tops" set, as well as objects
	// with cyclic subtrees - the latter would be invisible in the tree
	// view if we do not list them as top level objects.)  This set may
	// change after each database edit, but there will always be at least
	// one object in a valid .g hierarchy that has no parent.
	std::unordered_map<unsigned long long, gInstance *> *tops_instances = NULL;
	std::vector<QgItem *> tops_items;

	// .g Db interface and containers
	int run_cmd(struct bu_vls *msg, int argc, const char **argv);
	bool IsValid();
	struct ged *gedp = NULL;

	bool need_update_nref = false;

	/* Used by callers to identify which objects need to be redrawn when
	 * scene views are updated. */
	std::unordered_set<struct directory *> changed_dp;

	// Qt Model interface
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	QModelIndex index(int row, int column, const QModelIndex &p) const override;
	QModelIndex parent(const QModelIndex &child) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	bool canFetchMore(const QModelIndex &idx) const override;
	void fetchMore(const QModelIndex &idx) override;
	void remove_children(QgItem *node);
	QModelIndex NodeIndex(QgItem *node) const;
	int NodeRow(QgItem *node) const;

	Qt::ItemFlags flags(const QModelIndex &index) const override;
	bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
	bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role = Qt::EditRole) override;

	bool insertColumns(int column, int count, const QModelIndex &parent = QModelIndex()) override;
	bool removeColumns(int column, int count, const QModelIndex &parent = QModelIndex()) override;
	bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
	bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    signals:
	void mdl_changed_db(void *);

    private:
	QgItem *getItem(const QModelIndex &index) const;
	QgItem *rootItem;
	struct bview *empty_gvp = NULL;
	struct db_i *model_dbip = NULL;
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


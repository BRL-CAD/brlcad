/*       T E S T _ Q T C A D _ M O D E L _ I N V A R I A N T S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <cstdio>
#include <initializer_list>

#include <QAbstractItemModelTester>
#include <QApplication>
#include <QPersistentModelIndex>
#include <QSignalSpy>

#include "bu/app.h"
#include "bu/file.h"
#include "ged.h"
#include "ged/event.h"
#include "qtcad/QgModel.h"
#include "wdb.h"

#define CHECK(_condition, _message) do { \
    if (!(_condition)) { \
	std::fprintf(stderr, "FAIL: %s\n", (_message)); \
	return 1; \
    } \
} while (0)


static bool
write_comb(struct rt_wdb *wdbp, const char *name,
    std::initializer_list<const char *> members)
{
    struct wmember combination_members;
    BU_LIST_INIT(&combination_members.l);
    for (const char *member : members) {
	if (!mk_addmember(member, &combination_members.l, NULL, WMOP_UNION))
	    return false;
    }
    return mk_lcomb(wdbp, name, &combination_members, 0, NULL, NULL, NULL,
	0) == 0;
}


static bool
make_model_db(const char *path)
{
    struct rt_wdb *wdbp = wdb_fopen(path);
    if (!wdbp)
	return false;

    point_t center = VINIT_ZERO;
    bool created = mk_sph(wdbp, "first.s", center, 1.0) == 0;
    VSET(center, 3.0, 0.0, 0.0);
    created = created && mk_sph(wdbp, "second.s", center, 1.0) == 0;

    created = created && write_comb(wdbp, "child.c",
	{"first.s", "second.s"});
    created = created && write_comb(wdbp, "root.c", {"child.c"});

    wdb_close(wdbp);
    return created;
}


static QModelIndex
find_child(QAbstractItemModel *model, const QModelIndex &parent,
    const QString &name)
{
    for (int row = 0; row < model->rowCount(parent); row++) {
	QModelIndex index = model->index(row, 0, parent);
	if (model->data(index, Qt::DisplayRole).toString() == name)
	    return index;
    }
    return QModelIndex();
}


static bool
run_command(QgModel *model, int argc, const char **argv)
{
    return model->run_cmd(model->ged()->ged_result_str, argc, argv) ==
	BRLCAD_OK;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
	qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    const char *database_path = "qtcad_model_invariants_tmp.g";
    (void)bu_file_delete(database_path);
    CHECK(make_model_db(database_path), "could not create model database");

    {
	QgModel model(NULL, database_path);
	QAbstractItemModel *abstract_model = &model;
	QAbstractItemModelTester tester(&model,
	    QAbstractItemModelTester::FailureReportingMode::Fatal);
	QSignalSpy reset_spy(&model, &QAbstractItemModel::modelReset);
	QSignalSpy rows_inserted_spy(&model,
	    &QAbstractItemModel::rowsInserted);

	CHECK(model.columnCount() == 1, "model reports one display column");
	CHECK(model.rowCount() == 1, "model reports one top-level object");
	QModelIndex root_index = find_child(&model, QModelIndex(), "root.c");
	CHECK(root_index.isValid(), "model exposes the top-level combination");
	CHECK(!abstract_model->parent(root_index).isValid(),
	    "top-level item has no model parent");
	CHECK(model.hasChildren(root_index),
	    "top-level combination reports lazy children");
	if (model.canFetchMore(root_index))
	    model.fetchMore(root_index);
	CHECK(model.rowCount(root_index) == 1,
	    "top-level combination materializes its child");

	QModelIndex child_index = find_child(&model, root_index, "child.c");
	CHECK(child_index.isValid(), "nested combination is addressable");
	if (model.canFetchMore(child_index))
	    model.fetchMore(child_index);
	CHECK(model.rowCount(child_index) == 2,
	    "nested combination materializes both leaves");
	CHECK(find_child(&model, child_index, "first.s").isValid() &&
	    find_child(&model, child_index, "second.s").isValid(),
	    "materialized leaves retain their database names");
	QPersistentModelIndex persistent_root(root_index);
	QPersistentModelIndex persistent_child(child_index);

	const char *copy_command[] = {"cp", "first.s", "loose.s", NULL};
	CHECK(run_command(&model, 3, copy_command),
	    "copy command adds a top-level object");
	CHECK(reset_spy.count() == 0 && rows_inserted_spy.count() > 0,
	    "structural mutation publishes incremental row insertion");
	CHECK(persistent_root.isValid() && persistent_child.isValid(),
	    "incremental insertion preserves persistent model indexes");
	CHECK(find_child(&model, QModelIndex(), "loose.s").isValid(),
	    "new top-level object appears after reconciliation");

	QSignalSpy rows_removed_spy(&model,
	    &QAbstractItemModel::rowsRemoved);
	const int inserted_before_rename = rows_inserted_spy.count();
	const int removed_before_rename = rows_removed_spy.count();
	const char *rename_command[] = {
	    "move", "loose.s", "renamed.s", NULL
	};
	CHECK(run_command(&model, 3, rename_command),
	    "move command renames a top-level object");
	CHECK(reset_spy.count() == 0 &&
	    rows_inserted_spy.count() > inserted_before_rename &&
	    rows_removed_spy.count() > removed_before_rename,
	    "rename publishes row replacement without resetting the model");
	CHECK(persistent_root.isValid() && persistent_child.isValid(),
	    "rename preserves unrelated persistent model indexes");
	CHECK(!find_child(&model, QModelIndex(), "loose.s").isValid() &&
	    find_child(&model, QModelIndex(), "renamed.s").isValid(),
	    "rename replaces the old top-level identity with the new one");

	const char *group_command[] = {"g", "extra.c", "renamed.s", NULL};
	CHECK(run_command(&model, 3, group_command),
	    "group command reparents the new object");
	QModelIndex extra_index = find_child(&model, QModelIndex(), "extra.c");
	CHECK(extra_index.isValid(), "new top-level group appears");
	if (model.canFetchMore(extra_index))
	    model.fetchMore(extra_index);
	CHECK(find_child(&model, extra_index, "renamed.s").isValid(),
	    "reparented object appears under the new group");
	CHECK(!find_child(&model, QModelIndex(), "renamed.s").isValid(),
	    "reparented object is no longer top-level");

	struct rt_wdb *wdbp = wdb_dbopen(model.ged()->dbip,
	    RT_WDB_TYPE_DB_DEFAULT);
	CHECK(wdbp != RT_WDB_NULL,
	    "direct mutation fixture opens a database writer");
	point_t center = {10.0, 0.0, 0.0};
	CHECK(ged_event_batch_begin(model.ged()) == GED_EVENT_OK,
	    "cross-parent setup begins an event batch");
	CHECK(mk_sph(wdbp, "moved_leaf.s", center, 1.0) == 0,
	    "cross-parent setup creates the moved subtree leaf");
	VSET(center, 12.0, 0.0, 0.0);
	CHECK(mk_sph(wdbp, "source_keep.s", center, 1.0) == 0,
	    "cross-parent setup creates the source leaf");
	VSET(center, 14.0, 0.0, 0.0);
	CHECK(mk_sph(wdbp, "destination_keep.s", center, 1.0) == 0,
	    "cross-parent setup creates the destination leaf");
	CHECK(write_comb(wdbp, "moved.c", {"moved_leaf.s"}),
	    "cross-parent setup creates the moved subtree");
	CHECK(write_comb(wdbp, "source.c", {"moved.c", "source_keep.s"}),
	    "cross-parent setup creates the source combination");
	CHECK(write_comb(wdbp, "destination.c", {"destination_keep.s"}),
	    "cross-parent setup creates the destination combination");
	struct ged_event_result *setup_result = ged_event_result_create();
	CHECK(ged_event_batch_end(model.ged(), setup_result) == GED_EVENT_OK,
	    "cross-parent setup publishes one event batch");
	ged_event_result_destroy(setup_result);

	QModelIndex source_index = find_child(&model, QModelIndex(), "source.c");
	QModelIndex destination_index = find_child(&model, QModelIndex(),
	    "destination.c");
	CHECK(source_index.isValid() && destination_index.isValid(),
	    "cross-parent setup exposes both combinations");
	if (model.canFetchMore(source_index))
	    model.fetchMore(source_index);
	if (model.canFetchMore(destination_index))
	    model.fetchMore(destination_index);
	QModelIndex moved_index = find_child(&model, source_index, "moved.c");
	CHECK(moved_index.isValid(), "source exposes the movable child");
	if (model.canFetchMore(moved_index))
	    model.fetchMore(moved_index);
	QModelIndex moved_leaf_index = find_child(&model, moved_index,
	    "moved_leaf.s");
	CHECK(moved_leaf_index.isValid(),
	    "movable subtree exposes its nested leaf");
	QPersistentModelIndex persistent_moved(moved_index);
	QPersistentModelIndex persistent_moved_leaf(moved_leaf_index);
	QSignalSpy rows_moved_spy(&model, &QAbstractItemModel::rowsMoved);
	const int inserted_before_move = rows_inserted_spy.count();
	const int removed_before_move = rows_removed_spy.count();

	CHECK(ged_event_batch_begin(model.ged()) == GED_EVENT_OK,
	    "cross-parent rewrite begins an event batch");
	CHECK(write_comb(wdbp, "source.c", {"source_keep.s"}),
	    "cross-parent rewrite removes the child from its source");
	CHECK(write_comb(wdbp, "destination.c",
	    {"destination_keep.s", "moved.c"}),
	    "cross-parent rewrite adds the child to its destination");
	struct ged_event_result *move_result = ged_event_result_create();
	CHECK(ged_event_batch_end(model.ged(), move_result) == GED_EVENT_OK,
	    "cross-parent rewrite publishes one event batch");
	ged_event_result_destroy(move_result);

	CHECK(reset_spy.count() == 0 && rows_moved_spy.count() == 1 &&
	    rows_removed_spy.count() == removed_before_move &&
	    rows_inserted_spy.count() == inserted_before_move,
	    "cross-parent rewrite publishes one row move without replacement");
	destination_index = find_child(&model, QModelIndex(), "destination.c");
	QModelIndex moved_destination = find_child(&model, destination_index,
	    "moved.c");
	CHECK(persistent_moved.isValid() && moved_destination.isValid() &&
	    QModelIndex(persistent_moved) == moved_destination,
	    "cross-parent move preserves the child persistent index");
	QModelIndex moved_leaf_destination = find_child(&model, moved_destination,
	    "moved_leaf.s");
	CHECK(persistent_moved_leaf.isValid() &&
	    moved_leaf_destination.isValid() &&
	    QModelIndex(persistent_moved_leaf) == moved_leaf_destination,
	    "cross-parent move preserves descendant persistent indexes");
	QgItem *moved_item = model.getItem(moved_leaf_destination);
	model.selectionClear();
	model.itemSelect(moved_item);
	(void)model.selectionCommit();
	CHECK(model.itemSelected(moved_item),
	    "cross-parent move refreshes semantic path identity");
    }

    CHECK(bu_file_delete(database_path), "could not remove model database");
    return 0;
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

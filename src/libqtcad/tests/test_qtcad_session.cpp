/*                 T E S T _ Q T C A D _ S E S S I O N . C P P
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

#include <QAbstractItemModelTester>
#include <QFile>
#include <QPersistentModelIndex>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include "ged.h"
#include "ged/database.h"
#include "ged/scene.h"
#include "ged/selection.h"
#include "qtcad/QgAttributesModel.h"
#include "qtcad/QgGedEventBatch.h"
#include "qtcad/QgModel.h"
#include "qtcad/QgSession.h"
#include "qtcad/QgSignalFlags.h"

class QgSessionTest : public QObject
{
    Q_OBJECT

private slots:
    void ownsGedContext();
    void publishesChanges();
    void batchesGedEvents();
    void suppliesModelContext();
    void reconcilesModelEvents();
    void tracksDuplicateSelectionOccurrences();
    void tracksSessionAttributes();
};

struct qg_event_observer_state {
    int calls = 0;
    size_t events = 0;
};

static void
qg_event_observer(struct ged *, const struct ged_event *, size_t event_count,
	const struct ged_event_result *, void *client_data)
{
    qg_event_observer_state *state =
	static_cast<qg_event_observer_state *>(client_data);
    state->calls++;
    state->events += event_count;
}

static bool
no_pending_db_changes(struct ged *gedp)
{
    struct ged_db_change_set *changes = ged_db_change_set_create(gedp, 0);
    if (!changes)
	return false;
    bool empty = !ged_db_change_set_revision(changes);
    ged_db_change_set_destroy(changes);
    return empty;
}

void
QgSessionTest::ownsGedContext()
{
    QgSession session;

    QVERIFY(session.ged() != nullptr);
    QVERIFY(session.dbip() == nullptr);
    QVERIFY(session.activeView() != nullptr);
    QCOMPARE(session.activeView(), session.ged()->ged_gvp);
    QVERIFY(!ged_db_change_available(session.ged()));
    QCOMPARE(session.ged()->new_cmd_forms, 1);
}

void
QgSessionTest::publishesChanges()
{
    QgSession session;
    int databaseChanges = 0;
    int viewChanges = 0;

    connect(&session, &QgSession::db_changed, this,
	[&databaseChanges](struct db_i *) { databaseChanges++; });
    connect(&session, &QgSession::view_changed, this,
	[&viewChanges](unsigned long long) { viewChanges++; });

    session.notifyDbChanged(nullptr);
    session.notifyViewChanged(1);

    QCOMPARE(databaseChanges, 1);
    QCOMPARE(viewChanges, 1);
}

void
QgSessionTest::batchesGedEvents()
{
    QgSession session;
    qg_event_observer_state state;
    ged_event_observer_token token = ged_event_observer_add(session.ged(),
	GED_EVENT_OBSERVER_POST_RECONCILE, qg_event_observer, &state);
    QVERIFY(token != 0);

    QgGedEventBatch batch(session.ged());
    QVERIFY(batch.started());
    struct ged_event events[2];
    ged_event_init(&events[0]);
    events[0].kind = GED_EVENT_OBJECT_ADDED;
    events[0].name = "first.s";
    ged_event_init(&events[1]);
    events[1].kind = GED_EVENT_OBJECT_ADDED;
    events[1].name = "second.s";
    QCOMPARE(ged_event_publish(session.ged(), events, 2, nullptr),
	GED_EVENT_OK);
    QCOMPARE(state.calls, 0);

    struct ged_event_result *result = ged_event_result_create();
    QVERIFY(result != nullptr);
    QCOMPARE(batch.end(result), GED_EVENT_OK);
    QCOMPARE(state.calls, 1);
    QCOMPARE(state.events, size_t(2));
    QCOMPARE(ged_event_result_event_count(result), size_t(2));
    QCOMPARE(batch.end(result), GED_EVENT_OK);
    QCOMPARE(state.calls, 1);
    ged_event_result_destroy(result);
    QVERIFY(ged_event_observer_remove(session.ged(), token));

    QgGedEventBatch unavailable(nullptr);
    QVERIFY(!unavailable.started());
    QCOMPARE(unavailable.end(), GED_EVENT_OK);
}

void
QgSessionTest::suppliesModelContext()
{
    QgModel model;

    QVERIFY(model.session() != nullptr);
    QCOMPARE(model.ged(), model.session()->ged());
    QCOMPARE(model.gedp, model.ged());
    QVERIFY(model.ged()->ged_gvp != nullptr);
}

void
QgSessionTest::reconcilesModelEvents()
{
    qRegisterMetaType<struct db_i *>("db_i*");
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QByteArray databasePath = QFile::encodeName(
	temporaryDirectory.filePath("events.g"));
    constexpr int databaseVersion = 5;
    struct db_i *created = db_create(databasePath.constData(),
	databaseVersion);
    QVERIFY(created != DBI_NULL);
    db_close(created);

    QgModel model;
    QAbstractItemModelTester tester(&model,
	QAbstractItemModelTester::FailureReportingMode::QtTest);
    QgItem *rootItem = model.root();
    QVERIFY(rootItem != nullptr);
    QVERIFY(model.allItems().empty());
    QVERIFY(&model.allItems() == model.items);
    const char *openArguments[] = {"open", databasePath.constData()};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 2, openArguments),
	BRLCAD_OK);
    QVERIFY(ged_db_change_available(model.ged()));
    QCOMPARE(model.rowCount(), 0);

    QSignalSpy changes(model.session(), &QgSession::db_changed);
    QSignalSpy viewChanges(model.session(), &QgSession::view_changed);
    QSignalSpy compatibilityViewChanges(&model, &QgModel::view_change);
    QSignalSpy modelResets(&model, &QAbstractItemModel::modelReset);
    QSignalSpy rowsInserted(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy rowsRemoved(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy rowsMoved(&model, &QAbstractItemModel::rowsMoved);
    QSignalSpy dataChanges(&model, &QAbstractItemModel::dataChanged);
    const char *makeArguments[] = {"make", "event.s", "sph"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 3, makeArguments),
	BRLCAD_OK);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(changes.count(), 1);
    QCOMPARE(model.allItems().size(), size_t(1));

    QModelIndex eventIndex = static_cast<QAbstractItemModel *>(&model)->index(
	0, 0);
    QVERIFY(eventIndex.isValid());
    QgItem *eventItem = model.getItem(eventIndex);
    QCOMPARE(eventItem->model(), &model);
    QCOMPARE(eventItem->parent(), model.root());
    QVERIFY(eventItem->instanceHash() != 0);
    QCOMPARE(QString(eventItem->displayName()), QString("event.s"));
    QCOMPARE(eventItem->directoryEntry(),
	db_lookup(model.ged()->dbip, "event.s", LOOKUP_QUIET));
    QCOMPARE(eventItem->childCount(), 0);
    QVERIFY(eventItem->childItems().empty());
    QCOMPARE(model.allItems().size(), size_t(1));
    QCOMPARE(model.topItems().size(), size_t(1));
    QVERIFY(model.hasItem(eventItem));

    QVERIFY(no_pending_db_changes(model.ged()));
    QVERIFY(db_lookup(model.ged()->dbip, "event.s", LOOKUP_QUIET) !=
	RT_DIR_NULL);

    const char *makeKeepArguments[] = {"make", "keep.s", "sph"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 3,
	makeKeepArguments), BRLCAD_OK);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(changes.count(), 2);
    QCOMPARE(model.allItems().size(), size_t(2));

    QAbstractItemModel *abstractModel = &model;
    QModelIndex keepIndex;
    for (int row = 0; row < model.rowCount(); row++) {
	QModelIndex candidate = abstractModel->index(row, 0);
	if (model.data(candidate).toString() == QString("keep.s")) {
	    keepIndex = candidate;
	    break;
	}
    }
    QVERIFY(keepIndex.isValid());
    QgItem *keepItem = model.getItem(keepIndex);
    QPersistentModelIndex persistentKeepIndex(keepIndex);

    int insertedBeforeAttribute = rowsInserted.count();
    int removedBeforeAttribute = rowsRemoved.count();
    int changesBeforeAttribute = dataChanges.count();
    const char *attributeArguments[] = {
	"attr", "set", "keep.s", "test", "value"
    };
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 5,
	attributeArguments), BRLCAD_OK);
    QCOMPARE(changes.count(), 3);
    QCOMPARE(rowsInserted.count(), insertedBeforeAttribute);
    QCOMPARE(rowsRemoved.count(), removedBeforeAttribute);
    QVERIFY(dataChanges.count() > changesBeforeAttribute);
    QVERIFY(persistentKeepIndex.isValid());

    QCOMPARE(model.data(eventIndex, QgModel::DrawnDisplayRole).toInt(),
	int(GED_SCENE_PATH_NOT_DRAWN));
    int dataChangesBeforeDraw = dataChanges.count();
    QCOMPARE(model.draw("event.s"), BRLCAD_OK);
    QCOMPARE(viewChanges.count(), 1);
    QCOMPARE(compatibilityViewChanges.count(), 1);
    QCOMPARE(model.data(eventIndex, QgModel::DrawnDisplayRole).toInt(),
	int(GED_SCENE_PATH_DRAWN));
    QVERIFY(dataChanges.count() > dataChangesBeforeDraw);
    int dataChangesBeforeErase = dataChanges.count();
    QCOMPARE(model.erase("event.s"), BRLCAD_OK);
    QCOMPARE(viewChanges.count(), 2);
    QCOMPARE(compatibilityViewChanges.count(), 2);
    QCOMPARE(model.data(eventIndex, QgModel::DrawnDisplayRole).toInt(),
	int(GED_SCENE_PATH_NOT_DRAWN));
    QVERIFY(dataChanges.count() > dataChangesBeforeErase);

    const char *presentPaths[] = {"event.s"};
    struct ged_scene_draw_request sceneRequest;
    ged_scene_draw_request_init(&sceneRequest);
    sceneRequest.view = model.ged()->ged_gvp;
    sceneRequest.paths = presentPaths;
    sceneRequest.path_count = 1;
    sceneRequest.autoview = 1;
    struct ged_scene_result *sceneResult = ged_scene_result_create();
    QVERIFY(sceneResult != nullptr);
    int dataChangesBeforeFacadeDraw = dataChanges.count();
    QCOMPARE(ged_scene_draw(model.ged(), &sceneRequest, sceneResult),
	GED_SCENE_OK);
    QCOMPARE(ged_scene_result_changed(sceneResult), 1);
    ged_scene_result_destroy(sceneResult);
    QVERIFY(dataChanges.count() > dataChangesBeforeFacadeDraw);
    QCOMPARE(model.data(eventIndex, QgModel::DrawnDisplayRole).toInt(),
	int(GED_SCENE_PATH_DRAWN));
    QCOMPARE(viewChanges.count(), 2);
    QCOMPARE(compatibilityViewChanges.count(), 2);
    QCOMPARE(model.erase("event.s"), BRLCAD_OK);

    const char *groupArguments[] = {"g", "event.g", "event.s"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 3, groupArguments),
	BRLCAD_OK);
    QCOMPARE(changes.count(), 4);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.allItems().size(), size_t(2));
    QVERIFY(persistentKeepIndex.isValid());
    QCOMPARE(model.getItem(persistentKeepIndex), keepItem);

    QModelIndex groupIndex;
    for (int row = 0; row < model.rowCount(); row++) {
	QModelIndex candidate = abstractModel->index(row, 0);
	if (model.data(candidate).toString() == QString("event.g")) {
	    groupIndex = candidate;
	    break;
	}
    }
    QVERIFY(groupIndex.isValid());
    QCOMPARE(model.getItem(groupIndex)->availableChildCount(), size_t(1));
    QVERIFY(model.getItem(groupIndex)->childItems().empty());
    QVERIFY(model.canFetchMore(groupIndex));
    QCOMPARE(model.rowCount(groupIndex), 0);
    model.fetchMore(groupIndex);
    QCOMPARE(model.rowCount(groupIndex), 1);
    QVERIFY(!model.canFetchMore(groupIndex));
    QCOMPARE(model.allItems().size(), size_t(3));

    QModelIndex childIndex = abstractModel->index(0, 0, groupIndex);
    QVERIFY(childIndex.isValid());
    QgItem *groupItem = model.getItem(groupIndex);
    QgItem *childItem = model.getItem(childIndex);
    QPersistentModelIndex persistentChildIndex(childIndex);
    QCOMPARE(model.NodeIndex(groupItem), groupIndex);
    QCOMPARE(model.NodeIndex(childItem), childIndex);

    const char *duplicateArguments[] = {"g", "event.g", "event.s"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 3,
	duplicateArguments), BRLCAD_OK);
    QCOMPARE(changes.count(), 5);
    QCOMPARE(model.rowCount(groupIndex), 2);
    QCOMPARE(model.allItems().size(), size_t(4));
    QVERIFY(persistentChildIndex.isValid());
    QModelIndex duplicateIndex;
    for (int row = 0; row < model.rowCount(groupIndex); row++) {
	QModelIndex candidate = abstractModel->index(row, 0, groupIndex);
	if (model.getItem(candidate) != childItem) {
	    duplicateIndex = candidate;
	    break;
	}
    }
    QVERIFY(duplicateIndex.isValid());
    QPersistentModelIndex persistentDuplicateIndex(duplicateIndex);

    const char *removeArguments[] = {"comb", "event.g", "rm", "event.s"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 4, removeArguments),
	BRLCAD_OK);
    QCOMPARE(changes.count(), 6);
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.allItems().size(), size_t(3));
    QVERIFY(model.hasItem(groupItem));
    QVERIFY(!persistentChildIndex.isValid());
    QVERIFY(!persistentDuplicateIndex.isValid());
    QVERIFY(persistentKeepIndex.isValid());
    QCOMPARE(model.getItem(persistentKeepIndex), keepItem);

    const char *killArguments[] = {"kill", "event.g"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 2, killArguments),
	BRLCAD_OK);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(changes.count(), 7);
    QCOMPARE(model.allItems().size(), size_t(2));
    QVERIFY(!model.hasItem(groupItem));
    QVERIFY(persistentKeepIndex.isValid());
    QCOMPARE(model.getItem(persistentKeepIndex), keepItem);
    QVERIFY(no_pending_db_changes(model.ged()));
    QVERIFY(db_lookup(model.ged()->dbip, "event.g", LOOKUP_QUIET) ==
	RT_DIR_NULL);

    const char *renameArguments[] = {"mv", "keep.s", "renamed.s"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 3, renameArguments),
	BRLCAD_OK);
    QCOMPARE(changes.count(), 8);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(modelResets.count(), 1);
    QVERIFY(!persistentKeepIndex.isValid());
    QVERIFY(db_lookup(model.ged()->dbip, "renamed.s", LOOKUP_QUIET) !=
	RT_DIR_NULL);

    struct ged_event rebuildEvent;
    ged_event_init(&rebuildEvent);
    rebuildEvent.kind = GED_EVENT_BATCH_REBUILD;
    QCOMPARE(ged_event_publish(model.ged(), &rebuildEvent, 1, nullptr),
	GED_EVENT_OK);
    model.g_update(model.ged()->dbip);
    QCOMPARE(changes.count(), 9);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(modelResets.count(), 2);

    const char *closeArguments[] = {"close"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 1, closeArguments),
	BRLCAD_OK);
    QCOMPARE(changes.count(), 10);
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(model.allItems().empty());
    QCOMPARE(model.root(), rootItem);
    QCOMPARE(modelResets.count(), 3);
    QVERIFY(rowsInserted.count() > 0);
    QVERIFY(rowsRemoved.count() > 0);
    QVERIFY(rowsMoved.count() > 0);
}

void
QgSessionTest::tracksDuplicateSelectionOccurrences()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QByteArray databasePath = QFile::encodeName(
	temporaryDirectory.filePath("selection.g"));
    struct db_i *created = db_create(databasePath.constData(), 5);
    QVERIFY(created != DBI_NULL);
    db_close(created);

    QgModel model;
    QAbstractItemModel *abstractModel = &model;
    const char *openArguments[] = {"open", databasePath.constData()};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 2, openArguments),
	BRLCAD_OK);

    const char *makeArguments[] = {"make", "leaf.s", "sph"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 3, makeArguments),
	BRLCAD_OK);
    const char *innerArguments[] = {"g", "inner.g", "leaf.s"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 3, innerArguments),
	BRLCAD_OK);
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 3, innerArguments),
	BRLCAD_OK);
    const char *outerArguments[] = {"g", "outer.g", "inner.g"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 3, outerArguments),
	BRLCAD_OK);
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 3, outerArguments),
	BRLCAD_OK);

    QCOMPARE(model.rowCount(), 1);
    QModelIndex outerIndex = abstractModel->index(0, 0);
    QCOMPARE(model.data(outerIndex).toString(), QString("outer.g"));
    model.fetchMore(outerIndex);
    QCOMPARE(model.rowCount(outerIndex), 2);

    QModelIndex firstInnerIndex = abstractModel->index(0, 0, outerIndex);
    QModelIndex secondInnerIndex = abstractModel->index(1, 0, outerIndex);
    model.fetchMore(firstInnerIndex);
    QCOMPARE(model.rowCount(firstInnerIndex), 2);
    QCOMPARE(model.rowCount(secondInnerIndex), 0);

    QModelIndex firstLeafIndex = abstractModel->index(0, 0,
	firstInnerIndex);
    QModelIndex secondLeafIndex = abstractModel->index(1, 0,
	firstInnerIndex);
    QgItem *outerItem = model.getItem(outerIndex);
    QgItem *firstInnerItem = model.getItem(firstInnerIndex);
    QgItem *firstLeafItem = model.getItem(firstLeafIndex);
    QgItem *secondLeafItem = model.getItem(secondLeafIndex);
    QPersistentModelIndex persistentFirstLeaf(firstLeafIndex);
    QPersistentModelIndex persistentSecondLeaf(secondLeafIndex);
    QSignalSpy dataChanges(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy layoutChanges(&model, &QAbstractItemModel::layoutChanged);

    model.itemSelect(firstLeafItem);
    unsigned long long selectionFlags = model.selectionCommit();
    QVERIFY(selectionFlags & QG_VIEW_SELECT);
    QCOMPARE(model.selectionCount(), size_t(1));
    QVERIFY(model.itemSelected(firstLeafItem));
    QVERIFY(!model.itemSelected(secondLeafItem));
    QCOMPARE(model.data(firstLeafIndex, QgModel::SelectDisplayRole).toInt(),
	1);
    QCOMPARE(model.data(secondLeafIndex, QgModel::SelectDisplayRole).toInt(),
	0);
    QCOMPARE(model.itemHighlight(firstInnerItem), 1);
    QCOMPARE(model.itemHighlight(outerItem), 1);
    QVERIFY(dataChanges.count() > 0);
    QCOMPARE(layoutChanges.count(), 0);

    firstInnerItem->setOpen(true);
    QCOMPARE(model.itemHighlight(firstInnerItem), 0);
    firstInnerItem->setOpen(false);
    model.itemSelect(secondLeafItem);
    model.selectionCommit();
    QCOMPARE(model.selectionCount(), size_t(2));
    QVERIFY(model.itemSelected(firstLeafItem));
    QVERIFY(model.itemSelected(secondLeafItem));

    const char *clearDefault[] = {"select", "clear", "default"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 3, clearDefault),
	BRLCAD_OK);
    QCOMPARE(model.selectionCount(), size_t(0));
    QVERIFY(!model.itemSelected(firstLeafItem));
    QVERIFY(!model.itemSelected(secondLeafItem));

    model.itemSelect(firstLeafItem);
    model.itemSelect(secondLeafItem);
    model.selectionCommit();
    QCOMPARE(ged_selection_count(model.ged(), nullptr), size_t(2));

    model.setSelectionSetName("alternate");
    QCOMPARE(model.selectionCount(), size_t(0));
    model.itemSelect(firstLeafItem);
    model.selectionCommit();
    QCOMPARE(model.selectionCount(), size_t(1));
    QCOMPARE(ged_selection_count(model.ged(), nullptr), size_t(2));
    QCOMPARE(ged_selection_count(model.ged(), "alternate"), size_t(1));

    model.setSelectionSetName(nullptr);
    QCOMPARE(model.selectionCount(), size_t(2));
    QVERIFY(model.itemSelected(firstLeafItem));
    QVERIFY(model.itemSelected(secondLeafItem));

    model.fetchMore(secondInnerIndex);
    QCOMPARE(model.rowCount(secondInnerIndex), 2);
    for (int row = 0; row < model.rowCount(secondInnerIndex); row++) {
	QModelIndex leafIndex = abstractModel->index(row, 0, secondInnerIndex);
	QVERIFY(!model.itemSelected(model.getItem(leafIndex)));
    }

    const char *removeArguments[] = {"comb", "inner.g", "rm", "leaf.s"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 4, removeArguments),
	BRLCAD_OK);
    QCOMPARE(model.selectionCount(), size_t(0));
    QVERIFY(!persistentFirstLeaf.isValid());
    QVERIFY(!persistentSecondLeaf.isValid());
    QCOMPARE(model.rowCount(firstInnerIndex), 0);
    QCOMPARE(model.rowCount(secondInnerIndex), 0);
    QCOMPARE(layoutChanges.count(), 0);
}

void
QgSessionTest::tracksSessionAttributes()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QByteArray databasePath = QFile::encodeName(
	temporaryDirectory.filePath("attributes.g"));
    constexpr int databaseVersion = 5;
    struct db_i *created = db_create(databasePath.constData(),
	databaseVersion);
    QVERIFY(created != DBI_NULL);
    db_close(created);

    QgModel model;
    const char *openArguments[] = {"open", databasePath.constData()};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 2, openArguments),
	BRLCAD_OK);
    const char *makeArguments[] = {"make", "attribute.s", "sph"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 3, makeArguments),
	BRLCAD_OK);
    const char *firstAttribute[] = {
	"attr", "set", "attribute.s", "test", "first"
    };
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 5, firstAttribute),
	BRLCAD_OK);
    QgAttributesModel attributes(model.session(), nullptr, nullptr, 0, 1);
    QAbstractItemModelTester tester(&attributes,
	QAbstractItemModelTester::FailureReportingMode::QtTest);
    QModelIndex objectIndex = static_cast<QAbstractItemModel *>(&model)->index(
	0, 0);
    QVERIFY(objectIndex.isValid());
    attributes.refresh(objectIndex);
    QCOMPARE(attributes.rowCount(QModelIndex()), 1);
    QCOMPARE(attributes.data(attributes.index(0, 0, QModelIndex())).toString(),
	QString("test"));
    QCOMPARE(attributes.data(attributes.index(0, 1, QModelIndex())).toString(),
	QString("first"));

    QSignalSpy resets(&attributes, &QAbstractItemModel::modelReset);
    const char *secondAttribute[] = {
	"attr", "set", "attribute.s", "test", "second"
    };
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 5, secondAttribute),
	BRLCAD_OK);
    QCOMPARE(resets.count(), 1);
    QCOMPARE(attributes.data(attributes.index(0, 1, QModelIndex())).toString(),
	QString("second"));

    model.session()->notifyViewChanged(1);
    QCOMPARE(resets.count(), 1);

    const char *killArguments[] = {"kill", "attribute.s"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 2, killArguments),
	BRLCAD_OK);
    QCOMPARE(resets.count(), 2);
    QCOMPARE(attributes.rowCount(QModelIndex()), 0);

    const char *closeArguments[] = {"closedb"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 1, closeArguments),
	BRLCAD_OK);
    QCOMPARE(resets.count(), 3);
    QCOMPARE(attributes.rowCount(QModelIndex()), 0);
}

QTEST_APPLESS_MAIN(QgSessionTest)

#include "test_qtcad_session.moc"

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

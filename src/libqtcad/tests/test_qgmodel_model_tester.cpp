/*  T E S T _ Q G M O D E L _ M O D E L _ T E S T E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file test_qgmodel_model_tester.cpp
 *
 * Phase 8 model test coverage for QgModel:
 *  - headless QApplication initialization
 *  - QAbstractItemModelTester invariant checks
 *  - QSignalSpy coverage for model-open and layout-change signals
 */

#include "common.h"

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <QAbstractItemModelTester>
#include <QApplication>
#include <QPersistentModelIndex>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/process.h"
#include "bu/time.h"

#include "ged.h"
#include "ged/bsg_ged_draw.h"
#include "ged/event_txn.h"
#include "qtcad/QgModel.h"
#include "wdb.h"

static int g_fail = 0;

#define TCHECK(cond, msg) \
    do { \
	if (!(cond)) { \
	    bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, (msg)); \
	    g_fail++; \
	} else { \
	    bu_log("OK   %s\n", (msg)); \
	} \
    } while (0)

static QModelIndex
find_top_level_item(QAbstractItemModel *model, const char *name)
{
    if (!model || !name)
	return QModelIndex();

    const int rows = model->rowCount(QModelIndex());
    const QString wanted = QString::fromUtf8(name);
    for (int row = 0; row < rows; row++) {
	QModelIndex idx = model->index(row, 0, QModelIndex());
	if (idx.isValid() && model->data(idx, Qt::DisplayRole).toString() == wanted)
	    return idx;
    }

    return QModelIndex();
}

static QModelIndex
find_child_item(QAbstractItemModel *model, const QModelIndex &parent, const char *name)
{
    if (!model || !parent.isValid() || !name)
	return QModelIndex();

    const int rows = model->rowCount(parent);
    const QString wanted = QString::fromUtf8(name);
    for (int row = 0; row < rows; row++) {
	QModelIndex idx = model->index(row, 0, parent);
	if (idx.isValid() && model->data(idx, Qt::DisplayRole).toString() == wanted)
	    return idx;
    }

    return QModelIndex();
}

static int
count_child_items_with_prefix(QAbstractItemModel *model, const QModelIndex &parent, const char *name)
{
    if (!model || !parent.isValid() || !name)
	return 0;

    int count = 0;
    const int rows = model->rowCount(parent);
    const QString wanted = QString::fromUtf8(name);
    for (int row = 0; row < rows; row++) {
	QModelIndex idx = model->index(row, 0, parent);
	if (idx.isValid() && model->data(idx, Qt::DisplayRole).toString().startsWith(wanted))
	    count++;
    }

    return count;
}

static QString
child_name_at(QAbstractItemModel *model, const QModelIndex &parent, int row)
{
    if (!model || !parent.isValid() || row < 0 || row >= model->rowCount(parent))
	return QString();

    QModelIndex idx = model->index(row, 0, parent);
    return idx.isValid() ? model->data(idx, Qt::DisplayRole).toString() :
	QString();
}

static bool
model_has_item_path(QgModel *model, const char *path)
{
    if (!model || !path)
	return false;

    std::string wanted(path);
    for (QgItem *item : model->allItems()) {
	if (item && model->item_path(item) == wanted)
	    return true;
    }

    return false;
}

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    if (!qgetenv("QT_QPA_PLATFORM").size())
	qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);

    if (argc < 2) {
	bu_log("Usage: %s <path-to-moss.g>\n", argv[0]);
	return 1;
    }
    const char *g_path = argv[1];

    qRegisterMetaType<QgItem *>("QgItem *");

    QgModel model(nullptr, g_path);
    QAbstractItemModel *amodel = &model;

    TCHECK(amodel->columnCount(QModelIndex()) == 1, "QgModel reports one display column");

    int top_rows = amodel->rowCount(QModelIndex());
    TCHECK(top_rows > 0, "QgModel has top-level rows for moss.g");

    QModelIndex lazy_all_idx = find_top_level_item(amodel, "all.g");
    TCHECK(lazy_all_idx.isValid(), "QgModel can resolve unopened top-level all.g item");
    if (lazy_all_idx.isValid()) {
	TCHECK(amodel->rowCount(lazy_all_idx) == 0,
		"QgModel reports zero loaded rows for unopened branch");
	TCHECK(amodel->hasChildren(lazy_all_idx),
		"QgModel reports indexed children for unopened branch");
	TCHECK(amodel->canFetchMore(lazy_all_idx),
		"QgModel can fetch unopened branch children");
    }

    QAbstractItemModelTester tester(amodel, QAbstractItemModelTester::FailureReportingMode::Fatal);

    struct ged *gedp = model.ged();
    TCHECK(gedp != nullptr && gedp->ged_gvp != nullptr,
	    "QgModel exposes a GED with a current view");
    if (gedp && gedp->ged_gvp) {
	QModelIndex all_idx = find_top_level_item(amodel, "all.g");
	TCHECK(all_idx.isValid(), "QgModel can resolve top-level all.g item");
	QSignalSpy drawn_spy(amodel, &QAbstractItemModel::dataChanged);
	TCHECK(drawn_spy.isValid(), "dataChanged spy is valid");
	int before_draw_signals = drawn_spy.count();
	TCHECK(model.draw("all.g") == BRLCAD_OK,
		"QgModel draw action succeeds through retained draw state");
	TCHECK(ged_draw_path_state(gedp, gedp->ged_gvp, "all.g", -1) == 1,
		"QgModel draw action updates canonical drawn state");
	TCHECK(drawn_spy.count() > before_draw_signals,
		"QgModel draw action emits dataChanged for drawn role");
	if (all_idx.isValid())
	    TCHECK(amodel->data(all_idx, DrawnDisplayRole).toInt() == 1,
		    "QgModel DrawnDisplayRole reads canonical drawn state after draw");
	int before_erase_signals = drawn_spy.count();
	TCHECK(model.erase("all.g") == BRLCAD_OK,
		"QgModel erase action succeeds through retained draw state");
	TCHECK(ged_draw_path_state(gedp, gedp->ged_gvp, "all.g", -1) == 0,
		"QgModel erase action updates canonical drawn state");
	TCHECK(drawn_spy.count() > before_erase_signals,
		"QgModel erase action emits dataChanged for drawn role");
	if (all_idx.isValid())
	    TCHECK(amodel->data(all_idx, DrawnDisplayRole).toInt() == 0,
		    "QgModel DrawnDisplayRole reads canonical drawn state after erase");
	int before_noop_erase_signals = drawn_spy.count();
	TCHECK(model.erase("all.g") == BRLCAD_OK,
		"QgModel no-op erase action succeeds");
	TCHECK(drawn_spy.count() == before_noop_erase_signals,
		"QgModel no-op erase does not emit drawn role dataChanged");
	if (all_idx.isValid() && amodel->canFetchMore(all_idx))
	    amodel->fetchMore(all_idx);
	QCoreApplication::processEvents();
	QTest::qWait(1);
	QModelIndex box_idx = find_child_item(amodel, all_idx, "box.r");
	TCHECK(box_idx.isValid(), "QgModel can resolve child box.r item for partial drawn state");
	if (all_idx.isValid())
	    TCHECK(!amodel->canFetchMore(all_idx),
		    "QgModel does not advertise fetchMore for an already populated branch");
	QPersistentModelIndex all_persistent(all_idx);
	QPersistentModelIndex box_persistent(box_idx);
	TCHECK(all_persistent.isValid() && box_persistent.isValid(),
		"QgModel persistent indexes are valid before draw-state-only updates");
	int before_child_draw_signals = drawn_spy.count();
	model.resetNotificationStats();
	TCHECK(model.draw("all.g/box.r") == BRLCAD_OK,
		"QgModel child-path draw succeeds through retained draw state");
	TCHECK(ged_draw_path_state(gedp, gedp->ged_gvp, "all.g/box.r", -1) == 1,
		"QgModel child-path draw updates canonical child drawn state");
	TCHECK(ged_draw_path_state(gedp, gedp->ged_gvp, "all.g", -1) == 2,
		"QgModel child-path draw updates canonical parent partial drawn state");
	TCHECK(drawn_spy.count() > before_child_draw_signals,
		"QgModel child-path draw emits dataChanged for drawn role");
	QgModel::NotificationStats child_draw_stats = model.notificationStats();
	TCHECK(child_draw_stats.path_fallback_scans == 0,
		"QgModel child-path draw notification avoids full item path scans");
	TCHECK(child_draw_stats.path_queries > 0,
		"QgModel child-path draw notification uses path-hash queries");
	TCHECK(child_draw_stats.path_candidates > 0 &&
		child_draw_stats.path_candidates < model.allItems().size(),
		"QgModel child-path draw notification visits indexed candidates");
	if (all_idx.isValid())
	    TCHECK(amodel->data(all_idx, DrawnDisplayRole).toInt() == 2,
		    "QgModel DrawnDisplayRole reads canonical partial drawn state for parent");
	if (box_idx.isValid())
	    TCHECK(amodel->data(box_idx, DrawnDisplayRole).toInt() == 1,
		    "QgModel DrawnDisplayRole reads canonical drawn state for child path");
	TCHECK(model.erase("all.g/box.r") == BRLCAD_OK,
		"QgModel child-path erase succeeds through retained draw state");
	TCHECK(ged_draw_path_state(gedp, gedp->ged_gvp, "all.g/box.r", -1) == 0,
		"QgModel child-path erase clears canonical child drawn state");
	TCHECK(all_persistent.isValid() && box_persistent.isValid(),
		"QgModel persistent indexes survive child draw/erase data updates");
	const char *run_draw_av[3] = {"draw", "all.g", NULL};
	int before_run_draw_signals = drawn_spy.count();
	TCHECK(model.run_cmd(gedp->ged_result_str, 2, run_draw_av) == BRLCAD_OK,
		"QgModel run_cmd draw succeeds");
	TCHECK(ged_draw_path_state(gedp, gedp->ged_gvp, "all.g", -1) == 1,
		"QgModel run_cmd draw updates canonical drawn state");
	TCHECK(drawn_spy.count() > before_run_draw_signals,
		"QgModel run_cmd draw emits drawn role dataChanged");
	if (all_idx.isValid())
	    TCHECK(amodel->data(all_idx, DrawnDisplayRole).toInt() == 1,
		    "QgModel DrawnDisplayRole reads canonical drawn state after run_cmd draw");
	const char *redraw_av[3] = {"redraw", "all.g", NULL};
	int before_redraw_signals = drawn_spy.count();
	TCHECK(model.run_cmd(gedp->ged_result_str, 2, redraw_av) == BRLCAD_OK,
		"QgModel run_cmd redraw succeeds");
	TCHECK(ged_draw_path_state(gedp, gedp->ged_gvp, "all.g", -1) == 1,
		"QgModel run_cmd redraw preserves canonical drawn state");
	TCHECK(drawn_spy.count() > before_redraw_signals,
		"QgModel run_cmd redraw emits drawn role dataChanged");
	if (all_idx.isValid())
	    TCHECK(amodel->data(all_idx, DrawnDisplayRole).toInt() == 1,
		    "QgModel DrawnDisplayRole reads canonical drawn state after redraw");
	const char *zap_av[2] = {"zap", NULL};
	int before_zap_signals = drawn_spy.count();
	TCHECK(model.run_cmd(gedp->ged_result_str, 1, zap_av) == BRLCAD_OK,
		"QgModel run_cmd zap succeeds");
	TCHECK(ged_draw_path_state(gedp, gedp->ged_gvp, "all.g", -1) == 0,
		"QgModel run_cmd zap clears canonical drawn state");
	TCHECK(drawn_spy.count() > before_zap_signals,
		"QgModel run_cmd zap emits drawn role dataChanged");
	if (all_idx.isValid())
	    TCHECK(amodel->data(all_idx, DrawnDisplayRole).toInt() == 0,
		    "QgModel DrawnDisplayRole reads canonical drawn state after zap");
	before_run_draw_signals = drawn_spy.count();
	TCHECK(model.run_cmd(gedp->ged_result_str, 2, run_draw_av) == BRLCAD_OK,
		"QgModel run_cmd draw after zap succeeds");
	TCHECK(drawn_spy.count() > before_run_draw_signals,
		"QgModel run_cmd draw after zap emits drawn role dataChanged");
	const char *run_erase_av[3] = {"erase", "all.g", NULL};
	int before_run_erase_signals = drawn_spy.count();
	TCHECK(model.run_cmd(gedp->ged_result_str, 2, run_erase_av) == BRLCAD_OK,
		"QgModel run_cmd erase succeeds");
	TCHECK(ged_draw_path_state(gedp, gedp->ged_gvp, "all.g", -1) == 0,
		"QgModel run_cmd erase updates canonical drawn state");
	TCHECK(drawn_spy.count() > before_run_erase_signals,
		"QgModel run_cmd erase emits drawn role dataChanged");
	if (all_idx.isValid())
	    TCHECK(amodel->data(all_idx, DrawnDisplayRole).toInt() == 0,
		    "QgModel DrawnDisplayRole reads canonical drawn state after run_cmd erase");
	TCHECK(all_persistent.isValid() && box_persistent.isValid(),
		"QgModel persistent indexes survive draw, redraw, zap, and erase data updates");
    }

    {
	std::string event_tmp = std::string("qgmodel_event_tmp_") +
	    std::to_string((long long)bu_pid()) + ".g";
	bu_file_delete(event_tmp.c_str());

	bool copied = false;
	{
	    std::ifstream src(g_path, std::ios::binary);
	    std::ofstream dst(event_tmp.c_str(), std::ios::binary | std::ios::trunc);
	    if (src && dst) {
		dst << src.rdbuf();
		copied = !src.bad() && dst.good();
	    }
	}
	TCHECK(copied, "test creates private moss.g copy for event bridge");

	if (copied) {
	    QgModel event_model(nullptr, event_tmp.c_str());
	    QAbstractItemModel *event_amodel = &event_model;
	    std::unique_ptr<QAbstractItemModelTester> event_tester(
		    new QAbstractItemModelTester(event_amodel,
			QAbstractItemModelTester::FailureReportingMode::Fatal));
	    QSignalSpy db_spy(&event_model, SIGNAL(mdl_changed_db(void *)));
	    QSignalSpy reset_spy(&event_model, SIGNAL(modelReset()));
	    QSignalSpy rows_inserted_spy(event_amodel,
		    SIGNAL(rowsInserted(QModelIndex,int,int)));
	    QSignalSpy rows_removed_spy(event_amodel,
		    SIGNAL(rowsRemoved(QModelIndex,int,int)));
	    QSignalSpy rows_moved_spy(event_amodel,
		    SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)));
	    QSignalSpy event_data_spy(event_amodel,
		    &QAbstractItemModel::dataChanged);
	    TCHECK(db_spy.isValid(), "event bridge mdl_changed_db spy is valid");
	    TCHECK(reset_spy.isValid(), "event bridge modelReset spy is valid");
	    TCHECK(rows_inserted_spy.isValid(),
		    "event bridge rowsInserted spy is valid");
	    TCHECK(rows_removed_spy.isValid(),
		    "event bridge rowsRemoved spy is valid");
	    TCHECK(rows_moved_spy.isValid(),
		    "event bridge rowsMoved spy is valid");
	    TCHECK(event_data_spy.isValid(),
		    "event bridge dataChanged spy is valid");
	    TCHECK(find_top_level_item(event_amodel, "all.g").isValid(),
		    "event bridge model starts with all.g top-level item");

	    struct ged *event_gedp = event_model.ged();
	    QModelIndex all_event_idx = find_top_level_item(event_amodel, "all.g");
	    if (all_event_idx.isValid() && event_amodel->canFetchMore(all_event_idx))
		event_amodel->fetchMore(all_event_idx);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);

	    int before_box_mod_db_signals = db_spy.count();
	    int before_box_mod_reset_signals = reset_spy.count();
	    int before_box_mod_data_signals = event_data_spy.count();
	    event_model.resetNotificationStats();
	    struct ged_event_txn_result box_mod_result;
	    ged_event_txn_result_init(&box_mod_result);
	    TCHECK(event_gedp && ged_event_notify_object_modified(event_gedp,
		    "box.r", 0, &box_mod_result) >= 0,
		    "event bridge direct object modified event succeeds");
	    ged_event_txn_result_free(&box_mod_result);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(db_spy.count() > before_box_mod_db_signals,
		    "QgModel bridges direct object modified event into db changed signal");
	    TCHECK(reset_spy.count() == before_box_mod_reset_signals,
		    "QgModel direct object modified event avoids hierarchy reset");
	    TCHECK(event_data_spy.count() > before_box_mod_data_signals,
		    "QgModel direct object modified event emits targeted dataChanged");
	    QgModel::NotificationStats box_mod_stats =
		event_model.notificationStats();
	    TCHECK(box_mod_stats.path_fallback_scans == 0,
		    "QgModel direct object modified notification avoids full item path scans");
	    TCHECK(box_mod_stats.path_queries > 0,
		    "QgModel direct object modified notification uses path-hash queries");
	    TCHECK(box_mod_stats.path_candidates > 0 &&
		    box_mod_stats.path_candidates < event_model.allItems().size(),
		    "QgModel direct object modified notification visits indexed candidates");

	    int before_box_attr_db_signals = db_spy.count();
	    int before_box_attr_reset_signals = reset_spy.count();
	    int before_box_attr_data_signals = event_data_spy.count();
	    event_model.resetNotificationStats();
	    const char *box_attr_av[6] = {"attr", "set", "box.r",
		"qgmodel_attr", "1", NULL};
	    TCHECK(event_gedp && ged_exec(event_gedp, 5, box_attr_av) ==
		    BRLCAD_OK,
		    "event bridge attr set command succeeds");
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(db_spy.count() > before_box_attr_db_signals,
		    "QgModel bridges attr set command into db changed signal");
	    TCHECK(reset_spy.count() == before_box_attr_reset_signals,
		    "QgModel attr set command avoids hierarchy reset");
	    TCHECK(event_data_spy.count() > before_box_attr_data_signals,
		    "QgModel attr set command emits targeted dataChanged");
	    QgModel::NotificationStats box_attr_stats =
		event_model.notificationStats();
	    TCHECK(box_attr_stats.path_fallback_scans == 0,
		    "QgModel attr set command avoids full item path scans");
	    TCHECK(box_attr_stats.path_queries > 0,
		    "QgModel attr set command uses path-hash queries");
	    TCHECK(box_attr_stats.path_candidates > 0 &&
		    box_attr_stats.path_candidates < event_model.allItems().size(),
		    "QgModel attr set command visits indexed candidates");

	    int before_box_mat_db_signals = db_spy.count();
	    int before_box_mat_reset_signals = reset_spy.count();
	    int before_box_mat_data_signals = event_data_spy.count();
	    event_model.resetNotificationStats();
	    struct ged_event_txn_result box_mat_result;
	    ged_event_txn_result_init(&box_mat_result);
	    TCHECK(event_gedp && ged_event_notify_object_material_changed(
		    event_gedp, "box.r", &box_mat_result) >= 0,
		    "event bridge direct object material event succeeds");
	    TCHECK(std::string(bu_vls_cstr(&box_mat_result.affected_names)).
		    find("box.r") != std::string::npos,
		    "object material event reports affected path names");
	    ged_event_txn_result_free(&box_mat_result);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(db_spy.count() > before_box_mat_db_signals,
		    "QgModel bridges direct object material event into db changed signal");
	    TCHECK(reset_spy.count() == before_box_mat_reset_signals,
		    "QgModel direct object material event avoids hierarchy reset");
	    TCHECK(event_data_spy.count() > before_box_mat_data_signals,
		    "QgModel direct object material event emits targeted dataChanged");
	    QgModel::NotificationStats box_mat_stats =
		event_model.notificationStats();
	    TCHECK(box_mat_stats.path_fallback_scans == 0,
		    "QgModel direct object material notification avoids full item path scans");
	    TCHECK(box_mat_stats.path_queries > 0,
		    "QgModel direct object material notification uses path-hash queries");
	    TCHECK(box_mat_stats.path_candidates > 0 &&
		    box_mat_stats.path_candidates < event_model.allItems().size(),
		    "QgModel direct object material notification visits indexed candidates");

	    const char *direct_add_name = "_qgmodel_librt_event_add.s";
	    struct rt_wdb *event_wdbp = event_gedp ?
		wdb_dbopen(event_gedp->dbip, RT_WDB_TYPE_DB_DEFAULT) : RT_WDB_NULL;
	    point_t direct_add_center = {204.0, 0.0, 0.0};
	    int before_direct_add_db_signals = db_spy.count();
	    int before_direct_add_reset_signals = reset_spy.count();
	    int before_direct_add_insert_signals = rows_inserted_spy.count();
	    TCHECK(event_wdbp != RT_WDB_NULL,
		    "event bridge opens wdb handle for direct librt mutation");
	    TCHECK(event_gedp && ged_event_batch_begin(event_gedp) > 0,
		    "event bridge starts GedEventTxn batch for direct librt mutation");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, direct_add_name,
		    direct_add_center, 1.0) == 0,
		    "event bridge direct librt add succeeds");
	    struct ged_event_txn_result direct_add_result;
	    ged_event_txn_result_init(&direct_add_result);
	    TCHECK(event_gedp && ged_event_batch_end(event_gedp,
		    &direct_add_result) >= 0,
		    "event bridge ends GedEventTxn batch for direct librt mutation");
	    TCHECK(direct_add_result.db_index_status > 0,
		    "event bridge direct librt add queues db index reconciliation");
	    ged_event_txn_result_free(&direct_add_result);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(db_spy.count() > before_direct_add_db_signals,
		    "QgModel bridges batched direct librt add into db changed signal");
	    TCHECK(reset_spy.count() == before_direct_add_reset_signals,
		    "QgModel bridges batched direct librt add without hierarchy reset");
	    TCHECK(rows_inserted_spy.count() > before_direct_add_insert_signals,
		    "QgModel bridges batched direct librt add into targeted row insert");
	    TCHECK(find_top_level_item(event_amodel, direct_add_name).isValid(),
		    "event bridge model exposes directly added librt object");

	    const char *reuse_leaf = "_qgmodel_reuse_leaf.s";
	    const char *reuse_extra = "_qgmodel_reuse_extra.s";
	    const char *reuse_child = "_qgmodel_reuse_child.c";
	    const char *reuse_parent_a = "_qgmodel_reuse_parent_a.c";
	    const char *reuse_parent_b = "_qgmodel_reuse_parent_b.c";
	    const char *dup_leaf = "_qgmodel_dup_leaf.s";
	    const char *dup_extra = "_qgmodel_dup_extra.s";
	    const char *dup_parent = "_qgmodel_dup_parent.c";
	    point_t reuse_leaf_center = {208.0, 0.0, 0.0};
	    point_t reuse_extra_center = {212.0, 0.0, 0.0};
	    point_t dup_leaf_center = {216.0, 0.0, 0.0};
	    point_t dup_extra_center = {220.0, 0.0, 0.0};
	    int before_reuse_setup_reset_signals = reset_spy.count();
	    int before_reuse_setup_insert_signals = rows_inserted_spy.count();
	    TCHECK(event_gedp && ged_event_batch_begin(event_gedp) > 0,
		    "event bridge starts reused/duplicate hierarchy setup batch");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, reuse_leaf,
		    reuse_leaf_center, 1.0) == 0,
		    "event bridge creates reused comb leaf");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, reuse_extra,
		    reuse_extra_center, 1.0) == 0,
		    "event bridge creates reused comb extra leaf");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, dup_leaf,
		    dup_leaf_center, 1.0) == 0,
		    "event bridge creates duplicate child leaf");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, dup_extra,
		    dup_extra_center, 1.0) == 0,
		    "event bridge creates duplicate child extra leaf");
	    {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		TCHECK(mk_addmember(reuse_leaf, &wm.l, NULL, WMOP_UNION) != NULL,
			"event bridge queues reused child comb member");
		TCHECK(event_wdbp && mk_comb(event_wdbp, reuse_child, &wm.l, 0,
			NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
			"event bridge creates reused child comb");
	    }
	    {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		TCHECK(mk_addmember(reuse_child, &wm.l, NULL, WMOP_UNION) != NULL,
			"event bridge queues reused child in parent A");
		TCHECK(event_wdbp && mk_comb(event_wdbp, reuse_parent_a, &wm.l,
			0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
			"event bridge creates reused parent A");
	    }
	    {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		TCHECK(mk_addmember(reuse_child, &wm.l, NULL, WMOP_UNION) != NULL,
			"event bridge queues reused child in parent B");
		TCHECK(event_wdbp && mk_comb(event_wdbp, reuse_parent_b, &wm.l,
			0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
			"event bridge creates reused parent B");
	    }
	    {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		TCHECK(mk_addmember(dup_leaf, &wm.l, NULL, WMOP_UNION) != NULL,
			"event bridge queues duplicate parent first child");
		TCHECK(mk_addmember(dup_leaf, &wm.l, NULL, WMOP_UNION) != NULL,
			"event bridge queues duplicate parent second child");
		TCHECK(event_wdbp && mk_comb(event_wdbp, dup_parent, &wm.l, 0,
			NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
			"event bridge creates duplicate parent comb");
	    }
	    struct ged_event_txn_result reuse_setup_result;
	    ged_event_txn_result_init(&reuse_setup_result);
	    TCHECK(event_gedp && ged_event_batch_end(event_gedp,
		    &reuse_setup_result) >= 0,
		    "event bridge ends reused/duplicate hierarchy setup batch");
	    ged_event_txn_result_free(&reuse_setup_result);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(reset_spy.count() == before_reuse_setup_reset_signals,
		    "QgModel creates reused/duplicate hierarchy without hierarchy reset");
	    TCHECK(rows_inserted_spy.count() > before_reuse_setup_insert_signals,
		    "QgModel creates reused/duplicate hierarchy through row inserts");

	    QModelIndex reuse_parent_a_idx = find_top_level_item(event_amodel,
		    reuse_parent_a);
	    QModelIndex reuse_parent_b_idx = find_top_level_item(event_amodel,
		    reuse_parent_b);
	    QModelIndex dup_parent_idx = find_top_level_item(event_amodel,
		    dup_parent);
	    TCHECK(reuse_parent_a_idx.isValid() && reuse_parent_b_idx.isValid() &&
		    dup_parent_idx.isValid(),
		    "event bridge exposes reused and duplicate synthetic parents");
	    if (reuse_parent_a_idx.isValid() && event_amodel->canFetchMore(reuse_parent_a_idx))
		event_amodel->fetchMore(reuse_parent_a_idx);
	    if (reuse_parent_b_idx.isValid() && event_amodel->canFetchMore(reuse_parent_b_idx))
		event_amodel->fetchMore(reuse_parent_b_idx);
	    if (dup_parent_idx.isValid() && event_amodel->canFetchMore(dup_parent_idx))
		event_amodel->fetchMore(dup_parent_idx);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    QModelIndex reuse_child_a_idx = find_child_item(event_amodel,
		    reuse_parent_a_idx, reuse_child);
	    QModelIndex reuse_child_b_idx = find_child_item(event_amodel,
		    reuse_parent_b_idx, reuse_child);
	    TCHECK(reuse_child_a_idx.isValid() && reuse_child_b_idx.isValid(),
		    "event bridge exposes reused child under both parents");
	    TCHECK(count_child_items_with_prefix(event_amodel, dup_parent_idx,
		    dup_leaf) == 2,
		    "event bridge exposes duplicate child instances as two model rows");
	    if (reuse_child_a_idx.isValid() && event_amodel->canFetchMore(reuse_child_a_idx))
		event_amodel->fetchMore(reuse_child_a_idx);
	    if (reuse_child_b_idx.isValid() && event_amodel->canFetchMore(reuse_child_b_idx))
		event_amodel->fetchMore(reuse_child_b_idx);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    QPersistentModelIndex reuse_parent_a_persistent(reuse_parent_a_idx);
	    QPersistentModelIndex reuse_parent_b_persistent(reuse_parent_b_idx);
	    QPersistentModelIndex dup_parent_persistent(dup_parent_idx);

	    const char *reuse_add_av[4] = {"g", reuse_child, reuse_extra, NULL};
	    int before_reuse_add_reset_signals = reset_spy.count();
	    int before_reuse_add_insert_signals = rows_inserted_spy.count();
	    int before_reuse_add_remove_signals = rows_removed_spy.count();
	    TCHECK(event_gedp && ged_exec(event_gedp, 3, reuse_add_av) == BRLCAD_OK,
		    "direct GED add succeeds for reused child comb");
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(reset_spy.count() == before_reuse_add_reset_signals,
		    "QgModel reused comb edit avoids hierarchy reset");
	    TCHECK(rows_inserted_spy.count() > before_reuse_add_insert_signals &&
		    rows_removed_spy.count() > before_reuse_add_remove_signals,
		    "QgModel reused comb edit updates root and both child instances with grouped deltas");
	    TCHECK(reuse_parent_a_persistent.isValid() &&
		    reuse_parent_b_persistent.isValid(),
		    "QgModel preserves reused parent persistent indexes across reused comb edit");
	    reuse_parent_a_idx = find_top_level_item(event_amodel, reuse_parent_a);
	    reuse_parent_b_idx = find_top_level_item(event_amodel, reuse_parent_b);
	    reuse_child_a_idx = find_child_item(event_amodel, reuse_parent_a_idx,
		    reuse_child);
	    reuse_child_b_idx = find_child_item(event_amodel, reuse_parent_b_idx,
		    reuse_child);
	    TCHECK(find_child_item(event_amodel, reuse_child_a_idx,
		    reuse_extra).isValid() &&
		    find_child_item(event_amodel, reuse_child_b_idx,
		    reuse_extra).isValid(),
		    "QgModel reused comb edit inserts new child under both loaded reused instances");

	    const char *dup_add_av[4] = {"g", dup_parent, dup_extra, NULL};
	    int before_dup_add_reset_signals = reset_spy.count();
	    int before_dup_add_insert_signals = rows_inserted_spy.count();
	    int before_dup_add_remove_signals = rows_removed_spy.count();
	    TCHECK(event_gedp && ged_exec(event_gedp, 3, dup_add_av) == BRLCAD_OK,
		    "direct GED add succeeds for duplicate child parent");
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(reset_spy.count() == before_dup_add_reset_signals,
		    "QgModel duplicate-instance parent edit avoids hierarchy reset");
	    TCHECK(rows_inserted_spy.count() > before_dup_add_insert_signals &&
		    rows_removed_spy.count() > before_dup_add_remove_signals,
		    "QgModel duplicate-instance parent edit updates root and parent with grouped deltas");
	    TCHECK(dup_parent_persistent.isValid(),
		    "QgModel preserves duplicate parent persistent index across grouped edit");
	    dup_parent_idx = find_top_level_item(event_amodel, dup_parent);
	    TCHECK(count_child_items_with_prefix(event_amodel, dup_parent_idx,
		    dup_leaf) == 2 &&
		    find_child_item(event_amodel, dup_parent_idx, dup_extra).isValid(),
		    "QgModel duplicate-instance parent edit keeps duplicate rows and inserts new child");

	    {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		TCHECK(mk_addmember(dup_leaf, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues duplicate reorder first duplicate");
		TCHECK(mk_addmember(dup_extra, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues duplicate reorder middle child");
		TCHECK(mk_addmember(dup_leaf, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues duplicate reorder second duplicate");
		event_model.resetHierarchyDeltaStats();
		int before_dup_reorder_reset_signals = reset_spy.count();
		int before_dup_reorder_insert_signals =
		    rows_inserted_spy.count();
		int before_dup_reorder_remove_signals =
		    rows_removed_spy.count();
		int before_dup_reorder_move_signals =
		    rows_moved_spy.count();
		TCHECK(event_gedp && ged_event_batch_begin(event_gedp) > 0,
			"event bridge starts duplicate reorder rewrite batch");
		TCHECK(event_wdbp && mk_comb(event_wdbp, dup_parent, &wm.l,
			0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
			"event bridge rewrites duplicate reorder parent");
		struct ged_event_txn_result dup_reorder_result;
		ged_event_txn_result_init(&dup_reorder_result);
		TCHECK(event_gedp && ged_event_batch_end(event_gedp,
			&dup_reorder_result) >= 0,
			"event bridge ends duplicate reorder rewrite batch");
		ged_event_txn_result_free(&dup_reorder_result);
		QCoreApplication::processEvents();
		QTest::qWait(1);
		QgModel::HierarchyDeltaStats dup_reorder_stats =
		    event_model.hierarchyDeltaStats();
		dup_parent_idx = find_top_level_item(event_amodel, dup_parent);
		bu_log("BENCH qgmodel_delta_duplicate_reorder "
			"attempts=%llu applied=%llu fallbacks=%llu "
			"inserts=%llu removes=%llu moves=%llu "
			"insert_ranges=%llu remove_ranges=%llu move_ranges=%llu "
			"order='%s','%s','%s'\n",
			(unsigned long long)dup_reorder_stats.attempts,
			(unsigned long long)dup_reorder_stats.applied,
			(unsigned long long)dup_reorder_stats.reset_fallbacks,
			(unsigned long long)dup_reorder_stats.inserted_rows,
			(unsigned long long)dup_reorder_stats.removed_rows,
			(unsigned long long)dup_reorder_stats.moved_rows,
			(unsigned long long)dup_reorder_stats.inserted_row_ranges,
			(unsigned long long)dup_reorder_stats.removed_row_ranges,
			(unsigned long long)dup_reorder_stats.moved_row_ranges,
			child_name_at(event_amodel, dup_parent_idx, 0)
			    .toUtf8().constData(),
			child_name_at(event_amodel, dup_parent_idx, 1)
			    .toUtf8().constData(),
			child_name_at(event_amodel, dup_parent_idx, 2)
			    .toUtf8().constData());
		TCHECK(reset_spy.count() == before_dup_reorder_reset_signals,
			"QgModel duplicate sibling reorder avoids hierarchy reset");
		TCHECK(rows_inserted_spy.count() ==
			before_dup_reorder_insert_signals &&
			rows_removed_spy.count() ==
			before_dup_reorder_remove_signals &&
			rows_moved_spy.count() > before_dup_reorder_move_signals,
			"QgModel duplicate sibling reorder emits row move");
		TCHECK(dup_reorder_stats.applied > 0 &&
			dup_reorder_stats.reset_fallbacks == 0 &&
			dup_reorder_stats.moved_rows > 0,
			"QgModel duplicate sibling reorder records row-move delta");
	    }
	    dup_parent_idx = find_top_level_item(event_amodel, dup_parent);
	    TCHECK(dup_parent_persistent.isValid(),
		    "QgModel duplicate sibling reorder preserves parent persistent index");
	    TCHECK(child_name_at(event_amodel, dup_parent_idx, 0) ==
		    QString::fromUtf8(dup_leaf) &&
		    child_name_at(event_amodel, dup_parent_idx, 1) ==
		    QString::fromUtf8(dup_extra) &&
		    child_name_at(event_amodel, dup_parent_idx, 2)
		    .startsWith(QString::fromUtf8(dup_leaf)),
		    "QgModel duplicate sibling reorder exposes rewritten ambiguous order");

	    const char *invalid_parent = "_qgmodel_invalid_parent.c";
	    const char *invalid_missing = "_qgmodel_invalid_missing.s";
	    const char *invalid_extra = "_qgmodel_invalid_extra.s";
	    point_t invalid_extra_center = {224.0, 0.0, 0.0};
	    int before_invalid_setup_reset_signals = reset_spy.count();
	    int before_invalid_setup_insert_signals = rows_inserted_spy.count();
	    TCHECK(event_gedp && ged_event_batch_begin(event_gedp) > 0,
		    "event bridge starts invalid-reference setup batch");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, invalid_extra,
		    invalid_extra_center, 1.0) == 0,
		    "event bridge creates invalid-reference extra leaf");
	    {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		TCHECK(mk_addmember(invalid_missing, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues invalid-reference child");
		TCHECK(event_wdbp && mk_comb(event_wdbp, invalid_parent,
			&wm.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0,
			0) == 0,
			"event bridge creates invalid-reference parent comb");
	    }
	    struct ged_event_txn_result invalid_setup_result;
	    ged_event_txn_result_init(&invalid_setup_result);
	    TCHECK(event_gedp && ged_event_batch_end(event_gedp,
		    &invalid_setup_result) >= 0,
		    "event bridge ends invalid-reference setup batch");
	    ged_event_txn_result_free(&invalid_setup_result);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(reset_spy.count() == before_invalid_setup_reset_signals,
		    "QgModel creates invalid-reference hierarchy without hierarchy reset");
	    TCHECK(rows_inserted_spy.count() > before_invalid_setup_insert_signals,
		    "QgModel creates invalid-reference hierarchy through row inserts");
	    QModelIndex invalid_parent_idx = find_top_level_item(event_amodel,
		    invalid_parent);
	    TCHECK(invalid_parent_idx.isValid(),
		    "event bridge exposes invalid-reference parent");
	    if (invalid_parent_idx.isValid() &&
		    event_amodel->canFetchMore(invalid_parent_idx))
		event_amodel->fetchMore(invalid_parent_idx);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(find_child_item(event_amodel, invalid_parent_idx,
		    invalid_missing).isValid(),
		    "event bridge exposes invalid-reference child row");
	    QPersistentModelIndex invalid_parent_persistent(invalid_parent_idx);

	    const char *invalid_add_av[4] = {"g", invalid_parent,
		invalid_extra, NULL};
	    event_model.resetHierarchyDeltaStats();
	    int before_invalid_add_reset_signals = reset_spy.count();
	    int before_invalid_add_insert_signals = rows_inserted_spy.count();
	    int before_invalid_add_remove_signals = rows_removed_spy.count();
	    TCHECK(event_gedp && ged_exec(event_gedp, 3, invalid_add_av) == BRLCAD_OK,
		    "direct GED add succeeds for invalid-reference parent");
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    QgModel::HierarchyDeltaStats invalid_add_stats =
		event_model.hierarchyDeltaStats();
	    TCHECK(reset_spy.count() == before_invalid_add_reset_signals,
		    "QgModel invalid-reference append avoids hierarchy reset");
	    TCHECK(rows_inserted_spy.count() > before_invalid_add_insert_signals &&
		    rows_removed_spy.count() > before_invalid_add_remove_signals,
		    "QgModel invalid-reference append emits grouped row deltas");
	    TCHECK(invalid_add_stats.applied > 0 &&
		    invalid_add_stats.reset_fallbacks == 0,
		    "QgModel invalid-reference append applies grouped delta");
	    TCHECK(invalid_parent_persistent.isValid(),
		    "QgModel preserves invalid-reference parent persistent index across append");
	    invalid_parent_idx = find_top_level_item(event_amodel,
		    invalid_parent);
	    TCHECK(find_child_item(event_amodel, invalid_parent_idx,
		    invalid_missing).isValid() &&
		    find_child_item(event_amodel, invalid_parent_idx,
			invalid_extra).isValid(),
		    "QgModel invalid-reference append preserves missing child and inserts extra child");

	    std::string invalid_path = std::string(invalid_parent) + "/" +
		invalid_missing;
	    const char *invalid_rm_av[3] = {"rm", invalid_path.c_str(), NULL};
	    event_model.resetHierarchyDeltaStats();
	    int before_invalid_rm_reset_signals = reset_spy.count();
	    int before_invalid_rm_remove_signals = rows_removed_spy.count();
	    TCHECK(event_gedp && ged_exec(event_gedp, 2, invalid_rm_av) == BRLCAD_OK,
		    "direct GED rm succeeds for invalid-reference child path");
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    QgModel::HierarchyDeltaStats invalid_rm_stats =
		event_model.hierarchyDeltaStats();
	    TCHECK(reset_spy.count() == before_invalid_rm_reset_signals,
		    "QgModel invalid-reference removal avoids hierarchy reset");
	    TCHECK(rows_removed_spy.count() > before_invalid_rm_remove_signals,
		    "QgModel invalid-reference removal emits targeted row remove");
	    TCHECK(invalid_rm_stats.applied > 0 &&
		    invalid_rm_stats.reset_fallbacks == 0,
		    "QgModel invalid-reference removal applies grouped delta");
	    TCHECK(invalid_parent_persistent.isValid(),
		    "QgModel preserves invalid-reference parent persistent index across removal");
	    invalid_parent_idx = find_top_level_item(event_amodel,
		    invalid_parent);
	    TCHECK(!find_child_item(event_amodel, invalid_parent_idx,
		    invalid_missing).isValid() &&
		    find_child_item(event_amodel, invalid_parent_idx,
			invalid_extra).isValid(),
		    "QgModel invalid-reference removal retires missing child row only");

	    const int targeted_parent_count = 8;
	    const char *targeted_extra = "_qgmodel_targeted_extra.s";
	    const char *targeted_empty_parent =
		"_qgmodel_targeted_empty_parent.c";
	    const char *targeted_empty_extra =
		"_qgmodel_targeted_empty_extra.s";
	    std::vector<std::string> targeted_parents;
	    std::vector<std::string> targeted_leaves;
	    targeted_parents.reserve(targeted_parent_count);
	    targeted_leaves.reserve(targeted_parent_count);
	    int before_targeted_setup_reset_signals = reset_spy.count();
	    int before_targeted_setup_insert_signals =
		rows_inserted_spy.count();
	    TCHECK(event_gedp && ged_event_batch_begin(event_gedp) > 0,
		    "event bridge starts targeted-delta setup batch");
	    for (int i = 0; i < targeted_parent_count; i++) {
		char parent_name[64];
		char leaf_name[64];
		std::snprintf(parent_name, sizeof(parent_name),
			"_qgmodel_targeted_parent_%02d.c", i);
		std::snprintf(leaf_name, sizeof(leaf_name),
			"_qgmodel_targeted_leaf_%02d.s", i);
		targeted_parents.emplace_back(parent_name);
		targeted_leaves.emplace_back(leaf_name);
		point_t center = {260.0 + (fastf_t)i, 4.0, 0.0};
		TCHECK(event_wdbp && mk_sph(event_wdbp,
			targeted_leaves.back().c_str(), center, 1.0) == 0,
			"event bridge creates targeted-delta leaf");
	    }
	    {
		point_t center = {272.0, 4.0, 0.0};
		TCHECK(event_wdbp && mk_sph(event_wdbp, targeted_extra,
			center, 1.0) == 0,
			"event bridge creates targeted-delta extra leaf");
	    }
	    {
		point_t center = {276.0, 4.0, 0.0};
		TCHECK(event_wdbp && mk_sph(event_wdbp,
			targeted_empty_extra, center, 1.0) == 0,
			"event bridge creates targeted empty-branch extra leaf");
	    }
	    for (int i = 0; i < targeted_parent_count; i++) {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		TCHECK(mk_addmember(targeted_leaves[(size_t)i].c_str(),
			&wm.l, NULL, WMOP_UNION) != NULL,
			"event bridge queues targeted-delta child");
		TCHECK(event_wdbp && mk_comb(event_wdbp,
			targeted_parents[(size_t)i].c_str(), &wm.l, 0,
			NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
			"event bridge creates targeted-delta parent");
	    }
	    {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		TCHECK(event_wdbp && mk_comb(event_wdbp,
			targeted_empty_parent, &wm.l, 0, NULL, NULL, NULL,
			0, 0, 0, 0, 0, 0, 0) == 0,
			"event bridge creates targeted empty parent");
	    }
	    struct ged_event_txn_result targeted_setup_result;
	    ged_event_txn_result_init(&targeted_setup_result);
	    TCHECK(event_gedp && ged_event_batch_end(event_gedp,
		    &targeted_setup_result) >= 0,
		    "event bridge ends targeted-delta setup batch");
	    ged_event_txn_result_free(&targeted_setup_result);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(reset_spy.count() == before_targeted_setup_reset_signals,
		    "QgModel creates targeted-delta hierarchy without hierarchy reset");
	    TCHECK(rows_inserted_spy.count() >
		    before_targeted_setup_insert_signals,
		    "QgModel creates targeted-delta hierarchy through row inserts");
	    std::vector<QPersistentModelIndex> targeted_persistents;
	    targeted_persistents.reserve(targeted_parent_count);
	    for (int i = 0; i < targeted_parent_count; i++) {
		QModelIndex idx = find_top_level_item(event_amodel,
			targeted_parents[(size_t)i].c_str());
		TCHECK(idx.isValid(),
			"event bridge exposes targeted-delta parent");
		if (idx.isValid() && event_amodel->canFetchMore(idx))
		    event_amodel->fetchMore(idx);
		targeted_persistents.emplace_back(idx);
	    }
	    QModelIndex targeted_empty_idx = find_top_level_item(event_amodel,
		    targeted_empty_parent);
	    TCHECK(targeted_empty_idx.isValid(),
		    "event bridge exposes targeted empty parent");
	    if (targeted_empty_idx.isValid())
		event_model.item_expanded(targeted_empty_idx);
	    QPersistentModelIndex targeted_empty_persistent(
		    targeted_empty_idx);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);

	    event_model.resetHierarchyDeltaStats();
	    const char *targeted_add_av[4] = {"g",
		targeted_parents[0].c_str(), targeted_extra, NULL};
	    int before_targeted_add_reset_signals = reset_spy.count();
	    int before_targeted_add_insert_signals =
		rows_inserted_spy.count();
	    int before_targeted_add_remove_signals =
		rows_removed_spy.count();
	    TCHECK(event_gedp && ged_exec(event_gedp, 3,
		    targeted_add_av) == BRLCAD_OK,
		    "direct GED add succeeds for targeted-delta parent");
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    QgModel::HierarchyDeltaStats targeted_delta_stats =
		event_model.hierarchyDeltaStats();
	    bu_log("BENCH qgmodel_delta_targeted attempts=%llu "
		    "applied=%llu fallbacks=%llu parents=%llu rows=%llu "
		    "inserts=%llu removes=%llu expanded_parents=%d\n",
		    (unsigned long long)targeted_delta_stats.attempts,
		    (unsigned long long)targeted_delta_stats.applied,
		    (unsigned long long)targeted_delta_stats.reset_fallbacks,
		    (unsigned long long)targeted_delta_stats.planned_parents,
		    (unsigned long long)targeted_delta_stats.planned_child_rows,
		    (unsigned long long)targeted_delta_stats.inserted_rows,
		    (unsigned long long)targeted_delta_stats.removed_rows,
		    targeted_parent_count);
	    TCHECK(reset_spy.count() == before_targeted_add_reset_signals,
		    "QgModel targeted parent edit avoids hierarchy reset");
	    TCHECK(rows_inserted_spy.count() >
		    before_targeted_add_insert_signals &&
		    rows_removed_spy.count() >
		    before_targeted_add_remove_signals,
		    "QgModel targeted parent edit emits grouped row deltas");
	    TCHECK(targeted_delta_stats.applied > 0 &&
		    targeted_delta_stats.reset_fallbacks == 0,
		    "QgModel targeted parent edit applies structural delta");
	    TCHECK(targeted_delta_stats.planned_parents <= 3,
		    "QgModel targeted parent edit plans only root and affected loaded parent");
	    QModelIndex targeted_parent_idx = find_top_level_item(event_amodel,
		    targeted_parents[0].c_str());
	    TCHECK(targeted_parent_idx.isValid() &&
		    find_child_item(event_amodel, targeted_parent_idx,
			targeted_extra).isValid(),
		    "QgModel targeted parent edit inserts child on affected parent");
	    for (const QPersistentModelIndex &pidx : targeted_persistents)
		TCHECK(pidx.isValid(),
			"QgModel targeted parent edit preserves expanded parent persistent indexes");

	    event_model.resetHierarchyDeltaStats();
	    const char *targeted_empty_add_av[4] = {"g",
		targeted_empty_parent, targeted_empty_extra, NULL};
	    int before_targeted_empty_reset_signals = reset_spy.count();
	    int before_targeted_empty_insert_signals =
		rows_inserted_spy.count();
	    int before_targeted_empty_remove_signals =
		rows_removed_spy.count();
	    TCHECK(event_gedp && ged_exec(event_gedp, 3,
		    targeted_empty_add_av) == BRLCAD_OK,
		    "direct GED add succeeds for opened empty parent");
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    QgModel::HierarchyDeltaStats targeted_empty_stats =
		event_model.hierarchyDeltaStats();
	    TCHECK(reset_spy.count() ==
		    before_targeted_empty_reset_signals,
		    "QgModel opened empty parent edit avoids hierarchy reset");
	    TCHECK(rows_inserted_spy.count() >
		    before_targeted_empty_insert_signals &&
		    rows_removed_spy.count() >
		    before_targeted_empty_remove_signals,
		    "QgModel opened empty parent edit emits row deltas");
	    TCHECK(targeted_empty_stats.applied > 0 &&
		    targeted_empty_stats.reset_fallbacks == 0 &&
		    targeted_empty_stats.planned_parents <= 3,
		    "QgModel opened empty parent edit uses targeted structural delta");
	    targeted_empty_idx = QModelIndex(targeted_empty_persistent);
	    TCHECK(targeted_empty_idx.isValid() &&
		    event_amodel->rowCount(targeted_empty_idx) == 1 &&
		    find_child_item(event_amodel, targeted_empty_idx,
			targeted_empty_extra).isValid(),
		    "QgModel opened empty parent receives first child row");

	    const char *reorder_parent = "_qgmodel_reorder_parent.c";
	    const char *reorder_a = "_qgmodel_reorder_a.s";
	    const char *reorder_b = "_qgmodel_reorder_b.s";
	    point_t reorder_a_center = {228.0, 0.0, 0.0};
	    point_t reorder_b_center = {232.0, 0.0, 0.0};
	    int before_reorder_setup_reset_signals = reset_spy.count();
	    int before_reorder_setup_insert_signals = rows_inserted_spy.count();
	    TCHECK(event_gedp && ged_event_batch_begin(event_gedp) > 0,
		    "event bridge starts reorder row-move setup batch");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, reorder_a,
		    reorder_a_center, 1.0) == 0,
		    "event bridge creates reorder row-move first leaf");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, reorder_b,
		    reorder_b_center, 1.0) == 0,
		    "event bridge creates reorder row-move second leaf");
	    {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		TCHECK(mk_addmember(reorder_a, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues reorder row-move first child");
		TCHECK(mk_addmember(reorder_b, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues reorder row-move second child");
		TCHECK(event_wdbp && mk_comb(event_wdbp, reorder_parent,
			&wm.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0,
			0) == 0,
			"event bridge creates reorder row-move parent");
	    }
	    struct ged_event_txn_result reorder_setup_result;
	    ged_event_txn_result_init(&reorder_setup_result);
	    TCHECK(event_gedp && ged_event_batch_end(event_gedp,
		    &reorder_setup_result) >= 0,
		    "event bridge ends reorder row-move setup batch");
	    ged_event_txn_result_free(&reorder_setup_result);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(reset_spy.count() == before_reorder_setup_reset_signals,
		    "QgModel creates reorder row-move hierarchy without hierarchy reset");
	    TCHECK(rows_inserted_spy.count() > before_reorder_setup_insert_signals,
		    "QgModel creates reorder row-move hierarchy through row inserts");
	    QModelIndex reorder_parent_idx = find_top_level_item(event_amodel,
		    reorder_parent);
	    TCHECK(reorder_parent_idx.isValid(),
		    "event bridge exposes reorder fallback parent");
	    if (reorder_parent_idx.isValid() &&
		    event_amodel->canFetchMore(reorder_parent_idx))
		event_amodel->fetchMore(reorder_parent_idx);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(child_name_at(event_amodel, reorder_parent_idx, 0) ==
		    QString::fromUtf8(reorder_a) &&
		    child_name_at(event_amodel, reorder_parent_idx, 1) ==
		    QString::fromUtf8(reorder_b),
		    "event bridge loads reorder row-move children in initial order");
	    QModelIndex reorder_a_idx = find_child_item(event_amodel,
		    reorder_parent_idx, reorder_a);
	    QModelIndex reorder_b_idx = find_child_item(event_amodel,
		    reorder_parent_idx, reorder_b);
	    QPersistentModelIndex reorder_parent_persistent(reorder_parent_idx);
	    QPersistentModelIndex reorder_a_persistent(reorder_a_idx);
	    QPersistentModelIndex reorder_b_persistent(reorder_b_idx);
	    {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		TCHECK(mk_addmember(reorder_b, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues reordered second child first");
		TCHECK(mk_addmember(reorder_a, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues reordered first child second");
		event_model.resetHierarchyDeltaStats();
		int before_reorder_reset_signals = reset_spy.count();
		int before_reorder_move_signals = rows_moved_spy.count();
		TCHECK(event_gedp && ged_event_batch_begin(event_gedp) > 0,
			"event bridge starts reorder row-move rewrite batch");
		TCHECK(event_wdbp && mk_comb(event_wdbp, reorder_parent,
			&wm.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0,
			0) == 0,
			"event bridge rewrites reorder row-move parent");
		struct ged_event_txn_result reorder_result;
		ged_event_txn_result_init(&reorder_result);
		TCHECK(event_gedp && ged_event_batch_end(event_gedp,
			&reorder_result) >= 0,
			"event bridge ends reorder row-move rewrite batch");
		ged_event_txn_result_free(&reorder_result);
		QCoreApplication::processEvents();
		QTest::qWait(1);
		QgModel::HierarchyDeltaStats reorder_stats =
		    event_model.hierarchyDeltaStats();
		TCHECK(reset_spy.count() == before_reorder_reset_signals,
			"QgModel sibling reorder avoids hierarchy reset");
		TCHECK(rows_moved_spy.count() > before_reorder_move_signals,
			"QgModel sibling reorder emits row move");
		TCHECK(reorder_stats.attempts > 0 &&
			reorder_stats.applied > 0 &&
			reorder_stats.reset_fallbacks == 0 &&
			reorder_stats.moved_rows > 0,
			"QgModel sibling reorder records row-move delta");
	    }
	    reorder_parent_idx = find_top_level_item(event_amodel,
		    reorder_parent);
	    if (reorder_parent_idx.isValid() &&
		    event_amodel->canFetchMore(reorder_parent_idx))
		event_amodel->fetchMore(reorder_parent_idx);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(child_name_at(event_amodel, reorder_parent_idx, 0) ==
		    QString::fromUtf8(reorder_b) &&
		    child_name_at(event_amodel, reorder_parent_idx, 1) ==
		    QString::fromUtf8(reorder_a),
		    "QgModel sibling reorder row move exposes reordered children");
	    TCHECK(reorder_parent_persistent.isValid() &&
		    reorder_a_persistent.isValid() &&
		    reorder_b_persistent.isValid(),
		    "QgModel sibling reorder row move preserves persistent indexes");

	    const char *mixed_reorder_parent =
		"_qgmodel_mixed_reorder_parent.c";
	    const char *mixed_reorder_a = "_qgmodel_mixed_reorder_a.s";
	    const char *mixed_reorder_b = "_qgmodel_mixed_reorder_b.s";
	    const char *mixed_reorder_c = "_qgmodel_mixed_reorder_c.s";
	    const char *mixed_reorder_d = "_qgmodel_mixed_reorder_d.s";
	    point_t mixed_reorder_a_center = {236.0, 0.0, 0.0};
	    point_t mixed_reorder_b_center = {238.0, 0.0, 0.0};
	    point_t mixed_reorder_c_center = {240.0, 0.0, 0.0};
	    point_t mixed_reorder_d_center = {242.0, 0.0, 0.0};
	    int before_mixed_reorder_setup_reset_signals = reset_spy.count();
	    int before_mixed_reorder_setup_insert_signals =
		rows_inserted_spy.count();
	    TCHECK(event_gedp && ged_event_batch_begin(event_gedp) > 0,
		    "event bridge starts mixed reorder setup batch");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, mixed_reorder_a,
		    mixed_reorder_a_center, 1.0) == 0,
		    "event bridge creates mixed reorder first leaf");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, mixed_reorder_b,
		    mixed_reorder_b_center, 1.0) == 0,
		    "event bridge creates mixed reorder second leaf");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, mixed_reorder_c,
		    mixed_reorder_c_center, 1.0) == 0,
		    "event bridge creates mixed reorder third leaf");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, mixed_reorder_d,
		    mixed_reorder_d_center, 1.0) == 0,
		    "event bridge creates mixed reorder inserted leaf");
	    {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		TCHECK(mk_addmember(mixed_reorder_a, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues mixed reorder first child");
		TCHECK(mk_addmember(mixed_reorder_b, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues mixed reorder second child");
		TCHECK(mk_addmember(mixed_reorder_c, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues mixed reorder third child");
		TCHECK(event_wdbp && mk_comb(event_wdbp,
			mixed_reorder_parent, &wm.l, 0, NULL, NULL, NULL,
			0, 0, 0, 0, 0, 0, 0) == 0,
			"event bridge creates mixed reorder parent");
	    }
	    struct ged_event_txn_result mixed_reorder_setup_result;
	    ged_event_txn_result_init(&mixed_reorder_setup_result);
	    TCHECK(event_gedp && ged_event_batch_end(event_gedp,
		    &mixed_reorder_setup_result) >= 0,
		    "event bridge ends mixed reorder setup batch");
	    ged_event_txn_result_free(&mixed_reorder_setup_result);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(reset_spy.count() ==
		    before_mixed_reorder_setup_reset_signals,
		    "QgModel creates mixed reorder hierarchy without hierarchy reset");
	    TCHECK(rows_inserted_spy.count() >
		    before_mixed_reorder_setup_insert_signals,
		    "QgModel creates mixed reorder hierarchy through row inserts");
	    QModelIndex mixed_reorder_parent_idx = find_top_level_item(
		    event_amodel, mixed_reorder_parent);
	    TCHECK(mixed_reorder_parent_idx.isValid(),
		    "event bridge exposes mixed reorder parent");
	    if (mixed_reorder_parent_idx.isValid() &&
		    event_amodel->canFetchMore(mixed_reorder_parent_idx))
		event_amodel->fetchMore(mixed_reorder_parent_idx);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    QModelIndex mixed_reorder_b_idx = find_child_item(event_amodel,
		    mixed_reorder_parent_idx, mixed_reorder_b);
	    QModelIndex mixed_reorder_c_idx = find_child_item(event_amodel,
		    mixed_reorder_parent_idx, mixed_reorder_c);
	    QPersistentModelIndex mixed_reorder_parent_persistent(
		    mixed_reorder_parent_idx);
	    QPersistentModelIndex mixed_reorder_b_persistent(
		    mixed_reorder_b_idx);
	    QPersistentModelIndex mixed_reorder_c_persistent(
		    mixed_reorder_c_idx);
	    TCHECK(child_name_at(event_amodel, mixed_reorder_parent_idx, 0) ==
		    QString::fromUtf8(mixed_reorder_a) &&
		    child_name_at(event_amodel, mixed_reorder_parent_idx, 1) ==
		    QString::fromUtf8(mixed_reorder_b) &&
		    child_name_at(event_amodel, mixed_reorder_parent_idx, 2) ==
		    QString::fromUtf8(mixed_reorder_c),
		    "event bridge loads mixed reorder children in initial order");
	    {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		TCHECK(mk_addmember(mixed_reorder_c, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues mixed reorder moved child first");
		TCHECK(mk_addmember(mixed_reorder_b, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues mixed reorder retained child");
		TCHECK(mk_addmember(mixed_reorder_d, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues mixed reorder inserted child");
		event_model.resetHierarchyDeltaStats();
		int before_mixed_reorder_reset_signals = reset_spy.count();
		int before_mixed_reorder_insert_signals =
		    rows_inserted_spy.count();
		int before_mixed_reorder_remove_signals =
		    rows_removed_spy.count();
		int before_mixed_reorder_move_signals =
		    rows_moved_spy.count();
		TCHECK(event_gedp && ged_event_batch_begin(event_gedp) > 0,
			"event bridge starts mixed reorder rewrite batch");
		TCHECK(event_wdbp && mk_comb(event_wdbp,
			mixed_reorder_parent, &wm.l, 0, NULL, NULL, NULL,
			0, 0, 0, 0, 0, 0, 0) == 0,
			"event bridge rewrites mixed reorder parent");
		struct ged_event_txn_result mixed_reorder_result;
		ged_event_txn_result_init(&mixed_reorder_result);
		TCHECK(event_gedp && ged_event_batch_end(event_gedp,
			&mixed_reorder_result) >= 0,
			"event bridge ends mixed reorder rewrite batch");
		ged_event_txn_result_free(&mixed_reorder_result);
		QCoreApplication::processEvents();
		QTest::qWait(1);
		QgModel::HierarchyDeltaStats mixed_reorder_stats =
		    event_model.hierarchyDeltaStats();
		TCHECK(reset_spy.count() ==
			before_mixed_reorder_reset_signals,
			"QgModel mixed reorder avoids hierarchy reset");
		TCHECK(rows_inserted_spy.count() >
			before_mixed_reorder_insert_signals &&
			rows_removed_spy.count() >
			before_mixed_reorder_remove_signals &&
			rows_moved_spy.count() >
			before_mixed_reorder_move_signals,
			"QgModel mixed reorder emits insert, remove, and move rows");
		TCHECK(mixed_reorder_stats.applied > 0 &&
			mixed_reorder_stats.reset_fallbacks == 0 &&
			mixed_reorder_stats.inserted_rows > 0 &&
			mixed_reorder_stats.removed_rows > 0 &&
			mixed_reorder_stats.moved_rows > 0,
			"QgModel mixed reorder records grouped row operations");
	    }
	    mixed_reorder_parent_idx = find_top_level_item(event_amodel,
		    mixed_reorder_parent);
	    TCHECK(child_name_at(event_amodel, mixed_reorder_parent_idx, 0) ==
		    QString::fromUtf8(mixed_reorder_c) &&
		    child_name_at(event_amodel, mixed_reorder_parent_idx, 1) ==
		    QString::fromUtf8(mixed_reorder_b) &&
		    child_name_at(event_amodel, mixed_reorder_parent_idx, 2) ==
		    QString::fromUtf8(mixed_reorder_d),
		    "QgModel mixed reorder exposes rewritten child order");
	    TCHECK(mixed_reorder_parent_persistent.isValid() &&
		    mixed_reorder_b_persistent.isValid() &&
		    mixed_reorder_c_persistent.isValid(),
		    "QgModel mixed reorder preserves retained persistent indexes");

	    const char *cross_parent_source =
		"_qgmodel_cross_parent_source.c";
	    const char *cross_parent_dest =
		"_qgmodel_cross_parent_dest.c";
	    const char *cross_parent_child =
		"_qgmodel_cross_parent_child.s";
	    const char *cross_parent_source_keep =
		"_qgmodel_cross_parent_source_keep.s";
	    const char *cross_parent_dest_keep =
		"_qgmodel_cross_parent_dest_keep.s";
	    point_t cross_parent_child_center = {248.0, 0.0, 0.0};
	    point_t cross_parent_source_keep_center = {252.0, 0.0, 0.0};
	    point_t cross_parent_dest_keep_center = {256.0, 0.0, 0.0};
	    int before_cross_parent_setup_reset_signals = reset_spy.count();
	    int before_cross_parent_setup_insert_signals =
		rows_inserted_spy.count();
	    TCHECK(event_gedp && ged_event_batch_begin(event_gedp) > 0,
		    "event bridge starts cross-parent move setup batch");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, cross_parent_child,
		    cross_parent_child_center, 1.0) == 0,
		    "event bridge creates cross-parent moved child");
	    TCHECK(event_wdbp && mk_sph(event_wdbp,
		    cross_parent_source_keep,
		    cross_parent_source_keep_center, 1.0) == 0,
		    "event bridge creates cross-parent source retained child");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, cross_parent_dest_keep,
		    cross_parent_dest_keep_center, 1.0) == 0,
		    "event bridge creates cross-parent destination retained child");
	    {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		TCHECK(mk_addmember(cross_parent_child, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues cross-parent moved child in source");
		TCHECK(mk_addmember(cross_parent_source_keep, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues cross-parent retained source child");
		TCHECK(event_wdbp && mk_comb(event_wdbp,
			cross_parent_source, &wm.l, 0, NULL, NULL, NULL,
			0, 0, 0, 0, 0, 0, 0) == 0,
			"event bridge creates cross-parent source");
	    }
	    {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		TCHECK(mk_addmember(cross_parent_dest_keep, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues cross-parent retained destination child");
		TCHECK(event_wdbp && mk_comb(event_wdbp, cross_parent_dest,
			&wm.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0,
			0) == 0,
			"event bridge creates cross-parent destination");
	    }
	    struct ged_event_txn_result cross_parent_setup_result;
	    ged_event_txn_result_init(&cross_parent_setup_result);
	    TCHECK(event_gedp && ged_event_batch_end(event_gedp,
		    &cross_parent_setup_result) >= 0,
		    "event bridge ends cross-parent move setup batch");
	    ged_event_txn_result_free(&cross_parent_setup_result);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(reset_spy.count() ==
		    before_cross_parent_setup_reset_signals,
		    "QgModel creates cross-parent move hierarchy without hierarchy reset");
	    TCHECK(rows_inserted_spy.count() >
		    before_cross_parent_setup_insert_signals,
		    "QgModel creates cross-parent move hierarchy through row inserts");
	    QModelIndex cross_parent_source_idx = find_top_level_item(
		    event_amodel, cross_parent_source);
	    QModelIndex cross_parent_dest_idx = find_top_level_item(
		    event_amodel, cross_parent_dest);
	    TCHECK(cross_parent_source_idx.isValid() &&
		    cross_parent_dest_idx.isValid(),
		    "event bridge exposes cross-parent move parents");
	    if (cross_parent_source_idx.isValid() &&
		    event_amodel->canFetchMore(cross_parent_source_idx))
		event_amodel->fetchMore(cross_parent_source_idx);
	    if (cross_parent_dest_idx.isValid() &&
		    event_amodel->canFetchMore(cross_parent_dest_idx))
		event_amodel->fetchMore(cross_parent_dest_idx);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    QModelIndex cross_parent_child_idx = find_child_item(event_amodel,
		    cross_parent_source_idx, cross_parent_child);
	    TCHECK(cross_parent_child_idx.isValid() &&
		    find_child_item(event_amodel, cross_parent_source_idx,
			cross_parent_source_keep).isValid() &&
		    find_child_item(event_amodel, cross_parent_dest_idx,
			cross_parent_dest_keep).isValid(),
		    "event bridge loads cross-parent move initial children");
	    QPersistentModelIndex cross_parent_source_persistent(
		    cross_parent_source_idx);
	    QPersistentModelIndex cross_parent_dest_persistent(
		    cross_parent_dest_idx);
	    QPersistentModelIndex cross_parent_child_persistent(
		    cross_parent_child_idx);
	    {
		struct wmember source_wm;
		struct wmember dest_wm;
		BU_LIST_INIT(&source_wm.l);
		BU_LIST_INIT(&dest_wm.l);
		TCHECK(mk_addmember(cross_parent_source_keep,
			&source_wm.l, NULL, WMOP_UNION) != NULL,
			"event bridge queues cross-parent source after move");
		TCHECK(mk_addmember(cross_parent_dest_keep, &dest_wm.l,
			NULL, WMOP_UNION) != NULL,
			"event bridge queues cross-parent destination retained child");
		TCHECK(mk_addmember(cross_parent_child, &dest_wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues cross-parent moved child in destination");
		event_model.resetHierarchyDeltaStats();
		int before_cross_parent_reset_signals = reset_spy.count();
		int before_cross_parent_insert_signals =
		    rows_inserted_spy.count();
		int before_cross_parent_remove_signals =
		    rows_removed_spy.count();
		int before_cross_parent_move_signals =
		    rows_moved_spy.count();
		TCHECK(event_gedp && ged_event_batch_begin(event_gedp) > 0,
			"event bridge starts cross-parent move rewrite batch");
		TCHECK(event_wdbp && mk_comb(event_wdbp,
			cross_parent_source, &source_wm.l, 0, NULL, NULL,
			NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
			"event bridge rewrites cross-parent source");
		TCHECK(event_wdbp && mk_comb(event_wdbp,
			cross_parent_dest, &dest_wm.l, 0, NULL, NULL, NULL,
			0, 0, 0, 0, 0, 0, 0) == 0,
			"event bridge rewrites cross-parent destination");
		struct ged_event_txn_result cross_parent_result;
		ged_event_txn_result_init(&cross_parent_result);
		TCHECK(event_gedp && ged_event_batch_end(event_gedp,
			&cross_parent_result) >= 0,
			"event bridge ends cross-parent move rewrite batch");
		ged_event_txn_result_free(&cross_parent_result);
		QCoreApplication::processEvents();
		QTest::qWait(1);
		QgModel::HierarchyDeltaStats cross_parent_stats =
		    event_model.hierarchyDeltaStats();
		bu_log("BENCH qgmodel_delta_cross_parent_move "
			"attempts=%llu applied=%llu fallbacks=%llu "
			"inserts=%llu removes=%llu moves=%llu "
			"insert_ranges=%llu remove_ranges=%llu move_ranges=%llu "
			"elapsed_us=%llu\n",
			(unsigned long long)cross_parent_stats.attempts,
			(unsigned long long)cross_parent_stats.applied,
			(unsigned long long)cross_parent_stats.reset_fallbacks,
			(unsigned long long)cross_parent_stats.inserted_rows,
			(unsigned long long)cross_parent_stats.removed_rows,
			(unsigned long long)cross_parent_stats.moved_rows,
			(unsigned long long)cross_parent_stats.inserted_row_ranges,
			(unsigned long long)cross_parent_stats.removed_row_ranges,
			(unsigned long long)cross_parent_stats.moved_row_ranges,
			(unsigned long long)cross_parent_stats.elapsed_us);
		TCHECK(reset_spy.count() == before_cross_parent_reset_signals,
			"QgModel cross-parent child move avoids hierarchy reset");
		TCHECK(rows_inserted_spy.count() ==
			before_cross_parent_insert_signals &&
			rows_removed_spy.count() ==
			before_cross_parent_remove_signals &&
			rows_moved_spy.count() > before_cross_parent_move_signals,
			"QgModel cross-parent child move emits row move only");
		TCHECK(cross_parent_stats.applied > 0 &&
			cross_parent_stats.reset_fallbacks == 0 &&
			cross_parent_stats.inserted_rows == 0 &&
			cross_parent_stats.removed_rows == 0 &&
			cross_parent_stats.moved_rows > 0,
			"QgModel cross-parent child move records row-move delta");
	    }
	    cross_parent_source_idx = find_top_level_item(event_amodel,
		    cross_parent_source);
	    cross_parent_dest_idx = find_top_level_item(event_amodel,
		    cross_parent_dest);
	    QModelIndex moved_cross_parent_child_idx =
		find_child_item(event_amodel, cross_parent_dest_idx,
			cross_parent_child);
	    TCHECK(cross_parent_source_persistent.isValid() &&
		    cross_parent_dest_persistent.isValid() &&
		    cross_parent_child_persistent.isValid() &&
		    moved_cross_parent_child_idx.isValid() &&
		    QModelIndex(cross_parent_child_persistent) ==
		    moved_cross_parent_child_idx &&
		    event_amodel->parent(QModelIndex(
			cross_parent_child_persistent)) == cross_parent_dest_idx,
		    "QgModel cross-parent child move preserves moved persistent index");
	    TCHECK(!find_child_item(event_amodel, cross_parent_source_idx,
		    cross_parent_child).isValid() &&
		    find_child_item(event_amodel, cross_parent_source_idx,
			cross_parent_source_keep).isValid() &&
		    child_name_at(event_amodel, cross_parent_dest_idx, 0) ==
		    QString::fromUtf8(cross_parent_dest_keep) &&
		    child_name_at(event_amodel, cross_parent_dest_idx, 1) ==
		    QString::fromUtf8(cross_parent_child),
		    "QgModel cross-parent child move exposes rewritten parent contents");
	    {
		std::string moved_path = std::string(cross_parent_dest) +
		    "/" + cross_parent_child;
		event_model.resetNotificationStats();
		int before_cross_parent_draw_signals =
		    event_data_spy.count();
		TCHECK(event_model.draw(moved_path.c_str()) == BRLCAD_OK,
			"QgModel cross-parent moved child draw succeeds");
		QgModel::NotificationStats cross_parent_notify_stats =
		    event_model.notificationStats();
		TCHECK(event_data_spy.count() >
			before_cross_parent_draw_signals,
			"QgModel cross-parent moved child draw notifies moved row");
		TCHECK(cross_parent_notify_stats.path_fallback_scans == 0 &&
			cross_parent_notify_stats.path_candidates > 0,
			"QgModel cross-parent move refreshes moved path index");
		TCHECK(event_model.erase(moved_path.c_str()) == BRLCAD_OK,
			"QgModel cross-parent moved child erase succeeds");
	    }

	    const int deep_depth = 40;
	    const char *deep_leaf = "_qgmodel_deep_leaf.s";
	    const char *deep_extra = "_qgmodel_deep_extra.s";
	    std::vector<std::string> deep_combs;
	    deep_combs.reserve(deep_depth);
	    for (int i = 0; i < deep_depth; i++) {
		char name[64];
		std::snprintf(name, sizeof(name),
			"_qgmodel_deep_%02d.c", i);
		deep_combs.emplace_back(name);
	    }
	    point_t deep_leaf_center = {240.0, 0.0, 0.0};
	    point_t deep_extra_center = {244.0, 0.0, 0.0};
	    int before_deep_setup_reset_signals = reset_spy.count();
	    int before_deep_setup_insert_signals = rows_inserted_spy.count();
	    TCHECK(event_gedp && ged_event_batch_begin(event_gedp) > 0,
		    "event bridge starts deep expanded path setup batch");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, deep_leaf,
		    deep_leaf_center, 1.0) == 0,
		    "event bridge creates deep expanded path leaf");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, deep_extra,
		    deep_extra_center, 1.0) == 0,
		    "event bridge creates deep expanded path extra leaf");
	    for (int i = deep_depth - 1; i >= 0; i--) {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		const char *child = (i == deep_depth - 1) ?
		    deep_leaf : deep_combs[(size_t)i + 1].c_str();
		TCHECK(mk_addmember(child, &wm.l, NULL, WMOP_UNION) != NULL,
			"event bridge queues deep expanded path child");
		TCHECK(event_wdbp && mk_comb(event_wdbp,
			deep_combs[(size_t)i].c_str(), &wm.l, 0, NULL,
			NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
			"event bridge creates deep expanded path comb");
	    }
	    struct ged_event_txn_result deep_setup_result;
	    ged_event_txn_result_init(&deep_setup_result);
	    TCHECK(event_gedp && ged_event_batch_end(event_gedp,
		    &deep_setup_result) >= 0,
		    "event bridge ends deep expanded path setup batch");
	    ged_event_txn_result_free(&deep_setup_result);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(reset_spy.count() == before_deep_setup_reset_signals,
		    "QgModel creates deep expanded path hierarchy without hierarchy reset");
	    TCHECK(rows_inserted_spy.count() > before_deep_setup_insert_signals,
		    "QgModel creates deep expanded path hierarchy through row inserts");

	    QModelIndex deep_idx = find_top_level_item(event_amodel,
		    deep_combs[0].c_str());
	    TCHECK(deep_idx.isValid(),
		    "event bridge exposes deep expanded path root");
	    QPersistentModelIndex deep_root_persistent(deep_idx);
	    QPersistentModelIndex deep_mid_persistent;
	    QModelIndex deep_current_idx = deep_idx;
	    for (int level = 0; level < deep_depth; level++) {
		if (deep_current_idx.isValid() &&
			event_amodel->canFetchMore(deep_current_idx))
		    event_amodel->fetchMore(deep_current_idx);
		QCoreApplication::processEvents();
		QTest::qWait(1);
		if (level == deep_depth / 2)
		    deep_mid_persistent = QPersistentModelIndex(deep_current_idx);
		if (level + 1 < deep_depth) {
		    deep_current_idx = find_child_item(event_amodel,
			    deep_current_idx, deep_combs[(size_t)level + 1].c_str());
		}
	    }
	    QModelIndex deepest_idx = deep_current_idx;
	    TCHECK(deepest_idx.isValid() &&
		    find_child_item(event_amodel, deepest_idx, deep_leaf).isValid(),
		    "event bridge expands 40-level deep path to loaded leaf");
	    QPersistentModelIndex deepest_persistent(deepest_idx);

	    event_model.resetHierarchyDeltaStats();
	    const char *deep_add_av[4] = {"g", deep_combs.back().c_str(),
		deep_extra, NULL};
	    int before_deep_add_reset_signals = reset_spy.count();
	    int before_deep_add_insert_signals = rows_inserted_spy.count();
	    int before_deep_add_remove_signals = rows_removed_spy.count();
	    TCHECK(event_gedp && ged_exec(event_gedp, 3, deep_add_av) == BRLCAD_OK,
		    "direct GED add succeeds for deepest loaded comb");
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    QgModel::HierarchyDeltaStats deep_delta_stats =
		event_model.hierarchyDeltaStats();
	    bu_log("BENCH qgmodel_delta_deep attempts=%llu applied=%llu "
		    "fallbacks=%llu parents=%llu rows=%llu inserts=%llu "
		    "removes=%llu elapsed_us=%llu loaded_items=%llu depth=%d\n",
		    (unsigned long long)deep_delta_stats.attempts,
		    (unsigned long long)deep_delta_stats.applied,
		    (unsigned long long)deep_delta_stats.reset_fallbacks,
		    (unsigned long long)deep_delta_stats.planned_parents,
		    (unsigned long long)deep_delta_stats.planned_child_rows,
		    (unsigned long long)deep_delta_stats.inserted_rows,
		    (unsigned long long)deep_delta_stats.removed_rows,
		    (unsigned long long)deep_delta_stats.elapsed_us,
		    (unsigned long long)event_model.allItems().size(),
		    deep_depth);
	    TCHECK(reset_spy.count() == before_deep_add_reset_signals,
		    "QgModel deep expanded path edit avoids hierarchy reset");
	    TCHECK(rows_inserted_spy.count() > before_deep_add_insert_signals &&
		    rows_removed_spy.count() > before_deep_add_remove_signals,
		    "QgModel deep expanded path edit emits grouped row deltas");
	    TCHECK(deep_delta_stats.applied > 0 &&
		    deep_delta_stats.reset_fallbacks == 0,
		    "QgModel deep expanded path edit applies grouped delta");
	    TCHECK(deep_delta_stats.planned_parents >= (uint64_t)deep_depth &&
		    deep_delta_stats.planned_parents <= event_model.allItems().size() + 1,
		    "QgModel deep expanded path planning is bounded by loaded parents");
	    TCHECK(deep_delta_stats.planned_child_rows >= (uint64_t)deep_depth &&
		    deep_delta_stats.planned_child_rows <= event_model.allItems().size(),
		    "QgModel deep expanded path planning is bounded by loaded rows");
	    TCHECK(deep_delta_stats.inserted_rows > 0 &&
		    deep_delta_stats.removed_rows > 0,
		    "QgModel deep expanded path edit records targeted row operations");
	    TCHECK(deep_root_persistent.isValid() &&
		    deep_mid_persistent.isValid() &&
		    deepest_persistent.isValid(),
		    "QgModel deep expanded path edit preserves persistent indexes");
	    deepest_idx = QModelIndex(deepest_persistent);
	    TCHECK(find_child_item(event_amodel, deepest_idx,
		    deep_extra).isValid(),
		    "QgModel deep expanded path edit inserts child at deepest loaded comb");

	    auto run_scale_test = [&]() {
	    const int scale_child_count = 1024;
	    const char *scale_parent = "_qgmodel_delta_scale_parent.c";
	    const char *scale_extra = "_qgmodel_delta_scale_extra.s";
	    std::vector<std::string> scale_leaves;
	    scale_leaves.reserve(scale_child_count);
	    event_model.resetHierarchyDeltaStats();
	    event_model.resetFetchMoreStats();
	    int before_scale_setup_reset_signals = reset_spy.count();
	    int before_scale_setup_insert_signals = rows_inserted_spy.count();
	    TCHECK(event_gedp && ged_event_batch_begin(event_gedp) > 0,
		    "event bridge starts grouped-delta scale setup batch");
	    int scale_leaf_create_count = 0;
	    for (int i = 0; i < scale_child_count; i++) {
		char name[64];
		std::snprintf(name, sizeof(name),
			"_qgmodel_delta_scale_leaf_%04d.s", i);
		scale_leaves.emplace_back(name);
		point_t center = {300.0 + (fastf_t)i, 0.0, 0.0};
		if (event_wdbp && mk_sph(event_wdbp,
			scale_leaves.back().c_str(), center, 1.0) == 0)
		    scale_leaf_create_count++;
	    }
	    TCHECK(scale_leaf_create_count == scale_child_count,
		    "event bridge creates grouped-delta scale leaves");
	    point_t scale_extra_center = {300.0 + (fastf_t)scale_child_count,
		0.0, 0.0};
	    TCHECK(event_wdbp && mk_sph(event_wdbp, scale_extra,
		    scale_extra_center, 1.0) == 0,
		    "event bridge creates grouped-delta scale extra leaf");
	    {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		int scale_member_count = 0;
		for (const std::string &leaf : scale_leaves) {
		    if (mk_addmember(leaf.c_str(), &wm.l, NULL,
			    WMOP_UNION) != NULL)
			scale_member_count++;
		}
		TCHECK(scale_member_count == scale_child_count,
			"event bridge queues grouped-delta scale children");
		TCHECK(event_wdbp && mk_comb(event_wdbp, scale_parent, &wm.l,
			0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
			"event bridge creates grouped-delta scale parent");
	    }
	    struct ged_event_txn_result scale_setup_result;
	    ged_event_txn_result_init(&scale_setup_result);
	    TCHECK(event_gedp && ged_event_batch_end(event_gedp,
		    &scale_setup_result) >= 0,
		    "event bridge ends grouped-delta scale setup batch");
	    ged_event_txn_result_free(&scale_setup_result);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    QgModel::HierarchyDeltaStats scale_setup_stats =
		event_model.hierarchyDeltaStats();
	    QgModel::FetchMoreStats scale_setup_fetch_stats =
		event_model.fetchMoreStats();
	    bu_log("BENCH qgmodel_delta_scale_setup attempts=%llu applied=%llu "
		    "fallbacks=%llu parents=%llu rows=%llu inserts=%llu "
		    "removes=%llu insert_ranges=%llu remove_ranges=%llu "
		    "elapsed_us=%llu loaded_items=%llu\n",
		    (unsigned long long)scale_setup_stats.attempts,
		    (unsigned long long)scale_setup_stats.applied,
		    (unsigned long long)scale_setup_stats.reset_fallbacks,
		    (unsigned long long)scale_setup_stats.planned_parents,
		    (unsigned long long)scale_setup_stats.planned_child_rows,
		    (unsigned long long)scale_setup_stats.inserted_rows,
		    (unsigned long long)scale_setup_stats.removed_rows,
		    (unsigned long long)scale_setup_stats.inserted_row_ranges,
		    (unsigned long long)scale_setup_stats.removed_row_ranges,
		    (unsigned long long)scale_setup_stats.elapsed_us,
		    (unsigned long long)event_model.allItems().size());
	    bu_log("BENCH qgmodel_delta_scale_setup_fetch calls=%llu "
		    "populated=%llu rows=%llu ranges=%llu max_children=%llu "
		    "elapsed_us=%llu\n",
		    (unsigned long long)scale_setup_fetch_stats.calls,
		    (unsigned long long)scale_setup_fetch_stats.populated_calls,
		    (unsigned long long)scale_setup_fetch_stats.inserted_rows,
		    (unsigned long long)scale_setup_fetch_stats.inserted_row_ranges,
		    (unsigned long long)scale_setup_fetch_stats.max_child_count,
		    (unsigned long long)scale_setup_fetch_stats.elapsed_us);
	    TCHECK(reset_spy.count() == before_scale_setup_reset_signals,
		    "QgModel creates grouped-delta scale hierarchy without hierarchy reset");
	    TCHECK(rows_inserted_spy.count() > before_scale_setup_insert_signals,
		    "QgModel creates grouped-delta scale hierarchy through row inserts");
	    TCHECK(scale_setup_stats.applied > 0 &&
		    scale_setup_stats.reset_fallbacks == 0,
		    "QgModel creates grouped-delta scale hierarchy through structural deltas");
	    TCHECK(scale_setup_stats.inserted_rows >= 2 &&
		    scale_setup_stats.inserted_row_ranges > 0 &&
		    scale_setup_stats.inserted_row_ranges <= 2,
		    "QgModel groups lazy scale setup into bounded top-level row insert ranges");
	    TCHECK(scale_setup_fetch_stats.calls == 0 &&
		    scale_setup_fetch_stats.populated_calls == 0 &&
		    scale_setup_fetch_stats.inserted_rows == 0,
		    "QgModel scale setup leaves large child branch lazy without tester auto-fetch");

	    QModelIndex scale_parent_idx = find_top_level_item(event_amodel,
		    scale_parent);
	    TCHECK(scale_parent_idx.isValid(),
		    "event bridge exposes grouped-delta scale parent");
	    TCHECK(scale_parent_idx.isValid() &&
		    event_amodel->rowCount(scale_parent_idx) == 0 &&
		    event_amodel->canFetchMore(scale_parent_idx),
		    "event bridge keeps grouped-delta scale parent lazy before explicit fetch");
	    int before_scale_fetch_insert_signals = rows_inserted_spy.count();
	    int64_t scale_fetch_start_us = bu_gettime();
	    if (scale_parent_idx.isValid() &&
		    event_amodel->canFetchMore(scale_parent_idx))
		event_amodel->fetchMore(scale_parent_idx);
	    int64_t scale_fetch_elapsed_us = bu_gettime() -
		scale_fetch_start_us;
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    QgModel::FetchMoreStats scale_fetch_stats =
		event_model.fetchMoreStats();
	    bu_log("BENCH qgmodel_delta_scale_fetch children=%d "
		    "explicit_insert_signals=%d explicit_elapsed_us=%lld "
		    "calls=%llu populated=%llu rows=%llu ranges=%llu "
		    "max_children=%llu fetch_elapsed_us=%llu loaded_items=%llu\n",
		    scale_child_count,
		    (int)(rows_inserted_spy.count() -
		    before_scale_fetch_insert_signals),
		    (long long)scale_fetch_elapsed_us,
		    (unsigned long long)scale_fetch_stats.calls,
		    (unsigned long long)scale_fetch_stats.populated_calls,
		    (unsigned long long)scale_fetch_stats.inserted_rows,
		    (unsigned long long)scale_fetch_stats.inserted_row_ranges,
		    (unsigned long long)scale_fetch_stats.max_child_count,
		    (unsigned long long)scale_fetch_stats.elapsed_us,
		    (unsigned long long)event_model.allItems().size());
	    TCHECK(rows_inserted_spy.count() > before_scale_fetch_insert_signals,
		    "QgModel explicit scale fetch emits row insert signal");
	    TCHECK(scale_fetch_stats.calls == 1 &&
		    scale_fetch_stats.populated_calls == 1 &&
		    scale_fetch_stats.inserted_rows == (uint64_t)scale_child_count &&
		    scale_fetch_stats.max_child_count >=
		    (uint64_t)scale_child_count &&
		    scale_fetch_stats.inserted_row_ranges <= 2,
		    "QgModel fetchMore loads 1024-child scale parent with bounded row insert ranges");
	    TCHECK(event_amodel->rowCount(scale_parent_idx) == scale_child_count,
		    "event bridge loads grouped-delta scale parent children");
	    QPersistentModelIndex scale_parent_persistent(scale_parent_idx);

	    event_model.resetHierarchyDeltaStats();
	    const char *scale_add_av[4] = {"g", scale_parent, scale_extra, NULL};
	    int before_scale_add_reset_signals = reset_spy.count();
	    int before_scale_add_insert_signals = rows_inserted_spy.count();
	    int before_scale_add_remove_signals = rows_removed_spy.count();
	    TCHECK(event_gedp && ged_exec(event_gedp, 3, scale_add_av) == BRLCAD_OK,
		    "direct GED add succeeds for grouped-delta scale parent");
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    QgModel::HierarchyDeltaStats scale_delta_stats =
		event_model.hierarchyDeltaStats();
	    bu_log("BENCH qgmodel_delta_scale attempts=%llu applied=%llu "
		    "fallbacks=%llu parents=%llu rows=%llu inserts=%llu "
		    "removes=%llu insert_ranges=%llu remove_ranges=%llu "
		    "elapsed_us=%llu loaded_items=%llu\n",
		    (unsigned long long)scale_delta_stats.attempts,
		    (unsigned long long)scale_delta_stats.applied,
		    (unsigned long long)scale_delta_stats.reset_fallbacks,
		    (unsigned long long)scale_delta_stats.planned_parents,
		    (unsigned long long)scale_delta_stats.planned_child_rows,
		    (unsigned long long)scale_delta_stats.inserted_rows,
		    (unsigned long long)scale_delta_stats.removed_rows,
		    (unsigned long long)scale_delta_stats.inserted_row_ranges,
		    (unsigned long long)scale_delta_stats.removed_row_ranges,
		    (unsigned long long)scale_delta_stats.elapsed_us,
		    (unsigned long long)event_model.allItems().size());
	    TCHECK(reset_spy.count() == before_scale_add_reset_signals,
		    "QgModel grouped-delta scale edit avoids hierarchy reset");
	    TCHECK(rows_inserted_spy.count() > before_scale_add_insert_signals &&
		    rows_removed_spy.count() > before_scale_add_remove_signals,
		    "QgModel grouped-delta scale edit emits row deltas");
	    TCHECK(scale_delta_stats.attempts > 0 &&
		    scale_delta_stats.applied > 0,
		    "QgModel grouped-delta scale edit applies structural delta");
	    TCHECK(scale_delta_stats.reset_fallbacks == 0,
		    "QgModel grouped-delta scale edit has no reset fallback");
	    TCHECK(scale_delta_stats.db_index_refreshes == scale_delta_stats.attempts,
		    "QgModel grouped-delta scale edit refreshes index once per delta attempt");
	    TCHECK(scale_delta_stats.planned_parents >= 2 &&
		    scale_delta_stats.planned_parents <= event_model.allItems().size() + 1,
		    "QgModel grouped-delta scale planning is bounded by loaded parents");
	    TCHECK(scale_delta_stats.planned_child_rows >= (uint64_t)scale_child_count &&
		    scale_delta_stats.planned_child_rows <= event_model.allItems().size(),
		    "QgModel grouped-delta scale planning is bounded by loaded rows");
	    TCHECK(scale_delta_stats.inserted_rows > 0 &&
		    scale_delta_stats.removed_rows > 0,
		    "QgModel grouped-delta scale edit records targeted row operations");
	    TCHECK(scale_delta_stats.inserted_row_ranges <= 2 &&
		    scale_delta_stats.removed_row_ranges <= 2,
		    "QgModel grouped-delta scale edit emits bounded row operation ranges");
	    TCHECK(scale_parent_persistent.isValid(),
		    "QgModel grouped-delta scale edit preserves parent persistent index");
	    scale_parent_idx = find_top_level_item(event_amodel, scale_parent);
	    TCHECK(scale_parent_idx.isValid() &&
		    event_amodel->rowCount(scale_parent_idx) == scale_child_count + 1 &&
		    find_child_item(event_amodel, scale_parent_idx,
			scale_extra).isValid(),
		    "QgModel grouped-delta scale edit inserts extra child under loaded parent");
	    };

	    const char *mixed_parent = "_qgmodel_mixed_parent.c";
	    const char *mixed_a = "_qgmodel_mixed_a.s";
	    const char *mixed_b = "_qgmodel_mixed_b.s";
	    const char *mixed_new = "_qgmodel_mixed_new.s";
	    const char *mixed_rename_old = "_qgmodel_mixed_rename_old.s";
	    const char *mixed_rename_new = "_qgmodel_mixed_rename_new.s";
	    point_t mixed_a_center = {420.0, 0.0, 0.0};
	    point_t mixed_b_center = {424.0, 0.0, 0.0};
	    point_t mixed_new_center = {428.0, 0.0, 0.0};
	    point_t mixed_rename_center = {432.0, 0.0, 0.0};
	    int before_mixed_setup_reset_signals = reset_spy.count();
	    int before_mixed_setup_insert_signals = rows_inserted_spy.count();
	    TCHECK(event_gedp && ged_event_batch_begin(event_gedp) > 0,
		    "event bridge starts mixed structural setup batch");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, mixed_a,
		    mixed_a_center, 1.0) == 0,
		    "event bridge creates mixed structural first child");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, mixed_b,
		    mixed_b_center, 1.0) == 0,
		    "event bridge creates mixed structural retained child");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, mixed_new,
		    mixed_new_center, 1.0) == 0,
		    "event bridge creates mixed structural future child");
	    TCHECK(event_wdbp && mk_sph(event_wdbp, mixed_rename_old,
		    mixed_rename_center, 1.0) == 0,
		    "event bridge creates mixed structural rename source");
	    {
		struct wmember wm;
		BU_LIST_INIT(&wm.l);
		TCHECK(mk_addmember(mixed_a, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues mixed structural first child");
		TCHECK(mk_addmember(mixed_b, &wm.l, NULL,
			WMOP_UNION) != NULL,
			"event bridge queues mixed structural retained child");
		TCHECK(event_wdbp && mk_comb(event_wdbp, mixed_parent,
			&wm.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0,
			0) == 0,
			"event bridge creates mixed structural parent");
	    }
	    struct ged_event_txn_result mixed_setup_result;
	    ged_event_txn_result_init(&mixed_setup_result);
	    TCHECK(event_gedp && ged_event_batch_end(event_gedp,
		    &mixed_setup_result) >= 0,
		    "event bridge ends mixed structural setup batch");
	    ged_event_txn_result_free(&mixed_setup_result);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(reset_spy.count() == before_mixed_setup_reset_signals,
		    "QgModel creates mixed structural hierarchy without hierarchy reset");
	    TCHECK(rows_inserted_spy.count() > before_mixed_setup_insert_signals,
		    "QgModel creates mixed structural hierarchy through row inserts");

	    QModelIndex mixed_parent_idx = find_top_level_item(event_amodel,
		    mixed_parent);
	    TCHECK(mixed_parent_idx.isValid(),
		    "event bridge exposes mixed structural parent");
	    if (mixed_parent_idx.isValid() &&
		    event_amodel->canFetchMore(mixed_parent_idx))
		event_amodel->fetchMore(mixed_parent_idx);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    QModelIndex mixed_b_idx = find_child_item(event_amodel,
		    mixed_parent_idx, mixed_b);
	    TCHECK(mixed_b_idx.isValid() &&
		    find_child_item(event_amodel, mixed_parent_idx,
			mixed_a).isValid(),
		    "event bridge loads mixed structural parent children");
	    QPersistentModelIndex mixed_parent_persistent(mixed_parent_idx);
	    QPersistentModelIndex mixed_b_persistent(mixed_b_idx);

	    const char *mixed_add_av[4] = {"g", mixed_parent, mixed_new, NULL};
	    std::string mixed_rm_path = std::string(mixed_parent) + "/" +
		mixed_a;
	    const char *mixed_rm_av[3] = {"rm", mixed_rm_path.c_str(), NULL};
	    const char *mixed_move_av[4] = {"move", mixed_rename_old,
		mixed_rename_new, NULL};
	    event_model.resetHierarchyDeltaStats();
	    int before_mixed_reset_signals = reset_spy.count();
	    int before_mixed_insert_signals = rows_inserted_spy.count();
	    int before_mixed_remove_signals = rows_removed_spy.count();
	    TCHECK(event_gedp && ged_event_batch_begin(event_gedp) > 0,
		    "event bridge starts mixed structural edit batch");
	    TCHECK(event_gedp && ged_exec(event_gedp, 3, mixed_add_av) == BRLCAD_OK,
		    "direct GED add succeeds inside mixed structural batch");
	    TCHECK(event_gedp && ged_exec(event_gedp, 2, mixed_rm_av) == BRLCAD_OK,
		    "direct GED rm succeeds inside mixed structural batch");
	    TCHECK(event_gedp && ged_exec(event_gedp, 3, mixed_move_av) == BRLCAD_OK,
		    "direct GED move succeeds inside mixed structural batch");
	    struct ged_event_txn_result mixed_result;
	    ged_event_txn_result_init(&mixed_result);
	    TCHECK(event_gedp && ged_event_batch_end(event_gedp,
		    &mixed_result) >= 0,
		    "event bridge ends mixed structural edit batch");
	    ged_event_txn_result_free(&mixed_result);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    QgModel::HierarchyDeltaStats mixed_stats =
		event_model.hierarchyDeltaStats();
	    bu_log("BENCH qgmodel_delta_mixed attempts=%llu applied=%llu "
		    "fallbacks=%llu parents=%llu rows=%llu changed=%llu "
		    "inserts=%llu removes=%llu elapsed_us=%llu loaded_items=%llu\n",
		    (unsigned long long)mixed_stats.attempts,
		    (unsigned long long)mixed_stats.applied,
		    (unsigned long long)mixed_stats.reset_fallbacks,
		    (unsigned long long)mixed_stats.planned_parents,
		    (unsigned long long)mixed_stats.planned_child_rows,
		    (unsigned long long)mixed_stats.changed_parents,
		    (unsigned long long)mixed_stats.inserted_rows,
		    (unsigned long long)mixed_stats.removed_rows,
		    (unsigned long long)mixed_stats.elapsed_us,
		    (unsigned long long)event_model.allItems().size());
	    TCHECK(reset_spy.count() == before_mixed_reset_signals,
		    "QgModel mixed structural batch avoids hierarchy reset");
	    TCHECK(rows_inserted_spy.count() > before_mixed_insert_signals &&
		    rows_removed_spy.count() > before_mixed_remove_signals,
		    "QgModel mixed structural batch emits grouped row deltas");
	    TCHECK(mixed_stats.applied > 0 &&
		    mixed_stats.reset_fallbacks == 0,
		    "QgModel mixed structural batch applies grouped delta");
	    TCHECK(mixed_stats.changed_parents >= 2,
		    "QgModel mixed structural batch changes multiple parents");
	    TCHECK(mixed_stats.planned_parents <= event_model.allItems().size() + 1 &&
		    mixed_stats.planned_child_rows <= event_model.allItems().size(),
		    "QgModel mixed structural batch planning is bounded by loaded model state");
	    TCHECK(mixed_parent_persistent.isValid() &&
		    mixed_b_persistent.isValid(),
		    "QgModel mixed structural batch preserves unaffected persistent indexes");
	    mixed_parent_idx = find_top_level_item(event_amodel, mixed_parent);
	    TCHECK(mixed_parent_idx.isValid() &&
		    !find_child_item(event_amodel, mixed_parent_idx,
			mixed_a).isValid() &&
		    find_child_item(event_amodel, mixed_parent_idx,
			mixed_b).isValid() &&
		    find_child_item(event_amodel, mixed_parent_idx,
			mixed_new).isValid(),
		    "QgModel mixed structural batch updates loaded parent contents");
	    TCHECK(find_top_level_item(event_amodel, mixed_rename_new).isValid() &&
		    !find_top_level_item(event_amodel,
			mixed_rename_old).isValid(),
		    "QgModel mixed structural batch exposes renamed top-level object");

	    all_event_idx = find_top_level_item(event_amodel, "all.g");
	    if (all_event_idx.isValid() && event_amodel->canFetchMore(all_event_idx))
		event_amodel->fetchMore(all_event_idx);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);

	    QModelIndex platform_idx = find_child_item(event_amodel, all_event_idx, "platform.r");
	    TCHECK(platform_idx.isValid(),
		    "event bridge expanded all.g exposes platform.r child before remove");
	    QPersistentModelIndex all_event_persistent(all_event_idx);
	    TCHECK(model_has_item_path(&event_model, "all.g/platform.r"),
		    "event bridge tracks expanded all.g/platform.r item path before remove");
	    TCHECK(event_model.draw("all.g") == BRLCAD_OK,
		    "event bridge draw all.g succeeds before direct remove");
	    TCHECK(event_gedp && ged_draw_path_state(event_gedp,
		    event_gedp->ged_gvp, "all.g/platform.r", -1) == 1,
		    "event bridge canonical draw state includes platform.r before remove");
	    const char *remove_av[4] = {"remove", "all.g", "platform.r", NULL};
	    int before_remove_db_signals = db_spy.count();
	    int before_remove_reset_signals = reset_spy.count();
	    int before_remove_insert_signals = rows_inserted_spy.count();
	    int before_remove_remove_signals = rows_removed_spy.count();
	    TCHECK(event_gedp && ged_exec(event_gedp, 3, remove_av) == BRLCAD_OK,
		    "direct GED remove succeeds for event bridge subtree test");
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(db_spy.count() > before_remove_db_signals,
		    "QgModel bridges direct GedEventTxn remove into db changed signal");
	    TCHECK(reset_spy.count() == before_remove_reset_signals,
		    "QgModel bridges direct GedEventTxn remove without hierarchy reset");
	    TCHECK(rows_inserted_spy.count() > before_remove_insert_signals &&
		    rows_removed_spy.count() > before_remove_remove_signals,
		    "QgModel bridges direct GedEventTxn remove into grouped row deltas");
	    QModelIndex all_after_remove_idx = find_top_level_item(event_amodel, "all.g");
	    TCHECK(all_after_remove_idx.isValid(),
		    "event bridge model keeps all.g top-level item after child remove");
	    TCHECK(all_event_persistent.isValid() &&
		    all_event_persistent == all_after_remove_idx,
		    "event bridge preserves all.g persistent index across grouped remove deltas");
	    if (all_after_remove_idx.isValid() && event_amodel->canFetchMore(all_after_remove_idx))
		event_amodel->fetchMore(all_after_remove_idx);
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(!find_child_item(event_amodel, all_after_remove_idx, "platform.r").isValid(),
		    "event bridge model removes platform.r child row after direct remove");
	    TCHECK(!model_has_item_path(&event_model, "all.g/platform.r"),
		    "event bridge retires removed all.g/platform.r item path after direct remove");
	    TCHECK(find_top_level_item(event_amodel, "platform.r").isValid(),
		    "event bridge keeps unreferenced platform.r object as a top-level item");
	    TCHECK(event_gedp && ged_draw_path_state(event_gedp,
		    event_gedp->ged_gvp, "all.g/platform.r", -1) == 0,
		    "event bridge canonical draw state erases removed platform.r path");
	    TCHECK(event_gedp && ged_draw_path_state(event_gedp,
		    event_gedp->ged_gvp, "all.g", -1) == 1,
		    "event bridge canonical draw state keeps parent all.g drawn after remove redraw");
	    if (all_after_remove_idx.isValid())
		TCHECK(event_amodel->data(all_after_remove_idx, DrawnDisplayRole).toInt() == 1,
			"event bridge DrawnDisplayRole reads redrawn parent state after direct remove");

	    QModelIndex platform_top_idx = find_top_level_item(event_amodel, "platform.r");
	    TCHECK(platform_top_idx.isValid(),
		    "event bridge exposes unreferenced platform.r top-level item before kill");
	    TCHECK(event_model.draw("platform.r") == BRLCAD_OK,
		    "event bridge draw platform.r succeeds before direct kill");
	    TCHECK(event_gedp && ged_draw_path_state(event_gedp,
		    event_gedp->ged_gvp, "platform.r", -1) == 1,
		    "event bridge canonical draw state includes platform.r before kill");
	    if (platform_top_idx.isValid())
		TCHECK(event_amodel->data(platform_top_idx, DrawnDisplayRole).toInt() == 1,
		    "event bridge DrawnDisplayRole reads drawn platform.r before direct kill");
	    const char *kill_av[3] = {"kill", "platform.r", NULL};
	    int before_kill_db_signals = db_spy.count();
	    int before_kill_reset_signals = reset_spy.count();
	    int before_kill_insert_signals = rows_inserted_spy.count();
	    int before_kill_remove_signals = rows_removed_spy.count();
	    TCHECK(event_gedp && ged_exec(event_gedp, 2, kill_av) == BRLCAD_OK,
		    "direct GED kill succeeds for event bridge drawn-role test");
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(db_spy.count() > before_kill_db_signals,
		    "QgModel bridges direct GedEventTxn kill into db changed signal");
	    TCHECK(reset_spy.count() == before_kill_reset_signals,
		    "QgModel bridges direct GedEventTxn kill without hierarchy reset");
	    TCHECK(rows_removed_spy.count() > before_kill_remove_signals,
		    "QgModel bridges direct GedEventTxn kill into targeted row remove");
	    TCHECK(rows_inserted_spy.count() > before_kill_insert_signals,
		    "QgModel exposes newly unreferenced kill child through row insert");
	    TCHECK(!find_top_level_item(event_amodel, "platform.r").isValid(),
		    "event bridge model removes killed platform.r top-level item");
	    TCHECK(!model_has_item_path(&event_model, "platform.r"),
		    "event bridge retires killed platform.r item path after direct kill");
	    TCHECK(event_gedp && ged_draw_path_state(event_gedp,
		    event_gedp->ged_gvp, "platform.r", -1) == 0,
		    "event bridge canonical draw state erases killed platform.r path");

	    const char *move_av[4] = {"move", "all.g", "all_qg_event.g", NULL};
	    int before_db_signals = db_spy.count();
	    int before_reset_signals = reset_spy.count();
	    int before_move_insert_signals = rows_inserted_spy.count();
	    int before_move_remove_signals = rows_removed_spy.count();
	    TCHECK(event_gedp && ged_exec(event_gedp, 3, move_av) == BRLCAD_OK,
		    "direct GED move succeeds for event bridge test");
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    TCHECK(db_spy.count() > before_db_signals,
		    "QgModel bridges direct GedEventTxn rename into db changed signal");
	    TCHECK(reset_spy.count() == before_reset_signals,
		    "QgModel bridges direct GedEventTxn rename without hierarchy reset");
	    TCHECK(rows_inserted_spy.count() > before_move_insert_signals &&
		    rows_removed_spy.count() > before_move_remove_signals,
		    "QgModel bridges direct GedEventTxn rename into grouped row deltas");
	    TCHECK(find_top_level_item(event_amodel, "all_qg_event.g").isValid(),
		    "event bridge model exposes renamed top-level item");
	    TCHECK(!find_top_level_item(event_amodel, "all.g").isValid(),
		    "event bridge model removes old top-level item name");

	    event_tester.reset();
	    QCoreApplication::processEvents();
	    QTest::qWait(1);
	    run_scale_test();
	}

	bu_file_delete(event_tmp.c_str());
    }

    if (top_rows > 0) {
	QSignalSpy opened_spy(&model, SIGNAL(opened_item(QgItem *)));
	QSignalSpy reset_spy(&model, SIGNAL(modelReset()));
	TCHECK(opened_spy.isValid(), "opened_item spy is valid");
	TCHECK(reset_spy.isValid(), "modelReset spy is valid");

	QModelIndex first = amodel->index(0, 0, QModelIndex());
	TCHECK(first.isValid(), "first top-level index is valid");

	bool can_fetch = amodel->canFetchMore(first);
	if (can_fetch)
	    amodel->fetchMore(first);
	QCoreApplication::processEvents();
	QTest::qWait(1);

	if (can_fetch) {
	    TCHECK(opened_spy.count() > 0, "fetchMore emits opened_item");
	    TCHECK(amodel->rowCount(first) > 0, "fetchMore populates first top-level item children");
	} else {
	    TCHECK(true, "first top-level item has no fetchable children");
	}

	model.toggle_hierarchy();
	QCoreApplication::processEvents();
	QTest::qWait(1);
	TCHECK(reset_spy.count() > 0, "toggle_hierarchy emits modelReset");
    }

    if (g_fail) {
	bu_log("\ntest_qgmodel_model_tester: FAILED (%d check(s))\n", g_fail);
	return 1;
    }
    bu_log("\ntest_qgmodel_model_tester: PASSED\n");
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

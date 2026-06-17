/*                  T E S T _ Q G M O D E L _ S C A L E . C P P
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
/** @file test_qgmodel_scale.cpp
 *
 * Focused QgModel scale coverage.  This deliberately avoids
 * QAbstractItemModelTester so larger loaded-tree fan-out can be validated
 * without making every invariant probe traverse the large branch.
 */

#include "common.h"

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QPersistentModelIndex>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/process.h"
#include "bu/str.h"
#include "bu/time.h"
#include "ged.h"
#include "ged/event_txn.h"
#include "qtcad/QgModel.h"
#include "vmath.h"
#include "wdb.h"

static int g_fail = 0;
static const int default_scale_child_count = 4096;
static const int slow_scale_child_count = 16384;
static const int default_draw_batch_count = 8;

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
	if (idx.isValid() &&
		model->data(idx, Qt::DisplayRole).toString() == wanted)
	    return idx;
    }

    return QModelIndex();
}

static QModelIndex
find_child_item(QAbstractItemModel *model, const QModelIndex &parent,
	const char *name)
{
    if (!model || !parent.isValid() || !name)
	return QModelIndex();

    const int rows = model->rowCount(parent);
    const QString wanted = QString::fromUtf8(name);
    for (int row = 0; row < rows; row++) {
	QModelIndex idx = model->index(row, 0, parent);
	if (idx.isValid() &&
		model->data(idx, Qt::DisplayRole).toString() == wanted)
	    return idx;
    }

    return QModelIndex();
}

static bool
copy_file(const char *src_path, const char *dst_path)
{
    if (!src_path || !dst_path)
	return false;

    std::ifstream src(src_path, std::ios::binary);
    std::ofstream dst(dst_path, std::ios::binary | std::ios::trunc);
    if (src && dst) {
	dst << src.rdbuf();
	return !src.bad() && dst.good();
    }

    return false;
}

static int
scale_child_count_from_env()
{
    QByteArray env = qgetenv("BRLCAD_QGMODEL_SCALE_CHILDREN");
    if (env.isEmpty())
	return default_scale_child_count;

    bool ok = false;
    int count = env.toInt(&ok);
    if (ok && count > 0)
	return count;

    bu_log("Ignoring invalid BRLCAD_QGMODEL_SCALE_CHILDREN=%s\n",
	    env.constData());
    return default_scale_child_count;
}

static int
slow_scale_tests_enabled()
{
    const char *run_slow = std::getenv("BRLCAD_RUN_SLOW_TESTS");
    return run_slow && run_slow[0] && !BU_STR_EQUAL(run_slow, "0");
}

static int
draw_batch_count_from_env(int scale_child_count)
{
    int count = default_draw_batch_count;
    QByteArray env = qgetenv("BRLCAD_QGMODEL_DRAW_CHILDREN");
    if (!env.isEmpty()) {
	bool ok = false;
	int requested = env.toInt(&ok);
	if (ok && requested > 0)
	    count = requested;
	else
	    bu_log("Ignoring invalid BRLCAD_QGMODEL_DRAW_CHILDREN=%s\n",
		    env.constData());
    }

    if (count > default_draw_batch_count && !slow_scale_tests_enabled()) {
	bu_log("Clamping BRLCAD_QGMODEL_DRAW_CHILDREN=%d to %d; "
		"set BRLCAD_RUN_SLOW_TESTS=1 for larger draw batches.\n",
		count, default_draw_batch_count);
	count = default_draw_batch_count;
    }

    if (count > scale_child_count)
	count = scale_child_count;
    if (count < 1)
	count = 1;
    return count;
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
    const int scale_child_count = scale_child_count_from_env();
    if (scale_child_count >= slow_scale_child_count &&
	    !slow_scale_tests_enabled()) {
	bu_log("Skipping slow QgModel scale children=%d test; set BRLCAD_RUN_SLOW_TESTS=1 to run it.\n",
		scale_child_count);
	return 123;
    }

    char scale_parent_buf[128] = {0};
    char scale_extra_buf[128] = {0};
    char draw_parent_buf[128] = {0};
    std::snprintf(scale_parent_buf, sizeof(scale_parent_buf),
	    "_qgmodel_scale_%d_parent.c", scale_child_count);
    std::snprintf(scale_extra_buf, sizeof(scale_extra_buf),
	    "_qgmodel_scale_%d_extra.s", scale_child_count);
    std::snprintf(draw_parent_buf, sizeof(draw_parent_buf),
	    "_qgmodel_scale_%d_draw_parent.c", scale_child_count);
    std::string scale_parent_storage(scale_parent_buf);
    std::string scale_extra_storage(scale_extra_buf);
    std::string draw_parent_storage(draw_parent_buf);
    const char *scale_parent = scale_parent_storage.c_str();
    const char *scale_extra = scale_extra_storage.c_str();
    const char *draw_parent = draw_parent_storage.c_str();
    const int draw_batch_count = draw_batch_count_from_env(scale_child_count);
    bu_log("BENCH qgmodel_scale_config children=%d draw_children=%d\n",
	    scale_child_count, draw_batch_count);

    std::string event_tmp = std::string("qgmodel_scale_tmp_") +
	std::to_string((long long)bu_pid()) + ".g";
    bu_file_delete(event_tmp.c_str());

    QByteArray fixture_env = qgetenv("BRLCAD_QGMODEL_SCALE_FIXTURE");
    std::string fixture_path = fixture_env.isEmpty() ?
	std::string() : std::string(fixture_env.constData());
    int using_cached_fixture = 0;
    const char *copy_source = g_path;
    if (!fixture_path.empty()) {
	if (bu_file_exists(fixture_path.c_str(), NULL)) {
	    copy_source = fixture_path.c_str();
	    using_cached_fixture = 1;
	} else {
	    bu_log("Ignoring missing BRLCAD_QGMODEL_SCALE_FIXTURE=%s; "
		    "generating fixture from %s\n", fixture_path.c_str(),
		    g_path);
	}
    }
    bool copied = copy_file(copy_source, event_tmp.c_str());
    TCHECK(copied, "scale test creates private moss.g copy");
    if (!copied)
	return 1;

    QgModel *modelp = new QgModel(nullptr, event_tmp.c_str());
    QgModel &model = *modelp;
    QAbstractItemModel *amodel = &model;
    struct ged *gedp = model.ged();
    struct rt_wdb *wdbp = gedp ?
	wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT) : RT_WDB_NULL;

    QSignalSpy reset_spy(amodel, SIGNAL(modelReset()));
    QSignalSpy rows_inserted_spy(amodel,
	    SIGNAL(rowsInserted(QModelIndex,int,int)));
    QSignalSpy rows_removed_spy(amodel,
	    SIGNAL(rowsRemoved(QModelIndex,int,int)));
    QSignalSpy data_changed_spy(amodel,
	    &QAbstractItemModel::dataChanged);
    TCHECK(reset_spy.isValid(), "scale modelReset spy is valid");
    TCHECK(rows_inserted_spy.isValid(), "scale rowsInserted spy is valid");
    TCHECK(rows_removed_spy.isValid(), "scale rowsRemoved spy is valid");
    TCHECK(data_changed_spy.isValid(), "scale dataChanged spy is valid");
    TCHECK(gedp != nullptr && wdbp != RT_WDB_NULL,
	    "scale test opens model GED and wdb handles");

    std::vector<std::string> leaves;
    leaves.reserve(scale_child_count);
    std::vector<std::string> draw_leaves;
    draw_leaves.reserve((size_t)draw_batch_count);
    for (int i = 0; i < draw_batch_count; i++) {
	char member_name[64];
	std::snprintf(member_name, sizeof(member_name),
		"_qgmodel_scale_draw_leaf_%04d.r", i);
	draw_leaves.emplace_back(member_name);
    }

    int64_t setup_start_us = bu_gettime();
    if (!using_cached_fixture) {
	TCHECK(gedp && ged_event_batch_begin(gedp) > 0,
		"scale test starts child setup batch");
	int create_count = 0;
	for (int i = 0; i < scale_child_count; i++) {
	    char prim_name[64];
	    std::snprintf(prim_name, sizeof(prim_name),
		    "_qgmodel_scale_leaf_%04d.s", i);
	    leaves.emplace_back(prim_name);
	    point_t center = {1000.0 + (fastf_t)i, 0.0, 0.0};
	    if (wdbp && mk_sph(wdbp, prim_name, center, 1.0) == 0)
		create_count++;
	}
	TCHECK(create_count == scale_child_count,
		"scale test creates all leaf primitives");
	point_t extra_center = {1000.0 + (fastf_t)scale_child_count, 0.0, 0.0};
	TCHECK(wdbp && mk_sph(wdbp, scale_extra, extra_center, 1.0) == 0,
		"scale test creates extra leaf primitive");
	{
	    struct wmember wm;
	    BU_LIST_INIT(&wm.l);
	    int member_count = 0;
	    for (const std::string &leaf : leaves) {
		if (mk_addmember(leaf.c_str(), &wm.l, NULL, WMOP_UNION) != NULL)
		    member_count++;
	    }
	    TCHECK(member_count == scale_child_count,
		    "scale test queues comb members");
	    TCHECK(wdbp && mk_comb(wdbp, scale_parent, &wm.l, 0, NULL, NULL,
		    NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		    "scale test creates scale comb");
	}
	{
	    struct wmember wm;
	    BU_LIST_INIT(&wm.l);
	    int draw_prim_count = 0;
	    int draw_wrapper_count = 0;
	    int draw_member_count = 0;
	    for (int i = 0; i < draw_batch_count; i++) {
		char prim_name[64];
		std::snprintf(prim_name, sizeof(prim_name),
			"_qgmodel_scale_draw_leaf_%04d.s", i);
		point_t center = {5000.0 + (fastf_t)i, 0.0, 0.0};
		if (wdbp && mk_sph(wdbp, prim_name, center, 1.0) == 0)
		    draw_prim_count++;
		struct wmember child_wm;
		BU_LIST_INIT(&child_wm.l);
		if (wdbp && mk_addmember(prim_name, &child_wm.l, NULL,
			    WMOP_UNION) &&
			mk_comb(wdbp, draw_leaves[(size_t)i].c_str(),
			    &child_wm.l, 1, NULL, NULL, NULL, 0, 0,
			    0, 0, 0, 0, 0) == 0)
		    draw_wrapper_count++;
		if (mk_addmember(draw_leaves[(size_t)i].c_str(), &wm.l,
			NULL, WMOP_UNION) != NULL)
		    draw_member_count++;
	    }
	    TCHECK(draw_prim_count == draw_batch_count,
		    "scale test creates draw fixture primitives");
	    TCHECK(draw_wrapper_count == draw_batch_count,
		    "scale test creates draw fixture wrappers");
	    TCHECK(draw_member_count == draw_batch_count,
		    "scale test queues draw fixture members");
	    TCHECK(wdbp && mk_comb(wdbp, draw_parent, &wm.l, 0, NULL, NULL,
		    NULL, 0, 0, 0, 0, 0, 0, 0) == 0,
		    "scale test creates draw fixture comb");
	}
	struct ged_event_txn_result setup_result;
	ged_event_txn_result_init(&setup_result);
	TCHECK(gedp && ged_event_batch_end(gedp, &setup_result) >= 0,
		"scale test ends child setup batch");
	ged_event_txn_result_free(&setup_result);
	QCoreApplication::processEvents();
	QTest::qWait(1);
	QByteArray save_fixture_env =
	    qgetenv("BRLCAD_QGMODEL_SAVE_FIXTURE");
	if (!save_fixture_env.isEmpty()) {
	    TCHECK(copy_file(event_tmp.c_str(), save_fixture_env.constData()),
		    "scale test saves generated fixture copy");
	}
    } else {
	bu_log("Using cached QgModel scale fixture: %s\n",
		fixture_path.c_str());
    }
    int64_t setup_elapsed_us = bu_gettime() - setup_start_us;

    QgModel::HierarchyDeltaStats setup_stats = model.hierarchyDeltaStats();
    bu_log("BENCH qgmodel_scale_setup children=%d attempts=%llu "
	    "applied=%llu fallbacks=%llu rows=%llu insert_ranges=%llu "
	    "elapsed_us=%llu wall_us=%lld loaded_items=%llu fixture=%s\n",
	    scale_child_count,
	    (unsigned long long)setup_stats.attempts,
	    (unsigned long long)setup_stats.applied,
	    (unsigned long long)setup_stats.reset_fallbacks,
	    (unsigned long long)setup_stats.planned_child_rows,
	    (unsigned long long)setup_stats.inserted_row_ranges,
	    (unsigned long long)setup_stats.elapsed_us,
	    (long long)setup_elapsed_us,
	    (unsigned long long)model.allItems().size(),
	    using_cached_fixture ? "cached" : "generated");
    TCHECK(reset_spy.count() == 0,
	    "scale setup avoids hierarchy reset");
    if (using_cached_fixture) {
	TCHECK(setup_stats.reset_fallbacks == 0,
		"scale cached fixture starts without hierarchy reset fallback");
    } else {
	TCHECK(setup_stats.applied > 0 && setup_stats.reset_fallbacks == 0,
		"scale setup applies grouped structural delta");
    }

    QModelIndex parent_idx = find_top_level_item(amodel, scale_parent);
    TCHECK(parent_idx.isValid(), "scale test exposes scale parent");
    TCHECK(amodel->rowCount(parent_idx) == 0 && amodel->canFetchMore(parent_idx),
	    "scale parent remains lazy before explicit fetch");

    model.resetFetchMoreStats();
    int64_t fetch_start_us = bu_gettime();
    if (parent_idx.isValid() && amodel->canFetchMore(parent_idx))
	amodel->fetchMore(parent_idx);
    QCoreApplication::processEvents();
    QTest::qWait(1);
    int64_t fetch_wall_us = bu_gettime() - fetch_start_us;

    QgModel::FetchMoreStats fetch_stats = model.fetchMoreStats();
    bu_log("BENCH qgmodel_scale_fetch children=%d calls=%llu populated=%llu "
	    "rows=%llu ranges=%llu max_children=%llu elapsed_us=%llu "
	    "wall_us=%lld loaded_items=%llu\n",
	    scale_child_count,
	    (unsigned long long)fetch_stats.calls,
	    (unsigned long long)fetch_stats.populated_calls,
	    (unsigned long long)fetch_stats.inserted_rows,
	    (unsigned long long)fetch_stats.inserted_row_ranges,
	    (unsigned long long)fetch_stats.max_child_count,
	    (unsigned long long)fetch_stats.elapsed_us,
	    (long long)fetch_wall_us,
	    (unsigned long long)model.allItems().size());
    TCHECK(fetch_stats.populated_calls == 1 &&
	    fetch_stats.inserted_rows == (uint64_t)scale_child_count &&
	    fetch_stats.inserted_row_ranges == 1 &&
	    fetch_stats.max_child_count == (uint64_t)scale_child_count,
	    "scale fetchMore loads configured children in one row range");
    TCHECK(amodel->rowCount(parent_idx) == scale_child_count,
	    "scale parent has configured loaded child rows");

    QModelIndex draw_parent_idx = find_top_level_item(amodel, draw_parent);
    TCHECK(draw_parent_idx.isValid(), "scale test exposes draw fixture parent");
    int64_t draw_fetch_start_us = bu_gettime();
    if (draw_parent_idx.isValid() && amodel->canFetchMore(draw_parent_idx))
	amodel->fetchMore(draw_parent_idx);
    QCoreApplication::processEvents();
    QTest::qWait(1);
    int64_t draw_fetch_wall_us = bu_gettime() - draw_fetch_start_us;
    TCHECK(draw_parent_idx.isValid() &&
	    amodel->rowCount(draw_parent_idx) == draw_batch_count,
	    "scale draw fixture parent has loaded child rows");
    bu_log("BENCH qgmodel_scale_draw_fetch children=%d draw_paths=%d "
	    "wall_us=%lld loaded_items=%llu\n",
	    scale_child_count,
	    draw_batch_count,
	    (long long)draw_fetch_wall_us,
	    (unsigned long long)model.allItems().size());

    model.resetNotificationStats();
    struct ged_event_txn_result material_result;
    ged_event_txn_result_init(&material_result);
    int before_material_reset = reset_spy.count();
    int before_material_insert = rows_inserted_spy.count();
    int before_material_remove = rows_removed_spy.count();
    int before_material_data = data_changed_spy.count();
    int64_t material_start_us = bu_gettime();
    TCHECK(gedp && ged_event_notify_object_material_changed(gedp,
	    scale_parent, &material_result) >= 0,
	    "scale test publishes named material change for loaded scale comb");
    int64_t material_wall_us = bu_gettime() - material_start_us;
    TCHECK(std::string(bu_vls_cstr(&material_result.affected_names)).
	    find(scale_parent) != std::string::npos,
	    "scale material event reports affected path names");
    ged_event_txn_result_free(&material_result);
    QCoreApplication::processEvents();
    QTest::qWait(1);

    QgModel::NotificationStats material_notify_stats =
	model.notificationStats();
    bu_log("BENCH qgmodel_scale_material children=%d notify_queries=%llu "
	    "notify_candidates=%llu notify_fallbacks=%llu "
	    "items_notified=%llu full_items_notified=%llu "
	    "subtree_items_notified=%llu path_notify_us=%llu "
	    "wall_us=%lld loaded_items=%llu\n",
	    scale_child_count,
	    (unsigned long long)material_notify_stats.path_queries,
	    (unsigned long long)material_notify_stats.path_candidates,
	    (unsigned long long)material_notify_stats.path_fallback_scans,
	    (unsigned long long)material_notify_stats.items_notified,
	    (unsigned long long)material_notify_stats.full_items_notified,
	    (unsigned long long)material_notify_stats.subtree_items_notified,
	    (unsigned long long)material_notify_stats.path_notify_us,
	    (long long)material_wall_us,
	    (unsigned long long)model.allItems().size());
    TCHECK(reset_spy.count() == before_material_reset,
	    "scale material notification avoids hierarchy reset");
    TCHECK(rows_inserted_spy.count() == before_material_insert &&
	    rows_removed_spy.count() == before_material_remove,
	    "scale material notification emits no row operations");
    TCHECK(data_changed_spy.count() > before_material_data,
	    "scale material notification emits targeted dataChanged");
    TCHECK(material_notify_stats.path_fallback_scans == 0,
	    "scale material notification avoids full item path scans");
    TCHECK(material_notify_stats.path_queries > 0 &&
	    material_notify_stats.path_candidates > 0 &&
	    material_notify_stats.path_candidates < model.allItems().size(),
	    "scale material notification uses indexed loaded-row candidates");
    TCHECK(material_notify_stats.subtree_items_notified == 0 &&
	    material_notify_stats.items_notified <= 2,
	    "scale material notification avoids loaded-subtree repaint fan-out");

    model.resetNotificationStats();
    int before_attr_reset = reset_spy.count();
    int before_attr_insert = rows_inserted_spy.count();
    int before_attr_remove = rows_removed_spy.count();
    int before_attr_data = data_changed_spy.count();
    const char *scale_attr_av[6] = {"attr", "set", scale_parent,
	"qgmodel_scale_attr", "1", NULL};
    int64_t attr_start_us = bu_gettime();
    TCHECK(gedp && ged_exec(gedp, 5, scale_attr_av) == BRLCAD_OK,
	    "scale test publishes attr set command for loaded scale comb");
    int64_t attr_wall_us = bu_gettime() - attr_start_us;
    QCoreApplication::processEvents();
    QTest::qWait(1);

    QgModel::NotificationStats attr_notify_stats =
	model.notificationStats();
    bu_log("BENCH qgmodel_scale_attr children=%d notify_queries=%llu "
	    "notify_candidates=%llu notify_fallbacks=%llu "
	    "items_notified=%llu full_items_notified=%llu "
	    "subtree_items_notified=%llu path_notify_us=%llu "
	    "wall_us=%lld loaded_items=%llu\n",
	    scale_child_count,
	    (unsigned long long)attr_notify_stats.path_queries,
	    (unsigned long long)attr_notify_stats.path_candidates,
	    (unsigned long long)attr_notify_stats.path_fallback_scans,
	    (unsigned long long)attr_notify_stats.items_notified,
	    (unsigned long long)attr_notify_stats.full_items_notified,
	    (unsigned long long)attr_notify_stats.subtree_items_notified,
	    (unsigned long long)attr_notify_stats.path_notify_us,
	    (long long)attr_wall_us,
	    (unsigned long long)model.allItems().size());
    TCHECK(reset_spy.count() == before_attr_reset,
	    "scale attr notification avoids hierarchy reset");
    TCHECK(rows_inserted_spy.count() == before_attr_insert &&
	    rows_removed_spy.count() == before_attr_remove,
	    "scale attr notification emits no row operations");
    TCHECK(data_changed_spy.count() > before_attr_data,
	    "scale attr notification emits targeted dataChanged");
    TCHECK(attr_notify_stats.path_fallback_scans == 0,
	    "scale attr notification avoids full item path scans");
    TCHECK(attr_notify_stats.path_queries > 0 &&
	    attr_notify_stats.path_candidates > 0 &&
	    attr_notify_stats.path_candidates < model.allItems().size(),
	    "scale attr notification uses indexed loaded-row candidates");
    TCHECK(attr_notify_stats.subtree_items_notified == 0 &&
	    attr_notify_stats.items_notified <= 2,
	    "scale attr notification avoids loaded-subtree repaint fan-out");

    model.resetNotificationStats();
    int before_metadata_reset = reset_spy.count();
    int before_metadata_insert = rows_inserted_spy.count();
    int before_metadata_remove = rows_removed_spy.count();
    int before_metadata_data = data_changed_spy.count();
    const char *scale_title_av[3] = {"title", "qgmodel scale metadata", NULL};
    const char *scale_units_av[3] = {"units", "mm", NULL};
    int64_t metadata_start_us = bu_gettime();
    TCHECK(model.run_cmd(gedp->ged_result_str, 2, scale_title_av) == BRLCAD_OK,
	    "scale test publishes title metadata command");
    TCHECK(model.run_cmd(gedp->ged_result_str, 2, scale_units_av) == BRLCAD_OK,
	    "scale test publishes units metadata command");
    int64_t metadata_wall_us = bu_gettime() - metadata_start_us;
    QCoreApplication::processEvents();
    QTest::qWait(1);

    QgModel::NotificationStats metadata_notify_stats =
	model.notificationStats();
    bu_log("BENCH qgmodel_scale_metadata children=%d notify_queries=%llu "
	    "notify_candidates=%llu notify_fallbacks=%llu "
	    "items_notified=%llu full_items_notified=%llu "
	    "subtree_items_notified=%llu path_notify_us=%llu "
	    "wall_us=%lld loaded_items=%llu\n",
	    scale_child_count,
	    (unsigned long long)metadata_notify_stats.path_queries,
	    (unsigned long long)metadata_notify_stats.path_candidates,
	    (unsigned long long)metadata_notify_stats.path_fallback_scans,
	    (unsigned long long)metadata_notify_stats.items_notified,
	    (unsigned long long)metadata_notify_stats.full_items_notified,
	    (unsigned long long)metadata_notify_stats.subtree_items_notified,
	    (unsigned long long)metadata_notify_stats.path_notify_us,
	    (long long)metadata_wall_us,
	    (unsigned long long)model.allItems().size());
    TCHECK(reset_spy.count() == before_metadata_reset,
	    "scale metadata notification avoids hierarchy reset");
    TCHECK(rows_inserted_spy.count() == before_metadata_insert &&
	    rows_removed_spy.count() == before_metadata_remove,
	    "scale metadata notification emits no row operations");
    TCHECK(data_changed_spy.count() == before_metadata_data,
	    "scale metadata notification emits no item dataChanged");
    TCHECK(metadata_notify_stats.path_queries == 0 &&
	    metadata_notify_stats.path_candidates == 0 &&
	    metadata_notify_stats.path_fallback_scans == 0,
	    "scale metadata notification avoids path lookup work");
    TCHECK(metadata_notify_stats.items_notified == 0 &&
	    metadata_notify_stats.full_items_notified == 0 &&
	    metadata_notify_stats.subtree_items_notified == 0,
	    "scale metadata notification avoids row repaint fan-out");

    QPersistentModelIndex parent_persistent(parent_idx);
    model.resetHierarchyDeltaStats();
    model.resetNotificationStats();
    const char *scale_add_av[4] = {"g", scale_parent, scale_extra, NULL};
    int before_add_reset = reset_spy.count();
    int before_add_insert = rows_inserted_spy.count();
    int before_add_remove = rows_removed_spy.count();
    int64_t delta_start_us = bu_gettime();
    TCHECK(gedp && ged_exec(gedp, 3, scale_add_av) == BRLCAD_OK,
	    "scale test adds one child to loaded scale comb");
    QCoreApplication::processEvents();
    QTest::qWait(1);
    int64_t delta_wall_us = bu_gettime() - delta_start_us;

    QgModel::HierarchyDeltaStats delta_stats = model.hierarchyDeltaStats();
    QgModel::NotificationStats delta_notify_stats =
	model.notificationStats();
    bu_log("BENCH qgmodel_scale_delta children=%d attempts=%llu "
	    "applied=%llu fallbacks=%llu parents=%llu rows=%llu inserts=%llu "
	    "removes=%llu insert_ranges=%llu remove_ranges=%llu notify_queries=%llu "
	    "notify_candidates=%llu notify_fallbacks=%llu "
	    "items_notified=%llu subtree_items_notified=%llu "
	    "path_notify_us=%llu elapsed_us=%llu wall_us=%lld "
	    "loaded_items=%llu\n",
	    scale_child_count,
	    (unsigned long long)delta_stats.attempts,
	    (unsigned long long)delta_stats.applied,
	    (unsigned long long)delta_stats.reset_fallbacks,
	    (unsigned long long)delta_stats.planned_parents,
	    (unsigned long long)delta_stats.planned_child_rows,
	    (unsigned long long)delta_stats.inserted_rows,
	    (unsigned long long)delta_stats.removed_rows,
	    (unsigned long long)delta_stats.inserted_row_ranges,
	    (unsigned long long)delta_stats.removed_row_ranges,
	    (unsigned long long)delta_notify_stats.path_queries,
	    (unsigned long long)delta_notify_stats.path_candidates,
	    (unsigned long long)delta_notify_stats.path_fallback_scans,
	    (unsigned long long)delta_notify_stats.items_notified,
	    (unsigned long long)delta_notify_stats.subtree_items_notified,
	    (unsigned long long)delta_notify_stats.path_notify_us,
	    (unsigned long long)delta_stats.elapsed_us,
	    (long long)delta_wall_us,
	    (unsigned long long)model.allItems().size());
    TCHECK(reset_spy.count() == before_add_reset,
	    "scale delta avoids hierarchy reset");
    TCHECK(rows_inserted_spy.count() > before_add_insert &&
	    rows_removed_spy.count() > before_add_remove,
	    "scale delta emits row operations");
    TCHECK(delta_stats.applied > 0 && delta_stats.reset_fallbacks == 0,
	    "scale delta applies grouped structural update");
    TCHECK(delta_stats.db_index_refreshes == delta_stats.attempts,
	    "scale delta refreshes the database index once per attempt");
    TCHECK(delta_stats.planned_child_rows >= (uint64_t)scale_child_count &&
	    delta_stats.planned_child_rows <= model.allItems().size(),
	    "scale delta planning is bounded by loaded rows");
    TCHECK(delta_stats.inserted_row_ranges <= 2 &&
	    delta_stats.removed_row_ranges <= 2,
	    "scale delta emits bounded row operation ranges");
    TCHECK(delta_notify_stats.path_fallback_scans == 0,
	    "scale delta notification avoids full item path scans");
    TCHECK(parent_persistent.isValid(),
	    "scale delta preserves parent persistent index");
    parent_idx = find_top_level_item(amodel, scale_parent);
    TCHECK(parent_idx.isValid() &&
	    amodel->rowCount(parent_idx) == scale_child_count + 1 &&
	    find_child_item(amodel, parent_idx, scale_extra).isValid(),
	    "scale delta inserts extra child under loaded scale parent");

    model.resetHierarchyDeltaStats();
    model.resetNotificationStats();
    std::string scale_rm_path = std::string(scale_parent) + "/" + scale_extra;
    const char *scale_rm_av[3] = {"rm", scale_rm_path.c_str(), NULL};
    int before_rm_reset = reset_spy.count();
    int before_rm_insert = rows_inserted_spy.count();
    int before_rm_remove = rows_removed_spy.count();
    int64_t rm_start_us = bu_gettime();
    TCHECK(gedp && ged_exec(gedp, 2, scale_rm_av) == BRLCAD_OK,
	    "scale test removes one child from loaded scale comb");
    QCoreApplication::processEvents();
    QTest::qWait(1);
    int64_t rm_wall_us = bu_gettime() - rm_start_us;

    QgModel::HierarchyDeltaStats rm_stats = model.hierarchyDeltaStats();
    QgModel::NotificationStats rm_notify_stats = model.notificationStats();
    bu_log("BENCH qgmodel_scale_remove children=%d attempts=%llu "
	    "applied=%llu fallbacks=%llu parents=%llu rows=%llu inserts=%llu "
	    "removes=%llu insert_ranges=%llu remove_ranges=%llu notify_queries=%llu "
	    "notify_candidates=%llu notify_fallbacks=%llu "
	    "items_notified=%llu subtree_items_notified=%llu "
	    "path_notify_us=%llu elapsed_us=%llu wall_us=%lld "
	    "loaded_items=%llu\n",
	    scale_child_count,
	    (unsigned long long)rm_stats.attempts,
	    (unsigned long long)rm_stats.applied,
	    (unsigned long long)rm_stats.reset_fallbacks,
	    (unsigned long long)rm_stats.planned_parents,
	    (unsigned long long)rm_stats.planned_child_rows,
	    (unsigned long long)rm_stats.inserted_rows,
	    (unsigned long long)rm_stats.removed_rows,
	    (unsigned long long)rm_stats.inserted_row_ranges,
	    (unsigned long long)rm_stats.removed_row_ranges,
	    (unsigned long long)rm_notify_stats.path_queries,
	    (unsigned long long)rm_notify_stats.path_candidates,
	    (unsigned long long)rm_notify_stats.path_fallback_scans,
	    (unsigned long long)rm_notify_stats.items_notified,
	    (unsigned long long)rm_notify_stats.subtree_items_notified,
	    (unsigned long long)rm_notify_stats.path_notify_us,
	    (unsigned long long)rm_stats.elapsed_us,
	    (long long)rm_wall_us,
	    (unsigned long long)model.allItems().size());
    TCHECK(reset_spy.count() == before_rm_reset,
	    "scale remove delta avoids hierarchy reset");
    TCHECK(rows_inserted_spy.count() > before_rm_insert &&
	    rows_removed_spy.count() > before_rm_remove,
	    "scale remove delta emits row operations");
    TCHECK(rm_stats.applied > 0 && rm_stats.reset_fallbacks == 0,
	    "scale remove delta applies grouped structural update");
    TCHECK(rm_stats.db_index_refreshes == rm_stats.attempts,
	    "scale remove delta refreshes the database index once per attempt");
    TCHECK(rm_stats.planned_child_rows >= (uint64_t)scale_child_count &&
	    rm_stats.planned_child_rows <= model.allItems().size(),
	    "scale remove delta planning is bounded by loaded rows");
    TCHECK(rm_stats.inserted_row_ranges <= 2 &&
	    rm_stats.removed_row_ranges <= 2,
	    "scale remove delta emits bounded row operation ranges");
    TCHECK(rm_notify_stats.path_fallback_scans == 0,
	    "scale remove delta notification avoids full item path scans");
    TCHECK(parent_persistent.isValid(),
	    "scale remove delta preserves parent persistent index");
    parent_idx = find_top_level_item(amodel, scale_parent);
    TCHECK(parent_idx.isValid() &&
	    amodel->rowCount(parent_idx) == scale_child_count &&
	    !find_child_item(amodel, parent_idx, scale_extra).isValid(),
	    "scale remove delta removes extra child from loaded scale parent");
    TCHECK(find_top_level_item(amodel, scale_extra).isValid(),
	    "scale remove delta returns extra primitive to top level");

    std::vector<std::string> draw_paths;
    std::vector<const char *> erase_cmd_av;
    draw_paths.reserve((size_t)draw_batch_count);
    erase_cmd_av.reserve((size_t)draw_batch_count + 2);
    erase_cmd_av.push_back("erase");
    for (int i = 0; i < draw_batch_count; i++) {
	draw_paths.push_back(std::string(draw_parent) + "/" +
		draw_leaves[(size_t)i]);
	erase_cmd_av.push_back(draw_paths.back().c_str());
    }
    erase_cmd_av.push_back(NULL);
    bu_log("BENCH qgmodel_scale_draw_config children=%d draw_paths=%d\n",
	    scale_child_count, draw_batch_count);

    model.resetNotificationStats();
    model.resetDrawTimingStats();
    model.setDrawTimingStatsEnabled(true);
    int before_draw_reset = reset_spy.count();
    int before_draw_insert = rows_inserted_spy.count();
    int before_draw_remove = rows_removed_spy.count();
    int before_draw_data = data_changed_spy.count();
    int64_t draw_start_us = bu_gettime();
    TCHECK(model.drawPaths(draw_paths) == BRLCAD_OK,
	    "scale test draws loaded paths through QgModel batch");
    int64_t draw_loop_wall_us = bu_gettime() - draw_start_us;
    int64_t draw_drain_start_us = bu_gettime();
    QCoreApplication::processEvents();
    QTest::qWait(1);
    int64_t draw_event_drain_us = bu_gettime() - draw_drain_start_us;
    int64_t draw_wall_us = draw_loop_wall_us + draw_event_drain_us;

    QgModel::NotificationStats draw_notify_stats =
	model.notificationStats();
    QgModel::DrawTimingStats draw_timing_stats =
	model.drawTimingStats();
    bu_log("BENCH qgmodel_scale_draw_batch children=%d paths=%d "
	    "notify_queries=%llu notify_candidates=%llu "
	    "notify_fallbacks=%llu items_notified=%llu "
	    "path_notify_us=%llu wall_us=%lld loaded_items=%llu\n",
	    scale_child_count,
	    draw_batch_count,
	    (unsigned long long)draw_notify_stats.path_queries,
	    (unsigned long long)draw_notify_stats.path_candidates,
	    (unsigned long long)draw_notify_stats.path_fallback_scans,
	    (unsigned long long)draw_notify_stats.items_notified,
	    (unsigned long long)draw_notify_stats.path_notify_us,
	    (long long)draw_wall_us,
	    (unsigned long long)model.allItems().size());
    bu_log("BENCH qgmodel_scale_draw_timing children=%d paths=%d "
	    "calls=%llu ok=%llu loop_wall_us=%lld event_drain_us=%lld "
	    "blank_slate_us=%llu txn_us=%llu observer_us=%llu "
	    "notify_path_us=%llu notify_all_us=%llu view_signal_us=%llu\n",
	    scale_child_count,
	    draw_batch_count,
	    (unsigned long long)draw_timing_stats.draw_calls,
	    (unsigned long long)draw_timing_stats.successful_draw_calls,
	    (long long)draw_loop_wall_us,
	    (long long)draw_event_drain_us,
	    (unsigned long long)draw_timing_stats.blank_slate_check_us,
	    (unsigned long long)draw_timing_stats.transaction_us,
	    (unsigned long long)draw_timing_stats.observer_callback_us,
	    (unsigned long long)draw_timing_stats.notify_path_us,
	    (unsigned long long)draw_timing_stats.notify_all_us,
	    (unsigned long long)draw_timing_stats.view_signal_us);
    TCHECK(reset_spy.count() == before_draw_reset,
	    "scale draw batch notification avoids hierarchy reset");
    TCHECK(rows_inserted_spy.count() == before_draw_insert &&
	    rows_removed_spy.count() == before_draw_remove,
	    "scale draw batch notification emits no row operations");
    TCHECK(data_changed_spy.count() > before_draw_data,
	    "scale draw batch notification emits targeted dataChanged");
    TCHECK(draw_notify_stats.path_fallback_scans == 0,
	    "scale draw batch notification avoids full item path scans");
    TCHECK(draw_notify_stats.path_queries > 0 &&
	    draw_notify_stats.path_candidates > 0 &&
	    draw_notify_stats.path_candidates <= (uint64_t)draw_batch_count * 3,
	    "scale draw batch notification uses indexed loaded-row candidates");
    draw_parent_idx = find_top_level_item(amodel, draw_parent);
    TCHECK(draw_parent_idx.isValid() &&
	    amodel->data(draw_parent_idx, DrawnDisplayRole).toInt() != 0,
	    "scale draw batch updates DrawnDisplayRole on loaded draw parent");
    if (draw_parent_idx.isValid() && draw_batch_count > 0) {
	QModelIndex first_child_idx = find_child_item(amodel, draw_parent_idx,
		draw_leaves.front().c_str());
	TCHECK(first_child_idx.isValid() &&
		amodel->data(first_child_idx, DrawnDisplayRole).toInt() == 1,
		"scale draw batch updates DrawnDisplayRole on loaded child");
    }

    model.resetNotificationStats();
    model.resetDrawTimingStats();
    int before_erase_reset = reset_spy.count();
    int before_erase_insert = rows_inserted_spy.count();
    int before_erase_remove = rows_removed_spy.count();
    int before_erase_data = data_changed_spy.count();
    int64_t erase_start_us = bu_gettime();
    TCHECK(model.run_cmd(gedp->ged_result_str,
		(int)erase_cmd_av.size() - 1, erase_cmd_av.data()) == BRLCAD_OK,
	    "scale test erases batched loaded paths through QgModel");
    int64_t erase_cmd_wall_us = bu_gettime() - erase_start_us;
    int64_t erase_drain_start_us = bu_gettime();
    QCoreApplication::processEvents();
    QTest::qWait(1);
    int64_t erase_event_drain_us = bu_gettime() - erase_drain_start_us;
    int64_t erase_wall_us = erase_cmd_wall_us + erase_event_drain_us;

    QgModel::NotificationStats erase_notify_stats =
	model.notificationStats();
    QgModel::DrawTimingStats erase_timing_stats =
	model.drawTimingStats();
    bu_log("BENCH qgmodel_scale_erase_batch children=%d paths=%d "
	    "notify_queries=%llu notify_candidates=%llu "
	    "notify_fallbacks=%llu items_notified=%llu "
	    "path_notify_us=%llu wall_us=%lld loaded_items=%llu\n",
	    scale_child_count,
	    draw_batch_count,
	    (unsigned long long)erase_notify_stats.path_queries,
	    (unsigned long long)erase_notify_stats.path_candidates,
	    (unsigned long long)erase_notify_stats.path_fallback_scans,
	    (unsigned long long)erase_notify_stats.items_notified,
	    (unsigned long long)erase_notify_stats.path_notify_us,
	    (long long)erase_wall_us,
	    (unsigned long long)model.allItems().size());
    bu_log("BENCH qgmodel_scale_erase_timing children=%d paths=%d "
	    "command_wall_us=%lld event_drain_us=%lld observer_us=%llu "
	    "notify_path_us=%llu notify_all_us=%llu\n",
	    scale_child_count,
	    draw_batch_count,
	    (long long)erase_cmd_wall_us,
	    (long long)erase_event_drain_us,
	    (unsigned long long)erase_timing_stats.observer_callback_us,
	    (unsigned long long)erase_timing_stats.notify_path_us,
	    (unsigned long long)erase_timing_stats.notify_all_us);
    TCHECK(reset_spy.count() == before_erase_reset,
	    "scale erase batch notification avoids hierarchy reset");
    TCHECK(rows_inserted_spy.count() == before_erase_insert &&
	    rows_removed_spy.count() == before_erase_remove,
	    "scale erase batch notification emits no row operations");
    TCHECK(data_changed_spy.count() > before_erase_data,
	    "scale erase batch notification emits targeted dataChanged");
    TCHECK(erase_notify_stats.path_fallback_scans == 0,
	    "scale erase batch notification avoids full item path scans");
    TCHECK(erase_notify_stats.path_queries > 0 &&
	    erase_notify_stats.path_candidates > 0 &&
	    erase_notify_stats.path_candidates <= (uint64_t)draw_batch_count * 3,
	    "scale erase batch notification uses indexed loaded-row candidates");
    draw_parent_idx = find_top_level_item(amodel, draw_parent);
    TCHECK(draw_parent_idx.isValid() &&
	    amodel->data(draw_parent_idx, DrawnDisplayRole).toInt() == 0,
	    "scale erase batch clears DrawnDisplayRole on loaded draw parent");
    if (draw_parent_idx.isValid() && draw_batch_count > 0) {
	QModelIndex first_child_idx = find_child_item(amodel, draw_parent_idx,
		draw_leaves.front().c_str());
	TCHECK(first_child_idx.isValid() &&
		amodel->data(first_child_idx, DrawnDisplayRole).toInt() == 0,
		"scale erase batch clears DrawnDisplayRole on loaded child");
    }

    uint64_t loaded_before_teardown = (uint64_t)model.allItems().size();
    bu_log("BENCH qgmodel_scale_before_model_delete children=%d "
	    "draw_paths=%d loaded_items=%llu\n",
	    scale_child_count, draw_batch_count,
	    (unsigned long long)loaded_before_teardown);
    int64_t model_delete_start_us = bu_gettime();
    delete modelp;
    modelp = nullptr;
    int64_t model_delete_us = bu_gettime() - model_delete_start_us;
    bu_log("BENCH qgmodel_scale_model_delete children=%d draw_paths=%d "
	    "loaded_items=%llu elapsed_us=%lld\n",
	    scale_child_count, draw_batch_count,
	    (unsigned long long)loaded_before_teardown,
	    (long long)model_delete_us);

    bu_file_delete(event_tmp.c_str());

    if (g_fail) {
	bu_log("\ntest_qgmodel_scale: FAILED (%d check(s))\n", g_fail);
	return 1;
    }

    bu_log("\ntest_qgmodel_scale: PASSED\n");
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

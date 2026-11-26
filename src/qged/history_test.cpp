/*                   H I S T O R Y _ T E S T . C P P
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

#include <initializer_list>
#include <memory>
#include <vector>

#include <QCheckBox>
#include <QEvent>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>

#include "bu/app.h"
#include "bu/str.h"
#include "ged/history.h"

#include "QgEdApp.h"
#include "QgEdMainWindow.h"

namespace {

#define CHECK(condition) do { \
    if (!(condition)) { \
	bu_log("QGED history test failed at line %d: %s\n", __LINE__, #condition); \
	return false; \
    } \
} while (0)

using Status = std::unique_ptr<const struct ged_history_status, decltype(&ged_history_status_destroy)>;

Status
history_status(struct ged *gedp)
{
    return Status(ged_history_status_create(gedp, nullptr), ged_history_status_destroy);
}

QAction *
find_action(QgEdMainWindow &window, const QString &prefix)
{
    for (auto *action : window.findChildren<QAction *>()) {
	if (action->text().startsWith(prefix))
	    return action;
    }
    return nullptr;
}

bool
run(QgEdApp &app, std::initializer_list<const char *> command)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    std::vector<const char *> argv(command);
    const int ret = app.run_cmd(&result, static_cast<int>(argv.size()), argv.data());
    if (ret != BRLCAD_OK)
	bu_log("QGED command %s failed: %s\n", argv.front(), bu_vls_cstr(&result));
    bu_vls_free(&result);
    return ret == BRLCAD_OK;
}

template <typename Dialog, typename Inspect>
bool
inspect_dialog(QAction &action, Inspect inspect)
{
    bool passed = false;
    QTimer inspect_timer;
    inspect_timer.setSingleShot(true);
    QObject::connect(&inspect_timer, &QTimer::timeout, [&]() {
	auto *active = qobject_cast<QDialog *>(QApplication::activeModalWidget());
	if (auto *dialog = qobject_cast<Dialog *>(active))
	    passed = inspect(*dialog);
	if (active && active->isVisible())
	    active->reject();
    });
    inspect_timer.start(0);
    action.trigger();
    return passed;
}

bool
save_from_menu(QAction &action, const QString &path, bool include_history)
{
    bool saved = false;
    QTimer response_timer;
    response_timer.setSingleShot(true);
    QObject::connect(&response_timer, &QTimer::timeout, [&]() {
	auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
	if (auto *response = qobject_cast<QMessageBox *>(dialog))
	    saved = response->icon() == QMessageBox::Information;
	if (dialog)
	    dialog->reject();
    });

    const bool selected = inspect_dialog<QFileDialog>(action, [&](QFileDialog &dialog) {
	if (include_history) {
	    const auto filters = dialog.nameFilters();
	    CHECK(filters.size() == 2 && filters.back().contains("with history"));
	    dialog.selectNameFilter(filters.back());
	}
	dialog.selectFile(path);
	response_timer.start(0);
	static_cast<QDialog &>(dialog).accept();
	return true;
    });
    return selected && saved;
}

bool
compare_revisions(QDialog &dialog, const QString &revision)
{
    QTreeWidget *revisions = nullptr;
    QTreeWidget *changes = nullptr;
    for (auto *tree : dialog.findChildren<QTreeWidget *>()) {
	if (tree->headerItem()->text(0) == "Operation")
	    revisions = tree;
	if (tree->headerItem()->text(0) == "Change")
	    changes = tree;
    }
    auto *comparison = dialog.findChild<QCheckBox *>();
    CHECK(revisions && changes && comparison && comparison->isEnabled());
    QTreeWidgetItem *selected = nullptr;
    for (int i = 0; i < revisions->topLevelItemCount(); ++i) {
	auto *row = revisions->topLevelItem(i);
	if (row->data(1, Qt::UserRole).toString() == revision) {
	    selected = row;
	    break;
	}
    }
    CHECK(selected);
    revisions->setCurrentItem(selected);
    comparison->setChecked(true);
    CHECK(changes->topLevelItemCount() == 2);
    bool created_first = false;
    bool deleted_second = false;
    for (int i = 0; i < changes->topLevelItemCount(); ++i) {
	const auto *change = changes->topLevelItem(i);
	created_first |= change->text(0) == "Create" && change->text(1) == "first.s";
	deleted_second |= change->text(0) == "Delete" && change->text(1) == "second.s";
    }
    CHECK(created_first && deleted_second);

    bool found_checkout = false;
    for (const auto *button : dialog.findChildren<QPushButton *>()) {
	if (button->text() == "Check Out Selected Revision") {
	    CHECK(!button->isEnabled());
	    found_checkout = true;
	}
    }
    return found_checkout;
}

bool
check_export(const QByteArray &path, const QString &current_revision)
{
    const std::unique_ptr<struct ged, decltype(&ged_close)> reader(ged_create(), ged_close);
    const char *open[] = {"db", "open", "-r", path.constData()};
    CHECK(reader && ged_exec(reader.get(), 4, open) == BRLCAD_OK);
    const auto status = history_status(reader.get());
    CHECK(status && status->mode == GED_HISTORY_EMBEDDED &&
	QString::fromLatin1(status->selected_revision) == current_revision);
    CHECK(db_lookup(reader->dbip, "second.s", LOOKUP_QUIET) && !db_lookup(reader->dbip, "first.s", LOOKUP_QUIET));
    const char *verify[] = {"db", "history", "verify"};
    CHECK(ged_exec(reader.get(), 3, verify) == BRLCAD_OK);
    return true;
}

bool
check_db5_activation(QgEdApp &app, QAction &undo)
{
    auto *gedp = app.mdl->gedp;
    const QByteArray source(gedp->dbip->dbi_filename);
    const auto before = history_status(gedp);
    CHECK(before && before->selected_revision && undo.isEnabled());
    const std::unique_ptr<struct ged, decltype(&ged_close)> writer(ged_open("db", source.constData(), 1), ged_close);
    const char *title[] = {"title", "Changed by another editor"};
    CHECK(writer && ged_exec(writer.get(), 2, title) == BRLCAD_OK);

    QEvent activation(QEvent::WindowActivate);
    QCoreApplication::sendEvent(app.w, &activation);
    const auto stale = history_status(gedp);
    CHECK(stale && stale->state == GED_HISTORY_STALE && !stale->staged_conflict && stale->selected_revision);
    CHECK(BU_STR_EQUAL(stale->selected_revision, before->selected_revision) && !undo.isEnabled());
    CHECK(run(app, {"db", "open", source.constData()}));
    CHECK(BU_STR_EQUAL(gedp->dbip->dbi_title, title[1]) && undo.isEnabled());
    return true;
}

bool
exercise(QgEdApp &app, const QTemporaryDir &directory)
{
    const auto workspace = directory.filePath("History workspace");
    const auto encoded_workspace = workspace.toLocal8Bit();
    const auto output = directory.filePath("History snapshot.g");
    auto &window = *app.w;
    auto *gedp = app.mdl->gedp;
    auto *session = find_action(window, "Enable Session Undo");
    auto *embed = find_action(window, "Save History in This File");
    auto *undo = find_action(window, "Undo");
    auto *redo = find_action(window, "Redo");
    auto *create = find_action(window, "Create Workspace");
    auto *history = find_action(window, "History");
    auto *publish = find_action(window, "Export DB5 Snapshot");
    CHECK(session && embed && undo && redo && create && history && publish);
    CHECK(session->isEnabled() && embed->isEnabled() && !undo->isEnabled());

    session->trigger();
    const auto journal = history_status(gedp);
    CHECK(journal && journal->mode == GED_HISTORY_SESSION && !session->isEnabled());
    app.run_qcmd("make model.s sph");
    CHECK(db_lookup(gedp->dbip, "model.s", LOOKUP_QUIET) && undo->isEnabled());
    CHECK(undo->text().startsWith("Undo "));
    undo->trigger();
    CHECK(!db_lookup(gedp->dbip, "model.s", LOOKUP_QUIET) && redo->isEnabled());
    CHECK(inspect_dialog<QMessageBox>(*embed, [](QMessageBox &dialog) {
	return dialog.icon() == QMessageBox::Information;
    }));
    CHECK(redo->isEnabled() && !embed->isEnabled());
    redo->trigger();
    CHECK(db_lookup(gedp->dbip, "model.s", LOOKUP_QUIET));
    CHECK(check_db5_activation(app, *undo));

    CHECK(save_from_menu(*create, workspace, false));
    CHECK(QFileInfo(window.windowFilePath()).canonicalFilePath() == QFileInfo(workspace).canonicalFilePath());
    CHECK(create->text().startsWith("Copy Workspace"));
    CHECK(run(app, {"make", "first.s", "sph"}));
    const auto first = history_status(gedp);
    CHECK(first && first->mode == GED_HISTORY_WORKSPACE && first->selected_revision);
    CHECK(run(app, {"db", "undo"}) && run(app, {"make", "second.s", "sph"}));
    const auto second = history_status(gedp);
    CHECK(second && second->selected_revision);
    const QString second_id = QString::fromLatin1(second->selected_revision);
    CHECK(run(app, {"db", "open", "-r", encoded_workspace.constData()}));
    CHECK(history->isEnabled() && publish->isEnabled() && !undo->isEnabled() && !redo->isEnabled());
    CHECK(inspect_dialog<QDialog>(*history, [&](QDialog &dialog) {
	return compare_revisions(dialog, QString::fromLatin1(first->selected_revision));
    }));
    CHECK(save_from_menu(*publish, output, true));
    CHECK(check_export(output.toLocal8Bit(), second_id));
    const auto final_status = history_status(gedp);
    CHECK(final_status && !final_status->writable && QString::fromLatin1(final_status->selected_revision) == second_id);
    CHECK(run(app, {"db", "close"}));
    CHECK(!history->isEnabled() && !publish->isEnabled() && window.windowFilePath().isEmpty());
    return true;
}

} // namespace

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    QTemporaryDir directory;
    if (!directory.isValid()) {
	bu_log("Could not create the QGED test directory\n");
	return 1;
    }
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, directory.path());
    qputenv("BU_DIR_CACHE", directory.path().toLocal8Bit());
    qputenv("LIBRT_CACHE", directory.path().toLocal8Bit());

    const QByteArray source = directory.filePath("History source.g").toLocal8Bit();
    auto *database = db_create(source.constData(), 5);
    if (!database) {
	bu_log("Could not create the QGED test database\n");
	return 1;
    }
    db_close(database);
    QgEdApp app(argc, argv, source.constData(), 1);

    const QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "BRL-CAD", "QGED");
    if (!QFileInfo(settings.fileName()).absoluteFilePath().startsWith(directory.path() + "/")) {
	bu_log("QGED settings are outside the test directory\n");
	return 1;
    }
    QCoreApplication::processEvents();
    return exercise(app, directory) ? 0 : 1;
}

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// End:
// ex: shiftwidth=4 tabstop=8

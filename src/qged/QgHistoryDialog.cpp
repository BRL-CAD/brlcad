/*                  Q G H I S T O R Y D I A L O G . C P P
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

#include "common.h"

#include <memory>

#include <QDialog>
#include <QDialogButtonBox>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "bu/vls.h"
#include "ged/history.h"
#include "qtcad/QgView.h"

#include "QgEdApp.h"
#include "QgEdMainWindow.h"
#include "../libged/dbi.h"

namespace {

class HistoryDialog : public QDialog {
public:
    HistoryDialog(QgEdApp *app, QWidget *parent);

private:
    enum RevisionColumn {Operation, Revision, Parent, Checkpoints};

    static int addRevision(const struct ged_history_revision *revision, void *data);
    static int addCheckpoint(const char *name, const char *revision, void *data);
    static int addChange(const struct ged_history_change *change, void *data);

    void refresh();
    void updateSelection();
    void previewRevision();
    void markRevision();
    void runCommand(int argc, const char **argv);
    QString selectedRevision() const;

    QgEdApp *app;
    QTreeWidget *revisions;
    QTreeWidget *changes;
    QLabel *message;
    QPushButton *checkout;
    QPushButton *mark;
    QPushButton *preview;
    QHash<QString, QTreeWidgetItem *> revisionItems;
    QSet<QString> checkpoints;
    QString currentRevision;
    bool writable = false;
};

HistoryDialog::HistoryDialog(QgEdApp *application, QWidget *parent) :
    QDialog(parent), app(application)
{
    setWindowTitle(tr("Database History"));

    auto *layout = new QVBoxLayout(this);
    revisions = new QTreeWidget(this);
    revisions->setRootIsDecorated(false);
    revisions->setHeaderLabels({tr("Operation"), tr("Revision"), tr("Parent"), tr("Checkpoints")});
    revisions->header()->setSectionResizeMode(Operation, QHeaderView::Stretch);
    layout->addWidget(revisions);

    layout->addWidget(new QLabel(tr("Changed objects"), this));
    changes = new QTreeWidget(this);
    changes->setRootIsDecorated(false);
    changes->setHeaderLabels({tr("Change"), tr("Object"), tr("Rename destination")});
    changes->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(changes);

    message = new QLabel(this);
    message->setWordWrap(true);
    message->hide();
    layout->addWidget(message);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    preview = buttons->addButton(tr("Preview Selected Revision…"), QDialogButtonBox::ActionRole);
    checkout = buttons->addButton(tr("Check Out Selected Revision"), QDialogButtonBox::ActionRole);
    mark = buttons->addButton(tr("Mark Current Revision…"), QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(revisions, &QTreeWidget::itemSelectionChanged, this, &HistoryDialog::updateSelection);
    connect(app, &QgEdApp::history_update, this, &HistoryDialog::refresh);
    connect(mark, &QPushButton::clicked, this, &HistoryDialog::markRevision);
    connect(preview, &QPushButton::clicked, this, &HistoryDialog::previewRevision);
    connect(checkout, &QPushButton::clicked, this, [this]() {
	const QByteArray id = selectedRevision().toLatin1();
	const char *argv[] = {"db", "history", "checkout", id.constData()};
	runCommand(sizeof(argv) / sizeof(argv[0]), argv);
    });
    layout->addWidget(buttons);
    resize(parent->width() * 2 / 3, parent->height() * 2 / 3);
    refresh();
}

int
HistoryDialog::addRevision(const struct ged_history_revision *revision, void *data)
{
    auto *dialog = static_cast<HistoryDialog *>(data);
    const QString id = QString::fromLatin1(revision->id);
    const QString parent = QString::fromLatin1(revision->parent);
    auto *item = new QTreeWidgetItem(dialog->revisions,
	{QString::fromUtf8(revision->label), id.left(12), parent.left(12), {}});
    item->setData(Revision, Qt::UserRole, id);
    item->setToolTip(Revision, id);
    if (revision->retention_boundary)
	item->setText(Parent, tr("Retention boundary"));
    else
	item->setToolTip(Parent, parent);

    if (revision->selected) {
	dialog->currentRevision = id;
	for (int column = 0; column < dialog->revisions->columnCount(); ++column) {
	    QFont font = item->font(column);
	    font.setBold(true);
	    item->setFont(column, font);
	}
    }
    dialog->revisionItems.insert(id, item);
    return BRLCAD_OK;
}

int
HistoryDialog::addCheckpoint(const char *name, const char *revision, void *data)
{
    auto *dialog = static_cast<HistoryDialog *>(data);
    const QString label = QString::fromUtf8(name);
    dialog->checkpoints.insert(label);
    auto *item = dialog->revisionItems.value(QString::fromLatin1(revision));
    if (item) {
	const QString existing = item->text(Checkpoints);
	item->setText(Checkpoints, existing.isEmpty() ? label : existing + ", " + label);
    }
    return BRLCAD_OK;
}

int
HistoryDialog::addChange(const struct ged_history_change *change, void *data)
{
    auto *dialog = static_cast<HistoryDialog *>(data);
    QString action;
    switch (change->type) {
	case GED_HISTORY_CREATE: action = tr("Create"); break;
	case GED_HISTORY_DELETE: action = tr("Delete"); break;
	case GED_HISTORY_UPDATE: action = tr("Update"); break;
	case GED_HISTORY_RENAME: action = tr("Rename"); break;
    }
    new QTreeWidgetItem(dialog->changes,
	{action, QString::fromUtf8(change->name), QString::fromUtf8(change->new_name)});
    return BRLCAD_OK;
}

QString
HistoryDialog::selectedRevision() const
{
    auto *item = revisions->currentItem();
    return item ? item->data(Revision, Qt::UserRole).toString() : QString();
}

void
HistoryDialog::refresh()
{
    const QSignalBlocker block(revisions);
    const QString previous = selectedRevision();
    revisions->clear();
    revisionItems.clear();
    checkpoints.clear();
    currentRevision.clear();

    auto *gedp = app->mdl->gedp;
    struct bu_vls diagnostic = BU_VLS_INIT_ZERO;
    const std::unique_ptr<const struct ged_history_status, decltype(&ged_history_status_destroy)>
	status(ged_history_status_create(gedp, &diagnostic), ged_history_status_destroy);
    writable = !app->command_pending() && status && status->writable && status->state == GED_HISTORY_CURRENT &&
	!status->edit_active && !status->staged_conflict;
    mark->setEnabled(writable && (status->mode == GED_HISTORY_EMBEDDED || status->mode == GED_HISTORY_WORKSPACE));
    if (status && ged_history_visit(gedp, nullptr, addRevision, addCheckpoint, this, &diagnostic) == BRLCAD_OK) {
	auto *item = revisionItems.value(previous, revisionItems.value(currentRevision));
	revisions->setCurrentItem(item);
    }
    message->setText(QString::fromUtf8(bu_vls_cstr(&diagnostic)));
    message->setVisible(bu_vls_strlen(&diagnostic) != 0);
    bu_vls_free(&diagnostic);
    updateSelection();
}

void
HistoryDialog::updateSelection()
{
    changes->clear();
    const QString revision = selectedRevision();
    checkout->setEnabled(writable && !revision.isEmpty() && revision != currentRevision);
    preview->setEnabled(!revision.isEmpty());
    if (revision.isEmpty())
	return;
    struct bu_vls diagnostic = BU_VLS_INIT_ZERO;
    const QByteArray id = revision.toLatin1();
    if (ged_history_visit_revision(app->mdl->gedp, id.constData(), nullptr, addChange, this, &diagnostic) != BRLCAD_OK) {
	message->setText(QString::fromUtf8(bu_vls_cstr(&diagnostic)));
	message->show();
    } else {
	message->hide();
    }
    bu_vls_free(&diagnostic);
}

void
HistoryDialog::previewRevision()
{
    const QByteArray id = selectedRevision().toLatin1();
    struct bu_vls diagnostic = BU_VLS_INIT_ZERO;
    auto *snapshot = ged_history_open_revision(app->mdl->gedp, id.constData(), &diagnostic);
    if (!snapshot) {
	message->setText(QString::fromUtf8(bu_vls_cstr(&diagnostic)));
	message->show();
	bu_vls_free(&diagnostic);
	return;
    }
    bu_vls_free(&diagnostic);

    const std::unique_ptr<struct ged, decltype(&ged_close)> context(ged_create(), ged_close);
    context->dbip = snapshot;
    context->new_cmd_forms = 1;
    context->dbi_state = new DbiState(context.get());
    context->ged_gvp->gv_local2base = snapshot->dbi_local2base;
    context->ged_gvp->gv_base2local = snapshot->dbi_base2local;

    // Destroy the canvas before the separate GED context that owns its view.
    QDialog window(this);
    window.setWindowTitle(tr("Preview Revision %1").arg(QString::fromLatin1(id).left(12)));
    auto *layout = new QVBoxLayout(&window);
    auto *selector = new QHBoxLayout;
    auto *path = new QLineEdit(&window);
    path->setPlaceholderText(tr("Object or combination path"));
    selector->addWidget(path);
    auto *draw = new QPushButton(tr("Draw"), &window);
    draw->setAutoDefault(false);
    selector->addWidget(draw);
    layout->addLayout(selector);

    auto *change = changes->currentItem();
    if (!change)
	change = changes->topLevelItem(0);
    if (change) {
	const QString name = change->text(2).isEmpty() ? change->text(1) : change->text(2);
	const auto *dp = db_lookup(snapshot, name.toUtf8().constData(), LOOKUP_QUIET);
	if (dp && !(dp->d_flags & DB_LS_HIDDEN))
	    path->setText(name);
    }

    auto *canvas = new QgView(&window, QgView_SW);
    canvas->set_view(context->ged_gvp);
    canvas->enableDefaultKeyBindings();
    canvas->enableDefaultMouseBindings();
    layout->addWidget(canvas, 1);

    auto *draw_error = new QLabel(&window);
    draw_error->setWordWrap(true);
    draw_error->hide();
    layout->addWidget(draw_error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &window);
    connect(buttons, &QDialogButtonBox::rejected, &window, &QDialog::reject);
    layout->addWidget(buttons);

    const auto draw_path = [gedp = context.get(), canvas, path, draw_error]() {
	QString name = path->text();
	if (name.isEmpty())
	    return;
	if (!name.startsWith('/'))
	    name.prepend('/');
	const QByteArray object = name.toUtf8();
	const char *clear[] = {"zap"};
	const char *argv[] = {"draw", object.constData()};
	const int ret = ged_exec_zap(gedp, sizeof(clear) / sizeof(clear[0]), clear);
	if (ret == BRLCAD_OK && ged_exec_draw(gedp, sizeof(argv) / sizeof(argv[0]), argv) == BRLCAD_OK) {
	    draw_error->hide();
	} else {
	    draw_error->setText(QString::fromUtf8(bu_vls_cstr(gedp->ged_result_str)));
	    draw_error->show();
	}
	canvas->need_update(QG_VIEW_DRAWN);
    };
    connect(canvas, &QgView::init_done, &window, draw_path);
    connect(canvas, &QgView::changed, &window, [canvas]() { canvas->need_update(QG_VIEW_DRAWN); });
    connect(path, &QLineEdit::returnPressed, &window, draw_path);
    connect(draw, &QPushButton::clicked, &window, draw_path);
    window.resize(size());
    window.exec();
}

void
HistoryDialog::markRevision()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(this, tr("Mark Revision"), tr("Checkpoint name"),
	QLineEdit::Normal, QString(), &accepted);
    if (!accepted || name.isEmpty())
	return;
    const bool move = checkpoints.contains(name);
    if (move && QMessageBox::question(this, tr("Move Checkpoint"),
	    tr("Move %1 to the current revision?").arg(name),
	    QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
	return;

    const QByteArray encoded = name.toUtf8();
    const char *argv[] = {"db", "history", "mark", encoded.constData(), "--move"};
    const int argc = sizeof(argv) / sizeof(argv[0]);
    runCommand(move ? argc : argc - 1, argv);
}

void
HistoryDialog::runCommand(int argc, const char **argv)
{
    struct bu_vls diagnostic = BU_VLS_INIT_ZERO;
    if (app->run_cmd(&diagnostic, argc, argv) != BRLCAD_OK) {
	message->setText(QString::fromUtf8(bu_vls_cstr(&diagnostic)));
	message->show();
    }
    bu_vls_free(&diagnostic);
}

} // namespace

void
QgEdMainWindow::ShowHistory()
{
    HistoryDialog dialog(static_cast<QgEdApp *>(qApp), this);
    dialog.exec();
}

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// End:
// ex: shiftwidth=4 tabstop=8

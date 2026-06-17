/*    T E S T _ Q T C A D _ P L U G I N _ R E U S E . C P P
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
/** @file test_qtcad_plugin_reuse.cpp
 *
 * Phase 8 libqtcad reuse validation — proves that the libqtcad plugin
 * framework works independently of qged.
 *
 * This binary deliberately links only against libqtcad, libbu, and
 * Qt6::Widgets.  It does NOT link qged or libged.  It exercises:
 *
 *   1. QgPluginManager construction with a host-supplied directory.
 *   2. Descriptor enumeration and diagnosticSummary().
 *   3. IQgCommand discovery and invocation without a real ged instance.
 *   4. IQgPanelFactory and IQgDialogFactory instantiation (offscreen).
 *   5. Enable/disable round-trip via QgPluginManager.
 *   6. Safety of an empty-directory manager (no crash, zero plugins).
 *
 * The tested plugins are the Phase 7 sample extension plugins built
 * as part of qged (qged.panel, qged.dialog, qged.command categories).
 * Discovering them here proves that category strings are host-owned
 * strings — any host can load and query them without qged.
 *
 * Usage: test_qtcad_plugin_reuse <qged-plugin-directory>
 *
 * Returns 0 on all checks passing, 1 otherwise.
 */

#include "common.h"

#include <QApplication>
#include <QDockWidget>
#include <QDialog>

#include "bu/app.h"
#include "bu/log.h"

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgPluginManager.h"
#include "qtcad/QgToolBase.h"

/* ------------------------------------------------------------------ */
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

/* ================================================================== */
int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    /* Require exactly one argument: the plugin directory path. */
    if (argc < 2) {
	bu_log("Usage: %s <qged-plugin-directory>\n", argv[0]);
	return 1;
    }
    const char *plugin_dir = argv[1];

    /* Ensure the offscreen platform is used so no X11 display is needed. */
    if (!qgetenv("QT_QPA_PLATFORM").size())
	qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);

    /* ================================================================
     * 1. Empty-directory manager: zero plugins, no crash.
     * ================================================================ */
    {
        QgPluginManager empty_mgr(QStringList() << QStringLiteral("/nonexistent_empty_dir"));
        empty_mgr.rescan();
        QList<QgPluginDescriptor> all = empty_mgr.descriptors();
        TCHECK(all.isEmpty(), "empty-dir manager reports zero plugins");

        QString summary = empty_mgr.diagnosticSummary();
        TCHECK(summary.isNull() || summary.isEmpty(),
               "empty-dir diagnosticSummary is empty (no plugins)");
        TCHECK(true, "empty-dir diagnosticSummary does not crash");
    }

    /* ================================================================
     * 2. Real manager scanning the provided plugin directory.
     * ================================================================ */
    QgPluginManager mgr(QStringList() << QString::fromLocal8Bit(plugin_dir),
                        QStringLiteral("qtcad-test/plugins-phase8"));
    mgr.rescan();

    QList<QgPluginDescriptor> all_descs = mgr.descriptors();
    TCHECK(!all_descs.isEmpty(), "manager discovers plugins in provided directory");

    /* ================================================================
     * 3. diagnosticSummary should mention known plugin IDs.
     * ================================================================ */
    QString summary = mgr.diagnosticSummary();
    TCHECK(summary.contains(QStringLiteral("host_status")),
           "diagnosticSummary mentions host_status plugin IDs");

    /* ================================================================
     * 4. Category-filtered descriptor queries.
     * ================================================================ */
    QList<QgPluginDescriptor> cmd_descs  = mgr.descriptors(QStringLiteral("qged.command"));
    QList<QgPluginDescriptor> panel_descs = mgr.descriptors(QStringLiteral("qged.panel"));
    QList<QgPluginDescriptor> dialog_descs = mgr.descriptors(QStringLiteral("qged.dialog"));

    TCHECK(!cmd_descs.isEmpty(),   "at least one qged.command plugin discovered");
    TCHECK(!panel_descs.isEmpty(), "at least one qged.panel plugin discovered");
    TCHECK(!dialog_descs.isEmpty(),"at least one qged.dialog plugin discovered");

    /* ================================================================
     * 5. QgPluginContext with NULL ged/bsg_view -- simulates a minimal host
     *    (no ged instance, no open database).
     * ================================================================ */
    QgPluginNotifier notifier;
    QgPluginContext ctx;
    ctx.hostName = QStringLiteral("test_qtcad_plugin_reuse");
    ctx.notifier = &notifier;
    /* Deliberately leave gedAccessor and viewAccessor as nullptr
     * to exercise the null-safe paths in the sample plugins. */
    ctx.log = [](const QString &msg) { bu_log("plugin: %s\n", qPrintable(msg)); };

    /* ================================================================
     * 6. IQgCommand: enumerate, find host_status_command, invoke it.
     * ================================================================ */
    QList<IQgCommand *> cmds = mgr.factories<IQgCommand>(QStringLiteral("qged.command"));
    TCHECK(!cmds.isEmpty(), "IQgCommand factories found for qged.command category");

    IQgCommand *host_cmd = nullptr;
    for (IQgCommand *c : cmds) {
        if (c && c->verb() == QStringLiteral("host_status")) {
            host_cmd = c;
            break;
        }
    }
    TCHECK(host_cmd != nullptr, "host_status IQgCommand found by verb");

    if (host_cmd) {
        QString out;
        QString err;
        int ret = host_cmd->run(&ctx, QStringList() << QStringLiteral("host_status"), &out, &err);
        TCHECK(ret == 0, "host_status command returns 0 (success)");
        TCHECK(out.contains(QStringLiteral("Host:")),    "output contains 'Host:'");
        TCHECK(out.contains(QStringLiteral("Database:")), "output contains 'Database:'");
        TCHECK(out.contains(QStringLiteral("View:")),    "output contains 'View:'");
        bu_log("host_status output:\n%s", qPrintable(out));

        /* Invoke with an alias. */
        QString out2;
        QString err2;
        TCHECK(host_cmd->aliases().contains(QStringLiteral("qged_status")),
               "host_status command has expected alias 'qged_status'");
        ret = host_cmd->run(&ctx, QStringList() << QStringLiteral("qged_status"), &out2, &err2);
        TCHECK(ret == 0, "host_status command via alias returns 0");

        /* Invoke with --refresh flag (no view crash). */
        QString out3;
        QString err3;
        ret = host_cmd->run(&ctx,
                            QStringList() << QStringLiteral("host_status")
                                          << QStringLiteral("--refresh"),
                            &out3, &err3);
        TCHECK(ret == 0, "host_status --refresh returns 0 (null view is safe)");
        TCHECK(out3.contains(QStringLiteral("Requested view refresh")),
               "output mentions view refresh");

        /* Invoke with unknown argument to exercise error path. */
        QString out4;
        QString err4;
        ret = host_cmd->run(&ctx,
                            QStringList() << QStringLiteral("host_status")
                                          << QStringLiteral("--unknown"),
                            &out4, &err4);
        TCHECK(ret != 0, "host_status with unknown arg returns non-zero");
        TCHECK(!err4.isEmpty(), "host_status with unknown arg writes to err");
    }

    /* ================================================================
     * 7. IQgCommand discovery via descriptor().
     * ================================================================ */
    {
        QgPluginDescriptor d = mgr.descriptor(QStringLiteral("org.brlcad.qged.command.host_status"));
        TCHECK(!d.id.isEmpty(), "descriptor() by id returns a valid descriptor");
        TCHECK(d.category == QStringLiteral("qged.command"),
               "descriptor category matches expected value");
        TCHECK(!d.description.isEmpty(), "descriptor has a non-empty description");
    }

    /* ================================================================
     * 8. IQgPanelFactory: instantiate a dock without attaching to a window.
     * ================================================================ */
    QList<IQgPanelFactory *> panels = mgr.factories<IQgPanelFactory>(QStringLiteral("qged.panel"));
    TCHECK(!panels.isEmpty(), "IQgPanelFactory factories found for qged.panel category");

    for (IQgPanelFactory *fac : panels) {
        if (!fac)
            continue;
        QgPluginDescriptor desc = fac->descriptor();
        TCHECK(!desc.id.isEmpty(), "IQgPanelFactory descriptor has non-empty id");
        QDockWidget *dock = fac->create(&ctx, nullptr);
        TCHECK(dock != nullptr, "IQgPanelFactory::create returns non-null dock");
        if (dock) {
            TCHECK(!dock->windowTitle().isEmpty() || !dock->objectName().isEmpty(),
                   "created dock has non-empty title or object name");
            delete dock;
        }
    }

    /* ================================================================
     * 9. IQgDialogFactory: instantiate a dialog without showing it.
     * ================================================================ */
    QList<IQgDialogFactory *> dialogs = mgr.factories<IQgDialogFactory>(QStringLiteral("qged.dialog"));
    TCHECK(!dialogs.isEmpty(), "IQgDialogFactory factories found for qged.dialog category");

    for (IQgDialogFactory *fac : dialogs) {
        if (!fac)
            continue;
        QgPluginDescriptor desc = fac->descriptor();
        TCHECK(!desc.id.isEmpty(), "IQgDialogFactory descriptor has non-empty id");
        QDialog *dialog = fac->create(&ctx, nullptr);
        TCHECK(dialog != nullptr, "IQgDialogFactory::create returns non-null dialog");
        if (dialog) {
            TCHECK(!dialog->windowTitle().isEmpty(),
                   "created dialog has non-empty window title");
            delete dialog;
        }
    }

    /* ================================================================
     * 10. IQgToolFactory: instantiate ell edit tool without ged/view and
     *     exercise the startup refresh path that previously segfaulted.
     * ================================================================ */
    QList<IQgToolFactory *> tools = mgr.factories<IQgToolFactory>(QStringLiteral("qged.object"));
    TCHECK(!tools.isEmpty(), "IQgToolFactory factories found for qged.object category");

    IQgToolFactory *ell_tool_factory = nullptr;
    for (IQgToolFactory *fac : tools) {
        if (fac && fac->descriptor().id == QStringLiteral("org.brlcad.qged.edit.ell")) {
            ell_tool_factory = fac;
            break;
        }
    }
    TCHECK(ell_tool_factory != nullptr, "ell edit IQgToolFactory found by id");

    if (ell_tool_factory) {
        QgToolBase *tool = ell_tool_factory->create(&ctx, nullptr);
        TCHECK(tool != nullptr, "ell edit factory creates a tool");
        if (tool) {
            QgToolPaletteElement *el = tool->paletteElement();
            TCHECK(el != nullptr, "ell edit tool creates a palette element");
            tool->refresh();
            tool->onDbChanged();
            tool->onViewChanged();
            TCHECK(true, "ell edit tool refresh path is safe without ged/view");
            delete tool;
        }
    }

    /* ================================================================
     * 11. Enable/disable round-trip.
     * ================================================================ */
    {
        const QString test_id = QStringLiteral("org.brlcad.qged.command.host_status");
        TCHECK(mgr.isEnabled(test_id), "plugin is enabled by default");
        bool ok = mgr.disable(test_id);
        TCHECK(ok, "disable() returns true for known id");
        TCHECK(!mgr.isEnabled(test_id), "plugin is disabled after disable()");
        ok = mgr.enable(test_id);
        TCHECK(ok, "enable() returns true for known id");
        TCHECK(mgr.isEnabled(test_id), "plugin is enabled after re-enable()");

        /* Unknown id: enable/disable return false, no crash. */
        TCHECK(!mgr.enable(QStringLiteral("nonexistent.id.does.not.exist")),
               "enable() of unknown id returns false");
        TCHECK(!mgr.disable(QStringLiteral("nonexistent.id.does.not.exist")),
               "disable() of unknown id returns false");
    }

    /* ================================================================
     * 12. diagnosticInfo for a known id.
     * ================================================================ */
    {
        QString info = mgr.diagnosticInfo(QStringLiteral("org.brlcad.qged.command.host_status"));
        TCHECK(!info.isEmpty(), "diagnosticInfo returns non-empty for known id");
        QString info_bad = mgr.diagnosticInfo(QStringLiteral("does.not.exist"));
        /* Should return something (even an error notice) without crashing. */
        (void)info_bad;
        TCHECK(true, "diagnosticInfo for unknown id does not crash");
    }

    /* ================================================================
     * 13. unloadAll: no crash, descriptor list becomes accessible again
     *     after a rescan.
     * ================================================================ */
    {
        mgr.unloadAll();
        mgr.rescan();
        QList<QgPluginDescriptor> after = mgr.descriptors();
        TCHECK(!after.isEmpty(), "descriptors accessible again after unloadAll+rescan");
    }

    /* ================================================================
     * Summary.
     * ================================================================ */
    if (g_fail) {
        bu_log("\ntest_qtcad_plugin_reuse: FAILED (%d check(s))\n", g_fail);
        return 1;
    }
    bu_log("\ntest_qtcad_plugin_reuse: PASSED\n");
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

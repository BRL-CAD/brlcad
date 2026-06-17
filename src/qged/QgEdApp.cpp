/*                      Q G E D A P P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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
/** @file QgEdApp.cpp
 *
 * Application level data and functionality implementations.
 *
 */

#include <set>
#include <unordered_set>
#include <QFileInfo>
#include <QFile>
#include <QPlainTextEdit>
#include <QTextStream>
#include <QThread>
#include "brlcad_version.h"
#include "bu/malloc.h"
#include "bu/file.h"
#include "dm.h"
#include "ged/bsg_ged_draw.h"
#include "ged/db_index.h"
#include "ged/selection_state.h"
#include "qtcad/QgGeomImport.h"
#include "qtcad/QgPluginCommands.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgPluginManager.h"
#include "qtcad/QgTreeSelectionModel.h"
#include "QgEdApp.h"
#include "fbserv.h"
#include "QgEdCategories.h"
#include "QgEdFilter.h"

int
qged_pre_opendb_clbk(int UNUSED(ac), const char **UNUSED(av), void *UNUSED(gedp), void *UNUSED(ctx))
{
    return BRLCAD_OK;
}

int
qged_post_opendb_clbk(int UNUSED(ac), const char **UNUSED(av), void *UNUSED(gedp), void *ctx)
{
    QgEdApp *a = (QgEdApp *)ctx;
    if (!a)
	return BRLCAD_OK;
    if (a->mdl && a->mdl->ged())
	emit a->dbi_update(a->mdl->ged()->dbip);
    if (!a->w)
	return BRLCAD_OK;
    if (!a->mdl || !a->mdl->ged() || !a->mdl->ged()->dbip) {
	a->w->statusBar()->showMessage("open failed");
	return BRLCAD_OK;
    }
    QString fileName(a->mdl->ged()->dbip->dbi_filename);
    a->w->statusBar()->showMessage(fileName);
    return BRLCAD_OK;
}

int
qged_pre_closedb_clbk(int UNUSED(ac), const char **UNUSED(av), void *UNUSED(gedp), void *UNUSED(ctx))
{
    return BRLCAD_OK;
}

int
qged_post_closedb_clbk(int UNUSED(ac), const char **UNUSED(av), void *UNUSED(gedp), void *ctx)
{
    QgEdApp *a = (QgEdApp *)ctx;
    if (a)
	emit a->dbi_update(DBI_NULL);
    return BRLCAD_OK;
}

extern "C" void
qt_create_io_handler(struct ged_subprocess *p, bu_process_io_t t, ged_io_func_t callback, void *data)
{
    if (!p || !p->p || !p->gedp || !p->gedp->ged_io_data)
	return;

    int fd = bu_process_fileno(p->p, t);
    if (fd < 0)
	return;

    QgEdApp *ca = (QgEdApp *)p->gedp->ged_io_data;
    QgConsole *c = ca->w->console;
    c->listen(fd, p, t, callback, data);

    switch (t) {
	case BU_PROCESS_STDIN:
	    p->stdin_active = 1;
	    break;
	case BU_PROCESS_STDOUT:
	    p->stdout_active = 1;
	    break;
	case BU_PROCESS_STDERR:
	    p->stderr_active = 1;
	    break;
    }
}

extern "C" void
qt_delete_io_handler(struct ged_subprocess *p, bu_process_io_t t)
{
    if (!p || !p->gedp || !p->gedp->ged_io_data) return;

    QgEdApp *ca = (QgEdApp *)p->gedp->ged_io_data;
    QgConsole *c = ca->w->console;

    auto it = c->findListener(p, (int)t);
    if (!it)
	return;
    QgConsoleListener *l = it;

    // Stop the QSocketNotifier from firing again on the worker thread
    // *immediately*.  This must happen on whatever thread we are called
    // from so that no further activated() lambda invocations land in
    // the libged callback after we return.
    l->disconnectNotifier();

    // Two callers reach this code:
    //
    //  1. The GUI thread (e.g. ged_close, or QgConsole::detach finishing
    //     up a queued is_finished signal).  We can finish synchronously.
    //
    //  2. The QgConsoleListener::m_thread worker thread, via
    //     _ged_rt_output_handler2 dispatched from the QSocketNotifier
    //     activated() lambda.  Anything touching Qt widgets (including
    //     the per-subprocess end_clbk, which in qged drives QAction
    //     icon state) MUST happen on the GUI thread.  So we hop back
    //     by emitting the queued is_finished signal and let
    //     QgConsole::detach do the real teardown over there.
    //
    // We never fire p->end_clbk here.  end_clbk is fired by libged
    // (_ged_rt_output_handler2 with type == -1) from the GUI thread
    // when QgConsole::detach observes the last listener has gone away.
    if (QThread::currentThread() == c->thread()) {
	// GUI thread: tear the listener down directly.  Do *not* fire the
	// libged callback with type == -1 here; that path is owned by
	// QgConsole::detach (it is the one that knows whether all streams
	// for the subprocess have been retired).  However, in the
	// synchronous (GUI-thread) case we also do not want a stale
	// queued is_finished to arrive later and re-enter detach for an
	// already-removed listener, so erase it now.
	c->removeListener(p, (int)t);
	switch (t) {
	    case BU_PROCESS_STDIN:  p->stdin_active  = 0; break;
	    case BU_PROCESS_STDOUT: p->stdout_active = 0; break;
	    case BU_PROCESS_STDERR: p->stderr_active = 0; break;
	}
    } else {
	// Worker thread: hop back to GUI thread.  on_finished() emits
	// the queued is_finished signal which is connected to
	// QgConsole::detach.  detach() will erase the listener, clear
	// the stream-active flag for this stream, and (once all streams
	// for the subprocess are retired) re-enter the libged callback
	// with type == -1 so it can finalize on the GUI thread.
	l->on_finished();
    }
}

struct qged_qcmd_cleanup {
    const char *cmd = NULL;
    char *input = NULL;
    char **av = NULL;

    ~qged_qcmd_cleanup()
    {
	if (cmd)
	    bu_free((void *)cmd, "cmd");
	if (input)
	    bu_free(input, "input copy");
	if (av)
	    bu_free(av, "input argv");
    }
};


QgEdApp::QgEdApp(int &argc, char *argv[], int swrast_mode, int quad_mode) :QApplication(argc, argv)
{
    setOrganizationName("BRL-CAD");
    setOrganizationDomain("brlcad.org");
    setApplicationName("QGED");
    setApplicationVersion(brlcad_version());

    // NOTE - these env variables should ultimately be temporary - we are using
    // them to enable behavior in LIBRT/LIBGED we don't yet want on by default
    // in all applications

    /* Let LIBRT know to process comb instance specifiers in paths */
    bu_setenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS", "1", 1);

    mdl = new QgModel();
    m_plugin_notifier = new QgPluginNotifier(this);
    QObject::connect(this, &QgEdApp::dbi_update, m_plugin_notifier, &QgPluginNotifier::dbChanged);
    QObject::connect(this, &QgEdApp::view_update, m_plugin_notifier, &QgPluginNotifier::viewUpdated);
    /* QgEdApp owns mdl for the lifetime of the application; the accessor
     * re-reads the member so future replacements are observed as well. */
    m_plugin_context.gedAccessor = [this]() -> struct ged * {
	return mdl ? mdl->ged() : GED_NULL;
    };
    m_plugin_context.viewAccessor = [this]() -> struct bsg_view * {
	/* Prefer the main window's current display when it exists; before window
	 * construction falls back to the model's current ged view pointer. */
	if (w)
	    return w->CurrentView();
	return (mdl && mdl->ged()) ? mdl->ged()->ged_gvp : NULL;
    };
    m_plugin_context.model = mdl;
    m_plugin_context.notifier = m_plugin_notifier;
    m_plugin_context.hostName = QStringLiteral("qged");
    m_plugin_context.log = [this](const QString &msg) {
	if (w && w->console) {
	    w->console->printStringBeforePrompt(msg);
	    return;
	}
	bu_log("%s", msg.toLocal8Bit().constData());
    };
    char plugin_dir[MAXPATHLEN] = {0};
    bu_dir(plugin_dir, MAXPATHLEN, BU_DIR_LIBEXEC, "qged", NULL);
    QString plugin_dir_path = QString::fromLocal8Bit(plugin_dir);
    QStringList plugin_search_dirs;
    if (!plugin_dir_path.isEmpty())
	plugin_search_dirs.append(plugin_dir_path);
    m_plugin_manager = new QgPluginManager(plugin_search_dirs, QStringLiteral("qged/plugins"), this);

    // Install a filter to handle the top level key bindings and other
    // interactive event management requiring global application knowledge.
    QgEdFilter *efilter = new QgEdFilter();
    installEventFilter(efilter);

    // Use the dark theme from https://github.com/Alexhuszagh/BreezeStyleSheets
    QFile file(":/dark.qss");
    if (!file.open(QFile::ReadOnly | QFile::Text))
	bu_exit(EXIT_FAILURE, "Error - unable to find dark.qss embedded theme!\n");
    QTextStream stream(&file);
    setStyleSheet(stream.readAll());

    // Backend policy:
    //   - dm-qtgl  (hardware OpenGL via QgGL) is the default when BRL-CAD is
    //     built with OpenGL support.  It delegates rendering to the system GPU
    //     and is substantially faster than software rasterization for
    //     interactive 3-D work.
    //   - dm-swrast (Mesa OSMesa software rasterizer via QgSW) is the fallback
    //     selected by the user with the -s / --swrast command-line flag, or
    //     automatically used in environments where OpenGL is unavailable.
    //
    // The QgEdMainWindow constructor expects a QgView_* canvas type constant
    // (QgView_GL or QgView_SW), not a raw swrast_mode boolean.  The
    // translation is done here so that the policy is stated in one place.
#ifdef BRLCAD_OPENGL
    int canvas_type = swrast_mode ? QgView_SW : QgView_GL;
#else
    int canvas_type = QgView_SW; /* No OpenGL support — software rasterizer only */
#endif

    // Create the windows
    w = new QgEdMainWindow(canvas_type, quad_mode);

    /* GED needs some information and methods from QGED - make
     * those assignment */
    struct ged *gedp = mdl->ged();

    // Let GED know to use the QgQuadView view as its current view
    gedp->ged_gvp = w->CurrentView();

    // Set up the connections needed for embedded raytracing
    gedp->ged_fbs->fbs_is_listening = &qdm_is_listening;
    gedp->ged_fbs->fbs_listen_on_port = &qdm_listen_on_port;
    gedp->ged_fbs->fbs_open_server_handler = &qdm_open_server_handler;
    gedp->ged_fbs->fbs_close_server_handler = &qdm_close_server_handler;

    // Unfortunately, there are technical differences involved with
    // the embedded fb mechanisms depending on whether we are using
    // the system native OpenGL or our fallback software rasterizer
    int type = w->CurrentDisplay()->view_type();
#ifdef BRLCAD_OPENGL
    if (type == QgView_GL) {
	gedp->ged_fbs->fbs_open_client_handler     = &qdm_open_client_handler;
	gedp->ged_fbs->fbs_open_ipc_client_handler = &qdm_open_ipc_client_handler;
    }
#endif
    if (type == QgView_SW) {
	gedp->ged_fbs->fbs_open_client_handler     = &qdm_open_sw_client_handler;
	gedp->ged_fbs->fbs_open_ipc_client_handler = &qdm_open_ipc_sw_client_handler;
    }
    gedp->ged_fbs->fbs_close_client_handler     = &qdm_close_client_handler;
    gedp->ged_fbs->fbs_close_ipc_client_handler = &qdm_close_ipc_client_handler;

    // Read the saved window size, if any
    QSettings settings("BRL-CAD", "QGED");

    // (Debugging) Report settings filename
    if (QFileInfo(settings.fileName()).exists())
	std::cout << "Reading settings from " << settings.fileName().toStdString() << "\n";

    if (!QFileInfo(settings.fileName()).exists()) {
	w->resize(QSize(1100, 800));
    } else {
	//https://bugreports.qt.io/browse/QTBUG-16252?focusedCommentId=250562&page=com.atlassian.jira.plugin.system.issuetabpanels%3Acomment-tabpanel#comment-250562
	if(settings.contains("geometry"))
	    w->setGeometry(settings.value("geometry").value<QRect>());
	w->restoreState(settings.value("windowState").toByteArray());
    }

    // This is when the window and widgets are actually drawn (do this after
    // loading settings so the window size matches the saved config, if any)
    w->show();

    // If the 3D view didn't set up appropriately, let the user know they
    // should try the fallback SW rendering mode.  We must do this after the
    // show() call, because it isn't until after that point that we know
    // whether the setup of the system's OpenGL context setup was successful.
    //
    // TODO - if we put up an Archer-style splash screen using OpenGL before
    // we try to set up the main window, we might be able to find out the
    // answer to "is OpenGL working" before we have to do the primary GUI
    // setup.  We originally fell back automatically to swrast if OpenGL wasn't
    // working, but that didn't end up working reliably - however, if we
    // instead figure out a way to get the answer without having to try
    // and recover the OpenGL state of the main app, we might still be able
    // to achieve an automatic transparent fallback feature for the end user.
    if (!w->isValid3D()) {
	bu_exit(EXIT_FAILURE, "OpenGL failed to initialize properly.  Recommend running qged with '-s' option to use fallback swrast rendering.");
    }

    // Assign QGED specific open/close db handlers to the gedp
    ged_clbk_set(mdl->ged(), "opendb", BU_CLBK_PRE, &qged_pre_opendb_clbk, (void *)qApp);
    ged_clbk_set(mdl->ged(), "opendb", BU_CLBK_POST, &qged_post_opendb_clbk, (void *)qApp);
    ged_clbk_set(mdl->ged(), "closedb", BU_CLBK_PRE, &qged_pre_closedb_clbk, (void *)qApp);
    ged_clbk_set(mdl->ged(), "closedb", BU_CLBK_POST, &qged_post_closedb_clbk, (void *)qApp);

    // Assign QGED specific I/O handlers to the gedp
    mdl->ged()->ged_create_io_handler = &qt_create_io_handler;
    mdl->ged()->ged_delete_io_handler = &qt_delete_io_handler;
    mdl->ged()->ged_io_data = (void *)qApp;

    // If we have a default filename supplied, open it.  We've delayed doing so
    // until now in order to have the display related containers from graphical
    // initialization/show() available - the GED structure will need to know
    // about some of them to have drawing commands connect properly to the 3D
    // displays.
    if (argc) {
	char *fname = bu_strdup(bu_dir(NULL, 0, BU_DIR_CURR, argv[0], NULL));
	if (!bu_file_exists(fname, NULL)) {
	    // Current dir prefix didn't work - were we given a full path rather
	    // than a relative path?
	    bu_free(fname, "path");
	    fname = bu_strdup(bu_path_normalize(argv[0]));
	}
	int ret = load_g_file(fname, false);
	if (ret != BRLCAD_OK) {
	    bu_exit(EXIT_FAILURE, "Error opening file %s\n", fname);
	}
	bu_free(fname, "path");
	emit dbi_update(mdl->ged()->dbip);
    }

    // Send a view_change signal so widgets depending on view information
    // can initialize themselves
    emit view_update(QG_VIEW_REFRESH);

    // Generally speaking if we're going to have trouble initializing, it will
    // be with either the GED plugins or the dm plugins.  Print relevant
    // messages from those initialization routines (if any) so the user can
    // tell what's going on.
    int have_msg = 0;
    std::string ged_msgs(ged_init_msgs());
    if (ged_msgs.size()) {
	w->console->printString(ged_msgs.c_str());
	w->console->printString("\n");
	have_msg = 1;
    }
    std::string dm_msgs(dm_init_msgs());
    if (dm_msgs.size()) {
	if (dm_msgs.find("qtgl") != std::string::npos || dm_msgs.find("swrast") != std::string::npos) {
	    w->console->printString(dm_msgs.c_str());
	    w->console->printString("\n");
	    have_msg = 1;
	}
    }

    // If we did write any messages, need to restore the prompt
    if (have_msg) {
	w->console->prompt("$ ");
    }
}

QgEdApp::~QgEdApp() {
    delete mdl;
    // TODO - free rt_vlfree?
}

void
QgEdApp::do_quad_view_change(QgView *cv)
{
    QTCAD_SLOT("QgEdApp::do_quad_view_change", 1);
    mdl->ged()->ged_gvp = cv->view();
    if (w)
	w->setActiveView(cv);
    if (m_plugin_notifier)
	emit m_plugin_notifier->viewChanged();
    emit view_update(QG_VIEW_REFRESH);
}

void
QgEdApp::do_view_changed(QgViewUpdateFlags flags)
{
    bsg_log(1, "QgEdApp::do_view_changed");
    QTCAD_SLOT("QgEdApp::do_view_changed", 1);

    if (flags & QG_VIEW_DRAWN) {
	struct ged_draw_transaction txn =
	    ged_draw_transaction_make(GED_DRAW_TXN_REDRAW, NULL);
	ged_draw_apply_transaction(mdl->ged(), &txn, NULL);
    }

    emit view_update(flags);
}

void
QgEdApp::open_file()
{
    QTCAD_SLOT("QgEdApp::open_file", 1);

    load_g_file();
}

int
QgEdApp::load_g_file(const char *gfile, bool do_conversion)
{
    QTCAD_SLOT("QgEdApp::load_g_file", 1);

    QgGeomImport importer((QWidget *)this->w);
    importer.enable_conversion = do_conversion;
    QString fileName = importer.gfile(gfile);
    if (fileName.isEmpty()) {
	if (w)
	    w->statusBar()->showMessage(bu_vls_cstr(&importer.conv_msg));
	return BRLCAD_ERROR;
    }

    int ac = 2;
    const char *av[3];
    av[0] = "opendb";
    av[1] = bu_strdup(fileName.toLocal8Bit().data());
    av[2] = NULL;
    int ret = mdl->run_cmd(mdl->ged()->ged_result_str, ac, (const char **)av);
    bu_free((void *)av[1], "filename cpy");
    return ret;
}

QgViewUpdateFlags
qged_view_update(struct ged *gedp)
{
    QgViewUpdateFlags view_flags;

    unsigned long long updated = ged_db_index_refresh_flags(gedp);
    if (updated & GED_DB_INDEX_REFRESH_VIEW_CHANGE)
	view_flags |= QG_VIEW_DRAWN;

    return view_flags;
}

extern "C" int
raytrace_start(int UNUSED(argc), const char **UNUSED(argv), void *valp, void *ctx)
{
    if (!valp)
	return BRLCAD_OK;
    int val = *(int *)valp;
    QgEdApp *ap = (QgEdApp *)ctx;
    ap->w->IndicateRaytraceStart(val);
    return BRLCAD_OK;
}

extern "C" int
raytrace_done(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(valp), void *ctx)
{
    QgEdApp *ap = (QgEdApp *)ctx;
    ap->w->IndicateRaytraceDone();
    return BRLCAD_OK;
}

int
QgEdApp::run_cmd(struct bu_vls *msg, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    QgViewUpdateFlags view_flags;

    if (!mdl || !argc || !argv)
	return BRLCAD_ERROR;

    struct ged *gedp = mdl->ged();

    select_hash = ged_selection_state_hash(gedp, nullptr);

    /* Set the local unit conversions */
    if (gedp->dbip) {
	struct bsg_view *v = w->CurrentView();
	v->gv_base2local = gedp->dbip->dbi_base2local;
	v->gv_local2base = gedp->dbip->dbi_local2base;
    }

    if (!tmp_av.size()) {

	// If we're not in the middle of an incremental command,
	// stash the view state(s) for later comparison and make
	// sure our unit conversions are right
	w->DisplayCheckpoint();
	//select_hash = ged_selection_hash_sets(gedp->ged_selection_sets);

	// If we need command-specific subprocess awareness for
	// a command, set it up
	if (BU_STR_EQUAL(argv[0], "ert")) {
	    ged_clbk_set(gedp, "ert", BU_CLBK_DURING, &raytrace_start, (void *)this);
	    ged_clbk_set(gedp, "ert", BU_CLBK_LINGER, &raytrace_done, (void *)this);
	}

	// Ask the model to execute the command
	ret = mdl->run_cmd(msg, argc, argv);

	if (BU_STR_EQUAL(argv[0], "ert")) {
	    ged_clbk_set(gedp, "ert", BU_CLBK_DURING, NULL, NULL);
	    ged_clbk_set(gedp, "ert", BU_CLBK_LINGER, NULL, NULL);
	}
    } else {
	for (int i = 0; i < argc; i++) {
	    char *tstr = bu_strdup(argv[i]);
	    tmp_av.push_back(tstr);
	}
	char **av = (char **)bu_calloc(tmp_av.size() + 1, sizeof(char *), "argv array");
	// Assemble the full command we have thus var
	for (size_t i = 0; i < tmp_av.size(); i++) {
	    av[i] = tmp_av[i];
	}
	int ac = (int)tmp_av.size();
	ret = mdl->run_cmd(msg, ac, (const char **)av);
    }

    if (!(ret & GED_MORE)) {

	// Handle any necessary redrawing.
	view_flags = qged_view_update(gedp);

	/* Check if the ged_exec call changed either the display manager or
	 * the view settings - in either case we'll need to redraw */
	// TODO - there may be some utility in checking only the camera or only
	// the who list, since we can set different update flags for each case...
	// that's a complexity vs. performance trade-off determination
	if (w->DisplayDiff())
	    view_flags |= QG_VIEW_DRAWN;

	unsigned long long cs_hash = ged_selection_state_hash(gedp, nullptr);
	if (cs_hash != select_hash) {
	    view_flags |= QG_VIEW_SELECT;
	    // This is what notifies currently drawn solids to update
	    // in response to a command line selection change
	    if (ged_selection_draw_sync(gedp, nullptr))
		view_flags |= QG_VIEW_DRAWN;
	}
    }

    if (ret & GED_MORE) {
	// If this is the first time through stash in tmp_av, since we
	// didn't know to do it above
	if (!tmp_av.size()) {
	    for (int i = 0; i < argc; i++) {
		char *tstr = bu_strdup(argv[i]);
		tmp_av.push_back(tstr);
	    }
	    QgConsole *console = w->console;
	    if (console)
		history_mark_start = console->historyCount() - 2;
	}
    } else {
	// If we were in an incremental command, we're done now -
	if (tmp_av.size()) {
	    // clear tmp_av
	    for (size_t i = 0; i < tmp_av.size(); i++) {
		delete tmp_av[i];
	    }
	    tmp_av.clear();
	    // let the console know that we're done with MORE
	    QgConsole *console = w->console;
	    if (console)
		history_mark_end = console->historyCount() - 1;
	}
    }

    // TODO - should be able to emit this regardless - if execution methods
    // are well behaved, they'll just ignore a zero flags value...
    if (view_flags)
	emit view_update(view_flags);

    return ret;
}

void
QgEdApp::run_qcmd(const QString &command)
{
    QTCAD_SLOT("QgEdApp::run_qcmd", 1);
    if (!w)
	return;

    QgConsole *console = w->console;
    const char *cmd = bu_strdup(command.toLocal8Bit().data());

    // TODO - replace with "quit" cmd libged callback
    if (BU_STR_EQUAL(cmd, "q")) {
	w->closeEvent(NULL);
	bu_exit(0, "exit");
    }

    // TODO - replace with "clear" cmd libged callback
    if (BU_STR_EQUAL(cmd, "clear")) {
	if (console) {
	    console->clear();
	    console->prompt("$ ");
	}
	bu_free((void *)cmd, "cmd");
	return;
    }

    if (!mdl) {
	bu_free((void *)cmd, "cmd");
	return;
    }

    // make an argv array
    struct bu_vls ged_prefixed = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&ged_prefixed, "%s", cmd);
    char *input = bu_strdup(bu_vls_addr(&ged_prefixed));
    bu_vls_free(&ged_prefixed);
    char **av = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
    int ac = bu_argv_from_string(av, strlen(input), input);
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct qged_qcmd_cleanup cleanup = {cmd, input, av};

    if (ac > 0 && BU_STR_EQUAL(av[0], "plugins")) {
	QString out;
	QString err;
	QStringList plugin_argv;
	if (ac > 1)
	    plugin_argv.reserve(ac - 1);
	for (int i = 1; i < ac; ++i)
	    plugin_argv.append(QString::fromLocal8Bit(av[i]));
	QgPluginCommands::run(m_plugin_manager, plugin_argv, &out, &err);
	if (console) {
	    if (!out.isEmpty())
		console->printString(out);
	    if (!err.isEmpty())
		console->printString(err);
	    console->prompt("$ ");
	}
	bu_vls_free(&msg);
	return;
    }

    if (ac > 0 && m_plugin_manager) {
	QString verb = QString::fromLocal8Bit(av[0]);
	QString out;
	QString err;
	QStringList plugin_argv;
	plugin_argv.reserve(ac);
	for (int i = 0; i < ac; ++i)
	    plugin_argv.append(QString::fromLocal8Bit(av[i]));

	IQgCommand *plugin_cmd = NULL;
	QList<IQgCommand *> cmds =
	    m_plugin_manager->factories<IQgCommand>(QStringLiteral(QGED_CATEGORY_COMMAND));
	for (IQgCommand *plugin_factory : cmds) {
	    if (!plugin_factory)
		continue;
	    if (plugin_factory->verb() == verb || plugin_factory->aliases().contains(verb)) {
		plugin_cmd = plugin_factory;
		break;
	    }
	}

	if (plugin_cmd) {
	    int ret = plugin_cmd->run(&m_plugin_context, plugin_argv, &out, &err);
	    if (console) {
		if (!out.isEmpty())
		    console->printString(out);
		if (!err.isEmpty())
		    console->printString(err);
		console->prompt("$ ");
	    }
	    if (mdl && mdl->ged())
		bu_vls_trunc(mdl->ged()->ged_result_str, 0);
	    bu_vls_free(&msg);
	    (void)ret;
	    return;
	}
    }

    // Run as a GED command.
    int ret = BRLCAD_OK;
    ret = run_cmd(&msg, ac, (const char **)av);
    if (bu_vls_strlen(&msg) > 0 && console) {
	console->printString(bu_vls_cstr(&msg));
    }

    if (console) {
	if (ret & GED_MORE) {
	    console->prompt(bu_vls_cstr(mdl->ged()->ged_result_str));
	} else {
	    console->prompt("$ ");
	    if (history_mark_start >= 0 && history_mark_end >= 0) {
		console->consolidateHistory(history_mark_start, history_mark_end);
		history_mark_start = -1;
		history_mark_end = -1;
	    }
	}
    }

    if (mdl && mdl->ged()) {
	bu_vls_trunc(mdl->ged()->ged_result_str, 0);
    }
    bu_vls_free(&msg);
}

void
QgEdApp::element_selected(QgToolPaletteElement *el)
{
    QTCAD_SLOT("QgEdApp::element_selected", 1);
    QWidget *controls = el->controlsWidget();
    // Palette elements may legitimately omit a controls widget and expose only
    // button-driven behavior, so guard that case before manipulating visibility.
    if (!controls || !controls->isVisible()) {
	// Apparently this can happen when we have docked widgets
	// closed and we click on the border between the view and
	// the dock - need to avoid messing with the event filters
	// if that happens or we break the user interactions.
	return;
    }

    QgView *curr_view = w->CurrentDisplay();
    QObject *active_filter = curr_view->active_event_filter();

    if (active_filter)
	curr_view->clear_event_filter(active_filter);

    if (el->use_event_filter)
	curr_view->add_event_filter(controls);
    if (curr_view->view()) {
	curr_view->view()->gv_width = curr_view->width();
	curr_view->view()->gv_height = curr_view->height();
    }
}

void
QgEdApp::write_settings()
{
    QTCAD_SLOT("QgEdApp::write_settings", 1);
    QSettings settings("BRL-CAD", "QGED");

    // TODO - write user settings here.  Window state saving is handled by
    // QgEdMainWindow closeEvent
    if (m_plugin_notifier)
	emit m_plugin_notifier->settingsChanged();
}

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

/*                      Q G E D A P P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2024 United States Government as represented by
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
#include "bu/malloc.h"
#include "bu/file.h"
#include "qtcad/QgGeomImport.h"
#include "qtcad/QgTreeSelectionModel.h"
#include "QgEdApp.h"
#include "fbserv.h"
#include "QgEdFilter.h"

void
qged_pre_opendb_clbk(struct ged *UNUSED(gedp), void *UNUSED(ctx))
{
}

void
qged_post_opendb_clbk(struct ged *UNUSED(gedp), void *ctx)
{
    QgEdApp *a = (QgEdApp *)ctx;
    emit a->dbi_update(a->mdl->gedp->dbip);
    if (!a->w)
	return;
    if (!a->mdl->gedp->dbip) {
	a->w->statusBar()->showMessage("open failed");
	return;
    }
    QString fileName(a->mdl->gedp->dbip->dbi_filename);
    a->w->statusBar()->showMessage(fileName);
}

void
qged_pre_closedb_clbk(struct ged *UNUSED(gedp), void *UNUSED(ctx))
{
}

void
qged_post_closedb_clbk(struct ged *UNUSED(gedp), void *UNUSED(ctx))
{
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
    if (!p) return;

    QgEdApp *ca = (QgEdApp *)p->gedp->ged_io_data;
    QgConsole *c = ca->w->console;

    // Since these callbacks are invoked from the listener, we can't call
    // the listener destructors directly.  We instead call a routine that
    // emits a single that will notify the console widget it's time to
    // detach the listener.
    switch (t) {
	case BU_PROCESS_STDIN:
	    bu_log("stdin\n");
	    if (p->stdin_active && c->listeners.find(std::make_pair(p, t)) != c->listeners.end()) {
		c->listeners[std::make_pair(p, t)]->m_notifier->disconnect();
		c->listeners[std::make_pair(p, t)]->on_finished();
	    }
	    p->stdin_active = 0;
	    break;
	case BU_PROCESS_STDOUT:
	    if (p->stdout_active && c->listeners.find(std::make_pair(p, t)) != c->listeners.end()) {
		c->listeners[std::make_pair(p, t)]->m_notifier->disconnect();
		c->listeners[std::make_pair(p, t)]->on_finished();
		bu_log("stdout: %d\n", p->stdout_active);
	    }
	    p->stdout_active = 0;
	    break;
	case BU_PROCESS_STDERR:
	    if (p->stderr_active && c->listeners.find(std::make_pair(p, t)) != c->listeners.end()) {
		c->listeners[std::make_pair(p, t)]->m_notifier->disconnect();
		c->listeners[std::make_pair(p, t)]->on_finished();
		bu_log("stderr: %d\n", p->stderr_active);
	    }
	    p->stderr_active = 0;
	    break;
    }

    // All communication has ceased between the app and the subprocess,
    // time to call the end callback (if any)
    if (!p->stdin_active && !p->stdout_active && !p->stderr_active) {
	if (p->end_clbk)
	    p->end_clbk(0, p->end_clbk_data);
    }

    emit ca->view_update(QG_VIEW_REFRESH);
}


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
    /* Let LIBGED know to initialize its instance state container */
    bu_setenv("LIBGED_DBI_STATE", "1", 1);
    /* Let LIBGED know to use new command forms */
    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);

    mdl = new QgModel();
    BU_LIST_INIT(&RTG.rtg_vlfree);

    // Install a filter to handle the top level key bindings and other
    // interactive event management requiring global application knowledge.
    QgEdFilter *efilter = new QgEdFilter();
    installEventFilter(efilter);

    // Use the dark theme from https://github.com/Alexhuszagh/BreezeStyleSheets
    //
    // TODO - need to fix a bug with the theme - observing it in qged.  See
    // https://github.com/Alexhuszagh/BreezeStyleSheets/issues/25
    QFile file(":/dark.qss");
    file.open(QFile::ReadOnly | QFile::Text);
    QTextStream stream(&file);
    setStyleSheet(stream.readAll());

    // Create the windows
    w = new QgEdMainWindow(swrast_mode, quad_mode);

    /* GED needs some information and methods from QGED - make
     * those assignment */
    struct ged *gedp = mdl->gedp;

    // Let GED know to use the QgQuadView view as its current view
    gedp->ged_gvp = w->CurrentView();

    // Set up the connections needed for embedded raytracing
    gedp->fbs_is_listening = &qdm_is_listening;
    gedp->fbs_listen_on_port = &qdm_listen_on_port;
    gedp->fbs_open_server_handler = &qdm_open_server_handler;
    gedp->fbs_close_server_handler = &qdm_close_server_handler;

    // Unfortunately, there are technical differences involved with
    // the embedded fb mechanisms depending on whether we are using
    // the system native OpenGL or our fallback software rasterizer
    int type = w->CurrentDisplay()->view_type();
#ifdef BRLCAD_OPENGL
    if (type == QgView_GL) {
	gedp->fbs_open_client_handler = &qdm_open_client_handler;
    }
#endif
    if (type == QgView_SW) {
	gedp->fbs_open_client_handler = &qdm_open_sw_client_handler;
    }
    gedp->fbs_close_client_handler = &qdm_close_client_handler;

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
    mdl->gedp->ged_pre_opendb_callback = &qged_pre_opendb_clbk;
    mdl->gedp->ged_post_opendb_callback = &qged_post_opendb_clbk;
    mdl->gedp->ged_pre_closedb_callback = &qged_pre_closedb_clbk;
    mdl->gedp->ged_post_closedb_callback = &qged_post_closedb_clbk;
    mdl->gedp->ged_db_callback_udata = (void *)qApp;

    // Assign QGED specific I/O handlers to the gedp
    mdl->gedp->ged_create_io_handler = &qt_create_io_handler;
    mdl->gedp->ged_delete_io_handler = &qt_delete_io_handler;
    mdl->gedp->ged_io_data = (void *)qApp;

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
	emit dbi_update(mdl->gedp->dbip);
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
    // TODO - free RTG.rtg_vlfree?
}

void
QgEdApp::do_quad_view_change(QgView *cv)
{
    QTCAD_SLOT("QgEdApp::do_quad_view_change", 1);
    mdl->gedp->ged_gvp = cv->view();
    emit view_update(QG_VIEW_REFRESH);
}

void
QgEdApp::do_view_changed(unsigned long long flags)
{
    bv_log(1, "QgEdApp::do_view_changed");
    QTCAD_SLOT("QgEdApp::do_view_changed", 1);

    if (flags & QG_VIEW_DRAWN) {
	// For all associated view states, execute any necessary changes to
	// view objects and lists
	std::unordered_map<BViewState *, std::unordered_set<struct bview *>> vmap;
	struct bu_ptbl *views = bv_set_views(&mdl->gedp->ged_views);
	if (mdl->gedp->dbi_state) {
	    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
		struct bview *v = (struct bview *)BU_PTBL_GET(views, i);
		BViewState *bvs = mdl->gedp->dbi_state->get_view_state(v);
		if (!bvs)
		    continue;
		vmap[bvs].insert(v);
	    }
	    std::unordered_map<BViewState *, std::unordered_set<struct bview *>>::iterator bv_it;
	    for (bv_it = vmap.begin(); bv_it != vmap.end(); bv_it++) {
		bv_it->first->redraw(NULL, bv_it->second, 1);
	    }
	}
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
    int ret = mdl->run_cmd(mdl->gedp->ged_result_str, ac, (const char **)av);
    bu_free((void *)av[1], "filename cpy");
    return ret;
}

int
qged_view_update(struct ged *gedp)
{
    int view_flags = 0;

    if (!gedp->dbi_state)
	return view_flags;

    unsigned long long updated = gedp->dbi_state->update();
    if (updated & GED_DBISTATE_VIEW_CHANGE)
	view_flags |= QG_VIEW_DRAWN;

    return view_flags;
}

extern "C" void
raytrace_start(int val, void *ctx)
{
    QgEdApp *ap = (QgEdApp *)ctx;
    ap->w->IndicateRaytraceStart(val);
}

extern "C" void
raytrace_done(int val, void *ctx)
{
    QgEdApp *ap = (QgEdApp *)ctx;
    ap->w->IndicateRaytraceDone(val);
}

int
QgEdApp::run_cmd(struct bu_vls *msg, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    int view_flags = 0;

    if (!mdl || !argc || !argv)
	return BRLCAD_ERROR;

    struct ged *gedp = mdl->gedp;

    BSelectState *ss = (gedp->dbi_state) ? gedp->dbi_state->find_selected_state(NULL) : NULL;
    select_hash = (ss) ? ss->state_hash() : 0;

    /* Set the local unit conversions */
    if (gedp->dbip) {
	struct bview *v = w->CurrentView();
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
	    gedp->ged_subprocess_init_callback = &raytrace_start;
	    gedp->ged_subprocess_end_callback = &raytrace_done;
	    gedp->ged_subprocess_clbk_context = (void *)this;
	}

	// Ask the model to execute the command
	ret = mdl->run_cmd(msg, argc, argv);

	gedp->ged_subprocess_init_callback = NULL;
	gedp->ged_subprocess_end_callback = NULL;
	gedp->ged_subprocess_clbk_context = NULL;

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

	unsigned long long cs_hash = (ss) ? ss->state_hash() : 0;
	if (cs_hash != select_hash) {
	    view_flags |= QG_VIEW_SELECT;
	    // This is what notifies currently drawn solids to update
	    // in response to a command line selection change
	    if (ss && ss->draw_sync())
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

    if (BU_STR_EQUAL(cmd, "q")) {
	w->closeEvent(NULL);
	bu_exit(0, "exit");
    }

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

    // Run as a GED command.
    int ret = BRLCAD_OK;
    ret = run_cmd(&msg, ac, (const char **)av);
    if (bu_vls_strlen(&msg) > 0 && console) {
	console->printString(bu_vls_cstr(&msg));
    }

    if (console) {
	if (ret & GED_MORE) {
	    console->prompt(bu_vls_cstr(mdl->gedp->ged_result_str));
	} else {
	    console->prompt("$ ");
	    if (history_mark_start >= 0 && history_mark_end >= 0) {
		console->consolidateHistory(history_mark_start, history_mark_end);
		history_mark_start = -1;
		history_mark_end = -1;
	    }
	}
    }

    if (mdl && mdl->gedp) {
	bu_vls_trunc(mdl->gedp->ged_result_str, 0);
    }

    bu_free((void *)cmd, "cmd");
    bu_vls_free(&msg);
    bu_free(input, "input copy");
    bu_free(av, "input argv");
}

void
QgEdApp::element_selected(QgToolPaletteElement *el)
{
    QTCAD_SLOT("QgEdApp::element_selected", 1);
    if (!el->controls->isVisible()) {
	// Apparently this can happen when we have docked widgets
	// closed and we click on the border between the view and
	// the dock - need to avoid messing with the event filters
	// if that happens or we break the user interactions.
	return;
    }

    QgView *curr_view = w->CurrentDisplay();

    if (curr_view->curr_event_filter) {
	curr_view->clear_event_filter(curr_view->curr_event_filter);
	curr_view->curr_event_filter = NULL;
    }

    if (el->use_event_filter) {
	curr_view->add_event_filter(el->controls);
	curr_view->curr_event_filter = el->controls;
    }
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
}

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


/*                      C A D A P P . C X X
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file cadapp.cxx
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
#include "qtcad/QtAppExecDialog.h"
#include "app.h"
#include "fbserv.h"

extern "C" void
qt_create_io_handler(struct ged_subprocess *p, bu_process_io_t t, ged_io_func_t callback, void *data)
{
    if (!p || !p->p || !p->gedp || !p->gedp->ged_io_data)
	return;

    BRLCAD_MainWindow *w = (BRLCAD_MainWindow *)p->gedp->ged_io_data;
    QtConsole *c = w->console;

    int fd = bu_process_fileno(p->p, t);
    if (fd < 0)
	return;

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

    BRLCAD_MainWindow *w = (BRLCAD_MainWindow *)p->gedp->ged_io_data;
    QtConsole *c = w->console;

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

    if (w->canvas)
	w->canvas->need_update(NULL);
    if (w->c4)
	w->c4->need_update(NULL);
}

extern "C" int
app_open(void *p, int argc, const char **argv)
{
    CADApp *ap = (CADApp *)p;

    QtConsole *console = ap->w->console;
    if (argc > 1) {
	if (ap->mdl)
	    delete ap->mdl;
	int ret = ap->opendb(argv[1]);
	if (ret && console) {
	    struct bu_vls msg = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&msg, "Could not open %s as a .g file\n", argv[1]) ;
	    console->printString(bu_vls_cstr(&msg));
	    bu_vls_free(&msg);
	}
    } else {
	if (console) {
	    if (ap->db_filename.length()) {
		console->printString(ap->db_filename);
		console->printString("\n");
	    } else {
		console->printString("No file specified and no file open.\n");
	    }
	}
    }

    return 0;
}

extern "C" int
app_close(void *p, int UNUSED(argc), const char **UNUSED(argv))
{
    CADApp *ap = (CADApp *)p;
    QtConsole *console = ap->w->console;
    ap->closedb();
    if (ap->w->canvas) {
	QtCADView *canvas = ap->w->canvas;
	canvas->set_view(NULL);
	//canvas->dm_set = NULL;
	canvas->set_dm_current(NULL);
	canvas->set_base2local(NULL);
	canvas->set_local2base(NULL);
    }
    if (ap->w->c4) {
	QtCADQuad *c4= ap->w->c4;
	for (int i = 1; i < 5; i++) {
	    QtCADView *c = c4->get(i);
	    //c->dm_set = NULL;
	    c->set_view(NULL);
	    c->set_dm_current(NULL);
	    c->set_base2local(NULL);
	    c->set_local2base(NULL);
	}
    }
    if (console)
	console->printString("closed database\n");

    return 0;
}

extern "C" int
app_man(void *ip, int argc, const char **argv)
{
    CADApp *ap = (CADApp *)ip;
    QtConsole *console = ap->w->console;
    int bac = (argc > 1) ? 5 : 4;
    const char *bav[6];
    char brlman[MAXPATHLEN] = {0};
    bu_dir(brlman, MAXPATHLEN, BU_DIR_BIN, "brlman", BU_DIR_EXT, NULL);
    bav[0] = (const char *)brlman;
    bav[1] = "-g";
    bav[2] = "-S";
    bav[3] = "n";
    bav[4] = (argc > 1) ? argv[1] : NULL;
    bav[5] = NULL;
    struct bu_process *p = NULL;
    bu_process_exec(&p, bav[0], bac, (const char **)bav, 0, 0);
    if (bu_process_pid(p) == -1 && console) {
	console->printString("Failed to launch man page viewer\n") ;
	return -1;
    }
    return 0;
}

void
CADApp::initialize()
{
    mdl = NULL;
    BU_LIST_INIT(&RTG.rtg_vlfree);

    // TODO - eventually, load these as plugins
    app_cmd_map[QString("open")] = &app_open;
    app_cmd_map[QString("close")] = &app_close;
    app_cmd_map[QString("man")] = &app_man;

}

void
CADApp::do_quad_view_change(QtCADView *cv)
{
    if (mdl && mdl->ctx && mdl->ctx->gedp && cv) {
	mdl->ctx->gedp->ged_gvp = cv->view();
    }
    do_gui_update_from_view_change();
}

void
CADApp::do_view_update_from_gui_change(struct bview **nv)
{
    if (mdl && mdl->ctx && mdl->ctx->gedp && nv) {
	mdl->ctx->gedp->ged_gvp = *nv;
    }
    emit gui_changed_view(NULL);
}

void
CADApp::do_db_update_from_gui_change()
{
    emit app_changed_db(NULL);
}

void
CADApp::do_gui_update_from_view_change()
{
    if (mdl && mdl->ctx && mdl->ctx->gedp) {
	emit view_change(&mdl->ctx->gedp->ged_gvp);
    }
}

void
CADApp::tree_update()
{
    if (!w)
	return;
    CADPalette *v = NULL;
    CADPalette *vc = w->vc;
    CADPalette *ic = w->ic;
    CADPalette *oc = w->oc;
    switch (interaction_mode) {
	case 0:
	    v = vc;
	    break;
	case 1:
	    v = ic;
	    break;
	case 2:
	    v = oc;
	    break;
	default:
	    v = NULL;
	    break;
    }
    if (!v)
	return;
    vc->makeCurrent(v);
    ic->makeCurrent(v);
    oc->makeCurrent(v);
}

int
CADApp::opendb(QString filename)
{

    /* First, make sure the file is actually there */
    QFileInfo fileinfo(filename);
    if (!fileinfo.exists()) return 1;

    /* If we've got something other than a .g, run a conversion if we have it */
    QString g_path = import_db(filename);

    /* If we couldn't open or convert it, we're done */
    if (g_path == "") {
        std::cout << "unsupported file type!\n";
        return 1;
    }

    if (!mdl)
	mdl = new QgModel();


    if (mdl->opendb(g_path))
	return 3;

    // We opened something - record the full path
    char fp[MAXPATHLEN];
    bu_file_realpath(g_path.toLocal8Bit().data(), fp);
    db_filename = QString(fp);

    // Connect the wires with the view(s)
    struct ged *gedp = mdl->ctx->gedp;
    if (w->canvas) {
	BU_GET(gedp->ged_gvp, struct bview);
	bv_init(gedp->ged_gvp);
	bu_vls_sprintf(&gedp->ged_gvp->gv_name, "default");
	gedp->ged_gvp->gv_db_grps = &gedp->ged_db_grps;
	gedp->ged_gvp->gv_view_shared_objs = &gedp->ged_view_shared_objs;
	gedp->ged_gvp->independent = 0;
	bu_ptbl_ins_unique(&gedp->ged_views, (long int *)gedp->ged_gvp);
	w->canvas->set_view(gedp->ged_gvp);
	//w->canvas->dm_set = gedp->ged_all_dmp;
	w->canvas->set_dm_current((struct dm **)&gedp->ged_dmp);
	w->canvas->set_base2local(&gedp->dbip->dbi_base2local);
	w->canvas->set_local2base(&gedp->dbip->dbi_local2base);
	gedp->ged_gvp = w->canvas->view();
    } else if (w->c4) {
	for (int i = 1; i < 5; i++) {
	    QtCADView *c = w->c4->get(i);
	    struct bview *nv;
	    BU_GET(nv, struct bview);
	    bv_init(nv);
	    bu_vls_sprintf(&nv->gv_name, "Q%d", i);
	    nv->gv_db_grps = &gedp->ged_db_grps;
	    nv->gv_view_shared_objs = &gedp->ged_view_shared_objs;
	    nv->independent = 0;
	    bu_ptbl_ins_unique(&gedp->ged_views, (long int *)nv);
	    c->set_view(nv);
	    //c->dm_set = gedp->ged_all_dmp;
	    c->set_dm_current((struct dm **)&gedp->ged_dmp);
	    c->set_base2local(&gedp->dbip->dbi_base2local);
	    c->set_local2base(&gedp->dbip->dbi_local2base);
	}
	w->c4->cv = &gedp->ged_gvp;
	w->c4->select(1);
	w->c4->default_views();
    }

    gedp->fbs_is_listening = &qdm_is_listening;
    gedp->fbs_listen_on_port = &qdm_listen_on_port;
    gedp->fbs_open_server_handler = &qdm_open_server_handler;
    gedp->fbs_close_server_handler = &qdm_close_server_handler;
    if (w->canvas) {
	int type = w->canvas->view_type();
#ifdef BRLCAD_OPENGL
	if (type == QtCADView_GL) {
	    gedp->fbs_open_client_handler = &qdm_open_client_handler;
	}
#endif
	if (type == QtCADView_SW) {
	    gedp->fbs_open_client_handler = &qdm_open_sw_client_handler;
	}
    }
#ifdef BRLCAD_OPENGL
    if (w->c4) {
	int type = w->c4->get(0)->view_type();
#ifdef BRLCAD_OPENGL
	if (type == QtCADView_GL) {
	    gedp->fbs_open_client_handler = &qdm_open_client_handler;
	}
#endif
	if (type == QtCADView_SW) {
	    gedp->fbs_open_client_handler = &qdm_open_sw_client_handler;
	}
    }
#endif
    gedp->fbs_close_client_handler = &qdm_close_client_handler;

    if (w && w->treeview)
	w->treeview->m->dbip = gedp->dbip;

    // Inform the world the database has changed
    emit app_changed_db((void *)gedp);

    // Also have a new view...
    emit view_change(&gedp->ged_gvp);

    //cadaccordion->highlight_selected(cadaccordion->view_obj);

    return 0;
}

void
CADApp::open_file()
{
    const char *file_filters = "BRL-CAD (*.g *.asc);;Rhino (*.3dm);;STEP (*.stp *.step);;All Files (*)";
    QString fileName = QFileDialog::getOpenFileName((QWidget *)this->w,
	    "Open Geometry File",
	    applicationDirPath(),
	    file_filters,
	    NULL,
	    QFileDialog::DontUseNativeDialog);
    if (!fileName.isEmpty()) {
	int ret = opendb(fileName.toLocal8Bit());
	if (w) {
	    if (ret) {
		w->statusBar()->showMessage("open failed");
	    } else {
		w->statusBar()->showMessage(fileName);
	    }
	}
    }
}


void
CADApp::closedb()
{
    delete mdl;
    db_filename.clear();
    if (w && w->treeview)
	w->treeview->m->dbip = DBI_NULL;
}


extern "C" void
qged_db_changed(struct db_i *UNUSED(dbip), struct directory *dp, int ctype, void *ctx)
{
    std::unordered_set<struct directory *> *changed = (std::unordered_set<struct directory *> *)ctx;
    if (ctype == 0)
	changed->insert(dp);
}


int
qged_view_update(struct ged *gedp, std::unordered_set<struct directory *> *changed)
{
    struct db_i *dbip = gedp->dbip;
    struct bview *v = gedp->ged_gvp;
    struct bu_ptbl *sg = v->gv_db_grps;
    std::set<struct bv_scene_group *> regen;
    std::set<struct bv_scene_group *> erase;
    std::set<struct bv_scene_group *>::iterator r_it;
    for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	// When checking a scene group, there are two things to confirm:
	// 1.  All dp pointers in all paths are valid
	// 2.  No dps in path have changed.
	// If either of these things is not true, the group must be
	// regenerated.
	struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
	int invalid = 0;
	for (size_t j = 0; j < BU_PTBL_LEN(&cg->g->children); j++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cg->g->children, j);
	    struct draw_update_data_t *ud = (struct draw_update_data_t *)s->s_i_data;

	    // First, check the root - if it is no longer present, we're
	    // erasing rather than redrawing.
	    struct directory *dp = db_lookup(dbip, ud->fp.fp_names[0]->d_namep, LOOKUP_QUIET);
	    if (dp == RT_DIR_NULL) {
		erase.insert(cg);
		break;
	    }

	    for (size_t fp_i = 0; fp_i < ud->fp.fp_len; fp_i++) {
		dp = db_lookup(dbip, ud->fp.fp_names[fp_i]->d_namep, LOOKUP_QUIET);
		if (dp == RT_DIR_NULL || dp != ud->fp.fp_names[fp_i] || changed->find(dp) != changed->end()) {
		    invalid = 1;
		    break;
		}
	    }
	    if (invalid) {
		regen.insert(cg);
		break;
	    }
	}
    }

    for (r_it = erase.begin(); r_it != erase.end(); r_it++) {
	struct bv_scene_group *cg = *r_it;
	struct bu_vls opath = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&opath, "%s", bu_vls_cstr(&cg->g->s_name));
	const char *av[3];
	av[0] = "erase";
	av[1] = bu_vls_cstr(&opath);
	av[2] = NULL;
	ged_exec(gedp, 2, av);
	bu_vls_free(&opath);
    }
    for (r_it = regen.begin(); r_it != regen.end(); r_it++) {
	struct bv_scene_group *cg = *r_it;
	struct bu_vls opath = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&opath, "%s", bu_vls_cstr(&cg->g->s_name));
	const char *av[4];
	av[0] = "draw";
	av[1] = "-R";
	av[2] = bu_vls_cstr(&opath);
	av[3] = NULL;
	ged_exec(gedp, 3, av);
	bu_vls_free(&opath);
    }

    return (int)(regen.size() + erase.size());
}


bool
CADApp::ged_run_cmd(struct bu_vls *msg, int argc, const char **argv)
{
    bool ret = false;

    if (!mdl || !mdl->ctx)
	return false;

    struct ged *prev_gedp = mdl->ctx->gedp;
    struct ged *gedp = mdl->ctx->gedp;

    if (gedp) {
	gedp->ged_create_io_handler = &qt_create_io_handler;
	gedp->ged_delete_io_handler = &qt_delete_io_handler;
	gedp->ged_io_data = (void *)this->w;
    }

    if (w->canvas)
	w->canvas->stash_hashes();
    if (w->c4)
	w->c4->stash_hashes();


    // Ask the model to execute the command
    ret = mdl->run_cmd(msg, argc, argv);
    if (!ret)
	return false;

    // It's possible that a ged_exec will introduce a new gedp - set up accordingly
    gedp = mdl->ctx->gedp;
    if (gedp && prev_gedp != gedp) {
	bu_ptbl_reset(gedp->ged_all_dmp);
	if (w->canvas) {
	    gedp->ged_dmp = w->canvas->dmp();
	    gedp->ged_gvp = w->canvas->view();
	    dm_set_vp(w->canvas->dmp(), &gedp->ged_gvp->gv_scale);
	}
	if (w->c4) {
	    QtCADView *c = w->c4->get();
	    gedp->ged_dmp = c->dmp();
	    gedp->ged_gvp = c->view();
	    dm_set_vp(c->dmp(), &gedp->ged_gvp->gv_scale);
	}
	bu_ptbl_ins_unique(gedp->ged_all_dmp, (long int *)gedp->ged_dmp);
    }

    if (gedp) {
	if (w->canvas) {
	    w->canvas->set_base2local(&gedp->dbip->dbi_base2local);
	    w->canvas->set_local2base(&gedp->dbip->dbi_local2base);
	}
	if (w->c4 && w->c4->get(0)) {
	    w->c4->get(0)->set_base2local(&gedp->dbip->dbi_base2local);
	    w->c4->get(0)->set_local2base(&gedp->dbip->dbi_local2base);
	}
	// Checks the dp edit flags and does any necessary redrawing.  If
	// anything changed with the geometry, we also need to redraw
	if (qged_view_update(gedp, &mdl->changed_dp) > 0) {
	    if (w->canvas)
		w->canvas->need_update(NULL);
	    if (w->c4)
		w->c4->need_update(NULL);
	}
    } else {
	// gedp == NULL - can't cheat and use the gedp pointer
	if (prev_gedp != gedp) {
	    if (w->canvas) {
		w->canvas->need_update(NULL);
	    }
	    if (w->c4) {
		QtCADView *c = w->c4->get();
		c->update();
	    }
	}
    }

    /* Check if the ged_exec call changed either the display manager or
     * the view settings - in either case we'll need to redraw */
    if (w->canvas) {
	ret = w->canvas->diff_hashes();
    }
    if (w->c4) {
	ret = w->c4->diff_hashes();
    }

    return ret;
}

void
CADApp::run_cmd(const QString &command)
{
    if (!w)
	return;

    QtConsole *console = w->console;
    if (BU_STR_EQUAL(command.toStdString().c_str(), "q"))
	bu_exit(0, "exit");

    if (BU_STR_EQUAL(command.toStdString().c_str(), "clear")) {
	if (console) {
	    console->clear();
	    console->prompt("$ ");
	}
	return;
    }

    // make an argv array
    struct bu_vls ged_prefixed = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&ged_prefixed, "%s", command.toStdString().c_str());
    char *input = bu_strdup(bu_vls_addr(&ged_prefixed));
    bu_vls_free(&ged_prefixed);
    char **av = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
    int ac = bu_argv_from_string(av, strlen(input), input);
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    // First, see if we have an application level command.
    int cmd_run = 0;
    if (app_cmd_map.contains(QString(av[0]))) {
	app_cmd_ptr acmd = app_cmd_map.value(QString(av[0]));
	(*acmd)((void *)this, ac, (const char **)av);
	cmd_run = 1;
    }

    // If it wasn't an app level command, try it as a GED command.
    if (!cmd_run) {
	bool ret = ged_run_cmd(&msg, ac, (const char **)av);
	if (bu_vls_strlen(&msg) > 0 && console) {
	    console->printString(bu_vls_cstr(&msg));
	}
	if (ret && mdl && mdl->ctx && mdl->ctx->gedp)
	    emit view_change(&mdl->ctx->gedp->ged_gvp);
    }

    if (mdl && mdl->ctx && mdl->ctx->gedp) {
	bu_vls_trunc(mdl->ctx->gedp->ged_result_str, 0);
    }

    bu_vls_free(&msg);
    bu_free(input, "input copy");
    bu_free(av, "input argv");

    if (console)
	console->prompt("$ ");

}

void
CADApp::element_selected(QToolPaletteElement *el)
{
    if (!el->controls->isVisible()) {
	// Apparently this can happen when we have docked widgets
	// closed and we click on the border between the view and
	// the dock - need to avoid messing with the event filters
	// if that happens or we break the user interactions.
	return;
    }

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

int
CADApp::exec_console_app_in_window(QString command, QStringList options, QString lfile)
{
    if (command.length() > 0) {

	QtAppExecDialog *out_win = new QtAppExecDialog(0, command, options, lfile);
	QString win_title("Running ");
	win_title.append(command);
	out_win->setWindowTitle(win_title);
	out_win->proc = new QProcess(out_win);
	out_win->console->setMinimumHeight(800);
	out_win->console->setMinimumWidth(800);
	out_win->console->printString(command);
	out_win->console->printString(QString(" "));
	out_win->console->printString(options.join(" "));
	out_win->console->printString(QString("\n"));
	out_win->proc->setProgram(command);
	out_win->proc->setArguments(options);
	connect(out_win->proc, &QProcess::readyReadStandardOutput, out_win, &QtAppExecDialog::read_stdout);
	connect(out_win->proc, &QProcess::readyReadStandardError, out_win, &QtAppExecDialog::read_stderr);
	connect(out_win->proc, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), out_win, &QtAppExecDialog::process_done);
	out_win->proc->start();
	out_win->exec();
    }
    return 0;
}

void
CADApp::readSettings()
{
    QSettings settings("BRL-CAD", "QGED");

    settings.beginGroup("BRLCAD_MainWindow");
    if (w)
	w->resize(settings.value("size", QSize(1100, 800)).toSize());
    settings.endGroup();
}

void
CADApp::write_settings()
{
    QSettings settings("BRL-CAD", "QGED");

    if (w) {
	settings.beginGroup("BRLCAD_MainWindow");
	settings.setValue("size", w->size());
	settings.endGroup();
    }
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


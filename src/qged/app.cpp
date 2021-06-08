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
	w->canvas->need_update();
#if 0
    if (w->c4)
	w->c4->need_update();
#endif
}

void
CADApp::initialize()
{
    gedp = GED_NULL;
    BU_LIST_INIT(&RTG.rtg_vlfree);
}

struct db_i *
CADApp::dbip()
{
    if (gedp == GED_NULL) return DBI_NULL;
    if (gedp->ged_wdbp == RT_WDB_NULL) return DBI_NULL;
    return gedp->ged_wdbp->dbip;
}

struct rt_wdb *
CADApp::wdbp()
{
    if (gedp == GED_NULL) return RT_WDB_NULL;
    return gedp->ged_wdbp;
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

    // If we've already got an open file, close it.
    if (gedp != GED_NULL) (void)closedb();

    // Call BRL-CAD's database open function
    gedp = ged_open("db", g_path.toLocal8Bit(), 1);

    if (!gedp)
	return 3;

    db_update_nref(gedp->ged_wdbp->dbip, &rt_uniresource);

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
	w->canvas->set_base2local(&gedp->ged_wdbp->dbip->dbi_base2local);
	w->canvas->set_local2base(&gedp->ged_wdbp->dbip->dbi_local2base);
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
	    c->set_base2local(&gedp->ged_wdbp->dbip->dbi_base2local);
	    c->set_local2base(&gedp->ged_wdbp->dbip->dbi_local2base);
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

    current_file = filename;

    cadtreeview->m->dbip = dbip();

    // Inform the world the database has changed
    emit db_change();

    //cadaccordion->highlight_selected(cadaccordion->view_obj);

    return 0;
}

void
CADApp::closedb()
{
    if (gedp != GED_NULL) {
        ged_close(gedp);
        BU_PUT(gedp, struct ged);
    }
    gedp = GED_NULL;
    current_file.clear();
    cadtreeview->m->dbip = DBI_NULL;
}

int
CADApp::register_gui_command(QString cmdname, gui_cmd_ptr func, QString role)
{
    if (role == QString()) {
	if (gui_cmd_map.find(cmdname) != gui_cmd_map.end()) return -1;
	gui_cmd_map.insert(cmdname, func);
    }
    if (role == QString("preprocess")) {
	if (preprocess_cmd_map.find(cmdname) != preprocess_cmd_map.end()) return -1;
	preprocess_cmd_map.insert(cmdname, func);
    }
    if (role == QString("postprocess")) {
	if (postprocess_cmd_map.find(cmdname) != postprocess_cmd_map.end()) return -1;
	postprocess_cmd_map.insert(cmdname, func);
    }
    return 0;
}

void
CADApp::ged_run_cmd(struct bu_vls *msg, int argc, const char **argv)
{
    struct ged *prev_gedp = gedp;

    if (gedp) {
	gedp->ged_create_io_handler = &qt_create_io_handler;
	gedp->ged_delete_io_handler = &qt_delete_io_handler;
	gedp->ged_io_data = (void *)this->w;
    }

    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);

    if (ged_cmd_valid(argv[0], NULL)) {
	const char *ccmd = NULL;
	int edist = ged_cmd_lookup(&ccmd, argv[0]);
	if (edist) {
	    if (msg)
		bu_vls_sprintf(msg, "Command %s not found, did you mean %s (edit distance %d)?\n", argv[0],   ccmd, edist);
	    return;
	}
    } else {

	if (w->canvas)
	    w->canvas->stash_hashes();
#if 0
	if (w->c4)
	    w->c4->stash_hashes();
#endif
	if (gedp) {
	    // Clear the edit flags ahead of the ged_exec call, so we can tell if
	    // any geometry changed.
	    struct directory *dp;
	    for (int i = 0; i < RT_DBNHASH; i++) {
		for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		    dp->edit_flag = 0;
		}
	    }
	}

	ged_exec(gedp, argc, argv);
	if (msg && gedp)
	    bu_vls_printf(msg, "%s", bu_vls_cstr(gedp->ged_result_str));


	// It's possible that a ged_exec will introduce a new gedp - set up accordingly
	if (gedp && prev_gedp != gedp) {
	    bu_ptbl_reset(gedp->ged_all_dmp);
	    if (w->canvas) {
		gedp->ged_dmp = w->canvas->dmp();
		gedp->ged_gvp = w->canvas->view();
		dm_set_vp(w->canvas->dmp(), &gedp->ged_gvp->gv_scale);
	    }
#if 0
	    if (w->c4) {
		QtCADView *c = w->c4->get();
		gedp->ged_dmp = c->dmp();
		gedp->ged_gvp = c->view();
		dm_set_vp(c->dmp(), &gedp->ged_gvp->gv_scale);
	    }
#endif
	    bu_ptbl_ins_unique(gedp->ged_all_dmp, (long int *)gedp->ged_dmp);
	}

	if (gedp) {
	    if (w->canvas) {
		w->canvas->set_base2local(&gedp->ged_wdbp->dbip->dbi_base2local);
		w->canvas->set_local2base(&gedp->ged_wdbp->dbip->dbi_local2base);
	    }
#if 0
	    if (w->c4 && w->c4->get(0)) {
		w->c4->get(0)->set_base2local(&gedp->ged_wdbp->dbip->dbi_base2local);
		w->c4->get(0)->set_local2base(&gedp->ged_wdbp->dbip->dbi_local2base);
	    }
#endif
	    // Checks the dp edit flags and does any necessary redrawing.  If
	    // anything changed with the geometry, we also need to redraw
	    if (ged_view_update(gedp) > 0) {
		if (w->canvas)
		    w->canvas->need_update();
#if 0
		if (w->c4)
		    w->c4->need_update();
#endif
	    }
	} else {
	    // gedp == NULL - can't cheat and use the gedp pointer
	    if (prev_gedp != gedp) {
		if (w->canvas) {
		    w->canvas->need_update();
		}
#if 0
		if (w->c4) {
		    QtCADView *c = w->c4->get();
		    c->update();
		}
#endif
	    }
	}

	/* Check if the ged_exec call changed either the display manager or
	 * the view settings - in either case we'll need to redraw */
	if (w->canvas)
	    w->canvas->diff_hashes();

#if 0
	if (w->c4)
	    w->c4->diff_hashes();
#endif

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


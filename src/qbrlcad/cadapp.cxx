/*                      C A D A P P . C X X
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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

#include "cadapp.h"
#include "cadappexec.h"
#include <QFileInfo>
#include <QFile>
#include <QPlainTextEdit>
#include <QTextStream>
#include "bu/malloc.h"
#include "bu/file.h"

void
CADApp::initialize()
{
    ged_pointer = GED_NULL;
    current_idx = QModelIndex();
    interaction_mode = 0;
}

struct db_i *
CADApp::dbip()
{
    if (!ged_pointer || ged_pointer == GED_NULL) return DBI_NULL;
    if (!ged_pointer->ged_wdbp || ged_pointer->ged_wdbp == RT_WDB_NULL) return DBI_NULL;
    return ged_pointer->ged_wdbp->dbip;
}

struct rt_wdb *
CADApp::wdbp()
{
    if (!ged_pointer || ged_pointer == GED_NULL) return RT_WDB_NULL;
    return ged_pointer->ged_wdbp;
}

struct ged *
CADApp::gedp()
{
    return ged_pointer;
}

int
CADApp::opendb(QString filename)
{
    struct db_i *f_dbip = DBI_NULL;
    struct rt_wdb *f_wdbp = RT_WDB_NULL;

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
    if (ged_pointer != GED_NULL) (void)closedb();

    // Call BRL-CAD's database open function
    if ((f_dbip = db_open(g_path.toLocal8Bit(), DB_OPEN_READONLY)) == DBI_NULL) {
        return 2;
    }

    // Do the rest of the standard initialization steps
    RT_CK_DBI(f_dbip);
    if (db_dirbuild(f_dbip) < 0) {
        db_close(f_dbip);
        return 3;
    }
    db_update_nref(f_dbip, &rt_uniresource);

    f_wdbp = wdb_dbopen(f_dbip, RT_WDB_TYPE_DB_DISK);
    BU_GET(ged_pointer, struct ged);
    GED_INIT(ged_pointer, f_wdbp);

    current_file = filename;

    // Inform the world the database has changed
    emit db_change();

    return 0;
}

void
CADApp::closedb()
{
    if (ged_pointer && ged_pointer != GED_NULL) {
        ged_close(ged_pointer);
        BU_PUT(ged_pointer, struct ged);
    }
    ged_pointer = GED_NULL;
    current_file.clear();
}

// TODO - make the type an enum or bit flag or something, instead of multiple integers...
int
CADApp::register_command(QString cmdname, ged_func_ptr func, QString role)
{
    if (role == QString()) {
	if (cmd_map.find(cmdname) != cmd_map.end()) return -1;
	cmd_map.insert(cmdname, func);
    }
    if (role == QString("edit")) {
	edit_cmds.insert(cmdname);
    }
    if (role == QString("view")) {
	view_cmds.insert(cmdname);
    }
    return 0;
}

int
CADApp::register_gui_command(QString cmdname, gui_cmd_ptr func, QString role)
{
    if (role == QString()) {
	if (gui_cmd_map.find(cmdname) != gui_cmd_map.end()) return -1;
	gui_cmd_map.insert(cmdname, func);
    }
    if (role == QString("edit")) {
	edit_cmds.insert(cmdname);
    }
    if (role == QString("view")) {
	view_cmds.insert(cmdname);
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

// TODO - the function below does not take into account the possibility of
// different command prompt languages - the "exec_command" call needs to
// be a set of such calls, one per language, with the console switching
// which one it calls based on a language setting.
int
CADApp::exec_command(QString *command, QString *result)
{
    int ret = -1;
    QString lcommand = *command;
    QMap<QString, gui_cmd_ptr>::iterator preprocess_cmd_itr;
    QMap<QString, ged_func_ptr>::iterator ged_cmd_itr;
    QMap<QString, gui_cmd_ptr>::iterator gui_cmd_itr;
    QMap<QString, gui_cmd_ptr>::iterator postprocess_cmd_itr;
    char *lcmd = NULL;
    char **largv = NULL;
    int largc = 0;

    if (ged_pointer != GED_NULL && command && command->length() > 0) {

	QStringList command_items = lcommand.split(" ", QString::SkipEmptyParts);

	QString cargv0 = command_items.at(0);

	// First, see if the command needs any extra information from the application
	// before being run (list of items in view, for example.)
	preprocess_cmd_itr = preprocess_cmd_map.find(cargv0);
	if (preprocess_cmd_itr != preprocess_cmd_map.end()) {
	    (*(preprocess_cmd_itr.value()))(&lcommand, (CADApp *)qApp);
	}

	ged_cmd_itr = cmd_map.find(cargv0);
	if (ged_cmd_itr != cmd_map.end()) {
	    // Prepare libged arguments
	    lcmd = bu_strdup(lcommand.toLocal8Bit());
	    largv = (char **)bu_calloc(lcommand.length()/2+1, sizeof(char *), "cmd_eval argv");
	    largc = bu_argv_from_string(largv, lcommand.length()/2, lcmd);
	    bu_vls_trunc(ged_pointer->ged_result_str, 0);

	    // TODO - need a way to launch commands in a separate thread, or some other non-blocking
	    // fashion as far as the rest of the gui is concerned.  Also need to support Ctrl-C
	    // to abort a command on the terminal.  Don't worry about allowing other commands to
	    // execute from the command prompt while one is running, since that will confuse the
	    // text output quite a lot.  However, view commands should return while allowing long
	    // drawing routines to execute in the background - using something like Z or B should
	    // abort drawing and clear the view, but otherwise let it complete...
	    ret = (*(ged_cmd_itr.value()))(ged_pointer, largc, (const char **)largv);

	    if (result && bu_vls_strlen(ged_pointer->ged_result_str) > 0) {
		*result = QString(QLatin1String(bu_vls_addr(ged_pointer->ged_result_str)));
	    }

	    // Now that the command is run, emit any signals that need emitting.  What would
	    // really handle this properly is for the ged structure to contain a list of directory
	    // pointers that were impacted by the command, so we could emit signals with the
	    // specifics of what objects need updating.
	    if (edit_cmds.find(QString(largv[0])) != edit_cmds.end()) emit db_change();
	    if (view_cmds.find(QString(largv[0])) != view_cmds.end()) emit view_change();
	    bu_free(lcmd, "free tmp cmd str");
	    bu_free(largv, "free tmp argv");
	    goto postprocess;
	}

	gui_cmd_itr = gui_cmd_map.find(cargv0);
	if (gui_cmd_itr != gui_cmd_map.end()) {
	    QString args(lcommand);
	    args.replace(0, cargv0.length()+1, QString(""));
	    ret = (*(gui_cmd_itr.value()))(&args, (CADApp *)qApp);
	    goto postprocess;
	}

postprocess:
	// Take care of any results post-processing needed
	postprocess_cmd_itr = postprocess_cmd_map.find(cargv0);
	if (postprocess_cmd_itr != postprocess_cmd_map.end()) {
	    ret = (*(postprocess_cmd_itr.value()))(result, (CADApp *)qApp);
	}

	if (ret == -1)
	    *result = QString("command not found");
    }
    return ret;
}


int
CADApp::exec_console_app_in_window(QString command, QStringList options, QString lfile)
{
    if (command.length() > 0) {

	QDialog_App *out_win = new QDialog_App(0, command, options, lfile);
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
	connect(out_win->proc, SIGNAL(readyReadStandardOutput()), out_win, SLOT(read_stdout()) );
	connect(out_win->proc, SIGNAL(readyReadStandardError()), out_win, SLOT(read_stderr()) );
	connect(out_win->proc, SIGNAL(finished(int, QProcess::ExitStatus)), out_win, SLOT(process_done(int, QProcess::ExitStatus)) );
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


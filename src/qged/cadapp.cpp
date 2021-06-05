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
#include "cadapp.h"
#include "cadappexec.h"
#include "cadaccordion.h"
#include "bu/malloc.h"
#include "bu/file.h"

void
CADApp::initialize()
{
    ged_pointer = GED_NULL;
    current_idx = QModelIndex();
    interaction_mode = 0;
    BU_LIST_INIT(&RTG.rtg_vlfree);
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

    interaction_mode = 0;
    cadaccordion->highlight_selected(cadaccordion->view_obj);

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
    const char *ccmd = NULL;
    int edist = 0;
    struct bu_vls rmsg = BU_VLS_INIT_ZERO;

    if (ged_pointer != GED_NULL && command && command->length() > 0) {

	QStringList command_items = lcommand.split(" ", QString::SkipEmptyParts);

	QString cargv0 = command_items.at(0);

	// First, see if the command needs any extra information from the application
	// before being run (list of items in view, for example.)
	preprocess_cmd_itr = preprocess_cmd_map.find(cargv0);
	if (preprocess_cmd_itr != preprocess_cmd_map.end()) {
	    (*(preprocess_cmd_itr.value()))(&lcommand, (CADApp *)qApp);
	}

	if (!ged_cmd_valid(cargv0.toLocal8Bit().constData(), NULL)) {
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
	    ret = ged_exec(ged_pointer, largc, (const char **)largv);

	    if (result && bu_vls_strlen(ged_pointer->ged_result_str) > 0) {
		*result = QString(QLatin1String(bu_vls_addr(ged_pointer->ged_result_str)));
	    }

	    // Now that the command is run, emit any signals that need emitting.  TODO - we need
	    // some better mechanism for this.  Once we move to transactions, we can analyze the
	    // transaction list - hardcoding this per-command is not a great way to go...
	    //if (edit_cmds.find(QString(largv[0])) != edit_cmds.end()) emit db_change();
	    //if (view_cmds.find(QString(largv[0])) != view_cmds.end()) emit view_change();
	    bu_free(lcmd, "free tmp cmd str");
	    bu_free(largv, "free tmp argv");
	    goto postprocess;
	}

	// Not a valid GED command - see if it's an application level command
	gui_cmd_itr = gui_cmd_map.find(cargv0);
	if (gui_cmd_itr != gui_cmd_map.end()) {
	    QString args(lcommand);
	    args.replace(0, cargv0.length()+1, QString(""));
	    ret = (*(gui_cmd_itr.value()))(&args, (CADApp *)qApp);
	    goto postprocess;
	}

	// If we didn't find the command either as a ged command or a gui command,
	// see if libged has a suggestion.
	edist = ged_cmd_lookup(&ccmd, cargv0.toLocal8Bit().constData());
	if (edist <= cargv0.length()/2) {
	    bu_vls_sprintf(&rmsg, "Command \"%s\" not found, did you mean \"%s\"?\n", cargv0.toLocal8Bit().constData(), ccmd);
	    *result = QString(QLatin1String(bu_vls_cstr(&rmsg)));
	    ret = 1;
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

    bu_vls_free(&rmsg);
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
	connect(out_win->proc, &QProcess::readyReadStandardOutput, out_win, &QDialog_App::read_stdout);
	connect(out_win->proc, &QProcess::readyReadStandardError, out_win, &QDialog_App::read_stderr);
	connect(out_win->proc, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), out_win, &QDialog_App::process_done);
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


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
    current_object = RT_DIR_NULL;
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
CADApp::register_command(QString cmdname, ged_func_ptr func, int db_changer, int view_changer)
{
    if (cmd_map.find(cmdname) != cmd_map.end()) return -1;
    cmd_map.insert(cmdname, func);
    if (db_changer) {
	edit_cmds.insert(cmdname);
	view_cmds.insert(cmdname);
    }
    if (view_changer && !db_changer) {
	view_cmds.insert(cmdname);
    }
    return 0;
}

int
CADApp::exec_command(QString *command, QString *result)
{
    int ret = 0;
    if (ged_pointer != GED_NULL && command && command->length() > 0) {
	char *lcmd = bu_strdup(command->toLocal8Bit());
	char **largv = (char **)bu_calloc(command->length()/2+1, sizeof(char *), "cmd_eval argv");
	int largc = bu_argv_from_string(largv, command->length()/2, lcmd);

	QMap<QString, ged_func_ptr>::iterator cmd_itr = cmd_map.find(QString(largv[0]));
	if (cmd_itr != cmd_map.end()) {
	    int is_edit_cmd = 0;
	    int is_view_cmd = 0;
	    if (edit_cmds.find(QString(largv[0])) != edit_cmds.end()) is_edit_cmd = 1;
	    if (view_cmds.find(QString(largv[0])) != view_cmds.end()) is_view_cmd = 1;
	    // TODO - need a way to launch commands in a separate thread, or some other non-blocking
	    // fashion as far as the rest of the gui is concerned.  Also need to support Ctrl-C
	    // to abort a command on the terminal.  Don't worry about allowing other commands to
	    // execute from the command prompt while one is running, since that will confuse the
	    // text output quite a lot.  However, view commands should return while allowing long
	    // drawing routines to execute in the background - using something like Z or B should
	    // abort drawing and clear the view, but otherwise let it complete...
	    bu_vls_trunc(ged_pointer->ged_result_str, 0);
	    ret = (*(cmd_itr.value()))(ged_pointer, largc, (const char **)largv);
	    if (result && bu_vls_strlen(ged_pointer->ged_result_str) > 0) {
		*result = QString(QLatin1String(bu_vls_addr(ged_pointer->ged_result_str)));
	    }
	    if (is_edit_cmd) emit db_change();
	    if (is_view_cmd) emit view_change();
	} else {
	    *result = QString("command not found");
	    ret = -1;
	}
	bu_free(lcmd, "free tmp cmd str");
	bu_free(largv, "free tmp argv");
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
	out_win->console->printString(" ");
	out_win->console->printString(options.join(" "));
	out_win->console->printString("\n");
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

void
CADApp::update_current_object(const QModelIndex & index)
{
    current_idx = index;
    current_object = (struct directory *)(index.data(DirectoryInternalRole).value<void *>());
    emit treeview_needs_update(index);
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


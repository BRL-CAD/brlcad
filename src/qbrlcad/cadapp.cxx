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
#include <QFileInfo>
#include "bu/malloc.h"
#include "bu/file.h"

void
CADApp::initialize()
{
    ged_pointer = GED_NULL;
}

struct db_i *
CADApp::dbip()
{
    return ged_pointer->ged_wdbp->dbip;
}

struct rt_wdb *
CADApp::wdbp()
{
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
    QFileInfo *fileinfo = new QFileInfo(filename);
    if (!fileinfo->exists()) {
        delete fileinfo;
        return 1;
    }

    // TODO - somewhere in here we'll need to handle
    // starting the conversion process if we don't have
    // a .g file.  For now, punt unless it's a .g file.
    if (fileinfo->suffix() != "g") {
        std::cout << "unsupported file type!\n";
        delete fileinfo;
        return 1;
    }

    // If we've already got an open file, close it.
    if (ged_pointer != GED_NULL) (void)closedb();

    // Call BRL-CAD's database open function
    if ((f_dbip = db_open(filename.toLocal8Bit(), DB_OPEN_READONLY)) == DBI_NULL) {
        delete fileinfo;
        return 2;
    }

    // Do the rest of the standard initialization steps
    RT_CK_DBI(f_dbip);
    if (db_dirbuild(f_dbip) < 0) {
        db_close(f_dbip);
        delete fileinfo;
        return 3;
    }
    db_update_nref(f_dbip, &rt_uniresource);

    f_wdbp = wdb_dbopen(f_dbip, RT_WDB_TYPE_DB_DISK);
    BU_GET(ged_pointer, struct ged);
    GED_INIT(ged_pointer, f_wdbp);

    current_file = filename;

    // Inform the world the database has changed
    emit db_change();

    delete fileinfo;
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
	    // TODO - launch commands in a separate thread.  Need some locks:
	    // edit_lock - view and regular commands can run, but not other edit commands until the locking command is done
	    // view_lock - a command is trying to update the view already, wait for it (maybe override it?)
	    // all_lock - waiting on a normal command - need some sort of "special" command to abort long running commands
	    // io_lock? - need to figure out how to keep output on console at least semi coherent
	    //
	    // also use a timer - wait at least a second or two before allowing commands in so most simple cases will be linear.  Also need some way to make sure scripts execute serially...
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


/*                        C A D A P P . H
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
/** @file cadapp.h
 *
 *  Specialization of QApplication that adds information specific
 *  to BRL-CAD's data and functionality
 *
 */

#ifndef CADAPP_H
#define CADAPP_H

#include <QApplication>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QSet>
#include <QModelIndex>

#include "raytrace.h"
#include "ged.h"
#include "qtcad/QtCADTree.h"

#include "main_window.h"

class CADApp;

typedef int (*gui_cmd_ptr)(QString *command_string, CADApp *app);
#define GUI_CMD_PTR_NULL ((gui_cmd_ptr)0)

class CADApp : public QApplication
{
    Q_OBJECT

    public:
	CADApp(int &argc, char *argv[]) :QApplication(argc, argv) {};
	~CADApp() {};

	void initialize();
	int opendb(QString filename);
	void closedb();

	int register_gui_command(QString cmdname, gui_cmd_ptr func, QString role = QString());

	void ged_run_cmd(struct bu_vls *msg, int argc, const char **argv);

	int exec_console_app_in_window(QString command, QStringList options, QString log_file = "");

	struct ged *gedp;
	struct db_i *dbip();
	struct rt_wdb *wdbp();

	BRLCAD_MainWindow *w = NULL;
	CADTreeView *cadtreeview = NULL;

    signals:
	void db_change();  // TODO - need this to carry some information about what has changed, if possible...
	void view_change();

    public:
	QString current_file;
	QMap<QString, gui_cmd_ptr> gui_cmd_map;
	QMap<QString, gui_cmd_ptr> preprocess_cmd_map;
	QMap<QString, gui_cmd_ptr> postprocess_cmd_map;
};

QString import_db(QString filename);

enum CADDataRoles {
    BoolInternalRole = Qt::UserRole + 1000,
    BoolDisplayRole = Qt::UserRole + 1001,
    DirectoryInternalRole = Qt::UserRole + 1002,
    TypeIconDisplayRole = Qt::UserRole + 1003,
    RelatedHighlightDisplayRole = Qt::UserRole + 1004,
    InstanceHighlightDisplayRole = Qt::UserRole + 1005
};


#endif // CADAPP_H

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


/*                        C A D A P P . H
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

#include "cadtreeview.h"
#include "raytrace.h"
#include "ged.h"

class CADApp : public QApplication
{
    Q_OBJECT

    public:
	CADApp(int &argc, char *argv[]) :QApplication(argc, argv) {};
	~CADApp() {};

	void initialize();
	int opendb(QString filename);
	void closedb();

	int register_command(QString cmdname, ged_func_ptr func, int db_changer = 0, int view_changer = 0);
	int exec_command(QString *command, QString *result);

	int exec_console_app_in_window(QString command, QStringList options, QString log_file = "");

	struct ged *gedp();
	struct db_i *dbip();
	struct rt_wdb *wdbp();

	QModelIndex current_idx;
	struct directory *current_object;

	CADTreeView *cadtreeview;

    signals:
	void db_change();  // TODO - need this to carry some information about what has changed, if possible...
	void view_change();
	void treeview_needs_update(const QModelIndex & index);

    public slots:
	void update_current_object(const QModelIndex & index);

    private:
	struct ged *ged_pointer;
	QString current_file;
	QMap<QString, ged_func_ptr> cmd_map;
	QSet<QString> edit_cmds;  // Commands that potentially change the database contents */
	QSet<QString> view_cmds;  // Commands that potentially change the view, but not the database contents */

};

QString import_db(QString filename);

enum CADDataRoles {
    BoolInternalRole = Qt::UserRole + 1000,
    BoolDisplayRole = Qt::UserRole + 1001,
    DirectoryInternalRole = Qt::UserRole + 1002,
    TypeIconDisplayRole = Qt::UserRole + 1003,
    RelatedHighlightDisplayRole = Qt::UserRole + 1004
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


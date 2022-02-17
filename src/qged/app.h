/*                        C A D A P P . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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

#include "bv.h"
#include "raytrace.h"
#include "ged.h"
#include "qtcad/QgSelectionProxyModel.h"

#include "main_window.h"

class CADApp;

/* Command type for application level commands */
typedef int (*app_cmd_ptr)(void *, int, const char **);

/* Derive the core application class from QApplication.  This is central to
 * QGED, as the primary application state is managed in this container.
 * The main window is primarily responsible for graphical windows, but
 * application wide facilities like the current GED pointer and the ability
 * to run commands are defined here. */

class CADApp : public QApplication
{
    Q_OBJECT

    public:
	CADApp(int &argc, char *argv[]) :QApplication(argc, argv) {
	    QgModel *m = new QgModel();
	    mdl = new QgSelectionProxyModel();
	    mdl->setSourceModel(m);
	    BU_LIST_INIT(&RTG.rtg_vlfree);
	    connect(m, &QgModel::mdl_changed_db, this, &CADApp::view_connect);
	};
	~CADApp() {};

	void initialize();

	int run_cmd(struct bu_vls *msg, int argc, const char **argv);
	bool view_dirty = false;

	int opendb(QString filename);
	void closedb();

	QgSelectionProxyModel *mdl = NULL;
	int exec_console_app_in_window(QString command, QStringList options, QString log_file = "");

	//int prev_interaction_mode = 0;

    signals:
	void view_change(struct bview **);
	void gui_changed_view(void *);

        /* Menu slots */
    public slots:
	void open_file();
        void write_settings();
	void view_connect();

	/* GUI/View connection slots */
    public slots:

	// Called if the view is being altered by one of the app's graphical
	// elements, rather than being driven by mouse movements or commands.
	// Changes the view settings, but does NOT update GUI elements.
	void do_view_update_from_gui_change(struct bview **);

	// Called if the db is being altered by one of the app's graphical
	// elements, rather than being driven by mouse movements or commands.
	void do_db_update_from_gui_change();

        // Called when the view has been changed by mouse events or commands.
	// It is responsible for updating the GUI elements that might be
	// displaying view information.  Does not change the view settings.
        void do_gui_update_from_view_change();

	// This slot is used for quad view configurations - it is called if the
	// user uses the mouse to select one of multiple views.  This slot has
	// the responsibility to notify GUI elements of a view change via
	// do_gui_view_change, after updating the current gedp->ged_gvp
	// pointer to refer to the now-current view.
	void do_quad_view_change(QtCADView *);

       	/* Utility slots */
    public slots:
	void run_qcmd(const QString &command);
        void readSettings();
	void tree_update();
	void element_selected(QToolPaletteElement *el);

    public:
	BRLCAD_MainWindow *w = NULL;
	QtCADView *curr_view = NULL;
	QString db_filename;
	struct bu_vls init_msgs = BU_VLS_INIT_ZERO;

    private:
	QMap<QString, app_cmd_ptr> app_cmd_map;
	std::vector<char *> tmp_av;
};

QString convert_to_g(QString filename);

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


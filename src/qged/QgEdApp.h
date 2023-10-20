/*                        Q G E D A P P . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2023 United States Government as represented by
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
/** @file QgEdApp.h
 *
 * Specialization of QApplication that adds information specific
 * to BRL-CAD's data and functionality
 */

#ifndef QGEDAPP_H
#define QGEDAPP_H

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
#include "qtcad/QgModel.h"
#include "qtcad/QgTreeView.h"
#include "qtcad/QgSignalFlags.h"

#include "QgEdMainWindow.h"

/* Command type for application level commands */

/* Derive the core application class from QApplication.  This is central to
 * QGED, as the primary application state is managed in this container.
 * The main window is primarily responsible for graphical windows, but
 * application wide facilities like the current GED pointer and the ability
 * to run commands are defined here. */

class QgEdApp : public QApplication
{
    Q_OBJECT

    public:
	QgEdApp(int &argc, char *argv[], int swrast_mode = 0, int quad_mode = 0);
	~QgEdApp();

	int run_cmd(struct bu_vls *msg, int argc, const char **argv);
	int load_g_file(const char *gfile = NULL, bool do_conversion = true);

	QgModel *mdl = NULL;

    signals:
	void view_update(unsigned long long);
	void dbi_update(struct db_i *dbip);

        /* Menu slots */
    public slots:
	void open_file();
        void write_settings();

	/* GUI/View connection slots */
    public slots:

	// "Universal" slot to be connected to for widgets altering the view.
	// Will emit the view_update signal to notify all concerned widgets in
	// the application of the change.  Note that nothing attached to that
	// signal should trigger ANY logic (directly OR indirectly) that leads
	// back to this slot being called again, or an infinite loop may
	// result.
	void do_view_changed(unsigned long long);

	// This slot is used for quad view configurations - it is called if the
	// user uses the mouse to select one of multiple views.  This slot has
	// the responsibility to notify GUI elements of a view change via
	// do_view_change, after updating the current gedp->ged_gvp pointer to
	// refer to the now-current view.
	void do_quad_view_change(QgView *);

       	/* Utility slots */
    public slots:
	void run_qcmd(const QString &command);
	void element_selected(QgToolPaletteElement *el);

    public:
	QgEdMainWindow *w = NULL;

    private:
	std::vector<char *> tmp_av;
	unsigned long long select_hash = 0;
	long history_mark_start = -1;
	long history_mark_end = -1;
};

#endif // QGEDAPP_H

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


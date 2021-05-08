/*                       D M A P P . H
 * BRL-DM
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
 *  to DM's data and functionality
 *
 */

#ifndef DMAPP_H
#define DMAPP_H

#include <QApplication>
#include <QObject>
#include <QString>
#include "pqConsoleWidget.h"
#include "ged.h"
#include "main_window.h"

class DMApp : public QApplication
{
    Q_OBJECT

    public:
	DMApp(int &argc, char *argv[]) :QApplication(argc, argv) {
	    if (argc && BU_STR_EQUAL(argv[1], "-s")) {
		w = new DM_MainWindow(1);
	    } else {
		w = new DM_MainWindow(0);
	    }
	};
	~DMApp() {};

	int load_g(const char *filename, int argc, const char **argv);

	void ged_run_cmd(struct bu_vls *msg, int argc, const char **argv);


	unsigned long long prev_dhash = 0;
	unsigned long long prev_vhash = 0;

	DM_MainWindow *w = NULL;
	struct ged *gedp = NULL;
};

#endif // DMAPP_H

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


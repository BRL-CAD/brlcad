/*                        M A I N . C X X
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
/** @file main.cxx
 *
 * Command line parsing and main application launching for qbrlcad
 *
 */

#include <iostream>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QSettings>
#include <QSurfaceFormat>
#include <QFileInfo>

#include "bu/app.h"
#include "bu/opt.h"
#include "brlcad_version.h"

#include "qtcad/Application.h"

#ifndef Q_MOC_RUN
#include "bu/malloc.h" // for struct rt_wdb
#endif

static void
qged_usage(const char *cmd, struct bu_opt_desc *d) {
    struct bu_vls str = BU_VLS_INIT_ZERO;
    char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(&str, "Usage: %s [options] [file.g]\n", cmd);
    if (option_help) {
	bu_vls_printf(&str, "Options:\n%s\n", option_help);
    }
    bu_free(option_help, "help str");
    bu_log("%s", bu_vls_cstr(&str));
    bu_vls_free(&str);
}

int main(int argc, char *argv[])
{
    int consoleMode = 0;
    int swrastMode = 0;
    int quadMode = 0;
    int printHelp = 0;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const char *exec_name = argv[0];

    // All BRL-CAD programs need to set this in order for relative path lookups
    // to work reliably.
    bu_setprogname(argv[0]);

    /* Done with command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* Handle top level application options */
    struct bu_opt_desc d[6];
    BU_OPT(d[0],  "h", "help",   "", NULL, &printHelp,    "Print help and exit");
    BU_OPT(d[1],  "?", "",       "", NULL, &printHelp,    "");
    BU_OPT(d[2],  "c", "no-gui", "", NULL, &consoleMode,  "Run without GUI");
    BU_OPT(d[3],  "s", "swrast", "", NULL, &swrastMode,   "Use software rendering for 3D view");
    BU_OPT(d[4],  "4", "quad",   "", NULL, &quadMode,     "Launch using quad view");
    BU_OPT_NULL(d[5]);

    // High level options are only defined prior to the file argument (if there
    // is one).  See if we need to limit our processing
    int acmax = 1;
    for (int i = 0; i < argc; i++) {
	if (argv[i][0] == '-') {
	    acmax++;
	} else {
	    break;
	}
    }

    // Process high level args
    int opt_ac = bu_opt_parse(&msg, acmax, (const char **)argv, d);
    if (opt_ac < 0) {
	bu_log("%s\n", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return BRLCAD_ERROR;
    }

    // Shift everything not processed by bu_opt_parse down in argv to the last
    // option left behind by bu_opt_parse (or the beginning of the array if
    // opt_ac == 0
    int opt_rem = opt_ac;
    for (int i = acmax; i < argc; i++) {
	argv[opt_ac + (i - acmax)] = argv[i];
	opt_rem++;
    }

    // Set argc to the full count of whatever is left
    argc = opt_rem;

    if (printHelp) {
	qged_usage(exec_name, d);
	bu_vls_free(&msg);
	return BRLCAD_OK;
    }

    // TODO - if we have commands beyond a .g file, we're supposed to process
    // and exit...
    if (argc > 1 && !consoleMode) {
	bu_log("For qged GUI mode need either zero or one .g files specified\n");
	return BRLCAD_ERROR;
    }

    if (consoleMode) {
	bu_log("Unimplemented\n");
	return BRLCAD_ERROR;
    }

    // (Debugging) Report settings filename
    QSettings dmsettings("BRL-CAD", "QGED");
    if (QFileInfo::exists(dmsettings.fileName())) {
	std::cout << "Reading settings from " << dmsettings.fileName().toStdString() << "\n";
    }

    qtcad::Application application = qtcad::Application(argc, argv);

    // Setup complete - time to enter the interactive event loop
    return application.exec();
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

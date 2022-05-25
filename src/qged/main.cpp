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

#include <QTimer>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QApplication>
#include <QTextStream>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/opt.h"
#include "brlcad_version.h"

#include "main_window.h"
#include "app.h"
#include "event_filter.h"
#include "fbserv.h"

extern "C" void
qt_create_io_handler(struct ged_subprocess *p, bu_process_io_t t, ged_io_func_t callback, void *data)
{
    if (!p || !p->p || !p->gedp || !p->gedp->ged_io_data)
	return;

    BRLCAD_MainWindow *w = (BRLCAD_MainWindow *)p->gedp->ged_io_data;
    QtConsole *c = w->console;

    int fd = bu_process_fileno(p->p, t);
    if (fd < 0)
	return;

    c->listen(fd, p, t, callback, data);

    switch (t) {
	case BU_PROCESS_STDIN:
	    p->stdin_active = 1;
	    break;
	case BU_PROCESS_STDOUT:
	    p->stdout_active = 1;
	    break;
	case BU_PROCESS_STDERR:
	    p->stderr_active = 1;
	    break;
    }
}

extern "C" void
qt_delete_io_handler(struct ged_subprocess *p, bu_process_io_t t)
{
    if (!p) return;

    BRLCAD_MainWindow *w = (BRLCAD_MainWindow *)p->gedp->ged_io_data;
    QtConsole *c = w->console;

    // Since these callbacks are invoked from the listener, we can't call
    // the listener destructors directly.  We instead call a routine that
    // emits a single that will notify the console widget it's time to
    // detach the listener.
    switch (t) {
	case BU_PROCESS_STDIN:
	    bu_log("stdin\n");
	    if (p->stdin_active && c->listeners.find(std::make_pair(p, t)) != c->listeners.end()) {
		c->listeners[std::make_pair(p, t)]->m_notifier->disconnect();
		c->listeners[std::make_pair(p, t)]->on_finished();
	    }
	    p->stdin_active = 0;
	    break;
	case BU_PROCESS_STDOUT:
	    if (p->stdout_active && c->listeners.find(std::make_pair(p, t)) != c->listeners.end()) {
		c->listeners[std::make_pair(p, t)]->m_notifier->disconnect();
		c->listeners[std::make_pair(p, t)]->on_finished();
		bu_log("stdout: %d\n", p->stdout_active);
	    }
	    p->stdout_active = 0;
	    break;
	case BU_PROCESS_STDERR:
	    if (p->stderr_active && c->listeners.find(std::make_pair(p, t)) != c->listeners.end()) {
		c->listeners[std::make_pair(p, t)]->m_notifier->disconnect();
		c->listeners[std::make_pair(p, t)]->on_finished();
		bu_log("stderr: %d\n", p->stderr_active);
	    }
	    p->stderr_active = 0;
	    break;
    }

    // All communication has ceased between the app and the subprocess,
    // time to call the end callback (if any)
    if (!p->stdin_active && !p->stdout_active && !p->stderr_active) {
	if (p->end_clbk)
            p->end_clbk(0);
    }

    if (w->c4)
	w->c4->need_update(NULL);
}


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
    int console_mode = 0;
    int swrast_mode = 0;
    int quad_mode = 0;
    int print_help = 0;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const char *exec_name = argv[0];

    // All BRL-CAD programs need to set this in order for relative path lookups
    // to work reliably.
    bu_setprogname(argv[0]);

    /* Done with command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* Handle top level application options */
    struct bu_opt_desc d[6];
    BU_OPT(d[0],  "h", "help",   "", NULL, &print_help,    "Print help and exit");
    BU_OPT(d[1],  "?", "",       "", NULL, &print_help,    "");
    BU_OPT(d[2],  "c", "no-gui", "", NULL, &console_mode,  "Run without GUI");
    BU_OPT(d[3],  "s", "swrast", "", NULL, &swrast_mode,   "Use software rendering for 3D view");
    BU_OPT(d[4],  "4", "quad",   "", NULL, &quad_mode,     "Launch using quad view");
    BU_OPT_NULL(d[5]);

    // High level options are only defined prior to the file argument (if there
    // is one).  See if we need to limit our processing
    int acmax = 0;
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

    if (print_help) {
	qged_usage(exec_name, d);
	bu_vls_free(&msg);
	return BRLCAD_OK;
    }

    // TODO - if we have commands beyond a .g file, we're supposed to process
    // and exit...
    if (argc > 1 && !console_mode) {
	bu_log("For qged GUI mode need either zero or one .g files specified\n");
	return BRLCAD_ERROR;
    }

    if (console_mode) {
	bu_log("Unimplemented\n");
	return BRLCAD_ERROR;
    }

#ifdef BRLCAD_OPENGL
    // Define the surface format for QOpenGLWidget
    QSurfaceFormat format;
    format.setDepthBufferSize(16);
    QSurfaceFormat::setDefaultFormat(format);
#endif

    // We derive our own app type from QApplication so we can store any custom
    // application-wide state there - using the Qt provided global rather than
    // introducing any of our own.  In particular, that is where the current GED
    // state (gedp) lives.
    CADApp app(argc, argv);
    app.setOrganizationName("BRL-CAD");
    app.setOrganizationDomain("brlcad.org");
    app.setApplicationName("QGED");
    app.setApplicationVersion(brlcad_version());
    app.initialize();

    QGEDFilter *efilter = new QGEDFilter();
    app.installEventFilter(efilter);

    // Use the dark theme from https://github.com/Alexhuszagh/BreezeStyleSheets
    QFile file(":/dark.qss");
    file.open(QFile::ReadOnly | QFile::Text);
    QTextStream stream(&file);
    app.setStyleSheet(stream.readAll());

    // The main window defines the primary BRL-CAD interface.
    app.w = new BRLCAD_MainWindow(swrast_mode, quad_mode);

    // Read any saved settings
    app.readSettings();

    // (Debugging) Report settings filename
    QSettings dmsettings("BRL-CAD", "QGED");
    if (QFileInfo(dmsettings.fileName()).exists()) {
	std::cout << "Reading settings from " << dmsettings.fileName().toStdString() << "\n";
    }

    // Disable animated redrawing to minimize performance issues
    app.w->setAnimated(false);

    // This is when the window and widgets are actually drawn
    app.w->show();

    // If the 3D view didn't set up appropriately, try the fallback rendering
    // mode.  We must do this after the show() call, because it isn't until
    // after that point that we know whether the setup of the system's OpenGL
    // context setup was successful.
    if (!app.w->isValid3D()) {
	app.w->fallback3D();
    }

    // If we have a default .g file supplied, open it.  We've delayed doing so
    // until now in order to have the display related containers from graphical
    // initialization available - the GED structure will need to know about some
    // of them to have drawing commands connect properly to the 3D displays.
    QgModel *m = (QgModel *)app.mdl->sourceModel();
    if (argc) {
	int ac = 2;
	const char *av[3];
	av[0] = "open";
	av[1] = argv[0];
	av[2] = NULL;
	int ret = m->run_cmd(m->gedp->ged_result_str, ac, (const char **)av);
	if (ret != BRLCAD_OK) {
	    bu_log("Error opening file %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
    }

    // Connect I/O handlers
    m->gedp->ged_create_io_handler = &qt_create_io_handler;
    m->gedp->ged_delete_io_handler = &qt_delete_io_handler;
    m->gedp->ged_io_data = (void *)app.w;

    // Send a view_change signal so widgets depending on view information
    // can initialize themselves
    emit app.view_change(&m->gedp->ged_gvp);

    // Generally speaking if we're going to have trouble initializing, it will
    // be with either the GED plugins or the dm plugins.  Print relevant
    // messages from those initialization routines (if any) so the user can
    // tell what's going on.
    int have_msg = 0;
    std::string ged_msgs(ged_init_msgs());
    if (ged_msgs.size()) {
	app.w->console->printString(ged_msgs.c_str());
	app.w->console->printString("\n");
	have_msg = 1;
    }
    std::string dm_msgs(dm_init_msgs());
    if (dm_msgs.size()) {
	if (dm_msgs.find("qtgl") != std::string::npos || dm_msgs.find("swrast") != std::string::npos) {
	    app.w->console->printString(dm_msgs.c_str());
	    app.w->console->printString("\n");
	    have_msg = 1;
	}
    }
    if (bu_vls_strlen(&app.init_msgs)) {
    	app.w->console->printString(bu_vls_cstr(&app.init_msgs));
	app.w->console->printString("\n");
	have_msg = 1;
    }

    // If we did write any messages, need to restore the prompt
    if (have_msg) {
	app.w->console->prompt("$ ");
    }

    // Setup complete - time to enter the interactive event loop
    return app.exec();
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

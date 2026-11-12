/*                        M A I N . C X X
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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

#include "common.h"

#include <iostream>

#include <QTimer>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QApplication>
#include <QTextStream>

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/log.h"
#include "brlcad_version.h"

#include "QgEdApp.h"
#include "fbserv.h"

struct qged_args {
    int print_help;
    int console_mode;
    int swrast_mode;
    int quad_mode;
};

static const struct bu_arg_option qged_options[] = {
    BU_ARG_FLAG("h", "help", struct qged_args, print_help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("c", "no-gui", struct qged_args, console_mode, "Run without GUI"),
    BU_ARG_FLAG("s", "swrast", struct qged_args, swrast_mode, "Use software rendering for 3D view"),
    BU_ARG_FLAG("4", "quad", struct qged_args, quad_mode, "Launch using quad view"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand qged_operands[] = {
    BU_ARG_OPERAND("file.g", BU_ARG_VALUE_FILE, 0, 1, "Geometry database to open", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema qged_schema = {
    "qged", "Start the Qt geometry editor", qged_options, qged_operands,
    BU_ARG_PARSE_STOP_AT_FIRST_OPERAND, BU_ARG_SCHEMA_CONSTRAINTS(NULL, NULL)
};

#ifdef HAVE_WINDOWS_H
int APIENTRY
WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpszCmdLine,
	int nCmdShow)
{
    int argc = __argc;
    char **argv = __argv;
#else
int
main(int argc, char **argv)
{
#endif

    struct qged_args args = {};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const char *exec_name = argv[0];

    // QApplication requires argv[0] to remain the executable name.  Parse
    // BRL-CAD's options through a separate view of the argument vector.
    int qt_argc = argc;
    char **qt_argv = argv;
    // All BRL-CAD programs need to set this in order for relative path lookups
    // to work reliably.
    bu_setprogname(argv[0]);

    /* Done with command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    int operand_index = bu_arg_schema_parse_complete(&qged_schema, &args,
	&msg, argc, (const char **)argv);
    if (operand_index < 0) {
	bu_log("%s\n", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return BRLCAD_ERROR;
    }

    argc -= operand_index;
    argv += operand_index;

    if (args.print_help) {
	char *help = bu_arg_schema_help(&qged_schema, exec_name);
	if (help)
	    bu_log("%s", help);
	bu_free(help, "help str");
	bu_vls_free(&msg);
	return BRLCAD_OK;
    }

    // TODO - if we have commands beyond a .g file, we're supposed to process
    // and exit...
    if (args.console_mode) {
	bu_log("Unimplemented\n");
	return BRLCAD_ERROR;
    }

    // We derive our own app type from QApplication.
    const char *g_file = argc ? argv[0] : NULL;
    QgEdApp app(qt_argc, qt_argv, g_file, args.swrast_mode, args.quad_mode);

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

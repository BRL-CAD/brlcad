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

#include "bu/app.h"
#include "bu/cmdschema.h"
#include "bu/log.h"
#include "brlcad_version.h"

#include "QgEdApp.h"
#include "fbserv.h"

struct qged_args {
    int help;
    int console_mode;
    int swrast_mode;
    int quad_mode;
};

static const struct bu_cmd_option qged_options[] = {
    BU_CMD_FLAG("h", "help", struct qged_args, help, "Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_FLAG("c", "no-gui", struct qged_args, console_mode, "Run without GUI"),
    BU_CMD_FLAG("s", "swrast", struct qged_args, swrast_mode, "Use software rendering for the 3D view"),
    BU_CMD_FLAG("4", "quad", struct qged_args, quad_mode, "Launch using quad view"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand qged_operands[] = {
    BU_CMD_OPERAND("database", BU_CMD_VALUE_FILE, 0, 1, "Geometry database file", "ged.file_path"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema qged_schema = {
    "qged", "Launch the QGED geometry editor", qged_options, qged_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static void
qged_usage(const char *cmd)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    char *option_help = bu_cmd_schema_describe(&qged_schema);
    bu_vls_sprintf(&str, "Usage: %s [options] [file.g]\n", cmd);
	if (option_help) {
	bu_vls_printf(&str, "Options:\n%s\n", option_help);
    }
    if (option_help)
	bu_free(option_help, "qged native option help");
    bu_log("%s", bu_vls_cstr(&str));
    bu_vls_free(&str);
}

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

    struct qged_args args = {0, 0, 0, 0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const char *exec_name = argv[0];

    // All BRL-CAD programs need to set this in order for relative path lookups
    // to work reliably.
    bu_setprogname(argv[0]);

    /* Done with command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

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
    int opt_ac = bu_cmd_schema_parse(&qged_schema, &args, &msg, acmax,
	(const char **)argv);
    if (opt_ac < 0) {
	bu_log("%s\n", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return BRLCAD_ERROR;
    }

    // Root options precede the optional database argument.  The native parser
    // returns that operand boundary without rewriting the caller's argv.
    argc -= opt_ac;
    argv += opt_ac;

    if (args.help) {
	qged_usage(exec_name);
	bu_vls_free(&msg);
	return BRLCAD_OK;
    }

    // TODO - if we have commands beyond a .g file, we're supposed to process
    // and exit...
    if (argc > 1 && !args.console_mode) {
	bu_log("For qged GUI mode need either zero or one .g files specified\n");
	return BRLCAD_ERROR;
    }

    if (args.console_mode) {
	bu_log("Unimplemented\n");
	return BRLCAD_ERROR;
    }

    // We derive our own app type from QApplication
    QgEdApp app(argc, argv, args.swrast_mode, args.quad_mode);

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
